# Plan - B+ 트리 인덱스 확장

## 목표

기존 CSV 기반 SQL 처리기에 메모리 기반 B+ 트리 인덱스를 추가한다.
이번 데모의 기준 테이블은 학생 관리용 `students`이며, 스키마는 아래와 같다.

```text
table=students
columns=id:int,student_id:int,name:string,grade:int,age:int,region:string,score:float
pkey=id
autoincrement=true
```

완성 기준:
- `WHERE id = ?` 쿼리가 B+ 트리를 통해 O(log n)으로 동작
- `WHERE id BETWEEN a AND b` 범위 검색 지원
- `INSERT` 시 `id` 자동 부여 및 B+ 트리 등록
- `student_id`, `name`, `grade`, `age`, `region`, `score` 조회는 기존 선형 탐색 유지
- 1M 학생 레코드 기준 인덱스 탐색 vs 선형 탐색 성능 비교 가능
- 기존 통합 테스트 전부 통과

---

## 접근 방식

### 1. B+ 트리 모듈 (`bptree.h` / `bptree.c`)

#### 핵심 설계 결정

- **Order**: `BPTREE_ORDER 100` (노드당 최대 99개 키, 100개 자식)
- **키**: `int` (`id` 컬럼 전용)
- **값**: `int row_index` (DbContext 내 RowSet 배열 인덱스)
- **리프 노드 연결**: 양방향 linked list -> 범위 탐색 O(log n + k)
- **노드 내 탐색**: 이진 탐색

#### 구조체 설계

```c
#define BPTREE_ORDER 100

typedef struct BPTreeNode {
    int is_leaf;
    int key_count;
    int keys[BPTREE_ORDER - 1];

    /* 내부 노드 전용 */
    struct BPTreeNode *children[BPTREE_ORDER];

    /* 리프 노드 전용 */
    int values[BPTREE_ORDER - 1];    /* row_index */
    struct BPTreeNode *next;         /* 오른쪽 리프 */
    struct BPTreeNode *prev;         /* 왼쪽 리프 */
} BPTreeNode;

typedef struct {
    BPTreeNode *root;
} BPTree;
```

#### 공개 API

```c
/* 생성/소멸 */
BPTree *bptree_create(void);
void    bptree_destroy(BPTree *tree);

/* 삽입 */
/* key 중복 허용 안 함. 중복 시 0 반환 */
int bptree_insert(BPTree *tree, int key, int row_index, SqlError *error);

/* 단일 키 탐색 */
/* 찾으면 row_index에 값 설정 후 1 반환, 없으면 0 반환 */
int bptree_search(const BPTree *tree, int key, int *row_index);

/* 범위 탐색 */
/* [min_key, max_key] 범위에 해당하는 row_index 배열 반환 */
/* 호출자가 *out_indexes를 free해야 함 */
int bptree_range_search(
    const BPTree *tree,
    int min_key,
    int max_key,
    int **out_indexes,
    int *out_count,
    SqlError *error
);

/* 디버그/통계 */
int bptree_height(const BPTree *tree);
int bptree_node_count(const BPTree *tree);
```

#### 트레이드오프

| 옵션 | 장점 | 단점 |
|------|------|------|
| Order 4 (소) | 트리 구조 교육적으로 시각화 용이 | 높이 ~20, 탐색 비교 횟수 많음 |
| **Order 100 (채택)** | 높이 ~4, 발표 시 임팩트 최대 | 노드 배열 크기 고정으로 메모리 다소 낭비 |
| 동적 차수 | 유연 | 구현 복잡도 증가 |

-> 발표 임팩트와 구현 단순성 균형상 Order 100 채택

---

### 2. DbContext 모듈 (`db_context.h` / `db_context.c`)

#### 목적

세션 동안 in-memory 상태를 유지하는 컨테이너.
현재 executor.c는 호출마다 CSV를 읽는 stateless 구조인데,
1M 학생 레코드 환경에서는 프로그램 시작 시 한 번 로딩 후 메모리에서 처리해야 한다.

#### 구조체 설계

```c
typedef struct {
    char name[256];
    Schema schema;
    RowSet rowset;       /* 전체 레코드 메모리 보유 */
    BPTree *index;       /* id 컬럼 B+ 트리 */
    int next_id;         /* auto-increment 카운터 */
} TableState;

typedef struct {
    char db_root[SQL_PATH_BUFFER_SIZE];
    TableState *tables;
    int table_count;
} DbContext;
```

#### 공개 API

```c
/* db_root 아래 모든 .schema 파일을 읽어 TableState 초기화 */
/* 각 테이블의 CSV 로딩 및 B+ 트리 빌드 수행 */
DbContext *db_context_create(const char *db_root, SqlError *error);

void db_context_destroy(DbContext *ctx);

/* 테이블 이름으로 TableState 조회 */
TableState *db_context_find_table(DbContext *ctx, const char *table_name);

/* INSERT 시 새 레코드를 RowSet에 추가하고 B+ 트리에 등록 */
/* CSV 파일에도 동시에 append */
int db_context_insert_row(
    DbContext *ctx,
    const char *table_name,
    char **fields,         /* schema 순서대로 정렬된 값 배열 (id 포함) */
    SqlError *error
);
```

#### 초기화 흐름

```
db_context_create(db_root)
  \- scan db_root/schema/*.schema
       \- for each schema file:
            |- load_schema()
            |- read_csv_rows()           <- 기존 storage.c 재사용
            |- bptree_create()
            |- for each row:
            |    bptree_insert(id_value, row_index)
            \- next_id = max(id) + 1
```

---

### 3. Executor 변경

#### 시그니처 변경

```c
/* 기존 */
int execute_query(const Query *query, const char *db_root, FILE *out, SqlError *error);

/* 변경 후 */
int execute_query(const Query *query, DbContext *ctx, FILE *out, SqlError *error);
int execute_query_list(const QueryList *queries, DbContext *ctx, FILE *out, SqlError *error);
```

#### INSERT 실행 변경

1. schema 로드 -> `db_context_find_table()` 사용으로 교체
2. **id 컬럼 명시 여부 검사** - `id`가 포함된 경우 즉시 오류
3. auto-increment id 할당 (`table_state->next_id++`)
4. ordered_fields 배열에 `id` 자동 삽입
5. `db_context_insert_row()` 호출 (CSV + 인덱스 동시 처리)
6. `student_id`, `name`, `grade`, `age`, `region`, `score`는 사용자가 제공
7. `ensure_primary_key_unique()` 제거 -> B+ 트리 삽입 실패로 대체

#### SELECT 실행 변경

```
if WHERE 조건 없음:
    -> RowSet 전체 출력

if WHERE id = value (COND_EQ, 컬럼이 id):
    -> bptree_search(index, value, &row_index)
    -> rowset.rows[row_index] 직접 접근

if WHERE id BETWEEN low AND high (COND_BETWEEN, 컬럼이 id):
    -> bptree_range_search(index, low, high, &indexes, &count)
    -> 해당 row_index 배열로 결과 구성

else (student_id, name, grade, age, region, score WHERE):
    -> 기존 선형 탐색 유지
```

이 분기가 "인덱스 사용 vs 선형 탐색" 성능 비교의 핵심이다.

---

### 4. AST 변경 (`ast.h`)

```c
typedef enum {
    COND_EQ,       /* column = value */
    COND_BETWEEN   /* column BETWEEN low AND high */
} ConditionType;

typedef struct {
    ConditionType type;
    char *column;
    Value value;   /* COND_EQ 전용 */
    Value low;     /* COND_BETWEEN 전용 */
    Value high;    /* COND_BETWEEN 전용 */
} Condition;
```

기존 `Condition`의 `value` 필드는 유지하여 COND_EQ 하위 호환 보장.

---

### 5. Schema 변경

#### schema 파일 포맷

```text
table=students
columns=id:int,student_id:int,name:string,grade:int,age:int,region:string,score:float
pkey=id
autoincrement=true
```

#### Schema 구조체 추가

```c
typedef struct {
    char *table_name;
    ColumnDef *columns;
    int column_count;
    char *primary_key;
    int primary_key_index;
    int autoincrement;   /* 신규: 1이면 pkey 자동 부여 */
} Schema;
```

---

### 6. 파서 변경 (`parser.c`)

#### INSERT 파싱

- 컬럼 목록에 `id`(autoincrement 컬럼)가 있으면 파싱 단계가 아니라
  executor에서 오류 처리
- 컬럼 수 검사는 executor로 위임
- `score`의 소수 리터럴 입력을 허용해야 하므로 `float` 값 파싱이 안정적으로 유지되어야 함

#### SELECT WHERE BETWEEN 파싱

```
BETWEEN 파싱 흐름:
  consume identifier (column)
  consume BETWEEN
  consume value -> low
  consume AND
  consume value -> high
  -> Condition { type=COND_BETWEEN, column, low, high }
```

---

### 7. 벤치마크 (`bench/bench.c` + `--bench` 플래그)

#### CLI 변경

```bash
./sql_processor --bench 1000000 --db ./data
```

#### 벤치마크 데이터 생성

```
1. bulk_insert(N rows):
   - student_id: 202500001 ~ 202500000 + N
   - name: "Student_1" ~ "Student_N"
   - grade: 랜덤 1~4
   - age: 랜덤 19~27
   - region: Seoul / Busan / Incheon / Daegu / Daejeon 중 하나
   - score: 0.0 ~ 4.5 범위의 float
   - timing: 삽입 총 시간 측정
```

#### 벤치마크 질의

```
2. indexed_select():
   - SELECT * FROM students WHERE id = N/2;
   - B+ 트리 경로 사용
   - timing 측정

3. linear_select_exact():
   - SELECT * FROM students WHERE student_id = 202500000 + N/2;
   - 선형 탐색 경로 사용
   - timing 측정

4. linear_select_domain():
   - SELECT * FROM students WHERE region = 'Seoul';
   - 선형 탐색 경로 사용
   - timing 측정

5. range_select():
   - SELECT * FROM students WHERE id BETWEEN N/4 AND 3*N/4;
   - B+ 트리 범위 탐색
   - timing 측정
```

출력 예시:
```
[BENCH] Inserted 1,000,000 student rows in 3.42s
[BENCH] Indexed  SELECT (id=500000):                  0.000012s  (~4 comparisons)
[BENCH] Linear   SELECT (student_id=202550000):       1.823400s  (~500000 comparisons)
[BENCH] Linear   SELECT (region='Seoul'):             1.956200s  (full scan)
[BENCH] Range    SELECT (id BETWEEN ...):             0.000031s  (250000 results)
```

타이밍: `clock()` 또는 Windows 환경에 맞는 monotonic 타이머 사용

---

### 8. 기능 테스트 (통합): 학생 관리 데모 기준 검증

```sql
-- tests/integration/bptree_students_select.sql
INSERT INTO students (student_id, name, grade, age, region, score)
VALUES (20250001, 'Kim Minji', 1, 19, 'Seoul', 4.25);

INSERT INTO students (student_id, name, grade, age, region, score)
VALUES (20250002, 'Lee Jiwon', 2, 20, 'Busan', 3.75);

SELECT * FROM students WHERE id = 1;
SELECT * FROM students WHERE student_id = 20250002;
SELECT * FROM students WHERE id BETWEEN 1 AND 2;
```

#### 엣지 케이스

- `WHERE id = 0` -> 결과 없음 (`id`는 1부터)
- `WHERE id BETWEEN 5 AND 3` -> 결과 없음 (`low > high`)
- `WHERE student_id = 99999999` -> 존재하지 않는 학번
- `WHERE score = 4.5` -> `float` 값 비교 경로 검증
- `INSERT INTO students (id, student_id, name, grade, age, region, score) VALUES (...)` -> 오류

---

## 8시간 구현 계획

| 시간 | 작업 | 산출물 |
|------|------|--------|
| 0-1h | B+ 트리 노드 구조체 + 기본 삽입 | `bptree.h`, `bptree.c` 뼈대 |
| 1-2h | B+ 트리 탐색 + 범위 탐색 | `bptree.c` 완성 |
| 2-3h | DbContext 구조체 + 초기화 + 테이블 로딩 | `db_context.h`, `db_context.c` |
| 3-4h | Executor 변경: auto-ID, 인덱스 INSERT | `make test` 통과 |
| 4-5h | Executor 변경: 인덱스 SELECT, 범위 SELECT | 인덱스 분기 동작 확인 |
| 5-6h | Parser BETWEEN + AST 변경 + 학생 스키마 연동 | 학생 SQL 쿼리 동작 |
| 6-7h | 벤치마크 코드 + --bench 플래그 | 성능 수치 출력 |
| 7-8h | 엣지 케이스 보강 + README/데모 정리 | 최종 `make test` 통과 |

---

## 파일 변경 목록

### 신규 생성
- `CLAUDE.md`
- `include/bptree.h`
- `include/db_context.h`
- `src/bptree.c`
- `src/db_context.c`
- `bench/bench.c`
- `tests/integration/bptree_students_select.sql`
- `tests/integration/bptree_students_select.expected`
- `tests/integration/bptree_students_range.sql`
- `tests/integration/bptree_students_range.expected`

### 수정
- `include/ast.h` - ConditionType, Condition BETWEEN 필드 추가
- `include/executor.h` - execute_query 시그니처 변경
- `include/schema.h` - autoincrement 필드 추가
- `src/executor.c` - DbContext 사용, 인덱스 분기, auto-ID
- `src/parser.c` - BETWEEN 파싱
- `src/schema.c` - `float`, `autoincrement` 파싱
- `src/main.c` - DbContext 생성/소멸, --bench 플래그
- `data/schema/students.schema` - 학생 관리 데모 schema 반영
- `tests/fixtures/db/schema/students.schema` - 학생 데모 fixture 반영
- `Makefile` - bptree, db_context, bench 빌드 타겟

---

## 리스크 및 대응

| 리스크 | 대응 |
|--------|------|
| B+ 트리 분열 로직 버그 | 통합 테스트에 분열 경계 케이스(Order 경계 근처 삽입) 포함 |
| DbContext 도입 시 기존 통합 테스트 파손 | executor.h 시그니처 변경 직후 `make integration` 즉시 확인 |
| 1M 학생 레코드 삽입 시 메모리 부족 | RowSet realloc 패턴 유지, 필요 시 청크 단위 로딩 검토 |
| `score:float` 비교가 회귀를 만들 수 있음 | float 파싱/비교 단위 테스트 유지, 샘플 값은 4.25/3.75/4.5처럼 명확한 값 사용 |
| `student_id`와 `id` 의미 혼동 | README와 데모에서 내부 PK vs 도메인 학번 역할을 분리 설명 |
| CSV 줄 버퍼 4096 바이트 제한 | 기존 `storage.c`의 line 버퍼 크기 확인, 레코드 길이 제한 문서화 |

---

## 진행 상황

- [ ] B+ 트리 구조체 + 삽입
- [ ] B+ 트리 탐색 + 범위 탐색
- [ ] DbContext 초기화 + 테이블 로딩
- [ ] Executor auto-ID INSERT
- [ ] Executor 인덱스 SELECT
- [ ] Executor BETWEEN 범위 SELECT
- [ ] Parser BETWEEN 지원
- [ ] 학생 관리 벤치마크 코드 + --bench 플래그
- [ ] 통합 테스트 추가
- [ ] `make test` 전체 통과
