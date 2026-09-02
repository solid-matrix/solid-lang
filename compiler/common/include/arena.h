/**
 * @file arena.h
 * @brief Region allocation over a private mimalloc heap.
 * @author solid-matrix
 *
 * An Arena hands out allocations that are never released individually;
 * arena_destroy() reclaims everything at once. It serves lifetimes
 * that end together — currently the syntax tree of a single parse.
 *
 * mimalloc is a build-internal dependency: this header exposes only an
 * opaque pointer, so no consumer ever includes <mimalloc.h>.
 */

#pragma once

#include <stddef.h>

typedef struct Arena Arena;

/**
 * @brief Creates an empty arena backed by a fresh mimalloc heap.
 * @return The new arena; never NULL (out of memory aborts).
 */
Arena *arena_create(void);

/**
 * @brief Allocates @p size bytes inside the arena.
 * @param arena The arena to allocate from.
 * @param size Bytes to allocate; must be > 0.
 * @return Uninitialized memory; never NULL.
 */
void *arena_alloc(Arena *arena, size_t size);

/**
 * @brief Grows or shrinks a previous allocation of the same arena.
 *
 * Allocates a new block and copies min(old, new) bytes; the old block
 * is abandoned and reclaimed by arena_destroy().
 * @param arena The arena the pointer came from.
 * @param ptr Previous result from this arena; NULL behaves like
 *            arena_alloc().
 * @param size New size in bytes; must be > 0.
 * @return The relocated memory; never NULL.
 */
void *arena_realloc(Arena *arena, void *ptr, size_t size);

/**
 * @brief Releases every allocation of the arena, then the arena itself.
 * @param arena The arena to destroy; NULL is allowed and is a no-op.
 */
void arena_destroy(Arena *arena);
