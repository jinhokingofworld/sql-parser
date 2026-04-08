#ifndef PARSER_H
#define PARSER_H

#include "ast.h"
#include "tokenizer.h"

typedef struct {
    const TokenArray *tokens;
    int index;
} TokenStream;

int parse_queries(const TokenArray *tokens, QueryList *queries, SqlError *error);

#endif
