/**
 * @file nodelist.c
 * @brief Node chain operations.
 * @author solid-matrix
 * @version 0.0.5
 */

#include <assert.h>

#include "node.h"

SyntaxNodeList *syntax_nodelist_empty(void) { return NULL; }

SyntaxNodeList *syntax_nodelist_from_array(Arena *arena, SyntaxNode *const *nodes, size_t count) {
  assert(arena != NULL);
  assert(count == 0 || nodes != NULL);

  SyntaxNodeList *list = NULL;
  for (size_t i = count; i > 0; i--) // build back-to-front: O(n)
    list = syntax_nodelist_prepend(arena, list, nodes[i - 1]);
  return list;
}

SyntaxNodeList *syntax_nodelist_prepend(Arena *arena, SyntaxNodeList *list, SyntaxNode *node) {
  assert(arena != NULL);
  assert(node != NULL);

  SyntaxNodeList *cell = arena_alloc(arena, sizeof(SyntaxNodeList)); // OOM is fatal
  cell->node = node;
  cell->next = list;
  return cell;
}

SyntaxNodeList *syntax_nodelist_append(Arena *arena, SyntaxNodeList *list, SyntaxNode *node) {
  assert(arena != NULL);
  assert(node != NULL);

  if (list == NULL)
    return syntax_nodelist_prepend(arena, NULL, node);

  // Copy every cell so the source list stays valid and unchanged.
  SyntaxNodeList *head = syntax_nodelist_prepend(arena, NULL, list->node);
  SyntaxNodeList *tail = head;
  for (const SyntaxNodeList *it = list->next; it != NULL; it = it->next) {
    tail->next = syntax_nodelist_prepend(arena, NULL, it->node);
    tail = tail->next;
  }
  tail->next = syntax_nodelist_prepend(arena, NULL, node);
  return head;
}

SyntaxNode *syntax_nodelist_head(SyntaxNodeList *list) {
  assert(!syntax_nodelist_is_empty(list));

  return list->node;
}

SyntaxNodeList *syntax_nodelist_tail(SyntaxNodeList *list) {
  assert(!syntax_nodelist_is_empty(list));

  return list->next;
}

SyntaxNode *syntax_nodelist_at(SyntaxNodeList *list, size_t n) {
  const SyntaxNodeList *it = list;
  while (n-- > 0) {
    assert(!syntax_nodelist_is_empty(it)); // out of range

    it = it->next;
  }
  assert(!syntax_nodelist_is_empty(it));

  return it->node;
}

bool syntax_nodelist_is_empty(const SyntaxNodeList *list) { return list == NULL; }

SyntaxNodeList *syntax_nodelist_reverse(Arena *arena, SyntaxNodeList *list) {
  assert(arena != NULL);

  SyntaxNodeList *result = NULL;
  for (const SyntaxNodeList *it = list; it != NULL; it = it->next)
    result = syntax_nodelist_prepend(arena, result, it->node);
  return result;
}

SyntaxNodeList *syntax_nodelist_concat(Arena *arena, SyntaxNodeList *list_a, SyntaxNodeList *list_b) {
  assert(arena != NULL);

  if (syntax_nodelist_is_empty(list_a))
    return list_b; // shares b wholesale

  SyntaxNodeList *head = syntax_nodelist_prepend(arena, NULL, list_a->node);
  SyntaxNodeList *tail = head;
  for (const SyntaxNodeList *it = list_a->next; it != NULL; it = it->next) {
    tail->next = syntax_nodelist_prepend(arena, NULL, it->node);
    tail = tail->next;
  }
  tail->next = list_b; // share b wholesale
  return head;
}

size_t syntax_nodelist_length(SyntaxNodeList *list) {
  size_t n = 0;
  for (const SyntaxNodeList *it = list; it != NULL; it = it->next)
    n++;
  return n;
}
