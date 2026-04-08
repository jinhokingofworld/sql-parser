#include "tokenizer.h"

static int assert_true(int condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "assertion failed: %s\n", message);
        return 0;
    }
    return 1;
}

int main(void) {
    const char *sql = "SeLeCt id, name FROM users WHERE age = 20 ORDER BY name;";
    TokenArray tokens = {NULL, 0};
    SqlError error = {0, 0, {0}};

    if (!tokenize_sql(sql, &tokens, &error)) {
        fprintf(stderr, "tokenize_sql failed: %s\n", error.message);
        return 1;
    }

    if (!assert_true(tokens.count == 15, "unexpected token count")) {
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
    if (!assert_true(tokens.items[9].type == TOKEN_NUMBER, "WHERE value should be number")) {
        free_token_array(&tokens);
        return 1;
    }
    if (!assert_true(tokens.items[12].type == TOKEN_IDENTIFIER, "ORDER BY target should be identifier")) {
        free_token_array(&tokens);
        return 1;
    }

    free_token_array(&tokens);
    return 0;
}
