#include "common.h"
#include "db_context.h"
#include "executor.h"
#include "parser.h"
#include "tokenizer.h"

#include <time.h>

typedef struct {
    char **items;
    int count;
} StatementList;

static void print_usage(FILE *out, const char *program_name) {
    fprintf(out, "Usage: %s --sql <file> --db <path>\n", program_name);
}

static void free_statement_list(StatementList *statements) {
    int index;

    if (statements == NULL) {
        return;
    }

    for (index = 0; index < statements->count; index++) {
        free(statements->items[index]);
    }
    free(statements->items);
    statements->items = NULL;
    statements->count = 0;
}

static int append_statement(StatementList *statements, char *statement, SqlError *error) {
    char **next_items = realloc(statements->items, sizeof(char *) * (size_t) (statements->count + 1));

    if (next_items == NULL) {
        free(statement);
        sql_set_error(error, 0, 0, "out of memory while preparing demo statements");
        return 0;
    }

    statements->items = next_items;
    statements->items[statements->count++] = statement;
    return 1;
}

static int is_sql_whitespace(char ch) {
    return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n';
}

static int split_sql_statements(const char *sql, StatementList *statements, SqlError *error) {
    const char *segment_start = sql;
    const char *cursor = sql;
    int in_string = 0;

    statements->items = NULL;
    statements->count = 0;

    while (*cursor != '\0') {
        if (*cursor == '\'') {
            if (in_string && cursor[1] == '\'') {
                cursor += 2;
                continue;
            }
            in_string = !in_string;
        } else if (*cursor == ';' && !in_string) {
            const char *trimmed_start = segment_start;
            const char *trimmed_end = cursor + 1;
            char *statement = NULL;

            while (trimmed_start < trimmed_end && is_sql_whitespace(*trimmed_start)) {
                trimmed_start++;
            }
            while (trimmed_end > trimmed_start && is_sql_whitespace(trimmed_end[-1])) {
                trimmed_end--;
            }

            if (trimmed_end > trimmed_start) {
                statement = sql_strndup(trimmed_start, (size_t) (trimmed_end - trimmed_start));
                if (statement == NULL) {
                    free_statement_list(statements);
                    sql_set_error(error, 0, 0, "out of memory while preparing demo statements");
                    return 0;
                }
                if (!append_statement(statements, statement, error)) {
                    free_statement_list(statements);
                    return 0;
                }
            }

            segment_start = cursor + 1;
        }
        cursor++;
    }

    while (*segment_start != '\0' && is_sql_whitespace(*segment_start)) {
        segment_start++;
    }
    if (*segment_start != '\0') {
        free_statement_list(statements);
        sql_set_error(error, 0, 0, "each demo statement must end with `;`");
        return 0;
    }

    if (statements->count == 0) {
        sql_set_error(error, 0, 0, "SQL file does not contain any statements");
        return 0;
    }

    return 1;
}

static const char *access_path_label(QueryAccessPath path) {
    switch (path) {
        case QUERY_ACCESS_LINEAR:
            return "선형 탐색";
        case QUERY_ACCESS_INDEX_EQ:
            return "인덱스 단건 조회";
        case QUERY_ACCESS_INDEX_RANGE:
            return "인덱스 범위 조회";
        case QUERY_ACCESS_NONE:
            break;
    }

    return "해당 없음";
}

static long long demo_now_ns(void) {
    struct timespec ts;

    if (timespec_get(&ts, TIME_UTC) != TIME_UTC) {
        return 0LL;
    }

    return (long long) ts.tv_sec * 1000000000LL + (long long) ts.tv_nsec;
}

static void print_elapsed_time(long long elapsed_ns) {
    if (elapsed_ns >= 1000000000LL) {
        printf("%.6f초 (%lldns)\n", (double) elapsed_ns / 1000000000.0, elapsed_ns);
        return;
    }
    if (elapsed_ns >= 1000000LL) {
        printf("%.3fms (%lldns)\n", (double) elapsed_ns / 1000000.0, elapsed_ns);
        return;
    }
    if (elapsed_ns >= 1000LL) {
        printf("%.3fus (%lldns)\n", (double) elapsed_ns / 1000.0, elapsed_ns);
        return;
    }

    printf("%lldns\n", elapsed_ns);
}

static int copy_stream(FILE *src, FILE *dst, SqlError *error) {
    char buffer[512];
    size_t read_count;

    if (fseek(src, 0L, SEEK_SET) != 0) {
        sql_set_error(error, 0, 0, "failed to rewind temporary result output");
        return 0;
    }

    while ((read_count = fread(buffer, 1U, sizeof(buffer), src)) > 0U) {
        if (fwrite(buffer, 1U, read_count, dst) != read_count) {
            sql_set_error(error, 0, 0, "failed to render demo result output");
            return 0;
        }
    }

    if (ferror(src)) {
        sql_set_error(error, 0, 0, "failed to read temporary result output");
        return 0;
    }

    return 1;
}

static int parse_single_query(const char *statement_sql, Query **out_query, SqlError *error) {
    TokenArray tokens = {NULL, 0};
    QueryList queries = {NULL, 0};
    int ok = 0;

    *out_query = NULL;

    if (!tokenize_sql(statement_sql, &tokens, error)) {
        goto cleanup;
    }
    if (!parse_queries(&tokens, &queries, error)) {
        goto cleanup;
    }
    if (queries.count != 1) {
        sql_set_error(error, 0, 0, "demo helper expected exactly one statement");
        goto cleanup;
    }

    *out_query = queries.items[0];
    queries.items[0] = NULL;
    ok = 1;

cleanup:
    free_token_array(&tokens);
    free_query_list(&queries);
    return ok;
}

static int run_statement(const char *statement_sql, int statement_index, DbContext *ctx, SqlError *error) {
    Query *query = NULL;
    FILE *result_capture = NULL;
    QueryExecutionStats stats;
    long long started_ns = 0LL;
    long long finished_ns = 0LL;
    long long elapsed_ns = 0LL;
    int ok = 0;

    if (!parse_single_query(statement_sql, &query, error)) {
        return 0;
    }

    result_capture = tmpfile();
    if (result_capture == NULL) {
        sql_set_error(error, 0, 0, "failed to create temporary result buffer");
        goto cleanup;
    }

    if (statement_index > 0) {
        printf("============================================================\n");
    }

    printf("[쿼리]\n%s\n\n", statement_sql);

    started_ns = demo_now_ns();
    if (!execute_query_with_stats(query, ctx, result_capture, &stats, error)) {
        finished_ns = demo_now_ns();
        elapsed_ns = finished_ns >= started_ns ? finished_ns - started_ns : 0LL;
        printf("[결과]\nerror: %s\n\n", error->message);
        printf("[소요시간]\n");
        print_elapsed_time(elapsed_ns);
        error->message[0] = '\0';
        ok = 0;
        goto cleanup;
    }
    finished_ns = demo_now_ns();
    elapsed_ns = finished_ns >= started_ns ? finished_ns - started_ns : 0LL;

    printf("[결과]\n");
    if (!copy_stream(result_capture, stdout, error)) {
        goto cleanup;
    }
    printf("\n[소요시간]\n");
    print_elapsed_time(elapsed_ns);

    if (stats.has_select_stats) {
        printf("\n[탐색 지표]\n");
        printf("탐색 방식: %s\n", access_path_label(stats.access_path));
        printf("전체 행 수: %d\n", stats.total_rows);
        printf("행 탐색 수: %d\n", stats.rows_examined);
        printf("인덱스 탐색 단계 수: %d\n", stats.index_steps);
        printf("결과 행 수: %d\n", stats.result_rows);
    }

    ok = 1;

cleanup:
    if (result_capture != NULL) {
        fclose(result_capture);
    }
    free_query(query);
    return ok;
}

int main(int argc, char **argv) {
    const char *sql_path = NULL;
    const char *db_root = NULL;
    char *sql_text = NULL;
    StatementList statements = {NULL, 0};
    DbContext *ctx = NULL;
    SqlError error = {0, 0, {0}};
    int index;
    int show_usage = 0;

    for (index = 1; index < argc; index++) {
        if (strcmp(argv[index], "--sql") == 0) {
            if (index + 1 >= argc) {
                sql_set_error(&error, 0, 0, "`--sql` requires a file path");
                show_usage = 1;
                goto fail;
            }
            sql_path = argv[++index];
        } else if (strcmp(argv[index], "--db") == 0) {
            if (index + 1 >= argc) {
                sql_set_error(&error, 0, 0, "`--db` requires a directory path");
                show_usage = 1;
                goto fail;
            }
            db_root = argv[++index];
        } else {
            sql_set_error(&error, 0, 0, "unexpected argument `%s`", argv[index]);
            show_usage = 1;
            goto fail;
        }
    }

    if (sql_path == NULL || db_root == NULL) {
        sql_set_error(&error, 0, 0, "missing required `--sql` or `--db` option");
        show_usage = 1;
        goto fail;
    }

    sql_text = sql_read_text_file(sql_path, &error);
    if (sql_text == NULL) {
        goto fail;
    }
    if (!split_sql_statements(sql_text, &statements, &error)) {
        goto fail;
    }

    ctx = db_context_create(db_root, &error);
    if (ctx == NULL) {
        goto fail;
    }

    for (index = 0; index < statements.count; index++) {
        if (!run_statement(statements.items[index], index, ctx, &error)) {
            goto fail;
        }
        if (index + 1 < statements.count) {
            printf("\n");
        }
    }

    db_context_destroy(ctx);
    free_statement_list(&statements);
    free(sql_text);
    return 0;

fail:
    if (show_usage) {
        print_usage(stderr, argv[0]);
    }
    if (error.line > 0) {
        fprintf(stderr, "error at %d:%d: %s\n", error.line, error.column, error.message);
    } else if (error.message[0] != '\0') {
        fprintf(stderr, "error: %s\n", error.message);
    }
    db_context_destroy(ctx);
    free_statement_list(&statements);
    free(sql_text);
    return 1;
}
