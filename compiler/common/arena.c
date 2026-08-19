/**
 * @file arena.c
 * @brief Implementation of the block-list arena allocator.
 * @author solid-matrix
 * @version 0.0.5
 */

#include <stdint.h>
#include <string.h>

#include "arena.h"
#include "mem.h"

/** @brief Alignment for arena allocations: 16 bytes on 64-bit, 8 on 32-bit. */
#define ARENA_ALIGN (sizeof(void *) >= 8 ? 16 : 8)

/** @brief Default size of the first arena block when no initial capacity is given. */
#define ARENA_DEFAULT_BLOCK_SIZE ((size_t)8192)

/**
 * @brief A linked-list node that owns one contiguous block of arena memory.
 */
struct ArenaBlock
{
  ArenaBlock *next;     // next block, or NULL for the block being filled
  size_t size;          // usable bytes in this block, not counting the header
  unsigned char data[]; // storage for the block; aligned up by block_data()
};

/**
 * @brief Rounds @p n up to the next multiple of @p align.
 * @param n The size to round.
 * @param align The alignment; must be a power of two.
 * @return @p n rounded up to the next multiple of @p align.
 */
static size_t align_up(size_t n, size_t align)
{
  return (n + align - 1) / align * align;
}

/**
 * @brief Start of @p b's usable storage, aligned up to ARENA_ALIGN.
 * @param b The block to query.
 * @return A pointer to the aligned region inside @p b's flexible array.
 */
static unsigned char *block_data(ArenaBlock *b)
{
  return (unsigned char *)align_up((size_t)(uintptr_t)b->data, ARENA_ALIGN);
}

/**
 * @brief Allocates a new block with @p size usable bytes.
 * @param size Number of usable bytes (storage after the header).
 * @return The new block; it is not yet linked into any list.
 */
static ArenaBlock *block_new(size_t size)
{
  ArenaBlock *b = xmalloc(sizeof(ArenaBlock) + size);
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

/**
 * @brief Allocates a fresh block and makes it the new head of @p a.
 * @param a The arena to grow.
 * @param required Minimum usable bytes the new block must provide.
 */
static void arena_grow(Arena *a, size_t required)
{
  size_t new_size = a->block_size;
  if (new_size < required)
  {
    new_size = required;
  }
  a->block_size = new_size > SIZE_MAX / 2 ? new_size : new_size * 2;
  ArenaBlock *b = block_new(new_size);
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
    xfree(b);
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
    xfree(b);
    b = next;
  }
  a->head = NULL;
  a->offset = 0;
  a->block_size = 0;
}
