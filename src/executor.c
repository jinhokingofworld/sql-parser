#include "executor.h"

#include <limits.h>

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

static const char *column_type_name(ColumnType type) {
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
    long parsed_int = 0;
    double parsed_float = 0.0;

    if (type == COLUMN_TYPE_INT) {
        return value->type == VALUE_INT && parse_int_strict(value->raw, &parsed_int);
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

static int validate_condition_value(
    const Condition *condition,
    const Schema *schema,
    int column_index,
    SqlError *error
) {
    ColumnType type = schema->columns[column_index].type;

    if (condition->type == COND_BETWEEN) {
        if (!is_value_compatible(type, &condition->low) || !is_value_compatible(type, &condition->high)) {
            sql_set_error(
                error,
                0,
                0,
                "type mismatch: column `%s` expects %s",
                condition->column,
                column_type_name(type)
            );
            return 0;
        }
        return 1;
    }

    if (!is_value_compatible(type, &condition->value)) {
        sql_set_error(
            error,
            0,
            0,
            "type mismatch: column `%s` expects %s",
            condition->column,
            column_type_name(type)
        );
        return 0;
    }

    return 1;
}

static int row_matches_condition(const Row *row, const Schema *schema, const Condition *condition, SqlError *error) {
    int column_index = schema_find_column(schema, condition->column);
    ColumnType type;

    if (column_index < 0) {
        sql_set_error(error, 0, 0, "unknown column `%s`", condition->column);
        return 0;
    }

    if (!validate_condition_value(condition, schema, column_index, error)) {
        return 0;
    }

    type = schema->columns[column_index].type;
    if (condition->type == COND_BETWEEN) {
        if (compare_field_values(condition->low.raw, condition->high.raw, type) > 0) {
            return 0;
        }
        return compare_field_values(row->fields[column_index], condition->low.raw, type) >= 0 &&
            compare_field_values(row->fields[column_index], condition->high.raw, type) <= 0;
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

static int append_match(MatchedRows *matches, Row *row, SqlError *error) {
    Row **next_rows = realloc(matches->rows, sizeof(Row *) * (size_t) (matches->count + 1));

    if (next_rows == NULL) {
        sql_set_error(error, 0, 0, "out of memory while filtering rows");
        return 0;
    }

    matches->rows = next_rows;
    matches->rows[matches->count++] = row;
    return 1;
}

static int collect_linear_matches(
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
        if (!append_match(matches, &rowset->rows[row_index], error)) {
            free_matched_rows(matches);
            return 0;
        }
    }

    return 1;
}

static int collect_indexed_eq_matches(
    const TableState *table,
    const Condition *condition,
    MatchedRows *matches,
    SqlError *error
) {
    long key = 0;
    int row_index = 0;

    matches->rows = NULL;
    matches->count = 0;

    if (condition->value.type != VALUE_INT ||
        !parse_int_strict(condition->value.raw, &key) ||
        key < INT_MIN ||
        key > INT_MAX) {
        sql_set_error(error, 0, 0, "type mismatch: column `id` expects int");
        return 0;
    }
    if (!bptree_search(table->index, (int) key, &row_index)) {
        return 1;
    }

    return append_match(matches, &table->rowset.rows[row_index], error);
}

static int collect_indexed_between_matches(
    const TableState *table,
    const Condition *condition,
    MatchedRows *matches,
    SqlError *error
) {
    long low = 0;
    long high = 0;
    int *row_indexes = NULL;
    int row_count = 0;
    int index;

    matches->rows = NULL;
    matches->count = 0;

    if (condition->low.type != VALUE_INT ||
        condition->high.type != VALUE_INT ||
        !parse_int_strict(condition->low.raw, &low) ||
        !parse_int_strict(condition->high.raw, &high) ||
        low < INT_MIN ||
        low > INT_MAX ||
        high < INT_MIN ||
        high > INT_MAX) {
        sql_set_error(error, 0, 0, "type mismatch: column `id` expects int");
        return 0;
    }
    if (!bptree_range_search(table->index, (int) low, (int) high, &row_indexes, &row_count, error)) {
        return 0;
    }

    for (index = 0; index < row_count; index++) {
        if (!append_match(matches, &table->rowset.rows[row_indexes[index]], error)) {
            free(row_indexes);
            free_matched_rows(matches);
            return 0;
        }
    }

    free(row_indexes);
    return 1;
}

static int collect_matches(
    const SelectQuery *query,
    const TableState *table,
    MatchedRows *matches,
    SqlError *error
) {
    int id_index = schema_find_column(&table->schema, "id");

    if (query->has_where &&
        id_index >= 0 &&
        sql_stricmp(query->where.column, "id") == 0 &&
        table->index != NULL) {
        if (query->where.type == COND_EQ) {
            return collect_indexed_eq_matches(table, &query->where, matches, error);
        }
        if (query->where.type == COND_BETWEEN) {
            return collect_indexed_between_matches(table, &query->where, matches, error);
        }
    }

    return collect_linear_matches(query, &table->schema, &table->rowset, matches, error);
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

static int build_autoincrement_value(TableState *table, char **out_value, SqlError *error) {
    char buffer[32];

    if (snprintf(buffer, sizeof(buffer), "%d", table->next_id) >= (int) sizeof(buffer)) {
        sql_set_error(error, 0, 0, "failed to format auto-increment id");
        return 0;
    }

    *out_value = sql_strdup(buffer);
    if (*out_value == NULL) {
        sql_set_error(error, 0, 0, "out of memory while assigning auto-increment id");
        return 0;
    }

    return 1;
}

static int execute_insert(const InsertQuery *query, DbContext *ctx, FILE *out, SqlError *error) {
    TableState *table = db_context_find_table(ctx, query->table_name);
    Schema *schema;
    char **ordered_fields = NULL;
    int *seen = NULL;
    int id_index;
    int expected_column_count;
    int index;
    int ok = 0;

    if (table == NULL) {
        sql_set_error(error, 0, 0, "unknown table `%s`", query->table_name);
        return 0;
    }

    schema = &table->schema;
    id_index = schema_find_column(schema, "id");
    if (schema->autoincrement) {
        for (index = 0; index < query->column_count; index++) {
            if (sql_stricmp(query->columns[index], "id") == 0) {
                sql_set_error(error, 0, 0, "column 'id' is reserved and cannot be specified manually");
                return 0;
            }
        }
    }

    expected_column_count = schema->column_count - (schema->autoincrement ? 1 : 0);
    if (query->column_count != expected_column_count) {
        sql_set_error(error, 0, 0, "INSERT must provide exactly %d columns", expected_column_count);
        return 0;
    }

    ordered_fields = calloc((size_t) schema->column_count, sizeof(char *));
    seen = calloc((size_t) schema->column_count, sizeof(int));
    if (ordered_fields == NULL || seen == NULL) {
        sql_set_error(error, 0, 0, "out of memory while executing INSERT");
        goto cleanup;
    }

    for (index = 0; index < query->column_count; index++) {
        int schema_index;

        schema_index = schema_find_column(schema, query->columns[index]);
        if (schema_index < 0) {
            sql_set_error(error, 0, 0, "unknown column `%s`", query->columns[index]);
            goto cleanup;
        }
        if (seen[schema_index]) {
            sql_set_error(error, 0, 0, "duplicate column `%s`", query->columns[index]);
            goto cleanup;
        }
        if (!is_value_compatible(schema->columns[schema_index].type, &query->values[index])) {
            sql_set_error(
                error,
                0,
                0,
                "type mismatch: column `%s` expects %s",
                query->columns[index],
                column_type_name(schema->columns[schema_index].type)
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

    for (index = 0; index < schema->column_count; index++) {
        if (schema->autoincrement && index == id_index) {
            continue;
        }
        if (!seen[index]) {
            sql_set_error(error, 0, 0, "missing value for column `%s`", schema->columns[index].name);
            goto cleanup;
        }
    }

    if (schema->autoincrement) {
        if (id_index < 0) {
            sql_set_error(error, 0, 0, "autoincrement schema is missing reserved column `id`");
            goto cleanup;
        }
        if (!build_autoincrement_value(table, &ordered_fields[id_index], error)) {
            goto cleanup;
        }
    }

    if (!db_context_insert_row(ctx, query->table_name, ordered_fields, error)) {
        goto cleanup;
    }

    fprintf(out, "INSERT 1\n");
    ok = 1;

cleanup:
    if (ordered_fields != NULL) {
        for (index = 0; index < schema->column_count; index++) {
            free(ordered_fields[index]);
        }
    }
    free(ordered_fields);
    free(seen);
    return ok;
}

static int execute_select(const SelectQuery *query, DbContext *ctx, FILE *out, SqlError *error) {
    TableState *table = db_context_find_table(ctx, query->table_name);
    MatchedRows matches;
    int *selected_indexes = NULL;
    int selected_count = 0;
    size_t *widths = NULL;
    char **header_values = NULL;
    int index;
    int ok = 0;

    if (table == NULL) {
        sql_set_error(error, 0, 0, "unknown table `%s`", query->table_name);
        return 0;
    }
    if (!prepare_projection(query, &table->schema, &selected_indexes, &selected_count, error)) {
        return 0;
    }
    if (!collect_matches(query, table, &matches, error)) {
        free(selected_indexes);
        return 0;
    }

    if (query->has_order_by) {
        int order_index = schema_find_column(&table->schema, query->order_by.column);
        if (order_index < 0) {
            sql_set_error(error, 0, 0, "unknown column `%s`", query->order_by.column);
            goto cleanup;
        }
        g_sort_column_index = order_index;
        g_sort_column_type = table->schema.columns[order_index].type;
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
        header_values[index] = table->schema.columns[selected_indexes[index]].name;
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
    return ok;
}

int execute_query(const Query *query, DbContext *ctx, FILE *out, SqlError *error) {
    error->message[0] = '\0';

    if (query->type == QUERY_INSERT) {
        return execute_insert(&query->insert_query, ctx, out, error);
    }

    return execute_select(&query->select_query, ctx, out, error);
}

int execute_query_list(const QueryList *queries, DbContext *ctx, FILE *out, SqlError *error) {
    int index;

    for (index = 0; index < queries->count; index++) {
        if (index > 0) {
            fputc('\n', out);
        }
        if (!execute_query(queries->items[index], ctx, out, error)) {
            return 0;
        }
    }

    return 1;
}
