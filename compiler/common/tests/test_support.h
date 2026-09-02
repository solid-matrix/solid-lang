/**
 * @file test_support.h
 * @brief Unity glue shared by every module's unit tests.
 * @author solid-matrix
 */

#pragma once

#include <stdio.h>
#include <string.h>

#include "strview.h"
#include "unity.h"

/** One selectable unit-test case: argv name plus its run function. */
typedef struct {
  const char *name;
  void (*run)(void);
} TestDispatchEntry;

#ifndef TEST_SUPPORT_NO_DEFAULT_FIXTURES
// Default per-test fixtures for binaries without their own; define
// TEST_SUPPORT_NO_DEFAULT_FIXTURES before including to supply both.
__attribute__((weak)) void setUp(void) {}
__attribute__((weak)) void tearDown(void) {}
#endif

/**
 * Expands into main(): with no arguments every case runs in one Unity
 * session; with an argument only the matching case runs, which lets
 * CTest register one entry per case and report failures individually.
 * Exits 2 when the requested name matches no case (wiring mistake).
 */
#define TEST_DISPATCH_MAIN(entries)                                            \
  int main(int argc, char **argv) {                                            \
    int total = (int)(sizeof(entries) / sizeof((entries)[0]));                 \
    int picked[total];                                                         \
    int picked_count = 0;                                                      \
    for (int i = 0; i < total; i++) {                                          \
      if (argc <= 1 || strcmp(argv[1], entries[i].name) == 0)                  \
        picked[picked_count++] = i;                                            \
    }                                                                          \
    if (picked_count == 0) {                                                   \
      fprintf(stderr, "error: no test case matches '%s'\n",                    \
              argc > 1 ? argv[1] : "<all>");                                   \
      return 2;                                                                \
    }                                                                          \
    UNITY_BEGIN();                                                             \
    for (int i = 0; i < picked_count; i++)                                     \
      RUN_TEST(entries[picked[i]].run);                                        \
    return UNITY_END();                                                        \
  }

/** Asserts that @p actual (a Strview) equals the C string literal. */
#define TEST_ASSERT_STRVIEW_EQ(actual, expected_cstr)                          \
  do {                                                                         \
    Strview sv_ = (actual);                                                    \
    const char *es_ = (expected_cstr);                                         \
    size_t el_ = strlen(es_);                                                  \
    if (!(sv_.len == el_ && memcmp(sv_.data, es_, el_) == 0)) {                \
      fprintf(stderr, "  strview mismatch: expected \"%s\" (len %zu), got "    \
                      "len %zu\n",                                             \
              es_, el_, sv_.len);                                              \
      TEST_FAIL_MESSAGE("Strview contents differ");                            \
    }                                                                          \
  } while (0)
