#include "db_context.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#endif

static int parse_int_strict(const char *text, int *value) {
    char *end = NULL;
    long parsed = 0;

    errno = 0;
    parsed = strtol(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || parsed < INT_MIN || parsed > INT_MAX) {
        return 0;
    }

    *value = (int) parsed;
    return 1;
}

static int build_schema_dir_path(const char *db_root, char *buffer, size_t size) {
    return snprintf(buffer, size, "%s/schema", db_root) < (int) size;
}

static int build_table_path(const char *db_root, const char *table_name, char *buffer, size_t size) {
    return snprintf(buffer, size, "%s/tables/%s.csv", db_root, table_name) < (int) size;
}

static int has_schema_suffix(const char *filename) {
    size_t length = strlen(filename);

    return length > 7 && strcmp(filename + length - 7, ".schema") == 0;
}

static int extract_table_name(const char *filename, char *buffer, size_t size) {
    size_t length = strlen(filename);
    size_t table_length;

    if (!has_schema_suffix(filename)) {
        return 0;
    }

    table_length = length - 7;
    if (table_length + 1 > size) {
        return 0;
    }

    memcpy(buffer, filename, table_length);
    buffer[table_length] = '\0';
    return 1;
}

static void free_row_fields(Row *row) {
    int field_index;

    for (field_index = 0; field_index < row->field_count; field_index++) {
        free(row->fields[field_index]);
    }
    free(row->fields);
    row->fields = NULL;
    row->field_count = 0;
}

static void free_table_state(TableState *table) {
    if (table == NULL) {
        return;
    }

    free_schema(&table->schema);
    free_rowset(&table->rowset);
    bptree_destroy(table->index);
    table->index = NULL;
    table->next_id = 1;
    table->id_column_index = -1;
    table->name[0] = '\0';
}

static void free_db_context(DbContext *ctx) {
    int index;

    if (ctx == NULL) {
        return;
    }

    for (index = 0; index < ctx->table_count; index++) {
        free_table_state(&ctx->tables[index]);
    }
    free(ctx->tables);
    free(ctx);
}

static int load_existing_rows(
    const char *db_root,
    const Schema *schema,
    RowSet *rowset,
    SqlError *error
) {
    char table_path[SQL_PATH_BUFFER_SIZE];
    FILE *table_file;

    rowset->rows = NULL;
    rowset->row_count = 0;
    rowset->column_count = schema->column_count;

    if (!build_table_path(db_root, schema->table_name, table_path, sizeof(table_path))) {
        sql_set_error(error, 0, 0, "table path is too long");
        return 0;
    }

    table_file = fopen(table_path, "r");
    if (table_file == NULL) {
        if (errno == ENOENT) {
            return 1;
        }
        sql_set_error(error, 0, 0, "failed to open table `%s`: %s", table_path, strerror(errno));
        return 0;
    }
    fclose(table_file);

    return read_csv_rows(db_root, schema->table_name, schema->column_count, rowset, error);
}

static int build_table_index(TableState *table, SqlError *error) {
    int row_index;
    int max_id = 0;

    table->id_column_index = schema_find_column(&table->schema, "id");
    table->next_id = 1;
    table->index = NULL;

    if (table->id_column_index < 0) {
        return 1;
    }

    table->index = bptree_create();
    if (table->index == NULL) {
        sql_set_error(error, 0, 0, "out of memory while creating B+ tree index");
        return 0;
    }

    for (row_index = 0; row_index < table->rowset.row_count; row_index++) {
        int id_value = 0;

        if (!parse_int_strict(table->rowset.rows[row_index].fields[table->id_column_index], &id_value)) {
            sql_set_error(
                error,
                0,
                0,
                "invalid integer value in id column for table `%s`",
                table->schema.table_name
            );
            return 0;
        }

        if (!bptree_insert(table->index, id_value, row_index, error)) {
            return 0;
        }
        if (id_value > max_id) {
            max_id = id_value;
        }
    }

    table->next_id = max_id + 1;
    return 1;
}

static int append_table_state(
    DbContext *ctx,
    const char *table_name,
    SqlError *error
) {
    TableState *next_tables;
    TableState *table;

    next_tables = realloc(ctx->tables, sizeof(TableState) * (size_t) (ctx->table_count + 1));
    if (next_tables == NULL) {
        sql_set_error(error, 0, 0, "out of memory while building database context");
        return 0;
    }

    ctx->tables = next_tables;
    table = &ctx->tables[ctx->table_count];
    memset(table, 0, sizeof(*table));
    table->id_column_index = -1;
    snprintf(table->name, sizeof(table->name), "%s", table_name);

    if (!load_schema(ctx->db_root, table_name, &table->schema, error)) {
        return 0;
    }
    if (!load_existing_rows(ctx->db_root, &table->schema, &table->rowset, error)) {
        free_table_state(table);
        return 0;
    }
    if (!build_table_index(table, error)) {
        free_table_state(table);
        return 0;
    }

    ctx->table_count++;
    return 1;
}

#ifdef _WIN32
static int scan_schema_directory(DbContext *ctx, SqlError *error) {
    char pattern[SQL_PATH_BUFFER_SIZE];
    WIN32_FIND_DATAA find_data;
    HANDLE handle;
    int found_any = 0;

    if (!build_schema_dir_path(ctx->db_root, pattern, sizeof(pattern))) {
        sql_set_error(error, 0, 0, "schema directory path is too long");
        return 0;
    }
    if (snprintf(pattern + strlen(pattern), sizeof(pattern) - strlen(pattern), "\\*.schema") >=
        (int) (sizeof(pattern) - strlen(pattern))) {
        sql_set_error(error, 0, 0, "schema search pattern is too long");
        return 0;
    }

    handle = FindFirstFileA(pattern, &find_data);
    if (handle == INVALID_HANDLE_VALUE) {
        if (GetLastError() == ERROR_FILE_NOT_FOUND) {
            sql_set_error(error, 0, 0, "no schema files found under `%s/schema`", ctx->db_root);
            return 0;
        }
        sql_set_error(error, 0, 0, "failed to scan schema directory");
        return 0;
    }

    do {
        char table_name[256];

        if ((find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            continue;
        }
        if (!extract_table_name(find_data.cFileName, table_name, sizeof(table_name))) {
            continue;
        }
        if (!append_table_state(ctx, table_name, error)) {
            FindClose(handle);
            return 0;
        }
        found_any = 1;
    } while (FindNextFileA(handle, &find_data) != 0);

    FindClose(handle);

    if (!found_any) {
        sql_set_error(error, 0, 0, "no schema files found under `%s/schema`", ctx->db_root);
        return 0;
    }

    return 1;
}
#else
static int scan_schema_directory(DbContext *ctx, SqlError *error) {
    char schema_dir[SQL_PATH_BUFFER_SIZE];
    DIR *dir;
    struct dirent *entry;
    int found_any = 0;

    if (!build_schema_dir_path(ctx->db_root, schema_dir, sizeof(schema_dir))) {
        sql_set_error(error, 0, 0, "schema directory path is too long");
        return 0;
    }

    dir = opendir(schema_dir);
    if (dir == NULL) {
        sql_set_error(error, 0, 0, "failed to open schema directory `%s`: %s", schema_dir, strerror(errno));
        return 0;
    }

    while ((entry = readdir(dir)) != NULL) {
        char table_name[256];

        if (!extract_table_name(entry->d_name, table_name, sizeof(table_name))) {
            continue;
        }
        if (!append_table_state(ctx, table_name, error)) {
            closedir(dir);
            return 0;
        }
        found_any = 1;
    }

    closedir(dir);

    if (!found_any) {
        sql_set_error(error, 0, 0, "no schema files found under `%s/schema`", ctx->db_root);
        return 0;
    }

    return 1;
}
#endif

DbContext *db_context_create(const char *db_root, SqlError *error) {
    DbContext *ctx = calloc(1, sizeof(DbContext));

    if (ctx == NULL) {
        sql_set_error(error, 0, 0, "out of memory while creating database context");
        return NULL;
    }

    if (snprintf(ctx->db_root, sizeof(ctx->db_root), "%s", db_root) >= (int) sizeof(ctx->db_root)) {
        sql_set_error(error, 0, 0, "database root path is too long");
        free(ctx);
        return NULL;
    }

    if (!scan_schema_directory(ctx, error)) {
        free_db_context(ctx);
        return NULL;
    }

    return ctx;
}

void db_context_destroy(DbContext *ctx) {
    free_db_context(ctx);
}

TableState *db_context_find_table(DbContext *ctx, const char *table_name) {
    int index;

    if (ctx == NULL) {
        return NULL;
    }

    for (index = 0; index < ctx->table_count; index++) {
        if (sql_stricmp(ctx->tables[index].name, table_name) == 0) {
            return &ctx->tables[index];
        }
    }

    return NULL;
}

static int ensure_unique_primary_key(TableState *table, char **fields, SqlError *error) {
    int row_index;

    if (table->schema.primary_key_index < 0) {
        return 1;
    }

    if (table->index != NULL && table->schema.primary_key_index == table->id_column_index) {
        int key = 0;
        int existing_row_index = -1;

        if (!parse_int_strict(fields[table->id_column_index], &key)) {
            sql_set_error(error, 0, 0, "primary key column `id` expects int");
            return 0;
        }

        if (bptree_search(table->index, key, &existing_row_index)) {
            sql_set_error(
                error,
                0,
                0,
                "duplicate primary key for column `%s`: `%s`",
                table->schema.primary_key,
                fields[table->schema.primary_key_index]
            );
            return 0;
        }

        return 1;
    }

    for (row_index = 0; row_index < table->rowset.row_count; row_index++) {
        if (strcmp(
                table->rowset.rows[row_index].fields[table->schema.primary_key_index],
                fields[table->schema.primary_key_index]
            ) == 0) {
            sql_set_error(
                error,
                0,
                0,
                "duplicate primary key for column `%s`: `%s`",
                table->schema.primary_key,
                fields[table->schema.primary_key_index]
            );
            return 0;
        }
    }

    return 1;
}

int db_context_insert_row(
    DbContext *ctx,
    const char *table_name,
    char **fields,
    SqlError *error
) {
    TableState *table = db_context_find_table(ctx, table_name);
    RowSet *rowset;
    Row new_row;
    Row *next_rows;
    int field_index;
    int row_index;

    if (table == NULL) {
        sql_set_error(error, 0, 0, "unknown table `%s`", table_name);
        return 0;
    }

    if (!ensure_unique_primary_key(table, fields, error)) {
        return 0;
    }

    new_row.fields = calloc((size_t) table->schema.column_count, sizeof(char *));
    new_row.field_count = table->schema.column_count;
    if (new_row.fields == NULL) {
        sql_set_error(error, 0, 0, "out of memory while appending row");
        return 0;
    }

    for (field_index = 0; field_index < table->schema.column_count; field_index++) {
        new_row.fields[field_index] = sql_strdup(fields[field_index]);
        if (new_row.fields[field_index] == NULL) {
            sql_set_error(error, 0, 0, "out of memory while appending row");
            free_row_fields(&new_row);
            return 0;
        }
    }

    rowset = &table->rowset;
    next_rows = realloc(rowset->rows, sizeof(Row) * (size_t) (rowset->row_count + 1));
    if (next_rows == NULL) {
        sql_set_error(error, 0, 0, "out of memory while growing rowset");
        free_row_fields(&new_row);
        return 0;
    }
    rowset->rows = next_rows;

    if (!append_csv_row(ctx->db_root, table->schema.table_name, fields, table->schema.column_count, error)) {
        free_row_fields(&new_row);
        return 0;
    }

    row_index = rowset->row_count;
    rowset->rows[row_index] = new_row;
    rowset->row_count++;

    if (table->index != NULL && table->id_column_index >= 0) {
        int id_value = 0;

        if (!parse_int_strict(fields[table->id_column_index], &id_value)) {
            sql_set_error(error, 0, 0, "primary key column `id` expects int");
            return 0;
        }
        if (!bptree_insert(table->index, id_value, row_index, error)) {
            return 0;
        }
        if (id_value >= table->next_id) {
            table->next_id = id_value + 1;
        }
    }

    return 1;
}
