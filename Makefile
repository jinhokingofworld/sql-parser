CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude
PYTHON ?= python3

TARGET := sql_processor
DEMO_TARGET := sql_demo_helper
LIB_SRCS := src/cli.c src/tokenizer.c src/parser.c src/schema.c src/storage.c src/bptree.c src/db_context.c src/executor.c src/utils.c src/bench.c
UNIT_TEST_BINS := tests/unit/test_tokenizer tests/unit/test_parser tests/unit/test_storage tests/unit/test_executor
TEST_SUPPORT_SRCS := tests/support/unity.c tests/support/test_helpers.c
BPTREE_TEST_BINS := tests/unit/test_bptree_contract
VERIFY_BINS := tests/verify/verify_students_dataset
BENCH_BINS := tests/bench/benchmark_query_paths
SMALL_ROWS ?= 1000
LARGE_ROWS ?= 1000000
SMALL_OUTPUT ?= data/generated/students_1k.csv
LARGE_OUTPUT ?= data/generated/students_1m.csv
DATA_SEED ?= 42
DATA_START_ID ?= 1
BENCH_ROWS ?= 10000
BENCH_ITERATIONS ?= 200

.PHONY: all clean test unit integration bptree-contract verify benchmark benchmark-run \
        gen-small gen-large verify-small verify-large verify-fail-id verify-fail-age \
        verify-fail-score verify-fail-columns verify-fail-count \
        demo demo-users demo-range demo-fail \
        t u bt all csd cld psd pld vfid vfage vfsco vfcol vfcnt bm




all: $(TARGET)

$(TARGET): src/main.c $(LIB_SRCS)
	@printf "[BUILD] %s\n" "$@"
	@$(CC) $(CFLAGS) -o $@ src/main.c $(LIB_SRCS)

$(DEMO_TARGET): src/demo_helper.c $(LIB_SRCS)
	@printf "[BUILD] %s\n" "$@"
	@$(CC) $(CFLAGS) -o $@ src/demo_helper.c $(LIB_SRCS)

tests/unit/test_tokenizer: tests/unit/test_tokenizer.c $(LIB_SRCS)
	@printf "[BUILD] %s\n" "$@"
	@$(CC) $(CFLAGS) -o $@ tests/unit/test_tokenizer.c $(LIB_SRCS)

tests/unit/test_parser: tests/unit/test_parser.c $(LIB_SRCS)
	@printf "[BUILD] %s\n" "$@"
	@$(CC) $(CFLAGS) -o $@ tests/unit/test_parser.c $(LIB_SRCS)

tests/unit/test_storage: tests/unit/test_storage.c $(LIB_SRCS)
	@printf "[BUILD] %s\n" "$@"
	@$(CC) $(CFLAGS) -Itests/support -o $@ tests/unit/test_storage.c $(LIB_SRCS) $(TEST_SUPPORT_SRCS)

tests/unit/test_executor: tests/unit/test_executor.c $(LIB_SRCS)
	@printf "[BUILD] %s\n" "$@"
	@$(CC) $(CFLAGS) -o $@ tests/unit/test_executor.c $(LIB_SRCS)

tests/unit/test_bptree_contract: tests/unit/test_bptree_contract.c tests/support/unity.c src/bptree.c src/utils.c
	@printf "[BUILD] %s\n" "$@"
	@$(CC) $(CFLAGS) -Itests/support -o $@ tests/unit/test_bptree_contract.c tests/support/unity.c src/bptree.c src/utils.c

tests/verify/verify_students_dataset: tests/verify/verify_students_dataset.c $(LIB_SRCS)
	@printf "[BUILD] %s\n" "$@"
	@$(CC) $(CFLAGS) -Itests/support -o $@ tests/verify/verify_students_dataset.c tests/support/test_helpers.c $(LIB_SRCS)

tests/bench/benchmark_query_paths: tests/bench/benchmark_query_paths.c $(LIB_SRCS)
	@printf "[BUILD] %s\n" "$@"
	@$(CC) $(CFLAGS) -Itests/support -o $@ tests/bench/benchmark_query_paths.c $(LIB_SRCS) tests/support/test_helpers.c

unit: $(UNIT_TEST_BINS)
	@printf "[TEST] unit\n"
	@./tests/unit/test_tokenizer
	@./tests/unit/test_parser
	@./tests/unit/test_storage
	@./tests/unit/test_executor
	@printf "[PASS] unit\n"

bptree-contract: $(BPTREE_TEST_BINS)
	@printf "[TEST] bptree-contract\n"
	@./tests/unit/test_bptree_contract
	@printf "[PASS] bptree-contract\n"

verify: $(VERIFY_BINS)

benchmark: $(BENCH_BINS)

benchmark-run: $(BENCH_BINS)
	@printf "[BENCH] rows=%s iterations=%s\n" "$(BENCH_ROWS)" "$(BENCH_ITERATIONS)"
	@./tests/bench/benchmark_query_paths $(BENCH_ROWS) $(BENCH_ITERATIONS)

gen-small:
	@printf "[DATA] generate %s rows -> %s\n" "$(SMALL_ROWS)" "$(SMALL_OUTPUT)"
	@$(PYTHON) tools/generate_students_csv.py --rows $(SMALL_ROWS) --output $(SMALL_OUTPUT) --seed $(DATA_SEED) --start-id $(DATA_START_ID)

gen-large:
	@printf "[DATA] generate %s rows -> %s\n" "$(LARGE_ROWS)" "$(LARGE_OUTPUT)"
	@$(PYTHON) tools/generate_students_csv.py --rows $(LARGE_ROWS) --output $(LARGE_OUTPUT) --seed $(DATA_SEED) --start-id $(DATA_START_ID)

verify-small: $(VERIFY_BINS) gen-small
	@printf "[VERIFY] %s\n" "$(SMALL_OUTPUT)"
	@./tests/verify/verify_students_dataset $(SMALL_OUTPUT) $(SMALL_ROWS) $(DATA_START_ID)
	@printf "[PASS] verify-small\n"

verify-large: $(VERIFY_BINS) gen-large
	@printf "[VERIFY] %s\n" "$(LARGE_OUTPUT)"
	@./tests/verify/verify_students_dataset $(LARGE_OUTPUT) $(LARGE_ROWS) $(DATA_START_ID)
	@printf "[PASS] verify-large\n"

verify-fail-id: $(VERIFY_BINS)
	@printf "[VERIFY] expected failure students_fail_id.csv\n"
	@! ./tests/verify/verify_students_dataset tests/verify/fixtures/students_fail_id.csv 3 1
	@printf "[PASS] verify-fail-id\n"

verify-fail-age: $(VERIFY_BINS)
	@printf "[VERIFY] expected failure students_fail_age.csv\n"
	@! ./tests/verify/verify_students_dataset tests/verify/fixtures/students_fail_age.csv 3 1
	@printf "[PASS] verify-fail-age\n"

verify-fail-score: $(VERIFY_BINS)
	@printf "[VERIFY] expected failure students_fail_score.csv\n"
	@! ./tests/verify/verify_students_dataset tests/verify/fixtures/students_fail_score.csv 3 1
	@printf "[PASS] verify-fail-score\n"

verify-fail-columns: $(VERIFY_BINS)
	@printf "[VERIFY] expected failure students_fail_columns.csv\n"
	@! ./tests/verify/verify_students_dataset tests/verify/fixtures/students_fail_columns.csv 3 1
	@printf "[PASS] verify-fail-columns\n"

verify-fail-count: $(VERIFY_BINS)
	@printf "[VERIFY] expected failure row-count mismatch\n"
	@! ./tests/verify/verify_students_dataset tests/verify/fixtures/students_fail_id.csv 2 1
	@printf "[PASS] verify-fail-count\n"

integration: $(TARGET)
	@printf "[TEST] integration\n"
	@sh tools/run_integration.sh
	@printf "[PASS] integration\n"

test: unit bptree-contract integration
	@printf "[PASS] test\n"

demo: $(DEMO_TARGET)
	@printf "[DEMO] all scenarios\n"
	@sh tools/run_demo.sh all
	@printf "[PASS] demo\n"

demo-users: $(DEMO_TARGET)
	@printf "[DEMO] users\n"
	@sh tools/run_demo.sh users
	@printf "[PASS] demo-users\n"

demo-range: $(DEMO_TARGET)
	@printf "[DEMO] range\n"
	@sh tools/run_demo.sh range
	@printf "[PASS] demo-range\n"

demo-fail: $(DEMO_TARGET)
	@printf "[DEMO] fail\n"
	@sh tools/run_demo.sh fail
	@printf "[PASS] demo-fail\n"

t: test
u: unit
bt: bptree-contract
all: integration
csd: gen-small
cld: gen-large
psd: verify-small
pld: verify-large
vfid: verify-fail-id
vfage: verify-fail-age
vfsco: verify-fail-score
vfcol: verify-fail-columns
vfcnt: verify-fail-count
bm: benchmark-run

clean:
	@printf "[CLEAN] build outputs\n"
	@rm -f $(TARGET) $(DEMO_TARGET) $(UNIT_TEST_BINS) $(BPTREE_TEST_BINS) $(VERIFY_BINS) $(BENCH_BINS)
