#ifndef TESTS_SUPPORT_UNITY_H
#define TESTS_SUPPORT_UNITY_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef void (*UnityTestFunction)(void);

/* ms: Existing unit tests now share these Unity-style hooks and assertions. */
void UnityBegin(const char *suite_name);
int UnityEnd(void);
void UnityDefaultTestRun(UnityTestFunction func, const char *name, int line);
void UnityAssertImplementation(
    int condition,
    const char *expression,
    const char *message,
    const char *file,
    int line
);
void UnityAssertEqualIntImplementation(
    long expected,
    long actual,
    const char *message,
    const char *file,
    int line
);
void UnityAssertEqualStringImplementation(
    const char *expected,
    const char *actual,
    const char *message,
    const char *file,
    int line
);
void UnityAssertNullImplementation(const void *pointer, const char *message, const char *file, int line);
void UnityAssertNotNullImplementation(const void *pointer, const char *message, const char *file, int line);

#define UNITY_BEGIN() \
    do { \
        UnityBegin(__FILE__); \
    } while (0)
#define UNITY_END() UnityEnd()
#define RUN_TEST(func) UnityDefaultTestRun(func, #func, __LINE__)

#define TEST_ASSERT_TRUE(condition) \
    UnityAssertImplementation((condition) != 0, #condition, NULL, __FILE__, __LINE__)
#define TEST_ASSERT_FALSE(condition) \
    UnityAssertImplementation((condition) == 0, #condition, NULL, __FILE__, __LINE__)
#define TEST_ASSERT_TRUE_MESSAGE(condition, message) \
    UnityAssertImplementation((condition) != 0, #condition, message, __FILE__, __LINE__)
#define TEST_ASSERT_FALSE_MESSAGE(condition, message) \
    UnityAssertImplementation((condition) == 0, #condition, message, __FILE__, __LINE__)
#define TEST_ASSERT_EQUAL_INT(expected, actual) \
    UnityAssertEqualIntImplementation((long) (expected), (long) (actual), NULL, __FILE__, __LINE__)
#define TEST_ASSERT_EQUAL_INT_MESSAGE(expected, actual, message) \
    UnityAssertEqualIntImplementation((long) (expected), (long) (actual), message, __FILE__, __LINE__)
#define TEST_ASSERT_EQUAL_STRING(expected, actual) \
    UnityAssertEqualStringImplementation((expected), (actual), NULL, __FILE__, __LINE__)
#define TEST_ASSERT_EQUAL_STRING_MESSAGE(expected, actual, message) \
    UnityAssertEqualStringImplementation((expected), (actual), message, __FILE__, __LINE__)
#define TEST_ASSERT_NULL(pointer) \
    UnityAssertNullImplementation((pointer), NULL, __FILE__, __LINE__)
#define TEST_ASSERT_NULL_MESSAGE(pointer, message) \
    UnityAssertNullImplementation((pointer), (message), __FILE__, __LINE__)
#define TEST_ASSERT_NOT_NULL(pointer) \
    UnityAssertNotNullImplementation((pointer), NULL, __FILE__, __LINE__)
#define TEST_ASSERT_NOT_NULL_MESSAGE(pointer, message) \
    UnityAssertNotNullImplementation((pointer), (message), __FILE__, __LINE__)
#define TEST_FAIL_MESSAGE(message) \
    UnityAssertImplementation(0, "TEST_FAIL", (message), __FILE__, __LINE__)

#endif
