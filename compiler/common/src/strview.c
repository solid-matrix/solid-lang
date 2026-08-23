/**
 * @file strview.c
 * @brief Implementation of Strview operations.
 * @author solid-matrix
 * @version 0.0.5
 */

#include <assert.h>
#include <string.h>

#include "strview.h"

Strview strview_create(const uint8_t *str, size_t len) {
  assert(str != NULL || len == 0);
  Strview sv = {str, len};
  return sv;
}

Strview strview_empty(void) {
  Strview sv = {NULL, 0};
  return sv;
}

Strview strview_from_cstr(const char *str) {
  assert(str != NULL);
  return strview_create((const uint8_t *)str, strlen(str));
}

bool strview_is_empty(Strview sv) { return sv.len == 0; }

bool strview_equals(Strview a, Strview b) {
  return a.len == b.len && (a.len == 0 || memcmp(a.data, b.data, a.len) == 0);
}

int strview_compare(Strview a, Strview b) {
  size_t min_len = a.len < b.len ? a.len : b.len;
  if (min_len > 0) {
    int cmp = memcmp(a.data, b.data, min_len);
    if (cmp != 0) {
      return cmp;
    }
  }
  if (a.len > b.len) {
    return 1;
  }
  if (a.len < b.len) {
    return -1;
  }
  return 0;
}

bool strview_starts_with(Strview sv, Strview prefix) {
  if (prefix.len > sv.len) {
    return false;
  }
  return prefix.len == 0 || memcmp(sv.data, prefix.data, prefix.len) == 0;
}

bool strview_ends_with(Strview sv, Strview suffix) {
  if (suffix.len > sv.len) {
    return false;
  }
  return suffix.len == 0 ||
         memcmp(sv.data + sv.len - suffix.len, suffix.data, suffix.len) == 0;
}

Strview strview_slice(Strview sv, size_t start, size_t len) {
  assert(start <= sv.len);
  assert(len <= sv.len - start);
  return strview_create(sv.data + start, len);
}

uint8_t strview_byte_at(Strview sv, size_t pos) {
  assert(pos < sv.len);
  return sv.data[pos];
}

void strview_write(Strview sv, FILE *stream) {
  if (sv.len > 0) {
    fwrite(sv.data, 1, sv.len, stream);
  }
}

void strview_copy(Strview sv, uint8_t *dst, size_t dst_size) {
  assert(dst != NULL || dst_size == 0);
  if (dst_size == 0) {
    return;
  }
  size_t n = sv.len < dst_size - 1 ? sv.len : dst_size - 1;
  if (n > 0) {
    memcpy(dst, sv.data, n);
  }
  dst[n] = '\0';
}
