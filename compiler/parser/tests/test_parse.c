#define TEST_SUPPORT_NO_DEFAULT_FIXTURES

#include <string.h>

#include "parser.h"
#include "parser_fixture.h"
#include "syntax_error.h"
#include "syntax_node.h"
#include "test_support.h"

void setUp(void) {}
void tearDown(void) { fx_release(); }

static size_t error_count(const ParserResult *r) {
  size_t n = 0;
  for (const SyntaxErrorList *e = r->errors; e != NULL; e = e->next)
    n++;
  return n;
}

// Asserts that the chain holds exactly the given segments in
// newest-at-head order: chain[i] is expected[count - 1 - i].
static void check_path(const SyntaxNamePath *path,
                       const char *const *expected, size_t count) {
  if (expected == NULL) {
    TEST_ASSERT_NULL(path); // decl carries no path at all
    return;
  }

  TEST_ASSERT_NOT_NULL(path);
  if (!path)
    return;
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_NAME_PATH, path->header.kind);

  const SyntaxNodeList *n = path->segments;
  for (size_t i = 0; i < count; i++) {
    TEST_ASSERT_NOT_NULL(n);
    if (!n)
      return;

    const char *want = expected[count - 1 - i];
    const SyntaxIdentifier *id = (const SyntaxIdentifier *)n->node;
    TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_IDENTIFIER, id->header.kind);
    TEST_ASSERT_STRVIEW_EQ(id->strview, want);
    n = n->next;
  }
  TEST_ASSERT_NULL(n); // exactly @p count segments
}

/* ---- parse_identifier ---------------------------------------------- */

void test_identifier_basic(void) {
  fx_begin("abc");
  ParserResult r = parse_identifier(fx_parser, source_get_span(fx_source));

  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_IDENTIFIER, r.node->kind);
  TEST_ASSERT_STRVIEW_EQ(((SyntaxIdentifier *)r.node)->strview, "abc");
  TEST_ASSERT_EQUAL_size_t(3, r.rem.start);
}

void test_identifier_stops_at_non_word(void) {
  fx_begin("ab.c");
  ParserResult r = parse_identifier(fx_parser, source_get_span(fx_source));

  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_STRVIEW_EQ(((SyntaxIdentifier *)r.node)->strview, "ab");
  TEST_ASSERT_EQUAL_size_t(2, r.rem.start);
}

void test_identifier_rejects_digit_start(void) {
  fx_begin("1abc");
  ParserResult r = parse_identifier(fx_parser, source_get_span(fx_source));

  TEST_ASSERT_FALSE(r.matched);
  TEST_ASSERT_NULL(r.node);
  TEST_ASSERT_NULL(r.errors);
}

/* ---- parse_name_path ------------------------------------------------ */

void test_name_path_single(void) {
  static const char *const ONE[] = {"std"};
  fx_begin("std");
  ParserResult r = parse_name_path(fx_parser, source_get_span(fx_source));

  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(strlen("std"), r.rem.start);
  check_path((const SyntaxNamePath *)r.node, ONE, 1);
}

void test_name_path_multi_and_trivia(void) {
  static const char *const TWO[] = {"std", "io"};
  fx_begin("std :: io");
  ParserResult r = parse_name_path(fx_parser, source_get_span(fx_source));

  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_NULL(r.errors); // junction trivia never leaks diagnostics
  TEST_ASSERT_EQUAL_size_t(strlen("std :: io"), r.rem.start);
  check_path((const SyntaxNamePath *)r.node, TWO, 2);
}

void test_name_path_trailing_separator_reports_identifier(void) {
  static const char *const ONE[] = {"a"};
  fx_begin("a::-1");
  ParserResult r = parse_name_path(fx_parser, source_get_span(fx_source));

  // The [a] prefix is kept; the consumed "::" reports one diagnostic
  // pointing past the separator.
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_NOT_NULL(r.node);
  TEST_ASSERT_EQUAL_size_t(strlen("a::"), r.rem.start);
  TEST_ASSERT_EQUAL_size_t(1, error_count(&r));
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_EXPECTED_IDENTIFIER, r.errors->error.code);
  check_path((const SyntaxNamePath *)r.node, ONE, 1);
}

void test_name_path_missing_first_segment_is_silent_not_match(void) {
  fx_begin(";abc");
  ParserResult r = parse_name_path(fx_parser, source_get_span(fx_source));

  TEST_ASSERT_FALSE(r.matched);
  TEST_ASSERT_NULL(r.node);
  TEST_ASSERT_NULL(r.errors); // convention: not-match carries no list
}

/* ---- namespace / using declarations --------------------------------- */

typedef ParserResult (*DeclFn)(const Parser *, Span);

static const SyntaxNamePath *decl_path(const ParserResult *r,
                                       SyntaxKind kind) {
  return kind == SYNTAX_KIND_NAMESPACE_DECL
             ? ((const SyntaxNamespaceDecl *)r->node)->path
             : ((const SyntaxUsingDecl *)r->node)->path;
}

// Asserts a fully formed declaration consuming the whole text.
static void expect_decl_ok(DeclFn fn, SyntaxKind kind, const char *text,
                           const char *const *segs, size_t count) {
  fx_begin(text);
  ParserResult r = fn(fx_parser, source_get_span(fx_source));

  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_NOT_NULL(r.node);
  TEST_ASSERT_EQUAL_HEX32(kind, r.node->kind);
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(strlen(text), r.rem.start);
  check_path(decl_path(&r, kind), segs, count);
}

// Asserts a recovered declaration: exact diagnostics in order
// (codes[0] = head = newest), exact rem, path per segments-or-NULL.
static void expect_decl_bad(DeclFn fn, SyntaxKind kind, const char *text,
                            const char *const *segs, size_t count,
                            const SyntaxErrorCode *codes, size_t code_count,
                            size_t rem_at) {
  fx_begin(text);
  ParserResult r = fn(fx_parser, source_get_span(fx_source));

  TEST_ASSERT_TRUE(r.matched); // recovered
  TEST_ASSERT_NOT_NULL(r.node);
  TEST_ASSERT_EQUAL_HEX32(kind, r.node->kind);
  TEST_ASSERT_EQUAL_size_t(rem_at, r.rem.start);

  TEST_ASSERT_EQUAL_size_t(code_count, error_count(&r));
  const SyntaxErrorList *e = r.errors;
  for (size_t i = 0; i < code_count; i++) {
    TEST_ASSERT_NOT_NULL(e);
    if (!e)
      return;
    TEST_ASSERT_EQUAL_HEX32(codes[i], e->error.code);
    e = e->next;
  }
  TEST_ASSERT_NULL(e);

  check_path(decl_path(&r, kind), segs, count);
}

void test_namespace_decl_valid_forms(void) {
  static const char *const ONE[] = {"std"};
  static const char *const TWO[] = {"std", "io"};
  static const char *const THREE[] = {"a", "b", "c"};

  expect_decl_ok(parse_namespace_decl, SYNTAX_KIND_NAMESPACE_DECL,
                 "namespace std;", ONE, 1);
  expect_decl_ok(parse_namespace_decl, SYNTAX_KIND_NAMESPACE_DECL,
                 "namespace std::io;", TWO, 2);
  // Trivia at the junctions is part of the contract.
  expect_decl_ok(parse_namespace_decl, SYNTAX_KIND_NAMESPACE_DECL,
                 "namespace std :: io ;", TWO, 2);
  expect_decl_ok(parse_namespace_decl, SYNTAX_KIND_NAMESPACE_DECL,
                 "namespace a::b::c;", THREE, 3);
  // Trivia before the semicolon: the check reads the post-trivia byte.
  expect_decl_ok(parse_namespace_decl, SYNTAX_KIND_NAMESPACE_DECL,
                 "namespace std ;", ONE, 1);
}

void test_using_decl_valid_forms(void) {
  static const char *const ONE[] = {"std"};
  static const char *const TWO[] = {"std", "io"};

  expect_decl_ok(parse_using_decl, SYNTAX_KIND_USING_DECL, "using std;", ONE,
                 1);
  expect_decl_ok(parse_using_decl, SYNTAX_KIND_USING_DECL, "using std::io;",
                 TWO, 2);
  expect_decl_ok(parse_using_decl, SYNTAX_KIND_USING_DECL,
                 "using std :: io ;", TWO, 2);
  expect_decl_ok(parse_using_decl, SYNTAX_KIND_USING_DECL, "using std ;", ONE,
                 1);
}

void test_decl_keyword_boundary(void) {
  // "namespacex" is one identifier, not the keyword + "x".
  fx_begin("namespacex");
  ParserResult r =
      parse_namespace_decl(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_FALSE(r.matched);
  TEST_ASSERT_NULL(r.node);
  TEST_ASSERT_NULL(r.errors);

  fx_begin("usingx");
  r = parse_using_decl(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_FALSE(r.matched);
}

void test_namespace_decl_malforms(void) {
  static const SyntaxErrorCode NAME_PATH[] = {SYNTAX_EXPECTED_NAME_PATH};
  static const SyntaxErrorCode SEMI[] = {SYNTAX_EXPECTED_SEMICOLON};
  static const SyntaxErrorCode SEMI_THEN_IDENT[] = {
      SYNTAX_EXPECTED_SEMICOLON, SYNTAX_EXPECTED_IDENTIFIER};
  static const char *const STD[] = {"std"};
  static const char *const A[] = {"a"};

  expect_decl_bad(parse_namespace_decl, SYNTAX_KIND_NAMESPACE_DECL,
                  "namespace ;", NULL, 0, NAME_PATH, 1,
                  strlen("namespace"));
  expect_decl_bad(parse_namespace_decl, SYNTAX_KIND_NAMESPACE_DECL,
                  "namespace", NULL, 0, NAME_PATH, 1, strlen("namespace"));
  expect_decl_bad(parse_namespace_decl, SYNTAX_KIND_NAMESPACE_DECL,
                  "namespace std io", STD, 1, SEMI, 1,
                  strlen("namespace std"));
  expect_decl_bad(parse_namespace_decl, SYNTAX_KIND_NAMESPACE_DECL,
                  "namespace a::", A, 1, SEMI_THEN_IDENT, 2,
                  strlen("namespace a::"));
}

void test_using_decl_malforms(void) {
  static const SyntaxErrorCode NAME_PATH[] = {SYNTAX_EXPECTED_NAME_PATH};
  static const SyntaxErrorCode SEMI[] = {SYNTAX_EXPECTED_SEMICOLON};
  static const SyntaxErrorCode IDENT[] = {SYNTAX_EXPECTED_IDENTIFIER};
  static const char *const STD[] = {"std"};
  static const char *const A[] = {"a"};

  expect_decl_bad(parse_using_decl, SYNTAX_KIND_USING_DECL, "using", NULL, 0,
                  NAME_PATH, 1, strlen("using"));
  expect_decl_bad(parse_using_decl, SYNTAX_KIND_USING_DECL, "using std io",
                  STD, 1, SEMI, 1, strlen("using std"));
  // Dangling "::" then a well-formed ";": closes cleanly with only the
  // identifier diagnostic.
  expect_decl_bad(parse_using_decl, SYNTAX_KIND_USING_DECL, "using a::;", A,
                  1, IDENT, 1, strlen("using a::;"));
}

/* ---- parse_program --------------------------------------------------- */

static size_t top_level_count(const SyntaxProgram *p) {
  size_t n = 0;
  for (const SyntaxNodeList *i = p->top_levels; i != NULL; i = i->next)
    n++;
  return n;
}

void test_program_accumulates_decls_newest_first(void) {
  fx_begin("namespace a;\nusing b;\n");
  ParserResult r = parse_program(fx_parser, source_get_span(fx_source));

  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_PROGRAM, r.node->kind);

  const SyntaxProgram *p = (const SyntaxProgram *)r.node;
  TEST_ASSERT_EQUAL_size_t(2, top_level_count(p));

  // Newest-at-head: the using decl was parsed last.
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_USING_DECL,
                          p->top_levels->node->kind);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_NAMESPACE_DECL,
                          p->top_levels->next->node->kind);
}

void test_program_junk_tail_reports_expected_eof(void) {
  fx_begin("namespace a;\n@@");
  ParserResult r = parse_program(fx_parser, source_get_span(fx_source));

  const SyntaxProgram *p = (const SyntaxProgram *)r.node;
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_EQUAL_size_t(1, top_level_count(p));
  TEST_ASSERT_EQUAL_size_t(1, error_count(&r));
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_EXPECTED_EOF, r.errors->error.code);
}

void test_program_empty_and_trivia_only(void) {
  fx_begin("");
  ParserResult r = parse_program(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_EQUAL_size_t(
      0, top_level_count((const SyntaxProgram *)r.node));
  TEST_ASSERT_NULL(r.errors);

  fx_begin("  \n\t// comment\n");
  r = parse_program(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_EQUAL_size_t(
      0, top_level_count((const SyntaxProgram *)r.node));
  TEST_ASSERT_NULL(r.errors);
}

static const TestDispatchEntry ENTRIES[] = {
    {"identifier_basic", test_identifier_basic},
    {"identifier_stops_at_non_word", test_identifier_stops_at_non_word},
    {"identifier_rejects_digit_start", test_identifier_rejects_digit_start},
    {"name_path_single", test_name_path_single},
    {"name_path_multi_and_trivia", test_name_path_multi_and_trivia},
    {"name_path_trailing_separator_reports_identifier",
     test_name_path_trailing_separator_reports_identifier},
    {"name_path_missing_first_segment_is_silent_not_match",
     test_name_path_missing_first_segment_is_silent_not_match},
    {"namespace_decl_valid_forms", test_namespace_decl_valid_forms},
    {"using_decl_valid_forms", test_using_decl_valid_forms},
    {"decl_keyword_boundary", test_decl_keyword_boundary},
    {"namespace_decl_malforms", test_namespace_decl_malforms},
    {"using_decl_malforms", test_using_decl_malforms},
    {"program_accumulates_decls_newest_first",
     test_program_accumulates_decls_newest_first},
    {"program_junk_tail_reports_expected_eof",
     test_program_junk_tail_reports_expected_eof},
    {"program_empty_and_trivia_only", test_program_empty_and_trivia_only},
};

TEST_DISPATCH_MAIN(ENTRIES)
