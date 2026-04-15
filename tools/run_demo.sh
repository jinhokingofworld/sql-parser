#!/bin/sh

set -eu

TARGET="./sql_demo_helper"
DEMO_DB_ROOT="./data/demo"

run_success_scenario() {
    scenario_name="$1"
    description="$2"
    sql_path="$3"

    printf "[DEMO] %s\n" "${scenario_name}"
    printf "[INFO] %s\n" "${description}"
    printf "[DB] %s\n\n" "${DEMO_DB_ROOT}"
    "${TARGET}" --sql "${sql_path}" --db "${DEMO_DB_ROOT}"
}

run_failure_scenario() {
    scenario_name="$1"
    description="$2"
    sql_path="$3"
    expected_error="$4"
    output_path="${DEMO_DB_ROOT}/demo_fail.out"

    printf "[DEMO] %s\n" "${scenario_name}"
    printf "[INFO] %s\n" "${description}"
    printf "[DB] %s\n\n" "${DEMO_DB_ROOT}"

    if "${TARGET}" --sql "${sql_path}" --db "${DEMO_DB_ROOT}" > "${output_path}" 2>&1; then
        cat "${output_path}"
        rm -f "${output_path}"
        printf "[FAIL] %s expected a non-zero exit\n" "${scenario_name}" >&2
        exit 1
    fi

    if ! grep -F "${expected_error}" "${output_path}" > /dev/null; then
        cat "${output_path}"
        rm -f "${output_path}"
        printf "[FAIL] %s missing expected error: %s\n" "${scenario_name}" "${expected_error}" >&2
        exit 1
    fi

    cat "${output_path}"
    rm -f "${output_path}"
}

run_demo_case() {
    case "$1" in
        users)
            run_success_scenario \
                "users auto-increment" \
                "영속 demo DB에서 auto-increment INSERT와 인덱스/선형 조회를 함께 확인합니다." \
                "tests/demo/demo_users.sql"
            ;;
        range)
            run_success_scenario \
                "range lookup" \
                "영속 demo DB에서 WHERE id BETWEEN 조회를 수행합니다." \
                "tests/demo/demo_range.sql"
            ;;
        fail)
            run_failure_scenario \
                "reserved id failure" \
                "영속 demo DB에서 수동 id 입력 금지 오류를 확인합니다." \
                "tests/demo/demo_fail.sql" \
                "column 'id' is reserved and cannot be specified manually"
            ;;
        *)
            printf "usage: sh tools/run_demo.sh {users|range|fail|all}\n" >&2
            exit 1
            ;;
    esac
}

scenario="${1:-all}"

case "${scenario}" in
    all)
        run_demo_case users
        printf "\n"
        run_demo_case range
        printf "\n"
        run_demo_case fail
        ;;
    users|range|fail)
        run_demo_case "${scenario}"
        ;;
    *)
        printf "usage: sh tools/run_demo.sh {users|range|fail|all}\n" >&2
        exit 1
        ;;
esac
