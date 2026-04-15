# Make And Raw Data Guide

이 문서는 팀에서 전달받은 `make` 명령과 `/tools` 기반 raw data 준비 흐름을 처음 보는 사람 기준으로 설명합니다.
핵심은 "이 명령이 무엇을 하는지", "언제 써야 하는지", "어떤 파일이 생기거나 바뀌는지"를 빠르게 알 수 있게 하는 것입니다.

## Purpose

- `Makefile` 타깃의 의미를 이해한다.
- 테스트 실행 순서와 용도를 구분한다.
- `tools/` 스크립트를 써서 raw data를 생성하고 적재하는 흐름을 이해한다.
- 실수로 실제 DB를 오염시키지 않도록 안전한 사용법을 안내한다.

## Prerequisites

권장 환경:

- devcontainer
- Docker 컨테이너
- Linux / WSL

필수 도구:

- `cc` 또는 `gcc`
- `make`
- `python3`
- Python 패키지: `Faker>=40.13.0,<41`

설치:

```bash
python3 -m pip install -r requirements.txt
```

Windows PowerShell만 쓰는 경우에는 `make` 대신 아래 스크립트를 사용하는 편이 안전합니다.

```powershell
.\tools\build_windows.ps1 -Target tests
.\tools\test_windows.ps1 -Suite all
```

## Makefile At A Glance

`Makefile`은 크게 네 가지 역할을 합니다.

1. 바이너리 빌드
2. 테스트 실행
3. raw data 생성과 검증
4. 벤치마크 실행

## Core Build Targets

### `make`

가장 먼저 시도하는 기본 명령입니다.

```bash
make
```

현재 `Makefile` 구조상 `all` 타깃이 중복 선언되어 있어서, 실제로는 아래를 함께 수행하는 현재 동작으로 이해하는 편이 안전합니다.

- `sql_processor` 빌드
- `integration` 실행

즉, 단순 빌드라고 생각했는데 통합 테스트까지 같이 돌 수 있습니다.

### `make clean`

빌드 산출물을 지웁니다.

```bash
make clean
```

삭제 대상:

- `sql_processor`
- unit test 바이너리
- B+ tree contract test 바이너리
- verify 바이너리
- benchmark 바이너리

주의:

- `data/generated/*.csv`는 지우지 않습니다.
- `data/tables/*.csv`도 지우지 않습니다.

## Test Targets

### `make unit`

모듈 단위 테스트를 실행합니다.

```bash
make unit
```

실행 대상:

- `tests/unit/test_tokenizer`
- `tests/unit/test_parser`
- `tests/unit/test_storage`
- `tests/unit/test_executor`

### `make bptree-contract`

B+ 트리 계약 테스트를 실행합니다.

```bash
make bptree-contract
```

### `make integration`

CLI 실행 흐름을 end-to-end로 검증합니다.

```bash
make integration
```

무엇을 하나:

- `tests/fixtures/db` 기반 임시 DB를 만듭니다.
- 각 `tests/integration/*.sql`을 `./sql_processor --sql ... --db <tmp>`로 실행합니다.
- 결과를 `tests/integration/*.expected`와 비교합니다.

주의:

- `mktemp`, `cp`, `diff`에 의존하므로 POSIX 환경에서 돌리는 것이 안전합니다.

### `make test`

대표 회귀 테스트 묶음입니다.

```bash
make test
```

포함:

- `unit`
- `bptree-contract`
- `integration`

### `make demo`

발표용 시연 흐름입니다.

```bash
make demo
```

무엇을 하느냐:

- 발표용 SQL fixture를 순서대로 실행합니다.
- 영속 demo DB인 `./data/demo`를 직접 사용합니다.
- 각 statement마다 `쿼리`, `결과`, `소요시간`을 출력합니다.
- `SELECT`는 `탐색 방식`, `전체 행 수`, `행 탐색 수`, `인덱스 탐색 단계 수`, `결과 행 수`를 함께 보여줍니다.
- 다중 statement SQL 파일은 쿼리 블록 사이에 구분선을 출력합니다.

개별 시나리오:

```bash
make demo-users
make demo-range
make demo-fail
```

구분:

- `make integration`
  - 개발자용 regression 검증
- `make demo`
  - 발표용 terminal demo

주의:

- `make demo*`는 `./data/demo`를 직접 변경하므로 결과가 누적됩니다.
- integration처럼 임시 fixture DB를 만들지 않습니다.

## Data Generation And Verification Targets

### `make gen-small`

작은 샘플 raw data를 생성합니다.

```bash
make gen-small
```

기본 출력:

- `data/generated/students_1k.csv`

### `make gen-large`

대용량 raw data를 생성합니다.

```bash
make gen-large
```

기본 출력:

- `data/generated/students_1m.csv`

### `make verify-small`

작은 raw data를 생성한 뒤 verify 바이너리로 검증합니다.

```bash
make verify-small
```

### `make verify-large`

대용량 raw data 생성 후 검증합니다.

```bash
make verify-large
```

### 실패 fixture 검증 타깃

이 타깃들은 "실패해야 정상"인 테스트입니다.

```bash
make verify-fail-id
make verify-fail-age
make verify-fail-score
make verify-fail-columns
make verify-fail-count
```

## Benchmark Targets

### `make benchmark`

벤치마크 바이너리만 빌드합니다.

```bash
make benchmark
```

### `make benchmark-run`

벤치마크 바이너리를 실행합니다.

```bash
make benchmark-run
```

기본 파라미터:

- `BENCH_ROWS=10000`
- `BENCH_ITERATIONS=200`

## Short Alias Targets

짧은 별칭도 있습니다.

```text
t      -> test
u      -> unit
bt     -> bptree-contract
csd    -> gen-small
cld    -> gen-large
psd    -> verify-small
pld    -> verify-large
vfid   -> verify-fail-id
vfage  -> verify-fail-age
vfsco  -> verify-fail-score
vfcol  -> verify-fail-columns
vfcnt  -> verify-fail-count
bm     -> benchmark-run
```

## Overriding Variables

`Makefile` 기본값은 실행 시 덮어쓸 수 있습니다.

예시:

```bash
make gen-small SMALL_ROWS=50 SMALL_OUTPUT=data/generated/students_50.csv DATA_SEED=7
make benchmark-run BENCH_ROWS=50000 BENCH_ITERATIONS=100
```

## Raw Data Workflow

raw data 준비는 보통 아래 순서로 이해하면 됩니다.

1. 생성: `generate_students_csv.py`
2. 검증: `verify_students_dataset`
3. 적재: direct load 또는 SQL load
4. 조회/벤치마크: `sql_processor` 실행

### Step 1. Raw Data 생성

가장 작은 예:

```bash
python3 tools/generate_students_csv.py \
  --rows 10 \
  --output data/generated/students_sample10.csv \
  --seed 42
```

기본 대용량 예:

```bash
python3 tools/generate_students_csv.py \
  --rows 1000000 \
  --output data/generated/students_1m.csv \
  --seed 42 \
  --start-id 1
```

주의:

- header 없는 row-only CSV입니다.
- 바로 `sql_processor`가 읽는 테이블 파일이 아니라 loader 입력용 원본입니다.

### Step 2. Raw Data 검증

```bash
./tests/verify/verify_students_dataset data/generated/students_1k.csv 1000 1
```

또는:

```bash
make verify-small
```

### Step 3-A. Direct Load

가장 빠른 적재 방법입니다.

```bash
python3 tools/load_students_csv.py \
  --input data/generated/students_1k.csv \
  --db-root data \
  --table students \
  --truncate
```

결과:

- `data/tables/students.csv`가 갱신됩니다.

### Step 3-B. SQL Load

실제 SQL 경로를 타는 적재입니다.

```bash
make

python3 tools/load_students_sql.py \
  --input data/generated/students_1k.csv \
  --db-root data \
  --table students \
  --truncate \
  --batch-size 100
```

주의:

- direct load보다 훨씬 느릴 수 있습니다.
- `sql_processor` 바이너리가 먼저 있어야 합니다.

## Recommended Recipes

### Recipe 1. 가장 안전한 1k 데이터셋 준비

```bash
python3 -m pip install -r requirements.txt
make verify
make gen-small
make verify-small
python3 tools/load_students_csv.py --input data/generated/students_1k.csv --db-root data --table students --truncate
```

### Recipe 2. SQL 경로 실험용 1k 입력

```bash
python3 -m pip install -r requirements.txt
make
make gen-small
make verify-small
python3 tools/load_students_sql.py --input data/generated/students_1k.csv --db-root data --table students --truncate --batch-size 100
```

### Recipe 3. 벤치마크용 대용량 준비

```bash
python3 -m pip install -r requirements.txt
make gen-large
make verify-large
python3 tools/load_students_csv.py --input data/generated/students_1m.csv --db-root data --table students --truncate --trust-sequential-pk
```

## Output Files And Side Effects

읽기만 하는 명령:

- `make unit`
- `make bptree-contract`
- `make integration`
- `make test`
- `make demo`
- `make benchmark-run`

파일을 생성하거나 바꾸는 명령:

- `make gen-small`
  - `data/generated/students_1k.csv`
- `make gen-large`
  - `data/generated/students_1m.csv`
- `python3 tools/load_students_csv.py ...`
  - `data/tables/students.csv`
- `python3 tools/load_students_sql.py ...`
  - `data/tables/students.csv`

## Troubleshooting Or Recovery

- `make: command not found`
  - devcontainer/Docker/WSL로 옮기거나 PowerShell 스크립트를 사용합니다.
- `python3: No module named faker`
  - `python3 -m pip install -r requirements.txt`
- `verify` 실패
  - 입력 row 수, 타입, id 중복을 먼저 확인합니다.
- `load_students_sql.py`가 느리다
  - 정상일 수 있습니다. SQL 경로를 거치기 때문입니다.

## Related Docs

- [tests/README.md](/C:/Users/cutan/Documents/krafton_jungle_git/sql-parser/tests/README.md)
- [data/README.md](/C:/Users/cutan/Documents/krafton_jungle_git/sql-parser/data/README.md)
- [tools/README.md](/C:/Users/cutan/Documents/krafton_jungle_git/sql-parser/tools/README.md)
- [docs/testing_and_demo.md](/C:/Users/cutan/Documents/krafton_jungle_git/sql-parser/docs/testing_and_demo.md)
