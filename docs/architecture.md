# 아키텍처

## 개요

현재 `sql-parser`는 C11 기반 파일형 SQL 처리기로, CSV를 테이블 저장소로 사용합니다. 최신 구현에서는 프로그램 시작 시 CSV를 메모리로 적재하고, `id` 컬럼에 대해 B+ 트리 인덱스를 구축한 뒤 세션 동안 유지합니다.

핵심 목표는 다음과 같습니다.

- 반복적인 CSV 전체 재읽기 제거
- `id` 기반 조회 성능 개선
- auto-increment `id` 지원
- `BETWEEN`과 `float` 타입 지원

## 실행 흐름

```text
main.c
  -> parse_cli_args()
  -> sql_read_text_file()
  -> tokenize_sql()
  -> parse_queries()
  -> db_context_create()
       -> schema/*.schema 스캔
       -> 스키마 로드
       -> tables/*.csv 로드
       -> RowSet 구성
       -> id 기준 B+ 트리 인덱스 빌드
       -> next_id 계산
       -> execute_query_list(queries, ctx)
            -> execute_query(query, ctx)
                 -> INSERT: id 자동 할당, 메모리/인덱스 준비, CSV append, 실패 시 row rollback
                 -> SELECT: id 조건은 인덱스, 그 외는 선형 탐색
  -> db_context_destroy()
```

## 모듈 책임

### 기존 모듈

- `cli.c`
  - `--sql`, `--db`, `--explain` 파싱
- `tokenizer.c`
  - SQL 문자열을 토큰으로 분해
  - 정수, 실수, 문자열, `BETWEEN`, `AND` 지원
- `parser.c`
  - `INSERT`, `SELECT` AST 생성
  - `WHERE column = value`
  - `WHERE column BETWEEN low AND high`
  - `ORDER BY column [ASC|DESC]`
- `schema.c`
  - `.schema` 파일 로드
  - `int`, `float`, `string`, `autoincrement` 파싱
- `storage.c`
  - CSV row 읽기/쓰기
  - `RowSet` 메모리 관리
- `executor.c`
  - 타입 검증
  - auto-increment `id` 채우기
  - 인덱스 조회 분기
  - 결과 포맷팅
- `utils.c`
  - AST 해제 및 공통 유틸

### 신규 모듈

- `bptree.c`
  - 메모리 기반 B+ 트리 구현
  - 단건 조회와 범위 조회 지원
- `db_context.c`
  - 프로세스 생명주기 동안 유지되는 테이블 상태 관리
  - 스키마/CSV 로드
  - 인덱스 빌드
  - `next_id` 관리
  - INSERT 시 메모리 상태 갱신

## 데이터 모델

### schema 파일

```text
table=users
columns=id:int,name:string,grade:int,age:int,region:string,score:float
pkey=id
autoincrement=true
```

### 메모리 상 테이블 상태

```c
typedef struct {
    char name[256];
    Schema schema;
    RowSet rowset;
    BPTree *index;
    int next_id;
} TableState;
```

### 프로세스 전역 상태

```c
typedef struct {
    char db_root[SQL_PATH_BUFFER_SIZE];
    TableState *tables;
    int table_count;
} DbContext;
```

## 조회 경로

### 인덱스를 사용하는 경우

- `WHERE id = N`
  - B+ 트리 단건 조회
- `WHERE id BETWEEN a AND b`
  - B+ 트리 범위 조회
  - 리프 연결 리스트를 통해 순차 결과 수집

### 선형 탐색을 사용하는 경우

- `id` 외 다른 컬럼 조건
- 인덱스가 무효화된 뒤의 `id` 조건 조회

즉, SQL 문법으로는 `BETWEEN`이 일반 컬럼에도 허용되지만, 현재 최적화는 `id` 컬럼에만 적용됩니다.

## INSERT 처리 규칙

### auto-increment

- `autoincrement=true`인 스키마는 `id`를 내부에서 자동 생성합니다.
- 시작 시 `next_id = max(id) + 1`로 계산합니다.
- 사용자가 `id`를 직접 넣으면 오류를 반환합니다.

오류 메시지:

```text
column 'id' is reserved and cannot be specified manually
```

### 메모리 증가 전략

- `RowSet`은 `row_capacity`를 두고 기하급수적으로 증가합니다.
- 매 INSERT마다 전체 row 배열을 새로 복사하지 않습니다.
- 이 변경으로 대량 적재 시 per-insert 전체 복사 O(n) 병목을 제거했습니다.

## 일관성 규칙

현재 구현의 일관성 전략은 다음과 같습니다.

- CSV는 durable source of truth 역할을 합니다.
- `INSERT`는 메모리와 인덱스 상태를 먼저 준비하고, 마지막 단계에서만 CSV append를 시도합니다.
- CSV append가 실패하면 방금 publish한 row를 즉시 롤백합니다.
- 인덱스 반영 또는 append 이후 정리 과정에서 stale index 가능성이 생기면 해당 인덱스를 무효화합니다.
- 인덱스가 무효화된 이후 `id` 조회는 선형 fallback으로 정확성을 유지합니다.

이 설계는 correctness를 우선한 선택입니다. 다만 인덱스 무효화가 발생한 세션에서는 `id` 조회 성능이 저하될 수 있습니다.

## 시간 복잡도

- 시작 시 테이블 로드 + 인덱스 빌드: `O(n log_B n)`
- `WHERE id = ?`: `O(log_B n)`
- `WHERE id BETWEEN a AND b`: `O(log_B n + k)`
- non-id 조건 조회: `O(n)`
- INSERT duplicate check: `O(log_B n)` 또는 인덱스 무효화 후 `O(n)`
- INSERT row append: amortized `O(1)`
- ORDER BY: 매치된 결과 수를 `k`라고 할 때 `O(k log k)`

## 제약 사항

- `DbContext`는 `main.c`에서만 생성합니다.
- B+ 트리는 메모리 전용입니다.
- 보조 인덱스는 아직 없습니다.
- benchmark 실행 코드는 아직 구현 범위에 포함되지 않았습니다.
- 테스트용 `Makefile` 타깃은 POSIX 계열 명령에 의존하므로 devcontainer 환경에서 실행하는 것이 가장 안정적입니다.

## 관련 문서

- [README.md](/C:/Users/cutan/Documents/krafton_jungle_git/sql-parser/README.md)
- [plan_bptree.md](/C:/Users/cutan/Documents/krafton_jungle_git/sql-parser/docs/plan_bptree.md)
- [plan.md](/C:/Users/cutan/Documents/krafton_jungle_git/sql-parser/docs/plan.md)
