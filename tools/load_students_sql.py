#!/usr/bin/env python3
"""Load row-only student CSV data by executing SQL INSERT statements."""

from __future__ import annotations

import argparse
import csv
import subprocess
import sys
import tempfile
import time
from pathlib import Path

from load_students_csv import DEFAULT_DB_ROOT, DEFAULT_TABLE, Schema, parse_schema, positive_int, validate_row


DEFAULT_SQL_PROCESSOR = Path("./sql_processor")
DEFAULT_BATCH_SIZE = 1_000


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Load row-only CSV records by executing SQL INSERT statements"
    )
    parser.add_argument(
        "--input",
        type=Path,
        required=True,
        help="row-only CSV input path",
    )
    parser.add_argument(
        "--db-root",
        type=Path,
        default=DEFAULT_DB_ROOT,
        help=f"database root containing schema/ and tables/ (default: {DEFAULT_DB_ROOT})",
    )
    parser.add_argument(
        "--table",
        default=DEFAULT_TABLE,
        help=f"target table name (default: {DEFAULT_TABLE})",
    )
    parser.add_argument(
        "--sql-processor",
        type=Path,
        default=DEFAULT_SQL_PROCESSOR,
        help=f"sql_processor executable path (default: {DEFAULT_SQL_PROCESSOR})",
    )
    parser.add_argument(
        "--truncate",
        action="store_true",
        help="clear the target table before executing INSERT statements",
    )
    parser.add_argument(
        "--batch-size",
        type=positive_int,
        default=DEFAULT_BATCH_SIZE,
        help=f"INSERT statements per sql_processor run (default: {DEFAULT_BATCH_SIZE})",
    )
    parser.add_argument(
        "--progress-interval",
        type=positive_int,
        default=10_000,
        help="progress reporting interval in loaded rows (default: 10000)",
    )
    parser.add_argument(
        "--keep-batch-sql",
        action="store_true",
        help="keep temporary SQL batch files for debugging",
    )
    return parser


def quote_sql_string(value: str) -> str:
    return "'" + value.replace("'", "''") + "'"


def sql_literal(value: str, type_name: str) -> str:
    if type_name == "string":
        return quote_sql_string(value)
    return value


def build_insert_statement(schema: Schema, row: list[str]) -> str:
    columns = ", ".join(column.name for column in schema.columns)
    values = ", ".join(
        sql_literal(value, column.type_name)
        for value, column in zip(row, schema.columns)
    )
    return f"INSERT INTO {schema.table_name} ({columns}) VALUES ({values});\n"


def truncate_table(db_root: Path, table: str) -> None:
    table_path = db_root / "tables" / f"{table}.csv"
    table_path.parent.mkdir(parents=True, exist_ok=True)
    table_path.write_text("", encoding="utf-8")


def run_batch(
    sql_processor: Path,
    db_root: Path,
    statements: list[str],
    keep_batch_sql: bool,
) -> None:
    if not statements:
        return

    batch_file = tempfile.NamedTemporaryFile(
        mode="w",
        encoding="utf-8",
        newline="",
        suffix=".sql",
        prefix="students_insert_batch_",
        delete=False,
    )
    batch_path = Path(batch_file.name)

    try:
        with batch_file:
            batch_file.writelines(statements)

        result = subprocess.run(
            [
                str(sql_processor),
                "--sql",
                str(batch_path),
                "--db",
                str(db_root),
            ],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.PIPE,
            text=True,
            check=False,
        )
        if result.returncode != 0:
            message = result.stderr.strip() or f"sql_processor exited with {result.returncode}"
            raise RuntimeError(message)
    finally:
        if not keep_batch_sql:
            try:
                batch_path.unlink()
            except FileNotFoundError:
                pass


def load_with_sql(args: argparse.Namespace) -> int:
    input_path = args.input
    schema_path = args.db_root / "schema" / f"{args.table}.schema"
    sql_processor = args.sql_processor
    if not sql_processor.is_absolute():
        sql_processor = Path.cwd() / sql_processor

    if not input_path.exists():
        raise FileNotFoundError(input_path)
    if not schema_path.exists():
        raise FileNotFoundError(schema_path)
    if not sql_processor.exists():
        raise FileNotFoundError(sql_processor)

    schema = parse_schema(schema_path, args.table)
    if args.truncate:
        truncate_table(args.db_root, args.table)

    loaded = 0
    next_progress = args.progress_interval
    statements: list[str] = []
    started_at = time.perf_counter()

    with input_path.open("r", newline="", encoding="utf-8") as input_file:
        reader = csv.reader(input_file)
        for line_number, row in enumerate(reader, start=1):
            validate_row(row, schema, line_number)
            statements.append(build_insert_statement(schema, row))

            if len(statements) >= args.batch_size:
                run_batch(sql_processor, args.db_root, statements, args.keep_batch_sql)
                loaded += len(statements)
                statements.clear()

                while loaded >= next_progress:
                    print(f"inserted {loaded} rows...", file=sys.stderr)
                    next_progress += args.progress_interval

        if statements:
            run_batch(sql_processor, args.db_root, statements, args.keep_batch_sql)
            loaded += len(statements)

    elapsed = time.perf_counter() - started_at
    print(f"inserted {loaded} rows through SQL -> {args.db_root / 'tables' / (args.table + '.csv')}")
    print(f"elapsed {elapsed:.3f}s")
    return loaded


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()

    try:
        load_with_sql(args)
    except (OSError, RuntimeError, ValueError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
