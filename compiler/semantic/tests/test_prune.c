/**
 * @file test_prune.c
 * @brief End-to-end tests for the prune pass: the surviving declarations are
 *        the program (spec §10.4 Effect, §10.6 P2 before P4).
 * @author solid-matrix
 */

#include <stdbool.h>

#include "semantic_analyze.h"
#include "semantic_fixture.h"
#include "semantic_prune.h"
#include "test_support.h"

// The k-th top-level declaration of the first unit of @p modules.
static SyntaxNode *top_level_at(const SemanticModuleList *modules, size_t k) {
  SyntaxNodeList *it = modules->module->programs->program->top_levels;
  for (size_t i = 0; i < k && it != NULL; i++)
    it = it->tail;
  return it != NULL ? it->head : NULL;
}

static size_t top_level_count(const SemanticModuleList *modules) {
  size_t n = 0;
  for (SyntaxNodeList *it = modules->module->programs->program->top_levels; it != NULL; it = it->tail)
    n++;
  return n;
}

// Is @p decl the declaration named @p name?
static bool named_decl(SyntaxNode *decl, const char *name) {
  return decl != NULL && decl->kind == SYNTAX_KIND_LET_DECL &&
         strview_equals(((SyntaxLetDecl *)decl)->id->value, strview_from_cstr(name));
}

void prune_drops_pruned_declaration(void) {
  Arena *a = arena_create();
  SemanticModule *app =
      module_of(a, path_of(a, 1, "app"),
                units_of(a, 1, "namespace cfg;\n"
                               "let A = 1u;\n"
                               "@when(1u == 2u) let B = 2u;\n"
                               "let C = 3u;\n"));
  SemanticModuleList *mods = modules_of(a, 1, app);

  SemanticPruneResult prune = semantic_prune(a, mods, NULL);
  SemanticModuleList *live = semantic_prune_survivors(a, &prune.registry, mods);

  // Prologue kept; B dropped; A and C keep their source order.
  TEST_ASSERT_EQUAL_size_t(3, top_level_count(live));
  TEST_ASSERT_EQUAL_HEX32(SYNTAX_KIND_NAMESPACE_DECL, top_level_at(live, 0)->kind);
  TEST_ASSERT_TRUE(named_decl(top_level_at(live, 1), "A"));
  TEST_ASSERT_TRUE(named_decl(top_level_at(live, 2), "C"));

  arena_destroy(a);
}

void prune_pruned_decl_never_resolved(void) {
  Arena *a = arena_create();
  // The pruned declaration names a type that does not exist: had it been
  // collected and resolved, the pass would report an unknown name.
  SemanticModule *app =
      module_of(a, path_of(a, 1, "app"),
                units_of(a, 1, "@when(1u == 2u) func f(v: NoSuchType):i32;\n"
                               "let Y = 2u;\n"));
  SemanticModuleList *mods = modules_of(a, 1, app);

  SemanticAnalyzeResult r = semantic_analyze(a, mods, NULL);

  TEST_ASSERT_NULL(r.errors);

  arena_destroy(a);
}

void prune_surviving_alternative_is_collected(void) {
  Arena *a = arena_create();
  // The first spelling is pruned, so the surviving alternative is the one
  // collected and resolved — the world table's first-wins lookup does not
  // leak into the program.
  SemanticModule *app =
      module_of(a, path_of(a, 1, "app"),
                units_of(a, 1, "@when(1u == 2u) let V = 1u;\n"
                               "@when(1u == 1u) let V = 2u;\n"
                               "let W = V;\n"));
  SemanticModuleList *mods = modules_of(a, 1, app);

  SemanticPruneResult prune = semantic_prune(a, mods, NULL);
  SemanticModuleList *live = semantic_prune_survivors(a, &prune.registry, mods);
  TEST_ASSERT_EQUAL_size_t(2, top_level_count(live));
  TEST_ASSERT_TRUE(named_decl(top_level_at(live, 0), "V"));
  TEST_ASSERT_TRUE(named_decl(top_level_at(live, 1), "W"));

  SemanticAnalyzeResult r = semantic_analyze(a, mods, NULL);
  TEST_ASSERT_NULL(r.errors); // disjoint guards: no redeclaration

  arena_destroy(a);
}

void prune_both_surviving_alternatives_collide(void) {
  Arena *a = arena_create();
  SemanticModule *app = module_of(a, path_of(a, 1, "app"),
                                  units_of(a, 1, "let V = 1u;\n"
                                                 "@when(1u == 1u) let V = 2u;\n"));
  SemanticModuleList *mods = modules_of(a, 1, app);

  SemanticAnalyzeResult r = semantic_analyze(a, mods, NULL);

  TEST_ASSERT_EQUAL_size_t(1, semantic_errorlist_length(r.errors));
  TEST_ASSERT_EQUAL_HEX32(SEMANTIC_SYMBOL_REDEFINED, r.errors->head.code);

  arena_destroy(a);
}

static const TestDispatchEntry ENTRIES[] = {
    {"prune_drops_pruned_declaration", prune_drops_pruned_declaration},
    {"prune_pruned_decl_never_resolved", prune_pruned_decl_never_resolved},
    {"prune_surviving_alternative_is_collected", prune_surviving_alternative_is_collected},
    {"prune_both_surviving_alternatives_collide", prune_both_surviving_alternatives_collide},
};

TEST_DISPATCH_MAIN(ENTRIES)
