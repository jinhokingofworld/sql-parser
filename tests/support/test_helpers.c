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

/* ms: Test fixtures create schema and table files directly instead of depending on external setup scripts. */
int test_create_users_db(const char *db_root, const char *schema_text, const char *csv_text) {
    char schema_dir[1024];
    char table_dir[1024];
    char schema_path[1024];
    char table_path[1024];

    if (!test_create_db_layout(db_root, 1, 1)) {
        return 0;
    }
    if (!test_build_path(schema_dir, sizeof(schema_dir), db_root, "schema")) {
        return 0;
    }
    if (!test_build_path(table_dir, sizeof(table_dir), db_root, "tables")) {
        return 0;
    }
    if (!test_build_path(schema_path, sizeof(schema_path), schema_dir, "users.schema")) {
        return 0;
    }
    if (!test_build_path(table_path, sizeof(table_path), table_dir, "users.csv")) {
        return 0;
    }

    return test_write_text_file(schema_path, schema_text) &&
        test_write_text_file(table_path, csv_text);
}

/* ms: Cleanup matches the users fixture shape used by current unit tests. */
void test_cleanup_users_db(const char *db_root) {
    char schema_dir[1024];
    char table_dir[1024];
    char schema_path[1024];
    char table_path[1024];

    if (!test_build_path(schema_dir, sizeof(schema_dir), db_root, "schema")) {
        return;
    }
    if (!test_build_path(table_dir, sizeof(table_dir), db_root, "tables")) {
        return;
    }
    if (!test_build_path(schema_path, sizeof(schema_path), schema_dir, "users.schema")) {
        return;
    }
    if (!test_build_path(table_path, sizeof(table_path), table_dir, "users.csv")) {
        return;
    }

    unlink(schema_path);
    unlink(table_path);
    rmdir(schema_dir);
    rmdir(table_dir);
    rmdir(db_root);
}
