#include "arena.h"
#include "syntax_node.h"
#include "test_support.h"

/* ---- syntax_node_header --------------------------------------------- */

void test_header_carries_kind_and_span(void) {
  SyntaxNode n = syntax_node_header(SYNTAX_KIND_PROGRAM,
                                    (Span){.start = 3, .end = 9});
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_PROGRAM, n.kind);
  TEST_ASSERT_EQUAL_size_t(3, n.span.start);
  TEST_ASSERT_EQUAL_size_t(9, n.span.end);
}

/* ---- nodelist -------------------------------------------------------- */

static SyntaxNode *dummy(Arena *a) {
  // Only pointer identity matters here.
  return arena_alloc(a, sizeof(SyntaxNode));
}

void test_nodelist_empty(void) {
  Arena *a = arena_create();

  TEST_ASSERT_NULL(syntax_nodelist_empty());
  TEST_ASSERT_TRUE(syntax_nodelist_is_empty(NULL));
  TEST_ASSERT_EQUAL_size_t(0, syntax_nodelist_length(NULL));
  TEST_ASSERT_NULL(syntax_nodelist_reverse(a, NULL));
  TEST_ASSERT_NULL(syntax_nodelist_concat(a, NULL, NULL));

  arena_destroy(a);
}

void test_nodelist_from_array(void) {
  Arena *a = arena_create();
  SyntaxNode *n1 = dummy(a), *n2 = dummy(a), *n3 = dummy(a);
  SyntaxNode *nodes[] = {n1, n2, n3};

  SyntaxNodeList *l = syntax_nodelist_from_array(a, nodes, 3);
  TEST_ASSERT_FALSE(syntax_nodelist_is_empty(l));
  TEST_ASSERT_EQUAL_size_t(3, syntax_nodelist_length(l));
  TEST_ASSERT_EQUAL_PTR(n1, syntax_nodelist_head(l));
  TEST_ASSERT_EQUAL_PTR(n2, syntax_nodelist_at(l, 1));
  TEST_ASSERT_EQUAL_PTR(n3, syntax_nodelist_at(l, 2));

  TEST_ASSERT_NULL(syntax_nodelist_from_array(a, nodes, 0));

  arena_destroy(a);
}

void test_nodelist_persistence(void) {
  Arena *a = arena_create();
  SyntaxNode *n1 = dummy(a), *n2 = dummy(a), *n3 = dummy(a);

  SyntaxNodeList *one = syntax_nodelist_prepend(a, NULL, n1); // [1]
  TEST_ASSERT_EQUAL_size_t(1, syntax_nodelist_length(one));

  SyntaxNodeList *two = syntax_nodelist_prepend(a, one, n2); // [2,1]
  TEST_ASSERT_EQUAL_PTR(one, two->next);                     // shares spine
  TEST_ASSERT_EQUAL_PTR(n1, syntax_nodelist_head(one));
  TEST_ASSERT_NULL(one->next);

  SyntaxNodeList *three = syntax_nodelist_append(a, two, n3); // [2,1,3]
  TEST_ASSERT_EQUAL_size_t(3, syntax_nodelist_length(three));
  TEST_ASSERT_EQUAL_PTR(n2, syntax_nodelist_at(three, 0));
  TEST_ASSERT_EQUAL_PTR(n1, syntax_nodelist_at(three, 1));
  TEST_ASSERT_EQUAL_PTR(n3, syntax_nodelist_at(three, 2));

  // Sources remain valid and unchanged after append.
  TEST_ASSERT_EQUAL_PTR(n2, syntax_nodelist_head(two));
  TEST_ASSERT_EQUAL_PTR(n1, syntax_nodelist_at(two, 1));
  TEST_ASSERT_NULL(two->next->next);

  arena_destroy(a);
}

void test_nodelist_tail(void) {
  Arena *a = arena_create();
  SyntaxNode *n1 = dummy(a), *n2 = dummy(a);
  SyntaxNode *nodes[] = {n1, n2};
  SyntaxNodeList *l = syntax_nodelist_from_array(a, nodes, 2);

  TEST_ASSERT_EQUAL_PTR(n1, syntax_nodelist_head(l));
  SyntaxNodeList *rest = syntax_nodelist_tail(l); // [2]
  TEST_ASSERT_EQUAL_PTR(n2, syntax_nodelist_head(rest));
  TEST_ASSERT_NULL(syntax_nodelist_tail(rest)); // [2] has no tail

  arena_destroy(a);
}

void test_nodelist_reverse(void) {
  Arena *a = arena_create();
  SyntaxNode *n1 = dummy(a), *n2 = dummy(a), *n3 = dummy(a);
  SyntaxNode *nodes[] = {n1, n2, n3};
  SyntaxNodeList *l = syntax_nodelist_from_array(a, nodes, 3);

  SyntaxNodeList *r = syntax_nodelist_reverse(a, l);
  TEST_ASSERT_EQUAL_size_t(3, syntax_nodelist_length(r));
  TEST_ASSERT_EQUAL_PTR(n3, syntax_nodelist_at(r, 0));
  TEST_ASSERT_EQUAL_PTR(n2, syntax_nodelist_at(r, 1));
  TEST_ASSERT_EQUAL_PTR(n1, syntax_nodelist_at(r, 2));

  // Source untouched.
  TEST_ASSERT_EQUAL_PTR(n1, syntax_nodelist_head(l));
  TEST_ASSERT_EQUAL_PTR(n3, syntax_nodelist_at(l, 2));

  arena_destroy(a);
}

void test_nodelist_concat(void) {
  Arena *a = arena_create();
  SyntaxNode *n1 = dummy(a), *n2 = dummy(a), *n3 = dummy(a);
  SyntaxNode *ab[] = {n1, n2};
  SyntaxNodeList *lhs = syntax_nodelist_from_array(a, ab, 2); // [1,2]
  SyntaxNodeList *rhs = syntax_nodelist_prepend(a, NULL, n3); // [3]

  SyntaxNodeList *joined = syntax_nodelist_concat(a, lhs, rhs); // [1,2,3]
  TEST_ASSERT_EQUAL_size_t(3, syntax_nodelist_length(joined));
  TEST_ASSERT_EQUAL_PTR(n1, syntax_nodelist_at(joined, 0));
  TEST_ASSERT_EQUAL_PTR(n3, syntax_nodelist_at(joined, 2));
  TEST_ASSERT_EQUAL_PTR(rhs, joined->next->next); // shares b wholesale

  // Left operand unchanged.
  TEST_ASSERT_EQUAL_PTR(n1, syntax_nodelist_head(lhs));
  TEST_ASSERT_EQUAL_PTR(n2, syntax_nodelist_at(lhs, 1));
  TEST_ASSERT_NULL(lhs->next->next);

  TEST_ASSERT_NULL(syntax_nodelist_concat(a, lhs, NULL)->next->next);
  TEST_ASSERT_EQUAL_PTR(rhs, syntax_nodelist_concat(a, NULL, rhs));

  arena_destroy(a);
}

void test_nodelist_length(void) {
  Arena *a = arena_create();
  SyntaxNode *n1 = dummy(a);

  TEST_ASSERT_EQUAL_size_t(0, syntax_nodelist_length(NULL));
  TEST_ASSERT_EQUAL_size_t(
      1, syntax_nodelist_length(syntax_nodelist_prepend(a, NULL, n1)));

  arena_destroy(a);
}

static const TestDispatchEntry ENTRIES[] = {
    {"header_carries_kind_and_span", test_header_carries_kind_and_span},
    {"nodelist_empty", test_nodelist_empty},
    {"nodelist_from_array", test_nodelist_from_array},
    {"nodelist_persistence", test_nodelist_persistence},
    {"nodelist_tail", test_nodelist_tail},
    {"nodelist_reverse", test_nodelist_reverse},
    {"nodelist_concat", test_nodelist_concat},
    {"nodelist_length", test_nodelist_length},
};

TEST_DISPATCH_MAIN(ENTRIES)
