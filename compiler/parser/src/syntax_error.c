#include <assert.h>

#include "syntax_error.h"

SyntaxError syntax_error_create(SyntaxErrorCode code, Span span) {
  return (SyntaxError){.code = code, .span = span};
}

void syntax_errorlist_append(Arena *arena, SyntaxErrorList **list,
                             SyntaxError error) {
  assert(arena != NULL);
  assert(list != NULL);

  SyntaxErrorList *node = arena_alloc(arena, sizeof(SyntaxErrorList));
  *node = (SyntaxErrorList){.error = error, .next = NULL};

  if (*list == NULL) {
    *list = node;
    return;
  }

  SyntaxErrorList *tail = *list;
  while (tail->next != NULL)
    tail = tail->next;

  tail->next = node;
}

void syntax_errorlist_merge(SyntaxErrorList **dst, SyntaxErrorList **src) {
  assert(dst != NULL);
  assert(src != NULL);
  assert(dst != src);

  if (*src == NULL)
    return;

  if (*dst == NULL) {
    *dst = *src;
    *src = NULL;
    return;
  }

  SyntaxErrorList *tail = *dst;
  while (tail->next != NULL)
    tail = tail->next;

  tail->next = *src;
  *src = NULL;
}
