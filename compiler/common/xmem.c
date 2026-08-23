/**
 * @file xmem.c
 * @brief Implementation of the fatal-failure allocation wrappers.
 * @author solid-matrix
 * @version 0.0.5
 *
 * The public API and its documentation live in xmem.h.
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "xmem.h"

void *xmalloc(size_t size) {
  assert(size > 0);

  void *p = malloc(size);
  if (p == NULL) {
    fprintf(stderr, "xmalloc: out of memory requesting %zu bytes\n", size);
    abort();
  }
  return p;
}

void *xrealloc(void *ptr, size_t size) {
  assert(size > 0);

  void *p = realloc(ptr, size);
  if (p == NULL) {
    fprintf(stderr, "xrealloc: out of memory resizing to %zu bytes\n", size);
    abort();
  }
  return p;
}

void *xcalloc(size_t num, size_t size) {
  assert(num > 0);
  assert(size > 0);

  void *p = calloc(num, size);
  if (p == NULL) {
    fprintf(stderr, "xcalloc: out of memory requesting %zu * %zu bytes\n",
            num, size);
    abort();
  }
  return p;
}

void xfree(void *ptr) { free(ptr); }
