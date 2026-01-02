/*
 * Tank Game - Minimal Test Framework
 *
 * Usage:
 *   TEST(test_name) {
 *       ASSERT(condition);
 *       ASSERT_EQ(expected, actual);
 *       ASSERT_NEAR(expected, actual, epsilon);
 *   }
 */

#ifndef PZ_TEST_FRAMEWORK_H
#define PZ_TEST_FRAMEWORK_H

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Colors for terminal output
#define PZ_COLOR_RED "\x1b[31m"
#define PZ_COLOR_GREEN "\x1b[32m"
#define PZ_COLOR_YELLOW "\x1b[33m"
#define PZ_COLOR_RESET "\x1b[0m"

// Test function type
typedef void (*pz_test_fn)(void);

// Test entry structure
typedef struct pz_test_entry {
    const char *test_name;
    pz_test_fn test_fn;
} pz_test_entry;

// Test state (static globals)
static int pz_tests_run = 0;
static int pz_tests_passed = 0;
static int pz_tests_failed = 0;
static const char *pz_current_test = NULL;

#define PZ_MAX_TESTS 256
static pz_test_entry pz_all_tests[PZ_MAX_TESTS];
static int pz_test_count = 0;

// Register a test
#define TEST(name)                                                             \
    static void test_##name(void);                                             \
    __attribute__((constructor)) static void register_test_##name(void)        \
    {                                                                          \
        if (pz_test_count < PZ_MAX_TESTS) {                                    \
            pz_all_tests[pz_test_count].test_name = #name;                     \
            pz_all_tests[pz_test_count].test_fn = test_##name;                 \
            pz_test_count++;                                                   \
        }                                                                      \
    }                                                                          \
    static void test_##name(void)

// Assertions
#define ASSERT(cond)                                                           \
    do {                                                                       \
        if (!(cond)) {                                                         \
            printf(PZ_COLOR_RED "  FAIL: %s:%d: %s" PZ_COLOR_RESET "\n",       \
                __FILE__, __LINE__, #cond);                                    \
            pz_tests_failed++;                                                 \
            return;                                                            \
        }                                                                      \
    } while (0)

#define ASSERT_EQ(expected, actual)                                            \
    do {                                                                       \
        if ((expected) != (actual)) {                                          \
            printf(PZ_COLOR_RED                                                \
                "  FAIL: %s:%d: expected %d, got %d" PZ_COLOR_RESET "\n",      \
                __FILE__, __LINE__, (int)(expected), (int)(actual));           \
            pz_tests_failed++;                                                 \
            return;                                                            \
        }                                                                      \
    } while (0)

#define ASSERT_STR_EQ(expected, actual)                                        \
    do {                                                                       \
        if (strcmp((expected), (actual)) != 0) {                               \
            printf(PZ_COLOR_RED                                                \
                "  FAIL: %s:%d: expected \"%s\", got \"%s\"" PZ_COLOR_RESET    \
                "\n",                                                          \
                __FILE__, __LINE__, (expected), (actual));                     \
            pz_tests_failed++;                                                 \
            return;                                                            \
        }                                                                      \
    } while (0)

#define ASSERT_NEAR(expected, actual, epsilon)                                 \
    do {                                                                       \
        double _diff = fabs((double)(expected) - (double)(actual));            \
        if (_diff > (epsilon)) {                                               \
            printf(PZ_COLOR_RED "  FAIL: %s:%d: expected %.6f, got %.6f "      \
                                "(diff=%.6f)" PZ_COLOR_RESET "\n",             \
                __FILE__, __LINE__, (double)(expected), (double)(actual),      \
                _diff);                                                        \
            pz_tests_failed++;                                                 \
            return;                                                            \
        }                                                                      \
    } while (0)

#define ASSERT_NULL(ptr)                                                       \
    do {                                                                       \
        if ((ptr) != NULL) {                                                   \
            printf(PZ_COLOR_RED "  FAIL: %s:%d: expected NULL" PZ_COLOR_RESET  \
                                "\n",                                          \
                __FILE__, __LINE__);                                           \
            pz_tests_failed++;                                                 \
            return;                                                            \
        }                                                                      \
    } while (0)

#define ASSERT_NOT_NULL(ptr)                                                   \
    do {                                                                       \
        if ((ptr) == NULL) {                                                   \
            printf(PZ_COLOR_RED                                                \
                "  FAIL: %s:%d: unexpected NULL" PZ_COLOR_RESET "\n",          \
                __FILE__, __LINE__);                                           \
            pz_tests_failed++;                                                 \
            return;                                                            \
        }                                                                      \
    } while (0)

// Run all tests
static int
pz_run_all_tests(void)
{
    printf("\n" PZ_COLOR_YELLOW "Running %d tests..." PZ_COLOR_RESET "\n\n",
        pz_test_count);

    for (int i = 0; i < pz_test_count; i++) {
        pz_current_test = pz_all_tests[i].test_name;
        pz_tests_run++;

        int failed_before = pz_tests_failed;
        pz_all_tests[i].test_fn();

        if (pz_tests_failed == failed_before) {
            printf(PZ_COLOR_GREEN "  PASS" PZ_COLOR_RESET ": %s\n",
                pz_current_test);
            pz_tests_passed++;
        } else {
            printf("        in test: %s\n", pz_current_test);
        }
    }

    printf("\n"
           "-----------------------------------\n");
    printf("Tests: %d total, ", pz_tests_run);
    if (pz_tests_passed > 0)
        printf(PZ_COLOR_GREEN "%d passed" PZ_COLOR_RESET ", ", pz_tests_passed);
    if (pz_tests_failed > 0)
        printf(PZ_COLOR_RED "%d failed" PZ_COLOR_RESET, pz_tests_failed);
    else
        printf("0 failed");
    printf("\n\n");

    return pz_tests_failed > 0 ? 1 : 0;
}

// Main entry point for test executable
#define TEST_MAIN()                                                            \
    int main(int argc, char **argv)                                            \
    {                                                                          \
        (void)argc;                                                            \
        (void)argv;                                                            \
        return pz_run_all_tests();                                             \
    }

#endif // PZ_TEST_FRAMEWORK_H
