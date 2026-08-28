#pragma once

#include <stdbool.h>

#include "arena.h"
#include "syntax_node.h"

typedef struct SyntaxNodeList SyntaxNodeList;
struct SyntaxNodeList {
  SyntaxNode *node;
  SyntaxNodeList *next;
};

/**
 * @brief The empty list. Returns NULL; exists for explicit call sites.
 */
SyntaxNodeList *syntax_nodelist_empty(void);

/**
 * @brief Builds a list holding @p count array elements, preserving
 *        order. Returns NULL when @p count is zero.
 */
SyntaxNodeList *syntax_nodelist_from_array(Arena *arena, SyntaxNode *const *nodes, size_t count);

/**
 * @brief A list with @p node followed by all of @p list. O(1); shares
 *        the whole old spine.
 */
SyntaxNodeList *syntax_nodelist_prepend(Arena *arena, SyntaxNodeList *list, SyntaxNode *node);

/**
 * @brief A list with all elements of @p list followed by @p node.
 *        Copies @p list's cells; the source stays valid and unchanged.
 */
SyntaxNodeList *syntax_nodelist_append(Arena *arena, SyntaxNodeList *list, SyntaxNode *node);

/**
 * @brief The first node. Asserts non-empty.
 */
SyntaxNode *syntax_nodelist_head(SyntaxNodeList *list);

/**
 * @brief Every element except the first (NULL when length is one).
 *        Asserts non-empty.
 */
SyntaxNodeList *syntax_nodelist_tail(SyntaxNodeList *list);

/**
 * @brief The node at zero-based position @p n. Asserts in range.
 */
SyntaxNode *syntax_nodelist_at(SyntaxNodeList *list, size_t n);

/**
 * @brief True when the list holds no nodes.
 */
bool syntax_nodelist_is_empty(const SyntaxNodeList *list);

/**
 * @brief Fresh cells holding @p list's nodes in reverse order; the
 *        source stays valid and unchanged.
 */
SyntaxNodeList *syntax_nodelist_reverse(Arena *arena, SyntaxNodeList *list);

/**
 * @brief All nodes of @p list_a followed by all of @p list_b. Copies
 *        @p list_a's cells and shares @p list_b wholesale.
 */
SyntaxNodeList *syntax_nodelist_concat(Arena *arena, SyntaxNodeList *list_a, SyntaxNodeList *list_b);

/**
 * @brief Number of nodes. O(n).
 */
size_t syntax_nodelist_length(SyntaxNodeList *list);