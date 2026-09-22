/**
 * @file test_gate.c
 * @brief Unit tests for the gate stage: guard decisions, gating-graph
 *        acyclicity, and survivor collisions (spec §10.4).
 * @author solid-matrix
 */

#include "semantic_gate.h"
#include "semantic_const_fold.h"
#include "semantic_fixture.h"
#include "test_support.h"
#include <stdio.h>

static SemanticRegistryEntry *find_entry(const SemanticRegistry *r, const char *name) {
  Strview n = strview_from_cstr(name);
  for (SemanticRegistryEntry *e = semantic_registry_entries(r); e != NULL; e = e->next)
    if (strview_equals(e->name, n)) return e;
  return NULL;
}

static int errors_with_code(const SemanticRegistry *r, SemanticErrorCode code) {
  int n = 0;
  for (SemanticErrorList *e = semantic_registry_errors(r); e != NULL; e = e->tail)
    if (e->head.code == code) n++;
  return n;
}

// Survivors among the declarations of @p name: alternatives share a name, so
// the entry chain holds one node per spelling.
static int count_survivors(const SemanticRegistry *r, const char *name) {
  Strview n = strview_from_cstr(name);
  int survivors = 0;
  for (SemanticRegistryEntry *e = semantic_registry_entries(r); e != NULL; e = e->next)
    if (strview_equals(e->name, n) && e->survive) survivors++;
  return survivors;
}

static void run_pipeline(Arena *a, SemanticModuleList *mods, SemanticParamList *params,
                         SemanticRegistry *r) {
  *r = semantic_registry_build(a, mods, params);
  semantic_fold_world(r, a);
  semantic_decide_gates(r, a);
}

void gate_flag_selects_declaration(void) {
  Arena *a = arena_create();
  SemanticParam os = {.name = strview_from_cstr("TARGET_OS"), .value = strview_from_cstr("linux")};
  SemanticParamList params = {.param = os, .next = NULL};
  SemanticModule *core =
      module_of(a, path_of(a, 1, "core"), units_of(a, 1, "@intrinsic let TARGET_OS: String8;\n"));
  SemanticModule *app = module_of(a, path_of(a, 1, "app"),
                                  units_of(a, 1, "let OS_LINUX = \"linux\";\n"
                                                 "let IS_LINUX = TARGET_OS == OS_LINUX;\n"
                                                 "@when(IS_LINUX) let PAGE = 4096u;\n"
                                                 "@when(!IS_LINUX) let PAGE_BIG = 16384u;\n"));
  SemanticModuleList *mods = modules_of(a, 2, core, app);
  SemanticRegistry r;
  run_pipeline(a, mods, &params, &r);

  TEST_ASSERT_TRUE(find_entry(&r, "PAGE")->survive);
  TEST_ASSERT_FALSE(find_entry(&r, "PAGE_BIG")->survive);
  TEST_ASSERT_NULL(semantic_registry_errors(&r));

  arena_destroy(a);
}

void gate_guard_on_all_kinds(void) {
  Arena *a = arena_create();
  SemanticModule *app =
      module_of(a, path_of(a, 1, "app"),
                units_of(a, 1, "@when(1u == 1u) let V = 1u;\n"
                               "@when(1u == 1u) func f():i32;\n"
                               "@when(1u == 1u) struct S;\n"
                               "@when(1u == 1u) contract C(p:i32):i32;\n"));
  SemanticModuleList *mods = modules_of(a, 1, app);
  SemanticRegistry r;
  run_pipeline(a, mods, NULL, &r);

  const char *names[] = {"V", "f", "S", "C"};
  for (size_t i = 0; i < sizeof names / sizeof names[0]; i++) {
    SemanticRegistryEntry *e = find_entry(&r, names[i]);
    TEST_ASSERT_NOT_NULL(e);
    TEST_ASSERT_TRUE(e->survive);
  }
  TEST_ASSERT_NULL(semantic_registry_errors(&r));

  arena_destroy(a);
}

void gate_condition_must_be_bool(void) {
  Arena *a = arena_create();
  SemanticParam os = {.name = strview_from_cstr("TARGET_OS"), .value = strview_from_cstr("linux")};
  SemanticParamList params = {.param = os, .next = NULL};
  SemanticModule *core =
      module_of(a, path_of(a, 1, "core"), units_of(a, 1, "@intrinsic let TARGET_OS: String8;\n"));
  SemanticModule *app = module_of(a, path_of(a, 1, "app"),
                                  units_of(a, 1, "@when(1u) let A = 1u;\n"
                                                 "@when(TARGET_OS) let B = 2u;\n"));
  SemanticModuleList *mods = modules_of(a, 2, core, app);
  SemanticRegistry r;
  run_pipeline(a, mods, &params, &r);

  TEST_ASSERT_EQUAL_INT(2, errors_with_code(&r, SEMANTIC_CONST_TYPE)); // int and String8
  TEST_ASSERT_FALSE(find_entry(&r, "A")->survive);
  TEST_ASSERT_FALSE(find_entry(&r, "B")->survive);

  arena_destroy(a);
}

void gate_unknown_name_diag(void) {
  Arena *a = arena_create();
  SemanticModule *app = module_of(a, path_of(a, 1, "app"), units_of(a, 1, "@when(NOPE) let A = 1u;\n"));
  SemanticModuleList *mods = modules_of(a, 1, app);
  SemanticRegistry r;
  run_pipeline(a, mods, NULL, &r);

  TEST_ASSERT_EQUAL_INT(1, errors_with_code(&r, SEMANTIC_CONST_TYPE));
  TEST_ASSERT_FALSE(find_entry(&r, "A")->survive);

  arena_destroy(a);
}

void gate_symbolic_and_layout_banned(void) {
  Arena *a = arena_create();
  SemanticModule *app = module_of(a, path_of(a, 1, "app"),
                                  units_of(a, 1, "func read():i32;\n"
                                                 "struct S;\n"
                                                 "let N = @sizeof(S);\n"
                                                 "@when(read) let A = 1u;\n"
                                                 "@when(N == N) let B = 2u;\n"));
  SemanticModuleList *mods = modules_of(a, 1, app);
  SemanticRegistry r;
  run_pipeline(a, mods, NULL, &r);

  TEST_ASSERT_EQUAL_INT(2, errors_with_code(&r, SEMANTIC_CONST_TYPE)); // symbolic and layout
  TEST_ASSERT_FALSE(find_entry(&r, "A")->survive);
  TEST_ASSERT_FALSE(find_entry(&r, "B")->survive);

  arena_destroy(a);
}

void gate_self_reference_cycle(void) {
  Arena *a = arena_create();
  SemanticModule *app = module_of(a, path_of(a, 1, "app"), units_of(a, 1, "@when(A) let A = 1u;\n"));
  SemanticModuleList *mods = modules_of(a, 1, app);
  SemanticRegistry r;
  run_pipeline(a, mods, NULL, &r);

  TEST_ASSERT_EQUAL_INT(1, errors_with_code(&r, SEMANTIC_CONST_CYCLE));
  TEST_ASSERT_EQUAL_INT(1, errors_with_code(&r, SEMANTIC_CONST_TYPE)); // guard sees an int

  arena_destroy(a);
}

void gate_mutual_gating_cycle(void) {
  Arena *a = arena_create();
  SemanticModule *app = module_of(a, path_of(a, 1, "app"),
                                  units_of(a, 1, "@when(B == B) let A = 1u;\n"
                                                 "@when(A == A) let B = 2u;\n"));
  SemanticModuleList *mods = modules_of(a, 1, app);
  SemanticRegistry r;
  run_pipeline(a, mods, NULL, &r);

  // Determinate under total evaluation, still rejected for readability.
  TEST_ASSERT_EQUAL_INT(1, errors_with_code(&r, SEMANTIC_CONST_CYCLE));

  arena_destroy(a);
}

void gate_transitive_cycle(void) {
  Arena *a = arena_create();
  SemanticModule *app = module_of(a, path_of(a, 1, "app"),
                                  units_of(a, 1, "@when(C == 1u) let A = 1u;\n"
                                                 "let C = B;\n"
                                                 "@when(A == 1u) let B = 2u;\n"));
  SemanticModuleList *mods = modules_of(a, 1, app);
  SemanticRegistry r;
  run_pipeline(a, mods, NULL, &r);

  TEST_ASSERT_EQUAL_INT(1, errors_with_code(&r, SEMANTIC_CONST_CYCLE));

  arena_destroy(a);
}

void gate_disjoint_alternatives_legal(void) {
  Arena *a = arena_create();
  SemanticModule *app = module_of(a, path_of(a, 1, "app"),
                                  units_of(a, 1, "@when(1u == 1u) let V = 1u;\n"
                                                 "@when(1u == 2u) let V = 2u;\n"));
  SemanticModuleList *mods = modules_of(a, 1, app);
  SemanticRegistry r;
  run_pipeline(a, mods, NULL, &r);

  TEST_ASSERT_EQUAL_INT(1, count_survivors(&r, "V")); // disjoint guards: one alternative
  int redefined = 0;
  for (SemanticErrorList *e = semantic_registry_errors(&r); e != NULL; e = e->tail)
    if (e->head.code == SEMANTIC_SYMBOL_REDEFINED) redefined++;
  TEST_ASSERT_EQUAL_INT(0, redefined); // exactly one survivor: no collision

  arena_destroy(a);
}

void gate_feature_let_forbidden(void) {
  Arena *a = arena_create();
  SemanticParam lvl = {.name = strview_from_cstr("LOG_LEVEL"), .value = strview_from_cstr("2")};
  SemanticParamList params = {.param = lvl, .next = NULL};
  SemanticModule *app = module_of(a, path_of(a, 1, "app"),
                                  units_of(a, 1, "@when(1u == 1u) @feature let LOG_LEVEL: i32;\n"));
  SemanticModuleList *mods = modules_of(a, 1, app);
  SemanticRegistry r;
  run_pipeline(a, mods, &params, &r);

  TEST_ASSERT_EQUAL_INT(1, errors_with_code(&r, SEMANTIC_GUARD_FORBIDDEN));
  TEST_ASSERT_FALSE(find_entry(&r, "LOG_LEVEL")->survive);

  arena_destroy(a);
}

void gate_grammar_rejects_unsuffixed_literal(void) {
  Arena *a = arena_create();
  // A `@when` condition is a typed slot: literals in it shall carry suffixes
  // (§4.4). Both operands here violate it, so both are reported.
  SemanticModule *app = module_of(a, path_of(a, 1, "app"), units_of(a, 1, "@when(1 == 1) let V = 1u;\n"));
  SemanticModuleList *mods = modules_of(a, 1, app);
  SemanticRegistry r;
  run_pipeline(a, mods, NULL, &r);

  TEST_ASSERT_EQUAL_INT(2, errors_with_code(&r, SEMANTIC_GUARD_CONDITION));
  TEST_ASSERT_FALSE(find_entry(&r, "V")->survive);

  arena_destroy(a);
}

void gate_grammar_rejects_operators_outside_the_set(void) {
  Arena *a = arena_create();
  SemanticModule *app = module_of(a, path_of(a, 1, "app"),
                                  units_of(a, 1, "@when(5u % 2u == 1u) let R = 1u;\n"
                                                 "@when(1u << 2u == 4u) let S = 1u;\n"
                                                 "@when(1u32 & 1u32 == 1u32) let B = 1u;\n"));
  SemanticModuleList *mods = modules_of(a, 1, app);
  SemanticRegistry r;
  run_pipeline(a, mods, NULL, &r);

  TEST_ASSERT_EQUAL_INT(3, errors_with_code(&r, SEMANTIC_GUARD_CONDITION)); // the banned operator, once per guard
  TEST_ASSERT_FALSE(find_entry(&r, "R")->survive);
  TEST_ASSERT_FALSE(find_entry(&r, "S")->survive);
  TEST_ASSERT_FALSE(find_entry(&r, "B")->survive);

  arena_destroy(a);
}

void gate_grammar_rejects_non_integer_literals(void) {
  Arena *a = arena_create();
  // No strings and no runes appear in conditions; floating-point arithmetic
  // is outside the closed set as well.
  SemanticModule *app = module_of(a, path_of(a, 1, "app"),
                                  units_of(a, 1, "@when(\"a\" == \"a\") let S = 1u;\n"
                                                 "@when('a' == 'a') let C = 1u;\n"
                                                 "@when(1.5_d > 1.0_d) let F = 1u;\n"));
  SemanticModuleList *mods = modules_of(a, 1, app);
  SemanticRegistry r;
  run_pipeline(a, mods, NULL, &r);

  TEST_ASSERT_EQUAL_INT(6, errors_with_code(&r, SEMANTIC_GUARD_CONDITION)); // both operands of all three
  TEST_ASSERT_FALSE(find_entry(&r, "S")->survive);
  TEST_ASSERT_FALSE(find_entry(&r, "C")->survive);
  TEST_ASSERT_FALSE(find_entry(&r, "F")->survive);

  arena_destroy(a);
}

void gate_grammar_accepts_the_closed_set(void) {
  Arena *a = arena_create();
  SemanticModule *app =
      module_of(a, path_of(a, 1, "app"),
                units_of(a, 1, "let N = 3i32;\n"
                               "@when(1u32 + 2u32 * 3u32 / 3u32 == 3u32) let A = 1u;\n"
                               "@when(!(N < 4i32) || N <= 3i32) let B = 1u;\n"
                               "@when(N - 3i32 == 0i32) let C = 1u;\n"
                               "@when(-1i32 < 0i32) let D = 1u;\n"));
  SemanticModuleList *mods = modules_of(a, 1, app);
  SemanticRegistry r;
  run_pipeline(a, mods, NULL, &r);

  TEST_ASSERT_NULL(semantic_registry_errors(&r));
  const char *names[] = {"A", "B", "C", "D"};
  for (size_t i = 0; i < sizeof names / sizeof names[0]; i++)
    TEST_ASSERT_TRUE(find_entry(&r, names[i])->survive);

  arena_destroy(a);
}

void gate_guard_without_condition_diag(void) {
  Arena *a = arena_create();
  // A `@when` carrying no condition has nothing to decide: ill-formed, and
  // never a silent "always true".
  SemanticModule *app = module_of(a, path_of(a, 1, "app"),
                                  units_of(a, 1, "@when() let A = 1u;\n"
                                                 "@when let B = 2u;\n"));
  SemanticModuleList *mods = modules_of(a, 1, app);
  SemanticRegistry r;
  run_pipeline(a, mods, NULL, &r);

  TEST_ASSERT_EQUAL_INT(2, errors_with_code(&r, SEMANTIC_GUARD_CONDITION));
  TEST_ASSERT_FALSE(find_entry(&r, "A")->survive);
  TEST_ASSERT_FALSE(find_entry(&r, "B")->survive);

  arena_destroy(a);
}

void gate_nested_positions_forbidden(void) {
  Arena *a = arena_create();
  // `@when` guards only the six top-level declaration kinds: fields and
  // formal/generic parameters cannot be guarded (§10.4 Position).
  SemanticModule *app = module_of(a, path_of(a, 1, "app"),
                                  units_of(a, 1, "struct S<@when(1u == 1u) T> { @when(1u == 1u) x: i32 }\n"
                                                 "enum E { @when(1u == 1u) A }\n"
                                                 "func f(@when(1u == 1u) v: i32) {\n}\n"
                                                 "contract C(@when(1u == 1u) p: i32): i32;\n"));
  SemanticModuleList *mods = modules_of(a, 1, app);
  SemanticRegistry r;
  run_pipeline(a, mods, NULL, &r);

  // One per position: the struct's generic parameter and field, the enum
  // field, the function's parameter, the contract's parameter.
  TEST_ASSERT_EQUAL_INT(5, errors_with_code(&r, SEMANTIC_GUARD_FORBIDDEN));
  TEST_ASSERT_TRUE(find_entry(&r, "S")->survive); // the declarations themselves are legal

  arena_destroy(a);
}

void gate_nested_custom_annotations_allowed(void) {
  Arena *a = arena_create();
  // Only `@when` is position-bound: custom annotations on fields and
  // parameters keep their verbatim, semantics-free meaning (§10.2).
  SemanticModule *app =
      module_of(a, path_of(a, 1, "app"),
                units_of(a, 1, "struct S { @packed x: i32 }\nfunc f(@noinline v: i32) {\n}\n"));
  SemanticModuleList *mods = modules_of(a, 1, app);
  SemanticRegistry r;
  run_pipeline(a, mods, NULL, &r);

  TEST_ASSERT_NULL(semantic_registry_errors(&r));

  arena_destroy(a);
}

static const TestDispatchEntry ENTRIES[] = {
    {"gate_flag_selects_declaration", gate_flag_selects_declaration},
    {"gate_guard_on_all_kinds", gate_guard_on_all_kinds},
    {"gate_condition_must_be_bool", gate_condition_must_be_bool},
    {"gate_unknown_name_diag", gate_unknown_name_diag},
    {"gate_symbolic_and_layout_banned", gate_symbolic_and_layout_banned},
    {"gate_self_reference_cycle", gate_self_reference_cycle},
    {"gate_mutual_gating_cycle", gate_mutual_gating_cycle},
    {"gate_transitive_cycle", gate_transitive_cycle},
    {"gate_disjoint_alternatives_legal", gate_disjoint_alternatives_legal},
    {"gate_feature_let_forbidden", gate_feature_let_forbidden},
    {"gate_grammar_rejects_unsuffixed_literal", gate_grammar_rejects_unsuffixed_literal},
    {"gate_grammar_rejects_operators_outside_the_set", gate_grammar_rejects_operators_outside_the_set},
    {"gate_grammar_rejects_non_integer_literals", gate_grammar_rejects_non_integer_literals},
    {"gate_grammar_accepts_the_closed_set", gate_grammar_accepts_the_closed_set},
    {"gate_guard_without_condition_diag", gate_guard_without_condition_diag},
    {"gate_nested_positions_forbidden", gate_nested_positions_forbidden},
    {"gate_nested_custom_annotations_allowed", gate_nested_custom_annotations_allowed},
};

TEST_DISPATCH_MAIN(ENTRIES)
