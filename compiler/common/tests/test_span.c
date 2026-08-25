#include "span.h"
#include "test_support.h"

void test_empty(void) {
  Span e = span_empty();
  TEST_ASSERT_EQUAL_size_t(0, e.start);
  TEST_ASSERT_EQUAL_size_t(0, e.end);
  TEST_ASSERT_EQUAL_size_t(0, span_len(e));
  TEST_ASSERT_TRUE(span_is_empty(e));
}

void test_len_and_is_empty(void) {
  Span sp = {.start = 2, .end = 7};
  TEST_ASSERT_EQUAL_size_t(5, span_len(sp));
  TEST_ASSERT_FALSE(span_is_empty(sp));

  Span zero = {.start = 9, .end = 9};
  TEST_ASSERT_EQUAL_size_t(0, span_len(zero));
  TEST_ASSERT_TRUE(span_is_empty(zero));
}

void test_slice(void) {
  Span sp = {.start = 2, .end = 7}; // len 5

  // Relative offsets map to absolute ones.
  Span sub = span_slice(sp, 1, 4);
  TEST_ASSERT_EQUAL_size_t(3, sub.start);
  TEST_ASSERT_EQUAL_size_t(6, sub.end);
  TEST_ASSERT_EQUAL_size_t(3, span_len(sub));

  // Zero-length slice at a relative position keeps absolute placement.
  Span z = span_slice(sp, 2, 2);
  TEST_ASSERT_EQUAL_size_t(4, z.start);
  TEST_ASSERT_EQUAL_size_t(4, z.end);
  TEST_ASSERT_TRUE(span_is_empty(z));

  // Full-length slice reproduces the original.
  Span full = span_slice(sp, 0, span_len(sp));
  TEST_ASSERT_EQUAL_size_t(sp.start, full.start);
  TEST_ASSERT_EQUAL_size_t(sp.end, full.end);

  // Slices of slices stay within the original range.
  Span deep = span_slice(sub, 1, span_len(sub));
  TEST_ASSERT_EQUAL_size_t(4, deep.start);
  TEST_ASSERT_EQUAL_size_t(6, deep.end);
}

static const TestDispatchEntry ENTRIES[] = {
    {"empty", test_empty},
    {"len_and_is_empty", test_len_and_is_empty},
    {"slice", test_slice},
};

TEST_DISPATCH_MAIN(ENTRIES)
