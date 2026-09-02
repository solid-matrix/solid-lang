/**
 * @file parse_stmt.c
 * @brief Statement parsers.
 * @author solid-matrix
 */

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include "parse.h"
#include "span.h"
#include "syntax_error.h"
#include "syntax_node.h"

SyntaxNodeResult parse_stmt(const SyntaxParser *parser, Span span) {
  // Keyword-led statements come before expr_stmt: a bare keyword whose
  // frame recovered consumes exactly as much as the fallback Named
  // expression would, and the keyword frame's more precise diagnostic
  // must win the tie.
  SyntaxNodeResult results[] = {
      parse_empty_stmt(parser, span), parse_body_stmt(parser, span),     parse_let_stmt(parser, span),
      parse_set_stmt(parser, span),   parse_if_stmt(parser, span),       parse_loop_stmt(parser, span),
      parse_break_stmt(parser, span), parse_continue_stmt(parser, span), parse_return_stmt(parser, span),
      parse_while_stmt(parser, span), parse_expr_stmt(parser, span),
  };
  return complete_longest_match(results, COUNT_OF(results));
}

SyntaxNodeResult parse_empty_stmt(const SyntaxParser *parser, Span span) {
  SyntaxMatchResult mres = match(parser, span, PUNCTUATION_SEMICOLON);
  if (!mres.matched)
    return syntax_node_result_not_match(span);

  SyntaxEmptyStmt *stmt = arena_alloc(parser->arena, sizeof(SyntaxEmptyStmt));
  stmt->header = syntax_node_create(SYNTAX_KIND_EMPTY_STMT, span_consumed(span, mres.rem));

  return syntax_node_result_matched(mres.rem, (SyntaxNode *)stmt, NULL);
}

SyntaxNodeResult parse_body_stmt(const SyntaxParser *parser, Span span) {
  SyntaxMatchResult mres = match(parser, span, PUNCTUATION_LBRACE);
  if (!mres.matched)
    return syntax_node_result_not_match(span);

  Span rem = mres.rem;
  SyntaxErrorList *errors = syntax_errorlist_empty();
  SyntaxNodeList *stmts = syntax_nodelist_empty();

  while (true) {
    Span adv = skip_trivia(parser->source, rem);

    // rem stays before the brace here; the closing check consumes it.
    if (match(parser, adv, PUNCTUATION_RBRACE).matched)
      break;

    SyntaxNodeResult res = parse_stmt(parser, adv);
    if (!res.matched)
      break; // neither a statement nor "}": the closing check reports

    rem = res.rem;
    errors = syntax_errorlist_concat(parser->arena, res.errors, errors);
    stmts = syntax_nodelist_prepend(parser->arena, stmts, res.node);
  }

  SyntaxMatchResult close = match(parser, skip_trivia(parser->source, rem), PUNCTUATION_RBRACE);
  if (!close.matched) {
    errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_RBRACE, rem));
  } else {
    rem = close.rem;
  }

  SyntaxBodyStmt *stmt = arena_alloc(parser->arena, sizeof(SyntaxBodyStmt));
  stmt->header = syntax_node_create(SYNTAX_KIND_BODY_STMT, span_consumed(span, rem));
  stmt->stmts = syntax_nodelist_reverse(parser->arena, stmts);

  return syntax_node_result_matched(rem, (SyntaxNode *)stmt, errors);
}

SyntaxNodeResult parse_let_stmt(const SyntaxParser *parser, Span span) {
  SyntaxMatchResult mres = match_keyword(parser, span, KEYWORD_LET);
  if (!mres.matched)
    return syntax_node_result_not_match(span);

  Span rem = mres.rem;
  SyntaxErrorList *errors = syntax_errorlist_empty();
  SyntaxIdentifier *id = NULL;
  SyntaxNode *type = NULL;
  SyntaxNode *value = NULL;

  SyntaxNodeResult id_res = parse_identifier(parser, skip_trivia(parser->source, rem));
  if (!id_res.matched) {
    errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_IDENTIFIER, rem));
  } else {
    rem = id_res.rem;
    id = (SyntaxIdentifier *)id_res.node;
    errors = syntax_errorlist_concat(parser->arena, id_res.errors, errors);
  }

  mres = match(parser, skip_trivia(parser->source, rem), PUNCTUATION_COLON);
  if (mres.matched) {
    rem = mres.rem;

    SyntaxNodeResult type_res = parse_type(parser, skip_trivia(parser->source, rem));
    if (!type_res.matched) {
      errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_TYPE, rem));
    } else {
      rem = type_res.rem;
      type = type_res.node;
      errors = syntax_errorlist_concat(parser->arena, type_res.errors, errors);
    }
  }

  mres = match(parser, skip_trivia(parser->source, rem), PUNCTUATION_EQUALS);
  if (!mres.matched) {
    errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_EQUALS, rem));
  } else {
    rem = mres.rem;
  }

  SyntaxNodeResult value_res = parse_expr(parser, skip_trivia(parser->source, rem));
  if (!value_res.matched) {
    errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_EXPR, rem));
  } else {
    rem = value_res.rem;
    value = value_res.node;
    errors = syntax_errorlist_concat(parser->arena, value_res.errors, errors);
  }

  mres = match(parser, skip_trivia(parser->source, rem), PUNCTUATION_SEMICOLON);
  if (!mres.matched) {
    errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_SEMICOLON, rem));
  } else {
    rem = mres.rem;
  }

  SyntaxLetStmt *stmt = arena_alloc(parser->arena, sizeof(SyntaxLetStmt));
  stmt->header = syntax_node_create(SYNTAX_KIND_LET_STMT, span_consumed(span, rem));
  stmt->id = id;
  stmt->type = type;
  stmt->value = value;

  return syntax_node_result_matched(rem, (SyntaxNode *)stmt, errors);
}

SyntaxNodeResult parse_set_stmt(const SyntaxParser *parser, Span span) {
  SyntaxMatchResult mres = match_keyword(parser, span, KEYWORD_SET);
  if (!mres.matched)
    return syntax_node_result_not_match(span);

  Span rem = mres.rem;
  SyntaxErrorList *errors = syntax_errorlist_empty();
  SyntaxNode *left = NULL;
  SyntaxNode *right = NULL;

  SyntaxNodeResult left_res = parse_expr(parser, skip_trivia(parser->source, rem));
  if (!left_res.matched) {
    errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_EXPR, rem));
  } else {
    rem = left_res.rem;
    left = left_res.node;
    errors = syntax_errorlist_concat(parser->arena, left_res.errors, errors);
  }

  mres = match(parser, skip_trivia(parser->source, rem), PUNCTUATION_EQUALS);
  if (!mres.matched) {
    errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_EQUALS, rem));
  } else {
    rem = mres.rem;
  }

  SyntaxNodeResult right_res = parse_expr(parser, skip_trivia(parser->source, rem));
  if (!right_res.matched) {
    errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_EXPR, rem));
  } else {
    rem = right_res.rem;
    right = right_res.node;
    errors = syntax_errorlist_concat(parser->arena, right_res.errors, errors);
  }

  mres = match(parser, skip_trivia(parser->source, rem), PUNCTUATION_SEMICOLON);
  if (!mres.matched) {
    errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_SEMICOLON, rem));
  } else {
    rem = mres.rem;
  }

  SyntaxSetStmt *stmt = arena_alloc(parser->arena, sizeof(SyntaxSetStmt));
  stmt->header = syntax_node_create(SYNTAX_KIND_SET_STMT, span_consumed(span, rem));
  stmt->left = left;
  stmt->right = right;

  return syntax_node_result_matched(rem, (SyntaxNode *)stmt, errors);
}

SyntaxNodeResult parse_expr_stmt(const SyntaxParser *parser, Span span) {
  SyntaxNodeResult expr = parse_expr(parser, span);
  if (!expr.matched)
    return syntax_node_result_not_match(span);

  Span rem = expr.rem;
  SyntaxErrorList *errors = expr.errors;

  SyntaxMatchResult mres = match(parser, skip_trivia(parser->source, rem), PUNCTUATION_SEMICOLON);
  if (!mres.matched) {
    errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_SEMICOLON, rem));
  } else {
    rem = mres.rem;
  }

  SyntaxExprStmt *stmt = arena_alloc(parser->arena, sizeof(SyntaxExprStmt));
  stmt->header = syntax_node_create(SYNTAX_KIND_EXPR_STMT, span_consumed(span, rem));
  stmt->expr = expr.node;

  return syntax_node_result_matched(rem, (SyntaxNode *)stmt, errors);
}

SyntaxNodeResult parse_if_stmt(const SyntaxParser *parser, Span span) {
  SyntaxMatchResult mres = match_keyword(parser, span, KEYWORD_IF);
  if (!mres.matched)
    return syntax_node_result_not_match(span);

  Span rem = mres.rem;
  SyntaxErrorList *errors = syntax_errorlist_empty();
  SyntaxNode *condition = NULL;
  SyntaxNode *then_stmt = NULL;
  SyntaxNode *else_stmt = NULL;

  mres = match(parser, skip_trivia(parser->source, rem), PUNCTUATION_LPAREN);
  if (!mres.matched) {
    errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_LPAREN, rem));
  } else {
    rem = mres.rem;

    SyntaxNodeResult cond_res = parse_expr(parser, skip_trivia(parser->source, rem));
    if (!cond_res.matched) {
      errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_EXPR, rem));
    } else {
      rem = cond_res.rem;
      condition = cond_res.node;
      errors = syntax_errorlist_concat(parser->arena, cond_res.errors, errors);
    }

    mres = match(parser, skip_trivia(parser->source, rem), PUNCTUATION_RPAREN);
    if (!mres.matched) {
      errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_RPAREN, rem));
    } else {
      rem = mres.rem;
    }
  }

  SyntaxNodeResult then_res = parse_body_position(parser, rem);
  rem = then_res.rem;
  errors = syntax_errorlist_concat(parser->arena, then_res.errors, errors);
  then_stmt = then_res.node;

  mres = match_keyword(parser, skip_trivia(parser->source, rem), KEYWORD_ELSE);
  if (mres.matched) {
    rem = mres.rem;

    // else branch: BodyStmt | EmptyStmt | IfStmt (the else-if chain).
    Span adv = skip_trivia(parser->source, rem);
    SyntaxNodeResult results[] = {
        parse_body_stmt(parser, adv),
        parse_if_stmt(parser, adv),
        parse_empty_stmt(parser, adv),
    };
    SyntaxNodeResult else_res = complete_longest_match(results, COUNT_OF(results));
    if (!else_res.matched) {
      errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_BODY, rem));
    } else {
      rem = else_res.rem;
      else_stmt = else_res.node;
      errors = syntax_errorlist_concat(parser->arena, else_res.errors, errors);
    }
  }

  SyntaxIfStmt *stmt = arena_alloc(parser->arena, sizeof(SyntaxIfStmt));
  stmt->header = syntax_node_create(SYNTAX_KIND_IF_STMT, span_consumed(span, rem));
  stmt->condition = condition;
  stmt->then_stmt = then_stmt;
  stmt->else_stmt = else_stmt;

  return syntax_node_result_matched(rem, (SyntaxNode *)stmt, errors);
}

SyntaxNodeResult parse_loop_stmt(const SyntaxParser *parser, Span span) {
  SyntaxMatchResult mres = match_keyword(parser, span, KEYWORD_LOOP);
  if (!mres.matched)
    return syntax_node_result_not_match(span);

  Span rem = mres.rem;

  SyntaxNodeResult body_res = parse_body_position(parser, rem);
  rem = body_res.rem;

  SyntaxLoopStmt *stmt = arena_alloc(parser->arena, sizeof(SyntaxLoopStmt));
  stmt->header = syntax_node_create(SYNTAX_KIND_LOOP_STMT, span_consumed(span, rem));
  stmt->stmt = body_res.node;

  return syntax_node_result_matched(rem, (SyntaxNode *)stmt, body_res.errors);
}

SyntaxNodeResult parse_break_stmt(const SyntaxParser *parser, Span span) {
  SyntaxMatchResult mres = match_keyword(parser, span, KEYWORD_BREAK);
  if (!mres.matched)
    return syntax_node_result_not_match(span);

  Span rem = mres.rem;
  SyntaxErrorList *errors = syntax_errorlist_empty();

  mres = match(parser, skip_trivia(parser->source, rem), PUNCTUATION_SEMICOLON);
  if (!mres.matched) {
    errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_SEMICOLON, rem));
  } else {
    rem = mres.rem;
  }

  SyntaxBreakStmt *stmt = arena_alloc(parser->arena, sizeof(SyntaxBreakStmt));
  stmt->header = syntax_node_create(SYNTAX_KIND_BREAK_STMT, span_consumed(span, rem));

  return syntax_node_result_matched(rem, (SyntaxNode *)stmt, errors);
}

SyntaxNodeResult parse_continue_stmt(const SyntaxParser *parser, Span span) {
  SyntaxMatchResult mres = match_keyword(parser, span, KEYWORD_CONTINUE);
  if (!mres.matched)
    return syntax_node_result_not_match(span);

  Span rem = mres.rem;
  SyntaxErrorList *errors = syntax_errorlist_empty();

  mres = match(parser, skip_trivia(parser->source, rem), PUNCTUATION_SEMICOLON);
  if (!mres.matched) {
    errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_SEMICOLON, rem));
  } else {
    rem = mres.rem;
  }

  SyntaxContinueStmt *stmt = arena_alloc(parser->arena, sizeof(SyntaxContinueStmt));
  stmt->header = syntax_node_create(SYNTAX_KIND_CONTINUE_STMT, span_consumed(span, rem));

  return syntax_node_result_matched(rem, (SyntaxNode *)stmt, errors);
}

SyntaxNodeResult parse_return_stmt(const SyntaxParser *parser, Span span) {
  SyntaxMatchResult mres = match_keyword(parser, span, KEYWORD_RETURN);
  if (!mres.matched)
    return syntax_node_result_not_match(span);

  Span rem = mres.rem;
  SyntaxErrorList *errors = syntax_errorlist_empty();
  SyntaxNode *expr = NULL;

  // The expression is the default reading; a bare "return;" is the only
  // case where the attempt fails and the statement still parses cleanly.
  // A failed attempt with no following ";" is reported once below.
  SyntaxNodeResult expr_res = parse_expr(parser, skip_trivia(parser->source, rem));
  if (expr_res.matched) {
    rem = expr_res.rem;
    expr = expr_res.node;
    errors = syntax_errorlist_concat(parser->arena, expr_res.errors, errors);
  }

  mres = match(parser, skip_trivia(parser->source, rem), PUNCTUATION_SEMICOLON);
  if (!mres.matched) {
    errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_SEMICOLON, rem));
  } else {
    rem = mres.rem;
  }

  SyntaxReturnStmt *stmt = arena_alloc(parser->arena, sizeof(SyntaxReturnStmt));
  stmt->header = syntax_node_create(SYNTAX_KIND_RETURN_STMT, span_consumed(span, rem));
  stmt->expr = expr;

  return syntax_node_result_matched(rem, (SyntaxNode *)stmt, errors);
}

SyntaxNodeResult parse_while_stmt(const SyntaxParser *parser, Span span) {
  SyntaxMatchResult mres = match_keyword(parser, span, KEYWORD_WHILE);
  if (!mres.matched)
    return syntax_node_result_not_match(span);

  Span rem = mres.rem;
  SyntaxErrorList *errors = syntax_errorlist_empty();
  SyntaxNode *condition = NULL;

  mres = match(parser, skip_trivia(parser->source, rem), PUNCTUATION_LPAREN);
  if (!mres.matched) {
    errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_LPAREN, rem));
  } else {
    rem = mres.rem;

    SyntaxNodeResult cond_res = parse_expr(parser, skip_trivia(parser->source, rem));
    if (!cond_res.matched) {
      errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_EXPR, rem));
    } else {
      rem = cond_res.rem;
      condition = cond_res.node;
      errors = syntax_errorlist_concat(parser->arena, cond_res.errors, errors);
    }

    mres = match(parser, skip_trivia(parser->source, rem), PUNCTUATION_RPAREN);
    if (!mres.matched) {
      errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_RPAREN, rem));
    } else {
      rem = mres.rem;
    }
  }

  SyntaxNodeResult body_res = parse_body_position(parser, rem);
  rem = body_res.rem;
  errors = syntax_errorlist_concat(parser->arena, body_res.errors, errors);

  SyntaxWhileStmt *stmt = arena_alloc(parser->arena, sizeof(SyntaxWhileStmt));
  stmt->header = syntax_node_create(SYNTAX_KIND_WHILE_STMT, span_consumed(span, rem));
  stmt->condition = condition;
  stmt->stmt = body_res.node;

  return syntax_node_result_matched(rem, (SyntaxNode *)stmt, errors);
}
