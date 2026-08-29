#define TEST_SUPPORT_NO_DEFAULT_FIXTURES

#include <string.h>

#include "parser_fixture.h"
#include "syntax_error.h"
#include "syntax_node.h"
#include "syntax_nodes.h"
#include "syntax_parses.h"
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

/* ---- parse_named_type / parse_ref_type -------------------------------- */
// Minimal Named: path segments only, no generic arguments yet.

static const SyntaxNamed *as_named(const SyntaxNode *n) {
  TEST_ASSERT_NOT_NULL(n);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_NAMED, n->kind);
  return (const SyntaxNamed *)n;
}

void test_named_type_single_and_path(void) {
  fx_begin("i32");
  SyntaxNodeResult r = parse_named_type(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  const SyntaxNamed *t = as_named(r.node);
  static const char *const I32[] = {"i32"};
  check_path(t->path, I32, 1);
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(3, r.rem.start);

  fx_begin("std::math::Vector2");
  r = parse_named_type(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  t = as_named(r.node);
  static const char *const PATH[] = {"std", "math", "Vector2"};
  check_path(t->path, PATH, 3);
  TEST_ASSERT_EQUAL_size_t(strlen("std::math::Vector2"), r.rem.start);
}

void test_named_type_trivia_between_segments(void) {
  fx_begin("a :: b");
  SyntaxNodeResult r = parse_named_type(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  const SyntaxNamed *t = as_named(r.node);
  static const char *const AB[] = {"a", "b"};
  check_path(t->path, AB, 2);
  TEST_ASSERT_NULL(r.errors);
}

void test_named_type_not_match_on_non_identifier(void) {
  fx_begin("&i32");
  SyntaxNodeResult r = parse_named_type(fx_parser, source_get_span(fx_source));
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

/* ---- decl helpers ------------------------------------------------------ */

static size_t error_chain_length(const SyntaxErrorList *e) {
  size_t n = 0;
  for (; e != NULL; e = e->next)
    n++;
  return n;
}

static const SyntaxCompileTime *as_ct_node(const SyntaxNode *n) {
  TEST_ASSERT_NOT_NULL(n);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_COMPILE_TIME, n->kind);
  return (const SyntaxCompileTime *)n;
}

void test_annotations_none(void) {
  fx_begin("foo");
  SyntaxListResult l = parse_annotations(fx_parser, source_get_span(fx_source));

  TEST_ASSERT_NULL(l.list); // zero annotations: empty chain
  TEST_ASSERT_NULL(l.errors);
  TEST_ASSERT_EQUAL_size_t(0, l.rem.start);
}

void test_annotations_single_and_multi(void) {
  fx_begin("@a @b(1) foo");
  SyntaxListResult l = parse_annotations(fx_parser, source_get_span(fx_source));

  TEST_ASSERT_EQUAL_size_t(2, syntax_nodelist_length(l.list));
  TEST_ASSERT_STRVIEW_EQ(as_ct_node(l.list->node)->id->value, "b");
  TEST_ASSERT_STRVIEW_EQ(as_ct_node(l.list->next->node)->id->value, "a");
  TEST_ASSERT_NULL(l.errors);
  // Trivia before the next non-annotation stays with the enclosing sequence.
  TEST_ASSERT_EQUAL_size_t(strlen("@a @b(1)"), l.rem.start);
}

void test_annotations_error_frame(void) {
  // A bare "@" keeps its frame (node with no name) and one diagnostic.
  fx_begin("@");
  SyntaxListResult l = parse_annotations(fx_parser, source_get_span(fx_source));

  TEST_ASSERT_EQUAL_size_t(1, syntax_nodelist_length(l.list));
  TEST_ASSERT_NULL(as_ct_node(l.list->node)->id);
  TEST_ASSERT_EQUAL_size_t(1, error_chain_length(l.errors));
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_EXPECTED_IDENTIFIER, l.errors->error.code);
  TEST_ASSERT_EQUAL_size_t(1, l.rem.start);
}

void test_generic_param_bare_and_bound(void) {
  fx_begin("T");
  SyntaxNodeResult r = parse_generic_param(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  const SyntaxGenericParam *p = (const SyntaxGenericParam *)r.node;
  TEST_ASSERT_STRVIEW_EQ(p->id->value, "T");
  TEST_ASSERT_NULL(p->type);
  TEST_ASSERT_TRUE(syntax_nodelist_is_empty(p->annotations));
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(1, r.rem.start);

  fx_begin("T : i32");
  r = parse_generic_param(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  p = (const SyntaxGenericParam *)r.node;
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_NAMED, p->type->kind);
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(strlen("T : i32"), r.rem.start);
}

void test_generic_param_annotated(void) {
  fx_begin("@align(8) T: i32");
  SyntaxNodeResult r = parse_generic_param(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  const SyntaxGenericParam *p = (const SyntaxGenericParam *)r.node;
  TEST_ASSERT_EQUAL_size_t(1, syntax_nodelist_length(p->annotations));
  TEST_ASSERT_STRVIEW_EQ(p->id->value, "T");
  TEST_ASSERT_NOT_NULL(p->type);
  TEST_ASSERT_NULL(r.errors);
}

void test_call_param_requires_colon_and_type(void) {
  fx_begin("x: i32");
  SyntaxNodeResult r = parse_call_param(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  const SyntaxCallParam *p = (const SyntaxCallParam *)r.node;
  TEST_ASSERT_STRVIEW_EQ(p->id->value, "x");
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_NAMED, p->type->kind);
  TEST_ASSERT_NULL(r.errors);

  // No colon and no type: the frame survives with two diagnostics.
  fx_begin("x");
  r = parse_call_param(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  p = (const SyntaxCallParam *)r.node;
  TEST_ASSERT_STRVIEW_EQ(p->id->value, "x");
  TEST_ASSERT_NULL(p->type);
  TEST_ASSERT_EQUAL_size_t(2, error_chain_length(r.errors));
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_EXPECTED_TYPE, r.errors->error.code);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_EXPECTED_COLON, r.errors->next->error.code);
}

void test_call_param_annotated(void) {
  fx_begin("@intrinsic x : i32");
  SyntaxNodeResult r = parse_call_param(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  const SyntaxCallParam *p = (const SyntaxCallParam *)r.node;
  TEST_ASSERT_EQUAL_size_t(1, syntax_nodelist_length(p->annotations));
  TEST_ASSERT_STRVIEW_EQ(p->id->value, "x");
  TEST_ASSERT_NOT_NULL(p->type);
  TEST_ASSERT_NULL(r.errors);
}

/* ---- let declaration --------------------------------------------------- */

void test_let_decl_three_forms(void) {
  // Value only, through the dispatch: parse_decl picks the let branch.
  fx_begin("let PI = 3.1415926;");
  SyntaxNodeResult r = parse_decl(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_LET_DECL, r.node->kind);
  const SyntaxLetDecl *d = (const SyntaxLetDecl *)r.node;
  TEST_ASSERT_STRVIEW_EQ(d->id->value, "PI");
  TEST_ASSERT_NULL(d->type);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_FLOAT_LIT_EXPR, d->value->kind);
  TEST_ASSERT_TRUE(syntax_nodelist_is_empty(d->annotations));
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(strlen("let PI = 3.1415926;"), r.rem.start);

  // Both type and value.
  fx_begin("let PI: f64 = 3.1415926;");
  r = parse_let_decl(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  d = (const SyntaxLetDecl *)r.node;
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_NAMED, d->type->kind);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_FLOAT_LIT_EXPR, d->value->kind);
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(strlen("let PI: f64 = 3.1415926;"), r.rem.start);

  // Type only.
  fx_begin("let TMP: i32;");
  r = parse_let_decl(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  d = (const SyntaxLetDecl *)r.node;
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_NAMED, d->type->kind);
  TEST_ASSERT_NULL(d->value);
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(strlen("let TMP: i32;"), r.rem.start);

  // Annotations ride along (doc example).
  fx_begin("@import(\"COUNT\") let COUNT: usize;");
  r = parse_let_decl(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  d = (const SyntaxLetDecl *)r.node;
  TEST_ASSERT_EQUAL_size_t(1, syntax_nodelist_length(d->annotations));
  TEST_ASSERT_STRVIEW_EQ(d->id->value, "COUNT");
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_NAMED, d->type->kind);
  TEST_ASSERT_NULL(d->value);
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(strlen("@import(\"COUNT\") let COUNT: usize;"), r.rem.start);
}

void test_let_decl_malforms(void) {
  // Neither type nor value: one diagnostic for the likelier intent.
  fx_begin("let x;");
  SyntaxNodeResult r = parse_let_decl(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  const SyntaxLetDecl *d = (const SyntaxLetDecl *)r.node;
  TEST_ASSERT_STRVIEW_EQ(d->id->value, "x");
  TEST_ASSERT_NULL(d->type);
  TEST_ASSERT_NULL(d->value);
  TEST_ASSERT_EQUAL_size_t(1, error_chain_length(r.errors));
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_EXPECTED_EQUALS, r.errors->error.code);
  TEST_ASSERT_EQUAL_size_t(strlen("let x;"), r.rem.start);

  // Missing ";".
  fx_begin("let x = 1");
  r = parse_let_decl(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_NOT_NULL(((const SyntaxLetDecl *)r.node)->value);
  TEST_ASSERT_EQUAL_size_t(1, error_chain_length(r.errors));
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_EXPECTED_SEMICOLON, r.errors->error.code);
  TEST_ASSERT_EQUAL_size_t(strlen("let x = 1"), r.rem.start);

  // Missing identifier still binds the value.
  fx_begin("let = 1;");
  r = parse_let_decl(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  d = (const SyntaxLetDecl *)r.node;
  TEST_ASSERT_NULL(d->id);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_INT_LIT_EXPR, d->value->kind);
  TEST_ASSERT_EQUAL_size_t(1, error_chain_length(r.errors));
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_EXPECTED_IDENTIFIER, r.errors->error.code);

  // Dangling ":" reports the missing type and still closes.
  fx_begin("let x : ;");
  r = parse_let_decl(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  d = (const SyntaxLetDecl *)r.node;
  TEST_ASSERT_NULL(d->type);
  TEST_ASSERT_NULL(d->value);
  TEST_ASSERT_EQUAL_size_t(1, error_chain_length(r.errors));
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_EXPECTED_TYPE, r.errors->error.code);
  TEST_ASSERT_EQUAL_size_t(strlen("let x : ;"), r.rem.start);
}

/* ---- struct / union declarations --------------------------------------- */

void test_struct_decl_bare_and_empty_body(void) {
  // Bare form, through the dispatch.
  fx_begin("struct Foo;");
  SyntaxNodeResult r = parse_decl(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_STRUCT_DECL, r.node->kind);
  const SyntaxStructDecl *d = (const SyntaxStructDecl *)r.node;
  TEST_ASSERT_STRVIEW_EQ(d->id->value, "Foo");
  TEST_ASSERT_TRUE(syntax_nodelist_is_empty(d->generic_params));
  TEST_ASSERT_TRUE(syntax_nodelist_is_empty(d->fields));
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(strlen("struct Foo;"), r.rem.start);

  // Empty braced body.
  fx_begin("struct Foo {}");
  r = parse_struct_decl(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_TRUE(syntax_nodelist_is_empty(((const SyntaxStructDecl *)r.node)->fields));
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(strlen("struct Foo {}"), r.rem.start);
}

void test_struct_decl_fields_and_trailing_comma(void) {
  fx_begin("struct Vector2F { x: f32, y: f32 }");
  SyntaxNodeResult r = parse_struct_decl(fx_parser, source_get_span(fx_source));
  const SyntaxStructDecl *d = (const SyntaxStructDecl *)r.node;
  TEST_ASSERT_EQUAL_size_t(2, syntax_nodelist_length(d->fields));
  // Newest-at-head: y leads.
  TEST_ASSERT_STRVIEW_EQ(((const SyntaxStructField *)d->fields->node)->id->value, "y");
  TEST_ASSERT_STRVIEW_EQ(((const SyntaxStructField *)d->fields->next->node)->id->value, "x");
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_NAMED, ((const SyntaxStructField *)d->fields->node)->type->kind);
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(strlen("struct Vector2F { x: f32, y: f32 }"), r.rem.start);

  // A trailing comma hands the brace back cleanly.
  fx_begin("struct V { x: f32, }");
  r = parse_struct_decl(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_EQUAL_size_t(1, syntax_nodelist_length(((const SyntaxStructDecl *)r.node)->fields));
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(strlen("struct V { x: f32, }"), r.rem.start);
}

void test_struct_decl_generics(void) {
  fx_begin("@pack(4) struct Vector2<T> { x: T, y: T }");
  SyntaxNodeResult r = parse_struct_decl(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  const SyntaxStructDecl *d = (const SyntaxStructDecl *)r.node;
  TEST_ASSERT_EQUAL_size_t(1, syntax_nodelist_length(d->annotations));
  TEST_ASSERT_EQUAL_size_t(1, syntax_nodelist_length(d->generic_params));
  TEST_ASSERT_STRVIEW_EQ(((const SyntaxGenericParam *)d->generic_params->node)->id->value, "T");
  TEST_ASSERT_EQUAL_size_t(2, syntax_nodelist_length(d->fields));
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_NAMED, ((const SyntaxStructField *)d->fields->node)->type->kind);
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(strlen("@pack(4) struct Vector2<T> { x: T, y: T }"), r.rem.start);
}

void test_struct_decl_generics_malforms(void) {
  // Empty "<>" reports the missing parameter and still closes.
  fx_begin("struct V<> { x: i32 }");
  SyntaxNodeResult r = parse_struct_decl(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_TRUE(syntax_nodelist_is_empty(((const SyntaxStructDecl *)r.node)->generic_params));
  TEST_ASSERT_EQUAL_size_t(1, error_chain_length(r.errors));
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_EXPECTED_IDENTIFIER, r.errors->error.code);

  // Missing ">": the clause frame survives and the body still parses.
  fx_begin("struct V<T { x: i32 }");
  r = parse_struct_decl(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_EQUAL_size_t(1, syntax_nodelist_length(((const SyntaxStructDecl *)r.node)->generic_params));
  TEST_ASSERT_EQUAL_size_t(1, error_chain_length(r.errors));
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_EXPECTED_GT, r.errors->error.code);
}

void test_struct_field_frames(void) {
  // Missing colon: the frame survives holding the type.
  fx_begin("struct V { x i32 }");
  SyntaxNodeResult r = parse_struct_decl(fx_parser, source_get_span(fx_source));
  const SyntaxStructDecl *d = (const SyntaxStructDecl *)r.node;
  const SyntaxStructField *f = (const SyntaxStructField *)d->fields->node;
  TEST_ASSERT_STRVIEW_EQ(f->id->value, "x");
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_NAMED, f->type->kind);
  TEST_ASSERT_EQUAL_size_t(1, error_chain_length(r.errors));
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_EXPECTED_COLON, r.errors->error.code);

  // Missing type: the frame survives with a NULL type.
  fx_begin("struct V { x: }");
  r = parse_struct_decl(fx_parser, source_get_span(fx_source));
  d = (const SyntaxStructDecl *)r.node;
  f = (const SyntaxStructField *)d->fields->node;
  TEST_ASSERT_STRVIEW_EQ(f->id->value, "x");
  TEST_ASSERT_NULL(f->type);
  TEST_ASSERT_EQUAL_size_t(1, error_chain_length(r.errors));
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_EXPECTED_TYPE, r.errors->error.code);
}

void test_struct_decl_body_malforms(void) {
  // Neither ";" nor "{": one DECL_BODY diagnostic, rem at the header end.
  fx_begin("struct Foo");
  SyntaxNodeResult r = parse_struct_decl(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_EQUAL_size_t(1, error_chain_length(r.errors));
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_EXPECTED_DECL_BODY, r.errors->error.code);
  TEST_ASSERT_EQUAL_size_t(strlen("struct Foo"), r.rem.start);

  // Missing "}": the fields frame survives.
  fx_begin("struct V { x: f32");
  r = parse_struct_decl(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_EQUAL_size_t(1, syntax_nodelist_length(((const SyntaxStructDecl *)r.node)->fields));
  TEST_ASSERT_EQUAL_size_t(1, error_chain_length(r.errors));
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_EXPECTED_RBRACE, r.errors->error.code);
  TEST_ASSERT_EQUAL_size_t(strlen("struct V { x: f32"), r.rem.start);
}

void test_union_decl_forms(void) {
  fx_begin("union U;");
  SyntaxNodeResult r = parse_decl(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_UNION_DECL, r.node->kind);
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(strlen("union U;"), r.rem.start);

  fx_begin("union FooUnion<T> { value: T, ptr: &T }");
  r = parse_union_decl(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  const SyntaxUnionDecl *d = (const SyntaxUnionDecl *)r.node;
  TEST_ASSERT_EQUAL_size_t(1, syntax_nodelist_length(d->generic_params));
  TEST_ASSERT_EQUAL_size_t(2, syntax_nodelist_length(d->fields));
  // The reference-typed field keeps REF_TYPE with a NAMED inner type.
  const SyntaxUnionField *ptr = (const SyntaxUnionField *)d->fields->node; // newest-at-head
  TEST_ASSERT_STRVIEW_EQ(ptr->id->value, "ptr");
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_REF_TYPE, ptr->type->kind);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_NAMED, ((const SyntaxRefType *)ptr->type)->inner_type->kind);
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(strlen("union FooUnion<T> { value: T, ptr: &T }"), r.rem.start);
}

/* ---- enum / variant declarations --------------------------------------- */

void test_enum_decl_forms(void) {
  fx_begin("enum Color;");
  SyntaxNodeResult r = parse_decl(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_ENUM_DECL, r.node->kind);
  const SyntaxEnumDecl *d = (const SyntaxEnumDecl *)r.node;
  TEST_ASSERT_STRVIEW_EQ(d->id->value, "Color");
  TEST_ASSERT_NULL(d->behind_type);
  TEST_ASSERT_TRUE(syntax_nodelist_is_empty(d->fields));
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(strlen("enum Color;"), r.rem.start);

  fx_begin("enum Color { Red, Green, Blue }");
  r = parse_enum_decl(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  d = (const SyntaxEnumDecl *)r.node;
  TEST_ASSERT_EQUAL_size_t(3, syntax_nodelist_length(d->fields));
  TEST_ASSERT_STRVIEW_EQ(((const SyntaxEnumField *)d->fields->node)->id->value, "Blue");
  TEST_ASSERT_NULL(((const SyntaxEnumField *)d->fields->node)->value);
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(strlen("enum Color { Red, Green, Blue }"), r.rem.start);

  fx_begin("enum SomeFlag: u32 { A = 0x0001_u32, B = 0x0002_u32 }");
  r = parse_enum_decl(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  d = (const SyntaxEnumDecl *)r.node;
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_NAMED, d->behind_type->kind);
  TEST_ASSERT_EQUAL_size_t(2, syntax_nodelist_length(d->fields));
  const SyntaxEnumField *f = (const SyntaxEnumField *)d->fields->node;
  TEST_ASSERT_STRVIEW_EQ(f->id->value, "B");
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_INT_LIT_EXPR, f->value->kind);
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(strlen("enum SomeFlag: u32 { A = 0x0001_u32, B = 0x0002_u32 }"), r.rem.start);
}

void test_enum_decl_trailing_comma(void) {
  fx_begin("enum Color { Red, }");
  SyntaxNodeResult r = parse_enum_decl(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_EQUAL_size_t(1, syntax_nodelist_length(((const SyntaxEnumDecl *)r.node)->fields));
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(strlen("enum Color { Red, }"), r.rem.start);
}

void test_enum_decl_field_malform(void) {
  fx_begin("enum C { A = }");
  SyntaxNodeResult r = parse_enum_decl(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  const SyntaxEnumDecl *d = (const SyntaxEnumDecl *)r.node;
  TEST_ASSERT_EQUAL_size_t(1, syntax_nodelist_length(d->fields));
  TEST_ASSERT_NULL(((const SyntaxEnumField *)d->fields->node)->value);
  TEST_ASSERT_EQUAL_size_t(1, error_chain_length(r.errors));
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_EXPECTED_EXPR, r.errors->error.code);
  TEST_ASSERT_EQUAL_size_t(strlen("enum C { A = }"), r.rem.start);
}

void test_variant_decl_forms(void) {
  fx_begin("variant Option;");
  SyntaxNodeResult r = parse_decl(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_VARIANT_DECL, r.node->kind);
  TEST_ASSERT_NULL(r.errors);

  fx_begin("variant Option<T> { None, Value: T }");
  r = parse_variant_decl(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  const SyntaxVariantDecl *d = (const SyntaxVariantDecl *)r.node;
  TEST_ASSERT_STRVIEW_EQ(d->id->value, "Option");
  TEST_ASSERT_EQUAL_size_t(1, syntax_nodelist_length(d->generic_params));
  TEST_ASSERT_EQUAL_size_t(2, syntax_nodelist_length(d->fields));
  TEST_ASSERT_STRVIEW_EQ(((const SyntaxVariantField *)d->fields->node)->id->value, "Value");
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_NAMED, ((const SyntaxVariantField *)d->fields->node)->type->kind);
  TEST_ASSERT_NULL(((const SyntaxVariantField *)d->fields->next->node)->type);
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(strlen("variant Option<T> { None, Value: T }"), r.rem.start);

  fx_begin("variant State: u8 { Idle, Run, }");
  r = parse_variant_decl(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  d = (const SyntaxVariantDecl *)r.node;
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_NAMED, d->behind_type->kind);
  TEST_ASSERT_EQUAL_size_t(2, syntax_nodelist_length(d->fields));
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(strlen("variant State: u8 { Idle, Run, }"), r.rem.start);
}

/* ---- contract declaration ---------------------------------------------- */

void test_contract_decl_forms(void) {
  fx_begin("contract Addable<TLeft, TRight, TResult>(left: TLeft, right: TRight): TResult;");
  SyntaxNodeResult r = parse_decl(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_CONTRACT_DECL, r.node->kind);
  const SyntaxContractDecl *d = (const SyntaxContractDecl *)r.node;
  TEST_ASSERT_STRVIEW_EQ(d->id->value, "Addable");
  TEST_ASSERT_EQUAL_size_t(3, syntax_nodelist_length(d->generic_params));
  TEST_ASSERT_EQUAL_size_t(2, syntax_nodelist_length(d->call_params));
  TEST_ASSERT_STRVIEW_EQ(((const SyntaxCallParam *)d->call_params->node)->id->value, "right");
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_NAMED, ((const SyntaxCallParam *)d->call_params->node)->type->kind);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_NAMED, d->return_type->kind);
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(strlen("contract Addable<TLeft, TRight, TResult>(left: TLeft, right: TRight): TResult;"),
                           r.rem.start);

  fx_begin("contract Foo();");
  r = parse_contract_decl(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  d = (const SyntaxContractDecl *)r.node;
  TEST_ASSERT_TRUE(syntax_nodelist_is_empty(d->call_params));
  TEST_ASSERT_NULL(d->return_type);
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(strlen("contract Foo();"), r.rem.start);
}

void test_contract_decl_malforms(void) {
  fx_begin("contract Foo");
  SyntaxNodeResult r = parse_contract_decl(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_EQUAL_size_t(2, error_chain_length(r.errors));
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_EXPECTED_SEMICOLON, r.errors->error.code);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_EXPECTED_LPAREN, r.errors->next->error.code);
  TEST_ASSERT_EQUAL_size_t(strlen("contract Foo"), r.rem.start);

  fx_begin("contract Foo(x: T");
  r = parse_contract_decl(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_EQUAL_size_t(1, syntax_nodelist_length(((const SyntaxContractDecl *)r.node)->call_params));
  TEST_ASSERT_EQUAL_size_t(2, error_chain_length(r.errors));
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_EXPECTED_SEMICOLON, r.errors->error.code);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_EXPECTED_RPAREN, r.errors->next->error.code);
  TEST_ASSERT_EQUAL_size_t(strlen("contract Foo(x: T"), r.rem.start);

  fx_begin("contract Foo()");
  r = parse_contract_decl(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_EQUAL_size_t(1, error_chain_length(r.errors));
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_EXPECTED_SEMICOLON, r.errors->error.code);

  fx_begin("contract Foo(): ;");
  r = parse_contract_decl(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_NULL(((const SyntaxContractDecl *)r.node)->return_type);
  TEST_ASSERT_EQUAL_size_t(1, error_chain_length(r.errors));
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_EXPECTED_TYPE, r.errors->error.code);
  TEST_ASSERT_EQUAL_size_t(strlen("contract Foo(): ;"), r.rem.start);
}

/* ---- func declaration --------------------------------------------------- */

void test_func_decl_full_ladder(void) {
  fx_begin("@private func add<T: i32, U>(x: T, y: U)cdecl: i32 fulfills Addable { return x; }");
  SyntaxNodeResult r = parse_func_decl(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  const SyntaxFuncDecl *d = (const SyntaxFuncDecl *)r.node;
  TEST_ASSERT_STRVIEW_EQ(d->id->value, "add");
  TEST_ASSERT_EQUAL_size_t(1, syntax_nodelist_length(d->annotations));
  TEST_ASSERT_EQUAL_size_t(2, syntax_nodelist_length(d->generic_params));
  TEST_ASSERT_STRVIEW_EQ(((const SyntaxGenericParam *)d->generic_params->node)->id->value, "U");
  TEST_ASSERT_EQUAL_size_t(2, syntax_nodelist_length(d->call_params));
  TEST_ASSERT_STRVIEW_EQ(d->callconv->value, "cdecl");
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_NAMED, d->return_type->kind);
  TEST_ASSERT_EQUAL_size_t(1, syntax_nodelist_length(d->fulfills));
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_BODY_STMT, d->body->kind);
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(strlen("@private func add<T: i32, U>(x: T, y: U)cdecl: i32 fulfills Addable { return x; }"),
                           r.rem.start);
}

void test_func_decl_body_forms(void) {
  fx_begin("func f();");
  SyntaxNodeResult r = parse_func_decl(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_EMPTY_STMT, ((const SyntaxFuncDecl *)r.node)->body->kind);
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(strlen("func f();"), r.rem.start);

  fx_begin("func f() {}");
  r = parse_func_decl(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_BODY_STMT, ((const SyntaxFuncDecl *)r.node)->body->kind);
  TEST_ASSERT_NULL(r.errors);

  fx_begin("func main():i32{ return 0; }");
  r = parse_func_decl(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  const SyntaxFuncDecl *d = (const SyntaxFuncDecl *)r.node;
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_NAMED, d->return_type->kind);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_BODY_STMT, d->body->kind);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_RETURN_STMT, ((const SyntaxBodyStmt *)d->body)->stmts->node->kind);
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(strlen("func main():i32{ return 0; }"), r.rem.start);
}

void test_func_decl_callconv_and_fulfills(void) {
  fx_begin("func f()cdecl:i32{}");
  SyntaxNodeResult r = parse_func_decl(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_STRVIEW_EQ(((const SyntaxFuncDecl *)r.node)->callconv->value, "cdecl");
  TEST_ASSERT_TRUE(syntax_nodelist_is_empty(((const SyntaxFuncDecl *)r.node)->fulfills));

  // "fulfills" starts its clause; it is not lexed as a calling convention.
  fx_begin("func f() fulfills Addable;");
  r = parse_func_decl(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  const SyntaxFuncDecl *d = (const SyntaxFuncDecl *)r.node;
  TEST_ASSERT_NULL(d->callconv);
  TEST_ASSERT_EQUAL_size_t(1, syntax_nodelist_length(d->fulfills));
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_EMPTY_STMT, d->body->kind);
  TEST_ASSERT_NULL(r.errors);

  fx_begin("func f(): i32{}");
  r = parse_func_decl(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  d = (const SyntaxFuncDecl *)r.node;
  TEST_ASSERT_NULL(d->callconv);
  TEST_ASSERT_TRUE(syntax_nodelist_is_empty(d->fulfills));
  TEST_ASSERT_NOT_NULL(d->return_type);
}

void test_program_func_sample_end_to_end(void) {
  static const char *const SAMPLE = "namespace std::math;\n"
                                    "using std::core;\n"
                                    "func main():i32{\n"
                                    "  return 0;\n"
                                    "}\n";
  fx_begin(SAMPLE);
  SyntaxNodeResult r = parse_program(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_PROGRAM, r.node->kind);
  TEST_ASSERT_EQUAL_size_t(strlen(SAMPLE), r.rem.start);

  const SyntaxProgram *p = (const SyntaxProgram *)r.node;
  TEST_ASSERT_EQUAL_size_t(3, syntax_nodelist_length(p->top_levels));
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_FUNC_DECL, p->top_levels->node->kind);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_USING_DECL, p->top_levels->next->node->kind);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_NAMESPACE_DECL, p->top_levels->next->next->node->kind);
}

/* ---- statements -------------------------------------------------------- */

static const SyntaxBodyStmt *as_body(const SyntaxNode *n) {
  TEST_ASSERT_NOT_NULL(n);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_BODY_STMT, n->kind);
  return (const SyntaxBodyStmt *)n;
}

void test_empty_stmt_bare(void) {
  fx_begin(";");
  SyntaxNodeResult r = parse_stmt(fx_parser, source_get_span(fx_source));

  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_EMPTY_STMT, r.node->kind);
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(1, r.rem.start);
}

void test_body_stmt_empty_and_stmts(void) {
  fx_begin("{}");
  SyntaxNodeResult r = parse_body_stmt(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_TRUE(syntax_nodelist_is_empty(as_body(r.node)->stmts));
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(2, r.rem.start);

  // Source order [empty, let, empty]; the chain is newest-at-head.
  fx_begin("{ ; let a = 1; ; }");
  r = parse_body_stmt(fx_parser, source_get_span(fx_source));
  const SyntaxBodyStmt *b = as_body(r.node);
  TEST_ASSERT_EQUAL_size_t(3, syntax_nodelist_length(b->stmts));
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_EMPTY_STMT, b->stmts->node->kind);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_LET_STMT, b->stmts->next->node->kind);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_EMPTY_STMT, b->stmts->next->next->node->kind);
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(strlen("{ ; let a = 1; ; }"), r.rem.start);
}

void test_body_stmt_nested(void) {
  fx_begin("{ { ; } }");
  SyntaxNodeResult r = parse_body_stmt(fx_parser, source_get_span(fx_source));
  const SyntaxBodyStmt *outer = as_body(r.node);
  TEST_ASSERT_EQUAL_size_t(1, syntax_nodelist_length(outer->stmts));
  const SyntaxBodyStmt *inner = as_body(outer->stmts->node);
  TEST_ASSERT_EQUAL_size_t(1, syntax_nodelist_length(inner->stmts));
  TEST_ASSERT_NULL(r.errors);
}

void test_body_stmt_trivia(void) {
  fx_begin("{ \n\t // c\n }");
  SyntaxNodeResult r = parse_body_stmt(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_TRUE(syntax_nodelist_is_empty(as_body(r.node)->stmts));
  TEST_ASSERT_NULL(r.errors);
}

void test_body_stmt_missing_rbrace_frame(void) {
  // The frame survives holding the parsed statements; one diagnostic.
  fx_begin("{ ;");
  SyntaxNodeResult r = parse_body_stmt(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_EQUAL_size_t(1, syntax_nodelist_length(as_body(r.node)->stmts));
  TEST_ASSERT_EQUAL_size_t(1, error_count(&r));
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_EXPECTED_RBRACE, r.errors->error.code);
  TEST_ASSERT_EQUAL_size_t(3, r.rem.start);
}

void test_let_stmt_typed_and_inferred(void) {
  fx_begin("let a = 1;");
  SyntaxNodeResult r = parse_stmt(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_LET_STMT, r.node->kind);
  const SyntaxLetStmt *s = (const SyntaxLetStmt *)r.node;
  TEST_ASSERT_STRVIEW_EQ(s->id->value, "a");
  TEST_ASSERT_NULL(s->type);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_INT_LIT_EXPR, s->value->kind);
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(10, r.rem.start);

  fx_begin("let b : i32 = 2 ;");
  r = parse_stmt(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  s = (const SyntaxLetStmt *)r.node;
  TEST_ASSERT_STRVIEW_EQ(s->id->value, "b");
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_NAMED, s->type->kind);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_INT_LIT_EXPR, s->value->kind);
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(strlen("let b : i32 = 2 ;"), r.rem.start);
}

void test_let_stmt_malforms(void) {
  // Missing "=": the frame survives holding the identifier.
  fx_begin("let a 1;");
  SyntaxNodeResult r = parse_let_stmt(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_NOT_NULL(((const SyntaxLetStmt *)r.node)->id);
  TEST_ASSERT_EQUAL_size_t(1, error_count(&r));
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_EXPECTED_EQUALS, r.errors->error.code);

  // Missing ";": the frame survives holding the value.
  fx_begin("let a = 1");
  r = parse_let_stmt(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_EQUAL_size_t(1, error_count(&r));
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_EXPECTED_SEMICOLON, r.errors->error.code);
  TEST_ASSERT_EQUAL_size_t(9, r.rem.start);

  // Missing identifier recovers at the "=" and still binds the value.
  fx_begin("let = 1;");
  r = parse_let_stmt(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_NULL(((const SyntaxLetStmt *)r.node)->id);
  TEST_ASSERT_NOT_NULL(((const SyntaxLetStmt *)r.node)->value);
  TEST_ASSERT_EQUAL_size_t(1, error_count(&r));
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_EXPECTED_IDENTIFIER, r.errors->error.code);
}

void test_set_stmt_basic(void) {
  fx_begin("set a = 10;");
  SyntaxNodeResult r = parse_stmt(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_SET_STMT, r.node->kind);
  const SyntaxSetStmt *s = (const SyntaxSetStmt *)r.node;
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_NAMED, s->left->kind);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_INT_LIT_EXPR, s->right->kind);
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(11, r.rem.start);
}

void test_set_stmt_malforms(void) {
  // Missing left-hand expression; the rest still binds.
  fx_begin("set = 1;");
  SyntaxNodeResult r = parse_set_stmt(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_NULL(((const SyntaxSetStmt *)r.node)->left);
  TEST_ASSERT_NOT_NULL(((const SyntaxSetStmt *)r.node)->right);
  TEST_ASSERT_EQUAL_size_t(1, error_count(&r));
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_EXPECTED_EXPR, r.errors->error.code);

  // Missing "=": both sides survive.
  fx_begin("set a 1;");
  r = parse_set_stmt(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_NOT_NULL(((const SyntaxSetStmt *)r.node)->left);
  TEST_ASSERT_NOT_NULL(((const SyntaxSetStmt *)r.node)->right);
  TEST_ASSERT_EQUAL_size_t(1, error_count(&r));
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_EXPECTED_EQUALS, r.errors->error.code);
}

void test_expr_stmt_basic(void) {
  fx_begin("foo();");
  SyntaxNodeResult r = parse_stmt(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_EXPR_STMT, r.node->kind);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_CALL_EXPR, ((const SyntaxExprStmt *)r.node)->expr->kind);
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(6, r.rem.start);
}

void test_expr_stmt_missing_semicolon_frame(void) {
  fx_begin("1+2");
  SyntaxNodeResult r = parse_expr_stmt(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_NOT_NULL(((const SyntaxExprStmt *)r.node)->expr);
  TEST_ASSERT_EQUAL_size_t(1, error_count(&r));
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_EXPECTED_SEMICOLON, r.errors->error.code);
  TEST_ASSERT_EQUAL_size_t(3, r.rem.start);
}

void test_break_continue_stmts(void) {
  fx_begin("break;");
  SyntaxNodeResult r = parse_stmt(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_BREAK_STMT, r.node->kind);
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(6, r.rem.start);

  fx_begin("continue;");
  r = parse_stmt(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_CONTINUE_STMT, r.node->kind);
  TEST_ASSERT_NULL(r.errors);

  // Missing ";" keeps the keyword frame with one diagnostic.
  fx_begin("break");
  r = parse_break_stmt(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_EQUAL_size_t(1, error_count(&r));
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_EXPECTED_SEMICOLON, r.errors->error.code);
  TEST_ASSERT_EQUAL_size_t(5, r.rem.start);
}

void test_return_with_and_without_expr(void) {
  fx_begin("return;");
  SyntaxNodeResult r = parse_stmt(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_RETURN_STMT, r.node->kind);
  TEST_ASSERT_NULL(((const SyntaxReturnStmt *)r.node)->expr);
  TEST_ASSERT_NULL(r.errors);

  fx_begin("return 1;");
  r = parse_stmt(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_INT_LIT_EXPR, ((const SyntaxReturnStmt *)r.node)->expr->kind);
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(9, r.rem.start);
}

void test_return_malform(void) {
  // Neither expression nor ";": one diagnostic at the post-keyword spot.
  fx_begin("return");
  SyntaxNodeResult r = parse_return_stmt(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_NULL(((const SyntaxReturnStmt *)r.node)->expr);
  TEST_ASSERT_EQUAL_size_t(1, error_count(&r));
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_EXPECTED_SEMICOLON, r.errors->error.code);
  TEST_ASSERT_EQUAL_size_t(6, r.rem.start);
}

void test_if_else_if_chain(void) {
  fx_begin("if (a) {} else if (b) {} else {}");
  SyntaxNodeResult r = parse_stmt(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_IF_STMT, r.node->kind);
  const SyntaxIfStmt *outer = (const SyntaxIfStmt *)r.node;
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_NAMED, outer->condition->kind);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_BODY_STMT, outer->then_stmt->kind);

  const SyntaxIfStmt *inner = (const SyntaxIfStmt *)outer->else_stmt;
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_IF_STMT, inner->header.kind);
  const SyntaxNamed *cond = (const SyntaxNamed *)inner->condition;
  TEST_ASSERT_STRVIEW_EQ(((const SyntaxIdentifier *)cond->path->node)->value, "b");
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_BODY_STMT, inner->then_stmt->kind);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_BODY_STMT, inner->else_stmt->kind);
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(strlen("if (a) {} else if (b) {} else {}"), r.rem.start);
}

void test_if_empty_bodies(void) {
  // ";" is the empty body at both positions.
  fx_begin("if (a); else ;");
  SyntaxNodeResult r = parse_stmt(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  const SyntaxIfStmt *s = (const SyntaxIfStmt *)r.node;
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_EMPTY_STMT, s->then_stmt->kind);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_EMPTY_STMT, s->else_stmt->kind);
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(strlen("if (a); else ;"), r.rem.start);

  // Braced body, no else.
  fx_begin("if (a) {}");
  r = parse_stmt(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  s = (const SyntaxIfStmt *)r.node;
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_BODY_STMT, s->then_stmt->kind);
  TEST_ASSERT_NULL(s->else_stmt);
}

void test_if_missing_body_reports_body(void) {
  fx_begin("if (a)");
  SyntaxNodeResult r = parse_if_stmt(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_NAMED, ((const SyntaxIfStmt *)r.node)->condition->kind);
  TEST_ASSERT_NULL(((const SyntaxIfStmt *)r.node)->then_stmt);
  TEST_ASSERT_NULL(((const SyntaxIfStmt *)r.node)->else_stmt);
  TEST_ASSERT_EQUAL_size_t(1, error_count(&r));
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_EXPECTED_BODY, r.errors->error.code);
  TEST_ASSERT_EQUAL_size_t(6, r.rem.start);
}

void test_if_struct_lit_condition(void) {
  // The parenthesized condition keeps the struct literal unambiguous.
  fx_begin("if (a{c = 1}) {} else {}");
  SyntaxNodeResult r = parse_stmt(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  const SyntaxIfStmt *s = (const SyntaxIfStmt *)r.node;
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_STRUCT_LIT_EXPR, s->condition->kind);
  TEST_ASSERT_EQUAL_size_t(1, syntax_nodelist_length(((const SyntaxStructLitExpr *)s->condition)->fields));
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_BODY_STMT, s->then_stmt->kind);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_BODY_STMT, s->else_stmt->kind);
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(strlen("if (a{c = 1}) {} else {}"), r.rem.start);
}

void test_if_missing_lparen(void) {
  fx_begin("if a {}");
  SyntaxNodeResult r = parse_if_stmt(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_NULL(((const SyntaxIfStmt *)r.node)->condition);
  TEST_ASSERT_NULL(((const SyntaxIfStmt *)r.node)->then_stmt);
  TEST_ASSERT_EQUAL_size_t(2, error_chain_length(r.errors));
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_EXPECTED_BODY, r.errors->error.code);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_EXPECTED_LPAREN, r.errors->next->error.code);
  TEST_ASSERT_EQUAL_size_t(strlen("if"), r.rem.start);
}

void test_loop_stmt(void) {
  fx_begin("loop {}");
  SyntaxNodeResult r = parse_stmt(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_LOOP_STMT, r.node->kind);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_BODY_STMT, ((const SyntaxLoopStmt *)r.node)->stmt->kind);
  TEST_ASSERT_NULL(r.errors);

  fx_begin("loop;");
  r = parse_stmt(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_EMPTY_STMT, ((const SyntaxLoopStmt *)r.node)->stmt->kind);
  TEST_ASSERT_NULL(r.errors);
}

void test_while_stmt(void) {
  fx_begin("while (a) {}");
  SyntaxNodeResult r = parse_stmt(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_WHILE_STMT, r.node->kind);
  const SyntaxWhileStmt *s = (const SyntaxWhileStmt *)r.node;
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_NAMED, s->condition->kind);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_BODY_STMT, s->stmt->kind);
  TEST_ASSERT_NULL(r.errors);

  fx_begin("while (a);");
  r = parse_stmt(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_EMPTY_STMT, ((const SyntaxWhileStmt *)r.node)->stmt->kind);
  TEST_ASSERT_NULL(r.errors);
}

void test_stmt_keyword_boundaries(void) {
  // "letx" is one identifier: the dispatch falls through to expr_stmt.
  fx_begin("letx;");
  SyntaxNodeResult r = parse_stmt(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_EXPR_STMT, r.node->kind);
  TEST_ASSERT_NULL(r.errors);

  // "ifs" likewise starts an expression, not an if.
  fx_begin("ifs;");
  r = parse_stmt(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_EXPR_STMT, r.node->kind);
  TEST_ASSERT_NULL(r.errors);
}

void test_stmt_dispatch_ladder(void) {
  // One body holding every statement form; the chain is newest-at-head.
  fx_begin("{ ; let a = 1; set a = 2; a; if (a) {} else ; loop {} while (a); break; continue; return a; }");
  SyntaxNodeResult r = parse_body_stmt(fx_parser, source_get_span(fx_source));
  const SyntaxBodyStmt *b = as_body(r.node);
  TEST_ASSERT_EQUAL_size_t(10, syntax_nodelist_length(b->stmts));

  static const SyntaxKind WANT[] = {
      SYNTAX_KIND_RETURN_STMT, SYNTAX_KIND_CONTINUE_STMT, SYNTAX_KIND_BREAK_STMT, SYNTAX_KIND_WHILE_STMT,
      SYNTAX_KIND_LOOP_STMT,   SYNTAX_KIND_IF_STMT,       SYNTAX_KIND_EXPR_STMT,  SYNTAX_KIND_SET_STMT,
      SYNTAX_KIND_LET_STMT,    SYNTAX_KIND_EMPTY_STMT,
  };
  const SyntaxNodeList *n = b->stmts;
  for (size_t i = 0; i < sizeof(WANT) / sizeof(WANT[0]); i++) {
    TEST_ASSERT_EQUAL_HEX32(WANT[i], n->node->kind);
    n = n->next;
  }
  TEST_ASSERT_NULL(n);
  TEST_ASSERT_NULL(r.errors);
}

/* ---- array / func types, struct / array literals ----------------------- */

void test_array_type_forms(void) {
  fx_begin("[5]i32");
  SyntaxNodeResult r = parse_type(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_ARRAY_TYPE, r.node->kind);
  const SyntaxArrayType *t = (const SyntaxArrayType *)r.node;
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_INT_LIT_EXPR, t->len->kind);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_NAMED, t->inner_type->kind);
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(strlen("[5]i32"), r.rem.start);

  fx_begin("[5+10]i32");
  r = parse_type(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  t = (const SyntaxArrayType *)r.node;
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_BINARY_EXPR, t->len->kind);

  fx_begin("[2][2]i32");
  r = parse_type(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  t = (const SyntaxArrayType *)r.node;
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_ARRAY_TYPE, t->inner_type->kind);
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(strlen("[2][2]i32"), r.rem.start);
}

void test_array_type_malform(void) {
  fx_begin("[]i32");
  SyntaxNodeResult r = parse_type(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  const SyntaxArrayType *t = (const SyntaxArrayType *)r.node;
  TEST_ASSERT_NULL(t->len);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_NAMED, t->inner_type->kind);
  TEST_ASSERT_EQUAL_size_t(1, error_chain_length(r.errors));
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_EXPECTED_EXPR, r.errors->error.code);
  TEST_ASSERT_EQUAL_size_t(strlen("[]i32"), r.rem.start);

  fx_begin("[5 i32");
  r = parse_array_type(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_EQUAL_size_t(1, error_chain_length(r.errors));
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_EXPECTED_RBRACKET, r.errors->error.code);

  fx_begin("[5]");
  r = parse_array_type(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_NULL(((const SyntaxArrayType *)r.node)->inner_type);
  TEST_ASSERT_EQUAL_size_t(1, error_chain_length(r.errors));
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_EXPECTED_TYPE, r.errors->error.code);
}

void test_func_type_forms(void) {
  fx_begin("&func()");
  SyntaxNodeResult r = parse_type(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_FUNC_TYPE, r.node->kind);
  const SyntaxFuncType *t = (const SyntaxFuncType *)r.node;
  TEST_ASSERT_TRUE(syntax_nodelist_is_empty(t->call_params));
  TEST_ASSERT_NULL(t->callconv);
  TEST_ASSERT_NULL(t->return_type);
  TEST_ASSERT_NULL(r.errors);

  fx_begin("&func(i32, i32):i32");
  r = parse_type(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  t = (const SyntaxFuncType *)r.node;
  TEST_ASSERT_EQUAL_size_t(2, syntax_nodelist_length(t->call_params));
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_NAMED, t->return_type->kind);
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(strlen("&func(i32, i32):i32"), r.rem.start);

  fx_begin("&func(i32)cdecl:i32");
  r = parse_type(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  t = (const SyntaxFuncType *)r.node;
  TEST_ASSERT_STRVIEW_EQ(t->callconv->value, "cdecl");
  TEST_ASSERT_EQUAL_size_t(1, syntax_nodelist_length(t->call_params));
  TEST_ASSERT_NULL(r.errors);
}

void test_func_type_word_boundary(void) {
  fx_begin("&functional");
  SyntaxNodeResult r = parse_type(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_REF_TYPE, r.node->kind);
  const SyntaxRefType *ref = (const SyntaxRefType *)r.node;
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_NAMED, ref->inner_type->kind);
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(strlen("&functional"), r.rem.start);
}

void test_func_type_malform(void) {
  fx_begin("&func(i32");
  SyntaxNodeResult r = parse_func_type(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_EQUAL_size_t(1, syntax_nodelist_length(((const SyntaxFuncType *)r.node)->call_params));
  TEST_ASSERT_EQUAL_size_t(1, error_chain_length(r.errors));
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_EXPECTED_RPAREN, r.errors->error.code);
  TEST_ASSERT_EQUAL_size_t(strlen("&func(i32"), r.rem.start);
}

void test_struct_lit_forms(void) {
  fx_begin("Vector2{}");
  SyntaxNodeResult r = parse_expr(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_STRUCT_LIT_EXPR, r.node->kind);
  TEST_ASSERT_TRUE(syntax_nodelist_is_empty(((const SyntaxStructLitExpr *)r.node)->fields));
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(strlen("Vector2{}"), r.rem.start);

  fx_begin("Vector2{ x = 0_f32, y = 1_f32 }");
  r = parse_expr(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  const SyntaxStructLitExpr *s = (const SyntaxStructLitExpr *)r.node;
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_NAMED, s->type->header.kind);
  TEST_ASSERT_EQUAL_size_t(2, syntax_nodelist_length(s->fields));
  TEST_ASSERT_STRVIEW_EQ(((const SyntaxStructLitField *)s->fields->node)->id->value, "y");
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_FLOAT_LIT_EXPR, ((const SyntaxStructLitField *)s->fields->node)->value->kind);
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(strlen("Vector2{ x = 0_f32, y = 1_f32 }"), r.rem.start);

  fx_begin("Vector2{ x = 1_f32, }");
  r = parse_expr(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_EQUAL_size_t(1, syntax_nodelist_length(((const SyntaxStructLitExpr *)r.node)->fields));
  TEST_ASSERT_NULL(r.errors);

  fx_begin("Segment{ from = Vector2{ x = 0_f32 } }");
  r = parse_expr(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  s = (const SyntaxStructLitExpr *)r.node;
  TEST_ASSERT_EQUAL_size_t(1, syntax_nodelist_length(s->fields));
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_STRUCT_LIT_EXPR, ((const SyntaxStructLitField *)s->fields->node)->value->kind);
  TEST_ASSERT_NULL(r.errors);
}

void test_struct_lit_not_a_literal(void) {
  fx_begin("Vector2");
  SyntaxNodeResult r = parse_expr(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_NAMED, r.node->kind);
  TEST_ASSERT_NULL(r.errors);
}

void test_struct_lit_field_frame(void) {
  fx_begin("Vector2{ x = }");
  SyntaxNodeResult r = parse_struct_lit_expr(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  const SyntaxStructLitField *f = (const SyntaxStructLitField *)((const SyntaxStructLitExpr *)r.node)->fields->node;
  TEST_ASSERT_STRVIEW_EQ(f->id->value, "x");
  TEST_ASSERT_NULL(f->value);
  TEST_ASSERT_EQUAL_size_t(1, error_chain_length(r.errors));
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_EXPECTED_EXPR, r.errors->error.code);
  TEST_ASSERT_EQUAL_size_t(strlen("Vector2{ x = }"), r.rem.start);
}

void test_array_lit_forms(void) {
  fx_begin("[5]i32{}");
  SyntaxNodeResult r = parse_expr(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_ARRAY_LIT_EXPR, r.node->kind);
  TEST_ASSERT_TRUE(syntax_nodelist_is_empty(((const SyntaxArrayLitExpr *)r.node)->elements));
  TEST_ASSERT_NULL(r.errors);

  fx_begin("[5]i32{ 1, 2, 3, 4, 5 }");
  r = parse_expr(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  const SyntaxArrayLitExpr *a = (const SyntaxArrayLitExpr *)r.node;
  TEST_ASSERT_EQUAL_size_t(5, syntax_nodelist_length(a->elements));
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_INT_LIT_EXPR, a->elements->node->kind);
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(strlen("[5]i32{ 1, 2, 3, 4, 5 }"), r.rem.start);

  fx_begin("[5]i32{ 1, }");
  r = parse_expr(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_EQUAL_size_t(1, syntax_nodelist_length(((const SyntaxArrayLitExpr *)r.node)->elements));
  TEST_ASSERT_NULL(r.errors);

  fx_begin("[2][2]i32{ [2]i32{ 1, 2 }, [2]i32{ 3, 4 } }");
  r = parse_expr(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  a = (const SyntaxArrayLitExpr *)r.node;
  TEST_ASSERT_EQUAL_size_t(2, syntax_nodelist_length(a->elements));
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_ARRAY_LIT_EXPR, a->elements->node->kind);
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(strlen("[2][2]i32{ [2]i32{ 1, 2 }, [2]i32{ 3, 4 } }"), r.rem.start);
}

void test_array_lit_malform(void) {
  fx_begin("[5]i32{ 1 2 }");
  SyntaxNodeResult r = parse_array_lit_expr(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_EQUAL_size_t(1, syntax_nodelist_length(((const SyntaxArrayLitExpr *)r.node)->elements));
  TEST_ASSERT_EQUAL_size_t(1, error_chain_length(r.errors));
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_EXPECTED_RBRACE, r.errors->error.code);
  TEST_ASSERT_EQUAL_size_t(strlen("[5]i32{ 1"), r.rem.start);
}

/* ---- generic arguments ------------------------------------------------- */

void test_named_type_generic_forms(void) {
  fx_begin("Array<i32, N = 5>");
  SyntaxNodeResult r = parse_named_type(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  const SyntaxNamed *t = as_named(r.node);
  static const char *const PATH[] = {"Array"};
  check_path(t->path, PATH, 1);
  TEST_ASSERT_EQUAL_size_t(2, syntax_nodelist_length(t->generic_args));
  const SyntaxGenericArg *named = (const SyntaxGenericArg *)t->generic_args->node;
  TEST_ASSERT_STRVIEW_EQ(named->id->value, "N");
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_INT_LIT_EXPR, named->value->kind);
  const SyntaxGenericArg *ty = (const SyntaxGenericArg *)t->generic_args->next->node;
  TEST_ASSERT_NULL(ty->id);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_NAMED, ty->value->kind);
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(strlen("Array<i32, N = 5>"), r.rem.start);

  fx_begin("Box<[5]i32>");
  r = parse_named_type(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  t = as_named(r.node);
  const SyntaxGenericArg *arg = (const SyntaxGenericArg *)t->generic_args->node;
  TEST_ASSERT_NULL(arg->id);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_ARRAY_TYPE, arg->value->kind);
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(strlen("Box<[5]i32>"), r.rem.start);
}

void test_named_type_generic_paren_escape(void) {
  fx_begin("Array<i32, N = (LEN + 1)>");
  SyntaxNodeResult r = parse_named_type(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  const SyntaxNamed *t = as_named(r.node);
  const SyntaxGenericArg *named = (const SyntaxGenericArg *)t->generic_args->node;
  TEST_ASSERT_STRVIEW_EQ(named->id->value, "N");
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_BINARY_EXPR, named->value->kind);
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(strlen("Array<i32, N = (LEN + 1)>"), r.rem.start);
}

void test_named_type_generic_nested(void) {
  fx_begin("Box<Box<i32>>");
  SyntaxNodeResult r = parse_named_type(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  const SyntaxNamed *outer = as_named(r.node);
  TEST_ASSERT_EQUAL_size_t(1, syntax_nodelist_length(outer->generic_args));
  const SyntaxGenericArg *arg = (const SyntaxGenericArg *)outer->generic_args->node;
  TEST_ASSERT_NULL(arg->id);
  const SyntaxNamed *inner = as_named(arg->value);
  TEST_ASSERT_EQUAL_size_t(1, syntax_nodelist_length(inner->generic_args));
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(strlen("Box<Box<i32>>"), r.rem.start);
}

void test_named_type_generic_malform(void) {
  fx_begin("a<>");
  SyntaxNodeResult r = parse_named_type(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_TRUE(syntax_nodelist_is_empty(as_named(r.node)->generic_args));
  TEST_ASSERT_EQUAL_size_t(1, error_chain_length(r.errors));
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_EXPECTED_IDENTIFIER, r.errors->error.code);
  TEST_ASSERT_EQUAL_size_t(strlen("a<>"), r.rem.start);

  fx_begin("a<b");
  r = parse_named_type(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_EQUAL_size_t(1, syntax_nodelist_length(as_named(r.node)->generic_args));
  TEST_ASSERT_EQUAL_size_t(1, error_chain_length(r.errors));
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_EXPECTED_GT, r.errors->error.code);
  TEST_ASSERT_EQUAL_size_t(strlen("a<b"), r.rem.start);
}

void test_named_expr_generic_call(void) {
  fx_begin("f<a>(x)");
  SyntaxNodeResult r = parse_expr(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_CALL_EXPR, r.node->kind);
  const SyntaxCallExpr *call = (const SyntaxCallExpr *)r.node;
  const SyntaxNamed *receiver = as_named(call->receiver);
  TEST_ASSERT_EQUAL_size_t(1, syntax_nodelist_length(receiver->generic_args));
  TEST_ASSERT_EQUAL_size_t(1, syntax_nodelist_length(call->args));
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(strlen("f<a>(x)"), r.rem.start);
}

void test_named_expr_relational_fallback(void) {
  fx_begin("a<b>c");
  SyntaxNodeResult r = parse_expr(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  const SyntaxBinaryExpr *gt = as_bin(r.node, SYNTAX_OPERATOR_GT);
  const SyntaxBinaryExpr *lt = as_bin(gt->left, SYNTAX_OPERATOR_LT);
  const SyntaxNamed *a = as_named(lt->left);
  TEST_ASSERT_STRVIEW_EQ(((const SyntaxIdentifier *)a->path->node)->value, "a");
  const SyntaxNamed *b = as_named(lt->right);
  TEST_ASSERT_STRVIEW_EQ(((const SyntaxIdentifier *)b->path->node)->value, "b");
  const SyntaxNamed *c = as_named(gt->right);
  TEST_ASSERT_STRVIEW_EQ(((const SyntaxIdentifier *)c->path->node)->value, "c");
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(strlen("a<b>c"), r.rem.start);
}

void test_named_expr_shift_fallback(void) {
  fx_begin("a<b>>c");
  SyntaxNodeResult r = parse_expr(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  const SyntaxBinaryExpr *lt = as_bin(r.node, SYNTAX_OPERATOR_LT);
  const SyntaxNamed *a = as_named(lt->left);
  TEST_ASSERT_STRVIEW_EQ(((const SyntaxIdentifier *)a->path->node)->value, "a");
  const SyntaxBinaryExpr *shr = as_bin(lt->right, SYNTAX_OPERATOR_SHR);
  const SyntaxNamed *b = as_named(shr->left);
  TEST_ASSERT_STRVIEW_EQ(((const SyntaxIdentifier *)b->path->node)->value, "b");
  const SyntaxNamed *c = as_named(shr->right);
  TEST_ASSERT_STRVIEW_EQ(((const SyntaxIdentifier *)c->path->node)->value, "c");
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(strlen("a<b>>c"), r.rem.start);
}

void test_named_expr_two_comparisons(void) {
  fx_begin("f(a<b, c>d)");
  SyntaxNodeResult r = parse_expr(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_CALL_EXPR, r.node->kind);
  const SyntaxCallExpr *call = (const SyntaxCallExpr *)r.node;
  TEST_ASSERT_EQUAL_size_t(2, syntax_nodelist_length(call->args));
  const SyntaxBinaryExpr *gt = as_bin(call->args->node, SYNTAX_OPERATOR_GT);
  const SyntaxNamed *c = as_named(gt->left);
  TEST_ASSERT_STRVIEW_EQ(((const SyntaxIdentifier *)c->path->node)->value, "c");
  const SyntaxBinaryExpr *lt = as_bin(call->args->next->node, SYNTAX_OPERATOR_LT);
  const SyntaxNamed *a = as_named(lt->left);
  TEST_ASSERT_STRVIEW_EQ(((const SyntaxIdentifier *)a->path->node)->value, "a");
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(strlen("f(a<b, c>d)"), r.rem.start);
}

void test_named_expr_member_generic(void) {
  fx_begin("f<a>.x");
  SyntaxNodeResult r = parse_expr(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_DOT_EXPR, r.node->kind);
  const SyntaxDotExpr *dot = (const SyntaxDotExpr *)r.node;
  TEST_ASSERT_EQUAL_size_t(1, syntax_nodelist_length(as_named(dot->receiver)->generic_args));
  TEST_ASSERT_STRVIEW_EQ(dot->id->value, "x");
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(strlen("f<a>.x"), r.rem.start);
}

void test_named_expr_condition_generic(void) {
  fx_begin("if (a<b) {} else {}");
  SyntaxNodeResult r = parse_stmt(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  const SyntaxIfStmt *s = (const SyntaxIfStmt *)r.node;
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_BINARY_EXPR, s->condition->kind);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_OPERATOR_LT, ((const SyntaxBinaryExpr *)s->condition)->operator);
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(strlen("if (a<b) {} else {}"), r.rem.start);
}

void test_generic_arg_compile_time_value(void) {
  fx_begin("M<i32, N = @sizeof(i32)>");
  SyntaxNodeResult r = parse_named_type(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  const SyntaxNamed *t = as_named(r.node);
  const SyntaxGenericArg *named = (const SyntaxGenericArg *)t->generic_args->node;
  TEST_ASSERT_STRVIEW_EQ(named->id->value, "N");
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_COMPILE_TIME, named->value->kind);
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(strlen("M<i32, N = @sizeof(i32)>"), r.rem.start);
}

void test_generic_arg_named_generic_value(void) {
  fx_begin("add<i32, i32, F = Addable<i32, i32>>");
  SyntaxNodeResult r = parse_named_type(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  const SyntaxNamed *t = as_named(r.node);
  TEST_ASSERT_EQUAL_size_t(3, syntax_nodelist_length(t->generic_args));
  const SyntaxGenericArg *f = (const SyntaxGenericArg *)t->generic_args->node;
  TEST_ASSERT_STRVIEW_EQ(f->id->value, "F");
  const SyntaxNamed *v = as_named(f->value);
  TEST_ASSERT_EQUAL_size_t(2, syntax_nodelist_length(v->generic_args));
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(strlen("add<i32, i32, F = Addable<i32, i32>>"), r.rem.start);
}

void test_generic_struct_lit_generic_type(void) {
  fx_begin("Vector2<f32>{ x = 0_f32 }");
  SyntaxNodeResult r = parse_expr(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_STRUCT_LIT_EXPR, r.node->kind);
  const SyntaxStructLitExpr *s = (const SyntaxStructLitExpr *)r.node;
  const SyntaxNamed *t = as_named((const SyntaxNode *)s->type);
  TEST_ASSERT_EQUAL_size_t(1, syntax_nodelist_length(t->generic_args));
  TEST_ASSERT_EQUAL_size_t(1, syntax_nodelist_length(s->fields));
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(strlen("Vector2<f32>{ x = 0_f32 }"), r.rem.start);
}

void test_generic_fulfills(void) {
  fx_begin("func f() fulfills Addable<i32, i32>;");
  SyntaxNodeResult r = parse_func_decl(fx_parser, source_get_span(fx_source));
  TEST_ASSERT_TRUE(r.matched);
  const SyntaxFuncDecl *d = (const SyntaxFuncDecl *)r.node;
  TEST_ASSERT_EQUAL_size_t(1, syntax_nodelist_length(d->fulfills));
  const SyntaxNamed *n = as_named(d->fulfills->node);
  TEST_ASSERT_EQUAL_size_t(2, syntax_nodelist_length(n->generic_args));
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_EMPTY_STMT, d->body->kind);
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_size_t(strlen("func f() fulfills Addable<i32, i32>;"), r.rem.start);
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
    {"annotations_none", test_annotations_none},
    {"annotations_single_and_multi", test_annotations_single_and_multi},
    {"annotations_error_frame", test_annotations_error_frame},
    {"generic_param_bare_and_bound", test_generic_param_bare_and_bound},
    {"generic_param_annotated", test_generic_param_annotated},
    {"call_param_requires_colon_and_type", test_call_param_requires_colon_and_type},
    {"call_param_annotated", test_call_param_annotated},
    {"let_decl_three_forms", test_let_decl_three_forms},
    {"let_decl_malforms", test_let_decl_malforms},
    {"struct_decl_bare_and_empty_body", test_struct_decl_bare_and_empty_body},
    {"struct_decl_fields_and_trailing_comma", test_struct_decl_fields_and_trailing_comma},
    {"struct_decl_generics", test_struct_decl_generics},
    {"struct_decl_generics_malforms", test_struct_decl_generics_malforms},
    {"struct_field_frames", test_struct_field_frames},
    {"struct_decl_body_malforms", test_struct_decl_body_malforms},
    {"union_decl_forms", test_union_decl_forms},
    {"enum_decl_forms", test_enum_decl_forms},
    {"enum_decl_trailing_comma", test_enum_decl_trailing_comma},
    {"enum_decl_field_malform", test_enum_decl_field_malform},
    {"variant_decl_forms", test_variant_decl_forms},
    {"contract_decl_forms", test_contract_decl_forms},
    {"contract_decl_malforms", test_contract_decl_malforms},
    {"func_decl_full_ladder", test_func_decl_full_ladder},
    {"func_decl_body_forms", test_func_decl_body_forms},
    {"func_decl_callconv_and_fulfills", test_func_decl_callconv_and_fulfills},
    {"program_func_sample_end_to_end", test_program_func_sample_end_to_end},
    {"named_type_generic_forms", test_named_type_generic_forms},
    {"named_type_generic_paren_escape", test_named_type_generic_paren_escape},
    {"named_type_generic_nested", test_named_type_generic_nested},
    {"named_type_generic_malform", test_named_type_generic_malform},
    {"named_expr_generic_call", test_named_expr_generic_call},
    {"named_expr_relational_fallback", test_named_expr_relational_fallback},
    {"named_expr_shift_fallback", test_named_expr_shift_fallback},
    {"named_expr_two_comparisons", test_named_expr_two_comparisons},
    {"named_expr_member_generic", test_named_expr_member_generic},
    {"named_expr_condition_generic", test_named_expr_condition_generic},
    {"generic_arg_compile_time_value", test_generic_arg_compile_time_value},
    {"generic_arg_named_generic_value", test_generic_arg_named_generic_value},
    {"generic_struct_lit_generic_type", test_generic_struct_lit_generic_type},
    {"generic_fulfills", test_generic_fulfills},
    {"array_type_forms", test_array_type_forms},
    {"array_type_malform", test_array_type_malform},
    {"func_type_forms", test_func_type_forms},
    {"func_type_word_boundary", test_func_type_word_boundary},
    {"func_type_malform", test_func_type_malform},
    {"struct_lit_forms", test_struct_lit_forms},
    {"struct_lit_not_a_literal", test_struct_lit_not_a_literal},
    {"struct_lit_field_frame", test_struct_lit_field_frame},
    {"array_lit_forms", test_array_lit_forms},
    {"array_lit_malform", test_array_lit_malform},
    {"empty_stmt_bare", test_empty_stmt_bare},
    {"body_stmt_empty_and_stmts", test_body_stmt_empty_and_stmts},
    {"body_stmt_nested", test_body_stmt_nested},
    {"body_stmt_trivia", test_body_stmt_trivia},
    {"body_stmt_missing_rbrace_frame", test_body_stmt_missing_rbrace_frame},
    {"let_stmt_typed_and_inferred", test_let_stmt_typed_and_inferred},
    {"let_stmt_malforms", test_let_stmt_malforms},
    {"set_stmt_basic", test_set_stmt_basic},
    {"set_stmt_malforms", test_set_stmt_malforms},
    {"expr_stmt_basic", test_expr_stmt_basic},
    {"expr_stmt_missing_semicolon_frame", test_expr_stmt_missing_semicolon_frame},
    {"break_continue_stmts", test_break_continue_stmts},
    {"return_with_and_without_expr", test_return_with_and_without_expr},
    {"return_malform", test_return_malform},
    {"if_else_if_chain", test_if_else_if_chain},
    {"if_empty_bodies", test_if_empty_bodies},
    {"if_missing_body_reports_body", test_if_missing_body_reports_body},
    {"if_struct_lit_condition", test_if_struct_lit_condition},
    {"if_missing_lparen", test_if_missing_lparen},
    {"loop_stmt", test_loop_stmt},
    {"while_stmt", test_while_stmt},
    {"stmt_keyword_boundaries", test_stmt_keyword_boundaries},
    {"stmt_dispatch_ladder", test_stmt_dispatch_ladder},
    {"program_accumulates_decls_newest_first", test_program_accumulates_decls_newest_first},
    {"program_junk_tail_reports_expected_eof", test_program_junk_tail_reports_expected_eof},
    {"program_empty_and_trivia_only", test_program_empty_and_trivia_only},
};

TEST_DISPATCH_MAIN(ENTRIES)
