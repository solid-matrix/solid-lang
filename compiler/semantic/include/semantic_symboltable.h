/**
 * @file semantic_symboltable.h
 * @brief Immutable symbol table: a namespace tree of name-keyed levels.
 * @author solid-matrix
 * @version 0.0.5
 */

#pragma once

#include "arena.h"
#include "strview.h"
#include "syntax_node.h"

/**
 * @brief Chain of name segments ordered innermost first.
 * @details The head names the innermost entity: for define()/lookup() it is
 *          the symbol name, for sub() the innermost namespace. Trailing
 *          segments walk outward, so `a::b::X` reads as X -> b -> a.
 */
typedef struct SemanticNamePath SemanticNamePath;
struct SemanticNamePath {
  Strview name;
  SemanticNamePath *next;
};

/**
 * @brief Immutable symbol table holding one package's namespace tree.
 * @details define() returns a new table and leaves the old one untouched,
 *          so tables are values: persistent, shareable, and safe to keep
 *          across definitions. All storage lives in the arena given to
 *          semantic_symboltable_create().
 */
typedef struct SemanticSymbolTable SemanticSymbolTable;

/**
 * @brief Creates an empty table.
 * @param arena Backs the table and everything derived from it; must outlive
 *              the table and every table defined from it.
 * @return The new empty table.
 */
SemanticSymbolTable *semantic_symboltable_create(Arena *arena);

/**
 * @brief Registers a symbol or materializes a namespace path.
 * @param table The owning table to define into — never a sub() view.
 * @param path Innermost-first chain. With @p node non-NULL the head is the
 *             symbol name and the rest is its namespace chain; with NULL
 *             every segment materializes as a namespace, and redeclaring an
 *             existing namespace reuses its node (declarations across files
 *             merge).
 * @param node The declaration node, or NULL to materialize namespaces.
 * @return The table after the definition, sharing untouched structure with
 *         @p table; NULL on collision — a name already taken by the other
 *         kind — in which case @p table is unchanged.
 * @note All-or-nothing: a failed definition leaves no partial state.
 */
SemanticSymbolTable *semantic_symboltable_define(SemanticSymbolTable *table, const SemanticNamePath *path,
                                                 SyntaxNode *node);

/**
 * @brief Looks a name up along its namespace chain.
 * @param table The table, or a sub() view, to search.
 * @param path Innermost-first chain whose head is the symbol name.
 * @return The declaration node, or NULL when the name is unknown.
 */
SyntaxNode *semantic_symboltable_lookup(const SemanticSymbolTable *table, const SemanticNamePath *path);

/**
 * @brief Borrows the sub-table rooted at a namespace.
 * @param table The table to view. Must have seen its last define() — which
 *              the collect-then-resolve pass order guarantees — because the
 *              view aliases the frozen tree.
 * @param path Innermost-first chain of namespace names; an empty chain views
 *             the whole table.
 * @return The read-only view, or NULL when a segment is missing or names a
 *         symbol.
 */
const SemanticSymbolTable *semantic_symboltable_sub(const SemanticSymbolTable *table, const SemanticNamePath *path);
