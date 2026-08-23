/**
 * @file test_xmem.c
 * @brief Tests for the fatal-failure allocation wrappers.
 * @author solid-matrix
 * @version 0.0.5
 *
 * The failure paths (OOM -> abort, size == 0 asserts) terminate the
 * process and are intentionally not exercised here; these tests cover
 * the success-path contracts only.
 */

#include <stdint.h>
#include <string.h>

#include "xmem.h"
#include "test_util.h"

static void test_malloc(void) {
  uint8_t *a = xmalloc(16);
  CHECK(a != NULL);

  // Two allocations do not alias.
  uint8_t *b = xmalloc(16);
  CHECK(b != NULL && a != b);

  // The blocks are writable.
  memset(a, 0xAB, 16);
  CHECK(a[0] == 0xAB && a[15] == 0xAB);

  xfree(a);
  xfree(b);
}

static void test_calloc_zeroes(void) {
  const size_t n = 4, size = 8;
  uint8_t *p = xcalloc(n, size);
  CHECK(p != NULL);

  int all_zero = 1;
  for (size_t i = 0; i < n * size; i++)
    all_zero &= (p[i] == 0);
  CHECK(all_zero);

  xfree(p);
}

static void test_realloc_grow(void) {
  const size_t old_size = 8;
  uint8_t *p = xmalloc(old_size);
  memset(p, 0x5A, old_size);

  const size_t new_size = 32;
  p = xrealloc(p, new_size);

  // Contents up to the old size survive the resize.
  for (size_t i = 0; i < old_size; i++) {
    if (p[i] != 0x5A) {
      CHECK(!"xrealloc lost data while growing");
      break;
    }
  }

  // The grown tail is writable.
  memset(p + old_size, 0xC3, new_size - old_size);
  CHECK(p[new_size - 1] == 0xC3);

  xfree(p);
}

static void test_realloc_shrink(void) {
  uint8_t *p = xmalloc(32);
  memset(p, 1, 32);

  p = xrealloc(p, 4);
  CHECK(p != NULL);
  CHECK(p[3] == 1); // surviving prefix byte

  xfree(p);
}

static void test_realloc_null_is_malloc(void) {
  uint8_t *p = xrealloc(NULL, 4);
  CHECK(p != NULL);

  p[0] = 7;
  CHECK(p[0] == 7);

  xfree(p);
}

static void test_free_null_noop(void) {
  // Documented: NULL is allowed and does nothing.
  xfree(NULL);
  CHECK(1);
}

static const TestEntry ENTRIES[] = {
    {"malloc", test_malloc},
    {"calloc_zeroes", test_calloc_zeroes},
    {"realloc_grow", test_realloc_grow},
    {"realloc_shrink", test_realloc_shrink},
    {"realloc_null_is_malloc", test_realloc_null_is_malloc},
    {"free_null_noop", test_free_null_noop},
};

TEST_MAIN(ENTRIES)
