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
        "INSERT INTO users (id, name, age) VALUES (1, 'Alice', 20);"
        "SELECT id, name FROM users WHERE age = 20 ORDER BY name;";
    TokenArray tokens = {NULL, 0};
    QueryList queries = {NULL, 0};
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
    ok &= assert_true(queries.items[0]->insert_query.column_count == 3, "INSERT should have three columns");
    ok &= assert_true(strcmp(queries.items[1]->select_query.table_name, "users") == 0, "SELECT table mismatch");
    ok &= assert_true(queries.items[1]->select_query.has_where == 1, "WHERE clause missing");
    ok &= assert_true(strcmp(queries.items[1]->select_query.where.column, "age") == 0, "WHERE column mismatch");
    ok &= assert_true(queries.items[1]->select_query.has_order_by == 1, "ORDER BY clause missing");

    free_query_list(&queries);
    free_token_array(&tokens);
    return ok ? 0 : 1;
}
