/**
 * @file error.c
 * @brief SemanticError construction.
 * @author solid-matrix
 * @version 0.0.5
 */

#include <assert.h>

#include "error.h"

SemanticError semantic_error_create(SemanticErrorCode code, Span span) {
  return (SemanticError){.code = code, .span = span};
}

SemanticErrorList *semantic_errorlist_empty(void) { return NULL; }

SemanticErrorList *semantic_errorlist_from_array(Arena *arena, const SemanticError *errors, size_t count) {
  assert(arena != NULL);
  assert(count == 0 || errors != NULL);

  SemanticErrorList *list = NULL;
  for (size_t i = count; i > 0; i--) // build back-to-front: O(n)
    list = semantic_errorlist_prepend(arena, list, errors[i - 1]);
  return list;
}

SemanticErrorList *semantic_errorlist_prepend(Arena *arena, SemanticErrorList *list, SemanticError error) {
  assert(arena != NULL);

  SemanticErrorList *cell = arena_alloc(arena, sizeof(SemanticErrorList)); // OOM is fatal
  cell->error = error;
  cell->next = list;
  return cell;
}

SemanticErrorList *semantic_errorlist_append(Arena *arena, SemanticErrorList *list, SemanticError error) {
  assert(arena != NULL);

  if (list == NULL)
    return semantic_errorlist_prepend(arena, NULL, error);

  // Copy every cell so the source list stays valid and unchanged.
  SemanticErrorList *head = semantic_errorlist_prepend(arena, NULL, list->error);
  SemanticErrorList *tail = head;
  for (const SemanticErrorList *it = list->next; it != NULL; it = it->next) {
    tail->next = semantic_errorlist_prepend(arena, NULL, it->error);
    tail = tail->next;
  }
  tail->next = semantic_errorlist_prepend(arena, NULL, error);
  return head;
}

SemanticError semantic_errorlist_head(SemanticErrorList *list) {
  assert(!semantic_errorlist_is_empty(list));

  return list->error;
}

SemanticErrorList *semantic_errorlist_tail(SemanticErrorList *list) {
  assert(!semantic_errorlist_is_empty(list));

  return list->next;
}

SemanticError semantic_errorlist_at(SemanticErrorList *list, size_t n) {
  const SemanticErrorList *it = list;
  while (n-- > 0) {
    assert(!semantic_errorlist_is_empty(it)); // out of range

    it = it->next;
  }
  assert(!semantic_errorlist_is_empty(it));

  return it->error;
}

bool semantic_errorlist_is_empty(const SemanticErrorList *list) { return list == NULL; }

SemanticErrorList *semantic_errorlist_reverse(Arena *arena, SemanticErrorList *list) {
  assert(arena != NULL);

  SemanticErrorList *result = NULL;
  for (const SemanticErrorList *it = list; it != NULL; it = it->next)
    result = semantic_errorlist_prepend(arena, result, it->error);
  return result;
}

SemanticErrorList *semantic_errorlist_concat(Arena *arena, SemanticErrorList *list_a, SemanticErrorList *list_b) {
  assert(arena != NULL);

  if (semantic_errorlist_is_empty(list_a))
    return list_b; // shares b wholesale

  SemanticErrorList *head = semantic_errorlist_prepend(arena, NULL, list_a->error);
  SemanticErrorList *tail = head;
  for (const SemanticErrorList *it = list_a->next; it != NULL; it = it->next) {
    tail->next = semantic_errorlist_prepend(arena, NULL, it->error);
    tail = tail->next;
  }
  tail->next = list_b; // share b wholesale
  return head;
}

size_t semantic_errorlist_length(SemanticErrorList *list) {
  size_t n = 0;
  for (const SemanticErrorList *it = list; it != NULL; it = it->next)
    n++;
  return n;
}
