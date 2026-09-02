/**
 * @file binding_table.h
 * @brief Use-site to entity binding table over a persistent address-ordered treap.
 * @author solid-matrix
 */

#pragma once

#include <stddef.h>

#include "arena.h"
#include "syntax_node.h"

/**
 * @brief One binding entry, and the table itself.
 * @details The table handle is the treap root node, and NULL is the empty
 *          table. Entries are keyed by the use-site's address and ordered
 *          by a hash priority; all functions are pure — inputs stay valid
 *          and unchanged, and results share untouched structure with them.
 */
typedef struct SemanticBindingTable SemanticBindingTable;

/**
 * @brief The empty table. Returns NULL; exists for explicit call sites.
 * @return NULL.
 */
SemanticBindingTable *semantic_binding_table_empty(void);

/**
 * @brief Binds a use-site to the entity it refers to.
 * @param arena Backs every new node.
 * @param table The table to extend; NULL is the empty table.
 * @param syntax The use-site key.
 * @param decl The bound entity.
 * @return The extended table, sharing untouched structure with @p table.
 * @note Inserting an existing key is a caller bug: asserted.
 */
SemanticBindingTable *semantic_binding_table_insert(Arena *arena, SemanticBindingTable *table, SyntaxNode *syntax,
                                                    SyntaxNode *decl);

/**
 * @brief The entity a use-site binds to.
 * @param table The table to search; NULL is the empty table.
 * @param syntax The use-site key.
 * @return The bound entity, or NULL when the site has no binding.
 */
SyntaxNode *semantic_binding_table_lookup(const SemanticBindingTable *table, const SyntaxNode *syntax);

/**
 * @brief Number of entries.
 * @param table The table to measure; NULL is the empty table.
 * @return The length. O(n).
 */
size_t semantic_binding_table_length(const SemanticBindingTable *table);
