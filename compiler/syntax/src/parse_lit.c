/**
 * @file parse_lit.c
 * @brief Literal parsers and their scanners.
 * @author solid-matrix
 * @version 0.0.5
 */

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include "parse_aux.h"
#include "source.h"
#include "span.h"
#include "strview.h"
#include "syntax_error.h"
#include "syntax_errorlist.h"
#include "syntax_nodes.h"
#include "syntax_parses.h"
#include "syntax_result.h"

SyntaxMatchResult match_escape(const SyntaxParser *parser, Span span) {
  if (span_is_empty(span) || source_byte_at(parser->source, span.start) != '\\')
    return (SyntaxMatchResult){.matched = false, .rem = span, .errors = NULL};

  if (span_len(span) < 2) { // "\\" at the end: the escape character is missing
    Span consumed = span_slice(span, 0, 1);
    SyntaxErrorList *errors =
        syntax_errorlist_append(parser->arena, NULL, syntax_error_create(SYNTAX_INVALID_ESCAPE, consumed));
    return (SyntaxMatchResult){
        .matched = true, .rem = (Span){.start = consumed.end, .end = span.end}, .errors = errors};
  }

  switch (source_byte_at(parser->source, span.start + 1)) {
  case '\'':
  case '"':
  case 'n':
  case 'r':
  case 't':
  case '\\':
  case '0':
    return (SyntaxMatchResult){.matched = true, .rem = span_advance(span, 2), .errors = NULL};

  case 'x': {
    // "\\x" octal_digit hex_digit -- two digits, value <= 0x7F.
    size_t pos = span.start + 2;
    size_t digits = 0;
    while (digits < 2 && pos < span.end && is_base_digit(source_byte_at(parser->source, pos), 16)) {
      digits++;
      pos++;
    }

    Span rem = (Span){.start = pos, .end = span.end};
    Span consumed = span_slice(span, 0, pos);

    if (digits < 2) {
      SyntaxErrorList *errors =
          syntax_errorlist_append(parser->arena, NULL, syntax_error_create(SYNTAX_EXPECTED_HEX_DIGIT, consumed));
      return (SyntaxMatchResult){.matched = true, .rem = rem, .errors = errors};
    }

    uint32_t value = hex_value(source_byte_at(parser->source, span.start + 2)) * 16 +
                     hex_value(source_byte_at(parser->source, span.start + 3));
    if (value > 0x7F) {
      SyntaxErrorList *errors =
          syntax_errorlist_append(parser->arena, NULL, syntax_error_create(SYNTAX_ESCAPE_OUT_OF_RANGE, consumed));
      return (SyntaxMatchResult){.matched = true, .rem = rem, .errors = errors};
    }

    return (SyntaxMatchResult){.matched = true, .rem = rem, .errors = NULL};
  }

  case 'u': {
    // "\\u{" hex { { "_" } hex } "}" -- the value must be a Unicode scalar
    // value; underscore runs may sit between hex digits. The first error
    // freezes validation; scanning continues to the closing brace so the
    // whole escape is consumed.
    if (span_len(span) < 3 || source_byte_at(parser->source, span.start + 2) != '{') {
      Span consumed = span_slice(span, 0, 2);
      SyntaxErrorList *errors =
          syntax_errorlist_append(parser->arena, NULL, syntax_error_create(SYNTAX_INVALID_ESCAPE, consumed));
      return (SyntaxMatchResult){
          .matched = true, .rem = (Span){.start = consumed.end, .end = span.end}, .errors = errors};
    }

    size_t pos = span.start + 3;
    SyntaxErrorCode code = SYNTAX_OK;
    uint32_t value = 0;
    bool any_hex = false;

    while (true) {
      if (pos >= span.end) {
        if (code == SYNTAX_OK)
          code = SYNTAX_EXPECTED_BRACE;
        break;
      }

      uint8_t d = source_byte_at(parser->source, pos);
      if (d == '}') {
        pos++;
        if (code == SYNTAX_OK) {
          if (!any_hex)
            code = SYNTAX_EXPECTED_HEX_DIGIT; // empty braces
          else if (value >= 0xD800 && value <= 0xDFFF)
            code = SYNTAX_ESCAPE_OUT_OF_RANGE; // surrogate
        }
        break;
      }

      if (code == SYNTAX_OK && is_base_digit(d, 16)) {
        value = value * 16 + hex_value(d);
        if (value > 0x10FFFF)
          code = SYNTAX_ESCAPE_OUT_OF_RANGE; // the value can only grow
        any_hex = true;
        pos++;
        continue;
      }

      // An underscore run is grammatical only between hex digits: the
      // braces must open on a hex digit, and the run must resume with one.
      if (code == SYNTAX_OK && d == '_' && any_hex) {
        size_t run = pos;
        while (run < span.end && source_byte_at(parser->source, run) == '_')
          run++;
        if (run < span.end && is_base_digit(source_byte_at(parser->source, run), 16)) {
          pos = run;
          continue;
        }
        code = SYNTAX_EXPECTED_HEX_DIGIT;
        pos++;
        continue;
      }

      if (code == SYNTAX_OK)
        code = SYNTAX_EXPECTED_HEX_DIGIT;

      pos++;
    }

    Span rem = (Span){.start = pos, .end = span.end};
    if (code == SYNTAX_OK)
      return (SyntaxMatchResult){.matched = true, .rem = rem, .errors = NULL};

    SyntaxErrorList *errors =
        syntax_errorlist_append(parser->arena, NULL, syntax_error_create(code, span_slice(span, 0, pos)));
    return (SyntaxMatchResult){.matched = true, .rem = rem, .errors = errors};
  }

  default: {
    Span consumed = span_slice(span, 0, 2);
    SyntaxErrorList *errors =
        syntax_errorlist_append(parser->arena, NULL, syntax_error_create(SYNTAX_INVALID_ESCAPE, consumed));
    return (SyntaxMatchResult){
        .matched = true, .rem = (Span){.start = consumed.end, .end = span.end}, .errors = errors};
  }
  }
}

SyntaxMatchResult match_utf8_char(const SyntaxParser *parser, Span span) {
  if (span_is_empty(span))
    return (SyntaxMatchResult){.matched = false, .rem = span, .errors = NULL};

  static const uint32_t MIN_VALUE[] = {0x80, 0x800, 0x10000};

  uint8_t lead = source_byte_at(parser->source, span.start);
  size_t len;
  uint32_t value;
  bool valid = true;

  if (lead < 0x80) { // U+0000..U+007F: single byte
    len = 1;
    value = lead;
  } else if (lead >= 0xC2 && lead <= 0xDF) { // U+0080..U+07FF
    len = 2;
    value = lead & 0x1F;
  } else if (lead >= 0xE0 && lead <= 0xEF) { // U+0800..U+FFFF
    len = 3;
    value = lead & 0x0F;
  } else if (lead >= 0xF0 && lead <= 0xF4) { // U+10000..U+10FFFF
    len = 4;
    value = lead & 0x07;
  } else { // stray continuation byte, 0xC0/0xC1, or 0xF5..0xFF
    len = 1;
    valid = false;
  }

  for (size_t k = 1; valid && k < len; k++) {
    if (span.start + k >= span.end) { // truncated sequence
      valid = false;
      break;
    }

    uint8_t cont = source_byte_at(parser->source, span.start + k);
    if ((cont & 0xC0) != 0x80) { // not a continuation byte
      valid = false;
      break;
    }

    value = (value << 6) | (cont & 0x3F);
  }

  if (valid && len > 1 && (value < MIN_VALUE[len - 2] || (value >= 0xD800 && value <= 0xDFFF) || value > 0x10FFFF)) {
    valid = false; // overlong encoding, surrogate, or outside the scalar range
  }

  if (valid)
    return (SyntaxMatchResult){.matched = true, .rem = span_advance(span, len), .errors = NULL};

  Span consumed = span_slice(span, 0, 1);
  SyntaxErrorList *errors =
      syntax_errorlist_append(parser->arena, NULL, syntax_error_create(SYNTAX_INVALID_CHARACTER, consumed));
  return (SyntaxMatchResult){.matched = true, .rem = (Span){.start = consumed.end, .end = span.end}, .errors = errors};
}

/**
 * @brief Parses one struct literal field `name = expr`.
 * @param parser Parsing context.
 * @param span Where the construct starts.
 * @return Parse outcome; see SyntaxNodeResult.
 */
static SyntaxNodeResult parse_struct_lit_field(const SyntaxParser *parser, Span span) {
  SyntaxNodeResult id_res = parse_identifier(parser, span);
  if (!id_res.matched)
    return syntax_node_result_not_match(span);

  Span rem = id_res.rem;
  SyntaxErrorList *errors = id_res.errors;
  SyntaxNode *value = NULL;

  SyntaxMatchResult mres = match(parser, skip_trivia(parser->source, rem), PUNCTUATION_EQUALS);
  if (!mres.matched) {
    errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_EQUALS, rem));
  } else {
    rem = mres.rem;
  }

  SyntaxNodeResult value_res = parse_expr(parser, skip_trivia(parser->source, rem));
  if (!value_res.matched) {
    errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_EXPR, rem));
  } else {
    rem = value_res.rem;
    value = value_res.node;
    errors = syntax_errorlist_concat(parser->arena, value_res.errors, errors);
  }

  SyntaxStructLitField *field = arena_alloc(parser->arena, sizeof(SyntaxStructLitField));
  field->header = syntax_node_create(SYNTAX_KIND_STRUCT_LIT_FIELD, span_consumed(span, rem));
  field->id = (SyntaxIdentifier *)id_res.node;
  field->value = value;

  return syntax_node_result_matched(rem, (SyntaxNode *)field, errors);
}

/**
 * @brief Skips a run of underscores.
 * @param source The source to read.
 * @param span Where to start skipping.
 * @return The span from the first non-underscore byte to @p span's end.
 */
static Span skip_consecutive_underscores(const Source *source, Span span) {
  Span rem = span;
  while (!span_is_empty(rem) && source_byte_at(source, rem.start) == '_')
    rem = span_advance(rem, 1);

  return rem;
}

/**
 * @brief Matches one integer literal suffix.
 * @param parser Parsing context.
 * @param span Where the construct starts.
 * @return Match outcome; see SyntaxMatchResult.
 */
static SyntaxMatchResult match_int_suffix(const SyntaxParser *parser, Span span) {
  SyntaxMatchResult mres;
  for (size_t i = 0; i < COUNT_OF(INT_SUFFIXES); i++) {
    mres = match(parser, span, INT_SUFFIXES[i]);
    if (mres.matched) {
      return (SyntaxMatchResult){.matched = true, .errors = NULL, .rem = mres.rem};
    }
  }

  return (SyntaxMatchResult){.matched = false, .errors = NULL, .rem = span};
}

/**
 * @brief Matches decimal_digits: digits with underscore runs between them.
 * @param parser Parsing context.
 * @param span Where the construct starts.
 * @return Match outcome; see SyntaxMatchResult.
 */
static SyntaxMatchResult match_decimal_digits(const SyntaxParser *parser, Span span) {
  if (span_is_empty(span))
    return (SyntaxMatchResult){.matched = false, .rem = span, .errors = NULL};

  if (!is_decimal_digit(source_byte_at(parser->source, span.start)))
    return (SyntaxMatchResult){.matched = false, .rem = span, .errors = NULL};

  Span rem = span;

  while (!span_is_empty(rem) && is_decimal_digit(source_byte_at(parser->source, rem.start)))
    rem = span_advance(rem, 1);

  while (!span_is_empty(rem)) {
    Span rem2 = rem;
    while (!span_is_empty(rem2) && source_byte_at(parser->source, rem2.start) == '_')
      rem2 = span_advance(rem2, 1);

    Span rem3 = rem2;
    while (!span_is_empty(rem3) && is_decimal_digit(source_byte_at(parser->source, rem3.start)))
      rem3 = span_advance(rem3, 1);

    if (rem3.start > rem2.start)
      rem = rem3;
    else
      break;
  }

  return (SyntaxMatchResult){.matched = true, .rem = rem, .errors = NULL};
}

/**
 * @brief Matches a decimal literal.
 * @param parser Parsing context.
 * @param span Where the construct starts.
 * @return Match outcome; see SyntaxMatchResult.
 * @note A `0`-led digit run is consumed whole and reports INVALID_LEADING_ZERO.
 */
static SyntaxMatchResult match_decimal_lit(const SyntaxParser *parser, Span span) {
  if (span_is_empty(span))
    return (SyntaxMatchResult){.matched = false, .rem = span, .errors = NULL};

  if (!is_decimal_digit(source_byte_at(parser->source, span.start)))
    return (SyntaxMatchResult){.matched = false, .rem = span, .errors = NULL};

  Span rem = span;
  SyntaxErrorList *errors = syntax_errorlist_empty();

  while (!span_is_empty(rem) && is_decimal_digit(source_byte_at(parser->source, rem.start)))
    rem = span_advance(rem, 1);

  while (!span_is_empty(rem)) {
    Span rem2 = rem;
    while (!span_is_empty(rem2) && source_byte_at(parser->source, rem2.start) == '_')
      rem2 = span_advance(rem2, 1);

    Span rem3 = rem2;
    while (!span_is_empty(rem3) && is_decimal_digit(source_byte_at(parser->source, rem3.start)))
      rem3 = span_advance(rem3, 1);

    if (rem3.start > rem2.start)
      rem = rem3;
    else
      break;
  }

  if (source_byte_at(parser->source, span.start) == '0' && (rem.start - span.start > 1)) {
    SyntaxError error = syntax_error_create(SYNTAX_INVALID_LEADING_ZERO, rem);
    errors = syntax_errorlist_prepend(parser->arena, errors, error);
  }

  return (SyntaxMatchResult){.matched = true, .rem = rem, .errors = errors};
}

/**
 * @brief Parses a binary, octal or hex literal.
 * @param parser Parsing context.
 * @param span Where the construct starts.
 * @param base 2, 8 or 16.
 * @return Parse outcome; see SyntaxNodeResult.
 * @note A missing or out-of-base digit reports EXPECTED_DIGIT anchored at the failure byte, which stays unconsumed.
 */
static SyntaxNodeResult parse_base_prefixed_int_lit_expr(const SyntaxParser *parser, Span span, int base) {
  uint8_t lc, uc;

  if (base == 2) {
    lc = 'b';
    uc = 'B';
  } else if (base == 8) {
    lc = 'o';
    uc = 'O';
  } else if (base == 16) {
    lc = 'x';
    uc = 'X';
  } else {
    assert(false);
  }

  if (span_len(span) < 2)
    return syntax_node_result_not_match(span);

  Span rem = span;
  SyntaxErrorList *errors = syntax_errorlist_empty();

  if (source_byte_at(parser->source, rem.start) != '0')
    return syntax_node_result_not_match(span);
  rem = span_advance(rem, 1);

  uint8_t c = source_byte_at(parser->source, rem.start);
  if (c != lc && c != uc)
    return syntax_node_result_not_match(span);
  rem = span_advance(rem, 1);

  while (!span_is_empty(rem) && source_byte_at(parser->source, rem.start) == '_')
    rem = span_advance(rem, 1);

  if (span_is_empty(rem) || !is_base_digit(source_byte_at(parser->source, rem.start), base)) {
    SyntaxError error = syntax_error_create(SYNTAX_EXPECTED_DIGIT, rem);
    errors = syntax_errorlist_prepend(parser->arena, errors, error);
  }

  while (!span_is_empty(rem)) {
    Span rem2 = rem;
    while (!span_is_empty(rem2) && source_byte_at(parser->source, rem2.start) == '_')
      rem2 = span_advance(rem2, 1);

    Span rem3 = rem2;
    while (!span_is_empty(rem3) && is_base_digit(source_byte_at(parser->source, rem3.start), base))
      rem3 = span_advance(rem3, 1);

    if (rem3.start > rem2.start)
      rem = rem3;
    else
      break;
  }

  Span rem4 = skip_consecutive_underscores(parser->source, rem);

  SyntaxMatchResult mres = match_int_suffix(parser, rem4);
  if (mres.matched) {
    rem = mres.rem;
  } else if (rem4.start > rem.start) { // no suffix but has tailing underscores
    SyntaxError error = syntax_error_create(SYNTAX_EXPECTED_SUFFIX, rem4);
    errors = syntax_errorlist_prepend(parser->arena, errors, error);
    rem = rem4;
  }

  SyntaxIntLitExpr *node = arena_alloc(parser->arena, sizeof(SyntaxIntLitExpr));
  node->header = syntax_node_create(SYNTAX_KIND_INT_LIT_EXPR, span_consumed(span, rem));
  node->value = source_strview_at(parser->source, span_consumed(span, rem));
  return syntax_node_result_matched(rem, (SyntaxNode *)node, errors);
}

/**
 * @brief Parses a decimal literal plus optional suffix.
 * @param parser Parsing context.
 * @param span Where the construct starts.
 * @return Parse outcome; see SyntaxNodeResult.
 */
static SyntaxNodeResult parse_decimal_int_lit_expr(const SyntaxParser *parser, Span span) {
  SyntaxMatchResult mres = match_decimal_lit(parser, span);
  if (!mres.matched)
    return syntax_node_result_not_match(span);

  Span rem = mres.rem;
  SyntaxErrorList *errors = mres.errors;

  Span rem4 = skip_consecutive_underscores(parser->source, rem);

  mres = match_int_suffix(parser, rem4);
  if (mres.matched) {
    rem = mres.rem;
  } else if (rem4.start > rem.start) { // no suffix but has tailing underscores
    SyntaxError error = syntax_error_create(SYNTAX_EXPECTED_SUFFIX, rem4);
    errors = syntax_errorlist_prepend(parser->arena, errors, error);
    rem = rem4;
  }

  SyntaxIntLitExpr *node = arena_alloc(parser->arena, sizeof(SyntaxIntLitExpr));
  node->header = syntax_node_create(SYNTAX_KIND_INT_LIT_EXPR, span_consumed(span, rem));
  node->value = source_strview_at(parser->source, span_consumed(span, rem));
  return syntax_node_result_matched(rem, (SyntaxNode *)node, errors);
}

SyntaxNodeResult parse_int_lit_expr(const SyntaxParser *parser, Span span) {
  SyntaxNodeResult results[] = {
      parse_decimal_int_lit_expr(parser, span),
      parse_base_prefixed_int_lit_expr(parser, span, 2),
      parse_base_prefixed_int_lit_expr(parser, span, 8),
      parse_base_prefixed_int_lit_expr(parser, span, 16),
  };

  return complete_longest_match(results, COUNT_OF(results));
}

/**
 * @brief Matches one float literal suffix.
 * @param parser Parsing context.
 * @param span Where the construct starts.
 * @return Match outcome; see SyntaxMatchResult.
 */
static SyntaxMatchResult match_float_suffix(const SyntaxParser *parser, Span span) {
  SyntaxMatchResult mres;
  for (size_t i = 0; i < COUNT_OF(FLOAT_SUFFIXES); i++) {
    mres = match(parser, span, FLOAT_SUFFIXES[i]);
    if (mres.matched) {
      return (SyntaxMatchResult){.matched = true, .errors = NULL, .rem = mres.rem};
    }
  }

  return (SyntaxMatchResult){.matched = false, .errors = NULL, .rem = span};
}

/**
 * @brief Parses the exponent form:
 *        `decimal_lit ["." decimal_digits] float_exponent ["_" suffix]`.
 * @param parser Parsing context.
 * @param span Where the construct starts.
 * @return Parse outcome; see SyntaxNodeResult.
 */
static SyntaxNodeResult parse_exponent_float_lit_expr(const SyntaxParser *parser, Span span) {
  SyntaxMatchResult mres = match_decimal_lit(parser, span);
  if (!mres.matched)
    return syntax_node_result_not_match(span);

  Span rem = mres.rem;
  SyntaxErrorList *errors = mres.errors;

  mres = match(parser, rem, PUNCTUATION_DOT);

  if (mres.matched) {
    Span rem_dot = mres.rem;

    mres = match_decimal_digits(parser, rem_dot);
    if (mres.matched) {
      rem = mres.rem; // else: the dot group is absent as a whole
    }
  }

  mres = match(parser, rem, STRVIEW("e"));
  if (!mres.matched) {
    mres = match(parser, rem, STRVIEW("E"));
    if (!mres.matched)
      return syntax_node_result_not_match(span);
  }
  rem = mres.rem;

  mres = match(parser, rem, OPERATOR_PLUS);
  if (mres.matched) {
    rem = mres.rem;
  } else {
    mres = match(parser, rem, OPERATOR_MINUS);
    if (mres.matched) {
      rem = mres.rem;
    }
  }

  Span rem3 = skip_consecutive_underscores(parser->source, rem);

  mres = match_decimal_lit(parser, rem3);
  if (mres.matched) {
    rem = mres.rem;
    errors = syntax_errorlist_concat(parser->arena, mres.errors, errors);
  } else {
    SyntaxError error = syntax_error_create(SYNTAX_EXPECTED_DIGIT, rem3);
    errors = syntax_errorlist_prepend(parser->arena, errors, error);
    rem = rem3;
  }

  Span rem4 = skip_consecutive_underscores(parser->source, rem);

  mres = match_float_suffix(parser, rem4);
  if (mres.matched) {
    rem = mres.rem;
  } else if (rem4.start > rem.start) { // no suffix but has tailing underscores
    SyntaxError error = syntax_error_create(SYNTAX_EXPECTED_SUFFIX, rem4);
    errors = syntax_errorlist_prepend(parser->arena, errors, error);
    rem = rem4;
  }

  SyntaxFloatLitExpr *node = arena_alloc(parser->arena, sizeof(SyntaxFloatLitExpr));
  node->header = syntax_node_create(SYNTAX_KIND_FLOAT_LIT_EXPR, span_consumed(span, rem));
  node->value = source_strview_at(parser->source, span_consumed(span, rem));
  return syntax_node_result_matched(rem, (SyntaxNode *)node, errors);
}

/**
 * @brief Parses the dot form: `decimal_lit "." [decimal_digits] ["_" suffix]`.
 * @param parser Parsing context.
 * @param span Where the construct starts.
 * @return Parse outcome; see SyntaxNodeResult.
 */
static SyntaxNodeResult parse_dot_float_lit_expr(const SyntaxParser *parser, Span span) {
  SyntaxMatchResult mres = match_decimal_lit(parser, span);
  if (!mres.matched)
    return syntax_node_result_not_match(span);

  Span rem = mres.rem;
  SyntaxErrorList *errors = mres.errors;

  mres = match(parser, rem, PUNCTUATION_DOT);
  if (!mres.matched)
    return syntax_node_result_not_match(span);
  rem = mres.rem;

  mres = match_decimal_digits(parser, rem);
  if (mres.matched) {
    rem = mres.rem;
  }

  Span rem4 = skip_consecutive_underscores(parser->source, rem);

  mres = match_float_suffix(parser, rem4);
  if (mres.matched) {
    rem = mres.rem;
  } else if (rem4.start > rem.start) { // no suffix but has tailing underscores
    SyntaxError error = syntax_error_create(SYNTAX_EXPECTED_SUFFIX, rem4);
    errors = syntax_errorlist_prepend(parser->arena, errors, error);
    rem = rem4;
  }

  SyntaxFloatLitExpr *node = arena_alloc(parser->arena, sizeof(SyntaxFloatLitExpr));
  node->header = syntax_node_create(SYNTAX_KIND_FLOAT_LIT_EXPR, span_consumed(span, rem));
  node->value = source_strview_at(parser->source, span_consumed(span, rem));
  return syntax_node_result_matched(rem, (SyntaxNode *)node, errors);
}

/**
 * @brief Parses the suffix form: `decimal_lit { "_"} float_lit_suffix`.
 * @param parser Parsing context.
 * @param span Where the construct starts.
 * @return Parse outcome; see SyntaxNodeResult.
 */
static SyntaxNodeResult parse_suffix_float_lit_expr(const SyntaxParser *parser, Span span) {
  SyntaxMatchResult mres = match_decimal_lit(parser, span);
  if (!mres.matched)
    return syntax_node_result_not_match(span);

  Span rem = mres.rem;
  SyntaxErrorList *errors = mres.errors;

  Span rem4 = skip_consecutive_underscores(parser->source, rem);

  mres = match_float_suffix(parser, rem4);
  if (mres.matched) {
    rem = mres.rem;
  } else if (rem4.start > rem.start) { // no suffix but has tailing underscores
    SyntaxError error = syntax_error_create(SYNTAX_EXPECTED_SUFFIX, rem4);
    errors = syntax_errorlist_prepend(parser->arena, errors, error);
    rem = rem4;
  } else {
    return syntax_node_result_not_match(span);
  }

  SyntaxFloatLitExpr *node = arena_alloc(parser->arena, sizeof(SyntaxFloatLitExpr));
  node->header = syntax_node_create(SYNTAX_KIND_FLOAT_LIT_EXPR, span_consumed(span, rem));
  node->value = source_strview_at(parser->source, span_consumed(span, rem));
  return syntax_node_result_matched(rem, (SyntaxNode *)node, errors);
}

SyntaxNodeResult parse_float_lit_expr(const SyntaxParser *parser, Span span) {

  SyntaxNodeResult results[] = {
      parse_exponent_float_lit_expr(parser, span),
      parse_dot_float_lit_expr(parser, span),
      parse_suffix_float_lit_expr(parser, span),
  };

  return complete_longest_match(results, COUNT_OF(results));
}

SyntaxNodeResult parse_rune_lit_expr(const SyntaxParser *parser, Span span) {
  SyntaxMatchResult mres = match(parser, span, PUNCTUATION_SINGLE_QUOTE);
  if (!mres.matched)
    return syntax_node_result_not_match(span);

  Span rem = mres.rem;
  SyntaxErrorList *errors = syntax_errorlist_empty();

  size_t start = rem.start;

  while (true) {
    if (span_is_empty(rem)) {
      SyntaxError error = syntax_error_create(SYNTAX_EXPECTED_CHARACTER, rem);
      errors = syntax_errorlist_prepend(parser->arena, errors, error);
      break;
    }

    uint8_t c = source_byte_at(parser->source, rem.start);
    if (c == '\'') {
      SyntaxError error = syntax_error_create(SYNTAX_EXPECTED_CHARACTER, rem);
      errors = syntax_errorlist_prepend(parser->arena, errors, error);
      break;
    }

    if (c == '\t' || c == '\n' || c == '\r') {
      SyntaxError error = syntax_error_create(SYNTAX_INVALID_CHARACTER, rem);
      errors = syntax_errorlist_prepend(parser->arena, errors, error);
      rem = span_advance(rem, 1); // consume the offending byte
      break;
    }

    mres = match_escape(parser, rem);
    if (mres.matched) {
      rem = mres.rem;
      errors = syntax_errorlist_concat(parser->arena, mres.errors, errors);
      break;
    }

    mres = match_utf8_char(parser, rem);
    if (mres.matched) {
      rem = mres.rem;
      errors = syntax_errorlist_concat(parser->arena, mres.errors, errors);
      break;
    }

    break;
  }
  size_t end = rem.start;

  mres = match(parser, rem, PUNCTUATION_SINGLE_QUOTE);
  if (mres.matched) {
    rem = mres.rem;
  } else {
    SyntaxError error = syntax_error_create(SYNTAX_EXPECTED_SINGLE_QUOTE, rem);
    errors = syntax_errorlist_prepend(parser->arena, errors, error);
  }

  SyntaxRuneLitExpr *rune = arena_alloc(parser->arena, sizeof(SyntaxRuneLitExpr));
  rune->header = syntax_node_create(SYNTAX_KIND_RUNE_LIT_EXPR, span_consumed(span, rem));
  rune->value = source_strview_at(parser->source, span_create(start, end));
  return syntax_node_result_matched(rem, (SyntaxNode *)rune, errors);
}

SyntaxNodeResult parse_string_lit_expr(const SyntaxParser *parser, Span span) {
  SyntaxMatchResult mres = match(parser, span, PUNCTUATION_DOUBLE_QUOTE);
  if (!mres.matched)
    return syntax_node_result_not_match(span);

  Span rem = mres.rem;
  SyntaxErrorList *errors = syntax_errorlist_empty();

  size_t start = rem.start;

  while (true) {
    if (span_is_empty(rem)) {
      SyntaxError error = syntax_error_create(SYNTAX_EXPECTED_DOUBLE_QUOTE, rem);
      errors = syntax_errorlist_prepend(parser->arena, errors, error);
      break;
    }

    uint8_t c = source_byte_at(parser->source, rem.start);
    if (c == '"') {
      break; // the closing quote is consumed after the loop
    }

    if (c == '\t' || c == '\n' || c == '\r') {
      SyntaxError error = syntax_error_create(SYNTAX_INVALID_CHARACTER, rem);
      errors = syntax_errorlist_prepend(parser->arena, errors, error);
      rem = span_advance(rem, 1); // consume the offending byte; strings continue
      continue;
    }

    mres = match_escape(parser, rem);
    if (mres.matched) {
      rem = mres.rem;
      errors = syntax_errorlist_concat(parser->arena, mres.errors, errors);
      continue;
    }

    mres = match_utf8_char(parser, rem);
    rem = mres.rem;
    errors = syntax_errorlist_concat(parser->arena, mres.errors, errors);
  }

  size_t end = rem.start;

  mres = match(parser, rem, PUNCTUATION_DOUBLE_QUOTE);
  if (mres.matched) {
    rem = mres.rem;
  }

  SyntaxStringLitExpr *string = arena_alloc(parser->arena, sizeof(SyntaxStringLitExpr));
  string->header = syntax_node_create(SYNTAX_KIND_STRING_LIT_EXPR, span_consumed(span, rem));
  string->value = source_strview_at(parser->source, span_create(start, end));
  return syntax_node_result_matched(rem, (SyntaxNode *)string, errors);
}

SyntaxNodeResult parse_struct_lit_expr(const SyntaxParser *parser, Span span) {
  SyntaxNodeResult named = parse_named_type(parser, span);
  if (!named.matched)
    return syntax_node_result_not_match(span);

  SyntaxMatchResult mres = match(parser, skip_trivia(parser->source, named.rem), PUNCTUATION_LBRACE);
  if (!mres.matched)
    return syntax_node_result_not_match(span);

  Span rem = mres.rem;
  SyntaxErrorList *errors = named.errors;

  SyntaxListResult flist =
      parse_field_list(parser, skip_trivia(parser->source, rem), parse_struct_lit_field, SYNTAX_EXPECTED_IDENTIFIER);
  rem = flist.rem;
  errors = syntax_errorlist_concat(parser->arena, flist.errors, errors);

  mres = match(parser, skip_trivia(parser->source, rem), PUNCTUATION_RBRACE);
  if (!mres.matched) {
    errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_RBRACE, rem));
  } else {
    rem = mres.rem;
  }

  SyntaxStructLitExpr *expr = arena_alloc(parser->arena, sizeof(SyntaxStructLitExpr));
  expr->header = syntax_node_create(SYNTAX_KIND_STRUCT_LIT_EXPR, span_consumed(span, rem));
  expr->type = (SyntaxNamed *)named.node;
  expr->fields = flist.list;

  return syntax_node_result_matched(rem, (SyntaxNode *)expr, errors);
}

SyntaxNodeResult parse_array_lit_expr(const SyntaxParser *parser, Span span) {
  SyntaxNodeResult type_res = parse_array_type(parser, span);
  if (!type_res.matched)
    return syntax_node_result_not_match(span);

  SyntaxMatchResult mres = match(parser, skip_trivia(parser->source, type_res.rem), PUNCTUATION_LBRACE);
  if (!mres.matched)
    return syntax_node_result_not_match(span);

  Span rem = mres.rem;
  SyntaxErrorList *errors = type_res.errors;

  SyntaxListResult elist = parse_field_list(parser, skip_trivia(parser->source, rem), parse_expr, SYNTAX_EXPECTED_EXPR);
  rem = elist.rem;
  errors = syntax_errorlist_concat(parser->arena, elist.errors, errors);

  mres = match(parser, skip_trivia(parser->source, rem), PUNCTUATION_RBRACE);
  if (!mres.matched) {
    errors = syntax_errorlist_prepend(parser->arena, errors, syntax_error_create(SYNTAX_EXPECTED_RBRACE, rem));
  } else {
    rem = mres.rem;
  }

  SyntaxArrayLitExpr *expr = arena_alloc(parser->arena, sizeof(SyntaxArrayLitExpr));
  expr->header = syntax_node_create(SYNTAX_KIND_ARRAY_LIT_EXPR, span_consumed(span, rem));
  expr->type = type_res.node;
  expr->elements = elist.list;

  return syntax_node_result_matched(rem, (SyntaxNode *)expr, errors);
}
