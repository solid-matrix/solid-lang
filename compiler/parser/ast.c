/**
 * @file ast.c
 * @brief AST helpers: node kind test, node lists, and name paths.
 * @author solid-matrix
 * @version 0.0.5
 *
 * All storage uses xrealloc/xfree (OOM is fatal; see common/mem.h). A
 * list/path owns only its backing array of pointers (or StringViews); it
 * never owns the nodes or source text those pointers/views refer to.
 */

#include <assert.h>
#include <stdbool.h>

#include "ast.h"
#include "mem.h"

SyntaxNodeList syntax_node_list_create(void) {
  SyntaxNodeList list;
  list.len = 0;
  list.cap = 0;
  list.nodes = NULL;
  return list;
}

void syntax_node_list_append(SyntaxNodeList *list, SyntaxNode *node) {
  if (list->len == list->cap) {
    size_t new_cap = list->cap == 0 ? 4 : list->cap * 2;
    list->nodes =
        (SyntaxNode **)xrealloc(list->nodes, new_cap * sizeof(SyntaxNode *));
    list->cap = new_cap;
  }
  list->nodes[list->len++] = node;
}

void syntax_node_list_destroy(SyntaxNodeList *list) {
  if (list == NULL)
    return;
  xfree(list->nodes);
  list->nodes = NULL;
  list->len = 0;
  list->cap = 0;
}
