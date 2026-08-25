#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "parse_shared.h"
#include "parser.h"
#include "parser_result.h"
#include "source.h"
#include "span.h"
#include "strview.h"
#include "syntax_error.h"

bool is_letter_or_underscore(uint8_t c) {
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_';
}

bool is_letter_digit_or_underscore(uint8_t c) {
  return is_letter_or_underscore(c) || (c >= '0' && c <= '9');
}

bool is_decimal_digit(uint8_t c) { return c >= '0' && c <= '9'; }

bool is_binary_digit(uint8_t c) { return c == '0' || c == '1'; }

bool is_octal_digit(uint8_t c) { return c >= '0' && c <= '7'; }

bool is_hex_digit(uint8_t c) {
  return is_decimal_digit(c) || (c >= 'a' && c <= 'f') ||
         (c >= 'A' && c <= 'F');
}

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

bool is_whitespace(uint8_t c) {
  return c == ' ' || c == '\t' || c == '\v' || c == '\f' || c == '\r' ||
         c == '\n';
}

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
      while (i < span.end && source_byte_at(source, i) != '\n' &&
             source_byte_at(source, i) != '\r')
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

bool match_keyword(const Source *source, Span span, Strview keyword) {
  if (keyword.len == 0 || keyword.len > span_len(span))
    return false;

  Strview token = source_strview_at(source, span_slice(span, 0, keyword.len));

  if (!strview_equals(keyword, token)) {
    return false;
  }

  Span rem = span_advance(span, keyword.len);

  if (!span_is_empty(rem) &&
      is_letter_digit_or_underscore(source_first_byte_at(source, rem))) {
    return false;
  }

  return true;
}

ParserResult complete_longest_match(ParserResult *results, size_t count) {
  assert(count > 0);

  size_t selected = 0;

  for (size_t i = 1; i < count; i++) {
    if (results[i].rem.start > results[selected].rem.start) {
      selected = i;
    }
  }

  // Losing alternatives are abandoned on purpose: their nodes and
  // diagnostics live in the parse arena and die with it.
  return results[selected];
}

ParserResult parse_name_path(const Parser *parser, Span span) {
  SyntaxNodeList *segments = NULL;

  // Best-prefix semantics: a "::" not followed by an identifier ends
  // the path BEFORE the separator (trivia rolled back with it), so the
  // enclosing construct sees an unconsumed "::". confirmed tracks the
  // position after the last accepted segment for that rollback.
  Span rem = span;
  Span confirmed = span;
  while (true) {
    ParserResult seg = parse_identifier(parser, rem);
    if (!seg.matched)
      break;

    syntax_nodelist_append(parser->arena, &segments, seg.node);
    confirmed = seg.rem;
    rem = seg.rem;

    Span probe = skip_trivia(parser->source, rem);
    if (!(span_len(probe) >= 2 &&
          source_byte_at(parser->source, probe.start) == ':' &&
          source_byte_at(parser->source, probe.start + 1) == ':'))
      break; // trivia stays with the enclosing construct

    // Tentatively step over "::"; a failing segment below rolls back
    // to the pre-separator position via confirmed.
    rem = skip_trivia(
        parser->source,
        (Span){.start = probe.start + 2, .end = probe.end});
  }

  if (confirmed.start == span.start) { // not even one segment
    return parser_result_not_match(span); // the empty list dies with arena
  }
  rem = confirmed;

  SyntaxNamePath *path = arena_alloc(parser->arena, sizeof(SyntaxNamePath));
  *path = (SyntaxNamePath){
      .header =
          syntax_node_header(SYNTAX_KIND_NAME_PATH, span_consumed(span, rem)),
      .segments = segments,
  };

  return parser_result_matched(rem, (SyntaxNode *)path, NULL);
}