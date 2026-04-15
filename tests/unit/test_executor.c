#include "executor.h"
#include "db_context.h"
#include "parser.h"
#include "tokenizer.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static int assert_true(int condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "assertion failed: %s\n", message);
        return 0;
    }
    return 1;
}

static int write_text_file(const char *path, const char *text) {
    FILE *file = fopen(path, "w");

    if (file == NULL) {
        return 0;
    }
    fputs(text, file);
    fclose(file);
    return 1;
}

static int build_path(char *buffer, size_t size, const char *dir, const char *leaf) {
    int written = snprintf(buffer, size, "%s/%s", dir, leaf);

    return written >= 0 && (size_t) written < size;
}

static int run_query_list_with_ctx(const char *sql, DbContext *ctx, FILE *output, SqlError *error) {
    TokenArray tokens = {NULL, 0};
    QueryList queries = {NULL, 0};
    int ok = 0;

    if (tokenize_sql(sql, &tokens, error) &&
        parse_queries(&tokens, &queries, error) &&
        execute_query_list(&queries, ctx, output, error)) {
        ok = 1;
    }

    free_token_array(&tokens);
    free_query_list(&queries);
    return ok;
}

static int create_temp_db(char *path, size_t size) {
    char schema_path[1024];
    char table_path[1024];
    char schema_dir[1024];
    char table_dir[1024];

    snprintf(path, size, "/tmp/sql-parser-executor-%ld", (long) getpid());
    unlink(path);
    rmdir(path);
    if (mkdir(path, 0700) != 0) {
        return 0;
    }

    if (!build_path(schema_dir, sizeof(schema_dir), path, "schema") ||
        !build_path(table_dir, sizeof(table_dir), path, "tables")) {
        return 0;
    }
    if (mkdir(schema_dir, 0700) != 0 || mkdir(table_dir, 0700) != 0) {
        return 0;
    }

    if (!build_path(schema_path, sizeof(schema_path), schema_dir, "users.schema") ||
        !build_path(table_path, sizeof(table_path), table_dir, "users.csv")) {
        return 0;
    }
    return write_text_file(
            schema_path,
            "table=users\ncolumns=id:int,name:string,grade:int,age:int,region:string,score:float\npkey=id\nautoincrement=true\n"
        ) &&
        write_text_file(table_path, "1,Alice,2,20,Seoul,3.80\n2,Bob,4,25,Busan,4.10\n");
}

static void cleanup_temp_db(const char *db_root) {
    char schema_path[1024];
    char table_path[1024];
    char schema_dir[1024];
    char table_dir[1024];

    if (!build_path(schema_dir, sizeof(schema_dir), db_root, "schema") ||
        !build_path(table_dir, sizeof(table_dir), db_root, "tables") ||
        !build_path(schema_path, sizeof(schema_path), schema_dir, "users.schema") ||
        !build_path(table_path, sizeof(table_path), table_dir, "users.csv")) {
        return;
    }

    unlink(schema_path);
    unlink(table_path);
    rmdir(table_path);
    rmdir(schema_dir);
    rmdir(table_dir);
    rmdir(db_root);
}

static char *run_sql(const char *sql, const char *db_root) {
    DbContext *ctx = NULL;
    SqlError error = {0, 0, {0}};
    FILE *output = tmpfile();
    long length;
    char *buffer;

    if (output == NULL) {
        return NULL;
    }

    ctx = db_context_create(db_root, &error);
    if (ctx == NULL || !run_query_list_with_ctx(sql, ctx, output, &error)) {
        fprintf(stderr, "run_sql failed: %s\n", error.message);
        fclose(output);
        db_context_destroy(ctx);
        return NULL;
    }

    fflush(output);
    fseek(output, 0L, SEEK_END);
    length = ftell(output);
    rewind(output);

    buffer = malloc((size_t) length + 1U);
    if (buffer == NULL) {
        fclose(output);
        db_context_destroy(ctx);
        return NULL;
    }

    fread(buffer, 1U, (size_t) length, output);
    buffer[length] = '\0';

    fclose(output);
    db_context_destroy(ctx);
    return buffer;
}

static int run_sql_expect_failure(const char *sql, const char *db_root, char *buffer, size_t size) {
    DbContext *ctx = NULL;
    SqlError error = {0, 0, {0}};
    int ok = 0;

    ctx = db_context_create(db_root, &error);
    if (ctx != NULL && run_query_list_with_ctx(sql, ctx, stdout, &error)) {
        ok = 0;
    } else {
        snprintf(buffer, size, "%s", error.message);
        ok = 1;
    }

    db_context_destroy(ctx);
    return ok;
}

static int replace_table_file_with_directory(const char *db_root) {
    char tables_dir[1024];
    char table_path[1024];

    if (!build_path(tables_dir, sizeof(tables_dir), db_root, "tables")) {
        return 0;
    }
    if (!build_path(table_path, sizeof(table_path), tables_dir, "users.csv")) {
        return 0;
    }

    unlink(table_path);
    return mkdir(table_path, 0700) == 0;
}

int main(void) {
    char db_root[1024];
    char *insert_output;
    char duplicate_error[256] = {0};
    char *select_output;
    DbContext *ctx = NULL;
    SqlError error = {0, 0, {0}};
    FILE *failure_output = NULL;
    char failure_message[256] = {0};
    TableState *table = NULL;
    int ok = 1;

    if (!create_temp_db(db_root, sizeof(db_root))) {
        fprintf(stderr, "failed to create temp db\n");
        return 1;
    }

    insert_output = run_sql(
        "INSERT INTO users (name, grade, age, region, score) VALUES ('Charlie', 1, 19, 'Incheon', 3.25);",
        db_root
    );
    select_output = run_sql(
        "SELECT name FROM users WHERE id BETWEEN 2 AND 3 ORDER BY id;",
        db_root
    );
    ok &= assert_true(
        run_sql_expect_failure(
            "INSERT INTO users (id, name, grade, age, region, score) VALUES (2, 'Bobby', 3, 28, 'Daegu', 3.10);",
            db_root,
            duplicate_error,
            sizeof(duplicate_error)
        ),
        "duplicate INSERT should fail"
    );

    ok &= assert_true(insert_output != NULL, "INSERT output should exist");
    ok &= assert_true(select_output != NULL, "SELECT output should exist");
    if (insert_output != NULL) {
        ok &= assert_true(strcmp(insert_output, "INSERT 1\n") == 0, "INSERT output mismatch");
    }
    if (select_output != NULL) {
        ok &= assert_true(strstr(select_output, "Bob") != NULL, "SELECT should include Bob");
        ok &= assert_true(strstr(select_output, "Charlie") != NULL, "SELECT should include Charlie");
        ok &= assert_true(strstr(select_output, "(2 rows)") != NULL, "SELECT row count mismatch");
    }
    ok &= assert_true(
        strstr(duplicate_error, "reserved and cannot be specified manually") != NULL,
        "manual id INSERT should report reserved column error"
    );

    ctx = db_context_create(db_root, &error);
    ok &= assert_true(ctx != NULL, "DbContext should be created for append failure test");
    ok &= assert_true(replace_table_file_with_directory(db_root), "users.csv should be replaced with a directory");
    failure_output = tmpfile();
    ok &= assert_true(failure_output != NULL, "failure output stream should exist");
    if (ctx != NULL && failure_output != NULL) {
        ok &= assert_true(
            !run_query_list_with_ctx(
                "INSERT INTO users (name, grade, age, region, score) VALUES ('Delta', 3, 23, 'Daejeon', 3.55);",
                ctx,
                failure_output,
                &error
            ),
            "INSERT should fail when CSV append fails"
        );
        snprintf(failure_message, sizeof(failure_message), "%s", error.message);
        ok &= assert_true(strstr(failure_message, "failed to open table") != NULL, "append failure should report file open error");
        table = db_context_find_table(ctx, "users");
        ok &= assert_true(table != NULL, "users table should still exist after failed INSERT");
        if (table != NULL) {
            ok &= assert_true(table->rowset.row_count == 3, "failed INSERT should roll back the published row");
            ok &= assert_true(table->index == NULL, "failed append should invalidate the index");
            ok &= assert_true(table->next_id == 4, "failed INSERT should not advance next_id");
        }
    }

    if (ctx != NULL && failure_output != NULL) {
        FILE *select_after_failure = tmpfile();
        char buffer[1024];
        long length = 0;

        ok &= assert_true(select_after_failure != NULL, "post-failure SELECT output should exist");
        if (select_after_failure != NULL) {
            error.message[0] = '\0';
            ok &= assert_true(
                run_query_list_with_ctx(
                    "SELECT name FROM users ORDER BY id;",
                    ctx,
                    select_after_failure,
                    &error
                ),
                "SELECT should still succeed on the in-memory context after failed INSERT"
            );
            fflush(select_after_failure);
            fseek(select_after_failure, 0L, SEEK_END);
            length = ftell(select_after_failure);
            rewind(select_after_failure);
            memset(buffer, 0, sizeof(buffer));
            if ((size_t) length < sizeof(buffer)) {
                fread(buffer, 1U, (size_t) length, select_after_failure);
                ok &= assert_true(strstr(buffer, "Alice") != NULL, "post-failure SELECT should include Alice");
                ok &= assert_true(strstr(buffer, "Bob") != NULL, "post-failure SELECT should include Bob");
                ok &= assert_true(strstr(buffer, "Charlie") != NULL, "post-failure SELECT should still include Charlie");
                ok &= assert_true(strstr(buffer, "Delta") == NULL, "failed INSERT row should not be published");
                ok &= assert_true(strstr(buffer, "(3 rows)") != NULL, "failed INSERT should preserve row count");
            } else {
                ok &= assert_true(0, "post-failure SELECT output exceeded buffer");
            }
            fclose(select_after_failure);
        }
    }

    if (failure_output != NULL) {
        fclose(failure_output);
    }
    db_context_destroy(ctx);
    free(insert_output);
    free(select_output);
    cleanup_temp_db(db_root);
    return ok ? 0 : 1;
}
