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

/* Asserts that the path segments of a parsed decl match expectations.
 * A NULL @p segments asserts that the decl carries no path at all. */
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

  if (segments == NULL) {
    CHECK(path == NULL);
    return;
  }

  CHECK(path != NULL);
  if (!path)
    return;

  CHECK(path->header.kind == SYNTAX_KIND_NAME_PATH);

  // Lists accumulate newest-at-head, so the chain holds the path in
  // reverse source order: chain[i] corresponds to
  // segments[count - 1 - i].
  const SyntaxNodeList *n = path->segments;
  for (size_t i = 0; i < count; i++) {
    CHECK(n != NULL);
    if (!n)
      return;

    const char *expected = segments[count - 1 - i];
    const SyntaxIdentifier *id = (const SyntaxIdentifier *)n->node;
    CHECK(id->header.kind == SYNTAX_KIND_IDENTIFIER);
    CHECK(strview_equals(id->strview,
                         strview_create((const uint8_t *)expected,
                                        strlen(expected))));
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

/* Asserts a recovered declaration whose diagnostics are exactly
 * @p codes in order (codes[0] is the head, i.e. the newest), whose
 * path holds exactly @p segments (or is NULL when @p segments is
 * NULL), and which consumed up to @p rem_at bytes. */
static void expect_bad(bool use_namespace, const char *text,
                       const char *const *segments, size_t count,
                       const SyntaxErrorCode *codes, size_t code_count,
                       size_t rem_at) {
  begin(text);
  ParserResult r =
      use_namespace ? parse_namespace_decl(g_parser, source_get_span(g_source))
                    : parse_using_decl(g_parser, source_get_span(g_source));

  CHECK(r.matched); // recovered

  SyntaxKind kind =
      use_namespace ? SYNTAX_KIND_NAMESPACE_DECL : SYNTAX_KIND_USING_DECL;
  CHECK(r.node != NULL && r.node->kind == kind); // decl shell always kept
  CHECK(r.rem.start == rem_at);

  // Diagnostics: newest-at-head, exact multiset in exact order.
  CHECK(error_count(&r) == code_count);
  const SyntaxErrorList *e = r.errors;
  for (size_t i = 0; i < code_count; i++) {
    CHECK(e != NULL && e->error.code == codes[i]);
    if (!e)
      return;
    e = e->next;
  }
  CHECK(e == NULL);

  check_paths(&r, segments, count,
              use_namespace ? SYNTAX_KIND_NAMESPACE_DECL
                            : SYNTAX_KIND_USING_DECL);
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
  // Trivia before the semicolon: the check reads the post-trivia byte.
  expect_decl(true, "namespace std ;", ONE, 1);
}

static void test_using(void) {
  static const char *const ONE[] = {"std"};
  static const char *const TWO[] = {"std", "io"};

  expect_decl(false, "using std;", ONE, 1);
  expect_decl(false, "using std::io;", TWO, 2);
  expect_decl(false, "using std :: io ;", TWO, 2);
  expect_decl(false, "using std ;", ONE, 1);
}

static void test_malformed(void) {
  static const char *const NONE_PATH[] = {"std"};
  static const char *const A[] = {"a"};

  static const SyntaxErrorCode NAME_PATH[] = {SYNTAX_EXPECTED_NAME_PATH};
  static const SyntaxErrorCode SEMI[] = {SYNTAX_EXPECTED_SEMICOLON};
  static const SyntaxErrorCode IDENT[] = {SYNTAX_EXPECTED_IDENTIFIER};
  static const SyntaxErrorCode SEMI_THEN_IDENT[] = {
      SYNTAX_EXPECTED_SEMICOLON, SYNTAX_EXPECTED_IDENTIFIER};

  // Keyword hit but no usable path: decl shell with NULL path.
  expect_bad(true, "namespace ;", NULL, 0, NAME_PATH, 1, strlen("namespace"));
  expect_bad(true, "namespace", NULL, 0, NAME_PATH, 1, strlen("namespace"));
  expect_bad(false, "using", NULL, 0, NAME_PATH, 1, strlen("using"));

  // Path parsed but no ";": one EXPECTED_SEMICOLON.
  expect_bad(true, "namespace std io", NONE_PATH, 1, SEMI, 1,
             strlen("namespace std"));
  expect_bad(true, "namespace std", NONE_PATH, 1, SEMI, 1,
             strlen("namespace std"));

  // Trailing "::" is consumed by the path and reports
  // EXPECTED_IDENTIFIER; here it also ate the input, so the missing
  // ";" stacks a second diagnostic on top (newest first).
  expect_bad(true, "namespace a::", A, 1, SEMI_THEN_IDENT, 2,
             strlen("namespace a::"));

  // Same dangling "::", but a well-formed ";" follows: the declaration
  // closes cleanly and carries only the identifier diagnostic.
  expect_bad(false, "using a::;", A, 1, IDENT, 1, strlen("using a::;"));
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
