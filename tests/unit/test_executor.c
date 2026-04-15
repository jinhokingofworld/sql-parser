#include "executor.h"
#include "parser.h"
#include "tokenizer.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#ifdef _WIN32
#include <direct.h>
#define sql_mkdir(path) _mkdir(path)
#else
#define sql_mkdir(path) mkdir(path, 0700)
#endif

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

static int build_path(char *buffer, size_t size, const char *dir, const char *name) {
    int written = snprintf(buffer, size, "%s/%s", dir, name);

    return written >= 0 && (size_t) written < size;
}

static int create_temp_db(char *path, size_t size) {
    char schema_path[1024];
    char table_path[1024];
    char student_schema_path[1024];
    char student_table_path[1024];
    char schema_dir[1024];
    char table_dir[1024];

    snprintf(path, size, "tmp-sql-parser-executor-%ld", (long) getpid());
    unlink(path);
    rmdir(path);
    if (sql_mkdir(path) != 0) {
        return 0;
    }

    snprintf(schema_dir, sizeof(schema_dir), "%s/schema", path);
    snprintf(table_dir, sizeof(table_dir), "%s/tables", path);
    if (sql_mkdir(schema_dir) != 0 || sql_mkdir(table_dir) != 0) {
        return 0;
    }

    if (!build_path(schema_path, sizeof(schema_path), schema_dir, "users.schema") ||
        !build_path(table_path, sizeof(table_path), table_dir, "users.csv") ||
        !build_path(student_schema_path, sizeof(student_schema_path), schema_dir, "students.schema") ||
        !build_path(student_table_path, sizeof(student_table_path), table_dir, "students.csv")) {
        return 0;
    }

    return write_text_file(schema_path, "table=users\ncolumns=id:int,name:string,age:int\npkey=id\n") &&
        write_text_file(table_path, "1,Alice,20\n2,Bob,31\n") &&
        write_text_file(
            student_schema_path,
            "table=students\ncolumns=id:int,name:string,grade:int,age:int,region:string,score:float\npkey=id\n"
        ) &&
        write_text_file(student_table_path, "1,Alice,2,20,Seoul,4.25\n2,Bob,3,21,Busan,3.50\n");
}

static void cleanup_temp_db(const char *db_root) {
    char schema_path[1024];
    char table_path[1024];
    char student_schema_path[1024];
    char student_table_path[1024];
    char schema_dir[1024];
    char table_dir[1024];

    if (!build_path(schema_dir, sizeof(schema_dir), db_root, "schema") ||
        !build_path(table_dir, sizeof(table_dir), db_root, "tables") ||
        !build_path(schema_path, sizeof(schema_path), schema_dir, "users.schema") ||
        !build_path(table_path, sizeof(table_path), table_dir, "users.csv") ||
        !build_path(student_schema_path, sizeof(student_schema_path), schema_dir, "students.schema") ||
        !build_path(student_table_path, sizeof(student_table_path), table_dir, "students.csv")) {
        return;
    }

    unlink(schema_path);
    unlink(table_path);
    unlink(student_schema_path);
    unlink(student_table_path);
    rmdir(schema_dir);
    rmdir(table_dir);
    rmdir(db_root);
}

static char *run_sql(const char *sql, const char *db_root) {
    TokenArray tokens = {NULL, 0};
    QueryList queries = {NULL, 0};
    SqlError error = {0, 0, {0}};
    FILE *output = tmpfile();
    long length;
    char *buffer;

    if (output == NULL) {
        return NULL;
    }

    if (!tokenize_sql(sql, &tokens, &error) ||
        !parse_queries(&tokens, &queries, &error) ||
        !execute_query_list(&queries, db_root, output, &error)) {
        fprintf(stderr, "run_sql failed: %s\n", error.message);
        fclose(output);
        free_token_array(&tokens);
        free_query_list(&queries);
        return NULL;
    }

    fflush(output);
    fseek(output, 0L, SEEK_END);
    length = ftell(output);
    rewind(output);

    buffer = malloc((size_t) length + 1U);
    if (buffer == NULL) {
        fclose(output);
        free_token_array(&tokens);
        free_query_list(&queries);
        return NULL;
    }

    fread(buffer, 1U, (size_t) length, output);
    buffer[length] = '\0';

    fclose(output);
    free_token_array(&tokens);
    free_query_list(&queries);
    return buffer;
}

static int run_sql_expect_failure(const char *sql, const char *db_root, char *buffer, size_t size) {
    TokenArray tokens = {NULL, 0};
    QueryList queries = {NULL, 0};
    SqlError error = {0, 0, {0}};
    int ok = 0;

    if (tokenize_sql(sql, &tokens, &error) &&
        parse_queries(&tokens, &queries, &error) &&
        execute_query_list(&queries, db_root, stdout, &error)) {
        ok = 0;
    } else {
        snprintf(buffer, size, "%s", error.message);
        ok = 1;
    }

    free_token_array(&tokens);
    free_query_list(&queries);
    return ok;
}

int main(void) {
    char db_root[1024];
    char *insert_output;
    char duplicate_error[256] = {0};
    char *select_output;
    char *float_insert_output;
    char *float_select_output;
    int ok = 1;

    if (!create_temp_db(db_root, sizeof(db_root))) {
        fprintf(stderr, "failed to create temp db\n");
        return 1;
    }

    insert_output = run_sql(
        "INSERT INTO users (id, name, age) VALUES (3, 'Charlie', 19);",
        db_root
    );
    select_output = run_sql(
        "SELECT name FROM users WHERE age = 31 ORDER BY name;",
        db_root
    );
    float_insert_output = run_sql(
        "INSERT INTO students (id, name, grade, age, region, score) VALUES (3, 'Charlie', 4, 23, 'Incheon', 3.75);",
        db_root
    );
    float_select_output = run_sql(
        "SELECT name FROM students WHERE score = 4.25 ORDER BY name;",
        db_root
    );
    ok &= assert_true(
        run_sql_expect_failure(
            "INSERT INTO users (id, name, age) VALUES (2, 'Bobby', 28);",
            db_root,
            duplicate_error,
            sizeof(duplicate_error)
        ),
        "duplicate INSERT should fail"
    );

    ok &= assert_true(insert_output != NULL, "INSERT output should exist");
    ok &= assert_true(select_output != NULL, "SELECT output should exist");
    ok &= assert_true(float_insert_output != NULL, "float INSERT output should exist");
    ok &= assert_true(float_select_output != NULL, "float SELECT output should exist");
    if (insert_output != NULL) {
        ok &= assert_true(strcmp(insert_output, "INSERT 1\n") == 0, "INSERT output mismatch");
    }
    if (select_output != NULL) {
        ok &= assert_true(strstr(select_output, "Bob") != NULL, "SELECT should include Bob");
        ok &= assert_true(strstr(select_output, "(1 rows)") != NULL, "SELECT row count mismatch");
    }
    if (float_insert_output != NULL) {
        ok &= assert_true(strcmp(float_insert_output, "INSERT 1\n") == 0, "float INSERT output mismatch");
    }
    if (float_select_output != NULL) {
        ok &= assert_true(strstr(float_select_output, "Alice") != NULL, "float SELECT should include Alice");
        ok &= assert_true(strstr(float_select_output, "(1 rows)") != NULL, "float SELECT row count mismatch");
    }
    ok &= assert_true(
        strstr(duplicate_error, "duplicate primary key") != NULL,
        "duplicate INSERT should report primary key error"
    );

    free(insert_output);
    free(select_output);
    free(float_insert_output);
    free(float_select_output);
    cleanup_temp_db(db_root);
    return ok ? 0 : 1;
}
