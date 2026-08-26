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

/* ---- expressions ------------------------------------------------------ */

static const SyntaxNumberLitExpr *as_int(const SyntaxNode *n,
                                         const char *text) {
  TEST_ASSERT_NOT_NULL(n);
  if (!n)
    return NULL;
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_INT_LIT_EXPR, n->kind);
  TEST_ASSERT_STRVIEW_EQ(((const SyntaxNumberLitExpr *)n)->value, text);
  return (const SyntaxNumberLitExpr *)n;
}

static const SyntaxBinaryExpr *as_bin(const SyntaxNode *n,
                                      SyntaxOperator op) {
  TEST_ASSERT_NOT_NULL(n);
  if (!n)
    return NULL;
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_BINARY_EXPR, n->kind);
  TEST_ASSERT_EQUAL_HEX32(op, ((const SyntaxBinaryExpr *)n)->operator);
  return (const SyntaxBinaryExpr *)n;
}

static const SyntaxUnaryExpr *as_unary(const SyntaxNode *n,
                                       SyntaxOperator op) {
  TEST_ASSERT_NOT_NULL(n);
  if (!n)
    return NULL;
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_UNARY_EXPR, n->kind);
  TEST_ASSERT_EQUAL_HEX32(op, ((const SyntaxUnaryExpr *)n)->operator);
  return (const SyntaxUnaryExpr *)n;
}

static ParserResult run_expr(const char *text) {
  fx_begin(text);
  return parse_expr(fx_parser, source_get_span(fx_source));
}

void test_expr_precedence_mul_over_add(void) {
  ParserResult r = run_expr("1+2*3");
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_NULL(r.errors);

  const SyntaxBinaryExpr *add = as_bin(r.node, SYNTAX_OPERATOR_ADD);
  as_int(add->left, "1");
  const SyntaxBinaryExpr *mul =
      as_bin(add->right, SYNTAX_OPERATOR_MUL);
  as_int(mul->left, "2");
  as_int(mul->right, "3");
}

void test_expr_parens_override_grouping(void) {
  // Transparent parens: the inner node surfaces unchanged.
  ParserResult r = run_expr("(1+2)*3");
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_NULL(r.errors);

  const SyntaxBinaryExpr *mul = as_bin(r.node, SYNTAX_OPERATOR_MUL);
  const SyntaxBinaryExpr *add = as_bin(mul->left, SYNTAX_OPERATOR_ADD);
  as_int(add->left, "1");
  as_int(add->right, "2");
  as_int(mul->right, "3");
}

void test_expr_left_associativity(void) {
  ParserResult r = run_expr("1-2-3");
  TEST_ASSERT_TRUE(r.matched);

  const SyntaxBinaryExpr *outer = as_bin(r.node, SYNTAX_OPERATOR_SUB);
  const SyntaxBinaryExpr *inner = as_bin(outer->left, SYNTAX_OPERATOR_SUB);
  as_int(inner->left, "1");
  as_int(inner->right, "2");
  as_int(outer->right, "3");
}

void test_expr_unary_binds_tighter_than_mul(void) {
  ParserResult r = run_expr("-2*3");
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
      {"-1", SYNTAX_OPERATOR_MINUS}, {"+1", SYNTAX_OPERATOR_PLUS},
      {"!0", SYNTAX_OPERATOR_LNOT},  {"~0", SYNTAX_OPERATOR_BNOT},
      {"*1", SYNTAX_OPERATOR_DEREF},
  };

  for (size_t i = 0; i < sizeof(CASES) / sizeof(CASES[0]); i++) {
    ParserResult r = run_expr(CASES[i].text);
    TEST_ASSERT_TRUE(r.matched);
    TEST_ASSERT_NULL(r.errors);
    const SyntaxUnaryExpr *u = as_unary(r.node, CASES[i].op);
    as_int(u->operand, &CASES[i].text[1]);
  }
}

void test_expr_postfix_chain_on_literal(void) {
  // (((1)[2])(3)).x -- every postfix form applied once, in order.
  ParserResult r = run_expr("(1)[2](3).x");
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_NULL(r.errors);

  const SyntaxDotExpr *dot = (const SyntaxDotExpr *)r.node;
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_DOT_EXPR, r.node->kind);
  TEST_ASSERT_STRVIEW_EQ(dot->name->strview, "x");

  const SyntaxCallExpr *call = (const SyntaxCallExpr *)dot->receiver;
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_CALL_EXPR, call->header.kind);
  TEST_ASSERT_EQUAL_size_t(1,
                            syntax_nodelist_length(call->arguments->exprs));

  const SyntaxIndexExpr *index = (const SyntaxIndexExpr *)call->callee;
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_INDEX_EXPR, index->header.kind);
  as_int(index->receiver, "1");
  as_int(index->index, "2");

  as_int(call->arguments->exprs->node, "3");
}

void test_expr_call_empty_args(void) {
  ParserResult r = run_expr("(1)()");
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_NULL(r.errors);

  const SyntaxCallExpr *call = (const SyntaxCallExpr *)r.node;
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_CALL_EXPR, r.node->kind);
  TEST_ASSERT_EQUAL_size_t(0,
                            syntax_nodelist_length(call->arguments->exprs));
  as_int(call->callee, "1");
}

void test_expr_float_dot_beats_member_access(void) {
  // Longest match at the primary: "5." is a float; ".x" is left over.
  ParserResult r = run_expr("5.x");
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_FLOAT_LIT_EXPR, r.node->kind);
  TEST_ASSERT_STRVIEW_EQ(((const SyntaxNumberLitExpr *)r.node)->value,
                         "5.");
  TEST_ASSERT_EQUAL_size_t(2, r.rem.start);
}

void test_expr_malformed_missing_right_hand_side(void) {
  // Recovery doctrine: the dangling operator itself is consumed.
  ParserResult r = run_expr("1+");
  TEST_ASSERT_TRUE(r.matched);      // partial tree kept
  TEST_ASSERT_NOT_NULL(r.node);     // the left operand survives
  as_int(r.node, "1");
  TEST_ASSERT_EQUAL_size_t(1, error_count(&r));
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_EXPECTED_EXPR, r.errors->error.code);
  TEST_ASSERT_EQUAL_size_t(2, r.rem.start); // past the "+"
}

void test_expr_malformed_missing_rhs_logical_or(void) {
  ParserResult r = run_expr("1||");
  TEST_ASSERT_TRUE(r.matched);
  as_int(r.node, "1");
  TEST_ASSERT_EQUAL_size_t(1, error_count(&r));
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_EXPECTED_EXPR, r.errors->error.code);
  TEST_ASSERT_EQUAL_size_t(3, r.rem.start); // past the "||"
}

void test_expr_malformed_dangling_call_comma(void) {
  // The dangling "," is consumed as a recovery run; the call frame
  // survives holding every argument parsed before it.
  ParserResult r = run_expr("(1)(2,)");
  TEST_ASSERT_TRUE(r.matched);

  const SyntaxCallExpr *call = (const SyntaxCallExpr *)r.node;
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_CALL_EXPR, r.node->kind);
  as_int(call->callee, "1");
  TEST_ASSERT_EQUAL_size_t(1,
                           syntax_nodelist_length(call->arguments->exprs));
  as_int(call->arguments->exprs->node, "2");

  TEST_ASSERT_EQUAL_size_t(1, error_count(&r));
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_EXPECTED_EXPR, r.errors->error.code);
  TEST_ASSERT_EQUAL_size_t(strlen("(1)(2,)"), r.rem.start); // past it all
}

void test_expr_malformed_unclosed_paren(void) {
  ParserResult r = run_expr("(1");
  TEST_ASSERT_TRUE(r.matched);
  as_int(r.node, "1"); // transparent parens keep the inner node
  TEST_ASSERT_EQUAL_size_t(1, error_count(&r));
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_EXPECTED_RPAREN, r.errors->error.code);
  TEST_ASSERT_EQUAL_size_t(2, r.rem.start);
}

void test_expr_malformed_empty_parens(void) {
  ParserResult r = run_expr("()");
  TEST_ASSERT_TRUE(r.matched);  // recovery run
  TEST_ASSERT_NULL(r.node);
  TEST_ASSERT_EQUAL_size_t(1, error_count(&r));
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_EXPECTED_EXPR, r.errors->error.code);
}

void test_expr_malformed_dangling_unary(void) {
  // The operator frame survives with a NULL operand; one diagnostic.
  ParserResult r = run_expr("*");
  TEST_ASSERT_TRUE(r.matched);

  const SyntaxUnaryExpr *u = as_unary(r.node, SYNTAX_OPERATOR_DEREF);
  if (u)
    TEST_ASSERT_NULL(u->operand);

  TEST_ASSERT_EQUAL_size_t(1, error_count(&r));
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_EXPECTED_EXPR, r.errors->error.code);
}

void test_expr_relational_two_byte_forms(void) {
  // "<=" / ">=" must win over their single-byte prefixes "<" / ">".
  ParserResult r = run_expr("1<=2");
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
  ParserResult r = run_expr("(1)(2,3)");
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_NULL(r.errors);

  const SyntaxCallExpr *call = (const SyntaxCallExpr *)r.node;
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_CALL_EXPR, r.node->kind);
  TEST_ASSERT_EQUAL_size_t(2,
                           syntax_nodelist_length(call->arguments->exprs));
  as_int(call->arguments->exprs->node, "3");
  as_int(call->arguments->exprs->next->node, "2");
}

void test_expr_dot_missing_identifier_frame(void) {
  // The dot frame survives with a NULL name; one diagnostic.
  ParserResult r = run_expr("(1).!");
  TEST_ASSERT_TRUE(r.matched);

  const SyntaxDotExpr *dot = (const SyntaxDotExpr *)r.node;
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_DOT_EXPR, r.node->kind);
  TEST_ASSERT_NULL(dot->name);
  as_int(dot->receiver, "1");

  TEST_ASSERT_EQUAL_size_t(1, error_count(&r));
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_EXPECTED_IDENTIFIER, r.errors->error.code);
  TEST_ASSERT_EQUAL_size_t(strlen("(1)."), r.rem.start); // at the fault
}

void test_expr_index_frames(void) {
  // Missing "]": frame keeps the parsed index, one RBRACKET diagnostic.
  ParserResult r = run_expr("(1)[2");
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
  TEST_ASSERT_EQUAL_size_t(1, error_count(&r));
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_EXPECTED_EXPR, r.errors->error.code);
}

void test_expr_call_missing_rparen_frame(void) {
  ParserResult r = run_expr("(1)(2");
  TEST_ASSERT_TRUE(r.matched);

  const SyntaxCallExpr *call = (const SyntaxCallExpr *)r.node;
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_CALL_EXPR, r.node->kind);
  as_int(call->callee, "1");
  TEST_ASSERT_EQUAL_size_t(1,
                           syntax_nodelist_length(call->arguments->exprs));
  as_int(call->arguments->exprs->node, "2");

  TEST_ASSERT_EQUAL_size_t(1, error_count(&r));
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_EXPECTED_RPAREN, r.errors->error.code);
}

void test_expr_full_ladder_shape(void) {
  // 1|2 ^^ 3 && 4 == 5<<6+7 || 8
  // = LOR( LXOR( BOR(1,2), LAND( 3, EQ( 4, SHL( 5, ADD(6,7) ) ) ) ), 8 )
  // ^^ is looser than &&, so the whole "3 && ..." lands on its RIGHT.
  ParserResult r = run_expr("1|2^^3&&4==5<<6+7||8");
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
    {"expr_precedence_mul_over_add", test_expr_precedence_mul_over_add},
    {"expr_parens_override_grouping", test_expr_parens_override_grouping},
    {"expr_left_associativity", test_expr_left_associativity},
    {"expr_unary_binds_tighter_than_mul",
     test_expr_unary_binds_tighter_than_mul},
    {"expr_all_unary_operators", test_expr_all_unary_operators},
    {"expr_postfix_chain_on_literal", test_expr_postfix_chain_on_literal},
    {"expr_call_empty_args", test_expr_call_empty_args},
    {"expr_float_dot_beats_member_access",
     test_expr_float_dot_beats_member_access},
    {"expr_malformed_missing_right_hand_side",
     test_expr_malformed_missing_right_hand_side},
    {"expr_malformed_missing_rhs_logical_or",
     test_expr_malformed_missing_rhs_logical_or},
    {"expr_malformed_dangling_call_comma",
     test_expr_malformed_dangling_call_comma},
    {"expr_malformed_unclosed_paren", test_expr_malformed_unclosed_paren},
    {"expr_malformed_empty_parens", test_expr_malformed_empty_parens},
    {"expr_malformed_dangling_unary", test_expr_malformed_dangling_unary},
    {"expr_relational_two_byte_forms", test_expr_relational_two_byte_forms},
    {"expr_call_args_reverse_order", test_expr_call_args_reverse_order},
    {"expr_dot_missing_identifier_frame",
     test_expr_dot_missing_identifier_frame},
    {"expr_index_frames", test_expr_index_frames},
    {"expr_call_missing_rparen_frame", test_expr_call_missing_rparen_frame},
    {"expr_full_ladder_shape", test_expr_full_ladder_shape},
    {"program_accumulates_decls_newest_first",
     test_program_accumulates_decls_newest_first},
    {"program_junk_tail_reports_expected_eof",
     test_program_junk_tail_reports_expected_eof},
    {"program_empty_and_trivia_only", test_program_empty_and_trivia_only},
};

TEST_DISPATCH_MAIN(ENTRIES)
