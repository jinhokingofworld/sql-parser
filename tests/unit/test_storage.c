#include "storage.h"
#include "test_helpers.h"
#include "unity.h"

static char g_db_root[1024];
static const char *g_users_schema_text =
    "table=users\n"
    "columns=id:int,name:string,grade:int,age:int,region:string,score:string\n"
    "pkey=id\n";

/* ms: Each test gets its own temp root so fixture-based checks stay isolated. */
void setUp(void) {
    TEST_ASSERT_TRUE(test_build_temp_root(g_db_root, sizeof(g_db_root), "sql-parser-storage"));
}

void tearDown(void) {
    test_cleanup_users_db(g_db_root);
}

/* ms: Shared parser keeps dataset contract checks explicit instead of duplicating atoi/strtod logic in each test. */
static int parse_int_strict(const char *text, long *value) {
    char *end = NULL;
    long parsed;

    errno = 0;
    parsed = strtol(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0') {
        return 0;
    }

    *value = parsed;
    return 1;
}

/* ms: Score validation needs both numeric range checking and decimal-place checking. */
static int parse_score_strict(const char *text, double *value) {
    char *end = NULL;
    char *dot = strchr(text, '.');
    double parsed;

    errno = 0;
    parsed = strtod(text, &end);
    if (errno != 0 || end == text || *end != '\0') {
        return 0;
    }
    if (dot != NULL && strlen(dot + 1) > 2U) {
        return 0;
    }

    *value = parsed;
    return 1;
}

/* ms: Grade-specific age ranges come from the dataset contract and should stay centralized. */
static int age_matches_grade(long grade, long age) {
    switch (grade) {
        case 1:
            return age >= 19 && age <= 23;
        case 2:
            return age >= 20 && age <= 24;
        case 3:
            return age >= 21 && age <= 25;
        case 4:
            return age >= 22 && age <= 26;
        default:
            return 0;
    }
}

/* ms: Text fixture comparisons should ignore CRLF/LF differences across local environments. */
static void normalize_newlines(char *text) {
    char *read_cursor = text;
    char *write_cursor = text;

    while (*read_cursor != '\0') {
        if (*read_cursor != '\r') {
            *write_cursor++ = *read_cursor;
        }
        read_cursor++;
    }

    *write_cursor = '\0';
}

/* ms: This helper is the reusable contract checker that future generator outputs can run against unchanged. */
static void assert_dataset_row_contract(const RowSet *rowset, long start_id) {
    int row_index;

    for (row_index = 0; row_index < rowset->row_count; row_index++) {
        long id = 0;
        long grade = 0;
        long age = 0;
        double score = 0.0;
        long expected_id = start_id + row_index;

        TEST_ASSERT_EQUAL_INT_MESSAGE(6, rowset->rows[row_index].field_count, "dataset rows must have 6 columns");
        TEST_ASSERT_TRUE_MESSAGE(
            parse_int_strict(rowset->rows[row_index].fields[0], &id),
            "id must be a strict integer"
        );
        TEST_ASSERT_TRUE_MESSAGE(id == expected_id, "id must increase by 1 from --start-id");
        TEST_ASSERT_TRUE_MESSAGE(
            rowset->rows[row_index].fields[1][0] != '\0',
            "name must be a non-empty string"
        );
        TEST_ASSERT_TRUE_MESSAGE(
            parse_int_strict(rowset->rows[row_index].fields[2], &grade),
            "grade must be a strict integer"
        );
        TEST_ASSERT_TRUE_MESSAGE(grade >= 1 && grade <= 4, "grade must stay in the range 1..4");
        TEST_ASSERT_TRUE_MESSAGE(
            parse_int_strict(rowset->rows[row_index].fields[3], &age),
            "age must be a strict integer"
        );
        TEST_ASSERT_TRUE_MESSAGE(age_matches_grade(grade, age), "age must stay inside the grade-specific range");
        TEST_ASSERT_TRUE_MESSAGE(
            rowset->rows[row_index].fields[4][0] != '\0',
            "region must be a non-empty string"
        );
        TEST_ASSERT_TRUE_MESSAGE(
            parse_score_strict(rowset->rows[row_index].fields[5], &score),
            "score must be a float with at most two decimal places"
        );
        TEST_ASSERT_TRUE_MESSAGE(score >= 0.0 && score <= 4.5, "score must stay in the range 0.00..4.50");
    }
}

/* ms: Positive generator fixtures should still prove that ids stay unique across the produced row set. */
static void assert_ids_are_unique(const RowSet *rowset) {
    int left;

    for (left = 0; left < rowset->row_count; left++) {
        int right;

        for (right = left + 1; right < rowset->row_count; right++) {
            TEST_ASSERT_FALSE_MESSAGE(
                strcmp(rowset->rows[left].fields[0], rowset->rows[right].fields[0]) == 0,
                "generated ids must be unique"
            );
        }
    }
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

    ok = append_csv_row(g_db_root, "users", fields, 3, &error);
    if (!ok) {
        TEST_ASSERT_TRUE_MESSAGE(ok, error.message);
        return;
    }

    ok = read_csv_rows(g_db_root, "users", 3, &rowset, &error);
    if (!ok) {
        TEST_ASSERT_TRUE_MESSAGE(ok, error.message);
        return;
    }
    TEST_ASSERT_EQUAL_INT(1, rowset.row_count);
    TEST_ASSERT_EQUAL_STRING("Alice, Jr.", rowset.rows[0].fields[1]);
    TEST_ASSERT_EQUAL_STRING("She said \"hi\"", rowset.rows[0].fields[2]);

    free_rowset(&rowset);
}

/* ms: Schema validation stays text-based for now because the SQL schema parser does not yet support char[] and float. */
static void test_generated_dataset_schema_matches_contract(void) {
    char schema_dir[1024];
    char schema_path[1024];
    char *schema_text = NULL;
    SqlError error = {0, 0, {0}};
    int ok = test_create_users_db(
        g_db_root,
        g_users_schema_text,
        "100,Alice Kim,1,19,Seoul,3.50\n"
    );

    if (!ok) {
        TEST_ASSERT_TRUE_MESSAGE(ok, "failed to create schema fixture");
        return;
    }

    ok = test_build_path(schema_dir, sizeof(schema_dir), g_db_root, "schema");
    if (!ok) {
        TEST_ASSERT_TRUE_MESSAGE(ok, "failed to build schema directory path");
        return;
    }
    ok = test_build_path(schema_path, sizeof(schema_path), schema_dir, "users.schema");
    if (!ok) {
        TEST_ASSERT_TRUE_MESSAGE(ok, "failed to build schema file path");
        return;
    }

    schema_text = sql_read_text_file(schema_path, &error);
    if (schema_text == NULL) {
        TEST_ASSERT_NOT_NULL_MESSAGE(schema_text, error.message);
        return;
    }

    normalize_newlines(schema_text);
    TEST_ASSERT_EQUAL_STRING(g_users_schema_text, schema_text);
    free(schema_text);
}

/* ms: Representative fixture rows are checked against the agreed generator contract instead of exact Faker strings. */
static void test_read_generated_fixture_rows_support_dataset_checks(void) {
    RowSet rowset;
    SqlError error = {0, 0, {0}};
    int ok = test_create_users_db(
        g_db_root,
        g_users_schema_text,
        "100,Alice Kim,1,19,Seoul,3.50\n"
        "101,Minho Park,2,24,Busan,4.25\n"
        "102,Sora Lee,4,22,Jeju,0.00\n"
    );

    if (!ok) {
        TEST_ASSERT_TRUE_MESSAGE(ok, "failed to create dataset fixture");
        return;
    }

    ok = read_csv_rows(g_db_root, "users", 6, &rowset, &error);
    if (!ok) {
        TEST_ASSERT_TRUE_MESSAGE(ok, error.message);
        return;
    }
    TEST_ASSERT_EQUAL_INT(3, rowset.row_count);
    assert_dataset_row_contract(&rowset, 100);
    assert_ids_are_unique(&rowset);

    free_rowset(&rowset);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_append_and_read_csv_preserves_escaped_fields);
    RUN_TEST(test_generated_dataset_schema_matches_contract);
    RUN_TEST(test_read_generated_fixture_rows_support_dataset_checks);
    return UNITY_END();
}
