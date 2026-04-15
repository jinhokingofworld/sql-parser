# 초기 조사 기록

## 문서 목적

이 문서는 프로젝트 초기 조사 단계에서 정리한 핵심 관찰 결과를 현재 상태 기준으로 다시 정리한 요약본입니다. 세부 구현은 이후 여러 차례 변경되었으므로, 본 문서는 출발점과 문제 인식에 초점을 둡니다.

## 당시 확인한 핵심 흐름

초기 코드베이스는 아래 구조를 중심으로 이해했습니다.

- `main.c`
- `cli.c`
- `tokenizer.c`
- `parser.c`
- `executor.c`
- `schema.c`
- `storage.c`

핵심 흐름:

1. CLI 입력 수집
2. SQL 파일 읽기
3. 토큰화
4. AST 생성
5. CSV 기반 실행
6. 결과 출력

## 초기 장점

- 계층이 비교적 분리되어 있음
- tokenizer/parser/executor 구조가 명확함
- 단위 테스트와 통합 테스트 구조가 이미 있음
- schema와 storage가 분리되어 있어 확장 가능성이 있음

## 당시 확인한 한계

### 1. 매번 CSV 전체 재조회

- `SELECT` 실행 때마다 CSV 전체를 읽는 구조
- 데이터가 커질수록 조회 비용이 커짐

### 2. `INSERT` 중복 검사 비용

- PK 중복 확인을 위해 전체 데이터를 다시 확인해야 하는 구조
- 대량 적재에 불리함

### 3. `id` 자동 부여 부재

- 사용자가 `id`를 직접 넣어야 함
- 스키마 차원에서 auto-increment 개념이 없음

### 4. 조건식 범위 제한

- 단일 equality 조건 중심
- 범위 조회 최적화가 어려움

## 후속 설계 방향으로 이어진 판단

초기 조사 결과를 바탕으로 아래 방향이 자연스럽다고 판단했습니다.

- 세션 동안 메모리 상태를 유지하는 `DbContext` 도입
- `id` 컬럼 인덱스 도입
- auto-increment 스키마 속성 추가
- `BETWEEN` 지원
- `float` 타입 지원

이 판단이 이후 B+ 트리 확장 설계로 이어졌습니다.

## 현재 기준 참고

지금 시점의 최신 구조는 아래 문서를 참고하는 것이 가장 정확합니다.

- [README.md](/C:/Users/cutan/Documents/krafton_jungle_git/sql-parser/README.md)
- [architecture.md](/C:/Users/cutan/Documents/krafton_jungle_git/sql-parser/docs/architecture.md)
- [plan_bptree.md](/C:/Users/cutan/Documents/krafton_jungle_git/sql-parser/docs/plan_bptree.md)
