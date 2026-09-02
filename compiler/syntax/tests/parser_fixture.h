/**
 * @file parser_fixture.h
 * @brief Source/Parser lifecycle fixture and shared expectations for
 *        the parser's unit tests.
 * @author solid-matrix
 */

#pragma once

#include "arena.h"
#include "parse.h"
#include "source.h"
#include "test_support.h"

static Source *fx_source;
static Arena *fx_arena;
static SyntaxParser fx_parser_storage;
static SyntaxParser *fx_parser; // points at fx_parser_storage

/**
 * @brief Releases the current parse.
 * @note Safe to call repeatedly.
 */
static void fx_release(void) {
  fx_parser = NULL;
  if (fx_arena != NULL) {
    arena_destroy(fx_arena); // reclaims every node of this parse
    fx_arena = NULL;
  }
  if (fx_source != NULL) {
    source_destroy(fx_source);
    fx_source = NULL;
  }
}

/**
 * @brief Points the fixture at @p text, releasing any previous parse.
 * @param text The source text to parse.
 */
static void fx_begin(const char *text) {
  fx_release();
  fx_source = source_from_cstr(text);
  fx_arena = arena_create();
  fx_parser_storage = (SyntaxParser){.source = fx_source, .arena = fx_arena};
  fx_parser = &fx_parser_storage;
}

/* ---- shared expectations ---------------------------------------------- */

/**
 * @brief Number of diagnostics on the outcome's chain.
 * @param r The outcome to measure.
 * @return The chain length; 0 when errors is NULL.
 */
static inline size_t error_count(const SyntaxNodeResult *r) {
  size_t n = 0;
  for (const SyntaxErrorList *e = r->errors; e != NULL; e = e->tail)
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
  for (; e != NULL; e = e->tail)
    n++;
  return n;
}

/**
 * @brief Asserts that the chain holds exactly the given segments in source
 *        order: chain[i] is expected[i].
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

    const char *want = expected[i];
    const SyntaxIdentifier *id = (const SyntaxIdentifier *)n->head;
    TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_IDENTIFIER, id->header.kind);
    TEST_ASSERT_STRVIEW_EQ(id->value, want);
    n = n->tail;
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
