/**
 * @file semantic_prune.h
 * @brief The prune pass: builds the pre-collect constant world and decides
 *        every `@when` guard (spec §10.3–§10.6).
 * @author solid-matrix
 */

#pragma once

#include "arena.h"
#include "semantic_common.h"
#include "semantic_error.h"
#include "semantic_registry.h"

/**
 * @brief Prune outcome: the name registry over all closure declarations and
 *        the pass's diagnostics (redefinitions, missing initializers,
 *        multiple guards). The survivor lists and the folded constant values
 *        land here as the pass grows.
 */
typedef struct {
  SemanticRegistry registry;    ///< immutable after build + fold
  SemanticErrorList *errors;    ///< newest first
} SemanticPruneResult;

/**
 * @brief Runs the prune pass over every module of the closure.
 * @param arena Backs the registry and every diagnostic; must outlive both.
 * @param modules The dependency closure to prune; NULL prunes nothing.
 * @param params Injected values — platform facts and knob defaults — matched
 *               to `@intrinsic`/`@feature` declarations by name.
 * @return The registry and the diagnostics, newest-first.
 */
SemanticPruneResult semantic_prune(Arena *arena, const SemanticModuleList *modules,
                                   const SemanticParamList *params);

/**
 * @brief The surviving declarations constitute the program (§10.4 Effect):
 *        rebuilds @p modules with every gated-out declaration removed, so
 *        later passes never see them. Namespace prologues and `using`
 *        declarations are not gated and are always kept; kept declarations
 *        keep their source order. Pure: the inputs are unchanged and the
 *        result shares the kept AST nodes, allocating only the new spine.
 * @param arena Backs the new nodes.
 * @param registry A pruned registry (semantic_prune has decided its gates).
 * @param modules The same closure semantic_prune was given.
 * @return The filtered closure.
 */
SemanticModuleList *semantic_prune_survivors(Arena *arena, const SemanticRegistry *registry,
                                             const SemanticModuleList *modules);
