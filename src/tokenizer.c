#include "tokenizer.h"

typedef struct {
    const char *name;
} KeywordEntry;

static const KeywordEntry KEYWORDS[] = {
    {"INSERT"},
    {"INTO"},
    {"VALUES"},
    {"SELECT"},
    {"FROM"},
    {"WHERE"},
    {"ORDER"},
    {"BY"},
    {"ASC"},
    {"DESC"}
};

static int append_token(
    TokenArray *tokens,
    TokenType type,
    const char *lexeme,
    size_t length,
    int line,
    int column,
    SqlError *error
) {
    Token *next_items;
    char *copy;

    next_items = realloc(tokens->items, sizeof(Token) * (size_t) (tokens->count + 1));
    if (next_items == NULL) {
        sql_set_error(error, line, column, "out of memory while tokenizing");
        return 0;
    }

    copy = sql_strndup(lexeme, length);
    if (copy == NULL) {
        sql_set_error(error, line, column, "out of memory while tokenizing");
        return 0;
    }

    tokens->items = next_items;
    tokens->items[tokens->count].type = type;
    tokens->items[tokens->count].lexeme = copy;
    tokens->items[tokens->count].line = line;
    tokens->items[tokens->count].column = column;
    tokens->count++;
    return 1;
}

static int is_keyword(const char *text) {
    size_t index;

    for (index = 0; index < sizeof(KEYWORDS) / sizeof(KEYWORDS[0]); index++) {
        if (sql_stricmp(KEYWORDS[index].name, text) == 0) {
            return 1;
        }
    }

    return 0;
}

static int scan_string(
    const char *sql,
    size_t *position,
    int *line,
    int *column,
    TokenArray *tokens,
    SqlError *error
) {
    size_t cursor = *position + 1;
    int start_line = *line;
    int start_column = *column;
    char *buffer = NULL;
    size_t length = 0;

    while (sql[cursor] != '\0') {
        if (sql[cursor] == '\'') {
            if (sql[cursor + 1] == '\'') {
                char *next = realloc(buffer, length + 2);
                if (next == NULL) {
                    free(buffer);
                    sql_set_error(error, start_line, start_column, "out of memory while tokenizing");
                    return 0;
                }
                buffer = next;
                buffer[length++] = '\'';
                cursor += 2;
                *column += 2;
                continue;
            }

            if (!append_token(tokens, TOKEN_STRING, buffer == NULL ? "" : buffer, length, start_line, start_column, error)) {
                free(buffer);
                return 0;
            }
            free(buffer);
            cursor++;
            *position = cursor;
            (*column)++;
            return 1;
        }

        if (sql[cursor] == '\n') {
            free(buffer);
            sql_set_error(error, start_line, start_column, "unterminated string literal");
            return 0;
        }

        {
            char *next = realloc(buffer, length + 2);
            if (next == NULL) {
                free(buffer);
                sql_set_error(error, start_line, start_column, "out of memory while tokenizing");
                return 0;
            }
            buffer = next;
            buffer[length++] = sql[cursor];
            buffer[length] = '\0';
        }

        cursor++;
        (*column)++;
    }

    free(buffer);
    sql_set_error(error, start_line, start_column, "unterminated string literal");
    return 0;
}

/* Returns a stable label for each token type to improve diagnostics and tests. */
const char *token_type_name(TokenType type) {
    switch (type) {
        case TOKEN_KEYWORD:
            return "keyword";
        case TOKEN_IDENTIFIER:
            return "identifier";
        case TOKEN_NUMBER:
            return "number";
        case TOKEN_STRING:
            return "string";
        case TOKEN_COMMA:
            return "comma";
        case TOKEN_LPAREN:
            return "left parenthesis";
        case TOKEN_RPAREN:
            return "right parenthesis";
        case TOKEN_SEMICOLON:
            return "semicolon";
        case TOKEN_ASTERISK:
            return "asterisk";
        case TOKEN_EQUALS:
            return "equals";
        case TOKEN_EOF:
            return "eof";
    }

    return "unknown";
}

/* Releases every token lexeme allocated during tokenization. */
void free_token_array(TokenArray *tokens) {
    int index;

    if (tokens == NULL) {
        return;
    }

    for (index = 0; index < tokens->count; index++) {
        free(tokens->items[index].lexeme);
    }

    free(tokens->items);
    tokens->items = NULL;
    tokens->count = 0;
}

void free_token_array_on_failure(TokenArray *tokens) {
    free_token_array(tokens);
}

/* Converts raw SQL text into typed tokens while preserving source positions. */
int tokenize_sql(const char *sql, TokenArray *tokens, SqlError *error) {
    size_t position = 0;
    int line = 1;
    int column = 1;

    tokens->items = NULL;
    tokens->count = 0;

    while (sql[position] != '\0') {
        if (isspace((unsigned char) sql[position])) {
            if (sql[position] == '\n') {
                line++;
                column = 1;
            } else {
                column++;
            }
            position++;
            continue;
        }

        if (isalpha((unsigned char) sql[position]) || sql[position] == '_') {
            size_t start = position;
            int start_column = column;

            while (isalnum((unsigned char) sql[position]) || sql[position] == '_') {
                position++;
                column++;
            }

            {
                char *lexeme = sql_strndup(sql + start, position - start);
                int keyword = 0;

                if (lexeme == NULL) {
                    sql_set_error(error, line, start_column, "out of memory while tokenizing");
                    free_token_array_on_failure(tokens);
                    return 0;
                }

                keyword = is_keyword(lexeme);
                free(lexeme);

                if (!append_token(
                        tokens,
                        keyword ? TOKEN_KEYWORD : TOKEN_IDENTIFIER,
                        sql + start,
                        position - start,
                        line,
                        start_column,
                        error
                    )) {
                    free_token_array_on_failure(tokens);
                    return 0;
                }
            }

            continue;
        }

        if (isdigit((unsigned char) sql[position]) ||
            (sql[position] == '-' && isdigit((unsigned char) sql[position + 1]))) {
            size_t start = position;
            int start_column = column;

            if (sql[position] == '-') {
                position++;
                column++;
            }

            while (isdigit((unsigned char) sql[position])) {
                position++;
                column++;
            }

            if (!append_token(tokens, TOKEN_NUMBER, sql + start, position - start, line, start_column, error)) {
                free_token_array_on_failure(tokens);
                return 0;
            }
            continue;
        }

        if (sql[position] == '\'') {
            if (!scan_string(sql, &position, &line, &column, tokens, error)) {
                free_token_array_on_failure(tokens);
                return 0;
            }
            continue;
        }

        if (sql[position] == ',') {
            if (!append_token(tokens, TOKEN_COMMA, ",", 1U, line, column, error)) {
                free_token_array_on_failure(tokens);
                return 0;
            }
            position++;
            column++;
            continue;
        }

        if (sql[position] == '(') {
            if (!append_token(tokens, TOKEN_LPAREN, "(", 1U, line, column, error)) {
                free_token_array_on_failure(tokens);
                return 0;
            }
            position++;
            column++;
            continue;
        }

        if (sql[position] == ')') {
            if (!append_token(tokens, TOKEN_RPAREN, ")", 1U, line, column, error)) {
                free_token_array_on_failure(tokens);
                return 0;
            }
            position++;
            column++;
            continue;
        }

        if (sql[position] == ';') {
            if (!append_token(tokens, TOKEN_SEMICOLON, ";", 1U, line, column, error)) {
                free_token_array_on_failure(tokens);
                return 0;
            }
            position++;
            column++;
            continue;
        }

        if (sql[position] == '*') {
            if (!append_token(tokens, TOKEN_ASTERISK, "*", 1U, line, column, error)) {
                free_token_array_on_failure(tokens);
                return 0;
            }
            position++;
            column++;
            continue;
        }

        if (sql[position] == '=') {
            if (!append_token(tokens, TOKEN_EQUALS, "=", 1U, line, column, error)) {
                free_token_array_on_failure(tokens);
                return 0;
            }
            position++;
            column++;
            continue;
        }

        sql_set_error(error, line, column, "unexpected character `%c`", sql[position]);
        free_token_array_on_failure(tokens);
        return 0;
    }

    if (!append_token(tokens, TOKEN_EOF, "", 0U, line, column, error)) {
        free_token_array_on_failure(tokens);
        return 0;
    }

    return 1;
}
