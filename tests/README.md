# Tests Guide

`tests/`는 이 저장소의 동작 보증, 회귀 방지, 시연용 fixture를 모아둔 폴더입니다.
테스트팀이 만든 산출물이 섞여 있어서 처음 보면 복잡하지만, 실제로는 아래처럼 역할이 나뉩니다.

## Purpose

- `unit/`
  - tokenizer, parser, storage, executor 같은 핵심 모듈을 함수 단위로 검증합니다.
  - 빠르게 실패 원인을 좁히고 싶을 때 가장 먼저 보는 영역입니다.
- `integration/`
  - `sql_processor`를 실제 CLI처럼 실행해서 SQL 입력과 출력이 기대값과 같은지 확인합니다.
  - parser -> executor -> storage 흐름을 한 번에 검증합니다.
- `verify/`
  - `students` 데이터셋이 schema 계약을 만족하는지 점검합니다.
  - 대량 CSV 생성 뒤 품질 검증용으로 사용합니다.
- `bench/`
  - 성능 비교용 벤치마크 실행 파일과 소스가 들어 있습니다.
  - 기능 정합성보다 `id` 인덱스 경로와 일반 조건 경로의 차이를 보는 용도입니다.
- `fixtures/`
  - 테스트와 시연에서 복사해서 쓰는 고정 입력 데이터입니다.
  - 실제 `data/`를 오염시키지 않기 위해 이 디렉터리를 기준으로 임시 DB를 만듭니다.
- `support/`
  - Unity, helper 함수, 테스트용 adapter 같은 공용 지원 코드입니다.
- `tmp/`
  - 테스트 중 생성된 임시 산출물입니다.
  - 필요 없으면 지워도 됩니다.

## Directory Map

```text
tests/
├─ unit/          # 모듈 단위 테스트
├─ integration/   # SQL 파일 + expected 출력 비교
├─ verify/        # students CSV 계약 검증
├─ bench/         # 성능 측정 실행 파일/소스
├─ fixtures/      # 테스트/시연용 고정 DB
├─ support/       # 공통 helper, Unity, adapter
└─ tmp/           # 테스트 실행 중 생성되는 임시 파일
```

## Recommended Order

처음 파악할 때는 아래 순서가 가장 이해하기 쉽습니다.

1. `tests/fixtures/db/`
2. `tests/integration/*.sql`
3. `tests/integration/*.expected`
4. `tests/unit/*.c`
5. `tests/verify/verify_students_dataset.c`
6. `tests/bench/benchmark_query_paths.c`

## Setup Or Usage

권장 환경은 Linux, WSL, devcontainer, Docker 컨테이너입니다.
이유는 `Makefile`의 `integration` 타깃이 `mktemp`, `cp`, `diff` 같은 POSIX 명령을 쓰기 때문입니다.

### Linux Or Devcontainer

```bash
make clean
make
make unit
make bptree-contract
make integration
make test
```

자주 쓰는 타깃:

- `make unit`
- `make bptree-contract`
- `make integration`
- `make test`
- `make benchmark-run`

### Windows PowerShell

PowerShell에서는 `make` 대신 `tools/build_windows.ps1`, `tools/test_windows.ps1`를 쓰는 편이 안전합니다.

```powershell
.\tools\build_windows.ps1 -Target tests
.\tools\test_windows.ps1 -Suite all
```

개별 실행:

```powershell
.\tools\test_windows.ps1 -Suite unit
.\tools\test_windows.ps1 -Suite integration
.\tools\test_windows.ps1 -Suite verify
.\tools\test_windows.ps1 -Suite bptree-contract
```

## Key Constraints

- `tests/integration/*.sql`은 그대로 `./data`에 실행하지 않는 편이 안전합니다.
  - 일부 테스트는 `INSERT`를 포함하므로 실제 CSV 상태를 바꿀 수 있습니다.
- 통합 테스트와 시연은 `tests/fixtures/db`를 임시 디렉터리로 복사해서 돌리는 방식이 권장됩니다.
- `tests/tmp`는 장기 보관 대상이 아닙니다.
- `tests/bench/benchmark_query_paths.c`는 정답 비교 테스트가 아니라 성능 관측용입니다.

## Demo Entry Points

시연은 아래 fixture를 복사해서 시작하면 가장 안정적입니다.

- `tests/fixtures/db/schema/users.schema`
- `tests/fixtures/db/tables/users.csv`
- `tests/fixtures/db/schema/students.schema`
- `tests/fixtures/db/tables/students.csv`

시연 절차는 [docs/testing_and_demo.md](/C:/Users/cutan/Documents/krafton_jungle_git/sql-parser/docs/testing_and_demo.md)에 정리해 두었습니다.
`make` 타깃 설명과 raw data 준비 흐름은 [docs/make_and_raw_data_guide.md](/C:/Users/cutan/Documents/krafton_jungle_git/sql-parser/docs/make_and_raw_data_guide.md)를 함께 보면 더 빠릅니다.

## Troubleshooting Or Recovery

- `make`가 없으면:
  - devcontainer 또는 Docker 안에서 실행하거나
  - Windows에서는 `tools/build_windows.ps1`, `tools/test_windows.ps1`를 사용합니다.
- `integration`이 실패하면:
  - expected와 actual 차이인지
  - fixture DB를 직접 수정했는지
  - 줄바꿈 차이인지 먼저 확인합니다.
- verify 실패가 나면:
  - `students` CSV 컬럼 수
  - `id` 중복
  - `age`, `score` 타입 오류를 먼저 확인합니다.

## Follow-up

- 테스트 추가 시에는 어떤 계층을 검증하는지 먼저 정하고 해당 디렉터리에 넣습니다.
- 새로운 시연 시나리오를 추가하면 [docs/testing_and_demo.md](/C:/Users/cutan/Documents/krafton_jungle_git/sql-parser/docs/testing_and_demo.md)에도 같이 반영하는 것이 좋습니다.
