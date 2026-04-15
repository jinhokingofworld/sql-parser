#include "executor.h"
#include "schema.h"
#include "storage.h"

typedef struct {
    Row **rows;
    int count;
} MatchedRows;

static int g_sort_column_index = 0;
static ColumnType g_sort_column_type = COLUMN_TYPE_STRING;
static int g_sort_ascending = 1;

static int parse_int_strict(const char *text, long *value) {
    char *end = NULL;
    long parsed;

    errno = 0;
    parsed = strtol(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0') {
        return 0;
    }

    *value = parsed;
    return 1;
}

static int parse_float_strict(const char *text, double *value) {
    char *end = NULL;
    double parsed;

    errno = 0;
    parsed = strtod(text, &end);
    if (errno != 0 || end == text || *end != '\0') {
        return 0;
    }

    *value = parsed;
    return 1;
}

static const char *column_type_label(ColumnType type) {
    switch (type) {
        case COLUMN_TYPE_INT:
            return "int";
        case COLUMN_TYPE_FLOAT:
            return "float";
        case COLUMN_TYPE_STRING:
            return "string";
    }

    return "unknown";
}

static int is_value_compatible(ColumnType type, const Value *value) {
    long parsed = 0;
    double parsed_float = 0.0;

    if (type == COLUMN_TYPE_INT) {
        return value->type == VALUE_INT && parse_int_strict(value->raw, &parsed);
    }
    if (type == COLUMN_TYPE_FLOAT) {
        return (value->type == VALUE_INT || value->type == VALUE_FLOAT) &&
            parse_float_strict(value->raw, &parsed_float);
    }

    return value->type == VALUE_STRING;
}

static int compare_field_values(const char *left, const char *right, ColumnType type) {
    if (type == COLUMN_TYPE_INT) {
        long left_value = 0;
        long right_value = 0;

        parse_int_strict(left, &left_value);
        parse_int_strict(right, &right_value);
        if (left_value < right_value) {
            return -1;
        }
        if (left_value > right_value) {
            return 1;
        }
        return 0;
    }
    if (type == COLUMN_TYPE_FLOAT) {
        double left_value = 0.0;
        double right_value = 0.0;

        parse_float_strict(left, &left_value);
        parse_float_strict(right, &right_value);
        if (left_value < right_value) {
            return -1;
        }
        if (left_value > right_value) {
            return 1;
        }
        return 0;
    }

    return strcmp(left, right);
}

static int row_matches_condition(const Row *row, const Schema *schema, const Condition *condition, SqlError *error) {
    int column_index = schema_find_column(schema, condition->column);
    ColumnType type;

    if (column_index < 0) {
        sql_set_error(error, 0, 0, "unknown column `%s`", condition->column);
        return 0;
    }

    type = schema->columns[column_index].type;
    if (!is_value_compatible(type, &condition->value)) {
        sql_set_error(
            error,
            0,
            0,
            "type mismatch: column `%s` expects %s",
            condition->column,
            column_type_label(type)
        );
        return 0;
    }

    return compare_field_values(row->fields[column_index], condition->value.raw, type) == 0;
}

static int qsort_row_compare(const void *left, const void *right) {
    const Row *lhs = *(const Row *const *) left;
    const Row *rhs = *(const Row *const *) right;
    int cmp = compare_field_values(
        lhs->fields[g_sort_column_index],
        rhs->fields[g_sort_column_index],
        g_sort_column_type
    );

    return g_sort_ascending ? cmp : -cmp;
}

static void free_matched_rows(MatchedRows *matches) {
    free(matches->rows);
    matches->rows = NULL;
    matches->count = 0;
}

static int collect_matches(
    const SelectQuery *query,
    const Schema *schema,
    const RowSet *rowset,
    MatchedRows *matches,
    SqlError *error
) {
    int row_index;

    matches->rows = NULL;
    matches->count = 0;

    for (row_index = 0; row_index < rowset->row_count; row_index++) {
        int is_match = 1;
        Row **next_rows;

        if (query->has_where) {
            is_match = row_matches_condition(&rowset->rows[row_index], schema, &query->where, error);
            if (!is_match && error->message[0] != '\0') {
                free_matched_rows(matches);
                return 0;
            }
        }

        if (!is_match) {
            continue;
        }

        next_rows = realloc(matches->rows, sizeof(Row *) * (size_t) (matches->count + 1));
        if (next_rows == NULL) {
            sql_set_error(error, 0, 0, "out of memory while filtering rows");
            free_matched_rows(matches);
            return 0;
        }

        matches->rows = next_rows;
        matches->rows[matches->count] = &rowset->rows[row_index];
        matches->count++;
    }

    return 1;
}

static int prepare_projection(
    const SelectQuery *query,
    const Schema *schema,
    int **selected_indexes,
    int *selected_count,
    SqlError *error
) {
    int index;

    if (query->select_all) {
        *selected_indexes = malloc(sizeof(int) * (size_t) schema->column_count);
        if (*selected_indexes == NULL) {
            sql_set_error(error, 0, 0, "out of memory while preparing projection");
            return 0;
        }
        for (index = 0; index < schema->column_count; index++) {
            (*selected_indexes)[index] = index;
        }
        *selected_count = schema->column_count;
        return 1;
    }

    *selected_indexes = malloc(sizeof(int) * (size_t) query->column_count);
    if (*selected_indexes == NULL) {
        sql_set_error(error, 0, 0, "out of memory while preparing projection");
        return 0;
    }

    for (index = 0; index < query->column_count; index++) {
        (*selected_indexes)[index] = schema_find_column(schema, query->columns[index]);
        if ((*selected_indexes)[index] < 0) {
            sql_set_error(error, 0, 0, "unknown column `%s`", query->columns[index]);
            free(*selected_indexes);
            *selected_indexes = NULL;
            return 0;
        }
    }

    *selected_count = query->column_count;
    return 1;
}

static void print_border(FILE *out, const size_t *widths, int count) {
    int index;

    fputc('+', out);
    for (index = 0; index < count; index++) {
        size_t width_index;

        for (width_index = 0; width_index < widths[index] + 2U; width_index++) {
            fputc('-', out);
        }
        fputc('+', out);
    }
    fputc('\n', out);
}

static void print_row(FILE *out, char **values, const size_t *widths, int count) {
    int index;

    fputc('|', out);
    for (index = 0; index < count; index++) {
        fprintf(out, " %-*s |", (int) widths[index], values[index]);
    }
    fputc('\n', out);
}

static int ensure_primary_key_unique(
    const Schema *schema,
    const char *db_root,
    char **ordered_fields,
    SqlError *error
) {
    RowSet rowset;
    int row_index;
    int ok = 0;

    if (schema->primary_key_index < 0) {
        return 1;
    }

    if (!read_csv_rows(db_root, schema->table_name, schema->column_count, &rowset, error)) {
        return 0;
    }

    for (row_index = 0; row_index < rowset.row_count; row_index++) {
        if (strcmp(
                rowset.rows[row_index].fields[schema->primary_key_index],
                ordered_fields[schema->primary_key_index]
            ) == 0) {
            sql_set_error(
                error,
                0,
                0,
                "duplicate primary key for column `%s`: `%s`",
                schema->primary_key,
                ordered_fields[schema->primary_key_index]
            );
            free_rowset(&rowset);
            return 0;
        }
    }

    ok = 1;
    free_rowset(&rowset);
    return ok;
}

static int execute_insert(const InsertQuery *query, const char *db_root, FILE *out, SqlError *error) {
    Schema schema;
    char **ordered_fields = NULL;
    int *seen = NULL;
    int index;
    int ok = 0;

    if (!load_schema(db_root, query->table_name, &schema, error)) {
        return 0;
    }

    if (query->column_count != schema.column_count) {
        sql_set_error(error, 0, 0, "INSERT must provide exactly %d columns", schema.column_count);
        free_schema(&schema);
        return 0;
    }

    ordered_fields = calloc((size_t) schema.column_count, sizeof(char *));
    seen = calloc((size_t) schema.column_count, sizeof(int));
    if (ordered_fields == NULL || seen == NULL) {
        sql_set_error(error, 0, 0, "out of memory while executing INSERT");
        goto cleanup;
    }

    for (index = 0; index < query->column_count; index++) {
        int schema_index = schema_find_column(&schema, query->columns[index]);

        if (schema_index < 0) {
            sql_set_error(error, 0, 0, "unknown column `%s`", query->columns[index]);
            goto cleanup;
        }
        if (seen[schema_index]) {
            sql_set_error(error, 0, 0, "duplicate column `%s`", query->columns[index]);
            goto cleanup;
        }
        if (!is_value_compatible(schema.columns[schema_index].type, &query->values[index])) {
            sql_set_error(
                error,
                0,
                0,
                "type mismatch: column `%s` expects %s",
                query->columns[index],
                column_type_label(schema.columns[schema_index].type)
            );
            goto cleanup;
        }

        ordered_fields[schema_index] = sql_strdup(query->values[index].raw);
        if (ordered_fields[schema_index] == NULL) {
            sql_set_error(error, 0, 0, "out of memory while executing INSERT");
            goto cleanup;
        }
        seen[schema_index] = 1;
    }

    for (index = 0; index < schema.column_count; index++) {
        if (!seen[index]) {
            sql_set_error(error, 0, 0, "missing value for column `%s`", schema.columns[index].name);
            goto cleanup;
        }
    }

    if (!ensure_primary_key_unique(&schema, db_root, ordered_fields, error)) {
        goto cleanup;
    }

    if (!append_csv_row(db_root, query->table_name, ordered_fields, schema.column_count, error)) {
        goto cleanup;
    }

    fprintf(out, "INSERT 1\n");
    ok = 1;

cleanup:
    if (ordered_fields != NULL) {
        for (index = 0; index < schema.column_count; index++) {
            free(ordered_fields[index]);
        }
    }
    free(ordered_fields);
    free(seen);
    free_schema(&schema);
    return ok;
}

static int execute_select(const SelectQuery *query, const char *db_root, FILE *out, SqlError *error) {
    Schema schema;
    RowSet rowset;
    MatchedRows matches;
    int *selected_indexes = NULL;
    int selected_count = 0;
    size_t *widths = NULL;
    char **header_values = NULL;
    int index;
    int ok = 0;

    if (!load_schema(db_root, query->table_name, &schema, error)) {
        return 0;
    }
    if (!read_csv_rows(db_root, query->table_name, schema.column_count, &rowset, error)) {
        free_schema(&schema);
        return 0;
    }
    if (!prepare_projection(query, &schema, &selected_indexes, &selected_count, error)) {
        free_rowset(&rowset);
        free_schema(&schema);
        return 0;
    }
    if (!collect_matches(query, &schema, &rowset, &matches, error)) {
        free(selected_indexes);
        free_rowset(&rowset);
        free_schema(&schema);
        return 0;
    }

    if (query->has_order_by) {
        int order_index = schema_find_column(&schema, query->order_by.column);
        if (order_index < 0) {
            sql_set_error(error, 0, 0, "unknown column `%s`", query->order_by.column);
            goto cleanup;
        }
        g_sort_column_index = order_index;
        g_sort_column_type = schema.columns[order_index].type;
        g_sort_ascending = query->order_by.ascending;
        qsort(matches.rows, (size_t) matches.count, sizeof(Row *), qsort_row_compare);
    }

    widths = malloc(sizeof(size_t) * (size_t) selected_count);
    header_values = malloc(sizeof(char *) * (size_t) selected_count);
    if (widths == NULL || header_values == NULL) {
        sql_set_error(error, 0, 0, "out of memory while formatting SELECT output");
        goto cleanup;
    }

    for (index = 0; index < selected_count; index++) {
        header_values[index] = schema.columns[selected_indexes[index]].name;
        widths[index] = strlen(header_values[index]);
    }

    for (index = 0; index < matches.count; index++) {
        int column_index;

        for (column_index = 0; column_index < selected_count; column_index++) {
            size_t cell_len = strlen(matches.rows[index]->fields[selected_indexes[column_index]]);
            if (cell_len > widths[column_index]) {
                widths[column_index] = cell_len;
            }
        }
    }

    print_border(out, widths, selected_count);
    print_row(out, header_values, widths, selected_count);
    print_border(out, widths, selected_count);
    for (index = 0; index < matches.count; index++) {
        int column_index;
        char **projected = malloc(sizeof(char *) * (size_t) selected_count);
        if (projected == NULL) {
            sql_set_error(error, 0, 0, "out of memory while formatting SELECT output");
            goto cleanup;
        }
        for (column_index = 0; column_index < selected_count; column_index++) {
            projected[column_index] = matches.rows[index]->fields[selected_indexes[column_index]];
        }
        print_row(out, projected, widths, selected_count);
        free(projected);
    }
    print_border(out, widths, selected_count);
    fprintf(out, "(%d rows)\n", matches.count);
    ok = 1;

cleanup:
    free(widths);
    free(header_values);
    free(selected_indexes);
    free_matched_rows(&matches);
    free_rowset(&rowset);
    free_schema(&schema);
    return ok;
}

/* Executes a single parsed query against the file-based database root. */
int execute_query(const Query *query, const char *db_root, FILE *out, SqlError *error) {
    error->message[0] = '\0';

    if (query->type == QUERY_INSERT) {
        return execute_insert(&query->insert_query, db_root, out, error);
    }

    return execute_select(&query->select_query, db_root, out, error);
}

/* Executes all parsed statements in order so one SQL file can drive a workflow. */
int execute_query_list(const QueryList *queries, const char *db_root, FILE *out, SqlError *error) {
    int index;

    for (index = 0; index < queries->count; index++) {
        if (index > 0) {
            fputc('\n', out);
        }
        if (!execute_query(queries->items[index], db_root, out, error)) {
            return 0;
        }
    }

    return 1;
}
