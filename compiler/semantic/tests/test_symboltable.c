/**
 * @file test_symboltable.c
 * @brief Unit tests for the immutable symbol table.
 * @author solid-matrix
 * @version 0.0.5
 */

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "arena.h"
#include "semantic_symboltable.h"
#include "syntax_node.h"
#include "test_support.h"

static SemanticNamePath *path_of(Arena *arena, int count, ...) {
  SemanticNamePath *head = NULL;
  SemanticNamePath *tail = NULL;
  va_list args;
  va_start(args, count);
  for (int i = 0; i < count; i++) {
    SemanticNamePath *segment = arena_alloc(arena, sizeof *segment);
    segment->name = strview_from_cstr(va_arg(args, const char *));
    segment->next = NULL;
    if (tail == NULL)
      head = segment;
    else
      tail->next = segment;
    tail = segment;
  }
  va_end(args);
  return head;
}

static SyntaxNode *decl_node(Arena *arena, int tag) {
  SyntaxNode *node = arena_alloc(arena, sizeof *node);
  *node = syntax_node_create(SYNTAX_KIND_FUNC_DECL, (Span){.start = (size_t)tag, .end = (size_t)tag + 1});
  return node;
}

void test_create_empty_lookup_miss(void) {
  Arena *a = arena_create();
  SemanticSymbolTable *t = semantic_symboltable_create(a);

  TEST_ASSERT_NOT_NULL(t);
  TEST_ASSERT_NULL(semantic_symboltable_lookup(t, path_of(a, 1, "X")));
  TEST_ASSERT_NULL(semantic_symboltable_sub(t, path_of(a, 1, "a")));

  arena_destroy(a);
}

void test_define_and_lookup_hit(void) {
  Arena *a = arena_create();
  SemanticSymbolTable *t = semantic_symboltable_create(a);
  SyntaxNode *n = decl_node(a, 1);

  SemanticSymbolTable *t1 = semantic_symboltable_define(t, path_of(a, 1, "X"), n);
  TEST_ASSERT_NOT_NULL(t1);
  TEST_ASSERT_TRUE(semantic_symboltable_lookup(t1, path_of(a, 1, "X")) == n);

  arena_destroy(a);
}

void test_define_duplicate_returns_null_table_unchanged(void) {
  Arena *a = arena_create();
  SemanticSymbolTable *t = semantic_symboltable_create(a);
  SyntaxNode *first = decl_node(a, 1);

  SemanticSymbolTable *t1 = semantic_symboltable_define(t, path_of(a, 1, "X"), first);
  TEST_ASSERT_NOT_NULL(t1);
  TEST_ASSERT_NULL(semantic_symboltable_define(t1, path_of(a, 1, "X"), decl_node(a, 2)));
  TEST_ASSERT_TRUE(semantic_symboltable_lookup(t1, path_of(a, 1, "X")) == first);

  arena_destroy(a);
}

void test_define_persistence_old_table_unchanged(void) {
  Arena *a = arena_create();
  SemanticSymbolTable *t = semantic_symboltable_create(a);
  SyntaxNode *n = decl_node(a, 1);

  SemanticSymbolTable *t1 = semantic_symboltable_define(t, path_of(a, 1, "X"), n);
  TEST_ASSERT_NOT_NULL(t1);
  TEST_ASSERT_NULL(semantic_symboltable_lookup(t, path_of(a, 1, "X")));
  TEST_ASSERT_TRUE(semantic_symboltable_lookup(t1, path_of(a, 1, "X")) == n);

  arena_destroy(a);
}

void test_define_nested_path_scoping(void) {
  Arena *a = arena_create();
  SemanticSymbolTable *t = semantic_symboltable_create(a);
  SyntaxNode *n = decl_node(a, 1);

  SemanticSymbolTable *t1 = semantic_symboltable_define(t, path_of(a, 3, "a", "b", "X"), n);
  TEST_ASSERT_NOT_NULL(t1);
  TEST_ASSERT_TRUE(semantic_symboltable_lookup(t1, path_of(a, 3, "a", "b", "X")) == n);
  TEST_ASSERT_NULL(semantic_symboltable_lookup(t1, path_of(a, 2, "a", "X")));
  TEST_ASSERT_NULL(semantic_symboltable_lookup(t1, path_of(a, 1, "X")));
  TEST_ASSERT_NULL(semantic_symboltable_lookup(t1, path_of(a, 2, "a", "b")));

  arena_destroy(a);
}

void test_collision_symbol_and_namespace(void) {
  Arena *a = arena_create();
  SemanticSymbolTable *t = semantic_symboltable_create(a);

  SemanticSymbolTable *t1 = semantic_symboltable_define(t, path_of(a, 1, "X"), decl_node(a, 1));
  TEST_ASSERT_NOT_NULL(t1);
  TEST_ASSERT_NULL(semantic_symboltable_define(t1, path_of(a, 1, "X"), NULL));

  SemanticSymbolTable *t2 = semantic_symboltable_define(t, path_of(a, 1, "Y"), NULL);
  TEST_ASSERT_NOT_NULL(t2);
  TEST_ASSERT_NULL(semantic_symboltable_define(t2, path_of(a, 1, "Y"), decl_node(a, 2)));
  TEST_ASSERT_NULL(semantic_symboltable_lookup(t2, path_of(a, 1, "Y")));

  arena_destroy(a);
}

void test_namespace_materializes_empty(void) {
  Arena *a = arena_create();
  SemanticSymbolTable *t = semantic_symboltable_create(a);

  SemanticSymbolTable *t1 = semantic_symboltable_define(t, path_of(a, 2, "a", "b"), NULL);
  TEST_ASSERT_NOT_NULL(t1);
  const SemanticSymbolTable *view = semantic_symboltable_sub(t1, path_of(a, 2, "a", "b"));
  TEST_ASSERT_NOT_NULL(view);
  TEST_ASSERT_NULL(semantic_symboltable_lookup(view, path_of(a, 1, "X")));
  TEST_ASSERT_NULL(semantic_symboltable_lookup(t1, path_of(a, 2, "a", "b")));

  arena_destroy(a);
}

void test_namespace_path_through_symbol_fails(void) {
  Arena *a = arena_create();
  SemanticSymbolTable *t = semantic_symboltable_create(a);

  SemanticSymbolTable *t1 = semantic_symboltable_define(t, path_of(a, 1, "X"), decl_node(a, 1));
  TEST_ASSERT_NOT_NULL(t1);
  TEST_ASSERT_NULL(semantic_symboltable_define(t1, path_of(a, 2, "X", "z"), NULL));

  arena_destroy(a);
}

void test_namespace_redefine_reuses_entry(void) {
  Arena *a = arena_create();
  SemanticSymbolTable *t = semantic_symboltable_create(a);
  SyntaxNode *n = decl_node(a, 1);

  SemanticSymbolTable *t1 = semantic_symboltable_define(t, path_of(a, 2, "a", "b"), NULL);
  SemanticSymbolTable *t2 = semantic_symboltable_define(t1, path_of(a, 3, "a", "b", "X"), n);
  SemanticSymbolTable *t3 = semantic_symboltable_define(t2, path_of(a, 2, "a", "b"), NULL);
  TEST_ASSERT_NOT_NULL(t1);
  TEST_ASSERT_NOT_NULL(t2);
  TEST_ASSERT_NOT_NULL(t3);
  TEST_ASSERT_TRUE(semantic_symboltable_lookup(t3, path_of(a, 3, "a", "b", "X")) == n);
  TEST_ASSERT_NULL(semantic_symboltable_lookup(t1, path_of(a, 3, "a", "b", "X")));

  arena_destroy(a);
}

void test_sub_view_lookup_probes(void) {
  Arena *a = arena_create();
  SemanticSymbolTable *t = semantic_symboltable_create(a);
  SyntaxNode *n = decl_node(a, 1);

  SemanticSymbolTable *t1 = semantic_symboltable_define(t, path_of(a, 3, "a", "b", "X"), n);
  TEST_ASSERT_NOT_NULL(t1);
  const SemanticSymbolTable *view = semantic_symboltable_sub(t1, path_of(a, 2, "a", "b"));
  TEST_ASSERT_NOT_NULL(view);
  TEST_ASSERT_TRUE(semantic_symboltable_lookup(view, path_of(a, 1, "X")) == n);
  TEST_ASSERT_NULL(semantic_symboltable_lookup(view, path_of(a, 1, "Y")));

  arena_destroy(a);
}

void test_sub_unknown_segment_returns_null(void) {
  Arena *a = arena_create();
  SemanticSymbolTable *t = semantic_symboltable_create(a);

  SemanticSymbolTable *t1 = semantic_symboltable_define(t, path_of(a, 3, "a", "b", "X"), decl_node(a, 1));
  TEST_ASSERT_NOT_NULL(t1);
  TEST_ASSERT_NULL(semantic_symboltable_sub(t1, path_of(a, 2, "zz", "a")));
  TEST_ASSERT_NULL(semantic_symboltable_sub(t1, path_of(a, 1, "zz")));

  arena_destroy(a);
}

void test_sub_rejects_symbol_leaf(void) {
  Arena *a = arena_create();
  SemanticSymbolTable *t = semantic_symboltable_create(a);

  SemanticSymbolTable *t1 = semantic_symboltable_define(t, path_of(a, 3, "a", "b", "X"), decl_node(a, 1));
  TEST_ASSERT_NOT_NULL(t1);
  TEST_ASSERT_NULL(semantic_symboltable_sub(t1, path_of(a, 3, "a", "b", "X")));
  TEST_ASSERT_NULL(semantic_symboltable_sub(t1, path_of(a, 1, "X")));

  arena_destroy(a);
}

void test_sub_chained_views(void) {
  Arena *a = arena_create();
  SemanticSymbolTable *t = semantic_symboltable_create(a);
  SyntaxNode *n = decl_node(a, 1);

  SemanticSymbolTable *t1 = semantic_symboltable_define(t, path_of(a, 3, "a", "b", "X"), n);
  TEST_ASSERT_NOT_NULL(t1);
  const SemanticSymbolTable *va = semantic_symboltable_sub(t1, path_of(a, 1, "a"));
  const SemanticSymbolTable *vb = semantic_symboltable_sub(va, path_of(a, 1, "b"));
  TEST_ASSERT_NOT_NULL(va);
  TEST_ASSERT_NOT_NULL(vb);
  TEST_ASSERT_TRUE(semantic_symboltable_lookup(vb, path_of(a, 1, "X")) == n);
  TEST_ASSERT_TRUE(semantic_symboltable_sub(t1, NULL) == t1);

  arena_destroy(a);
}

static const char *const MANY_NAMES[] = {
    "alpha",  "beta",   "gamma", "delta", "epsilon", "zeta",  "eta",  "theta", "iota",  "kappa",
    "lambda", "mu",     "nu",    "xi",    "omicron", "pi",    "rho",  "sigma", "tau",   "upsilon",
    "phi",    "chi",    "psi",   "omega", "foo",     "bar",   "baz",  "qux",   "quux",  "corge",
    "grault", "garply", "waldo", "fred",  "plugh",   "xyzzy", "thud", "main",  "print", "value",
};

static int compare_by_name(const void *l, const void *r) {
  return strcmp(MANY_NAMES[*(const size_t *)l], MANY_NAMES[*(const size_t *)r]);
}

static void define_and_verify(SemanticSymbolTable **table, Arena *arena, const size_t *order, size_t count) {
  SyntaxNode *nodes[64];
  for (size_t k = 0; k < count; k++) {
    nodes[k] = decl_node(arena, (int)k);
    SemanticSymbolTable *next = semantic_symboltable_define(*table, path_of(arena, 1, MANY_NAMES[order[k]]), nodes[k]);
    TEST_ASSERT_NOT_NULL(next);
    *table = next;
  }
  for (size_t k = 0; k < count; k++)
    TEST_ASSERT_TRUE(semantic_symboltable_lookup(*table, path_of(arena, 1, MANY_NAMES[order[k]])) == nodes[k]);
  TEST_ASSERT_NULL(semantic_symboltable_lookup(*table, path_of(arena, 1, "zzz_missing")));
}

void test_define_many_orders_all_lookups(void) {
  const size_t count = sizeof MANY_NAMES / sizeof MANY_NAMES[0];
  size_t natural[64], sorted[64], reversed[64];
  for (size_t i = 0; i < count; i++)
    natural[i] = i;
  memcpy(sorted, natural, count * sizeof sorted[0]);
  qsort(sorted, count, sizeof sorted[0], compare_by_name);
  for (size_t i = 0; i < count; i++)
    reversed[i] = sorted[count - 1 - i];

  const size_t *orders[] = {natural, sorted, reversed};
  for (size_t o = 0; o < sizeof orders / sizeof orders[0]; o++) {
    Arena *a = arena_create();
    SemanticSymbolTable *t = semantic_symboltable_create(a);
    define_and_verify(&t, a, orders[o], count);
    arena_destroy(a);
  }
}

void test_deterministic_behavior_across_orders(void) {
  const size_t count = sizeof MANY_NAMES / sizeof MANY_NAMES[0];
  Arena *a = arena_create();
  SemanticSymbolTable *t1 = semantic_symboltable_create(a);
  SemanticSymbolTable *t2 = semantic_symboltable_create(a);
  SyntaxNode *by_name[64];

  size_t natural[64], reversed[64];
  for (size_t i = 0; i < count; i++) {
    natural[i] = i;
    reversed[count - 1 - i] = i;
    by_name[i] = decl_node(a, (int)i);
  }

  for (size_t k = 0; k < count; k++) {
    t1 = semantic_symboltable_define(t1, path_of(a, 1, MANY_NAMES[natural[k]]), by_name[natural[k]]);
    t2 = semantic_symboltable_define(t2, path_of(a, 1, MANY_NAMES[reversed[k]]), by_name[reversed[k]]);
    TEST_ASSERT_NOT_NULL(t1);
    TEST_ASSERT_NOT_NULL(t2);
  }
  for (size_t i = 0; i < count; i++) {
    TEST_ASSERT_TRUE(semantic_symboltable_lookup(t1, path_of(a, 1, MANY_NAMES[i])) == by_name[i]);
    TEST_ASSERT_TRUE(semantic_symboltable_lookup(t2, path_of(a, 1, MANY_NAMES[i])) == by_name[i]);
  }
  TEST_ASSERT_NULL(semantic_symboltable_lookup(t1, path_of(a, 1, "zzz_missing")));
  TEST_ASSERT_NULL(semantic_symboltable_lookup(t2, path_of(a, 1, "zzz_missing")));

  arena_destroy(a);
}

static const TestDispatchEntry ENTRIES[] = {
    {"create_empty_lookup_miss", test_create_empty_lookup_miss},
    {"define_and_lookup_hit", test_define_and_lookup_hit},
    {"define_duplicate_returns_null_table_unchanged", test_define_duplicate_returns_null_table_unchanged},
    {"define_persistence_old_table_unchanged", test_define_persistence_old_table_unchanged},
    {"define_nested_path_scoping", test_define_nested_path_scoping},
    {"collision_symbol_and_namespace", test_collision_symbol_and_namespace},
    {"namespace_materializes_empty", test_namespace_materializes_empty},
    {"namespace_path_through_symbol_fails", test_namespace_path_through_symbol_fails},
    {"namespace_redefine_reuses_entry", test_namespace_redefine_reuses_entry},
    {"sub_view_lookup_probes", test_sub_view_lookup_probes},
    {"sub_unknown_segment_returns_null", test_sub_unknown_segment_returns_null},
    {"sub_rejects_symbol_leaf", test_sub_rejects_symbol_leaf},
    {"sub_chained_views", test_sub_chained_views},
    {"define_many_orders_all_lookups", test_define_many_orders_all_lookups},
    {"deterministic_behavior_across_orders", test_deterministic_behavior_across_orders},
};

TEST_DISPATCH_MAIN(ENTRIES)
