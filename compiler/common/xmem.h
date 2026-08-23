/**
 * @file xmem.h
 * @brief Fatal-failure allocation wrappers.
 * @author solid-matrix
 * @version 0.0.5
 *
 * Wrappers around the standard allocation functions that treat running
 * out of memory as a fatal, unrecoverable error: each x* function
 * prints a diagnostic to stderr and aborts the program instead of
 * returning NULL. Routing every allocation in the compiler through
 * these functions removes the need for a NULL check at each call site;
 * this is the project-wide "OOM is fatal" convention.
 */

#pragma once

#include <stddef.h>

/**
 * @brief Allocates @p size bytes, aborting the program on failure.
 * @param size Number of bytes to allocate; must be > 0.
 * @return A pointer to the newly allocated, uninitialized memory.
 */
void *xmalloc(size_t size);

/**
 * @brief Resizes a block to @p size bytes, aborting the program on failure.
 * @param ptr The block to resize; may be NULL, in which case xrealloc()
 *            behaves like xmalloc().
 * @param size New size in bytes; must be > 0.
 * @return A pointer to the resized memory; the address may have moved.
 * @note Freeing via xrealloc(p, 0) is intentionally not supported; use
 *       xfree() to release a block.
 */
void *xrealloc(void *ptr, size_t size);

/**
 * @brief Allocates zero-initialized memory, aborting the program on failure.
 * @param num Number of elements to allocate.
 * @param size Size in bytes of each element.
 * @return A pointer to num * size bytes of zeroed memory.
 */
void *xcalloc(size_t num, size_t size);

/**
 * @brief Frees memory allocated by one of the x* allocators.
 * @param ptr The block to free; NULL is allowed and is a no-op, matching
 *            the behavior of the standard free().
 */
void xfree(void *ptr);
