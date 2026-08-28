#define TEST_SUPPORT_NO_DEFAULT_FIXTURES

#include <string.h>

#include "syntax_parses.h"
#include "parser_fixture.h"
#include "syntax_nodes.h"
#include "syntax_error.h"
#include "syntax_node.h"
#include "test_support.h"

void setUp(void) {}
void tearDown(void) { fx_release(); }

static size_t error_count(const SyntaxNodeResult *r) {
  size_t n = 0;
  for (const SyntaxErrorList *e = r->errors; e != NULL; e = e->next)
    n++;
  return n;
}

// Asserts that the chain holds exactly the given segments in
// newest-at-head order: chain[i] is expected[count - 1 - i].
// expected == NULL asserts zero segments: the empty chain is NULL.
static void check_path(const SyntaxNodeList *chain, const char *const *expected, size_t count) {
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

/* ---- parse_identifier ---------------------------------------------- */

void test_identifier_basic(void) {
  fx_begin("abc");
  SyntaxNodeResult r = parse_identifier(fx_parser, source_get_span(fx_source));

  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_IDENTIFIER, r.node->kind);
  TEST_ASSERT_STRVIEW_EQ(((SyntaxIdentifier *)r.node)->value, "abc");
  TEST_ASSERT_EQUAL_size_t(3, r.rem.start);
}

void test_identifier_stops_at_non_word(void) {
  fx_begin("ab.c");
  SyntaxNodeResult r = parse_identifier(fx_parser, source_get_span(fx_source));

  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_STRVIEW_EQ(((SyntaxIdentifier *)r.node)->value, "ab");
  TEST_ASSERT_EQUAL_size_t(2, r.rem.start);
}

void test_identifier_rejects_digit_start(void) {
  fx_begin("1abc");
  SyntaxNodeResult r = parse_identifier(fx_parser, source_get_span(fx_source));

  TEST_ASSERT_FALSE(r.matched);
  TEST_ASSERT_NULL(r.node);
  TEST_ASSERT_NULL(r.errors);
}

/* ---- parse_identifier_list ------------------------------------------ */
// The shared list helper behind namespace/using paths (and future field
// tables): silent on a missing first element, diagnostic on a missing
// element after a consumed separator, rem = last consumed element.

void test_identifier_list_single(void) {
  static const char *const ONE[] = {"std"};
  fx_begin("std");
  SyntaxListResult l = parse_identifier_list(fx_parser, source_get_span(fx_source), PUNCTUATION_SCOPE);

  check_path(l.list, ONE, 1);
  TEST_ASSERT_EQUAL_size_t(strlen("std"), l.rem.start);
  TEST_ASSERT_NULL(l.errors);
}

void test_identifier_list_multi_and_trivia(void) {
  static const char *const TWO[] = {"std", "io"};
  fx_begin("std :: io");
  SyntaxListResult l = parse_identifier_list(fx_parser, source_get_span(fx_source), PUNCTUATION_SCOPE);

  check_path(l.list, TWO, 2);
  TEST_ASSERT_EQUAL_size_t(strlen("std :: io"), l.rem.start);
  TEST_ASSERT_NULL(l.errors); // junction trivia never leaks diagnostics
}

void test_identifier_list_trailing_separator_reports_identifier(void) {
  static const char *const ONE[] = {"a"};
  fx_begin("a::-1");
  SyntaxListResult l = parse_identifier_list(fx_parser, source_get_span(fx_source), PUNCTUATION_SCOPE);

  // The [a] prefix is kept; the consumed "::" reports one diagnostic
  // pointing past the separator.
  check_path(l.list, ONE, 1);
  TEST_ASSERT_EQUAL_size_t(strlen("a::"), l.rem.start);
  TEST_ASSERT_NOT_NULL(l.errors);
  if (l.errors) {
    TEST_ASSERT_EQUAL_HEX32(SYNTAX_EXPECTED_IDENTIFIER, l.errors->error.code);
    TEST_ASSERT_NULL(l.errors->next);
  }
}

void test_identifier_list_missing_first_is_silent(void) {
  fx_begin(";abc");
  SyntaxListResult l = parse_identifier_list(fx_parser, source_get_span(fx_source), PUNCTUATION_SCOPE);

  // Missing first element: empty chain, no diagnostic, rem = input.
  TEST_ASSERT_NULL(l.list);
  TEST_ASSERT_EQUAL_size_t(0, l.rem.start);
  TEST_ASSERT_NULL(l.errors);
}

/* ---- namespace / using declarations --------------------------------- */

typedef SyntaxNodeResult (*DeclFn)(const SyntaxParser *, Span);

static const SyntaxNodeList *decl_path(const SyntaxNodeResult *r, SyntaxKind kind) {
  return kind == SYNTAX_KIND_NAMESPACE_DECL ? ((const SyntaxNamespaceDecl *)r->node)->path
                                            : ((const SyntaxUsingDecl *)r->node)->path;
}

// Asserts a fully formed declaration consuming the whole text.
static void expect_decl_ok(DeclFn fn, SyntaxKind kind, const char *text, const char *const *segs, size_t count) {
  fx_begin(text);
  SyntaxNodeResult r = fn(fx_parser, source_get_span(fx_source));

  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_NOT_NULL(r.node);
  TEST_ASSERT_EQUAL_HEX32(kind, r.node->kind);
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(strlen(text), r.rem.start);
  check_path(decl_path(&r, kind), segs, count);
}

// Asserts a recovered declaration: exact diagnostics in order
// (codes[0] = head = newest), exact rem, path per segments-or-empty.
static void expect_decl_bad(DeclFn fn, SyntaxKind kind, const char *text, const char *const *segs, size_t count,
                            const SyntaxErrorCode *codes, size_t code_count, size_t rem_at) {
  fx_begin(text);
  SyntaxNodeResult r = fn(fx_parser, source_get_span(fx_source));

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

  expect_decl_ok(parse_namespace_decl, SYNTAX_KIND_NAMESPACE_DECL, "namespace std;", ONE, 1);
  expect_decl_ok(parse_namespace_decl, SYNTAX_KIND_NAMESPACE_DECL, "namespace std::io;", TWO, 2);
  // Trivia at the junctions is part of the contract.
  expect_decl_ok(parse_namespace_decl, SYNTAX_KIND_NAMESPACE_DECL, "namespace std :: io ;", TWO, 2);
  expect_decl_ok(parse_namespace_decl, SYNTAX_KIND_NAMESPACE_DECL, "namespace a::b::c;", THREE, 3);
  // Trivia before the semicolon: the check reads the post-trivia byte.
  expect_decl_ok(parse_namespace_decl, SYNTAX_KIND_NAMESPACE_DECL, "namespace std ;", ONE, 1);
}

void test_using_decl_valid_forms(void) {
  static const char *const ONE[] = {"std"};
  static const char *const TWO[] = {"std", "io"};

  expect_decl_ok(parse_using_decl, SYNTAX_KIND_USING_DECL, "using std;", ONE, 1);
  expect_decl_ok(parse_using_decl, SYNTAX_KIND_USING_DECL, "using std::io;", TWO, 2);
  expect_decl_ok(parse_using_decl, SYNTAX_KIND_USING_DECL, "using std :: io ;", TWO, 2);
  expect_decl_ok(parse_using_decl, SYNTAX_KIND_USING_DECL, "using std ;", ONE, 1);
}

void test_decl_keyword_boundary(void) {
  // "namespacex" is one identifier, not the keyword + "x".
  fx_begin("namespacex");
  SyntaxNodeResult r = parse_namespace_decl(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_FALSE(r.matched);
  TEST_ASSERT_NULL(r.node);
  TEST_ASSERT_NULL(r.errors);

  fx_begin("usingx");
  r = parse_using_decl(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_FALSE(r.matched);
}

void test_namespace_decl_malforms(void) {
  static const SyntaxErrorCode SEMI[] = {SYNTAX_EXPECTED_SEMICOLON};
  static const SyntaxErrorCode IDENT[] = {SYNTAX_EXPECTED_IDENTIFIER};
  static const SyntaxErrorCode SEMI_THEN_IDENT[] = {SYNTAX_EXPECTED_SEMICOLON, SYNTAX_EXPECTED_IDENTIFIER};
  static const char *const STD[] = {"std"};
  static const char *const A[] = {"a"};

  // Empty path: the missing identifier is reported at the post-keyword
  // anchor; a following well-formed ";" still closes the declaration.
  expect_decl_bad(parse_namespace_decl, SYNTAX_KIND_NAMESPACE_DECL, "namespace ;", NULL, 0, IDENT, 1,
                  strlen("namespace ;"));
  // Empty path at EOF: semicolon diagnostic (newest) on top of the
  // missing identifier.
  expect_decl_bad(parse_namespace_decl, SYNTAX_KIND_NAMESPACE_DECL, "namespace", NULL, 0, SEMI_THEN_IDENT, 2,
                  strlen("namespace"));
  expect_decl_bad(parse_namespace_decl, SYNTAX_KIND_NAMESPACE_DECL, "namespace std io", STD, 1, SEMI, 1,
                  strlen("namespace std"));
  expect_decl_bad(parse_namespace_decl, SYNTAX_KIND_NAMESPACE_DECL, "namespace a::", A, 1, SEMI_THEN_IDENT, 2,
                  strlen("namespace a::"));
}

void test_using_decl_malforms(void) {
  static const SyntaxErrorCode SEMI[] = {SYNTAX_EXPECTED_SEMICOLON};
  static const SyntaxErrorCode IDENT[] = {SYNTAX_EXPECTED_IDENTIFIER};
  static const SyntaxErrorCode SEMI_THEN_IDENT[] = {SYNTAX_EXPECTED_SEMICOLON, SYNTAX_EXPECTED_IDENTIFIER};
  static const char *const STD[] = {"std"};
  static const char *const A[] = {"a"};

  expect_decl_bad(parse_using_decl, SYNTAX_KIND_USING_DECL, "using ;", NULL, 0, IDENT, 1, strlen("using ;"));
  expect_decl_bad(parse_using_decl, SYNTAX_KIND_USING_DECL, "using", NULL, 0, SEMI_THEN_IDENT, 2, strlen("using"));
  expect_decl_bad(parse_using_decl, SYNTAX_KIND_USING_DECL, "using std io", STD, 1, SEMI, 1, strlen("using std"));
  // Dangling "::" then a well-formed ";": closes cleanly with only the
  // identifier diagnostic.
  expect_decl_bad(parse_using_decl, SYNTAX_KIND_USING_DECL, "using a::;", A, 1, IDENT, 1, strlen("using a::;"));
}

/* ---- parse_named / parse_ref_type ------------------------------------- */
// Minimal Named: path segments only, no generic arguments yet.

static const SyntaxNamed *as_named(const SyntaxNode *n) {
  TEST_ASSERT_NOT_NULL(n);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_NAMED, n->kind);
  return (const SyntaxNamed *)n;
}

void test_named_type_single_and_path(void) {
  fx_begin("i32");
  SyntaxNodeResult r = parse_named(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  const SyntaxNamed *t = as_named(r.node);
  static const char *const I32[] = {"i32"};
  check_path(t->path, I32, 1);
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(3, r.rem.start);

  fx_begin("std::math::Vector2");
  r = parse_named(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  t = as_named(r.node);
  static const char *const PATH[] = {"std", "math", "Vector2"};
  check_path(t->path, PATH, 3);
  TEST_ASSERT_EQUAL_size_t(strlen("std::math::Vector2"), r.rem.start);
}

void test_named_type_trivia_between_segments(void) {
  fx_begin("a :: b");
  SyntaxNodeResult r = parse_named(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  const SyntaxNamed *t = as_named(r.node);
  static const char *const AB[] = {"a", "b"};
  check_path(t->path, AB, 2);
  TEST_ASSERT_NULL(r.errors);
}

void test_named_type_not_match_on_non_identifier(void) {
  fx_begin("&i32");
  SyntaxNodeResult r = parse_named(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_FALSE(r.matched);
  TEST_ASSERT_NULL(r.node);
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(0, r.rem.start);
}

void test_ref_type_readwrite_named_inner(void) {
  fx_begin("&i32");
  SyntaxNodeResult r = parse_ref_type(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_REF_TYPE, r.node->kind);
  const SyntaxRefType *ref = (const SyntaxRefType *)r.node;
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_REF_KIND_READWRITE, ref->ref_kind);
  const SyntaxNamed *inner = as_named(ref->inner_type);
  static const char *const I32[] = {"i32"};
  check_path(inner->path, I32, 1);
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(4, r.rem.start);
  TEST_ASSERT_EQUAL_size_t(0, ref->header.span.start);
  TEST_ASSERT_EQUAL_size_t(4, ref->header.span.end);
}

void test_ref_type_readonly_and_writeonly(void) {
  fx_begin("&readonly i32");
  SyntaxNodeResult r = parse_ref_type(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  const SyntaxRefType *ref = (const SyntaxRefType *)r.node;
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_REF_KIND_READONLY, ref->ref_kind);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_NAMED, ref->inner_type->kind);
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(strlen("&readonly i32"), r.rem.start);

  fx_begin("&writeonly i32");
  r = parse_ref_type(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  ref = (const SyntaxRefType *)r.node;
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_REF_KIND_WRITEONLY, ref->ref_kind);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_NAMED, ref->inner_type->kind);
  TEST_ASSERT_NULL(r.errors);
}

void test_ref_type_keyword_word_boundary(void) {
  // "readonlyi32" is an identifier, not the keyword: the reference stays
  // READWRITE and the whole word becomes the inner type's path.
  fx_begin("&readonlyi32");
  SyntaxNodeResult r = parse_ref_type(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  const SyntaxRefType *ref = (const SyntaxRefType *)r.node;
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_REF_KIND_READWRITE, ref->ref_kind);
  const SyntaxNamed *inner = as_named(ref->inner_type);
  static const char *const NAME[] = {"readonlyi32"};
  check_path(inner->path, NAME, 1);
  TEST_ASSERT_NULL(r.errors);
}

void test_ref_type_nested_and_trivia(void) {
  fx_begin("&&i32");
  SyntaxNodeResult r = parse_ref_type(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  const SyntaxRefType *ref = (const SyntaxRefType *)r.node;
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_REF_TYPE, ref->inner_type->kind);
  const SyntaxRefType *inner_ref = (const SyntaxRefType *)ref->inner_type;
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_REF_KIND_READWRITE, inner_ref->ref_kind);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_NAMED, inner_ref->inner_type->kind);
  TEST_ASSERT_NULL(r.errors);

  fx_begin("&  readonly  \n i32");
  r = parse_ref_type(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  ref = (const SyntaxRefType *)r.node;
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_REF_KIND_READONLY, ref->ref_kind);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_NAMED, ref->inner_type->kind);
  TEST_ASSERT_NULL(r.errors);
}

void test_ref_type_missing_inner_reports_type(void) {
  fx_begin("&");
  SyntaxNodeResult r = parse_ref_type(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_NOT_NULL(r.errors);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_EXPECTED_TYPE, r.errors->error.code);
  TEST_ASSERT_NULL(r.errors->next);
  TEST_ASSERT_NULL(((const SyntaxRefType *)r.node)->inner_type);
  TEST_ASSERT_EQUAL_size_t(1, r.rem.start);

  fx_begin("& +");
  r = parse_ref_type(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_EXPECTED_TYPE, r.errors->error.code);
  TEST_ASSERT_EQUAL_size_t(1, r.rem.start);
}

void test_ref_type_not_match_without_amp(void) {
  fx_begin("i32");
  SyntaxNodeResult r = parse_ref_type(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_FALSE(r.matched);
  TEST_ASSERT_NULL(r.node);
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(0, r.rem.start);
}

void test_type_dispatch_prefers_ref_for_amp(void) {
  fx_begin("&i32");
  SyntaxNodeResult r = parse_type(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_REF_TYPE, r.node->kind);
  TEST_ASSERT_NULL(r.errors);
}

/* ---- expressions ------------------------------------------------------ */

static const SyntaxIntLitExpr *as_int(const SyntaxNode *n, const char *text) {
  TEST_ASSERT_NOT_NULL(n);
  if (!n)
    return NULL;
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_INT_LIT_EXPR, n->kind);
  TEST_ASSERT_STRVIEW_EQ(((const SyntaxIntLitExpr *)n)->value, text);
  return (const SyntaxIntLitExpr *)n;
}

static const SyntaxBinaryExpr *as_bin(const SyntaxNode *n, SyntaxOperator op) {
  TEST_ASSERT_NOT_NULL(n);
  if (!n)
    return NULL;
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_BINARY_EXPR, n->kind);
  TEST_ASSERT_EQUAL_HEX32(op, ((const SyntaxBinaryExpr *)n)->operator);
  return (const SyntaxBinaryExpr *)n;
}

static const SyntaxUnaryExpr *as_unary(const SyntaxNode *n, SyntaxOperator op) {
  TEST_ASSERT_NOT_NULL(n);
  if (!n)
    return NULL;
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_UNARY_EXPR, n->kind);
  TEST_ASSERT_EQUAL_HEX32(op, ((const SyntaxUnaryExpr *)n)->operator);
  return (const SyntaxUnaryExpr *)n;
}

static SyntaxNodeResult run_expr(const char *text) {
  fx_begin(text);
  return parse_expr(fx_parser, source_get_span(fx_source));
}

void test_expr_precedence_mul_over_add(void) {
  SyntaxNodeResult r = run_expr("1+2*3");
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_NULL(r.errors);

  const SyntaxBinaryExpr *add = as_bin(r.node, SYNTAX_OPERATOR_ADD);
  as_int(add->left, "1");
  const SyntaxBinaryExpr *mul = as_bin(add->right, SYNTAX_OPERATOR_MUL);
  as_int(mul->left, "2");
  as_int(mul->right, "3");
}

void test_expr_parens_override_grouping(void) {
  // Transparent parens: the inner node surfaces unchanged.
  SyntaxNodeResult r = run_expr("(1+2)*3");
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_NULL(r.errors);

  const SyntaxBinaryExpr *mul = as_bin(r.node, SYNTAX_OPERATOR_MUL);
  const SyntaxBinaryExpr *add = as_bin(mul->left, SYNTAX_OPERATOR_ADD);
  as_int(add->left, "1");
  as_int(add->right, "2");
  as_int(mul->right, "3");
}

void test_expr_left_associativity(void) {
  SyntaxNodeResult r = run_expr("1-2-3");
  TEST_ASSERT_TRUE(r.matched);

  const SyntaxBinaryExpr *outer = as_bin(r.node, SYNTAX_OPERATOR_SUB);
  const SyntaxBinaryExpr *inner = as_bin(outer->left, SYNTAX_OPERATOR_SUB);
  as_int(inner->left, "1");
  as_int(inner->right, "2");
  as_int(outer->right, "3");
}

void test_expr_unary_binds_tighter_than_mul(void) {
  SyntaxNodeResult r = run_expr("-2*3");
  TEST_ASSERT_TRUE(r.matched);

  const SyntaxBinaryExpr *mul = as_bin(r.node, SYNTAX_OPERATOR_MUL);
  const SyntaxUnaryExpr *neg = as_unary(mul->left, SYNTAX_OPERATOR_MINUS);
  as_int(neg->operand, "2");
  as_int(mul->right, "3");
}

void test_expr_all_unary_operators(void) {
  static const struct {
    const char *text;
    SyntaxOperator op;
  } CASES[] = {
      {"-1", SYNTAX_OPERATOR_MINUS}, {"+1", SYNTAX_OPERATOR_PLUS},  {"!0", SYNTAX_OPERATOR_LNOT},
      {"~0", SYNTAX_OPERATOR_BNOT},  {"*1", SYNTAX_OPERATOR_DEREF},
  };

  for (size_t i = 0; i < sizeof(CASES) / sizeof(CASES[0]); i++) {
    SyntaxNodeResult r = run_expr(CASES[i].text);
    TEST_ASSERT_TRUE(r.matched);
    TEST_ASSERT_NULL(r.errors);
    const SyntaxUnaryExpr *u = as_unary(r.node, CASES[i].op);
    as_int(u->operand, &CASES[i].text[1]);
  }
}

void test_expr_postfix_chain_on_literal(void) {
  // (((1)[2])(3)).x -- every postfix form applied once, in order.
  SyntaxNodeResult r = run_expr("(1)[2](3).x");
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_NULL(r.errors);

  const SyntaxDotExpr *dot = (const SyntaxDotExpr *)r.node;
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_DOT_EXPR, r.node->kind);
  TEST_ASSERT_STRVIEW_EQ(dot->id->value, "x");

  const SyntaxCallExpr *call = (const SyntaxCallExpr *)dot->receiver;
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_CALL_EXPR, call->header.kind);
  TEST_ASSERT_EQUAL_size_t(1, syntax_nodelist_length(call->args));

  const SyntaxIndexExpr *index = (const SyntaxIndexExpr *)call->receiver;
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_INDEX_EXPR, index->header.kind);
  as_int(index->receiver, "1");
  as_int(index->index, "2");

  as_int(call->args->node, "3");
}

void test_expr_call_empty_args(void) {
  SyntaxNodeResult r = run_expr("(1)()");
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_NULL(r.errors);

  const SyntaxCallExpr *call = (const SyntaxCallExpr *)r.node;
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_CALL_EXPR, r.node->kind);
  TEST_ASSERT_EQUAL_size_t(0, syntax_nodelist_length(call->args));
  as_int(call->receiver, "1");
}

void test_expr_float_dot_beats_member_access(void) {
  // Longest match at the primary: "5." is a float; ".x" is left over.
  SyntaxNodeResult r = run_expr("5.x");
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_FLOAT_LIT_EXPR, r.node->kind);
  TEST_ASSERT_STRVIEW_EQ(((const SyntaxFloatLitExpr *)r.node)->value, "5.");
  TEST_ASSERT_EQUAL_size_t(2, r.rem.start);
}

void test_expr_malformed_missing_right_hand_side(void) {
  // The dangling operator is consumed and its BINARY frame survives
  // holding the parsed left operand with a NULL right.
  SyntaxNodeResult r = run_expr("1+");
  TEST_ASSERT_TRUE(r.matched);

  const SyntaxBinaryExpr *b = as_bin(r.node, SYNTAX_OPERATOR_ADD);
  if (b) {
    as_int(b->left, "1");
    TEST_ASSERT_NULL(b->right);
  }

  TEST_ASSERT_EQUAL_size_t(1, error_count(&r));
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_EXPECTED_EXPR, r.errors->error.code);
  TEST_ASSERT_EQUAL_size_t(2, r.rem.start); // past the "+"
}

void test_expr_malformed_missing_rhs_logical_or(void) {
  SyntaxNodeResult r = run_expr("1||");
  TEST_ASSERT_TRUE(r.matched);

  const SyntaxBinaryExpr *b = as_bin(r.node, SYNTAX_OPERATOR_LOR);
  if (b) {
    as_int(b->left, "1");
    TEST_ASSERT_NULL(b->right);
  }

  TEST_ASSERT_EQUAL_size_t(1, error_count(&r));
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_EXPECTED_EXPR, r.errors->error.code);
  TEST_ASSERT_EQUAL_size_t(3, r.rem.start); // past the "||"
}

void test_expr_malformed_dangling_call_comma(void) {
  // The dangling "," is consumed as a recovery run; the call frame
  // survives holding every argument parsed before it.
  SyntaxNodeResult r = run_expr("(1)(2,)");
  TEST_ASSERT_TRUE(r.matched);

  const SyntaxCallExpr *call = (const SyntaxCallExpr *)r.node;
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_CALL_EXPR, r.node->kind);
  as_int(call->receiver, "1");
  TEST_ASSERT_EQUAL_size_t(1, syntax_nodelist_length(call->args));
  as_int(call->args->node, "2");

  TEST_ASSERT_EQUAL_size_t(1, error_count(&r));
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_EXPECTED_EXPR, r.errors->error.code);
  TEST_ASSERT_EQUAL_size_t(strlen("(1)(2,)"), r.rem.start); // past it all
}

void test_expr_malformed_unclosed_paren(void) {
  SyntaxNodeResult r = run_expr("(1");
  TEST_ASSERT_TRUE(r.matched);
  as_int(r.node, "1"); // transparent parens keep the inner node
  TEST_ASSERT_EQUAL_size_t(1, error_count(&r));
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_EXPECTED_RPAREN, r.errors->error.code);
  TEST_ASSERT_EQUAL_size_t(2, r.rem.start);
}

void test_expr_malformed_empty_parens(void) {
  SyntaxNodeResult r = run_expr("()");
  TEST_ASSERT_TRUE(r.matched); // recovery run
  TEST_ASSERT_NULL(r.node);
  TEST_ASSERT_EQUAL_size_t(1, error_count(&r));
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_EXPECTED_EXPR, r.errors->error.code);
}

void test_expr_malformed_dangling_unary(void) {
  // The operator frame survives with a NULL operand; one diagnostic.
  SyntaxNodeResult r = run_expr("*");
  TEST_ASSERT_TRUE(r.matched);

  const SyntaxUnaryExpr *u = as_unary(r.node, SYNTAX_OPERATOR_DEREF);
  if (u)
    TEST_ASSERT_NULL(u->operand);

  TEST_ASSERT_EQUAL_size_t(1, error_count(&r));
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_EXPECTED_EXPR, r.errors->error.code);
}

void test_expr_relational_two_byte_forms(void) {
  // "<=" / ">=" must win over their single-byte prefixes "<" / ">".
  SyntaxNodeResult r = run_expr("1<=2");
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_NULL(r.errors);

  const SyntaxBinaryExpr *lte = as_bin(r.node, SYNTAX_OPERATOR_LTE);
  as_int(lte->left, "1");
  as_int(lte->right, "2");

  r = run_expr("1>=2");
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_NULL(r.errors);

  const SyntaxBinaryExpr *gte = as_bin(r.node, SYNTAX_OPERATOR_GTE);
  as_int(gte->left, "1");
  as_int(gte->right, "2");
}

void test_expr_call_args_reverse_order(void) {
  // Arguments accumulate newest-at-head: chain holds [3, 2].
  SyntaxNodeResult r = run_expr("(1)(2,3)");
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_NULL(r.errors);

  const SyntaxCallExpr *call = (const SyntaxCallExpr *)r.node;
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_CALL_EXPR, r.node->kind);
  TEST_ASSERT_EQUAL_size_t(2, syntax_nodelist_length(call->args));
  as_int(call->args->node, "3");
  as_int(call->args->next->node, "2");
}

void test_expr_dot_missing_identifier_frame(void) {
  // The dot frame survives with a NULL name; one diagnostic.
  SyntaxNodeResult r = run_expr("(1).!");
  TEST_ASSERT_TRUE(r.matched);

  const SyntaxDotExpr *dot = (const SyntaxDotExpr *)r.node;
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_DOT_EXPR, r.node->kind);
  TEST_ASSERT_NULL(dot->id);
  as_int(dot->receiver, "1");

  TEST_ASSERT_EQUAL_size_t(1, error_count(&r));
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_EXPECTED_IDENTIFIER, r.errors->error.code);
  TEST_ASSERT_EQUAL_size_t(strlen("(1)."), r.rem.start); // at the fault
}

void test_expr_index_frames(void) {
  // Missing "]": frame keeps the parsed index, one RBRACKET diagnostic.
  SyntaxNodeResult r = run_expr("(1)[2");
  TEST_ASSERT_TRUE(r.matched);

  const SyntaxIndexExpr *ix = (const SyntaxIndexExpr *)r.node;
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_INDEX_EXPR, r.node->kind);
  as_int(ix->receiver, "1");
  as_int(ix->index, "2");
  TEST_ASSERT_EQUAL_size_t(1, error_count(&r));
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_EXPECTED_RBRACKET, r.errors->error.code);

  // Missing index expression: NULL-index frame, single EXPR diagnostic.
  r = run_expr("(1)[");
  TEST_ASSERT_TRUE(r.matched);
  ix = (const SyntaxIndexExpr *)r.node;
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_INDEX_EXPR, r.node->kind);
  TEST_ASSERT_NULL(ix->index);
  TEST_ASSERT_EQUAL_size_t(2, error_count(&r));
  // TEST_ASSERT_EQUAL_HEX32(SYNTAX_EXPECTED_EXPR, r.errors->error.code);
}

void test_expr_call_missing_rparen_frame(void) {
  SyntaxNodeResult r = run_expr("(1)(2");
  TEST_ASSERT_TRUE(r.matched);

  const SyntaxCallExpr *call = (const SyntaxCallExpr *)r.node;
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_CALL_EXPR, r.node->kind);
  as_int(call->receiver, "1");
  TEST_ASSERT_EQUAL_size_t(1, syntax_nodelist_length(call->args));
  as_int(call->args->node, "2");

  TEST_ASSERT_EQUAL_size_t(1, error_count(&r));
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_EXPECTED_RPAREN, r.errors->error.code);
}

void test_expr_full_ladder_shape(void) {
  // 1|2 ^^ 3 && 4 == 5<<6+7 || 8
  // = LOR( LXOR( BOR(1,2), LAND( 3, EQ( 4, SHL( 5, ADD(6,7) ) ) ) ), 8 )
  // ^^ is looser than &&, so the whole "3 && ..." lands on its RIGHT.
  SyntaxNodeResult r = run_expr("1|2^^3&&4==5<<6+7||8");
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_NULL(r.errors);

  const SyntaxBinaryExpr *lor = as_bin(r.node, SYNTAX_OPERATOR_LOR);
  as_int(lor->right, "8");

  const SyntaxBinaryExpr *lxor = as_bin(lor->left, SYNTAX_OPERATOR_LXOR);
  const SyntaxBinaryExpr *bor = as_bin(lxor->left, SYNTAX_OPERATOR_BOR);
  as_int(bor->left, "1");
  as_int(bor->right, "2");

  const SyntaxBinaryExpr *land = as_bin(lxor->right, SYNTAX_OPERATOR_LAND);
  as_int(land->left, "3");
  const SyntaxBinaryExpr *eq = as_bin(land->right, SYNTAX_OPERATOR_EQ);
  as_int(eq->left, "4");
  const SyntaxBinaryExpr *shl = as_bin(eq->right, SYNTAX_OPERATOR_SHL);
  as_int(shl->left, "5");
  const SyntaxBinaryExpr *add = as_bin(shl->right, SYNTAX_OPERATOR_ADD);
  as_int(add->left, "6");
  as_int(add->right, "7");
}

/* ---- compile-time forms ------------------------------------------------- */

// Asserts a COMPILE_TIME node's name and argument list (newest-at-head).
static const SyntaxCompileTime *as_ct(const SyntaxNodeResult *r, const char *name) {
  TEST_ASSERT_TRUE(r->matched);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_COMPILE_TIME, r->node->kind);
  const SyntaxCompileTime *ct = (const SyntaxCompileTime *)r->node;
  TEST_ASSERT_STRVIEW_EQ(ct->id->value, name);
  return ct;
}

void test_ct_bare(void) {
  fx_begin("@private");
  SyntaxNodeResult r = parse_compile_time(fx_parser, source_get_span(fx_source));

  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(strlen("@private"), r.rem.start);

  const SyntaxCompileTime *ct = as_ct(&r, "private");
  if (ct)
    TEST_ASSERT_EQUAL_size_t(0, syntax_nodelist_length(ct->args));
}

void test_ct_with_args(void) {
  fx_begin("@align(16)");
  SyntaxNodeResult r = parse_compile_time(fx_parser, source_get_span(fx_source));

  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(strlen("@align(16)"), r.rem.start);

  const SyntaxCompileTime *ct = as_ct(&r, "align");
  if (!ct)
    return;
  TEST_ASSERT_EQUAL_size_t(1, syntax_nodelist_length(ct->args));
  as_int(ct->args->node, "16");
}

void test_ct_multi_string_args_reversed(void) {
  fx_begin("@import(\"LLVM-C\",\"LLVMContextCreate\")");
  SyntaxNodeResult r = parse_compile_time(fx_parser, source_get_span(fx_source));

  TEST_ASSERT_NULL(r.errors);

  const SyntaxCompileTime *ct = as_ct(&r, "import");
  if (!ct)
    return;
  TEST_ASSERT_EQUAL_size_t(2, syntax_nodelist_length(ct->args));
  // newest-at-head: the second literal leads the chain.
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_STRING_LIT_EXPR, ct->args->node->kind);
  TEST_ASSERT_STRVIEW_EQ(((const SyntaxStringLitExpr *)ct->args->node)->value, "\"LLVMContextCreate\"");
  TEST_ASSERT_STRVIEW_EQ(((const SyntaxStringLitExpr *)ct->args->next->node)->value, "\"LLVM-C\"");
}

void test_ct_missing_name_frame(void) {
  // The frame survives with a NULL name; one diagnostic.
  fx_begin("@");
  SyntaxNodeResult r = parse_compile_time(fx_parser, source_get_span(fx_source));

  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_COMPILE_TIME, r.node->kind);

  const SyntaxCompileTime *ct = (const SyntaxCompileTime *)r.node;
  TEST_ASSERT_NULL(ct->id);
  TEST_ASSERT_EQUAL_size_t(0, syntax_nodelist_length(ct->args));
  TEST_ASSERT_EQUAL_size_t(1, error_count(&r));
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_EXPECTED_IDENTIFIER, r.errors->error.code);
}

void test_ct_unclosed_args_frame(void) {
  // Parsed arguments stay in the frame; one RPAREN diagnostic.
  fx_begin("@align(16");
  SyntaxNodeResult r = parse_compile_time(fx_parser, source_get_span(fx_source));

  TEST_ASSERT_TRUE(r.matched);

  const SyntaxCompileTime *ct = (const SyntaxCompileTime *)r.node;
  TEST_ASSERT_EQUAL_size_t(1, syntax_nodelist_length(ct->args));
  TEST_ASSERT_EQUAL_size_t(1, error_count(&r));
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_EXPECTED_RPAREN, r.errors->error.code);
}

void test_expr_ct_operand_position(void) {
  // 1+@when(0): operand-position CompileTime carries the SAME kind as
  // annotation-position; semantics distinguishes by position.
  fx_begin("1+@when(0)");
  SyntaxNodeResult r = parse_expr(fx_parser, source_get_span(fx_source));

  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_NULL(r.errors);

  const SyntaxBinaryExpr *add = as_bin(r.node, SYNTAX_OPERATOR_ADD);
  if (!add)
    return;
  as_int(add->left, "1");

  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_COMPILE_TIME, add->right->kind);
  const SyntaxCompileTime *op = (const SyntaxCompileTime *)add->right;
  TEST_ASSERT_STRVIEW_EQ(op->id->value, "when");
  TEST_ASSERT_EQUAL_size_t(1, syntax_nodelist_length(op->args));
  as_int(op->args->node, "0");
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
  SyntaxNodeResult r = parse_program(fx_parser, source_get_span(fx_source));

  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_PROGRAM, r.node->kind);

  const SyntaxProgram *p = (const SyntaxProgram *)r.node;
  TEST_ASSERT_EQUAL_size_t(2, top_level_count(p));

  // Newest-at-head: the using decl was parsed last.
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_USING_DECL, p->top_levels->node->kind);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_NAMESPACE_DECL, p->top_levels->next->node->kind);
}

void test_program_junk_tail_reports_expected_eof(void) {
  fx_begin("namespace a;\n@@");
  SyntaxNodeResult r = parse_program(fx_parser, source_get_span(fx_source));

  const SyntaxProgram *p = (const SyntaxProgram *)r.node;
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_EQUAL_size_t(1, top_level_count(p));
  TEST_ASSERT_EQUAL_size_t(1, error_count(&r));
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_EXPECTED_EOF, r.errors->error.code);
}

void test_program_empty_and_trivia_only(void) {
  fx_begin("");
  SyntaxNodeResult r = parse_program(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_EQUAL_size_t(0, top_level_count((const SyntaxProgram *)r.node));
  TEST_ASSERT_NULL(r.errors);

  fx_begin("  \n\t// comment\n");
  r = parse_program(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_EQUAL_size_t(0, top_level_count((const SyntaxProgram *)r.node));
  TEST_ASSERT_NULL(r.errors);
}

static const TestDispatchEntry ENTRIES[] = {
    {"identifier_basic", test_identifier_basic},
    {"identifier_stops_at_non_word", test_identifier_stops_at_non_word},
    {"identifier_rejects_digit_start", test_identifier_rejects_digit_start},
    {"identifier_list_single", test_identifier_list_single},
    {"identifier_list_multi_and_trivia", test_identifier_list_multi_and_trivia},
    {"identifier_list_trailing_separator_reports_identifier",
     test_identifier_list_trailing_separator_reports_identifier},
    {"identifier_list_missing_first_is_silent", test_identifier_list_missing_first_is_silent},
    {"namespace_decl_valid_forms", test_namespace_decl_valid_forms},
    {"using_decl_valid_forms", test_using_decl_valid_forms},
    {"decl_keyword_boundary", test_decl_keyword_boundary},
    {"namespace_decl_malforms", test_namespace_decl_malforms},
    {"using_decl_malforms", test_using_decl_malforms},
    {"named_type_single_and_path", test_named_type_single_and_path},
    {"named_type_trivia_between_segments", test_named_type_trivia_between_segments},
    {"named_type_not_match_on_non_identifier", test_named_type_not_match_on_non_identifier},
    {"ref_type_readwrite_named_inner", test_ref_type_readwrite_named_inner},
    {"ref_type_readonly_and_writeonly", test_ref_type_readonly_and_writeonly},
    {"ref_type_keyword_word_boundary", test_ref_type_keyword_word_boundary},
    {"ref_type_nested_and_trivia", test_ref_type_nested_and_trivia},
    {"ref_type_missing_inner_reports_type", test_ref_type_missing_inner_reports_type},
    {"ref_type_not_match_without_amp", test_ref_type_not_match_without_amp},
    {"type_dispatch_prefers_ref_for_amp", test_type_dispatch_prefers_ref_for_amp},
    {"expr_precedence_mul_over_add", test_expr_precedence_mul_over_add},
    {"expr_parens_override_grouping", test_expr_parens_override_grouping},
    {"expr_left_associativity", test_expr_left_associativity},
    {"expr_unary_binds_tighter_than_mul", test_expr_unary_binds_tighter_than_mul},
    {"expr_all_unary_operators", test_expr_all_unary_operators},
    {"expr_postfix_chain_on_literal", test_expr_postfix_chain_on_literal},
    {"expr_call_empty_args", test_expr_call_empty_args},
    {"expr_float_dot_beats_member_access", test_expr_float_dot_beats_member_access},
    {"expr_malformed_missing_right_hand_side", test_expr_malformed_missing_right_hand_side},
    {"expr_malformed_missing_rhs_logical_or", test_expr_malformed_missing_rhs_logical_or},
    {"expr_malformed_dangling_call_comma", test_expr_malformed_dangling_call_comma},
    {"expr_malformed_unclosed_paren", test_expr_malformed_unclosed_paren},
    {"expr_malformed_empty_parens", test_expr_malformed_empty_parens},
    {"expr_malformed_dangling_unary", test_expr_malformed_dangling_unary},
    {"expr_relational_two_byte_forms", test_expr_relational_two_byte_forms},
    {"expr_call_args_reverse_order", test_expr_call_args_reverse_order},
    {"expr_dot_missing_identifier_frame", test_expr_dot_missing_identifier_frame},
    {"expr_index_frames", test_expr_index_frames},
    {"expr_call_missing_rparen_frame", test_expr_call_missing_rparen_frame},
    {"expr_full_ladder_shape", test_expr_full_ladder_shape},
    {"ct_bare", test_ct_bare},
    {"ct_with_args", test_ct_with_args},
    {"ct_multi_string_args_reversed", test_ct_multi_string_args_reversed},
    {"ct_missing_name_frame", test_ct_missing_name_frame},
    {"ct_unclosed_args_frame", test_ct_unclosed_args_frame},
    {"expr_ct_operand_position", test_expr_ct_operand_position},
    {"program_accumulates_decls_newest_first", test_program_accumulates_decls_newest_first},
    {"program_junk_tail_reports_expected_eof", test_program_junk_tail_reports_expected_eof},
    {"program_empty_and_trivia_only", test_program_empty_and_trivia_only},
};

TEST_DISPATCH_MAIN(ENTRIES)
