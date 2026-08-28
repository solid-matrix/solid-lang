#include <string.h>

#include "syntax_parses.h"
#include "parser_fixture.h"
#include "syntax_nodes.h"
#include "syntax_node.h"
#include "test_support.h"

static size_t error_count(const SyntaxNodeResult *r) {
  size_t n = 0;
  for (const SyntaxErrorList *e = r->errors; e != NULL; e = e->next)
    n++;
  return n;
}

/* ---- shared expectations ------------------------------------------- */

// The number entry point: int and float branches, longest match wins.
static SyntaxNodeResult parse_number_now(void) {
  Span span = source_get_span(fx_source);
  SyntaxNodeResult results[] = {
      parse_int_lit_expr(fx_parser, span),
      parse_float_lit_expr(fx_parser, span),
  };
  return complete_longest_match(results, sizeof(results) / sizeof(results[0]));
}

// Parses the whole text as one literal of the given family.
typedef SyntaxNodeResult (*ParseFn)(const char *);

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

// Asserts that text scans wholly to one node whose value view is the
// full text and which carries no diagnostics.
static void expect_whole(ParseFn fn, const char *text, SyntaxKind kind) {
  SyntaxNodeResult r = fn(text);

  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_NOT_NULL(r.node);
  TEST_ASSERT_EQUAL_HEX32(kind, r.node->kind);
  TEST_ASSERT_STRVIEW_EQ(lit_value(r.node), text);
  TEST_ASSERT_EQUAL_size_t(strlen(text), r.rem.start);
}

// Asserts that only the first tok_len bytes form the token; the rest
// splits into later tokens (longest valid prefix).
static void expect_split(ParseFn fn, const char *text, SyntaxKind kind,
                         size_t tok_len) {
  SyntaxNodeResult r = fn(text);

  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_NOT_NULL(r.node);
  TEST_ASSERT_EQUAL_HEX32(kind, r.node->kind);
  TEST_ASSERT_EQUAL_size_t(tok_len, lit_value(r.node).len);
  TEST_ASSERT_EQUAL_size_t(tok_len, r.rem.start);
}

// Asserts a recovery run: matched, no node, exactly one diagnostic.
static void expect_malformed(ParseFn fn, const char *text,
                             SyntaxErrorCode code) {
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
      "0",         "0i32",   "0_i32",  "1",         "1i32",   "1_i32",
      "12",        "12i32",  "12_i32", "1_2",       "1_2i32", "1_2_i32",
      "1_234_567", "0isize", "1u128",  "1234567_u",
  };
  for (size_t i = 0; i < sizeof(CASES) / sizeof(CASES[0]); i++)
    expect_whole(parse_number, CASES[i], SYNTAX_KIND_INT_LIT_EXPR);
}

void test_int_base_prefixed(void) {
  static const char *const CASES[] = {
      "0b0",         "0b01",         "0b1",
      "0b_0",        "0b_0000_1111", "0B_0000_1111_u8",
      "0b1010_1101", "0b1u8",        "0o0",
      "0o17",        "0o_123",       "0O_123",
      "0o7_i16",     "0x0",          "0xFF",
      "0x_FFFF",     "0X_FFFF",      "0xDeAd_beEf",
      "0xF_u32",     "0xFFu64",
  };
  for (size_t i = 0; i < sizeof(CASES) / sizeof(CASES[0]); i++)
    expect_whole(parse_number, CASES[i], SYNTAX_KIND_INT_LIT_EXPR);
}

void test_malformed_base_prefixes(void) {
  // Prefix without digits, underscore without digits, or a digit
  // outside the base: reported and recovered as one run.
  static const char *const CASES[] = {
      "0b",  "0B",   "0x",   "0X",  "0o",
      "0O",  "0x_",  "0b_",  "0b__0", "0o8",
      "0b2", "0o9",  "0xG",  "0x_g",
  };
  for (size_t i = 0; i < sizeof(CASES) / sizeof(CASES[0]); i++)
    expect_malformed(parse_number, CASES[i], SYNTAX_MALFORMED_NUMBER);
}

/* ---- float literals ------------------------------------------------ */

void test_float_exponent(void) {
  static const char *const CASES[] = {
      "1e5",  "1e5_f32", "1.5e5",  "1.5e5_f32", "1e+5",  "1e-5",
      "1e_5", "1E5",     "1E+5",   "1E-5",      "1e+_5", "1e-_5",
      "0e0",  "1_000e3", "1e5f64", "1.5e5_f64",
  };
  for (size_t i = 0; i < sizeof(CASES) / sizeof(CASES[0]); i++)
    expect_whole(parse_number, CASES[i], SYNTAX_KIND_FLOAT_LIT_EXPR);
}

void test_float_dot(void) {
  static const char *const CASES[] = {
      "1.", "1.5", "1.5_f32", "1.5f32", "0.5", "0.0", "12.75", "1.f32",
      "1.5d",
  };
  for (size_t i = 0; i < sizeof(CASES) / sizeof(CASES[0]); i++)
    expect_whole(parse_number, CASES[i], SYNTAX_KIND_FLOAT_LIT_EXPR);
}

/* ---- longest-prefix splits ----------------------------------------- */

void test_splits_int(void) {
  expect_split(parse_number, "01", SYNTAX_KIND_INT_LIT_EXPR, 1);
  expect_split(parse_number, "00", SYNTAX_KIND_INT_LIT_EXPR, 1);
  expect_split(parse_number, "1_", SYNTAX_KIND_INT_LIT_EXPR, 1);
  expect_split(parse_number, "0_0", SYNTAX_KIND_INT_LIT_EXPR, 1);
  expect_split(parse_number, "0_", SYNTAX_KIND_INT_LIT_EXPR, 1);
  expect_split(parse_number, "1__2", SYNTAX_KIND_INT_LIT_EXPR, 1);
  expect_split(parse_number, "12abc", SYNTAX_KIND_INT_LIT_EXPR, 2);
  // "_" binds to a following suffix only, never to an exponent.
  expect_split(parse_number, "1_e5", SYNTAX_KIND_INT_LIT_EXPR, 1);
  // A bare "e"/sign is not an exponent without digits.
  expect_split(parse_number, "1e", SYNTAX_KIND_INT_LIT_EXPR, 1);
  expect_split(parse_number, "1e+", SYNTAX_KIND_INT_LIT_EXPR, 1);
  // Suffix greediness stops at the first non-suffix character.
  expect_split(parse_number, "0i8x", SYNTAX_KIND_INT_LIT_EXPR, 3);
  expect_split(parse_number, "1u128x", SYNTAX_KIND_INT_LIT_EXPR, 5);
}

void test_splits_float(void) {
  expect_split(parse_number, "1.e5", SYNTAX_KIND_FLOAT_LIT_EXPR, 2);
  expect_split(parse_number, "1e05", SYNTAX_KIND_FLOAT_LIT_EXPR, 3);
  expect_split(parse_number, "1e5.5", SYNTAX_KIND_FLOAT_LIT_EXPR, 3);
  expect_split(parse_number, "1e5_", SYNTAX_KIND_FLOAT_LIT_EXPR, 3);
  expect_split(parse_number, "1.e5f32", SYNTAX_KIND_FLOAT_LIT_EXPR, 2);
  // The dot branch stops before a second dot or a dead exponent.
  expect_split(parse_number, "1.5.5", SYNTAX_KIND_FLOAT_LIT_EXPR, 3);
  expect_split(parse_number, "1.fx", SYNTAX_KIND_FLOAT_LIT_EXPR, 3);
  expect_split(parse_number, "1.5e", SYNTAX_KIND_FLOAT_LIT_EXPR, 3);
  // One exponent only; a second one splits.
  expect_split(parse_number, "1e5e5", SYNTAX_KIND_FLOAT_LIT_EXPR, 3);
  expect_split(parse_number, "1e5x", SYNTAX_KIND_FLOAT_LIT_EXPR, 3);
  expect_split(parse_number, "0dx", SYNTAX_KIND_FLOAT_LIT_EXPR, 2);
  expect_split(parse_number, "1__5", SYNTAX_KIND_INT_LIT_EXPR, 1);
}

void test_boundaries_number(void) {
  // Not a number start: rejected without consuming.
  expect_not_match(parse_number, ".5");

  // Underscore-separated hex digits keep working with a suffix.
  expect_whole(parse_number, "0xF_F_u8", SYNTAX_KIND_INT_LIT_EXPR);

  // Uppercase F32 is not a suffix; the token ends before it.
  expect_split(parse_number, "0_F32", SYNTAX_KIND_INT_LIT_EXPR, 1);

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
  static const char *const SIMPLE[] = {"'a'",    "'字'",  "'\\''", "'\\\"'",
                                       "'\\\\'", "'\\n'", "'\\r'", "'\\t'",
                                       "'\\0'",  "'€'",   "'😀'"};
  static const char *const NUMERIC[] = {
      "'\\x41'",       "'\\x30'",        "'\\x7f'",
      "'\\u{41}'",     "'\\u{0}'",       "'\\u{1_F600}'",
      "'\\u{10FFFF}'", "'\\u{10_FFFF}'", "'\\u{00e9}'"};

  for (size_t i = 0; i < sizeof(SIMPLE) / sizeof(SIMPLE[0]); i++)
    expect_whole(parse_rune, SIMPLE[i], SYNTAX_KIND_RUNE_LIT_EXPR);
  for (size_t i = 0; i < sizeof(NUMERIC) / sizeof(NUMERIC[0]); i++)
    expect_whole(parse_rune, NUMERIC[i], SYNTAX_KIND_RUNE_LIT_EXPR);
}

void test_rune_malformed(void) {
  static const char *const CASES[] = {
      "''",            // empty rune
      "'''",           // unescaped single quote
      "'ab'",          // more than one character
      "'ab",           // unterminated
      "'\\q'",         // unknown escape
      "'\\x'",         // missing digits
      "'\\x8'",        // one digit missing
      "'\\x80'",       // out of ASCII range
      "'\\x7G'",       // G is not a hexadecimal digit
      "'\\u{}'",       // empty unicode escape
      "'\\u{_41}'",    // leading underscore
      "'\\u{41_}'",    // trailing underscore
      "'\\u{1__F}'",   // consecutive underscores
      "'\\u{D800}'",   // surrogate
      "'\\u{DFFF}'",   // surrogate
      "'\\u{110000}'", // out of Unicode scalar range
      "'\\u{FFFFFF}'", // far out of range
      "'\t'",          // raw horizontal tab
      "'\n'",          // raw line feed
      "'\r'",          // raw carriage return
      "'\x80'",        // lone continuation byte
      "'\xc0\xaf'",    // overlong encoding
  };

  for (size_t i = 0; i < sizeof(CASES) / sizeof(CASES[0]); i++)
    expect_malformed(parse_rune, CASES[i], SYNTAX_MALFORMED_RUNE);
}

void test_rune_not_match(void) {
  expect_not_match(parse_rune, "x");
  expect_not_match(parse_rune, "\"");
  expect_not_match(parse_rune, "");
}

/* ---- string literals ----------------------------------------------- */

void test_string_simple_and_escapes(void) {
  static const char *const SIMPLE[] = {
      "\"\"",     "\"hello\"",  "\"'a'\"",        "\"\\\"\"",
      "\"\\\\\"", "\"中文😀\"", "\"tab\\there\"",
  };
  static const char *const ESCAPES[] = {
      "\"a\\nb\\tc\\0d\\r\"", "\"\\x09\"", "\"\\x7f\"",
      "\"\\u{1F600}\"",       "\"\\u{10_FFFF}\"",
  };

  for (size_t i = 0; i < sizeof(SIMPLE) / sizeof(SIMPLE[0]); i++)
    expect_whole(parse_string, SIMPLE[i], SYNTAX_KIND_STRING_LIT_EXPR);
  for (size_t i = 0; i < sizeof(ESCAPES) / sizeof(ESCAPES[0]); i++)
    expect_whole(parse_string, ESCAPES[i], SYNTAX_KIND_STRING_LIT_EXPR);
}

void test_string_malformed(void) {
  static const char *const CASES[] = {
      "\"abc",          // missing closing quote
      "\"a\nb\"",       // raw line feed
      "\"a\r b\"",      // raw carriage return
      "\"a\tb\"",       // raw horizontal tab
      "\"\\q\"",        // unknown escape
      "\"\\x\"",        // missing digits
      "\"\\x7G\"",      // G is not a hexadecimal digit
      "\"\\x8\"",       // one digit missing
      "\"\x80\"",       // lone continuation byte
      "\"\xc0\xaf\"",   // overlong encoding
      "\"\\u{}\"",      // empty unicode escape
      "\"\\u{41_}\"",   // trailing underscore
      "\"\\u{D800}\"",  // surrogate
      "\"\\u{110000}\"" // out of Unicode scalar range
  };

  for (size_t i = 0; i < sizeof(CASES) / sizeof(CASES[0]); i++)
    expect_malformed(parse_string, CASES[i], SYNTAX_MALFORMED_STRING);
}

void test_string_not_match(void) {
  expect_not_match(parse_string, "x");
  expect_not_match(parse_string, "'");
  expect_not_match(parse_string, "");
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
};

TEST_DISPATCH_MAIN(ENTRIES)
