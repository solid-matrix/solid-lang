#include <assert.h>
#include <string.h>

#include "parser.h"
#include "xmem.h"

ParserResult parse_identifier(const Parser *parser, Span span) {
  if (span_is_empty(span))
    return parser_result_not_match(span);

  uint8_t c = source_byte_at(parser->source, span.start);
  if (!is_letter_or_underscore(c))
    return parser_result_not_match(span);

  size_t i = span.start + 1;
  while (i < span.end) {
    c = source_byte_at(parser->source, i);
    if (!is_letter_digit_or_underscore(c))
      break;

    i++;
  }

  Span consumed = span_create(span.start, i);
  Span rem = span_create(i, span.end);

  SyntaxIdentifier *id = xmalloc(sizeof(SyntaxIdentifier));
  *id = (SyntaxIdentifier){
      .header = syntax_node_header(SYNTAX_KIND_IDENTIFIER, consumed),
      .strview = source_strview_at(parser->source, consumed),
  };

  return (ParserResult){
      .matched = true,
      .errors = NULL,
      .node = (SyntaxNode *)id,
      .rem = rem,
  };
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
  size_t i = start;
  size_t digits = 0;
  while (i < span.end) {
    if (is_base_digit(source_byte_at(parser->source, i), base)) {
      i++;
      digits++;
    } else if (i + 1 < span.end && source_byte_at(parser->source, i) == '_' &&
               is_base_digit(source_byte_at(parser->source, i + 1), base)) {
      i += 2;
      digits++;
    } else {
      break;
    }
  }
  *end = i;
  return digits;
}

/**
 * @brief Matches one of @p candidates at @p pos.
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
static size_t match_suffix_at(const Parser *parser, Span span, size_t pos,
                              const char *const *candidates, size_t count) {
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

static size_t match_int_suffix_at(const Parser *parser, Span span, size_t pos) {
  return match_suffix_at(parser, span, pos, INT_SUFFIXES,
                         COUNT_OF(INT_SUFFIXES));
}

/* [ "_" ] int_lit_suffix ; returns the end position or 0. */
static size_t try_int_suffix(const Parser *parser, Span span, size_t pos) {
  if (pos < span.end && source_byte_at(parser->source, pos) == '_')
    return match_int_suffix_at(parser, span, pos + 1);
  return match_int_suffix_at(parser, span, pos);
}

static size_t match_float_suffix_at(const Parser *parser, Span span,
                                    size_t pos) {
  return match_suffix_at(parser, span, pos, FLOAT_SUFFIXES,
                         COUNT_OF(FLOAT_SUFFIXES));
}

/* [ "_" ] float_lit_suffix ; returns the end position or 0. */
static size_t try_float_suffix(const Parser *parser, Span span, size_t pos) {
  if (pos < span.end && source_byte_at(parser->source, pos) == '_')
    return match_float_suffix_at(parser, span, pos + 1);
  return match_float_suffix_at(parser, span, pos);
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

/* decimal_lit = "0" | ( "1" �?"9" ) [ "_" ] decimal_digits . Returns
   the position just past the body; @p start must sit on a decimal
   digit. */
static size_t scan_decimal_lit(const Parser *parser, Span span, size_t start) {
  if (source_byte_at(parser->source, start) == '0')
    return start + 1;

  size_t end;
  scan_digits(parser, span, start + 1, 10, &end);
  return end;
}

/* Builds the node for a scanned literal covering
   [span.start, number_end). */
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

/* Winner-takes-errors longest-match selection between two attempts
   from the same span origin. Leaves never strip trivia, so rem.start -
   span.start is exactly the consumed length and the comparison is a
   pure length test. The winner's diagnostics are adopted; the loser's
   branch �?and its diagnostics with it �?is released, so a discarded
   alternative can never leak into the result. */
static void select_longest(ParserResult *best, ParserResult *cand) {
  if (cand->rem.start > best->rem.start) {
    syntax_errorlist_free(&best->errors);
    *best = *cand;
    return;
  }

  syntax_errorlist_free(&cand->errors);
}

/* Consumes the maximal identifier/number-looking run from span.start;
   used as the recovery span for malformed numeric tokens. */
static Span scan_malformed_number_run(const Source *source, Span span) {
  size_t i = span.start;
  while (i < span.end) {
    uint8_t c = source_byte_at(source, i);
    if (!is_letter_digit_or_underscore(c) && c != '.')
      break;
    i++;
  }
  return span_slice(span, 0, i - span.start);
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
 * @return Standard ParserResult contract (see the struct docs).
 */
static ParserResult parse_base_prefixed_int_lit(const Parser *parser, Span span,
                                                uint8_t lo, uint8_t hi,
                                                int base) {
  if (span_len(span) < 2 || source_byte_at(parser->source, span.start) != '0')
    return parser_result_not_match(span);

  uint8_t marker = source_byte_at(parser->source, span.start + 1);
  if (marker != lo && marker != hi)
    return parser_result_not_match(span);

  size_t i = span.start + 2;
  // One optional separator is allowed before the first digit.
  if (i < span.end && source_byte_at(parser->source, i) == '_')
    i++;

  if (i >= span.end ||
      !is_base_digit(source_byte_at(parser->source, i), base)) {
    Span bad = scan_malformed_number_run(parser->source, span);
    ParserResult res = {.matched = true,
                        .node = NULL,
                        .rem = (Span){.start = bad.end, .end = span.end}};
    syntax_errorlist_append(&res.errors,
                            syntax_error_create(SYNTAX_MALFORMED_NUMBER, bad));
    return res;
  }

  size_t body_end;
  scan_digits(parser, span, i, base, &body_end);

  size_t suf = try_int_suffix(parser, span, body_end);
  size_t number_end = suf ? suf : body_end;

  return (ParserResult){.matched = true,
                        .node = make_number_lit_node(parser, span, number_end,
                                                     SYNTAX_KIND_INT_LIT_EXPR),
                        .rem = (Span){.start = number_end, .end = span.end}};
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
 * @brief decimal_lit plus an optional int_lit_suffix:
 *
 *   decimal_lit = "0" | ( "1" �?"9" ) [ "_" ] decimal_digits .
 */
static ParserResult parse_decimal_int_lit_expr(const Parser *parser,
                                               Span span) {
  if (span_is_empty(span) ||
      !is_decimal_digit(source_byte_at(parser->source, span.start)))
    return parser_result_not_match(span);

  size_t body_end = scan_decimal_lit(parser, span, span.start);

  size_t suf = try_int_suffix(parser, span, body_end);
  size_t number_end = suf ? suf : body_end;

  return (ParserResult){.matched = true,
                        .node = make_number_lit_node(parser, span, number_end,
                                                     SYNTAX_KIND_INT_LIT_EXPR),
                        .rem = (Span){.start = number_end, .end = span.end}};
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
  if (span_is_empty(span) ||
      !is_decimal_digit(source_byte_at(parser->source, span.start)))
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

  return (ParserResult){.matched = true,
                        .node =
                            make_number_lit_node(parser, span, number_end,
                                                 SYNTAX_KIND_FLOAT_LIT_EXPR),
                        .rem = (Span){.start = number_end, .end = span.end}};
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
  if (span_is_empty(span) ||
      !is_decimal_digit(source_byte_at(parser->source, span.start)))
    return parser_result_not_match(span);

  size_t pos = scan_decimal_lit(parser, span, span.start);

  if (pos >= span.end || source_byte_at(parser->source, pos) != '.')
    return parser_result_not_match(span);

  size_t after;
  scan_digits(parser, span, pos + 1, 10, &after);

  size_t suf = try_float_suffix(parser, span, after);
  size_t number_end = suf ? suf : after;

  return (ParserResult){.matched = true,
                        .node =
                            make_number_lit_node(parser, span, number_end,
                                                 SYNTAX_KIND_FLOAT_LIT_EXPR),
                        .rem = (Span){.start = number_end, .end = span.end}};
}

/**
 * @brief float_lit third alternative:
 *
 *   decimal_lit [ "_" ] float_lit_suffix .
 */
static ParserResult parse_suffix_float_lit_expr(const Parser *parser,
                                                Span span) {
  if (span_is_empty(span) ||
      !is_decimal_digit(source_byte_at(parser->source, span.start)))
    return parser_result_not_match(span);

  size_t body_end = scan_decimal_lit(parser, span, span.start);

  size_t suf = try_float_suffix(parser, span, body_end);
  if (suf == 0)
    return parser_result_not_match(span);

  return (ParserResult){.matched = true,
                        .node = make_number_lit_node(
                            parser, span, suf, SYNTAX_KIND_FLOAT_LIT_EXPR),
                        .rem = (Span){.start = suf, .end = span.end}};
}

/* int_lit realized as longest-match across the four radix leaves. The
   leaves are mutually exclusive by their second character, so exactly
   one competes with the short decimal reading of a prefixed form and
   always wins it; winner-takes-errors keeps that outcome authoritative
   without relying on the proof. */
static ParserResult parse_int_lit_expr(const Parser *parser, Span span) {
  ParserResult best = parse_decimal_int_lit_expr(parser, span);

  ParserResult r = parse_binary_int_lit_expr(parser, span);
  select_longest(&best, &r);

  r = parse_octal_int_lit_expr(parser, span);
  select_longest(&best, &r);

  r = parse_hex_int_lit_expr(parser, span);
  select_longest(&best, &r);

  return best;
}

/* The three float alternatives may genuinely overlap ("1.5e5" matches
   both a dot prefix and the full exponent branch), so the winner is
   decided purely by consumption length. */
static ParserResult parse_float_lit_expr(const Parser *parser, Span span) {
  ParserResult best = parse_exponent_float_lit_expr(parser, span);

  ParserResult r = parse_dot_float_lit_expr(parser, span);
  select_longest(&best, &r);

  r = parse_suffix_float_lit_expr(parser, span);
  select_longest(&best, &r);

  return best;
}

ParserResult parse_number_lit_expr(const Parser *parser, Span span) {
  if (span_is_empty(span))
    return parser_result_not_match(span);

  uint8_t first = source_byte_at(parser->source, span.start);
  if (!is_decimal_digit(first))
    return parser_result_not_match(span);

  ParserResult flt = parse_float_lit_expr(parser, span);
  ParserResult integ = parse_int_lit_expr(parser, span);

  // Distinct token bodies order strictly under longest-match, so the
  // selection never needs a tie-break; the winner carries its own
  // diagnostics and the loser's are released with it.
  select_longest(&flt, &integ);
  return flt;
}