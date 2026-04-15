#ifndef CLI_H
#define CLI_H

#include "common.h"

typedef struct {
    const char *sql_path;
    const char *db_root;
    int explain;
    int bench_rows;
} CliOptions;

int parse_cli_args(int argc, char **argv, CliOptions *options, SqlError *error);
void print_usage(FILE *out, const char *program_name);

#endif
