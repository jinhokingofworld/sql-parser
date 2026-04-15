# Student CSV Generator

`generate_students_csv.py`는 B+Tree 인덱스 성능 테스트용 학생 데이터를 CSV로 생성한다.

생성되는 CSV는 header 없는 row-only 형식이다.

```text
1,김수민,1,19,고양시 일산서구,3.34
2,김정호,2,21,천안시 동남구,0.63
```

컬럼 순서는 항상 아래와 같다.

```text
id,name,grade,age,region,score
```

## 의존성 설치

프로젝트 루트에서 실행한다.

```bash
python3 -m pip install -r requirements.txt
```

## 기본 사용법

기본값으로 1,000,000건을 생성한다.

```bash
python3 tools/generate_students_csv.py
```

기본 출력 경로는 아래와 같다.

```text
data/generated/students_1m.csv
```

## 10건 테스트 생성

```bash
python3 tools/generate_students_csv.py \
  --rows 10 \
  --output data/generated/students_sample10.csv \
  --seed 42
```

## 1,000,000건 생성

```bash
python3 tools/generate_students_csv.py \
  --rows 1000000 \
  --output data/generated/students_1m.csv \
  --seed 42 \
  --start-id 1
```

## 옵션

| 옵션 | 설명 | 기본값 |
| --- | --- | --- |
| `--rows` | 생성할 row 수 | `1000000` |
| `--output` | 출력 CSV 경로 | `data/generated/students_1m.csv` |
| `--seed` | 같은 데이터를 다시 만들기 위한 seed | 없음 |
| `--start-id` | 시작 학번/id | `1` |
| `--locale` | Faker locale | `ko_KR` |

## 데이터 범위

- `id`: `--start-id`부터 1씩 증가
- `name`: Faker가 생성한 이름
- `grade`: 1 이상 4 이하 정수
- `age`: 학년에 맞춘 범위의 정수
- `region`: Faker가 생성한 지역명
- `score`: `0.00` 이상 `4.50` 이하, 소수점 2자리

## 도움말

```bash
python3 tools/generate_students_csv.py --help
```

