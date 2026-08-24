#include <assert.h>

#include "syntax_error.h"
#include "xmem.h"

SyntaxError syntax_error_create(SyntaxErrorCode code, Span span) {
  return (SyntaxError){.code = code, .span = span};
}

void syntax_errorlist_append(SyntaxErrorList **list, SyntaxError error) {
  assert(list != NULL);

  SyntaxErrorList *node = xmalloc(sizeof(SyntaxErrorList));
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

void syntax_errorlist_destroy(SyntaxErrorList **list) {
  if (list == NULL)
    return;

  SyntaxErrorList *node = *list;
  while (node != NULL) {
    SyntaxErrorList *next = node->next;
    xfree(node);
    node = next;
  }

  *list = NULL;
}
