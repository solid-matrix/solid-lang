/**
 * @file parse_expr.c
 * @brief Expression parsers.
 * @author solid-matrix
 * @version 0.0.5
 */

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include "error.h"
#include "node.h"
#include "parse.h"
#include "span.h"
#include "strview.h"
#include "syntax_error.h"

SyntaxNodeResult parse_expr(const SyntaxParser *parser, Span span) {
  if (span_is_empty(span))
    return syntax_node_result_not_match(span);

  return parse_logical_or_expr(parser, span);
}

SyntaxNodeResult parse_logical_or_expr(const SyntaxParser *parser, Span span) {

  SyntaxNodeResult xor_res = parse_logical_xor_expr(parser, span);
  if (!xor_res.matched)
    return syntax_node_result_not_match(span);

  SyntaxErrorList *errors = xor_res.errors;
  SyntaxNode *left = xor_res.node;
  Span rem = xor_res.rem;

  while (true) {
    SyntaxMatchResult mres = match(parser, skip_trivia(parser->source, rem), OPERATOR_LOR);
    if (!mres.matched)
      break;
    rem = mres.rem;

    xor_res = parse_logical_xor_expr(parser, skip_trivia(parser->source, rem));
    if (!xor_res.matched) {
      SyntaxError error = syntax_error_create(SYNTAX_EXPECTED_EXPR, rem);
      errors = syntax_errorlist_prepend(parser->arena, errors, error);
    } else {
      rem = xor_res.rem;
      errors = syntax_errorlist_concat(parser->arena, xor_res.errors, errors);
    }

    SyntaxBinaryExpr *expr = arena_alloc(parser->arena, sizeof(SyntaxBinaryExpr));
    expr->header = syntax_node_create(SYNTAX_KIND_BINARY_EXPR, span_consumed(span, rem));
    expr->operator = SYNTAX_OPERATOR_LOR;
    expr->left = left;
    expr->right = xor_res.node;

    left = (SyntaxNode *)expr;
  }

  return syntax_node_result_matched(rem, left, errors);
}

SyntaxNodeResult parse_logical_xor_expr(const SyntaxParser *parser, Span span) {

  SyntaxNodeResult and_res = parse_logical_and_expr(parser, span);
  if (!and_res.matched)
    return syntax_node_result_not_match(span);

  SyntaxErrorList *errors = and_res.errors;
  SyntaxNode *left = and_res.node;
  Span rem = and_res.rem;

  while (true) {
    SyntaxMatchResult mres = match(parser, skip_trivia(parser->source, rem), OPERATOR_LXOR);
    if (!mres.matched)
      break;
    rem = mres.rem;

    and_res = parse_logical_and_expr(parser, skip_trivia(parser->source, rem));
    if (!and_res.matched) {
      SyntaxError error = syntax_error_create(SYNTAX_EXPECTED_EXPR, rem);
      errors = syntax_errorlist_prepend(parser->arena, errors, error);
    } else {
      rem = and_res.rem;
      errors = syntax_errorlist_concat(parser->arena, and_res.errors, errors);
    }

    SyntaxBinaryExpr *expr = arena_alloc(parser->arena, sizeof(SyntaxBinaryExpr));
    expr->header = syntax_node_create(SYNTAX_KIND_BINARY_EXPR, span_consumed(span, rem));
    expr->operator = SYNTAX_OPERATOR_LXOR;
    expr->left = left;
    expr->right = and_res.node;

    left = (SyntaxNode *)expr;
  }

  return syntax_node_result_matched(rem, left, errors);
}

SyntaxNodeResult parse_logical_and_expr(const SyntaxParser *parser, Span span) {

  SyntaxNodeResult rel_res = parse_relational_expr(parser, span);
  if (!rel_res.matched)
    return syntax_node_result_not_match(span);

  SyntaxErrorList *errors = rel_res.errors;
  SyntaxNode *left = rel_res.node;
  Span rem = rel_res.rem;

  while (true) {
    SyntaxMatchResult mres = match(parser, skip_trivia(parser->source, rem), OPERATOR_LAND);
    if (!mres.matched)
      break;
    rem = mres.rem;

    rel_res = parse_relational_expr(parser, skip_trivia(parser->source, rem));
    if (!rel_res.matched) {
      SyntaxError error = syntax_error_create(SYNTAX_EXPECTED_EXPR, rem);
      errors = syntax_errorlist_prepend(parser->arena, errors, error);
    } else {
      rem = rel_res.rem;
      errors = syntax_errorlist_concat(parser->arena, rel_res.errors, errors);
    }

    SyntaxBinaryExpr *expr = arena_alloc(parser->arena, sizeof(SyntaxBinaryExpr));
    expr->header = syntax_node_create(SYNTAX_KIND_BINARY_EXPR, span_consumed(span, rem));
    expr->operator = SYNTAX_OPERATOR_LAND;
    expr->left = left;
    expr->right = rel_res.node;

    left = (SyntaxNode *)expr;
  }

  return syntax_node_result_matched(rem, left, errors);
}

SyntaxNodeResult parse_relational_expr(const SyntaxParser *parser, Span span) {

  const static Strview OP_STRS[] = {OPERATOR_EQ, OPERATOR_NEQ, OPERATOR_LTE, OPERATOR_GTE, OPERATOR_LT, OPERATOR_GT};
  const static SyntaxOperator OPS[] = {SYNTAX_OPERATOR_EQ,  SYNTAX_OPERATOR_NEQ, SYNTAX_OPERATOR_LTE,
                                       SYNTAX_OPERATOR_GTE, SYNTAX_OPERATOR_LT,  SYNTAX_OPERATOR_GT};

  SyntaxNodeResult bw_res = parse_bitwise_expr(parser, span);
  if (!bw_res.matched)
    return syntax_node_result_not_match(span);

  SyntaxErrorList *errors = bw_res.errors;
  SyntaxNode *left = bw_res.node;
  Span rem = bw_res.rem;

  while (true) {
    Span adv = skip_trivia(parser->source, rem);

    SyntaxOperator op = SYNTAX_OPERATOR_INVALID;
    for (size_t i = 0; i < COUNT_OF(OP_STRS); i++) {
      SyntaxMatchResult mres = match(parser, adv, OP_STRS[i]);
      if (mres.matched) {
        op = OPS[i];
        rem = mres.rem;
        break;
      }
    }

    if (op == SYNTAX_OPERATOR_INVALID)
      break;

    bw_res = parse_bitwise_expr(parser, skip_trivia(parser->source, rem));
    if (!bw_res.matched) {
      SyntaxError error = syntax_error_create(SYNTAX_EXPECTED_EXPR, rem);
      errors = syntax_errorlist_prepend(parser->arena, errors, error);
    } else {
      rem = bw_res.rem;
      errors = syntax_errorlist_concat(parser->arena, bw_res.errors, errors);
    }

    SyntaxBinaryExpr *expr = arena_alloc(parser->arena, sizeof(SyntaxBinaryExpr));
    expr->header = syntax_node_create(SYNTAX_KIND_BINARY_EXPR, span_consumed(span, rem));
    expr->operator = op;
    expr->left = left;
    expr->right = bw_res.node;

    left = (SyntaxNode *)expr;
  }

  return syntax_node_result_matched(rem, left, errors);
}

SyntaxNodeResult parse_bitwise_expr(const SyntaxParser *parser, Span span) {

  const static Strview OP_STRS[] = {OPERATOR_BAND, OPERATOR_BOR, OPERATOR_BXOR};
  const static Strview OP_NOT_STRS[] = {OPERATOR_LAND, OPERATOR_LOR, OPERATOR_LXOR};
  const static SyntaxOperator OPS[] = {SYNTAX_OPERATOR_BAND, SYNTAX_OPERATOR_BOR, SYNTAX_OPERATOR_BXOR};

  SyntaxNodeResult sh_res = parse_shift_expr(parser, span);
  if (!sh_res.matched)
    return syntax_node_result_not_match(span);

  SyntaxErrorList *errors = sh_res.errors;
  SyntaxNode *left = sh_res.node;
  Span rem = sh_res.rem;

  while (true) {
    Span adv = skip_trivia(parser->source, rem);

    SyntaxOperator op = SYNTAX_OPERATOR_INVALID;
    for (size_t i = 0; i < COUNT_OF(OP_STRS); i++) {
      SyntaxMatchResult mres = match(parser, adv, OP_STRS[i]);
      SyntaxMatchResult mres_not = match(parser, adv, OP_NOT_STRS[i]);
      if (mres.matched && !mres_not.matched) {
        op = OPS[i];
        rem = mres.rem;
        break;
      }
    }

    if (op == SYNTAX_OPERATOR_INVALID)
      break;

    sh_res = parse_shift_expr(parser, skip_trivia(parser->source, rem));
    if (!sh_res.matched) {
      SyntaxError error = syntax_error_create(SYNTAX_EXPECTED_EXPR, rem);
      errors = syntax_errorlist_prepend(parser->arena, errors, error);
    } else {
      rem = sh_res.rem;
      errors = syntax_errorlist_concat(parser->arena, sh_res.errors, errors);
    }

    SyntaxBinaryExpr *expr = arena_alloc(parser->arena, sizeof(SyntaxBinaryExpr));
    expr->header = syntax_node_create(SYNTAX_KIND_BINARY_EXPR, span_consumed(span, rem));
    expr->operator = op;
    expr->left = left;
    expr->right = sh_res.node;

    left = (SyntaxNode *)expr;
  }

  return syntax_node_result_matched(rem, left, errors);
}

SyntaxNodeResult parse_shift_expr(const SyntaxParser *parser, Span span) {

  const static Strview OP_STRS[] = {OPERATOR_SHL, OPERATOR_SHR};
  const static SyntaxOperator OPS[] = {SYNTAX_OPERATOR_SHL, SYNTAX_OPERATOR_SHR};

  SyntaxNodeResult add_res = parse_additive_expr(parser, span);
  if (!add_res.matched)
    return syntax_node_result_not_match(span);

  SyntaxErrorList *errors = add_res.errors;
  SyntaxNode *left = add_res.node;
  Span rem = add_res.rem;

  while (true) {
    Span adv = skip_trivia(parser->source, rem);

    SyntaxOperator op = SYNTAX_OPERATOR_INVALID;
    for (size_t i = 0; i < COUNT_OF(OP_STRS); i++) {
      SyntaxMatchResult mres = match(parser, adv, OP_STRS[i]);
      if (mres.matched) {
        op = OPS[i];
        rem = mres.rem;
        break;
      }
    }

    if (op == SYNTAX_OPERATOR_INVALID)
      break;

    add_res = parse_additive_expr(parser, skip_trivia(parser->source, rem));
    if (!add_res.matched) {
      SyntaxError error = syntax_error_create(SYNTAX_EXPECTED_EXPR, rem);
      errors = syntax_errorlist_prepend(parser->arena, errors, error);
    } else {
      rem = add_res.rem;
      errors = syntax_errorlist_concat(parser->arena, add_res.errors, errors);
    }

    SyntaxBinaryExpr *expr = arena_alloc(parser->arena, sizeof(SyntaxBinaryExpr));
    expr->header = syntax_node_create(SYNTAX_KIND_BINARY_EXPR, span_consumed(span, rem));
    expr->operator = op;
    expr->left = left;
    expr->right = add_res.node;

    left = (SyntaxNode *)expr;
  }

  return syntax_node_result_matched(rem, left, errors);
}

SyntaxNodeResult parse_additive_expr(const SyntaxParser *parser, Span span) {

  const static Strview OP_STRS[] = {OPERATOR_ADD, OPERATOR_SUB};
  const static SyntaxOperator OPS[] = {SYNTAX_OPERATOR_ADD, SYNTAX_OPERATOR_SUB};

  SyntaxNodeResult mul_res = parse_multiplicative_expr(parser, span);
  if (!mul_res.matched)
    return syntax_node_result_not_match(span);

  SyntaxErrorList *errors = mul_res.errors;
  SyntaxNode *left = mul_res.node;
  Span rem = mul_res.rem;

  while (true) {
    Span adv = skip_trivia(parser->source, rem);

    SyntaxOperator op = SYNTAX_OPERATOR_INVALID;
    for (size_t i = 0; i < COUNT_OF(OP_STRS); i++) {
      SyntaxMatchResult mres = match(parser, adv, OP_STRS[i]);
      if (mres.matched) {
        op = OPS[i];
        rem = mres.rem;
        break;
      }
    }

    if (op == SYNTAX_OPERATOR_INVALID)
      break;

    mul_res = parse_multiplicative_expr(parser, skip_trivia(parser->source, rem));
    if (!mul_res.matched) {
      SyntaxError error = syntax_error_create(SYNTAX_EXPECTED_EXPR, rem);
      errors = syntax_errorlist_prepend(parser->arena, errors, error);
    } else {
      rem = mul_res.rem;
      errors = syntax_errorlist_concat(parser->arena, mul_res.errors, errors);
    }

    SyntaxBinaryExpr *expr = arena_alloc(parser->arena, sizeof(SyntaxBinaryExpr));
    expr->header = syntax_node_create(SYNTAX_KIND_BINARY_EXPR, span_consumed(span, rem));
    expr->operator = op;
    expr->left = left;
    expr->right = mul_res.node;

    left = (SyntaxNode *)expr;
  }

  return syntax_node_result_matched(rem, left, errors);
}

SyntaxNodeResult parse_multiplicative_expr(const SyntaxParser *parser, Span span) {

  const static Strview OP_STRS[] = {OPERATOR_MUL, OPERATOR_DIV, OPERATOR_MOD};
  const static SyntaxOperator OPS[] = {SYNTAX_OPERATOR_MUL, SYNTAX_OPERATOR_DIV, SYNTAX_OPERATOR_MOD};

  SyntaxNodeResult un_res = parse_unary_expr(parser, span);
  if (!un_res.matched)
    return syntax_node_result_not_match(span);

  SyntaxErrorList *errors = un_res.errors;
  SyntaxNode *left = un_res.node;
  Span rem = un_res.rem;

  while (true) {
    Span adv = skip_trivia(parser->source, rem);

    SyntaxOperator op = SYNTAX_OPERATOR_INVALID;
    for (size_t i = 0; i < COUNT_OF(OP_STRS); i++) {
      SyntaxMatchResult mres = match(parser, adv, OP_STRS[i]);
      if (mres.matched) {
        op = OPS[i];
        rem = mres.rem;
        break;
      }
    }

    if (op == SYNTAX_OPERATOR_INVALID)
      break;

    un_res = parse_unary_expr(parser, skip_trivia(parser->source, rem));
    if (!un_res.matched) {
      SyntaxError error = syntax_error_create(SYNTAX_EXPECTED_EXPR, rem);
      errors = syntax_errorlist_prepend(parser->arena, errors, error);
    } else {
      rem = un_res.rem;
      errors = syntax_errorlist_concat(parser->arena, un_res.errors, errors);
    }

    SyntaxBinaryExpr *expr = arena_alloc(parser->arena, sizeof(SyntaxBinaryExpr));
    expr->header = syntax_node_create(SYNTAX_KIND_BINARY_EXPR, span_consumed(span, rem));
    expr->operator = op;
    expr->left = left;
    expr->right = un_res.node;

    left = (SyntaxNode *)expr;
  }

  return syntax_node_result_matched(rem, left, errors);
}

SyntaxNodeResult parse_unary_expr(const SyntaxParser *parser, Span span) {

  const static Strview OP_STRS[] = {OPERATOR_MINUS, OPERATOR_PLUS, OPERATOR_LNOT, OPERATOR_BNOT, OPERATOR_DEREF};
  const static SyntaxOperator OPS[] = {SYNTAX_OPERATOR_MINUS, SYNTAX_OPERATOR_PLUS, SYNTAX_OPERATOR_LNOT,
                                       SYNTAX_OPERATOR_BNOT, SYNTAX_OPERATOR_DEREF};

  Span rem = span;
  SyntaxErrorList *errors = syntax_errorlist_empty();

  SyntaxOperator op = SYNTAX_OPERATOR_INVALID;
  for (size_t i = 0; i < COUNT_OF(OP_STRS); i++) {
    SyntaxMatchResult mres = match(parser, span, OP_STRS[i]);
    if (mres.matched) {
      op = OPS[i];
      rem = mres.rem;
      break;
    }
  }

  if (op == SYNTAX_OPERATOR_INVALID) {
    return parse_postfix_expr(parser, span);
  }

  SyntaxNodeResult un_res = parse_unary_expr(parser, skip_trivia(parser->source, rem));
  if (!un_res.matched) {
    SyntaxError error = syntax_error_create(SYNTAX_EXPECTED_EXPR, rem);
    errors = syntax_errorlist_prepend(parser->arena, errors, error);
  } else {
    rem = un_res.rem;
    errors = syntax_errorlist_concat(parser->arena, un_res.errors, errors);
  }

  SyntaxUnaryExpr *expr = arena_alloc(parser->arena, sizeof(SyntaxUnaryExpr));
  expr->header = syntax_node_create(SYNTAX_KIND_UNARY_EXPR, span_consumed(span, rem));
  expr->operator = op;
  expr->operand = un_res.node;

  return syntax_node_result_matched(rem, (SyntaxNode *)expr, errors);
}

/**
 * @brief Parses `.name` onto @p receiver.
 * @param parser Parsing context.
 * @param span Where the construct starts.
 * @param receiver The receiver expression.
 * @return Parse outcome; see SyntaxNodeResult.
 */
static SyntaxNodeResult parse_dot_expr(const SyntaxParser *parser, Span span, SyntaxNode *receiver) {
  SyntaxMatchResult mres = match(parser, span, PUNCTUATION_DOT);
  if (!mres.matched)
    return syntax_node_result_not_match(span);

  Span rem = mres.rem;
  SyntaxErrorList *errors = syntax_errorlist_empty();

  SyntaxNodeResult id_res = parse_identifier(parser, skip_trivia(parser->source, rem));
  if (!id_res.matched) {
    SyntaxError error = syntax_error_create(SYNTAX_EXPECTED_IDENTIFIER, rem);
    errors = syntax_errorlist_prepend(parser->arena, errors, error);
  } else {
    rem = id_res.rem;
    errors = syntax_errorlist_concat(parser->arena, id_res.errors, errors);
  }

  SyntaxDotExpr *node = arena_alloc(parser->arena, sizeof(SyntaxDotExpr));
  node->header = syntax_node_create(SYNTAX_KIND_DOT_EXPR, span_consumed(span, rem));
  node->receiver = receiver;
  node->id = (SyntaxIdentifier *)id_res.node;

  return syntax_node_result_matched(rem, (SyntaxNode *)node, errors);
}

/**
 * @brief Parses `[index]` onto @p receiver.
 * @param parser Parsing context.
 * @param span Where the construct starts.
 * @param receiver The receiver expression.
 * @return Parse outcome; see SyntaxNodeResult.
 */
static SyntaxNodeResult parse_index_expr(const SyntaxParser *parser, Span span, SyntaxNode *receiver) {
  SyntaxMatchResult mres = match(parser, span, PUNCTUATION_LBRACKET);
  if (!mres.matched) {
    return syntax_node_result_not_match(span);
  }

  Span rem = mres.rem;
  SyntaxErrorList *errors = syntax_errorlist_empty();

  SyntaxNodeResult ex_res = parse_expr(parser, skip_trivia(parser->source, rem));
  if (!ex_res.matched) {
    SyntaxError error = syntax_error_create(SYNTAX_EXPECTED_EXPR, rem);
    errors = syntax_errorlist_prepend(parser->arena, errors, error);
  } else {
    rem = ex_res.rem;
    errors = syntax_errorlist_concat(parser->arena, ex_res.errors, errors);
  }

  mres = match(parser, skip_trivia(parser->source, rem), PUNCTUATION_RBRACKET);
  if (!mres.matched) {
    SyntaxError error = syntax_error_create(SYNTAX_EXPECTED_RBRACKET, rem);
    errors = syntax_errorlist_prepend(parser->arena, errors, error);
  } else {
    rem = mres.rem;
  }

  SyntaxIndexExpr *node = arena_alloc(parser->arena, sizeof(SyntaxIndexExpr));
  node->header = syntax_node_create(SYNTAX_KIND_INDEX_EXPR, span_consumed(span, rem));
  node->receiver = receiver;
  node->index = ex_res.node;

  return syntax_node_result_matched(rem, (SyntaxNode *)node, errors);
}

/**
 * @brief Parses `(args)` onto @p receiver.
 * @param parser Parsing context.
 * @param span Where the construct starts.
 * @param receiver The receiver expression.
 * @return Parse outcome; see SyntaxNodeResult.
 */
static SyntaxNodeResult parse_call_expr(const SyntaxParser *parser, Span span, SyntaxNode *receiver) {
  SyntaxMatchResult mres = match(parser, span, PUNCTUATION_LPAREN);
  if (!mres.matched) {
    return syntax_node_result_not_match(span);
  }

  Span rem = mres.rem;
  SyntaxErrorList *errors = syntax_errorlist_empty();
  SyntaxNodeList *args = syntax_nodelist_empty();

  SyntaxListResult lres = parse_expr_list(parser, skip_trivia(parser->source, rem), PUNCTUATION_COMMA);
  args = lres.list;
  errors = syntax_errorlist_concat(parser->arena, lres.errors, errors);
  rem = lres.rem;

  mres = match(parser, skip_trivia(parser->source, rem), PUNCTUATION_RPAREN);
  if (!mres.matched) {
    SyntaxError error = syntax_error_create(SYNTAX_EXPECTED_RPAREN, rem);
    errors = syntax_errorlist_prepend(parser->arena, errors, error);
  } else {
    rem = mres.rem;
  }

  SyntaxCallExpr *node = arena_alloc(parser->arena, sizeof(SyntaxCallExpr));
  node->header = syntax_node_create(SYNTAX_KIND_CALL_EXPR, span_consumed(span, rem));
  node->receiver = receiver;
  node->args = args;

  return syntax_node_result_matched(rem, (SyntaxNode *)node, errors);
}

SyntaxNodeResult parse_postfix_expr(const SyntaxParser *parser, Span span) {
  SyntaxNodeResult op_res = parse_primary_expr(parser, span);
  if (!op_res.matched)
    return syntax_node_result_not_match(span);

  SyntaxErrorList *errors = op_res.errors;
  SyntaxNode *node = op_res.node;
  Span rem = op_res.rem;

  while (true) {
    Span adv = skip_trivia(parser->source, rem);

    SyntaxNodeResult dot_res = parse_dot_expr(parser, adv, node);
    if (dot_res.matched) {
      errors = syntax_errorlist_concat(parser->arena, dot_res.errors, errors);
      node = dot_res.node;
      rem = dot_res.rem;
      continue;
    }

    SyntaxNodeResult index_res = parse_index_expr(parser, adv, node);
    if (index_res.matched) {
      errors = syntax_errorlist_concat(parser->arena, index_res.errors, errors);
      node = index_res.node;
      rem = index_res.rem;
      continue;
    }

    SyntaxNodeResult call_res = parse_call_expr(parser, adv, node);
    if (call_res.matched) {
      errors = syntax_errorlist_concat(parser->arena, call_res.errors, errors);
      node = call_res.node;
      rem = call_res.rem;
      continue;
    }

    break; // no postfix follows; trivia belongs to the enclosing construct
  }

  return syntax_node_result_matched(rem, node, errors);
}

SyntaxNodeResult parse_primary_expr(const SyntaxParser *parser, Span span) {
  SyntaxNodeResult results[] = {
      parse_int_lit_expr(parser, span),    parse_float_lit_expr(parser, span), parse_rune_lit_expr(parser, span),
      parse_string_lit_expr(parser, span), parse_named_expr(parser, span),     parse_sub_expr(parser, span),
      parse_struct_lit_expr(parser, span), parse_array_lit_expr(parser, span), parse_compile_time(parser, span),
  };
  return complete_longest_match(results, sizeof(results) / sizeof(results[0]));
}

SyntaxNodeResult parse_named_expr(const SyntaxParser *parser, Span span) {
  SyntaxListResult path = parse_identifier_list(parser, span, PUNCTUATION_SCOPE);
  if (syntax_nodelist_is_empty(path.list))
    return syntax_node_result_not_match(span);

  SyntaxMatchResult mres = match(parser, skip_trivia(parser->source, path.rem), PUNCTUATION_LT);
  if (mres.matched) {
    Span t_rem = mres.rem;
    SyntaxListResult glist = parse_generic_arg_list(parser, skip_trivia(parser->source, t_rem));

    bool closed = false;
    SyntaxMatchResult close = match(parser, skip_trivia(parser->source, glist.rem), PUNCTUATION_GT);
    if (close.matched) {
      closed = true;
      t_rem = close.rem;
    }

    if (closed) {
      const static Strview FOLLOW[] = {PUNCTUATION_LPAREN,    PUNCTUATION_RPAREN, PUNCTUATION_LBRACKET,
                                       PUNCTUATION_RBRACKET,  PUNCTUATION_LBRACE, PUNCTUATION_RBRACE,
                                       PUNCTUATION_SEMICOLON, PUNCTUATION_COMMA,  PUNCTUATION_DOT};
      Span adv = skip_trivia(parser->source, t_rem);

      bool gated = false;
      for (size_t i = 0; i < COUNT_OF(FOLLOW); i++) {
        if (match(parser, adv, FOLLOW[i]).matched) {
          gated = true;
          break;
        }
      }

      if (gated) {
        SyntaxErrorList *errors = glist.errors;
        if (syntax_nodelist_is_empty(glist.list)) {
          errors =
              syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_IDENTIFIER, t_rem));
        }

        SyntaxNamed *node = arena_alloc(parser->arena, sizeof(SyntaxNamed));
        node->header = syntax_node_create(SYNTAX_KIND_NAMED, span_consumed(span, t_rem));
        node->path = path.list;
        node->generic_args = glist.list;

        return syntax_node_result_matched(t_rem, (SyntaxNode *)node, errors);
      }
    }
  }

  SyntaxNamed *node = arena_alloc(parser->arena, sizeof(SyntaxNamed));
  node->header = syntax_node_create(SYNTAX_KIND_NAMED, span_consumed(span, path.rem));
  node->path = path.list;
  node->generic_args = syntax_nodelist_empty();

  return syntax_node_result_matched(path.rem, (SyntaxNode *)node, path.errors);
}

SyntaxNodeResult parse_sub_expr(const SyntaxParser *parser, Span span) {
  SyntaxMatchResult mres = match(parser, span, PUNCTUATION_LPAREN);
  if (!mres.matched)
    return syntax_node_result_not_match(span);

  Span rem = mres.rem;
  SyntaxErrorList *errors = syntax_errorlist_empty();

  SyntaxNodeResult expr = parse_expr(parser, skip_trivia(parser->source, rem));
  if (!expr.matched) {
    SyntaxError error = syntax_error_create(SYNTAX_EXPECTED_EXPR, rem);
    errors = syntax_errorlist_prepend(parser->arena, errors, error);
  } else {
    rem = expr.rem;
    errors = syntax_errorlist_concat(parser->arena, expr.errors, errors);
  }

  mres = match(parser, skip_trivia(parser->source, rem), PUNCTUATION_RPAREN);
  if (!mres.matched) {
    SyntaxError error = syntax_error_create(SYNTAX_EXPECTED_RPAREN, rem);
    errors = syntax_errorlist_prepend(parser->arena, errors, error);
  } else {
    rem = mres.rem;
  }

  return syntax_node_result_matched(rem, expr.node, errors);
}
