#include "test_helpers.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static int make_directory(const char *path) {
#ifdef _WIN32
    return mkdir(path);
#else
    return mkdir(path, 0700);
#endif
}

/* ms: Centralize path joins so test files do not duplicate platform-sensitive string handling. */
int test_build_path(char *buffer, size_t size, const char *left, const char *right) {
    return snprintf(buffer, size, "%s/%s", left, right) < (int) size;
}

/* ms: Put temporary databases under tests/tmp so local runs work on this Windows workspace too. */
int test_build_temp_root(char *buffer, size_t size, const char *prefix) {
    if (make_directory("tests/tmp") != 0 && errno != EEXIST) {
        return 0;
    }

    return snprintf(buffer, size, "tests/tmp/%s-%ld", prefix, (long) getpid()) < (int) size;
}

/* ms: Shared temp DB layout helper lets storage/executor tests reuse the same setup pattern. */
int test_create_db_layout(const char *db_root, int create_schema_dir, int create_tables_dir) {
    char schema_dir[1024];
    char table_dir[1024];

    unlink(db_root);
    rmdir(db_root);
    if (make_directory(db_root) != 0) {
        return 0;
    }

    if (create_schema_dir) {
        if (!test_build_path(schema_dir, sizeof(schema_dir), db_root, "schema")) {
            return 0;
        }
        if (make_directory(schema_dir) != 0) {
            return 0;
        }
    }

    if (create_tables_dir) {
        if (!test_build_path(table_dir, sizeof(table_dir), db_root, "tables")) {
            return 0;
        }
        if (make_directory(table_dir) != 0) {
            return 0;
        }
    }

    return 1;
}

int test_write_text_file(const char *path, const char *text) {
    FILE *file = fopen(path, "w");

    if (file == NULL) {
        return 0;
    }

    fputs(text, file);
    fclose(file);
    return 1;
}

/* ms: Test fixtures can now target arbitrary table names so dataset tests are not locked to users. */
int test_create_table_db(const char *db_root, const char *table_name, const char *schema_text, const char *csv_text) {
    char schema_dir[1024];
    char table_dir[1024];
    char schema_path[1024];
    char table_path[1024];
    char schema_filename[256];
    char table_filename[256];

    if (!test_create_db_layout(db_root, 1, 1)) {
        return 0;
    }
    if (!test_build_path(schema_dir, sizeof(schema_dir), db_root, "schema")) {
        return 0;
    }
    if (!test_build_path(table_dir, sizeof(table_dir), db_root, "tables")) {
        return 0;
    }
    if (snprintf(schema_filename, sizeof(schema_filename), "%s.schema", table_name) >= (int) sizeof(schema_filename)) {
        return 0;
    }
    if (snprintf(table_filename, sizeof(table_filename), "%s.csv", table_name) >= (int) sizeof(table_filename)) {
        return 0;
    }
    if (!test_build_path(schema_path, sizeof(schema_path), schema_dir, schema_filename)) {
        return 0;
    }
    if (!test_build_path(table_path, sizeof(table_path), table_dir, table_filename)) {
        return 0;
    }

    return test_write_text_file(schema_path, schema_text) &&
        test_write_text_file(table_path, csv_text);
}

/* ms: Cleanup follows the table-specific fixture path so temporary schema/table files do not leak. */
void test_cleanup_table_db(const char *db_root, const char *table_name) {
    char schema_dir[1024];
    char table_dir[1024];
    char schema_path[1024];
    char table_path[1024];
    char schema_filename[256];
    char table_filename[256];

    if (!test_build_path(schema_dir, sizeof(schema_dir), db_root, "schema")) {
        return;
    }
    if (!test_build_path(table_dir, sizeof(table_dir), db_root, "tables")) {
        return;
    }
    if (snprintf(schema_filename, sizeof(schema_filename), "%s.schema", table_name) >= (int) sizeof(schema_filename)) {
        return;
    }
    if (snprintf(table_filename, sizeof(table_filename), "%s.csv", table_name) >= (int) sizeof(table_filename)) {
        return;
    }
    if (!test_build_path(schema_path, sizeof(schema_path), schema_dir, schema_filename)) {
        return;
    }
    if (!test_build_path(table_path, sizeof(table_path), table_dir, table_filename)) {
        return;
    }

    unlink(schema_path);
    unlink(table_path);
    rmdir(schema_dir);
    rmdir(table_dir);
    rmdir(db_root);
}

/* ms: Compatibility wrapper keeps older users-based tests working while new dataset tests move to students. */
int test_create_users_db(const char *db_root, const char *schema_text, const char *csv_text) {
    return test_create_table_db(db_root, "users", schema_text, csv_text);
}

/* ms: Compatibility wrapper keeps existing cleanup calls unchanged for legacy users tests. */
void test_cleanup_users_db(const char *db_root) {
    test_cleanup_table_db(db_root, "users");
}
