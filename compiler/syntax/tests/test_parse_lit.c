#define TEST_SUPPORT_NO_DEFAULT_FIXTURES

#include <string.h>

#include "node.h"
#include "parse.h"
#include "parser_fixture.h"
#include "syntax_node.h"
#include "test_support.h"

void setUp(void) {}
void tearDown(void) { fx_release(); }

// The number entry point: int and float branches, longest match wins.
typedef SyntaxNodeResult (*ParseFn)(const char *);

static SyntaxNodeResult parse_number_now(void) {
  Span span = source_get_span(fx_source);
  SyntaxNodeResult results[] = {
      parse_int_lit_expr(fx_parser, span),
      parse_float_lit_expr(fx_parser, span),
  };
  return complete_longest_match(results, sizeof(results) / sizeof(results[0]));
}

static SyntaxNodeResult parse_number(const char *t) {
  fx_begin(t);
  return parse_number_now();
}

static SyntaxNodeResult parse_rune(const char *t) {
  fx_begin(t);
  return parse_rune_lit_expr(fx_parser, source_get_span(fx_source));
}

static SyntaxNodeResult parse_string(const char *t) {
  fx_begin(t);
  return parse_string_lit_expr(fx_parser, source_get_span(fx_source));
}

// The literal's value view: every literal node is a header + Strview.
static Strview lit_value(const SyntaxNode *node) {
  switch (node->kind) {
  case SYNTAX_KIND_INT_LIT_EXPR:
    return ((const SyntaxIntLitExpr *)node)->value;
  case SYNTAX_KIND_FLOAT_LIT_EXPR:
    return ((const SyntaxFloatLitExpr *)node)->value;
  case SYNTAX_KIND_RUNE_LIT_EXPR:
    return ((const SyntaxRuneLitExpr *)node)->value;
  default:
    return ((const SyntaxStringLitExpr *)node)->value;
  }
}

static void expect_whole(ParseFn fn, const char *text, SyntaxKind kind) {
  SyntaxNodeResult r = fn(text);

  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_NOT_NULL(r.node);
  TEST_ASSERT_EQUAL_HEX32(kind, r.node->kind);
  TEST_ASSERT_STRVIEW_EQ(lit_value(r.node), text);
  TEST_ASSERT_EQUAL_size_t(strlen(text), r.rem.start);
}

// Asserts a recovery frame: matched, partial node of @p kind, exactly one
// diagnostic of @p code, and the parse advanced to @p rem.
static void expect_error_frame(const char *text, SyntaxKind kind, SyntaxErrorCode code, size_t rem) {
  fx_begin(text);
  SyntaxNodeResult r = parse_number_now();
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_NOT_NULL(r.node);
  TEST_ASSERT_EQUAL_HEX32(kind, r.node->kind);
  TEST_ASSERT_EQUAL_size_t(1, error_count(&r));
  TEST_ASSERT_EQUAL_HEX32(code, r.errors->error.code);
  TEST_ASSERT_EQUAL_size_t(rem, r.rem.start);
}

// Asserts that only the first tok_len bytes form the token; the rest
// splits into later tokens.
static void expect_split(ParseFn fn, const char *text, SyntaxKind kind, size_t tok_len) {
  SyntaxNodeResult r = fn(text);

  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_NOT_NULL(r.node);
  TEST_ASSERT_EQUAL_HEX32(kind, r.node->kind);
  TEST_ASSERT_EQUAL_size_t(tok_len, lit_value(r.node).len);
  TEST_ASSERT_EQUAL_size_t(tok_len, r.rem.start);
}

// Asserts a recovery run: matched, no node, exactly one diagnostic.
static void expect_malformed(ParseFn fn, const char *text, SyntaxErrorCode code) {
  SyntaxNodeResult r = fn(text);

  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_NULL(r.node);
  TEST_ASSERT_EQUAL_size_t(1, error_count(&r));
  TEST_ASSERT_NOT_NULL(r.errors);
  TEST_ASSERT_EQUAL_HEX32(code, r.errors->error.code);
}

// Asserts a clean not-match: nothing consumed, nothing recorded.
static void expect_not_match(ParseFn fn, const char *text) {
  size_t len = strlen(text);
  SyntaxNodeResult r = fn(text);

  TEST_ASSERT_FALSE(r.matched);
  TEST_ASSERT_NULL(r.node);
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(0, r.rem.start);
  TEST_ASSERT_EQUAL_size_t(len, r.rem.end);
}

/* ---- int literals -------------------------------------------------- */

void test_int_decimal(void) {
  static const char *const CASES[] = {
      "0",      "0i32", "0_i32",  "1",       "1i32",      "1_i32",  "12",    "12i32",
      "12_i32", "1_2",  "1_2i32", "1_2_i32", "1_234_567", "0isize", "1u128", "1234567_u",
  };
  for (size_t i = 0; i < sizeof(CASES) / sizeof(CASES[0]); i++)
    expect_whole(parse_number, CASES[i], SYNTAX_KIND_INT_LIT_EXPR);
}

void test_int_base_prefixed(void) {
  static const char *const CASES[] = {
      "0b0",   "0b01",    "0b1",     "0b_0",        "0b_0000_1111", "0B_0000_1111_u8", "0b1010_1101",
      "0b1u8", "0o0",     "0o17",    "0o_123",      "0O_123",       "0o7_i16",         "0x0",
      "0xFF",  "0x_FFFF", "0X_FFFF", "0xDeAd_beEf", "0xF_u32",      "0xFFu64",
  };
  for (size_t i = 0; i < sizeof(CASES) / sizeof(CASES[0]); i++)
    expect_whole(parse_number, CASES[i], SYNTAX_KIND_INT_LIT_EXPR);
}

void test_malformed_base_prefixes(void) {
  // Prefix without digits, or a digit outside the base: reported at the
  // failure byte; the recovery frame carries the partial IntLit node.
  static const char *const CASES[] = {
      "0b", "0B", "0x", "0X", "0o", "0O", "0x_", "0b_", "0o8", "0b2", "0o9", "0xG", "0x_g",
  };
  for (size_t i = 0; i < sizeof(CASES) / sizeof(CASES[0]); i++) {
    fx_begin(CASES[i]);
    SyntaxNodeResult r = parse_number_now();
    TEST_ASSERT_TRUE(r.matched); // recovery frame carrying a partial node
    TEST_ASSERT_NOT_NULL(r.node);
    TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_INT_LIT_EXPR, r.node->kind);
    TEST_ASSERT_EQUAL_size_t(1, error_count(&r));
    TEST_ASSERT_EQUAL_HEX32(SYNTAX_EXPECTED_DIGIT, r.errors->error.code);
  }
}

/* ---- float literals ------------------------------------------------ */

void test_float_exponent(void) {
  static const char *const CASES[] = {
      "1e5",  "1e5_f32", "1.5e5", "1.5e5_f32", "1e+5", "1e-5",    "1e_5",   "1E5",
      "1E+5", "1E-5",    "1e+_5", "1e-_5",     "0e0",  "1_000e3", "1e5f64", "1.5e5_f64",
  };
  for (size_t i = 0; i < sizeof(CASES) / sizeof(CASES[0]); i++)
    expect_whole(parse_number, CASES[i], SYNTAX_KIND_FLOAT_LIT_EXPR);
}

void test_float_dot(void) {
  static const char *const CASES[] = {
      "1.", "1.5", "1.5_f32", "1.5f32", "0.5", "0.0", "12.75", "1.f32", "1.5d",
  };
  for (size_t i = 0; i < sizeof(CASES) / sizeof(CASES[0]); i++)
    expect_whole(parse_number, CASES[i], SYNTAX_KIND_FLOAT_LIT_EXPR);
}

/* ---- longest-prefix splits ----------------------------------------- */

void test_splits_int(void) {
  // Leading zeros: consumed as one token with a specific diagnostic.
  expect_error_frame("01", SYNTAX_KIND_INT_LIT_EXPR, SYNTAX_INVALID_LEADING_ZERO, 2);
  expect_error_frame("00", SYNTAX_KIND_INT_LIT_EXPR, SYNTAX_INVALID_LEADING_ZERO, 2);
  // Zero cannot carry separators: same treatment.
  expect_error_frame("0_0", SYNTAX_KIND_INT_LIT_EXPR, SYNTAX_INVALID_LEADING_ZERO, 3);
  // Trailing underscores: consumed by the suffix-separator scan, reported.
  expect_error_frame("1_", SYNTAX_KIND_INT_LIT_EXPR, SYNTAX_EXPECTED_SUFFIX, 2);
  expect_error_frame("0_", SYNTAX_KIND_INT_LIT_EXPR, SYNTAX_EXPECTED_SUFFIX, 2);
  // Consecutive underscores are grammatical under { "_" }: one clean token.
  expect_whole(parse_number, "1__2", SYNTAX_KIND_INT_LIT_EXPR);
  expect_split(parse_number, "12abc", SYNTAX_KIND_INT_LIT_EXPR, 2);
  // "_" binds to a following suffix only, never to an exponent.
  expect_error_frame("1_e5", SYNTAX_KIND_INT_LIT_EXPR, SYNTAX_EXPECTED_SUFFIX, 2);
  // A dead exponent (marker/sign without digits) is consumed and reported.
  expect_error_frame("1e", SYNTAX_KIND_FLOAT_LIT_EXPR, SYNTAX_EXPECTED_DIGIT, 2);
  expect_error_frame("1e+", SYNTAX_KIND_FLOAT_LIT_EXPR, SYNTAX_EXPECTED_DIGIT, 3);
  // Suffix greediness stops at the first non-suffix character.
  expect_split(parse_number, "0i8x", SYNTAX_KIND_INT_LIT_EXPR, 3);
  expect_split(parse_number, "1u128x", SYNTAX_KIND_INT_LIT_EXPR, 5);
}

void test_splits_float(void) {
  expect_split(parse_number, "1.e5", SYNTAX_KIND_FLOAT_LIT_EXPR, 2);
  // Leading zero in the exponent: consumed as one token with a diagnostic.
  expect_error_frame("1e05", SYNTAX_KIND_FLOAT_LIT_EXPR, SYNTAX_INVALID_LEADING_ZERO, 4);
  expect_split(parse_number, "1e5.5", SYNTAX_KIND_FLOAT_LIT_EXPR, 3);
  // Trailing underscores: consumed by the suffix-separator scan, reported.
  expect_error_frame("1e5_", SYNTAX_KIND_FLOAT_LIT_EXPR, SYNTAX_EXPECTED_SUFFIX, 4);
  expect_split(parse_number, "1.e5f32", SYNTAX_KIND_FLOAT_LIT_EXPR, 2);
  // The dot branch stops before a second dot or a dead exponent.
  expect_split(parse_number, "1.5.5", SYNTAX_KIND_FLOAT_LIT_EXPR, 3);
  expect_split(parse_number, "1.fx", SYNTAX_KIND_FLOAT_LIT_EXPR, 3);
  expect_error_frame("1.5e", SYNTAX_KIND_FLOAT_LIT_EXPR, SYNTAX_EXPECTED_DIGIT, 4);
  // One exponent only; a second one splits.
  expect_split(parse_number, "1e5e5", SYNTAX_KIND_FLOAT_LIT_EXPR, 3);
  expect_split(parse_number, "1e5x", SYNTAX_KIND_FLOAT_LIT_EXPR, 3);
  expect_split(parse_number, "0dx", SYNTAX_KIND_FLOAT_LIT_EXPR, 2);
  // Consecutive underscores are grammatical under { "_" }: one clean token.
  expect_whole(parse_number, "1__5", SYNTAX_KIND_INT_LIT_EXPR);
}

/* ---- boundaries ----------------------------------------------------- */

void test_boundaries_number(void) {
  // Not a number start: rejected without consuming.
  expect_not_match(parse_number, ".5");

  // Underscore-separated hex digits keep working with a suffix.
  expect_whole(parse_number, "0xF_F_u8", SYNTAX_KIND_INT_LIT_EXPR);

  // Uppercase F32 is not a suffix; the "_" is consumed and reported.
  expect_error_frame("0_F32", SYNTAX_KIND_INT_LIT_EXPR, SYNTAX_EXPECTED_SUFFIX, 2);

  // A suffix must not swallow identifier characters beyond it.
  expect_split(parse_number, "12isizex", SYNTAX_KIND_INT_LIT_EXPR, 7);

  // Trailing trivia belongs to the enclosing sequence.
  fx_begin("12 // c\nx");
  SyntaxNodeResult r = parse_number_now();
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_NOT_NULL(r.node);
  TEST_ASSERT_EQUAL_size_t(0, r.node->span.start);
  TEST_ASSERT_EQUAL_size_t(2, r.node->span.end);
  TEST_ASSERT_EQUAL_size_t(2, r.rem.start);
}

/* ---- rune literals ------------------------------------------------- */

void test_rune_simple_and_numeric(void) {
  static const struct {
    const char *text;
    const char *value;
  } CASES[] = {
      {"'a'", "a"},
      {"'字'", "字"},
      {"'\\''", "\\'"},
      {"'\\\"'", "\\\""},
      {"'\\\\'", "\\\\"},
      {"'\\n'", "\\n"},
      {"'\\r'", "\\r"},
      {"'\\t'", "\\t"},
      {"'\\0'", "\\0"},
      {"'€'", "€"},
      {"'😀'", "😀"},
      {"'\\x41'", "\\x41"},
      {"'\\x30'", "\\x30"},
      {"'\\x7f'", "\\x7f"},
      {"'\\u{41}'", "\\u{41}"},
      {"'\\u{0}'", "\\u{0}"},
      {"'\\u{1_F600}'", "\\u{1_F600}"},
      {"'\\u{10FFFF}'", "\\u{10FFFF}"},
      {"'\\u{10_FFFF}'", "\\u{10_FFFF}"},
      {"'\\u{1__F600}'", "\\u{1__F600}"},
      {"'\\u{00e9}'", "\\u{00e9}"},
  };

  // The value view covers the character content between the quotes.
  for (size_t i = 0; i < sizeof(CASES) / sizeof(CASES[0]); i++) {
    fx_begin(CASES[i].text);
    SyntaxNodeResult r = parse_rune(CASES[i].text);
    TEST_ASSERT_TRUE(r.matched);
    TEST_ASSERT_NULL(r.errors);
    TEST_ASSERT_NOT_NULL(r.node);
    TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_RUNE_LIT_EXPR, r.node->kind);
    TEST_ASSERT_STRVIEW_EQ(((SyntaxRuneLitExpr *)r.node)->value, CASES[i].value);
    TEST_ASSERT_EQUAL_size_t(strlen(CASES[i].text), r.rem.start);
  }
}

void test_rune_malformed(void) {
  static const struct {
    const char *text;
    SyntaxErrorCode code;
    size_t count;
  } CASES[] = {
      {"''", SYNTAX_EXPECTED_CHARACTER, 1},
      {"'''", SYNTAX_EXPECTED_CHARACTER, 1},
      {"'ab'", SYNTAX_EXPECTED_SINGLE_QUOTE, 1},
      {"'ab", SYNTAX_EXPECTED_SINGLE_QUOTE, 1},
      {"'\\q'", SYNTAX_INVALID_ESCAPE, 1},
      {"'\\x'", SYNTAX_EXPECTED_HEX_DIGIT, 1},
      {"'\\x8'", SYNTAX_EXPECTED_HEX_DIGIT, 1},
      {"'\\x80'", SYNTAX_ESCAPE_OUT_OF_RANGE, 1},
      {"'\\x7G'", SYNTAX_EXPECTED_HEX_DIGIT, 2},
      {"'\\u{}'", SYNTAX_EXPECTED_HEX_DIGIT, 1},
      {"'\\u{_41}'", SYNTAX_EXPECTED_HEX_DIGIT, 1},
      {"'\\u{41_}'", SYNTAX_EXPECTED_HEX_DIGIT, 1},
      {"'\\u{D800}'", SYNTAX_ESCAPE_OUT_OF_RANGE, 1},
      {"'\\u{DFFF}'", SYNTAX_ESCAPE_OUT_OF_RANGE, 1},
      {"'\\u{110000}'", SYNTAX_ESCAPE_OUT_OF_RANGE, 1},
      {"'\\u{FFFFFF}'", SYNTAX_ESCAPE_OUT_OF_RANGE, 1},
      {"'\t'", SYNTAX_INVALID_CHARACTER, 1},
      {"'\n'", SYNTAX_INVALID_CHARACTER, 1},
      {"'\r'", SYNTAX_INVALID_CHARACTER, 1},
      {"'\x80'", SYNTAX_INVALID_CHARACTER, 1},
      {"'\xc0\xaf'", SYNTAX_INVALID_CHARACTER, 2},
  };

  for (size_t i = 0; i < sizeof(CASES) / sizeof(CASES[0]); i++) {
    fx_begin(CASES[i].text);
    SyntaxNodeResult r = parse_rune(CASES[i].text);
    TEST_ASSERT_TRUE(r.matched); // recovery frame carrying a partial node
    TEST_ASSERT_NOT_NULL(r.node);
    TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_RUNE_LIT_EXPR, r.node->kind);
    TEST_ASSERT_EQUAL_size_t(CASES[i].count, error_count(&r));
    TEST_ASSERT_NOT_NULL(r.errors);
    if (r.errors != NULL) {
      if (CASES[i].count == 2) {
        // A failed closing quote is always the newest diagnostic.
        TEST_ASSERT_EQUAL_HEX32(SYNTAX_EXPECTED_SINGLE_QUOTE, r.errors->error.code);
        TEST_ASSERT_EQUAL_HEX32(CASES[i].code, r.errors->next->error.code);
      } else {
        TEST_ASSERT_EQUAL_HEX32(CASES[i].code, r.errors->error.code);
      }
    }
  }
}

void test_rune_not_match(void) {
  expect_not_match(parse_rune, "x");
  expect_not_match(parse_rune, "\"");
  expect_not_match(parse_rune, "");
}

/* ---- string literals ----------------------------------------------- */

void test_string_simple_and_escapes(void) {
  static const struct {
    const char *text;
    const char *value;
  } SIMPLE[] = {
      {"\"\"", ""},
      {"\"hello\"", "hello"},
      {"\"'a'\"", "'a'"},
      {"\"\\\"\"", "\\\""},
      {"\"\\\\\"", "\\\\"},
      {"\"中文😀\"", "中文😀"},
      {"\"tab\\there\"", "tab\\there"},
  };
  static const struct {
    const char *text;
    const char *value;
  } ESCAPES[] = {
      {"\"a\\nb\\tc\\0d\\r\"", "a\\nb\\tc\\0d\\r"},
      {"\"\\x09\"", "\\x09"},
      {"\"\\x7f\"", "\\x7f"},
      {"\"\\u{1F600}\"", "\\u{1F600}"},
      {"\"\\u{10_FFFF}\"", "\\u{10_FFFF}"},
      {"\"\\u{1__F600}\"", "\\u{1__F600}"},
  };

  // The value view covers the content between the quotes.
  for (size_t i = 0; i < sizeof(SIMPLE) / sizeof(SIMPLE[0]); i++) {
    fx_begin(SIMPLE[i].text);
    SyntaxNodeResult r = parse_string(SIMPLE[i].text);
    TEST_ASSERT_TRUE(r.matched);
    TEST_ASSERT_NULL(r.errors);
    TEST_ASSERT_NOT_NULL(r.node);
    TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_STRING_LIT_EXPR, r.node->kind);
    TEST_ASSERT_STRVIEW_EQ(((SyntaxStringLitExpr *)r.node)->value, SIMPLE[i].value);
    TEST_ASSERT_EQUAL_size_t(strlen(SIMPLE[i].text), r.rem.start);
  }
  for (size_t i = 0; i < sizeof(ESCAPES) / sizeof(ESCAPES[0]); i++) {
    fx_begin(ESCAPES[i].text);
    SyntaxNodeResult r = parse_string(ESCAPES[i].text);
    TEST_ASSERT_TRUE(r.matched);
    TEST_ASSERT_NULL(r.errors);
    TEST_ASSERT_STRVIEW_EQ(((SyntaxStringLitExpr *)r.node)->value, ESCAPES[i].value);
    TEST_ASSERT_EQUAL_size_t(strlen(ESCAPES[i].text), r.rem.start);
  }
}

void test_string_malformed(void) {
  static const struct {
    const char *text;
    size_t count;
    SyntaxErrorCode head;
    SyntaxErrorCode next;
  } CASES[] = {
      {"\"abc", 1, SYNTAX_EXPECTED_DOUBLE_QUOTE, SYNTAX_OK},
      {"\"a\nb\"", 1, SYNTAX_INVALID_CHARACTER, SYNTAX_OK},
      {"\"a\tb", 2, SYNTAX_EXPECTED_DOUBLE_QUOTE, SYNTAX_INVALID_CHARACTER},
      {"\"\\q\"", 1, SYNTAX_INVALID_ESCAPE, SYNTAX_OK},
      {"\"\\x\"", 1, SYNTAX_EXPECTED_HEX_DIGIT, SYNTAX_OK},
      {"\"\\x7G\"", 1, SYNTAX_EXPECTED_HEX_DIGIT, SYNTAX_OK},
      {"\"\\x8\"", 1, SYNTAX_EXPECTED_HEX_DIGIT, SYNTAX_OK},
      {"\"\\x80\"", 1, SYNTAX_ESCAPE_OUT_OF_RANGE, SYNTAX_OK},
      {"\"\x80\"", 1, SYNTAX_INVALID_CHARACTER, SYNTAX_OK},
      {"\"\xc0\xaf\"", 2, SYNTAX_INVALID_CHARACTER, SYNTAX_INVALID_CHARACTER},
      {"\"\\u{}\"", 1, SYNTAX_EXPECTED_HEX_DIGIT, SYNTAX_OK},
      {"\"\\u{_41}\"", 1, SYNTAX_EXPECTED_HEX_DIGIT, SYNTAX_OK},
      {"\"\\u{41_}\"", 1, SYNTAX_EXPECTED_HEX_DIGIT, SYNTAX_OK},
      {"\"\\u{D800}\"", 1, SYNTAX_ESCAPE_OUT_OF_RANGE, SYNTAX_OK},
      {"\"\\u{110000}\"", 1, SYNTAX_ESCAPE_OUT_OF_RANGE, SYNTAX_OK},
  };

  for (size_t i = 0; i < sizeof(CASES) / sizeof(CASES[0]); i++) {
    SyntaxNodeResult r = parse_string(CASES[i].text);
    TEST_ASSERT_TRUE(r.matched); // recovery frame carrying a partial node
    TEST_ASSERT_NOT_NULL(r.node);
    TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_STRING_LIT_EXPR, r.node->kind);
    TEST_ASSERT_EQUAL_size_t(CASES[i].count, error_count(&r));
    TEST_ASSERT_NOT_NULL(r.errors);
    if (r.errors != NULL) {
      TEST_ASSERT_EQUAL_HEX32(CASES[i].head, r.errors->error.code);
      if (CASES[i].next != SYNTAX_OK)
        TEST_ASSERT_EQUAL_HEX32(CASES[i].next, r.errors->next->error.code);
    }
  }
}

void test_string_not_match(void) {
  expect_not_match(parse_string, "x");
  expect_not_match(parse_string, "'");
  expect_not_match(parse_string, "");
}

/* ---- struct / array literals ----------------------------------------- */

void test_struct_lit_forms(void) {
  fx_begin("Vector2{}");
  SyntaxNodeResult r = parse_expr(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_STRUCT_LIT_EXPR, r.node->kind);
  TEST_ASSERT_TRUE(syntax_nodelist_is_empty(((const SyntaxStructLitExpr *)r.node)->fields));
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(strlen("Vector2{}"), r.rem.start);

  fx_begin("Vector2{ x = 0_f32, y = 1_f32 }");
  r = parse_expr(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  const SyntaxStructLitExpr *s = (const SyntaxStructLitExpr *)r.node;
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_NAMED, s->type->header.kind);
  TEST_ASSERT_EQUAL_size_t(2, syntax_nodelist_length(s->fields));
  TEST_ASSERT_STRVIEW_EQ(((const SyntaxStructLitField *)s->fields->node)->id->value, "x"); // source order
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_FLOAT_LIT_EXPR, ((const SyntaxStructLitField *)s->fields->node)->value->kind);
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(strlen("Vector2{ x = 0_f32, y = 1_f32 }"), r.rem.start);

  fx_begin("Vector2{ x = 1_f32, }");
  r = parse_expr(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_EQUAL_size_t(1, syntax_nodelist_length(((const SyntaxStructLitExpr *)r.node)->fields));
  TEST_ASSERT_NULL(r.errors);

  fx_begin("Segment{ from = Vector2{ x = 0_f32 } }");
  r = parse_expr(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  s = (const SyntaxStructLitExpr *)r.node;
  TEST_ASSERT_EQUAL_size_t(1, syntax_nodelist_length(s->fields));
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_STRUCT_LIT_EXPR, ((const SyntaxStructLitField *)s->fields->node)->value->kind);
  TEST_ASSERT_NULL(r.errors);
}

void test_struct_lit_not_a_literal(void) {
  fx_begin("Vector2");
  SyntaxNodeResult r = parse_expr(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_NAMED, r.node->kind);
  TEST_ASSERT_NULL(r.errors);
}

void test_struct_lit_field_frame(void) {
  fx_begin("Vector2{ x = }");
  SyntaxNodeResult r = parse_struct_lit_expr(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  const SyntaxStructLitField *f = (const SyntaxStructLitField *)((const SyntaxStructLitExpr *)r.node)->fields->node;
  TEST_ASSERT_STRVIEW_EQ(f->id->value, "x");
  TEST_ASSERT_NULL(f->value);
  TEST_ASSERT_EQUAL_size_t(1, error_chain_length(r.errors));
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_EXPECTED_EXPR, r.errors->error.code);
  TEST_ASSERT_EQUAL_size_t(strlen("Vector2{ x = }"), r.rem.start);
}

void test_array_lit_forms(void) {
  fx_begin("[5]i32{}");
  SyntaxNodeResult r = parse_expr(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_ARRAY_LIT_EXPR, r.node->kind);
  TEST_ASSERT_TRUE(syntax_nodelist_is_empty(((const SyntaxArrayLitExpr *)r.node)->elements));
  TEST_ASSERT_NULL(r.errors);

  fx_begin("[5]i32{ 1, 2, 3, 4, 5 }");
  r = parse_expr(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  const SyntaxArrayLitExpr *a = (const SyntaxArrayLitExpr *)r.node;
  TEST_ASSERT_EQUAL_size_t(5, syntax_nodelist_length(a->elements));
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_INT_LIT_EXPR, a->elements->node->kind);
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(strlen("[5]i32{ 1, 2, 3, 4, 5 }"), r.rem.start);

  fx_begin("[5]i32{ 1, }");
  r = parse_expr(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_EQUAL_size_t(1, syntax_nodelist_length(((const SyntaxArrayLitExpr *)r.node)->elements));
  TEST_ASSERT_NULL(r.errors);

  fx_begin("[2][2]i32{ [2]i32{ 1, 2 }, [2]i32{ 3, 4 } }");
  r = parse_expr(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  a = (const SyntaxArrayLitExpr *)r.node;
  TEST_ASSERT_EQUAL_size_t(2, syntax_nodelist_length(a->elements));
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_ARRAY_LIT_EXPR, a->elements->node->kind);
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(strlen("[2][2]i32{ [2]i32{ 1, 2 }, [2]i32{ 3, 4 } }"), r.rem.start);
}

void test_array_lit_malform(void) {
  fx_begin("[5]i32{ 1 2 }");
  SyntaxNodeResult r = parse_array_lit_expr(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_EQUAL_size_t(1, syntax_nodelist_length(((const SyntaxArrayLitExpr *)r.node)->elements));
  TEST_ASSERT_EQUAL_size_t(1, error_chain_length(r.errors));
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_EXPECTED_RBRACE, r.errors->error.code);
  TEST_ASSERT_EQUAL_size_t(strlen("[5]i32{ 1"), r.rem.start);
}

void test_generic_struct_lit_generic_type(void) {
  fx_begin("Vector2<f32>{ x = 0_f32 }");
  SyntaxNodeResult r = parse_expr(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_STRUCT_LIT_EXPR, r.node->kind);
  const SyntaxStructLitExpr *s = (const SyntaxStructLitExpr *)r.node;
  const SyntaxNamed *t = as_named((const SyntaxNode *)s->type);
  TEST_ASSERT_EQUAL_size_t(1, syntax_nodelist_length(t->generic_args));
  TEST_ASSERT_EQUAL_size_t(1, syntax_nodelist_length(s->fields));
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(strlen("Vector2<f32>{ x = 0_f32 }"), r.rem.start);
}

static const TestDispatchEntry ENTRIES[] = {
    {"int_decimal", test_int_decimal},
    {"int_base_prefixed", test_int_base_prefixed},
    {"malformed_base_prefixes", test_malformed_base_prefixes},
    {"float_exponent", test_float_exponent},
    {"float_dot", test_float_dot},
    {"splits_int", test_splits_int},
    {"splits_float", test_splits_float},
    {"boundaries_number", test_boundaries_number},
    {"rune_simple_and_numeric", test_rune_simple_and_numeric},
    {"rune_malformed", test_rune_malformed},
    {"rune_not_match", test_rune_not_match},
    {"string_simple_and_escapes", test_string_simple_and_escapes},
    {"string_malformed", test_string_malformed},
    {"string_not_match", test_string_not_match},
    {"struct_lit_forms", test_struct_lit_forms},
    {"struct_lit_not_a_literal", test_struct_lit_not_a_literal},
    {"struct_lit_field_frame", test_struct_lit_field_frame},
    {"array_lit_forms", test_array_lit_forms},
    {"array_lit_malform", test_array_lit_malform},
    {"generic_struct_lit_generic_type", test_generic_struct_lit_generic_type},
};

TEST_DISPATCH_MAIN(ENTRIES)
