/**
 * @file ast.c
 * @brief AST helpers: node kind test and node lists.
 * @author solid-matrix
 * @version 0.0.5
 *
 * Node storage comes from the parser's arena (OOM is fatal; see
 * common/arena.h): a nodelist owns only its backing array of pointers,
 * never the nodes or source text those pointers refer to, and nothing
 * is released individually — everything is reclaimed with the arena.
 */

#include <assert.h>

#include "syntax_node.h"

SyntaxNode syntax_node_header(SyntaxKind kind, Span span) {
  return (SyntaxNode){.kind = kind, .span = span};
}

void syntax_nodelist_append(Arena *arena, SyntaxNodeList **list,
                            SyntaxNode *node) {
  assert(arena != NULL);
  assert(list != NULL);

  SyntaxNodeList *link = arena_alloc(arena, sizeof(SyntaxNodeList));
  link->node = node;
  link->next = NULL;

  if (*list == NULL) {
    *list = link;
    return;
  }

  SyntaxNodeList *tail = *list;
  while (tail->next != NULL)
    tail = tail->next;

  tail->next = link;
}
