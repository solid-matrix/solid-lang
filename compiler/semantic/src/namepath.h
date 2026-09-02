#pragma once

#include "list.h"
#include "semantic_common.h"

/**
 * @brief Declares the name path list. The payload is a name; a NULL path is
 *        empty. See list.h for the shared contract of every function.
 */
LIST_DECLARE(SemanticNamePath, semantic_namepath, Strview);

/**
 * @brief A path from a source-order chain of SyntaxIdentifier nodes.
 * @param arena Backs the new cells.
 * @param ids The identifier chain, in source order; NULL yields an empty
 *            path.
 * @return The new path.
 */
SemanticNamePath *semantic_namepath_from_identifiers(Arena *arena, const SyntaxNodeList *ids);

/**
 * @brief A single-entry path holding @p id's name.
 * @param arena Backs the new cell.
 * @param id The identifier to read.
 * @return The new path. O(1).
 */
SemanticNamePath *semantic_namepath_from_identifier(Arena *arena, const SyntaxIdentifier *id);

/**
 * @brief True when the paths hold the same names in the same order.
 * @param path_a The first path.
 * @param path_b The second path.
 * @return True when both are empty, or name-by-name equal.
 */
bool semantic_namepath_equals(const SemanticNamePath *path_a, const SemanticNamePath *path_b);