# Testing And Demo Guide

이 문서는 `sql-parser`를 처음 받는 팀원이 테스트 구조를 이해하고, 발표나 리뷰 자리에서 바로 시연할 수 있도록 정리한 안내서입니다.

## Purpose

- 어떤 테스트가 어디에 있는지 빠르게 파악한다.
- Linux/devcontainer 기준 명령과 Windows 대체 경로를 같이 제공한다.
- 실제 시연 시 `users`와 `students`를 어떻게 보여줄지 정리한다.

## Recommended Environment

가장 권장하는 환경은 아래 중 하나입니다.

- devcontainer
- Docker 컨테이너
- Linux / WSL

이유:

- `Makefile`의 통합 테스트가 `mktemp`, `cp`, `diff` 같은 POSIX 명령에 의존합니다.
- PowerShell만으로도 가능하지만, Windows에서는 `tools/build_windows.ps1`, `tools/test_windows.ps1`를 쓰는 별도 흐름이 필요합니다.

## Test Map

- [docs/make_and_raw_data_guide.md](/C:/Users/cutan/Documents/krafton_jungle_git/sql-parser/docs/make_and_raw_data_guide.md)
  - `make` 명령과 raw data 준비 흐름 설명
- [tests/README.md](/C:/Users/cutan/Documents/krafton_jungle_git/sql-parser/tests/README.md)
  - 전체 테스트 구조 설명
- [data/README.md](/C:/Users/cutan/Documents/krafton_jungle_git/sql-parser/data/README.md)
  - 실제 DB 루트와 schema/table 규칙 설명
- [tools/README.md](/C:/Users/cutan/Documents/krafton_jungle_git/sql-parser/tools/README.md)
  - 생성기, loader, Windows 스크립트 설명

## Setup Or Usage

### Linux Or Devcontainer

```bash
make clean
make
make unit
make bptree-contract
make integration
make test
```

### Windows PowerShell

```powershell
.\tools\build_windows.ps1 -Target tests
.\tools\test_windows.ps1 -Suite all
```

개별 suite:

```powershell
.\tools\test_windows.ps1 -Suite unit
.\tools\test_windows.ps1 -Suite integration
.\tools\test_windows.ps1 -Suite verify
.\tools\test_windows.ps1 -Suite bptree-contract
```

## What Each Test Proves

- `make unit`
  - tokenizer, parser, storage, executor 모듈의 세부 동작을 점검합니다.
- `make bptree-contract`
  - B+ 트리 API 계약을 별도로 점검합니다.
- `make integration`
  - 실제 SQL 파일을 CLI로 실행했을 때 출력이 기대값과 같은지 확인합니다.
- `make test`
  - 위 흐름 전체를 묶어 회귀를 확인합니다.
- `verify_students_dataset`
  - `students` 데이터셋이 schema 계약과 타입 규칙을 만족하는지 확인합니다.
- `--bench`
  - `WHERE id = ?`와 일반 조건 경로의 상대 성능을 관찰합니다.

## Demo Scenario

시연은 항상 실제 `./data`가 아니라 임시 DB 복사본으로 진행하는 것을 권장합니다.

### Demo 1. `users` Auto-Increment

```bash
TMP_DB=$(mktemp -d /tmp/sql-parser-demo-users-XXXXXX)
mkdir -p "$TMP_DB/schema" "$TMP_DB/tables"
cp tests/fixtures/db/schema/users.schema "$TMP_DB/schema/users.schema"
cp tests/fixtures/db/tables/users.csv "$TMP_DB/tables/users.csv"

cat > "$TMP_DB/demo_users.sql" <<'SQL'
INSERT INTO users (name, grade, age, region, score)
VALUES ('Alice', 2, 20, 'Seoul', 3.80);

INSERT INTO users (name, grade, age, region, score)
VALUES ('Bob', 4, 25, 'Busan', 4.10);

SELECT id, name, age FROM users WHERE id = 2;
SQL

./sql_processor --sql "$TMP_DB/demo_users.sql" --db "$TMP_DB"
```

확인 포인트:

- `id`를 직접 넣지 않았는데 자동 부여되는지
- `WHERE id = 2` 조회가 되는지

### Demo 2. `WHERE id BETWEEN`

```bash
TMP_DB=$(mktemp -d /tmp/sql-parser-demo-range-XXXXXX)
mkdir -p "$TMP_DB/schema" "$TMP_DB/tables"
cp tests/fixtures/db/schema/users.schema "$TMP_DB/schema/users.schema"
cp tests/fixtures/db/tables/users.csv "$TMP_DB/tables/users.csv"

cat > "$TMP_DB/demo_range.sql" <<'SQL'
INSERT INTO users (name, grade, age, region, score) VALUES ('Alice', 2, 20, 'Seoul', 3.80);
INSERT INTO users (name, grade, age, region, score) VALUES ('Bob', 4, 25, 'Busan', 4.10);
INSERT INTO users (name, grade, age, region, score) VALUES ('Charlie', 1, 19, 'Incheon', 3.25);

SELECT id, name FROM users WHERE id BETWEEN 2 AND 3 ORDER BY id;
SQL

./sql_processor --sql "$TMP_DB/demo_range.sql" --db "$TMP_DB"
```

확인 포인트:

- `Bob`, `Charlie`만 출력되는지
- `id` 기반 범위 검색 시나리오를 설명할 수 있는지

### Demo 3. `students`는 수동 `id` 허용

```bash
TMP_DB=$(mktemp -d /tmp/sql-parser-demo-students-XXXXXX)
mkdir -p "$TMP_DB/schema" "$TMP_DB/tables"
cp tests/fixtures/db/schema/students.schema "$TMP_DB/schema/students.schema"
cp tests/fixtures/db/tables/students.csv "$TMP_DB/tables/students.csv"

cat > "$TMP_DB/demo_students.sql" <<'SQL'
INSERT INTO students (id, name, grade, age, region, score)
VALUES (1, 'Kim', 2, 20, 'Seoul', 3.50);

SELECT * FROM students ORDER BY id;
SQL

./sql_processor --sql "$TMP_DB/demo_students.sql" --db "$TMP_DB"
```

확인 포인트:

- `students`는 auto-increment가 아니라 수동 `id` 입력을 허용한다는 점
- `users`와 `students`의 계약이 다르다는 점

### Demo 4. 실패 케이스

```bash
TMP_DB=$(mktemp -d /tmp/sql-parser-demo-fail-XXXXXX)
mkdir -p "$TMP_DB/schema" "$TMP_DB/tables"
cp tests/fixtures/db/schema/users.schema "$TMP_DB/schema/users.schema"
cp tests/fixtures/db/tables/users.csv "$TMP_DB/tables/users.csv"

cat > "$TMP_DB/demo_fail.sql" <<'SQL'
INSERT INTO users (id, name, grade, age, region, score)
VALUES (1, 'BadCase', 1, 19, 'Seoul', 3.00);
SQL

./sql_processor --sql "$TMP_DB/demo_fail.sql" --db "$TMP_DB"
```

기대 메시지:

```text
column 'id' is reserved and cannot be specified manually
```

## Benchmark

Linux/devcontainer 기준:

```bash
./sql_processor --bench 100000 --db ./data
```

처음에는 `100000`처럼 작은 값으로 확인하고, 발표 전 최종 측정만 더 큰 수로 올리는 편이 안전합니다.

## Key Constraints

- 통합 테스트 SQL을 실제 `./data`에 직접 실행하지 않는 편이 안전합니다.
- 시연은 항상 fixture 기반 임시 DB에서 진행합니다.
- `load_students_csv.py`와 `load_students_sql.py`는 목적이 다릅니다.
  - 전자는 빠른 bulk load
  - 후자는 실제 SQL 경로 측정

## Troubleshooting Or Recovery

- `make: command not found`
  - devcontainer/Docker를 쓰거나 PowerShell 스크립트로 우회합니다.
- integration 실패
  - fixture 수정 여부와 expected 차이를 먼저 확인합니다.
- verify 실패
  - 컬럼 수, 타입, PK 중복을 먼저 봅니다.
- 데이터가 꼬였다고 느껴지면
  - `tests/fixtures/db`에서 새 임시 DB를 만들어 다시 시작합니다.

## Follow-up

- 발표용 1페이지 문서가 필요하면 이 문서를 기반으로 체크리스트 버전을 추가하면 됩니다.
- 새 테스트가 들어오면 [tests/README.md](/C:/Users/cutan/Documents/krafton_jungle_git/sql-parser/tests/README.md)와 같이 갱신하는 것이 좋습니다.
