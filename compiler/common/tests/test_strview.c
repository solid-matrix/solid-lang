/**
 * @file test_strview.c
 * @brief Tests for the Strview operations.
 * @author solid-matrix
 * @version 0.0.5
 */

#include <stdint.h>
#include <stdio.h>

#include "strview.h"
#include "test_util.h"

static void test_create_macro(void) {
  Strview v = STRVIEW("hello");
  CHECK(v.len == 5);
  CHECK(v.data[0] == 'h' && v.data[4] == 'o');

  // Concatenated string literals are also literals and keep working.
  Strview c = STRVIEW("a"
                    "b");
  CHECK(c.len == 2);
  CHECK(strview_equals(c, STRVIEW("ab")));

  // The empty literal produces an empty view.
  Strview e = STRVIEW("");
  CHECK(e.len == 0);

  // Views never copy: writes to the underlying buffer are observable
  // through the view (zero-copy semantics).
  char buf[3] = {'a', 'b', 'c'};
  Strview alias = strview_create((const uint8_t *)buf, 3);
  buf[0] = 'X';
  CHECK(strview_byte_at(alias, 0) == 'X');
}

static void test_create_functions(void) {
  Strview v = strview_create((const uint8_t *)"xyz", 3);
  CHECK(v.len == 3 && v.data[0] == 'x');

  Strview e = strview_empty();
  CHECK(e.data == NULL && e.len == 0);

  Strview s = strview_from_cstr("str");
  CHECK(s.len == 3 && strview_equals(s, STRVIEW("str")));
}

static void test_is_empty(void) {
  CHECK(strview_is_empty(strview_empty()));
  CHECK(!strview_is_empty(STRVIEW("a")));
}

static void test_equals(void) {
  CHECK(strview_equals(STRVIEW("same"), STRVIEW("same")));
  CHECK(!strview_equals(STRVIEW("same"), STRVIEW("sane")));
  CHECK(!strview_equals(STRVIEW("short"), STRVIEW("shorter")));
  CHECK(!strview_equals(STRVIEW("shorter"), STRVIEW("short")));

  // Equality is length-bounded: embedded NUL bytes are just bytes.
  uint8_t buf[3] = {'a', '\0', 'b'};
  Strview v = strview_create(buf, 3);
  CHECK(strview_equals(v, v));
  CHECK(!strview_equals(v, STRVIEW("a")));
  CHECK(strview_equals(v, strview_slice(v, 0, 3)));

  // Two empty views are equal regardless of their data pointers.
  CHECK(strview_equals(strview_empty(), STRVIEW("")));
}

static void test_compare(void) {
  CHECK(strview_compare(STRVIEW("a"), STRVIEW("b")) < 0);
  CHECK(strview_compare(STRVIEW("b"), STRVIEW("a")) > 0);
  CHECK(strview_compare(STRVIEW("x"), STRVIEW("x")) == 0);

  // A proper prefix compares less than its extension.
  CHECK(strview_compare(STRVIEW("ab"), STRVIEW("abc")) < 0);
  CHECK(strview_compare(STRVIEW("abc"), STRVIEW("ab")) > 0);

  CHECK(strview_compare(strview_empty(), strview_empty()) == 0);
  CHECK(strview_compare(strview_empty(), STRVIEW("a")) < 0);
}

static void test_starts_with(void) {
  CHECK(strview_starts_with(STRVIEW("hello"), STRVIEW("he")));
  CHECK(strview_starts_with(STRVIEW("hello"), STRVIEW("hello"))); // full match
  CHECK(strview_starts_with(STRVIEW("hello"), STRVIEW("")));      // empty prefix
  CHECK(!strview_starts_with(STRVIEW("hi"), STRVIEW("hello")));   // longer than the view
  CHECK(!strview_starts_with(STRVIEW("hello"), STRVIEW("he!")));
  CHECK(strview_starts_with(strview_empty(), STRVIEW(""))); // empty view + empty prefix
}

static void test_ends_with(void) {
  CHECK(strview_ends_with(STRVIEW("hello"), STRVIEW("lo")));
  CHECK(strview_ends_with(STRVIEW("hello"), STRVIEW("hello"))); // full match
  CHECK(strview_ends_with(STRVIEW("hello"), STRVIEW("")));      // empty suffix
  CHECK(!strview_ends_with(STRVIEW("lo"), STRVIEW("hello")));   // longer than the view
  CHECK(!strview_ends_with(STRVIEW("hello"), STRVIEW("lo!")));
  CHECK(strview_ends_with(strview_empty(), STRVIEW(""))); // empty view + empty suffix
}

static void test_slice(void) {
  Strview base = STRVIEW("abcdef");

  Strview mid = strview_slice(base, 1, 4);
  CHECK(mid.len == 4 && mid.data == base.data + 1);
  CHECK(strview_equals(mid, STRVIEW("bcde")));

  // A zero-length slice keeps pointing into the source buffer; it never
  // falls back to a NULL-data empty view.
  Strview z = strview_slice(base, 3, 0);
  CHECK(z.len == 0 && z.data == base.data + 3);

  // Slicing at offset len is valid and yields the same zero-length shape.
  Strview tail = strview_slice(base, base.len, 0);
  CHECK(tail.len == 0 && tail.data == base.data + base.len);

  // Full-length slice reproduces the original.
  CHECK(strview_equals(strview_slice(base, 0, base.len), base));
}

static void test_byte_at(void) {
  Strview v = STRVIEW("abc");
  CHECK(strview_byte_at(v, 0) == 'a');
  CHECK(strview_byte_at(v, 2) == 'c');
}

static void test_write(void) {
  FILE *f = tmpfile();
  CHECK(f != NULL);
  if (!f)
    return;

  strview_write(STRVIEW("hello"), f);
  rewind(f);
  char out[8] = {0};
  size_t n = fread(out, 1, sizeof(out), f);
  CHECK(n == 5);
  CHECK(memcmp(out, "hello", 5) == 0);
  fclose(f);

  // Writing an empty view writes nothing.
  f = tmpfile();
  CHECK(f != NULL);
  if (!f)
    return;
  strview_write(strview_empty(), f);
  rewind(f);
  n = fread(out, 1, sizeof(out), f);
  CHECK(n == 0);
  fclose(f);
}

static void test_copy(void) {
  uint8_t dst[6];

  // Fits exactly: content plus terminating NUL.
  strview_copy(STRVIEW("hello"), dst, sizeof(dst));
  CHECK(memcmp(dst, "hello", 5) == 0 && dst[5] == '\0');

  // Truncates silently to dst_size - 1 content bytes.
  strview_copy(STRVIEW("truncated"), dst, 3);
  CHECK(dst[0] == 't' && dst[1] == 'r' && dst[2] == '\0');

  // Shorter source NUL-terminates early.
  uint8_t small[8];
  strview_copy(STRVIEW("ab"), small, sizeof(small));
  CHECK(small[2] == '\0');

  // Zero-size destination is a no-op.
  strview_copy(STRVIEW("ignored"), NULL, 0);
}

static const TestEntry ENTRIES[] = {
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

TEST_MAIN(ENTRIES)
