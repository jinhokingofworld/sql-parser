# B+Tree Index Performance Test Data Plan

## 현재 코드베이스와 DB 상태 확인

- 현재 프로젝트는 C 기반 파일 DB이며, `data/schema/<table>.schema`와 `data/tables/<table>.csv`를 사용한다.
- 현재 기본 테이블은 `users`이고, 스키마 파일은 `data/schema/users.schema`에 있다.
- 현재 `users` 스키마는 아래 3개 컬럼만 가진다.

```text
table=users
columns=id:int,name:string,age:int
pkey=id
```

- 현재 실제 데이터 파일인 `data/tables/users.csv`는 비어 있다.
- `src/storage.c`는 CSV 파일을 append/read 하는 구조이고, `read_csv_rows()`는 CSV 전체를 메모리에 읽는다.
- `src/executor.c`의 `INSERT`는 primary key 중복 확인을 위해 매 insert마다 전체 CSV를 읽는다.
- 현재 타입 시스템은 `int`, `string`만 지원한다. 요청 데이터 구조의 `score:float`를 쓰려면 `float` 타입 지원을 추가하거나, 임시로 `score`를 `string`으로 저장하는 선택이 필요하다.

## 목표 데이터 구조

대량 성능 테스트용 테이블은 최소 1,000,000개 이상의 레코드를 가진다.

```text
id, 학번, int
name, 이름, string
grade, 학년, int
age, 나이, int
region, 지역, string
score, 학점, float
```

권장 테이블명은 `students`로 한다.

```text
table=students
columns=id:int,name:string,grade:int,age:int,region:string,score:float
pkey=id
```

## 작업 순서

### 1. 대량 데이터용 스키마 결정

1. `students` 테이블을 새로 만들지, 기존 `users` 테이블을 확장할지 결정한다.
>students 테이블을 하나 만들어줘.
2. B+Tree 인덱스 실험 대상 컬럼을 정한다.
> 다른 팀원들이 B+Tree인덱스를 만들거니까, 일단 pkey인 id를 기반으로 만든다고 생각을 해줘.
3. `id`는 primary key이자 학번으로 사용한다.
4. `score`를 실제 `float` 타입으로 지원할지 결정한다.
>float타입 지원해줘.
5. 실제 `float` 타입을 지원한다면 `ColumnType`, schema parser, value parser, type checker, comparison 로직까지 확장한다.

진행 상태:
- [x] `students` 테이블을 별도로 만들기로 결정했다.
- [x] B+Tree 인덱스 실험은 우선 primary key인 `id` 기준으로 생각한다.
- [x] `score`는 실제 `float` 타입으로 지원하기로 결정했다.
- [ ] C 코드의 `float` 타입 지원 구현은 아직 진행하지 않았다.

### 2. `students` 스키마와 빈 테이블 파일 추가

1. `data/schema/students.schema` 파일을 만든다.
2. `data/tables/students.csv` 파일을 만든다.
3. 테스트 fixture가 필요하면 `tests/fixtures/db/schema/students.schema`와 `tests/fixtures/db/tables/students.csv`도 추가한다.
>테스트는 다른 팀원에게 맡겨놔서, 제외해도 돼.

진행 상태:
- [ ] `data/schema/students.schema` 추가
- [ ] `data/tables/students.csv` 추가
- [x] 테스트 fixture 추가는 이번 작업 범위에서 제외하기로 결정했다.

### 3. Python 데이터 생성기 작성

1. `tools/generate_students_csv.py`를 만든다.
2. `faker` 기반으로 이름(name), 지역(region)을 생성한다.
3. CLI 옵션을 제공한다.
   - `--rows`: 생성할 레코드 수, 기본값 `1000000`
   - `--output`: 출력 CSV 경로, 기본값 `data/generated/students_1m.csv`
   - `--seed`: 재현 가능한 데이터 생성을 위한 seed
   - `--start-id`: 시작 id, 기본값 `1`
4. CSV 컬럼 순서는 스키마와 동일하게 `id,name,grade,age,region,score`로 고정한다.
> score는 0 <= score <= 4.5로 해줘.
5. DB 테이블 파일과 같은 row-only CSV로 만들지, header 포함 CSV로 만들지 결정한다.
> row-only CSV
6. 삽입기는 header 포함/미포함을 모두 처리할 수 있게 만드는 것을 권장한다.
> 일단 미포함으로 가자
7. 1,000,000건 생성 시 메모리에 전체 데이터를 쌓지 말고 streaming write 방식으로 파일에 바로 쓴다.

진행 상태:
- [x] `tools/generate_students_csv.py` 작성 완료
- [x] `Faker` 기반으로 이름(name), 지역(region)을 생성하도록 구현
- [x] `--rows`, `--output`, `--seed`, `--start-id` 옵션 구현
- [x] 추가 옵션으로 `--locale` 구현, 기본값은 `ko_KR`
- [x] CSV 컬럼 순서를 `id,name,grade,age,region,score`로 고정
- [x] header 없는 row-only CSV로 생성
- [x] `score`는 `0.00` 이상 `4.50` 이하, 소수점 2자리 문자열로 생성
- [x] 전체 데이터를 메모리에 쌓지 않고 streaming write 방식으로 파일에 기록
- [x] `requirements.txt`에 `Faker>=40.13.0,<41` 추가
- [x] 대량 생성 파일을 git에 넣지 않도록 `.gitignore`에 `data/generated/` 추가

### 4. 데이터 생성기 검증

1. 먼저 10건짜리 샘플 CSV를 생성한다.
2. CSV escaping이 정상인지 확인한다.
3. 컬럼 개수와 타입 범위를 확인한다.
4. `id`가 중복 없이 증가하는지 확인한다.
5. 1,000,000건 생성 시간을 측정한다.
6. 생성된 파일 크기를 기록한다.

> 데이터 생성기가 검증 완료 되기 전까지는 삽입기를 작성하지 말아야 해.
> 일단 생성기부터 작성하자

진행 상태:
- [x] 10건짜리 샘플 CSV 생성 완료
- [x] 샘플 파일 경로: `data/generated/students_sample10.csv`
- [x] header 없이 row-only 형식으로 생성되는 것 확인
- [x] 컬럼 개수가 모든 row에서 6개인지 확인
- [x] `id`가 `1..10`으로 중복 없이 증가하는지 확인
- [x] `score`가 `0.0 <= score <= 4.5` 범위인지 확인
- [x] 같은 seed로 생성한 파일이 완전히 동일한지 확인

검증에 사용한 명령:

```bash
python3 tools/generate_students_csv.py --rows 10 --output data/generated/students_sample10.csv --seed 42
python3 tools/generate_students_csv.py --rows 10 --output data/generated/students_sample10_again.csv --seed 42
cmp data/generated/students_sample10.csv data/generated/students_sample10_again.csv
```

검증 결과:

```text
generated 10 rows -> data/generated/students_sample10.csv
ok rows=10 columns=6 ids=1..10 score_range=0.0..4.5
ok reproducible_sample_valid
```

### 5. CSV 기반 DB 데이터 삽입기 작성

1. `tools/load_students_csv.py` 또는 C 기반 bulk loader 중 하나를 선택한다.
> 우리가 만든 DB를 이용하는 bulk loader를 만들자.
2. 단기 구현은 Python loader를 권장한다.
3. loader는 생성 CSV를 읽어서 DB 테이블 CSV인 `data/tables/students.csv`에 append한다.
4. loader CLI 옵션을 제공한다.
   - `--input`: 생성된 CSV 경로
   - `--db-root`: DB 루트, 기본값 `data`
   - `--table`: 테이블명, 기본값 `students`
   - `--truncate`: 기존 테이블 파일을 비우고 새로 적재
   - `--batch-size`: 진행률 표시 단위
5. 기존 SQL `INSERT` 경로를 그대로 1,000,000번 호출하는 방식은 피한다.
6. 이유는 현재 `INSERT`가 매번 전체 테이블을 읽어 primary key 중복을 검사하므로 O(n^2)에 가까운 비용이 발생하기 때문이다.
7. bulk loader는 적재 전 중복 id 검사를 한 번만 수행하거나, 생성기가 보장하는 순차 id를 신뢰하는 fast path를 제공한다.

진행 상태:
- [ ] `float` 타입 지원
- [ ] `students` 스키마 추가
- [ ] loader는 header 없는 CSV를 입력으로 받는 방향을 유지한다.

### 6. 삽입기 검증

1. 10건 샘플 CSV를 `students.csv`에 적재한다.
2. `SELECT * FROM students;` 또는 `SELECT id, name FROM students WHERE id = 1;`로 조회를 확인한다.
3. 1,000,000건 적재 전에 10,000건, 100,000건으로 단계 테스트한다.
4. 적재 후 line count가 기대값과 같은지 확인한다.
5. 잘못된 컬럼 개수, 잘못된 타입, 중복 id 입력에 대한 실패 케이스를 확인한다.

### 7. 현재 SQL 엔진의 대량 데이터 병목 정리

1. `read_csv_rows()`가 전체 CSV를 메모리에 올리므로 1,000,000건 SELECT는 메모리 사용량이 커진다.
2. 현재 WHERE 검색은 full scan이다.
3. 현재 ORDER BY는 matching row 포인터 배열을 만든 뒤 `qsort()`를 수행한다.
4. B+Tree 인덱스 도입 전 성능 기준선을 측정한다.
5. 측정 쿼리는 `id`, `grade`, `region`, `score` 기준으로 나눈다.

### 8. B+Tree 성능 테스트 기준선 만들기
> 이거는 아직 필요 없을 것 같아. 일단 대량의 데이터를 삽입할 수 있다는 것을 확정한 후에 브랜치를 나눠서, 인덱스가 없는 버전, 있는 버전에서 따로 성능 테스트기를 만들어야 발표 때 보여주기 좋을 것 같다는 생각이야.

1. 인덱스 없는 상태에서 아래 쿼리 시간을 측정한다.

```sql
SELECT * FROM students WHERE id = 500000;
SELECT * FROM students WHERE region = 'Seoul';
SELECT * FROM students ORDER BY id;
```

2. 각 쿼리는 cold run과 warm run을 나누어 최소 3회 이상 측정한다.
3. 실행 시간, 메모리 사용량, 결과 row 수를 기록한다.
4. 이후 B+Tree 인덱스 적용 후 같은 쿼리를 다시 측정해 비교한다.



### 9. 문서화

1. README에 대량 데이터 생성 방법을 추가한다.
2. README에 bulk load 방법을 추가한다.
3. B+Tree 적용 전후 성능 측정 결과를 `docs/performance.md`에 기록한다.
4. 생성된 1,000,000건 CSV는 용량이 크므로 git tracking 대상에서 제외한다.
5. `.gitignore`에 `data/generated/`와 대량 적재 결과 파일 정책을 추가한다.

진행 상태:
- [x] `.gitignore`에 `data/generated/` 추가
- [x] `.gitignore`에 Python 생성물인 `__pycache__/`, `*.pyc` 추가
- [ ] README에 대량 데이터 생성 방법 추가
- [ ] README에 bulk load 방법 추가
- [ ] `docs/performance.md` 작성

## 우선 구현 체크리스트

- [x] `float` 타입 지원 여부 결정
- [ ] `students` 스키마 추가
- [x] `tools/generate_students_csv.py` 작성
- [x] 소량 샘플 데이터 생성 테스트
- [ ] `tools/load_students_csv.py` 작성
- [ ] 소량 CSV bulk load 테스트
- [ ] 10,000건 bulk load 테스트
- [ ] 100,000건 bulk load 테스트
- [ ] 1,000,000건 bulk load 테스트
- [ ] full scan 기준 성능 측정
- [ ] B+Tree 인덱스 설계와 적용 작업으로 넘어가기
