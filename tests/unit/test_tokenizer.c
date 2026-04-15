#include "tokenizer.h"

static int assert_true(int condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "assertion failed: %s\n", message);
        return 0;
    }
    return 1;
}

int main(void) {
    const char *float_sql = "INSERT INTO students (score) VALUES (4.5);";
    const char *sql = "SeLeCt id, name FROM users WHERE score BETWEEN 3.25 AND 4.50 ORDER BY score DESC;";
    TokenArray tokens = {NULL, 0};
    TokenArray float_tokens = {NULL, 0};
    SqlError error = {0, 0, {0}};
    int float_index = -1;
    int index;

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

    if (!tokenize_sql(float_sql, &float_tokens, &error)) {
        fprintf(stderr, "tokenize_sql float_sql failed: %s\n", error.message);
        free_token_array(&tokens);
        return 1;
    }

    for (index = 0; index < float_tokens.count; index++) {
        if (float_tokens.items[index].type == TOKEN_NUMBER &&
            strcmp(float_tokens.items[index].lexeme, "4.5") == 0) {
            float_index = index;
            break;
        }
    }

    if (!assert_true(float_index >= 0, "float literal should be tokenized as number")) {
        free_token_array(&tokens);
        free_token_array(&float_tokens);
        return 1;
    }

    free_token_array(&tokens);
    free_token_array(&float_tokens);
    return 0;
}
