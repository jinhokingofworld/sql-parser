#include "bptree.h"

static int assert_true(int condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "assertion failed: %s\n", message);
        return 0;
    }
    return 1;
}

int main(void) {
    BPTree *tree = bptree_create();
    SqlError error = {0, 0, {0}};
    int *range_indexes = NULL;
    int range_count = 0;
    int row_index = -1;
    int ok = 1;
    int index;

    if (tree == NULL) {
        fprintf(stderr, "failed to create tree\n");
        return 1;
    }

    for (index = 1; index <= 250; index++) {
        if (!bptree_insert(tree, index, index * 10, &error)) {
            fprintf(stderr, "bptree_insert failed: %s\n", error.message);
            bptree_destroy(tree);
            return 1;
        }
    }

    ok &= assert_true(bptree_height(tree) >= 2, "tree height should grow after many inserts");
    ok &= assert_true(bptree_node_count(tree) > 1, "tree node count should be greater than 1");
    ok &= assert_true(bptree_search(tree, 125, &row_index) == 1, "existing key should be found");
    ok &= assert_true(row_index == 1250, "row index for key 125 should match");
    ok &= assert_true(bptree_search(tree, 9999, &row_index) == 0, "missing key should not be found");

    if (!bptree_range_search(tree, 10, 15, &range_indexes, &range_count, &error)) {
        fprintf(stderr, "bptree_range_search failed: %s\n", error.message);
        bptree_destroy(tree);
        return 1;
    }

    ok &= assert_true(range_count == 6, "range search count mismatch");
    ok &= assert_true(range_indexes[0] == 100, "first range index mismatch");
    ok &= assert_true(range_indexes[5] == 150, "last range index mismatch");

    free(range_indexes);
    bptree_destroy(tree);
    return ok ? 0 : 1;
}
