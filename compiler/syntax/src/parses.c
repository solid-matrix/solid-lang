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

SyntaxListResult parse_generic_param_list(const SyntaxParser *parser, Span span) {
  Span rem = span;
  SyntaxNodeList *list = syntax_nodelist_empty();
  SyntaxErrorList *errors = syntax_errorlist_empty();

  SyntaxNodeResult res = parse_generic_param(parser, rem);
  if (res.matched) {
    list = syntax_nodelist_prepend(parser->arena, list, res.node);
    errors = syntax_errorlist_concat(parser->arena, res.errors, errors);
    rem = res.rem;

    while (true) {
      SyntaxMatchResult mres = match(parser->source, skip_trivia(parser->source, rem), PUNCTUATION_COMMA);
      if (!mres.matched)
        break;

      rem = mres.rem;

      res = parse_generic_param(parser, skip_trivia(parser->source, rem));
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

SyntaxListResult parse_call_param_list(const SyntaxParser *parser, Span span) {
  Span rem = span;
  SyntaxNodeList *list = syntax_nodelist_empty();
  SyntaxErrorList *errors = syntax_errorlist_empty();

  SyntaxNodeResult res = parse_call_param(parser, rem);
  if (res.matched) {
    list = syntax_nodelist_prepend(parser->arena, list, res.node);
    errors = syntax_errorlist_concat(parser->arena, res.errors, errors);
    rem = res.rem;

    while (true) {
      SyntaxMatchResult mres = match(parser->source, skip_trivia(parser->source, rem), PUNCTUATION_COMMA);
      if (!mres.matched)
        break;

      rem = mres.rem;

      res = parse_call_param(parser, skip_trivia(parser->source, rem));
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

SyntaxListResult parse_field_list(const SyntaxParser *parser, Span span, SyntaxFieldFn parse_field,
                                  SyntaxErrorCode missing_code) {
  Span rem = span;
  SyntaxNodeList *list = syntax_nodelist_empty();
  SyntaxErrorList *errors = syntax_errorlist_empty();

  SyntaxNodeResult res = parse_field(parser, rem);
  if (res.matched) {
    rem = res.rem;
    list = syntax_nodelist_prepend(parser->arena, list, res.node);
    errors = syntax_errorlist_concat(parser->arena, res.errors, errors);

    while (true) {
      SyntaxMatchResult mres = match(parser->source, skip_trivia(parser->source, rem), PUNCTUATION_COMMA);
      if (!mres.matched)
        break;
      rem = mres.rem;

      if (match(parser->source, skip_trivia(parser->source, rem), PUNCTUATION_RBRACE).matched)
        break;

      res = parse_field(parser, skip_trivia(parser->source, rem));
      if (!res.matched) {
        errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(missing_code, rem));
        break;
      }

      rem = res.rem;
      list = syntax_nodelist_prepend(parser->arena, list, res.node);
      errors = syntax_errorlist_concat(parser->arena, res.errors, errors);
    }
  }

  return (SyntaxListResult){.list = list, .errors = errors, .rem = rem};
}

// The body position shared by if/loop/while (and later func decls):
// a braced BodyStmt or a lone ";" EmptyStmt. Always returns a matched
// frame; when neither form starts at the span, an EXPECTED_BODY
// diagnostic is reported and the body node stays NULL.
static SyntaxNodeResult parse_body_position(const SyntaxParser *parser, Span span) {
  Span adv = skip_trivia(parser->source, span);

  SyntaxNodeResult results[] = {
      parse_body_stmt(parser, adv),
      parse_empty_stmt(parser, adv),
  };
  SyntaxNodeResult res = complete_longest_match(results, COUNT_OF(results));
  if (res.matched)
    return res;

  SyntaxErrorList *errors = syntax_errorlist_prepend(parser->arena, syntax_errorlist_empty(),
                                                     syntax_error_create(SYNTAX_EXPECTED_BODY, span));
  return syntax_node_result_matched(span, NULL, errors);
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

SyntaxListResult parse_annotations(const SyntaxParser *parser, Span span) {
  Span rem = span;
  SyntaxNodeList *list = syntax_nodelist_empty();
  SyntaxErrorList *errors = syntax_errorlist_empty();

  SyntaxNodeResult res = parse_compile_time(parser, rem);
  while (res.matched) {
    rem = res.rem;
    errors = syntax_errorlist_concat(parser->arena, res.errors, errors);
    list = syntax_nodelist_prepend(parser->arena, list, res.node);

    res = parse_compile_time(parser, skip_trivia(parser->source, rem));
  }

  return (SyntaxListResult){.list = list, .errors = errors, .rem = rem};
}

SyntaxNodeResult parse_generic_param(const SyntaxParser *parser, Span span) {
  SyntaxListResult ann = parse_annotations(parser, span);
  Span rem = ann.rem;
  SyntaxErrorList *errors = ann.errors;
  SyntaxIdentifier *id = NULL;
  SyntaxNode *type = NULL;

  SyntaxNodeResult id_res = parse_identifier(parser, skip_trivia(parser->source, rem));
  if (!id_res.matched)
    return syntax_node_result_not_match(span);

  rem = id_res.rem;
  id = (SyntaxIdentifier *)id_res.node;
  errors = syntax_errorlist_concat(parser->arena, id_res.errors, errors);

  SyntaxMatchResult mres = match(parser->source, skip_trivia(parser->source, rem), PUNCTUATION_COLON);
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

  SyntaxGenericParam *param = arena_alloc(parser->arena, sizeof(SyntaxGenericParam));
  param->header = syntax_node_create(SYNTAX_KIND_GENERIC_PARAMETER, span_consumed(span, rem));
  param->annotations = ann.list;
  param->id = id;
  param->type = type;

  return syntax_node_result_matched(rem, (SyntaxNode *)param, errors);
}

SyntaxNodeResult parse_call_param(const SyntaxParser *parser, Span span) {
  SyntaxListResult ann = parse_annotations(parser, span);
  Span rem = ann.rem;
  SyntaxErrorList *errors = ann.errors;
  SyntaxIdentifier *id = NULL;
  SyntaxNode *type = NULL;

  SyntaxNodeResult id_res = parse_identifier(parser, skip_trivia(parser->source, rem));
  if (!id_res.matched)
    return syntax_node_result_not_match(span);

  rem = id_res.rem;
  id = (SyntaxIdentifier *)id_res.node;
  errors = syntax_errorlist_concat(parser->arena, id_res.errors, errors);

  SyntaxMatchResult mres = match(parser->source, skip_trivia(parser->source, rem), PUNCTUATION_COLON);
  if (!mres.matched) {
    errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_COLON, rem));
  } else {
    rem = mres.rem;
  }

  SyntaxNodeResult type_res = parse_type(parser, skip_trivia(parser->source, rem));
  if (!type_res.matched) {
    errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_TYPE, rem));
  } else {
    rem = type_res.rem;
    type = type_res.node;
    errors = syntax_errorlist_concat(parser->arena, type_res.errors, errors);
  }

  SyntaxCallParam *param = arena_alloc(parser->arena, sizeof(SyntaxCallParam));
  param->header = syntax_node_create(SYNTAX_KIND_CALL_PARAMETER, span_consumed(span, rem));
  param->annotations = ann.list;
  param->id = id;
  param->type = type;

  return syntax_node_result_matched(rem, (SyntaxNode *)param, errors);
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
  SyntaxMatchResult mres = match(parser->source, span, PUNCTUATION_LBRACKET);
  if (!mres.matched)
    return syntax_node_result_not_match(span);

  Span rem = mres.rem;
  SyntaxErrorList *errors = syntax_errorlist_empty();
  SyntaxNode *len = NULL;
  SyntaxNode *inner_type = NULL;

  SyntaxNodeResult len_res = parse_expr(parser, skip_trivia(parser->source, rem));
  if (!len_res.matched) {
    errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_EXPR, rem));
  } else {
    rem = len_res.rem;
    len = len_res.node;
    errors = syntax_errorlist_concat(parser->arena, len_res.errors, errors);
  }

  mres = match(parser->source, skip_trivia(parser->source, rem), PUNCTUATION_RBRACKET);
  if (!mres.matched) {
    errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_RBRACKET, rem));
  } else {
    rem = mres.rem;
  }

  SyntaxNodeResult inner_res = parse_type(parser, skip_trivia(parser->source, rem));
  if (!inner_res.matched) {
    errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_TYPE, rem));
  } else {
    rem = inner_res.rem;
    inner_type = inner_res.node;
    errors = syntax_errorlist_concat(parser->arena, inner_res.errors, errors);
  }

  SyntaxArrayType *type = arena_alloc(parser->arena, sizeof(SyntaxArrayType));
  type->header = syntax_node_create(SYNTAX_KIND_ARRAY_TYPE, span_consumed(span, rem));
  type->len = len;
  type->inner_type = inner_type;

  return syntax_node_result_matched(rem, (SyntaxNode *)type, errors);
}

SyntaxNodeResult parse_func_type(const SyntaxParser *parser, Span span) {
  SyntaxMatchResult mres = match(parser->source, span, PUNCTUATION_AMP);
  if (!mres.matched)
    return syntax_node_result_not_match(span);

  Span rem = mres.rem;

  mres = match_keyword(parser->source, skip_trivia(parser->source, rem), KEYWORD_FUNC);
  if (!mres.matched)
    return syntax_node_result_not_match(span);

  rem = mres.rem;
  SyntaxErrorList *errors = syntax_errorlist_empty();
  SyntaxNodeList *call_params = syntax_nodelist_empty();
  SyntaxIdentifier *callconv = NULL;
  SyntaxNode *return_type = NULL;

  mres = match(parser->source, skip_trivia(parser->source, rem), PUNCTUATION_LPAREN);
  if (!mres.matched) {
    errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_LPAREN, rem));
  } else {
    rem = mres.rem;

    SyntaxNodeResult type_res = parse_type(parser, skip_trivia(parser->source, rem));
    if (type_res.matched) {
      rem = type_res.rem;
      call_params = syntax_nodelist_prepend(parser->arena, call_params, type_res.node);
      errors = syntax_errorlist_concat(parser->arena, type_res.errors, errors);

      while (true) {
        mres = match(parser->source, skip_trivia(parser->source, rem), PUNCTUATION_COMMA);
        if (!mres.matched)
          break;
        rem = mres.rem;

        type_res = parse_type(parser, skip_trivia(parser->source, rem));
        if (!type_res.matched) {
          errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_TYPE, rem));
          break;
        }

        rem = type_res.rem;
        call_params = syntax_nodelist_prepend(parser->arena, call_params, type_res.node);
        errors = syntax_errorlist_concat(parser->arena, type_res.errors, errors);
      }
    }

    mres = match(parser->source, skip_trivia(parser->source, rem), PUNCTUATION_RPAREN);
    if (!mres.matched) {
      errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_RPAREN, rem));
    } else {
      rem = mres.rem;
    }
  }

  SyntaxNodeResult cc_res = parse_identifier(parser, skip_trivia(parser->source, rem));
  if (cc_res.matched) {
    rem = cc_res.rem;
    callconv = (SyntaxIdentifier *)cc_res.node;
    errors = syntax_errorlist_concat(parser->arena, cc_res.errors, errors);
  }

  mres = match(parser->source, skip_trivia(parser->source, rem), PUNCTUATION_COLON);
  if (mres.matched) {
    rem = mres.rem;

    SyntaxNodeResult type_res = parse_type(parser, skip_trivia(parser->source, rem));
    if (!type_res.matched) {
      errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_TYPE, rem));
    } else {
      rem = type_res.rem;
      return_type = type_res.node;
      errors = syntax_errorlist_concat(parser->arena, type_res.errors, errors);
    }
  }

  SyntaxFuncType *type = arena_alloc(parser->arena, sizeof(SyntaxFuncType));
  type->header = syntax_node_create(SYNTAX_KIND_FUNC_TYPE, span_consumed(span, rem));
  type->call_params = call_params;
  type->callconv = callconv;
  type->return_type = return_type;

  return syntax_node_result_matched(rem, (SyntaxNode *)type, errors);
}

#pragma endregion

#pragma region DECL

SyntaxNodeResult parse_decl(const SyntaxParser *parser, Span span) {
  SyntaxNodeResult results[] = {
      parse_namespace_decl(parser, span), parse_using_decl(parser, span),    parse_let_decl(parser, span),
      parse_struct_decl(parser, span),    parse_union_decl(parser, span),    parse_enum_decl(parser, span),
      parse_variant_decl(parser, span),   parse_contract_decl(parser, span), parse_func_decl(parser, span),
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
  SyntaxListResult ann = parse_annotations(parser, span);

  SyntaxMatchResult mres = match_keyword(parser->source, skip_trivia(parser->source, ann.rem), KEYWORD_LET);
  if (!mres.matched)
    return syntax_node_result_not_match(span);

  Span rem = mres.rem;
  SyntaxErrorList *errors = ann.errors;
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

  bool typed = false;
  mres = match(parser->source, skip_trivia(parser->source, rem), PUNCTUATION_COLON);
  if (mres.matched) {
    typed = true;
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

  // Optional "= Expr". The grammar requires at least one of the two; a
  // declaration with neither clause reports the likelier intent (a
  // failed type clause is already diagnosed on its own).
  mres = match(parser->source, skip_trivia(parser->source, rem), PUNCTUATION_EQUALS);
  if (mres.matched) {
    rem = mres.rem;

    SyntaxNodeResult value_res = parse_expr(parser, skip_trivia(parser->source, rem));
    if (!value_res.matched) {
      errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_EXPR, rem));
    } else {
      rem = value_res.rem;
      value = value_res.node;
      errors = syntax_errorlist_concat(parser->arena, value_res.errors, errors);
    }
  } else if (!typed) {
    errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_EQUALS, rem));
  }

  mres = match(parser->source, skip_trivia(parser->source, rem), PUNCTUATION_SEMICOLON);
  if (!mres.matched) {
    errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_SEMICOLON, rem));
  } else {
    rem = mres.rem;
  }

  SyntaxLetDecl *decl = arena_alloc(parser->arena, sizeof(SyntaxLetDecl));
  decl->header = syntax_node_create(SYNTAX_KIND_LET_DECL, span_consumed(span, rem));
  decl->annotations = ann.list;
  decl->id = id;
  decl->type = type;
  decl->value = value;

  return syntax_node_result_matched(rem, (SyntaxNode *)decl, errors);
}

static SyntaxNodeResult parse_struct_field(const SyntaxParser *parser, Span span) {
  SyntaxListResult ann = parse_annotations(parser, span);
  Span rem = ann.rem;
  SyntaxErrorList *errors = ann.errors;
  SyntaxIdentifier *id = NULL;
  SyntaxNode *type = NULL;

  SyntaxNodeResult id_res = parse_identifier(parser, skip_trivia(parser->source, rem));
  if (!id_res.matched)
    return syntax_node_result_not_match(span);

  rem = id_res.rem;
  id = (SyntaxIdentifier *)id_res.node;
  errors = syntax_errorlist_concat(parser->arena, id_res.errors, errors);

  SyntaxMatchResult mres = match(parser->source, skip_trivia(parser->source, rem), PUNCTUATION_COLON);
  if (!mres.matched) {
    errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_COLON, rem));
  } else {
    rem = mres.rem;
  }

  SyntaxNodeResult type_res = parse_type(parser, skip_trivia(parser->source, rem));
  if (!type_res.matched) {
    errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_TYPE, rem));
  } else {
    rem = type_res.rem;
    type = type_res.node;
    errors = syntax_errorlist_concat(parser->arena, type_res.errors, errors);
  }

  SyntaxStructField *field = arena_alloc(parser->arena, sizeof(SyntaxStructField));
  field->header = syntax_node_create(SYNTAX_KIND_STRUCT_FIELD, span_consumed(span, rem));
  field->annotations = ann.list;
  field->id = id;
  field->type = type;

  return syntax_node_result_matched(rem, (SyntaxNode *)field, errors);
}

SyntaxNodeResult parse_struct_decl(const SyntaxParser *parser, Span span) {
  SyntaxListResult ann = parse_annotations(parser, span);

  SyntaxMatchResult mres = match_keyword(parser->source, skip_trivia(parser->source, ann.rem), KEYWORD_STRUCT);
  if (!mres.matched)
    return syntax_node_result_not_match(span);

  Span rem = mres.rem;
  SyntaxErrorList *errors = ann.errors;
  SyntaxIdentifier *id = NULL;
  SyntaxNodeList *generic_params = syntax_nodelist_empty();
  SyntaxNodeList *fields = syntax_nodelist_empty();

  SyntaxNodeResult id_res = parse_identifier(parser, skip_trivia(parser->source, rem));
  if (!id_res.matched) {
    errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_IDENTIFIER, rem));
  } else {
    rem = id_res.rem;
    id = (SyntaxIdentifier *)id_res.node;
    errors = syntax_errorlist_concat(parser->arena, id_res.errors, errors);
  }

  mres = match(parser->source, skip_trivia(parser->source, rem), PUNCTUATION_LT);
  if (mres.matched) {
    rem = mres.rem;

    SyntaxListResult glist = parse_generic_param_list(parser, skip_trivia(parser->source, rem));
    generic_params = glist.list;
    errors = syntax_errorlist_concat(parser->arena, glist.errors, errors);
    rem = glist.rem;

    if (syntax_nodelist_is_empty(generic_params)) {
      errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_IDENTIFIER, rem));
    }

    mres = match(parser->source, skip_trivia(parser->source, rem), PUNCTUATION_GT);
    if (!mres.matched) {
      errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_GT, rem));
    } else {
      rem = mres.rem;
    }
  }

  Span adv = skip_trivia(parser->source, rem);
  mres = match(parser->source, adv, PUNCTUATION_SEMICOLON);
  if (mres.matched) {
    rem = mres.rem;
  } else {
    mres = match(parser->source, adv, PUNCTUATION_LBRACE);
    if (mres.matched) {
      rem = mres.rem;

      SyntaxListResult flist =
          parse_field_list(parser, skip_trivia(parser->source, rem), parse_struct_field, SYNTAX_EXPECTED_IDENTIFIER);
      fields = flist.list;
      errors = syntax_errorlist_concat(parser->arena, flist.errors, errors);
      rem = flist.rem;

      mres = match(parser->source, skip_trivia(parser->source, rem), PUNCTUATION_RBRACE);
      if (!mres.matched) {
        errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_RBRACE, rem));
      } else {
        rem = mres.rem;
      }
    } else {
      errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_DECL_BODY, rem));
    }
  }

  SyntaxStructDecl *decl = arena_alloc(parser->arena, sizeof(SyntaxStructDecl));
  decl->header = syntax_node_create(SYNTAX_KIND_STRUCT_DECL, span_consumed(span, rem));
  decl->annotations = ann.list;
  decl->id = id;
  decl->generic_params = generic_params;
  decl->fields = fields;

  return syntax_node_result_matched(rem, (SyntaxNode *)decl, errors);
}

static SyntaxNodeResult parse_enum_field(const SyntaxParser *parser, Span span) {
  SyntaxListResult ann = parse_annotations(parser, span);
  Span rem = ann.rem;
  SyntaxErrorList *errors = ann.errors;
  SyntaxIdentifier *id = NULL;
  SyntaxNode *value = NULL;

  SyntaxNodeResult id_res = parse_identifier(parser, skip_trivia(parser->source, rem));
  if (!id_res.matched)
    return syntax_node_result_not_match(span);

  rem = id_res.rem;
  id = (SyntaxIdentifier *)id_res.node;
  errors = syntax_errorlist_concat(parser->arena, id_res.errors, errors);

  SyntaxMatchResult mres = match(parser->source, skip_trivia(parser->source, rem), PUNCTUATION_EQUALS);
  if (mres.matched) {
    rem = mres.rem;

    SyntaxNodeResult value_res = parse_expr(parser, skip_trivia(parser->source, rem));
    if (!value_res.matched) {
      errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_EXPR, rem));
    } else {
      rem = value_res.rem;
      value = value_res.node;
      errors = syntax_errorlist_concat(parser->arena, value_res.errors, errors);
    }
  }

  SyntaxEnumField *field = arena_alloc(parser->arena, sizeof(SyntaxEnumField));
  field->header = syntax_node_create(SYNTAX_KIND_ENUM_FIELD, span_consumed(span, rem));
  field->annotations = ann.list;
  field->id = id;
  field->value = value;

  return syntax_node_result_matched(rem, (SyntaxNode *)field, errors);
}

SyntaxNodeResult parse_enum_decl(const SyntaxParser *parser, Span span) {
  SyntaxListResult ann = parse_annotations(parser, span);

  SyntaxMatchResult mres = match_keyword(parser->source, skip_trivia(parser->source, ann.rem), KEYWORD_ENUM);
  if (!mres.matched)
    return syntax_node_result_not_match(span);

  Span rem = mres.rem;
  SyntaxErrorList *errors = ann.errors;
  SyntaxIdentifier *id = NULL;
  SyntaxNode *behind_type = NULL;
  SyntaxNodeList *fields = syntax_nodelist_empty();

  SyntaxNodeResult id_res = parse_identifier(parser, skip_trivia(parser->source, rem));
  if (!id_res.matched) {
    errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_IDENTIFIER, rem));
  } else {
    rem = id_res.rem;
    id = (SyntaxIdentifier *)id_res.node;
    errors = syntax_errorlist_concat(parser->arena, id_res.errors, errors);
  }

  mres = match(parser->source, skip_trivia(parser->source, rem), PUNCTUATION_COLON);
  if (mres.matched) {
    rem = mres.rem;

    SyntaxNodeResult type_res = parse_type(parser, skip_trivia(parser->source, rem));
    if (!type_res.matched) {
      errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_TYPE, rem));
    } else {
      rem = type_res.rem;
      behind_type = type_res.node;
      errors = syntax_errorlist_concat(parser->arena, type_res.errors, errors);
    }
  }

  Span adv = skip_trivia(parser->source, rem);
  mres = match(parser->source, adv, PUNCTUATION_SEMICOLON);
  if (mres.matched) {
    rem = mres.rem;
  } else {
    mres = match(parser->source, adv, PUNCTUATION_LBRACE);
    if (mres.matched) {
      rem = mres.rem;

      SyntaxListResult flist =
          parse_field_list(parser, skip_trivia(parser->source, rem), parse_enum_field, SYNTAX_EXPECTED_IDENTIFIER);
      fields = flist.list;
      errors = syntax_errorlist_concat(parser->arena, flist.errors, errors);
      rem = flist.rem;

      mres = match(parser->source, skip_trivia(parser->source, rem), PUNCTUATION_RBRACE);
      if (!mres.matched) {
        errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_RBRACE, rem));
      } else {
        rem = mres.rem;
      }
    } else {
      errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_DECL_BODY, rem));
    }
  }

  SyntaxEnumDecl *decl = arena_alloc(parser->arena, sizeof(SyntaxEnumDecl));
  decl->header = syntax_node_create(SYNTAX_KIND_ENUM_DECL, span_consumed(span, rem));
  decl->annotations = ann.list;
  decl->id = id;
  decl->behind_type = behind_type;
  decl->fields = fields;

  return syntax_node_result_matched(rem, (SyntaxNode *)decl, errors);
}

static SyntaxNodeResult parse_union_field(const SyntaxParser *parser, Span span) {
  SyntaxListResult ann = parse_annotations(parser, span);
  Span rem = ann.rem;
  SyntaxErrorList *errors = ann.errors;
  SyntaxIdentifier *id = NULL;
  SyntaxNode *type = NULL;

  SyntaxNodeResult id_res = parse_identifier(parser, skip_trivia(parser->source, rem));
  if (!id_res.matched)
    return syntax_node_result_not_match(span);

  rem = id_res.rem;
  id = (SyntaxIdentifier *)id_res.node;
  errors = syntax_errorlist_concat(parser->arena, id_res.errors, errors);

  SyntaxMatchResult mres = match(parser->source, skip_trivia(parser->source, rem), PUNCTUATION_COLON);
  if (!mres.matched) {
    errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_COLON, rem));
  } else {
    rem = mres.rem;
  }

  SyntaxNodeResult type_res = parse_type(parser, skip_trivia(parser->source, rem));
  if (!type_res.matched) {
    errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_TYPE, rem));
  } else {
    rem = type_res.rem;
    type = type_res.node;
    errors = syntax_errorlist_concat(parser->arena, type_res.errors, errors);
  }

  SyntaxUnionField *field = arena_alloc(parser->arena, sizeof(SyntaxUnionField));
  field->header = syntax_node_create(SYNTAX_KIND_UNION_FIELD, span_consumed(span, rem));
  field->annotations = ann.list;
  field->id = id;
  field->type = type;

  return syntax_node_result_matched(rem, (SyntaxNode *)field, errors);
}

SyntaxNodeResult parse_union_decl(const SyntaxParser *parser, Span span) {
  SyntaxListResult ann = parse_annotations(parser, span);

  SyntaxMatchResult mres = match_keyword(parser->source, skip_trivia(parser->source, ann.rem), KEYWORD_UNION);
  if (!mres.matched)
    return syntax_node_result_not_match(span);

  Span rem = mres.rem;
  SyntaxErrorList *errors = ann.errors;
  SyntaxIdentifier *id = NULL;
  SyntaxNodeList *generic_params = syntax_nodelist_empty();
  SyntaxNodeList *fields = syntax_nodelist_empty();

  SyntaxNodeResult id_res = parse_identifier(parser, skip_trivia(parser->source, rem));
  if (!id_res.matched) {
    errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_IDENTIFIER, rem));
  } else {
    rem = id_res.rem;
    id = (SyntaxIdentifier *)id_res.node;
    errors = syntax_errorlist_concat(parser->arena, id_res.errors, errors);
  }

  mres = match(parser->source, skip_trivia(parser->source, rem), PUNCTUATION_LT);
  if (mres.matched) {
    rem = mres.rem;

    SyntaxListResult glist = parse_generic_param_list(parser, skip_trivia(parser->source, rem));
    generic_params = glist.list;
    errors = syntax_errorlist_concat(parser->arena, glist.errors, errors);
    rem = glist.rem;

    if (syntax_nodelist_is_empty(generic_params)) {
      errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_IDENTIFIER, rem));
    }

    mres = match(parser->source, skip_trivia(parser->source, rem), PUNCTUATION_GT);
    if (!mres.matched) {
      errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_GT, rem));
    } else {
      rem = mres.rem;
    }
  }

  Span adv = skip_trivia(parser->source, rem);
  mres = match(parser->source, adv, PUNCTUATION_SEMICOLON);
  if (mres.matched) {
    rem = mres.rem;
  } else {
    mres = match(parser->source, adv, PUNCTUATION_LBRACE);
    if (mres.matched) {
      rem = mres.rem;

      SyntaxListResult flist =
          parse_field_list(parser, skip_trivia(parser->source, rem), parse_union_field, SYNTAX_EXPECTED_IDENTIFIER);
      fields = flist.list;
      errors = syntax_errorlist_concat(parser->arena, flist.errors, errors);
      rem = flist.rem;

      mres = match(parser->source, skip_trivia(parser->source, rem), PUNCTUATION_RBRACE);
      if (!mres.matched) {
        errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_RBRACE, rem));
      } else {
        rem = mres.rem;
      }
    } else {
      errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_DECL_BODY, rem));
    }
  }

  SyntaxUnionDecl *decl = arena_alloc(parser->arena, sizeof(SyntaxUnionDecl));
  decl->header = syntax_node_create(SYNTAX_KIND_UNION_DECL, span_consumed(span, rem));
  decl->annotations = ann.list;
  decl->id = id;
  decl->generic_params = generic_params;
  decl->fields = fields;

  return syntax_node_result_matched(rem, (SyntaxNode *)decl, errors);
}

static SyntaxNodeResult parse_variant_field(const SyntaxParser *parser, Span span) {
  SyntaxListResult ann = parse_annotations(parser, span);
  Span rem = ann.rem;
  SyntaxErrorList *errors = ann.errors;
  SyntaxIdentifier *id = NULL;
  SyntaxNode *type = NULL;

  SyntaxNodeResult id_res = parse_identifier(parser, skip_trivia(parser->source, rem));
  if (!id_res.matched)
    return syntax_node_result_not_match(span);

  rem = id_res.rem;
  id = (SyntaxIdentifier *)id_res.node;
  errors = syntax_errorlist_concat(parser->arena, id_res.errors, errors);

  SyntaxMatchResult mres = match(parser->source, skip_trivia(parser->source, rem), PUNCTUATION_COLON);
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

  SyntaxVariantField *field = arena_alloc(parser->arena, sizeof(SyntaxVariantField));
  field->header = syntax_node_create(SYNTAX_KIND_VARIANT_FIELD, span_consumed(span, rem));
  field->annotations = ann.list;
  field->id = id;
  field->type = type;

  return syntax_node_result_matched(rem, (SyntaxNode *)field, errors);
}

SyntaxNodeResult parse_variant_decl(const SyntaxParser *parser, Span span) {
  SyntaxListResult ann = parse_annotations(parser, span);

  SyntaxMatchResult mres = match_keyword(parser->source, skip_trivia(parser->source, ann.rem), KEYWORD_VARIANT);
  if (!mres.matched)
    return syntax_node_result_not_match(span);

  Span rem = mres.rem;
  SyntaxErrorList *errors = ann.errors;
  SyntaxIdentifier *id = NULL;
  SyntaxNode *behind_type = NULL;
  SyntaxNodeList *generic_params = syntax_nodelist_empty();
  SyntaxNodeList *fields = syntax_nodelist_empty();

  SyntaxNodeResult id_res = parse_identifier(parser, skip_trivia(parser->source, rem));
  if (!id_res.matched) {
    errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_IDENTIFIER, rem));
  } else {
    rem = id_res.rem;
    id = (SyntaxIdentifier *)id_res.node;
    errors = syntax_errorlist_concat(parser->arena, id_res.errors, errors);
  }

  mres = match(parser->source, skip_trivia(parser->source, rem), PUNCTUATION_LT);
  if (mres.matched) {
    rem = mres.rem;

    SyntaxListResult glist = parse_generic_param_list(parser, skip_trivia(parser->source, rem));
    generic_params = glist.list;
    errors = syntax_errorlist_concat(parser->arena, glist.errors, errors);
    rem = glist.rem;

    if (syntax_nodelist_is_empty(generic_params)) {
      errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_IDENTIFIER, rem));
    }

    mres = match(parser->source, skip_trivia(parser->source, rem), PUNCTUATION_GT);
    if (!mres.matched) {
      errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_GT, rem));
    } else {
      rem = mres.rem;
    }
  }

  mres = match(parser->source, skip_trivia(parser->source, rem), PUNCTUATION_COLON);
  if (mres.matched) {
    rem = mres.rem;

    SyntaxNodeResult type_res = parse_type(parser, skip_trivia(parser->source, rem));
    if (!type_res.matched) {
      errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_TYPE, rem));
    } else {
      rem = type_res.rem;
      behind_type = type_res.node;
      errors = syntax_errorlist_concat(parser->arena, type_res.errors, errors);
    }
  }

  Span adv = skip_trivia(parser->source, rem);
  mres = match(parser->source, adv, PUNCTUATION_SEMICOLON);
  if (mres.matched) {
    rem = mres.rem;
  } else {
    mres = match(parser->source, adv, PUNCTUATION_LBRACE);
    if (mres.matched) {
      rem = mres.rem;

      SyntaxListResult flist =
          parse_field_list(parser, skip_trivia(parser->source, rem), parse_variant_field, SYNTAX_EXPECTED_IDENTIFIER);
      fields = flist.list;
      errors = syntax_errorlist_concat(parser->arena, flist.errors, errors);
      rem = flist.rem;

      mres = match(parser->source, skip_trivia(parser->source, rem), PUNCTUATION_RBRACE);
      if (!mres.matched) {
        errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_RBRACE, rem));
      } else {
        rem = mres.rem;
      }
    } else {
      errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_DECL_BODY, rem));
    }
  }

  SyntaxVariantDecl *decl = arena_alloc(parser->arena, sizeof(SyntaxVariantDecl));
  decl->header = syntax_node_create(SYNTAX_KIND_VARIANT_DECL, span_consumed(span, rem));
  decl->annotations = ann.list;
  decl->id = id;
  decl->behind_type = behind_type;
  decl->generic_params = generic_params;
  decl->fields = fields;

  return syntax_node_result_matched(rem, (SyntaxNode *)decl, errors);
}

SyntaxNodeResult parse_contract_decl(const SyntaxParser *parser, Span span) {
  SyntaxListResult ann = parse_annotations(parser, span);

  SyntaxMatchResult mres = match_keyword(parser->source, skip_trivia(parser->source, ann.rem), KEYWORD_CONTRACT);
  if (!mres.matched)
    return syntax_node_result_not_match(span);

  Span rem = mres.rem;
  SyntaxErrorList *errors = ann.errors;
  SyntaxIdentifier *id = NULL;
  SyntaxNodeList *generic_params = syntax_nodelist_empty();
  SyntaxNodeList *call_params = syntax_nodelist_empty();
  SyntaxNode *return_type = NULL;

  SyntaxNodeResult id_res = parse_identifier(parser, skip_trivia(parser->source, rem));
  if (!id_res.matched) {
    errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_IDENTIFIER, rem));
  } else {
    rem = id_res.rem;
    id = (SyntaxIdentifier *)id_res.node;
    errors = syntax_errorlist_concat(parser->arena, id_res.errors, errors);
  }

  mres = match(parser->source, skip_trivia(parser->source, rem), PUNCTUATION_LT);
  if (mres.matched) {
    rem = mres.rem;

    SyntaxListResult glist = parse_generic_param_list(parser, skip_trivia(parser->source, rem));
    generic_params = glist.list;
    errors = syntax_errorlist_concat(parser->arena, glist.errors, errors);
    rem = glist.rem;

    if (syntax_nodelist_is_empty(generic_params)) {
      errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_IDENTIFIER, rem));
    }

    mres = match(parser->source, skip_trivia(parser->source, rem), PUNCTUATION_GT);
    if (!mres.matched) {
      errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_GT, rem));
    } else {
      rem = mres.rem;
    }
  }

  mres = match(parser->source, skip_trivia(parser->source, rem), PUNCTUATION_LPAREN);
  if (mres.matched) {
    rem = mres.rem;

    SyntaxListResult clist = parse_call_param_list(parser, skip_trivia(parser->source, rem));
    call_params = clist.list;
    errors = syntax_errorlist_concat(parser->arena, clist.errors, errors);
    rem = clist.rem;

    mres = match(parser->source, skip_trivia(parser->source, rem), PUNCTUATION_RPAREN);
    if (!mres.matched) {
      errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_RPAREN, rem));
    } else {
      rem = mres.rem;
    }
  } else {
    errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_LPAREN, rem));
  }

  mres = match(parser->source, skip_trivia(parser->source, rem), PUNCTUATION_COLON);
  if (mres.matched) {
    rem = mres.rem;

    SyntaxNodeResult type_res = parse_type(parser, skip_trivia(parser->source, rem));
    if (!type_res.matched) {
      errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_TYPE, rem));
    } else {
      rem = type_res.rem;
      return_type = type_res.node;
      errors = syntax_errorlist_concat(parser->arena, type_res.errors, errors);
    }
  }

  mres = match(parser->source, skip_trivia(parser->source, rem), PUNCTUATION_SEMICOLON);
  if (!mres.matched) {
    errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_SEMICOLON, rem));
  } else {
    rem = mres.rem;
  }

  SyntaxContractDecl *decl = arena_alloc(parser->arena, sizeof(SyntaxContractDecl));
  decl->header = syntax_node_create(SYNTAX_KIND_CONTRACT_DECL, span_consumed(span, rem));
  decl->annotations = ann.list;
  decl->id = id;
  decl->generic_params = generic_params;
  decl->call_params = call_params;
  decl->return_type = return_type;

  return syntax_node_result_matched(rem, (SyntaxNode *)decl, errors);
}

SyntaxNodeResult parse_func_decl(const SyntaxParser *parser, Span span) {
  SyntaxListResult ann = parse_annotations(parser, span);

  SyntaxMatchResult mres = match_keyword(parser->source, skip_trivia(parser->source, ann.rem), KEYWORD_FUNC);
  if (!mres.matched)
    return syntax_node_result_not_match(span);

  Span rem = mres.rem;
  SyntaxErrorList *errors = ann.errors;
  SyntaxIdentifier *id = NULL;
  SyntaxNodeList *generic_params = syntax_nodelist_empty();
  SyntaxNodeList *call_params = syntax_nodelist_empty();
  SyntaxIdentifier *callconv = NULL;
  SyntaxNode *return_type = NULL;
  SyntaxNodeList *fulfills = syntax_nodelist_empty();
  SyntaxNode *body = NULL;

  SyntaxNodeResult id_res = parse_identifier(parser, skip_trivia(parser->source, rem));
  if (!id_res.matched) {
    errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_IDENTIFIER, rem));
  } else {
    rem = id_res.rem;
    id = (SyntaxIdentifier *)id_res.node;
    errors = syntax_errorlist_concat(parser->arena, id_res.errors, errors);
  }

  mres = match(parser->source, skip_trivia(parser->source, rem), PUNCTUATION_LT);
  if (mres.matched) {
    rem = mres.rem;

    SyntaxListResult glist = parse_generic_param_list(parser, skip_trivia(parser->source, rem));
    generic_params = glist.list;
    errors = syntax_errorlist_concat(parser->arena, glist.errors, errors);
    rem = glist.rem;

    if (syntax_nodelist_is_empty(generic_params)) {
      errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_IDENTIFIER, rem));
    }

    mres = match(parser->source, skip_trivia(parser->source, rem), PUNCTUATION_GT);
    if (!mres.matched) {
      errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_GT, rem));
    } else {
      rem = mres.rem;
    }
  }

  mres = match(parser->source, skip_trivia(parser->source, rem), PUNCTUATION_LPAREN);
  if (mres.matched) {
    rem = mres.rem;

    SyntaxListResult clist = parse_call_param_list(parser, skip_trivia(parser->source, rem));
    call_params = clist.list;
    errors = syntax_errorlist_concat(parser->arena, clist.errors, errors);
    rem = clist.rem;

    mres = match(parser->source, skip_trivia(parser->source, rem), PUNCTUATION_RPAREN);
    if (!mres.matched) {
      errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_RPAREN, rem));
    } else {
      rem = mres.rem;
    }
  } else {
    errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_LPAREN, rem));
  }

  // CallConv reads any identifier; "fulfills" is tried first so the
  // keyword can start its clause instead of being lexed as the name.
  // Whether the name is a supported calling convention is sema's call.
  Span adv = skip_trivia(parser->source, rem);
  mres = match_keyword(parser->source, adv, KEYWORD_FULFILLS);
  if (!mres.matched) {
    SyntaxNodeResult cc_res = parse_identifier(parser, adv);
    if (cc_res.matched) {
      rem = cc_res.rem;
      callconv = (SyntaxIdentifier *)cc_res.node;
      errors = syntax_errorlist_concat(parser->arena, cc_res.errors, errors);
    }
  }

  mres = match(parser->source, skip_trivia(parser->source, rem), PUNCTUATION_COLON);
  if (mres.matched) {
    rem = mres.rem;

    SyntaxNodeResult type_res = parse_type(parser, skip_trivia(parser->source, rem));
    if (!type_res.matched) {
      errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_TYPE, rem));
    } else {
      rem = type_res.rem;
      return_type = type_res.node;
      errors = syntax_errorlist_concat(parser->arena, type_res.errors, errors);
    }
  }

  mres = match_keyword(parser->source, skip_trivia(parser->source, rem), KEYWORD_FULFILLS);
  if (mres.matched) {
    rem = mres.rem;

    SyntaxNodeResult nres = parse_named(parser, skip_trivia(parser->source, rem));
    if (!nres.matched) {
      errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_TYPE, rem));
    } else {
      rem = nres.rem;
      fulfills = syntax_nodelist_prepend(parser->arena, fulfills, nres.node);
      errors = syntax_errorlist_concat(parser->arena, nres.errors, errors);

      while (true) {
        SyntaxMatchResult sep = match(parser->source, skip_trivia(parser->source, rem), PUNCTUATION_COMMA);
        if (!sep.matched)
          break;
        rem = sep.rem;

        nres = parse_named(parser, skip_trivia(parser->source, rem));
        if (!nres.matched) {
          errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_TYPE, rem));
          break;
        }

        rem = nres.rem;
        fulfills = syntax_nodelist_prepend(parser->arena, fulfills, nres.node);
        errors = syntax_errorlist_concat(parser->arena, nres.errors, errors);
      }
    }
  }

  SyntaxNodeResult body_res = parse_body_position(parser, rem);
  rem = body_res.rem;
  errors = syntax_errorlist_concat(parser->arena, body_res.errors, errors);
  body = body_res.node;

  SyntaxFuncDecl *decl = arena_alloc(parser->arena, sizeof(SyntaxFuncDecl));
  decl->header = syntax_node_create(SYNTAX_KIND_FUNC_DECL, span_consumed(span, rem));
  decl->annotations = ann.list;
  decl->id = id;
  decl->generic_params = generic_params;
  decl->call_params = call_params;
  decl->callconv = callconv;
  decl->return_type = return_type;
  decl->fulfills = fulfills;
  decl->body = body;

  return syntax_node_result_matched(rem, (SyntaxNode *)decl, errors);
}

#pragma endregion

#pragma region STMT

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
  SyntaxMatchResult mres = match(parser->source, span, PUNCTUATION_SEMICOLON);
  if (!mres.matched)
    return syntax_node_result_not_match(span);

  SyntaxEmptyStmt *stmt = arena_alloc(parser->arena, sizeof(SyntaxEmptyStmt));
  stmt->header = syntax_node_create(SYNTAX_KIND_EMPTY_STMT, span_consumed(span, mres.rem));

  return syntax_node_result_matched(mres.rem, (SyntaxNode *)stmt, NULL);
}

SyntaxNodeResult parse_body_stmt(const SyntaxParser *parser, Span span) {
  SyntaxMatchResult mres = match(parser->source, span, PUNCTUATION_LBRACE);
  if (!mres.matched)
    return syntax_node_result_not_match(span);

  Span rem = mres.rem;
  SyntaxErrorList *errors = syntax_errorlist_empty();
  SyntaxNodeList *stmts = syntax_nodelist_empty();

  while (true) {
    Span adv = skip_trivia(parser->source, rem);

    // rem stays before the brace here; the closing check consumes it.
    if (match(parser->source, adv, PUNCTUATION_RBRACE).matched)
      break;

    SyntaxNodeResult res = parse_stmt(parser, adv);
    if (!res.matched)
      break; // neither a statement nor "}": the closing check reports

    rem = res.rem;
    errors = syntax_errorlist_concat(parser->arena, res.errors, errors);
    stmts = syntax_nodelist_prepend(parser->arena, stmts, res.node);
  }

  SyntaxMatchResult close = match(parser->source, skip_trivia(parser->source, rem), PUNCTUATION_RBRACE);
  if (!close.matched) {
    errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_RBRACE, rem));
  } else {
    rem = close.rem;
  }

  SyntaxBodyStmt *stmt = arena_alloc(parser->arena, sizeof(SyntaxBodyStmt));
  stmt->header = syntax_node_create(SYNTAX_KIND_BODY_STMT, span_consumed(span, rem));
  stmt->stmts = stmts;

  return syntax_node_result_matched(rem, (SyntaxNode *)stmt, errors);
}

SyntaxNodeResult parse_let_stmt(const SyntaxParser *parser, Span span) {
  SyntaxMatchResult mres = match_keyword(parser->source, span, KEYWORD_LET);
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

  mres = match(parser->source, skip_trivia(parser->source, rem), PUNCTUATION_COLON);
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

  mres = match(parser->source, skip_trivia(parser->source, rem), PUNCTUATION_EQUALS);
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

  mres = match(parser->source, skip_trivia(parser->source, rem), PUNCTUATION_SEMICOLON);
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
  SyntaxMatchResult mres = match_keyword(parser->source, span, KEYWORD_SET);
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

  mres = match(parser->source, skip_trivia(parser->source, rem), PUNCTUATION_EQUALS);
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

  mres = match(parser->source, skip_trivia(parser->source, rem), PUNCTUATION_SEMICOLON);
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

  SyntaxMatchResult mres = match(parser->source, skip_trivia(parser->source, rem), PUNCTUATION_SEMICOLON);
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
  SyntaxMatchResult mres = match_keyword(parser->source, span, KEYWORD_IF);
  if (!mres.matched)
    return syntax_node_result_not_match(span);

  Span rem = mres.rem;
  SyntaxErrorList *errors = syntax_errorlist_empty();
  SyntaxNode *condition = NULL;
  SyntaxNode *then_stmt = NULL;
  SyntaxNode *else_stmt = NULL;

  mres = match(parser->source, skip_trivia(parser->source, rem), PUNCTUATION_LPAREN);
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

    mres = match(parser->source, skip_trivia(parser->source, rem), PUNCTUATION_RPAREN);
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

  mres = match_keyword(parser->source, skip_trivia(parser->source, rem), KEYWORD_ELSE);
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
  SyntaxMatchResult mres = match_keyword(parser->source, span, KEYWORD_LOOP);
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
  SyntaxMatchResult mres = match_keyword(parser->source, span, KEYWORD_BREAK);
  if (!mres.matched)
    return syntax_node_result_not_match(span);

  Span rem = mres.rem;
  SyntaxErrorList *errors = syntax_errorlist_empty();

  mres = match(parser->source, skip_trivia(parser->source, rem), PUNCTUATION_SEMICOLON);
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
  SyntaxMatchResult mres = match_keyword(parser->source, span, KEYWORD_CONTINUE);
  if (!mres.matched)
    return syntax_node_result_not_match(span);

  Span rem = mres.rem;
  SyntaxErrorList *errors = syntax_errorlist_empty();

  mres = match(parser->source, skip_trivia(parser->source, rem), PUNCTUATION_SEMICOLON);
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
  SyntaxMatchResult mres = match_keyword(parser->source, span, KEYWORD_RETURN);
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

  mres = match(parser->source, skip_trivia(parser->source, rem), PUNCTUATION_SEMICOLON);
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
  SyntaxMatchResult mres = match_keyword(parser->source, span, KEYWORD_WHILE);
  if (!mres.matched)
    return syntax_node_result_not_match(span);

  Span rem = mres.rem;
  SyntaxErrorList *errors = syntax_errorlist_empty();
  SyntaxNode *condition = NULL;

  mres = match(parser->source, skip_trivia(parser->source, rem), PUNCTUATION_LPAREN);
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

    mres = match(parser->source, skip_trivia(parser->source, rem), PUNCTUATION_RPAREN);
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

static SyntaxNodeResult parse_struct_lit_field(const SyntaxParser *parser, Span span) {
  SyntaxNodeResult id_res = parse_identifier(parser, span);
  if (!id_res.matched)
    return syntax_node_result_not_match(span);

  Span rem = id_res.rem;
  SyntaxErrorList *errors = id_res.errors;
  SyntaxNode *value = NULL;

  SyntaxMatchResult mres = match(parser->source, skip_trivia(parser->source, rem), PUNCTUATION_EQUALS);
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

  SyntaxStructLitField *field = arena_alloc(parser->arena, sizeof(SyntaxStructLitField));
  field->header = syntax_node_create(SYNTAX_KIND_STRUCT_LIT_FIELD, span_consumed(span, rem));
  field->id = (SyntaxIdentifier *)id_res.node;
  field->value = value;

  return syntax_node_result_matched(rem, (SyntaxNode *)field, errors);
}

SyntaxNodeResult parse_struct_lit_expr(const SyntaxParser *parser, Span span) {
  SyntaxNodeResult named = parse_named(parser, span);
  if (!named.matched)
    return syntax_node_result_not_match(span);

  SyntaxMatchResult mres = match(parser->source, skip_trivia(parser->source, named.rem), PUNCTUATION_LBRACE);
  if (!mres.matched)
    return syntax_node_result_not_match(span);

  Span rem = mres.rem;
  SyntaxErrorList *errors = named.errors;

  SyntaxListResult flist =
      parse_field_list(parser, skip_trivia(parser->source, rem), parse_struct_lit_field, SYNTAX_EXPECTED_IDENTIFIER);
  rem = flist.rem;
  errors = syntax_errorlist_concat(parser->arena, flist.errors, errors);

  mres = match(parser->source, skip_trivia(parser->source, rem), PUNCTUATION_RBRACE);
  if (!mres.matched) {
    errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_RBRACE, rem));
  } else {
    rem = mres.rem;
  }

  SyntaxStructLitExpr *expr = arena_alloc(parser->arena, sizeof(SyntaxStructLitExpr));
  expr->header = syntax_node_create(SYNTAX_KIND_STRUCT_LIT_EXPR, span_consumed(span, rem));
  expr->type = (SyntaxNamed *)named.node;
  expr->fields = flist.list;

  return syntax_node_result_matched(rem, (SyntaxNode *)expr, errors);
}

SyntaxNodeResult parse_array_lit_expr(const SyntaxParser *parser, Span span) {
  SyntaxNodeResult type_res = parse_array_type(parser, span);
  if (!type_res.matched)
    return syntax_node_result_not_match(span);

  SyntaxMatchResult mres = match(parser->source, skip_trivia(parser->source, type_res.rem), PUNCTUATION_LBRACE);
  if (!mres.matched)
    return syntax_node_result_not_match(span);

  Span rem = mres.rem;
  SyntaxErrorList *errors = type_res.errors;

  SyntaxListResult elist = parse_field_list(parser, skip_trivia(parser->source, rem), parse_expr, SYNTAX_EXPECTED_EXPR);
  rem = elist.rem;
  errors = syntax_errorlist_concat(parser->arena, elist.errors, errors);

  mres = match(parser->source, skip_trivia(parser->source, rem), PUNCTUATION_RBRACE);
  if (!mres.matched) {
    errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_RBRACE, rem));
  } else {
    rem = mres.rem;
  }

  SyntaxArrayLitExpr *expr = arena_alloc(parser->arena, sizeof(SyntaxArrayLitExpr));
  expr->header = syntax_node_create(SYNTAX_KIND_ARRAY_LIT_EXPR, span_consumed(span, rem));
  expr->type = type_res.node;
  expr->elements = elist.list;

  return syntax_node_result_matched(rem, (SyntaxNode *)expr, errors);
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
