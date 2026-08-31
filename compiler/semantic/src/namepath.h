#pragma once

#include "arena.h"
#include "semantic_common.h"
/**
 * @brief The empty path. Returns NULL; exists for explicit call sites.
 * @return NULL.
 */
SemanticNamePath *semantic_namepath_empty(void);

/**
 * @brief A path holding @p count names, preserving order.
 * @param arena Backs the new cells.
 * @param names The names, in head-to-tail order.
 * @param count Number of names; zero yields NULL.
 * @return The new path.
 */
SemanticNamePath *semantic_namepath_from_array(Arena *arena, const Strview *names, size_t count);

/**
 * @brief A path with @p name followed by all of @p path.
 * @param arena Backs the new cell.
 * @param path The tail; shared wholesale.
 * @param name The new head name.
 * @return The extended path. O(1).
 */
SemanticNamePath *semantic_namepath_prepend(Arena *arena, SemanticNamePath *path, Strview name);

/**
 * @brief A path with all names of @p path followed by @p name.
 * @param arena Backs the copied cells.
 * @param path The prefix; stays valid and unchanged.
 * @param name The new last name.
 * @return The extended path.
 */
SemanticNamePath *semantic_namepath_append(Arena *arena, SemanticNamePath *path, Strview name);

/**
 * @brief The first name.
 * @param path A non-empty path.
 * @return The head name. Asserts non-empty.
 */
Strview semantic_namepath_head(SemanticNamePath *path);

/**
 * @brief Every segment except the first.
 * @param path A non-empty path.
 * @return The path without its head; NULL when the length is one.
 *         Asserts non-empty.
 */
SemanticNamePath *semantic_namepath_tail(SemanticNamePath *path);

/**
 * @brief The name at zero-based position @p n.
 * @param path The path to index.
 * @param n Zero-based position.
 * @return The name. Asserts in range.
 */
Strview semantic_namepath_at(SemanticNamePath *path, size_t n);

/**
 * @brief True when the path holds no names.
 * @param path The path to test.
 * @return True for NULL or an empty path.
 */
bool semantic_namepath_is_empty(const SemanticNamePath *path);

/**
 * @brief Fresh cells holding @p path's names in reverse order.
 * @param arena Backs the new cells.
 * @param path The source; stays valid and unchanged.
 * @return The reversed path.
 */
SemanticNamePath *semantic_namepath_reverse(Arena *arena, SemanticNamePath *path);

/**
 * @brief All names of @p path_a followed by all of @p path_b.
 * @param arena Backs the copied cells.
 * @param path_a The prefix; copied, stays valid and unchanged.
 * @param path_b The suffix; shared wholesale.
 * @return The joined path.
 */
SemanticNamePath *semantic_namepath_concat(Arena *arena, SemanticNamePath *path_a, SemanticNamePath *path_b);

/**
 * @brief Number of names.
 * @param path The path to measure.
 * @return The length. O(n).
 */
size_t semantic_namepath_length(SemanticNamePath *path);
