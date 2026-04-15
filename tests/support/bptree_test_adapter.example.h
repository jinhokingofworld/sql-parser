#ifndef TESTS_SUPPORT_BPTREE_TEST_ADAPTER_EXAMPLE_H
#define TESTS_SUPPORT_BPTREE_TEST_ADAPTER_EXAMPLE_H

/*
 * ms: Copy this file to tests/support/bptree_test_adapter.h and replace the
 * placeholder types/functions with the real B+ Tree API once that code lands.
 *
 * Expected adapter surface used by tests/unit/test_bptree_contract.c:
 *
 *   typedef <your-tree-type> *BptreeTestHandle;
 *   BptreeTestHandle bptree_test_create(void);
 *   void bptree_test_destroy(BptreeTestHandle tree);
 *   int bptree_test_insert(BptreeTestHandle tree, long key, long value);
 *   int bptree_test_find(BptreeTestHandle tree, long key, long *out_value);
 */

/* ms: Keep the example commented out so the repository compiles before the real adapter exists. */
/*
typedef struct BPlusTree *BptreeTestHandle;

static inline BptreeTestHandle bptree_test_create(void) {
    return bptree_create();
}

static inline void bptree_test_destroy(BptreeTestHandle tree) {
    bptree_destroy(tree);
}

static inline int bptree_test_insert(BptreeTestHandle tree, long key, long value) {
    return bptree_insert(tree, key, value);
}

static inline int bptree_test_find(BptreeTestHandle tree, long key, long *out_value) {
    return bptree_find(tree, key, out_value);
}
*/

#endif
