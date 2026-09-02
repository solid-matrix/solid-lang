/**
 * @file parse_decl.c
 * @brief Declaration parsers.
 * @author solid-matrix
 */

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include "arena.h"
#include "parse.h"
#include "span.h"
#include "syntax_error.h"
#include "syntax_node.h"

SyntaxNodeResult parse_decl(const SyntaxParser *parser, Span span) {
  SyntaxNodeResult results[] = {
      parse_namespace_decl(parser, span), parse_using_decl(parser, span),    parse_let_decl(parser, span),
      parse_struct_decl(parser, span),    parse_union_decl(parser, span),    parse_enum_decl(parser, span),
      parse_variant_decl(parser, span),   parse_contract_decl(parser, span), parse_func_decl(parser, span),
  };
  return complete_longest_match(results, COUNT_OF(results));
}

SyntaxNodeResult parse_program(const SyntaxParser *parser, Span span) {
  span = skip_trivia(parser->source, span);

  SyntaxErrorList *errors = syntax_errorlist_empty();
  SyntaxNodeList *decls = syntax_nodelist_empty();
  Span rem = span;

  SyntaxNodeResult res = parse_namespace_decl(parser, skip_trivia(parser->source, rem));
  if (res.matched) {
    errors = syntax_errorlist_concat(parser->arena, res.errors, errors);
    rem = res.rem;
    decls = syntax_nodelist_prepend(parser->arena, decls, res.node);
  }

  while (true) {
    SyntaxNodeResult res = parse_using_decl(parser, skip_trivia(parser->source, rem));
    if (!res.matched)
      break;

    errors = syntax_errorlist_concat(parser->arena, res.errors, errors);
    rem = res.rem;
    decls = syntax_nodelist_prepend(parser->arena, decls, res.node);
  }

  while (true) {
    res = parse_decl(parser, skip_trivia(parser->source, rem));
    if (!res.matched)
      break;

    errors = syntax_errorlist_concat(parser->arena, res.errors, errors);
    rem = res.rem;

    if (res.node->kind == SYNTAX_KIND_NAMESPACE_DECL) {
      SyntaxErrorCode code = SYNTAX_MISPLACED_NAMESPACE;
      errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(code, res.node->span));
    } else if (res.node->kind == SYNTAX_KIND_USING_DECL) {
      SyntaxErrorCode code = SYNTAX_MISPLACED_USING;
      errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(code, res.node->span));
    } else {
      decls = syntax_nodelist_prepend(parser->arena, decls, res.node);
    }
  }

  SyntaxProgram *program = arena_alloc(parser->arena, sizeof(SyntaxProgram));
  program->header = syntax_node_create(SYNTAX_KIND_PROGRAM, span_consumed(span, rem));
  program->top_levels = syntax_nodelist_reverse(parser->arena, decls);

  rem = skip_trivia(parser->source, rem);

  if (!span_is_empty(rem))
    errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_EOF, rem));

  return syntax_node_result_matched(rem, (SyntaxNode *)program, errors);
}

SyntaxNodeResult parse_namespace_decl(const SyntaxParser *parser, Span span) {
  SyntaxMatchResult mres = match_keyword(parser, span, KEYWORD_NAMESPACE);
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

  mres = match(parser, skip_trivia(parser->source, rem), PUNCTUATION_SEMICOLON);
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
  SyntaxMatchResult mres = match_keyword(parser, span, KEYWORD_USING);
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

  mres = match(parser, skip_trivia(parser->source, rem), PUNCTUATION_SEMICOLON);
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

  SyntaxMatchResult mres = match_keyword(parser, skip_trivia(parser->source, ann.rem), KEYWORD_LET);
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
  mres = match(parser, skip_trivia(parser->source, rem), PUNCTUATION_COLON);
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
  mres = match(parser, skip_trivia(parser->source, rem), PUNCTUATION_EQUALS);
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

  mres = match(parser, skip_trivia(parser->source, rem), PUNCTUATION_SEMICOLON);
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

/**
 * @brief Parses one struct field `[annotations] name : type`.
 * @param parser Parsing context.
 * @param span Where the construct starts.
 * @return Parse outcome; see SyntaxNodeResult.
 */
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

  SyntaxStructField *field = arena_alloc(parser->arena, sizeof(SyntaxStructField));
  field->header = syntax_node_create(SYNTAX_KIND_STRUCT_FIELD, span_consumed(span, rem));
  field->annotations = ann.list;
  field->id = id;
  field->type = type;

  return syntax_node_result_matched(rem, (SyntaxNode *)field, errors);
}

SyntaxNodeResult parse_struct_decl(const SyntaxParser *parser, Span span) {
  SyntaxListResult ann = parse_annotations(parser, span);

  SyntaxMatchResult mres = match_keyword(parser, skip_trivia(parser->source, ann.rem), KEYWORD_STRUCT);
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

  mres = match(parser, skip_trivia(parser->source, rem), PUNCTUATION_LT);
  if (mres.matched) {
    rem = mres.rem;

    SyntaxListResult glist = parse_generic_param_list(parser, skip_trivia(parser->source, rem));
    generic_params = glist.list;
    errors = syntax_errorlist_concat(parser->arena, glist.errors, errors);
    rem = glist.rem;

    if (syntax_nodelist_is_empty(generic_params)) {
      errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_IDENTIFIER, rem));
    }

    mres = match(parser, skip_trivia(parser->source, rem), PUNCTUATION_GT);
    if (!mres.matched) {
      errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_GT, rem));
    } else {
      rem = mres.rem;
    }
  }

  Span adv = skip_trivia(parser->source, rem);
  mres = match(parser, adv, PUNCTUATION_SEMICOLON);
  if (mres.matched) {
    rem = mres.rem;
  } else {
    mres = match(parser, adv, PUNCTUATION_LBRACE);
    if (mres.matched) {
      rem = mres.rem;

      SyntaxListResult flist =
          parse_field_list(parser, skip_trivia(parser->source, rem), parse_struct_field, SYNTAX_EXPECTED_IDENTIFIER);
      fields = flist.list;
      errors = syntax_errorlist_concat(parser->arena, flist.errors, errors);
      rem = flist.rem;

      mres = match(parser, skip_trivia(parser->source, rem), PUNCTUATION_RBRACE);
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

/**
 * @brief Parses one enum field `[annotations] name [= value]`.
 * @param parser Parsing context.
 * @param span Where the construct starts.
 * @return Parse outcome; see SyntaxNodeResult.
 */
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

  SyntaxMatchResult mres = match(parser, skip_trivia(parser->source, rem), PUNCTUATION_EQUALS);
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

  SyntaxMatchResult mres = match_keyword(parser, skip_trivia(parser->source, ann.rem), KEYWORD_ENUM);
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

  mres = match(parser, skip_trivia(parser->source, rem), PUNCTUATION_COLON);
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
  mres = match(parser, adv, PUNCTUATION_SEMICOLON);
  if (mres.matched) {
    rem = mres.rem;
  } else {
    mres = match(parser, adv, PUNCTUATION_LBRACE);
    if (mres.matched) {
      rem = mres.rem;

      SyntaxListResult flist =
          parse_field_list(parser, skip_trivia(parser->source, rem), parse_enum_field, SYNTAX_EXPECTED_IDENTIFIER);
      fields = flist.list;
      errors = syntax_errorlist_concat(parser->arena, flist.errors, errors);
      rem = flist.rem;

      mres = match(parser, skip_trivia(parser->source, rem), PUNCTUATION_RBRACE);
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

/**
 * @brief Parses one union field `[annotations] name : type`.
 * @param parser Parsing context.
 * @param span Where the construct starts.
 * @return Parse outcome; see SyntaxNodeResult.
 */
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

  SyntaxUnionField *field = arena_alloc(parser->arena, sizeof(SyntaxUnionField));
  field->header = syntax_node_create(SYNTAX_KIND_UNION_FIELD, span_consumed(span, rem));
  field->annotations = ann.list;
  field->id = id;
  field->type = type;

  return syntax_node_result_matched(rem, (SyntaxNode *)field, errors);
}

SyntaxNodeResult parse_union_decl(const SyntaxParser *parser, Span span) {
  SyntaxListResult ann = parse_annotations(parser, span);

  SyntaxMatchResult mres = match_keyword(parser, skip_trivia(parser->source, ann.rem), KEYWORD_UNION);
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

  mres = match(parser, skip_trivia(parser->source, rem), PUNCTUATION_LT);
  if (mres.matched) {
    rem = mres.rem;

    SyntaxListResult glist = parse_generic_param_list(parser, skip_trivia(parser->source, rem));
    generic_params = glist.list;
    errors = syntax_errorlist_concat(parser->arena, glist.errors, errors);
    rem = glist.rem;

    if (syntax_nodelist_is_empty(generic_params)) {
      errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_IDENTIFIER, rem));
    }

    mres = match(parser, skip_trivia(parser->source, rem), PUNCTUATION_GT);
    if (!mres.matched) {
      errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_GT, rem));
    } else {
      rem = mres.rem;
    }
  }

  Span adv = skip_trivia(parser->source, rem);
  mres = match(parser, adv, PUNCTUATION_SEMICOLON);
  if (mres.matched) {
    rem = mres.rem;
  } else {
    mres = match(parser, adv, PUNCTUATION_LBRACE);
    if (mres.matched) {
      rem = mres.rem;

      SyntaxListResult flist =
          parse_field_list(parser, skip_trivia(parser->source, rem), parse_union_field, SYNTAX_EXPECTED_IDENTIFIER);
      fields = flist.list;
      errors = syntax_errorlist_concat(parser->arena, flist.errors, errors);
      rem = flist.rem;

      mres = match(parser, skip_trivia(parser->source, rem), PUNCTUATION_RBRACE);
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

/**
 * @brief Parses one variant field `[annotations] name [: type]`.
 * @param parser Parsing context.
 * @param span Where the construct starts.
 * @return Parse outcome; see SyntaxNodeResult.
 */
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

  SyntaxVariantField *field = arena_alloc(parser->arena, sizeof(SyntaxVariantField));
  field->header = syntax_node_create(SYNTAX_KIND_VARIANT_FIELD, span_consumed(span, rem));
  field->annotations = ann.list;
  field->id = id;
  field->type = type;

  return syntax_node_result_matched(rem, (SyntaxNode *)field, errors);
}

SyntaxNodeResult parse_variant_decl(const SyntaxParser *parser, Span span) {
  SyntaxListResult ann = parse_annotations(parser, span);

  SyntaxMatchResult mres = match_keyword(parser, skip_trivia(parser->source, ann.rem), KEYWORD_VARIANT);
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

  mres = match(parser, skip_trivia(parser->source, rem), PUNCTUATION_LT);
  if (mres.matched) {
    rem = mres.rem;

    SyntaxListResult glist = parse_generic_param_list(parser, skip_trivia(parser->source, rem));
    generic_params = glist.list;
    errors = syntax_errorlist_concat(parser->arena, glist.errors, errors);
    rem = glist.rem;

    if (syntax_nodelist_is_empty(generic_params)) {
      errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_IDENTIFIER, rem));
    }

    mres = match(parser, skip_trivia(parser->source, rem), PUNCTUATION_GT);
    if (!mres.matched) {
      errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_GT, rem));
    } else {
      rem = mres.rem;
    }
  }

  mres = match(parser, skip_trivia(parser->source, rem), PUNCTUATION_COLON);
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
  mres = match(parser, adv, PUNCTUATION_SEMICOLON);
  if (mres.matched) {
    rem = mres.rem;
  } else {
    mres = match(parser, adv, PUNCTUATION_LBRACE);
    if (mres.matched) {
      rem = mres.rem;

      SyntaxListResult flist =
          parse_field_list(parser, skip_trivia(parser->source, rem), parse_variant_field, SYNTAX_EXPECTED_IDENTIFIER);
      fields = flist.list;
      errors = syntax_errorlist_concat(parser->arena, flist.errors, errors);
      rem = flist.rem;

      mres = match(parser, skip_trivia(parser->source, rem), PUNCTUATION_RBRACE);
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

  SyntaxMatchResult mres = match_keyword(parser, skip_trivia(parser->source, ann.rem), KEYWORD_CONTRACT);
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

  mres = match(parser, skip_trivia(parser->source, rem), PUNCTUATION_LT);
  if (mres.matched) {
    rem = mres.rem;

    SyntaxListResult glist = parse_generic_param_list(parser, skip_trivia(parser->source, rem));
    generic_params = glist.list;
    errors = syntax_errorlist_concat(parser->arena, glist.errors, errors);
    rem = glist.rem;

    if (syntax_nodelist_is_empty(generic_params)) {
      errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_IDENTIFIER, rem));
    }

    mres = match(parser, skip_trivia(parser->source, rem), PUNCTUATION_GT);
    if (!mres.matched) {
      errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_GT, rem));
    } else {
      rem = mres.rem;
    }
  }

  mres = match(parser, skip_trivia(parser->source, rem), PUNCTUATION_LPAREN);
  if (mres.matched) {
    rem = mres.rem;

    SyntaxListResult clist = parse_call_param_list(parser, skip_trivia(parser->source, rem));
    call_params = clist.list;
    errors = syntax_errorlist_concat(parser->arena, clist.errors, errors);
    rem = clist.rem;

    mres = match(parser, skip_trivia(parser->source, rem), PUNCTUATION_RPAREN);
    if (!mres.matched) {
      errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_RPAREN, rem));
    } else {
      rem = mres.rem;
    }
  } else {
    errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_LPAREN, rem));
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

  mres = match(parser, skip_trivia(parser->source, rem), PUNCTUATION_SEMICOLON);
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

  SyntaxMatchResult mres = match_keyword(parser, skip_trivia(parser->source, ann.rem), KEYWORD_FUNC);
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

  mres = match(parser, skip_trivia(parser->source, rem), PUNCTUATION_LT);
  if (mres.matched) {
    rem = mres.rem;

    SyntaxListResult glist = parse_generic_param_list(parser, skip_trivia(parser->source, rem));
    generic_params = glist.list;
    errors = syntax_errorlist_concat(parser->arena, glist.errors, errors);
    rem = glist.rem;

    if (syntax_nodelist_is_empty(generic_params)) {
      errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_IDENTIFIER, rem));
    }

    mres = match(parser, skip_trivia(parser->source, rem), PUNCTUATION_GT);
    if (!mres.matched) {
      errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_GT, rem));
    } else {
      rem = mres.rem;
    }
  }

  mres = match(parser, skip_trivia(parser->source, rem), PUNCTUATION_LPAREN);
  if (mres.matched) {
    rem = mres.rem;

    SyntaxListResult clist = parse_call_param_list(parser, skip_trivia(parser->source, rem));
    call_params = clist.list;
    errors = syntax_errorlist_concat(parser->arena, clist.errors, errors);
    rem = clist.rem;

    mres = match(parser, skip_trivia(parser->source, rem), PUNCTUATION_RPAREN);
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
  mres = match_keyword(parser, adv, KEYWORD_FULFILLS);
  if (!mres.matched) {
    SyntaxNodeResult cc_res = parse_identifier(parser, adv);
    if (cc_res.matched) {
      rem = cc_res.rem;
      callconv = (SyntaxIdentifier *)cc_res.node;
      errors = syntax_errorlist_concat(parser->arena, cc_res.errors, errors);
    }
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

  mres = match_keyword(parser, skip_trivia(parser->source, rem), KEYWORD_FULFILLS);
  if (mres.matched) {
    rem = mres.rem;

    SyntaxNodeResult nres = parse_named_type(parser, skip_trivia(parser->source, rem));
    if (!nres.matched) {
      errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_TYPE, rem));
    } else {
      rem = nres.rem;
      fulfills = syntax_nodelist_prepend(parser->arena, fulfills, nres.node);
      errors = syntax_errorlist_concat(parser->arena, nres.errors, errors);

      while (true) {
        SyntaxMatchResult sep = match(parser, skip_trivia(parser->source, rem), PUNCTUATION_COMMA);
        if (!sep.matched)
          break;
        rem = sep.rem;

        nres = parse_named_type(parser, skip_trivia(parser->source, rem));
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
  decl->fulfills = syntax_nodelist_reverse(parser->arena, fulfills);
  decl->body = body;

  return syntax_node_result_matched(rem, (SyntaxNode *)decl, errors);
}
