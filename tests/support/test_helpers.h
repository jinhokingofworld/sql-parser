#ifndef TESTS_SUPPORT_TEST_HELPERS_H
#define TESTS_SUPPORT_TEST_HELPERS_H

#include <stddef.h>

int test_build_temp_root(char *buffer, size_t size, const char *prefix);
int test_create_db_layout(const char *db_root, int create_schema_dir, int create_tables_dir);
int test_write_text_file(const char *path, const char *text);
int test_build_path(char *buffer, size_t size, const char *left, const char *right);
int test_create_users_db(const char *db_root, const char *schema_text, const char *csv_text);
void test_cleanup_users_db(const char *db_root);

#endif
