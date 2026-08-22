/**
 * @file test_source.c
 * @brief Tests for the Source line index and span accessors.
 * @author solid-matrix
 * @version 0.0.5
 */

#include <string.h>

#include "source.h"
#include "test_util.h"

static void check_line(Source *s, size_t line, size_t want_start,
                       size_t want_end) {
  CHECK(source_get_line_start(s, line) == want_start);
  CHECK(source_get_line_end(s, line) == want_end);
  Span sp = source_get_line_span(s, line);
  CHECK(sp.start == want_start && sp.end == want_end);
}

static void test_lines_lf(void) {
  // "a\nb\nc": three lines, the last without a terminator.
  Source s = source_from_cstr("a\nb\nc");
  CHECK(s.line_count == 3);

  check_line(&s, 0, 0, 1);
  check_line(&s, 1, 2, 3);
  check_line(&s, 2, 4, 5); // last line ends at text length

  StringView l0 = source_string_view_at(&s, source_get_line_span(&s, 0));
  CHECK(sv_equals(l0, SV("a")));
  StringView l2 = source_string_view_at(&s, source_get_line_span(&s, 2));
  CHECK(sv_equals(l2, SV("c")));

  source_destroy(&s);
}

static void test_lines_crlf(void) {
  // "ab\r\ncd": CRLF is one terminator and is excluded from line content.
  Source s = source_from_cstr("ab\r\ncd");
  CHECK(s.line_count == 2);

  check_line(&s, 0, 0, 2);
  check_line(&s, 1, 4, 6); // last line: no terminator

  StringView l0 = source_string_view_at(&s, source_get_line_span(&s, 0));
  CHECK(sv_equals(l0, SV("ab")));

  source_destroy(&s);
}

static void test_lines_lone_cr(void) {
  // A bare '\r' also terminates a line.
  Source s = source_from_cstr("a\rb");
  CHECK(s.line_count == 2);

  check_line(&s, 0, 0, 1);
  check_line(&s, 1, 2, 3);

  source_destroy(&s);
}

static void test_lines_mixed(void) {
  // "x\r\ny\nz\rw": all three terminator forms in one text.
  Source s = source_from_cstr("x\r\ny\nz\rw");
  CHECK(s.line_count == 4);

  check_line(&s, 0, 0, 1);
  check_line(&s, 1, 3, 4);
  check_line(&s, 2, 5, 6);
  check_line(&s, 3, 7, 8);

  source_destroy(&s);
}

static void test_empty_source(void) {
  Source s = source_from_cstr("");
  CHECK(s.line_count == 1); // always at least one line

  check_line(&s, 0, 0, 0);
  CHECK(span_is_empty(source_get_span(&s)));

  SourcePosition p = source_get_position(&s, 0);
  CHECK(p.row == 0 && p.col == 0);

  source_destroy(&s);
}

static void test_bom_stripped(void) {
  const char *text = "\xEF\xBB\xBFlet";
  Source s = source_from_cstr(text);

  // The BOM does not count toward the length or any position.
  CHECK(source_get_span(&s).end == 3);
  CHECK(source_byte_at(&s, 0) == 'l');

  SourcePosition p = source_get_position(&s, 0);
  CHECK(p.row == 0 && p.col == 0);

  source_destroy(&s);
}

static void test_get_position(void) {
  //  h  e  l  l  o  \n  w  o  r  l  d
  //  0  1  2  3  4  5   6  7  8  9  10
  Source s = source_from_cstr("hello\nworld");

  SourcePosition p;

  p = source_get_position(&s, 0);
  CHECK(p.row == 0 && p.col == 0);

  p = source_get_position(&s, 4);
  CHECK(p.row == 0 && p.col == 4);

  // Offset 5 is the '\n': it still belongs to line 0 by column counting.
  p = source_get_position(&s, 5);
  CHECK(p.row == 0 && p.col == 5);

  p = source_get_position(&s, 6);
  CHECK(p.row == 1 && p.col == 0);

  p = source_get_position(&s, 11);
  CHECK(p.row == 1 && p.col == 5);

  // One past the last byte lies on the final line.
  p = source_get_position(&s, source_get_span(&s).end);
  CHECK(p.row == 1 && p.col == 5);

  source_destroy(&s);
}

static void test_accessors(void) {
  Source s = source_from_cstr("hi\nyou");

  Span whole = source_get_span(&s);
  CHECK(whole.start == 0 && whole.end == 6);

  CHECK(source_byte_at(&s, 0) == 'h');
  CHECK(source_byte_at(&s, 3) == 'y');

  CHECK(sv_equals(source_string_view_at(&s, whole), SV("hi\nyou")));
  CHECK(sv_equals(source_string_view_at(&s, source_get_line_span(&s, 1)),
                  SV("you")));

  source_destroy(&s);
}

static void test_destroy(void) {
  Source s = source_from_cstr("abc");
  source_destroy(&s);
  CHECK(s.line_offsets == NULL);
  CHECK(s.line_count == 0);
}

static const TestEntry ENTRIES[] = {
    {"lines_lf", test_lines_lf},
    {"lines_crlf", test_lines_crlf},
    {"lines_lone_cr", test_lines_lone_cr},
    {"lines_mixed", test_lines_mixed},
    {"empty_source", test_empty_source},
    {"bom_stripped", test_bom_stripped},
    {"get_position", test_get_position},
    {"accessors", test_accessors},
    {"destroy", test_destroy},
};

TEST_MAIN(ENTRIES)
