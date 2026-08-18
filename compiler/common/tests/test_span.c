/**
 * @file test_span.c
 * @brief Tests for the Span operations.
 * @author solid-matrix
 * @version 0.0.5
 */

#include <stdio.h>

#include "span.h"

static int g_failures;

#define CHECK(cond)                                                    \
  do {                                                                 \
    if (!(cond)) {                                                     \
      fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);  \
      g_failures++;                                                    \
    }                                                                  \
  } while (0)

int main(void)
{
  // Empty span
  Span e = span_empty();
  CHECK(e.start == 0 && e.end == 0);
  CHECK(span_len(e) == 0);
  CHECK(span_is_empty(e));

  // Length and emptiness of a non-empty span
  Span sp = {.start = 2, .end = 7};
  CHECK(span_len(sp) == 5);
  CHECK(!span_is_empty(sp));

  // Relative slicing
  Span sub = span_slice(sp, 1, 4);
  CHECK(sub.start == 3 && sub.end == 6);
  CHECK(span_len(sub) == 3);

  // Full-length and zero-length slices
  Span full = span_slice(sp, 0, span_len(sp));
  CHECK(full.start == 2 && full.end == 7);
  CHECK(span_is_empty(span_slice(sp, 3, 3)));

  if (g_failures == 0) {
    printf("test_span: all ok\n");
    return 0;
  }
  fprintf(stderr, "test_span: %d failure(s)\n", g_failures);
  return 1;
}
