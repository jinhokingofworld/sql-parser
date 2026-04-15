#ifndef BENCH_H
#define BENCH_H

#include "common.h"
#include "db_context.h"

int run_benchmark(DbContext *ctx, int row_count, FILE *out, SqlError *error);

#endif
