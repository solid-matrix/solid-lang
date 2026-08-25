#include <assert.h>
#include <stdbool.h>
#include <stddef.h>

#include "parse_shared.h"
#include "parser.h"
#include "parser_result.h"
#include "span.h"
#include "strview.h"
#include "syntax_error.h"
#include "syntax_node.h"
#include "xmem.h"

#define COUNT_OF(a) (sizeof(a) / sizeof((a)[0]))

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
  syntax_errorlist_append(parser->arena, &errors,
                          syntax_error_create(code, bad));

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
  if (!match_keyword(parser->source, span, keyword))
    return parser_result_not_match(span);

  Span after_kw = span_advance(span, keyword.len);
  Span rem = skip_trivia(parser->source, after_kw);

  ParserResult path_res = parse_name_path(parser, rem);
  if (!path_res.matched)
    return malformed_decl(parser, span, after_kw.start,
                          SYNTAX_MALFORMED_NAMEPATH);

  SyntaxNamePath *path = (SyntaxNamePath *)path_res.node;
  rem = skip_trivia(parser->source, path_res.rem);

  if (!(span_len(rem) > 0 &&
        source_first_byte_at(parser->source, rem) == ';')) {
    // The abandoned path stays reachable until arena_destroy: nothing
    // to release by hand.
    return malformed_decl(parser, span, rem.start, SYNTAX_EXPECTED_SEMICOLON);
  }
  rem = span_advance(rem, 1); // consume ";"

  if (kind == SYNTAX_KIND_NAMESPACE_DECL) {
    SyntaxNamespaceDecl *decl =
        arena_alloc(parser->arena, sizeof(SyntaxNamespaceDecl));
    *decl = (SyntaxNamespaceDecl){
        .header = syntax_node_header(kind, span_consumed(span, rem)),
        .path = path,
    };
    return parser_result_matched(rem, (SyntaxNode *)decl, NULL);
  }

  SyntaxUsingDecl *decl =
      arena_alloc(parser->arena, sizeof(SyntaxUsingDecl));
  *decl = (SyntaxUsingDecl){
      .header = syntax_node_header(kind, span_consumed(span, rem)),
      .path = path,
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
