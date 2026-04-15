#ifndef DB_CONTEXT_H
#define DB_CONTEXT_H

#include "bptree.h"
#include "schema.h"
#include "storage.h"

typedef struct {
    char name[256];
    Schema schema;
    RowSet rowset;
    BPTree *index;
    int next_id;
} TableState;

typedef struct {
    char db_root[SQL_PATH_BUFFER_SIZE];
    TableState *tables;
    int table_count;
} DbContext;

DbContext *db_context_create(const char *db_root, SqlError *error);
void db_context_destroy(DbContext *ctx);
TableState *db_context_find_table(DbContext *ctx, const char *table_name);
int db_context_insert_row(
    DbContext *ctx,
    const char *table_name,
    char **fields,
    SqlError *error
);

#endif
