#include "bptree.h"

struct BPTreeNode {
    int is_leaf;
    int key_count;
    int keys[BPTREE_ORDER - 1];
    struct BPTreeNode *children[BPTREE_ORDER];
    int values[BPTREE_ORDER - 1];
    struct BPTreeNode *next;
    struct BPTreeNode *prev;
};

static BPTreeNode *bptree_create_node(int is_leaf) {
    BPTreeNode *node = calloc(1, sizeof(BPTreeNode));

    if (node == NULL) {
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

static int find_leaf_insert_index(const BPTreeNode *node, int key, int *exists) {
    int low = 0;
    int high = node->key_count;

    *exists = 0;
    while (low < high) {
        int mid = low + (high - low) / 2;

        if (node->keys[mid] == key) {
            *exists = 1;
            return mid;
        }
        if (node->keys[mid] < key) {
            low = mid + 1;
        } else {
            high = mid;
        }
    }

    return low;
}

static int find_child_index(const BPTreeNode *node, int key) {
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

static BPTreeNode *find_leaf_node(const BPTree *tree, int key) {
    BPTreeNode *node;

    if (tree == NULL || tree->root == NULL) {
        return NULL;
    }

    node = tree->root;
    while (!node->is_leaf) {
        node = node->children[find_child_index(node, key)];
    }

    return node;
}

static int insert_into_leaf(
    BPTreeNode *leaf,
    int key,
    int row_index,
    int *promoted_key,
    BPTreeNode **split_right,
    SqlError *error
) {
    int exists = 0;
    int insert_index = find_leaf_insert_index(leaf, key, &exists);
    int max_keys = BPTREE_ORDER - 1;

    *split_right = NULL;
    *promoted_key = 0;

    if (exists) {
        sql_set_error(error, 0, 0, "duplicate primary key for column `id`: `%d`", key);
        return 0;
    }

    if (leaf->key_count < max_keys) {
        int move_count = leaf->key_count - insert_index;

        if (move_count > 0) {
            memmove(&leaf->keys[insert_index + 1], &leaf->keys[insert_index], sizeof(int) * (size_t) move_count);
            memmove(&leaf->values[insert_index + 1], &leaf->values[insert_index], sizeof(int) * (size_t) move_count);
        }
        leaf->keys[insert_index] = key;
        leaf->values[insert_index] = row_index;
        leaf->key_count++;
        return 1;
    }

    {
        int temp_keys[BPTREE_ORDER];
        int temp_values[BPTREE_ORDER];
        int total = leaf->key_count + 1;
        int left_count = total / 2;
        int right_count = total - left_count;
        int read_index = 0;
        int write_index = 0;
        BPTreeNode *right = bptree_create_node(1);

        if (right == NULL) {
            sql_set_error(error, 0, 0, "out of memory while splitting B+ tree leaf");
            return 0;
        }

        for (write_index = 0; write_index < total; write_index++) {
            if (write_index == insert_index) {
                temp_keys[write_index] = key;
                temp_values[write_index] = row_index;
            } else {
                temp_keys[write_index] = leaf->keys[read_index];
                temp_values[write_index] = leaf->values[read_index];
                read_index++;
            }
        }

        leaf->key_count = left_count;
        for (write_index = 0; write_index < left_count; write_index++) {
            leaf->keys[write_index] = temp_keys[write_index];
            leaf->values[write_index] = temp_values[write_index];
        }

        right->key_count = right_count;
        for (write_index = 0; write_index < right_count; write_index++) {
            right->keys[write_index] = temp_keys[left_count + write_index];
            right->values[write_index] = temp_values[left_count + write_index];
        }

        right->next = leaf->next;
        if (right->next != NULL) {
            right->next->prev = right;
        }
        right->prev = leaf;
        leaf->next = right;

        *promoted_key = right->keys[0];
        *split_right = right;
        return 1;
    }
}

static int insert_into_internal(
    BPTreeNode *node,
    int insert_index,
    int key,
    BPTreeNode *right_child,
    int *promoted_key,
    BPTreeNode **split_right,
    SqlError *error
) {
    int max_keys = BPTREE_ORDER - 1;

    *promoted_key = 0;
    *split_right = NULL;

    if (node->key_count < max_keys) {
        int key_move_count = node->key_count - insert_index;
        int child_move_count = node->key_count - insert_index;

        if (key_move_count > 0) {
            memmove(&node->keys[insert_index + 1], &node->keys[insert_index], sizeof(int) * (size_t) key_move_count);
        }
        if (child_move_count > 0) {
            memmove(
                &node->children[insert_index + 2],
                &node->children[insert_index + 1],
                sizeof(BPTreeNode *) * (size_t) child_move_count
            );
        }

        node->keys[insert_index] = key;
        node->children[insert_index + 1] = right_child;
        node->key_count++;
        return 1;
    }

    {
        int temp_keys[BPTREE_ORDER];
        BPTreeNode *temp_children[BPTREE_ORDER + 1];
        int total_keys = node->key_count + 1;
        int left_key_count;
        int right_key_count;
        int mid;
        int read_key_index = 0;
        int read_child_index = 0;
        int write_index = 0;
        BPTreeNode *right = bptree_create_node(0);

        if (right == NULL) {
            sql_set_error(error, 0, 0, "out of memory while splitting B+ tree internal node");
            return 0;
        }

        for (write_index = 0; write_index < total_keys; write_index++) {
            if (write_index == insert_index) {
                temp_keys[write_index] = key;
            } else {
                temp_keys[write_index] = node->keys[read_key_index++];
            }
        }

        for (write_index = 0; write_index < total_keys + 1; write_index++) {
            if (write_index == insert_index + 1) {
                temp_children[write_index] = right_child;
            } else {
                temp_children[write_index] = node->children[read_child_index++];
            }
        }

        mid = total_keys / 2;
        left_key_count = mid;
        right_key_count = total_keys - mid - 1;

        node->key_count = left_key_count;
        for (write_index = 0; write_index < left_key_count; write_index++) {
            node->keys[write_index] = temp_keys[write_index];
        }
        for (write_index = 0; write_index <= left_key_count; write_index++) {
            node->children[write_index] = temp_children[write_index];
        }
        for (write_index = left_key_count + 1; write_index < BPTREE_ORDER; write_index++) {
            node->children[write_index] = NULL;
        }

        right->key_count = right_key_count;
        for (write_index = 0; write_index < right_key_count; write_index++) {
            right->keys[write_index] = temp_keys[mid + 1 + write_index];
        }
        for (write_index = 0; write_index <= right_key_count; write_index++) {
            right->children[write_index] = temp_children[mid + 1 + write_index];
        }

        *promoted_key = temp_keys[mid];
        *split_right = right;
        return 1;
    }
}

static int insert_recursive(
    BPTreeNode *node,
    int key,
    int row_index,
    int *promoted_key,
    BPTreeNode **split_right,
    SqlError *error
) {
    *promoted_key = 0;
    *split_right = NULL;

    if (node->is_leaf) {
        return insert_into_leaf(node, key, row_index, promoted_key, split_right, error);
    }

    {
        int child_index = find_child_index(node, key);
        int child_promoted_key = 0;
        BPTreeNode *child_split_right = NULL;

        if (!insert_recursive(
                node->children[child_index],
                key,
                row_index,
                &child_promoted_key,
                &child_split_right,
                error
            )) {
            return 0;
        }

        if (child_split_right == NULL) {
            return 1;
        }

        return insert_into_internal(
            node,
            child_index,
            child_promoted_key,
            child_split_right,
            promoted_key,
            split_right,
            error
        );
    }
}

static int node_height(const BPTreeNode *node) {
    if (node == NULL) {
        return 0;
    }
    if (node->is_leaf) {
        return 1;
    }

    return 1 + node_height(node->children[0]);
}

static int node_count_recursive(const BPTreeNode *node) {
    int count = 1;
    int index;

    if (node == NULL) {
        return 0;
    }
    if (node->is_leaf) {
        return 1;
    }

    for (index = 0; index <= node->key_count; index++) {
        count += node_count_recursive(node->children[index]);
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
    int promoted_key = 0;
    BPTreeNode *split_right = NULL;

    if (tree == NULL) {
        sql_set_error(error, 0, 0, "B+ tree is not initialized");
        return 0;
    }

    if (tree->root == NULL) {
        tree->root = bptree_create_node(1);
        if (tree->root == NULL) {
            sql_set_error(error, 0, 0, "out of memory while creating B+ tree root");
            return 0;
        }
    }

    if (!insert_recursive(tree->root, key, row_index, &promoted_key, &split_right, error)) {
        return 0;
    }

    if (split_right != NULL) {
        BPTreeNode *new_root = bptree_create_node(0);

        if (new_root == NULL) {
            sql_set_error(error, 0, 0, "out of memory while growing B+ tree root");
            return 0;
        }

        new_root->keys[0] = promoted_key;
        new_root->children[0] = tree->root;
        new_root->children[1] = split_right;
        new_root->key_count = 1;
        tree->root = new_root;
    }

    return 1;
}

int bptree_search(const BPTree *tree, int key, int *row_index) {
    BPTreeNode *leaf = find_leaf_node(tree, key);
    int exists = 0;
    int index;

    if (row_index != NULL) {
        *row_index = -1;
    }
    if (leaf == NULL) {
        return 0;
    }

    index = find_leaf_insert_index(leaf, key, &exists);
    if (!exists) {
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
    BPTreeNode *leaf;
    int *indexes = NULL;
    int count = 0;

    (void) error;

    *out_indexes = NULL;
    *out_count = 0;

    if (tree == NULL || tree->root == NULL || min_key > max_key) {
        return 1;
    }

    leaf = find_leaf_node(tree, min_key);
    while (leaf != NULL) {
        int index;

        for (index = 0; index < leaf->key_count; index++) {
            int key = leaf->keys[index];
            int *next_indexes;

            if (key < min_key) {
                continue;
            }
            if (key > max_key) {
                *out_indexes = indexes;
                *out_count = count;
                return 1;
            }

            next_indexes = realloc(indexes, sizeof(int) * (size_t) (count + 1));
            if (next_indexes == NULL) {
                free(indexes);
                sql_set_error(error, 0, 0, "out of memory while collecting B+ tree range search");
                return 0;
            }

            indexes = next_indexes;
            indexes[count++] = leaf->values[index];
        }

        leaf = leaf->next;
    }

    *out_indexes = indexes;
    *out_count = count;
    return 1;
}

int bptree_height(const BPTree *tree) {
    if (tree == NULL) {
        return 0;
    }

    return node_height(tree->root);
}

int bptree_node_count(const BPTree *tree) {
    if (tree == NULL) {
        return 0;
    }

    return node_count_recursive(tree->root);
}
