/**
 * @file string_view.c
 * @brief Implementation of StringView operations.
 * @author solid-matrix
 * @version 0.0.5
 */

#include "string_view.h"

#include <assert.h>
#include <string.h>

StringView sv_create(const char *str, size_t len)
{
  assert(str != NULL || len == 0);
  StringView sv = {str, len};
  return sv;
}

StringView sv_empty(void)
{
  StringView sv = {NULL, 0};
  return sv;
}

StringView sv_from_cstr(const char *str)
{
  assert(str != NULL);
  return sv_create(str, strlen(str));
}

bool sv_is_empty(StringView sv) { return sv.len == 0; }

bool sv_equals(StringView a, StringView b)
{
  return a.len == b.len && (a.len == 0 || memcmp(a.data, b.data, a.len) == 0);
}

int sv_compare(StringView a, StringView b)
{
  size_t min_len = a.len < b.len ? a.len : b.len;
  if (min_len > 0)
  {
    int cmp = memcmp(a.data, b.data, min_len);
    if (cmp != 0)
    {
      return cmp;
    }
  }
  if (a.len > b.len)
  {
    return 1;
  }
  if (a.len < b.len)
  {
    return -1;
  }
  return 0;
}

StringView sv_slice(StringView sv, size_t start, size_t len)
{
  assert(start <= sv.len);
  assert(len <= sv.len - start);
  if (len == 0)
  {
    return sv_empty();
  }
  return sv_create(sv.data + start, len);
}

char sv_char_at(StringView sv, size_t pos)
{
  assert(pos < sv.len);
  return sv.data[pos];
}

void sv_write(StringView sv, FILE *stream)
{
  if (sv.len > 0)
  {
    fwrite(sv.data, 1, sv.len, stream);
  }
}

void sv_copy(StringView sv, char *dst, size_t dst_size)
{
  assert(dst != NULL || dst_size == 0);
  if (dst_size == 0)
  {
    return;
  }
  size_t n = sv.len < dst_size - 1 ? sv.len : dst_size - 1;
  if (n > 0)
  {
    memcpy(dst, sv.data, n);
  }
  dst[n] = '\0';
}
