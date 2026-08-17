/**
 * @file test_source.c
 * @brief Tests for the Source line index and SourceSpan behavior.
 * @author solid-matrix
 * @version 0.0.5
 */

#include <stdio.h>
#include <string.h>

#include "source.h"

static int g_failures;

#define CHECK(cond)                                                    \
  do {                                                                 \
    if (!(cond)) {                                                     \
      fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);  \
      g_failures++;                                                    \
    }                                                                  \
  } while (0)

static void check_span_text(SourceSpan span, const char *expected)
{
  CHECK(sv_equals(span_to_string_view(span), sv_from_cstr(expected)));
}

int main(void)
{
  // LF: trailing newline yields a trailing empty line
  Source s = source_from_cstr("hello\nworld\n");
  CHECK(s.line_count == 3);
  CHECK(source_get_line_start(&s, 0) == 0);
  CHECK(source_get_line_end(&s, 0) == 5);
  CHECK(source_get_line_start(&s, 1) == 6);
  CHECK(source_get_line_end(&s, 1) == 11);
  CHECK(source_get_line_start(&s, 2) == 12);
  CHECK(source_get_line_end(&s, 2) == 12);
  check_span_text(source_get_line_span(&s, 0), "hello");
  check_span_text(source_get_line_span(&s, 1), "world");
  CHECK(span_is_empty(source_get_line_span(&s, 2)));

  // Position queries: line start, mid-line, trailing empty line
  Position p = source_get_position(&s, 0);
  CHECK(p.row == 0 && p.col == 0);
  p = source_get_position(&s, 7);
  CHECK(p.row == 1 && p.col == 1);
  p = source_get_position(&s, 12);
  CHECK(p.row == 2 && p.col == 0);

  // Whole-source span and relative slicing
  SourceSpan whole = source_to_span(&s);
  CHECK(span_len(whole) == 12);
  SourceSpan sub = span_slice(whole, 6, 11);
  check_span_text(sub, "world");
  CHECK(span_get_char(sub, 0) == 'w');
  CHECK(span_get_char(sub, 4) == 'd');
  CHECK(span_is_empty(span_slice(whole, 0, 0)));
  source_destroy(&s);

  // CR: a lone '\r' is also a line terminator
  Source cr = source_from_cstr("a\rb");
  CHECK(cr.line_count == 2);
  check_span_text(source_get_line_span(&cr, 0), "a");
  check_span_text(source_get_line_span(&cr, 1), "b");
  source_destroy(&cr);

  // CRLF: "\r\n" is a single terminator, excluded from line content
  Source crlf = source_from_cstr("hello\r\nworld");
  CHECK(crlf.line_count == 2);
  CHECK(source_get_line_start(&crlf, 1) == 7);
  check_span_text(source_get_line_span(&crlf, 0), "hello");
  check_span_text(source_get_line_span(&crlf, 1), "world");
  source_destroy(&crlf);

  // A bare "\r\n": two empty lines
  Source only = source_from_cstr("\r\n");
  CHECK(only.line_count == 2);
  CHECK(span_is_empty(source_get_line_span(&only, 0)));
  CHECK(span_is_empty(source_get_line_span(&only, 1)));
  source_destroy(&only);

  // Mixed terminators: a\rb\nc\r\nd -> 4 lines
  Source mix = source_from_cstr("a\rb\nc\r\nd");
  CHECK(mix.line_count == 4);
  CHECK(source_get_line_start(&mix, 0) == 0);
  CHECK(source_get_line_start(&mix, 1) == 2);
  CHECK(source_get_line_start(&mix, 2) == 4);
  CHECK(source_get_line_start(&mix, 3) == 7);
  check_span_text(source_get_line_span(&mix, 0), "a");
  check_span_text(source_get_line_span(&mix, 1), "b");
  check_span_text(source_get_line_span(&mix, 2), "c");
  check_span_text(source_get_line_span(&mix, 3), "d");
  p = source_get_position(&mix, 7);
  CHECK(p.row == 3 && p.col == 0);
  source_destroy(&mix);

  // Empty text: treated as a single line
  Source empty = source_from_cstr("");
  CHECK(empty.line_count == 1);
  CHECK(source_get_position(&empty, 0).row == 0);
  CHECK(span_is_empty(source_to_span(&empty)));
  source_destroy(&empty);

  // StringView-based constructor
  StringView sv = sv_from_cstr("abc\n");
  Source from_sv = source_from_string_view(sv);
  CHECK(from_sv.line_count == 2);
  check_span_text(source_get_line_span(&from_sv, 0), "abc");
  source_destroy(&from_sv);

  if (g_failures == 0) {
    printf("test_source: all ok\n");
    return 0;
  }
  fprintf(stderr, "test_source: %d failure(s)\n", g_failures);
  return 1;
}
