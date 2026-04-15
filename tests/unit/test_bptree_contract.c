#include "unity.h"

/*
 * ms: This suite is intentionally adapter-driven so the incoming B+ Tree API
 * can be mapped here without forcing immediate changes in the shared test
 * harness. To enable these tests, add tests/support/bptree_test_adapter.h.
 */

#if defined(__has_include)
#if __has_include("bptree_test_adapter.h")
#include "bptree_test_adapter.h"
#define BPTREE_TESTS_AVAILABLE 1
#else
#define BPTREE_TESTS_AVAILABLE 0
#endif
#else
#define BPTREE_TESTS_AVAILABLE 0
#endif

void setUp(void) {
}

void tearDown(void) {
}

#if BPTREE_TESTS_AVAILABLE

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

#else

/* ms: Before the B+ Tree lands, keep this target green with an explicit skip marker. */
static void test_bptree_contract_waits_for_adapter(void) {
    printf("[SKIP] add tests/support/bptree_test_adapter.h to enable B+ Tree contract tests\n");
    TEST_ASSERT_TRUE(1);
}

#endif

int main(void) {
    /* ms: Drive either the real contract suite or the explicit skip placeholder. */
    UNITY_BEGIN();
#if BPTREE_TESTS_AVAILABLE
    RUN_TEST(test_insert_then_find_single_key);
    RUN_TEST(test_find_missing_key_returns_not_found);
    RUN_TEST(test_many_inserts_remain_searchable);
#else
    RUN_TEST(test_bptree_contract_waits_for_adapter);
#endif
    return UNITY_END();
}
