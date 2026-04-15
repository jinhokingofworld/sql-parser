#include "parser.h"
#include "unity.h"

void setUp(void) {
}

void tearDown(void) {
}

/* ms: Preserves the original multi-statement parser contract while using Unity assertions. */
static void test_parse_insert_followed_by_select(void) {
    const char *sql =
        "INSERT INTO users (id, name, age) VALUES (1, 'Alice', 20);"
        "SELECT id, name FROM users WHERE age = 20 ORDER BY name;";
    const char *float_sql = "SELECT score FROM students WHERE score = 3.75;";
    TokenArray tokens = {NULL, 0};
    TokenArray float_tokens = {NULL, 0};
    QueryList queries = {NULL, 0};
    QueryList float_queries = {NULL, 0};
    SqlError error = {0, 0, {0}};
    int ok = tokenize_sql(sql, &tokens, &error);

    TEST_ASSERT_TRUE_MESSAGE(ok, error.message);
    ok = parse_queries(&tokens, &queries, &error);
    TEST_ASSERT_TRUE_MESSAGE(ok, error.message);

    TEST_ASSERT_EQUAL_INT(2, queries.count);
    TEST_ASSERT_EQUAL_INT(QUERY_INSERT, queries.items[0]->type);
    TEST_ASSERT_EQUAL_INT(3, queries.items[0]->insert_query.column_count);
    TEST_ASSERT_EQUAL_STRING("users", queries.items[1]->select_query.table_name);
    TEST_ASSERT_TRUE(queries.items[1]->select_query.has_where);
    TEST_ASSERT_EQUAL_STRING("age", queries.items[1]->select_query.where.column);
    TEST_ASSERT_TRUE(queries.items[1]->select_query.has_order_by);

    if (!tokenize_sql(float_sql, &float_tokens, &error)) {
        fprintf(stderr, "tokenize_sql float_sql failed: %s\n", error.message);
        free_query_list(&queries);
        free_token_array(&tokens);
        return 1;
    }

    if (!parse_queries(&float_tokens, &float_queries, &error)) {
        fprintf(stderr, "parse_queries float_sql failed: %s\n", error.message);
        free_query_list(&queries);
        free_token_array(&tokens);
        free_token_array(&float_tokens);
        return 1;
    }

    ok &= assert_true(float_queries.count == 1, "expected one float statement");
    ok &= assert_true(float_queries.items[0]->select_query.has_where == 1, "float WHERE clause missing");
    ok &= assert_true(
        float_queries.items[0]->select_query.where.value.type == VALUE_FLOAT,
        "float WHERE value type mismatch"
    );
    ok &= assert_true(
        strcmp(float_queries.items[0]->select_query.where.value.raw, "3.75") == 0,
        "float WHERE value raw mismatch"
    );

    free_query_list(&float_queries);
    free_token_array(&float_tokens);
    free_query_list(&queries);
    free_token_array(&tokens);
}

/* ms: Locks in the WHERE id = number shape that the index path will depend on later. */
static void test_parse_where_id_equality_for_indexable_shape(void) {
    const char *sql = "SELECT * FROM users WHERE id = 500000;";
    TokenArray tokens = {NULL, 0};
    QueryList queries = {NULL, 0};
    SqlError error = {0, 0, {0}};
    int ok = tokenize_sql(sql, &tokens, &error);

    TEST_ASSERT_TRUE_MESSAGE(ok, error.message);
    ok = parse_queries(&tokens, &queries, &error);
    TEST_ASSERT_TRUE_MESSAGE(ok, error.message);

    TEST_ASSERT_EQUAL_INT(1, queries.count);
    TEST_ASSERT_EQUAL_INT(QUERY_SELECT, queries.items[0]->type);
    TEST_ASSERT_TRUE(queries.items[0]->select_query.has_where);
    TEST_ASSERT_EQUAL_STRING("id", queries.items[0]->select_query.where.column);
    TEST_ASSERT_EQUAL_STRING("500000", queries.items[0]->select_query.where.value.raw);

    free_query_list(&queries);
    free_token_array(&tokens);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_parse_insert_followed_by_select);
    RUN_TEST(test_parse_where_id_equality_for_indexable_shape);
    return UNITY_END();
}
