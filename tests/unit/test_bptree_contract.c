#include "unity.h"
#include "bptree_test_adapter.h"

/*
 * ms: This suite is intentionally adapter-driven so the incoming B+ Tree API
 * can be mapped here without forcing test changes when the production B+ Tree
 * API shifts.
 */

/* ms: Keep the handle global so each test can rebuild a fresh tree through the adapter. */
static BptreeTestHandle g_tree = NULL;

void setUp(void) {
    g_tree = bptree_test_create();
    TEST_ASSERT_NOT_NULL_MESSAGE(g_tree, "B+ Tree adapter failed to create a tree");
}

void tearDown(void) {
    if (g_tree != NULL) {
        bptree_test_destroy(g_tree);
        g_tree = NULL;
    }
}

/* ms: The minimum contract is that a single inserted key can be found exactly once. */
static void test_insert_then_find_single_key(void) {
    long value = -1;

    TEST_ASSERT_TRUE_MESSAGE(
        bptree_test_insert(g_tree, 10L, 100L),
        "insert should succeed for a fresh key"
    );
    TEST_ASSERT_TRUE_MESSAGE(
        bptree_test_find(g_tree, 10L, &value),
        "find should succeed for an inserted key"
    );
    TEST_ASSERT_EQUAL_INT(100L, value);
}

/* ms: Missing-key lookups must fail cleanly without mutating the output value. */
static void test_find_missing_key_returns_not_found(void) {
    long value = 777L;

    TEST_ASSERT_FALSE_MESSAGE(
        bptree_test_find(g_tree, 999L, &value),
        "find should report not found for a missing key"
    );
    TEST_ASSERT_EQUAL_INT(777L, value);
}

/* ms: Many inserts should stay searchable, which indirectly covers internal splits and routing. */
static void test_many_inserts_remain_searchable(void) {
    long key;

    for (key = 1; key <= 64; key++) {
        TEST_ASSERT_TRUE_MESSAGE(
            bptree_test_insert(g_tree, key, key * 10L),
            "bulk insert should succeed for unique keys"
        );
    }

    for (key = 1; key <= 64; key++) {
        long value = -1;

        TEST_ASSERT_TRUE_MESSAGE(
            bptree_test_find(g_tree, key, &value),
            "all inserted keys should remain searchable"
        );
        TEST_ASSERT_EQUAL_INT(key * 10L, value);
    }
}

int main(void) {
    /* ms: Drive the adapter-backed contract suite against the real B+ Tree implementation. */
    UNITY_BEGIN();
    RUN_TEST(test_insert_then_find_single_key);
    RUN_TEST(test_find_missing_key_returns_not_found);
    RUN_TEST(test_many_inserts_remain_searchable);
    return UNITY_END();
}
