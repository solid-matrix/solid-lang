#include <stdio.h>
#include <string.h>

#include "source.h"
#include "test_support.h"

static void check_line(Source *s, size_t line, size_t want_start,
                       size_t want_end) {
  TEST_ASSERT_EQUAL_size_t(want_start, source_get_line_start(s, line));
  TEST_ASSERT_EQUAL_size_t(want_end, source_get_line_end(s, line));
  Span sp = source_get_line_span(s, line);
  TEST_ASSERT_EQUAL_size_t(want_start, sp.start);
  TEST_ASSERT_EQUAL_size_t(want_end, sp.end);
}

void test_lines_lf(void) {
  Source *s = source_from_cstr("a\nb\nc");
  TEST_ASSERT_EQUAL_size_t(3, source_line_count(s));

  check_line(s, 0, 0, 1);
  check_line(s, 1, 2, 3);
  check_line(s, 2, 4, 5); // last line ends at text length

  TEST_ASSERT_STRVIEW_EQ(
      source_strview_at(s, source_get_line_span(s, 0)), "a");
  TEST_ASSERT_STRVIEW_EQ(
      source_strview_at(s, source_get_line_span(s, 2)), "c");

  source_destroy(s);
}

void test_lines_crlf(void) {
  // CRLF is one terminator and is excluded from line content.
  Source *s = source_from_cstr("ab\r\ncd");
  TEST_ASSERT_EQUAL_size_t(2, source_line_count(s));

  check_line(s, 0, 0, 2);
  check_line(s, 1, 4, 6);

  TEST_ASSERT_STRVIEW_EQ(
      source_strview_at(s, source_get_line_span(s, 0)), "ab");

  source_destroy(s);
}

void test_lines_lone_cr(void) {
  // A bare '\r' also terminates a line.
  Source *s = source_from_cstr("a\rb");
  TEST_ASSERT_EQUAL_size_t(2, source_line_count(s));

  check_line(s, 0, 0, 1);
  check_line(s, 1, 2, 3);

  source_destroy(s);
}

void test_lines_mixed(void) {
  Source *s = source_from_cstr("x\r\ny\nz\rw");
  TEST_ASSERT_EQUAL_size_t(4, source_line_count(s));

  check_line(s, 0, 0, 1);
  check_line(s, 1, 3, 4);
  check_line(s, 2, 5, 6);
  check_line(s, 3, 7, 8);

  source_destroy(s);
}

void test_empty_source(void) {
  Source *s = source_from_cstr("");
  TEST_ASSERT_EQUAL_size_t(1, source_line_count(s)); // always one line

  check_line(s, 0, 0, 0);
  TEST_ASSERT_TRUE(span_is_empty(source_get_span(s)));

  SourcePosition p = source_get_position(s, 0);
  TEST_ASSERT_EQUAL_size_t(0, p.row);
  TEST_ASSERT_EQUAL_size_t(0, p.col);

  source_destroy(s);
}

void test_bom_stripped(void) {
  const char *text = "\xEF\xBB\xBFlet";
  Source *s = source_from_cstr(text);

  // The BOM does not count toward the length or any position.
  TEST_ASSERT_EQUAL_size_t(3, source_get_span(s).end);
  TEST_ASSERT_EQUAL_UINT8('l', source_byte_at(s, 0));

  SourcePosition p = source_get_position(s, 0);
  TEST_ASSERT_EQUAL_size_t(0, p.row);
  TEST_ASSERT_EQUAL_size_t(0, p.col);

  source_destroy(s);
}

void test_get_position(void) {
  //  h  e  l  l  o  \n  w  o  r  l  d
  //  0  1  2  3  4  5   6  7  8  9  10
  Source *s = source_from_cstr("hello\nworld");

  SourcePosition p = source_get_position(s, 0);
  TEST_ASSERT_EQUAL_size_t(0, p.row);
  TEST_ASSERT_EQUAL_size_t(0, p.col);

  p = source_get_position(s, 4);
  TEST_ASSERT_EQUAL_size_t(0, p.row);
  TEST_ASSERT_EQUAL_size_t(4, p.col);

  // Offset 5 is the '\n': it still belongs to line 0 by column counting.
  p = source_get_position(s, 5);
  TEST_ASSERT_EQUAL_size_t(0, p.row);
  TEST_ASSERT_EQUAL_size_t(5, p.col);

  p = source_get_position(s, 6);
  TEST_ASSERT_EQUAL_size_t(1, p.row);
  TEST_ASSERT_EQUAL_size_t(0, p.col);

  p = source_get_position(s, 11);
  TEST_ASSERT_EQUAL_size_t(1, p.row);
  TEST_ASSERT_EQUAL_size_t(5, p.col);

  // One past the last byte lies on the final line.
  p = source_get_position(s, source_get_span(s).end);
  TEST_ASSERT_EQUAL_size_t(1, p.row);
  TEST_ASSERT_EQUAL_size_t(5, p.col);

  source_destroy(s);
}

void test_accessors(void) {
  Source *s = source_from_cstr("hi\nyou");

  Span whole = source_get_span(s);
  TEST_ASSERT_EQUAL_size_t(0, whole.start);
  TEST_ASSERT_EQUAL_size_t(6, whole.end);

  TEST_ASSERT_EQUAL_UINT8('h', source_byte_at(s, 0));
  TEST_ASSERT_EQUAL_UINT8('y', source_byte_at(s, 3));

  TEST_ASSERT_STRVIEW_EQ(source_strview_at(s, whole), "hi\nyou");
  TEST_ASSERT_STRVIEW_EQ(
      source_strview_at(s, source_get_line_span(s, 1)), "you");

  source_destroy(s);
}

void test_owns_bytes(void) {
  // The constructors copy: mutating the caller's buffer afterwards must
  // not change the Source's text.
  uint8_t buffer[] = "hi";
  Source *view_source = source_from_strview(strview_create(buffer, 2));
  buffer[0] = 'X';
  TEST_ASSERT_EQUAL_UINT8('h', source_byte_at(view_source, 0));
  TEST_ASSERT_EQUAL_size_t(2, source_get_span(view_source).end);
  source_destroy(view_source);

  char cstr[] = "hello";
  Source *cstr_source = source_from_cstr(cstr);
  cstr[0] = 'Y';
  TEST_ASSERT_EQUAL_UINT8('h', source_byte_at(cstr_source, 0));
  source_destroy(cstr_source);
}

void test_destroy_cycles(void) {
  // Sanitizer builds turn leaks and double frees here into failures.
  for (int i = 0; i < 64; i++) {
    Source *s = source_from_cstr("abc\ndef");
    TEST_ASSERT_EQUAL_UINT8('a', source_byte_at(s, 0));
    source_destroy(s);
  }
}

#define TEMP_FILE "solid-lang-test-source.tmp"

static bool write_temp_file(const char *bytes, size_t len) {
  FILE *f = fopen(TEMP_FILE, "wb");
  if (f == NULL)
    return false;
  size_t n = fwrite(bytes, 1, len, f);
  fclose(f);
  return n == len;
}

void test_from_file(void) {
  const char *text = "a\r\nb";
  TEST_ASSERT_TRUE(write_temp_file(text, 4));

  Source *s = source_from_file(TEMP_FILE);
  TEST_ASSERT_NOT_NULL(s);
  if (s != NULL) {
    TEST_ASSERT_EQUAL_size_t(2, source_line_count(s));
    check_line(s, 0, 0, 1);
    check_line(s, 1, 3, 4);

    TEST_ASSERT_STRVIEW_EQ(source_strview_at(s, source_get_span(s)),
                           "a\r\nb");
    source_destroy(s);
  }
  remove(TEMP_FILE);

  // A file that cannot be opened yields NULL instead of aborting.
  TEST_ASSERT_NULL(source_from_file("solid-lang-no-such-file.tmp"));
}

void test_from_file_bom_and_empty(void) {
  // A leading UTF-8 BOM in a file is dropped exactly as for strings.
  TEST_ASSERT_TRUE(write_temp_file("\xEF\xBB\xBFlet", 6));

  Source *s = source_from_file(TEMP_FILE);
  TEST_ASSERT_NOT_NULL(s);
  if (s != NULL) {
    TEST_ASSERT_EQUAL_size_t(3, source_get_span(s).end);
    TEST_ASSERT_EQUAL_UINT8('l', source_byte_at(s, 0));
    source_destroy(s);
  }
  remove(TEMP_FILE);

  // An empty file still has one (empty) line.
  TEST_ASSERT_TRUE(write_temp_file("", 0));
  s = source_from_file(TEMP_FILE);
  TEST_ASSERT_NOT_NULL(s);
  if (s != NULL) {
    TEST_ASSERT_EQUAL_size_t(1, source_line_count(s));
    TEST_ASSERT_TRUE(span_is_empty(source_get_span(s)));
    source_destroy(s);
  }
  remove(TEMP_FILE);
}

#undef TEMP_FILE

static const TestDispatchEntry ENTRIES[] = {
    {"lines_lf", test_lines_lf},
    {"lines_crlf", test_lines_crlf},
    {"lines_lone_cr", test_lines_lone_cr},
    {"lines_mixed", test_lines_mixed},
    {"empty_source", test_empty_source},
    {"bom_stripped", test_bom_stripped},
    {"get_position", test_get_position},
    {"accessors", test_accessors},
    {"owns_bytes", test_owns_bytes},
    {"destroy_cycles", test_destroy_cycles},
    {"from_file", test_from_file},
    {"from_file_bom_and_empty", test_from_file_bom_and_empty},
};

TEST_DISPATCH_MAIN(ENTRIES)
