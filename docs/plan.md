# 초기 구현 계획

## 문서 목적

이 문서는 프로젝트 초기에 정의했던 기본 SQL 처리기 구현 계획을 정리한 기록입니다. 현재 최신 인덱스 확장 설계는 [plan_bptree.md](/C:/Users/cutan/Documents/krafton_jungle_git/sql-parser/docs/plan_bptree.md)를 기준으로 보아야 하며, 본 문서는 초기 범위와 설계 의도를 이해하기 위한 참고 자료입니다.

## 초기 목표

초기 차수의 목표는 다음과 같았습니다.

- SQL 파일을 읽어 실행
- `INSERT`
- `SELECT`
- 파일 기반 테이블 저장
- 단위 테스트와 통합 테스트 구성

초기 범위에서는 다음 항목은 제외했습니다.

- `UPDATE`, `DELETE`
- `JOIN`
- 복잡한 집계
- 인덱스

## 초기 실행 흐름

초기 버전은 아래 흐름을 전제로 설계되었습니다.

1. CLI에서 SQL 파일 경로를 받음
2. SQL 문자열을 읽음
3. tokenizer가 토큰화
4. parser가 AST 생성
5. executor가 스키마와 CSV를 읽어 실행
6. 결과를 stdout에 출력

## 초기 SQL 범위

### INSERT

```sql
INSERT INTO users (id, name, age) VALUES (1, 'Alice', 20);
```

### SELECT

```sql
SELECT * FROM users;
SELECT id, name FROM users;
SELECT * FROM users WHERE id = 1;
SELECT * FROM users ORDER BY id;
```

초기 `WHERE`는 `column = value` 단일 조건만 고려했습니다.

## 초기 설계 포인트

### AST 분리

- `InsertQuery`
- `SelectQuery`
- `Condition`
- `OrderByClause`

이 구조를 통해 parser와 executor의 책임을 분리하는 것을 목표로 했습니다.

### tokenizer 도입

단순 문자열 분할 대신 tokenizer를 사용해 다음 문제를 피하려 했습니다.

- 문자열 literal 내부 공백
- 괄호, 쉼표, 세미콜론 구분
- 더 나은 오류 메시지

### storage 분리

CSV 읽기/쓰기를 executor에서 분리해, 이후 기능 확장 시 storage 계층을 독립적으로 개선할 수 있도록 설계했습니다.

## 현재 상태와의 차이

현재 구현은 초기 계획보다 다음이 추가되었습니다.

- `DbContext`
- B+ 트리 인덱스
- auto-increment `id`
- `BETWEEN`
- `float` 타입
- 메모리 상 `RowSet` 유지

따라서 최신 동작 설명은 반드시 README와 아키텍처 문서를 기준으로 확인해야 합니다.

## 문서 상태

이 문서는 역사적 맥락을 보존하기 위한 요약본입니다. 세부 구현 계획이 필요하면 다음 문서를 참고합니다.

- [README.md](/C:/Users/cutan/Documents/krafton_jungle_git/sql-parser/README.md)
- [architecture.md](/C:/Users/cutan/Documents/krafton_jungle_git/sql-parser/docs/architecture.md)
- [plan_bptree.md](/C:/Users/cutan/Documents/krafton_jungle_git/sql-parser/docs/plan_bptree.md)
