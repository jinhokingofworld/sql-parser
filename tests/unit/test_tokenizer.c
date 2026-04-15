#include "tokenizer.h"

static int assert_true(int condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "assertion failed: %s\n", message);
        return 0;
    }
    return 1;
}

int main(void) {
    const char *sql = "SeLeCt id, name FROM users WHERE score BETWEEN 3.25 AND 4.50 ORDER BY score DESC;";
    TokenArray tokens = {NULL, 0};
    SqlError error = {0, 0, {0}};

    if (!tokenize_sql(sql, &tokens, &error)) {
        fprintf(stderr, "tokenize_sql failed: %s\n", error.message);
        return 1;
    }

    if (!assert_true(tokens.count == 18, "unexpected token count")) {
        free_token_array(&tokens);
        return 1;
    }
    if (!assert_true(tokens.items[0].type == TOKEN_KEYWORD, "SELECT should be keyword")) {
        free_token_array(&tokens);
        return 1;
    }
    if (!assert_true(strcmp(tokens.items[1].lexeme, "id") == 0, "column name should match")) {
        free_token_array(&tokens);
        return 1;
    }
    if (!assert_true(tokens.items[9].type == TOKEN_NUMBER, "BETWEEN low value should be number")) {
        free_token_array(&tokens);
        return 1;
    }
    if (!assert_true(strcmp(tokens.items[9].lexeme, "3.25") == 0, "float literal should match")) {
        free_token_array(&tokens);
        return 1;
    }
    if (!assert_true(tokens.items[11].type == TOKEN_NUMBER, "BETWEEN high value should be number")) {
        free_token_array(&tokens);
        return 1;
    }
    if (!assert_true(tokens.items[14].type == TOKEN_IDENTIFIER, "ORDER BY target should be identifier")) {
        free_token_array(&tokens);
        return 1;
    }

    free_token_array(&tokens);
    return 0;
}
