#include "parser_result.h"
#include "syntax_error.h"
#include "syntax_node.h"

ParserResult parser_result_not_match(Span span) {
  return (ParserResult){
      .matched = false,
      .errors = syntax_errorlist_empty(),
      .node = NULL,
      .rem = span,
  };
}

ParserResult parser_result_matched(Span rem, SyntaxNode *node,
                                   SyntaxErrorList *errors) {
  return (ParserResult){
      .matched = true,
      .rem = rem,
      .node = node,
      .errors = errors,
  };
}

bool parser_result_is_ok(ParserResult result) {
  return result.matched && result.errors == NULL;
}