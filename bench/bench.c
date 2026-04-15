#include "bench.h"

#include <time.h>

static int append_bench_row(
    DbContext *ctx,
    TableState *table,
    int offset,
    SqlError *error
) {
    char id[32];
    char name[64];
    char grade[16];
    char age[16];
    char score[32];
    const char *regions[] = {"Seoul", "Busan", "Incheon", "Daegu", "Daejeon"};
    char *fields[6];
    int generated_id = 202500001 + offset;

    (void) table;

    snprintf(id, sizeof(id), "%d", generated_id);
    snprintf(name, sizeof(name), "Student_%d", generated_id);
    snprintf(grade, sizeof(grade), "%d", (offset % 4) + 1);
    snprintf(age, sizeof(age), "%d", 19 + (offset % 9));
    snprintf(score, sizeof(score), "%.2f", ((double) (offset % 46)) / 10.0);

    fields[0] = id;
    fields[1] = name;
    fields[2] = grade;
    fields[3] = age;
    fields[4] = (char *) regions[offset % 5];
    fields[5] = score;

    return db_context_insert_row(ctx, "students", fields, error);
}

static double seconds_since(clock_t start) {
    return (double) (clock() - start) / (double) CLOCKS_PER_SEC;
}

int run_benchmark(DbContext *ctx, int row_count, FILE *out, SqlError *error) {
    TableState *table = db_context_find_table(ctx, "students");
    clock_t started;
    clock_t indexed_started;
    clock_t linear_started;
    clock_t range_started;
    double insert_seconds;
    double indexed_seconds;
    double linear_seconds;
    double range_seconds;
    int middle_id;
    int row_index = -1;
    int linear_matches = 0;
    int *range_indexes = NULL;
    int range_count = 0;
    int low_id;
    int high_id;
    int index;

    if (table == NULL) {
        sql_set_error(error, 0, 0, "benchmark requires `students` table");
        return 0;
    }
    if (table->schema.column_count != 6 || table->id_column_index < 0) {
        sql_set_error(error, 0, 0, "benchmark expects students schema with id/name/grade/age/region/score");
        return 0;
    }

    started = clock();
    for (index = 0; index < row_count; index++) {
        if (!append_bench_row(ctx, table, index, error)) {
            return 0;
        }
    }
    insert_seconds = seconds_since(started);

    middle_id = 202500001 + (row_count / 2);
    low_id = middle_id - (row_count / 4);
    if (low_id < 1) {
        low_id = 1;
    }
    high_id = middle_id + (row_count / 4);

    indexed_started = clock();
    (void) bptree_search(table->index, middle_id, &row_index);
    indexed_seconds = seconds_since(indexed_started);

    linear_started = clock();
    for (index = 0; index < table->rowset.row_count; index++) {
        if (strcmp(table->rowset.rows[index].fields[4], "Seoul") == 0) {
            linear_matches++;
            break;
        }
    }
    linear_seconds = seconds_since(linear_started);

    range_started = clock();
    if (!bptree_range_search(table->index, low_id, high_id, &range_indexes, &range_count, error)) {
        return 0;
    }
    range_seconds = seconds_since(range_started);

    fprintf(out, "[BENCH] Inserted %d student rows in %.6fs\n", row_count, insert_seconds);
    fprintf(
        out,
        "[BENCH] Indexed  SELECT (id=%d):                  %.6fs  (~%d comparisons)\n",
        middle_id,
        indexed_seconds,
        bptree_height(table->index)
    );
    fprintf(
        out,
        "[BENCH] Linear   SELECT (region='Seoul'):       %.6fs  (~%d comparisons)\n",
        linear_seconds,
        table->rowset.row_count / 2
    );
    fprintf(
        out,
        "[BENCH] Range    SELECT (id BETWEEN ...):             %.6fs  (%d results)\n",
        range_seconds,
        range_count
    );

    free(range_indexes);
    return linear_matches > 0 && row_index >= 0;
}
