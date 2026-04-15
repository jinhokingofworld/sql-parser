#ifndef SCHEMA_H
#define SCHEMA_H

#include "common.h"

typedef enum {
    COLUMN_TYPE_INT,
    COLUMN_TYPE_FLOAT,
    COLUMN_TYPE_STRING
} ColumnType;

typedef struct {
    char *name;
    ColumnType type;
} ColumnDef;

typedef struct {
    char *table_name;
    ColumnDef *columns;
    int column_count;
    char *primary_key;
    int primary_key_index;
    int autoincrement;
} Schema;

int load_schema(const char *db_root, const char *table_name, Schema *schema, SqlError *error);
void free_schema(Schema *schema);
int schema_find_column(const Schema *schema, const char *name);

#endif
