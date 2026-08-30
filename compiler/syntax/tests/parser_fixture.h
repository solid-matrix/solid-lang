/**
 * @file parser_fixture.h
 * @brief Source/Parser lifecycle fixture and shared expectations for
 *        the parser's unit tests.
 * @author solid-matrix
 * @version 0.0.5
 */

#pragma once

#include "source.h"
#include "syntax_errorlist.h"
#include "syntax_nodes.h"
#include "syntax_parser.h"
#include "syntax_result.h"
#include "test_support.h"

static Source *fx_source;
static SyntaxParser *fx_parser;

/**
 * @brief Points the fixture at @p text, releasing any previous parse.
 * @param text The source text to parse.
 */
static void fx_begin(const char *text) {
  if (fx_parser != NULL) {
    syntax_parser_destroy(fx_parser);
    source_destroy(fx_source);
  }
  fx_source = source_from_cstr(text);
  fx_parser = syntax_parser_create(fx_source);
}

/**
 * @brief Releases the current parse.
 * @note Safe to call repeatedly.
 */
static void fx_release(void) {
  if (fx_parser != NULL) {
    syntax_parser_destroy(fx_parser);
    fx_parser = NULL;
  }
  if (fx_source != NULL) {
    source_destroy(fx_source);
    fx_source = NULL;
  }
}

/* ---- shared expectations ---------------------------------------------- */

/**
 * @brief Number of diagnostics on the outcome's chain.
 * @param r The outcome to measure.
 * @return The chain length; 0 when errors is NULL.
 */
static inline size_t error_count(const SyntaxNodeResult *r) {
  size_t n = 0;
  for (const SyntaxErrorList *e = r->errors; e != NULL; e = e->next)
    n++;
  return n;
}

/**
 * @brief Number of diagnostics on the chain.
 * @param e The chain head, possibly NULL.
 * @return The chain length.
 */
static inline size_t error_chain_length(const SyntaxErrorList *e) {
  size_t n = 0;
  for (; e != NULL; e = e->next)
    n++;
  return n;
}

/**
 * @brief Asserts that the chain holds exactly the given segments in
 *        newest-at-head order: chain[i] is expected[count - 1 - i].
 * @param chain The path chain to check.
 * @param expected Segments in source order; NULL asserts an empty chain.
 * @param count Number of segments.
 */
static inline void check_path(const SyntaxNodeList *chain, const char *const *expected, size_t count) {
  if (expected == NULL) {
    TEST_ASSERT_NULL(chain);
    return;
  }

  const SyntaxNodeList *n = chain;
  for (size_t i = 0; i < count; i++) {
    TEST_ASSERT_NOT_NULL(n);
    if (!n)
      return;

    const char *want = expected[count - 1 - i];
    const SyntaxIdentifier *id = (const SyntaxIdentifier *)n->node;
    TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_IDENTIFIER, id->header.kind);
    TEST_ASSERT_STRVIEW_EQ(id->value, want);
    n = n->next;
  }
  TEST_ASSERT_NULL(n); // exactly @p count segments
}

/**
 * @brief Asserts a NAMED node and returns it.
 * @param n The node to test.
 * @return The node cast to SyntaxNamed.
 */
static inline const SyntaxNamed *as_named(const SyntaxNode *n) {
  TEST_ASSERT_NOT_NULL(n);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_NAMED, n->kind);
  return (const SyntaxNamed *)n;
}

/**
 * @brief Asserts an integer literal whose value view is @p text.
 * @param n The node to test.
 * @param text The expected literal text.
 * @return The node cast to SyntaxIntLitExpr.
 */
static inline const SyntaxIntLitExpr *as_int(const SyntaxNode *n, const char *text) {
  TEST_ASSERT_NOT_NULL(n);
  if (!n)
    return NULL;
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_INT_LIT_EXPR, n->kind);
  TEST_ASSERT_STRVIEW_EQ(((const SyntaxIntLitExpr *)n)->value, text);
  return (const SyntaxIntLitExpr *)n;
}
