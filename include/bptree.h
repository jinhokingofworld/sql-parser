#ifndef BPTREE_H
#define BPTREE_H

#include "common.h"

#define BPTREE_ORDER 100

typedef struct BPTreeNode {
    int is_leaf;
    int key_count;
    int keys[BPTREE_ORDER - 1];
    struct BPTreeNode *children[BPTREE_ORDER];
    int values[BPTREE_ORDER - 1];
    struct BPTreeNode *next;
    struct BPTreeNode *prev;
} BPTreeNode;

typedef struct {
    BPTreeNode *root;
} BPTree;

BPTree *bptree_create(void);
void bptree_destroy(BPTree *tree);
int bptree_insert(BPTree *tree, int key, int row_index, SqlError *error);
int bptree_search(const BPTree *tree, int key, int *row_index);
int bptree_search_with_stats(
    const BPTree *tree,
    int key,
    int *row_index,
    int *out_steps
);
int bptree_range_search(
    const BPTree *tree,
    int min_key,
    int max_key,
    int **out_indexes,
    int *out_count,
    SqlError *error
);
int bptree_range_search_with_stats(
    const BPTree *tree,
    int min_key,
    int max_key,
    int **out_indexes,
    int *out_count,
    int *out_steps,
    SqlError *error
);
int bptree_height(const BPTree *tree);
int bptree_node_count(const BPTree *tree);

#endif
