#ifndef TOKENIZER_H
#define TOKENIZER_H

#include "common.h"

typedef enum {
    TOKEN_KEYWORD,
    TOKEN_IDENTIFIER,
    TOKEN_NUMBER,
    TOKEN_STRING,
    TOKEN_COMMA,
    TOKEN_LPAREN,
    TOKEN_RPAREN,
    TOKEN_SEMICOLON,
    TOKEN_ASTERISK,
    TOKEN_EQUALS,
    TOKEN_EOF
} TokenType;

typedef struct {
    TokenType type;
    char *lexeme;
    int line;
    int column;
} Token;

typedef struct {
    Token *items;
    int count;
} TokenArray;

int tokenize_sql(const char *sql, TokenArray *tokens, SqlError *error);
void free_token_array(TokenArray *tokens);
const char *token_type_name(TokenType type);

#endif
