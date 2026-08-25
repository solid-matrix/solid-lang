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
      .header =
          syntax_node_header(SYNTAX_KIND_IDENTIFIER, span_consumed(span, rem)),
      .strview = source_strview_at(parser->source, span_consumed(span, rem)),
  };

  return parser_result_matched(rem, (SyntaxNode *)id, NULL);
}

ParserResult parse_name_path(const Parser *parser, Span span) {
  // Semantics: segments accumulate newest-at-head. A trailing "::" is
  // consumed into the path and reports exactly one EXPECTED_IDENTIFIER
  // pointing past the separator; a missing FIRST segment is a silent
  // not_match (the enclosing construct reports EXPECTED_NAME_PATH).
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
    if (!(span_len(adv) >= 2 &&
          source_byte_at(parser->source, adv.start) == ':' &&
          source_byte_at(parser->source, adv.start + 1) == ':')) {
      break;
    }
    rem = span_advance(adv, 2);

    adv = skip_trivia(parser->source, rem);
    id_res = parse_identifier(parser, adv);

    if (!id_res.matched) {
      errors = syntax_errorlist_prepend(
          parser->arena, errors,
          syntax_error_create(SYNTAX_EXPECTED_IDENTIFIER, rem));

      break;
    }

    segments = syntax_nodelist_prepend(parser->arena, segments, id_res.node);
    rem = id_res.rem;
  }

  SyntaxNamePath *path = arena_alloc(parser->arena, sizeof(SyntaxNamePath));
  *path = (SyntaxNamePath){
      .header =
          syntax_node_header(SYNTAX_KIND_NAME_PATH, span_consumed(span, rem)),
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
      .header =
          syntax_node_header(SYNTAX_KIND_PROGRAM, span_consumed(span, rem)),
      .top_levels = decls,
  };

  rem = skip_trivia(parser->source, rem);

  if (!span_is_empty(rem))
    errors = syntax_errorlist_prepend(
        parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_EOF, rem));

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
    errors = syntax_errorlist_prepend(
        parser->arena, errors,
        syntax_error_create(SYNTAX_EXPECTED_NAME_PATH, rem));
  } else {
    errors = syntax_errorlist_concat(parser->arena, np_res.errors, errors);
    rem = np_res.rem;

    adv = skip_trivia(parser->source, rem);

    if (!(span_len(adv) > 0 &&
          source_byte_at(parser->source, adv.start) == ';')) {
      errors = syntax_errorlist_prepend(
          parser->arena, errors,
          syntax_error_create(SYNTAX_EXPECTED_SEMICOLON, rem));
    } else {
      rem = span_advance(adv, 1);
    }
  }

  SyntaxNamespaceDecl *decl =
      arena_alloc(parser->arena, sizeof(SyntaxNamespaceDecl));

  *decl = (SyntaxNamespaceDecl){
      .header = syntax_node_header(SYNTAX_KIND_NAMESPACE_DECL,
                                   span_consumed(span, rem)),
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
    errors = syntax_errorlist_prepend(
        parser->arena, errors,
        syntax_error_create(SYNTAX_EXPECTED_NAME_PATH, rem));
  } else {
    errors = syntax_errorlist_concat(parser->arena, np_res.errors, errors);
    rem = np_res.rem;

    adv = skip_trivia(parser->source, rem);

    if (!(span_len(adv) > 0 &&
          source_byte_at(parser->source, adv.start) == ';')) {
      errors = syntax_errorlist_prepend(
          parser->arena, errors,
          syntax_error_create(SYNTAX_EXPECTED_SEMICOLON, rem));
    } else {
      rem = span_advance(adv, 1);
    }
  }

  SyntaxUsingDecl *decl = arena_alloc(parser->arena, sizeof(SyntaxUsingDecl));

  *decl = (SyntaxUsingDecl){
      .header =
          syntax_node_header(SYNTAX_KIND_USING_DECL, span_consumed(span, rem)),
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

ParserResult parse_expr(const Parser *parser, Span span) {
  // TODO
  (void)parser;
  (void)span;
  return parser_result_not_match(span);
}

ParserResult parse_number_lit_expr(const Parser *parser, Span span);

ParserResult parse_rune_lit_expr(const Parser *parser, Span span);

ParserResult parse_string_lit_expr(const Parser *parser, Span span);

ParserResult parse_struct_lit_expr(const Parser *parser, Span span);

ParserResult parse_array_lit_expr(const Parser *parser, Span span);

ParserResult parse_named_expr(const Parser *parser, Span span);

ParserResult parse_sub_expr(const Parser *parser, Span span);

ParserResult parse_operand_expr(const Parser *parser, Span span);