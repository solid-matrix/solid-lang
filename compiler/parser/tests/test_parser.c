/**
 * @file test_parser.c
 * @brief Tests for the parser entry points, one construct at a time.
 * @author solid-matrix
 * @version 0.0.5
 *
 * Number-literal expectations follow doc/syntax.md (Number Literals):
 * malformed base-prefixed forms report SYNTAX_MALFORMED_NUMBER and
 * recover over the whole token-looking run, while other "invalid" forms
 * in the document split into multiple valid tokens and are rejected
 * later by the grammar, not by the scanner.
 */

#include <string.h>

#include "ast.h"
#include "parser.h"
#include "source.h"
#include "test_util.h"

static Source g_source;
static Parser g_parser;
static bool g_active;

static void begin(const char *text) {
  if (g_active)
    source_destroy(&g_source);
  g_source = source_from_cstr(text);
  g_parser = parser_create(&g_source);
  g_active = true;
}

/* Parses the whole text as one number literal (no surrounding trivia). */
static ParserResult parse_number(const char *text) {
  begin(text);
  return parse_number_lit_expr(&g_parser, source_get_span(&g_source));
}

static size_t error_count(const ParserResult *r) {
  size_t n = 0;
  for (const SyntaxErrorLinkedList *e = r->errors; e; e = e->next)
    n++;
  return n;
}

/* Asserts that text parses to a single INT literal whose value is the
   full original text. */
static void expect_int(const char *text) {
  ParserResult r = parse_number(text);
  CHECK(r.matched);
  CHECK(r.errors == NULL);
  CHECK(r.node != NULL);
  if (!r.node)
    return;
  CHECK(r.node->kind == SYNTAX_KIND_INT_LIT_EXPR);
  CHECK(sv_equals(((SyntaxNumberLitExpr *)r.node)->value,
                  sv_create((const uint8_t *)text, strlen(text))));
}

/* Same for FLOAT literals. */
static void expect_float(const char *text) {
  ParserResult r = parse_number(text);
  CHECK(r.matched);
  CHECK(r.errors == NULL);
  CHECK(r.node != NULL);
  if (!r.node)
    return;
  CHECK(r.node->kind == SYNTAX_KIND_FLOAT_LIT_EXPR);
  CHECK(sv_equals(((SyntaxNumberLitExpr *)r.node)->value,
                  sv_create((const uint8_t *)text, strlen(text))));
}

/* Asserts that only the first tok_len bytes form an int token and rem
   points just past it (the rest splits into later tokens). */
static void expect_int_split(const char *text, size_t tok_len) {
  ParserResult r = parse_number(text);
  CHECK(r.matched);
  CHECK(r.errors == NULL);
  CHECK(r.node != NULL);
  if (!r.node)
    return;
  CHECK(r.node->kind == SYNTAX_KIND_INT_LIT_EXPR);
  CHECK(sv_equals(((SyntaxNumberLitExpr *)r.node)->value,
                  sv_create((const uint8_t *)text, tok_len)));
  CHECK(r.rem.start == tok_len);
}

static void expect_float_split(const char *text, size_t tok_len) {
  ParserResult r = parse_number(text);
  CHECK(r.matched);
  CHECK(r.errors == NULL);
  CHECK(r.node != NULL);
  if (!r.node)
    return;
  CHECK(r.node->kind == SYNTAX_KIND_FLOAT_LIT_EXPR);
  CHECK(sv_equals(((SyntaxNumberLitExpr *)r.node)->value,
                  sv_create((const uint8_t *)text, tok_len)));
  CHECK(r.rem.start == tok_len);
}

static void expect_malformed(const char *text) {
  ParserResult r = parse_number(text);
  CHECK(r.matched);      // consumed as a recovery run
  CHECK(r.node == NULL); // nothing worth keeping
  CHECK(error_count(&r) == 1);

  const SyntaxErrorLinkedList *e = r.errors;
  CHECK(e != NULL && e->error.code == SYNTAX_MALFORMED_NUMBER);
}

static void test_int_valid(void) {
  static const char *const DECIMAL[] = {"0",      "0i32",  "0_i32",  "1",
                                        "1i32",   "1_i32", "12",     "12i32",
                                        "12_i32", "1_2",   "1_2i32", "1_2_i32"};
  static const char *const BASES[] = {"0b0",          "0b01",
                                      "0b1",          "0b_0",
                                      "0b_0000_1111", "0B_0000_1111_u8",
                                      "0o_123",       "0O_123",
                                      "0x_FFFF",      "0X_FFFF"};
  static const char *const SUFFIXED[] = {
      "0_i8", "0_i16", "0_i64",  "0_i128",  "0_isize", "0_i",
      "0_u8", "0_u16", "0_u128", "0_usize", "0_u"};

  for (size_t i = 0; i < sizeof(DECIMAL) / sizeof(DECIMAL[0]); i++)
    expect_int(DECIMAL[i]);
  for (size_t i = 0; i < sizeof(BASES) / sizeof(BASES[0]); i++)
    expect_int(BASES[i]);
  for (size_t i = 0; i < sizeof(SUFFIXED) / sizeof(SUFFIXED[0]); i++)
    expect_int(SUFFIXED[i]);
}

static void test_float_valid(void) {
  static const char *const EXPONENT[] = {
      "1e5", "1E5", "1e5_f32", "1.5e5", "1.5e5_f32", "1e+5", "1e-5", "1e_5"};
  static const char *const DOT[] = {"1.", "1.5", "1.5_f32", "1.5f32", "0.5"};
  static const char *const SUFFIXED[] = {"1f", "1f32", "1_f32", "0d"};

  for (size_t i = 0; i < sizeof(EXPONENT) / sizeof(EXPONENT[0]); i++)
    expect_float(EXPONENT[i]);
  for (size_t i = 0; i < sizeof(DOT) / sizeof(DOT[0]); i++)
    expect_float(DOT[i]);
  for (size_t i = 0; i < sizeof(SUFFIXED) / sizeof(SUFFIXED[0]); i++)
    expect_float(SUFFIXED[i]);
}

static void test_malformed_prefixes(void) {
  // Base prefix without digits, underscore without digits, or a digit
  // outside the base: reported and recovered as one run.
  static const char *const CASES[] = {"0b", "0B",  "0x",  "0X",    "0o",
                                      "0O", "0x_", "0b_", "0b__0", "0o8"};

  for (size_t i = 0; i < sizeof(CASES) / sizeof(CASES[0]); i++)
    expect_malformed(CASES[i]);
}

static void test_splits(void) {
  // Forms the document marks invalid but which tokenize cleanly into a
  // leading number plus further tokens; the scanner takes the longest
  // valid prefix.
  expect_int_split("01", 1);    // INT(0), then "1"
  expect_int_split("00", 1);    // INT(0), then "0"
  expect_int_split("1_", 1);    // INT(1), then "_"
  expect_int_split("0_0", 1);   // INT(0), then "_0"
  expect_int_split("1__2", 1);  // INT(1), then "__2"
  expect_int_split("12abc", 2); // INT(12), then "abc"

  expect_float_split("1.e5", 2);    // FLOAT(1.), then "e5"
  expect_float_split("1e05", 3);    // FLOAT(1e0), then "5"
  expect_float_split("1e5.5", 3);   // FLOAT(1e5), then ".5"
  expect_float_split("1e5_", 3);    // FLOAT(1e5), then "_"
  expect_float_split("1.e5f32", 2); // FLOAT(1.), then "e5f32"

  expect_int_split("1__5", 1); // INT(1), then "__5"
}

static void test_boundaries(void) {
  // Not a number start: rejected without consuming.
  begin(".5");
  ParserResult r = parse_number_lit_expr(&g_parser, source_get_span(&g_source));
  CHECK(!r.matched);
  CHECK(r.node == NULL);
  CHECK(r.errors == NULL);
  CHECK(r.rem.start == 0);

  // Underscore-separated hex digits keep working with a suffix.
  expect_int("0xF_F_u8");

  // Float suffixes are lowercase-only per the grammar; uppercase F32 is
  // not a suffix, so the token ends before it.
  expect_int_split("0_F32", 1);

  // A suffix must not swallow identifier characters beyond it.
  expect_int_split("12isizex", 7); // INT(12isize), then "x"

  // Trailing trivia belongs to the enclosing sequence: rem stops right
  // after the token and the kept node's span matches it exactly.
  begin("12 // c\nx");
  r = parse_number_lit_expr(&g_parser, source_get_span(&g_source));
  CHECK(r.matched);
  CHECK(r.errors == NULL);
  CHECK(r.node != NULL);
  CHECK(r.node->span.end == 2 && r.node->span.start == 0);
  CHECK(r.rem.start == 2);
}

static const TestEntry ENTRIES[] = {
    {"int_valid", test_int_valid},
    {"float_valid", test_float_valid},
    {"malformed_prefixes", test_malformed_prefixes},
    {"splits", test_splits},
    {"boundaries", test_boundaries},
};

TEST_MAIN(ENTRIES)
