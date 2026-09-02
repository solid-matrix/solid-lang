#include "arena.h"
#include "syntax_error.h"
#include "test_support.h"

static SyntaxError err(int code, int start) {
  return syntax_error_create((SyntaxErrorCode)code, (Span){.start = start, .end = start + 1});
}

static bool err_equals(SyntaxError e, int code, int start) {
  return e.code == (SyntaxErrorCode)code && e.span.start == (size_t)start;
}

void test_errorlist_empty(void) {
  Arena *a = arena_create();

  TEST_ASSERT_NULL(syntax_errorlist_empty());
  TEST_ASSERT_TRUE(syntax_errorlist_is_empty(NULL));
  TEST_ASSERT_EQUAL_size_t(0, syntax_errorlist_length(NULL));
  TEST_ASSERT_NULL(syntax_errorlist_reverse(a, NULL));
  TEST_ASSERT_NULL(syntax_errorlist_concat(a, NULL, NULL));

  arena_destroy(a);
}

void test_errorlist_from_array(void) {
  Arena *a = arena_create();
  SyntaxError errors[] = {err(1, 10), err(2, 20), err(3, 30)};

  SyntaxErrorList *l = syntax_errorlist_from_array(a, errors, 3);
  TEST_ASSERT_FALSE(syntax_errorlist_is_empty(l));
  TEST_ASSERT_EQUAL_size_t(3, syntax_errorlist_length(l));
  TEST_ASSERT_TRUE(err_equals(l->head, 1, 10));
  TEST_ASSERT_TRUE(err_equals(syntax_errorlist_at(l, 1), 2, 20));
  TEST_ASSERT_TRUE(err_equals(syntax_errorlist_at(l, 2), 3, 30));

  TEST_ASSERT_NULL(syntax_errorlist_from_array(a, errors, 0));

  arena_destroy(a);
}

void test_errorlist_persistence(void) {
  Arena *a = arena_create();

  SyntaxErrorList *one = syntax_errorlist_prepend(a, NULL, err(1, 10));
  TEST_ASSERT_EQUAL_size_t(1, syntax_errorlist_length(one));

  SyntaxErrorList *two = syntax_errorlist_prepend(a, one, err(2, 20));
  TEST_ASSERT_EQUAL_PTR(one, two->tail); // shares spine
  TEST_ASSERT_TRUE(err_equals(one->head, 1, 10));
  TEST_ASSERT_NULL(one->tail);

  SyntaxErrorList *three = syntax_errorlist_append(a, two, err(3, 30)); // [2,1,3]
  TEST_ASSERT_EQUAL_size_t(3, syntax_errorlist_length(three));
  TEST_ASSERT_TRUE(err_equals(syntax_errorlist_at(three, 0), 2, 20));
  TEST_ASSERT_TRUE(err_equals(syntax_errorlist_at(three, 1), 1, 10));
  TEST_ASSERT_TRUE(err_equals(syntax_errorlist_at(three, 2), 3, 30));

  // Sources remain valid and unchanged after append.
  TEST_ASSERT_TRUE(err_equals(two->head, 2, 20));
  TEST_ASSERT_TRUE(err_equals(syntax_errorlist_at(two, 1), 1, 10));
  TEST_ASSERT_NULL(two->tail->tail);

  arena_destroy(a);
}

void test_errorlist_reverse(void) {
  Arena *a = arena_create();
  SyntaxError errors[] = {err(1, 10), err(2, 20), err(3, 30)};
  SyntaxErrorList *l = syntax_errorlist_from_array(a, errors, 3);

  SyntaxErrorList *r = syntax_errorlist_reverse(a, l);
  TEST_ASSERT_EQUAL_size_t(3, syntax_errorlist_length(r));
  TEST_ASSERT_TRUE(err_equals(syntax_errorlist_at(r, 0), 3, 30));
  TEST_ASSERT_TRUE(err_equals(syntax_errorlist_at(r, 2), 1, 10));

  // Source untouched.
  TEST_ASSERT_TRUE(err_equals(l->head, 1, 10));

  arena_destroy(a);
}

void test_errorlist_concat(void) {
  Arena *a = arena_create();
  SyntaxError ab[] = {err(1, 10), err(2, 20)};
  SyntaxErrorList *lhs = syntax_errorlist_from_array(a, ab, 2);
  SyntaxErrorList *rhs = syntax_errorlist_prepend(a, NULL, err(3, 30));

  SyntaxErrorList *joined = syntax_errorlist_concat(a, lhs, rhs);
  TEST_ASSERT_EQUAL_size_t(3, syntax_errorlist_length(joined));
  TEST_ASSERT_TRUE(err_equals(joined->head, 1, 10));
  TEST_ASSERT_TRUE(err_equals(syntax_errorlist_at(joined, 2), 3, 30));
  TEST_ASSERT_EQUAL_PTR(rhs, joined->tail->tail); // shares b wholesale

  // Left operand unchanged.
  TEST_ASSERT_TRUE(err_equals(lhs->head, 1, 10));
  TEST_ASSERT_NULL(lhs->tail->tail);

  TEST_ASSERT_EQUAL_PTR(rhs, syntax_errorlist_concat(a, NULL, rhs));

  arena_destroy(a);
}
static const TestDispatchEntry ENTRIES[] = {
    {"errorlist_empty", test_errorlist_empty},
    {"errorlist_from_array", test_errorlist_from_array},
    {"errorlist_persistence", test_errorlist_persistence},
    {"errorlist_reverse", test_errorlist_reverse},
    {"errorlist_concat", test_errorlist_concat},
};

TEST_DISPATCH_MAIN(ENTRIES)
