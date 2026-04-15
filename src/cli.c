#include "cli.h"

/* Prints the supported CLI contract for humans and failing argument parses. */
void print_usage(FILE *out, const char *program_name) {
    fprintf(
        out,
        "Usage: %s [--sql <file>] [--db <path>] [--explain]\n"
        "       %s <sql-file>\n",
        program_name,
        program_name
    );
}

/* Parses command-line options into a stable runtime configuration structure. */
int parse_cli_args(int argc, char **argv, CliOptions *options, SqlError *error) {
    int index;

    options->sql_path = NULL;
    options->db_root = "./data";
    options->explain = 0;

    for (index = 1; index < argc; index++) {
        if (strcmp(argv[index], "--sql") == 0) {
            if (index + 1 >= argc) {
                sql_set_error(error, 0, 0, "`--sql` requires a file path");
                return 0;
            }
            options->sql_path = argv[++index];
        } else if (strcmp(argv[index], "--db") == 0) {
            if (index + 1 >= argc) {
                sql_set_error(error, 0, 0, "`--db` requires a directory path");
                return 0;
            }
            options->db_root = argv[++index];
        } else if (strcmp(argv[index], "--explain") == 0) {
            options->explain = 1;
        } else if (argv[index][0] == '-') {
            sql_set_error(error, 0, 0, "unknown option `%s`", argv[index]);
            return 0;
        } else if (options->sql_path == NULL) {
            options->sql_path = argv[index];
        } else {
            sql_set_error(error, 0, 0, "unexpected argument `%s`", argv[index]);
            return 0;
        }
    }

    if (options->sql_path == NULL) {
        sql_set_error(error, 0, 0, "missing SQL file path");
        return 0;
    }

    return 1;
}
