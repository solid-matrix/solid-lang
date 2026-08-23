/**
 * @file parser.c
 * @brief Parser implementation.
 * @author solid-matrix
 * @version 0.0.5
 */

#include <assert.h>
#include <stdbool.h>

#include "ast.h"
#include "xmem.h"
#include "parser.h"

#pragma region PARSER

Parser *parser_create(const Source *source) {
  Parser *parser = xmalloc(sizeof(Parser));
  *parser = (Parser){.source = source};
  return parser;
}

void parser_destroy(Parser *parser) {
  assert(parser != NULL);

  xfree(parser);
}

SyntaxError syntax_error_create(SyntaxErrorCode code, Span span) {
  return (SyntaxError){.code = code, .span = span};
}

SyntaxErrorList *syntax_error_list_create() { return NULL; }

void syntax_error_list_append(SyntaxErrorList **list, SyntaxError error) {
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

void syntax_error_list_merge(SyntaxErrorList **dst, SyntaxErrorList **src) {
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

void syntax_error_list_free(SyntaxErrorList **list) {
  SyntaxErrorList *node = *list;

  while (node != NULL) {
    SyntaxErrorList *tmp = node->next;
    xfree(node);
    node = tmp;
  }
  *list = NULL;
}

ParserResult parser_result_not_match(Span span) {
  return (ParserResult){
      .matched = false,
      .errors = NULL,
      .node = NULL,
      .rem = span,
  };
}

ParserResult parser_result_matched(Span rem, SyntaxNode *node,
                                   SyntaxErrorList *errors) {
  return (ParserResult){
      .matched = true,
      .errors = errors,
      .node = node,
      .rem = rem,
  };
}

#pragma endregion
