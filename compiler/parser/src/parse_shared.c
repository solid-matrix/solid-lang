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

ParserResult match_keyword(const Parser *parser, Span span, Strview keyword) {
  if (keyword.len == 0 || keyword.len > span_len(span))
    return parser_result_not_match(span);

  ParserResult res = parse_identifier(parser, span);

  if (res.matched == false) {
    syntax_errorlist_destroy(&res.errors);
    return parser_result_not_match(span);
  }

  Span consumed = span_consumed(span, res.rem);

  Strview id = source_strview_at(parser->source, consumed);

  if (!strview_equals(id, keyword)) {
    return parser_result_not_match(span);
  }

  return parser_result_matched(res.rem, NULL, res.errors);

  // Strview part =
  //     source_strview_at(parser->source, span_slice(span, 0, keyword.len));
  // if (!strview_equals(part, keyword))
  //   return parser_result_not_match(span);

  // // Identifier boundary: "in" must not match the front of "input".
  // size_t after = span.start + keyword.len;
  // if (after < span.end &&
  //     is_letter_digit_or_underscore(source_byte_at(parser->source, after)))
  //   return parser_result_not_match(span);

  // return (ParserResult){
  //     .matched = true,
  //     .rem = (Span){.start = after, .end = span.end},
  //     .node = NULL,
  //     .errors = NULL,
  // };
}

ParserResult complete_longest_match(ParserResult *results, size_t count) {
  assert(count > 0);

  size_t selected = 0;

  for (size_t i = 1; i < count; i++) {
    if (results[i].rem.start > results[selected].rem.start) {
      selected = i;
    }
  }

  for (size_t i = 0; i < count; i++) {
    if (i != selected)
      syntax_errorlist_destroy(&results[i].errors);
  }

  return results[selected];
}