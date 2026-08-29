#include <stddef.h>
#include <stdint.h>

#include "source.h"
#include "span.h"
#include "syntax_error.h"
#include "syntax_errorlist.h"
#include "syntax_nodes.h"
#include "syntax_operator.h"
#include "syntax_parses.h"
#include "syntax_result.h"

#define COUNT_OF(a) (sizeof(a) / sizeof((a)[0]))

static const Strview INT_SUFFIXES[] = {
    STRVIEW("isize"), STRVIEW("usize"), STRVIEW("i128"), STRVIEW("u128"), STRVIEW("i64"),
    STRVIEW("u64"),   STRVIEW("i32"),   STRVIEW("u32"),  STRVIEW("i16"),  STRVIEW("u16"),
    STRVIEW("i8"),    STRVIEW("u8"),    STRVIEW("i"),    STRVIEW("u"),
};

static const Strview FLOAT_SUFFIXES[] = {STRVIEW("f32"), STRVIEW("f64"), STRVIEW("f"), STRVIEW("d")};

// digit { ["_"] digit } -- an underscore is consumed only together with
// the base digit that follows it, so trailing underscores end the run
// and consecutive underscores stop it after the first pair.
static size_t scan_digits(const SyntaxParser *parser, Span span, size_t start, int base, size_t *end) {
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

// decimal_lit = "0" | ( "1" .. "9" ) [ "_" ] decimal_digits ; @p start
// must sit on a decimal digit.
static size_t scan_decimal_lit(const SyntaxParser *parser, Span span, size_t start) {
  if (source_byte_at(parser->source, start) == '0')
    return start + 1;

  size_t end;
  scan_digits(parser, span, start + 1, 10, &end);
  return end;
}

// [ "_" ] suffix -- candidates are ordered longest first, so a longer
// entry always wins over its own prefixes ("isize" over "i", "f32" over
// "f"). Returns the position just past the match, or 0.
static size_t try_suffix(const SyntaxParser *parser, Span span, size_t pos, const Strview *candidates, size_t count) {
  if (pos < span.end && source_byte_at(parser->source, pos) == '_')
    pos++; // one optional separator before the suffix

  size_t rel = pos - span.start;
  for (size_t k = 0; k < count; k++) {
    if (rel + candidates[k].len > span_len(span))
      continue;

    Strview part = source_strview_at(parser->source, span_slice(span, rel, rel + candidates[k].len));
    if (strview_equals(part, candidates[k]))
      return pos + candidates[k].len;
  }
  return 0;
}

// float_exponent = ( "e" | "E" ) [ "+" | "-" ] [ "_" ] decimal_lit .
// Returns the end position, or 0 when there is no exponent here. A lone
// underscore after the sign fails the whole exponent: decimal_lit is
// mandatory and may not start with "_".
static size_t try_exponent(const SyntaxParser *parser, Span span, size_t pos) {
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

// The literal node covering [span.start, number_end).
static SyntaxNode *make_number_lit_node(const SyntaxParser *parser, Span span, size_t number_end, SyntaxKind kind) {
  Span lit_span = {.start = span.start, .end = number_end};

  if (kind == SYNTAX_KIND_FLOAT_LIT_EXPR) {
    SyntaxFloatLitExpr *lit = arena_alloc(parser->arena, sizeof(SyntaxFloatLitExpr));
    lit->header = syntax_node_create(kind, lit_span);
    lit->value = source_strview_at(parser->source, lit_span);
    return (SyntaxNode *)lit;
  }

  SyntaxIntLitExpr *lit = arena_alloc(parser->arena, sizeof(SyntaxIntLitExpr));
  lit->header = syntax_node_create(kind, lit_span);
  lit->value = source_strview_at(parser->source, lit_span);
  return (SyntaxNode *)lit;
}

// decimal_lit plus an optional int_lit_suffix:
//
//   decimal_lit = "0" | ( "1" .. "9" ) [ "_" ] decimal_digits .
static SyntaxNodeResult parse_decimal_int_lit_expr(const SyntaxParser *parser, Span span) {
  if (span_is_empty(span))
    return syntax_node_result_not_match(span);

  if (!is_decimal_digit(source_byte_at(parser->source, span.start)))
    return syntax_node_result_not_match(span);

  size_t body_end = scan_decimal_lit(parser, span, span.start);

  size_t suf = try_suffix(parser, span, body_end, INT_SUFFIXES, COUNT_OF(INT_SUFFIXES));
  size_t number_end = suf ? suf : body_end;

  return syntax_node_result_matched((Span){.start = number_end, .end = span.end},
                                    make_number_lit_node(parser, span, number_end, SYNTAX_KIND_INT_LIT_EXPR), NULL);
}

// Shared body of binary_lit, octal_lit, and hex_lit:
//
//   "0" ( lo | hi ) [ "_" ] { base digits } [ [ "_" ] int_lit_suffix ]
//
// A prefix whose optional separator is not followed by a base digit is
// malformed: SYNTAX_MALFORMED_NUMBER is reported over the whole
// token-looking run, which is consumed so the caller's longest-match
// selection prefers this error path over the bare decimal reading.
static SyntaxNodeResult parse_base_prefixed_int_lit(const SyntaxParser *parser, Span span, uint8_t lo, uint8_t hi,
                                                    int base) {
  if (span_len(span) < 2)
    return syntax_node_result_not_match(span);

  if (source_byte_at(parser->source, span.start) != '0')
    return syntax_node_result_not_match(span);

  uint8_t marker = source_byte_at(parser->source, span.start + 1);
  if (marker != lo && marker != hi)
    return syntax_node_result_not_match(span);

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
    return syntax_node_result_matched((Span){.start = bad.end, .end = span.end}, NULL, errors);
  }

  size_t body_end;
  scan_digits(parser, span, body.start, base, &body_end);

  size_t suf = try_suffix(parser, span, body_end, INT_SUFFIXES, COUNT_OF(INT_SUFFIXES));
  size_t number_end = suf ? suf : body_end;

  return syntax_node_result_matched((Span){.start = number_end, .end = span.end},
                                    make_number_lit_node(parser, span, number_end, SYNTAX_KIND_INT_LIT_EXPR), NULL);
}

// float_lit first alternative:
//
//   decimal_lit [ "." decimal_digits ] float_exponent
//   [ [ "_" ] float_lit_suffix ]
//
// The bracketed dot group is all-or-nothing: without digits the dot is
// not consumed and the exponent must sit directly at the body end, so
// "1.e5" fails here and splits via the dot branch instead.
static SyntaxNodeResult parse_exponent_float_lit_expr(const SyntaxParser *parser, Span span) {
  if (span_is_empty(span))
    return syntax_node_result_not_match(span);

  if (!is_decimal_digit(source_byte_at(parser->source, span.start)))
    return syntax_node_result_not_match(span);

  size_t pos = scan_decimal_lit(parser, span, span.start);

  if (pos < span.end && source_byte_at(parser->source, pos) == '.') {
    size_t after;
    if (scan_digits(parser, span, pos + 1, 10, &after) > 0)
      pos = after;
  }

  size_t exp = try_exponent(parser, span, pos);
  if (exp == 0)
    return syntax_node_result_not_match(span);

  size_t suf = try_suffix(parser, span, exp, FLOAT_SUFFIXES, COUNT_OF(FLOAT_SUFFIXES));
  size_t number_end = suf ? suf : exp;

  return syntax_node_result_matched((Span){.start = number_end, .end = span.end},
                                    make_number_lit_node(parser, span, number_end, SYNTAX_KIND_FLOAT_LIT_EXPR), NULL);
}

// float_lit second alternative:
//
//   decimal_lit "." [ decimal_digits ] [ [ "_" ] float_lit_suffix ]
//
// No exponent is reachable on this branch, so "1.5e5" belongs to the
// exponent branch while "1." alone is already a complete literal.
static SyntaxNodeResult parse_dot_float_lit_expr(const SyntaxParser *parser, Span span) {
  if (span_is_empty(span))
    return syntax_node_result_not_match(span);

  if (!is_decimal_digit(source_byte_at(parser->source, span.start)))
    return syntax_node_result_not_match(span);

  size_t pos = scan_decimal_lit(parser, span, span.start);

  if (pos >= span.end || source_byte_at(parser->source, pos) != '.')
    return syntax_node_result_not_match(span);

  size_t after;
  scan_digits(parser, span, pos + 1, 10, &after);

  size_t suf = try_suffix(parser, span, after, FLOAT_SUFFIXES, COUNT_OF(FLOAT_SUFFIXES));
  size_t number_end = suf ? suf : after;

  return syntax_node_result_matched((Span){.start = number_end, .end = span.end},
                                    make_number_lit_node(parser, span, number_end, SYNTAX_KIND_FLOAT_LIT_EXPR), NULL);
}

// float_lit third alternative: decimal_lit [ "_" ] float_lit_suffix .
static SyntaxNodeResult parse_suffix_float_lit_expr(const SyntaxParser *parser, Span span) {
  if (span_is_empty(span))
    return syntax_node_result_not_match(span);

  if (!is_decimal_digit(source_byte_at(parser->source, span.start)))
    return syntax_node_result_not_match(span);

  size_t body_end = scan_decimal_lit(parser, span, span.start);

  size_t suf = try_suffix(parser, span, body_end, FLOAT_SUFFIXES, COUNT_OF(FLOAT_SUFFIXES));
  if (suf == 0)
    return syntax_node_result_not_match(span);

  return syntax_node_result_matched((Span){.start = suf, .end = span.end},
                                    make_number_lit_node(parser, span, suf, SYNTAX_KIND_FLOAT_LIT_EXPR), NULL);
}

SyntaxNodeResult parse_int_lit_expr(const SyntaxParser *parser, Span span) {
  SyntaxNodeResult results[] = {
      parse_decimal_int_lit_expr(parser, span),
      parse_base_prefixed_int_lit(parser, span, 'b', 'B', 2),
      parse_base_prefixed_int_lit(parser, span, 'o', 'O', 8),
      parse_base_prefixed_int_lit(parser, span, 'x', 'X', 16),
  };

  return complete_longest_match(results, COUNT_OF(results));
}

SyntaxNodeResult parse_float_lit_expr(const SyntaxParser *parser, Span span) {

  SyntaxNodeResult results[] = {
      parse_exponent_float_lit_expr(parser, span),
      parse_dot_float_lit_expr(parser, span),
      parse_suffix_float_lit_expr(parser, span),
  };

  return complete_longest_match(results, COUNT_OF(results));
}
