#pragma once

#include <stddef.h>

#include "arena.h"
#include "namepath.h"
#include "syntax_node.h"

/**
 * @brief One reverse-table entry, and the table itself.
 * @details The table handle is the treap root node, and NULL is the empty
 *          table. Entries are keyed by the declaration's address; all
 *          functions are pure — inputs stay valid and unchanged, and
 *          results share untouched structure with them. collect fills one
 *          entry per live symbol; the lowering pass reads it to give NIT
 *          declarations their mangled names.
 */
typedef struct SemanticNamePathTable SemanticNamePathTable;

/**
 * @brief The empty table. Returns NULL; exists for explicit call sites.
 * @return NULL.
 */
SemanticNamePathTable *semantic_namepath_table_empty(void);

/**
 * @brief Defines the qualified path of a declaration.
 * @param arena Backs every new node.
 * @param table The table to extend; NULL is the empty table.
 * @param decl The declaration key.
 * @param path The declaration's full world path.
 * @return The extended table, sharing untouched structure with @p table.
 * @note Inserting an existing key is a caller bug: asserted.
 */
SemanticNamePathTable *semantic_namepath_table_insert(Arena *arena, SemanticNamePathTable *table, SyntaxNode *decl,
                                                      SemanticNamePath *path);

/**
 * @brief The qualified path a declaration was defined under.
 * @param table The table to search; NULL is the empty table.
 * @param decl The declaration key.
 * @return The qualified path, e.g. app::x::T, or NULL when the declaration
 *         is unknown.
 */
SemanticNamePath *semantic_namepath_table_lookup(const SemanticNamePathTable *table, const SyntaxNode *decl);

/**
 * @brief Number of entries.
 * @param table The table to measure; NULL is the empty table.
 * @return The length. O(n).
 */
size_t semantic_namepath_table_length(const SemanticNamePathTable *table);
