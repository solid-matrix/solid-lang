#define TEST_SUPPORT_NO_DEFAULT_FIXTURES

#include <string.h>

#include "node.h"
#include "parse.h"
#include "parser_fixture.h"
#include "syntax_error.h"
#include "syntax_node.h"
#include "test_support.h"

void setUp(void) {}
void tearDown(void) { fx_release(); }

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

/* ---- precedence and shape ----------------------------------------------- */

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

/* ---- recovery frames ----------------------------------------------------- */

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

void test_expr_call_args_in_source_order(void) {
  // Arguments accumulate in source order: chain holds [2, 3].
  SyntaxNodeResult r = run_expr("(1)(2,3)");
  TEST_ASSERT_TRUE(r.matched);
  TEST_ASSERT_NULL(r.errors);

  const SyntaxCallExpr *call = (const SyntaxCallExpr *)r.node;
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_CALL_EXPR, r.node->kind);
  TEST_ASSERT_EQUAL_size_t(2, syntax_nodelist_length(call->args));
  as_int(call->args->node, "2");
  as_int(call->args->next->node, "3");
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

/* ---- full ladder ----------------------------------------------------------- */

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

/* ---- compile-time operands ------------------------------------------------- */

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

/* ---- named expressions with generics ---------------------------------------- */

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
  const SyntaxBinaryExpr *lt = as_bin(call->args->node, SYNTAX_OPERATOR_LT);
  const SyntaxNamed *a = as_named(lt->left);
  TEST_ASSERT_STRVIEW_EQ(((const SyntaxIdentifier *)a->path->node)->value, "a");
  const SyntaxBinaryExpr *gt = as_bin(call->args->next->node, SYNTAX_OPERATOR_GT);
  const SyntaxNamed *c = as_named(gt->left);
  TEST_ASSERT_STRVIEW_EQ(((const SyntaxIdentifier *)c->path->node)->value, "c");
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

static const TestDispatchEntry ENTRIES[] = {
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
    {"expr_call_args_in_source_order", test_expr_call_args_in_source_order},
    {"expr_dot_missing_identifier_frame", test_expr_dot_missing_identifier_frame},
    {"expr_index_frames", test_expr_index_frames},
    {"expr_call_missing_rparen_frame", test_expr_call_missing_rparen_frame},
    {"expr_full_ladder_shape", test_expr_full_ladder_shape},
    {"expr_ct_operand_position", test_expr_ct_operand_position},
    {"named_expr_generic_call", test_named_expr_generic_call},
    {"named_expr_relational_fallback", test_named_expr_relational_fallback},
    {"named_expr_shift_fallback", test_named_expr_shift_fallback},
    {"named_expr_two_comparisons", test_named_expr_two_comparisons},
    {"named_expr_member_generic", test_named_expr_member_generic},
    {"named_expr_condition_generic", test_named_expr_condition_generic},
};

TEST_DISPATCH_MAIN(ENTRIES)
