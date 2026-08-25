#include <assert.h>

#include "syntax_error.h"

SyntaxError syntax_error_create(SyntaxErrorCode code, Span span) {
  return (SyntaxError){.code = code, .span = span};
}

SyntaxErrorList *syntax_errorlist_empty(void) { return NULL; }

SyntaxErrorList *syntax_errorlist_from_array(Arena *arena,
                                             const SyntaxError *errors,
                                             size_t count) {
  assert(arena != NULL);
  assert(count == 0 || errors != NULL);

  SyntaxErrorList *list = NULL;
  for (size_t i = count; i > 0; i--) // build back-to-front: O(n)
    list = syntax_errorlist_prepend(arena, list, errors[i - 1]);
  return list;
}

SyntaxErrorList *syntax_errorlist_prepend(Arena *arena, SyntaxErrorList *list,
                                          SyntaxError error) {
  assert(arena != NULL);

  SyntaxErrorList *cell =
      arena_alloc(arena, sizeof(SyntaxErrorList)); // OOM is fatal
  cell->error = error;
  cell->next = list;
  return cell;
}

SyntaxErrorList *syntax_errorlist_append(Arena *arena, SyntaxErrorList *list,
                                         SyntaxError error) {
  assert(arena != NULL);

  if (list == NULL)
    return syntax_errorlist_prepend(arena, NULL, error);

  // Copy every cell so the source list stays valid and unchanged.
  SyntaxErrorList *head = syntax_errorlist_prepend(arena, NULL, list->error);
  SyntaxErrorList *tail = head;
  for (const SyntaxErrorList *it = list->next; it != NULL; it = it->next) {
    tail->next = syntax_errorlist_prepend(arena, NULL, it->error);
    tail = tail->next;
  }
  tail->next = syntax_errorlist_prepend(arena, NULL, error);
  return head;
}

SyntaxError syntax_errorlist_head(SyntaxErrorList *list) {
  assert(!syntax_errorlist_is_empty(list));

  return list->error;
}

SyntaxErrorList *syntax_errorlist_tail(SyntaxErrorList *list) {
  assert(!syntax_errorlist_is_empty(list));

  return list->next;
}

SyntaxError syntax_errorlist_at(SyntaxErrorList *list, size_t n) {
  const SyntaxErrorList *it = list;
  while (n-- > 0) {
    assert(!syntax_errorlist_is_empty(it)); // out of range

    it = it->next;
  }
  assert(!syntax_errorlist_is_empty(it));

  return it->error;
}

bool syntax_errorlist_is_empty(const SyntaxErrorList *list) {
  return list == NULL;
}

SyntaxErrorList *syntax_errorlist_reverse(Arena *arena, SyntaxErrorList *list) {
  assert(arena != NULL);

  SyntaxErrorList *result = NULL;
  for (const SyntaxErrorList *it = list; it != NULL; it = it->next)
    result = syntax_errorlist_prepend(arena, result, it->error);
  return result;
}

SyntaxErrorList *syntax_errorlist_concat(Arena *arena, SyntaxErrorList *list_a,
                                         SyntaxErrorList *list_b) {
  assert(arena != NULL);

  if (syntax_errorlist_is_empty(list_a))
    return list_b; // shares b wholesale

  SyntaxErrorList *head = syntax_errorlist_prepend(arena, NULL, list_a->error);
  SyntaxErrorList *tail = head;
  for (const SyntaxErrorList *it = list_a->next; it != NULL; it = it->next) {
    tail->next = syntax_errorlist_prepend(arena, NULL, it->error);
    tail = tail->next;
  }
  tail->next = list_b; // share b wholesale
  return head;
}

size_t syntax_errorlist_length(SyntaxErrorList *list) {
  size_t n = 0;
  for (const SyntaxErrorList *it = list; it != NULL; it = it->next)
    n++;
  return n;
}
