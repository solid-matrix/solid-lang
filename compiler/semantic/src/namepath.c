/**
 * @file namepath.c
 * @brief Name path chain operations.
 * @author solid-matrix
 */

#include <assert.h>

#include "namepath.h"
#include "strview.h"

LIST_DEFINE(SemanticNamePath, semantic_namepath, Strview);

SemanticNamePath *semantic_namepath_from_identifiers(Arena *arena, const SyntaxNodeList *ids) {
  assert(arena != NULL);

  if (ids == NULL)
    return semantic_namepath_empty();
  return semantic_namepath_prepend(arena, semantic_namepath_from_identifiers(arena, ids->tail),
                                   ((SyntaxIdentifier *)ids->head)->value);
}

SemanticNamePath *semantic_namepath_from_identifier(Arena *arena, const SyntaxIdentifier *id) {
  assert(arena != NULL);
  assert(id != NULL);

  return semantic_namepath_prepend(arena, semantic_namepath_empty(), id->value);
}

bool semantic_namepath_equals(const SemanticNamePath *path_a, const SemanticNamePath *path_b) {
  for (;;) {
    if (path_a == NULL && path_b == NULL)
      return true;
    if (path_a == NULL || path_b == NULL)
      return false;
    if (!strview_equals(path_a->head, path_b->head))
      return false;
    path_a = path_a->tail;
    path_b = path_b->tail;
  }
}