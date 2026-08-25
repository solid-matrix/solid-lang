/**
 * @file test_decl.c
 * @brief Tests for the declaration parsers, against doc/syntax.md.
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

static size_t error_count(const ParserResult *r) {
  size_t n = 0;
  for (const SyntaxErrorList *e = r->errors; e != NULL; e = e->next)
    n++;
  return n;
}

/* Asserts that the path segments of a parsed decl match expectations. */
static void check_paths(ParserResult *r, const char *const *segments,
                        size_t count, SyntaxKind kind) {
  CHECK(r->node != NULL);
  if (!r->node)
    return;

  CHECK(r->node->kind == kind);

  const SyntaxNamePath *path =
      kind == SYNTAX_KIND_NAMESPACE_DECL
          ? ((const SyntaxNamespaceDecl *)r->node)->path
          : ((const SyntaxUsingDecl *)r->node)->path;
  CHECK(path != NULL);
  if (!path)
    return;

  CHECK(path->header.kind == SYNTAX_KIND_NAME_PATH);

  // Walk the chain in source order, comparing each segment.
  const SyntaxNodeList *n = path->segments;
  for (size_t i = 0; i < count; i++) {
    CHECK(n != NULL);
    if (!n)
      return;

    const SyntaxIdentifier *id = (const SyntaxIdentifier *)n->node;
    CHECK(id->header.kind == SYNTAX_KIND_IDENTIFIER);
    CHECK(
        strview_equals(id->strview, strview_create((const uint8_t *)segments[i],
                                                   strlen(segments[i]))));
    n = n->next;
  }
  CHECK(n == NULL); // exactly @p count segments
}

/* Asserts a fully formed namespace/using declaration. */
static void expect_decl(bool use_namespace, const char *text,
                        const char *const *segments, size_t count) {
  begin(text);
  ParserResult r =
      use_namespace ? parse_namespace_decl(g_parser, source_get_span(g_source))
                    : parse_using_decl(g_parser, source_get_span(g_source));

  CHECK(r.matched);
  CHECK(r.errors == NULL);
  CHECK(r.rem.start == strlen(text));
  check_paths(&r, segments, count,
              use_namespace ? SYNTAX_KIND_NAMESPACE_DECL
                            : SYNTAX_KIND_USING_DECL);
}

/* Asserts a recovered declaration carrying exactly one diagnostic. */
static void expect_malformed(bool use_namespace, const char *text,
                             SyntaxErrorCode code) {
  begin(text);
  ParserResult r =
      use_namespace ? parse_namespace_decl(g_parser, source_get_span(g_source))
                    : parse_using_decl(g_parser, source_get_span(g_source));

  CHECK(r.matched);      // recovered
  CHECK(r.node == NULL); // nothing worth keeping
  CHECK(error_count(&r) == 1);

  const SyntaxErrorList *e = r.errors;
  CHECK(e != NULL && e->error.code == code);
}

/* Asserts that text does not start this declaration at all. */
static void expect_not_match(bool use_namespace, const char *text) {
  begin(text);
  ParserResult r =
      use_namespace ? parse_namespace_decl(g_parser, source_get_span(g_source))
                    : parse_using_decl(g_parser, source_get_span(g_source));

  CHECK(!r.matched);
  CHECK(r.node == NULL);
  CHECK(r.errors == NULL); // convention: not-matched carries no list
}

static void test_namespace(void) {
  static const char *const ONE[] = {"std"};
  static const char *const TWO[] = {"std", "io"};
  static const char *const THREE[] = {"a", "b", "c"};

  expect_decl(true, "namespace std;", ONE, 1);
  expect_decl(true, "namespace std::io;", TWO, 2);
  expect_decl(true, "namespace a::b::c;", THREE, 3);
  // Trivia at the junctions of the path is part of the contract.
  expect_decl(true, "namespace std :: io ;", TWO, 2);
}

static void test_using(void) {
  static const char *const ONE[] = {"std"};
  static const char *const TWO[] = {"std", "io"};

  expect_decl(false, "using std;", ONE, 1);
  expect_decl(false, "using std::io;", TWO, 2);
  expect_decl(false, "using std :: io ;", TWO, 2);
}

static void test_malformed(void) {
  // Keyword hit but no path: SYNTAX_MALFORMED_NAMEPATH.
  expect_malformed(true, "namespace ;", SYNTAX_MALFORMED_NAMEPATH);
  expect_malformed(true, "namespace", SYNTAX_MALFORMED_NAMEPATH);
  expect_malformed(false, "using", SYNTAX_MALFORMED_NAMEPATH);

  // Path parsed but no ";": SYNTAX_EXPECTED_SEMICOLON.
  expect_malformed(true, "namespace std io", SYNTAX_EXPECTED_SEMICOLON);
  expect_malformed(true, "namespace std", SYNTAX_EXPECTED_SEMICOLON);
  // Best-prefix rollback: "::" is where ";" was expected.
  expect_malformed(true, "namespace a::", SYNTAX_EXPECTED_SEMICOLON);
  expect_malformed(false, "using a::;", SYNTAX_EXPECTED_SEMICOLON);
}

static void test_boundaries(void) {
  // Identifier boundary: "namespacex" is one identifier, not keyword +
  // identifier "x".
  expect_not_match(true, "namespacex");
  expect_not_match(false, "usingx");

  // Not these declarations at all.
  expect_not_match(true, "x");
  expect_not_match(false, "");
  expect_not_match(true, "");
}

static const TestEntry ENTRIES[] = {
    {"namespace", test_namespace},
    {"using", test_using},
    {"malformed", test_malformed},
    {"boundaries", test_boundaries},
};

TEST_MAIN(ENTRIES)
