#include "tokenizer.h"
#include "unity.h"

void setUp(void) {
}

void tearDown(void) {
}

/* ms: Keeps the original tokenizer coverage for SELECT/WHERE/ORDER BY in one focused case. */
static void test_tokenize_select_with_where_and_order_by(void) {
    const char *sql = "SeLeCt id, name FROM users WHERE age = 20 ORDER BY name;";
    TokenArray tokens = {NULL, 0};
    SqlError error = {0, 0, {0}};
    int ok = tokenize_sql(sql, &tokens, &error);

    TEST_ASSERT_TRUE_MESSAGE(ok, error.message);
    TEST_ASSERT_EQUAL_INT(15, tokens.count);
    TEST_ASSERT_EQUAL_INT(TOKEN_KEYWORD, tokens.items[0].type);
    TEST_ASSERT_EQUAL_STRING("id", tokens.items[1].lexeme);
    TEST_ASSERT_EQUAL_INT(TOKEN_NUMBER, tokens.items[9].type);
    TEST_ASSERT_EQUAL_INT(TOKEN_IDENTIFIER, tokens.items[12].type);

    free_token_array(&tokens);
}

/* ms: Added to protect dataset values that contain punctuation from tokenization regressions. */
static void test_tokenize_preserves_string_literal_contents(void) {
    const char *sql = "INSERT INTO users (name) VALUES ('Alice, Jr.');";
    TokenArray tokens = {NULL, 0};
    SqlError error = {0, 0, {0}};
    int ok = tokenize_sql(sql, &tokens, &error);

    TEST_ASSERT_TRUE_MESSAGE(ok, error.message);
    TEST_ASSERT_EQUAL_INT(TOKEN_STRING, tokens.items[8].type);
    TEST_ASSERT_EQUAL_STRING("Alice, Jr.", tokens.items[8].lexeme);

    free_token_array(&tokens);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_tokenize_select_with_where_and_order_by);
    RUN_TEST(test_tokenize_preserves_string_literal_contents);
    return UNITY_END();
}
