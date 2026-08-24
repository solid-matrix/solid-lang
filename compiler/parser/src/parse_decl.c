#include <assert.h>
#include <stdbool.h>
#include <stddef.h>

#include "parse_common.h"
#include "parse_shared.h"
#include "strview.h"
#include "syntax_error.h"
#include "xmem.h"

#define COUNT_OF(a) (sizeof(a) / sizeof((a)[0]))

ParserResult parse_program(const Parser *parser, Span span) {

  span = skip_trivia(parser->source, span);

  SyntaxProgram *program = xmalloc(sizeof(SyntaxProgram));

  *program = (SyntaxProgram){
      .header = syntax_node_header(SYNTAX_KIND_PROGRAM, span_empty()),
      .top_levels = syntax_node_list_create(),
  };

  Span rem = span;
  SyntaxErrorList *errors = NULL;

  while (true) {
    // Layout between top-level declarations is this loop's duty; the
    // last skip also positions the SYNTAX_EXPECTED_EOF check below.
    rem = skip_trivia(parser->source, rem);

    ParserResult res = parse_decl(parser, rem);
    rem = res.rem;

    if (!res.matched)
      break;

    // The nodes move into the accumulator; the source slot is left
    // empty by the merge itself.
    syntax_errorlist_merge(&errors, &res.errors);

    if (res.node == NULL)
      continue;

    assert((res.node->kind & SYNTAX_KIND_DECL_MASK) != 0);
    syntax_node_list_append(&(program->top_levels), res.node);
  }

  program->header.span = span_create(span.start, rem.start);

  rem = skip_trivia(parser->source, rem);

  if (span_len(rem) > 0)
    syntax_errorlist_append(&errors,
                            syntax_error_create(SYNTAX_EXPECTED_EOF, rem));

  return parser_result_matched(rem, (SyntaxNode *)program, errors);
}

/**
 * @brief Recovery span for a malformed declaration tail: from
 *        @p start up to (but excluding) the next ";" or raw line
 *        terminator; the ";" itself is consumed when found.
 */
static Span scan_decl_tail(const Parser *parser, Span span, size_t start) {
  Span rem = (Span){.start = start, .end = span.end};

  while (span_len(rem) > 0) {
    uint8_t c = source_first_byte_at(parser->source, rem);
    if (c == ';') {
      rem = span_advance(rem, 1);
      break;
    }
    if (c == '\n' || c == '\r')
      break;

    rem = span_advance(rem, 1);
  }

  return (Span){.start = start, .end = rem.start};
}

/**
 * @brief Builds the outcome for a malformed namespace/using tail:
 *        reports @p code over the recovery run started at @p bad_start
 *        and consumes it (matched with node == NULL).
 */
static ParserResult malformed_decl(const Parser *parser, Span span,
                                   size_t bad_start, SyntaxErrorCode code) {
  Span bad = scan_decl_tail(parser, span, bad_start);

  SyntaxErrorList *errors = NULL;
  syntax_errorlist_append(&errors, syntax_error_create(code, bad));

  return parser_result_matched((Span){.start = bad.end, .end = span.end}, NULL,
                               errors);
}

/**
 * @brief Shared body of NamespaceDecl and UsingDecl:
 *
 *   keyword NamePath ";" .
 *
 * @param parser The parser performing the scan.
 * @param span Position to test; leading trivia must already be skipped.
 * @param keyword The anchor keyword ("namespace" or "using").
 * @param kind SYNTAX_KIND_NAMESPACE_DECL or SYNTAX_KIND_USING_DECL.
 * @return Standard ParserResult contract (see parser_result.h).
 */
static ParserResult parse_path_decl(const Parser *parser, Span span,
                                    Strview keyword, SyntaxKind kind) {
  ParserResult kw = match_keyword(parser, span, keyword);
  if (!kw.matched)
    return parser_result_not_match(span);

  Span rem = skip_trivia(parser->source, kw.rem);

  SyntaxNodeList paths = syntax_node_list_create();
  size_t path_end;
  if (!parse_name_path(parser, rem, &paths, &path_end)) {
    syntax_node_list_destroy(&paths);
    return malformed_decl(parser, span, kw.rem.start,
                          SYNTAX_MALFORMED_NAMEPATH);
  }
  rem = (Span){.start = path_end, .end = span.end};

  rem = skip_trivia(parser->source, rem);
  if (!(span_len(rem) > 0 &&
        source_first_byte_at(parser->source, rem) == ';')) {
    syntax_node_list_destroy(&paths);
    return malformed_decl(parser, span, rem.start, SYNTAX_EXPECTED_SEMICOLON);
  }
  rem = span_advance(rem, 1); // consume ";"

  if (kind == SYNTAX_KIND_NAMESPACE_DECL) {
    SyntaxNamespaceDecl *decl = xmalloc(sizeof(SyntaxNamespaceDecl));
    *decl = (SyntaxNamespaceDecl){
        .header = syntax_node_header(kind, span_consumed(span, rem)),
        .paths = paths,
    };
    return parser_result_matched(rem, (SyntaxNode *)decl, NULL);
  }

  SyntaxUsingDecl *decl = xmalloc(sizeof(SyntaxUsingDecl));
  *decl = (SyntaxUsingDecl){
      .header = syntax_node_header(kind, span_consumed(span, rem)),
      .paths = paths,
  };
  return parser_result_matched(rem, (SyntaxNode *)decl, NULL);
}

ParserResult parse_namespace_decl(const Parser *parser, Span span) {
  return parse_path_decl(parser, span, STRVIEW("namespace"),
                         SYNTAX_KIND_NAMESPACE_DECL);
}

ParserResult parse_using_decl(const Parser *parser, Span span) {
  return parse_path_decl(parser, span, STRVIEW("using"),
                         SYNTAX_KIND_USING_DECL);
}

ParserResult parse_decl(const Parser *parser, Span span) {
  ParserResult results[] = {
      parse_namespace_decl(parser, span),
      parse_using_decl(parser, span),
  };

  return complete_longest_match(results, COUNT_OF(results));
}
