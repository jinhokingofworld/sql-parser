#!/usr/bin/env python3
"""Generate row-only student CSV data for B+Tree index performance tests."""

from __future__ import annotations

import argparse
import csv
import random
import sys
import time
from pathlib import Path

try:
    from faker import Faker
except ImportError:  # pragma: no cover - exercised before dependency install
    Faker = None


DEFAULT_ROWS = 1_000_000
DEFAULT_OUTPUT = Path("data/generated/students_1m.csv")
DEFAULT_START_ID = 1
DEFAULT_LOCALE = "ko_KR"
PROGRESS_BAR_WIDTH = 30


def positive_int(value: str) -> int:
    parsed = int(value)
    if parsed <= 0:
        raise argparse.ArgumentTypeError("must be greater than 0")
    return parsed


def non_negative_int(value: str) -> int:
    parsed = int(value)
    if parsed < 0:
        raise argparse.ArgumentTypeError("must be greater than or equal to 0")
    return parsed


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Generate row-only CSV records: id,name,grade,age,region,score"
    )
    parser.add_argument(
        "--rows",
        type=positive_int,
        default=DEFAULT_ROWS,
        help=f"number of records to generate (default: {DEFAULT_ROWS})",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=DEFAULT_OUTPUT,
        help=f"output CSV path (default: {DEFAULT_OUTPUT})",
    )
    parser.add_argument(
        "--seed",
        type=int,
        default=None,
        help="seed for reproducible data",
    )
    parser.add_argument(
        "--start-id",
        type=non_negative_int,
        default=DEFAULT_START_ID,
        help=f"first student id to generate (default: {DEFAULT_START_ID})",
    )
    parser.add_argument(
        "--locale",
        default=DEFAULT_LOCALE,
        help=f"Faker locale for names and regions (default: {DEFAULT_LOCALE})",
    )
    return parser


def make_age(rng: random.Random, grade: int) -> int:
    min_age_by_grade = {
        1: 19,
        2: 20,
        3: 21,
        4: 22,
    }
    return rng.randint(min_age_by_grade[grade], min_age_by_grade[grade] + 4)


def progress_step(total: int) -> int:
    return max(10_000, min(50_000, total // 20 if total >= 20 else 1))


def render_progress_bar(current: int, total: int) -> str:
    filled = (current * PROGRESS_BAR_WIDTH) // total
    if filled > PROGRESS_BAR_WIDTH:
        filled = PROGRESS_BAR_WIDTH
    return "#" * filled + "." * (PROGRESS_BAR_WIDTH - filled)


class ProgressReporter:
    def __init__(self, total: int, label: str) -> None:
        self.total = total
        self.label = label
        self.step = progress_step(total)
        self.is_tty = sys.stderr.isatty()
        self.started_at = time.time()
        self.last_reported = 0

    def update(self, current: int) -> None:
        if current < self.total and current - self.last_reported < self.step:
            return

        elapsed = time.time() - self.started_at
        if self.is_tty:
            percent = (current * 100) // self.total
            bar = render_progress_bar(current, self.total)
            print(
                f"\r[{self.label}] [{bar}] {percent:3d}% "
                f"({current}/{self.total}) elapsed={elapsed:0.1f}s",
                end="",
                file=sys.stderr,
                flush=True,
            )
            if current >= self.total:
                print(file=sys.stderr, flush=True)
        else:
            print(
                f"[{self.label}] {current}/{self.total} rows written "
                f"({(current * 100) // self.total}%) elapsed={elapsed:0.1f}s",
                file=sys.stderr,
                flush=True,
            )

        self.last_reported = current


def generate_rows(args: argparse.Namespace):
    if Faker is None:
        raise RuntimeError("Faker is not installed. Install it with: python3 -m pip install Faker")

    fake = Faker(args.locale)
    rng = random.Random(args.seed)
    if args.seed is not None:
        Faker.seed(args.seed)
        fake.seed_instance(args.seed)

    for offset in range(args.rows):
        student_id = args.start_id + offset
        grade = rng.randint(1, 4)
        age = make_age(rng, grade)
        score = round(rng.uniform(0.0, 4.5), 2)

        yield [
            student_id,
            fake.name(),
            grade,
            age,
            fake.city(),
            f"{score:.2f}",
        ]


def write_csv(args: argparse.Namespace) -> int:
    reporter = ProgressReporter(args.rows, "GEN")

    args.output.parent.mkdir(parents=True, exist_ok=True)

    with args.output.open("w", newline="", encoding="utf-8") as csv_file:
        writer = csv.writer(csv_file)
        for index, row in enumerate(generate_rows(args), start=1):
            writer.writerow(row)
            reporter.update(index)

    return args.rows


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()

    try:
        written = write_csv(args)
    except (OSError, RuntimeError, ValueError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1

    print(f"generated {written} rows -> {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
