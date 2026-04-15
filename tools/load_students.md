# Student CSV Bulk Loader

`load_students_csv.py`는 생성된 row-only 학생 CSV를 파일 DB의 테이블 CSV로 적재한다.

기본 대상은 아래 파일이다.

```text
data/tables/students.csv
```

입력 CSV와 테이블 CSV의 컬럼 순서는 항상 아래와 같아야 한다.

```text
id,name,grade,age,region,score
```

대상 스키마는 `data/schema/students.schema`를 사용한다.

```text
table=students
columns=id:int,name:string,grade:int,age:int,region:string,score:float
pkey=id
```

## 기본 사용법

프로젝트 루트에서 실행한다.

```bash
python3 tools/load_students_csv.py \
  --input data/generated/students_1m.csv
```

기본값은 아래와 같다.

```text
db-root=data
table=students
batch-size=100000
```

## 기존 테이블을 비우고 적재

기존 `data/tables/students.csv`를 비운 뒤 새로 적재하려면 `--truncate`를 사용한다.

```bash
python3 tools/load_students_csv.py \
  --input data/generated/students_1m.csv \
  --db-root data \
  --table students \
  --truncate
```

## 순차 primary key fast path

`generate_students_csv.py`가 만든 기본 CSV는 `id`가 1씩 증가한다. 이 경우 `--trust-sequential-pk`를 사용하면 입력 파일 내부 중복 검사를 set으로 저장하지 않고, primary key가 strict increasing인지 확인한다.

```bash
python3 tools/load_students_csv.py \
  --input data/generated/students_1m.csv \
  --truncate \
  --trust-sequential-pk
```

append 모드에서 이미 존재하는 테이블 데이터와의 primary key 중복은 여전히 먼저 확인한다.

## 10건 샘플 적재

```bash
python3 tools/load_students_csv.py \
  --input data/generated/students_sample10.csv \
  --db-root data \
  --table students \
  --truncate \
  --batch-size 5
```

line count 확인:

```bash
wc -l data/tables/students.csv
```

조회 확인:

```bash
printf 'SELECT id, name, score FROM students WHERE id = 1;\n' > /tmp/students_select_id.sql
./sql_processor --sql /tmp/students_select_id.sql --db data
```

float 조건 조회 확인:

```bash
printf 'SELECT id, name, score FROM students WHERE score = 4.31;\n' > /tmp/students_select_score.sql
./sql_processor --sql /tmp/students_select_score.sql --db data
```

## 1,000,000건 적재

```bash
/usr/bin/time -p python3 tools/load_students_csv.py \
  --input data/generated/students_1m.csv \
  --db-root data \
  --table students \
  --truncate \
  --batch-size 250000 \
  --trust-sequential-pk
```

검증에 사용한 1,000,000건 적재 결과는 약 1.81초였다.

```text
loaded 250000 rows...
loaded 500000 rows...
loaded 750000 rows...
loaded 1000000 rows...
loaded 1000000 rows -> <db-root>/tables/students.csv
real 1.81
```

## 옵션

| 옵션 | 설명 | 기본값 |
| --- | --- | --- |
| `--input` | 적재할 row-only CSV 경로 | 필수 |
| `--db-root` | `schema/`, `tables/`를 포함하는 DB 루트 | `data` |
| `--table` | 대상 테이블명 | `students` |
| `--truncate` | 기존 테이블 파일을 비우고 적재 | 꺼짐 |
| `--batch-size` | 진행률 출력 간격 | `100000` |
| `--trust-sequential-pk` | primary key가 strict increasing이라고 보고 빠르게 검증 | 꺼짐 |

## 검증 내용

loader는 적재 전에 각 row를 스키마 기준으로 검증한다.

- 컬럼 개수가 스키마와 같은지 확인
- `int` 컬럼이 정수인지 확인
- `float` 컬럼이 유한한 실수인지 확인
- primary key 중복 확인
- `--trust-sequential-pk` 사용 시 primary key가 strict increasing인지 확인

## 실패 케이스 예시

컬럼 수가 부족한 경우:

```text
error: line 1: expected 6 columns, got 5
```

`score`가 float이 아닌 경우:

```text
error: line 1, column `score`: invalid float value `not-a-float`
```

primary key가 중복된 경우:

```text
error: line 2: duplicate primary key `1`
```

## 도움말

```bash
python3 tools/load_students_csv.py --help
```
