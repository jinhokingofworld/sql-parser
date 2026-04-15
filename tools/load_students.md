# Student CSV Direct Loader

`tools/load_students_csv.py` copies generated student rows directly into `data/tables/students.csv`.

Input CSV columns:

```text
id,name,grade,age,region,score
```

Database table schema:

```text
id,name,grade,age,region,score
```

The loader writes rows directly using the same six-column shape.

## Example

```bash
py -3 tools/load_students_csv.py ^
  --input data/generated/students_sample10.csv ^
  --db-root data ^
  --table students ^
  --truncate
```

If your machine does not provide `py -3`, replace it with the Python 3 command available in your environment.

## What It Checks

- input column count matches the schema shape
- `id`, `grade`, and `age` are valid ints
- `score` is a valid float
- existing primary key collisions are prevented

## Notes

- Use this path when you want fast bulk data preparation.
- This path bypasses SQL execution, so it is not the right benchmark for INSERT-through-engine performance.
- `--trust-sequential-pk` can be used because the generated ids are sequential.
