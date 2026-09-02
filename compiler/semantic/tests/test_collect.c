/**
 * @file test_collect.c
 * @brief Unit tests for the collect pass.
 * @author solid-matrix
 */

#include <stdarg.h>

#include "error.h"
#include "internal.h"
#include "semantic_fixture.h"
#include "test_support.h"

static SyntaxNode *lookup_at(SemanticSymbolTable *table, Arena *arena, int count, ...) {
  va_list args;
  va_start(args, count);
  SemanticNamePath *path = path_vof(arena, count, args);
  va_end(args);
  return semantic_symbol_table_lookup(table, path);
}

void test_symbols_at_root_and_namespaced(void) {
  Arena *a = arena_create();
  SemanticModule *app =
      module_of(a, path_of(a, 1, "app"), units_of(a, 2, "struct T;\n", "namespace x;\nstruct Inner;\n"));
  SemanticCollectResult r = run_collect(a, modules_of(a, 1, app));

  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_NOT_NULL(lookup_at(r.symbol_table, a, 2, "app", "T"));
  TEST_ASSERT_NOT_NULL(lookup_at(r.symbol_table, a, 3, "app", "x", "Inner"));
  TEST_ASSERT_NULL(lookup_at(r.symbol_table, a, 2, "app", "Inner"));
  TEST_ASSERT_NULL(lookup_at(r.symbol_table, a, 1, "T"));

  arena_destroy(a);
}

void test_namespace_context_from_prologue(void) {
  Arena *a = arena_create();
  const char *text = "namespace x;\nstruct S;\nlet v:i32;\nfunc f():i32;\n";
  SemanticModule *app = module_of(a, path_of(a, 1, "app"), units_of(a, 1, text));
  SemanticCollectResult r = run_collect(a, modules_of(a, 1, app));

  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_NOT_NULL(lookup_at(r.symbol_table, a, 3, "app", "x", "S"));
  TEST_ASSERT_NOT_NULL(lookup_at(r.symbol_table, a, 3, "app", "x", "v"));
  TEST_ASSERT_NOT_NULL(lookup_at(r.symbol_table, a, 3, "app", "x", "f"));
  TEST_ASSERT_NULL(lookup_at(r.symbol_table, a, 2, "app", "S"));

  arena_destroy(a);
}

void test_nested_namespace_segments(void) {
  Arena *a = arena_create();
  const char *text = "namespace x::y;\nstruct S;\n";
  SemanticModule *app = module_of(a, path_of(a, 1, "app"), units_of(a, 1, text));
  SemanticCollectResult r = run_collect(a, modules_of(a, 1, app));

  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_NOT_NULL(lookup_at(r.symbol_table, a, 4, "app", "x", "y", "S"));
  TEST_ASSERT_NULL(lookup_at(r.symbol_table, a, 3, "app", "x", "S"));

  arena_destroy(a);
}

void test_two_files_merge_namespace(void) {
  Arena *a = arena_create();
  SemanticModule *app =
      module_of(a, path_of(a, 1, "app"), units_of(a, 2, "namespace x;\nstruct T;\n", "namespace x;\nstruct U;\n"));
  SemanticCollectResult r = run_collect(a, modules_of(a, 1, app));

  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_NOT_NULL(lookup_at(r.symbol_table, a, 3, "app", "x", "T"));
  TEST_ASSERT_NOT_NULL(lookup_at(r.symbol_table, a, 3, "app", "x", "U"));

  arena_destroy(a);
}

void test_distinct_modules_no_collision(void) {
  Arena *a = arena_create();
  const char *text = "namespace x;\nstruct T;\n";
  SemanticModule *am = module_of(a, path_of(a, 2, "a", "m"), units_of(a, 1, text));
  SemanticModule *bm = module_of(a, path_of(a, 2, "b", "m"), units_of(a, 1, text));
  SemanticCollectResult r = run_collect(a, modules_of(a, 2, am, bm));

  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_NOT_NULL(lookup_at(r.symbol_table, a, 4, "a", "m", "x", "T"));
  TEST_ASSERT_NOT_NULL(lookup_at(r.symbol_table, a, 4, "b", "m", "x", "T"));

  arena_destroy(a);
}

void test_symbol_redefined_reports_and_keeps_first(void) {
  Arena *a = arena_create();
  SyntaxProgram *unit = parse_unit(a, "struct T;\nstruct T;\n");
  SemanticProgramList *units = arena_alloc(a, sizeof *units);
  units->program = unit;
  units->next = NULL;
  SemanticModule *app = module_of(a, path_of(a, 1, "app"), units);
  SyntaxNode *first = unit->top_levels->head;
  SyntaxNode *second = unit->top_levels->tail->head;

  SemanticCollectResult r = run_collect(a, modules_of(a, 1, app));

  TEST_ASSERT_EQUAL_size_t(1, semantic_errorlist_length(r.errors));
  TEST_ASSERT_EQUAL_HEX32(SEMANTIC_SYMBOL_REDEFINED, r.errors->head.code);
  TEST_ASSERT_EQUAL_size_t(second->span.start, r.errors->head.span.start);
  TEST_ASSERT_TRUE(lookup_at(r.symbol_table, a, 2, "app", "T") == first);

  arena_destroy(a);
}

void test_clash_symbol_then_namespace(void) {
  Arena *a = arena_create();
  SemanticModule *app =
      module_of(a, path_of(a, 1, "app"), units_of(a, 2, "namespace m;\nstruct x;\n", "namespace m::x::y;\n"));
  SemanticCollectResult r = run_collect(a, modules_of(a, 1, app));

  TEST_ASSERT_EQUAL_size_t(1, semantic_errorlist_length(r.errors));
  TEST_ASSERT_EQUAL_HEX32(SEMANTIC_SYMBOL_NAMESPACE_CLASH, r.errors->head.code);
  TEST_ASSERT_NOT_NULL(lookup_at(r.symbol_table, a, 3, "app", "m", "x"));
  TEST_ASSERT_NULL(lookup_at(r.symbol_table, a, 4, "app", "m", "x", "y"));

  arena_destroy(a);
}

void test_clash_namespace_then_symbol(void) {
  Arena *a = arena_create();
  SemanticModule *app =
      module_of(a, path_of(a, 1, "app"), units_of(a, 2, "namespace m::x;\n", "namespace m;\nstruct x;\n"));
  SemanticCollectResult r = run_collect(a, modules_of(a, 1, app));

  TEST_ASSERT_EQUAL_size_t(1, semantic_errorlist_length(r.errors));
  TEST_ASSERT_EQUAL_HEX32(SEMANTIC_SYMBOL_NAMESPACE_CLASH, r.errors->head.code);
  TEST_ASSERT_NULL(lookup_at(r.symbol_table, a, 3, "app", "m", "x"));

  arena_destroy(a);
}

void test_enum_fields_not_registered(void) {
  Arena *a = arena_create();
  SemanticModule *app = module_of(a, path_of(a, 1, "app"), units_of(a, 1, "enum Color { Red, Green }\n"));
  SemanticCollectResult r = run_collect(a, modules_of(a, 1, app));

  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_NOT_NULL(lookup_at(r.symbol_table, a, 2, "app", "Color"));
  TEST_ASSERT_NULL(lookup_at(r.symbol_table, a, 3, "app", "Color", "Red"));

  arena_destroy(a);
}

void test_all_decl_kinds_defined(void) {
  Arena *a = arena_create();
  const char *text = "let v:i32;\n"
                     "struct S;\n"
                     "enum E { A }\n"
                     "union U { m:i32 }\n"
                     "variant V { None, Some:i32 }\n"
                     "contract C(p:i32):i32;\n"
                     "func f():i32;\n";
  SemanticModule *app = module_of(a, path_of(a, 1, "app"), units_of(a, 1, text));
  SemanticCollectResult r = run_collect(a, modules_of(a, 1, app));

  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_NOT_NULL(lookup_at(r.symbol_table, a, 2, "app", "v"));
  TEST_ASSERT_NOT_NULL(lookup_at(r.symbol_table, a, 2, "app", "S"));
  TEST_ASSERT_NOT_NULL(lookup_at(r.symbol_table, a, 2, "app", "E"));
  TEST_ASSERT_NOT_NULL(lookup_at(r.symbol_table, a, 2, "app", "U"));
  TEST_ASSERT_NOT_NULL(lookup_at(r.symbol_table, a, 2, "app", "V"));
  TEST_ASSERT_NOT_NULL(lookup_at(r.symbol_table, a, 2, "app", "C"));
  TEST_ASSERT_NOT_NULL(lookup_at(r.symbol_table, a, 2, "app", "f"));

  arena_destroy(a);
}

void test_errors_newest_first_order_contract(void) {
  Arena *a = arena_create();
  SemanticModule *app = module_of(a, path_of(a, 1, "app"), units_of(a, 1, "struct T;\nstruct T;\nstruct T;\n"));
  SemanticCollectResult r = run_collect(a, modules_of(a, 1, app));

  TEST_ASSERT_EQUAL_size_t(2, semantic_errorlist_length(r.errors));
  TEST_ASSERT_EQUAL_HEX32(SEMANTIC_SYMBOL_REDEFINED, r.errors->head.code);
  TEST_ASSERT_EQUAL_HEX32(SEMANTIC_SYMBOL_REDEFINED, r.errors->tail->head.code);
  TEST_ASSERT_TRUE(r.errors->head.span.start > r.errors->tail->head.span.start);

  arena_destroy(a);
}

void test_using_declarations_skipped(void) {
  Arena *a = arena_create();
  SemanticModule *app =
      module_of(a, path_of(a, 1, "app"),
                units_of(a, 2, "namespace x;\nusing a;\nusing b::c;\nstruct T;\n", "using d;\nstruct U;\n"));
  SemanticCollectResult r = run_collect(a, modules_of(a, 1, app));

  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_NOT_NULL(lookup_at(r.symbol_table, a, 3, "app", "x", "T"));
  TEST_ASSERT_NOT_NULL(lookup_at(r.symbol_table, a, 2, "app", "U"));

  arena_destroy(a);
}

void test_reverse_table_maps_symbols_to_paths(void) {
  Arena *a = arena_create();
  SyntaxProgram *unit = parse_unit(a, "namespace x;\nstruct T;\n");
  SemanticProgramList *units = arena_alloc(a, sizeof *units);
  units->program = unit;
  units->next = NULL;
  SemanticModule *app = module_of(a, path_of(a, 1, "app"), units);
  SyntaxNode *decl = unit->top_levels->tail->head;

  SemanticCollectResult r = run_collect(a, modules_of(a, 1, app));

  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_TRUE(semantic_namepath_equals(semantic_namepath_table_lookup(r.namepath_table, decl), path_of(a, 3, "app", "x", "T")));

  arena_destroy(a);
}

void test_reverse_table_skips_redefined_symbols(void) {
  Arena *a = arena_create();
  SyntaxProgram *unit = parse_unit(a, "struct T;\nstruct T;\n");
  SemanticProgramList *units = arena_alloc(a, sizeof *units);
  units->program = unit;
  units->next = NULL;
  SemanticModule *app = module_of(a, path_of(a, 1, "app"), units);
  SyntaxNode *first = unit->top_levels->head;
  SyntaxNode *second = unit->top_levels->tail->head;

  SemanticCollectResult r = run_collect(a, modules_of(a, 1, app));

  TEST_ASSERT_EQUAL_size_t(1, semantic_errorlist_length(r.errors));
  TEST_ASSERT_TRUE(semantic_namepath_equals(semantic_namepath_table_lookup(r.namepath_table, first), path_of(a, 2, "app", "T")));
  TEST_ASSERT_NULL(semantic_namepath_table_lookup(r.namepath_table, second));

  arena_destroy(a);
}

void test_reverse_table_excludes_namespaces(void) {
  Arena *a = arena_create();
  SyntaxProgram *unit = parse_unit(a, "namespace n;\nstruct T;\n");
  SemanticProgramList *units = arena_alloc(a, sizeof *units);
  units->program = unit;
  units->next = NULL;
  SemanticModule *app = module_of(a, path_of(a, 1, "app"), units);
  SyntaxNode *ns_decl = unit->top_levels->head;

  SemanticCollectResult r = run_collect(a, modules_of(a, 1, app));

  TEST_ASSERT_NULL(r.errors);
  TEST_ASSERT_NULL(semantic_namepath_table_lookup(r.namepath_table, ns_decl));
  TEST_ASSERT_TRUE(semantic_namepath_equals(semantic_namepath_table_lookup(r.namepath_table, unit->top_levels->tail->head),
                               path_of(a, 3, "app", "n", "T")));

  arena_destroy(a);
}

static const TestDispatchEntry ENTRIES[] = {
    {"symbols_at_root_and_namespaced", test_symbols_at_root_and_namespaced},
    {"namespace_context_from_prologue", test_namespace_context_from_prologue},
    {"nested_namespace_segments", test_nested_namespace_segments},
    {"two_files_merge_namespace", test_two_files_merge_namespace},
    {"distinct_modules_no_collision", test_distinct_modules_no_collision},
    {"symbol_redefined_reports_and_keeps_first", test_symbol_redefined_reports_and_keeps_first},
    {"clash_symbol_then_namespace", test_clash_symbol_then_namespace},
    {"clash_namespace_then_symbol", test_clash_namespace_then_symbol},
    {"enum_fields_not_registered", test_enum_fields_not_registered},
    {"all_decl_kinds_defined", test_all_decl_kinds_defined},
    {"using_declarations_skipped", test_using_declarations_skipped},
    {"reverse_table_maps_symbols_to_paths", test_reverse_table_maps_symbols_to_paths},
    {"reverse_table_skips_redefined_symbols", test_reverse_table_skips_redefined_symbols},
    {"reverse_table_excludes_namespaces", test_reverse_table_excludes_namespaces},
    {"errors_newest_first_order_contract", test_errors_newest_first_order_contract},
};

TEST_DISPATCH_MAIN(ENTRIES)
