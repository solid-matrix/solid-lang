#include <assert.h>
#include <stdlib.h>

#include "parse_shared.h"
#include "parser.h"
#include "parser_result.h"
#include "span.h"
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
  *id = (SyntaxIdentifier){
      .header = syntax_node_header(SYNTAX_KIND_IDENTIFIER, span_consumed(span, rem)),
      .strview = source_strview_at(parser->source, span_consumed(span, rem)),
  };

  return parser_result_matched(rem, (SyntaxNode *)id, NULL);
}

ParserResult parse_name_path(const Parser *parser, Span span) {
  if (span_is_empty(span))
    return parser_result_not_match(span);

  Span rem = span;
  SyntaxNodeList *segments = syntax_nodelist_empty();
  SyntaxErrorList *errors = syntax_errorlist_empty();

  ParserResult id_res = parse_identifier(parser, rem);

  if (!id_res.matched)
    return parser_result_not_match(span);

  segments = syntax_nodelist_prepend(parser->arena, segments, id_res.node);
  rem = id_res.rem;

  while (true) {

    Span adv = skip_trivia(parser->source, rem);
    if (!(span_len(adv) >= 2 && source_byte_at(parser->source, adv.start) == ':' &&
          source_byte_at(parser->source, adv.start + 1) == ':')) {
      break;
    }
    rem = span_advance(adv, 2);

    adv = skip_trivia(parser->source, rem);
    id_res = parse_identifier(parser, adv);

    if (!id_res.matched) {
      errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_IDENTIFIER, rem));

      break;
    }

    segments = syntax_nodelist_prepend(parser->arena, segments, id_res.node);
    rem = id_res.rem;
  }

  SyntaxNamePath *path = arena_alloc(parser->arena, sizeof(SyntaxNamePath));
  *path = (SyntaxNamePath){
      .header = syntax_node_header(SYNTAX_KIND_NAME_PATH, span_consumed(span, rem)),
      .segments = segments,
  };

  return parser_result_matched(rem, (SyntaxNode *)path, errors);
}

ParserResult parse_compile_time(const Parser *parser, Span span) {
  // TODO
  (void)parser;
  (void)span;
  return parser_result_not_match(span);
}

ParserResult parse_program(const Parser *parser, Span span) {
  span = skip_trivia(parser->source, span);

  // Both accumulators build newest-at-head (prepend/concat-new-left),
  // by design: program.top_levels ends up in reverse source order and
  // errors in reverse chronological order, with any trailing
  // EXPECTED_EOF diagnostic at the very head.
  SyntaxErrorList *errors = syntax_errorlist_empty();
  SyntaxNodeList *decls = syntax_nodelist_empty();
  Span rem = span;

  while (!span_is_empty(rem)) {
    rem = skip_trivia(parser->source, rem);

    ParserResult res = parse_decl(parser, rem);
    if (!res.matched)
      break;

    assert(res.rem.start > rem.start);
    rem = res.rem;

    errors = syntax_errorlist_concat(parser->arena, res.errors, errors);

    if (res.node != NULL) {
      assert((res.node->kind & SYNTAX_KIND_DECL_MASK) != 0);
      decls = syntax_nodelist_prepend(parser->arena, decls, res.node);
    }
  }

  SyntaxProgram *program = arena_alloc(parser->arena, sizeof(SyntaxProgram));
  *program = (SyntaxProgram){
      .header = syntax_node_header(SYNTAX_KIND_PROGRAM, span_consumed(span, rem)),
      .top_levels = decls,
  };

  rem = skip_trivia(parser->source, rem);

  if (!span_is_empty(rem))
    errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_EOF, rem));

  return parser_result_matched(rem, (SyntaxNode *)program, errors);
}

ParserResult parse_type(const Parser *parser, Span span) {
  // TODO
  (void)parser;
  (void)span;
  return parser_result_not_match(span);
}

ParserResult parse_named_type(const Parser *parser, Span span) {
  // TODO
  (void)parser;
  (void)span;
  return parser_result_not_match(span);
}

ParserResult parse_ref_type(const Parser *parser, Span span) {
  // TODO
  (void)parser;
  (void)span;
  return parser_result_not_match(span);
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
  };

  // TODO

  return complete_longest_match(results, COUNT_OF(results));
}

ParserResult parse_namespace_decl(const Parser *parser, Span span) {
  Strview keyword = STRVIEW("namespace");

  if (!match_keyword(parser->source, span, keyword))
    return parser_result_not_match(span);

  Span rem = span_advance(span, keyword.len);
  Span adv = skip_trivia(parser->source, rem);

  SyntaxErrorList *errors = syntax_errorlist_empty();

  ParserResult np_res = parse_name_path(parser, adv);

  if (!np_res.matched) {
    errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_NAME_PATH, rem));
  } else {
    errors = syntax_errorlist_concat(parser->arena, np_res.errors, errors);
    rem = np_res.rem;

    adv = skip_trivia(parser->source, rem);

    if (!(span_len(adv) > 0 && source_byte_at(parser->source, adv.start) == ';')) {
      errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_SEMICOLON, rem));
    } else {
      rem = span_advance(adv, 1);
    }
  }

  SyntaxNamespaceDecl *decl = arena_alloc(parser->arena, sizeof(SyntaxNamespaceDecl));

  *decl = (SyntaxNamespaceDecl){
      .header = syntax_node_header(SYNTAX_KIND_NAMESPACE_DECL, span_consumed(span, rem)),
      .path = (SyntaxNamePath *)np_res.node,
  };

  return parser_result_matched(rem, (SyntaxNode *)decl, errors);
}
ParserResult parse_using_decl(const Parser *parser, Span span) {
  Strview keyword = STRVIEW("using");

  if (!match_keyword(parser->source, span, keyword))
    return parser_result_not_match(span);

  Span rem = span_advance(span, keyword.len);
  Span adv = skip_trivia(parser->source, rem);

  SyntaxErrorList *errors = syntax_errorlist_empty();

  ParserResult np_res = parse_name_path(parser, adv);

  if (!np_res.matched) {
    errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_NAME_PATH, rem));
  } else {
    errors = syntax_errorlist_concat(parser->arena, np_res.errors, errors);
    rem = np_res.rem;

    adv = skip_trivia(parser->source, rem);

    if (!(span_len(adv) > 0 && source_byte_at(parser->source, adv.start) == ';')) {
      errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_SEMICOLON, rem));
    } else {
      rem = span_advance(adv, 1);
    }
  }

  SyntaxUsingDecl *decl = arena_alloc(parser->arena, sizeof(SyntaxUsingDecl));

  *decl = (SyntaxUsingDecl){
      .header = syntax_node_header(SYNTAX_KIND_USING_DECL, span_consumed(span, rem)),
      .path = (SyntaxNamePath *)np_res.node,
  };

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
  if (!match(parser->source, span, PUNCTUATION_LPAREN))
    return parser_result_not_match(span);
  Span rem = span_advance(span, PUNCTUATION_LPAREN.len);

  SyntaxErrorList *errors = syntax_errorlist_empty();

  ParserResult expr = parse_expr(parser, skip_trivia(parser->source, rem));
  if (!expr.matched) {
    SyntaxError error = syntax_error_create(SYNTAX_EXPECTED_EXPR, rem);
    errors = syntax_errorlist_prepend(parser->arena, errors, error);
  } else {
    rem = expr.rem;
    errors = syntax_errorlist_concat(parser->arena, expr.errors, errors);
  }

  Span adv = skip_trivia(parser->source, rem);
  if (!match(parser->source, adv, PUNCTUATION_RPAREN)) {
    SyntaxError error = syntax_error_create(SYNTAX_EXPECTED_RPAREN, rem);
    errors = syntax_errorlist_prepend(parser->arena, errors, error);
  } else {
    rem = span_advance(adv, PUNCTUATION_RPAREN.len);
  }

  return parser_result_matched(rem, expr.node, errors);
}

static ParserResult parse_operand_expr(const Parser *parser, Span span) {
  ParserResult results[] = {
      parse_number_lit_expr(parser, span), parse_rune_lit_expr(parser, span), parse_string_lit_expr(parser, span),
      parse_named_expr(parser, span),      parse_sub_expr(parser, span),      parse_struct_lit_expr(parser, span),
      parse_array_lit_expr(parser, span),
  };

  return complete_longest_match(results, sizeof(results) / sizeof(results[0]));
}

static ParserResult parse_dot_expr(const Parser *parser, Span span, SyntaxNode *receiver) {
  SyntaxErrorList *errors = syntax_errorlist_empty();
  Span rem = span;

  if (!match(parser->source, rem, PUNCTUATION_DOT)) {
    return parser_result_not_match(span);
  } else {
    rem = span_advance(rem, PUNCTUATION_DOT.len);
  }

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
  Span rem = span;
  SyntaxErrorList *errors = syntax_errorlist_empty();

  if (!match(parser->source, rem, PUNCTUATION_LBRACKET)) {
    return parser_result_not_match(span);
  } else {
    rem = span_advance(rem, PUNCTUATION_LBRACKET.len);
  }

  ParserResult ex_res = parse_expr(parser, skip_trivia(parser->source, rem));
  if (!ex_res.matched) {
    SyntaxError error = syntax_error_create(SYNTAX_EXPECTED_EXPR, rem);
    errors = syntax_errorlist_prepend(parser->arena, errors, error);
  } else {
    rem = ex_res.rem;
    errors = syntax_errorlist_concat(parser->arena, ex_res.errors, errors);
  }

  Span adv = skip_trivia(parser->source, rem);
  if (!match(parser->source, adv, PUNCTUATION_RBRACKET)) {
    SyntaxError error = syntax_error_create(SYNTAX_EXPECTED_RBRACKET, rem);
    errors = syntax_errorlist_prepend(parser->arena, errors, error);
  } else {
    rem = span_advance(adv, PUNCTUATION_RBRACKET.len);
  }

  SyntaxIndexExpr *node = arena_alloc(parser->arena, sizeof(SyntaxIndexExpr));
  node->header = syntax_node_header(SYNTAX_KIND_INDEX_EXPR, span_consumed(span, rem));
  node->receiver = receiver;
  node->index = ex_res.node;

  return parser_result_matched(rem, (SyntaxNode *)node, errors);
}

static ParserResult parse_call_expr(const Parser *parser, Span span, SyntaxNode *receiver) {
  Span rem = span;
  SyntaxErrorList *errors = syntax_errorlist_empty();
  SyntaxNodeList *args = syntax_nodelist_empty();

  if (!match(parser->source, rem, PUNCTUATION_LPAREN)) {
    return parser_result_not_match(span);
  } else {
    rem = span_advance(rem, PUNCTUATION_LPAREN.len);
  }

  ParserResult ex_res = parse_expr(parser, skip_trivia(parser->source, rem));
  if (ex_res.matched) {
    errors = syntax_errorlist_concat(parser->arena, ex_res.errors, errors);
    args = syntax_nodelist_prepend(parser->arena, args, ex_res.node);
    rem = ex_res.rem;

    while (true) {
      Span adv = skip_trivia(parser->source, rem);
      if (!match(parser->source, adv, PUNCTUATION_COMMA))
        break;

      rem = span_advance(adv, PUNCTUATION_COMMA.len);

      ex_res = parse_expr(parser, skip_trivia(parser->source, rem));
      if (!ex_res.matched) {
        SyntaxError error = syntax_error_create(SYNTAX_EXPECTED_EXPR, rem);
        errors = syntax_errorlist_prepend(parser->arena, errors, error);
      } else {
        errors = syntax_errorlist_concat(parser->arena, ex_res.errors, errors);
        args = syntax_nodelist_prepend(parser->arena, args, ex_res.node);
        rem = ex_res.rem;
      }
    }
  }

  Span adv = skip_trivia(parser->source, rem);
  if (!match(parser->source, adv, PUNCTUATION_RPAREN)) {
    SyntaxError error = syntax_error_create(SYNTAX_EXPECTED_RPAREN, rem);
    errors = syntax_errorlist_prepend(parser->arena, errors, error);
  } else {
    rem = span_advance(adv, PUNCTUATION_RPAREN.len);
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
  if (match(parser->source, span, OPERATOR_MINUS)) {
    op = SYNTAX_OPERATOR_MINUS;
    rem = span_advance(span, OPERATOR_MINUS.len);
  } else if (match(parser->source, span, OPERATOR_PLUS)) {
    op = SYNTAX_OPERATOR_PLUS;
    rem = span_advance(span, OPERATOR_PLUS.len);
  } else if (match(parser->source, span, OPERATOR_LNOT)) {
    op = SYNTAX_OPERATOR_LNOT;
    rem = span_advance(span, OPERATOR_LNOT.len);
  } else if (match(parser->source, span, OPERATOR_BNOT)) {
    op = SYNTAX_OPERATOR_BNOT;
    rem = span_advance(span, OPERATOR_BNOT.len);
  } else if (match(parser->source, span, OPERATOR_DEREF)) {
    op = SYNTAX_OPERATOR_DEREF;
    rem = span_advance(span, OPERATOR_DEREF.len);
  } else {
    return parse_postfix_expr(parser, span);
  }

  ParserResult un_res = parse_unary_expr(parser, skip_trivia(parser->source, rem));
  if (!un_res.matched) {
    SyntaxError error = syntax_error_create(SYNTAX_EXPECTED_EXPR, rem);
    errors = syntax_errorlist_prepend(parser->arena, errors, error);
  } else {
    rem = un_res.rem;
    errors = syntax_errorlist_concat(parser->arena, un_res.errors, errors);
  }

  SyntaxUnaryExpr *expr = arena_alloc(parser->arena, sizeof(SyntaxUnaryExpr));
  *expr = (SyntaxUnaryExpr){
      .header = syntax_node_header(SYNTAX_KIND_UNARY_EXPR, span_consumed(span, rem)),
      .operator = op,
      .operand = un_res.node,
  };

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
    if (match(parser->source, adv, OPERATOR_MUL)) {
      op = SYNTAX_OPERATOR_MUL;
      rem = span_advance(adv, OPERATOR_MUL.len);
    } else if (match(parser->source, adv, OPERATOR_DIV)) {
      op = SYNTAX_OPERATOR_DIV;
      rem = span_advance(adv, OPERATOR_DIV.len);
    } else if (match(parser->source, adv, OPERATOR_MOD)) {
      op = SYNTAX_OPERATOR_MOD;
      rem = span_advance(adv, OPERATOR_MOD.len);
    } else {
      break;
    }

    un_res = parse_unary_expr(parser, skip_trivia(parser->source, rem));
    if (!un_res.matched) {
      SyntaxError error = syntax_error_create(SYNTAX_EXPECTED_EXPR, rem);
      errors = syntax_errorlist_prepend(parser->arena, errors, error);
    } else {
      rem = un_res.rem;
      errors = syntax_errorlist_concat(parser->arena, un_res.errors, errors);
    }

    SyntaxBinaryExpr *expr = arena_alloc(parser->arena, sizeof(SyntaxBinaryExpr));
    *expr = (SyntaxBinaryExpr){
        .header = syntax_node_header(SYNTAX_KIND_BINARY_EXPR, span_consumed(span, rem)),
        .operator = op,
        .left = left,
        .right = un_res.node,
    };

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
    if (match(parser->source, adv, OPERATOR_ADD)) {
      op = SYNTAX_OPERATOR_ADD;
      rem = span_advance(adv, OPERATOR_ADD.len);
    } else if (match(parser->source, adv, OPERATOR_SUB)) {
      op = SYNTAX_OPERATOR_SUB;
      rem = span_advance(adv, OPERATOR_SUB.len);
    } else {
      break;
    }

    mul_res = parse_multiplicative_expr(parser, skip_trivia(parser->source, rem));
    if (!mul_res.matched) {
      SyntaxError error = syntax_error_create(SYNTAX_EXPECTED_EXPR, rem);
      errors = syntax_errorlist_prepend(parser->arena, errors, error);
    } else {
      rem = mul_res.rem;
      errors = syntax_errorlist_concat(parser->arena, mul_res.errors, errors);
    }

    SyntaxBinaryExpr *expr = arena_alloc(parser->arena, sizeof(SyntaxBinaryExpr));
    *expr = (SyntaxBinaryExpr){
        .header = syntax_node_header(SYNTAX_KIND_BINARY_EXPR, span_consumed(span, rem)),
        .operator = op,
        .left = left,
        .right = mul_res.node,
    };

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
    if (match(parser->source, adv, OPERATOR_SHL)) {
      op = SYNTAX_OPERATOR_SHL;
      rem = span_advance(adv, OPERATOR_SHL.len);
    } else if (match(parser->source, adv, OPERATOR_SHR)) {
      op = SYNTAX_OPERATOR_SHR;
      rem = span_advance(adv, OPERATOR_SHR.len);
    } else {
      break;
    }

    add_res = parse_additive_expr(parser, skip_trivia(parser->source, rem));
    if (!add_res.matched) {
      SyntaxError error = syntax_error_create(SYNTAX_EXPECTED_EXPR, rem);
      errors = syntax_errorlist_prepend(parser->arena, errors, error);
    } else {
      rem = add_res.rem;
      errors = syntax_errorlist_concat(parser->arena, add_res.errors, errors);
    }

    SyntaxBinaryExpr *expr = arena_alloc(parser->arena, sizeof(SyntaxBinaryExpr));
    *expr = (SyntaxBinaryExpr){
        .header = syntax_node_header(SYNTAX_KIND_BINARY_EXPR, span_consumed(span, rem)),
        .operator = op,
        .left = left,
        .right = add_res.node,
    };

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
    if (match(parser->source, adv, OPERATOR_BAND) && !match(parser->source, adv, OPERATOR_LAND)) {
      op = SYNTAX_OPERATOR_BAND;
      rem = span_advance(adv, OPERATOR_BAND.len);
    } else if (match(parser->source, adv, OPERATOR_BOR) && !match(parser->source, adv, OPERATOR_LOR)) {
      op = SYNTAX_OPERATOR_BOR;
      rem = span_advance(adv, OPERATOR_BOR.len);
    } else if (match(parser->source, adv, OPERATOR_BXOR) && !match(parser->source, adv, OPERATOR_LXOR)) {
      op = SYNTAX_OPERATOR_BXOR;
      rem = span_advance(adv, OPERATOR_BXOR.len);
    } else {
      break;
    }

    sh_res = parse_shift_expr(parser, skip_trivia(parser->source, rem));
    if (!sh_res.matched) {
      SyntaxError error = syntax_error_create(SYNTAX_EXPECTED_EXPR, rem);
      errors = syntax_errorlist_prepend(parser->arena, errors, error);
    } else {
      rem = sh_res.rem;
      errors = syntax_errorlist_concat(parser->arena, sh_res.errors, errors);
    }

    SyntaxBinaryExpr *expr = arena_alloc(parser->arena, sizeof(SyntaxBinaryExpr));
    *expr = (SyntaxBinaryExpr){
        .header = syntax_node_header(SYNTAX_KIND_BINARY_EXPR, span_consumed(span, rem)),
        .operator = op,
        .left = left,
        .right = sh_res.node,
    };

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
    if (match(parser->source, adv, OPERATOR_EQ)) {
      op = SYNTAX_OPERATOR_EQ;
      rem = span_advance(adv, OPERATOR_EQ.len);
    } else if (match(parser->source, adv, OPERATOR_NEQ)) {
      op = SYNTAX_OPERATOR_NEQ;
      rem = span_advance(adv, OPERATOR_NEQ.len);
    } else if (match(parser->source, adv, OPERATOR_LTE)) {
      op = SYNTAX_OPERATOR_LTE;
      rem = span_advance(adv, OPERATOR_LTE.len);
    } else if (match(parser->source, adv, OPERATOR_GTE)) {
      op = SYNTAX_OPERATOR_GTE;
      rem = span_advance(adv, OPERATOR_GTE.len);
    } else if (match(parser->source, adv, OPERATOR_LT)) {
      op = SYNTAX_OPERATOR_LT;
      rem = span_advance(adv, OPERATOR_LT.len);
    } else if (match(parser->source, adv, OPERATOR_GT)) {
      op = SYNTAX_OPERATOR_GT;
      rem = span_advance(adv, OPERATOR_GT.len);
    } else {
      break;
    }

    bw_res = parse_bitwise_expr(parser, skip_trivia(parser->source, rem));
    if (!bw_res.matched) {
      SyntaxError error = syntax_error_create(SYNTAX_EXPECTED_EXPR, rem);
      errors = syntax_errorlist_prepend(parser->arena, errors, error);
    } else {
      rem = bw_res.rem;
      errors = syntax_errorlist_concat(parser->arena, bw_res.errors, errors);
    }

    SyntaxBinaryExpr *expr = arena_alloc(parser->arena, sizeof(SyntaxBinaryExpr));
    *expr = (SyntaxBinaryExpr){
        .header = syntax_node_header(SYNTAX_KIND_BINARY_EXPR, span_consumed(span, rem)),
        .operator = op,
        .left = left,
        .right = bw_res.node,
    };

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
    Span adv = skip_trivia(parser->source, rem);
    if (!match(parser->source, adv, OPERATOR_LAND))
      break;
    rem = span_advance(adv, OPERATOR_LAND.len);

    rel_res = parse_relational_expr(parser, skip_trivia(parser->source, rem));
    if (!rel_res.matched) {
      SyntaxError error = syntax_error_create(SYNTAX_EXPECTED_EXPR, rem);
      errors = syntax_errorlist_prepend(parser->arena, errors, error);
    } else {
      rem = rel_res.rem;
      errors = syntax_errorlist_concat(parser->arena, rel_res.errors, errors);
    }

    SyntaxBinaryExpr *expr = arena_alloc(parser->arena, sizeof(SyntaxBinaryExpr));
    *expr = (SyntaxBinaryExpr){
        .header = syntax_node_header(SYNTAX_KIND_BINARY_EXPR, span_consumed(span, rem)),
        .operator = SYNTAX_OPERATOR_LAND,
        .left = left,
        .right = rel_res.node,
    };

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
    Span adv = skip_trivia(parser->source, rem);
    if (!match(parser->source, adv, OPERATOR_LXOR))
      break;
    rem = span_advance(adv, OPERATOR_LXOR.len);

    and_res = parse_logical_and_expr(parser, skip_trivia(parser->source, rem));
    if (!and_res.matched) {
      SyntaxError error = syntax_error_create(SYNTAX_EXPECTED_EXPR, rem);
      errors = syntax_errorlist_prepend(parser->arena, errors, error);
    } else {
      rem = and_res.rem;
      errors = syntax_errorlist_concat(parser->arena, and_res.errors, errors);
    }

    SyntaxBinaryExpr *expr = arena_alloc(parser->arena, sizeof(SyntaxBinaryExpr));
    *expr = (SyntaxBinaryExpr){
        .header = syntax_node_header(SYNTAX_KIND_BINARY_EXPR, span_consumed(span, rem)),
        .operator = SYNTAX_OPERATOR_LXOR,
        .left = left,
        .right = and_res.node,
    };

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
    Span adv = skip_trivia(parser->source, rem);
    if (!match(parser->source, adv, OPERATOR_LOR))
      break;
    rem = span_advance(adv, OPERATOR_LOR.len);

    xor_res = parse_logical_xor_expr(parser, skip_trivia(parser->source, rem));
    if (!xor_res.matched) {
      SyntaxError error = syntax_error_create(SYNTAX_EXPECTED_EXPR, rem);
      errors = syntax_errorlist_prepend(parser->arena, errors, error);
    } else {
      rem = xor_res.rem;
      errors = syntax_errorlist_concat(parser->arena, xor_res.errors, errors);
    }

    SyntaxBinaryExpr *expr = arena_alloc(parser->arena, sizeof(SyntaxBinaryExpr));
    *expr = (SyntaxBinaryExpr){
        .header = syntax_node_header(SYNTAX_KIND_BINARY_EXPR, span_consumed(span, rem)),
        .operator = SYNTAX_OPERATOR_LOR,
        .left = left,
        .right = xor_res.node,
    };

    left = (SyntaxNode *)expr;
  }

  return parser_result_matched(rem, left, errors);
}

ParserResult parse_expr(const Parser *parser, Span span) {
  if (span_is_empty(span))
    return parser_result_not_match(span);

  return parse_logical_or_expr(parser, span);
}
