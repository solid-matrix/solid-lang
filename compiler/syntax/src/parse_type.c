/**
 * @file parse_type.c
 * @brief Type parsers.
 * @author solid-matrix
 */

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include "parse.h"
#include "span.h"
#include "syntax_error.h"
#include "syntax_node.h"

SyntaxNodeResult parse_type(const SyntaxParser *parser, Span span) {
  SyntaxNodeResult results[] = {
      parse_named_type(parser, span),
      parse_ref_type(parser, span),
      parse_array_type(parser, span),
      parse_func_type(parser, span),
  };
  return complete_longest_match(results, sizeof(results) / sizeof(results[0]));
}

SyntaxNodeResult parse_ref_type(const SyntaxParser *parser, Span span) {
  SyntaxMatchResult mres = match(parser, span, PUNCTUATION_AMP);
  if (!mres.matched)
    return syntax_node_result_not_match(span);

  Span rem = mres.rem;
  SyntaxErrorList *errors = syntax_errorlist_empty();
  SyntaxRefKind ref_kind = SYNTAX_REF_KIND_READWRITE;

  Span adv = skip_trivia(parser->source, rem);
  SyntaxMatchResult kw = match_keyword(parser, adv, KEYWORD_READONLY);
  if (kw.matched) {
    ref_kind = SYNTAX_REF_KIND_READONLY;
    rem = kw.rem;
  } else {
    kw = match_keyword(parser, adv, KEYWORD_WRITEONLY);
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
  SyntaxMatchResult mres = match(parser, span, PUNCTUATION_LBRACKET);
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

  mres = match(parser, skip_trivia(parser->source, rem), PUNCTUATION_RBRACKET);
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
  SyntaxMatchResult mres = match(parser, span, PUNCTUATION_AMP);
  if (!mres.matched)
    return syntax_node_result_not_match(span);

  Span rem = mres.rem;

  mres = match_keyword(parser, skip_trivia(parser->source, rem), KEYWORD_FUNC);
  if (!mres.matched)
    return syntax_node_result_not_match(span);

  rem = mres.rem;
  SyntaxErrorList *errors = syntax_errorlist_empty();
  SyntaxNodeList *call_params = syntax_nodelist_empty();
  SyntaxIdentifier *callconv = NULL;
  SyntaxNode *return_type = NULL;

  mres = match(parser, skip_trivia(parser->source, rem), PUNCTUATION_LPAREN);
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
        mres = match(parser, skip_trivia(parser->source, rem), PUNCTUATION_COMMA);
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

    mres = match(parser, skip_trivia(parser->source, rem), PUNCTUATION_RPAREN);
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

  mres = match(parser, skip_trivia(parser->source, rem), PUNCTUATION_COLON);
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
  type->call_params = syntax_nodelist_reverse(parser->arena, call_params);
  type->callconv = callconv;
  type->return_type = return_type;

  return syntax_node_result_matched(rem, (SyntaxNode *)type, errors);
}

SyntaxNodeResult parse_named_type(const SyntaxParser *parser, Span span) {
  SyntaxListResult lres = parse_identifier_list(parser, span, PUNCTUATION_SCOPE);
  if (syntax_nodelist_is_empty(lres.list))
    return syntax_node_result_not_match(span);

  Span rem = lres.rem;
  SyntaxErrorList *errors = lres.errors;
  SyntaxNodeList *generic_arguments = syntax_nodelist_empty();

  SyntaxMatchResult mres = match(parser, skip_trivia(parser->source, rem), PUNCTUATION_LT);
  if (mres.matched) {
    rem = mres.rem;

    SyntaxListResult glist = parse_generic_arg_list(parser, skip_trivia(parser->source, rem));
    generic_arguments = glist.list;
    errors = syntax_errorlist_concat(parser->arena, glist.errors, errors);
    rem = glist.rem;

    if (syntax_nodelist_is_empty(generic_arguments)) {
      errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_IDENTIFIER, rem));
    }

    mres = match(parser, skip_trivia(parser->source, rem), PUNCTUATION_GT);
    if (!mres.matched) {
      errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_GT, rem));
    } else {
      rem = mres.rem;
    }
  }

  SyntaxNamed *node = arena_alloc(parser->arena, sizeof(SyntaxNamed));
  node->header = syntax_node_create(SYNTAX_KIND_NAMED, span_consumed(span, rem));
  node->path = lres.list;
  node->generic_args = generic_arguments;

  return syntax_node_result_matched(rem, (SyntaxNode *)node, errors);
}
