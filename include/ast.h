#ifndef AST_H
#define AST_H

#include "common.h"

typedef enum {
    QUERY_INSERT,
    QUERY_SELECT
} QueryType;

typedef enum {
    VALUE_INT,
    VALUE_STRING
} ValueType;

typedef struct {
    ValueType type;
    char *raw;
} Value;

typedef struct {
    char *column;
    Value value;
} Condition;

typedef struct {
    char *column;
    int ascending;
} OrderByClause;

typedef struct {
    char *table_name;
    char **columns;
    int column_count;
    Value *values;
} InsertQuery;

typedef struct {
    char *table_name;
    char **columns;
    int column_count;
    int select_all;
    int has_where;
    Condition where;
    int has_order_by;
    OrderByClause order_by;
} SelectQuery;

typedef struct {
    QueryType type;
    union {
        InsertQuery insert_query;
        SelectQuery select_query;
    };
} Query;

typedef struct {
    Query **items;
    int count;
} QueryList;

void free_query(Query *query);
void free_query_list(QueryList *list);
void print_query(const Query *query, FILE *out);

#endif
