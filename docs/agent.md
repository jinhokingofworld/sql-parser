# AI 에이전트 작업 가이드

## 목적

이 문서는 `sql-parser` 저장소에서 AI 에이전트가 작업할 때 따라야 하는 기본 원칙을 정리한 운영 가이드입니다. 목표는 단순히 코드를 생성하는 것이 아니라, 구현 근거와 검증 결과가 남는 방식으로 작업하는 것입니다.

## 기본 원칙

### 1. 코드보다 이해가 먼저

- 관련 파일과 실행 흐름을 먼저 파악합니다.
- 문서와 코드가 다르면 코드를 우선 source of truth로 봅니다.
- 추정으로 인터페이스를 만들기보다 실제 구현을 확인합니다.

### 2. 작업 순서를 지킵니다

권장 순서:

1. 조사
2. 계획
3. 구현
4. 검증
5. 문서화
6. 피드백 정리

### 3. 변경 이유를 남깁니다

- 왜 바꿨는지
- 어떤 대안을 버렸는지
- 어떤 리스크가 남는지
- 어떤 테스트로 확인했는지

이 네 가지는 가능한 한 문서나 handoff에 남깁니다.

## 조사 단계 체크리스트

다음 항목을 먼저 확인합니다.

- 입력 경로: `main.c`, `cli.c`
- SQL 파싱 경로: `tokenizer.c`, `parser.c`
- 실행 경로: `executor.c`
- 스키마/스토리지 경로: `schema.c`, `storage.c`
- 테스트 구조: `tests/unit`, `tests/integration`
- 문서 구조: `README.md`, `docs/*.md`

특히 아래를 빠뜨리지 않습니다.

- 현재 빌드 명령
- 현재 테스트 명령
- 운영 데이터 경로
- fixture 경로
- 최근 변경된 설계 제약

## 구현 단계 체크리스트

### 기능 변경 시

- 입력 계약이 바뀌는지 확인
- 기존 테스트가 깨지는지 확인
- 문서와 fixture를 함께 갱신

### 성능 변경 시

- 시간 복잡도 변화를 설명
- 기존 병목이 정말 제거됐는지 검증
- 정확성과 성능 중 어떤 선택을 했는지 명시

### 파일 저장 변경 시

- append 순서
- 실패 시 복구/무효화 전략
- 메모리와 디스크의 source of truth

이 세 가지를 반드시 정리합니다.

## 검증 기준

최소한 아래를 남기는 것을 권장합니다.

- 실행한 빌드 명령
- 실행한 테스트 명령
- 통과/실패 여부
- 실패했다면 환경 문제인지 로직 문제인지 구분

문서 작업만 한 경우에도 다음은 확인합니다.

- 현재 구현과 설명이 일치하는지
- 더 이상 유효하지 않은 예시가 없는지
- 깨진 문자 인코딩이나 링크가 없는지

## 이 저장소에서 특히 중요한 규칙

- `DbContext`는 `main.c`에서만 생성합니다.
- `id`는 auto-increment 예약 컬럼입니다.
- B+ 트리는 메모리 전용입니다.
- 벤치마크 코드는 아직 후속 범위입니다.
- devcontainer 기준 `make test` 통과가 현재 품질 게이트입니다.

## 문서 우선순위

현재 기준 권장 참조 순서는 다음과 같습니다.

1. [README.md](/C:/Users/cutan/Documents/krafton_jungle_git/sql-parser/README.md)
2. [docs/architecture.md](/C:/Users/cutan/Documents/krafton_jungle_git/sql-parser/docs/architecture.md)
3. [docs/plan_bptree.md](/C:/Users/cutan/Documents/krafton_jungle_git/sql-parser/docs/plan_bptree.md)
4. [docs/release_plan.md](/C:/Users/cutan/Documents/krafton_jungle_git/sql-parser/docs/release_plan.md)

## 후속 권장 사항

- 큰 설계 변경이 생기면 먼저 계획 문서를 갱신합니다.
- 구현 후에는 README와 아키텍처 문서를 최소 한 번 교차 검토합니다.
- 제출 직전에는 release plan과 docs index를 기준으로 누락 문서가 없는지 확인합니다.
