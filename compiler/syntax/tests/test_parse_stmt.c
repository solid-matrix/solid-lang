#define TEST_SUPPORT_NO_DEFAULT_FIXTURES

#include <string.h>

#include "parse_aux.h"
#include "parser_fixture.h"
#include "syntax_error.h"
#include "syntax_node.h"
#include "syntax_nodes.h"
#include "syntax_parses.h"
#include "test_support.h"

void setUp(void) {}
void tearDown(void) { fx_release(); }


static const SyntaxBodyStmt *as_body(const SyntaxNode *n) {
  TEST_ASSERT_NOT_NULL(n);
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_BODY_STMT, n->kind);
  return (const SyntaxBodyStmt *)n;
}

/* ---- empty / body statements ------------------------------------------ */

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

/* ---- let / set statements ---------------------------------------------- */

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

/* ---- expression statements --------------------------------------------- */

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

/* ---- break / continue / return ----------------------------------------- */

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

/* ---- if / loop / while -------------------------------------------------- */

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

/* ---- statement dispatch ------------------------------------------------- */

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

static const TestDispatchEntry ENTRIES[] = {
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
};

TEST_DISPATCH_MAIN(ENTRIES)
