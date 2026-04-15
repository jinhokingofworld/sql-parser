#include "bench.h"

#include "db_context.h"
#include "executor.h"
#include "parser.h"
#include "tokenizer.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#include <process.h>
#define bench_mkdir(path) _mkdir(path)
#define bench_stderr_isatty() _isatty(_fileno(stderr))
#else
#include <unistd.h>
#define bench_mkdir(path) mkdir(path, 0700)
#define bench_stderr_isatty() isatty(STDERR_FILENO)
#endif

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

static void cleanup_parsed_sql(ParsedSql *parsed) {
    free_token_array(&parsed->tokens);
    free_query_list(&parsed->queries);
    parsed->tokens.items = NULL;
    parsed->tokens.count = 0;
    parsed->queries.items = NULL;
    parsed->queries.count = 0;
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

static int bench_build_path(char *buffer, size_t size, const char *left, const char *right) {
    int written = snprintf(buffer, size, "%s/%s", left, right);

    return written >= 0 && (size_t) written < size;
}

static int bench_write_text_file(const char *path, const char *text) {
    FILE *file = fopen(path, "w");

    if (file == NULL) {
        return 0;
    }

    fputs(text, file);
    fclose(file);
    return 1;
}

static int bench_build_temp_root(char *buffer, size_t size, const char *prefix) {
    long pid = 0;

#ifdef _WIN32
    pid = (long) _getpid();
#else
    pid = (long) getpid();
#endif

    return snprintf(buffer, size, "%s/%s-%ld", "tests/tmp", prefix, pid) < (int) size;
}

static int bench_create_db_layout(const char *db_root) {
    char schema_dir[1024];
    char tables_dir[1024];

    (void) unlink(db_root);
    (void) rmdir(db_root);
    (void) bench_mkdir("tests/tmp");
    if (bench_mkdir(db_root) != 0) {
        return 0;
    }
    if (!bench_build_path(schema_dir, sizeof(schema_dir), db_root, "schema") ||
        !bench_build_path(tables_dir, sizeof(tables_dir), db_root, "tables")) {
        return 0;
    }
    if (bench_mkdir(schema_dir) != 0 || bench_mkdir(tables_dir) != 0) {
        return 0;
    }

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
    char schema_dir[1024];
    char tables_dir[1024];
    char schema_path[1024];
    char table_path[1024];

    if (!bench_create_db_layout(db_root)) {
        return 0;
    }
    if (!bench_build_path(schema_dir, sizeof(schema_dir), db_root, "schema") ||
        !bench_build_path(tables_dir, sizeof(tables_dir), db_root, "tables") ||
        !bench_build_path(schema_path, sizeof(schema_path), schema_dir, "users.schema") ||
        !bench_build_path(table_path, sizeof(table_path), tables_dir, "users.csv")) {
        return 0;
    }

    return bench_write_text_file(
            schema_path,
            "table=users\n"
            "columns=id:int,name:string,age:int\n"
            "pkey=id\n"
            "autoincrement=true\n"
        ) &&
        bench_write_text_file(table_path, "");
}

static void bench_cleanup_users_db(const char *db_root) {
    char schema_dir[1024];
    char tables_dir[1024];
    char schema_path[1024];
    char table_path[1024];

    if (!bench_build_path(schema_dir, sizeof(schema_dir), db_root, "schema") ||
        !bench_build_path(tables_dir, sizeof(tables_dir), db_root, "tables") ||
        !bench_build_path(schema_path, sizeof(schema_path), schema_dir, "users.schema") ||
        !bench_build_path(table_path, sizeof(table_path), tables_dir, "users.csv")) {
        return;
    }

    unlink(schema_path);
    unlink(table_path);
    rmdir(schema_dir);
    rmdir(tables_dir);
    rmdir(db_root);
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

static void print_metric(FILE *out, const char *label, double total_ms, long operations) {
    fprintf(
        out,
        "[BENCH] %-24s total_ms=%9.3f avg_ms=%8.6f ops=%ld\n",
        label,
        total_ms,
        operations > 0 ? total_ms / (double) operations : 0.0,
        operations
    );
}

int bench_run(const char *db_root, long row_count, long iterations, FILE *out, SqlError *error) {
    char temp_db_root[1024];
    char *insert_sql = NULL;
    char select_id_sql[128];
    char select_age_sql[128];
    ParsedSql insert_workload = {{NULL, 0}, {NULL, 0}};
    ParsedSql select_id_workload = {{NULL, 0}, {NULL, 0}};
    ParsedSql select_age_workload = {{NULL, 0}, {NULL, 0}};
    double insert_elapsed_ms = 0.0;
    double select_id_elapsed_ms = 0.0;
    double select_age_elapsed_ms = 0.0;
    int ok = 0;

    (void) db_root;

    if (!bench_build_temp_root(temp_db_root, sizeof(temp_db_root), "sql-benchmark")) {
        sql_set_error(error, 0, 0, "failed to create benchmark temp root");
        return 0;
    }
    if (!create_empty_users_db(temp_db_root)) {
        sql_set_error(error, 0, 0, "failed to create benchmark database fixture");
        goto cleanup;
    }

    if (!build_insert_sql(row_count, &insert_sql, error)) {
        goto cleanup;
    }
    if (!parse_sql_text(insert_sql, &insert_workload, error)) {
        goto cleanup;
    }

    fprintf(stderr, "[BENCH] parsing select workloads\n");
    fflush(stderr);
    if (snprintf(select_id_sql, sizeof(select_id_sql), "SELECT * FROM users WHERE id = %ld;", row_count / 2L) >=
        (int) sizeof(select_id_sql)) {
        sql_set_error(error, 0, 0, "failed to format id benchmark query");
        goto cleanup;
    }
    if (snprintf(select_age_sql, sizeof(select_age_sql), "SELECT * FROM users WHERE age = 42;") >=
        (int) sizeof(select_age_sql)) {
        sql_set_error(error, 0, 0, "failed to format age benchmark query");
        goto cleanup;
    }
    if (!parse_sql_text(select_id_sql, &select_id_workload, error)) {
        goto cleanup;
    }
    if (!parse_sql_text(select_age_sql, &select_age_workload, error)) {
        goto cleanup;
    }

    if (!run_query_iterations(&insert_workload, temp_db_root, 1, "bulk insert", &insert_elapsed_ms, error)) {
        goto cleanup;
    }
    if (!run_query_iterations(
            &select_id_workload,
            temp_db_root,
            iterations,
            "select where id",
            &select_id_elapsed_ms,
            error
        )) {
        goto cleanup;
    }
    if (!run_query_iterations(
            &select_age_workload,
            temp_db_root,
            iterations,
            "select where age",
            &select_age_elapsed_ms,
            error
        )) {
        goto cleanup;
    }

    fprintf(out, "[BENCH] rows=%ld iterations=%ld\n", row_count, iterations);
    print_metric(out, "bulk insert", insert_elapsed_ms, row_count);
    print_metric(out, "select where id", select_id_elapsed_ms, iterations);
    print_metric(out, "select where age", select_age_elapsed_ms, iterations);
    if (select_id_elapsed_ms > 0.0) {
        fprintf(
            out,
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
    bench_cleanup_users_db(temp_db_root);
    return ok;
}
