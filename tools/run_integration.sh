#!/bin/sh

set -eu

TARGET="./sql_processor"
FIXTURE_SCHEMA="tests/fixtures/db/schema/users.schema"
FIXTURE_TABLE="tests/fixtures/db/tables/users.csv"

cleanup_tmp() {
    if [ -n "${TMP_DB:-}" ] && [ -d "${TMP_DB}" ]; then
        rm -rf "${TMP_DB}"
    fi
}

run_case() {
    case_name="$1"
    sql_path="$2"
    expected_path="$3"
    actual_path=

    TMP_DB="$(mktemp -d /tmp/sql-parser-integration-XXXXXX)"
    trap cleanup_tmp EXIT INT TERM

    mkdir -p "${TMP_DB}/schema" "${TMP_DB}/tables"
    cp "${FIXTURE_SCHEMA}" "${TMP_DB}/schema/users.schema"
    cp "${FIXTURE_TABLE}" "${TMP_DB}/tables/users.csv"
    actual_path="${TMP_DB}/${case_name}.out"

    printf "[CASE] %s\n" "${case_name}"
    printf "[SQL] %s\n" "${sql_path}"
    printf "[RUN] %s --sql %s --db %s\n" "${TARGET}" "${sql_path}" "${TMP_DB}"
    "${TARGET}" --sql "${sql_path}" --db "${TMP_DB}" > "${actual_path}"
    diff -u --strip-trailing-cr "${expected_path}" "${actual_path}"
    printf "[PASS] %s\n" "${case_name}"

    cleanup_tmp
    trap - EXIT INT TERM
}

run_case "insert_select" "tests/integration/insert_select.sql" "tests/integration/insert_select.expected"
run_case "select_where" "tests/integration/select_where.sql" "tests/integration/select_where.expected"
run_case "select_order_by" "tests/integration/select_order_by.sql" "tests/integration/select_order_by.expected"
run_case "select_primary_key" "tests/integration/select_primary_key.sql" "tests/integration/select_primary_key.expected"
run_case \
    "insert_then_select_primary_key" \
    "tests/integration/insert_then_select_primary_key.sql" \
    "tests/integration/insert_then_select_primary_key.expected"
