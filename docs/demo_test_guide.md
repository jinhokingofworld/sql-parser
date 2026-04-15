# 시연용 테스트 가이드

## 목적

이 문서는 발표나 데모에서 `sql-parser`의 현재 구현 범위를 안정적으로 보여주기 위한 테스트 절차를 정리한 가이드입니다.

시연 포인트는 세 가지입니다.

1. `users` 테이블의 auto-increment 동작
2. `WHERE id = ?`, `WHERE id BETWEEN ? AND ?` 인덱스 조회
3. `students` 테이블의 수동 `id` 입력 동작

## 시연 전 준비

권장 환경:

- devcontainer
- 또는 Linux/WSL + `make` 가능 환경

먼저 전체 테스트를 확인합니다.

```bash
make test
```

## 저장소 계약 요약

### `users`

- 6컬럼
- `autoincrement=true`
- `id` 직접 입력 금지

### `students`

- 6컬럼
- 수동 `id` 입력 허용
- 테스트팀 전달 형식을 반영한 보조 테스트 테이블

## 데모 1: `users` auto-increment

임시 DB를 만들어 깨끗한 상태에서 시작합니다.

```bash
TMP_DB=$(mktemp -d /tmp/sql-parser-demo-users-XXXXXX)
mkdir -p "$TMP_DB/schema" "$TMP_DB/tables"
cp data/schema/users.schema "$TMP_DB/schema/users.schema"
cp data/tables/users.csv "$TMP_DB/tables/users.csv"
```

SQL 파일:

```sql
INSERT INTO users (name, grade, age, region, score)
VALUES ('Alice', 2, 20, 'Seoul', 3.80);

INSERT INTO users (name, grade, age, region, score)
VALUES ('Bob', 4, 25, 'Busan', 4.10);

SELECT * FROM users ORDER BY id;
```

확인 포인트:

- `id`가 1, 2로 자동 부여되는지
- 결과 테이블이 `id` 오름차순으로 출력되는지

## 데모 2: `WHERE id BETWEEN`

같은 임시 DB 또는 새 임시 DB에서 아래 SQL을 실행합니다.

```sql
INSERT INTO users (name, grade, age, region, score)
VALUES ('Alice', 2, 20, 'Seoul', 3.80);
INSERT INTO users (name, grade, age, region, score)
VALUES ('Bob', 4, 25, 'Busan', 4.10);
INSERT INTO users (name, grade, age, region, score)
VALUES ('Charlie', 1, 19, 'Incheon', 3.25);

SELECT name FROM users WHERE id BETWEEN 2 AND 3 ORDER BY id;
```

확인 포인트:

- `Bob`, `Charlie`만 나오는지
- `id` 조건은 인덱스 경로를 사용한다는 점을 설명할 수 있는지

## 데모 3: `students` 수동 `id`

`students`는 auto-increment가 없으므로 수동 `id`를 넣는 예시로 시연합니다.

```bash
TMP_DB=$(mktemp -d /tmp/sql-parser-demo-students-XXXXXX)
mkdir -p "$TMP_DB/schema" "$TMP_DB/tables"
cp data/schema/students.schema "$TMP_DB/schema/students.schema"
cp data/tables/students.csv "$TMP_DB/tables/students.csv"
```

SQL 파일:

```sql
INSERT INTO students (id, name, grade, age, region, score)
VALUES (1, 'Kim', 2, 20, 'Seoul', 3.50);

SELECT * FROM students ORDER BY id;
```

확인 포인트:

- `students`는 수동 `id`가 허용되는지
- `users`와 `students`의 계약 차이를 설명할 수 있는지

## 시연 중 설명하면 좋은 리스크

현재 구현은 correctness를 우선합니다.

- CSV append 이후 인덱스 갱신이 실패하면 인덱스를 무효화합니다.
- 이후 `id` 조회는 선형 fallback으로 동작합니다.
- 따라서 같은 세션에서 데이터 불일치를 그대로 노출하지는 않지만, 성능이 떨어질 수 있습니다.

이 항목은 “남은 품질 리스크”로 설명하면 좋습니다.

## 시연 시 피해야 할 방법

- `tests/integration/*.sql`을 실제 `./data`에 바로 실행
- auto-increment 테이블인 `users`에 수동 `id`를 넣는 예시를 정상 경로처럼 소개
- Windows PowerShell에서 `/tmp/...` 경로를 그대로 복사해 실행

## 추천 마무리 멘트 포인트

- `users`는 제품 기준 계약 테이블
- `students`는 테스트팀 전달 형식을 수용한 보조 fixture
- B+ 트리로 `id` 조회를 최적화했고, 나머지 조건은 선형 탐색 유지
- correctness를 해치지 않기 위해 실패 시 인덱스 무효화 fallback 전략을 사용
