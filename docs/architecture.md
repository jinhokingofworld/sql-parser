# Architecture

## 실행 흐름
1. `main.c`가 CLI 인자를 해석하고 SQL 파일을 읽습니다.
2. `tokenizer.c`가 SQL 문자열을 토큰 배열로 변환합니다.
3. `parser.c`가 토큰을 `Query` AST로 변환합니다.
4. `executor.c`가 schema 검증과 storage 호출을 통해 실제 동작을 수행합니다.
5. `storage.c`가 CSV 파일을 읽고 씁니다.

## 모듈 책임
- `cli.c`: 명령행 옵션 파싱
- `tokenizer.c`: 키워드, 식별자, 숫자, 문자열 토큰화
- `parser.c`: `INSERT`/`SELECT` 구문 해석
- `schema.c`: schema metadata 로딩과 컬럼 조회
- `storage.c`: CSV escape/unescape, row append/read
- `executor.c`: 타입 검증, WHERE 필터링, ORDER BY 정렬, 출력
- `utils.c`: 공통 유틸과 AST 메모리 정리

## 저장 구조
- `schema/<table>.schema`
- `tables/<table>.csv`

예시 schema:

```text
table=users
columns=id:int,name:string,age:int
```
