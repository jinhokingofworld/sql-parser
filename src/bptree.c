#include "bptree.h"

typedef struct {
    int split;
    int split_key;
    BPTreeNode *right;
} BPTreeInsertResult;

static BPTreeNode *bptree_create_node(int is_leaf, SqlError *error) {
    BPTreeNode *node = calloc(1, sizeof(BPTreeNode));

    if (node == NULL) {
        sql_set_error(error, 0, 0, "out of memory while creating B+ tree node");
        return NULL;
    }

    node->is_leaf = is_leaf;
    return node;
}

static void bptree_destroy_node(BPTreeNode *node) {
    int index;

    if (node == NULL) {
        return;
    }

    if (!node->is_leaf) {
        for (index = 0; index <= node->key_count; index++) {
            bptree_destroy_node(node->children[index]);
        }
    }

    free(node);
}

static int bptree_leaf_lower_bound(const BPTreeNode *node, int key) {
    int low = 0;
    int high = node->key_count;

    while (low < high) {
        int mid = low + (high - low) / 2;

        if (node->keys[mid] < key) {
            low = mid + 1;
        } else {
            high = mid;
        }
    }

    return low;
}

static int bptree_internal_child_index(const BPTreeNode *node, int key) {
    int low = 0;
    int high = node->key_count;

    while (low < high) {
        int mid = low + (high - low) / 2;

        if (key < node->keys[mid]) {
            high = mid;
        } else {
            low = mid + 1;
        }
    }

    return low;
}

static void bptree_leaf_insert_at(BPTreeNode *node, int index, int key, int value) {
    int cursor;

    for (cursor = node->key_count; cursor > index; cursor--) {
        node->keys[cursor] = node->keys[cursor - 1];
        node->values[cursor] = node->values[cursor - 1];
    }

    node->keys[index] = key;
    node->values[index] = value;
    node->key_count++;
}

static void bptree_internal_insert_at(BPTreeNode *node, int index, int key, BPTreeNode *right_child) {
    int cursor;

    for (cursor = node->key_count; cursor > index; cursor--) {
        node->keys[cursor] = node->keys[cursor - 1];
    }
    for (cursor = node->key_count + 1; cursor > index + 1; cursor--) {
        node->children[cursor] = node->children[cursor - 1];
    }

    node->keys[index] = key;
    node->children[index + 1] = right_child;
    node->key_count++;
}

static int bptree_insert_recursive(
    BPTreeNode *node,
    int key,
    int row_index,
    BPTreeInsertResult *result,
    SqlError *error
) {
    result->split = 0;
    result->split_key = 0;
    result->right = NULL;

    if (node->is_leaf) {
        int index = bptree_leaf_lower_bound(node, key);

        if (index < node->key_count && node->keys[index] == key) {
            sql_set_error(error, 0, 0, "duplicate primary key for column `id`: `%d`", key);
            return 0;
        }

        if (node->key_count < BPTREE_ORDER - 1) {
            bptree_leaf_insert_at(node, index, key, row_index);
            return 1;
        }

        {
            int temp_keys[BPTREE_ORDER];
            int temp_values[BPTREE_ORDER];
            int left_count = BPTREE_ORDER / 2;
            int right_count = BPTREE_ORDER - left_count;
            int cursor;
            BPTreeNode *right = NULL;

            for (cursor = 0; cursor < index; cursor++) {
                temp_keys[cursor] = node->keys[cursor];
                temp_values[cursor] = node->values[cursor];
            }
            temp_keys[index] = key;
            temp_values[index] = row_index;
            for (cursor = index; cursor < node->key_count; cursor++) {
                temp_keys[cursor + 1] = node->keys[cursor];
                temp_values[cursor + 1] = node->values[cursor];
            }

            right = bptree_create_node(1, error);
            if (right == NULL) {
                return 0;
            }

            node->key_count = left_count;
            for (cursor = 0; cursor < left_count; cursor++) {
                node->keys[cursor] = temp_keys[cursor];
                node->values[cursor] = temp_values[cursor];
            }

            right->key_count = right_count;
            for (cursor = 0; cursor < right_count; cursor++) {
                right->keys[cursor] = temp_keys[left_count + cursor];
                right->values[cursor] = temp_values[left_count + cursor];
            }

            right->next = node->next;
            if (right->next != NULL) {
                right->next->prev = right;
            }
            right->prev = node;
            node->next = right;

            result->split = 1;
            result->split_key = right->keys[0];
            result->right = right;
            return 1;
        }
    }

    {
        int child_index = bptree_internal_child_index(node, key);
        BPTreeInsertResult child_result;

        if (!bptree_insert_recursive(node->children[child_index], key, row_index, &child_result, error)) {
            return 0;
        }
        if (!child_result.split) {
            return 1;
        }

        if (node->key_count < BPTREE_ORDER - 1) {
            bptree_internal_insert_at(node, child_index, child_result.split_key, child_result.right);
            return 1;
        }

        {
            int temp_keys[BPTREE_ORDER];
            BPTreeNode *temp_children[BPTREE_ORDER + 1];
            int split_index = BPTREE_ORDER / 2;
            int right_key_count = (BPTREE_ORDER - 1) - split_index;
            int cursor;
            int promoted_key;
            BPTreeNode *right = NULL;

            for (cursor = 0; cursor < child_index; cursor++) {
                temp_keys[cursor] = node->keys[cursor];
            }
            temp_keys[child_index] = child_result.split_key;
            for (cursor = child_index; cursor < node->key_count; cursor++) {
                temp_keys[cursor + 1] = node->keys[cursor];
            }

            for (cursor = 0; cursor <= child_index; cursor++) {
                temp_children[cursor] = node->children[cursor];
            }
            temp_children[child_index + 1] = child_result.right;
            for (cursor = child_index + 1; cursor <= node->key_count; cursor++) {
                temp_children[cursor + 1] = node->children[cursor];
            }

            right = bptree_create_node(0, error);
            if (right == NULL) {
                return 0;
            }

            promoted_key = temp_keys[split_index];
            node->key_count = split_index;
            for (cursor = 0; cursor < split_index; cursor++) {
                node->keys[cursor] = temp_keys[cursor];
            }
            for (cursor = 0; cursor <= split_index; cursor++) {
                node->children[cursor] = temp_children[cursor];
            }

            right->key_count = right_key_count;
            for (cursor = 0; cursor < right_key_count; cursor++) {
                right->keys[cursor] = temp_keys[split_index + 1 + cursor];
            }
            for (cursor = 0; cursor <= right_key_count; cursor++) {
                right->children[cursor] = temp_children[split_index + 1 + cursor];
            }

            result->split = 1;
            result->split_key = promoted_key;
            result->right = right;
            return 1;
        }
    }
}

static const BPTreeNode *bptree_find_leaf(const BPTree *tree, int key) {
    const BPTreeNode *node = tree->root;

    while (node != NULL && !node->is_leaf) {
        node = node->children[bptree_internal_child_index(node, key)];
    }

    return node;
}

static int bptree_append_index(int **indexes, int *count, int *capacity, int value, SqlError *error) {
    int new_capacity;
    int *next_indexes;

    if (*count < *capacity) {
        (*indexes)[(*count)++] = value;
        return 1;
    }

    new_capacity = *capacity == 0 ? 8 : *capacity * 2;
    next_indexes = realloc(*indexes, sizeof(int) * (size_t) new_capacity);
    if (next_indexes == NULL) {
        sql_set_error(error, 0, 0, "out of memory while collecting B+ tree range search results");
        return 0;
    }

    *indexes = next_indexes;
    *capacity = new_capacity;
    (*indexes)[(*count)++] = value;
    return 1;
}

static int bptree_node_count_recursive(const BPTreeNode *node) {
    int count = 1;
    int index;

    if (node == NULL) {
        return 0;
    }
    if (node->is_leaf) {
        return count;
    }

    for (index = 0; index <= node->key_count; index++) {
        count += bptree_node_count_recursive(node->children[index]);
    }

    return count;
}

BPTree *bptree_create(void) {
    return calloc(1, sizeof(BPTree));
}

void bptree_destroy(BPTree *tree) {
    if (tree == NULL) {
        return;
    }

    bptree_destroy_node(tree->root);
    free(tree);
}

int bptree_insert(BPTree *tree, int key, int row_index, SqlError *error) {
    BPTreeInsertResult result;

    if (tree == NULL) {
        sql_set_error(error, 0, 0, "B+ tree is not initialized");
        return 0;
    }

    if (tree->root == NULL) {
        tree->root = bptree_create_node(1, error);
        if (tree->root == NULL) {
            return 0;
        }
        tree->root->keys[0] = key;
        tree->root->values[0] = row_index;
        tree->root->key_count = 1;
        return 1;
    }

    if (!bptree_insert_recursive(tree->root, key, row_index, &result, error)) {
        return 0;
    }

    if (result.split) {
        BPTreeNode *new_root = bptree_create_node(0, error);
        if (new_root == NULL) {
            return 0;
        }
        new_root->keys[0] = result.split_key;
        new_root->children[0] = tree->root;
        new_root->children[1] = result.right;
        new_root->key_count = 1;
        tree->root = new_root;
    }

    return 1;
}

int bptree_search(const BPTree *tree, int key, int *row_index) {
    const BPTreeNode *leaf;
    int index;

    if (row_index != NULL) {
        *row_index = 0;
    }
    if (tree == NULL || tree->root == NULL) {
        return 0;
    }

    leaf = bptree_find_leaf(tree, key);
    if (leaf == NULL) {
        return 0;
    }

    index = bptree_leaf_lower_bound(leaf, key);
    if (index >= leaf->key_count || leaf->keys[index] != key) {
        return 0;
    }

    if (row_index != NULL) {
        *row_index = leaf->values[index];
    }
    return 1;
}

int bptree_range_search(
    const BPTree *tree,
    int min_key,
    int max_key,
    int **out_indexes,
    int *out_count,
    SqlError *error
) {
    const BPTreeNode *leaf;
    int *indexes = NULL;
    int count = 0;
    int capacity = 0;
    int first_leaf = 1;

    *out_indexes = NULL;
    *out_count = 0;

    if (tree == NULL || tree->root == NULL || min_key > max_key) {
        return 1;
    }

    leaf = bptree_find_leaf(tree, min_key);
    while (leaf != NULL) {
        int index = first_leaf ? bptree_leaf_lower_bound(leaf, min_key) : 0;

        first_leaf = 0;
        while (index < leaf->key_count) {
            if (leaf->keys[index] > max_key) {
                *out_indexes = indexes;
                *out_count = count;
                return 1;
            }
            if (!bptree_append_index(&indexes, &count, &capacity, leaf->values[index], error)) {
                free(indexes);
                return 0;
            }
            index++;
        }
        leaf = leaf->next;
    }

    *out_indexes = indexes;
    *out_count = count;
    return 1;
}

int bptree_height(const BPTree *tree) {
    const BPTreeNode *node;
    int height = 0;

    if (tree == NULL || tree->root == NULL) {
        return 0;
    }

    node = tree->root;
    while (node != NULL) {
        height++;
        if (node->is_leaf) {
            break;
        }
        node = node->children[0];
    }

    return height;
}

int bptree_node_count(const BPTree *tree) {
    if (tree == NULL || tree->root == NULL) {
        return 0;
    }

    return bptree_node_count_recursive(tree->root);
}
