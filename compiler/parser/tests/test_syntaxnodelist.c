/**
 * @file test_syntaxnodelist.c
 * @brief Tests for the functional node list.
 * @author solid-matrix
 * @version 0.0.5
 */

#include "arena.h"
#include "syntax_node.h"
#include "test_util.h"

static SyntaxNode *node(Arena *a) {
  // A dummy payload: only the pointer identity matters in these tests.
  return arena_alloc(a, sizeof(SyntaxNode));
}

static void test_empty(void) {
  Arena *a = arena_create();

  CHECK(syntax_nodelist_empty() == NULL);
  CHECK(syntax_nodelist_is_empty(NULL));
  CHECK(syntax_nodelist_length(NULL) == 0);
  CHECK(syntax_nodelist_reverse(a, NULL) == NULL);
  CHECK(syntax_nodelist_concat(a, NULL, NULL) == NULL);

  arena_destroy(a);
}

static void test_from_array(void) {
  Arena *a = arena_create();
  SyntaxNode *n1 = node(a), *n2 = node(a), *n3 = node(a);
  SyntaxNode *nodes[] = {n1, n2, n3};

  SyntaxNodeList *l = syntax_nodelist_from_array(a, nodes, 3);
  CHECK(!syntax_nodelist_is_empty(l));
  CHECK(syntax_nodelist_length(l) == 3);
  CHECK(syntax_nodelist_head(l) == n1);
  CHECK(syntax_nodelist_at(l, 1) == n2);
  CHECK(syntax_nodelist_at(l, 2) == n3);

  CHECK(syntax_nodelist_from_array(a, nodes, 0) == NULL);

  arena_destroy(a);
}

static void test_persistence(void) {
  Arena *a = arena_create();
  SyntaxNode *n1 = node(a), *n2 = node(a), *n3 = node(a);

  SyntaxNodeList *one = syntax_nodelist_prepend(a, NULL, n1); // [1]
  CHECK(syntax_nodelist_length(one) == 1 && syntax_nodelist_head(one) == n1);

  SyntaxNodeList *two = syntax_nodelist_prepend(a, one, n2); // [2,1]
  CHECK(two->next == one);                                   // shares spine
  CHECK(syntax_nodelist_head(one) == n1 && one->next == NULL);

  SyntaxNodeList *three = syntax_nodelist_append(a, two, n3); // [2,1,3]
  CHECK(syntax_nodelist_length(three) == 3);
  CHECK(syntax_nodelist_at(three, 0) == n2);
  CHECK(syntax_nodelist_at(three, 1) == n1);
  CHECK(syntax_nodelist_at(three, 2) == n3);

  // Sources remain valid and unchanged after append.
  CHECK(syntax_nodelist_head(two) == n2);
  CHECK(syntax_nodelist_at(two, 1) == n1 && two->next->next == NULL);

  arena_destroy(a);
}

static void test_access(void) {
  Arena *a = arena_create();
  SyntaxNode *n1 = node(a), *n2 = node(a), *n3 = node(a);
  SyntaxNode *nodes[] = {n1, n2, n3};
  SyntaxNodeList *l = syntax_nodelist_from_array(a, nodes, 3);

  CHECK(syntax_nodelist_head(l) == n1);

  SyntaxNodeList *rest = syntax_nodelist_tail(l); // [2,3]
  CHECK(syntax_nodelist_head(rest) == n2);
  CHECK(syntax_nodelist_tail(rest)->next == NULL);

  CHECK(syntax_nodelist_at(l, 0) == n1);
  CHECK(syntax_nodelist_at(l, 2) == n3);

  arena_destroy(a);
}

static void test_reverse(void) {
  Arena *a = arena_create();
  SyntaxNode *n1 = node(a), *n2 = node(a), *n3 = node(a);
  SyntaxNode *nodes[] = {n1, n2, n3};
  SyntaxNodeList *l = syntax_nodelist_from_array(a, nodes, 3);

  SyntaxNodeList *r = syntax_nodelist_reverse(a, l);
  CHECK(syntax_nodelist_length(r) == 3);
  CHECK(syntax_nodelist_at(r, 0) == n3);
  CHECK(syntax_nodelist_at(r, 1) == n2);
  CHECK(syntax_nodelist_at(r, 2) == n1);

  // Source untouched.
  CHECK(syntax_nodelist_head(l) == n1);
  CHECK(syntax_nodelist_at(l, 2) == n3);

  arena_destroy(a);
}

static void test_concat(void) {
  Arena *a = arena_create();
  SyntaxNode *n1 = node(a), *n2 = node(a), *n3 = node(a);
  SyntaxNode *ab[] = {n1, n2};
  SyntaxNodeList *lhs = syntax_nodelist_from_array(a, ab, 2); // [1,2]
  SyntaxNodeList *rhs = syntax_nodelist_prepend(a, NULL, n3); // [3]

  SyntaxNodeList *joined = syntax_nodelist_concat(a, lhs, rhs); // [1,2,3]
  CHECK(syntax_nodelist_length(joined) == 3);
  CHECK(syntax_nodelist_at(joined, 0) == n1);
  CHECK(syntax_nodelist_at(joined, 2) == n3);
  CHECK(joined->next->next == rhs); // right operand shared wholesale

  // Left operand unchanged.
  CHECK(syntax_nodelist_head(lhs) == n1);
  CHECK(syntax_nodelist_at(lhs, 1) == n2);
  CHECK(lhs->next->next == NULL);

  CHECK(syntax_nodelist_concat(a, lhs, NULL)->next->next == NULL);
  CHECK(syntax_nodelist_concat(a, NULL, rhs) == rhs);

  arena_destroy(a);
}

static void test_length(void) {
  Arena *a = arena_create();
  SyntaxNode *n1 = node(a);

  CHECK(syntax_nodelist_length(NULL) == 0);
  CHECK(syntax_nodelist_length(syntax_nodelist_prepend(a, NULL, n1)) == 1);

  arena_destroy(a);
}

static const TestEntry k_tests[] = {
    {"empty", test_empty},
    {"from_array", test_from_array},
    {"persistence", test_persistence},
    {"access", test_access},
    {"reverse", test_reverse},
    {"concat", test_concat},
    {"length", test_length},
};

TEST_MAIN(k_tests)
