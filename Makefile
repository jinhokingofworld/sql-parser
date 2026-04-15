CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude
PYTHON ?= python3

TARGET := sql_processor
LIB_SRCS := src/cli.c src/tokenizer.c src/parser.c src/schema.c src/storage.c src/bptree.c src/db_context.c src/executor.c src/utils.c
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
        t u bt all csd cld psd pld vfid vfage vfsco vfcol vfcnt bm




all: $(TARGET)

$(TARGET): src/main.c $(LIB_SRCS)
	$(CC) $(CFLAGS) -o $@ src/main.c $(LIB_SRCS)

tests/unit/test_tokenizer: tests/unit/test_tokenizer.c $(LIB_SRCS)
	$(CC) $(CFLAGS) -o $@ tests/unit/test_tokenizer.c $(LIB_SRCS)

tests/unit/test_parser: tests/unit/test_parser.c $(LIB_SRCS)
	$(CC) $(CFLAGS) -o $@ tests/unit/test_parser.c $(LIB_SRCS)

tests/unit/test_storage: tests/unit/test_storage.c $(LIB_SRCS)
	$(CC) $(CFLAGS) -Itests/support -o $@ tests/unit/test_storage.c $(LIB_SRCS) $(TEST_SUPPORT_SRCS)

tests/unit/test_executor: tests/unit/test_executor.c $(LIB_SRCS)
	$(CC) $(CFLAGS) -o $@ tests/unit/test_executor.c $(LIB_SRCS)

tests/unit/test_bptree_contract: tests/unit/test_bptree_contract.c tests/support/unity.c src/bptree.c src/utils.c
	$(CC) $(CFLAGS) -Itests/support -o $@ tests/unit/test_bptree_contract.c tests/support/unity.c src/bptree.c src/utils.c

tests/verify/verify_students_dataset: tests/verify/verify_students_dataset.c $(LIB_SRCS)
	$(CC) $(CFLAGS) -Itests/support -o $@ tests/verify/verify_students_dataset.c tests/support/test_helpers.c $(LIB_SRCS)

tests/bench/benchmark_query_paths: tests/bench/benchmark_query_paths.c $(LIB_SRCS)
	$(CC) $(CFLAGS) -Itests/support -o $@ tests/bench/benchmark_query_paths.c $(LIB_SRCS) tests/support/test_helpers.c

unit: $(UNIT_TEST_BINS)
	./tests/unit/test_tokenizer
	./tests/unit/test_parser
	./tests/unit/test_storage
	./tests/unit/test_executor

bptree-contract: $(BPTREE_TEST_BINS)
	./tests/unit/test_bptree_contract

verify: $(VERIFY_BINS)

benchmark: $(BENCH_BINS)

benchmark-run: $(BENCH_BINS)
	./tests/bench/benchmark_query_paths $(BENCH_ROWS) $(BENCH_ITERATIONS)

gen-small:
	$(PYTHON) tools/generate_students_csv.py --rows $(SMALL_ROWS) --output $(SMALL_OUTPUT) --seed $(DATA_SEED) --start-id $(DATA_START_ID)

gen-large:
	$(PYTHON) tools/generate_students_csv.py --rows $(LARGE_ROWS) --output $(LARGE_OUTPUT) --seed $(DATA_SEED) --start-id $(DATA_START_ID)

verify-small: $(VERIFY_BINS) gen-small
	./tests/verify/verify_students_dataset $(SMALL_OUTPUT) $(SMALL_ROWS) $(DATA_START_ID)

verify-large: $(VERIFY_BINS) gen-large
	./tests/verify/verify_students_dataset $(LARGE_OUTPUT) $(LARGE_ROWS) $(DATA_START_ID)

verify-fail-id: $(VERIFY_BINS)
	! ./tests/verify/verify_students_dataset tests/verify/fixtures/students_fail_id.csv 3 1

verify-fail-age: $(VERIFY_BINS)
	! ./tests/verify/verify_students_dataset tests/verify/fixtures/students_fail_age.csv 3 1

verify-fail-score: $(VERIFY_BINS)
	! ./tests/verify/verify_students_dataset tests/verify/fixtures/students_fail_score.csv 3 1

verify-fail-columns: $(VERIFY_BINS)
	! ./tests/verify/verify_students_dataset tests/verify/fixtures/students_fail_columns.csv 3 1

verify-fail-count: $(VERIFY_BINS)
	! ./tests/verify/verify_students_dataset tests/verify/fixtures/students_fail_id.csv 2 1

integration: $(TARGET)
	TMP_DB=$$(mktemp -d /tmp/sql-parser-integration-XXXXXX); \
	mkdir -p "$$TMP_DB/schema" "$$TMP_DB/tables"; \
	cp tests/fixtures/db/schema/users.schema "$$TMP_DB/schema/users.schema"; \
	cp tests/fixtures/db/tables/users.csv "$$TMP_DB/tables/users.csv"; \
	./$(TARGET) --sql tests/integration/insert_select.sql --db "$$TMP_DB" > "$$TMP_DB/insert_select.out"; \
	diff -u --strip-trailing-cr tests/integration/insert_select.expected "$$TMP_DB/insert_select.out"
	TMP_DB=$$(mktemp -d /tmp/sql-parser-integration-XXXXXX); \
	mkdir -p "$$TMP_DB/schema" "$$TMP_DB/tables"; \
	cp tests/fixtures/db/schema/users.schema "$$TMP_DB/schema/users.schema"; \
	cp tests/fixtures/db/tables/users.csv "$$TMP_DB/tables/users.csv"; \
	./$(TARGET) --sql tests/integration/select_where.sql --db "$$TMP_DB" > "$$TMP_DB/select_where.out"; \
	diff -u --strip-trailing-cr tests/integration/select_where.expected "$$TMP_DB/select_where.out"
	TMP_DB=$$(mktemp -d /tmp/sql-parser-integration-XXXXXX); \
	mkdir -p "$$TMP_DB/schema" "$$TMP_DB/tables"; \
	cp tests/fixtures/db/schema/users.schema "$$TMP_DB/schema/users.schema"; \
	cp tests/fixtures/db/tables/users.csv "$$TMP_DB/tables/users.csv"; \
	./$(TARGET) --sql tests/integration/select_order_by.sql --db "$$TMP_DB" > "$$TMP_DB/select_order_by.out"; \
	diff -u --strip-trailing-cr tests/integration/select_order_by.expected "$$TMP_DB/select_order_by.out"
	TMP_DB=$$(mktemp -d /tmp/sql-parser-integration-XXXXXX); \
	mkdir -p "$$TMP_DB/schema" "$$TMP_DB/tables"; \
	cp tests/fixtures/db/schema/users.schema "$$TMP_DB/schema/users.schema"; \
	cp tests/fixtures/db/tables/users.csv "$$TMP_DB/tables/users.csv"; \
	./$(TARGET) --sql tests/integration/select_primary_key.sql --db "$$TMP_DB" > "$$TMP_DB/select_primary_key.out"; \
	diff -u --strip-trailing-cr tests/integration/select_primary_key.expected "$$TMP_DB/select_primary_key.out"
	TMP_DB=$$(mktemp -d /tmp/sql-parser-integration-XXXXXX); \
	mkdir -p "$$TMP_DB/schema" "$$TMP_DB/tables"; \
	cp tests/fixtures/db/schema/users.schema "$$TMP_DB/schema/users.schema"; \
	cp tests/fixtures/db/tables/users.csv "$$TMP_DB/tables/users.csv"; \
	./$(TARGET) --sql tests/integration/insert_then_select_primary_key.sql --db "$$TMP_DB" > "$$TMP_DB/insert_then_select_primary_key.out"; \
	diff -u --strip-trailing-cr tests/integration/insert_then_select_primary_key.expected "$$TMP_DB/insert_then_select_primary_key.out"

test: unit bptree-contract integration

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
	rm -f $(TARGET) $(UNIT_TEST_BINS) $(BPTREE_TEST_BINS) $(VERIFY_BINS) $(BENCH_BINS)
