CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude
PYTHON ?= python3

TARGET := sql_processor
LIB_SRCS := src/cli.c src/tokenizer.c src/parser.c src/schema.c src/storage.c src/executor.c src/utils.c
TEST_SUPPORT_SRCS := tests/support/unity.c tests/support/test_helpers.c
UNIT_TEST_BINS := tests/unit/test_tokenizer tests/unit/test_parser tests/unit/test_storage tests/unit/test_executor
VERIFY_BINS := tests/verify/verify_students_dataset
SMALL_ROWS ?= 1000
LARGE_ROWS ?= 1000000
SMALL_OUTPUT ?= data/generated/students_1k.csv
LARGE_OUTPUT ?= data/generated/students_1m.csv
DATA_SEED ?= 42
DATA_START_ID ?= 1

.PHONY: all clean test unit integration verify gen-small gen-large verify-small verify-large verify-fail-id verify-fail-age verify-fail-score verify-fail-columns verify-fail-count

all: $(TARGET)

$(TARGET): src/main.c $(LIB_SRCS)
	$(CC) $(CFLAGS) -o $@ src/main.c $(LIB_SRCS)

tests/unit/test_tokenizer: tests/unit/test_tokenizer.c $(LIB_SRCS)
	$(CC) $(CFLAGS) -Itests/support -o $@ tests/unit/test_tokenizer.c $(LIB_SRCS) $(TEST_SUPPORT_SRCS)

tests/unit/test_parser: tests/unit/test_parser.c $(LIB_SRCS)
	$(CC) $(CFLAGS) -Itests/support -o $@ tests/unit/test_parser.c $(LIB_SRCS) $(TEST_SUPPORT_SRCS)

tests/unit/test_storage: tests/unit/test_storage.c $(LIB_SRCS)
	$(CC) $(CFLAGS) -Itests/support -o $@ tests/unit/test_storage.c $(LIB_SRCS) $(TEST_SUPPORT_SRCS)

tests/unit/test_executor: tests/unit/test_executor.c $(LIB_SRCS)
	$(CC) $(CFLAGS) -Itests/support -o $@ tests/unit/test_executor.c $(LIB_SRCS) $(TEST_SUPPORT_SRCS)

tests/verify/verify_students_dataset: tests/verify/verify_students_dataset.c $(LIB_SRCS)
	$(CC) $(CFLAGS) -Itests/support -o $@ tests/verify/verify_students_dataset.c $(LIB_SRCS) tests/support/test_helpers.c

unit: $(UNIT_TEST_BINS)
	./tests/unit/test_tokenizer
	./tests/unit/test_parser
	./tests/unit/test_storage
	./tests/unit/test_executor

verify: $(VERIFY_BINS)

gen-small:
	$(PYTHON) tools/generate_students_csv.py --rows $(SMALL_ROWS) --output $(SMALL_OUTPUT) --seed $(DATA_SEED) --start-id $(DATA_START_ID)

gen-large:
	$(PYTHON) tools/generate_students_csv.py --rows $(LARGE_ROWS) --output $(LARGE_OUTPUT) --seed $(DATA_SEED) --start-id $(DATA_START_ID)

verify-small: tests/verify/verify_students_dataset gen-small
	./tests/verify/verify_students_dataset $(SMALL_OUTPUT) $(SMALL_ROWS) $(DATA_START_ID)

verify-large: tests/verify/verify_students_dataset gen-large
	./tests/verify/verify_students_dataset $(LARGE_OUTPUT) $(LARGE_ROWS) $(DATA_START_ID)

verify-fail-id: tests/verify/verify_students_dataset
	./tests/verify/verify_students_dataset tests/verify/fixtures/students_fail_id.csv 3 1

verify-fail-age: tests/verify/verify_students_dataset
	./tests/verify/verify_students_dataset tests/verify/fixtures/students_fail_age.csv 3 1

verify-fail-score: tests/verify/verify_students_dataset
	./tests/verify/verify_students_dataset tests/verify/fixtures/students_fail_score.csv 3 1

verify-fail-columns: tests/verify/verify_students_dataset
	./tests/verify/verify_students_dataset tests/verify/fixtures/students_fail_columns.csv 3 1

verify-fail-count: tests/verify/verify_students_dataset
	./tests/verify/verify_students_dataset tests/verify/fixtures/students_fail_id.csv 2 1

integration: $(TARGET)
	TMP_DB=$$(mktemp -d /tmp/sql-parser-integration-XXXXXX); \
	mkdir -p "$$TMP_DB/schema" "$$TMP_DB/tables"; \
	cp tests/fixtures/db/schema/users.schema "$$TMP_DB/schema/users.schema"; \
	cp tests/fixtures/db/tables/users.csv "$$TMP_DB/tables/users.csv"; \
	./$(TARGET) --sql tests/integration/insert_select.sql --db "$$TMP_DB" > "$$TMP_DB/insert_select.out"; \
	cmp -s "$$TMP_DB/insert_select.out" tests/integration/insert_select.expected
	TMP_DB=$$(mktemp -d /tmp/sql-parser-integration-XXXXXX); \
	mkdir -p "$$TMP_DB/schema" "$$TMP_DB/tables"; \
	cp tests/fixtures/db/schema/users.schema "$$TMP_DB/schema/users.schema"; \
	cp tests/fixtures/db/tables/users.csv "$$TMP_DB/tables/users.csv"; \
	./$(TARGET) --sql tests/integration/select_where.sql --db "$$TMP_DB" > "$$TMP_DB/select_where.out"; \
	cmp -s "$$TMP_DB/select_where.out" tests/integration/select_where.expected
	TMP_DB=$$(mktemp -d /tmp/sql-parser-integration-XXXXXX); \
	mkdir -p "$$TMP_DB/schema" "$$TMP_DB/tables"; \
	cp tests/fixtures/db/schema/users.schema "$$TMP_DB/schema/users.schema"; \
	cp tests/fixtures/db/tables/users.csv "$$TMP_DB/tables/users.csv"; \
	./$(TARGET) --sql tests/integration/select_order_by.sql --db "$$TMP_DB" > "$$TMP_DB/select_order_by.out"; \
	cmp -s "$$TMP_DB/select_order_by.out" tests/integration/select_order_by.expected

test: unit integration

clean:
	rm -f $(TARGET) $(UNIT_TEST_BINS) $(VERIFY_BINS)
