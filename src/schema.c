#include "schema.h"

static int build_schema_path(const char *db_root, const char *table_name, char *buffer, size_t size) {
    return snprintf(buffer, size, "%s/schema/%s.schema", db_root, table_name) < (int) size;
}

static ColumnType parse_column_type(const char *text, int *ok) {
    if (sql_stricmp(text, "int") == 0) {
        *ok = 1;
        return COLUMN_TYPE_INT;
    }
    if (sql_stricmp(text, "string") == 0) {
        *ok = 1;
        return COLUMN_TYPE_STRING;
    }

    *ok = 0;
    return COLUMN_TYPE_STRING;
}

static void trim_trailing_newline(char *text) {
    size_t len = strlen(text);

    while (len > 0 && (text[len - 1] == '\n' || text[len - 1] == '\r')) {
        text[--len] = '\0';
    }
}

static int parse_columns_line(Schema *schema, char *line, SqlError *error) {
    char *cursor = line;

    while (*cursor != '\0') {
        char *comma = strchr(cursor, ',');
        char *separator;
        ColumnDef *next_columns;
        int type_ok = 0;

        if (comma != NULL) {
            *comma = '\0';
        }

        separator = strchr(cursor, ':');
        if (separator == NULL) {
            sql_set_error(error, 0, 0, "invalid schema column definition `%s`", cursor);
            return 0;
        }

        *separator = '\0';
        next_columns = realloc(schema->columns, sizeof(ColumnDef) * (size_t) (schema->column_count + 1));
        if (next_columns == NULL) {
            sql_set_error(error, 0, 0, "out of memory while loading schema");
            return 0;
        }

        schema->columns = next_columns;
        schema->columns[schema->column_count].name = sql_strdup(cursor);
        schema->columns[schema->column_count].type = parse_column_type(separator + 1, &type_ok);
        if (schema->columns[schema->column_count].name == NULL) {
            sql_set_error(error, 0, 0, "out of memory while loading schema");
            return 0;
        }
        if (!type_ok) {
            sql_set_error(error, 0, 0, "unsupported column type `%s`", separator + 1);
            return 0;
        }

        schema->column_count++;
        if (comma == NULL) {
            break;
        }
        cursor = comma + 1;
    }

    return 1;
}

/* Releases schema metadata loaded from the on-disk `.schema` file. */
void free_schema(Schema *schema) {
    int index;

    if (schema == NULL) {
        return;
    }

    free(schema->table_name);
    for (index = 0; index < schema->column_count; index++) {
        free(schema->columns[index].name);
    }
    free(schema->columns);

    schema->table_name = NULL;
    schema->columns = NULL;
    schema->column_count = 0;
}

/* Finds a column index by name so executor logic can stay schema-driven. */
int schema_find_column(const Schema *schema, const char *name) {
    int index;

    for (index = 0; index < schema->column_count; index++) {
        if (sql_stricmp(schema->columns[index].name, name) == 0) {
            return index;
        }
    }

    return -1;
}

/* Loads table metadata from `schema/<table>.schema` for validation and ordering. */
int load_schema(const char *db_root, const char *table_name, Schema *schema, SqlError *error) {
    char path[SQL_PATH_BUFFER_SIZE];
    FILE *file;
    char line[1024];

    memset(schema, 0, sizeof(*schema));

    if (!build_schema_path(db_root, table_name, path, sizeof(path))) {
        sql_set_error(error, 0, 0, "schema path is too long");
        return 0;
    }

    file = fopen(path, "r");
    if (file == NULL) {
        sql_set_error(error, 0, 0, "failed to open schema `%s`: %s", path, strerror(errno));
        return 0;
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        trim_trailing_newline(line);
        if (strncmp(line, "table=", 6) == 0) {
            free(schema->table_name);
            schema->table_name = sql_strdup(line + 6);
            if (schema->table_name == NULL) {
                sql_set_error(error, 0, 0, "out of memory while loading schema");
                fclose(file);
                free_schema(schema);
                return 0;
            }
        } else if (strncmp(line, "columns=", 8) == 0) {
            if (!parse_columns_line(schema, line + 8, error)) {
                fclose(file);
                free_schema(schema);
                return 0;
            }
        }
    }

    fclose(file);

    if (schema->table_name == NULL || schema->column_count == 0) {
        sql_set_error(error, 0, 0, "invalid schema `%s`", path);
        free_schema(schema);
        return 0;
    }

    if (sql_stricmp(schema->table_name, table_name) != 0) {
        sql_set_error(error, 0, 0, "schema/table mismatch for `%s`", table_name);
        free_schema(schema);
        return 0;
    }

    return 1;
}
