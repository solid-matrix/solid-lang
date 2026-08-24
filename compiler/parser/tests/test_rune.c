/**
 * @file test_rune.c
 * @brief Tests for parse_rune_lit_expr, against doc/syntax.md.
 * @author solid-matrix
 * @version 0.0.5
 */

#include <string.h>

#include "parser.h"
#include "source.h"
#include "syntax_node.h"
#include "test_util.h"

static Source *g_source;
static Parser *g_parser;
static bool g_active;

static void begin(const char *text) {
  if (g_active) {
    parser_destroy(g_parser);
    source_destroy(g_source);
  }
  g_source = source_from_cstr(text);
  g_parser = parser_create(g_source);
  g_active = true;
}

/* Parses the whole text as one rune literal (no surrounding trivia). */
static ParserResult parse(const char *text) {
  begin(text);
  return parse_rune_lit_expr(g_parser, source_get_span(g_source));
}

static void release(ParserResult *r) { syntax_errorlist_destroy(&r->errors); }

static size_t error_count(const ParserResult *r) {
  size_t n = 0;
  for (const SyntaxErrorList *e = r->errors; e != NULL; e = e->next)
    n++;
  return n;
}

/* Asserts that text scans to a rune whose raw token is the full text. */
static void expect_rune(const char *text) {
  ParserResult r = parse(text);
  CHECK(r.matched);
  CHECK(r.node != NULL);
  if (!r.node) {
    release(&r);
    return;
  }
  CHECK(r.node->kind == SYNTAX_KIND_RUNE_LIT_EXPR);
  CHECK(strview_equals(((SyntaxRuneLitExpr *)r.node)->value,
                       strview_create((const uint8_t *)text, strlen(text))));
  CHECK(r.errors == NULL);
  CHECK(r.rem.start == strlen(text));
  release(&r);
}

/* Asserts that text is consumed as a recovery run carrying exactly one
   SYNTAX_MALFORMED_RUNE diagnostic and no node. */
static void expect_malformed(const char *text) {
  ParserResult r = parse(text);
  CHECK(r.matched);      // recovered
  CHECK(r.node == NULL); // nothing worth keeping
  CHECK(error_count(&r) == 1);

  const SyntaxErrorList *e = r.errors;
  CHECK(e != NULL && e->error.code == SYNTAX_MALFORMED_RUNE);
  release(&r);
}

/* Asserts that text does not start a rune attempt at all. */
static void expect_not_match(const char *text) {
  ParserResult r = parse(text);
  CHECK(!r.matched);
  CHECK(r.node == NULL);
  CHECK(r.errors == NULL); // convention: not-matched carries no list
  CHECK(r.rem.start == 0 && r.rem.end == strlen(text));
}

static void test_valid(void) {
  static const char *const SIMPLE[] = {"'a'",    "'字'",  "'\\''", "'\\\"'",
                                       "'\\\\'", "'\\n'", "'\\r'", "'\\t'",
                                       "'\\0'",  "'€'",   "'😀'"};
  static const char *const NUMERIC[] = {
      "'\\x41'",       "'\\x30'",        "'\\x7f'",
      "'\\u{41}'",     "'\\u{0}'",       "'\\u{1_F600}'",
      "'\\u{10FFFF}'", "'\\u{10_FFFF}'", "'\\u{00e9}'"};

  for (size_t i = 0; i < sizeof(SIMPLE) / sizeof(SIMPLE[0]); i++)
    expect_rune(SIMPLE[i]);
  for (size_t i = 0; i < sizeof(NUMERIC) / sizeof(NUMERIC[0]); i++)
    expect_rune(NUMERIC[i]);
}

static void test_invalid(void) {
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
    expect_malformed(CASES[i]);
}

static void test_boundaries(void) {
  // Not a rune start: rejected without consuming.
  expect_not_match("x");
  expect_not_match("\"");
  expect_not_match("");
}

static const TestEntry ENTRIES[] = {
    {"valid", test_valid},
    {"invalid", test_invalid},
    {"boundaries", test_boundaries},
};

TEST_MAIN(ENTRIES)
