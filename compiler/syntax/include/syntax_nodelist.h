/**
 * @file syntax_nodelist.h
 * @brief Persistent singly-linked node chain, newest at head.
 * @author solid-matrix
 * @version 0.0.5
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "arena.h"
#include "syntax_node.h"

/**
 * @brief A chain cell. Lists are persistent: sharing cells is safe
 *        because they are never mutated after allocation.
 */
typedef struct SyntaxNodeList SyntaxNodeList;
struct SyntaxNodeList {
  SyntaxNode *node;
  SyntaxNodeList *next;
};

/**
 * @brief The empty list. Returns NULL; exists for explicit call sites.
 * @return NULL.
 */
SyntaxNodeList *syntax_nodelist_empty(void);

/**
 * @brief A list holding @p count array elements, preserving order.
 * @param arena Backs the new cells.
 * @param nodes The elements, in list-head-to-tail order.
 * @param count Number of elements; zero yields NULL.
 * @return The new list.
 */
SyntaxNodeList *syntax_nodelist_from_array(Arena *arena, SyntaxNode *const *nodes, size_t count);

/**
 * @brief A list with @p node followed by all of @p list.
 * @param arena Backs the new cell.
 * @param list The tail; shared wholesale.
 * @param node The new head node.
 * @return The extended list. O(1).
 */
SyntaxNodeList *syntax_nodelist_prepend(Arena *arena, SyntaxNodeList *list, SyntaxNode *node);

/**
 * @brief A list with all elements of @p list followed by @p node.
 * @param arena Backs the copied cells.
 * @param list The prefix; stays valid and unchanged.
 * @param node The new last node.
 * @return The extended list.
 */
SyntaxNodeList *syntax_nodelist_append(Arena *arena, SyntaxNodeList *list, SyntaxNode *node);

/**
 * @brief The first node.
 * @param list A non-empty list.
 * @return The head node. Asserts non-empty.
 */
SyntaxNode *syntax_nodelist_head(SyntaxNodeList *list);

/**
 * @brief Every element except the first.
 * @param list A non-empty list.
 * @return The list without its head; NULL when the length is one.
 *         Asserts non-empty.
 */
SyntaxNodeList *syntax_nodelist_tail(SyntaxNodeList *list);

/**
 * @brief The node at zero-based position @p n.
 * @param list The list to index.
 * @param n Zero-based position.
 * @return The node. Asserts in range.
 */
SyntaxNode *syntax_nodelist_at(SyntaxNodeList *list, size_t n);

/**
 * @brief True when the list holds no nodes.
 * @param list The list to test.
 * @return True for NULL or an empty list.
 */
bool syntax_nodelist_is_empty(const SyntaxNodeList *list);

/**
 * @brief Fresh cells holding @p list's nodes in reverse order.
 * @param arena Backs the new cells.
 * @param list The source; stays valid and unchanged.
 * @return The reversed list.
 */
SyntaxNodeList *syntax_nodelist_reverse(Arena *arena, SyntaxNodeList *list);

/**
 * @brief All nodes of @p list_a followed by all of @p list_b.
 * @param arena Backs the copied cells.
 * @param list_a The prefix; copied, stays valid and unchanged.
 * @param list_b The suffix; shared wholesale.
 * @return The joined list.
 */
SyntaxNodeList *syntax_nodelist_concat(Arena *arena, SyntaxNodeList *list_a, SyntaxNodeList *list_b);

/**
 * @brief Number of nodes.
 * @param list The list to measure.
 * @return The length. O(n).
 */
size_t syntax_nodelist_length(SyntaxNodeList *list);
