# Testing And Demo Guide

이 문서는 `sql-parser`를 처음 받는 팀원이 테스트 흐름과 발표용 demo 흐름을 빠르게 파악할 수 있도록 정리한 안내서입니다.

## Purpose

- 어떤 테스트를 언제 돌려야 하는지 한 번에 파악합니다.
- `make integration`과 `make demo`의 역할 차이를 분명히 구분합니다.
- 영속 demo DB인 `./data/demo`를 어떻게 다뤄야 하는지 설명합니다.

## Recommended Environment

권장 환경:

- devcontainer
- Docker 컨테이너
- Linux / WSL

이유:

- `make integration`은 `mktemp`, `cp`, `diff` 같은 POSIX 명령을 사용합니다.
- `make demo`도 `sh tools/run_demo.sh`를 사용하므로 POSIX 계열 환경이 가장 안전합니다.

## Test Map

- `make unit`
  - tokenizer, parser, storage, executor 단위 동작 확인
- `make bptree-contract`
  - B+ 트리 API 계약 확인
- `make integration`
  - fixture 기반 임시 DB에서 end-to-end regression 확인
- `make test`
  - `unit`, `bptree-contract`, `integration` 묶음 실행

## Demo Map

`make demo*`는 발표용 시연 진입점입니다.

- `make demo`
  - `users`, `range`, `fail` 시나리오를 순서대로 실행
- `make demo-users`
  - auto-increment INSERT와 인덱스 단건 조회, 선형 탐색 비교
- `make demo-range`
  - `WHERE id BETWEEN ...` 인덱스 범위 조회
- `make demo-fail`
  - reserved `id` 수동 입력 실패 메시지 확인

## Demo DB Policy

demo는 fixture 임시 복사본이 아니라 영속 demo DB `./data/demo`를 직접 사용합니다.

- demo 실행 결과는 `data/demo/tables/*.csv`에 그대로 누적됩니다.
- `make demo*`는 실행 전에 reset 하지 않습니다.
- 시연 기준 데이터를 바꾸고 싶으면 `data/demo/schema/`, `data/demo/tables/` 파일을 직접 수정합니다.

기본 예시 데이터:

- `data/demo/schema/users.schema`
- `data/demo/tables/users.csv`

## Demo Output Format

demo helper는 SQL 파일을 statement 단위로 나눠 아래 형식으로 출력합니다.

1. `쿼리`
2. `결과`
3. `소요시간`
4. `탐색 지표` (`SELECT`만)

다중 statement SQL 파일은 각 쿼리 블록 사이에 구분선을 출력합니다.

`탐색 지표` 필드:

- `탐색 방식`
- `전체 행 수`
- `행 탐색 수`
- `인덱스 탐색 단계 수`
- `결과 행 수`

## Demo Scenarios

### Demo 1. Auto-Increment And Query Path Comparison

```bash
make demo-users
```

확인 포인트:

- `INSERT`가 `id` 없이 성공하는지
- `WHERE id = 2`는 `탐색 방식: 인덱스 단건 조회`로 보이는지
- `WHERE name = 'Bob'`은 `탐색 방식: 선형 탐색`으로 보이는지
- `행 탐색 수`와 `인덱스 탐색 단계 수` 차이를 설명할 수 있는지

### Demo 2. Indexed Range Lookup

```bash
make demo-range
```

확인 포인트:

- `탐색 방식: 인덱스 범위 조회`가 보이는지
- `행 탐색 수`가 결과 행 수와 가깝게 나오는지

### Demo 3. Reserved `id` Failure

```bash
make demo-fail
```

기대 메시지:

```text
column 'id' is reserved and cannot be specified manually
```

## Manual Commands

테스트:

```bash
make clean
make
make unit
make bptree-contract
make integration
make test
```

demo:

```bash
make demo
./sql_demo_helper --sql tests/demo/demo_users.sql --db ./data/demo
```

## Key Constraints

- `make integration`은 개발자용 regression 경로입니다.
- `make demo`는 발표용 영속 demo 경로입니다.
- 통합 테스트 SQL은 실제 `./data`에 직접 실행하지 않는 편이 안전합니다.
- demo는 예외적으로 `./data/demo`를 직접 바꾸는 것이 정상 동작입니다.

## Troubleshooting

- `make: command not found`
  - devcontainer/Docker를 쓰거나 Windows 스크립트로 우회합니다.
- integration 실패
  - fixture와 expected 차이를 먼저 확인합니다.
- demo 결과가 예상과 다르면
  - `data/demo/tables/*.csv`의 현재 상태를 먼저 확인합니다.
- demo 데이터를 다시 맞추고 싶으면
  - `data/demo/tables/*.csv`를 원하는 기준 상태로 직접 되돌립니다.
