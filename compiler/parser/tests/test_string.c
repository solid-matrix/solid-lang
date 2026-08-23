/**
 * @file test_string.c
 * @brief Tests for parse_string_lit_expr, against doc/syntax.md.
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

/* Parses the whole text as one string literal (no surrounding trivia). */
static ParserResult parse(const char *text) {
  begin(text);
  return parse_string_lit_expr(g_parser, source_get_span(g_source));
}

static void release(ParserResult *r) { syntax_errorlist_destroy(r->errors); }

static size_t error_count(const ParserResult *r) {
  size_t n = 0;
  for (const SyntaxErrorListNode *e = r->errors->head; e != NULL; e = e->next)
    n++;
  return n;
}

/* Asserts that text scans to a string whose raw token is the full text. */
static void expect_string(const char *text) {
  ParserResult r = parse(text);
  CHECK(r.matched);
  CHECK(r.node != NULL);
  if (!r.node) {
    release(&r);
    return;
  }
  CHECK(r.node->kind == SYNTAX_KIND_STRING_LIT_EXPR);
  CHECK(strview_equals(((SyntaxStringLitExpr *)r.node)->value,
                       strview_create((const uint8_t *)text, strlen(text))));
  CHECK(syntax_errorlist_is_empty(r.errors));
  CHECK(r.rem.start == strlen(text));
  release(&r);
}

/* Asserts that text is consumed as a recovery run carrying exactly one
   SYNTAX_MALFORMED_STRING diagnostic and no node. */
static void expect_malformed(const char *text) {
  ParserResult r = parse(text);
  CHECK(r.matched);      // recovered
  CHECK(r.node == NULL); // nothing worth keeping
  CHECK(error_count(&r) == 1);

  const SyntaxErrorListNode *e = r.errors->head;
  CHECK(e != NULL && e->error.code == SYNTAX_MALFORMED_STRING);
  release(&r);
}

/* Asserts that text does not start a string attempt at all. */
static void expect_not_match(const char *text) {
  ParserResult r = parse(text);
  CHECK(!r.matched);
  CHECK(r.node == NULL);
  CHECK(r.errors == NULL); // convention: not-matched carries no list
  CHECK(r.rem.start == 0 && r.rem.end == strlen(text));
}

static void test_valid(void) {
  static const char *const SIMPLE[] = {"\"\"",      "\"hello\"", "\"'a'\"",
                                       "\"\\\"\"",  "\"\\\\\"",  "\"中文😀\"",
                                       "\"tab\\there\""};
  static const char *const ESCAPES[] = {"\"a\\nb\\tc\\0d\\r\"", "\"\\x09\"",
                                        "\"\\x7f\"", "\"\\u{1F600}\"",
                                        "\"\\u{10_FFFF}\""};

  for (size_t i = 0; i < sizeof(SIMPLE) / sizeof(SIMPLE[0]); i++)
    expect_string(SIMPLE[i]);
  for (size_t i = 0; i < sizeof(ESCAPES) / sizeof(ESCAPES[0]); i++)
    expect_string(ESCAPES[i]);
}

static void test_invalid(void) {
  static const char *const CASES[] = {
      "\"abc",        // missing closing quote
      "\"a\nb\"",     // raw line feed
      "\"a\r b\"",    // raw carriage return
      "\"a\tb\"",     // raw horizontal tab
      "\"\\q\"",      // unknown escape
      "\"\\x\"",      // missing digits
      "\"\\x7G\"",    // G is not a hexadecimal digit
      "\"\\x8\"",     // one digit missing
      "\"\x80\"",     // lone continuation byte
      "\"\xc0\xaf\"", // overlong encoding
      "\"\\u{}\"",    // empty unicode escape
      "\"\\u{41_}\"", // trailing underscore
      "\"\\u{D800}\"",// surrogate
      "\"\\u{110000}\""// out of Unicode scalar range
  };

  for (size_t i = 0; i < sizeof(CASES) / sizeof(CASES[0]); i++)
    expect_malformed(CASES[i]);
}

static void test_boundaries(void) {
  // Not a string start: rejected without consuming.
  expect_not_match("x");
  expect_not_match("'");
  expect_not_match("");
}

static const TestEntry ENTRIES[] = {
    {"valid", test_valid},
    {"invalid", test_invalid},
    {"boundaries", test_boundaries},
};

TEST_MAIN(ENTRIES)
