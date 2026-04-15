# SQL Processor

파일 기반 SQL 처리기입니다. C로 작성되었고, SQL 파일을 CLI로 받아 `INSERT`와 `SELECT`를 수행합니다.

## 지원 기능
- SQL 파일 입력
- `INSERT INTO ... VALUES ...`
- `SELECT ... FROM ...`
- `WHERE column = value`
- `ORDER BY column [ASC|DESC]`
- CSV 기반 테이블 저장
- schema metadata 기반 컬럼 검증
- 여러 SQL statement 순차 실행

## 디렉터리 구조
- `data/schema/<table>.schema`: 테이블 스키마 정의
- `data/tables/<table>.csv`: 실제 테이블 데이터
- `tests/integration/*.sql`: 실행 예시 SQL 파일
- `tools/generate_students_csv.py`: 대량 students CSV 생성기
- `tools/load_students_csv.py`: 생성 CSV를 DB 테이블 CSV로 적재하는 bulk loader

예시 schema:

```text
table=users
columns=id:int,name:string,age:int
```

예시 CSV:

```text
1,Alice,20
2,Bob,31
```

## 빌드
현재 실행 중인 환경에서 직접 빌드해야 합니다.

```bash
make clean
make
```

실행 파일은 아래에 생성됩니다.

```bash
./sql_processor
```

## 기본 실행 형식

권장 형식:

```bash
./sql_processor --sql <sql-file> --db <db-root>
```

예시:

```bash
./sql_processor --sql tests/integration/select_where.sql --db ./data
```

간단 형식도 지원합니다.

```bash
./sql_processor tests/integration/select_where.sql
```

이 경우 `--db`를 생략하면 기본값으로 `./data`를 사용합니다.

## INSERT 하는 법

### 1. SQL 파일 작성
예를 들어 `insert_user.sql` 파일을 아래처럼 만듭니다.

```sql
INSERT INTO users (id, name, age) VALUES (3, 'Charlie', 19);
```

지원 규칙:
- 컬럼 목록은 반드시 써야 합니다.
- 컬럼 개수와 값 개수가 같아야 합니다.
- 문자열은 작은따옴표 `'...'` 를 사용합니다.
- 현재 구현에서는 schema의 모든 컬럼을 정확히 한 번씩 넣는 방식으로 사용하는 것이 안전합니다.

### 2. 실행

```bash
./sql_processor --sql insert_user.sql --db ./data
```

### 3. 결과 확인
정상 실행되면 아래처럼 출력됩니다.

```text
INSERT 1
```

그리고 실제 데이터는 `data/tables/users.csv`에 한 줄 추가됩니다.
예:

```text
1,Alice,20
2,Bob,31
3,Charlie,19
```

## 대량 students 데이터 준비

B+Tree 인덱스 성능 테스트용 `students` 테이블은 아래 스키마를 사용합니다.

```text
table=students
columns=id:int,name:string,grade:int,age:int,region:string,score:float
pkey=id
```

현재 bulk loader는 `float` 타입을 지원한다는 전제로 `score` 값을 검증하고 적재합니다. SQL 엔진의 `float` 타입 구현은 팀원 코드가 merge된 뒤 조회/삽입 테스트를 진행합니다.

### 1. row-only CSV 생성

```bash
python3 tools/generate_students_csv.py \
  --rows 1000000 \
  --output data/generated/students_1m.csv \
  --seed 42
```

생성되는 컬럼 순서는 아래와 같고 header는 포함하지 않습니다.

```text
id,name,grade,age,region,score
```

`score`는 `0.00` 이상 `4.50` 이하의 소수점 2자리 값으로 생성됩니다.

### 2. DB 테이블로 bulk load

```bash
python3 tools/load_students_csv.py \
  --input data/generated/students_1m.csv \
  --db-root data \
  --table students \
  --truncate \
  --batch-size 100000
```

`--truncate`를 주면 기존 `data/tables/students.csv`를 비운 뒤 적재합니다. 기존 데이터를 유지하고 append하려면 `--truncate`를 생략합니다.

생성기가 만든 순차 id 파일을 빠르게 적재할 때는 아래 옵션으로 입력 파일 내부 중복 검사를 strictly increasing 검사로 대체할 수 있습니다.

```bash
python3 tools/load_students_csv.py \
  --input data/generated/students_1m.csv \
  --truncate \
  --trust-sequential-pk
```

## SELECT 하는 법

### 1. 전체 조회
`select_all.sql`

```sql
SELECT * FROM users;
```

실행:

```bash
./sql_processor --sql select_all.sql --db ./data
```

출력 예시:

```text
+----+---------+-----+
| id | name    | age |
+----+---------+-----+
| 1  | Alice   | 20  |
| 2  | Bob     | 31  |
+----+---------+-----+
(2 rows)
```

### 2. 특정 컬럼만 조회
`select_columns.sql`

```sql
SELECT id, name FROM users;
```

실행:

```bash
./sql_processor --sql select_columns.sql --db ./data
```

### 3. WHERE 조건으로 조회
현재 `WHERE column = value` 형태의 단일 조건만 지원합니다.

`select_where.sql`

```sql
SELECT name FROM users WHERE age = 31;
```

실행:

```bash
./sql_processor --sql select_where.sql --db ./data
```

### 4. ORDER BY로 정렬
현재 단일 컬럼 기준 정렬만 지원합니다.

`select_order.sql`

```sql
SELECT id, name FROM users ORDER BY name;
```

실행:

```bash
./sql_processor --sql select_order.sql --db ./data
```

내림차순도 지원합니다.

```sql
SELECT id, name FROM users ORDER BY name DESC;
```

### 5. WHERE + ORDER BY 함께 사용

```sql
SELECT * FROM users WHERE age = 20 ORDER BY name;
```

## 여러 SQL을 한 파일에서 실행하는 법
하나의 SQL 파일 안에 여러 statement를 넣을 수 있습니다.

`insert_then_select.sql`

```sql
INSERT INTO users (id, name, age) VALUES (3, 'Charlie', 19);
SELECT * FROM users ORDER BY id;
```

실행:

```bash
./sql_processor --sql insert_then_select.sql --db ./data
```

이 경우 `INSERT` 결과와 `SELECT` 결과가 순서대로 출력됩니다.

## `--explain` 옵션
파싱 결과(AST에 가까운 구조)를 먼저 보고 싶다면 `--explain` 옵션을 사용합니다.

```bash
./sql_processor --sql tests/integration/select_where.sql --db ./data --explain
```

예시 출력:

```text
QUERY_SELECT table=users columns=[name] where=age=31
+------+
| name |
+------+
| Bob  |
+------+
(1 rows)
```

## 자주 하는 실수

### 1. `main.c`를 직접 실행하려는 경우
실행 대상은 `src/main.c`가 아니라 빌드된 실행 파일 `./sql_processor` 입니다.

### 2. 다른 환경에서 만든 바이너리를 실행하는 경우
컨테이너나 다른 OS에서 실행할 때는 반드시 그 환경에서 다시 `make` 해야 합니다.

### 3. 세미콜론을 빼먹는 경우
각 SQL statement는 `;`로 끝나야 합니다.

예:

```sql
SELECT * FROM users;
```

### 4. 존재하지 않는 컬럼명을 쓰는 경우
schema에 없는 컬럼을 쓰면 에러가 납니다.

### 5. 데이터가 누적되는 경우
`INSERT`는 실제 `data/tables/*.csv` 파일에 반영됩니다.
같은 SQL을 여러 번 실행하면 row도 여러 번 추가됩니다.

## 테스트

단위 테스트:

```bash
make unit
```

통합 테스트:

```bash
make integration
```

전체 테스트:

```bash
make test
```

Windows PowerShell:

```powershell
.\tools\build_windows.ps1
.\tools\test_windows.ps1 -Build
```

Windows suite examples:

```powershell
.\tools\test_windows.ps1 -Build -Suite unit
.\tools\test_windows.ps1 -Build -Suite integration
.\tools\test_windows.ps1 -Build -Suite verify
.\tools\test_windows.ps1 -Build -Suite bptree-contract
.\tools\test_windows.ps1 -Build -Suite verify -IncludeLargeVerify
```
