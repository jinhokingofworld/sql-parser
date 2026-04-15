# Tools Guide

`tools/`는 테스트 데이터 생성, 적재, Windows 로컬 빌드/테스트를 도와주는 보조 스크립트 모음입니다.
애플리케이션 본체는 아니지만, 테스트팀 산출물을 재현하고 시연 환경을 준비할 때 매우 중요합니다.

## Purpose

- `generate_students_csv.py`
  - `students`용 row-only CSV를 생성합니다.
- `load_students_csv.py`
  - 생성한 CSV를 `data/tables/students.csv`에 직접 적재합니다.
  - SQL parser/executor 경로를 거치지 않는 빠른 bulk loader입니다.
- `load_students_sql.py`
  - 생성한 CSV를 `INSERT` SQL로 바꿔 `sql_processor`를 통해 적재합니다.
  - 실제 SQL 경로 성능과 동작을 보고 싶을 때 씁니다.
- `build_windows.ps1`
  - Windows PowerShell에서 `sql_processor.exe`와 테스트 바이너리를 빌드합니다.
- `test_windows.ps1`
  - Windows PowerShell에서 unit/integration/verify/bptree-contract를 실행합니다.
- `*.md`
  - 각 스크립트의 개별 사용 예시와 옵션 설명입니다.

## When To Use Which Tool

- 대량 입력 파일을 만들고 싶다
  - `generate_students_csv.py`
- 만든 CSV를 빠르게 `students.csv`로 옮기고 싶다
  - `load_students_csv.py`
- parser/executor/storage/index 경로를 실제로 타는 입력 시간을 보고 싶다
  - `load_students_sql.py`
- Windows에서 `make` 없이 빌드하고 싶다
  - `build_windows.ps1`
- Windows에서 테스트만 빠르게 돌리고 싶다
  - `test_windows.ps1`

## Setup Or Usage

Python 의존성 설치:

```bash
python3 -m pip install -r requirements.txt
```

`make gen-small`, `make verify-small`, `make benchmark-run` 같은 상위 명령과 이 스크립트들의 연결 관계는 [docs/make_and_raw_data_guide.md](/C:/Users/cutan/Documents/krafton_jungle_git/sql-parser/docs/make_and_raw_data_guide.md)에 정리해 두었습니다.

### 1. 샘플 데이터 생성

```bash
python3 tools/generate_students_csv.py \
  --rows 10 \
  --output data/generated/students_sample10.csv \
  --seed 42
```

### 2. 빠른 Direct Load

```bash
python3 tools/load_students_csv.py \
  --input data/generated/students_sample10.csv \
  --db-root data \
  --table students \
  --truncate
```

### 3. SQL 경로로 적재

```bash
python3 tools/load_students_sql.py \
  --input data/generated/students_sample10.csv \
  --db-root data \
  --table students \
  --truncate \
  --batch-size 10
```

### 4. Windows에서 빌드

```powershell
.\tools\build_windows.ps1 -Target tests
```

### 5. Windows에서 테스트

```powershell
.\tools\test_windows.ps1 -Suite all
```

## Key Constraints

- `load_students_csv.py`는 빠르지만 SQL parser/executor 경로를 타지 않습니다.
- `load_students_sql.py`는 느리지만 실제 제품 경로를 검증하는 데 의미가 있습니다.
- `load_students_sql.py`를 쓰려면 먼저 `sql_processor`가 빌드되어 있어야 합니다.
- `students` 적재 도구는 `data/schema/students.schema`를 기준으로 입력을 검증합니다.
- PowerShell 스크립트는 `gcc`가 PATH에 있어야 동작합니다.

## Recommended Workflows

### 데이터셋 검증용

```bash
python3 tools/generate_students_csv.py \
  --rows 1000 \
  --output data/generated/students_1k.csv \
  --seed 42

python3 tools/load_students_csv.py \
  --input data/generated/students_1k.csv \
  --db-root data \
  --table students \
  --truncate
```

### SQL 경로 성능 비교용

```bash
make

python3 tools/generate_students_csv.py \
  --rows 1000 \
  --output data/generated/students_1k.csv \
  --seed 42

python3 tools/load_students_sql.py \
  --input data/generated/students_1k.csv \
  --db-root data \
  --table students \
  --truncate \
  --batch-size 100
```

### Windows 로컬 회귀 확인용

```powershell
.\tools\build_windows.ps1 -Target tests
.\tools\test_windows.ps1 -Suite all
```

## Troubleshooting Or Recovery

- `Faker is not installed`
  - `python3 -m pip install -r requirements.txt`
- `sql_processor` not found
  - 먼저 `make` 또는 `.\tools\build_windows.ps1 -Target sql_processor`를 실행합니다.
- `duplicate primary key`
  - 같은 CSV를 `append` 모드로 여러 번 넣었는지 확인합니다.
- `schema/table mismatch`
  - `--table` 값과 `schema/<table>.schema` 파일명을 다시 확인합니다.

## Follow-up

- 새 loader를 추가하면 direct path인지 SQL path인지 먼저 문서에 명시하는 것이 좋습니다.
- 팀 내 온보딩용으로는 이 문서와 [tests/README.md](/C:/Users/cutan/Documents/krafton_jungle_git/sql-parser/tests/README.md), [data/README.md](/C:/Users/cutan/Documents/krafton_jungle_git/sql-parser/data/README.md)를 함께 보는 것이 가장 빠릅니다.
