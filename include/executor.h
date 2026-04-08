#ifndef EXECUTOR_H
#define EXECUTOR_H

#include "ast.h"
#include "common.h"

int execute_query(const Query *query, const char *db_root, FILE *out, SqlError *error);
int execute_query_list(const QueryList *queries, const char *db_root, FILE *out, SqlError *error);

#endif
