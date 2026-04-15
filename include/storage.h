#ifndef STORAGE_H
#define STORAGE_H

#include "common.h"

typedef struct {
    char **fields;
    int field_count;
} Row;

typedef struct {
    Row *rows;
    int row_count;
    int column_count;
} RowSet;

int append_csv_row(
    const char *db_root,
    const char *table_name,
    char **fields,
    int field_count,
    SqlError *error
);
int read_csv_rows(
    const char *db_root,
    const char *table_name,
    int expected_fields,
    RowSet *rowset,
    SqlError *error
);
void free_rowset(RowSet *rowset);

#endif
