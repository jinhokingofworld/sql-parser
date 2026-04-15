#include "storage.h"

static int build_table_path(const char *db_root, const char *table_name, char *buffer, size_t size) {
    return snprintf(buffer, size, "%s/tables/%s.csv", db_root, table_name) < (int) size;
}

static int needs_csv_quotes(const char *field) {
    while (*field != '\0') {
        if (*field == ',' || *field == '"' || *field == '\n' || *field == '\r') {
            return 1;
        }
        field++;
    }
    return 0;
}

static int write_csv_field(FILE *file, const char *field) {
    if (!needs_csv_quotes(field)) {
        return fputs(field, file) >= 0;
    }

    if (fputc('"', file) == EOF) {
        return 0;
    }

    while (*field != '\0') {
        if (*field == '"') {
            if (fputc('"', file) == EOF || fputc('"', file) == EOF) {
                return 0;
            }
        } else if (fputc(*field, file) == EOF) {
            return 0;
        }
        field++;
    }

    return fputc('"', file) != EOF;
}

static int ensure_row_boundary(FILE *file) {
    long end_position;

    if (fseek(file, 0L, SEEK_END) != 0) {
        return 0;
    }

    end_position = ftell(file);
    if (end_position < 0) {
        return 0;
    }

    if (end_position == 0) {
        return 1;
    }

    if (fseek(file, -1L, SEEK_END) != 0) {
        return 0;
    }

    if (fgetc(file) != '\n') {
        if (fseek(file, 0L, SEEK_END) != 0) {
            return 0;
        }
        if (fputc('\n', file) == EOF) {
            return 0;
        }
    }

    return fseek(file, 0L, SEEK_END) == 0;
}

static int append_field(char ***fields, int *count, const char *buffer, size_t length, SqlError *error) {
    char **next_fields = realloc(*fields, sizeof(char *) * (size_t) (*count + 1));

    if (next_fields == NULL) {
        sql_set_error(error, 0, 0, "out of memory while reading CSV");
        return 0;
    }

    *fields = next_fields;
    (*fields)[*count] = sql_strndup(buffer, length);
    if ((*fields)[*count] == NULL) {
        sql_set_error(error, 0, 0, "out of memory while reading CSV");
        return 0;
    }

    (*count)++;
    return 1;
}

static int parse_csv_line(const char *line, int expected_fields, Row *row, SqlError *error) {
    size_t cursor = 0;
    char *buffer = NULL;
    size_t buffer_len = 0;
    int in_quotes = 0;

    row->fields = NULL;
    row->field_count = 0;

    while (1) {
        char current = line[cursor];

        if (current == '\0' || (!in_quotes && current == ',')) {
            if (!append_field(&row->fields, &row->field_count, buffer == NULL ? "" : buffer, buffer_len, error)) {
                free(buffer);
                return 0;
            }
            free(buffer);
            buffer = NULL;
            buffer_len = 0;

            if (current == '\0') {
                break;
            }
            cursor++;
            continue;
        }

        if (current == '"') {
            if (in_quotes && line[cursor + 1] == '"') {
                char *next = realloc(buffer, buffer_len + 2);
                if (next == NULL) {
                    free(buffer);
                    sql_set_error(error, 0, 0, "out of memory while reading CSV");
                    return 0;
                }
                buffer = next;
                buffer[buffer_len++] = '"';
                buffer[buffer_len] = '\0';
                cursor += 2;
                continue;
            }
            in_quotes = !in_quotes;
            cursor++;
            continue;
        }

        {
            char *next = realloc(buffer, buffer_len + 2);
            if (next == NULL) {
                free(buffer);
                sql_set_error(error, 0, 0, "out of memory while reading CSV");
                return 0;
            }
            buffer = next;
            buffer[buffer_len++] = current;
            buffer[buffer_len] = '\0';
        }
        cursor++;
    }

    if (row->field_count != expected_fields) {
        sql_set_error(error, 0, 0, "row column count mismatch: expected %d fields, got %d", expected_fields, row->field_count);
        return 0;
    }

    return 1;
}

int rowset_reserve(RowSet *rowset, int min_capacity, SqlError *error) {
    int new_capacity;
    Row *next_rows;

    if (rowset->row_capacity >= min_capacity) {
        return 1;
    }

    new_capacity = rowset->row_capacity > 0 ? rowset->row_capacity : 8;
    while (new_capacity < min_capacity) {
        new_capacity *= 2;
    }

    next_rows = realloc(rowset->rows, sizeof(Row) * (size_t) new_capacity);
    if (next_rows == NULL) {
        sql_set_error(error, 0, 0, "out of memory while growing rowset");
        return 0;
    }

    rowset->rows = next_rows;
    rowset->row_capacity = new_capacity;
    return 1;
}

/* Releases every parsed CSV row and field string owned by a RowSet. */
void free_rowset(RowSet *rowset) {
    int row_index;
    int field_index;

    if (rowset == NULL) {
        return;
    }

    for (row_index = 0; row_index < rowset->row_count; row_index++) {
        for (field_index = 0; field_index < rowset->rows[row_index].field_count; field_index++) {
            free(rowset->rows[row_index].fields[field_index]);
        }
        free(rowset->rows[row_index].fields);
    }

    free(rowset->rows);
    rowset->rows = NULL;
    rowset->row_count = 0;
    rowset->row_capacity = 0;
    rowset->column_count = 0;
}

/* Appends a logical row to the table CSV file using safe CSV escaping rules. */
int append_csv_row(
    const char *db_root,
    const char *table_name,
    char **fields,
    int field_count,
    SqlError *error
) {
    char path[SQL_PATH_BUFFER_SIZE];
    FILE *file;
    int index;

    if (!build_table_path(db_root, table_name, path, sizeof(path))) {
        sql_set_error(error, 0, 0, "table path is too long");
        return 0;
    }

    file = fopen(path, "a+");
    if (file == NULL) {
        sql_set_error(error, 0, 0, "failed to open table `%s`: %s", path, strerror(errno));
        return 0;
    }

    if (!ensure_row_boundary(file)) {
        sql_set_error(error, 0, 0, "failed to prepare row boundary for `%s`", path);
        fclose(file);
        return 0;
    }

    for (index = 0; index < field_count; index++) {
        if (index > 0 && fputc(',', file) == EOF) {
            sql_set_error(error, 0, 0, "failed to write table `%s`", path);
            fclose(file);
            return 0;
        }
        if (!write_csv_field(file, fields[index])) {
            sql_set_error(error, 0, 0, "failed to write CSV field");
            fclose(file);
            return 0;
        }
    }

    if (fputc('\n', file) == EOF) {
        sql_set_error(error, 0, 0, "failed to finalize row");
        fclose(file);
        return 0;
    }

    fclose(file);
    return 1;
}

/* Reads an entire table CSV into memory so filtering and ordering can be applied. */
int read_csv_rows(
    const char *db_root,
    const char *table_name,
    int expected_fields,
    RowSet *rowset,
    SqlError *error
) {
    char path[SQL_PATH_BUFFER_SIZE];
    FILE *file;
    char line[4096];

    rowset->rows = NULL;
    rowset->row_count = 0;
    rowset->row_capacity = 0;
    rowset->column_count = expected_fields;

    if (!build_table_path(db_root, table_name, path, sizeof(path))) {
        sql_set_error(error, 0, 0, "table path is too long");
        return 0;
    }

    file = fopen(path, "r");
    if (file == NULL) {
        sql_set_error(error, 0, 0, "failed to open table `%s`: %s", path, strerror(errno));
        return 0;
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        size_t length = strlen(line);
        Row row;

        while (length > 0 && (line[length - 1] == '\n' || line[length - 1] == '\r')) {
            line[--length] = '\0';
        }

        if (length == 0) {
            continue;
        }

        if (!rowset_reserve(rowset, rowset->row_count + 1, error)) {
            fclose(file);
            free_rowset(rowset);
            return 0;
        }

        if (!parse_csv_line(line, expected_fields, &row, error)) {
            fclose(file);
            free_rowset(rowset);
            return 0;
        }
        rowset->rows[rowset->row_count] = row;
        rowset->row_count++;
    }

    fclose(file);
    return 1;
}
