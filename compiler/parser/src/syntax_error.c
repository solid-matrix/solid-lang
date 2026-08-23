#include <assert.h>

#include "syntax_error.h"
#include "xmem.h"

SyntaxError syntax_error_create(SyntaxErrorCode code, Span span) {
  return (SyntaxError){.code = code, .span = span};
}

SyntaxErrorList *syntax_errorlist_create(void) {
  SyntaxErrorList *list = xmalloc(sizeof(SyntaxErrorList));
  list->head = NULL;
  return list;
}

bool syntax_errorlist_is_empty(const SyntaxErrorList *list) {
  assert(list != NULL);

  return list->head == NULL;
}

void syntax_errorlist_append(SyntaxErrorList *list, SyntaxError error) {
  assert(list != NULL);

  SyntaxErrorListNode *node = xmalloc(sizeof(SyntaxErrorListNode));
  *node = (SyntaxErrorListNode){.error = error, .next = NULL};

  if (list->head == NULL) {
    list->head = node;
    return;
  }

  SyntaxErrorListNode *tail = list->head;
  while (tail->next != NULL)
    tail = tail->next;

  tail->next = node;
}

void syntax_errorlist_merge(SyntaxErrorList *dst, SyntaxErrorList *src) {
  assert(dst != NULL);
  assert(src != NULL);
  assert(dst != src);

  if (src->head == NULL)
    return;

  if (dst->head == NULL) {
    dst->head = src->head;
    src->head = NULL;
    return;
  }

  SyntaxErrorListNode *tail = dst->head;
  while (tail->next != NULL)
    tail = tail->next;

  tail->next = src->head;
  src->head = NULL;
}

void syntax_errorlist_destroy(SyntaxErrorList *list) {
  if (list == NULL)
    return;

  SyntaxErrorListNode *node = list->head;
  while (node != NULL) {
    SyntaxErrorListNode *next = node->next;
    xfree(node);
    node = next;
  }

  xfree(list);
}
