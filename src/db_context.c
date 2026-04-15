#include "db_context.h"

#include <dirent.h>
#include <limits.h>

static void db_context_free_table(TableState *table) {
    if (table == NULL) {
        return;
    }

    bptree_destroy(table->index);
    free_rowset(&table->rowset);
    free_schema(&table->schema);
    table->index = NULL;
    table->next_id = 0;
    table->name[0] = '\0';
}

static int db_context_parse_int_strict(const char *text, int *value) {
    char *end = NULL;
    long parsed;

    errno = 0;
    parsed = strtol(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || parsed < INT_MIN || parsed > INT_MAX) {
        return 0;
    }

    *value = (int) parsed;
    return 1;
}

static int db_context_has_schema_suffix(const char *name) {
    size_t length = strlen(name);
    const char *suffix = ".schema";
    size_t suffix_length = strlen(suffix);

    return length > suffix_length && strcmp(name + length - suffix_length, suffix) == 0;
}

static int db_context_build_schema_dir_path(const char *db_root, char *buffer, size_t size) {
    return snprintf(buffer, size, "%s/schema", db_root) < (int) size;
}

static int db_context_append_table(DbContext *ctx, const char *table_name, SqlError *error) {
    TableState *next_tables;
    TableState *table;
    int row_index;
    int id_index;
    int max_id = 0;

    next_tables = realloc(ctx->tables, sizeof(TableState) * (size_t) (ctx->table_count + 1));
    if (next_tables == NULL) {
        sql_set_error(error, 0, 0, "out of memory while loading database context");
        return 0;
    }

    ctx->tables = next_tables;
    table = &ctx->tables[ctx->table_count];
    memset(table, 0, sizeof(*table));

    if (snprintf(table->name, sizeof(table->name), "%s", table_name) >= (int) sizeof(table->name)) {
        sql_set_error(error, 0, 0, "table name `%s` is too long", table_name);
        return 0;
    }
    if (!load_schema(ctx->db_root, table_name, &table->schema, error)) {
        return 0;
    }
    if (!read_csv_rows(ctx->db_root, table_name, table->schema.column_count, &table->rowset, error)) {
        free_schema(&table->schema);
        return 0;
    }

    table->index = bptree_create();
    if (table->index == NULL) {
        sql_set_error(error, 0, 0, "out of memory while creating table index");
        db_context_free_table(table);
        return 0;
    }

    id_index = schema_find_column(&table->schema, "id");
    if (table->schema.autoincrement && id_index < 0) {
        sql_set_error(error, 0, 0, "autoincrement table `%s` is missing reserved column `id`", table_name);
        db_context_free_table(table);
        return 0;
    }

    for (row_index = 0; row_index < table->rowset.row_count; row_index++) {
        int id_value = 0;

        if (id_index < 0) {
            break;
        }
        if (!db_context_parse_int_strict(table->rowset.rows[row_index].fields[id_index], &id_value)) {
            sql_set_error(error, 0, 0, "invalid id value `%s` in table `%s`", table->rowset.rows[row_index].fields[id_index], table_name);
            db_context_free_table(table);
            return 0;
        }
        if (!bptree_insert(table->index, id_value, row_index, error)) {
            db_context_free_table(table);
            return 0;
        }
        if (id_value > max_id) {
            max_id = id_value;
        }
    }

    table->next_id = id_index < 0 ? 1 : max_id + 1;
    ctx->table_count++;
    return 1;
}

static int db_context_duplicate_fields(char **fields, int field_count, char ***out_fields, SqlError *error) {
    char **copied_fields = calloc((size_t) field_count, sizeof(char *));
    int index;

    if (copied_fields == NULL) {
        sql_set_error(error, 0, 0, "out of memory while copying row fields");
        return 0;
    }

    for (index = 0; index < field_count; index++) {
        copied_fields[index] = sql_strdup(fields[index]);
        if (copied_fields[index] == NULL) {
            int cleanup_index;

            for (cleanup_index = 0; cleanup_index < index; cleanup_index++) {
                free(copied_fields[cleanup_index]);
            }
            free(copied_fields);
            sql_set_error(error, 0, 0, "out of memory while copying row fields");
            return 0;
        }
    }

    *out_fields = copied_fields;
    return 1;
}

static void db_context_free_field_array(char **fields, int field_count) {
    int index;

    if (fields == NULL) {
        return;
    }

    for (index = 0; index < field_count; index++) {
        free(fields[index]);
    }
    free(fields);
}

static void db_context_rollback_published_row(TableState *table, int row_index) {
    Row *row;

    if (table == NULL || row_index < 0 || row_index >= table->rowset.row_count) {
        return;
    }

    row = &table->rowset.rows[row_index];
    db_context_free_field_array(row->fields, row->field_count);
    row->fields = NULL;
    row->field_count = 0;
    table->rowset.row_count = row_index;
}

static int db_context_find_row_by_id(const TableState *table, int id_value) {
    int id_index = schema_find_column(&table->schema, "id");
    int row_index;

    if (id_index < 0) {
        return -1;
    }

    if (table->index != NULL) {
        int found_row_index = -1;

        if (bptree_search(table->index, id_value, &found_row_index)) {
            return found_row_index;
        }
        return -1;
    }

    for (row_index = 0; row_index < table->rowset.row_count; row_index++) {
        int row_id = 0;

        if (db_context_parse_int_strict(table->rowset.rows[row_index].fields[id_index], &row_id) &&
            row_id == id_value) {
            return row_index;
        }
    }

    return -1;
}

static void db_context_invalidate_index(TableState *table) {
    bptree_destroy(table->index);
    table->index = NULL;
}

DbContext *db_context_create(const char *db_root, SqlError *error) {
    DbContext *ctx = NULL;
    char schema_dir[SQL_PATH_BUFFER_SIZE];
    DIR *dir = NULL;
    struct dirent *entry = NULL;

    ctx = calloc(1, sizeof(DbContext));
    if (ctx == NULL) {
        sql_set_error(error, 0, 0, "out of memory while creating database context");
        return NULL;
    }
    if (snprintf(ctx->db_root, sizeof(ctx->db_root), "%s", db_root) >= (int) sizeof(ctx->db_root)) {
        sql_set_error(error, 0, 0, "database root path is too long");
        free(ctx);
        return NULL;
    }
    if (!db_context_build_schema_dir_path(db_root, schema_dir, sizeof(schema_dir))) {
        sql_set_error(error, 0, 0, "schema path is too long");
        free(ctx);
        return NULL;
    }

    dir = opendir(schema_dir);
    if (dir == NULL) {
        sql_set_error(error, 0, 0, "failed to open schema directory `%s`: %s", schema_dir, strerror(errno));
        free(ctx);
        return NULL;
    }

    while ((entry = readdir(dir)) != NULL) {
        size_t length;
        char table_name[256];

        if (!db_context_has_schema_suffix(entry->d_name)) {
            continue;
        }

        length = strlen(entry->d_name) - strlen(".schema");
        if (length >= sizeof(table_name)) {
            sql_set_error(error, 0, 0, "schema file name `%s` is too long", entry->d_name);
            closedir(dir);
            db_context_destroy(ctx);
            return NULL;
        }

        memcpy(table_name, entry->d_name, length);
        table_name[length] = '\0';
        if (!db_context_append_table(ctx, table_name, error)) {
            closedir(dir);
            db_context_destroy(ctx);
            return NULL;
        }
    }

    closedir(dir);
    return ctx;
}

void db_context_destroy(DbContext *ctx) {
    int index;

    if (ctx == NULL) {
        return;
    }

    for (index = 0; index < ctx->table_count; index++) {
        db_context_free_table(&ctx->tables[index]);
    }
    free(ctx->tables);
    free(ctx);
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

int db_context_insert_row(
    DbContext *ctx,
    const char *table_name,
    char **fields,
    SqlError *error
) {
    TableState *table = db_context_find_table(ctx, table_name);
    Row *next_rows = NULL;
    char **copied_fields = NULL;
    int id_index;
    int id_value = 0;
    int row_index;
    int index_inserted = 0;

    if (table == NULL) {
        sql_set_error(error, 0, 0, "unknown table `%s`", table_name);
        return 0;
    }

    id_index = schema_find_column(&table->schema, "id");
    if (id_index < 0) {
        sql_set_error(error, 0, 0, "table `%s` does not have reserved column `id`", table_name);
        return 0;
    }
    if (!db_context_parse_int_strict(fields[id_index], &id_value)) {
        sql_set_error(error, 0, 0, "invalid id value `%s`", fields[id_index]);
        return 0;
    }
    if (db_context_find_row_by_id(table, id_value) >= 0) {
        sql_set_error(error, 0, 0, "duplicate primary key for column `id`: `%d`", id_value);
        return 0;
    }
    if (!db_context_duplicate_fields(fields, table->schema.column_count, &copied_fields, error)) {
        return 0;
    }

    row_index = table->rowset.row_count;
    if (!rowset_reserve(&table->rowset, row_index + 1, error)) {
        db_context_free_field_array(copied_fields, table->schema.column_count);
        return 0;
    }

    if (table->index != NULL) {
        if (!bptree_insert(table->index, id_value, row_index, error)) {
            error->message[0] = '\0';
            db_context_invalidate_index(table);
        } else {
            index_inserted = 1;
        }
    }

    next_rows = table->rowset.rows;
    next_rows[row_index].fields = copied_fields;
    next_rows[row_index].field_count = table->schema.column_count;
    table->rowset.row_count++;
    table->rowset.column_count = table->schema.column_count;

    if (!append_csv_row(ctx->db_root, table_name, fields, table->schema.column_count, error)) {
        db_context_rollback_published_row(table, row_index);
        if (index_inserted) {
            db_context_invalidate_index(table);
        }
        return 0;
    }
    if (table->schema.autoincrement && table->next_id <= id_value) {
        table->next_id = id_value + 1;
    }

    return 1;
}
