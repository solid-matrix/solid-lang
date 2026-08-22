/**
 * @file mem.c
 * @brief Implementation of the fatal-failure allocation wrappers.
 * @author solid-matrix
 * @version 0.0.5
 *
 * The public API and its documentation live in mem.h.
 */

#include <assert.h>
#include <stdlib.h>

#include "mem.h"

void *xmalloc(size_t size) {
  assert(size > 0);

  void *p = malloc(size);
  if (p == NULL)
    abort();
  return p;
}

void *xrealloc(void *ptr, size_t size) {
  assert(size > 0);

  void *p = realloc(ptr, size);
  if (p == NULL)
    abort();
  return p;
}

void *xcalloc(size_t num, size_t size) {
  assert(num > 0);
  assert(size > 0);

  void *p = calloc(num, size);
  if (p == NULL)
    abort();
  return p;
}

void xfree(void *ptr) { free(ptr); }
