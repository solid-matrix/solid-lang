/**
 * @file test_const_fold.c
 * @brief Unit tests for the constant-world folder (spec §10.1, §10.3).
 * @author solid-matrix
 */

#include "semantic_const_fold.h"
#include "semantic_fixture.h"
#include "symbol_table.h"
#include "test_support.h"

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

void fold_chain_int(void) {
  Arena *a = arena_create();
  SemanticModule *app = module_of(a, path_of(a, 1, "app"),
                                  units_of(a, 1, "let A = 1u32;\nlet B = A + 2u32;\n"));
  SemanticModuleList *mods = modules_of(a, 1, app);
  SemanticRegistry r = semantic_registry_build(a, mods, NULL);
  semantic_fold_world(&r, a);

  SemanticRegistryEntry *b = find_entry(&r, "B");
  TEST_ASSERT_NOT_NULL(b);
  TEST_ASSERT_EQUAL_HEX32(SEMANTIC_FC_FOLDABLE, b->fold_class);
  TEST_ASSERT_EQUAL_HEX32(SEMANTIC_CV_INT, b->value.kind);
  TEST_ASSERT_EQUAL_HEX32(SEMANTIC_INT_U32, b->value.int_type);
  TEST_ASSERT_EQUAL_UINT64(3, b->value.bits);
  TEST_ASSERT_TRUE(b->guard_usable);
  TEST_ASSERT_NULL(semantic_registry_errors(&r));

  arena_destroy(a);
}

void fold_cycle_diagnostic(void) {
  Arena *a = arena_create();
  SemanticModule *app = module_of(a, path_of(a, 1, "app"),
                                  units_of(a, 1, "let A = B;\nlet B = A;\n"));
  SemanticModuleList *mods = modules_of(a, 1, app);
  SemanticRegistry r = semantic_registry_build(a, mods, NULL);
  semantic_fold_world(&r, a);

  TEST_ASSERT_TRUE(errors_with_code(&r, SEMANTIC_CONST_CYCLE) >= 1);
  static const char *const cycle_names[] = {"A", "B"};
  for (int i = 0; i < 2; i++) {
    SemanticRegistryEntry *e = find_entry(&r, cycle_names[i]);
    TEST_ASSERT_NOT_NULL(e);
    TEST_ASSERT_EQUAL_HEX32(SEMANTIC_FC_DEFERRED, e->fold_class);
    TEST_ASSERT_FALSE(e->guard_usable);
  }

  arena_destroy(a);
}

void fold_fp_unfoldable(void) {
  Arena *a = arena_create();
  SemanticModule *app = module_of(a, path_of(a, 1, "app"),
                                  units_of(a, 1, "let PI: f64 = 3.14159265358979_d;\n"
                                                  "let DEG: f64 = PI / 180.0_d;\n"));
  SemanticModuleList *mods = modules_of(a, 1, app);
  SemanticRegistry r = semantic_registry_build(a, mods, NULL);
  semantic_fold_world(&r, a);

  SemanticRegistryEntry *pi = find_entry(&r, "PI");
  TEST_ASSERT_NOT_NULL(pi);
  TEST_ASSERT_EQUAL_HEX32(SEMANTIC_FC_FOLDABLE, pi->fold_class); // literal: foldable
  TEST_ASSERT_EQUAL_HEX32(SEMANTIC_CV_FLOAT, pi->value.kind);

  SemanticRegistryEntry *deg = find_entry(&r, "DEG");
  TEST_ASSERT_NOT_NULL(deg);
  TEST_ASSERT_EQUAL_HEX32(SEMANTIC_FC_UNFOLDABLE_FP, deg->fold_class); // irgen resolves
  TEST_ASSERT_FALSE(deg->guard_usable);

  arena_destroy(a);
}

void fold_layout_deferred(void) {
  Arena *a = arena_create();
  SemanticModule *app = module_of(a, path_of(a, 1, "app"),
                                  units_of(a, 1, "struct S;\nlet N = @sizeof(S);\n"));
  SemanticModuleList *mods = modules_of(a, 1, app);
  SemanticRegistry r = semantic_registry_build(a, mods, NULL);
  semantic_fold_world(&r, a);

  SemanticRegistryEntry *n = find_entry(&r, "N");
  TEST_ASSERT_NOT_NULL(n);
  TEST_ASSERT_EQUAL_HEX32(SEMANTIC_FC_LAYOUT, n->fold_class);
  TEST_ASSERT_FALSE(n->guard_usable);
  TEST_ASSERT_EQUAL_HEX32(SEMANTIC_CV_DEFERRED, n->value.kind);

  arena_destroy(a);
}

void fold_symbolic_and_address_alias(void) {
  Arena *a = arena_create();
  SemanticModule *app = module_of(a, path_of(a, 1, "app"),
                                  units_of(a, 1, "func read():i32;\n"
                                                 "let P: &u32 = @const(5u);\n"
                                                 "let F = read;\n"
                                                 "let Q = P;\n"));
  SemanticModuleList *mods = modules_of(a, 1, app);
  SemanticRegistry r = semantic_registry_build(a, mods, NULL);
  semantic_fold_world(&r, a);

  TEST_ASSERT_EQUAL_HEX32(SEMANTIC_FC_SYMBOLIC, find_entry(&r, "F")->fold_class);
  TEST_ASSERT_EQUAL_HEX32(SEMANTIC_FC_SYMBOLIC, find_entry(&r, "P")->fold_class);
  TEST_ASSERT_EQUAL_HEX32(SEMANTIC_FC_SYMBOLIC, find_entry(&r, "Q")->fold_class);

  arena_destroy(a);
}

void fold_string_flags(void) {
  Arena *a = arena_create();
  SemanticParam os = {.name = strview_from_cstr("TARGET_OS"), .value = strview_from_cstr("linux")};
  SemanticParamList params = {.param = os, .next = NULL};
  // The platform fact lives in core (prelude); the derived flag lives in app.
  SemanticModule *core = module_of(a, path_of(a, 1, "core"),
                                   units_of(a, 1, "@intrinsic let TARGET_OS: String8;\n"));
  SemanticModule *app = module_of(a, path_of(a, 1, "app"),
                                  units_of(a, 1, "let OS_LINUX = \"linux\";\n"
                                                 "let IS_LINUX = TARGET_OS == OS_LINUX;\n"));
  SemanticModuleList *mods = modules_of(a, 2, core, app);
  SemanticRegistry r = semantic_registry_build(a, mods, &params);
  semantic_fold_world(&r, a);

  SemanticRegistryEntry *flag = find_entry(&r, "IS_LINUX");
  TEST_ASSERT_NOT_NULL(flag);
  TEST_ASSERT_EQUAL_HEX32(SEMANTIC_FC_FOLDABLE, flag->fold_class);
  TEST_ASSERT_EQUAL_HEX32(SEMANTIC_CV_BOOL, flag->value.kind);
  TEST_ASSERT_EQUAL_UINT64(1, flag->value.bits);
  TEST_ASSERT_TRUE(flag->guard_usable);

  arena_destroy(a);
}

void fold_core_builtins(void) {
  Arena *a = arena_create();
  // core's own constants are toolchain-provided (§12.1): having core in the
  // closure and folding true/false shall need no driver-supplied value, and
  // shall produce no diagnostics at all.
  SemanticModule *core = module_of(a, path_of(a, 1, "core"),
                                   units_of(a, 1, "@intrinsic let true: bool;\n"
                                                  "@intrinsic let false: bool;\n"));
  SemanticModule *app = module_of(a, path_of(a, 1, "app"),
                                  units_of(a, 1, "let T = true;\nlet F = false;\n"));
  SemanticModuleList *mods = modules_of(a, 2, core, app);
  SemanticRegistry r = semantic_registry_build(a, mods, NULL);
  semantic_fold_world(&r, a);

  TEST_ASSERT_NULL(semantic_registry_errors(&r));

  SemanticRegistryEntry *t = find_entry(&r, "T");
  TEST_ASSERT_EQUAL_HEX32(SEMANTIC_FC_FOLDABLE, t->fold_class);
  TEST_ASSERT_EQUAL_HEX32(SEMANTIC_CV_BOOL, t->value.kind);
  TEST_ASSERT_EQUAL_UINT64(1, t->value.bits);
  TEST_ASSERT_TRUE(t->guard_usable);

  SemanticRegistryEntry *f = find_entry(&r, "F");
  TEST_ASSERT_EQUAL_HEX32(SEMANTIC_FC_FOLDABLE, f->fold_class);
  TEST_ASSERT_EQUAL_UINT64(0, f->value.bits);

  arena_destroy(a);
}

void fold_core_builtin_shadowing(void) {
  Arena *a = arena_create();
  // A package may shadow `true`/`false` (§10.4); the shadowing declaration is
  // an ordinary let and governs — the toolchain's value must not leak through.
  SemanticModule *core = module_of(a, path_of(a, 1, "core"),
                                   units_of(a, 1, "@intrinsic let true: bool;\n"));
  SemanticModule *app = module_of(a, path_of(a, 1, "app"),
                                  units_of(a, 1, "let true = 7u32;\nlet X = true;\n"));
  SemanticModuleList *mods = modules_of(a, 2, core, app);
  SemanticRegistry r = semantic_registry_build(a, mods, NULL);
  semantic_fold_world(&r, a);

  SemanticRegistryEntry *x = find_entry(&r, "X");
  TEST_ASSERT_EQUAL_HEX32(SEMANTIC_CV_INT, x->value.kind);
  TEST_ASSERT_EQUAL_HEX32(SEMANTIC_INT_U32, x->value.int_type);
  TEST_ASSERT_EQUAL_UINT64(7, x->value.bits);

  arena_destroy(a);
}

void fold_div_zero_diag(void) {
  Arena *a = arena_create();
  SemanticModule *app = module_of(a, path_of(a, 1, "app"), units_of(a, 1, "let Z = 1u32 / 0u32;\n"));
  SemanticModuleList *mods = modules_of(a, 1, app);
  SemanticRegistry r = semantic_registry_build(a, mods, NULL);
  semantic_fold_world(&r, a);

  TEST_ASSERT_EQUAL_INT(1, errors_with_code(&r, SEMANTIC_CONST_DIV_ZERO));

  arena_destroy(a);
}

void fold_unknown_name_deferred(void) {
  Arena *a = arena_create();
  SemanticModule *app = module_of(a, path_of(a, 1, "app"), units_of(a, 1, "let U = NOPE + 1u;\n"));
  SemanticModuleList *mods = modules_of(a, 1, app);
  SemanticRegistry r = semantic_registry_build(a, mods, NULL);
  semantic_fold_world(&r, a);

  SemanticRegistryEntry *u = find_entry(&r, "U");
  TEST_ASSERT_NOT_NULL(u);
  TEST_ASSERT_EQUAL_HEX32(SEMANTIC_FC_DEFERRED, u->fold_class);
  TEST_ASSERT_FALSE(u->guard_usable);
  TEST_ASSERT_NULL(semantic_registry_errors(&r)); // unknown at P1 is not a diagnostic

  arena_destroy(a);
}

void fold_knob_value(void) {
  Arena *a = arena_create();
  SemanticParam lvl = {.name = strview_from_cstr("LOG_LEVEL"), .value = strview_from_cstr("2")};
  SemanticParamList params = {.param = lvl, .next = NULL};
  SemanticModule *app = module_of(a, path_of(a, 1, "app"),
                                  units_of(a, 1, "@feature let LOG_LEVEL: i32;\n"
                                                 "let LOUD = LOG_LEVEL > 1i32;\n"));
  SemanticModuleList *mods = modules_of(a, 1, app);
  SemanticRegistry r = semantic_registry_build(a, mods, &params);
  semantic_fold_world(&r, a);

  SemanticRegistryEntry *knob = find_entry(&r, "LOG_LEVEL");
  TEST_ASSERT_NOT_NULL(knob);
  TEST_ASSERT_EQUAL_HEX32(SEMANTIC_FC_FOLDABLE, knob->fold_class);
  TEST_ASSERT_EQUAL_HEX32(SEMANTIC_CV_INT, knob->value.kind);
  TEST_ASSERT_EQUAL_HEX32(SEMANTIC_INT_I32, knob->value.int_type);
  TEST_ASSERT_EQUAL_UINT64(2, knob->value.bits);
  TEST_ASSERT_TRUE(knob->guard_usable);

  SemanticRegistryEntry *loud = find_entry(&r, "LOUD");
  TEST_ASSERT_NOT_NULL(loud);
  TEST_ASSERT_EQUAL_HEX32(SEMANTIC_FC_FOLDABLE, loud->fold_class);
  TEST_ASSERT_EQUAL_HEX32(SEMANTIC_CV_BOOL, loud->value.kind);
  TEST_ASSERT_EQUAL_UINT64(1, loud->value.bits);

  arena_destroy(a);
}

static const TestDispatchEntry ENTRIES[] = {
    {"fold_chain_int", fold_chain_int},
    {"fold_cycle_diagnostic", fold_cycle_diagnostic},
    {"fold_fp_unfoldable", fold_fp_unfoldable},
    {"fold_layout_deferred", fold_layout_deferred},
    {"fold_symbolic_and_address_alias", fold_symbolic_and_address_alias},
    {"fold_string_flags", fold_string_flags},
    {"fold_core_builtins", fold_core_builtins},
    {"fold_core_builtin_shadowing", fold_core_builtin_shadowing},
    {"fold_div_zero_diag", fold_div_zero_diag},
    {"fold_unknown_name_deferred", fold_unknown_name_deferred},
    {"fold_knob_value", fold_knob_value},
};

TEST_DISPATCH_MAIN(ENTRIES)
