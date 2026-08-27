#include <assert.h>

#include "arena.h"
#include "parse_internal.h"
#include "parser.h"
#include "parser_result.h"
#include "span.h"
#include "strview.h"
#include "syntax_error.h"
#include "syntax_node.h"

#define COUNT_OF(a) (sizeof(a) / sizeof((a)[0]))

ParserResult parse_identifier(const Parser *parser, Span span) {
  if (span_is_empty(span))
    return parser_result_not_match(span);

  uint8_t c = source_byte_at(parser->source, span.start);
  if (!is_letter_or_underscore(c))
    return parser_result_not_match(span);

  Span rem = span_advance(span, 1);
  while (!span_is_empty(rem)) {
    c = source_byte_at(parser->source, rem.start);
    if (!is_letter_digit_or_underscore(c))
      break;

    rem = span_advance(rem, 1);
  }

  SyntaxIdentifier *id = arena_alloc(parser->arena, sizeof(SyntaxIdentifier));
  id->header = syntax_node_header(SYNTAX_KIND_IDENTIFIER, span_consumed(span, rem));
  id->strview = source_strview_at(parser->source, span_consumed(span, rem));

  return parser_result_matched(rem, (SyntaxNode *)id, NULL);
}

ParserResult parse_compile_time(const Parser *parser, Span span) {
  ParserMatchResult mres = match(parser->source, span, PUNCTUATION_AT);
  if (!mres.matched) {
    return parser_result_not_match(span);
  }

  Span rem = mres.rem;
  SyntaxErrorList *errors = syntax_errorlist_empty();
  SyntaxNodeList *args = syntax_nodelist_empty();
  SyntaxIdentifier *name = NULL;

  ParserResult res = parse_identifier(parser, skip_trivia(parser->source, rem));
  if (!res.matched) {
    SyntaxError error = syntax_error_create(SYNTAX_EXPECTED_IDENTIFIER, rem);
    errors = syntax_errorlist_prepend(parser->arena, errors, error);
  } else {
    rem = res.rem;
    name = (SyntaxIdentifier *)res.node;
    errors = syntax_errorlist_concat(parser->arena, res.errors, errors);
  }

  mres = match(parser->source, skip_trivia(parser->source, rem), PUNCTUATION_LPAREN);
  if (mres.matched) {
    rem = mres.rem;

    ParserListResult lres = parse_expr_list(parser, skip_trivia(parser->source, rem), PUNCTUATION_COMMA);
    args = lres.list;
    errors = syntax_errorlist_concat(parser->arena, lres.errors, errors);
    rem = lres.rem;

    mres = match(parser->source, skip_trivia(parser->source, rem), PUNCTUATION_RPAREN);
    if (!mres.matched) {
      SyntaxError error = syntax_error_create(SYNTAX_EXPECTED_RPAREN, rem);
      errors = syntax_errorlist_prepend(parser->arena, errors, error);
    } else {
      rem = mres.rem;
    }
  }

  SyntaxCompileTime *node = arena_alloc(parser->arena, sizeof(SyntaxCompileTime));
  node->header = syntax_node_header(SYNTAX_KIND_COMPILE_TIME, span_consumed(span, rem));
  node->args = args;
  node->name = name;

  return parser_result_matched(rem, (SyntaxNode *)node, errors);
}

ParserResult parse_program(const Parser *parser, Span span) {
  span = skip_trivia(parser->source, span);

  SyntaxErrorList *errors = syntax_errorlist_empty();
  SyntaxNodeList *decls = syntax_nodelist_empty();
  Span rem = span;

  while (!span_is_empty(rem)) {
    rem = skip_trivia(parser->source, rem);

    ParserResult res = parse_decl(parser, rem);
    if (!res.matched)
      break;

    rem = res.rem;
    errors = syntax_errorlist_concat(parser->arena, res.errors, errors);
    decls = syntax_nodelist_prepend(parser->arena, decls, res.node);
  }

  SyntaxProgram *program = arena_alloc(parser->arena, sizeof(SyntaxProgram));
  program->header = syntax_node_header(SYNTAX_KIND_PROGRAM, span_consumed(span, rem));
  program->top_levels = decls;

  rem = skip_trivia(parser->source, rem);

  if (!span_is_empty(rem))
    errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_EOF, rem));

  return parser_result_matched(rem, (SyntaxNode *)program, errors);
}

ParserResult parse_type(const Parser *parser, Span span) {
  ParserResult results[] = {
      parse_named_type(parser, span),
      parse_ref_type(parser, span),
      parse_array_type(parser, span),
      parse_func_type(parser, span),
  };
  return complete_longest_match(results, sizeof(results) / sizeof(results[0]));
}

ParserResult parse_named_type(const Parser *parser, Span span) {
  ParserListResult lres = parse_identifier_list(parser, span, PUNCTUATION_SCOPE);
  if (syntax_nodelist_is_empty(lres.list))
    return parser_result_not_match(span);

  SyntaxNamedType *node = arena_alloc(parser->arena, sizeof(SyntaxNamedType));
  node->header = syntax_node_header(SYNTAX_KIND_NAMED_TYPE, span_consumed(span, lres.rem));
  node->path = lres.list;
  node->generic_arguments = syntax_nodelist_empty();

  return parser_result_matched(lres.rem, (SyntaxNode *)node, lres.errors);
}

ParserResult parse_ref_type(const Parser *parser, Span span) {
  ParserMatchResult mres = match(parser->source, span, PUNCTUATION_AMP);
  if (!mres.matched)
    return parser_result_not_match(span);

  Span rem = mres.rem;
  SyntaxErrorList *errors = syntax_errorlist_empty();
  SyntaxRefKind ref_kind = SYNTAX_REF_KIND_READWRITE;

  Span adv = skip_trivia(parser->source, rem);
  ParserMatchResult kw = match_keyword(parser->source, adv, KEYWORD_READONLY);
  if (kw.matched) {
    ref_kind = SYNTAX_REF_KIND_READONLY;
    rem = kw.rem;
  } else {
    kw = match_keyword(parser->source, adv, KEYWORD_WRITEONLY);
    if (kw.matched) {
      ref_kind = SYNTAX_REF_KIND_WRITEONLY;
      rem = kw.rem;
    }
  }

  ParserResult inner = parse_type(parser, skip_trivia(parser->source, rem));
  if (!inner.matched) {
    SyntaxError error = syntax_error_create(SYNTAX_EXPECTED_TYPE, rem);
    errors = syntax_errorlist_prepend(parser->arena, errors, error);
  } else {
    rem = inner.rem;
    errors = syntax_errorlist_concat(parser->arena, inner.errors, errors);
  }

  SyntaxRefType *node = arena_alloc(parser->arena, sizeof(SyntaxRefType));
  node->header = syntax_node_header(SYNTAX_KIND_REF_TYPE, span_consumed(span, rem));
  node->ref_kind = ref_kind;
  node->inner_type = inner.node;

  return parser_result_matched(rem, (SyntaxNode *)node, errors);
}

ParserResult parse_array_type(const Parser *parser, Span span) {
  // TODO
  (void)parser;
  (void)span;
  return parser_result_not_match(span);
}

ParserResult parse_func_type(const Parser *parser, Span span) {
  // TODO
  (void)parser;
  (void)span;
  return parser_result_not_match(span);
}

ParserResult parse_decl(const Parser *parser, Span span) {
  ParserResult results[] = {
      parse_namespace_decl(parser, span),
      parse_using_decl(parser, span),
      // TODO
  };
  return complete_longest_match(results, COUNT_OF(results));
}

ParserResult parse_namespace_decl(const Parser *parser, Span span) {
  ParserMatchResult mres = match_keyword(parser->source, span, KEYWORD_NAMESPACE);
  if (!mres.matched)
    return parser_result_not_match(span);

  Span rem = mres.rem;
  SyntaxErrorList *errors = syntax_errorlist_empty();
  SyntaxNodeList *segs = syntax_nodelist_empty();

  ParserListResult lres = parse_identifier_list(parser, skip_trivia(parser->source, rem), PUNCTUATION_SCOPE);
  if (syntax_nodelist_is_empty(lres.list)) {
    SyntaxError error = syntax_error_create(SYNTAX_EXPECTED_IDENTIFIER, rem);
    errors = syntax_errorlist_prepend(parser->arena, errors, error);
  }
  errors = syntax_errorlist_concat(parser->arena, lres.errors, errors);
  segs = lres.list;
  rem = lres.rem;

  mres = match(parser->source, skip_trivia(parser->source, rem), PUNCTUATION_SEMICOLON);
  if (!mres.matched) {
    errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_SEMICOLON, rem));
  } else {
    rem = mres.rem;
  }

  SyntaxNamespaceDecl *decl = arena_alloc(parser->arena, sizeof(SyntaxNamespaceDecl));
  decl->header = syntax_node_header(SYNTAX_KIND_NAMESPACE_DECL, span_consumed(span, rem));
  decl->path = segs;

  return parser_result_matched(rem, (SyntaxNode *)decl, errors);
}

ParserResult parse_using_decl(const Parser *parser, Span span) {
  ParserMatchResult mres = match_keyword(parser->source, span, KEYWORD_USING);
  if (!mres.matched)
    return parser_result_not_match(span);

  Span rem = mres.rem;
  SyntaxErrorList *errors = syntax_errorlist_empty();
  SyntaxNodeList *segs = syntax_nodelist_empty();

  ParserListResult lres = parse_identifier_list(parser, skip_trivia(parser->source, rem), PUNCTUATION_SCOPE);
  if (syntax_nodelist_is_empty(lres.list)) {
    SyntaxError error = syntax_error_create(SYNTAX_EXPECTED_IDENTIFIER, rem);
    errors = syntax_errorlist_prepend(parser->arena, errors, error);
  }
  errors = syntax_errorlist_concat(parser->arena, lres.errors, errors);
  segs = lres.list;
  rem = lres.rem;

  mres = match(parser->source, skip_trivia(parser->source, rem), PUNCTUATION_SEMICOLON);
  if (!mres.matched) {
    errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_SEMICOLON, rem));
  } else {
    rem = mres.rem;
  }

  SyntaxUsingDecl *decl = arena_alloc(parser->arena, sizeof(SyntaxUsingDecl));
  decl->header = syntax_node_header(SYNTAX_KIND_USING_DECL, span_consumed(span, rem));
  decl->path = segs;

  return parser_result_matched(rem, (SyntaxNode *)decl, errors);
}

ParserResult parse_let_decl(const Parser *parser, Span span) {
  // TODO
  (void)parser;
  (void)span;
  return parser_result_not_match(span);
}

ParserResult parse_struct_decl(const Parser *parser, Span span) {
  // TODO
  (void)parser;
  (void)span;
  return parser_result_not_match(span);
}

ParserResult parse_enum_decl(const Parser *parser, Span span) {
  // TODO
  (void)parser;
  (void)span;
  return parser_result_not_match(span);
}

ParserResult parse_union_decl(const Parser *parser, Span span) {
  // TODO
  (void)parser;
  (void)span;
  return parser_result_not_match(span);
}

ParserResult parse_variant_decl(const Parser *parser, Span span) {
  // TODO
  (void)parser;
  (void)span;
  return parser_result_not_match(span);
}

ParserResult parse_contract_decl(const Parser *parser, Span span) {
  // TODO
  (void)parser;
  (void)span;
  return parser_result_not_match(span);
}

ParserResult parse_func_decl(const Parser *parser, Span span) {
  // TODO
  (void)parser;
  (void)span;
  return parser_result_not_match(span);
}

ParserResult parse_stmt(const Parser *parser, Span span) {
  // TODO
  (void)parser;
  (void)span;
  return parser_result_not_match(span);
}

ParserResult parse_body_stmt(const Parser *parser, Span span) {
  // TODO
  (void)parser;
  (void)span;
  return parser_result_not_match(span);
}

ParserResult parse_let_stmt(const Parser *parser, Span span) {
  // TODO
  (void)parser;
  (void)span;
  return parser_result_not_match(span);
}

ParserResult parse_set_stmt(const Parser *parser, Span span) {
  // TODO
  (void)parser;
  (void)span;
  return parser_result_not_match(span);
}

ParserResult parse_expr_stmt(const Parser *parser, Span span) {
  // TODO
  (void)parser;
  (void)span;
  return parser_result_not_match(span);
}

ParserResult parse_if_stmt(const Parser *parser, Span span) {
  // TODO
  (void)parser;
  (void)span;
  return parser_result_not_match(span);
}

ParserResult parse_loop_stmt(const Parser *parser, Span span) {
  // TODO
  (void)parser;
  (void)span;
  return parser_result_not_match(span);
}

ParserResult parse_break_stmt(const Parser *parser, Span span) {
  // TODO
  (void)parser;
  (void)span;
  return parser_result_not_match(span);
}

ParserResult parse_continue_stmt(const Parser *parser, Span span) {
  // TODO
  (void)parser;
  (void)span;
  return parser_result_not_match(span);
}

ParserResult parse_return_stmt(const Parser *parser, Span span) {
  // TODO
  (void)parser;
  (void)span;
  return parser_result_not_match(span);
}

ParserResult parse_while_stmt(const Parser *parser, Span span) {
  // TODO
  (void)parser;
  (void)span;
  return parser_result_not_match(span);
}

static ParserResult parse_named_expr(const Parser *parser, Span span) {
  (void)parser;
  (void)span;
  return parser_result_not_match(span);
}

ParserResult parse_struct_lit_expr(const Parser *parser, Span span) {
  (void)parser;
  (void)span;
  return parser_result_not_match(span);
}

ParserResult parse_array_lit_expr(const Parser *parser, Span span) {
  (void)parser;
  (void)span;
  return parser_result_not_match(span);
}

static ParserResult parse_sub_expr(const Parser *parser, Span span) {
  ParserMatchResult mres = match(parser->source, span, PUNCTUATION_LPAREN);
  if (!mres.matched)
    return parser_result_not_match(span);

  Span rem = mres.rem;
  SyntaxErrorList *errors = syntax_errorlist_empty();

  ParserResult expr = parse_expr(parser, skip_trivia(parser->source, rem));
  if (!expr.matched) {
    SyntaxError error = syntax_error_create(SYNTAX_EXPECTED_EXPR, rem);
    errors = syntax_errorlist_prepend(parser->arena, errors, error);
  } else {
    rem = expr.rem;
    errors = syntax_errorlist_concat(parser->arena, expr.errors, errors);
  }

  mres = match(parser->source, skip_trivia(parser->source, rem), PUNCTUATION_RPAREN);
  if (!mres.matched) {
    SyntaxError error = syntax_error_create(SYNTAX_EXPECTED_RPAREN, rem);
    errors = syntax_errorlist_prepend(parser->arena, errors, error);
  } else {
    rem = mres.rem;
  }

  return parser_result_matched(rem, expr.node, errors);
}

static ParserResult parse_operand_expr(const Parser *parser, Span span) {
  ParserResult results[] = {
      parse_number_lit_expr(parser, span), parse_rune_lit_expr(parser, span), parse_string_lit_expr(parser, span),
      parse_named_expr(parser, span),      parse_sub_expr(parser, span),      parse_struct_lit_expr(parser, span),
      parse_array_lit_expr(parser, span),  parse_compile_time(parser, span),
  };
  return complete_longest_match(results, sizeof(results) / sizeof(results[0]));
}

static ParserResult parse_dot_expr(const Parser *parser, Span span, SyntaxNode *receiver) {
  ParserMatchResult mres = match(parser->source, span, PUNCTUATION_DOT);
  if (!mres.matched)
    return parser_result_not_match(span);

  Span rem = mres.rem;
  SyntaxErrorList *errors = syntax_errorlist_empty();

  ParserResult id_res = parse_identifier(parser, skip_trivia(parser->source, rem));
  if (!id_res.matched) {
    SyntaxError error = syntax_error_create(SYNTAX_EXPECTED_IDENTIFIER, rem);
    errors = syntax_errorlist_prepend(parser->arena, errors, error);
  } else {
    rem = id_res.rem;
    errors = syntax_errorlist_concat(parser->arena, id_res.errors, errors);
  }

  SyntaxDotExpr *node = arena_alloc(parser->arena, sizeof(SyntaxDotExpr));
  node->header = syntax_node_header(SYNTAX_KIND_DOT_EXPR, span_consumed(span, rem));
  node->receiver = receiver;
  node->name = (SyntaxIdentifier *)id_res.node;

  return parser_result_matched(rem, (SyntaxNode *)node, errors);
}

static ParserResult parse_index_expr(const Parser *parser, Span span, SyntaxNode *receiver) {
  ParserMatchResult mres = match(parser->source, span, PUNCTUATION_LBRACKET);
  if (!mres.matched) {
    return parser_result_not_match(span);
  }

  Span rem = mres.rem;
  SyntaxErrorList *errors = syntax_errorlist_empty();

  ParserResult ex_res = parse_expr(parser, skip_trivia(parser->source, rem));
  if (!ex_res.matched) {
    SyntaxError error = syntax_error_create(SYNTAX_EXPECTED_EXPR, rem);
    errors = syntax_errorlist_prepend(parser->arena, errors, error);
  } else {
    rem = ex_res.rem;
    errors = syntax_errorlist_concat(parser->arena, ex_res.errors, errors);
  }

  mres = match(parser->source, skip_trivia(parser->source, rem), PUNCTUATION_RBRACKET);
  if (!mres.matched) {
    SyntaxError error = syntax_error_create(SYNTAX_EXPECTED_RBRACKET, rem);
    errors = syntax_errorlist_prepend(parser->arena, errors, error);
  } else {
    rem = mres.rem;
  }

  SyntaxIndexExpr *node = arena_alloc(parser->arena, sizeof(SyntaxIndexExpr));
  node->header = syntax_node_header(SYNTAX_KIND_INDEX_EXPR, span_consumed(span, rem));
  node->receiver = receiver;
  node->index = ex_res.node;

  return parser_result_matched(rem, (SyntaxNode *)node, errors);
}

static ParserResult parse_call_expr(const Parser *parser, Span span, SyntaxNode *receiver) {
  ParserMatchResult mres = match(parser->source, span, PUNCTUATION_LPAREN);
  if (!mres.matched) {
    return parser_result_not_match(span);
  }

  Span rem = mres.rem;
  SyntaxErrorList *errors = syntax_errorlist_empty();
  SyntaxNodeList *args = syntax_nodelist_empty();

  ParserListResult lres = parse_expr_list(parser, skip_trivia(parser->source, rem), PUNCTUATION_COMMA);
  args = lres.list;
  errors = syntax_errorlist_concat(parser->arena, lres.errors, errors);
  rem = lres.rem;

  mres = match(parser->source, skip_trivia(parser->source, rem), PUNCTUATION_RPAREN);
  if (!mres.matched) {
    SyntaxError error = syntax_error_create(SYNTAX_EXPECTED_RPAREN, rem);
    errors = syntax_errorlist_prepend(parser->arena, errors, error);
  } else {
    rem = mres.rem;
  }

  SyntaxCallExpr *node = arena_alloc(parser->arena, sizeof(SyntaxCallExpr));
  node->header = syntax_node_header(SYNTAX_KIND_CALL_EXPR, span_consumed(span, rem));
  node->receiver = receiver;
  node->args = args;

  return parser_result_matched(rem, (SyntaxNode *)node, errors);
}

static ParserResult parse_postfix_expr(const Parser *parser, Span span) {
  ParserResult op_res = parse_operand_expr(parser, span);
  if (!op_res.matched)
    return parser_result_not_match(span);

  SyntaxErrorList *errors = op_res.errors;
  SyntaxNode *node = op_res.node;
  Span rem = op_res.rem;

  while (true) {
    Span adv = skip_trivia(parser->source, rem);

    ParserResult dot_res = parse_dot_expr(parser, adv, node);
    if (dot_res.matched) {
      errors = syntax_errorlist_concat(parser->arena, dot_res.errors, errors);
      node = dot_res.node;
      rem = dot_res.rem;
      continue;
    }

    ParserResult index_res = parse_index_expr(parser, adv, node);
    if (index_res.matched) {
      errors = syntax_errorlist_concat(parser->arena, index_res.errors, errors);
      node = index_res.node;
      rem = index_res.rem;
      continue;
    }

    ParserResult call_res = parse_call_expr(parser, adv, node);
    if (call_res.matched) {
      errors = syntax_errorlist_concat(parser->arena, call_res.errors, errors);
      node = call_res.node;
      rem = call_res.rem;
      continue;
    }

    break; // no postfix follows; trivia belongs to the enclosing construct
  }

  return parser_result_matched(rem, node, errors);
}

static ParserResult parse_unary_expr(const Parser *parser, Span span) {
  Span rem = span;
  SyntaxErrorList *errors = syntax_errorlist_empty();

  SyntaxOperator op;
  ParserMatchResult mres;

  mres = match(parser->source, span, OPERATOR_MINUS);
  if (mres.matched) {
    op = SYNTAX_OPERATOR_MINUS;
    rem = mres.rem;
    goto outside;
  }

  mres = match(parser->source, span, OPERATOR_PLUS);
  if (mres.matched) {
    op = SYNTAX_OPERATOR_PLUS;
    rem = mres.rem;
    goto outside;
  }

  mres = match(parser->source, span, OPERATOR_LNOT);
  if (mres.matched) {
    op = SYNTAX_OPERATOR_LNOT;
    rem = mres.rem;
    goto outside;
  }

  mres = match(parser->source, span, OPERATOR_BNOT);
  if (mres.matched) {
    op = SYNTAX_OPERATOR_BNOT;
    rem = mres.rem;
    goto outside;
  }

  mres = match(parser->source, span, OPERATOR_DEREF);
  if (mres.matched) {
    op = SYNTAX_OPERATOR_DEREF;
    rem = mres.rem;
    goto outside;
  }

  return parse_postfix_expr(parser, span);

outside:;

  ParserResult un_res = parse_unary_expr(parser, skip_trivia(parser->source, rem));
  if (!un_res.matched) {
    SyntaxError error = syntax_error_create(SYNTAX_EXPECTED_EXPR, rem);
    errors = syntax_errorlist_prepend(parser->arena, errors, error);
  } else {
    rem = un_res.rem;
    errors = syntax_errorlist_concat(parser->arena, un_res.errors, errors);
  }

  SyntaxUnaryExpr *expr = arena_alloc(parser->arena, sizeof(SyntaxUnaryExpr));
  expr->header = syntax_node_header(SYNTAX_KIND_UNARY_EXPR, span_consumed(span, rem));
  expr->operator = op;
  expr->operand = un_res.node;

  return parser_result_matched(rem, (SyntaxNode *)expr, errors);
}

static ParserResult parse_multiplicative_expr(const Parser *parser, Span span) {

  ParserResult un_res = parse_unary_expr(parser, span);
  if (!un_res.matched)
    return parser_result_not_match(span);

  SyntaxErrorList *errors = un_res.errors;
  SyntaxNode *left = un_res.node;
  Span rem = un_res.rem;

  while (true) {
    Span adv = skip_trivia(parser->source, rem);

    SyntaxOperator op;

    ParserMatchResult mres;
    mres = match(parser->source, adv, OPERATOR_MUL);
    if (mres.matched) {
      op = SYNTAX_OPERATOR_MUL;
      rem = mres.rem;
      goto outside;
    }

    mres = match(parser->source, adv, OPERATOR_DIV);
    if (mres.matched) {
      op = SYNTAX_OPERATOR_DIV;
      rem = mres.rem;
      goto outside;
    }

    mres = match(parser->source, adv, OPERATOR_MOD);
    if (mres.matched) {
      op = SYNTAX_OPERATOR_MOD;
      rem = mres.rem;
      goto outside;
    }

    break;

  outside:;

    un_res = parse_unary_expr(parser, skip_trivia(parser->source, rem));
    if (!un_res.matched) {
      SyntaxError error = syntax_error_create(SYNTAX_EXPECTED_EXPR, rem);
      errors = syntax_errorlist_prepend(parser->arena, errors, error);
    } else {
      rem = un_res.rem;
      errors = syntax_errorlist_concat(parser->arena, un_res.errors, errors);
    }

    SyntaxBinaryExpr *expr = arena_alloc(parser->arena, sizeof(SyntaxBinaryExpr));
    expr->header = syntax_node_header(SYNTAX_KIND_BINARY_EXPR, span_consumed(span, rem));
    expr->operator = op;
    expr->left = left;
    expr->right = un_res.node;

    left = (SyntaxNode *)expr;
  }

  return parser_result_matched(rem, left, errors);
}

static ParserResult parse_additive_expr(const Parser *parser, Span span) {

  ParserResult mul_res = parse_multiplicative_expr(parser, span);
  if (!mul_res.matched)
    return parser_result_not_match(span);

  SyntaxErrorList *errors = mul_res.errors;
  SyntaxNode *left = mul_res.node;
  Span rem = mul_res.rem;

  while (true) {
    Span adv = skip_trivia(parser->source, rem);

    SyntaxOperator op;

    ParserMatchResult mres;
    mres = match(parser->source, adv, OPERATOR_ADD);
    if (mres.matched) {
      op = SYNTAX_OPERATOR_ADD;
      rem = mres.rem;
      goto outside;
    }

    mres = match(parser->source, adv, OPERATOR_SUB);
    if (mres.matched) {
      op = SYNTAX_OPERATOR_SUB;
      rem = mres.rem;
      goto outside;
    }

    break;

  outside:;

    mul_res = parse_multiplicative_expr(parser, skip_trivia(parser->source, rem));
    if (!mul_res.matched) {
      SyntaxError error = syntax_error_create(SYNTAX_EXPECTED_EXPR, rem);
      errors = syntax_errorlist_prepend(parser->arena, errors, error);
    } else {
      rem = mul_res.rem;
      errors = syntax_errorlist_concat(parser->arena, mul_res.errors, errors);
    }

    SyntaxBinaryExpr *expr = arena_alloc(parser->arena, sizeof(SyntaxBinaryExpr));
    expr->header = syntax_node_header(SYNTAX_KIND_BINARY_EXPR, span_consumed(span, rem));
    expr->operator = op;
    expr->left = left;
    expr->right = mul_res.node;

    left = (SyntaxNode *)expr;
  }

  return parser_result_matched(rem, left, errors);
}

static ParserResult parse_shift_expr(const Parser *parser, Span span) {

  ParserResult add_res = parse_additive_expr(parser, span);
  if (!add_res.matched)
    return parser_result_not_match(span);

  SyntaxErrorList *errors = add_res.errors;
  SyntaxNode *left = add_res.node;
  Span rem = add_res.rem;

  while (true) {
    Span adv = skip_trivia(parser->source, rem);

    SyntaxOperator op;

    ParserMatchResult mres;
    mres = match(parser->source, adv, OPERATOR_SHL);
    if (mres.matched) {
      op = SYNTAX_OPERATOR_SHL;
      rem = mres.rem;
      goto outside;
    }

    mres = match(parser->source, adv, OPERATOR_SHR);
    if (mres.matched) {
      op = SYNTAX_OPERATOR_SHR;
      rem = mres.rem;
      goto outside;
    }

    break;

  outside:;

    add_res = parse_additive_expr(parser, skip_trivia(parser->source, rem));
    if (!add_res.matched) {
      SyntaxError error = syntax_error_create(SYNTAX_EXPECTED_EXPR, rem);
      errors = syntax_errorlist_prepend(parser->arena, errors, error);
    } else {
      rem = add_res.rem;
      errors = syntax_errorlist_concat(parser->arena, add_res.errors, errors);
    }

    SyntaxBinaryExpr *expr = arena_alloc(parser->arena, sizeof(SyntaxBinaryExpr));
    expr->header = syntax_node_header(SYNTAX_KIND_BINARY_EXPR, span_consumed(span, rem));
    expr->operator = op;
    expr->left = left;
    expr->right = add_res.node;

    left = (SyntaxNode *)expr;
  }

  return parser_result_matched(rem, left, errors);
}

static ParserResult parse_bitwise_expr(const Parser *parser, Span span) {

  ParserResult sh_res = parse_shift_expr(parser, span);
  if (!sh_res.matched)
    return parser_result_not_match(span);

  SyntaxErrorList *errors = sh_res.errors;
  SyntaxNode *left = sh_res.node;
  Span rem = sh_res.rem;

  while (true) {
    Span adv = skip_trivia(parser->source, rem);

    SyntaxOperator op;

    ParserMatchResult mres, mres2;

    mres = match(parser->source, adv, OPERATOR_BAND);
    mres2 = match(parser->source, adv, OPERATOR_LAND);
    if (mres.matched && !mres2.matched) {
      op = SYNTAX_OPERATOR_BAND;
      rem = mres.rem;
      goto outside;
    }

    mres = match(parser->source, adv, OPERATOR_BOR);
    mres2 = match(parser->source, adv, OPERATOR_LOR);
    if (mres.matched && !mres2.matched) {
      op = SYNTAX_OPERATOR_BOR;
      rem = mres.rem;
      goto outside;
    }

    mres = match(parser->source, adv, OPERATOR_BXOR);
    mres2 = match(parser->source, adv, OPERATOR_LXOR);
    if (mres.matched && !mres2.matched) {
      op = SYNTAX_OPERATOR_BXOR;
      rem = mres.rem;
      goto outside;
    }

    break;

  outside:;

    sh_res = parse_shift_expr(parser, skip_trivia(parser->source, rem));
    if (!sh_res.matched) {
      SyntaxError error = syntax_error_create(SYNTAX_EXPECTED_EXPR, rem);
      errors = syntax_errorlist_prepend(parser->arena, errors, error);
    } else {
      rem = sh_res.rem;
      errors = syntax_errorlist_concat(parser->arena, sh_res.errors, errors);
    }

    SyntaxBinaryExpr *expr = arena_alloc(parser->arena, sizeof(SyntaxBinaryExpr));
    expr->header = syntax_node_header(SYNTAX_KIND_BINARY_EXPR, span_consumed(span, rem));
    expr->operator = op;
    expr->left = left;
    expr->right = sh_res.node;

    left = (SyntaxNode *)expr;
  }

  return parser_result_matched(rem, left, errors);
}

static ParserResult parse_relational_expr(const Parser *parser, Span span) {

  ParserResult bw_res = parse_bitwise_expr(parser, span);
  if (!bw_res.matched)
    return parser_result_not_match(span);

  SyntaxErrorList *errors = bw_res.errors;
  SyntaxNode *left = bw_res.node;
  Span rem = bw_res.rem;

  while (true) {
    Span adv = skip_trivia(parser->source, rem);

    SyntaxOperator op;

    ParserMatchResult mres;
    mres = match(parser->source, adv, OPERATOR_EQ);
    if (mres.matched) {
      op = SYNTAX_OPERATOR_EQ;
      rem = mres.rem;
      goto outside;
    }

    mres = match(parser->source, adv, OPERATOR_NEQ);
    if (mres.matched) {
      op = SYNTAX_OPERATOR_NEQ;
      rem = mres.rem;
      goto outside;
    }

    mres = match(parser->source, adv, OPERATOR_LTE);
    if (mres.matched) {
      op = SYNTAX_OPERATOR_LTE;
      rem = mres.rem;
      goto outside;
    }

    mres = match(parser->source, adv, OPERATOR_GTE);
    if (mres.matched) {
      op = SYNTAX_OPERATOR_GTE;
      rem = mres.rem;
      goto outside;
    }

    mres = match(parser->source, adv, OPERATOR_LT);
    if (mres.matched) {
      op = SYNTAX_OPERATOR_LT;
      rem = mres.rem;
      goto outside;
    }

    mres = match(parser->source, adv, OPERATOR_GT);
    if (mres.matched) {
      op = SYNTAX_OPERATOR_GT;
      rem = mres.rem;
      goto outside;
    }

    break;

  outside:;

    bw_res = parse_bitwise_expr(parser, skip_trivia(parser->source, rem));
    if (!bw_res.matched) {
      SyntaxError error = syntax_error_create(SYNTAX_EXPECTED_EXPR, rem);
      errors = syntax_errorlist_prepend(parser->arena, errors, error);
    } else {
      rem = bw_res.rem;
      errors = syntax_errorlist_concat(parser->arena, bw_res.errors, errors);
    }

    SyntaxBinaryExpr *expr = arena_alloc(parser->arena, sizeof(SyntaxBinaryExpr));
    expr->header = syntax_node_header(SYNTAX_KIND_BINARY_EXPR, span_consumed(span, rem));
    expr->operator = op;
    expr->left = left;
    expr->right = bw_res.node;

    left = (SyntaxNode *)expr;
  }

  return parser_result_matched(rem, left, errors);
}

static ParserResult parse_logical_and_expr(const Parser *parser, Span span) {

  ParserResult rel_res = parse_relational_expr(parser, span);
  if (!rel_res.matched)
    return parser_result_not_match(span);

  SyntaxErrorList *errors = rel_res.errors;
  SyntaxNode *left = rel_res.node;
  Span rem = rel_res.rem;

  while (true) {
    ParserMatchResult mres = match(parser->source, skip_trivia(parser->source, rem), OPERATOR_LAND);
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
    expr->header = syntax_node_header(SYNTAX_KIND_BINARY_EXPR, span_consumed(span, rem));
    expr->operator = SYNTAX_OPERATOR_LAND;
    expr->left = left;
    expr->right = rel_res.node;

    left = (SyntaxNode *)expr;
  }

  return parser_result_matched(rem, left, errors);
}

static ParserResult parse_logical_xor_expr(const Parser *parser, Span span) {

  ParserResult and_res = parse_logical_and_expr(parser, span);
  if (!and_res.matched)
    return parser_result_not_match(span);

  SyntaxErrorList *errors = and_res.errors;
  SyntaxNode *left = and_res.node;
  Span rem = and_res.rem;

  while (true) {
    ParserMatchResult mres = match(parser->source, skip_trivia(parser->source, rem), OPERATOR_LXOR);
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
    expr->header = syntax_node_header(SYNTAX_KIND_BINARY_EXPR, span_consumed(span, rem));
    expr->operator = SYNTAX_OPERATOR_LXOR;
    expr->left = left;
    expr->right = and_res.node;

    left = (SyntaxNode *)expr;
  }

  return parser_result_matched(rem, left, errors);
}

static ParserResult parse_logical_or_expr(const Parser *parser, Span span) {

  ParserResult xor_res = parse_logical_xor_expr(parser, span);
  if (!xor_res.matched)
    return parser_result_not_match(span);

  SyntaxErrorList *errors = xor_res.errors;
  SyntaxNode *left = xor_res.node;
  Span rem = xor_res.rem;

  while (true) {
    ParserMatchResult mres = match(parser->source, skip_trivia(parser->source, rem), OPERATOR_LOR);
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
    expr->header = syntax_node_header(SYNTAX_KIND_BINARY_EXPR, span_consumed(span, rem));
    expr->operator = SYNTAX_OPERATOR_LOR;
    expr->left = left;
    expr->right = xor_res.node;

    left = (SyntaxNode *)expr;
  }

  return parser_result_matched(rem, left, errors);
}

ParserResult parse_expr(const Parser *parser, Span span) {
  if (span_is_empty(span))
    return parser_result_not_match(span);

  return parse_logical_or_expr(parser, span);
}
