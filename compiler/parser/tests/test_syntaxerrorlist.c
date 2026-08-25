/**
 * @file test_syntaxerrorlist.c
 * @brief Tests for the functional error list.
 * @author solid-matrix
 * @version 0.0.5
 */

#include "arena.h"
#include "syntax_error.h"
#include "test_util.h"

static SyntaxError err(int code, int start) {
  return syntax_error_create((SyntaxErrorCode)code,
                             (Span){.start = start, .end = start + 1});
}

static bool err_equals(SyntaxError e, int code, int start) {
  return e.code == (SyntaxErrorCode)code && e.span.start == (size_t)start;
}

static void test_empty(void) {
  Arena *a = arena_create();

  CHECK(syntax_errorlist_empty() == NULL);
  CHECK(syntax_errorlist_is_empty(NULL));
  CHECK(syntax_errorlist_length(NULL) == 0);
  CHECK(syntax_errorlist_reverse(a, NULL) == NULL);
  CHECK(syntax_errorlist_concat(a, NULL, NULL) == NULL);

  arena_destroy(a);
}

static void test_from_array(void) {
  Arena *a = arena_create();
  SyntaxError errors[] = {err(1, 10), err(2, 20), err(3, 30)};

  SyntaxErrorList *l = syntax_errorlist_from_array(a, errors, 3);
  CHECK(!syntax_errorlist_is_empty(l));
  CHECK(syntax_errorlist_length(l) == 3);
  CHECK(err_equals(syntax_errorlist_head(l), 1, 10));
  CHECK(err_equals(syntax_errorlist_at(l, 1), 2, 20));
  CHECK(err_equals(syntax_errorlist_at(l, 2), 3, 30));

  CHECK(syntax_errorlist_from_array(a, errors, 0) == NULL);

  arena_destroy(a);
}

static void test_persistence(void) {
  Arena *a = arena_create();

  SyntaxErrorList *one = syntax_errorlist_prepend(a, NULL, err(1, 10));
  CHECK(syntax_errorlist_length(one) == 1);
  CHECK(err_equals(syntax_errorlist_head(one), 1, 10));

  SyntaxErrorList *two = syntax_errorlist_prepend(a, one, err(2, 20));
  CHECK(two->next == one); // shares the old spine
  CHECK(err_equals(syntax_errorlist_head(one), 1, 10) && one->next == NULL);

  SyntaxErrorList *three =
      syntax_errorlist_append(a, two, err(3, 30)); // [2,1,3]
  CHECK(syntax_errorlist_length(three) == 3);
  CHECK(err_equals(syntax_errorlist_at(three, 0), 2, 20));
  CHECK(err_equals(syntax_errorlist_at(three, 1), 1, 10));
  CHECK(err_equals(syntax_errorlist_at(three, 2), 3, 30));

  // Sources remain valid and unchanged after append.
  CHECK(err_equals(syntax_errorlist_head(two), 2, 20));
  CHECK(err_equals(syntax_errorlist_at(two, 1), 1, 10) &&
        two->next->next == NULL);

  arena_destroy(a);
}

static void test_access(void) {
  Arena *a = arena_create();
  SyntaxError errors[] = {err(1, 10), err(2, 20), err(3, 30)};
  SyntaxErrorList *l = syntax_errorlist_from_array(a, errors, 3);

  CHECK(err_equals(syntax_errorlist_head(l), 1, 10));

  SyntaxErrorList *rest = syntax_errorlist_tail(l); // [2,3]
  CHECK(err_equals(syntax_errorlist_head(rest), 2, 20));
  CHECK(syntax_errorlist_tail(rest)->next == NULL);

  CHECK(err_equals(syntax_errorlist_at(l, 0), 1, 10));
  CHECK(err_equals(syntax_errorlist_at(l, 2), 3, 30));

  arena_destroy(a);
}

static void test_reverse(void) {
  Arena *a = arena_create();
  SyntaxError errors[] = {err(1, 10), err(2, 20), err(3, 30)};
  SyntaxErrorList *l = syntax_errorlist_from_array(a, errors, 3);

  SyntaxErrorList *r = syntax_errorlist_reverse(a, l);
  CHECK(syntax_errorlist_length(r) == 3);
  CHECK(err_equals(syntax_errorlist_at(r, 0), 3, 30));
  CHECK(err_equals(syntax_errorlist_at(r, 1), 2, 20));
  CHECK(err_equals(syntax_errorlist_at(r, 2), 1, 10));

  // Source untouched.
  CHECK(err_equals(syntax_errorlist_head(l), 1, 10));
  CHECK(err_equals(syntax_errorlist_at(l, 2), 3, 30));

  arena_destroy(a);
}

static void test_concat(void) {
  Arena *a = arena_create();
  SyntaxError ab[] = {err(1, 10), err(2, 20)};
  SyntaxErrorList *lhs = syntax_errorlist_from_array(a, ab, 2); // [1,2]
  SyntaxErrorList *rhs = syntax_errorlist_prepend(a, NULL, err(3, 30));

  SyntaxErrorList *joined = syntax_errorlist_concat(a, lhs, rhs); // [1,2,3]
  CHECK(syntax_errorlist_length(joined) == 3);
  CHECK(err_equals(syntax_errorlist_at(joined, 0), 1, 10));
  CHECK(err_equals(syntax_errorlist_at(joined, 2), 3, 30));
  CHECK(joined->next->next == rhs); // right operand shared wholesale

  // Left operand unchanged.
  CHECK(err_equals(syntax_errorlist_head(lhs), 1, 10));
  CHECK(err_equals(syntax_errorlist_at(lhs, 1), 2, 20));
  CHECK(lhs->next->next == NULL);

  CHECK(syntax_errorlist_concat(a, lhs, NULL)->next->next == NULL);
  CHECK(syntax_errorlist_concat(a, NULL, rhs) == rhs);

  arena_destroy(a);
}

static void test_length(void) {
  Arena *a = arena_create();

  CHECK(syntax_errorlist_length(NULL) == 0);
  CHECK(syntax_errorlist_length(
            syntax_errorlist_prepend(a, NULL, err(1, 10))) == 1);

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
