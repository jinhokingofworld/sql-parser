# Plan

## 목표
C 언어로 동작하는 파일 기반 SQL 처리기를 구현한다.

지원 범위:
- SQL 파일 입력
- `INSERT`
- `SELECT`
- 테이블별 파일 저장
- 단위 테스트
- 기능 테스트

완성 기준:
- CLI에서 SQL 파일을 받아 실행할 수 있다.
- 정상 입력과 대표 엣지 케이스를 테스트로 검증한다.
- 구조가 분리되어 이후 `DELETE`, `UPDATE`, `CREATE TABLE` 확장이 가능하다.

---

## 접근 방식

### 1. 전체 실행 흐름 설계
입력부터 저장까지 아래 흐름으로 고정한다.

1. CLI가 SQL 파일 경로를 받는다.
2. 파일 내용을 읽어 SQL 문자열을 확보한다.
3. tokenizer가 SQL을 토큰으로 분해한다.
4. parser가 토큰을 AST 또는 명령 구조체로 변환한다.
5. executor가 명령 타입에 따라 storage를 호출한다.
6. storage가 테이블 파일을 읽거나 쓴다.
7. 결과를 표준 출력으로 보여준다.

이유:
- 과제 핵심인 `입력 → 파싱 → 실행 → 저장` 흐름이 명확해진다.
- 모듈별 테스트가 쉬워진다.

---

### 2. 제안 디렉터리 구조
초기 프로젝트 구조를 아래처럼 잡는다.

```text
sql-parser/
├── agent.md
├── research.md
├── plan.md
├── Makefile
├── README.md
├── data/
│   ├── schema/
│   │   └── users.schema
│   └── tables/
│       └── users.csv
├── include/
│   ├── ast.h
│   ├── cli.h
│   ├── common.h
│   ├── executor.h
│   ├── parser.h
│   ├── schema.h
│   ├── storage.h
│   └── tokenizer.h
├── src/
│   ├── main.c
│   ├── cli.c
│   ├── tokenizer.c
│   ├── parser.c
│   ├── executor.c
│   ├── schema.c
│   ├── storage.c
│   └── utils.c
├── tests/
│   ├── unit/
│   │   ├── test_tokenizer.c
│   │   ├── test_parser.c
│   │   ├── test_storage.c
│   │   └── test_executor.c
│   ├── integration/
│   │   ├── insert_select.sql
│   │   ├── select_where.sql
│   │   └── select_order_by.sql
│   └── fixtures/
│       └── sample_users.schema
└── docs/
    └── architecture.md
```

이유:
- 포트폴리오 수준에서는 단순 동작뿐 아니라 구조적 분리가 중요하다.
- 이후 기능 확장 시 충돌을 줄일 수 있다.

---

### 3. SQL 문법 범위 정의
초기 구현 범위를 명확히 제한한다.

#### INSERT
지원 목표:

```sql
INSERT INTO users (id, name, age) VALUES (1, 'Alice', 20);
```

지원 규칙:
- 단일 row insert만 우선 지원
- 컬럼 목록 필수
- VALUES 개수와 컬럼 개수 일치 검사
- 문자열은 작은따옴표 우선 지원

#### SELECT
지원 목표:

```sql
SELECT * FROM users;
SELECT id, name FROM users;
SELECT * FROM users WHERE id = 1;
SELECT name FROM users WHERE age = 20;
SELECT id, name FROM users ORDER BY id;
SELECT * FROM users WHERE age = 20 ORDER BY name;
```

지원 규칙:
- `*` 또는 명시 컬럼 리스트 지원
- 단일 테이블만 지원
- WHERE는 `column = value` 단일 조건만 우선 지원
- ORDER BY는 단일 컬럼 기준 오름차순만 1차 구현에 포함
- JOIN, GROUP BY, LIMIT는 1차 구현에서 제외

이유:
- 과제 요구사항을 충족하면서도 구현 난도를 안정적으로 제어할 수 있다.
- WHERE와 ORDER BY 단일 컬럼 정렬까지 포함하면 데모와 포트폴리오 품질이 좋아진다.

---

### 4. AST / 명령 구조체 설계
파싱 결과는 문자열 조작 결과가 아니라 구조체로 보관한다.

예상 구조:

```c
typedef enum {
    QUERY_INSERT,
    QUERY_SELECT
} QueryType;

typedef enum {
    VALUE_INT,
    VALUE_STRING
} ValueType;

typedef struct {
    ValueType type;
    char *raw;
} Value;

typedef struct {
    char *column;
    Value value;
} Condition;

typedef struct {
    char *column;
    int ascending;
} OrderByClause;

typedef struct {
    char *table_name;
    char **columns;
    int column_count;
    Value *values;
} InsertQuery;

typedef struct {
    char *table_name;
    char **columns;
    int column_count;
    int select_all;
    int has_where;
    Condition where;
    int has_order_by;
    OrderByClause order_by;
} SelectQuery;

typedef struct {
    QueryType type;
    union {
        InsertQuery insert_query;
        SelectQuery select_query;
    };
} Query;
```

이유:
- executor가 parser 내부 구현에 의존하지 않게 된다.
- 테스트에서 파싱 결과를 명확히 검증할 수 있다.
- `ORDER BY`를 1차 구현에 포함하면서도 구조를 단순하게 유지할 수 있다.

트레이드오프:
- 장점: 확장성과 가독성이 좋다.
- 단점: 메모리 해제 로직이 늘어난다.

→ 구조체 기반 AST를 선택한다.

추가 확장 고려:
- 1차 구현은 단일 테이블만 지원한다.
- 다만 이후 스칼라 서브쿼리까지 확장할 수 있도록, `SelectQuery`는 WHERE/ORDER BY를 독립 필드로 분리해 둔다.
- 추후 `WHERE age = (SELECT max_age FROM limits)` 같은 형태를 지원하려면, `Condition`의 오른쪽 값을 리터럴뿐 아니라 표현식으로 일반화할 수 있어야 한다.
- 이번 단계에서는 구조를 과도하게 복잡하게 만들지 않고, 향후 `Value` 또는 `Condition`을 `LiteralOrSubquery` 형태로 확장할 수 있게 여지를 남긴다.

---

### 5. tokenizer 설계
단순 `split(" ")` 방식 대신 최소 tokenizer를 둔다.

토큰 종류:
- keyword
- identifier
- number
- string
- comma
- left parenthesis
- right parenthesis
- semicolon
- asterisk
- equals

필수 처리:
- 대소문자 비민감 키워드 처리
- 문자열 리터럴 내부 공백 유지
- 토큰 위치 정보(line, column) 보관

이유:
- 파서 복잡도를 줄이고 에러 메시지를 개선할 수 있다.

트레이드오프:
- 옵션 1: 문자열 split 기반
  - 장점: 가장 빠르게 구현 가능
  - 단점: 공백/문자열/구두점 처리에 취약
- 옵션 2: tokenizer 기반
  - 장점: 안정성, 확장성, 테스트성 우수
  - 단점: 초기 코드량 증가

→ tokenizer 기반을 선택한다.

---

### 6. parser 설계
재귀 하강까지는 필요 없고, 제한된 문법용 hand-written parser를 사용한다.

핵심 함수 예시:

```c
Query *parse_query(TokenStream *ts);
Query *parse_insert(TokenStream *ts);
Query *parse_select(TokenStream *ts);
Condition parse_where_clause(TokenStream *ts);
OrderByClause parse_order_by_clause(TokenStream *ts);
```

검증 항목:
- 지원하지 않는 키워드 탐지
- 괄호 쌍 검사
- 세미콜론 종료 검사
- 컬럼 수와 값 수 일치 여부
- WHERE 절 형식 유효성
- ORDER BY 절 형식 유효성

이유:
- 지원 문법이 작기 때문에 yacc/bison 없이도 충분하다.
- 핵심 로직을 팀이 이해하고 설명하기 쉽다.

주석 정책:
- 모든 공개 함수에는 입력, 출력, 실패 조건, 처리 이유를 설명하는 함수 단위 주석을 단다.
- tokenizer의 상태 전이, parser의 토큰 소비 지점, WHERE/ORDER BY 해석처럼 헷갈릴 수 있는 구간에는 라인 단위 주석도 추가한다.
- 주석은 "무엇을 하는지"뿐 아니라 "왜 이렇게 처리하는지"를 함께 설명한다.
- 목표는 팀원이 처음 읽어도 흐름을 따라갈 수 있도록 만드는 것이다.

---

### 7. schema 관리 방식
`schema 및 table은 이미 존재` 조건을 만족시키기 위해, 테이블 구조는 별도 schema 파일에서 읽는다.

예시 포맷:

```text
table=users
columns=id:int,name:string,age:int
```

schema 책임:
- 컬럼 이름 목록 제공
- 컬럼 순서 정의
- 타입 검증

이유:
- `CREATE TABLE` 없이도 INSERT/SELECT 검증이 가능하다.
- 저장 파일과 메타데이터를 분리해 안정성을 높일 수 있다.

트레이드오프:
- 장점: 컬럼 검증 가능, executor 단순화
- 단점: schema 파일 관리가 추가된다.

→ schema 메타데이터 파일 방식을 선택한다.

구현 범위 조정:
- 프로젝트에서는 schema 파일 포맷만 정의하고 예시 파일만 제공한다.
- 실제 테이블 구조 정의와 테이블 이름에 대응하는 CSV 파일 생성은 사용자가 직접 준비하는 것을 전제로 한다.
- 처리기는 준비된 schema 파일과 CSV 파일을 읽어 INSERT/SELECT를 수행한다.

---

### 8. 데이터 저장 포맷 설계
1차 구현은 CSV 기반 포맷을 사용한다.

예시:

```text
1,Alice,20
2,Bob,31
```

저장 원칙:
- 테이블당 하나의 `.csv` 파일
- 컬럼 순서는 schema 순서를 따른다
- SELECT 시 한 줄씩 읽으며 파싱한다
- CSV 문자열에 쉼표나 따옴표가 있는 경우를 대비해 escape 정책을 명확히 정의한다

이유:
- 사용자가 직접 테이블별 CSV를 준비하는 흐름과 자연스럽게 맞는다.
- 결과 파일을 사람이 바로 열어 확인하기 쉽다.

트레이드오프:
- 옵션 1: CSV
  - 장점: 익숙함
  - 단점: 인용부호와 쉼표 처리 복잡
- 옵션 2: binary
  - 장점: 성능 가능성
  - 단점: 디버깅과 포트폴리오 설명이 어려움
- 옵션 3: TSV/custom text
  - 장점: 구현 단순, 디버깅 쉬움
  - 단점: 완전 범용 포맷은 아님

→ CSV 포맷을 선택한다.

---

### 9. executor 설계
executor는 파싱 결과와 schema를 바탕으로 storage를 호출한다.

#### INSERT 실행
1. schema 로드
2. table 존재 확인
3. 컬럼 유효성 확인
4. 값 타입 검증
5. schema 순서로 row 재배열
6. 파일 끝에 append

#### SELECT 실행
1. schema 로드
2. 요청 컬럼 유효성 확인
3. 데이터 파일 한 줄씩 읽기
4. row 파싱
5. WHERE 조건 평가
6. ORDER BY가 있으면 결과를 메모리에 모아 정렬
7. 필요한 컬럼만 출력

이유:
- 파서와 저장소 역할을 명확히 분리할 수 있다.

---

### 10. CLI 설계
실행 예시는 아래 형태를 권장한다.

```bash
./sql_processor --sql tests/integration/insert_select.sql --db ./data
```

또는 간단 버전:

```bash
./sql_processor tests/integration/insert_select.sql
```

권장 옵션:
- `--sql <file>`
- `--db <path>`
- `--table-dir <path>` 는 2차 옵션으로 보류

이유:
- 과제 요구인 "텍스트파일 SQL 전달"을 직접 만족한다.
- demo 시 사용성이 좋다.

---

### 11. 에러 처리 정책
에러는 조용히 실패하지 않고 명시적으로 출력한다.

예시:
- syntax error near `VALUES`
- unknown table `users`
- unknown column `nickname`
- column/value count mismatch
- type mismatch: column `age` expects int

이유:
- 포트폴리오 품질 차이를 만드는 핵심 요소다.

---

## 구현 단계 제안

### Phase 1. 프로젝트 뼈대 구성
- `Makefile`
- `src/`, `include/`, `tests/`, `data/` 생성
- 공통 타입과 유틸 정의

완료 기준:
- 빈 실행 파일이 빌드된다.

### Phase 2. tokenizer 구현
- 토큰 구조체
- 문자열/숫자/식별자 스캔
- 키워드 분류

완료 기준:
- INSERT/SELECT 예제가 토큰 배열로 분해된다.

### Phase 3. parser 구현
- INSERT parser
- SELECT parser
- WHERE 단일 조건 parser
- ORDER BY 단일 컬럼 parser

완료 기준:
- SQL 문자열이 Query 구조체로 변환된다.

### Phase 4. schema / storage 구현
- schema 파일 로드
- row append
- row read iterator
- CSV escape / unescape 처리

완료 기준:
- 파일 입출력이 독립 테스트로 검증된다.

### Phase 5. executor 구현
- INSERT 실행
- SELECT 실행
- WHERE 평가
- ORDER BY 정렬

완료 기준:
- end-to-end SQL 처리 가능

### Phase 6. CLI / 출력 포맷 개선
- 명령행 인자 처리
- 사용자 친화적 출력
- 에러 메시지 정리

완료 기준:
- CLI에서 SQL 파일 실행 가능

### Phase 7. 테스트 및 마감 품질 강화
- 단위 테스트
- 기능 테스트
- 엣지 케이스 보강
- README / 아키텍처 문서 작성

완료 기준:
- 포트폴리오 제출 가능한 수준의 문서와 테스트 확보

---

## 파일 변경 목록

신규 생성 예정:
- Makefile
- README.md
- include/ast.h
- include/common.h
- include/tokenizer.h
- include/parser.h
- include/schema.h
- include/storage.h
- include/executor.h
- include/cli.h
- src/main.c
- src/cli.c
- src/tokenizer.c
- src/parser.c
- src/schema.c
- src/storage.c
- src/executor.c
- src/utils.c
- tests/unit/test_tokenizer.c
- tests/unit/test_parser.c
- tests/unit/test_storage.c
- tests/unit/test_executor.c

---

## 테스트 전략

### 단위 테스트
- tokenizer가 공백/문자열/구두점을 올바르게 분리하는지 검증
- parser가 INSERT/SELECT AST를 정확히 만드는지 검증
- storage가 append/read를 정확히 수행하는지 검증
- executor가 WHERE 조건을 정확히 평가하는지 검증
- parser가 ORDER BY 절을 정확히 파싱하는지 검증
- storage가 CSV escape 규칙을 올바르게 처리하는지 검증

### 기능 테스트
- SQL 파일 1개에 INSERT 1문장
- SQL 파일 1개에 SELECT 1문장
- INSERT 후 SELECT 결과 검증
- WHERE 조건 결과 검증
- ORDER BY 결과 정렬 검증

### 엣지 케이스
- 빈 SQL 파일
- 세미콜론 누락
- 존재하지 않는 테이블
- 존재하지 않는 컬럼
- 값 개수 불일치
- 문자열 따옴표 누락
- 공백이 많은 SQL
- 키워드 대소문자 혼합
- ORDER BY 대상 컬럼이 존재하지 않는 경우
- CSV 문자열 내부 쉼표 또는 따옴표 포함 경우

---

## 차별화 포인트 제안

### 1. 설명 가능한 AST 출력 모드
예시:

```bash
./sql_processor --sql query.sql --explain
```

효과:
- 파싱 결과를 구조적으로 보여줘 시연 품질이 좋아진다.
- 핵심 로직 이해도를 설명하기 좋다.

### 2. pretty table 출력
SELECT 결과를 정렬된 표 형태로 출력한다.

효과:
- 시연 인상이 좋아지고, 포트폴리오 완성도가 올라간다.

### 3. 에러 위치 표시
토큰 위치 기반으로 오류 컬럼을 보여준다.

효과:
- "그냥 돌아가는 과제"가 아니라 "사용성까지 고려한 도구"로 보인다.

### 4. multi-statement 지원
한 SQL 파일 안에 여러 문장을 순차 실행한다.

효과:
- 데모가 강해지고 실제 처리기 느낌이 살아난다.

우선순위:
1. AST 출력 모드
2. pretty table 출력
3. multi-statement 지원

---

## 리스크 및 대응

### 리스크 1. 파서 복잡도 증가
- 대응: 지원 문법을 명확히 제한한다.

### 리스크 2. 문자열 저장 포맷 깨짐
- 대응: CSV escape 정책을 초기에 정의하고, 저장/조회 테스트에 반드시 포함한다.

### 리스크 3. 테스트 작성 부담
- 대응: tokenizer/parser/storage를 독립 모듈로 나눠 초기에 테스트 가능하게 설계한다.

### 리스크 4. 일정 내 완성도 부족
- 대응: 우선순위를 "정확한 INSERT/SELECT + 안정적 테스트"에 둔다.

---

## 최종 선택 요약
- parser 방식: hand-written tokenizer + parser
- 저장 포맷: CSV
- schema 방식: 별도 schema metadata 파일
- SELECT 범위: `*`, 컬럼 리스트, 단일 WHERE, 단일 ORDER BY
- CLI 방식: SQL 파일 경로 입력
- 품질 방향: 테스트 우선 + 자세한 주석 + explain/pretty output로 차별화

---

## 다음 단계
구현 완료 후 기준으로 진행 상황을 기록한다.

## 진행 상황
- [x] Phase 1. 프로젝트 뼈대 구성
- [x] Phase 2. tokenizer 구현
- [x] Phase 3. parser 구현
- [x] Phase 4. schema / storage 구현
- [x] Phase 5. executor 구현
- [x] Phase 6. CLI / 출력 포맷 개선
- [x] Phase 7. 테스트 및 마감 품질 강화

## 구현 메모
- `INSERT`는 1차 구현 안정성을 위해 schema의 모든 컬럼을 정확히 한 번씩 제공해야 한다.
- `SELECT`는 `WHERE column = value`, `ORDER BY column [ASC|DESC]`까지 지원한다.
- SQL 파일 안의 여러 statement를 순차 실행할 수 있다.

## 리스크
- 현재 `SELECT ORDER BY`는 결과를 메모리에 모두 올린 뒤 정렬하므로 대용량 데이터에는 비효율적이다.
- `SELECT` 출력 포맷은 pretty table 중심이며, CSV/JSON 같은 기계 친화적 출력 모드는 아직 없다.
- CSV 한 줄 최대 길이는 현재 구현에서 고정 버퍼에 영향을 받으므로 극단적으로 긴 레코드는 추가 보강이 필요하다.
