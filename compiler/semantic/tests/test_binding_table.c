/**
 * @file test_binding_table.c
 * @brief Unit tests for the use-site to entity binding table.
 * @author solid-matrix
 */

#include <stdlib.h>

#include "arena.h"
#include "binding_table.h"
#include "syntax_node.h"
#include "test_support.h"

static SyntaxNode *node_of(Arena *arena, int tag) {
  SyntaxNode *node = arena_alloc(arena, sizeof *node);
  *node = (SyntaxNode){.kind = SYNTAX_KIND_FUNC_DECL, .span = {.start = (size_t)tag, .end = (size_t)tag + 1}};
  return node;
}

void test_empty_table_lookup_miss(void) {
  Arena *a = arena_create();
  SemanticBindingTable *t = semantic_binding_table_empty();

  TEST_ASSERT_NULL(t);
  TEST_ASSERT_NULL(semantic_binding_table_lookup(t, node_of(a, 1)));
  TEST_ASSERT_EQUAL_size_t(0, semantic_binding_table_length(t));

  arena_destroy(a);
}

void test_insert_into_null_grows(void) {
  Arena *a = arena_create();
  SyntaxNode *use = node_of(a, 1);
  SyntaxNode *decl = node_of(a, 2);

  SemanticBindingTable *t = semantic_binding_table_insert(a, NULL, use, decl);
  TEST_ASSERT_NOT_NULL(t);
  TEST_ASSERT_TRUE(semantic_binding_table_lookup(t, use) == decl);

  arena_destroy(a);
}

void test_insert_and_lookup_hit(void) {
  Arena *a = arena_create();
  SemanticBindingTable *t = semantic_binding_table_empty();
  SyntaxNode *use = node_of(a, 1);
  SyntaxNode *decl = node_of(a, 2);

  SemanticBindingTable *t1 = semantic_binding_table_insert(a, t, use, decl);
  TEST_ASSERT_NOT_NULL(t1);
  TEST_ASSERT_TRUE(semantic_binding_table_lookup(t1, use) == decl);

  arena_destroy(a);
}

void test_insert_persistence_old_table_unchanged(void) {
  Arena *a = arena_create();
  SemanticBindingTable *t = semantic_binding_table_empty();
  SyntaxNode *use1 = node_of(a, 1);
  SyntaxNode *use2 = node_of(a, 2);
  SyntaxNode *decl1 = node_of(a, 3);
  SyntaxNode *decl2 = node_of(a, 4);

  SemanticBindingTable *t1 = semantic_binding_table_insert(a, t, use1, decl1);
  SemanticBindingTable *t2 = semantic_binding_table_insert(a, t1, use2, decl2);

  TEST_ASSERT_NOT_NULL(t1);
  TEST_ASSERT_NOT_NULL(t2);
  TEST_ASSERT_TRUE(t1 != t2);
  TEST_ASSERT_TRUE(semantic_binding_table_lookup(t1, use1) == decl1);
  TEST_ASSERT_NULL(semantic_binding_table_lookup(t1, use2));
  TEST_ASSERT_TRUE(semantic_binding_table_lookup(t2, use1) == decl1);
  TEST_ASSERT_TRUE(semantic_binding_table_lookup(t2, use2) == decl2);

  arena_destroy(a);
}

void test_lookup_unknown_site_miss(void) {
  Arena *a = arena_create();
  SemanticBindingTable *t = semantic_binding_table_empty();
  t = semantic_binding_table_insert(a, t, node_of(a, 1), node_of(a, 2));

  TEST_ASSERT_NULL(semantic_binding_table_lookup(t, node_of(a, 3)));

  arena_destroy(a);
}

void test_multiple_entries_all_found(void) {
  Arena *a = arena_create();
  SemanticBindingTable *t = semantic_binding_table_empty();
  SyntaxNode *uses[3], *decls[3];

  for (int i = 0; i < 3; i++) {
    uses[i] = node_of(a, i);
    decls[i] = node_of(a, i + 10);
    t = semantic_binding_table_insert(a, t, uses[i], decls[i]);
    TEST_ASSERT_NOT_NULL(t);
  }
  TEST_ASSERT_EQUAL_size_t(3, semantic_binding_table_length(t));

  for (int i = 0; i < 3; i++)
    TEST_ASSERT_TRUE(semantic_binding_table_lookup(t, uses[i]) == decls[i]);

  arena_destroy(a);
}

static const TestDispatchEntry ENTRIES[] = {
    {"empty_table_lookup_miss", test_empty_table_lookup_miss},
    {"insert_into_null_grows", test_insert_into_null_grows},
    {"insert_and_lookup_hit", test_insert_and_lookup_hit},
    {"insert_persistence_old_table_unchanged", test_insert_persistence_old_table_unchanged},
    {"lookup_unknown_site_miss", test_lookup_unknown_site_miss},
    {"multiple_entries_all_found", test_multiple_entries_all_found},
};

TEST_DISPATCH_MAIN(ENTRIES)
