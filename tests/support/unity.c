#include "unity.h"

static const char *g_current_suite = NULL;
static const char *g_current_test = NULL;
static int g_total_tests = 0;
static int g_failed_tests = 0;
static int g_current_test_failed = 0;

/* ms: Keep suite labels short by printing the basename instead of the full path. */
static const char *unity_basename(const char *path) {
    const char *last_slash = strrchr(path, '/');
    const char *last_backslash = strrchr(path, '\\');
    const char *candidate = path;

    if (last_slash != NULL && last_slash[1] != '\0') {
        candidate = last_slash + 1;
    }
    if (last_backslash != NULL && last_backslash[1] != '\0' && last_backslash + 1 > candidate) {
        candidate = last_backslash + 1;
    }

    return candidate;
}

/* ms: Print optional assertion context without forcing every assertion to carry text. */
static void unity_print_message(const char *file, int line, const char *message) {
    if (message != NULL && message[0] != '\0') {
        fprintf(stderr, "%s:%d: %s\n", file, line, message);
    }
}

void UnityBegin(const char *suite_name) {
    g_current_suite = suite_name;
    g_current_test = NULL;
    g_total_tests = 0;
    g_failed_tests = 0;
    g_current_test_failed = 0;
    printf("[TEST SUITE] %s\n", unity_basename(g_current_suite));
}

/* ms: Keep the runner output compact so the existing Makefile-driven workflow stays readable. */
int UnityEnd(void) {
    printf("[RESULT] total=%d passed=%d failed=%d\n", g_total_tests, g_total_tests - g_failed_tests, g_failed_tests);
    return g_failed_tests == 0 ? 0 : 1;
}

/* ms: Mirrors Unity's per-test lifecycle so each legacy test file can grow by adding RUN_TEST calls. */
void UnityDefaultTestRun(UnityTestFunction func, const char *name, int line) {
    extern void setUp(void);
    extern void tearDown(void);

    (void) line;
    g_total_tests++;
    g_current_test = name;
    g_current_test_failed = 0;

    setUp();
    func();
    tearDown();

    if (g_current_test_failed) {
        g_failed_tests++;
        printf("[FAIL] %s\n", name);
    } else {
        printf("[PASS] %s\n", name);
    }
}

void UnityAssertImplementation(
    int condition,
    const char *expression,
    const char *message,
    const char *file,
    int line
) {
    if (condition) {
        return;
    }

    g_current_test_failed = 1;
    fprintf(stderr, "%s:%d: assertion failed in %s: %s\n", file, line, g_current_test, expression);
    unity_print_message(file, line, message);
}

void UnityAssertEqualIntImplementation(
    long expected,
    long actual,
    const char *message,
    const char *file,
    int line
) {
    if (expected == actual) {
        return;
    }

    g_current_test_failed = 1;
    fprintf(
        stderr,
        "%s:%d: assertion failed in %s: expected <%ld> but was <%ld>\n",
        file,
        line,
        g_current_test,
        expected,
        actual
    );
    unity_print_message(file, line, message);
}

void UnityAssertEqualStringImplementation(
    const char *expected,
    const char *actual,
    const char *message,
    const char *file,
    int line
) {
    const char *safe_expected = expected == NULL ? "(null)" : expected;
    const char *safe_actual = actual == NULL ? "(null)" : actual;

    if (expected != NULL && actual != NULL && strcmp(expected, actual) == 0) {
        return;
    }
    if (expected == NULL && actual == NULL) {
        return;
    }

    g_current_test_failed = 1;
    fprintf(
        stderr,
        "%s:%d: assertion failed in %s: expected <%s> but was <%s>\n",
        file,
        line,
        g_current_test,
        safe_expected,
        safe_actual
    );
    unity_print_message(file, line, message);
}

void UnityAssertNullImplementation(const void *pointer, const char *message, const char *file, int line) {
    if (pointer == NULL) {
        return;
    }

    g_current_test_failed = 1;
    fprintf(stderr, "%s:%d: assertion failed in %s: pointer expected to be NULL\n", file, line, g_current_test);
    unity_print_message(file, line, message);
}

void UnityAssertNotNullImplementation(const void *pointer, const char *message, const char *file, int line) {
    if (pointer != NULL) {
        return;
    }

    g_current_test_failed = 1;
    fprintf(stderr, "%s:%d: assertion failed in %s: pointer expected to be non-NULL\n", file, line, g_current_test);
    unity_print_message(file, line, message);
}
