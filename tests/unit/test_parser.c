#include "parser.h"

static int assert_true(int condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "assertion failed: %s\n", message);
        return 0;
    }
    return 1;
}

int main(void) {
    const char *sql =
        "INSERT INTO users (name, grade, age, region, score) VALUES ('Alice', 1, 20, 'Seoul', 4.25);"
        "SELECT id, name FROM users WHERE id BETWEEN 1 AND 3 ORDER BY score DESC;";
    const char *float_sql = "SELECT score FROM students WHERE score = 3.75;";
    TokenArray tokens = {NULL, 0};
    TokenArray float_tokens = {NULL, 0};
    QueryList queries = {NULL, 0};
    QueryList float_queries = {NULL, 0};
    SqlError error = {0, 0, {0}};
    int ok = 1;

    if (!tokenize_sql(sql, &tokens, &error)) {
        fprintf(stderr, "tokenize_sql failed: %s\n", error.message);
        return 1;
    }

    if (!parse_queries(&tokens, &queries, &error)) {
        fprintf(stderr, "parse_queries failed: %s\n", error.message);
        free_token_array(&tokens);
        return 1;
    }

    ok &= assert_true(queries.count == 2, "expected two statements");
    ok &= assert_true(queries.items[0]->type == QUERY_INSERT, "first query should be INSERT");
    ok &= assert_true(queries.items[0]->insert_query.column_count == 5, "INSERT should have five columns");
    ok &= assert_true(queries.items[0]->insert_query.values[4].type == VALUE_FLOAT, "score should parse as float");
    ok &= assert_true(strcmp(queries.items[1]->select_query.table_name, "users") == 0, "SELECT table mismatch");
    ok &= assert_true(queries.items[1]->select_query.has_where == 1, "WHERE clause missing");
    ok &= assert_true(strcmp(queries.items[1]->select_query.where.column, "id") == 0, "WHERE column mismatch");
    ok &= assert_true(queries.items[1]->select_query.where.type == COND_BETWEEN, "WHERE type mismatch");
    ok &= assert_true(queries.items[1]->select_query.has_order_by == 1, "ORDER BY clause missing");

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
    return ok ? 0 : 1;
}
