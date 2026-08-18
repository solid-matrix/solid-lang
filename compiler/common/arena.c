/**
 * @file arena.c
 * @brief Implementation of the block-list arena allocator.
 * @author solid-matrix
 * @version 0.0.5
 */

#include "arena.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define ARENA_ALIGN (sizeof(void *) >= 8 ? 16 : 8)
#define ARENA_DEFAULT_BLOCK_SIZE ((size_t)8192)

struct ArenaBlock
{
  ArenaBlock *next;
  size_t size;
  unsigned char data[];
};

static size_t align_up(size_t n, size_t align)
{
  return (n + align - 1) / align * align;
}

static unsigned char *block_data(ArenaBlock *b)
{
  return (unsigned char *)align_up((size_t)(uintptr_t)b->data, ARENA_ALIGN);
}

static ArenaBlock *block_new(size_t size)
{
  ArenaBlock *b = malloc(sizeof(ArenaBlock) + size);
  if (b == NULL)
  {
    return NULL;
  }
  b->next = NULL;
  b->size = size;
  return b;
}

bool arena_init(Arena *a, size_t initial_cap)
{
  a->head = NULL;
  a->offset = 0;
  a->block_size = initial_cap == 0 ? ARENA_DEFAULT_BLOCK_SIZE : initial_cap;
  a->head = block_new(a->block_size);
  return a->head != NULL;
}

static void arena_grow(Arena *a, size_t required)
{
  size_t new_size = a->block_size;
  if (new_size < required)
  {
    new_size = required;
  }
  a->block_size = new_size > SIZE_MAX / 2 ? new_size : new_size * 2;
  ArenaBlock *b = block_new(new_size);
  if (b == NULL)
  {
    abort();
  }
  b->next = a->head;
  a->head = b;
  a->offset = 0;
}

void *arena_alloc(Arena *a, size_t size)
{
  size = align_up(size, ARENA_ALIGN);
  if (a->offset + size > a->head->size)
  {
    arena_grow(a, size);
  }
  void *p = block_data(a->head) + a->offset;
  a->offset += size;
  return p;
}

void *arena_alloc_zeroed(Arena *a, size_t size)
{
  void *p = arena_alloc(a, size);
  memset(p, 0, size);
  return p;
}

void arena_reset(Arena *a)
{
  if (a->head == NULL)
  {
    return;
  }
  ArenaBlock *keep = a->head;
  ArenaBlock *b = keep->next;
  while (b != NULL)
  {
    ArenaBlock *next = b->next;
    free(b);
    b = next;
  }
  keep->next = NULL;
  a->head = keep;
  a->offset = 0;
}

void arena_destroy(Arena *a)
{
  ArenaBlock *b = a->head;
  while (b != NULL)
  {
    ArenaBlock *next = b->next;
    free(b);
    b = next;
  }
  a->head = NULL;
  a->offset = 0;
  a->block_size = 0;
}
