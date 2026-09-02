/**
 * @file test_symbol_table.c
 * @brief Unit tests for the immutable symbol table.
 * @author solid-matrix
 */

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "arena.h"
#include "namepath.h"
#include "symbol_table.h"
#include "syntax_node.h"
#include "test_support.h"

static SemanticNamePath *path_of(Arena *arena, int count, ...) {
  SemanticNamePath *head = NULL;
  SemanticNamePath *walk = NULL;
  va_list args;
  va_start(args, count);
  for (int i = 0; i < count; i++) {
    SemanticNamePath *segment = arena_alloc(arena, sizeof *segment);
    segment->head = strview_from_cstr(va_arg(args, const char *));
    segment->tail = NULL;
    if (walk == NULL)
      head = segment;
    else
      walk->tail = segment;
    walk = segment;
  }
  va_end(args);
  return head;
}

static SyntaxNode *decl_node(Arena *arena, int tag) {
  SyntaxNode *node = arena_alloc(arena, sizeof *node);
  *node = (SyntaxNode){.kind = SYNTAX_KIND_FUNC_DECL, .span = {.start = (size_t)tag, .end = (size_t)tag + 1}};
  return node;
}

void test_empty_table_lookup_miss(void) {
  Arena *a = arena_create();
  SemanticSymbolTable *t = semantic_symbol_table_empty();

  TEST_ASSERT_NULL(t);
  TEST_ASSERT_NULL(semantic_symbol_table_lookup(t, path_of(a, 1, "X")));
  TEST_ASSERT_NULL(semantic_symbol_table_subtable(t, path_of(a, 1, "a")));

  arena_destroy(a);
}

void test_insert_into_null_grows(void) {
  Arena *a = arena_create();
  SyntaxNode *n = decl_node(a, 1);

  SemanticSymbolTable *t = semantic_symbol_table_insert(a, NULL, path_of(a, 1, "T"), n);
  TEST_ASSERT_NOT_NULL(t);
  TEST_ASSERT_TRUE(semantic_symbol_table_lookup(t, path_of(a, 1, "T")) == n);

  arena_destroy(a);
}

void test_define_and_lookup_hit(void) {
  Arena *a = arena_create();
  SemanticSymbolTable *t = semantic_symbol_table_empty();
  SyntaxNode *n = decl_node(a, 1);

  SemanticSymbolTable *t1 = semantic_symbol_table_insert(a, t, path_of(a, 1, "X"), n);
  TEST_ASSERT_NOT_NULL(t1);
  TEST_ASSERT_TRUE(semantic_symbol_table_lookup(t1, path_of(a, 1, "X")) == n);

  arena_destroy(a);
}

void test_define_duplicate_returns_null_table_unchanged(void) {
  Arena *a = arena_create();
  SemanticSymbolTable *t = semantic_symbol_table_empty();
  SyntaxNode *first = decl_node(a, 1);

  SemanticSymbolTable *t1 = semantic_symbol_table_insert(a, t, path_of(a, 1, "X"), first);
  TEST_ASSERT_NOT_NULL(t1);
  TEST_ASSERT_NULL(semantic_symbol_table_insert(a, t1, path_of(a, 1, "X"), decl_node(a, 2)));
  TEST_ASSERT_TRUE(semantic_symbol_table_lookup(t1, path_of(a, 1, "X")) == first);

  arena_destroy(a);
}

void test_define_persistence_old_table_unchanged(void) {
  Arena *a = arena_create();
  SemanticSymbolTable *t = semantic_symbol_table_empty();
  SyntaxNode *n = decl_node(a, 1);

  SemanticSymbolTable *t1 = semantic_symbol_table_insert(a, t, path_of(a, 1, "X"), n);
  TEST_ASSERT_NOT_NULL(t1);
  TEST_ASSERT_NULL(semantic_symbol_table_lookup(t, path_of(a, 1, "X")));
  TEST_ASSERT_TRUE(semantic_symbol_table_lookup(t1, path_of(a, 1, "X")) == n);

  arena_destroy(a);
}

void test_define_nested_path_scoping(void) {
  Arena *a = arena_create();
  SemanticSymbolTable *t = semantic_symbol_table_empty();
  SyntaxNode *n = decl_node(a, 1);

  SemanticSymbolTable *t1 = semantic_symbol_table_insert(a, t, path_of(a, 3, "a", "b", "X"), n);
  TEST_ASSERT_NOT_NULL(t1);
  TEST_ASSERT_TRUE(semantic_symbol_table_lookup(t1, path_of(a, 3, "a", "b", "X")) == n);
  TEST_ASSERT_NULL(semantic_symbol_table_lookup(t1, path_of(a, 2, "a", "X")));
  TEST_ASSERT_NULL(semantic_symbol_table_lookup(t1, path_of(a, 1, "X")));
  TEST_ASSERT_NULL(semantic_symbol_table_lookup(t1, path_of(a, 2, "a", "b")));

  arena_destroy(a);
}

void test_collision_symbol_and_namespace(void) {
  Arena *a = arena_create();
  SemanticSymbolTable *t = semantic_symbol_table_empty();

  SemanticSymbolTable *t1 = semantic_symbol_table_insert(a, t, path_of(a, 1, "X"), decl_node(a, 1));
  TEST_ASSERT_NOT_NULL(t1);
  TEST_ASSERT_NULL(semantic_symbol_table_insert(a, t1, path_of(a, 1, "X"), NULL));

  SemanticSymbolTable *t2 = semantic_symbol_table_insert(a, t, path_of(a, 1, "Y"), NULL);
  TEST_ASSERT_NOT_NULL(t2);
  TEST_ASSERT_NULL(semantic_symbol_table_insert(a, t2, path_of(a, 1, "Y"), decl_node(a, 2)));
  TEST_ASSERT_NULL(semantic_symbol_table_lookup(t2, path_of(a, 1, "Y")));

  arena_destroy(a);
}

void test_namespace_materializes_empty(void) {
  Arena *a = arena_create();
  SemanticSymbolTable *t = semantic_symbol_table_empty();

  SemanticSymbolTable *t1 = semantic_symbol_table_insert(a, t, path_of(a, 2, "a", "b"), NULL);
  TEST_ASSERT_NOT_NULL(t1);
  // the materialized namespace is empty: its content level is the empty table
  TEST_ASSERT_NULL(semantic_symbol_table_subtable(t1, path_of(a, 2, "a", "b")));
  TEST_ASSERT_NULL(semantic_symbol_table_lookup(t1, path_of(a, 2, "a", "b")));
  // it exists though — a symbol defines under it afterwards
  SemanticSymbolTable *t2 = semantic_symbol_table_insert(a, t1, path_of(a, 3, "a", "b", "X"), decl_node(a, 1));
  TEST_ASSERT_NOT_NULL(t2);
  TEST_ASSERT_NOT_NULL(semantic_symbol_table_subtable(t2, path_of(a, 2, "a", "b")));

  arena_destroy(a);
}

void test_namespace_path_through_symbol_fails(void) {
  Arena *a = arena_create();
  SemanticSymbolTable *t = semantic_symbol_table_empty();

  SemanticSymbolTable *t1 = semantic_symbol_table_insert(a, t, path_of(a, 1, "X"), decl_node(a, 1));
  TEST_ASSERT_NOT_NULL(t1);
  TEST_ASSERT_NULL(semantic_symbol_table_insert(a, t1, path_of(a, 2, "X", "z"), NULL));

  arena_destroy(a);
}

void test_namespace_redefine_reuses_entry(void) {
  Arena *a = arena_create();
  SemanticSymbolTable *t = semantic_symbol_table_empty();
  SyntaxNode *n = decl_node(a, 1);

  SemanticSymbolTable *t1 = semantic_symbol_table_insert(a, t, path_of(a, 2, "a", "b"), NULL);
  SemanticSymbolTable *t2 = semantic_symbol_table_insert(a, t1, path_of(a, 3, "a", "b", "X"), n);
  SemanticSymbolTable *t3 = semantic_symbol_table_insert(a, t2, path_of(a, 2, "a", "b"), NULL);
  TEST_ASSERT_NOT_NULL(t1);
  TEST_ASSERT_NOT_NULL(t2);
  TEST_ASSERT_NOT_NULL(t3);
  TEST_ASSERT_TRUE(semantic_symbol_table_lookup(t3, path_of(a, 3, "a", "b", "X")) == n);
  TEST_ASSERT_NULL(semantic_symbol_table_lookup(t3, path_of(a, 3, "a", "b", "Y")));

  arena_destroy(a);
}

void test_sub_view_lookup_probes(void) {
  Arena *a = arena_create();
  SemanticSymbolTable *t = semantic_symbol_table_empty();
  SyntaxNode *n = decl_node(a, 1);

  SemanticSymbolTable *t1 = semantic_symbol_table_insert(a, t, path_of(a, 3, "a", "b", "X"), n);
  TEST_ASSERT_NOT_NULL(t1);
  const SemanticSymbolTable *view = semantic_symbol_table_subtable(t1, path_of(a, 2, "a", "b"));
  TEST_ASSERT_NOT_NULL(view);
  TEST_ASSERT_TRUE(semantic_symbol_table_lookup(view, path_of(a, 1, "X")) == n);
  TEST_ASSERT_NULL(semantic_symbol_table_lookup(view, path_of(a, 1, "Y")));

  arena_destroy(a);
}

void test_sub_unknown_segment_returns_null(void) {
  Arena *a = arena_create();
  SemanticSymbolTable *t = semantic_symbol_table_empty();

  SemanticSymbolTable *t1 = semantic_symbol_table_insert(a, t, path_of(a, 3, "a", "b", "X"), decl_node(a, 1));
  TEST_ASSERT_NOT_NULL(t1);
  TEST_ASSERT_NULL(semantic_symbol_table_subtable(t1, path_of(a, 2, "zz", "a")));
  TEST_ASSERT_NULL(semantic_symbol_table_subtable(t1, path_of(a, 1, "zz")));

  arena_destroy(a);
}

void test_sub_rejects_symbol_leaf(void) {
  Arena *a = arena_create();
  SemanticSymbolTable *t = semantic_symbol_table_empty();

  SemanticSymbolTable *t1 = semantic_symbol_table_insert(a, t, path_of(a, 3, "a", "b", "X"), decl_node(a, 1));
  TEST_ASSERT_NOT_NULL(t1);
  TEST_ASSERT_NULL(semantic_symbol_table_subtable(t1, path_of(a, 3, "a", "b", "X")));
  TEST_ASSERT_NULL(semantic_symbol_table_subtable(t1, path_of(a, 1, "X")));

  arena_destroy(a);
}

void test_sub_chained_views(void) {
  Arena *a = arena_create();
  SemanticSymbolTable *t = semantic_symbol_table_empty();
  SyntaxNode *n = decl_node(a, 1);

  SemanticSymbolTable *t1 = semantic_symbol_table_insert(a, t, path_of(a, 3, "a", "b", "X"), n);
  TEST_ASSERT_NOT_NULL(t1);
  const SemanticSymbolTable *va = semantic_symbol_table_subtable(t1, path_of(a, 1, "a"));
  const SemanticSymbolTable *vb = semantic_symbol_table_subtable(va, path_of(a, 1, "b"));
  TEST_ASSERT_NOT_NULL(va);
  TEST_ASSERT_NOT_NULL(vb);
  TEST_ASSERT_TRUE(semantic_symbol_table_lookup(vb, path_of(a, 1, "X")) == n);

  arena_destroy(a);
}

void test_subtable_empty_path_returns_table(void) {
  Arena *a = arena_create();
  SyntaxNode *n = decl_node(a, 1);

  SemanticSymbolTable *t = semantic_symbol_table_insert(a, semantic_symbol_table_empty(), path_of(a, 1, "T"), n);
  TEST_ASSERT_NOT_NULL(t);
  TEST_ASSERT_TRUE(semantic_symbol_table_subtable(t, NULL) == t);
  TEST_ASSERT_TRUE(semantic_symbol_table_subtable(t, semantic_namepath_empty()) == t);
  TEST_ASSERT_TRUE(semantic_symbol_table_lookup(semantic_symbol_table_subtable(t, semantic_namepath_empty()),
                                                path_of(a, 1, "T")) == n);

  arena_destroy(a);
}

void test_contains_accepts_namespace_and_symbol(void) {
  Arena *a = arena_create();
  SemanticSymbolTable *t = semantic_symbol_table_empty();

  // materialize the namespace a::b; b stays empty
  t = semantic_symbol_table_insert(a, t, path_of(a, 2, "a", "b"), NULL);
  TEST_ASSERT_TRUE(semantic_symbol_table_contains(t, path_of(a, 2, "a", "b")));
  TEST_ASSERT_TRUE(semantic_symbol_table_contains(t, path_of(a, 1, "a")));
  TEST_ASSERT_TRUE(semantic_symbol_table_contains(t, semantic_namepath_empty()));
  TEST_ASSERT_NULL(semantic_symbol_table_subtable(t, path_of(a, 2, "a", "b"))); // empty, not missing

  // a symbol name contains just as well
  t = semantic_symbol_table_insert(a, t, path_of(a, 1, "X"), decl_node(a, 1));
  TEST_ASSERT_TRUE(semantic_symbol_table_contains(t, path_of(a, 1, "X")));

  // a missing name, or a path crossing a symbol, contains nothing
  TEST_ASSERT_FALSE(semantic_symbol_table_contains(t, path_of(a, 1, "zz")));
  TEST_ASSERT_FALSE(semantic_symbol_table_contains(t, path_of(a, 2, "X", "z")));
  TEST_ASSERT_FALSE(semantic_symbol_table_contains(NULL, path_of(a, 1, "X")));

  arena_destroy(a);
}

void test_length_counts_level_entries(void) {
  Arena *a = arena_create();
  SemanticSymbolTable *t = semantic_symbol_table_empty();
  TEST_ASSERT_EQUAL_size_t(0, semantic_symbol_table_length(t));

  t = semantic_symbol_table_insert(a, t, path_of(a, 2, "ns", "X"), decl_node(a, 1));
  t = semantic_symbol_table_insert(a, t, path_of(a, 1, "Y"), decl_node(a, 2));
  TEST_ASSERT_EQUAL_size_t(2, semantic_symbol_table_length(t)); // "ns" and "Y"; ns content is a separate level

  const SemanticSymbolTable *ns = semantic_symbol_table_subtable(t, path_of(a, 1, "ns"));
  TEST_ASSERT_NOT_NULL(ns);
  TEST_ASSERT_EQUAL_size_t(1, semantic_symbol_table_length(ns));

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
    SemanticSymbolTable *next =
        semantic_symbol_table_insert(arena, *table, path_of(arena, 1, MANY_NAMES[order[k]]), nodes[k]);
    TEST_ASSERT_NOT_NULL(next);
    *table = next;
  }
  for (size_t k = 0; k < count; k++)
    TEST_ASSERT_TRUE(semantic_symbol_table_lookup(*table, path_of(arena, 1, MANY_NAMES[order[k]])) == nodes[k]);
  TEST_ASSERT_NULL(semantic_symbol_table_lookup(*table, path_of(arena, 1, "zzz_missing")));
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
    SemanticSymbolTable *t = semantic_symbol_table_empty();
    define_and_verify(&t, a, orders[o], count);
    arena_destroy(a);
  }
}

void test_deterministic_behavior_across_orders(void) {
  const size_t count = sizeof MANY_NAMES / sizeof MANY_NAMES[0];
  Arena *a = arena_create();
  SemanticSymbolTable *t1 = semantic_symbol_table_empty();
  SemanticSymbolTable *t2 = semantic_symbol_table_empty();
  SyntaxNode *by_name[64];

  size_t natural[64], reversed[64];
  for (size_t i = 0; i < count; i++) {
    natural[i] = i;
    reversed[count - 1 - i] = i;
    by_name[i] = decl_node(a, (int)i);
  }

  for (size_t k = 0; k < count; k++) {
    t1 = semantic_symbol_table_insert(a, t1, path_of(a, 1, MANY_NAMES[natural[k]]), by_name[natural[k]]);
    t2 = semantic_symbol_table_insert(a, t2, path_of(a, 1, MANY_NAMES[reversed[k]]), by_name[reversed[k]]);
    TEST_ASSERT_NOT_NULL(t1);
    TEST_ASSERT_NOT_NULL(t2);
  }
  for (size_t i = 0; i < count; i++) {
    TEST_ASSERT_TRUE(semantic_symbol_table_lookup(t1, path_of(a, 1, MANY_NAMES[i])) == by_name[i]);
    TEST_ASSERT_TRUE(semantic_symbol_table_lookup(t2, path_of(a, 1, MANY_NAMES[i])) == by_name[i]);
  }
  TEST_ASSERT_NULL(semantic_symbol_table_lookup(t1, path_of(a, 1, "zzz_missing")));
  TEST_ASSERT_NULL(semantic_symbol_table_lookup(t2, path_of(a, 1, "zzz_missing")));

  arena_destroy(a);
}

static const TestDispatchEntry ENTRIES[] = {
    {"empty_table_lookup_miss", test_empty_table_lookup_miss},
    {"insert_into_null_grows", test_insert_into_null_grows},
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
    {"subtable_empty_path_returns_table", test_subtable_empty_path_returns_table},
    {"contains_accepts_namespace_and_symbol", test_contains_accepts_namespace_and_symbol},
    {"length_counts_level_entries", test_length_counts_level_entries},
    {"define_many_orders_all_lookups", test_define_many_orders_all_lookups},
    {"deterministic_behavior_across_orders", test_deterministic_behavior_across_orders},
};

TEST_DISPATCH_MAIN(ENTRIES)
