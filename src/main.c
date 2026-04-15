#include "bench.h"
#include "cli.h"
#include "db_context.h"
#include "executor.h"
#include "parser.h"
#include "tokenizer.h"

int main(int argc, char **argv) {
    CliOptions options;
    SqlError error = {0, 0, {0}};
    char *sql = NULL;
    TokenArray tokens = {NULL, 0};
    QueryList queries = {NULL, 0};
    DbContext *ctx = NULL;
    int index;
    int ok = 0;

    if (!parse_cli_args(argc, argv, &options, &error)) {
        print_usage(stderr, argv[0]);
        fprintf(stderr, "error: %s\n", error.message);
        return 1;
    }

    if (options.bench_rows > 0) {
        if (!bench_run(options.db_root, options.bench_rows, 200L, stdout, &error)) {
            fprintf(stderr, "error: %s\n", error.message);
            return 1;
        }
        return 0;
    }

    sql = sql_read_text_file(options.sql_path, &error);
    if (sql == NULL) {
        fprintf(stderr, "error: %s\n", error.message);
        return 1;
    }

    if (!tokenize_sql(sql, &tokens, &error)) {
        fprintf(stderr, "error at %d:%d: %s\n", error.line, error.column, error.message);
        goto cleanup;
    }

    if (!parse_queries(&tokens, &queries, &error)) {
        if (error.line > 0) {
            fprintf(stderr, "error at %d:%d: %s\n", error.line, error.column, error.message);
        } else {
            fprintf(stderr, "error: %s\n", error.message);
        }
        goto cleanup;
    }

    if (options.explain) {
        for (index = 0; index < queries.count; index++) {
            print_query(queries.items[index], stdout);
        }
    }

    ctx = db_context_create(options.db_root, &error);
    if (ctx == NULL) {
        fprintf(stderr, "error: %s\n", error.message);
        goto cleanup;
    }

    if (!execute_query_list(&queries, ctx, stdout, &error)) {
        fprintf(stderr, "error: %s\n", error.message);
        goto cleanup;
    }

    ok = 1;

cleanup:
    db_context_destroy(ctx);
    free(sql);
    free_token_array(&tokens);
    free_query_list(&queries);
    return ok ? 0 : 1;
}
