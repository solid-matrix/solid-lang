/**
 * @file test_resolve.c
 * @brief Unit tests for the resolve pass.
 * @author solid-matrix
 */

#include <stdarg.h>
#include <stdlib.h>

#include "error.h"
#include "internal.h"
#include "semantic_fixture.h"
#include "test_support.h"

// Collects (must be clean) and then resolves the modules.
static SemanticResolveResult run_resolve(Arena *arena, const SemanticModuleList *modules) {
  SemanticAnalyzer analyzer = {.arena = arena, .modules = modules, .params = NULL};
  SemanticCollectResult cres = semantic_collect(&analyzer);
  if (cres.errors != NULL)
    abort();
  return semantic_resolve(&analyzer, cres.symbol_table);
}

// Chains already-parsed units: helper for tests that need node access.
static SemanticProgramList *chain_of(Arena *arena, size_t count, SyntaxProgram **programs) {
  SemanticProgramList *head = NULL;
  SemanticProgramList *tail = NULL;
  for (size_t i = 0; i < count; i++) {
    SemanticProgramList *cell = arena_alloc(arena, sizeof *cell);
    cell->program = programs[i];
    cell->next = NULL;
    if (tail == NULL)
      head = cell;
    else
      tail->next = cell;
    tail = cell;
  }
  return head;
}

static void assert_one_error(SemanticResolveResult r, SemanticErrorCode code) {
  TEST_ASSERT_EQUAL_size_t(1, semantic_errorlist_length(r.errors));
  TEST_ASSERT_EQUAL_HEX32(code, r.errors->head.code);
}

void test_types_resolved(void) {
  Arena *a = arena_create();
  SyntaxProgram *units[1] = {parse_unit(a, "struct T;\nfunc f(v: T) {\n}\n")};
  SemanticModule *app = module_of(a, path_of(a, 1, "app"), chain_of(a, 1, units));
  SyntaxNode *struct_t = units[0]->top_levels->head;
  SyntaxFuncDecl *f = (SyntaxFuncDecl *)units[0]->top_levels->tail->head;
  SyntaxNode *param_type = ((SyntaxCallParam *)f->call_params->head)->type;

  SemanticResolveResult r = run_resolve(a, modules_of(a, 1, app));

  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_TRUE(semantic_binding_table_lookup(r.binding_table, param_type) == struct_t);

  arena_destroy(a);
}

void test_forward_reference_across_files(void) {
  Arena *a = arena_create();
  SyntaxProgram *units[2] = {parse_unit(a, "func f(t: T) {\n}\n"), parse_unit(a, "struct T;\n")};
  SemanticModule *app = module_of(a, path_of(a, 1, "app"), chain_of(a, 2, units));
  SyntaxNode *struct_t = units[1]->top_levels->head;
  SyntaxFuncDecl *f = (SyntaxFuncDecl *)units[0]->top_levels->head;
  SyntaxNode *param_type = ((SyntaxCallParam *)f->call_params->head)->type;

  SemanticResolveResult r = run_resolve(a, modules_of(a, 1, app));

  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_TRUE(semantic_binding_table_lookup(r.binding_table, param_type) == struct_t);

  arena_destroy(a);
}

void test_using_import_bare_name(void) {
  Arena *a = arena_create();
  SyntaxProgram *units[2] = {parse_unit(a, "namespace lib;\nstruct Writer;\n"),
                             parse_unit(a, "using lib;\nstruct Doc { w: Writer }\n")};
  SemanticModule *app = module_of(a, path_of(a, 1, "app"), chain_of(a, 2, units));
  SyntaxNode *writer = units[0]->top_levels->tail->head;
  SyntaxStructDecl *doc = (SyntaxStructDecl *)units[1]->top_levels->tail->head;
  SyntaxNode *writer_type = ((SyntaxStructField *)doc->fields->head)->type;

  SemanticResolveResult r = run_resolve(a, modules_of(a, 1, app));

  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_TRUE(semantic_binding_table_lookup(r.binding_table, writer_type) == writer);

  arena_destroy(a);
}

void test_using_import_namespace_skips_prefix(void) {
  Arena *a = arena_create();
  SyntaxProgram *units[3] = {parse_unit(a, "namespace lib;\nstruct Writer;\n"),
                             parse_unit(a, "namespace lib::inner;\nstruct Deep;\n"),
                             parse_unit(a, "using lib;\nstruct Q { w: Writer, d: inner::Deep }\n")};
  SemanticModule *app = module_of(a, path_of(a, 1, "app"), chain_of(a, 3, units));
  SyntaxNode *writer = units[0]->top_levels->tail->head;
  SyntaxNode *deep = units[1]->top_levels->tail->head;
  SyntaxStructDecl *q = (SyntaxStructDecl *)units[2]->top_levels->tail->head;
  SyntaxNode *w_type = ((SyntaxStructField *)q->fields->head)->type;
  SyntaxNode *d_type = ((SyntaxStructField *)q->fields->tail->head)->type;

  SemanticResolveResult r = run_resolve(a, modules_of(a, 1, app));

  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_TRUE(semantic_binding_table_lookup(r.binding_table, w_type) == writer);
  TEST_ASSERT_TRUE(semantic_binding_table_lookup(r.binding_table, d_type) == deep);

  arena_destroy(a);
}

void test_using_target_not_namespace(void) {
  Arena *a = arena_create();
  SyntaxProgram *units[2] = {parse_unit(a, "struct S;\n"), parse_unit(a, "using S;\n")};
  SemanticModule *app = module_of(a, path_of(a, 1, "app"), chain_of(a, 2, units));
  SyntaxNode *using_decl = units[1]->top_levels->head;

  SemanticResolveResult r = run_resolve(a, modules_of(a, 1, app));

  assert_one_error(r, SEMANTIC_EXPECT_NAMESPACE);
  TEST_ASSERT_EQUAL_size_t(using_decl->span.start, r.errors->head.span.start);

  arena_destroy(a);
}

void test_qualified_member_binds_enum_field(void) {
  Arena *a = arena_create();
  SyntaxProgram *units[1] = {parse_unit(a, "enum Color { Red, Green }\nfunc f(c: Color) {\n  Color::Red;\n}\n")};
  SemanticModule *app = module_of(a, path_of(a, 1, "app"), chain_of(a, 1, units));
  SyntaxEnumDecl *color = (SyntaxEnumDecl *)units[0]->top_levels->head;
  SyntaxFuncDecl *f = (SyntaxFuncDecl *)units[0]->top_levels->tail->head;
  SyntaxBodyStmt *body = (SyntaxBodyStmt *)f->body;
  SyntaxNamed *use = (SyntaxNamed *)((SyntaxExprStmt *)body->stmts->head)->expr;

  SemanticResolveResult r = run_resolve(a, modules_of(a, 1, app));

  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_TRUE(semantic_binding_table_lookup(r.binding_table, (SyntaxNode *)use) == color->fields->head);

  arena_destroy(a);
}

void test_member_unknown_reports_no_member(void) {
  Arena *a = arena_create();
  SyntaxProgram *units[1] = {parse_unit(a, "enum Color { Red, Green }\nfunc f(c: Color) {\n  Color::Blue;\n}\n")};
  SemanticModule *app = module_of(a, path_of(a, 1, "app"), chain_of(a, 1, units));

  SemanticResolveResult r = run_resolve(a, modules_of(a, 1, app));

  assert_one_error(r, SEMANTIC_NO_MEMBER);

  arena_destroy(a);
}

void test_qualified_path_world_absolute(void) {
  Arena *a = arena_create();
  SyntaxProgram *other_units[1] = {parse_unit(a, "struct T;\n")};
  SyntaxProgram *app_units[1] = {parse_unit(a, "namespace app;\nlet v: other::T;\n")};
  SemanticModule *other = module_of(a, path_of(a, 1, "other"), chain_of(a, 1, other_units));
  SemanticModule *app = module_of(a, path_of(a, 1, "app"), chain_of(a, 1, app_units));
  SyntaxNode *struct_t = other_units[0]->top_levels->head;
  SyntaxLetDecl *let = (SyntaxLetDecl *)app_units[0]->top_levels->tail->head;

  SemanticResolveResult r = run_resolve(a, modules_of(a, 2, other, app));

  for (const SemanticErrorList *it = r.errors; it != NULL; it = it->tail)
    fprintf(stderr, "DEBUG error code=0x%x span=%zu..%zu\n", it->head.code, it->head.span.start, it->head.span.end);
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_TRUE(semantic_binding_table_lookup(r.binding_table, (SyntaxNode *)let->type) == struct_t);

  arena_destroy(a);
}

void test_param_shadows_global_symbol(void) {
  Arena *a = arena_create();
  SyntaxProgram *units[1] = {parse_unit(a, "struct S;\nstruct T;\nfunc f(T: S) {\n  T;\n}\n")};
  SemanticModule *app = module_of(a, path_of(a, 1, "app"), chain_of(a, 1, units));
  SyntaxFuncDecl *f = (SyntaxFuncDecl *)units[0]->top_levels->tail->tail->head;
  SyntaxBodyStmt *body = (SyntaxBodyStmt *)f->body;
  SyntaxIdentifier *use = (SyntaxIdentifier *)((SyntaxExprStmt *)body->stmts->head)->expr;
  SyntaxNode *param = f->call_params->head;

  SemanticResolveResult r = run_resolve(a, modules_of(a, 1, app));

  for (const SemanticErrorList *it = r.errors; it != NULL; it = it->tail)
    fprintf(stderr, "DEBUG error code=0x%x span=%zu..%zu\n", it->head.code, it->head.span.start, it->head.span.end);
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_TRUE(semantic_binding_table_lookup(r.binding_table, (SyntaxNode *)use) == param);

  arena_destroy(a);
}

void test_duplicate_generic_params(void) {
  Arena *a = arena_create();
  SyntaxProgram *units[1] = {parse_unit(a, "func g<T, T>(x: T) {\n}\n")};
  SemanticModule *app = module_of(a, path_of(a, 1, "app"), chain_of(a, 1, units));

  SemanticResolveResult r = run_resolve(a, modules_of(a, 1, app));

  assert_one_error(r, SEMANTIC_DUPLICATE_GENERIC_PARAM);

  arena_destroy(a);
}

void test_duplicate_call_params(void) {
  Arena *a = arena_create();
  SyntaxProgram *units[1] = {parse_unit(a, "struct S;\nfunc f(a: S, a: S) {\n}\n")};
  SemanticModule *app = module_of(a, path_of(a, 1, "app"), chain_of(a, 1, units));

  SemanticResolveResult r = run_resolve(a, modules_of(a, 1, app));

  assert_one_error(r, SEMANTIC_DUPLICATE_PARAM_NAME);

  arena_destroy(a);
}

void test_duplicate_fields(void) {
  Arena *a = arena_create();
  SyntaxProgram *units[1] = {parse_unit(a, "struct S { x: S, x: S }\n")};
  SemanticModule *app = module_of(a, path_of(a, 1, "app"), chain_of(a, 1, units));

  SemanticResolveResult r = run_resolve(a, modules_of(a, 1, app));

  assert_one_error(r, SEMANTIC_DUPLICATE_FIELD_NAME);

  arena_destroy(a);
}

void test_fulfills_binds_contract(void) {
  Arena *a = arena_create();
  SyntaxProgram *units[2] = {parse_unit(a, "struct I;\n"),
                             parse_unit(a, "contract Addable(l: I): I;\nfunc add(l: I): I fulfills Addable {\n}\n")};
  SemanticModule *app = module_of(a, path_of(a, 1, "app"), chain_of(a, 2, units));
  SyntaxNode *contract = units[1]->top_levels->head;
  SyntaxFuncDecl *add = (SyntaxFuncDecl *)units[1]->top_levels->tail->head;

  SemanticResolveResult r = run_resolve(a, modules_of(a, 1, app));

  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_TRUE(semantic_binding_table_lookup(r.binding_table, (SyntaxNode *)add->fulfills->head) == contract);

  arena_destroy(a);
}

void test_generic_arity_mismatch(void) {
  Arena *a = arena_create();
  SyntaxProgram *units[2] = {parse_unit(a, "struct I;\n"),
                             parse_unit(a, "contract C<T>(x: I): I;\nfunc f(x: I): I fulfills C {\n}\n")};
  SemanticModule *app = module_of(a, path_of(a, 1, "app"), chain_of(a, 2, units));

  SemanticResolveResult r = run_resolve(a, modules_of(a, 1, app));

  assert_one_error(r, SEMANTIC_GENERIC_ARITY_MISMATCH);

  arena_destroy(a);
}

void test_unknown_name_anchored(void) {
  Arena *a = arena_create();
  SyntaxProgram *units[1] = {parse_unit(a, "func f() {\n  foo;\n}\n")};
  SemanticModule *app = module_of(a, path_of(a, 1, "app"), chain_of(a, 1, units));

  SemanticResolveResult r = run_resolve(a, modules_of(a, 1, app));

  assert_one_error(r, SEMANTIC_UNKNOWN_NAME);
  TEST_ASSERT_EQUAL_size_t(13, r.errors->head.span.start);

  arena_destroy(a);
}

void test_using_name_ambiguity_reports_at_use(void) {
  Arena *a = arena_create();
  SyntaxProgram *units[3] = {parse_unit(a, "namespace a;\nstruct T;\n"), parse_unit(a, "namespace b;\nstruct T;\n"),
                             parse_unit(a, "using a;\nusing b;\nfunc f(v: T) {\n}\n")};
  SemanticModule *app = module_of(a, path_of(a, 1, "app"), chain_of(a, 3, units));
  SyntaxNamed *use = (SyntaxNamed *)((SyntaxCallParam *)
      ((SyntaxFuncDecl *)units[2]->top_levels->tail->tail->head)->call_params->head)->type;

  SemanticResolveResult r = run_resolve(a, modules_of(a, 1, app));

  assert_one_error(r, SEMANTIC_AMBIGUOUS_NAME);
  TEST_ASSERT_EQUAL_size_t(use->header.span.start, r.errors->head.span.start);

  arena_destroy(a);
}

void test_duplicate_using_is_harmless(void) {
  Arena *a = arena_create();
  SyntaxProgram *units[2] = {parse_unit(a, "namespace a;\nstruct T;\nstruct U;\n"),
                             parse_unit(a, "using a;\nusing a;\nfunc f(v: T, w: U) {\n}\n")};
  SemanticModule *app = module_of(a, path_of(a, 1, "app"), chain_of(a, 2, units));

  SemanticResolveResult r = run_resolve(a, modules_of(a, 1, app));

  TEST_ASSERT_NULL(r.errors);

  arena_destroy(a);
}

void test_param_shadows_generic_param_reports(void) {
  Arena *a = arena_create();
  SyntaxProgram *units[1] = {parse_unit(a, "struct S;\nfunc g<T>(T: S) {\n}\n")};
  SemanticModule *app = module_of(a, path_of(a, 1, "app"), chain_of(a, 1, units));

  SemanticResolveResult r = run_resolve(a, modules_of(a, 1, app));

  assert_one_error(r, SEMANTIC_DUPLICATE_PARAM_NAME);

  arena_destroy(a);
}

void test_outer_block_let_vs_param_reports(void) {
  Arena *a = arena_create();
  fprintf(stderr, "DEBUG running outer_block\n");
  SyntaxProgram *units[1] = {parse_unit(a, "struct S;\nfunc f(a: S) {\n  let a: S = f(a);\n}\n")};
  SemanticModule *app = module_of(a, path_of(a, 1, "app"), chain_of(a, 1, units));

  SemanticResolveResult r = run_resolve(a, modules_of(a, 1, app));

  for (const SemanticErrorList *it = r.errors; it != NULL; it = it->tail)
    fprintf(stderr, "DEBUG error code=0x%x span=%zu..%zu\n", it->head.code, it->head.span.start, it->head.span.end);
  assert_one_error(r, SEMANTIC_DUPLICATE_LOCAL);

  arena_destroy(a);
}

void test_nested_block_shadows_param(void) {
  Arena *a = arena_create();
  fprintf(stderr, "DEBUG running nested_block\n");
  SyntaxProgram *units[1] = {parse_unit(a, "struct S;\nfunc f(t: S) {\n  {\n    let t: S = f(t);\n    t;\n  }\n}\n")};
  SemanticModule *app = module_of(a, path_of(a, 1, "app"), chain_of(a, 1, units));
  SyntaxFuncDecl *f = (SyntaxFuncDecl *)units[0]->top_levels->tail->head;
  SyntaxBodyStmt *outer = (SyntaxBodyStmt *)f->body;
  SyntaxBodyStmt *inner = (SyntaxBodyStmt *)outer->stmts->head;
  SyntaxNode *inner_let = inner->stmts->head;
  SyntaxIdentifier *use = (SyntaxIdentifier *)((SyntaxExprStmt *)inner->stmts->tail->head)->expr;

  SemanticResolveResult r = run_resolve(a, modules_of(a, 1, app));

  for (const SemanticErrorList *it = r.errors; it != NULL; it = it->tail)
    fprintf(stderr, "DEBUG error code=0x%x span=%zu..%zu\n", it->head.code, it->head.span.start, it->head.span.end);
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_TRUE(semantic_binding_table_lookup(r.binding_table, (SyntaxNode *)use) == inner_let);

  arena_destroy(a);
}

void test_implicit_core_merge(void) {
  Arena *a = arena_create();
  SyntaxProgram *core_units[1] = {parse_unit(a, "struct i32;\n")};
  SyntaxProgram *app_units[1] = {parse_unit(a, "struct U { v: i32 }\n")};
  SemanticModule *core = module_of(a, path_of(a, 1, "core"), chain_of(a, 1, core_units));
  SemanticModule *app = module_of(a, path_of(a, 1, "app"), chain_of(a, 1, app_units));
  SyntaxNode *core_i32 = core_units[0]->top_levels->head;
  SyntaxStructDecl *u = (SyntaxStructDecl *)app_units[0]->top_levels->head;
  SyntaxNode *field_type = ((SyntaxStructField *)u->fields->head)->type;

  SemanticResolveResult r = run_resolve(a, modules_of(a, 2, core, app));

  for (const SemanticErrorList *it = r.errors; it != NULL; it = it->tail)
    fprintf(stderr, "DEBUG error code=0x%x span=%zu..%zu\n", it->head.code, it->head.span.start, it->head.span.end);
  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_TRUE(semantic_binding_table_lookup(r.binding_table, field_type) == core_i32);

  arena_destroy(a);
}

static const TestDispatchEntry ENTRIES[] = {
    {"types_resolved", test_types_resolved},
    {"forward_reference_across_files", test_forward_reference_across_files},
    {"using_import_bare_name", test_using_import_bare_name},
    {"using_import_namespace_skips_prefix", test_using_import_namespace_skips_prefix},
    {"using_target_not_namespace", test_using_target_not_namespace},
    {"qualified_member_binds_enum_field", test_qualified_member_binds_enum_field},
    {"member_unknown_reports_no_member", test_member_unknown_reports_no_member},
    {"qualified_path_world_absolute", test_qualified_path_world_absolute},
    {"param_shadows_global_symbol", test_param_shadows_global_symbol},
    {"duplicate_generic_params", test_duplicate_generic_params},
    {"duplicate_call_params", test_duplicate_call_params},
    {"duplicate_fields", test_duplicate_fields},
    {"fulfills_binds_contract", test_fulfills_binds_contract},
    {"generic_arity_mismatch", test_generic_arity_mismatch},
    {"unknown_name_anchored", test_unknown_name_anchored},
    {"using_name_ambiguity_reports_at_use", test_using_name_ambiguity_reports_at_use},
    {"duplicate_using_is_harmless", test_duplicate_using_is_harmless},
    {"param_shadows_generic_param_reports", test_param_shadows_generic_param_reports},
    {"outer_block_let_vs_param_reports", test_outer_block_let_vs_param_reports},
    {"nested_block_shadows_param", test_nested_block_shadows_param},
    {"implicit_core_merge", test_implicit_core_merge},
};

TEST_DISPATCH_MAIN(ENTRIES)
