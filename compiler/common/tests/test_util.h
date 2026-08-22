/**
 * @file test_util.h
 * @brief Minimal test harness shared by the unit test executables.
 * @author solid-matrix
 * @version 0.0.5
 *
 * Each test executable defines a TestEntry table and ends with
 * TEST_MAIN(entries). When invoked with no arguments every case runs;
 * with an argument only the case whose name matches runs, which lets
 * CTest register one entry per case and report failures individually.
 *
 * Conventions:
 *   - A failed CHECK increments g_failures and is printed to stderr; it
 *     never aborts the process.
 *   - The process exits EXIT_FAILURE when any selected case failed,
 *     EXIT_SUCCESS otherwise (and 2 when the requested name matched no
 *     case, which indicates a CMake/CTest wiring mistake).
 */

#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/** Number of CHECK() failures in the currently running case. */
static int g_failures;

/**
 * @brief Fails the current case when @p cond evaluates to false.
 */
#define CHECK(cond)                                                           \
  do {                                                                        \
    if (!(cond)) {                                                            \
      g_failures++;                                                           \
      fprintf(stderr, "  CHECK failed %s:%d: %s\n", __FILE__, __LINE__,       \
              #cond);                                                         \
    }                                                                         \
  } while (0)

/** One named unit-test case: a display name plus its run function. */
typedef struct {
  const char *name;
  void (*run)(void);
} TestEntry;

/**
 * @brief Expands into main(): runs all entries, or the one named by
 *        argv[1], and exits non-zero if any case failed.
 */
#define TEST_MAIN(tests)                                                      \
  int main(int argc, char **argv) {                                           \
    int total = (int)(sizeof(tests) / sizeof((tests)[0]));                    \
    int ran = 0, failed_cases = 0;                                            \
    for (int i = 0; i < total; i++) {                                         \
      if (argc > 1 && strcmp(argv[1], tests[i].name) != 0)                    \
        continue;                                                             \
      g_failures = 0;                                                         \
      fprintf(stderr, "[ RUN  ] %s\n", tests[i].name);                        \
      tests[i].run();                                                         \
      if (g_failures == 0)                                                    \
        fprintf(stderr, "[  OK  ] %s\n", tests[i].name);                      \
      else                                                                    \
        fprintf(stderr, "[ FAIL ] %s (%d check(s) failed)\n",                 \
                tests[i].name, g_failures);                                   \
      ran++;                                                                  \
      failed_cases += (g_failures != 0);                                      \
    }                                                                         \
    if (ran == 0) {                                                           \
      fprintf(stderr, "error: no test case matches '%s'\n",                   \
              argc > 1 ? argv[1] : "<all>");                                  \
      return 2;                                                               \
    }                                                                         \
    fprintf(stderr, "%d/%d case(s) passed\n", ran - failed_cases, ran);       \
    return failed_cases == 0 ? EXIT_SUCCESS : EXIT_FAILURE;                   \
  }
