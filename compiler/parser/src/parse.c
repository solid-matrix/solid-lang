#include <assert.h>
#include <stdlib.h>

#include "parse_shared.h"
#include "parser.h"
#include "span.h"

#define COUNT_OF(a) (sizeof(a) / sizeof((a)[0]))

ParserResult parse_identifier(const Parser *parser, Span span) {
  if (span_is_empty(span))
    return parser_result_not_match(span);

  uint8_t c = source_first_byte_at(parser->source, span);
  if (!is_letter_or_underscore(c))
    return parser_result_not_match(span);

  Span rem = span_advance(span, 1);
  while (!span_is_empty(rem)) {
    c = source_first_byte_at(parser->source, rem);
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

ParserResult parse_compile_time(const Parser *parser, Span span) {
  // TODO
  (void)parser;
  (void)span;
  return parser_result_not_match(span);
}

ParserResult parse_program(const Parser *parser, Span span) {
  span = skip_trivia(parser->source, span);

  SyntaxErrorList *errors = NULL;
  SyntaxNodeList *decls = NULL;
  Span rem = span;

  while (!span_is_empty(rem)) {
    rem = skip_trivia(parser->source, rem);

    ParserResult res = parse_decl(parser, rem);
    if (!res.matched)
      break; // not-match returns rem untouched: no progress is correct

    // Every accepted declaration must consume at least one byte,
    // otherwise this loop could spin forever.
    assert(res.rem.start > rem.start);
    rem = res.rem;

    syntax_errorlist_merge(&errors, &res.errors);

    if (res.node != NULL) {
      assert((res.node->kind & SYNTAX_KIND_DECL_MASK) != 0);
      // ownership of the node moves into the program's decl list
      syntax_nodelist_append(parser->arena, &decls, res.node);
    }
  }

  SyntaxProgram *program =
      arena_alloc(parser->arena, sizeof(SyntaxProgram));
  *program = (SyntaxProgram){
      .header =
          syntax_node_header(SYNTAX_KIND_PROGRAM, span_consumed(span, rem)),
      .top_levels = decls,
  };

  rem = skip_trivia(parser->source, rem);
  if (!span_is_empty(rem))
    syntax_errorlist_append(parser->arena, &errors,
                            syntax_error_create(SYNTAX_EXPECTED_EOF, rem));

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

ParserResult parse_namespace_decl(const Parser *parser, Span span);

ParserResult parse_using_decl(const Parser *parser, Span span);

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