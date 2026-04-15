# Architecture

## 실행 흐름 (학생 관리 + B+ 트리 인덱스 추가 후)

```
main.c
  |- parse_cli_args()
  |- db_context_create()        <- 신규: CSV 로딩 + B+ 트리 빌드
  |    \- for each table:
  |         |- load_schema()
  |         |- read_csv_rows()
  |         \- bptree_insert() x N
  |
  |- sql_read_text_file()
  |- tokenize_sql()
  |- parse_queries()
  |- execute_query_list(queries, ctx)
  |    \- execute_query(query, ctx)
  |         |- execute_insert -> db_context_insert_row()
  |         |    |- auto-increment id 할당
  |         |    |- append_csv_row()
  |         |    \- bptree_insert()
  |         \- execute_select
  |              |- WHERE id = ?          -> bptree_search()        O(log n)
  |              |- WHERE id BETWEEN ...  -> bptree_range_search()  O(log n + k)
  |              \- WHERE 기타 조건        -> 선형 탐색               O(n)
  \- db_context_destroy()
```

## 모듈 책임

### 기존 모듈
- `cli.c`: 명령행 옵션 파싱 (`--sql`, `--db`, `--bench`, `--explain`)
- `tokenizer.c`: 키워드, 식별자, 숫자, 문자열 토큰화
- `parser.c`: `INSERT`/`SELECT`/`BETWEEN` 구문 해석
- `schema.c`: schema metadata 로딩과 컬럼 조회 (`float`, `autoincrement` 포함)
- `storage.c`: CSV escape/unescape, row append/read
- `executor.c`: 타입 검증, 인덱스 분기, WHERE 필터링, ORDER BY 정렬, 출력
- `utils.c`: 공통 유틸과 AST 메모리 정리

### 신규 모듈
- `bptree.c`: B+ 트리 삽입/탐색/범위 탐색 (Order 100, 메모리 전용)
- `db_context.c`: 세션 상태 관리 - RowSet, BPTree, auto-increment 카운터
- `bench/bench.c`: 대용량 학생 데이터 삽입 + 성능 측정 (`--bench N`)

## 저장 구조
```
data/
  schema/<table>.schema    <- 메타데이터
  tables/<table>.csv       <- 레코드 (append-only)
```

학생 관리 데모 schema 예시:
```text
table=students
columns=id:int,student_id:int,name:string,grade:int,age:int,region:string,score:float
pkey=id
autoincrement=true
```

## 인덱스 구조

```
BPTree (메모리)
  \- 내부 노드: keys[], children[]
  \- 리프 노드: keys[], values(row_index)[], next<->prev 연결

DbContext
  \- TableState[]
       |- Schema
       |- RowSet      (전체 학생 레코드 메모리)
       |- BPTree*     (id 컬럼 인덱스)
       \- next_id     (auto-increment 카운터)
```

## 학생 관리 데모에서의 질의 분기

- `id`는 내부 기본 키이므로 B+ 트리 인덱스 대상
- `student_id`, `name`, `grade`, `age`, `region`, `score`는 기본적으로 비인덱스 컬럼
- 따라서 "같은 학생 1건 찾기" 비교는 아래처럼 설명하는 것이 가장 직관적이다
  - `WHERE id = 500000` -> 인덱스 사용
  - `WHERE student_id = 202550000` -> 선형 탐색

## 인덱스 vs 선형 탐색 분기 (학생 1M 레코드 기준)

| 쿼리 패턴 | 경로 | 비교 횟수 |
|-----------|------|-----------|
| `WHERE id = N` | B+ 트리 탐색 | ~4 |
| `WHERE id BETWEEN a AND b` | B+ 트리 범위 탐색 | ~4 + k |
| `WHERE student_id = S` | 선형 탐색 | ~500,000 |
| `WHERE region = 'Seoul'` | 선형 탐색 | ~500,000 |
| `WHERE score = 4.5` | 선형 탐색 | ~500,000 |
