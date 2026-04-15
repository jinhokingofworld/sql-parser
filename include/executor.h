#ifndef EXECUTOR_H
#define EXECUTOR_H

#include "ast.h"
#include "common.h"
#include "db_context.h"

int execute_query(const Query *query, DbContext *ctx, FILE *out, SqlError *error);
int execute_query_list(const QueryList *queries, DbContext *ctx, FILE *out, SqlError *error);

#endif
