# Data Guide

`data/`는 프로젝트가 실제로 읽는 파일형 DB 루트입니다. `--db <path>`로 전달하는 경로는 항상 이와 같은 구조를 따라야 합니다.

## Purpose

- `schema/`
  - 테이블 정의 파일이 들어 있습니다.
  - 컬럼, PK, auto-increment 여부를 정의합니다.
- `tables/`
  - 실제 row 데이터가 저장된 CSV 파일입니다.
- `generated/`
  - 테스트나 벤치마크용으로 생성한 대용량 CSV를 보관합니다.
- `demo/`
  - `make demo*`가 직접 읽고 쓰는 영속 demo DB 루트입니다.
  - `schema/`와 `tables/`를 함께 가지며, 시연 결과가 누적됩니다.

## Directory Map

```text
data/
├─ schema/       # users.schema, students.schema
├─ tables/       # 실제 DB CSV
├─ generated/    # 대용량 생성 CSV
└─ demo/         # 발표용 영속 demo DB
```

## Current Tables

### `users`

```text
table=users
columns=id:int,name:string,grade:int,age:int,region:string,score:float
pkey=id
autoincrement=true
```

규칙:

- `id`는 reserved 컬럼입니다.
- `INSERT INTO users (...)`에서는 `id`를 직접 넣으면 안 됩니다.
- executor가 `max(id) + 1` 규칙으로 자동 부여합니다.

오류 메시지:

```text
column 'id' is reserved and cannot be specified manually
```

### `students`

```text
table=students
columns=id:int,name:string,grade:int,age:int,region:string,score:float
pkey=id
```

규칙:

- `students`는 auto-increment가 없습니다.
- 수동 `id` 입력을 받는 일반 테이블 예제로 사용합니다.

## Setup Or Usage

기본 실행:

```bash
./sql_processor --sql query.sql --db ./data
```

demo 실행:

```bash
make demo
./sql_demo_helper --sql tests/demo/demo_users.sql --db ./data/demo
```

대용량 생성:

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

## Key Constraints

- `schema/`와 `tables/` 구조는 항상 짝이 맞아야 합니다.
- schema 컬럼 순서와 CSV 컬럼 순서가 다르면 loader와 executor가 실패할 수 있습니다.
- `users`와 `students`는 `id` 처리 규칙이 다릅니다.
- 대형 테스트를 하더라도 원본 `data/`를 직접 바꾸기보다 fixture 복사본이나 임시 DB를 두는 것이 안전합니다.
- `make demo*`는 예외적으로 `data/demo/`를 직접 변경합니다.

## Troubleshooting

- `error: ... schema ... not found`
  - `schema/<table>.schema`가 있는지 확인합니다.
- 조회 결과가 비어 있으면
  - `tables/<table>.csv`가 비어 있는지 먼저 봅니다.
- `users` insert가 실패하면
  - `id`를 수동으로 넣지 않았는지 확인합니다.
- demo 데이터가 꼬였다고 느껴지면
  - `data/demo/tables/*.csv`를 원하는 기준 상태로 직접 되돌린 뒤 다시 시연합니다.
