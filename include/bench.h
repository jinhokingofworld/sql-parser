#ifndef BENCH_H
#define BENCH_H

#include "common.h"

int bench_run(const char *db_root, long row_count, long iterations, FILE *out, SqlError *error);

#endif
