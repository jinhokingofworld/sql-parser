# 작업 메모

## 현재 구현 요약

최근 차수까지 반영된 핵심 사항은 다음과 같습니다.

- `users` 테이블을 6컬럼 스키마로 확장
- `score:float` 지원
- `BETWEEN` 지원
- `DbContext` 도입
- `id` 기준 B+ 트리 인덱스 도입
- auto-increment `id` 지원
- 대량 `INSERT` 시 `RowSet` amortized append 적용
- devcontainer 기준 `make test` 통과

## 최근 해결한 이슈

### 대량 INSERT 복잡도

이전에는 매 INSERT마다 row 배열 전체를 다시 복사할 수 있는 구조라 대량 적재 성능이 크게 저하될 수 있었습니다. 현재는 `row_capacity` 기반 증가 전략을 사용해 이 문제를 완화했습니다.

### devcontainer 테스트 게이트

다음 두 문제를 정리했습니다.

- unit test 경로 조합 경고
- integration expected 파일의 CRLF/LF 차이

## 현재 남아 있는 후속 과제

- benchmark 실행 코드 구현
- 인덱스 무효화 경로 실패 주입 테스트
- 보조 인덱스 필요성 재검토
- 제출/발표용 성능 수치 정리

## 참고 문서

- [README.md](/C:/Users/cutan/Documents/krafton_jungle_git/sql-parser/README.md)
- [architecture.md](/C:/Users/cutan/Documents/krafton_jungle_git/sql-parser/docs/architecture.md)
- [plan_bptree.md](/C:/Users/cutan/Documents/krafton_jungle_git/sql-parser/docs/plan_bptree.md)
