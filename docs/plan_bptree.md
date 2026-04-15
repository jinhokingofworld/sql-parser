# B+ 트리 인덱스 확장 계획

## 목표

기존 CSV 기반 SQL 처리기에 메모리 기반 B+ 트리 인덱스 계층을 추가해 다음을 달성합니다.

- 프로세스 실행 동안 테이블 상태를 메모리에 유지
- `INSERT` 시 `id` 자동 부여
- `WHERE id = ?`를 인덱스로 처리
- `WHERE id BETWEEN ? AND ?`를 인덱스로 처리

## 확정된 설계 결정

- `BPTREE_ORDER`는 `100`으로 고정
- `DbContext`는 `main.c`에서만 생성
- B+ 트리는 메모리 전용
- auto-increment 예약 컬럼은 `id`
- 사용자가 `id`를 직접 입력하면 아래 오류를 반환

```text
column 'id' is reserved and cannot be specified manually
```

## 이번 차수 범위

이번 구현 차수에 포함되는 항목:

- `BPTree`
- `DbContext`
- auto-increment insert 흐름
- `BETWEEN` 파싱과 실행
- `score`의 `float` 타입 지원

이번 차수에 포함하지 않는 항목:

- 실행 가능한 benchmark CLI
- Faker 기반 데이터 생성
- benchmark 결과 출력/리포트
- benchmark 전용 테스트 파일

즉 benchmark 요구는 문서 수준으로만 유지하고, 실제 코드는 후속 차수에서 진행합니다.

## 현재 기준 스키마

현재 `users` 스키마는 아래와 같습니다.

```text
table=users
columns=id:int,name:string,grade:int,age:int,region:string,score:float
pkey=id
autoincrement=true
```

정리:

- `region`은 `string`
- `score`는 실제 `float` 타입
- SQL 컬럼명은 `id, name, grade, age, region, score`

## 모듈 설계

### `bptree.h` / `bptree.c`

역할:

- `id` 정수 키만 지원
- 리프에는 `row_index` 저장
- 노드 내부는 이진 탐색
- 리프 연결 리스트로 범위 조회 지원

공개 API:

```c
BPTree *bptree_create(void);
void bptree_destroy(BPTree *tree);
int bptree_insert(BPTree *tree, int key, int row_index, SqlError *error);
int bptree_search(const BPTree *tree, int key, int *row_index);
int bptree_range_search(
    const BPTree *tree,
    int min_key,
    int max_key,
    int **out_indexes,
    int *out_count,
    SqlError *error
);
```

### `db_context.h` / `db_context.c`

역할:

- `schema/*.schema` 스캔
- CSV 전체를 메모리로 적재
- 테이블 단위 B+ 트리 빌드
- `max(id) + 1` 기준 `next_id` 계산
- `RowSet`, `Schema`, `BPTree`, `next_id`를 테이블별로 묶어 유지

추가 구현 결정:

- in-memory row 저장은 capacity doubling 방식으로 늘려 amortized append를 보장
- `INSERT`는 메모리와 인덱스 상태를 먼저 준비한 뒤 마지막에 CSV append를 수행
- CSV append가 실패하면 publish한 row를 롤백하고, stale 결과를 막기 위해 인덱스를 무효화한 뒤 이후 `id` 조회를 선형 fallback으로 처리

공개 API:

```c
DbContext *db_context_create(const char *db_root, SqlError *error);
void db_context_destroy(DbContext *ctx);
TableState *db_context_find_table(DbContext *ctx, const char *table_name);
int db_context_insert_row(DbContext *ctx, const char *table_name, char **fields, SqlError *error);
```

### Parser / AST

변경 항목:

- `VALUE_FLOAT` 추가
- `ConditionType` 추가
- 아래 두 가지 조건 지원
  - `column = value`
  - `column BETWEEN low AND high`

### Executor

변경 항목:

- stateless `db_root` 실행에서 `DbContext *` 기반 실행으로 변경
- `autoincrement=true` 스키마에서는 `id` 자동 채움
- `id` equality/range 조건은 B+ 트리 사용
- 그 외 조건은 선형 탐색 유지

## 검증 대상

현재 구현이 만족해야 하는 체크포인트:

- `INSERT INTO users (id, ...)`는 reserved-column 오류로 실패
- `INSERT INTO users (name, grade, age, region, score)`는 성공
- 시작 시 `next_id`는 기존 CSV 기준 `max(id) + 1`
- `WHERE id = existing`은 1건 반환
- `WHERE id = missing`은 0건 반환
- `WHERE id BETWEEN low AND high`는 기대 범위 반환
- `WHERE id BETWEEN 5 AND 3`은 빈 결과 반환
- `ORDER BY score`는 float 비교 기준으로 정렬
- CSV append 실패 시 rowset이 롤백되고 같은 세션 조회가 오염되지 않음

## 시간 복잡도 목표

- 시작 시 테이블 로드 + 인덱스 빌드: `O(n log_B n)`
- 인덱스가 살아있는 상태의 `INSERT` duplicate check: `O(log_B n)`
- `INSERT` 시 in-memory row publish: amortized `O(1)`
- `WHERE id = ?`: `O(log_B n)`
- `WHERE id BETWEEN a AND b`: `O(log_B n + k)`
- 인덱스를 쓰지 않는 필터: `O(n)`

## 구현 후 반영된 운영 규칙

- 대량 `INSERT` 시 per-insert 전체 row 배열 복사는 허용하지 않음
- devcontainer 기준 `make test`가 통과해야 함
- integration 비교는 CRLF/LF 차이로 실패하지 않도록 유지
- `id` 수동 입력은 컬럼 수 검증보다 먼저 차단해 오류 계약을 보장
- append 실패 시 rowset이 롤백되고 같은 세션 조회가 오염되지 않는 회귀 테스트를 유지

## 남아 있는 후속 과제

- benchmark 실행 코드 추가
- 대량 적재 성능 수치 측정
- `id` 외 보조 인덱스 필요 여부 재검토

## 관련 문서

- [README.md](/C:/Users/cutan/Documents/krafton_jungle_git/sql-parser/README.md)
- [architecture.md](/C:/Users/cutan/Documents/krafton_jungle_git/sql-parser/docs/architecture.md)
