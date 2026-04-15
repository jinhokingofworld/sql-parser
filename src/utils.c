#include "ast.h"
#include "common.h"

static void free_insert_query(InsertQuery *query) {
    int index;

    free(query->table_name);
    for (index = 0; index < query->column_count; index++) {
        free(query->columns[index]);
        free(query->values[index].raw);
    }
    free(query->columns);
    free(query->values);
}

static void free_select_query(SelectQuery *query) {
    int index;

    free(query->table_name);
    for (index = 0; index < query->column_count; index++) {
        free(query->columns[index]);
    }
    free(query->columns);
    free(query->where.column);
    free(query->where.value.raw);
    free(query->where.low.raw);
    free(query->where.high.raw);
    free(query->order_by.column);
}

/* Formats a user-facing error message with optional line and column metadata. */
void sql_set_error(SqlError *error, int line, int column, const char *fmt, ...) {
    va_list args;

    if (error == NULL) {
        return;
    }

    error->line = line;
    error->column = column;

    va_start(args, fmt);
    vsnprintf(error->message, sizeof(error->message), fmt, args);
    va_end(args);
}

/* Duplicates a null-terminated string so modules can own copied input safely. */
char *sql_strdup(const char *src) {
    size_t len;
    char *copy;

    if (src == NULL) {
        return NULL;
    }

    len = strlen(src);
    copy = malloc(len + 1);
    if (copy == NULL) {
        return NULL;
    }

    memcpy(copy, src, len + 1);
    return copy;
}

/* Duplicates a bounded string slice and appends a terminating null byte. */
char *sql_strndup(const char *src, size_t len) {
    char *copy = malloc(len + 1);

    if (copy == NULL) {
        return NULL;
    }

    memcpy(copy, src, len);
    copy[len] = '\0';
    return copy;
}

/* Compares two strings without case sensitivity for SQL keyword handling. */
int sql_stricmp(const char *left, const char *right) {
    unsigned char lhs;
    unsigned char rhs;

    while (*left != '\0' && *right != '\0') {
        lhs = (unsigned char) tolower((unsigned char) *left);
        rhs = (unsigned char) tolower((unsigned char) *right);
        if (lhs != rhs) {
            return lhs - rhs;
        }
        left++;
        right++;
    }

    return (unsigned char) *left - (unsigned char) *right;
}

/* Reads an entire text file into memory for tokenization and parsing. */
char *sql_read_text_file(const char *path, SqlError *error) {
    FILE *file;
    long length;
    size_t read_count;
    char *buffer;

    file = fopen(path, "rb");
    if (file == NULL) {
        sql_set_error(error, 0, 0, "failed to open `%s`: %s", path, strerror(errno));
        return NULL;
    }

    if (fseek(file, 0L, SEEK_END) != 0) {
        sql_set_error(error, 0, 0, "failed to seek `%s`", path);
        fclose(file);
        return NULL;
    }

    length = ftell(file);
    if (length < 0) {
        sql_set_error(error, 0, 0, "failed to read size of `%s`", path);
        fclose(file);
        return NULL;
    }

    if (fseek(file, 0L, SEEK_SET) != 0) {
        sql_set_error(error, 0, 0, "failed to rewind `%s`", path);
        fclose(file);
        return NULL;
    }

    buffer = malloc((size_t) length + 1);
    if (buffer == NULL) {
        sql_set_error(error, 0, 0, "out of memory while reading `%s`", path);
        fclose(file);
        return NULL;
    }

    read_count = fread(buffer, 1U, (size_t) length, file);
    if (read_count != (size_t) length) {
        sql_set_error(error, 0, 0, "failed to read `%s`", path);
        free(buffer);
        fclose(file);
        return NULL;
    }

    buffer[length] = '\0';
    fclose(file);
    return buffer;
}

/* Releases every heap allocation owned by a parsed query node. */
void free_query(Query *query) {
    if (query == NULL) {
        return;
    }

    if (query->type == QUERY_INSERT) {
        free_insert_query(&query->insert_query);
    } else {
        free_select_query(&query->select_query);
    }

    free(query);
}

/* Releases a list of parsed queries after execution or test assertions complete. */
void free_query_list(QueryList *list) {
    int index;

    if (list == NULL) {
        return;
    }

    for (index = 0; index < list->count; index++) {
        free_query(list->items[index]);
    }

    free(list->items);
    list->items = NULL;
    list->count = 0;
}

/* Prints a compact explain-style view of a parsed query for CLI inspection. */
void print_query(const Query *query, FILE *out) {
    int index;

    if (query->type == QUERY_INSERT) {
        fprintf(out, "QUERY_INSERT table=%s columns=[", query->insert_query.table_name);
        for (index = 0; index < query->insert_query.column_count; index++) {
            if (index > 0) {
                fputs(", ", out);
            }
            fprintf(
                out,
                "%s=%s",
                query->insert_query.columns[index],
                query->insert_query.values[index].raw
            );
        }
        fputs("]\n", out);
        return;
    }

    fprintf(out, "QUERY_SELECT table=%s columns=", query->select_query.table_name);
    if (query->select_query.select_all) {
        fputs("*", out);
    } else {
        fputc('[', out);
        for (index = 0; index < query->select_query.column_count; index++) {
            if (index > 0) {
                fputs(", ", out);
            }
            fputs(query->select_query.columns[index], out);
        }
        fputc(']', out);
    }

    if (query->select_query.has_where) {
        if (query->select_query.where.type == COND_BETWEEN) {
            fprintf(
                out,
                " where=%s BETWEEN %s AND %s",
                query->select_query.where.column,
                query->select_query.where.low.raw,
                query->select_query.where.high.raw
            );
        } else {
            fprintf(
                out,
                " where=%s=%s",
                query->select_query.where.column,
                query->select_query.where.value.raw
            );
        }
    }

    if (query->select_query.has_order_by) {
        fprintf(
            out,
            " order_by=%s %s",
            query->select_query.order_by.column,
            query->select_query.order_by.ascending ? "ASC" : "DESC"
        );
    }

    fputc('\n', out);
}
