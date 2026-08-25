/**
 * @file arena.c
 * @brief Implementation of the region allocator over mimalloc heaps.
 * @author solid-matrix
 * @version 0.0.5
 *
 * The public API and its documentation live in arena.h. mimalloc is a
 * build-internal dependency: it never leaks past this file and the
 * public header.
 *
 * By design, an Arena handle IS the mimalloc heap pointer itself — the
 * public header only knows the opaque `struct Arena`, and every entry
 * point casts it to mi_heap_t*. There is deliberately no wrapper
 * object: the handle costs no separate allocation.
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "arena.h"
#include <mimalloc.h>

/** Fatal-OOM convention shared with xmem: report and abort. */
static void *check_oom(void *ptr) {
  if (ptr == NULL) {
    fprintf(stderr, "arena: out of memory\n");
    abort();
  }
  return ptr;
}

Arena *arena_create(void) {
  return (Arena *)check_oom(mi_heap_new());
}

void *arena_alloc(Arena *arena, size_t size) {
  assert(arena != NULL);
  assert(size > 0);

  return check_oom(mi_heap_malloc((mi_heap_t *)arena, size));
}

void *arena_realloc(Arena *arena, void *ptr, size_t size) {
  assert(arena != NULL);
  assert(size > 0);

  if (ptr == NULL)
    return arena_alloc(arena, size);

  return check_oom(mi_heap_realloc((mi_heap_t *)arena, ptr, size));
}

void arena_destroy(Arena *arena) {
  if (arena == NULL)
    return;

  mi_heap_destroy((mi_heap_t *)arena); // every block dies here
}
