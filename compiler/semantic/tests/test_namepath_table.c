/**
 * @file test_namepath_table.c
 * @brief Unit tests for the declaration to qualified path reverse table.
 * @author solid-matrix
 */

#include <stdarg.h>
#include <stdlib.h>

#include "arena.h"
#include "namepath_table.h"
#include "syntax_node.h"
#include "test_support.h"

static SyntaxNode *decl_node(Arena *arena, int tag) {
  SyntaxNode *node = arena_alloc(arena, sizeof *node);
  *node = (SyntaxNode){.kind = SYNTAX_KIND_FUNC_DECL, .span = {.start = (size_t)tag, .end = (size_t)tag + 1}};
  return node;
}

static SemanticNamePath *path_vof(Arena *arena, int count, va_list args) {
  SemanticNamePath *head = NULL;
  SemanticNamePath *walk = NULL;
  for (int i = 0; i < count; i++) {
    SemanticNamePath *cell = arena_alloc(arena, sizeof *cell);
    cell->head = strview_from_cstr(va_arg(args, const char *));
    cell->tail = NULL;
    if (walk == NULL)
      head = cell;
    else
      walk->tail = cell;
    walk = cell;
  }
  return head;
}

static SemanticNamePath *path_of(Arena *arena, int count, ...) {
  va_list args;
  va_start(args, count);
  SemanticNamePath *path = path_vof(arena, count, args);
  va_end(args);
  return path;
}

void test_empty_table_lookup_miss(void) {
  Arena *a = arena_create();
  SemanticNamePathTable *t = semantic_namepath_table_empty();

  TEST_ASSERT_NULL(t);
  TEST_ASSERT_NULL(semantic_namepath_table_lookup(t, decl_node(a, 1)));
  TEST_ASSERT_EQUAL_size_t(0, semantic_namepath_table_length(t));

  arena_destroy(a);
}

void test_insert_into_null_grows(void) {
  Arena *a = arena_create();
  SyntaxNode *decl = decl_node(a, 1);
  SemanticNamePath *path = path_of(a, 2, "app", "T");

  SemanticNamePathTable *t = semantic_namepath_table_insert(a, NULL, decl, path);
  TEST_ASSERT_NOT_NULL(t);
  TEST_ASSERT_TRUE(semantic_namepath_table_lookup(t, decl) == path);

  arena_destroy(a);
}

void test_insert_and_lookup_hit(void) {
  Arena *a = arena_create();
  SemanticNamePathTable *t = semantic_namepath_table_empty();
  SyntaxNode *decl = decl_node(a, 1);
  SemanticNamePath *path = path_of(a, 3, "app", "x", "T");

  SemanticNamePathTable *t1 = semantic_namepath_table_insert(a, t, decl, path);
  TEST_ASSERT_NOT_NULL(t1);
  TEST_ASSERT_TRUE(semantic_namepath_table_lookup(t1, decl) == path);

  arena_destroy(a);
}

void test_insert_persistence_old_table_unchanged(void) {
  Arena *a = arena_create();
  SemanticNamePathTable *t = semantic_namepath_table_empty();
  SyntaxNode *d1 = decl_node(a, 1);
  SyntaxNode *d2 = decl_node(a, 2);

  SemanticNamePathTable *t1 = semantic_namepath_table_insert(a, t, d1, path_of(a, 1, "T"));
  SemanticNamePathTable *t2 = semantic_namepath_table_insert(a, t1, d2, path_of(a, 2, "app", "U"));

  TEST_ASSERT_NOT_NULL(t1);
  TEST_ASSERT_NOT_NULL(t2);
  TEST_ASSERT_TRUE(t1 != t2);
  TEST_ASSERT_TRUE(semantic_namepath_table_lookup(t1, d1) != NULL);
  TEST_ASSERT_NULL(semantic_namepath_table_lookup(t1, d2));
  TEST_ASSERT_TRUE(semantic_namepath_table_lookup(t2, d1) != NULL);
  TEST_ASSERT_TRUE(semantic_namepath_table_lookup(t2, d2) != NULL);

  arena_destroy(a);
}

void test_lookup_unknown_decl_miss(void) {
  Arena *a = arena_create();
  SemanticNamePathTable *t = semantic_namepath_table_empty();
  t = semantic_namepath_table_insert(a, t, decl_node(a, 1), path_of(a, 1, "T"));

  TEST_ASSERT_NOT_NULL(t);
  TEST_ASSERT_NULL(semantic_namepath_table_lookup(t, decl_node(a, 2)));

  arena_destroy(a);
}

void test_multiple_entries_all_found(void) {
  Arena *a = arena_create();
  SemanticNamePathTable *t = semantic_namepath_table_empty();
  SyntaxNode *decls[3];
  SemanticNamePath *paths[3];

  for (int i = 0; i < 3; i++) {
    decls[i] = decl_node(a, i);
    paths[i] = path_of(a, 2, "app", (i == 0 ? "A" : i == 1 ? "B" : "C"));
    t = semantic_namepath_table_insert(a, t, decls[i], paths[i]);
    TEST_ASSERT_NOT_NULL(t);
  }
  TEST_ASSERT_EQUAL_size_t(3, semantic_namepath_table_length(t));

  for (int i = 0; i < 3; i++)
    TEST_ASSERT_TRUE(semantic_namepath_table_lookup(t, decls[i]) == paths[i]);

  arena_destroy(a);
}

static const TestDispatchEntry ENTRIES[] = {
    {"empty_table_lookup_miss", test_empty_table_lookup_miss},
    {"insert_into_null_grows", test_insert_into_null_grows},
    {"insert_and_lookup_hit", test_insert_and_lookup_hit},
    {"insert_persistence_old_table_unchanged", test_insert_persistence_old_table_unchanged},
    {"lookup_unknown_decl_miss", test_lookup_unknown_decl_miss},
    {"multiple_entries_all_found", test_multiple_entries_all_found},
};

TEST_DISPATCH_MAIN(ENTRIES)
