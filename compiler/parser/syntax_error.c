#include "syntax_error.h"
#include "xmem.h"

SyntaxError syntax_error_create(SyntaxErrorCode code, Span span) {
  return (SyntaxError){.code = code, .span = span};
}

SyntaxErrorList *syntax_errorlist_create() { return NULL; }

void syntax_errorlist_append(SyntaxErrorList **list, SyntaxError error) {
  SyntaxErrorList *new = xmalloc(sizeof(SyntaxErrorList));
  *new = (SyntaxErrorList){.error = error, .next = NULL};

  if (*list == NULL) {
    *list = new;
    return;
  }

  SyntaxErrorList *node = *list;
  while (node->next != NULL)
    node = node->next;

  node->next = new;
}

void syntax_errorlist_merge(SyntaxErrorList **dst, SyntaxErrorList **src) {
  if (*src == NULL)
    return;

  if (*dst == NULL) {
    *dst = *src;
    *src = NULL;
    return;
  }
  SyntaxErrorList *node = *dst;
  while (node->next != NULL)
    node = node->next;

  node->next = *src;
  *src = NULL;
}

void syntax_errorlist_free(SyntaxErrorList **list) {
  SyntaxErrorList *node = *list;

  while (node != NULL) {
    SyntaxErrorList *tmp = node->next;
    xfree(node);
    node = tmp;
  }
  *list = NULL;
}