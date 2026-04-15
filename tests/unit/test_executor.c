#include "executor.h"
#include "parser.h"
#include "test_helpers.h"
#include "tokenizer.h"
#include "unity.h"

static char g_db_root[1024];

/* ms: Rebuild a tiny users database before each test so executor cases remain independent. */
void setUp(void) {
    int ok = test_build_temp_root(g_db_root, sizeof(g_db_root), "sql-parser-executor");

    TEST_ASSERT_TRUE(ok);
    ok = test_create_users_db(
        g_db_root,
        "table=users\ncolumns=id:int,name:string,age:int\npkey=id\n",
        "1,Alice,20\n2,Bob,31\n"
    );
    TEST_ASSERT_TRUE_MESSAGE(ok, "failed to create executor temp db");
}

void tearDown(void) {
    test_cleanup_users_db(g_db_root);
}

/* ms: Shared helper runs the full tokenize-parse-execute path and captures output for assertions. */
static char *run_sql(const char *sql, const char *db_root) {
    TokenArray tokens = {NULL, 0};
    QueryList queries = {NULL, 0};
    SqlError error = {0, 0, {0}};
    FILE *output = tmpfile();
    long length;
    char *buffer;

    if (output == NULL) {
        return NULL;
    }

    if (!tokenize_sql(sql, &tokens, &error) ||
        !parse_queries(&tokens, &queries, &error) ||
        !execute_query_list(&queries, db_root, output, &error)) {
        fprintf(stderr, "run_sql failed: %s\n", error.message);
        fclose(output);
        free_token_array(&tokens);
        free_query_list(&queries);
        return NULL;
    }

    fflush(output);
    fseek(output, 0L, SEEK_END);
    length = ftell(output);
    rewind(output);

    buffer = malloc((size_t) length + 1U);
    if (buffer == NULL) {
        fclose(output);
        free_token_array(&tokens);
        free_query_list(&queries);
        return NULL;
    }

    fread(buffer, 1U, (size_t) length, output);
    buffer[length] = '\0';

    fclose(output);
    free_token_array(&tokens);
    free_query_list(&queries);
    return buffer;
}

/* ms: Negative-path helper keeps error assertions in tests without printing noisy failures as successes. */
static int run_sql_expect_failure(const char *sql, const char *db_root, char *buffer, size_t size) {
    TokenArray tokens = {NULL, 0};
    QueryList queries = {NULL, 0};
    SqlError error = {0, 0, {0}};
    int ok = 0;

    if (tokenize_sql(sql, &tokens, &error) &&
        parse_queries(&tokens, &queries, &error) &&
        execute_query_list(&queries, db_root, stdout, &error)) {
        ok = 0;
    } else {
        snprintf(buffer, size, "%s", error.message);
        ok = 1;
    }

    free_token_array(&tokens);
    free_query_list(&queries);
    return ok;
}

/* ms: Verifies the happy-path INSERT contract before index integration changes executor behavior. */
static void test_execute_insert_returns_insert_count(void) {
    char *insert_output = run_sql(
        "INSERT INTO users (id, name, age) VALUES (3, 'Charlie', 19);",
        g_db_root
    );

    TEST_ASSERT_NOT_NULL(insert_output);
    TEST_ASSERT_EQUAL_STRING("INSERT 1\n", insert_output);
    free(insert_output);
}

/* ms: Keeps the existing SELECT filtering behavior under test as a regression baseline. */
static void test_execute_select_filters_rows(void) {
    char *select_output = run_sql(
        "SELECT name FROM users WHERE age = 31 ORDER BY name;",
        g_db_root
    );

    TEST_ASSERT_NOT_NULL(select_output);
    TEST_ASSERT_TRUE(strstr(select_output, "Bob") != NULL);
    TEST_ASSERT_TRUE(strstr(select_output, "(1 rows)") != NULL);
    free(select_output);
}

/* ms: Preserves duplicate primary-key rejection as an executor-level guardrail. */
static void test_execute_insert_rejects_duplicate_primary_key(void) {
    char duplicate_error[256] = {0};
    int ok = run_sql_expect_failure(
        "INSERT INTO users (id, name, age) VALUES (2, 'Bobby', 28);",
        g_db_root,
        duplicate_error,
        sizeof(duplicate_error)
    );

    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_TRUE(strstr(duplicate_error, "duplicate primary key") != NULL);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_execute_insert_returns_insert_count);
    RUN_TEST(test_execute_select_filters_rows);
    RUN_TEST(test_execute_insert_rejects_duplicate_primary_key);
    return UNITY_END();
}
