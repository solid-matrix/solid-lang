/**
 * @file test_namepath.c
 * @brief Direct tests for parse_name_path, independent of the decl
 *        layer.
 * @author solid-matrix
 * @version 0.0.5
 */

#include <string.h>

#include "parser.h"
#include "source.h"
#include "syntax_error.h"
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

/* Runs parse_name_path over the whole test source. */
static ParserResult run(void) {
  return parse_name_path(g_parser, source_get_span(g_source));
}

/* Asserts that the chain holds exactly the given segments in
 * newest-at-head order: chain[i] is expected[count - 1 - i]. */
static void check_segments(const ParserResult *r,
                           const char *const *expected, size_t count) {
  const SyntaxNamePath *path = (const SyntaxNamePath *)r->node;
  CHECK(path != NULL && path->header.kind == SYNTAX_KIND_NAME_PATH);
  if (!path)
    return;

  const SyntaxNodeList *n = path->segments;
  for (size_t i = 0; i < count; i++) {
    CHECK(n != NULL);
    if (!n)
      return;

    const char *want = expected[count - 1 - i];
    const SyntaxIdentifier *id = (const SyntaxIdentifier *)n->node;
    CHECK(id->header.kind == SYNTAX_KIND_IDENTIFIER);
    CHECK(strview_equals(id->strview,
                         strview_create((const uint8_t *)want,
                                        strlen(want))));
    n = n->next;
  }
  CHECK(n == NULL); // exactly @p count segments
}

static size_t error_count(const ParserResult *r) {
  size_t n = 0;
  for (const SyntaxErrorList *e = r->errors; e != NULL; e = e->next)
    n++;
  return n;
}

static void test_single(void) {
  static const char *const ONE[] = {"std"};
  begin("std");
  ParserResult r = run();

  CHECK(r.matched);
  CHECK(r.errors == NULL);
  CHECK(r.rem.start == strlen("std"));
  check_segments(&r, ONE, 1);
}

static void test_multi(void) {
  static const char *const TWO[] = {"std", "io"};
  begin("std::io");
  ParserResult r = run();

  CHECK(r.matched);
  CHECK(r.errors == NULL);
  CHECK(r.rem.start == strlen("std::io"));
  check_segments(&r, TWO, 2);
}

static void test_trivia(void) {
  static const char *const TWO[] = {"std", "io"};
  begin("std :: io");
  ParserResult r = run();

  CHECK(r.matched);
  CHECK(r.errors == NULL); // junction trivia never leaks diagnostics
  CHECK(r.rem.start == strlen("std :: io"));
  check_segments(&r, TWO, 2);
}

static void check_trailing_sep(const char *text, size_t rem_at) {
  static const char *const ONE[] = {"a"};
  begin(text);
  ParserResult r = run();

  CHECK(r.matched);          // the [a] prefix is kept
  CHECK(r.node != NULL);
  CHECK(r.rem.start == rem_at); // past the "::", at the bad token

  CHECK(error_count(&r) == 1); // exactly one diagnostic
  CHECK(r.errors != NULL &&
        r.errors->error.code == SYNTAX_EXPECTED_IDENTIFIER);

  check_segments(&r, ONE, 1);
}

static void test_trailing_sep(void) {
  check_trailing_sep("a::-1", strlen("a::"));
}

static void test_trailing_sep_trivia(void) {
  check_trailing_sep("a :: ", strlen("a ::"));
}

static void test_no_first_segment(void) {
  begin(";abc");
  ParserResult r = run();
  CHECK(!r.matched);
  CHECK(r.node == NULL);
  CHECK(r.errors == NULL); // convention: not-match carries no list

  begin("1abc");
  r = run();
  CHECK(!r.matched);
  CHECK(r.node == NULL);
  CHECK(r.errors == NULL);

  begin("");
  r = run();
  CHECK(!r.matched);
}

static const TestEntry k_tests[] = {
    {"single", test_single},
    {"multi", test_multi},
    {"trivia", test_trivia},
    {"trailing_sep", test_trailing_sep},
    {"trailing_sep_trivia", test_trailing_sep_trivia},
    {"no_first_segment", test_no_first_segment},
};

TEST_MAIN(k_tests)
