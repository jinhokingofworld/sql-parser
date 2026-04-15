#ifndef EXECUTOR_H
#define EXECUTOR_H

#include "ast.h"
#include "common.h"
#include "db_context.h"

typedef enum {
    QUERY_ACCESS_NONE,
    QUERY_ACCESS_LINEAR,
    QUERY_ACCESS_INDEX_EQ,
    QUERY_ACCESS_INDEX_RANGE
} QueryAccessPath;

typedef struct {
    int has_select_stats;
    QueryAccessPath access_path;
    int total_rows;
    int rows_examined;
    int index_steps;
    int result_rows;
} QueryExecutionStats;

int execute_query(const Query *query, DbContext *ctx, FILE *out, SqlError *error);
int execute_query_list(const QueryList *queries, DbContext *ctx, FILE *out, SqlError *error);
int execute_query_with_stats(
    const Query *query,
    DbContext *ctx,
    FILE *out,
    QueryExecutionStats *stats,
    SqlError *error
);
int execute_query_list_with_stats(
    const QueryList *queries,
    DbContext *ctx,
    FILE *out,
    QueryExecutionStats *stats_list,
    SqlError *error
);

#endif
