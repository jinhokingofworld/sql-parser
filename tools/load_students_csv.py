#!/usr/bin/env python3
"""Bulk-load student CSV data into the file DB table CSV."""

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
    autoincrement: bool

    @property
    def primary_key_index(self) -> int | None:
        if self.primary_key is None:
            return None
        for index, column in enumerate(self.columns):
            if column.name == self.primary_key:
                return index
        raise ValueError(f"primary key column `{self.primary_key}` not found in schema")


@dataclass(frozen=True)
class InputShape:
    columns: list[Column]
    omits_autoincrement: bool


def positive_int(value: str) -> int:
    parsed = int(value)
    if parsed <= 0:
        raise argparse.ArgumentTypeError("must be greater than 0")
    return parsed


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description=(
            "Bulk-load student CSV records into data/tables/<table>.csv. "
            "When the schema uses autoincrement id, input rows may omit that id column."
        )
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
        help="skip in-input duplicate checks when the input already contains a strictly increasing primary key",
    )
    return parser


def parse_schema(schema_path: Path, expected_table: str) -> Schema:
    table_name: str | None = None
    columns: list[Column] | None = None
    primary_key: str | None = None
    autoincrement = False

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
            elif line.startswith("autoincrement="):
                raw_value = line.removeprefix("autoincrement=").strip().lower()
                if raw_value not in {"true", "false"}:
                    raise ValueError(f"invalid autoincrement value `{raw_value}`")
                autoincrement = raw_value == "true"

    if table_name is None or columns is None:
        raise ValueError(f"invalid schema `{schema_path}`")
    if table_name != expected_table:
        raise ValueError(f"schema/table mismatch: expected `{expected_table}`, got `{table_name}`")
    if primary_key is not None and primary_key not in {column.name for column in columns}:
        raise ValueError(f"primary key column `{primary_key}` not found in schema")

    return Schema(
        table_name=table_name,
        columns=columns,
        primary_key=primary_key,
        autoincrement=autoincrement,
    )


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


def resolve_input_shape(schema: Schema, row_length: int, line_number: int) -> InputShape:
    full_length = len(schema.columns)
    primary_key_index = schema.primary_key_index

    if row_length == full_length:
        return InputShape(columns=schema.columns, omits_autoincrement=False)

    if schema.autoincrement and primary_key_index is not None and row_length == full_length - 1:
        columns = schema.columns[:primary_key_index] + schema.columns[primary_key_index + 1:]
        return InputShape(columns=columns, omits_autoincrement=True)

    if schema.autoincrement and primary_key_index is not None:
        expected = f"{full_length - 1} or {full_length}"
    else:
        expected = str(full_length)

    raise ValueError(f"line {line_number}: expected {expected} columns, got {row_length}")


def validate_values(row: list[str], columns: list[Column], line_number: int) -> None:
    for value, column in zip(row, columns):
        try:
            if column.type_name == "int":
                validate_int(value)
            elif column.type_name == "float":
                validate_float(value)
        except ValueError as exc:
            raise ValueError(f"line {line_number}, column `{column.name}`: {exc}") from exc


def read_existing_primary_keys(table_path: Path, schema: Schema) -> tuple[set[str], int]:
    primary_key_index = schema.primary_key_index
    max_primary_key = 0

    if primary_key_index is None or not table_path.exists() or table_path.stat().st_size == 0:
        return set(), 1

    if schema.autoincrement and schema.columns[primary_key_index].type_name != "int":
        raise ValueError("autoincrement primary key must be an int column")

    primary_keys: set[str] = set()
    with table_path.open("r", newline="", encoding="utf-8") as table_file:
        reader = csv.reader(table_file)
        for line_number, row in enumerate(reader, start=1):
            shape = resolve_input_shape(schema, len(row), line_number)
            if shape.omits_autoincrement:
                raise ValueError("existing table rows cannot omit the autoincrement column")
            validate_values(row, shape.columns, line_number)
            primary_key = row[primary_key_index]
            if primary_key in primary_keys:
                raise ValueError(f"existing table has duplicate primary key `{primary_key}`")
            primary_keys.add(primary_key)
            if schema.autoincrement:
                max_primary_key = max(max_primary_key, int(primary_key, 10))

    return primary_keys, max_primary_key + 1


def ensure_row_boundary(table_path: Path) -> None:
    if not table_path.exists() or table_path.stat().st_size == 0:
        return

    with table_path.open("rb+") as table_file:
        table_file.seek(-1, 2)
        if table_file.read(1) != b"\n":
            table_file.seek(0, 2)
            table_file.write(b"\n")


def materialize_db_row(
    row: list[str],
    schema: Schema,
    line_number: int,
    next_generated_primary_key: int,
) -> tuple[list[str], bool]:
    shape = resolve_input_shape(schema, len(row), line_number)
    validate_values(row, shape.columns, line_number)

    if not shape.omits_autoincrement:
        return list(row), False

    primary_key_index = schema.primary_key_index
    if primary_key_index is None:
        raise ValueError("autoincrement schema requires a primary key")

    db_row = list(row)
    db_row.insert(primary_key_index, str(next_generated_primary_key))
    return db_row, True


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

    if args.truncate:
        existing_primary_keys = set()
        next_generated_primary_key = 1
        table_mode = "w"
    else:
        existing_primary_keys, next_generated_primary_key = read_existing_primary_keys(table_path, schema)
        table_mode = "a"
        ensure_row_boundary(table_path)

    seen_primary_keys: set[str] = set()
    last_primary_key: int | None = None
    primary_key_index = schema.primary_key_index
    loaded = 0

    with input_path.open("r", newline="", encoding="utf-8") as input_file:
        reader = csv.reader(input_file)
        with table_path.open(table_mode, newline="", encoding="utf-8") as table_file:
            writer = csv.writer(table_file, lineterminator="\n")
            for line_number, row in enumerate(reader, start=1):
                db_row, generated_primary_key = materialize_db_row(
                    row,
                    schema,
                    line_number,
                    next_generated_primary_key,
                )

                if generated_primary_key:
                    next_generated_primary_key += 1

                if primary_key_index is not None:
                    primary_key = db_row[primary_key_index]
                    if primary_key in existing_primary_keys:
                        raise ValueError(
                            f"line {line_number}: duplicate primary key `{primary_key}` already exists"
                        )
                    if not generated_primary_key:
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

                writer.writerow(db_row)
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
