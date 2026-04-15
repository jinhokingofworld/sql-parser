#include "storage.h"
#include "test_helpers.h"
#include "unity.h"

static char g_db_root[1024];

/* ms: Each test gets its own temp root so fixture-based checks stay isolated. */
void setUp(void) {
    TEST_ASSERT_TRUE(test_build_temp_root(g_db_root, sizeof(g_db_root), "sql-parser-storage"));
}

void tearDown(void) {
    test_cleanup_table_db(g_db_root, "students");
}

/* ms: Keeps the original CSV escaping check but under the shared Unity runner. */
static void test_append_and_read_csv_preserves_escaped_fields(void) {
    char *fields[] = {"1", "Alice, Jr.", "She said \"hi\""};
    RowSet rowset;
    SqlError error = {0, 0, {0}};
    int ok = test_create_db_layout(g_db_root, 0, 1);

    if (!ok) {
        TEST_ASSERT_TRUE_MESSAGE(ok, "failed to create temp db");
        return;
    }

    ok = append_csv_row(g_db_root, "students", fields, 3, &error);
    if (!ok) {
        TEST_ASSERT_TRUE_MESSAGE(ok, error.message);
        return;
    }

    ok = read_csv_rows(g_db_root, "students", 3, &rowset, &error);
    if (!ok) {
        TEST_ASSERT_TRUE_MESSAGE(ok, error.message);
        return;
    }
    TEST_ASSERT_EQUAL_INT(1, rowset.row_count);
    TEST_ASSERT_EQUAL_STRING("Alice, Jr.", rowset.rows[0].fields[1]);
    TEST_ASSERT_EQUAL_STRING("She said \"hi\"", rowset.rows[0].fields[2]);

    free_rowset(&rowset);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_append_and_read_csv_preserves_escaped_fields);
    return UNITY_END();
}
