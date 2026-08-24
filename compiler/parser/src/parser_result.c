#include "parser_result.h"

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
  // errors == NULL simply means "no diagnostics": the common case
  // costs no allocation at all.
  return (ParserResult){
      .matched = true,
      .rem = rem,
      .node = node,
      .errors = errors,
  };
}