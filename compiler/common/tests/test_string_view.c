/**
 * @file test_string_view.c
 * @brief Tests for the StringView operations.
 * @author solid-matrix
 * @version 0.0.5
 */

#include <stdint.h>
#include <stdio.h>

#include "string_view.h"
#include "test_util.h"

static void test_create_macro(void) {
  StringView v = SV("hello");
  CHECK(v.len == 5);
  CHECK(v.data[0] == 'h' && v.data[4] == 'o');

  // Concatenated string literals are also literals and keep working.
  StringView c = SV("a" "b");
  CHECK(c.len == 2);
  CHECK(sv_equals(c, SV("ab")));

  // The empty literal produces an empty view.
  StringView e = SV("");
  CHECK(e.len == 0);

  // Views never copy: writes to the underlying buffer are observable
  // through the view (zero-copy semantics).
  char buf[3] = {'a', 'b', 'c'};
  StringView alias = sv_create((const uint8_t *)buf, 3);
  buf[0] = 'X';
  CHECK(sv_byte_at(alias, 0) == 'X');
}

static void test_create_functions(void) {
  StringView v = sv_create((const uint8_t *)"xyz", 3);
  CHECK(v.len == 3 && v.data[0] == 'x');

  StringView e = sv_empty();
  CHECK(e.data == NULL && e.len == 0);

  StringView s = sv_from_cstr("str");
  CHECK(s.len == 3 && sv_equals(s, SV("str")));
}

static void test_is_empty(void) {
  CHECK(sv_is_empty(sv_empty()));
  CHECK(!sv_is_empty(SV("a")));
}

static void test_equals(void) {
  CHECK(sv_equals(SV("same"), SV("same")));
  CHECK(!sv_equals(SV("same"), SV("sane")));
  CHECK(!sv_equals(SV("short"), SV("shorter")));
  CHECK(!sv_equals(SV("shorter"), SV("short")));

  // Equality is length-bounded: embedded NUL bytes are just bytes.
  uint8_t buf[3] = {'a', '\0', 'b'};
  StringView v = sv_create(buf, 3);
  CHECK(sv_equals(v, v));
  CHECK(!sv_equals(v, SV("a")));
  CHECK(sv_equals(v, sv_slice(v, 0, 3)));

  // Two empty views are equal regardless of their data pointers.
  CHECK(sv_equals(sv_empty(), SV("")));
}

static void test_compare(void) {
  CHECK(sv_compare(SV("a"), SV("b")) < 0);
  CHECK(sv_compare(SV("b"), SV("a")) > 0);
  CHECK(sv_compare(SV("x"), SV("x")) == 0);

  // A proper prefix compares less than its extension.
  CHECK(sv_compare(SV("ab"), SV("abc")) < 0);
  CHECK(sv_compare(SV("abc"), SV("ab")) > 0);

  CHECK(sv_compare(sv_empty(), sv_empty()) == 0);
  CHECK(sv_compare(sv_empty(), SV("a")) < 0);
}

static void test_starts_with(void) {
  CHECK(sv_starts_with(SV("hello"), SV("he")));
  CHECK(sv_starts_with(SV("hello"), SV("hello"))); // full match
  CHECK(sv_starts_with(SV("hello"), SV("")));      // empty prefix
  CHECK(!sv_starts_with(SV("hi"), SV("hello")));   // longer than the view
  CHECK(!sv_starts_with(SV("hello"), SV("he!")));
  CHECK(sv_starts_with(sv_empty(), SV("")));       // empty view + empty prefix
}

static void test_ends_with(void) {
  CHECK(sv_ends_with(SV("hello"), SV("lo")));
  CHECK(sv_ends_with(SV("hello"), SV("hello"))); // full match
  CHECK(sv_ends_with(SV("hello"), SV("")));      // empty suffix
  CHECK(!sv_ends_with(SV("lo"), SV("hello")));   // longer than the view
  CHECK(!sv_ends_with(SV("hello"), SV("lo!")));
  CHECK(sv_ends_with(sv_empty(), SV("")));       // empty view + empty suffix
}

static void test_slice(void) {
  StringView base = SV("abcdef");

  StringView mid = sv_slice(base, 1, 4);
  CHECK(mid.len == 4 && mid.data == base.data + 1);
  CHECK(sv_equals(mid, SV("bcde")));

  // A zero-length slice keeps pointing into the source buffer; it never
  // falls back to a NULL-data empty view.
  StringView z = sv_slice(base, 3, 0);
  CHECK(z.len == 0 && z.data == base.data + 3);

  // Slicing at offset len is valid and yields the same zero-length shape.
  StringView tail = sv_slice(base, base.len, 0);
  CHECK(tail.len == 0 && tail.data == base.data + base.len);

  // Full-length slice reproduces the original.
  CHECK(sv_equals(sv_slice(base, 0, base.len), base));
}

static void test_byte_at(void) {
  StringView v = SV("abc");
  CHECK(sv_byte_at(v, 0) == 'a');
  CHECK(sv_byte_at(v, 2) == 'c');
}

static void test_write(void) {
  FILE *f = tmpfile();
  CHECK(f != NULL);
  if (!f)
    return;

  sv_write(SV("hello"), f);
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
  sv_write(sv_empty(), f);
  rewind(f);
  n = fread(out, 1, sizeof(out), f);
  CHECK(n == 0);
  fclose(f);
}

static void test_copy(void) {
  uint8_t dst[6];

  // Fits exactly: content plus terminating NUL.
  sv_copy(SV("hello"), dst, sizeof(dst));
  CHECK(memcmp(dst, "hello", 5) == 0 && dst[5] == '\0');

  // Truncates silently to dst_size - 1 content bytes.
  sv_copy(SV("truncated"), dst, 3);
  CHECK(dst[0] == 't' && dst[1] == 'r' && dst[2] == '\0');

  // Shorter source NUL-terminates early.
  uint8_t small[8];
  sv_copy(SV("ab"), small, sizeof(small));
  CHECK(small[2] == '\0');

  // Zero-size destination is a no-op.
  sv_copy(SV("ignored"), NULL, 0);
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
