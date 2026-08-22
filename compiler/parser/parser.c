/**
 * @file parser.c
 * @brief Parser implementation.
 * @author solid-matrix
 * @version 0.0.5
 */

#include <assert.h>
#include <string.h>

#include "mem.h"
#include "parser.h"

#pragma region PARSER

Parser parser_create(Source *source) {
  return (Parser){.source = source, .errors = NULL};
}

void parser_destroy(Parser *parser) {
  SyntaxErrorLinkedList *en = parser->errors;
  while (en != NULL) {
    SyntaxErrorLinkedList *next = en->next;
    xfree(en);
    en = next;
  }
}

void parser_append_error(Parser *parser, Span span, SyntaxErrorCode code) {
  SyntaxErrorLinkedList *en = xmalloc(sizeof(SyntaxErrorLinkedList));
  en->error = (SyntaxError){.code = code, .span = span};
  en->next = parser->errors;
  parser->errors = en;
}

#pragma endregion

#pragma region AUXILIARY

static inline bool is_letter_or_underscore(uint8_t c) {
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_';
}

static inline bool is_letter_digit_or_underscore(uint8_t c) {
  return is_letter_or_underscore(c) || (c >= '0' && c <= '9');
}

static inline bool is_decimal_digit(uint8_t c) { return c >= '0' && c <= '9'; }

static inline bool is_binary_digit(uint8_t c) { return c == '0' || c == '1'; }

static inline bool is_octal_digit(uint8_t c) { return c >= '0' && c <= '7'; }

static inline bool is_hex_digit(uint8_t c) {
  return is_decimal_digit(c) || (c >= 'a' && c <= 'f') ||
         (c >= 'A' && c <= 'F');
}

static inline bool is_base_digit(uint8_t c, int base) {
  switch (base) {
  case 2:
    return is_binary_digit(c);
  case 8:
    return is_octal_digit(c);
  default:
    return is_decimal_digit(c);
  case 16:
    return is_hex_digit(c);
  }
}

static inline bool is_space(uint8_t c) {
  return c == ' ' || c == '\t' || c == '\v' || c == '\f' || c == '\r' ||
         c == '\n';
}

static Span skip_trivia(const Source *source, Span span) {
  size_t i = span.start;

  while (i < span.end) {
    uint8_t c = source_byte_at(source, i);

    if (is_space(c)) {
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

#pragma endregion

#pragma region NUMBER

/* Suffix tables are ordered longest first so that, e.g., "isize" wins
   over "i" and "f32" over "f". */

static const char *const INT_SUFFIXES[] = {
    "isize", "usize", "i128", "u128", "i64", "u64", "i32",
    "u32",   "i16",   "u16",  "i8",   "u8",  "i",   "u"};

static const char *const FLOAT_SUFFIXES[] = {"f32", "f64", "f", "d"};

#define COUNT_OF(a) (sizeof(a) / sizeof((a)[0]))

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

    StringView part =
        source_string_view_at(parser->source, span_slice(span, rel, rel + len));
    StringView cand = sv_create((const uint8_t *)candidates[k], len);
    if (sv_equals(part, cand))
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

/* A1: "." decimal_digits float_exponent [ float_lit_suffix ] . The dot
   binds to a required digit run, so "1.e5" never takes this path. */
static size_t try_float_dot_exp(const Parser *parser, Span span, size_t pos) {
  if (pos >= span.end || source_byte_at(parser->source, pos) != '.')
    return 0;

  size_t end;
  if (scan_digits(parser, span, pos + 1, 10, &end) == 0)
    return 0;

  size_t exp = try_exponent(parser, span, end);
  if (exp == 0)
    return 0;

  size_t suf = try_float_suffix(parser, span, exp);
  return suf ? suf : exp;
}

/* A2: "." [ decimal_digits ] [ float_lit_suffix ] . Covers "1." and
   "1.5_f32"; no exponent is reachable on this branch. */
static size_t try_float_dot(const Parser *parser, Span span, size_t pos) {
  if (pos >= span.end || source_byte_at(parser->source, pos) != '.')
    return 0;

  size_t end;
  scan_digits(parser, span, pos + 1, 10, &end);

  size_t suf = try_float_suffix(parser, span, end);
  return suf ? suf : end;
}

/* A3: [ "_" ] float_lit_suffix . Covers "1f32" and "1_f32". */
static size_t try_float_suffix_only(const Parser *parser, Span span,
                                    size_t pos) {
  if (pos < span.end && source_byte_at(parser->source, pos) == '_')
    return match_float_suffix_at(parser, span, pos + 1);
  return match_float_suffix_at(parser, span, pos);
}

/* A4: float_exponent [ float_lit_suffix ] . Covers "1e5" and
   "1e5_f32". */
static size_t try_float_exp(const Parser *parser, Span span, size_t pos) {
  size_t exp = try_exponent(parser, span, pos);
  if (exp == 0)
    return 0;

  size_t suf = try_float_suffix(parser, span, exp);
  return suf ? suf : exp;
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

#pragma endregion

#pragma region PARSE

SyntaxProgram *parse(Parser *parser) {
  Span span = source_get_span(parser->source);
  span = skip_trivia(parser->source, span);

  ParserResult res = parse_program(parser, span);

  assert(res.matched);
  assert(res.node->kind == SYNTAX_KIND_PROGRAM);

  return (SyntaxProgram *)res.node;
}

ParserResult parse_program(Parser *parser, Span span) {
  SyntaxProgram *program = xmalloc(sizeof(SyntaxProgram));

  *program = (SyntaxProgram){
      .header =
          {
              .kind = SYNTAX_KIND_PROGRAM,
              .span = {.start = span.start},
          },
      .top_levels = syntax_node_list_create(),
  };

  Span rem = span;
  ParserResult res;
  bool with_errors = false;

  while (true) {
    res = parse_decl(parser, rem);
    rem = res.rem;

    if (!res.matched)
      break;

    with_errors |= res.with_errors;

    if (res.node == NULL)
      continue;

    assert((res.node->kind & SYNTAX_KIND_DECL_MASK) != 0);
    syntax_node_list_append(&(program->top_levels), res.node);
  }

  if (program->top_levels.len == 0) {
    program->header.span.end = rem.start;
  } else {
    program->header.span.end =
        program->top_levels.nodes[program->top_levels.len - 1]->span.end;
  }

  rem = skip_trivia(parser->source, rem);

  if (span_len(rem) > 0) {
    parser_append_error(parser, rem, SYNTAX_EXPECTED_EOF);
    with_errors = true;
  }

  return (ParserResult){
      .matched = true,
      .with_errors = with_errors,
      .node = (SyntaxNode *)program,
      .rem = rem,
  };
}

ParserResult parse_identifier(Parser *parser, Span span) {
  if (span_is_empty(span))
    return (ParserResult){
        .matched = false, .with_errors = false, .node = NULL, .rem = span};

  uint8_t c = source_byte_at(parser->source, span.start);

  if (!is_letter_or_underscore(c))
    return (ParserResult){
        .matched = false, .with_errors = false, .node = NULL, .rem = span};

  size_t i = span.start + 1;

  while (i < span.end) {
    c = source_byte_at(parser->source, i);
    if (!is_letter_digit_or_underscore(c))
      break;

    i++;
  }

  Span consumed = {.start = span.start, .end = i};
  SyntaxIdentifier *id = xmalloc(sizeof(SyntaxIdentifier));
  *id = (SyntaxIdentifier){
      .header = {.kind = SYNTAX_KIND_IDENTIFIER, .span = consumed},
      .string_view = source_string_view_at(parser->source, consumed),
  };

  Span rem = skip_trivia(parser->source, (Span){.start = i, .end = span.end});

  return (ParserResult){
      .matched = true,
      .node = (SyntaxNode *)id,
      .rem = rem,
  };
}

ParserResult parse_number_lit_expr(Parser *parser, Span span) {
  if (span_is_empty(span))
    return (ParserResult){
        .matched = false, .with_errors = false, .node = NULL, .rem = span};

  uint8_t first = source_byte_at(parser->source, span.start);
  if (!is_decimal_digit(first))
    return (ParserResult){
        .matched = false, .with_errors = false, .node = NULL, .rem = span};

  // ---- numeric body ----
  size_t body_end = span.start;
  int base = 10;
  bool malformed = false;

  if (first == '0' && span.start + 1 < span.end) {
    uint8_t marker = source_byte_at(parser->source, span.start + 1);
    if (marker == 'b' || marker == 'B')
      base = 2;
    else if (marker == 'o' || marker == 'O')
      base = 8;
    else if (marker == 'x' || marker == 'X')
      base = 16;

    if (base != 10) {
      size_t i = span.start + 2;
      // One optional separator is allowed before the first digit.
      if (i < span.end && source_byte_at(parser->source, i) == '_')
        i++;

      if (i < span.end &&
          is_base_digit(source_byte_at(parser->source, i), base))
        scan_digits(parser, span, i, base, &body_end);
      else
        malformed = true; // "0b", "0x_", "0o8", ...
    }
  }

  if (!malformed && base == 10) {
    if (first == '0') {
      body_end = span.start + 1;
    } else {
      size_t end;
      scan_digits(parser, span, span.start + 1, 10, &end);
      body_end = end;
    }
  }

  if (malformed) {
    Span bad = scan_malformed_number_run(parser->source, span);
    parser_append_error(parser, bad, SYNTAX_MALFORMED_NUMBER);
    Span rem =
        skip_trivia(parser->source, (Span){.start = bad.end, .end = span.end});
    return (ParserResult){.matched = true, .node = NULL, .rem = rem};
  }

  // ---- float continuations (decimal bodies only) ----
  bool is_float = false;
  size_t number_end = body_end;
  if (base == 10) {
    size_t a1 = try_float_dot_exp(parser, span, body_end);
    size_t a2 = try_float_dot(parser, span, body_end);
    size_t a3 = try_float_suffix_only(parser, span, body_end);
    size_t a4 = try_float_exp(parser, span, body_end);

    size_t best = a1;
    if (a2 > best)
      best = a2;
    if (a3 > best)
      best = a3;
    if (a4 > best)
      best = a4;

    if (best > body_end) {
      is_float = true;
      number_end = best;
    }
  }

  if (!is_float) {
    size_t suf = try_int_suffix(parser, span, body_end);
    number_end = suf ? suf : body_end;
  }

  Span lit_span = {.start = span.start, .end = number_end};
  SyntaxNumberLitExpr *lit = xmalloc(sizeof(SyntaxNumberLitExpr));
  *lit = (SyntaxNumberLitExpr){
      .header = {.kind = is_float ? SYNTAX_KIND_FLOAT_LIT_EXPR
                                  : SYNTAX_KIND_INT_LIT_EXPR,
                 .span = lit_span},
      .value = source_string_view_at(parser->source, lit_span),
  };

  Span rem =
      skip_trivia(parser->source, (Span){.start = number_end, .end = span.end});
  return (ParserResult){.matched = true, .node = (SyntaxNode *)lit, .rem = rem};
}

/* Not implemented yet: the parser is being built bottom-up, one
   construct at a time. Each stub reports "no match" so that callers and
   tests link and behave as if the construct were absent. */
ParserResult parse_decl(Parser *parser, Span span) {
  (void)parser;
  (void)span;
  return (ParserResult){
      .matched = false, .with_errors = false, .node = NULL, .rem = span};
}

ParserResult parse_stmt(Parser *parser, Span span) {
  (void)parser;
  (void)span;
  return (ParserResult){
      .matched = false, .with_errors = false, .node = NULL, .rem = span};
}

ParserResult parse_expr(Parser *parser, Span span) {
  (void)parser;
  (void)span;
  return (ParserResult){
      .matched = false, .with_errors = false, .node = NULL, .rem = span};
}

ParserResult parse_type(Parser *parser, Span span) {
  (void)parser;
  (void)span;
  return (ParserResult){
      .matched = false, .with_errors = false, .node = NULL, .rem = span};
}

#pragma endregion