# Student CSV Generator

`tools/generate_students_csv.py` generates row-only input data for the student demo.

Generated CSV columns:

```text
id,name,grade,age,region,score
```

This file uses `id` as the student number itself and as the primary key.

## Install

```bash
py -3 -m pip install -r requirements.txt
```

If your machine does not provide `py -3`, replace it with the Python 3 command available in your environment.

## Example

```bash
py -3 tools/generate_students_csv.py ^
  --rows 10 ^
  --output data/generated/students_sample10.csv ^
  --seed 42
```

## Options

| Option | Meaning | Default |
| --- | --- | --- |
| `--rows` | number of rows to generate | `1000000` |
| `--output` | output CSV path | `data/generated/students_1m.csv` |
| `--seed` | reproducible seed | none |
| `--start-id` | first `id` value | `20250001` |
| `--locale` | Faker locale | `ko_KR` |

## Notes

- `grade`: integer in `1..4`
- `age`: derived from grade, then randomized in a realistic range
- `region`: generated with Faker city names
- `score`: float in `0.00..4.50`
