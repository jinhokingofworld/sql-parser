#include "common.h"
#include "db_context.h"
#include "executor.h"
#include "parser.h"
#include "test_helpers.h"
#include "tokenizer.h"

#include <time.h>

#ifdef _WIN32
#include <io.h>
#define bench_stderr_isatty() _isatty(_fileno(stderr))
#else
#include <unistd.h>
#define bench_stderr_isatty() isatty(STDERR_FILENO)
#endif

#define BENCH_PATH_BUFFER_SIZE 1024
#define BENCH_BAR_WIDTH 30

typedef struct {
    TokenArray tokens;
    QueryList queries;
} ParsedSql;

static double now_ms(void) {
    return ((double) clock() * 1000.0) / (double) CLOCKS_PER_SEC;
}

static long bench_progress_step(long total) {
    long candidate = total / 20L;

    if (candidate < 1L) {
        candidate = 1L;
    }
    if (candidate < 10000L) {
        return 10000L;
    }
    if (candidate > 50000L) {
        return 50000L;
    }
    return candidate;
}

static void bench_print_progress(const char *label, long current, long total, double started_ms) {
    long filled = (current * BENCH_BAR_WIDTH) / total;
    double elapsed_ms = now_ms() - started_ms;
    int index;

    if (filled > BENCH_BAR_WIDTH) {
        filled = BENCH_BAR_WIDTH;
    }

    fprintf(stderr, "\r[BENCH] %-20s [", label);
    for (index = 0; index < BENCH_BAR_WIDTH; index++) {
        fputc(index < filled ? '#' : '.', stderr);
    }
    fprintf(
        stderr,
        "] %3ld%% (%ld/%ld) elapsed=%0.2fs",
        (current * 100L) / total,
        current,
        total,
        elapsed_ms / 1000.0
    );
    fflush(stderr);

    if (current >= total) {
        fputc('\n', stderr);
    }
}

static void bench_report_periodic_progress(const char *label, long current, long total, double started_ms) {
    fprintf(
        stderr,
        "[BENCH] %-20s %ld/%ld (%ld%%) elapsed=%0.2fs\n",
        label,
        current,
        total,
        (current * 100L) / total,
        (now_ms() - started_ms) / 1000.0
    );
    fflush(stderr);
}

static void bench_update_progress(
    const char *label,
    long current,
    long total,
    long *last_reported,
    double started_ms
) {
    long step = bench_progress_step(total);

    if (current < total && current - *last_reported < step) {
        return;
    }

    if (bench_stderr_isatty()) {
        bench_print_progress(label, current, total, started_ms);
    } else {
        bench_report_periodic_progress(label, current, total, started_ms);
    }
    *last_reported = current;
}

static int parse_positive_int(const char *text, long *value) {
    char *end = NULL;
    long parsed;

    errno = 0;
    parsed = strtol(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || parsed <= 0) {
        return 0;
    }

    *value = parsed;
    return 1;
}

static void cleanup_parsed_sql(ParsedSql *parsed) {
    free_token_array(&parsed->tokens);
    free_query_list(&parsed->queries);
    parsed->tokens.items = NULL;
    parsed->tokens.count = 0;
    parsed->queries.items = NULL;
    parsed->queries.count = 0;
}

static int parse_sql_text(const char *sql, ParsedSql *parsed, SqlError *error) {
    parsed->tokens.items = NULL;
    parsed->tokens.count = 0;
    parsed->queries.items = NULL;
    parsed->queries.count = 0;

    if (!tokenize_sql(sql, &parsed->tokens, error)) {
        return 0;
    }
    if (!parse_queries(&parsed->tokens, &parsed->queries, error)) {
        free_token_array(&parsed->tokens);
        parsed->tokens.items = NULL;
        parsed->tokens.count = 0;
        return 0;
    }

    return 1;
}

static int append_text(char **buffer, size_t *length, size_t *capacity, const char *text) {
    size_t text_len = strlen(text);
    size_t required = *length + text_len + 1U;
    char *next;

    if (required > *capacity) {
        size_t next_capacity = *capacity == 0U ? 256U : *capacity;
        while (next_capacity < required) {
            next_capacity *= 2U;
        }
        next = realloc(*buffer, next_capacity);
        if (next == NULL) {
            return 0;
        }
        *buffer = next;
        *capacity = next_capacity;
    }

    memcpy(*buffer + *length, text, text_len + 1U);
    *length += text_len;
    return 1;
}

static int build_insert_sql(long row_count, char **out_sql, SqlError *error) {
    char *sql = NULL;
    size_t length = 0;
    size_t capacity = 0;
    long row;
    long last_reported = 0;
    double started_ms = now_ms();

    fprintf(stderr, "[BENCH] preparing insert workload for %ld rows\n", row_count);
    fflush(stderr);

    for (row = 1; row <= row_count; row++) {
        char statement[256];
        int written = snprintf(
            statement,
            sizeof(statement),
            "INSERT INTO users (name, age) VALUES ('User_%ld', %ld);\n",
            row,
            18L + (row % 50L)
        );

        if (written < 0 || written >= (int) sizeof(statement)) {
            free(sql);
            sql_set_error(error, 0, 0, "failed to format benchmark INSERT statement");
            return 0;
        }
        if (!append_text(&sql, &length, &capacity, statement)) {
            free(sql);
            sql_set_error(error, 0, 0, "out of memory while building benchmark INSERT workload");
            return 0;
        }
        bench_update_progress("build insert sql", row, row_count, &last_reported, started_ms);
    }

    *out_sql = sql;
    return 1;
}

static int create_empty_users_db(const char *db_root) {
    return test_create_users_db(
        db_root,
        "table=users\n"
        "columns=id:int,name:string,age:int\n"
        "pkey=id\n"
        "autoincrement=true\n",
        ""
    );
}

static int run_query_iterations(
    const ParsedSql *parsed,
    const char *db_root,
    long iterations,
    const char *label,
    double *elapsed_ms,
    SqlError *error
) {
    long iteration;
    FILE *sink = tmpfile();
    double started;
    DbContext *ctx = NULL;
    long last_reported = 0;

    if (sink == NULL) {
        sql_set_error(error, 0, 0, "failed to create temporary benchmark sink");
        return 0;
    }

    ctx = db_context_create(db_root, error);
    if (ctx == NULL) {
        fclose(sink);
        return 0;
    }

    started = now_ms();
    fprintf(stderr, "[BENCH] running %s\n", label);
    fflush(stderr);
    for (iteration = 0; iteration < iterations; iteration++) {
        rewind(sink);
        if (!execute_query_list(&parsed->queries, ctx, sink, error)) {
            db_context_destroy(ctx);
            fclose(sink);
            return 0;
        }
        clearerr(sink);
        bench_update_progress(label, iteration + 1, iterations, &last_reported, started);
    }
    *elapsed_ms = now_ms() - started;

    db_context_destroy(ctx);
    fclose(sink);
    return 1;
}

static void print_metric(const char *label, double total_ms, long operations) {
    printf(
        "[BENCH] %-24s total_ms=%9.3f avg_ms=%8.6f ops=%ld\n",
        label,
        total_ms,
        operations > 0 ? total_ms / (double) operations : 0.0,
        operations
    );
}

int main(int argc, char **argv) {
    char db_root[BENCH_PATH_BUFFER_SIZE];
    char *insert_sql = NULL;
    char select_id_sql[128];
    char select_age_sql[128];
    ParsedSql insert_workload = {{NULL, 0}, {NULL, 0}};
    ParsedSql select_id_workload = {{NULL, 0}, {NULL, 0}};
    ParsedSql select_age_workload = {{NULL, 0}, {NULL, 0}};
    SqlError error = {0, 0, {0}};
    long row_count = 10000;
    long iterations = 200;
    double insert_elapsed_ms = 0.0;
    double select_id_elapsed_ms = 0.0;
    double select_age_elapsed_ms = 0.0;
    int ok = 0;

    if (argc >= 2 && !parse_positive_int(argv[1], &row_count)) {
        fprintf(stderr, "usage: %s [row-count] [iterations]\n", argv[0]);
        return 1;
    }
    if (argc >= 3 && !parse_positive_int(argv[2], &iterations)) {
        fprintf(stderr, "usage: %s [row-count] [iterations]\n", argv[0]);
        return 1;
    }
    if (argc > 3) {
        fprintf(stderr, "usage: %s [row-count] [iterations]\n", argv[0]);
        return 1;
    }

    if (!test_build_temp_root(db_root, sizeof(db_root), "sql-benchmark")) {
        fprintf(stderr, "error: failed to create benchmark temp root\n");
        return 1;
    }
    if (!create_empty_users_db(db_root)) {
        fprintf(stderr, "error: failed to create benchmark database fixture\n");
        return 1;
    }

    if (!build_insert_sql(row_count, &insert_sql, &error)) {
        fprintf(stderr, "error: %s\n", error.message);
        goto cleanup;
    }
    if (!parse_sql_text(insert_sql, &insert_workload, &error)) {
        fprintf(stderr, "error: %s\n", error.message);
        goto cleanup;
    }
    fprintf(stderr, "[BENCH] parsing select workloads\n");
    fflush(stderr);
    if (snprintf(select_id_sql, sizeof(select_id_sql), "SELECT * FROM users WHERE id = %ld;", row_count / 2L) >=
        (int) sizeof(select_id_sql)) {
        fprintf(stderr, "error: failed to format id benchmark query\n");
        goto cleanup;
    }
    if (snprintf(select_age_sql, sizeof(select_age_sql), "SELECT * FROM users WHERE age = 42;") >=
        (int) sizeof(select_age_sql)) {
        fprintf(stderr, "error: failed to format age benchmark query\n");
        goto cleanup;
    }
    if (!parse_sql_text(select_id_sql, &select_id_workload, &error)) {
        fprintf(stderr, "error: %s\n", error.message);
        goto cleanup;
    }
    if (!parse_sql_text(select_age_sql, &select_age_workload, &error)) {
        fprintf(stderr, "error: %s\n", error.message);
        goto cleanup;
    }

    if (!run_query_iterations(&insert_workload, db_root, 1, "bulk insert", &insert_elapsed_ms, &error)) {
        fprintf(stderr, "error: %s\n", error.message);
        goto cleanup;
    }
    if (!run_query_iterations(&select_id_workload, db_root, iterations, "select where id", &select_id_elapsed_ms, &error)) {
        fprintf(stderr, "error: %s\n", error.message);
        goto cleanup;
    }
    if (!run_query_iterations(&select_age_workload, db_root, iterations, "select where age", &select_age_elapsed_ms, &error)) {
        fprintf(stderr, "error: %s\n", error.message);
        goto cleanup;
    }

    printf("[BENCH] rows=%ld iterations=%ld\n", row_count, iterations);
    print_metric("bulk insert", insert_elapsed_ms, row_count);
    print_metric("select where id", select_id_elapsed_ms, iterations);
    print_metric("select where age", select_age_elapsed_ms, iterations);
    if (select_id_elapsed_ms > 0.0) {
        printf(
            "[BENCH] select_age/select_id ratio=%8.2fx\n",
            select_age_elapsed_ms / select_id_elapsed_ms
        );
    }
    ok = 1;

cleanup:
    free(insert_sql);
    cleanup_parsed_sql(&insert_workload);
    cleanup_parsed_sql(&select_id_workload);
    cleanup_parsed_sql(&select_age_workload);
    test_cleanup_users_db(db_root);
    return ok ? 0 : 1;
}
