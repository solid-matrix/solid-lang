#include <assert.h>
#include <stddef.h>

#include "syntax_nodes.h"
#include "syntax_operator.h"
#include "syntax_parses.h"
#include "syntax_result.h"

#define COUNT_OF(a) (sizeof(a) / sizeof((a)[0]))

#pragma region AUXILLIARY

bool is_letter_or_underscore(uint8_t c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_'; }

bool is_letter_digit_or_underscore(uint8_t c) { return is_letter_or_underscore(c) || (c >= '0' && c <= '9'); }

bool is_decimal_digit(uint8_t c) { return c >= '0' && c <= '9'; }

bool is_binary_digit(uint8_t c) { return c == '0' || c == '1'; }

bool is_octal_digit(uint8_t c) { return c >= '0' && c <= '7'; }

bool is_hex_digit(uint8_t c) { return is_decimal_digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }

bool is_base_digit(uint8_t c, int base) {
  switch (base) {
  case 2:
    return is_binary_digit(c);
  case 8:
    return is_octal_digit(c);
  case 16:
    return is_hex_digit(c);
  default:
    return is_decimal_digit(c);
  }
}

bool is_whitespace(uint8_t c) { return c == ' ' || c == '\t' || c == '\v' || c == '\f' || c == '\r' || c == '\n'; }

Span skip_trivia(const Source *source, Span span) {
  size_t i = span.start;

  while (i < span.end) {
    uint8_t c = source_byte_at(source, i);

    if (is_whitespace(c)) {
      i += 1;
      continue;
    }

    if (c == '/' && i + 1 < span.end && source_byte_at(source, i + 1) == '/') {
      i += 2;
      while (i < span.end && source_byte_at(source, i) != '\n' && source_byte_at(source, i) != '\r')
        i += 1;

      continue;
    }

    break;
  }

  return (Span){.start = i, .end = span.end};
}

Span span_consumed(Span span, Span rem) {
  assert(span.start <= rem.start && rem.start <= span.end);
  return (Span){.start = span.start, .end = rem.start};
}

SyntaxNodeResult complete_longest_match(SyntaxNodeResult *results, size_t count) {
  assert(count > 0);

  size_t selected = 0;

  for (size_t i = 1; i < count; i++) {
    if (results[i].rem.start > results[selected].rem.start) {
      selected = i;
    }
  }

  return results[selected];
}

SyntaxListResult parse_expr_list(const SyntaxParser *parser, Span span, Strview separator) {
  Span rem = span;
  SyntaxNodeList *list = syntax_nodelist_empty();
  SyntaxErrorList *errors = syntax_errorlist_empty();

  SyntaxNodeResult res = parse_expr(parser, rem);
  if (res.matched) {
    list = syntax_nodelist_prepend(parser->arena, list, res.node);
    errors = syntax_errorlist_concat(parser->arena, res.errors, errors);
    rem = res.rem;

    while (true) {
      SyntaxMatchResult mres = match(parser->source, skip_trivia(parser->source, rem), separator);
      if (!mres.matched)
        break;

      rem = mres.rem;

      res = parse_expr(parser, skip_trivia(parser->source, rem));
      if (!res.matched) {
        SyntaxError error = syntax_error_create(SYNTAX_EXPECTED_EXPR, rem);
        errors = syntax_errorlist_prepend(parser->arena, errors, error);
      } else {
        list = syntax_nodelist_prepend(parser->arena, list, res.node);
        errors = syntax_errorlist_concat(parser->arena, res.errors, errors);
        rem = res.rem;
      }
    }
  }

  return (SyntaxListResult){.list = list, .errors = errors, .rem = rem};
}

SyntaxListResult parse_identifier_list(const SyntaxParser *parser, Span span, Strview separator) {
  Span rem = span;
  SyntaxNodeList *list = syntax_nodelist_empty();
  SyntaxErrorList *errors = syntax_errorlist_empty();

  SyntaxNodeResult res = parse_identifier(parser, rem);
  if (res.matched) {
    list = syntax_nodelist_prepend(parser->arena, list, res.node);
    errors = syntax_errorlist_concat(parser->arena, res.errors, errors);
    rem = res.rem;

    while (true) {
      SyntaxMatchResult mres = match(parser->source, skip_trivia(parser->source, rem), separator);
      if (!mres.matched)
        break;

      rem = mres.rem;

      res = parse_identifier(parser, skip_trivia(parser->source, rem));
      if (!res.matched) {
        SyntaxError error = syntax_error_create(SYNTAX_EXPECTED_IDENTIFIER, rem);
        errors = syntax_errorlist_prepend(parser->arena, errors, error);
      } else {
        list = syntax_nodelist_prepend(parser->arena, list, res.node);
        errors = syntax_errorlist_concat(parser->arena, res.errors, errors);
        rem = res.rem;
      }
    }
  }

  return (SyntaxListResult){.list = list, .errors = errors, .rem = rem};
}

SyntaxMatchResult match_keyword(const Source *source, Span span, Strview keyword) {
  if (keyword.len == 0 || keyword.len > span_len(span)) {
    return (SyntaxMatchResult){.matched = false, .rem = span};
  }

  Strview token = source_strview_at(source, span_slice(span, 0, keyword.len));
  if (!strview_equals(keyword, token))
    return (SyntaxMatchResult){.matched = false, .rem = span};

  Span rem = span_advance(span, keyword.len);
  if (!span_is_empty(rem) && is_letter_digit_or_underscore(source_byte_at(source, rem.start)))
    return (SyntaxMatchResult){.matched = false, .rem = span};

  return (SyntaxMatchResult){.matched = true, .rem = rem};
}

SyntaxMatchResult match(const Source *source, Span span, Strview strview) {
  if (strview.len == 0 || strview.len > span_len(span))
    return (SyntaxMatchResult){.matched = false, .rem = span};

  Strview token = source_strview_at(source, span_slice(span, 0, strview.len));
  if (!strview_equals(strview, token))
    return (SyntaxMatchResult){.matched = false, .rem = span};

  return (SyntaxMatchResult){.matched = true, .rem = span_advance(span, strview.len)};
}

#pragma endregion

#pragma region COMMON

SyntaxNodeResult parse_identifier(const SyntaxParser *parser, Span span) {
  if (span_is_empty(span))
    return syntax_node_result_not_match(span);

  uint8_t c = source_byte_at(parser->source, span.start);
  if (!is_letter_or_underscore(c))
    return syntax_node_result_not_match(span);

  Span rem = span_advance(span, 1);
  while (!span_is_empty(rem)) {
    c = source_byte_at(parser->source, rem.start);
    if (!is_letter_digit_or_underscore(c))
      break;

    rem = span_advance(rem, 1);
  }

  SyntaxIdentifier *id = arena_alloc(parser->arena, sizeof(SyntaxIdentifier));
  id->header = syntax_node_create(SYNTAX_KIND_IDENTIFIER, span_consumed(span, rem));
  id->value = source_strview_at(parser->source, span_consumed(span, rem));

  return syntax_node_result_matched(rem, (SyntaxNode *)id, NULL);
}

SyntaxNodeResult parse_compile_time(const SyntaxParser *parser, Span span) {
  SyntaxMatchResult mres = match(parser->source, span, PUNCTUATION_AT);
  if (!mres.matched) {
    return syntax_node_result_not_match(span);
  }

  Span rem = mres.rem;
  SyntaxErrorList *errors = syntax_errorlist_empty();
  SyntaxNodeList *args = syntax_nodelist_empty();
  SyntaxIdentifier *name = NULL;

  SyntaxNodeResult res = parse_identifier(parser, skip_trivia(parser->source, rem));
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

    SyntaxListResult lres = parse_expr_list(parser, skip_trivia(parser->source, rem), PUNCTUATION_COMMA);
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
  node->header = syntax_node_create(SYNTAX_KIND_COMPILE_TIME, span_consumed(span, rem));
  node->args = args;
  node->id = name;

  return syntax_node_result_matched(rem, (SyntaxNode *)node, errors);
}

SyntaxNodeResult parse_program(const SyntaxParser *parser, Span span) {
  span = skip_trivia(parser->source, span);

  SyntaxErrorList *errors = syntax_errorlist_empty();
  SyntaxNodeList *decls = syntax_nodelist_empty();
  Span rem = span;

  while (!span_is_empty(rem)) {
    rem = skip_trivia(parser->source, rem);

    SyntaxNodeResult res = parse_decl(parser, rem);
    if (!res.matched)
      break;

    rem = res.rem;
    errors = syntax_errorlist_concat(parser->arena, res.errors, errors);
    decls = syntax_nodelist_prepend(parser->arena, decls, res.node);
  }

  SyntaxProgram *program = arena_alloc(parser->arena, sizeof(SyntaxProgram));
  program->header = syntax_node_create(SYNTAX_KIND_PROGRAM, span_consumed(span, rem));
  program->top_levels = decls;

  rem = skip_trivia(parser->source, rem);

  if (!span_is_empty(rem))
    errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_EOF, rem));

  return syntax_node_result_matched(rem, (SyntaxNode *)program, errors);
}

SyntaxNodeResult parse_named(const SyntaxParser *parser, Span span) {
  // Minimal Named: path segments only; generic arguments come later.
  SyntaxListResult lres = parse_identifier_list(parser, span, PUNCTUATION_SCOPE);
  if (syntax_nodelist_is_empty(lres.list))
    return syntax_node_result_not_match(span);

  SyntaxNamed *node = arena_alloc(parser->arena, sizeof(SyntaxNamed));
  node->header = syntax_node_create(SYNTAX_KIND_NAMED, span_consumed(span, lres.rem));
  node->path = lres.list;

  return syntax_node_result_matched(lres.rem, (SyntaxNode *)node, lres.errors);
}

#pragma endregion

#pragma region TYPE

SyntaxNodeResult parse_type(const SyntaxParser *parser, Span span) {
  SyntaxNodeResult results[] = {
      parse_named(parser, span),
      parse_ref_type(parser, span),
      parse_array_type(parser, span),
      parse_func_type(parser, span),
  };
  return complete_longest_match(results, sizeof(results) / sizeof(results[0]));
}

SyntaxNodeResult parse_ref_type(const SyntaxParser *parser, Span span) {
  SyntaxMatchResult mres = match(parser->source, span, PUNCTUATION_AMP);
  if (!mres.matched)
    return syntax_node_result_not_match(span);

  Span rem = mres.rem;
  SyntaxErrorList *errors = syntax_errorlist_empty();
  SyntaxRefKind ref_kind = SYNTAX_REF_KIND_READWRITE;

  Span adv = skip_trivia(parser->source, rem);
  SyntaxMatchResult kw = match_keyword(parser->source, adv, KEYWORD_READONLY);
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

  SyntaxNodeResult inner = parse_type(parser, skip_trivia(parser->source, rem));
  if (!inner.matched) {
    SyntaxError error = syntax_error_create(SYNTAX_EXPECTED_TYPE, rem);
    errors = syntax_errorlist_prepend(parser->arena, errors, error);
  } else {
    rem = inner.rem;
    errors = syntax_errorlist_concat(parser->arena, inner.errors, errors);
  }

  SyntaxRefType *node = arena_alloc(parser->arena, sizeof(SyntaxRefType));
  node->header = syntax_node_create(SYNTAX_KIND_REF_TYPE, span_consumed(span, rem));
  node->ref_kind = ref_kind;
  node->inner_type = inner.node;

  return syntax_node_result_matched(rem, (SyntaxNode *)node, errors);
}

SyntaxNodeResult parse_array_type(const SyntaxParser *parser, Span span) {
  // TODO
  (void)parser;
  (void)span;
  return syntax_node_result_not_match(span);
}

SyntaxNodeResult parse_func_type(const SyntaxParser *parser, Span span) {
  // TODO
  (void)parser;
  (void)span;
  return syntax_node_result_not_match(span);
}

#pragma endregion

#pragma region DECL

SyntaxNodeResult parse_decl(const SyntaxParser *parser, Span span) {
  SyntaxNodeResult results[] = {
      parse_namespace_decl(parser, span),
      parse_using_decl(parser, span),
      // TODO
  };
  return complete_longest_match(results, COUNT_OF(results));
}

SyntaxNodeResult parse_namespace_decl(const SyntaxParser *parser, Span span) {
  SyntaxMatchResult mres = match_keyword(parser->source, span, KEYWORD_NAMESPACE);
  if (!mres.matched)
    return syntax_node_result_not_match(span);

  Span rem = mres.rem;
  SyntaxErrorList *errors = syntax_errorlist_empty();
  SyntaxNodeList *segs = syntax_nodelist_empty();

  SyntaxListResult lres = parse_identifier_list(parser, skip_trivia(parser->source, rem), PUNCTUATION_SCOPE);
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
  decl->header = syntax_node_create(SYNTAX_KIND_NAMESPACE_DECL, span_consumed(span, rem));
  decl->path = segs;

  return syntax_node_result_matched(rem, (SyntaxNode *)decl, errors);
}

SyntaxNodeResult parse_using_decl(const SyntaxParser *parser, Span span) {
  SyntaxMatchResult mres = match_keyword(parser->source, span, KEYWORD_USING);
  if (!mres.matched)
    return syntax_node_result_not_match(span);

  Span rem = mres.rem;
  SyntaxErrorList *errors = syntax_errorlist_empty();
  SyntaxNodeList *segs = syntax_nodelist_empty();

  SyntaxListResult lres = parse_identifier_list(parser, skip_trivia(parser->source, rem), PUNCTUATION_SCOPE);
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
  decl->header = syntax_node_create(SYNTAX_KIND_USING_DECL, span_consumed(span, rem));
  decl->path = segs;

  return syntax_node_result_matched(rem, (SyntaxNode *)decl, errors);
}

SyntaxNodeResult parse_let_decl(const SyntaxParser *parser, Span span) {
  // TODO
  (void)parser;
  (void)span;
  return syntax_node_result_not_match(span);
}

SyntaxNodeResult parse_struct_decl(const SyntaxParser *parser, Span span) {
  // TODO
  (void)parser;
  (void)span;
  return syntax_node_result_not_match(span);
}

SyntaxNodeResult parse_enum_decl(const SyntaxParser *parser, Span span) {
  // TODO
  (void)parser;
  (void)span;
  return syntax_node_result_not_match(span);
}

SyntaxNodeResult parse_union_decl(const SyntaxParser *parser, Span span) {
  // TODO
  (void)parser;
  (void)span;
  return syntax_node_result_not_match(span);
}

SyntaxNodeResult parse_variant_decl(const SyntaxParser *parser, Span span) {
  // TODO
  (void)parser;
  (void)span;
  return syntax_node_result_not_match(span);
}

SyntaxNodeResult parse_contract_decl(const SyntaxParser *parser, Span span) {
  // TODO
  (void)parser;
  (void)span;
  return syntax_node_result_not_match(span);
}

SyntaxNodeResult parse_func_decl(const SyntaxParser *parser, Span span) {
  // TODO
  (void)parser;
  (void)span;
  return syntax_node_result_not_match(span);
}

#pragma endregion

#pragma region STMT

SyntaxNodeResult parse_stmt(const SyntaxParser *parser, Span span) {
  // TODO
  (void)parser;
  (void)span;
  return syntax_node_result_not_match(span);
}

SyntaxNodeResult parse_empty_stmt(const SyntaxParser *parser, Span span) {
  // TODO
  (void)parser;
  (void)span;
  return syntax_node_result_not_match(span);
}

SyntaxNodeResult parse_body_stmt(const SyntaxParser *parser, Span span) {
  // TODO
  (void)parser;
  (void)span;
  return syntax_node_result_not_match(span);
}

SyntaxNodeResult parse_let_stmt(const SyntaxParser *parser, Span span) {
  // TODO
  (void)parser;
  (void)span;
  return syntax_node_result_not_match(span);
}

SyntaxNodeResult parse_set_stmt(const SyntaxParser *parser, Span span) {
  // TODO
  (void)parser;
  (void)span;
  return syntax_node_result_not_match(span);
}

SyntaxNodeResult parse_expr_stmt(const SyntaxParser *parser, Span span) {
  // TODO
  (void)parser;
  (void)span;
  return syntax_node_result_not_match(span);
}

SyntaxNodeResult parse_if_stmt(const SyntaxParser *parser, Span span) {
  // TODO
  (void)parser;
  (void)span;
  return syntax_node_result_not_match(span);
}

SyntaxNodeResult parse_loop_stmt(const SyntaxParser *parser, Span span) {
  // TODO
  (void)parser;
  (void)span;
  return syntax_node_result_not_match(span);
}

SyntaxNodeResult parse_break_stmt(const SyntaxParser *parser, Span span) {
  // TODO
  (void)parser;
  (void)span;
  return syntax_node_result_not_match(span);
}

SyntaxNodeResult parse_continue_stmt(const SyntaxParser *parser, Span span) {
  // TODO
  (void)parser;
  (void)span;
  return syntax_node_result_not_match(span);
}

SyntaxNodeResult parse_return_stmt(const SyntaxParser *parser, Span span) {
  // TODO
  (void)parser;
  (void)span;
  return syntax_node_result_not_match(span);
}

SyntaxNodeResult parse_while_stmt(const SyntaxParser *parser, Span span) {
  // TODO
  (void)parser;
  (void)span;
  return syntax_node_result_not_match(span);
}

#pragma endregion

#pragma region EXPR

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
    SyntaxMatchResult mres = match(parser->source, skip_trivia(parser->source, rem), OPERATOR_LOR);
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
    SyntaxMatchResult mres = match(parser->source, skip_trivia(parser->source, rem), OPERATOR_LXOR);
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
    SyntaxMatchResult mres = match(parser->source, skip_trivia(parser->source, rem), OPERATOR_LAND);
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
      SyntaxMatchResult mres = match(parser->source, adv, OP_STRS[i]);
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
      SyntaxMatchResult mres = match(parser->source, adv, OP_STRS[i]);
      SyntaxMatchResult mres_not = match(parser->source, adv, OP_NOT_STRS[i]);
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
      SyntaxMatchResult mres = match(parser->source, adv, OP_STRS[i]);
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
      SyntaxMatchResult mres = match(parser->source, adv, OP_STRS[i]);
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
      SyntaxMatchResult mres = match(parser->source, adv, OP_STRS[i]);
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
    SyntaxMatchResult mres = match(parser->source, span, OP_STRS[i]);
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

static SyntaxNodeResult parse_dot_expr(const SyntaxParser *parser, Span span, SyntaxNode *receiver) {
  SyntaxMatchResult mres = match(parser->source, span, PUNCTUATION_DOT);
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

static SyntaxNodeResult parse_index_expr(const SyntaxParser *parser, Span span, SyntaxNode *receiver) {
  SyntaxMatchResult mres = match(parser->source, span, PUNCTUATION_LBRACKET);
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

  mres = match(parser->source, skip_trivia(parser->source, rem), PUNCTUATION_RBRACKET);
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

static SyntaxNodeResult parse_call_expr(const SyntaxParser *parser, Span span, SyntaxNode *receiver) {
  SyntaxMatchResult mres = match(parser->source, span, PUNCTUATION_LPAREN);
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

  mres = match(parser->source, skip_trivia(parser->source, rem), PUNCTUATION_RPAREN);
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
      parse_string_lit_expr(parser, span), parse_named(parser, span),          parse_sub_expr(parser, span),
      parse_struct_lit_expr(parser, span), parse_array_lit_expr(parser, span), parse_compile_time(parser, span),
  };
  return complete_longest_match(results, sizeof(results) / sizeof(results[0]));
}

SyntaxNodeResult parse_struct_lit_expr(const SyntaxParser *parser, Span span) {
  (void)parser;
  (void)span;
  return syntax_node_result_not_match(span);
}

SyntaxNodeResult parse_array_lit_expr(const SyntaxParser *parser, Span span) {
  (void)parser;
  (void)span;
  return syntax_node_result_not_match(span);
}

SyntaxNodeResult parse_sub_expr(const SyntaxParser *parser, Span span) {
  SyntaxMatchResult mres = match(parser->source, span, PUNCTUATION_LPAREN);
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

  mres = match(parser->source, skip_trivia(parser->source, rem), PUNCTUATION_RPAREN);
  if (!mres.matched) {
    SyntaxError error = syntax_error_create(SYNTAX_EXPECTED_RPAREN, rem);
    errors = syntax_errorlist_prepend(parser->arena, errors, error);
  } else {
    rem = mres.rem;
  }

  return syntax_node_result_matched(rem, expr.node, errors);
}
