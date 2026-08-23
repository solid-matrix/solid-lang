#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "parse_shared.h"
#include "parser_result.h"
#include "source.h"
#include "span.h"
#include "syntax_error.h"
#include "syntax_node.h"

#define COUNT_OF(a) (sizeof(a) / sizeof((a)[0]))

static const char *const INT_SUFFIXES[] = {
    "isize", "usize", "i128", "u128", "i64", "u64", "i32",
    "u32",   "i16",   "u16",  "i8",   "u8",  "i",   "u"};

static const char *const FLOAT_SUFFIXES[] = {"f32", "f64", "f", "d"};

ParserResult parse_identifier(const Parser *parser, Span span) {
  if (span_is_empty(span))
    return parser_result_not_match(span);

  uint8_t c = source_first_byte_at(parser->source, span);
  if (!is_letter_or_underscore(c))
    return parser_result_not_match(span);

  Span rem = span_advance(span, 1);
  while (span_len(rem) > 0) {
    c = source_first_byte_at(parser->source, rem);
    if (!is_letter_digit_or_underscore(c))
      break;

    rem = span_advance(rem, 1);
  }

  SyntaxIdentifier *id = xmalloc(sizeof(SyntaxIdentifier));
  *id = (SyntaxIdentifier){
      .header =
          syntax_node_header(SYNTAX_KIND_IDENTIFIER, span_consumed(span, rem)),
      .strview = source_strview_at(parser->source, span_consumed(span, rem)),
  };

  return parser_result_matched(rem, (SyntaxNode *)id, NULL);
}

/**
 * @brief Scans digit { ["_"] digit } for @p base starting at @p start.
 *
 * An underscore is only consumed together with a base digit that follows
 * it, so trailing underscores end the run and consecutive underscores
 * stop it after the first pair.
 *
 * @param parser The parser providing the source text.
 * @param span The enclosing range to scan within.
 * @param start Where to begin scanning.
 * @param base Digit base: 2, 8, 10, or 16.
 * @param end Receives the position just past the run.
 * @return The number of digits consumed.
 */
static size_t scan_digits(const Parser *parser, Span span, size_t start,
                          int base, size_t *end) {
  Span rem = span_slice(span, start - span.start, span_len(span));
  size_t digits = 0;

  while (span_len(rem) > 0) {
    uint8_t c = source_first_byte_at(parser->source, rem);
    if (is_base_digit(c, base)) {
      digits++;
    } else if (
        c == '_' && span_len(rem) > 1 &&
        is_base_digit(source_first_byte_at(parser->source, span_advance(rem, 1)),
                      base)) {
      rem = span_advance(rem, 1); // "_" is consumed with its digit
      digits++;
    } else {
      break;
    }

    rem = span_advance(rem, 1);
  }

  *end = rem.start;
  return digits;
}

/**
 * @brief Scans decimal_lit = "0" | ( "1" … "9" ) [ "_" ] decimal_digits.
 *
 * Returns the position just past the body; @p start must sit on a
 * decimal digit.
 */
static size_t scan_decimal_lit(const Parser *parser, Span span, size_t start) {
  if (source_byte_at(parser->source, start) == '0')
    return start + 1;

  size_t end;
  scan_digits(parser, span, start + 1, 10, &end);
  return end;
}

/**
 * @brief Matches [ "_" ] suffix, one of @p candidates, at @p pos.
 *
 * Candidates are ordered longest first so that a longer entry always
 * wins over its own prefixes ("isize" over "i", "f32" over "f").
 *
 * @param parser The parser providing the source text.
 * @param span The enclosing range to match within.
 * @param pos Position to match at.
 * @param candidates Literal suffix strings, longest first.
 * @param count Number of entries in @p candidates.
 * @return The position just past the matched candidate, or 0.
 */
static size_t try_suffix(const Parser *parser, Span span, size_t pos,
                         const char *const *candidates, size_t count) {
  if (pos < span.end && source_byte_at(parser->source, pos) == '_')
    pos++; // one optional separator before the suffix

  size_t rel = pos - span.start;
  for (size_t k = 0; k < count; k++) {
    size_t len = strlen(candidates[k]);
    if (rel + len > span_len(span))
      continue;

    Strview part =
        source_strview_at(parser->source, span_slice(span, rel, rel + len));
    Strview cand = strview_create((const uint8_t *)candidates[k], len);
    if (strview_equals(part, cand))
      return pos + len;
  }
  return 0;
}

/* [ "_" ] int_lit_suffix ; returns the end position or 0. */
static size_t try_int_suffix(const Parser *parser, Span span, size_t pos) {
  return try_suffix(parser, span, pos, INT_SUFFIXES, COUNT_OF(INT_SUFFIXES));
}

/* [ "_" ] float_lit_suffix ; returns the end position or 0. */
static size_t try_float_suffix(const Parser *parser, Span span, size_t pos) {
  return try_suffix(parser, span, pos, FLOAT_SUFFIXES,
                    COUNT_OF(FLOAT_SUFFIXES));
}

/*
 * float_exponent = ( "e" | "E" ) [ "+" | "-" ] [ "_" ] decimal_lit .
 * Returns the end position, or 0 when there is no exponent here. A lone
 * underscore after the sign fails the whole exponent: decimal_lit is
 * mandatory and may not start with "_".
 */
static size_t try_exponent(const Parser *parser, Span span, size_t pos) {
  if (pos >= span.end)
    return 0;

  uint8_t c = source_byte_at(parser->source, pos);
  if (c != 'e' && c != 'E')
    return 0;
  pos++;

  if (pos < span.end) {
    c = source_byte_at(parser->source, pos);
    if (c == '+' || c == '-')
      pos++;
  }

  if (pos < span.end && source_byte_at(parser->source, pos) == '_')
    pos++;

  if (pos >= span.end)
    return 0;

  c = source_byte_at(parser->source, pos);
  if (c == '0')
    return pos + 1;
  if (c >= '1' && c <= '9') {
    size_t end;
    scan_digits(parser, span, pos + 1, 10, &end);
    return end;
  }
  return 0;
}

/**
 * @brief Builds the node for a literal covering [span.start, number_end).
 */
static SyntaxNode *make_number_lit_node(const Parser *parser, Span span,
                                        size_t number_end, SyntaxKind kind) {
  SyntaxNumberLitExpr *lit = xmalloc(sizeof(SyntaxNumberLitExpr));
  Span lit_span = {.start = span.start, .end = number_end};
  *lit = (SyntaxNumberLitExpr){
      .header = {.kind = kind, .span = lit_span},
      .value = source_strview_at(parser->source, lit_span),
  };
  return (SyntaxNode *)lit;
}

/**
 * @brief decimal_lit plus an optional int_lit_suffix:
 *
 *   decimal_lit = "0" | ( "1" … "9" ) [ "_" ] decimal_digits .
 */
static ParserResult parse_decimal_int_lit_expr(const Parser *parser,
                                               Span span) {
  if (span_is_empty(span))
    return parser_result_not_match(span);

  if (!is_decimal_digit(source_first_byte_at(parser->source, span)))
    return parser_result_not_match(span);

  size_t body_end = scan_decimal_lit(parser, span, span.start);

  size_t suf = try_int_suffix(parser, span, body_end);
  size_t number_end = suf ? suf : body_end;

  return parser_result_matched(
      (Span){.start = number_end, .end = span.end},
      make_number_lit_node(parser, span, number_end, SYNTAX_KIND_INT_LIT_EXPR),
      NULL);
}

/**
 * @brief Shared body of binary_lit, octal_lit, and hex_lit:
 *
 *   "0" ( lo | hi ) [ "_" ] { base digits } [ [ "_" ] int_lit_suffix ]
 *
 * A prefix whose optional separator is not followed by a base digit is
 * malformed: SYNTAX_MALFORMED_NUMBER is reported over the whole
 * token-looking run, which is consumed so the caller's longest-match
 * selection prefers this error path over the bare decimal reading.
 *
 * @param parser The parser performing the scan.
 * @param span Position to test; leading trivia must already be skipped.
 * @param lo Lowercase radix marker ("b", "o", or "x").
 * @param hi Uppercase radix marker.
 * @param base Digit base: 2, 8, or 16.
 * @return Standard ParserResult contract (see parser_result.h).
 */
static ParserResult parse_base_prefixed_int_lit(const Parser *parser, Span span,
                                                uint8_t lo, uint8_t hi,
                                                int base) {
  if (span_len(span) < 2)
    return parser_result_not_match(span);

  if (source_byte_at(parser->source, span.start) != '0')
    return parser_result_not_match(span);

  uint8_t marker = source_first_byte_at(parser->source, span_advance(span, 1));
  if (marker != lo && marker != hi)
    return parser_result_not_match(span);

  // One optional separator is allowed before the first digit.
  Span body = span_advance(span, 2);
  if (span_len(body) > 0 &&
      source_first_byte_at(parser->source, body) == '_')
    body = span_advance(body, 1);

  if (span_len(body) == 0 ||
      !is_base_digit(source_first_byte_at(parser->source, body), base)) {
    // Malformed prefix: recover over the maximal identifier/number-
    // looking run, so a discarded alternative can never leak into the
    // result and the error beats the shorter decimal reading.
    Span rem = span;
    while (span_len(rem) > 0) {
      uint8_t c = source_first_byte_at(parser->source, rem);
      if (!is_letter_digit_or_underscore(c) && c != '.')
        break;

      rem = span_advance(rem, 1);
    }
    Span bad = span_consumed(span, rem);

    SyntaxErrorList *errors = syntax_errorlist_create();
    syntax_errorlist_append(errors,
                            syntax_error_create(SYNTAX_MALFORMED_NUMBER, bad));
    return parser_result_matched((Span){.start = bad.end, .end = span.end},
                                 NULL, errors);
  }

  size_t body_end;
  scan_digits(parser, span, body.start, base, &body_end);

  size_t suf = try_int_suffix(parser, span, body_end);
  size_t number_end = suf ? suf : body_end;

  return parser_result_matched(
      (Span){.start = number_end, .end = span.end},
      make_number_lit_node(parser, span, number_end, SYNTAX_KIND_INT_LIT_EXPR),
      NULL);
}

/* binary_lit / octal_lit / hex_lit bind the shared scanner to one
   radix each. */

static ParserResult parse_binary_int_lit_expr(const Parser *parser, Span span) {
  return parse_base_prefixed_int_lit(parser, span, 'b', 'B', 2);
}

static ParserResult parse_octal_int_lit_expr(const Parser *parser, Span span) {
  return parse_base_prefixed_int_lit(parser, span, 'o', 'O', 8);
}

static ParserResult parse_hex_int_lit_expr(const Parser *parser, Span span) {
  return parse_base_prefixed_int_lit(parser, span, 'x', 'X', 16);
}

/**
 * @brief float_lit first alternative:
 *
 *   decimal_lit [ "." decimal_digits ] float_exponent
 *   [ [ "_" ] float_lit_suffix ]
 *
 * The bracketed dot group is all-or-nothing: without digits the dot is
 * not consumed and the exponent must sit directly at the body end, so
 * "1.e5" fails here and splits via the dot branch instead.
 */
static ParserResult parse_exponent_float_lit_expr(const Parser *parser,
                                                  Span span) {
  if (span_is_empty(span))
    return parser_result_not_match(span);

  if (!is_decimal_digit(source_first_byte_at(parser->source, span)))
    return parser_result_not_match(span);

  size_t pos = scan_decimal_lit(parser, span, span.start);

  if (pos < span.end && source_byte_at(parser->source, pos) == '.') {
    size_t after;
    if (scan_digits(parser, span, pos + 1, 10, &after) > 0)
      pos = after;
  }

  size_t exp = try_exponent(parser, span, pos);
  if (exp == 0)
    return parser_result_not_match(span);

  size_t suf = try_float_suffix(parser, span, exp);
  size_t number_end = suf ? suf : exp;

  return parser_result_matched((Span){.start = number_end, .end = span.end},
                               make_number_lit_node(parser, span, number_end,
                                                    SYNTAX_KIND_FLOAT_LIT_EXPR),
                               NULL);
}

/**
 * @brief float_lit second alternative:
 *
 *   decimal_lit "." [ decimal_digits ] [ [ "_" ] float_lit_suffix ]
 *
 * No exponent is reachable on this branch, so "1.5e5" belongs to the
 * exponent branch while "1." alone is already a complete literal.
 */
static ParserResult parse_dot_float_lit_expr(const Parser *parser, Span span) {
  if (span_is_empty(span))
    return parser_result_not_match(span);

  if (!is_decimal_digit(source_first_byte_at(parser->source, span)))
    return parser_result_not_match(span);

  size_t pos = scan_decimal_lit(parser, span, span.start);

  if (pos >= span.end || source_byte_at(parser->source, pos) != '.')
    return parser_result_not_match(span);

  size_t after;
  scan_digits(parser, span, pos + 1, 10, &after);

  size_t suf = try_float_suffix(parser, span, after);
  size_t number_end = suf ? suf : after;

  return parser_result_matched((Span){.start = number_end, .end = span.end},
                               make_number_lit_node(parser, span, number_end,
                                                    SYNTAX_KIND_FLOAT_LIT_EXPR),
                               NULL);
}

/**
 * @brief float_lit third alternative:
 *
 *   decimal_lit [ "_" ] float_lit_suffix .
 */
static ParserResult parse_suffix_float_lit_expr(const Parser *parser,
                                                Span span) {
  if (span_is_empty(span))
    return parser_result_not_match(span);

  if (!is_decimal_digit(source_first_byte_at(parser->source, span)))
    return parser_result_not_match(span);

  size_t body_end = scan_decimal_lit(parser, span, span.start);

  size_t suf = try_float_suffix(parser, span, body_end);
  if (suf == 0)
    return parser_result_not_match(span);

  return parser_result_matched(
      (Span){.start = suf, .end = span.end},
      make_number_lit_node(parser, span, suf, SYNTAX_KIND_FLOAT_LIT_EXPR),
      NULL);
}

static ParserResult parse_int_lit_expr(const Parser *parser, Span span) {
  ParserResult results[] = {
      parse_decimal_int_lit_expr(parser, span),
      parse_binary_int_lit_expr(parser, span),
      parse_octal_int_lit_expr(parser, span),
      parse_hex_int_lit_expr(parser, span),
  };

  return complete_longest_match(results, COUNT_OF(results));
}

static ParserResult parse_float_lit_expr(const Parser *parser, Span span) {

  ParserResult results[] = {
      parse_exponent_float_lit_expr(parser, span),
      parse_dot_float_lit_expr(parser, span),
      parse_suffix_float_lit_expr(parser, span),
  };

  return complete_longest_match(results, COUNT_OF(results));
}

ParserResult parse_number_lit_expr(const Parser *parser, Span span) {
  if (span_is_empty(span))
    return parser_result_not_match(span);

  if (!is_decimal_digit(source_first_byte_at(parser->source, span)))
    return parser_result_not_match(span);

  ParserResult results[] = {
      parse_float_lit_expr(parser, span),
      parse_int_lit_expr(parser, span),
  };

  return complete_longest_match(results, COUNT_OF(results));
}
