# Student CSV SQL Loader

`tools/load_students_sql.py` converts generated student CSV rows into batched SQL `INSERT` statements and runs them through `sql_processor`.

Input CSV columns:

```text
id,name,grade,age,region,score
```

Each input row is converted into a normal SQL `INSERT` using the same six columns.

## Example

```bash
py -3 tools/load_students_sql.py ^
  --input data/generated/students_sample10.csv ^
  --db-root data ^
  --sql-processor .\\sql_processor.exe ^
  --truncate ^
  --batch-size 100
```

If your machine does not provide `py -3`, replace it with the Python 3 command available in your environment.

## When To Use It

- use this loader when you want to measure or demonstrate inserts going through the SQL engine
- use `load_students_csv.py` instead when you only want to prepare a large dataset quickly

## Notes

- string values are SQL-escaped before batching
- temporary batch SQL files are removed automatically unless `--keep-batch-sql` is used
