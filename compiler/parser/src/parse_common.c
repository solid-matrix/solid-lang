/**
 * @file parse_common.c
 * @brief Implementation of the common productions (see parse_common.h).
 * @author solid-matrix
 * @version 0.0.5
 */

#include <stdbool.h>

#include "parse_common.h"
#include "parse_shared.h"
#include "xmem.h"

bool parse_name_path(const Parser *parser, Span span, SyntaxNodeList *paths,
                     size_t *end) {
  // First segment is mandatory.
  ParserResult seg = parse_identifier(parser, span);
  if (!seg.matched) {
    *end = span.start;
    return false;
  }

  syntax_errorlist_destroy(&seg.errors);
  syntax_node_list_append(paths, seg.node);
  Span rem = seg.rem;

  while (true) {
    Span probe = skip_trivia(parser->source, rem);
    if (!(span_len(probe) >= 2 &&
          source_byte_at(parser->source, probe.start) == ':' &&
          source_byte_at(parser->source, probe.start + 1) == ':')) {
      *end = rem.start; // trivia belongs to the enclosing construct
      return true;
    }

    // Roll back to before "::" when no segment follows it.
    Span after = skip_trivia(
        parser->source, (Span){.start = probe.start + 2, .end = probe.end});
    seg = parse_identifier(parser, after);
    if (!seg.matched) {
      *end = rem.start;
      return true;
    }

    syntax_errorlist_destroy(&seg.errors);
    syntax_node_list_append(paths, seg.node);
    rem = seg.rem;
  }
}
