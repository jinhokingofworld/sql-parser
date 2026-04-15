#ifndef BPTREE_H
#define BPTREE_H

#include "common.h"

#define BPTREE_ORDER 100

typedef struct BPTreeNode BPTreeNode;

typedef struct {
    BPTreeNode *root;
} BPTree;

BPTree *bptree_create(void);
void bptree_destroy(BPTree *tree);
int bptree_insert(BPTree *tree, int key, int row_index, SqlError *error);
int bptree_search(const BPTree *tree, int key, int *row_index);
int bptree_range_search(
    const BPTree *tree,
    int min_key,
    int max_key,
    int **out_indexes,
    int *out_count,
    SqlError *error
);
int bptree_height(const BPTree *tree);
int bptree_node_count(const BPTree *tree);

#endif
