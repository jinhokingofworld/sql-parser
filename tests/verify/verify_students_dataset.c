#include "common.h"
#include <time.h>

typedef struct {
    char **fields;
    int field_count;
} CsvRow;

typedef struct {
    long grade_counts[4];
    long min_age;
    long max_age;
    double min_score;
    double max_score;
} DatasetStats;

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

static int parse_score_strict(const char *text, double *value) {
    char *end = NULL;
    char *dot = strchr(text, '.');
    double parsed;

    errno = 0;
    parsed = strtod(text, &end);
    if (errno != 0 || end == text || *end != '\0') {
        return 0;
    }
    if (dot != NULL && strlen(dot + 1) > 2U) {
        return 0;
    }

    *value = parsed;
    return 1;
}

static int age_matches_grade(long grade, long age) {
    switch (grade) {
        case 1:
            return age >= 19 && age <= 23;
        case 2:
            return age >= 20 && age <= 24;
        case 3:
            return age >= 21 && age <= 25;
        case 4:
            return age >= 22 && age <= 26;
        default:
            return 0;
    }
}

static void free_csv_row(CsvRow *row) {
    int index;

    for (index = 0; index < row->field_count; index++) {
        free(row->fields[index]);
    }
    free(row->fields);
    row->fields = NULL;
    row->field_count = 0;
}

static int append_csv_field(CsvRow *row, const char *buffer, size_t length, char *message, size_t size) {
    char **next_fields = realloc(row->fields, sizeof(char *) * (size_t) (row->field_count + 1));

    if (next_fields == NULL) {
        snprintf(message, size, "out of memory while parsing csv");
        return 0;
    }
    row->fields = next_fields;
    row->fields[row->field_count] = sql_strndup(buffer, length);
    if (row->fields[row->field_count] == NULL) {
        snprintf(message, size, "out of memory while parsing csv");
        return 0;
    }

    row->field_count++;
    return 1;
}

static int parse_csv_line(const char *line, CsvRow *row, char *message, size_t size) {
    size_t cursor = 0;
    char *buffer = NULL;
    size_t buffer_len = 0;
    int in_quotes = 0;

    row->fields = NULL;
    row->field_count = 0;

    while (1) {
        char current = line[cursor];

        if (current == '\0' || (!in_quotes && current == ',')) {
            if (!append_csv_field(row, buffer == NULL ? "" : buffer, buffer_len, message, size)) {
                free(buffer);
                free_csv_row(row);
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
                char *next = realloc(buffer, buffer_len + 2U);
                if (next == NULL) {
                    snprintf(message, size, "out of memory while parsing csv");
                    free(buffer);
                    free_csv_row(row);
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
            char *next = realloc(buffer, buffer_len + 2U);
            if (next == NULL) {
                snprintf(message, size, "out of memory while parsing csv");
                free(buffer);
                free_csv_row(row);
                return 0;
            }
            buffer = next;
            buffer[buffer_len++] = current;
            buffer[buffer_len] = '\0';
        }
        cursor++;
    }

    return 1;
}

static void trim_line(char *line) {
    size_t length = strlen(line);

    while (length > 0 && (line[length - 1] == '\n' || line[length - 1] == '\r')) {
        line[--length] = '\0';
    }
}

static int verify_row_contract(
    const CsvRow *row,
    long row_number,
    long expected_id,
    DatasetStats *stats,
    char *message,
    size_t size
) {
    long id = 0;
    long grade = 0;
    long age = 0;
    double score = 0.0;

    if (row->field_count != 6) {
        snprintf(message, size, "row %ld has %d fields, expected 6", row_number, row->field_count);
        return 0;
    }
    if (!parse_int_strict(row->fields[0], &id)) {
        snprintf(message, size, "row %ld id is not a strict integer", row_number);
        return 0;
    }
    if (id != expected_id) {
        snprintf(message, size, "row %ld id mismatch: expected %ld but found %ld", row_number, expected_id, id);
        return 0;
    }
    if (row->fields[1][0] == '\0') {
        snprintf(message, size, "row %ld name is empty", row_number);
        return 0;
    }
    if (!parse_int_strict(row->fields[2], &grade) || grade < 1 || grade > 4) {
        snprintf(message, size, "row %ld grade is out of range", row_number);
        return 0;
    }
    if (!parse_int_strict(row->fields[3], &age) || !age_matches_grade(grade, age)) {
        snprintf(message, size, "row %ld age does not match grade range", row_number);
        return 0;
    }
    if (row->fields[4][0] == '\0') {
        snprintf(message, size, "row %ld region is empty", row_number);
        return 0;
    }
    if (!parse_score_strict(row->fields[5], &score) || score < 0.0 || score > 4.5) {
        snprintf(message, size, "row %ld score is invalid", row_number);
        return 0;
    }

    stats->grade_counts[grade - 1]++;
    if (age < stats->min_age) {
        stats->min_age = age;
    }
    if (age > stats->max_age) {
        stats->max_age = age;
    }
    if (score < stats->min_score) {
        stats->min_score = score;
    }
    if (score > stats->max_score) {
        stats->max_score = score;
    }

    return 1;
}

static int verify_dataset_file(
    const char *csv_path,
    long expected_rows,
    long start_id,
    DatasetStats *stats,
    char *message,
    size_t size
) {
    FILE *file = fopen(csv_path, "r");
    char line[8192];
    long row_number = 0;

    if (file == NULL) {
        snprintf(message, size, "failed to open `%s`: %s", csv_path, strerror(errno));
        return 0;
    }

    stats->min_age = LONG_MAX;
    stats->max_age = LONG_MIN;
    stats->min_score = 9999.0;
    stats->max_score = -1.0;

    while (fgets(line, sizeof(line), file) != NULL) {
        CsvRow row;

        trim_line(line);
        if (line[0] == '\0') {
            continue;
        }

        row_number++;
        if (!parse_csv_line(line, &row, message, size)) {
            fclose(file);
            return 0;
        }
        if (!verify_row_contract(&row, row_number, start_id + row_number - 1L, stats, message, size)) {
            free_csv_row(&row);
            fclose(file);
            return 0;
        }
        free_csv_row(&row);
    }

    fclose(file);

    if (row_number != expected_rows) {
        snprintf(message, size, "expected %ld rows but found %ld", expected_rows, row_number);
        return 0;
    }

    return 1;
}

int main(int argc, char **argv) {
    DatasetStats stats = {{0, 0, 0, 0}, 0, 0, 0.0, 0.0};
    char message[256];
    long expected_rows;
    long start_id;
    int ok;
    clock_t started;
    double elapsed_ms;

    if (argc != 4) {
        fprintf(stderr, "usage: %s <csv-path> <expected-rows> <start-id>\n", argv[0]);
        return 1;
    }
    if (!parse_int_strict(argv[2], &expected_rows) || expected_rows <= 0) {
        fprintf(stderr, "error: expected-rows must be a positive integer\n");
        return 1;
    }
    if (!parse_int_strict(argv[3], &start_id) || start_id < 0) {
        fprintf(stderr, "error: start-id must be a non-negative integer\n");
        return 1;
    }

    started = clock();
    ok = verify_dataset_file(argv[1], expected_rows, start_id, &stats, message, sizeof(message));
    elapsed_ms = ((double) (clock() - started) * 1000.0) / (double) CLOCKS_PER_SEC;
    if (!ok) {
        fprintf(stderr, "[VERIFY FAIL] students dataset\n");
        fprintf(stderr, "  file: %s\n", argv[1]);
        fprintf(stderr, "  detail: %s\n", message);
        fprintf(stderr, "  elapsed_ms: %.2f\n", elapsed_ms);
        return 1;
    }

    printf(
        "[VERIFY PASS] students dataset\n"
        "  file: %s\n"
        "  rows: %ld\n"
        "  elapsed_ms: %.2f\n",
        argv[1],
        expected_rows,
        elapsed_ms
    );
    return 0;
}
