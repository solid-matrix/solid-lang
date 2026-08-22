/**
 * @file test_span.c
 * @brief Tests for the Span operations.
 * @author solid-matrix
 * @version 0.0.5
 */

#include "span.h"
#include "test_util.h"

static void test_empty(void) {
  Span e = span_empty();
  CHECK(e.start == 0 && e.end == 0);
  CHECK(span_len(e) == 0);
  CHECK(span_is_empty(e));
}

static void test_len_and_is_empty(void) {
  Span sp = {.start = 2, .end = 7};
  CHECK(span_len(sp) == 5);
  CHECK(!span_is_empty(sp));

  Span zero = {.start = 9, .end = 9};
  CHECK(span_len(zero) == 0);
  CHECK(span_is_empty(zero));
}

static void test_slice(void) {
  Span sp = {.start = 2, .end = 7}; // len 5

  // Relative offsets map to absolute ones.
  Span sub = span_slice(sp, 1, 4);
  CHECK(sub.start == 3 && sub.end == 6);
  CHECK(span_len(sub) == 3);

  // Zero-length slice at a relative position keeps absolute placement.
  Span z = span_slice(sp, 2, 2);
  CHECK(z.start == 4 && z.end == 4);
  CHECK(span_is_empty(z));

  // Full-length slice reproduces the original.
  Span full = span_slice(sp, 0, span_len(sp));
  CHECK(full.start == sp.start && full.end == sp.end);

  // Slices of slices stay within the original range.
  Span deep = span_slice(sub, 1, span_len(sub));
  CHECK(deep.start == 4 && deep.end == 6);
}

static const TestEntry ENTRIES[] = {
    {"empty", test_empty},
    {"len_and_is_empty", test_len_and_is_empty},
    {"slice", test_slice},
};

TEST_MAIN(ENTRIES)
