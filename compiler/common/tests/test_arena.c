#include <stdint.h>
#include <string.h>

#include "arena.h"
#include "test_support.h"

void test_alloc_distinct_and_writable(void) {
  Arena *a = arena_create();
  TEST_ASSERT_NOT_NULL(a);

  uint8_t *x = arena_alloc(a, 16);
  uint8_t *y = arena_alloc(a, 16);
  TEST_ASSERT_NOT_NULL(x);
  TEST_ASSERT_NOT_NULL(y);
  TEST_ASSERT_NOT_EQUAL(x, y);

  memset(x, 0xAA, 16);
  memset(y, 0xBB, 16);
  TEST_ASSERT_EQUAL_UINT8(0xAA, x[15]);
  TEST_ASSERT_EQUAL_UINT8(0xBB, y[15]);

  arena_destroy(a);
}

void test_realloc_grow_preserves_content(void) {
  Arena *a = arena_create();

  uint8_t *p = arena_alloc(a, 8);
  memset(p, 0x5A, 8);

  p = arena_realloc(a, p, 32);
  for (size_t i = 0; i < 8; i++)
    TEST_ASSERT_EQUAL_UINT8(0x5A, p[i]);

  memset(p + 8, 0xC3, 24); // grown tail is writable
  TEST_ASSERT_EQUAL_UINT8(0xC3, p[31]);

  arena_destroy(a);
}

void test_realloc_null_is_alloc(void) {
  Arena *a = arena_create();
  uint8_t *p = arena_realloc(a, NULL, 4);
  TEST_ASSERT_NOT_NULL(p);
  p[0] = 7;
  TEST_ASSERT_EQUAL_UINT8(7, p[0]);
  arena_destroy(a);
}

void test_destroy_cycles(void) {
  // Sanitizer builds turn leaks into failures here.
  for (int i = 0; i < 64; i++) {
    Arena *a = arena_create();
    TEST_ASSERT_NOT_NULL(arena_alloc(a, 64));
    arena_destroy(a);
  }
}

void test_destroy_null_safe(void) {
  arena_destroy(NULL);
}

static const TestDispatchEntry ENTRIES[] = {
    {"alloc_distinct_and_writable", test_alloc_distinct_and_writable},
    {"realloc_grow_preserves_content", test_realloc_grow_preserves_content},
    {"realloc_null_is_alloc", test_realloc_null_is_alloc},
    {"destroy_cycles", test_destroy_cycles},
    {"destroy_null_safe", test_destroy_null_safe},
};

TEST_DISPATCH_MAIN(ENTRIES)
