# Data Guide

`data/`는 이 프로젝트가 실제로 읽는 파일형 DB 루트입니다.
실행 시 `--db <path>`로 전달하는 디렉터리도 같은 구조를 따라야 합니다.

## Purpose

- `schema/`
  - 테이블 정의 파일이 들어 있습니다.
  - 각 테이블의 컬럼, 타입, 기본키, auto-increment 여부를 정의합니다.
- `tables/`
  - 실제 row 데이터가 저장되는 CSV 파일입니다.
  - SQL 실행 결과가 여기에 반영됩니다.
- `generated/`
  - 테스트와 벤치마크용으로 생성한 대량 CSV를 보관합니다.
  - 원본 테이블이라기보다 입력 소스 저장소에 가깝습니다.

## Directory Map

```text
data/
├─ schema/       # users.schema, students.schema
├─ tables/       # 실제 DB CSV
└─ generated/    # 대량 생성 CSV
```

## Current Tables

### `users`

```text
table=users
columns=id:int,name:string,grade:int,age:int,region:string,score:float
pkey=id
autoincrement=true
```

의미:

- `id`는 예약 컬럼입니다.
- `INSERT INTO users (...)`에서는 `id`를 직접 넣으면 안 됩니다.
- executor가 `max(id) + 1` 규칙으로 자동 부여합니다.

직접 `id`를 넣으면 아래 오류가 나야 합니다.

```text
column 'id' is reserved and cannot be specified manually
```

### `students`

```text
table=students
columns=id:int,name:string,grade:int,age:int,region:string,score:float
pkey=id
```

의미:

- `students`는 auto-increment가 없습니다.
- `id`를 직접 넣는 보조 테이블입니다.
- 대량 데이터 생성, 적재, verify, 벤치마크 입력 용도로 많이 사용합니다.

## Setup Or Usage

기본 실행:

```bash
./sql_processor --sql query.sql --db ./data
```

예시 쿼리:

```sql
INSERT INTO users (name, grade, age, region, score)
VALUES ('Charlie', 1, 19, 'Incheon', 3.25);

SELECT id, name FROM users WHERE id BETWEEN 1 AND 10 ORDER BY id;
```

`generated/`를 이용한 데이터 준비:

```bash
python3 tools/generate_students_csv.py \
  --rows 1000 \
  --output data/generated/students_1k.csv \
  --seed 42
```

직접 적재:

```bash
python3 tools/load_students_csv.py \
  --input data/generated/students_1k.csv \
  --db-root data \
  --table students \
  --truncate
```

전체 `make` 타깃과 raw data 준비 순서는 [docs/make_and_raw_data_guide.md](/C:/Users/cutan/Documents/krafton_jungle_git/sql-parser/docs/make_and_raw_data_guide.md)에 따로 정리되어 있습니다.

## Key Constraints

- `schema/`와 `tables/` 구조는 항상 짝이 맞아야 합니다.
- schema 컬럼 순서와 CSV 컬럼 순서가 다르면 loader와 executor가 실패할 수 있습니다.
- `users`와 `students`는 `id` 처리 규칙이 다릅니다.
- 대량 테스트를 하더라도 원본 `data/`를 직접 바꾸기보다 fixture 복사본이나 임시 DB를 쓰는 것이 안전합니다.

## What Is In `generated/`

- `students_1k.csv`
  - 빠른 검증용 샘플
- `students_1m.csv`
  - 대량 적재와 벤치마크용 입력

이 파일들은 `sql_processor`가 직접 읽는 테이블이 아니라, loader가 `data/tables/students.csv`로 옮기는 입력 원본입니다.

## Troubleshooting Or Recovery

- `error: ... schema ... not found`
  - `schema/<table>.schema`가 있는지 확인합니다.
- 조회 결과가 비어 있으면
  - `tables/<table>.csv`가 비어 있는지 먼저 봅니다.
- `users` insert가 실패하면
  - `id`를 수동으로 넣지 않았는지 확인합니다.
- 데이터가 섞였으면
  - `tests/fixtures/db`를 복사해 새 임시 DB를 만들어 다시 시연합니다.

## Follow-up

- 새 테이블을 추가하면 `schema/`와 `tables/`를 동시에 정의하고, 테스트 fixture와 문서도 같이 추가하는 것이 좋습니다.
- `generated/` 파일이 커질 수 있으니, 샘플과 대용량 파일의 용도를 문서에 계속 유지하는 편이 좋습니다.
