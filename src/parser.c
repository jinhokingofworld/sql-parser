#include "parser.h"

static const Token *peek_token(TokenStream *stream) {
    return &stream->tokens->items[stream->index];
}

static const Token *consume_token(TokenStream *stream) {
    const Token *token = peek_token(stream);
    if (token->type != TOKEN_EOF) {
        stream->index++;
    }
    return token;
}

static int append_query(QueryList *queries, Query *query, SqlError *error) {
    Query **next = realloc(queries->items, sizeof(Query *) * (size_t) (queries->count + 1));

    if (next == NULL) {
        sql_set_error(error, 0, 0, "out of memory while building AST");
        return 0;
    }

    queries->items = next;
    queries->items[queries->count] = query;
    queries->count++;
    return 1;
}

static int token_is_keyword(const Token *token, const char *keyword) {
    return token->type == TOKEN_KEYWORD && sql_stricmp(token->lexeme, keyword) == 0;
}

static int expect_keyword(TokenStream *stream, const char *keyword, SqlError *error) {
    const Token *token = peek_token(stream);

    if (!token_is_keyword(token, keyword)) {
        sql_set_error(error, token->line, token->column, "expected keyword `%s`", keyword);
        return 0;
    }

    consume_token(stream);
    return 1;
}

static int expect_type(TokenStream *stream, TokenType type, SqlError *error, const char *label) {
    const Token *token = peek_token(stream);

    if (token->type != type) {
        sql_set_error(error, token->line, token->column, "expected %s", label);
        return 0;
    }

    consume_token(stream);
    return 1;
}

static char *duplicate_token_lexeme(const Token *token, SqlError *error) {
    char *copy = sql_strdup(token->lexeme);

    if (copy == NULL) {
        sql_set_error(error, token->line, token->column, "out of memory while parsing");
        return NULL;
    }

    return copy;
}

static int parse_identifier(TokenStream *stream, char **output, SqlError *error) {
    const Token *token = peek_token(stream);

    if (token->type != TOKEN_IDENTIFIER && token->type != TOKEN_KEYWORD) {
        sql_set_error(error, token->line, token->column, "expected identifier");
        return 0;
    }

    *output = duplicate_token_lexeme(token, error);
    if (*output == NULL) {
        return 0;
    }

    consume_token(stream);
    return 1;
}

static int parse_value(TokenStream *stream, Value *value, SqlError *error) {
    const Token *token = peek_token(stream);

    if (token->type == TOKEN_NUMBER) {
        value->type = strchr(token->lexeme, '.') != NULL ? VALUE_FLOAT : VALUE_INT;
        value->raw = duplicate_token_lexeme(token, error);
    } else if (token->type == TOKEN_STRING) {
        value->type = VALUE_STRING;
        value->raw = duplicate_token_lexeme(token, error);
    } else {
        sql_set_error(error, token->line, token->column, "expected literal value");
        return 0;
    }

    if (value->raw == NULL) {
        return 0;
    }

    consume_token(stream);
    return 1;
}

static int parse_identifier_list(
    TokenStream *stream,
    char ***items,
    int *count,
    SqlError *error
) {
    while (1) {
        char **next_items;

        next_items = realloc(*items, sizeof(char *) * (size_t) (*count + 1));
        if (next_items == NULL) {
            sql_set_error(error, 0, 0, "out of memory while parsing");
            return 0;
        }

        *items = next_items;
        if (!parse_identifier(stream, &(*items)[*count], error)) {
            return 0;
        }
        (*count)++;

        if (peek_token(stream)->type != TOKEN_COMMA) {
            break;
        }
        consume_token(stream);
    }

    return 1;
}

static int parse_value_list(TokenStream *stream, Value **items, int *count, SqlError *error) {
    while (1) {
        Value *next_items = realloc(*items, sizeof(Value) * (size_t) (*count + 1));

        if (next_items == NULL) {
            sql_set_error(error, 0, 0, "out of memory while parsing");
            return 0;
        }

        *items = next_items;
        (*items)[*count].raw = NULL;
        if (!parse_value(stream, &(*items)[*count], error)) {
            return 0;
        }
        (*count)++;

        if (peek_token(stream)->type != TOKEN_COMMA) {
            break;
        }
        consume_token(stream);
    }

    return 1;
}

static int parse_where_clause(TokenStream *stream, Condition *condition, SqlError *error) {
    memset(condition, 0, sizeof(*condition));

    if (!expect_keyword(stream, "WHERE", error)) {
        return 0;
    }
    if (!parse_identifier(stream, &condition->column, error)) {
        return 0;
    }

    if (token_is_keyword(peek_token(stream), "BETWEEN")) {
        condition->type = COND_BETWEEN;
        consume_token(stream);
        if (!parse_value(stream, &condition->low, error)) {
            return 0;
        }
        if (!expect_keyword(stream, "AND", error)) {
            return 0;
        }
        if (!parse_value(stream, &condition->high, error)) {
            return 0;
        }
    } else {
        condition->type = COND_EQ;
        if (!expect_type(stream, TOKEN_EQUALS, error, "`=`")) {
            return 0;
        }
        if (!parse_value(stream, &condition->value, error)) {
            return 0;
        }
    }

    return 1;
}

static int parse_order_by_clause(TokenStream *stream, OrderByClause *clause, SqlError *error) {
    memset(clause, 0, sizeof(*clause));

    if (!expect_keyword(stream, "ORDER", error)) {
        return 0;
    }
    if (!expect_keyword(stream, "BY", error)) {
        return 0;
    }
    if (!parse_identifier(stream, &clause->column, error)) {
        return 0;
    }

    clause->ascending = 1;
    if (token_is_keyword(peek_token(stream), "ASC")) {
        consume_token(stream);
    } else if (token_is_keyword(peek_token(stream), "DESC")) {
        clause->ascending = 0;
        consume_token(stream);
    }

    return 1;
}

static Query *parse_insert(TokenStream *stream, SqlError *error) {
    Query *query = calloc(1, sizeof(Query));
    InsertQuery *insert_query;
    Value *values = NULL;
    int value_count = 0;

    if (query == NULL) {
        sql_set_error(error, 0, 0, "out of memory while parsing");
        return NULL;
    }

    query->type = QUERY_INSERT;
    insert_query = &query->insert_query;

    if (!expect_keyword(stream, "INSERT", error) ||
        !expect_keyword(stream, "INTO", error) ||
        !parse_identifier(stream, &insert_query->table_name, error) ||
        !expect_type(stream, TOKEN_LPAREN, error, "`(`") ||
        !parse_identifier_list(stream, &insert_query->columns, &insert_query->column_count, error) ||
        !expect_type(stream, TOKEN_RPAREN, error, "`)`") ||
        !expect_keyword(stream, "VALUES", error) ||
        !expect_type(stream, TOKEN_LPAREN, error, "`(`") ||
        !parse_value_list(stream, &values, &value_count, error) ||
        !expect_type(stream, TOKEN_RPAREN, error, "`)`")) {
        free_query(query);
        return NULL;
    }

    if (insert_query->column_count <= 0) {
        sql_set_error(error, 0, 0, "INSERT requires at least one column");
        free_query(query);
        return NULL;
    }

    if (insert_query->column_count != value_count) {
        sql_set_error(error, 0, 0, "column/value count mismatch");
        free(values);
        free_query(query);
        return NULL;
    }

    insert_query->values = values;

    return query;
}

static Query *parse_select(TokenStream *stream, SqlError *error) {
    Query *query = calloc(1, sizeof(Query));
    SelectQuery *select_query;

    if (query == NULL) {
        sql_set_error(error, 0, 0, "out of memory while parsing");
        return NULL;
    }

    query->type = QUERY_SELECT;
    select_query = &query->select_query;

    if (!expect_keyword(stream, "SELECT", error)) {
        free_query(query);
        return NULL;
    }

    if (peek_token(stream)->type == TOKEN_ASTERISK) {
        select_query->select_all = 1;
        consume_token(stream);
    } else if (!parse_identifier_list(stream, &select_query->columns, &select_query->column_count, error)) {
        free_query(query);
        return NULL;
    }

    if (!expect_keyword(stream, "FROM", error) ||
        !parse_identifier(stream, &select_query->table_name, error)) {
        free_query(query);
        return NULL;
    }

    if (token_is_keyword(peek_token(stream), "WHERE")) {
        select_query->has_where = 1;
        if (!parse_where_clause(stream, &select_query->where, error)) {
            free_query(query);
            return NULL;
        }
    }

    if (token_is_keyword(peek_token(stream), "ORDER")) {
        select_query->has_order_by = 1;
        if (!parse_order_by_clause(stream, &select_query->order_by, error)) {
            free_query(query);
            return NULL;
        }
    }

    return query;
}

/* Parses one or more SQL statements into AST queries that the executor can run. */
int parse_queries(const TokenArray *tokens, QueryList *queries, SqlError *error) {
    TokenStream stream;

    queries->items = NULL;
    queries->count = 0;

    stream.tokens = tokens;
    stream.index = 0;

    while (peek_token(&stream)->type != TOKEN_EOF) {
        Query *query = NULL;
        const Token *token = peek_token(&stream);

        if (token->type == TOKEN_SEMICOLON) {
            consume_token(&stream);
            continue;
        }

        if (token_is_keyword(token, "INSERT")) {
            query = parse_insert(&stream, error);
        } else if (token_is_keyword(token, "SELECT")) {
            query = parse_select(&stream, error);
        } else {
            sql_set_error(error, token->line, token->column, "unsupported statement `%s`", token->lexeme);
            free_query_list(queries);
            return 0;
        }

        if (query == NULL) {
            free_query_list(queries);
            return 0;
        }

        if (!expect_type(&stream, TOKEN_SEMICOLON, error, "`;`")) {
            free_query(query);
            free_query_list(queries);
            return 0;
        }

        if (!append_query(queries, query, error)) {
            free_query(query);
            free_query_list(queries);
            return 0;
        }
    }

    if (queries->count == 0) {
        sql_set_error(error, 0, 0, "SQL file does not contain any statements");
        return 0;
    }

    return 1;
}
