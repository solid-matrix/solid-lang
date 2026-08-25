#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "strview.h"
#include "test_support.h"

void test_create_macro(void) {
  Strview v = STRVIEW("hello");
  TEST_ASSERT_EQUAL_size_t(5, v.len);
  TEST_ASSERT_EQUAL_UINT8('h', v.data[0]);
  TEST_ASSERT_EQUAL_UINT8('o', v.data[4]);

  Strview c = STRVIEW("a"
                      "b");
  TEST_ASSERT_EQUAL_size_t(2, c.len);
  TEST_ASSERT_TRUE(strview_equals(c, STRVIEW("ab")));

  Strview e = STRVIEW("");
  TEST_ASSERT_EQUAL_size_t(0, e.len);

  // Views never copy: writes to the buffer are visible through the view.
  char buf[3] = {'a', 'b', 'c'};
  Strview alias = strview_create((const uint8_t *)buf, 3);
  buf[0] = 'X';
  TEST_ASSERT_EQUAL_UINT8('X', strview_byte_at(alias, 0));
}

void test_create_functions(void) {
  Strview v = strview_create((const uint8_t *)"xyz", 3);
  TEST_ASSERT_EQUAL_size_t(3, v.len);
  TEST_ASSERT_EQUAL_UINT8('x', v.data[0]);

  Strview e = strview_empty();
  TEST_ASSERT_NULL(e.data);
  TEST_ASSERT_EQUAL_size_t(0, e.len);

  Strview s = strview_from_cstr("str");
  TEST_ASSERT_EQUAL_size_t(3, s.len);
  TEST_ASSERT_TRUE(strview_equals(s, STRVIEW("str")));
}

void test_is_empty(void) {
  TEST_ASSERT_TRUE(strview_is_empty(strview_empty()));
  TEST_ASSERT_FALSE(strview_is_empty(STRVIEW("a")));
}

void test_equals(void) {
  TEST_ASSERT_TRUE(strview_equals(STRVIEW("same"), STRVIEW("same")));
  TEST_ASSERT_FALSE(strview_equals(STRVIEW("same"), STRVIEW("sane")));
  TEST_ASSERT_FALSE(strview_equals(STRVIEW("short"), STRVIEW("shorter")));
  TEST_ASSERT_FALSE(strview_equals(STRVIEW("shorter"), STRVIEW("short")));

  // Equality is length-bounded: embedded NUL bytes are just bytes.
  uint8_t buf[3] = {'a', '\0', 'b'};
  Strview v = strview_create(buf, 3);
  TEST_ASSERT_TRUE(strview_equals(v, v));
  TEST_ASSERT_FALSE(strview_equals(v, STRVIEW("a")));
  TEST_ASSERT_TRUE(strview_equals(v, strview_slice(v, 0, 3)));

  TEST_ASSERT_TRUE(strview_equals(strview_empty(), STRVIEW("")));
}

void test_compare(void) {
  TEST_ASSERT_LESS_THAN_INT(0, strview_compare(STRVIEW("a"), STRVIEW("b")));
  TEST_ASSERT_GREATER_THAN_INT(0,
                               strview_compare(STRVIEW("b"), STRVIEW("a")));
  TEST_ASSERT_EQUAL_INT(0, strview_compare(STRVIEW("x"), STRVIEW("x")));

  // A proper prefix compares less than its extension.
  TEST_ASSERT_LESS_THAN_INT(0, strview_compare(STRVIEW("ab"), STRVIEW("abc")));
  TEST_ASSERT_GREATER_THAN_INT(
      0, strview_compare(STRVIEW("abc"), STRVIEW("ab")));

  TEST_ASSERT_EQUAL_INT(0, strview_compare(strview_empty(), strview_empty()));
  TEST_ASSERT_LESS_THAN_INT(0, strview_compare(strview_empty(), STRVIEW("a")));
}

void test_starts_with(void) {
  TEST_ASSERT_TRUE(strview_starts_with(STRVIEW("hello"), STRVIEW("he")));
  TEST_ASSERT_TRUE(strview_starts_with(STRVIEW("hello"), STRVIEW("hello")));
  TEST_ASSERT_TRUE(strview_starts_with(STRVIEW("hello"), STRVIEW("")));
  TEST_ASSERT_FALSE(strview_starts_with(STRVIEW("hi"), STRVIEW("hello")));
  TEST_ASSERT_FALSE(strview_starts_with(STRVIEW("hello"), STRVIEW("he!")));
  TEST_ASSERT_TRUE(strview_starts_with(strview_empty(), STRVIEW("")));
}

void test_ends_with(void) {
  TEST_ASSERT_TRUE(strview_ends_with(STRVIEW("hello"), STRVIEW("lo")));
  TEST_ASSERT_TRUE(strview_ends_with(STRVIEW("hello"), STRVIEW("hello")));
  TEST_ASSERT_TRUE(strview_ends_with(STRVIEW("hello"), STRVIEW("")));
  TEST_ASSERT_FALSE(strview_ends_with(STRVIEW("lo"), STRVIEW("hello")));
  TEST_ASSERT_FALSE(strview_ends_with(STRVIEW("hello"), STRVIEW("lo!")));
  TEST_ASSERT_TRUE(strview_ends_with(strview_empty(), STRVIEW("")));
}

void test_slice(void) {
  Strview base = STRVIEW("abcdef");

  Strview mid = strview_slice(base, 1, 4);
  TEST_ASSERT_EQUAL_size_t(4, mid.len);
  TEST_ASSERT_EQUAL_PTR(base.data + 1, mid.data);
  TEST_ASSERT_TRUE(strview_equals(mid, STRVIEW("bcde")));

  // A zero-length slice keeps pointing into the source buffer.
  Strview z = strview_slice(base, 3, 0);
  TEST_ASSERT_EQUAL_size_t(0, z.len);
  TEST_ASSERT_EQUAL_PTR(base.data + 3, z.data);

  Strview tail = strview_slice(base, base.len, 0);
  TEST_ASSERT_EQUAL_size_t(0, tail.len);
  TEST_ASSERT_EQUAL_PTR(base.data + base.len, tail.data);

  TEST_ASSERT_TRUE(strview_equals(strview_slice(base, 0, base.len), base));
}

void test_byte_at(void) {
  Strview v = STRVIEW("abc");
  TEST_ASSERT_EQUAL_UINT8('a', strview_byte_at(v, 0));
  TEST_ASSERT_EQUAL_UINT8('c', strview_byte_at(v, 2));
}

void test_write(void) {
  FILE *f = tmpfile();
  TEST_ASSERT_NOT_NULL(f);
  if (!f)
    return;

  strview_write(STRVIEW("hello"), f);
  rewind(f);
  char out[8] = {0};
  size_t n = fread(out, 1, sizeof(out), f);
  TEST_ASSERT_EQUAL_size_t(5, n);
  TEST_ASSERT_EQUAL_MEMORY("hello", out, 5);
  fclose(f);

  f = tmpfile();
  TEST_ASSERT_NOT_NULL(f);
  if (!f)
    return;
  strview_write(strview_empty(), f);
  rewind(f);
  n = fread(out, 1, sizeof(out), f);
  TEST_ASSERT_EQUAL_size_t(0, n);
  fclose(f);
}

void test_copy(void) {
  uint8_t dst[6];

  strview_copy(STRVIEW("hello"), dst, sizeof(dst));
  TEST_ASSERT_EQUAL_MEMORY("hello", dst, 5);
  TEST_ASSERT_EQUAL_UINT8('\0', dst[5]);

  strview_copy(STRVIEW("truncated"), dst, 3);
  TEST_ASSERT_EQUAL_UINT8('t', dst[0]);
  TEST_ASSERT_EQUAL_UINT8('r', dst[1]);
  TEST_ASSERT_EQUAL_UINT8('\0', dst[2]);

  uint8_t small[8];
  strview_copy(STRVIEW("ab"), small, sizeof(small));
  TEST_ASSERT_EQUAL_UINT8('\0', small[2]);

  strview_copy(STRVIEW("ignored"), NULL, 0); // zero-size dst is a no-op
}

static const TestDispatchEntry ENTRIES[] = {
    {"create_macro", test_create_macro},
    {"create_functions", test_create_functions},
    {"is_empty", test_is_empty},
    {"equals", test_equals},
    {"compare", test_compare},
    {"starts_with", test_starts_with},
    {"ends_with", test_ends_with},
    {"slice", test_slice},
    {"byte_at", test_byte_at},
    {"write", test_write},
    {"copy", test_copy},
};

TEST_DISPATCH_MAIN(ENTRIES)
