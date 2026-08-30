/**
 * @file parse_common.c
 * @brief Common building blocks: identifier, compile-time form,
 * annotations, parameters, generic argument and program.
 * @author solid-matrix
 * @version 0.0.5
 */

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include "error.h"
#include "node.h"
#include "parse.h"
#include "source.h"
#include "span.h"
#include "syntax_error.h"

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
  SyntaxMatchResult mres = match(parser, span, PUNCTUATION_AT);
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

  mres = match(parser, skip_trivia(parser->source, rem), PUNCTUATION_LPAREN);
  if (mres.matched) {
    rem = mres.rem;

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
  program->top_levels = syntax_nodelist_reverse(parser->arena, decls);

  rem = skip_trivia(parser->source, rem);

  if (!span_is_empty(rem))
    errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_EOF, rem));

  return syntax_node_result_matched(rem, (SyntaxNode *)program, errors);
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

  return (SyntaxListResult){.list = syntax_nodelist_reverse(parser->arena, list), .errors = errors, .rem = rem};
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

  SyntaxMatchResult mres = match(parser, skip_trivia(parser->source, rem), PUNCTUATION_COLON);
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
  param->header = syntax_node_create(SYNTAX_KIND_GENERIC_PARAM, span_consumed(span, rem));
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

  SyntaxMatchResult mres = match(parser, skip_trivia(parser->source, rem), PUNCTUATION_COLON);
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
  param->header = syntax_node_create(SYNTAX_KIND_CALL_PARAM, span_consumed(span, rem));
  param->annotations = ann.list;
  param->id = id;
  param->type = type;

  return syntax_node_result_matched(rem, (SyntaxNode *)param, errors);
}

SyntaxNodeResult parse_generic_arg(const SyntaxParser *parser, Span span) {
  SyntaxNodeResult probe = parse_identifier(parser, span);
  if (probe.matched) {
    SyntaxMatchResult mres = match(parser, skip_trivia(parser->source, probe.rem), PUNCTUATION_EQUALS);
    if (mres.matched) {
      Span rem = mres.rem;
      SyntaxErrorList *errors = syntax_errorlist_empty();

      SyntaxNodeResult results[] = {
          parse_int_lit_expr(parser, skip_trivia(parser->source, rem)),
          parse_float_lit_expr(parser, skip_trivia(parser->source, rem)),
          parse_rune_lit_expr(parser, skip_trivia(parser->source, rem)),
          parse_string_lit_expr(parser, skip_trivia(parser->source, rem)),
          parse_named_type(parser, skip_trivia(parser->source, rem)),
          parse_sub_expr(parser, skip_trivia(parser->source, rem)),
          parse_struct_lit_expr(parser, skip_trivia(parser->source, rem)),
          parse_array_lit_expr(parser, skip_trivia(parser->source, rem)),
          parse_compile_time(parser, skip_trivia(parser->source, rem)),
      };
      SyntaxNodeResult res = complete_longest_match(results, COUNT_OF(results));

      SyntaxGenericArg *arg = arena_alloc(parser->arena, sizeof(SyntaxGenericArg));
      arg->id = (SyntaxIdentifier *)probe.node;

      if (!res.matched) {
        errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_EXPR, rem));
        arg->header = syntax_node_create(SYNTAX_KIND_GENERIC_ARG, span_consumed(span, rem));
        arg->value = NULL;
        return syntax_node_result_matched(rem, (SyntaxNode *)arg, errors);
      }

      errors = syntax_errorlist_concat(parser->arena, res.errors, errors);
      arg->header = syntax_node_create(SYNTAX_KIND_GENERIC_ARG, span_consumed(span, res.rem));
      arg->value = res.node;
      return syntax_node_result_matched(res.rem, (SyntaxNode *)arg, errors);
    }
  }

  SyntaxNodeResult type_res = parse_type(parser, span);
  if (!type_res.matched)
    return syntax_node_result_not_match(span);

  SyntaxGenericArg *arg = arena_alloc(parser->arena, sizeof(SyntaxGenericArg));
  arg->header = syntax_node_create(SYNTAX_KIND_GENERIC_ARG, span_consumed(span, type_res.rem));
  arg->id = NULL;
  arg->value = type_res.node;

  return syntax_node_result_matched(type_res.rem, (SyntaxNode *)arg, type_res.errors);
}
