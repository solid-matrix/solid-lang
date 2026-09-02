#pragma once

#include "arena.h"
#include "binding_table.h"
#include "namepath_table.h"
#include "semantic_common.h"
#include "semantic_error.h"
#include "symbol_table.h"

typedef struct {
  Arena *arena;
  const SemanticModuleList *modules;
  const SemanticParamList *params;
} SemanticAnalyzer;

/**
 * @brief Collect outcome: the world symbol table, the reverse path table and
 *        the pass's diagnostics.
 * @details @p symbol_table is the world table: every module's declarations under
 *          its canonical root, namespaces relative to that root. @p namepath_table
 *          maps each live symbol's declaration to its full world path. @p
 *          errors is newest-first — the orchestrator concatenates pass
 *          chains and reverses once at the exit.
 */
typedef struct {
  SemanticSymbolTable *symbol_table;
  SemanticNamePathTable *namepath_table;
  SemanticErrorList *errors;
} SemanticCollectResult;

/**
 * @brief Collects every module's top-level declarations into one table.
 * @details Walks @p modules in list order, each program in list order, each
 *          declaration in source order. A collection error never stops the
 *          walk. @p params backs @c @when gating, which is not implemented
 *          yet — every declaration is currently live.
 * @param analyzer The compilation context: arena, modules and parameters.
 * @return The world table, the reverse path table and the diagnostics,
 *         newest-first.
 */
SemanticCollectResult semantic_collect(const SemanticAnalyzer *analyzer);

/**
 * @brief Resolve outcome: the binding map and the pass's diagnostics.
 * @details @p bindings holds one record per resolved use-site. @p errors is
 *          newest-first — the orchestrator concatenates pass chains and
 *          reverses once at the exit.
 */
typedef struct {
  SemanticBindingTable *binding_table;
  SemanticErrorList *errors;
} SemanticResolveResult;

/**
 * @brief Binds every name use-site to the entity it refers to.
 * @details Walks @p modules in list order, each program in list order, each
 *          declaration in source order. Types, expressions and statements
 *          are bound against the frozen world table @p symbols plus the
 *          per-file lexical scopes resolve builds as it walks. Qualified
 *          paths and using targets are module-relative first, world-root
 *          second; enum/variant members resolve through their declaration.
 *          A resolution error never stops the walk.
 * @param arena Backs the map and every diagnostic; must outlive both.
 * @param symbols The frozen world table from collect.
 * @param modules The modules to resolve; NULL resolves nothing.
 * @return The binding map and the diagnostics, newest-first.
 */
SemanticResolveResult semantic_resolve(const SemanticAnalyzer *analyzer, const SemanticSymbolTable *global_symbols);
