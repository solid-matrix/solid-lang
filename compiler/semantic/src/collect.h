#pragma once

#include "arena.h"

#include "semantic_common.h"
#include "semantic_error.h"
#include "semantic_module.h"
#include "symboltable.h"

/**
 * @brief Collect outcome: the world symbol table and the pass's diagnostics.
 * @details @p symbols is the world table: every module's declarations under
 *          its canonical root, namespaces relative to that root. @p errors
 *          is newest-first — the orchestrator concatenates pass chains and
 *          reverses once at the exit.
 */
typedef struct {
  SemanticSymbolTable *symbols;
  SemanticErrorList *errors;
} SemanticCollectResult;

/**
 * @brief Collects every module's top-level declarations into one table.
 * @details Walks @p modules in list order, each program in list order, each
 *          declaration in source order. A collection error never stops the
 *          walk. @p params backs @c @when gating, which is not implemented
 *          yet — every declaration is currently live.
 * @param arena Backs the table and every diagnostic; must outlive both.
 * @param modules The modules to collect; NULL collects nothing.
 * @param params Compile-time parameters; unused until the gate lands.
 * @return The world table and the diagnostics, newest-first.
 */
SemanticCollectResult semantic_collect(Arena *arena, const SemanticModuleList *modules,
                                       const SemanticParamList *params);
