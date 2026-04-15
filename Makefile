CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude

TARGET := sql_processor
LIB_SRCS := src/cli.c src/tokenizer.c src/parser.c src/schema.c src/storage.c src/executor.c src/utils.c src/bptree.c src/db_context.c
TARGET_SRCS := src/main.c bench/bench.c $(LIB_SRCS)
UNIT_TEST_BINS := tests/unit/test_tokenizer tests/unit/test_parser tests/unit/test_storage tests/unit/test_executor tests/unit/test_bptree

.PHONY: all clean test unit integration

all: $(TARGET)

$(TARGET): $(TARGET_SRCS)
	$(CC) $(CFLAGS) -o $@ $(TARGET_SRCS)

tests/unit/test_tokenizer: tests/unit/test_tokenizer.c $(LIB_SRCS)
	$(CC) $(CFLAGS) -o $@ tests/unit/test_tokenizer.c $(LIB_SRCS)

tests/unit/test_parser: tests/unit/test_parser.c $(LIB_SRCS)
	$(CC) $(CFLAGS) -o $@ tests/unit/test_parser.c $(LIB_SRCS)

tests/unit/test_storage: tests/unit/test_storage.c $(LIB_SRCS)
	$(CC) $(CFLAGS) -o $@ tests/unit/test_storage.c $(LIB_SRCS)

tests/unit/test_executor: tests/unit/test_executor.c $(LIB_SRCS)
	$(CC) $(CFLAGS) -o $@ tests/unit/test_executor.c $(LIB_SRCS)

tests/unit/test_bptree: tests/unit/test_bptree.c src/bptree.c src/utils.c include/bptree.h include/common.h
	$(CC) $(CFLAGS) -o $@ tests/unit/test_bptree.c src/bptree.c src/utils.c

unit: $(UNIT_TEST_BINS)
	./tests/unit/test_tokenizer
	./tests/unit/test_parser
	./tests/unit/test_storage
	./tests/unit/test_executor
	./tests/unit/test_bptree

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
	rm -f $(TARGET) $(UNIT_TEST_BINS)
