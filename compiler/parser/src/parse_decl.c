#include "parse_shared.h"

#include <assert.h>

ParserResult parse_program(const Parser *parser, Span span) {

  span = skip_trivia(parser->source, span);

  SyntaxProgram *program = xmalloc(sizeof(SyntaxProgram));

  *program = (SyntaxProgram){
      .header = syntax_node_header(SYNTAX_KIND_PROGRAM, span_empty()),
      .top_levels = syntax_node_list_create(),
  };

  Span rem = span;
  SyntaxErrorList *errors = syntax_errorlist_create();

  while (true) {
    // Layout between top-level declarations is this loop's duty; the
    // last skip also positions the SYNTAX_EXPECTED_EOF check below.
    rem = skip_trivia(parser->source, rem);

    ParserResult res = parse_decl(parser, rem);
    rem = res.rem;

    if (!res.matched)
      break;

    // The nodes move into the accumulator; the emptied handle's
    // lifetime ends here.
    syntax_errorlist_merge(errors, res.errors);
    syntax_errorlist_destroy(res.errors);

    if (res.node == NULL)
      continue;

    assert((res.node->kind & SYNTAX_KIND_DECL_MASK) != 0);
    syntax_node_list_append(&(program->top_levels), res.node);
  }

  program->header.span = span_create(span.start, rem.start);

  rem = skip_trivia(parser->source, rem);

  if (span_len(rem) > 0)
    syntax_errorlist_append(errors,
                            syntax_error_create(SYNTAX_EXPECTED_EOF, rem));

  return parser_result_matched(rem, (SyntaxNode *)program, errors);
}

ParserResult parse_decl(const Parser *parser, Span span) {
  (void)parser;
  (void)span;
  return parser_result_not_match(span);
}