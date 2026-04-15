#include "storage.h"

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

static int create_temp_db(char *path, size_t size) {
    snprintf(path, size, "tmp-sql-parser-storage-%ld", (long) getpid());
    unlink(path);
    rmdir(path);
    if (sql_mkdir(path) != 0) {
        return 0;
    }
    snprintf(path + strlen(path), size - strlen(path), "/tables");
    if (sql_mkdir(path) != 0) {
        return 0;
    }
    path[strlen(path) - strlen("/tables")] = '\0';
    return 1;
}

static void cleanup_temp_db(const char *db_root) {
    char file_path[1024];
    char tables_path[1024];

    snprintf(file_path, sizeof(file_path), "%s/tables/users.csv", db_root);
    snprintf(tables_path, sizeof(tables_path), "%s/tables", db_root);
    unlink(file_path);
    rmdir(tables_path);
    rmdir(db_root);
}

int main(void) {
    char db_root[1024];
    char *fields[] = {"1", "Alice, Jr.", "She said \"hi\""};
    RowSet rowset;
    SqlError error = {0, 0, {0}};
    int ok = 1;

    if (!create_temp_db(db_root, sizeof(db_root))) {
        fprintf(stderr, "failed to create temp db\n");
        return 1;
    }

    if (!append_csv_row(db_root, "users", fields, 3, &error)) {
        fprintf(stderr, "append_csv_row failed: %s\n", error.message);
        cleanup_temp_db(db_root);
        return 1;
    }

    if (!read_csv_rows(db_root, "users", 3, &rowset, &error)) {
        fprintf(stderr, "read_csv_rows failed: %s\n", error.message);
        cleanup_temp_db(db_root);
        return 1;
    }

    ok &= assert_true(rowset.row_count == 1, "expected one row");
    ok &= assert_true(strcmp(rowset.rows[0].fields[1], "Alice, Jr.") == 0, "CSV comma escape failed");
    ok &= assert_true(strcmp(rowset.rows[0].fields[2], "She said \"hi\"") == 0, "CSV quote escape failed");

    free_rowset(&rowset);
    cleanup_temp_db(db_root);
    return ok ? 0 : 1;
}
