/**
 * @file symbol_table.h
 * @brief Immutable symbol table: a namespace tree of name-keyed levels.
 * @author solid-matrix
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "arena.h"
#include "namepath.h"
#include "syntax_node.h"

/**
 * @brief An immutable namespace level of name-keyed entries.
 * @details The table handle is the level's treap root node, and NULL is the
 *          empty level. All functions are pure: inputs stay valid and
 *          unchanged, and results share untouched structure with them.
 */
typedef struct SemanticSymbolTable SemanticSymbolTable;

/**
 * @brief The empty table. Returns NULL; exists for explicit call sites.
 * @return NULL.
 */
SemanticSymbolTable *semantic_symbol_table_empty(void);

/**
 * @brief Registers a symbol or materializes a namespace path.
 * @param arena Backs every new node.
 * @param table The table to define into; NULL is the empty table.
 * @param path Source-order chain. With @p node non-NULL the tail is the
 *             symbol name and the rest is its namespace chain; with NULL
 *             every segment materializes as a namespace, and redefining an
 *             existing namespace reuses its node (declarations across files
 *             merge).
 * @param node The declaration node, or NULL to materialize namespaces.
 * @return The table after the definition, sharing untouched structure with
 *         @p table; NULL on collision — a name already taken by the other
 *         kind — in which case @p table is unchanged.
 * @note All-or-nothing: a failed definition leaves no partial state.
 */
SemanticSymbolTable *semantic_symbol_table_insert(Arena *arena, SemanticSymbolTable *table,
                                                  const SemanticNamePath *path, SyntaxNode *node);

/**
 * @brief Looks a name up along its namespace chain.
 * @param table The table to search; NULL is the empty table.
 * @param path Source-order chain whose tail is the symbol name.
 * @return The declaration node, or NULL when the name is unknown.
 */
SyntaxNode *semantic_symbol_table_lookup(const SemanticSymbolTable *table, const SemanticNamePath *path);

/**
 * @brief The content level of the namespace @p path names.
 * @param table The table to search; NULL is the empty table.
 * @param path Source-order chain of namespace names; an empty chain is the
 *             table itself.
 * @return The namespace's content level, or NULL when a segment is missing,
 *         names a symbol, or the namespace is empty — an empty level and a
 *         missing one are indistinguishable.
 */
SemanticSymbolTable *semantic_symbol_table_subtable(const SemanticSymbolTable *table, const SemanticNamePath *path);

/**
 * @brief True when @p path names an existing entry, namespace or symbol.
 * @details Unlike subtable(), contains() accepts an empty namespace: the
 *          entry exists in its parent level's tree whatever its content
 *          holds.
 * @param table The table to search; NULL is the empty table.
 * @param path Source-order chain whose tail may name a namespace or a
 *             symbol; an empty chain is the table itself.
 * @return True when every segment resolves to an entry; false when a
 *         segment is missing or a non-tail segment names a symbol.
 */
bool semantic_symbol_table_contains(const SemanticSymbolTable *table, const SemanticNamePath *path);

/**
 * @brief Number of entries of the level.
 * @details Counts only the level itself; a namespace entry counts as one
 *          whatever its @p down level holds.
 * @param table The table to measure; NULL is the empty table.
 * @return The length. O(n).
 */
size_t semantic_symbol_table_length(const SemanticSymbolTable *table);
