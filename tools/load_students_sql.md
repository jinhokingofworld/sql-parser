# Student SQL Insert Loader

`load_students_sql.py`는 생성된 row-only 학생 CSV를 SQL `INSERT` statement로 변환하고, `sql_processor`를 실행해서 실제 SQL 경로로 데이터를 삽입한다.

이 도구의 목적은 빠른 데이터 세팅이 아니라 성능 비교다.

```text
index 없음: SQL INSERT 실행 시간 측정
index 있음: SQL INSERT 실행 시간 측정
```

`tools/load_students_csv.py`는 테이블 CSV 파일에 직접 쓰는 빠른 bulk loader다. 반면 이 도구는 `src/parser.c`, `src/executor.c`, storage, index 갱신 경로를 실제로 지나가게 만든다.

## 입력 CSV

입력 CSV는 header 없는 row-only 형식이어야 한다.

```text
id,name,grade,age,region,score
```

예시:

```text
1,김수민,1,19,고양시 일산서구,3.34
2,김정호,2,21,천안시 동남구,0.63
```

## 기본 사용법

먼저 `sql_processor`를 빌드한다.

```bash
make
```

CSV를 SQL INSERT로 실행한다.

```bash
python3 tools/load_students_sql.py \
  --input data/generated/students_sample10.csv \
  --db-root data \
  --table students \
  --truncate \
  --batch-size 10
```

## 대량 INSERT 성능 측정

현재 SQL `INSERT` 경로는 primary key 중복 검사를 위해 매 INSERT마다 전체 테이블을 읽는다. 그래서 index가 없는 현재 구현에서는 큰 입력이 매우 느릴 수 있다.

단계적으로 측정한다.

```bash
python3 tools/load_students_sql.py \
  --input data/generated/students_sample10.csv \
  --db-root data \
  --table students \
  --truncate \
  --batch-size 10
```

```bash
python3 tools/generate_students_csv.py \
  --rows 1000 \
  --output data/generated/students_1k.csv \
  --seed 42

python3 tools/load_students_sql.py \
  --input data/generated/students_1k.csv \
  --db-root data \
  --table students \
  --truncate \
  --batch-size 100
```

index 적용 전후에 같은 CSV, 같은 batch size, 같은 DB 초기 상태로 측정한다.

## 옵션

| 옵션 | 설명 | 기본값 |
| --- | --- | --- |
| `--input` | 적재할 row-only CSV 경로 | 필수 |
| `--db-root` | `schema/`, `tables/`를 포함하는 DB 루트 | `data` |
| `--table` | 대상 테이블명 | `students` |
| `--sql-processor` | 실행할 SQL processor 경로 | `./sql_processor` |
| `--truncate` | INSERT 실행 전 대상 테이블 CSV를 비움 | 꺼짐 |
| `--batch-size` | 한 번의 `sql_processor` 실행에 넣을 INSERT statement 수 | `1000` |
| `--progress-interval` | 진행률 출력 간격 | `10000` |
| `--keep-batch-sql` | 임시 SQL batch 파일을 삭제하지 않음 | 꺼짐 |

## 생성되는 SQL

CSV row 하나는 아래 SQL로 변환된다.

```sql
INSERT INTO students (id, name, grade, age, region, score)
VALUES (1, '김수민', 1, 19, '고양시 일산서구', 3.34);
```

문자열 값은 작은따옴표로 감싸고, 내부 작은따옴표는 SQL 방식으로 escape한다.

## 검증

적재 후 line count를 확인한다.

```bash
wc -l data/tables/students.csv
```

SQL 조회를 확인한다.

```bash
printf 'SELECT id, name, score FROM students WHERE id = 1;\n' > /tmp/students_select_id.sql
./sql_processor --sql /tmp/students_select_id.sql --db data
```

float 조건 조회도 확인한다.

```bash
printf 'SELECT id, name, score FROM students WHERE score = 4.31;\n' > /tmp/students_select_score.sql
./sql_processor --sql /tmp/students_select_score.sql --db data
```

## 주의

- 이 도구는 SQL 경로의 INSERT 성능을 재기 위한 도구라서 직접 CSV 적재보다 훨씬 느릴 수 있다.
- 실패가 발생하면 이미 실행된 batch의 INSERT는 테이블 CSV에 남을 수 있다.
- 공정한 비교를 위해 index 없음/있음 실험 모두 같은 CSV와 같은 `--batch-size`를 사용한다.
- `--truncate`는 측정 시작 전 초기화 용도다. 순수 INSERT 시간만 기록하려면 loader가 출력하는 `elapsed` 값을 사용한다.

## 도움말

```bash
python3 tools/load_students_sql.py --help
```
