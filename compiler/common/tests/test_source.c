/**
 * @file test_source.c
 * @brief Tests for the Source line index and Span accessors.
 * @author solid-matrix
 * @version 0.0.5
 */

#include <stdio.h>
#include <string.h>

#include "source.h"

static int g_failures;

#define CHECK(cond)                                                   \
  do                                                                  \
  {                                                                   \
    if (!(cond))                                                      \
    {                                                                 \
      fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      g_failures++;                                                   \
    }                                                                 \
  } while (0)

static void check_span_text(const Source *src, Span span, const char *expected)
{
  CHECK(sv_equals(source_string_view_at(src, span), sv_from_cstr(expected)));
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
  check_span_text(&s, source_get_line_span(&s, 0), "hello");
  check_span_text(&s, source_get_line_span(&s, 1), "world");
  CHECK(span_is_empty(source_get_line_span(&s, 2)));

  // Position queries: line start, mid-line, trailing empty line
  Position p = source_get_position(&s, 0);
  CHECK(p.row == 0 && p.col == 0);
  p = source_get_position(&s, 7);
  CHECK(p.row == 1 && p.col == 1);
  p = source_get_position(&s, 12);
  CHECK(p.row == 2 && p.col == 0);

  // Whole-source span and relative slicing
  Span whole = source_get_span(&s);
  CHECK(span_len(whole) == 12);
  Span sub = span_slice(whole, 6, 11);
  check_span_text(&s, sub, "world");
  CHECK(source_byte_at(&s, 6) == 'w');
  CHECK(source_byte_at(&s, 10) == 'd');
  CHECK(span_is_empty(span_slice(whole, 0, 0)));
  source_destroy(&s);

  // CR: a lone '\r' is also a line terminator
  Source cr = source_from_cstr("a\rb");
  CHECK(cr.line_count == 2);
  check_span_text(&cr, source_get_line_span(&cr, 0), "a");
  check_span_text(&cr, source_get_line_span(&cr, 1), "b");
  source_destroy(&cr);

  // CRLF: "\r\n" is a single terminator, excluded from line content
  Source crlf = source_from_cstr("hello\r\nworld");
  CHECK(crlf.line_count == 2);
  CHECK(source_get_line_start(&crlf, 1) == 7);
  check_span_text(&crlf, source_get_line_span(&crlf, 0), "hello");
  check_span_text(&crlf, source_get_line_span(&crlf, 1), "world");
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
  check_span_text(&mix, source_get_line_span(&mix, 0), "a");
  check_span_text(&mix, source_get_line_span(&mix, 1), "b");
  check_span_text(&mix, source_get_line_span(&mix, 2), "c");
  check_span_text(&mix, source_get_line_span(&mix, 3), "d");
  p = source_get_position(&mix, 7);
  CHECK(p.row == 3 && p.col == 0);
  source_destroy(&mix);

  // Empty text: treated as a single line
  Source empty = source_from_cstr("");
  CHECK(empty.line_count == 1);
  CHECK(source_get_position(&empty, 0).row == 0);
  CHECK(span_is_empty(source_get_span(&empty)));
  source_destroy(&empty);

  // StringView-based constructor
  StringView sv = sv_from_cstr("abc\n");
  Source from_sv = source_from_string_view(sv);
  CHECK(from_sv.line_count == 2);
  check_span_text(&from_sv, source_get_line_span(&from_sv, 0), "abc");
  source_destroy(&from_sv);

  // UTF-8 BOM at the front is dropped by the Source constructor.
  const uint8_t with_bom[] = {0xEF, 0xBB, 0xBF, 'l', 'e', 't', ' ', 'x'};
  Source s_bom = source_from_string_view(sv_create(with_bom, sizeof(with_bom)));
  CHECK(s_bom.line_count == 1);
  CHECK(s_bom.string_view.len == 5);
  CHECK(source_byte_at(&s_bom, 0) == 'l');
  CHECK(source_get_position(&s_bom, 0).col == 0);
  check_span_text(&s_bom, source_get_span(&s_bom), "let x");
  source_destroy(&s_bom);

  // Control: without a BOM the text is indexed unchanged.
  Source s_plain = source_from_cstr("let x");
  CHECK(s_plain.string_view.len == 5);
  CHECK(source_byte_at(&s_plain, 0) == 'l');
  check_span_text(&s_plain, source_get_span(&s_plain), "let x");
  source_destroy(&s_plain);

  // BOM only (no content) yields an empty, non-crashing source.
  const uint8_t bom_only[] = {0xEF, 0xBB, 0xBF};
  Source s_bom_only = source_from_string_view(sv_create(bom_only, sizeof(bom_only)));
  CHECK(s_bom_only.line_count == 1);
  CHECK(span_is_empty(source_get_span(&s_bom_only)));
  source_destroy(&s_bom_only);

  if (g_failures == 0)
  {
    printf("test_source: all ok\n");
    return 0;
  }
  fprintf(stderr, "test_source: %d failure(s)\n", g_failures);
  return 1;
}
