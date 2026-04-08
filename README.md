# SQL Processor

파일 기반 SQL 처리기입니다. C로 작성되었고, SQL 파일을 CLI로 받아 `INSERT`와 `SELECT`를 처리합니다.

## 지원 기능
- SQL 파일 입력
- `INSERT INTO ... VALUES ...`
- `SELECT ... FROM ...`
- `WHERE column = value`
- `ORDER BY column [ASC|DESC]`
- CSV 기반 테이블 저장
- schema metadata 기반 컬럼 검증

## 빌드
```bash
make
```

## 실행
```bash
./sql_processor --sql tests/integration/insert_select.sql --db ./data
```

AST를 보고 싶다면:

```bash
./sql_processor --sql tests/integration/select_where.sql --db ./data --explain
```

## 테스트
```bash
make test
```
