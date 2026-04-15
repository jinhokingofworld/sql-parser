#ifndef TESTS_SUPPORT_BPTREE_TEST_ADAPTER_H
#define TESTS_SUPPORT_BPTREE_TEST_ADAPTER_H

#include "bptree.h"

/* ms: The contract tests operate on long values, so the adapter narrows them into the real tree API. */
typedef BPTree *BptreeTestHandle;

static inline BptreeTestHandle bptree_test_create(void) {
    return bptree_create();
}

static inline void bptree_test_destroy(BptreeTestHandle tree) {
    bptree_destroy(tree);
}

static inline int bptree_test_insert(BptreeTestHandle tree, long key, long value) {
    SqlError error = {0, 0, {0}};

    return bptree_insert(tree, (int) key, (int) value, &error);
}

static inline int bptree_test_find(BptreeTestHandle tree, long key, long *out_value) {
    int value = 0;

    if (!bptree_search(tree, (int) key, &value)) {
        return 0;
    }
    if (out_value != NULL) {
        *out_value = (long) value;
    }
    return 1;
}

#endif
