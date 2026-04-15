#include "bench.h"

int main(int argc, char **argv) {
    SqlError error = {0, 0, {0}};
    long row_count = 10000;
    long iterations = 200;
    char *end = NULL;

    if (argc >= 2) {
        errno = 0;
        row_count = strtol(argv[1], &end, 10);
        if (errno != 0 || end == argv[1] || *end != '\0' || row_count <= 0) {
            fprintf(stderr, "usage: %s [row-count] [iterations]\n", argv[0]);
            return 1;
        }
    }
    if (argc >= 3) {
        errno = 0;
        iterations = strtol(argv[2], &end, 10);
        if (errno != 0 || end == argv[2] || *end != '\0' || iterations <= 0) {
            fprintf(stderr, "usage: %s [row-count] [iterations]\n", argv[0]);
            return 1;
        }
    }
    if (argc > 3) {
        fprintf(stderr, "usage: %s [row-count] [iterations]\n", argv[0]);
        return 1;
    }

    if (!bench_run("./data", row_count, iterations, stdout, &error)) {
        fprintf(stderr, "error: %s\n", error.message);
        return 1;
    }

    return 0;
}
