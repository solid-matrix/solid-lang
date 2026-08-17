/**
 * @file test_string_view.c
 * @brief Tests for the StringView basic behavior.
 * @author solid-matrix
 * @version 0.0.5
 */

#include <stdio.h>
#include <string.h>

#include "string_view.h"

static int g_failures;

#define CHECK(cond)                                                            \
  do {                                                                         \
    if (!(cond)) {                                                             \
      fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);          \
      g_failures++;                                                            \
    }                                                                          \
  } while (0)

int main(void) {
  // Construction: SV literal macro, sv_from_cstr, sv_empty
  StringView sv = SV("hello");
  CHECK(sv.len == 5);
  CHECK(sv_char_at(sv, 0) == 'h');
  CHECK(sv_equals(sv, sv_from_cstr("hello")));
  CHECK(!sv_equals(sv, sv_from_cstr("world")));
  CHECK(sv_is_empty(sv_empty()));
  CHECK(sv_empty().data == NULL);

  // Equality: different lengths are not equal
  CHECK(!sv_equals(sv, sv_from_cstr("hell")));

  // Byte semantics: embedded NULs compare correctly
  StringView n1 = sv_create("a\0b", 3);
  StringView n2 = sv_create("a\0c", 3);
  StringView n3 = sv_create("a\0b", 3);
  CHECK(sv_equals(n1, n3));
  CHECK(!sv_equals(n1, n2));

  // Lexicographic comparison
  CHECK(sv_compare(sv_from_cstr("abc"), sv_from_cstr("abd")) < 0);
  CHECK(sv_compare(sv_from_cstr("ab"), sv_from_cstr("abc")) < 0);
  CHECK(sv_compare(sv_from_cstr("abc"), sv_from_cstr("abc")) == 0);
  CHECK(sv_compare(sv_from_cstr("abd"), sv_from_cstr("abc")) > 0);

  // Slicing: normal and zero-length
  StringView ell = sv_slice(sv, 1, 3);
  CHECK(ell.len == 3);
  CHECK(sv_equals(ell, sv_from_cstr("ell")));
  CHECK(sv_is_empty(sv_slice(sv, 0, 0)));

  // Per-character access
  CHECK(sv_char_at(sv, 4) == 'o');

  // Copy: full fit
  char buf[8];
  sv_copy(sv, buf, sizeof(buf));
  CHECK(strcmp(buf, "hello") == 0);

  // Copy: silent truncation, always NUL-terminated
  char small[3];
  sv_copy(sv, small, sizeof(small));
  CHECK(small[0] == 'h' && small[1] == 'e' && small[2] == '\0');

  // Copy: dst_size == 0 is a no-op
  sv_copy(sv, NULL, 0);

  // Writing to a stream
  FILE *f = tmpfile();
  CHECK(f != NULL);
  sv_write(sv, f);
  rewind(f);
  char got[6] = {0};
  CHECK(fread(got, 1, 5, f) == 5);
  CHECK(memcmp(got, "hello", 5) == 0);
  fclose(f);

  if (g_failures == 0) {
    printf("test_string_view: all ok\n");
    return 0;
  }
  fprintf(stderr, "test_string_view: %d failure(s)\n", g_failures);
  return 1;
}
