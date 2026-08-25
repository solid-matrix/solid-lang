#include <stdint.h>
#include <string.h>

#include "xmem.h"
#include "test_support.h"

void test_malloc(void) {
  uint8_t *a = xmalloc(16);
  TEST_ASSERT_NOT_NULL(a);

  uint8_t *b = xmalloc(16);
  TEST_ASSERT_NOT_NULL(b);
  TEST_ASSERT_NOT_EQUAL(a, b);

  memset(a, 0xAB, 16);
  TEST_ASSERT_EQUAL_UINT8(0xAB, a[0]);
  TEST_ASSERT_EQUAL_UINT8(0xAB, a[15]);

  xfree(a);
  xfree(b);
}

void test_calloc_zeroes(void) {
  const size_t n = 4, size = 8;
  uint8_t *p = xcalloc(n, size);
  TEST_ASSERT_NOT_NULL(p);

  for (size_t i = 0; i < n * size; i++)
    TEST_ASSERT_EQUAL_UINT8(0, p[i]);

  xfree(p);
}

void test_realloc_grow(void) {
  const size_t old_size = 8;
  uint8_t *p = xmalloc(old_size);
  memset(p, 0x5A, old_size);

  const size_t new_size = 32;
  p = xrealloc(p, new_size);

  // Contents up to the old size survive the resize.
  for (size_t i = 0; i < old_size; i++)
    TEST_ASSERT_EQUAL_UINT8(0x5A, p[i]);

  memset(p + old_size, 0xC3, new_size - old_size);
  TEST_ASSERT_EQUAL_UINT8(0xC3, p[new_size - 1]);

  xfree(p);
}

void test_realloc_shrink(void) {
  uint8_t *p = xmalloc(32);
  memset(p, 1, 32);

  p = xrealloc(p, 4);
  TEST_ASSERT_NOT_NULL(p);
  TEST_ASSERT_EQUAL_UINT8(1, p[3]); // surviving prefix byte

  xfree(p);
}

void test_realloc_null_is_malloc(void) {
  uint8_t *p = xrealloc(NULL, 4);
  TEST_ASSERT_NOT_NULL(p);

  p[0] = 7;
  TEST_ASSERT_EQUAL_UINT8(7, p[0]);

  xfree(p);
}

void test_free_null_noop(void) {
  xfree(NULL); // documented: NULL is allowed and does nothing
}

static const TestDispatchEntry ENTRIES[] = {
    {"malloc", test_malloc},
    {"calloc_zeroes", test_calloc_zeroes},
    {"realloc_grow", test_realloc_grow},
    {"realloc_shrink", test_realloc_shrink},
    {"realloc_null_is_malloc", test_realloc_null_is_malloc},
    {"free_null_noop", test_free_null_noop},
};

TEST_DISPATCH_MAIN(ENTRIES)
