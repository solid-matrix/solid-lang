/**
 * @file test_namepath.c
 * @brief Unit tests for the name path chain operations.
 * @author solid-matrix
 * @version 0.0.5
 */

#include "arena.h"
#include "namepath.h"
#include "test_support.h"

static SemanticNamePath *one(Arena *arena, const char *name) {
  return semantic_namepath_prepend(arena, semantic_namepath_empty(), strview_from_cstr(name));
}

void test_namepath_empty(void) {
  Arena *a = arena_create();

  TEST_ASSERT_NULL(semantic_namepath_empty());
  TEST_ASSERT_TRUE(semantic_namepath_is_empty(NULL));
  TEST_ASSERT_EQUAL_size_t(0, semantic_namepath_length(NULL));
  TEST_ASSERT_NULL(semantic_namepath_reverse(a, NULL));
  TEST_ASSERT_NULL(semantic_namepath_concat(a, NULL, NULL));

  arena_destroy(a);
}

void test_namepath_from_array(void) {
  Arena *a = arena_create();
  Strview names[] = {strview_from_cstr("a"), strview_from_cstr("b"), strview_from_cstr("c")};

  SemanticNamePath *p = semantic_namepath_from_array(a, names, 3);
  TEST_ASSERT_FALSE(semantic_namepath_is_empty(p));
  TEST_ASSERT_EQUAL_size_t(3, semantic_namepath_length(p));
  TEST_ASSERT_STRVIEW_EQ(semantic_namepath_head(p), "a");
  TEST_ASSERT_STRVIEW_EQ(semantic_namepath_at(p, 1), "b");
  TEST_ASSERT_STRVIEW_EQ(semantic_namepath_at(p, 2), "c");

  TEST_ASSERT_NULL(semantic_namepath_from_array(a, names, 0));

  arena_destroy(a);
}

void test_namepath_persistence(void) {
  Arena *a = arena_create();

  SemanticNamePath *one_ = one(a, "1");
  TEST_ASSERT_EQUAL_size_t(1, semantic_namepath_length(one_));

  SemanticNamePath *two = semantic_namepath_prepend(a, one_, strview_from_cstr("2"));
  TEST_ASSERT_EQUAL_PTR(one_, two->next); // shares spine
  TEST_ASSERT_STRVIEW_EQ(semantic_namepath_head(one_), "1");
  TEST_ASSERT_NULL(one_->next);

  SemanticNamePath *three = semantic_namepath_append(a, two, strview_from_cstr("3")); // [2,1,3]
  TEST_ASSERT_EQUAL_size_t(3, semantic_namepath_length(three));
  TEST_ASSERT_STRVIEW_EQ(semantic_namepath_at(three, 0), "2");
  TEST_ASSERT_STRVIEW_EQ(semantic_namepath_at(three, 1), "1");
  TEST_ASSERT_STRVIEW_EQ(semantic_namepath_at(three, 2), "3");

  // Sources remain valid and unchanged after append.
  TEST_ASSERT_STRVIEW_EQ(semantic_namepath_head(two), "2");
  TEST_ASSERT_STRVIEW_EQ(semantic_namepath_at(two, 1), "1");
  TEST_ASSERT_NULL(two->next->next);

  arena_destroy(a);
}

void test_namepath_reverse(void) {
  Arena *a = arena_create();
  Strview names[] = {strview_from_cstr("a"), strview_from_cstr("b"), strview_from_cstr("c")};
  SemanticNamePath *p = semantic_namepath_from_array(a, names, 3);

  SemanticNamePath *r = semantic_namepath_reverse(a, p);
  TEST_ASSERT_EQUAL_size_t(3, semantic_namepath_length(r));
  TEST_ASSERT_STRVIEW_EQ(semantic_namepath_at(r, 0), "c");
  TEST_ASSERT_STRVIEW_EQ(semantic_namepath_at(r, 2), "a");

  // Source untouched.
  TEST_ASSERT_STRVIEW_EQ(semantic_namepath_head(p), "a");

  arena_destroy(a);
}

void test_namepath_concat(void) {
  Arena *a = arena_create();
  Strview ab[] = {strview_from_cstr("a"), strview_from_cstr("b")};
  SemanticNamePath *lhs = semantic_namepath_from_array(a, ab, 2);
  SemanticNamePath *rhs = one(a, "c");

  SemanticNamePath *joined = semantic_namepath_concat(a, lhs, rhs);
  TEST_ASSERT_EQUAL_size_t(3, semantic_namepath_length(joined));
  TEST_ASSERT_STRVIEW_EQ(semantic_namepath_head(joined), "a");
  TEST_ASSERT_STRVIEW_EQ(semantic_namepath_at(joined, 2), "c");
  TEST_ASSERT_EQUAL_PTR(rhs, joined->next->next); // shares b wholesale

  // Left operand unchanged.
  TEST_ASSERT_STRVIEW_EQ(semantic_namepath_head(lhs), "a");
  TEST_ASSERT_NULL(lhs->next->next);

  TEST_ASSERT_EQUAL_PTR(rhs, semantic_namepath_concat(a, NULL, rhs));

  arena_destroy(a);
}

static const TestDispatchEntry ENTRIES[] = {
    {"namepath_empty", test_namepath_empty},
    {"namepath_from_array", test_namepath_from_array},
    {"namepath_persistence", test_namepath_persistence},
    {"namepath_reverse", test_namepath_reverse},
    {"namepath_concat", test_namepath_concat},
};

TEST_DISPATCH_MAIN(ENTRIES)
