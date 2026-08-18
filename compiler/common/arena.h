/**
 * @file arena.h
 * @brief A bump arena allocator backed by a linked list of blocks.
 * @author solid-matrix
 * @version 0.0.5
 */

#ifndef SOLID_ARENA_H
#define SOLID_ARENA_H

#include <stdbool.h>
#include <stddef.h>

/**
 * @brief A bump arena allocator backed by a linked list of blocks.
 *
 * Memory is released only by arena_reset() or arena_destroy();
 * individual deallocation is not supported.
 * NOT thread-safe: give each thread its own Arena, or guard access
 * with a lock if one Arena must be shared.
 *
 * Growth never moves previously returned pointers: when a block is
 * full a new block is appended, so all outstanding pointers stay valid
 * until the next arena_reset() or arena_destroy().
 */

typedef struct ArenaBlock ArenaBlock;

/**
 * @brief Arena state; the public part is the handle passed to all
 * functions.
 */
typedef struct
{
  ArenaBlock *head;  // block currently being filled
  size_t offset;     // cursor within the current block
  size_t block_size; // size of the next block to allocate
} Arena;

/**
 * @brief Initializes @p a and allocates its first block.
 * @param a The arena to initialize.
 * @param initial_cap Size of the first block; 0 selects a default.
 * @return False if the first block could not be allocated.
 */
bool arena_init(Arena *a, size_t initial_cap);

/**
 * @brief Allocates @p size bytes.
 *
 * Alignment is 16 bytes on 64-bit builds and 8 on 32-bit builds
 * (at least max_align_t). Aborts the program on out-of-memory.
 * @param a The arena to allocate from.
 * @param size Number of bytes; 0 returns a valid pointer.
 * @return The allocated memory.
 */
void *arena_alloc(Arena *a, size_t size);

/**
 * @brief Like arena_alloc(), with all returned bytes zeroed.
 * @param a The arena to allocate from.
 * @param size Number of bytes.
 * @return The zeroed memory.
 */
void *arena_alloc_zeroed(Arena *a, size_t size);

/**
 * @brief Makes all previously allocated memory reusable.
 *
 * Every outstanding pointer is invalidated. Keeps the most recent
 * block (its capacity is retained) and frees all older blocks.
 * @param a The arena to reset.
 */
void arena_reset(Arena *a);

/**
 * @brief Frees all memory owned by @p a.
 * @param a The arena to destroy. Must not be used afterwards.
 */
void arena_destroy(Arena *a);

#endif /* SOLID_ARENA_H */
