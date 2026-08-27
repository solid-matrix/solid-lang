#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "parse_internal.h"
#include "parser_result.h"
#include "source.h"
#include "span.h"
#include "strview.h"

bool is_letter_or_underscore(uint8_t c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_'; }

bool is_letter_digit_or_underscore(uint8_t c) { return is_letter_or_underscore(c) || (c >= '0' && c <= '9'); }

bool is_decimal_digit(uint8_t c) { return c >= '0' && c <= '9'; }

bool is_binary_digit(uint8_t c) { return c == '0' || c == '1'; }

bool is_octal_digit(uint8_t c) { return c >= '0' && c <= '7'; }

bool is_hex_digit(uint8_t c) { return is_decimal_digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }

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

bool is_whitespace(uint8_t c) { return c == ' ' || c == '\t' || c == '\v' || c == '\f' || c == '\r' || c == '\n'; }

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
      while (i < span.end && source_byte_at(source, i) != '\n' && source_byte_at(source, i) != '\r')
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

ParserResult complete_longest_match(ParserResult *results, size_t count) {
  assert(count > 0);

  size_t selected = 0;

  for (size_t i = 1; i < count; i++) {
    if (results[i].rem.start > results[selected].rem.start) {
      selected = i;
    }
  }

  return results[selected];
}

ParserMatchResult match_keyword(const Source *source, Span span, Strview keyword) {
  if (keyword.len == 0 || keyword.len > span_len(span)) {
    return (ParserMatchResult){.matched = false, .rem = span};
  }

  Strview token = source_strview_at(source, span_slice(span, 0, keyword.len));
  if (!strview_equals(keyword, token))
    return (ParserMatchResult){.matched = false, .rem = span};

  Span rem = span_advance(span, keyword.len);
  if (!span_is_empty(rem) && is_letter_digit_or_underscore(source_byte_at(source, rem.start)))
    return (ParserMatchResult){.matched = false, .rem = span};

  return (ParserMatchResult){.matched = true, .rem = rem};
}

ParserMatchResult match(const Source *source, Span span, Strview strview) {
  if (strview.len == 0 || strview.len > span_len(span))
    return (ParserMatchResult){.matched = false, .rem = span};

  Strview token = source_strview_at(source, span_slice(span, 0, strview.len));
  if (!strview_equals(strview, token))
    return (ParserMatchResult){.matched = false, .rem = span};

  return (ParserMatchResult){.matched = true, .rem = span_advance(span, strview.len)};
}

ParserListResult parse_expr_list(const Parser *parser, Span span, Strview separator);

ParserListResult parse_identifier_list(const Parser *parser, Span span, Strview separator);