/**
 * @file test_registry.c
 * @brief Unit tests for the pre-collect name registry.
 * @author solid-matrix
 */

#include "semantic_registry.h"
#include "semantic_const_fold.h"
#include "semantic_gate.h"
#include "semantic_fixture.h"
#include "symbol_table.h"
#include "test_support.h"

static int errors_with_code(const SemanticRegistry *r, SemanticErrorCode code) {
  int n = 0;
  for (SemanticErrorList *e = semantic_registry_errors(r); e != NULL; e = e->tail)
    if (e->head.code == code) n++;
  return n;
}

static int count_named(const SemanticRegistry *r, const char *name) {
  int n = 0;
  for (const SemanticRegistryEntry *e = semantic_registry_entries(r); e != NULL;
       e = e->next)
    if (strview_equals(e->name, strview_from_cstr(name))) n++;
  return n;
}

void registry_names_root_and_namespaced(void) {
  Arena *a = arena_create();
  SemanticModule *app = module_of(a, path_of(a, 1, "app"),
                                  units_of(a, 2, "let V = 1u;\n",
                                           "namespace cfg;\nlet D = 2u;\nlet E = 3u;\n"));
  SemanticModuleList *mods = modules_of(a, 1, app);
  SemanticRegistry r = semantic_registry_build(a, mods, NULL);

  SemanticNamePath *v = path_of(a, 2, "app", "V");
  TEST_ASSERT_TRUE(semantic_symbol_table_contains(semantic_registry_table(&r), v));
  SemanticNamePath *cfgd = path_of(a, 3, "app", "cfg", "D");
  TEST_ASSERT_TRUE(semantic_symbol_table_contains(semantic_registry_table(&r), cfgd));

  SyntaxNode *hit = NULL;
  TEST_ASSERT_TRUE(semantic_registry_lookup_bare(&r, app, strview_from_cstr("V"), &hit) ==
                   SEMANTIC_RLOOKUP_HIT);
  TEST_ASSERT_TRUE(semantic_registry_lookup_path(&r, app, path_of(a, 2, "cfg", "E"), &hit) ==
                   SEMANTIC_RLOOKUP_HIT);
  TEST_ASSERT_TRUE(semantic_registry_lookup_path(&r, app, path_of(a, 2, "cfg", "D"), &hit) ==
                   SEMANTIC_RLOOKUP_HIT);
  TEST_ASSERT_NULL(semantic_registry_errors(&r));

  arena_destroy(a);
}

void registry_two_files_merge_namespace(void) {
  Arena *a = arena_create();
  SemanticModule *app =
      module_of(a, path_of(a, 1, "app"),
                units_of(a, 2, "namespace cfg;\nlet D = 1u;\n", "namespace cfg;\nlet E = 2u;\n"));
  SemanticModuleList *mods = modules_of(a, 1, app);
  SemanticRegistry r = semantic_registry_build(a, mods, NULL);

  SyntaxNode *hit = NULL;
  TEST_ASSERT_TRUE(semantic_registry_lookup_path(&r, app, path_of(a, 2, "cfg", "D"), &hit) ==
                   SEMANTIC_RLOOKUP_HIT);
  TEST_ASSERT_TRUE(semantic_registry_lookup_path(&r, app, path_of(a, 2, "cfg", "E"), &hit) ==
                   SEMANTIC_RLOOKUP_HIT);
  TEST_ASSERT_NULL(semantic_registry_errors(&r));

  arena_destroy(a);
}

void registry_distinct_packages_prefix(void) {
  Arena *a = arena_create();
  SemanticModule *app = module_of(a, path_of(a, 1, "app"), units_of(a, 1, "let V = 1u;\n"));
  SemanticModule *net = module_of(a, path_of(a, 1, "net"), units_of(a, 1, "let V = 2u;\n"));
  SemanticModuleList *mods = modules_of(a, 2, app, net);
  SemanticRegistry r = semantic_registry_build(a, mods, NULL);

  SyntaxNode *hit = NULL;
  TEST_ASSERT_TRUE(semantic_registry_lookup_bare(&r, app, strview_from_cstr("V"), &hit) ==
                   SEMANTIC_RLOOKUP_HIT);
  TEST_ASSERT_NOT_NULL(hit);
  TEST_ASSERT_TRUE(semantic_registry_lookup_path(&r, app, path_of(a, 2, "net", "V"), &hit) ==
                   SEMANTIC_RLOOKUP_HIT);
  TEST_ASSERT_NOT_NULL(hit);
  TEST_ASSERT_NULL(semantic_registry_errors(&r));

  arena_destroy(a);
}

void registry_core_prelude_bare(void) {
  Arena *a = arena_create();
  SemanticModule *core = module_of(a, path_of(a, 1, "core"), units_of(a, 1, "let MAGIC = 7u;\n"));
  SemanticModule *app = module_of(a, path_of(a, 1, "app"), units_of(a, 1, "let V = 1u;\n"));
  SemanticModuleList *mods = modules_of(a, 2, core, app);
  SemanticRegistry r = semantic_registry_build(a, mods, NULL);

  SyntaxNode *hit = NULL;
  TEST_ASSERT_TRUE(semantic_registry_lookup_bare(&r, app, strview_from_cstr("MAGIC"), &hit) ==
                   SEMANTIC_RLOOKUP_HIT);
  TEST_ASSERT_TRUE(semantic_registry_lookup_bare(&r, app, strview_from_cstr("NOPE"), &hit) ==
                   SEMANTIC_RLOOKUP_MISS);
  // core itself does not fall back to core (it is core)
  TEST_ASSERT_TRUE(semantic_registry_lookup_bare(&r, core, strview_from_cstr("V"), &hit) ==
                   SEMANTIC_RLOOKUP_MISS);
  TEST_ASSERT_NULL(semantic_registry_errors(&r));

  arena_destroy(a);
}

void registry_duplicate_keeps_first(void) {
  Arena *a = arena_create();
  SemanticModule *app = module_of(a, path_of(a, 1, "app"),
                                  units_of(a, 2, "namespace cfg;\nlet D = 1u;\n",
                                           "namespace cfg;\nlet D = 2u;\n"));
  SemanticModuleList *mods = modules_of(a, 1, app);
  SemanticRegistry r = semantic_registry_build(a, mods, NULL);
  semantic_fold_world(&r, a);
  semantic_decide_gates(&r, a);

  // Both alternatives are registered; both survive unguarded, so the
  // collision is diagnosed after gating (§10.4 Effect) and the world table
  // keeps the first spelling.
  int redefined = 0;
  for (SemanticErrorList *e = semantic_registry_errors(&r); e != NULL; e = e->tail)
    if (e->head.code == SEMANTIC_SYMBOL_REDEFINED) redefined++;
  TEST_ASSERT_TRUE(redefined >= 1);
  TEST_ASSERT_EQUAL_INT(2, count_named(&r, "D"));

  SyntaxNode *hit = NULL;
  TEST_ASSERT_TRUE(semantic_registry_lookup_path(&r, app, path_of(a, 2, "cfg", "D"), &hit) ==
                   SEMANTIC_RLOOKUP_HIT);

  arena_destroy(a);
}

void registry_intrinsic_outside_core_diag(void) {
  Arena *a = arena_create();
  // Only core may declare `@intrinsic`: the marker promises an implementation
  // lowering, a promise only the compiler's own glue library can make (§12.1).
  SemanticModule *core = module_of(a, path_of(a, 1, "core"),
                                   units_of(a, 1, "@intrinsic let TARGET_OS: String8;\n"
                                                  "@intrinsic func _read():i32;\n"
                                                  "@intrinsic struct Array<T, N: usize>;\n"));
  SemanticModule *app = module_of(a, path_of(a, 1, "app"),
                                  units_of(a, 1, "@intrinsic let X: u32;\n"
                                                 "@intrinsic func f():i32;\n"
                                                 "@intrinsic struct S;\n"));
  SemanticModuleList *mods = modules_of(a, 2, core, app);
  SemanticRegistry r = semantic_registry_build(a, mods, NULL);

  TEST_ASSERT_EQUAL_INT(3, errors_with_code(&r, SEMANTIC_INTRINSIC_OUTSIDE_CORE));

  arena_destroy(a);
}

void registry_kinds_classification(void) {
  Arena *a = arena_create();
  // `@intrinsic` is core-only (§12.1), so the fact declaration lives there.
  const char *core_text = "@intrinsic let FACT: u32;\n";
  const char *text = "let folded = 1u;\n"
                     "let addr: &u32 = @const(5u);\n"
                     "let st: &u32 = @static(0u);\n"
                     "@feature let LOG_LEVEL: i32;\n"
                     "@import(\"X\") let X: u32;\n"
                     "let noinit: i32;\n"
                     "func f():i32;\n"
                     "struct S;\n"
                     "contract C(p:i32):i32;\n";
  SemanticModule *core = module_of(a, path_of(a, 1, "core"), units_of(a, 1, core_text));
  SemanticModule *app = module_of(a, path_of(a, 1, "app"), units_of(a, 1, text));
  SemanticModuleList *mods = modules_of(a, 2, core, app);
  SemanticRegistry r = semantic_registry_build(a, mods, NULL);

  SemanticRegistryKind want[] = {
      SEMANTIC_RK_LET,         SEMANTIC_RK_LET_ADDRESS,   SEMANTIC_RK_LET_ADDRESS,
      SEMANTIC_RK_LET_FEATURE, SEMANTIC_RK_LET_INTRINSIC, SEMANTIC_RK_LET_IMPORT,
      SEMANTIC_RK_LET_NO_INIT, SEMANTIC_RK_FUNC,          SEMANTIC_RK_STRUCT,
      SEMANTIC_RK_CONTRACT,
  };
  const char *names[] = {"folded", "addr",   "st",    "LOG_LEVEL", "FACT",
                         "X",      "noinit", "f",     "S",         "C"};

  int found[10] = {0};
  int missing_init_diag = 0;
  for (SemanticErrorList *e = semantic_registry_errors(&r); e != NULL; e = e->tail)
    if (e->head.code == SEMANTIC_LET_MISSING_INIT) missing_init_diag++;

  for (const SemanticRegistryEntry *e = semantic_registry_entries(&r); e != NULL; e = e->next) {
    for (size_t i = 0; i < sizeof names / sizeof names[0]; i++) {
      if (strview_equals(e->name, strview_from_cstr(names[i]))) {
        TEST_ASSERT_EQUAL_HEX32(want[i], e->kind);
        found[i] = 1;
      }
    }
  }
  for (size_t i = 0; i < sizeof found / sizeof found[0]; i++)
    TEST_ASSERT_TRUE(found[i]);
  TEST_ASSERT_EQUAL_INT(1, missing_init_diag); // one let without init or external form
  TEST_ASSERT_NOT_NULL(semantic_registry_errors(&r));

  arena_destroy(a);
}

void registry_lookup_ambiguous(void) {
  Arena *a = arena_create();
  SemanticModule *app = module_of(a, path_of(a, 1, "app"),
                                  units_of(a, 2, "namespace cfg;\nlet D = 1u;\n",
                                           "namespace oth;\nlet D = 2u;\n"));
  SemanticModuleList *mods = modules_of(a, 1, app);
  SemanticRegistry r = semantic_registry_build(a, mods, NULL);

  SyntaxNode *hit = NULL;
  TEST_ASSERT_TRUE(semantic_registry_lookup_bare(&r, app, strview_from_cstr("D"), &hit) ==
                   SEMANTIC_RLOOKUP_AMBIGUOUS);
  // namespace-qualified stays unambiguous
  TEST_ASSERT_TRUE(semantic_registry_lookup_path(&r, app, path_of(a, 2, "cfg", "D"), &hit) ==
                   SEMANTIC_RLOOKUP_HIT);
  TEST_ASSERT_NULL(semantic_registry_errors(&r));

  arena_destroy(a);
}

void registry_lookup_miss(void) {
  Arena *a = arena_create();
  SemanticModule *app = module_of(a, path_of(a, 1, "app"), units_of(a, 1, "let V = 1u;\n"));
  SemanticModuleList *mods = modules_of(a, 1, app);
  SemanticRegistry r = semantic_registry_build(a, mods, NULL);

  SyntaxNode *hit = NULL;
  TEST_ASSERT_TRUE(semantic_registry_lookup_bare(&r, app, strview_from_cstr("NOPE"), &hit) ==
                   SEMANTIC_RLOOKUP_MISS);
  TEST_ASSERT_TRUE(semantic_registry_lookup_path(&r, app, path_of(a, 2, "no", "pe"), &hit) ==
                   SEMANTIC_RLOOKUP_MISS);

  arena_destroy(a);
}

void registry_guards_collected(void) {
  Arena *a = arena_create();
  const char *text = "@when(IS_DEBUG) func f():i32;\n"
                     "func g():i32;\n"
                     "@when(A) @when(B) func h():i32;\n";
  SemanticModule *app = module_of(a, path_of(a, 1, "app"), units_of(a, 1, text));
  SemanticModuleList *mods = modules_of(a, 1, app);
  SemanticRegistry r = semantic_registry_build(a, mods, NULL);

  int guarded = 0;
  int h_multiple = 0;
  int multiple_diag = 0;
  for (const SemanticRegistryEntry *e = semantic_registry_entries(&r); e != NULL; e = e->next) {
    if (e->guard != NULL) guarded++;
    if (strview_equals(e->name, strview_from_cstr("h")) && e->guard != NULL) h_multiple = 1;
  }
  for (SemanticErrorList *e = semantic_registry_errors(&r); e != NULL; e = e->tail)
    if (e->head.code == SEMANTIC_MULTIPLE_GUARDS) multiple_diag++;
  TEST_ASSERT_EQUAL_INT(2, guarded);      // f and h carry guards
  TEST_ASSERT_EQUAL_INT(1, h_multiple);   // first guard wins
  TEST_ASSERT_EQUAL_INT(1, multiple_diag); // the second is diagnosed

  arena_destroy(a);
}

void registry_knob_values(void) {
  Arena *a = arena_create();
  SemanticParam knob = {.name = strview_from_cstr("LOG_LEVEL"), .value = strview_from_cstr("2")};
  SemanticParamList params = {.param = knob, .next = NULL};
  SemanticModule *app = module_of(a, path_of(a, 1, "app"),
                                  units_of(a, 1, "@feature let LOG_LEVEL: i32;\n"
                                                  "@feature let OTHER: i32;\n"));
  SemanticModuleList *mods = modules_of(a, 1, app);
  SemanticRegistry r = semantic_registry_build(a, mods, &params);

  int knob_found = 0;
  int other_seen = 0;
  for (const SemanticRegistryEntry *e = semantic_registry_entries(&r); e != NULL; e = e->next) {
    const Strview *v = semantic_registry_knob_value(&r, e);
    if (strview_equals(e->name, strview_from_cstr("LOG_LEVEL"))) {
      TEST_ASSERT_NOT_NULL(v);
      TEST_ASSERT_STRVIEW_EQ(*v, "2");
      knob_found = 1;
    }
    if (strview_equals(e->name, strview_from_cstr("OTHER"))) {
      other_seen = 1;
      TEST_ASSERT_NULL(v); // no manifest value supplied
    }
  }
  TEST_ASSERT_EQUAL_INT(1, knob_found);
  TEST_ASSERT_EQUAL_INT(1, other_seen);

  arena_destroy(a);
}

static const TestDispatchEntry ENTRIES[] = {
    {"registry_names_root_and_namespaced", registry_names_root_and_namespaced},
    {"registry_two_files_merge_namespace", registry_two_files_merge_namespace},
    {"registry_distinct_packages_prefix", registry_distinct_packages_prefix},
    {"registry_core_prelude_bare", registry_core_prelude_bare},
    {"registry_duplicate_keeps_first", registry_duplicate_keeps_first},
    {"registry_kinds_classification", registry_kinds_classification},
    {"registry_intrinsic_outside_core_diag", registry_intrinsic_outside_core_diag},
    {"registry_lookup_ambiguous", registry_lookup_ambiguous},
    {"registry_lookup_miss", registry_lookup_miss},
    {"registry_guards_collected", registry_guards_collected},
    {"registry_knob_values", registry_knob_values},
};

TEST_DISPATCH_MAIN(ENTRIES)
