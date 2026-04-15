# B+ Tree 인덱스 확장 세션

## 프로젝트 개요

C11 기반 파일형 SQL 처리기에 **B+ 트리 인덱스**를 추가하는 세션.
이전 차수(Phase 1-7)에서 tokenizer -> parser -> executor -> CSV storage 파이프라인을 완성했다.
이번 세션은 그 위에 인덱스 레이어를 얹어, 학생 관리 데모를 빠르게 시연할 수 있는 구조로 확장하는 작업이다.

이번 세션의 핵심 테이블은 `students`이며, 내부 기본 키인 `id`를 B+ 트리로 인덱싱한다.

---

## 현재 코드베이스 상태

### 실행 흐름
```
main.c -> cli.c -> tokenizer.c -> parser.c -> executor.c -> storage.c
                                                      ->
                                                  schema.c
```

### 핵심 파일
| 파일 | 역할 |
|------|------|
| `include/common.h` | 공통 타입, SqlError, 유틸 선언 |
| `include/ast.h` | Query AST 구조체 (INSERT/SELECT) |
| `include/schema.h` | Schema 구조체, 컬럼/pkey 정보 |
| `include/storage.h` | RowSet, Row, CSV 읽기/쓰기 |
| `include/executor.h` | execute_query, execute_query_list |
| `src/executor.c` | WHERE 필터링, ORDER BY 정렬, 출력 |
| `src/storage.c` | CSV escape/unescape, append/read |

### 현재 한계 (이번 세션에서 해결)
- `SELECT`마다 CSV 전체 파일을 읽음 -> 1M 레코드 시 수 초 소요
- `INSERT` 시 PK 중복 검사도 전체 읽기 O(n)
- `id` 컬럼을 사용자가 직접 명시해야 함
- 인덱스 없음

---

## 학생 관리 데모 스키마

```text
table=students
columns=id:int,student_id:int,name:string,grade:int,age:int,region:string,score:float
pkey=id
autoincrement=true
```

- `id`: 내부 기본 키, 자동 증가, B+ 트리 인덱스 대상
- `student_id`: 학번, 일반 정수 컬럼, 벤치마크에서 비인덱스 정확 일치 조회 대상으로 사용
- `name`: 학생 이름
- `grade`: 학년
- `age`: 나이
- `region`: 지역. SQL schema 타입은 `string`으로 다루고, C 내부 표현은 `char[]` 또는 문자열 포인터여도 무방
- `score`: 4.5 만점 학점, `float`

---

## 이번 세션 목표

1. **B+ 트리 구현** - 메모리 기반, Order 100 (`BPTREE_ORDER 100`)
2. **Auto-increment ID** - `students` 테이블 INSERT 시 `id` 컬럼 명시 금지, 내부 자동 부여
3. **DbContext** - 프로그램 시작 시 CSV -> 메모리 로딩 + B+ 트리 빌드, 세션 동안 유지
4. **인덱스 연동** - `WHERE id = ?` 시 B+ 트리 사용
5. **범위 검색** - `WHERE id BETWEEN a AND b` 지원
6. **성능 비교** - `WHERE id = ?`(인덱스) vs `WHERE student_id = ?`(선형 탐색) 속도 비교
7. **학생 도메인 시연성 강화** - `score(float)`와 `region` 같은 실제 데모 컬럼을 반영

---

## 확정된 설계 결정 (변경 불가)

### B+ 트리 차수
```c
#define BPTREE_ORDER 100  // 노드당 최대 자식 수
```
- 1M 레코드 -> 트리 높이 ~4
- 탐색: ~4 비교 vs 선형 탐색 평균 500,000 비교

### Auto-increment ID 규칙
- `INSERT INTO students (student_id, name, grade, age, region, score) VALUES (...)` <- `id` 생략 필수
- `INSERT INTO students (id, student_id, name, grade, age, region, score) VALUES (...)` <- **오류 반환**
  - 오류 메시지: `"column 'id' is reserved and cannot be specified manually"`
- 시작값: 1. CSV에 기존 레코드가 있으면 `max(id) + 1`에서 시작
- schema 파일에 `autoincrement=true` 추가
- `student_id`는 자동 생성하지 않고, 사용자 입력 또는 데이터 생성기가 직접 채운다

### DbContext 아키텍처
세션 동안 메모리에 유지되는 상태 객체:
```c
typedef struct {
    char db_root[SQL_PATH_BUFFER_SIZE];
    TableState *tables;   // per-table: RowSet + BPTree + next_id
    int table_count;
} DbContext;
```
- `execute_query(query, db_root, out, error)` 시그니처를
  `execute_query(query, ctx, out, error)` 로 변경
- `main.c`에서 DbContext 생성 후 전달

### WHERE 조건 확장 (BETWEEN 지원)
```c
typedef enum { COND_EQ, COND_BETWEEN } ConditionType;

typedef struct {
    ConditionType type;
    char *column;
    Value value;   // COND_EQ에 사용
    Value low;     // COND_BETWEEN에 사용
    Value high;    // COND_BETWEEN에 사용
} Condition;
```

---

## 새로 추가할 파일

```
include/
  bptree.h       <- B+ 트리 ADT
  db_context.h   <- DbContext, TableState

src/
  bptree.c       <- B+ 트리 구현
  db_context.c   <- 초기화, 테이블 로딩, 인덱스 빌드

bench/
  bench.c        <- --bench 플래그용 벤치마크 코드
```

## 수정할 기존 파일

| 파일 | 변경 내용 |
|------|-----------|
| `include/ast.h` | Condition -> ConditionType 추가, BETWEEN 필드 추가 |
| `include/executor.h` | execute_query 시그니처 변경 (db_root -> DbContext*) |
| `include/schema.h` | autoincrement 필드 추가 |
| `src/executor.c` | DbContext 사용, 인덱스 조회 분기 |
| `src/parser.c` | BETWEEN 파싱 추가 |
| `src/schema.c` | `float`, `autoincrement` 파싱 포함 |
| `src/main.c` | DbContext 생성/소멸 추가, --bench 플래그 처리 |
| `data/schema/students.schema` | 학생 관리 데모용 schema 반영 |
| `Makefile` | bptree, db_context, bench 빌드 타겟 추가 |

---

## 코딩 컨벤션 (기존 코드에서 도출)

- **표준**: C11 (`-std=c11 -Wall -Wextra -Werror -pedantic`)
- **들여쓰기**: 공백 4칸
- **함수 이름**: `snake_case`
- **구조체 이름**: `PascalCase` (typedef와 함께)
- **매크로/상수**: `UPPER_SNAKE_CASE`
- **에러 처리**: 모든 함수는 `SqlError *error`를 마지막 인자로 받고, 실패 시 0 반환
- **메모리**: 모든 heap 할당은 NULL 체크, 소유권 명확히
- **정적 함수**: 파일 내부 헬퍼는 `static`
- **주석**: 공개 함수에 입력/출력/실패 조건 설명. "무엇을" + "왜"

### B+ 트리 전용 컨벤션
- 노드 내 키 탐색은 이진 탐색 사용 (선형 탐색 금지)
- 리프 노드는 연결 리스트로 연결 (범위 검색용)
- `bptree_` 접두사 통일

---

## 빌드 및 테스트

```bash
# 전체 빌드
make

# 유닛 테스트
make unit

# 통합 테스트
make integration

# 전체 테스트
make test

# 벤치마크 (학생 1M 레코드)
./sql_processor --bench 1000000 --db ./data

# 일반 실행
./sql_processor --sql query.sql --db ./data
```

---

## 절대 규칙

1. **기존 통합 테스트를 깨뜨리면 안 된다** - `make integration`이 항상 통과해야 함
2. **id 컬럼 자동 부여** - executor가 schema의 autoincrement 플래그를 확인하고 `id`를 직접 할당
3. **`student_id`는 별도 도메인 컬럼** - 자동 증가 기본 키와 혼동하지 말 것
4. **DbContext는 main.c에서만 생성** - executor, storage 내부에서 직접 생성 금지
5. **B+ 트리는 순수 메모리** - 디스크 직렬화 없음
6. **plan_bptree.md를 벗어나는 임의 구현 금지** - 변경이 필요하면 먼저 계획 수정

---

## 참고 문서

- `docs/plan_bptree.md` - 이번 세션 구현 계획 (상세 API 포함)
- `docs/architecture.md` - 학생 관리 데모 기준 아키텍처
- `docs/agent.md` - AI 행동 규약 (Research -> Plan -> Annotate -> Implement -> Feedback)
