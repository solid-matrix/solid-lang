/**
 * @file test_source.c
 * @brief Tests for the Source line index and span accessors.
 * @author solid-matrix
 * @version 0.0.5
 */

#include <stdio.h>
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
  Source *s = source_from_cstr("a\nb\nc");
  CHECK(source_line_count(s) == 3);

  check_line(s, 0, 0, 1);
  check_line(s, 1, 2, 3);
  check_line(s, 2, 4, 5); // last line ends at text length

  Strview l0 = source_strview_at(s, source_get_line_span(s, 0));
  CHECK(strview_equals(l0, STRVIEW("a")));
  Strview l2 = source_strview_at(s, source_get_line_span(s, 2));
  CHECK(strview_equals(l2, STRVIEW("c")));

  source_destroy(s);
}

static void test_lines_crlf(void) {
  // "ab\r\ncd": CRLF is one terminator and is excluded from line content.
  Source *s = source_from_cstr("ab\r\ncd");
  CHECK(source_line_count(s) == 2);

  check_line(s, 0, 0, 2);
  check_line(s, 1, 4, 6); // last line: no terminator

  Strview l0 = source_strview_at(s, source_get_line_span(s, 0));
  CHECK(strview_equals(l0, STRVIEW("ab")));

  source_destroy(s);
}

static void test_lines_lone_cr(void) {
  // A bare '\r' also terminates a line.
  Source *s = source_from_cstr("a\rb");
  CHECK(source_line_count(s) == 2);

  check_line(s, 0, 0, 1);
  check_line(s, 1, 2, 3);

  source_destroy(s);
}

static void test_lines_mixed(void) {
  // "x\r\ny\nz\rw": all three terminator forms in one text.
  Source *s = source_from_cstr("x\r\ny\nz\rw");
  CHECK(source_line_count(s) == 4);

  check_line(s, 0, 0, 1);
  check_line(s, 1, 3, 4);
  check_line(s, 2, 5, 6);
  check_line(s, 3, 7, 8);

  source_destroy(s);
}

static void test_empty_source(void) {
  Source *s = source_from_cstr("");
  CHECK(source_line_count(s) == 1); // always at least one line

  check_line(s, 0, 0, 0);
  CHECK(span_is_empty(source_get_span(s)));

  SourcePosition p = source_get_position(s, 0);
  CHECK(p.row == 0 && p.col == 0);

  source_destroy(s);
}

static void test_bom_stripped(void) {
  const char *text = "\xEF\xBB\xBFlet";
  Source *s = source_from_cstr(text);

  // The BOM does not count toward the length or any position.
  CHECK(source_get_span(s).end == 3);
  CHECK(source_byte_at(s, 0) == 'l');

  SourcePosition p = source_get_position(s, 0);
  CHECK(p.row == 0 && p.col == 0);

  source_destroy(s);
}

static void test_get_position(void) {
  //  h  e  l  l  o  \n  w  o  r  l  d
  //  0  1  2  3  4  5   6  7  8  9  10
  Source *s = source_from_cstr("hello\nworld");

  SourcePosition p;

  p = source_get_position(s, 0);
  CHECK(p.row == 0 && p.col == 0);

  p = source_get_position(s, 4);
  CHECK(p.row == 0 && p.col == 4);

  // Offset 5 is the '\n': it still belongs to line 0 by column counting.
  p = source_get_position(s, 5);
  CHECK(p.row == 0 && p.col == 5);

  p = source_get_position(s, 6);
  CHECK(p.row == 1 && p.col == 0);

  p = source_get_position(s, 11);
  CHECK(p.row == 1 && p.col == 5);

  // One past the last byte lies on the final line.
  p = source_get_position(s, source_get_span(s).end);
  CHECK(p.row == 1 && p.col == 5);

  source_destroy(s);
}

static void test_accessors(void) {
  Source *s = source_from_cstr("hi\nyou");

  Span whole = source_get_span(s);
  CHECK(whole.start == 0 && whole.end == 6);

  CHECK(source_byte_at(s, 0) == 'h');
  CHECK(source_byte_at(s, 3) == 'y');

  CHECK(strview_equals(source_strview_at(s, whole), STRVIEW("hi\nyou")));
  CHECK(strview_equals(source_strview_at(s, source_get_line_span(s, 1)),
                  STRVIEW("you")));

  source_destroy(s);
}

static void test_owns_bytes(void) {
  // The constructors copy: mutating the caller's buffer afterwards must
  // not change the Source's text.
  uint8_t buffer[] = "hi";
  Source *strview_source = source_from_strview(strview_create(buffer, 2));
  buffer[0] = 'X';
  CHECK(source_byte_at(strview_source, 0) == 'h');
  CHECK(source_get_span(strview_source).end == 2);
  source_destroy(strview_source);

  char cstr[] = "hello";
  Source *cstr_source = source_from_cstr(cstr);
  cstr[0] = 'Y';
  CHECK(source_byte_at(cstr_source, 0) == 'h');
  source_destroy(cstr_source);
}

static void test_destroy(void) {
  // Create/destroy cycles exercise the full ownership path; sanitizer
  // builds turn leaks and double frees here into hard failures.
  for (int i = 0; i < 64; i++) {
    Source *s = source_from_cstr("abc\ndef");
    CHECK(source_byte_at(s, 0) == 'a');
    source_destroy(s);
  }
}

// Distinct name so concurrent test executables cannot collide on it.
#define TEMP_FILE "solid-lang-test-source.tmp"

static bool write_temp_file(const char *bytes, size_t len) {
  FILE *f = fopen(TEMP_FILE, "wb");
  if (f == NULL)
    return false;
  size_t n = fwrite(bytes, 1, len, f);
  fclose(f);
  return n == len;
}

static void test_from_file(void) {
  const char *text = "a\r\nb";
  CHECK(write_temp_file(text, 4));

  Source *s = source_from_file(TEMP_FILE);
  CHECK(s != NULL);
  if (s != NULL) {
    CHECK(source_line_count(s) == 2);
    check_line(s, 0, 0, 1);
    check_line(s, 1, 3, 4); // last line: no terminator

    CHECK(
        strview_equals(source_strview_at(s, source_get_span(s)), STRVIEW("a\r\nb")));

    source_destroy(s);
  }
  remove(TEMP_FILE);

  // A file that cannot be opened yields NULL instead of aborting.
  CHECK(source_from_file("solid-lang-no-such-file.tmp") == NULL);
}

static void test_from_file_bom_and_empty(void) {
  // A leading UTF-8 BOM in a file is dropped exactly as for strings.
  CHECK(write_temp_file("\xEF\xBB\xBFlet", 6));

  Source *s = source_from_file(TEMP_FILE);
  CHECK(s != NULL);
  if (s != NULL) {
    CHECK(source_get_span(s).end == 3);
    CHECK(source_byte_at(s, 0) == 'l');
    source_destroy(s);
  }
  remove(TEMP_FILE);

  // An empty file still has one (empty) line.
  CHECK(write_temp_file("", 0));
  s = source_from_file(TEMP_FILE);
  CHECK(s != NULL);
  if (s != NULL) {
    CHECK(source_line_count(s) == 1);
    CHECK(span_is_empty(source_get_span(s)));
    source_destroy(s);
  }
  remove(TEMP_FILE);
}

#undef TEMP_FILE

static const TestEntry ENTRIES[] = {
    {"lines_lf", test_lines_lf},
    {"lines_crlf", test_lines_crlf},
    {"lines_lone_cr", test_lines_lone_cr},
    {"lines_mixed", test_lines_mixed},
    {"empty_source", test_empty_source},
    {"bom_stripped", test_bom_stripped},
    {"get_position", test_get_position},
    {"accessors", test_accessors},
    {"owns_bytes", test_owns_bytes},
    {"destroy", test_destroy},
    {"from_file", test_from_file},
    {"from_file_bom_and_empty", test_from_file_bom_and_empty},
};

TEST_MAIN(ENTRIES)
