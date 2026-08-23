/**
 * @file parser.c
 * @brief Parser implementation.
 * @author solid-matrix
 * @version 0.0.5
 */

#include <assert.h>
#include <stdbool.h>

#include "parser.h"
#include "syntax_node.h"
#include "xmem.h"

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
