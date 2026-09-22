/**
 * @file prune.c
 * @brief The prune pass: pre-collect registry + conditional-compilation
 *        gating (spec §10.3–§10.6).
 * @author solid-matrix
 *
 * Stage layout inside this pass:
 *   build — every top-level declaration of the closure registered and
 *           shape-classified; guards attached; clashes diagnosed.
 *   fold  — the constant world: classes, folded values, cycles diagnosed.
 *   gate  — every `@when` decided against the unpruned world; survivors
 *           marked; gating-graph cycles and survivor collisions diagnosed.
 */

#include "semantic_prune.h"

#include <stdbool.h>

#include "namepath_table.h"
#include "semantic_const_fold.h"
#include "semantic_gate.h"

SemanticPruneResult semantic_prune(Arena *arena, const SemanticModuleList *modules,
                                   const SemanticParamList *params) {
  SemanticRegistry registry = semantic_registry_build(arena, modules, params);
  semantic_fold_world(&registry, arena);
  semantic_decide_gates(&registry, arena);
  return (SemanticPruneResult){.registry = registry, .errors = semantic_registry_errors(&registry)};
}

// Is @p decl subject to gating at all? Namespace prologues and using
// declarations are not declarations of the six gated kinds (§10.4 Position).
static bool gated_kind(const SyntaxNode *decl) {
  return decl->kind != SYNTAX_KIND_NAMESPACE_DECL && decl->kind != SYNTAX_KIND_USING_DECL;
}

// Copies @p decls, dropping every declaration gated out. The result shares
// the kept nodes; only list cells are new.
static SyntaxNodeList *filter_decls(Arena *arena, const SemanticNamePathTable *survivors,
                                    SyntaxNodeList *decls) {
  if (decls == NULL)
    return NULL;
  SyntaxNodeList *rest = filter_decls(arena, survivors, decls->tail);
  if (gated_kind(decls->head) && semantic_namepath_table_lookup(survivors, decls->head) == NULL)
    return rest; // pruned: never collected, resolved, or checked
  SyntaxNodeList *cell = arena_alloc(arena, sizeof *cell);
  cell->head = decls->head;
  cell->tail = rest;
  return cell;
}

SemanticModuleList *semantic_prune_survivors(Arena *arena, const SemanticRegistry *registry,
                                             const SemanticModuleList *modules) {
  // The membership index: one path entry per surviving declaration. A
  // declaration absent from the index was gated out.
  SemanticNamePathTable *survivors = semantic_namepath_table_empty();
  for (const SemanticRegistryEntry *e = semantic_registry_entries(registry); e != NULL; e = e->next)
    if (e->survive)
      survivors = semantic_namepath_table_insert(arena, survivors, e->decl, e->path);

  SemanticModuleList *head = NULL;
  SemanticModuleList *tail = NULL;
  for (const SemanticModuleList *it = modules; it != NULL; it = it->next) {
    SemanticProgramList *units_head = NULL;
    SemanticProgramList *units_tail = NULL;
    for (const SemanticProgramList *unit = it->module->programs; unit != NULL; unit = unit->next) {
      SyntaxProgram *program = arena_alloc(arena, sizeof *program);
      program->header = unit->program->header;
      program->top_levels = filter_decls(arena, survivors, unit->program->top_levels);

      SemanticProgramList *cell = arena_alloc(arena, sizeof *cell);
      cell->program = program;
      cell->next = NULL;
      if (units_tail == NULL)
        units_head = cell;
      else
        units_tail->next = cell;
      units_tail = cell;
    }

    SemanticModule *module = arena_alloc(arena, sizeof *module);
    module->path = it->module->path;
    module->programs = units_head;

    SemanticModuleList *cell = arena_alloc(arena, sizeof *cell);
    cell->module = module;
    cell->next = NULL;
    if (tail == NULL)
      head = cell;
    else
      tail->next = cell;
    tail = cell;
  }
  return head;
}
