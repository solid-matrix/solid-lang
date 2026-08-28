#include "syntax_result.h"

SyntaxNodeResult syntax_node_result_not_match(Span span) {
  return (SyntaxNodeResult){
      .matched = false,
      .errors = syntax_errorlist_empty(),
      .node = NULL,
      .rem = span,
  };
}

SyntaxNodeResult syntax_node_result_matched(Span rem, SyntaxNode *node, SyntaxErrorList *errors) {
  return (SyntaxNodeResult){
      .matched = true,
      .rem = rem,
      .node = node,
      .errors = errors,
  };
}

bool syntax_node_result_is_ok(SyntaxNodeResult result) { return result.matched && result.errors == NULL; }
