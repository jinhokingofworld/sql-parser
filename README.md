# SQL Processor

C11로 작성한 파일 기반 SQL 처리기입니다. SQL 파일을 입력받아 CSV 테이블에 대해 `INSERT`와 `SELECT`를 수행하며, 현재는 `users` 테이블의 `id` 컬럼에 대해 메모리 기반 B+ 트리 인덱스를 사용합니다.

## 프로젝트 상태

현재 구현된 핵심 범위는 다음과 같습니다.

- SQL 파일 기반 실행
- `INSERT INTO ... VALUES ...`
- `SELECT ... FROM ...`
- `WHERE column = value`
- `WHERE column BETWEEN low AND high`
- `ORDER BY column [ASC|DESC]`
- CSV 기반 테이블 저장
- schema metadata 기반 타입 검증
- `id` auto-increment
- `WHERE id = ?`, `WHERE id BETWEEN ? AND ?` 인덱스 조회
- 여러 SQL statement 순차 실행

아직 구현하지 않은 항목:

- `UPDATE`, `DELETE`, `CREATE TABLE`
- `JOIN`, `GROUP BY`, `LIMIT`
- 실행 가능한 벤치마크 CLI

## 현재 스키마

기본 예제와 테스트는 아래 `users` 스키마를 기준으로 동작합니다.

```text
table=users
columns=id:int,name:string,grade:int,age:int,region:string,score:float
pkey=id
autoincrement=true
```

컬럼 의미:

- `id`: 내부 자동 증가 정수 PK
- `name`: 이름 문자열
- `grade`: 학년 정수
- `age`: 나이 정수
- `region`: 지역 문자열
- `score`: 학점 실수

테스트팀 전달 기준을 반영해 저장소에는 보조 테스트 테이블인 `students`도 함께 둘 수 있습니다. `students`는 동일한 6컬럼 구조를 사용하지만, auto-increment 없이 수동 `id` 입력을 받는 일반 테이블 예제로 취급합니다.

중요 규칙:

- `autoincrement=true`인 테이블에서는 `id`를 직접 넣을 수 없습니다.
- 수동으로 `id`를 지정하면 다음 오류를 반환합니다.

```text
column 'id' is reserved and cannot be specified manually
```

## 디렉터리 구조

```text
sql-parser/
├─ data/
│  ├─ schema/
│  │  ├─ users.schema
│  │  └─ students.schema
│  └─ tables/
│     ├─ users.csv
│     └─ students.csv
├─ docs/
│  ├─ architecture.md
│  ├─ plan.md
│  ├─ plan_bptree.md
│  ├─ research.md
│  └─ agent.md
├─ include/
├─ src/
├─ tests/
│  ├─ fixtures/
│  ├─ integration/
│  └─ unit/
├─ Makefile
└─ README.md
```

주요 경로:

- `data/schema/<table>.schema`: 테이블 스키마 정의
- `data/tables/<table>.csv`: 실제 CSV 데이터
- `src/`: 런타임 구현
- `tests/unit/`: 단위 테스트
- `tests/integration/`: 통합 테스트
- `docs/architecture.md`: 현재 아키텍처 설명
- `docs/plan_bptree.md`: B+ 트리 확장 설계 문서

## 실행 흐름

현재 런타임은 아래 순서로 동작합니다.

1. CLI가 SQL 파일 경로와 DB 루트를 받습니다.
2. SQL 파일을 읽고 tokenizer가 토큰으로 분해합니다.
3. parser가 `INSERT` 또는 `SELECT` AST를 만듭니다.
4. 프로그램 시작 시 `DbContext`가 생성됩니다.
5. `DbContext`가 `schema/*.schema`와 `tables/*.csv`를 읽어 메모리에 적재합니다.
6. `id` 컬럼을 기준으로 B+ 트리 인덱스를 빌드합니다.
7. executor가 쿼리를 실행합니다.
8. 종료 시 `DbContext`를 해제합니다.

핵심 차이점:

- 이전의 stateless CSV 재조회 방식이 아니라, 프로세스 생명주기 동안 `RowSet + BPTree + next_id`를 유지합니다.
- `WHERE id = ?`, `WHERE id BETWEEN ? AND ?`는 인덱스를 타고, 그 외 조건은 메모리 상 선형 탐색을 사용합니다.

## 지원 SQL 예시

### INSERT

```sql
INSERT INTO users (name, grade, age, region, score)
VALUES ('Charlie', 1, 19, 'Incheon', 3.25);
```

지원 규칙:

- 컬럼 목록은 명시해야 합니다.
- 값 개수는 컬럼 개수와 같아야 합니다.
- `id`는 넣지 않습니다.
- `score`는 실수 literal을 받을 수 있습니다.

실행 결과:

```text
INSERT 1
```

### SELECT

전체 조회:

```sql
SELECT * FROM users;
```

특정 컬럼 조회:

```sql
SELECT id, name FROM users;
```

조건 조회:

```sql
SELECT name FROM users WHERE grade = 4;
```

범위 조회:

```sql
SELECT name FROM users WHERE id BETWEEN 2 AND 10 ORDER BY id;
```

정렬:

```sql
SELECT id, name, score FROM users ORDER BY score DESC;
```

여러 statement:

```sql
INSERT INTO users (name, grade, age, region, score)
VALUES ('Dana', 2, 21, 'Seoul', 4.10);

SELECT id, name FROM users ORDER BY id;
```

## 터미널 데모 빠른 시작

발표나 시연에서는 `make demo` 계열을 먼저 사용하는 흐름을 권장합니다. 이 경로는 영속 demo DB인 `./data/demo`를 기준으로, 실제 SQL 본문과 쿼리별 `결과`, `소요시간`, `탐색 지표`를 한 번에 보여주도록 정리되어 있어 쿼리 파일을 따로 열지 않고도 설명할 수 있습니다.

```bash
make
make demo
```

개별 시나리오만 보고 싶을 때:

```bash
make demo-users
make demo-range
make demo-fail
```

구성:

- `make demo`
  - 영속 `data/demo` 기준으로 auto-increment, `BETWEEN`, reserved `id` 실패 시나리오를 순서대로 실행
- `make demo-users`
  - auto-increment INSERT 뒤에 인덱스 단건 조회와 선형 탐색을 비교
- `make demo-range`
  - `WHERE id BETWEEN ...` 인덱스 범위 조회를 확인
- `make demo-fail`
  - `id` 수동 입력 금지 에러 메시지를 확인

주의:

- `make demo*`는 `./data/demo`를 직접 사용하므로 실행 결과가 누적됩니다.
- 기준 시연 데이터를 바꾸고 싶으면 `data/demo/schema/`, `data/demo/tables/` 아래 파일을 직접 편집하면 됩니다.

## CLI 사용법

권장 실행 형식:

```bash
./sql_processor --sql <sql-file> --db <db-root>
```

간단 실행 형식:

```bash
./sql_processor <sql-file>
```

지원 옵션:

- `--sql <file>`: 실행할 SQL 파일
- `--db <path>`: 데이터 루트 경로, 기본값은 `./data`
- `--explain`: 실행 전 파싱된 쿼리 구조를 출력

예시:

```bash
./sql_processor --sql query.sql --db ./data
./sql_processor --sql query.sql --db ./data --explain
```

## 빌드

현재 환경에서 직접 빌드해야 합니다.

```bash
make clean
make
```

생성 결과:

```bash
./sql_processor
```

## 테스트

단위 테스트:

```bash
make unit
```

통합 테스트:

```bash
make integration
```

전체 테스트:

```bash
make test
```

발표용 데모:

```bash
make demo
```

devcontainer 기준 검증:

- `make test` 통과
- integration 비교는 줄바꿈 차이(CRLF/LF)를 무시하도록 구성
- 기본 `data/tables/users.csv`는 빈 상태를 기준으로 유지
- integration SQL은 self-contained 테스트이므로 내부에서 필요한 데이터를 직접 삽입

## B+ 트리 인덱스 동작 요약

현재 인덱스는 `id` 컬럼 전용입니다.

- 자료구조: 메모리 기반 B+ 트리
- 차수: `BPTREE_ORDER 100`
- 키: `int id`
- 값: `row_index`
- 리프 노드: 연결 리스트로 범위 탐색 지원

복잡도 개요:

- 시작 시 인덱스 빌드: `O(n log_B n)`
- `WHERE id = ?`: `O(log_B n)`
- `WHERE id BETWEEN a AND b`: `O(log_B n + k)`
- non-id WHERE: `O(n)`
- INSERT 중 row append: amortized `O(1)`

## 구현 세부 사항

### Auto-increment

- `DbContext`가 시작 시 CSV를 읽어 `max(id) + 1`을 계산합니다.
- `INSERT` 시 executor가 `id`를 자동 채웁니다.
- 사용자가 `id`를 직접 지정하면 실패합니다.

### 메모리 상태와 일관성

- 각 테이블은 `Schema`, `RowSet`, `BPTree`, `next_id`를 함께 유지합니다.
- `RowSet`은 capacity를 두고 증가시켜 대량 `INSERT` 시 전체 복사를 피합니다.
- `INSERT`는 메모리와 인덱스 상태를 먼저 준비한 뒤 마지막에 CSV append를 수행합니다.
- CSV append가 실패하면 방금 publish한 row를 즉시 롤백하고, stale index를 막기 위해 인덱스를 무효화한 뒤 이후 `id` 조회를 선형 탐색으로 fallback 합니다.

## 현재 제약 사항

- 인덱스는 `id` 1개 컬럼만 지원합니다.
- 벤치마크 CLI와 Faker 기반 데이터 생성은 아직 문서 범위입니다.
- B+ 트리는 메모리 전용이며 디스크 직렬화는 하지 않습니다.
- Linux/Unix 계열 명령(`mktemp`, `cp`, `diff`)을 사용하는 테스트 타깃은 POSIX 계열 환경이나 devcontainer에서 실행하는 것이 안전합니다.

## 문서 안내

- [docs/docs_index.md](/C:/Users/cutan/Documents/krafton_jungle_git/sql-parser/docs/docs_index.md): 문서 인덱스와 제출용 묶음 안내
- [docs/architecture.md](/C:/Users/cutan/Documents/krafton_jungle_git/sql-parser/docs/architecture.md): 현재 런타임 구조와 복잡도
- [docs/plan_bptree.md](/C:/Users/cutan/Documents/krafton_jungle_git/sql-parser/docs/plan_bptree.md): B+ 트리 확장 설계와 검증 포인트
- [docs/demo_test_guide.md](/C:/Users/cutan/Documents/krafton_jungle_git/sql-parser/docs/demo_test_guide.md): 시연용 테스트 절차와 설명 포인트
- [docs/plan.md](/C:/Users/cutan/Documents/krafton_jungle_git/sql-parser/docs/plan.md): 초기 구현 계획
- [docs/research.md](/C:/Users/cutan/Documents/krafton_jungle_git/sql-parser/docs/research.md): 초기 조사 내용

## 빠른 시작

```bash
make
printf "SELECT * FROM users ORDER BY id;\n" > /tmp/sql-parser-quickstart.sql
./sql_processor --sql /tmp/sql-parser-quickstart.sql --db ./data
```

문제가 생기면 먼저 아래를 확인하면 좋습니다.

- SQL 문장이 세미콜론 `;`으로 끝나는지
- `--db` 경로 아래에 `schema/`, `tables/`가 모두 있는지
- schema와 CSV 컬럼 수가 일치하는지
- `autoincrement=true` 테이블에 `id`를 직접 넣지 않았는지

주의:

- `tests/integration/*.sql` 중 일부는 테스트를 위해 `INSERT`를 포함하므로, 실제 `./data`에 바로 실행하면 CSV가 바뀔 수 있습니다.
- integration SQL을 수동 실행할 때는 fixture를 복사한 임시 DB를 사용하는 편이 안전합니다.
