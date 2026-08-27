#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "parse_internal.h"
#include "parser.h"
#include "parser_result.h"
#include "source.h"
#include "span.h"
#include "syntax_error.h"
#include "syntax_node.h"

#define COUNT_OF(a) (sizeof(a) / sizeof((a)[0]))

static const char *const INT_SUFFIXES[] = {"isize", "usize", "i128", "u128", "i64", "u64", "i32",
                                           "u32",   "i16",   "u16",  "i8",   "u8",  "i",   "u"};

static const char *const FLOAT_SUFFIXES[] = {"f32", "f64", "f", "d"};

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
static size_t scan_digits(const Parser *parser, Span span, size_t start, int base, size_t *end) {
  Span rem = span_slice(span, start - span.start, span_len(span));
  size_t digits = 0;

  while (span_len(rem) > 0) {
    uint8_t c = source_byte_at(parser->source, rem.start);
    if (is_base_digit(c, base)) {
      digits++;
    } else if (c == '_' && span_len(rem) > 1 && is_base_digit(source_byte_at(parser->source, rem.start + 1), base)) {
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
 * @brief Scans decimal_lit = "0" | ( "1" .. "9" ) [ "_" ] decimal_digits.
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
static size_t try_suffix(const Parser *parser, Span span, size_t pos, const char *const *candidates, size_t count) {
  if (pos < span.end && source_byte_at(parser->source, pos) == '_')
    pos++; // one optional separator before the suffix

  size_t rel = pos - span.start;
  for (size_t k = 0; k < count; k++) {
    size_t len = strlen(candidates[k]);
    if (rel + len > span_len(span))
      continue;

    Strview part = source_strview_at(parser->source, span_slice(span, rel, rel + len));
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
  return try_suffix(parser, span, pos, FLOAT_SUFFIXES, COUNT_OF(FLOAT_SUFFIXES));
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
static SyntaxNode *make_number_lit_node(const Parser *parser, Span span, size_t number_end, SyntaxKind kind) {
  SyntaxNumberLitExpr *lit = arena_alloc(parser->arena, sizeof(SyntaxNumberLitExpr));
  Span lit_span = {.start = span.start, .end = number_end};
  lit->header = syntax_node_header(kind, lit_span);
  lit->value = source_strview_at(parser->source, lit_span);
  return (SyntaxNode *)lit;
}

/**
 * @brief decimal_lit plus an optional int_lit_suffix:
 *
 *   decimal_lit = "0" | ( "1" .. "9" ) [ "_" ] decimal_digits .
 */
static ParserResult parse_decimal_int_lit_expr(const Parser *parser, Span span) {
  if (span_is_empty(span))
    return parser_result_not_match(span);

  if (!is_decimal_digit(source_byte_at(parser->source, span.start)))
    return parser_result_not_match(span);

  size_t body_end = scan_decimal_lit(parser, span, span.start);

  size_t suf = try_int_suffix(parser, span, body_end);
  size_t number_end = suf ? suf : body_end;

  return parser_result_matched((Span){.start = number_end, .end = span.end},
                               make_number_lit_node(parser, span, number_end, SYNTAX_KIND_INT_LIT_EXPR), NULL);
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
static ParserResult parse_base_prefixed_int_lit(const Parser *parser, Span span, uint8_t lo, uint8_t hi, int base) {
  if (span_len(span) < 2)
    return parser_result_not_match(span);

  if (source_byte_at(parser->source, span.start) != '0')
    return parser_result_not_match(span);

  uint8_t marker = source_byte_at(parser->source, span.start + 1);
  if (marker != lo && marker != hi)
    return parser_result_not_match(span);

  // One optional separator is allowed before the first digit.
  Span body = span_advance(span, 2);
  if (span_len(body) > 0 && source_byte_at(parser->source, body.start) == '_')
    body = span_advance(body, 1);

  if (span_len(body) == 0 || !is_base_digit(source_byte_at(parser->source, body.start), base)) {
    // Malformed prefix: recover over the maximal identifier/number-
    // looking run, so a discarded alternative can never leak into the
    // result and the error beats the shorter decimal reading.
    Span rem = span;
    while (span_len(rem) > 0) {
      uint8_t c = source_byte_at(parser->source, rem.start);
      if (!is_letter_digit_or_underscore(c) && c != '.')
        break;

      rem = span_advance(rem, 1);
    }
    Span bad = span_consumed(span, rem);

    SyntaxErrorList *errors =
        syntax_errorlist_append(parser->arena, NULL, syntax_error_create(SYNTAX_MALFORMED_NUMBER, bad));
    return parser_result_matched((Span){.start = bad.end, .end = span.end}, NULL, errors);
  }

  size_t body_end;
  scan_digits(parser, span, body.start, base, &body_end);

  size_t suf = try_int_suffix(parser, span, body_end);
  size_t number_end = suf ? suf : body_end;

  return parser_result_matched((Span){.start = number_end, .end = span.end},
                               make_number_lit_node(parser, span, number_end, SYNTAX_KIND_INT_LIT_EXPR), NULL);
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
static ParserResult parse_exponent_float_lit_expr(const Parser *parser, Span span) {
  if (span_is_empty(span))
    return parser_result_not_match(span);

  if (!is_decimal_digit(source_byte_at(parser->source, span.start)))
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
                               make_number_lit_node(parser, span, number_end, SYNTAX_KIND_FLOAT_LIT_EXPR), NULL);
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

  if (!is_decimal_digit(source_byte_at(parser->source, span.start)))
    return parser_result_not_match(span);

  size_t pos = scan_decimal_lit(parser, span, span.start);

  if (pos >= span.end || source_byte_at(parser->source, pos) != '.')
    return parser_result_not_match(span);

  size_t after;
  scan_digits(parser, span, pos + 1, 10, &after);

  size_t suf = try_float_suffix(parser, span, after);
  size_t number_end = suf ? suf : after;

  return parser_result_matched((Span){.start = number_end, .end = span.end},
                               make_number_lit_node(parser, span, number_end, SYNTAX_KIND_FLOAT_LIT_EXPR), NULL);
}

/**
 * @brief float_lit third alternative:
 *
 *   decimal_lit [ "_" ] float_lit_suffix .
 */
static ParserResult parse_suffix_float_lit_expr(const Parser *parser, Span span) {
  if (span_is_empty(span))
    return parser_result_not_match(span);

  if (!is_decimal_digit(source_byte_at(parser->source, span.start)))
    return parser_result_not_match(span);

  size_t body_end = scan_decimal_lit(parser, span, span.start);

  size_t suf = try_float_suffix(parser, span, body_end);
  if (suf == 0)
    return parser_result_not_match(span);

  return parser_result_matched((Span){.start = suf, .end = span.end},
                               make_number_lit_node(parser, span, suf, SYNTAX_KIND_FLOAT_LIT_EXPR), NULL);
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

  if (!is_decimal_digit(source_byte_at(parser->source, span.start)))
    return parser_result_not_match(span);

  ParserResult results[] = {
      parse_float_lit_expr(parser, span),
      parse_int_lit_expr(parser, span),
  };

  return complete_longest_match(results, COUNT_OF(results));
}

/**
 * @brief Scans one UTF-8 encoded code point at @p pos.
 *
 * Well-formedness is validated strictly: every continuation byte must
 * be a continuation byte, the decoded value must not be an overlong
 * encoding, a surrogate, or above U+10FFFF.
 *
 * @param parser The parser providing the source text.
 * @param span The enclosing range to scan within.
 * @param pos Position to scan at.
 * @param end Receives the position just past the code point.
 * @return True when a valid code point was scanned.
 */
static bool scan_utf8_char(const Parser *parser, Span span, size_t pos, size_t *end) {
  if (pos >= span.end)
    return false;

  static const uint32_t MIN_VALUE[] = {0x80, 0x800, 0x10000};

  uint8_t lead = source_byte_at(parser->source, pos);
  size_t len;
  uint32_t value;

  if (lead < 0x80) { // U+0000..U+007F: single byte
    *end = pos + 1;
    return true;
  } else if (lead >= 0xC2 && lead <= 0xDF) { // U+0080..U+07FF
    len = 2;
    value = lead & 0x1F;
  } else if (lead >= 0xE0 && lead <= 0xEF) { // U+0800..U+FFFF
    len = 3;
    value = lead & 0x0F;
  } else if (lead >= 0xF0 && lead <= 0xF4) { // U+10000..U+10FFFF
    len = 4;
    value = lead & 0x07;
  } else {
    return false; // stray continuation byte, 0xC0/0xC1, or 0xF5..0xFF
  }

  if (pos + len > span.end)
    return false; // truncated sequence

  for (size_t k = 1; k < len; k++) {
    uint8_t cont = source_byte_at(parser->source, pos + k);
    if ((cont & 0xC0) != 0x80)
      return false;

    value = (value << 6) | (cont & 0x3F);
  }

  if (value < MIN_VALUE[len - 2])
    return false; // overlong encoding
  if (value >= 0xD800 && value <= 0xDFFF)
    return false; // surrogate
  if (value > 0x10FFFF)
    return false; // outside the Unicode scalar range

  *end = pos + len;
  return true;
}

/**
 * @brief Scans escape = quote_escape | ascii_escape | unicode_escape
 *        at @p pos.
 *
 *   quote_escape   = "\'" | "\"" .
 *   ascii_escape   = "\n" | "\r" | "\t" | "\\" | "\0"
 *                  | "\x" octal_digit hex_digit .
 *   unicode_escape = "\u{" hex_digit { ["_"] hex_digit } "}" .
 *
 * In \u{ ... } an underscore may only appear between two hex digits, and
 * the enclosed value must be a Unicode scalar value.
 *
 * @param parser The parser providing the source text.
 * @param span The enclosing range to scan within.
 * @param pos Position to scan at.
 * @param end Receives the position just past the escape.
 * @return True when an escape was scanned.
 */
static bool try_escape(const Parser *parser, Span span, size_t pos, size_t *end) {
  if (pos + 1 >= span.end || source_byte_at(parser->source, pos) != '\\')
    return false;

  uint8_t c = source_byte_at(parser->source, pos + 1);
  switch (c) {
  case '\'':
  case '"':
  case 'n':
  case 'r':
  case 't':
  case '\\':
  case '0':
    *end = pos + 2;
    return true;
  case 'x': {
    if (pos + 3 >= span.end) // two digit bytes must follow "\x"
      return false;

    uint8_t hi = source_byte_at(parser->source, pos + 2);
    uint8_t lo = source_byte_at(parser->source, pos + 3);
    if (!is_base_digit(hi, 8) || !is_base_digit(lo, 16))
      return false;

    *end = pos + 4;
    return true;
  }
  case 'u': {
    size_t i = pos + 2;
    if (i >= span.end || source_byte_at(parser->source, i) != '{')
      return false;
    i++;

    if (i >= span.end || !is_base_digit(source_byte_at(parser->source, i), 16))
      return false; // empty braces or a leading underscore

    uint32_t value = 0;
    while (i < span.end) {
      uint8_t d = source_byte_at(parser->source, i);
      if (d == '}') {
        *end = i + 1;
        return value <= 0x10FFFF && !(value >= 0xD800 && value <= 0xDFFF);
      }

      if (is_base_digit(d, 16)) {
        value = value * 16 + (uint32_t)(is_decimal_digit(d) ? d - '0' : (d | 0x20) - 'a' + 10);
        if (value > 0x10FFFF)
          return false; // the value can only grow; avoid wraparound
        i++;
        continue;
      }

      if (d == '_') { // allowed only between two hex digits
        if (i + 1 >= span.end || !is_base_digit(source_byte_at(parser->source, i + 1), 16))
          return false;
        i++;
        continue;
      }

      return false; // anything else breaks the escape
    }
    return false; // missing closing brace
  }
  default:
    return false;
  }
}

/**
 * @brief Recovery span for a malformed quoted literal: from the opening
 *        quote up to (but excluding) the matching quote or the first
 *        raw line terminator, whichever comes first. Escapes are not
 *        honored here: the run only bounds the diagnostic.
 */
static Span scan_malformed_lit_run(const Parser *parser, Span span, uint8_t quote) {
  Span rem = span_advance(span, 1); // skip the opening quote

  while (span_len(rem) > 0) {
    uint8_t c = source_byte_at(parser->source, rem.start);
    if (c == quote || c == '\n' || c == '\r')
      break;

    rem = span_advance(rem, 1);
  }

  return span_consumed(span, rem);
}

/**
 * @brief Builds the outcome for a malformed quoted literal: reports
 *        @p code over the recovery run started by @p quote and consumes
 *        it, so longest-match selection treats the error path like any
 *        other alternative (matched with node == NULL).
 */
static ParserResult malformed_quoted_lit(const Parser *parser, Span span, SyntaxErrorCode code, uint8_t quote) {
  Span bad = scan_malformed_lit_run(parser, span, quote);

  SyntaxErrorList *errors = syntax_errorlist_append(parser->arena, NULL, syntax_error_create(code, bad));

  return parser_result_matched((Span){.start = bad.end, .end = span.end}, NULL, errors);
}

ParserResult parse_rune_lit_expr(const Parser *parser, Span span) {
  if (span_is_empty(span))
    return parser_result_not_match(span);

  if (source_byte_at(parser->source, span.start) != '\'')
    return parser_result_not_match(span);

  Span rem = span_advance(span, 1); // consume the opening quote
  if (span_len(rem) == 0 || source_byte_at(parser->source, rem.start) == '\'')
    return malformed_quoted_lit(parser, span, SYNTAX_MALFORMED_RUNE, '\'');

  uint8_t c = source_byte_at(parser->source, rem.start);
  size_t content_end;
  bool scanned;
  if (c == '\\') {
    scanned = try_escape(parser, span, rem.start, &content_end);
  } else if (c == '\t' || c == '\n' || c == '\r') {
    scanned = false; // raw tab/line feed/carriage return are excluded
  } else {
    scanned = scan_utf8_char(parser, span, rem.start, &content_end);
  }

  if (!scanned)
    return malformed_quoted_lit(parser, span, SYNTAX_MALFORMED_RUNE, '\'');

  rem = (Span){.start = content_end, .end = rem.end};
  if (span_len(rem) == 0 || source_byte_at(parser->source, rem.start) != '\'')
    return malformed_quoted_lit(parser, span, SYNTAX_MALFORMED_RUNE, '\'');
  rem = span_advance(rem, 1); // consume the closing quote

  SyntaxRuneLitExpr *rune = arena_alloc(parser->arena, sizeof(SyntaxRuneLitExpr));
  rune->header = syntax_node_header(SYNTAX_KIND_RUNE_LIT_EXPR, span_consumed(span, rem));
  rune->value = source_strview_at(parser->source, span_consumed(span, rem));

  return parser_result_matched(rem, (SyntaxNode *)rune, NULL);
}

ParserResult parse_string_lit_expr(const Parser *parser, Span span) {
  if (span_is_empty(span))
    return parser_result_not_match(span);

  if (source_byte_at(parser->source, span.start) != '"')
    return parser_result_not_match(span);

  Span rem = span_advance(span, 1); // consume the opening quote
  while (span_len(rem) > 0) {
    uint8_t c = source_byte_at(parser->source, rem.start);

    if (c == '"') {
      rem = span_advance(rem, 1); // consume the closing quote

      SyntaxStringLitExpr *string = arena_alloc(parser->arena, sizeof(SyntaxStringLitExpr));
      string->header = syntax_node_header(SYNTAX_KIND_STRING_LIT_EXPR, span_consumed(span, rem));
      string->value = source_strview_at(parser->source, span_consumed(span, rem));
      return parser_result_matched(rem, (SyntaxNode *)string, NULL);
    }

    size_t elem_end;
    bool scanned;
    if (c == '\\')
      scanned = try_escape(parser, span, rem.start, &elem_end);
    else if (c == '\t' || c == '\n' || c == '\r')
      scanned = false; // raw controls terminate: strings are single-line
    else
      scanned = scan_utf8_char(parser, span, rem.start, &elem_end);

    if (!scanned)
      return malformed_quoted_lit(parser, span, SYNTAX_MALFORMED_STRING, '"');

    rem = (Span){.start = elem_end, .end = rem.end};
  }

  return malformed_quoted_lit(parser, span, SYNTAX_MALFORMED_STRING, '"');
}
