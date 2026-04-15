#!/usr/bin/env python3
"""Bulk-load row-only student CSV data into the file DB table CSV."""

from __future__ import annotations

import argparse
import csv
import math
import sys
from dataclasses import dataclass
from pathlib import Path


DEFAULT_DB_ROOT = Path("data")
DEFAULT_TABLE = "students"
SUPPORTED_TYPES = {"int", "string", "float"}


@dataclass(frozen=True)
class Column:
    name: str
    type_name: str


@dataclass(frozen=True)
class Schema:
    table_name: str
    columns: list[Column]
    primary_key: str | None

    @property
    def primary_key_index(self) -> int | None:
        if self.primary_key is None:
            return None
        for index, column in enumerate(self.columns):
            if column.name == self.primary_key:
                return index
        raise ValueError(f"primary key column `{self.primary_key}` not found in schema")


def positive_int(value: str) -> int:
    parsed = int(value)
    if parsed <= 0:
        raise argparse.ArgumentTypeError("must be greater than 0")
    return parsed


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Bulk-load row-only CSV records into data/tables/<table>.csv"
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
        "--truncate",
        action="store_true",
        help="clear the target table before loading",
    )
    parser.add_argument(
        "--batch-size",
        type=positive_int,
        default=100_000,
        help="progress reporting interval (default: 100000)",
    )
    parser.add_argument(
        "--trust-sequential-pk",
        action="store_true",
        help="skip in-input duplicate checks when primary key values are strictly increasing",
    )
    return parser


def parse_schema(schema_path: Path, expected_table: str) -> Schema:
    table_name: str | None = None
    columns: list[Column] | None = None
    primary_key: str | None = None

    with schema_path.open("r", encoding="utf-8") as schema_file:
        for raw_line in schema_file:
            line = raw_line.strip()
            if not line:
                continue
            if line.startswith("table="):
                table_name = line.removeprefix("table=")
            elif line.startswith("columns="):
                column_defs = line.removeprefix("columns=").split(",")
                parsed_columns: list[Column] = []
                for column_def in column_defs:
                    if ":" not in column_def:
                        raise ValueError(f"invalid column definition `{column_def}`")
                    name, type_name = column_def.split(":", 1)
                    type_name = type_name.lower()
                    if type_name not in SUPPORTED_TYPES:
                        raise ValueError(f"unsupported column type `{type_name}`")
                    parsed_columns.append(Column(name=name, type_name=type_name))
                columns = parsed_columns
            elif line.startswith("pkey="):
                primary_key = line.removeprefix("pkey=")

    if table_name is None or columns is None:
        raise ValueError(f"invalid schema `{schema_path}`")
    if table_name != expected_table:
        raise ValueError(f"schema/table mismatch: expected `{expected_table}`, got `{table_name}`")
    if primary_key is not None and primary_key not in {column.name for column in columns}:
        raise ValueError(f"primary key column `{primary_key}` not found in schema")

    return Schema(table_name=table_name, columns=columns, primary_key=primary_key)


def validate_int(value: str) -> None:
    if value == "":
        raise ValueError("empty int value")
    try:
        int(value, 10)
    except ValueError as exc:
        raise ValueError(f"invalid int value `{value}`") from exc


def validate_float(value: str) -> None:
    if value == "":
        raise ValueError("empty float value")
    try:
        parsed = float(value)
    except ValueError as exc:
        raise ValueError(f"invalid float value `{value}`") from exc
    if not math.isfinite(parsed):
        raise ValueError(f"invalid float value `{value}`")


def validate_row(row: list[str], schema: Schema, line_number: int) -> None:
    if len(row) != len(schema.columns):
        raise ValueError(
            f"line {line_number}: expected {len(schema.columns)} columns, got {len(row)}"
        )

    for value, column in zip(row, schema.columns):
        try:
            if column.type_name == "int":
                validate_int(value)
            elif column.type_name == "float":
                validate_float(value)
        except ValueError as exc:
            raise ValueError(f"line {line_number}, column `{column.name}`: {exc}") from exc


def read_existing_primary_keys(table_path: Path, schema: Schema) -> set[str]:
    primary_key_index = schema.primary_key_index
    if primary_key_index is None or not table_path.exists() or table_path.stat().st_size == 0:
        return set()

    primary_keys: set[str] = set()
    with table_path.open("r", newline="", encoding="utf-8") as table_file:
        reader = csv.reader(table_file)
        for line_number, row in enumerate(reader, start=1):
            validate_row(row, schema, line_number)
            primary_key = row[primary_key_index]
            if primary_key in primary_keys:
                raise ValueError(f"existing table has duplicate primary key `{primary_key}`")
            primary_keys.add(primary_key)
    return primary_keys


def ensure_row_boundary(table_path: Path) -> None:
    if not table_path.exists() or table_path.stat().st_size == 0:
        return

    with table_path.open("rb+") as table_file:
        table_file.seek(-1, 2)
        if table_file.read(1) != b"\n":
            table_file.seek(0, 2)
            table_file.write(b"\n")


def load_csv(args: argparse.Namespace) -> int:
    input_path = args.input
    schema_path = args.db_root / "schema" / f"{args.table}.schema"
    table_path = args.db_root / "tables" / f"{args.table}.csv"

    if input_path.resolve() == table_path.resolve():
        raise ValueError("input CSV and target table CSV must be different files")
    if not input_path.exists():
        raise FileNotFoundError(input_path)
    if not schema_path.exists():
        raise FileNotFoundError(schema_path)

    schema = parse_schema(schema_path, args.table)
    table_path.parent.mkdir(parents=True, exist_ok=True)

    existing_primary_keys = set() if args.truncate else read_existing_primary_keys(table_path, schema)
    seen_primary_keys: set[str] = set()
    last_primary_key: int | None = None
    primary_key_index = schema.primary_key_index
    loaded = 0

    if args.truncate:
        table_mode = "w"
    else:
        table_mode = "a"
        ensure_row_boundary(table_path)

    with input_path.open("r", newline="", encoding="utf-8") as input_file:
        reader = csv.reader(input_file)
        with table_path.open(table_mode, newline="", encoding="utf-8") as table_file:
            writer = csv.writer(table_file, lineterminator="\n")
            for line_number, row in enumerate(reader, start=1):
                validate_row(row, schema, line_number)

                if primary_key_index is not None:
                    primary_key = row[primary_key_index]
                    if primary_key in existing_primary_keys:
                        raise ValueError(
                            f"line {line_number}: duplicate primary key `{primary_key}` already exists"
                        )
                    if args.trust_sequential_pk:
                        parsed_primary_key = int(primary_key, 10)
                        if last_primary_key is not None and parsed_primary_key <= last_primary_key:
                            raise ValueError(
                                "primary key values are not strictly increasing; "
                                "rerun without --trust-sequential-pk"
                            )
                        last_primary_key = parsed_primary_key
                    elif primary_key in seen_primary_keys:
                        raise ValueError(f"line {line_number}: duplicate primary key `{primary_key}`")
                    else:
                        seen_primary_keys.add(primary_key)

                writer.writerow(row)
                loaded += 1
                if loaded % args.batch_size == 0:
                    print(f"loaded {loaded} rows...", file=sys.stderr)

    print(f"loaded {loaded} rows -> {table_path}")
    return loaded


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()

    try:
        load_csv(args)
    except (OSError, ValueError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
