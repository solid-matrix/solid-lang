/**
 * @file semantic_gate.h
 * @brief The gate stage of the prune pass: decides every `@when` guard over
 *        the folded constant world and prunes accordingly (spec §10.4).
 * @author solid-matrix
 */

#pragma once

#include "semantic_registry.h"

/**
 * @brief Decides every guard and marks the survivors. For each guarded
 *        declaration the condition is folded against the *unpruned* world —
 *        it shall fold to `bool`, else a diagnostic is emitted and the
 *        declaration is pruned. The gating graph (guard-referenced constants,
 *        transitively through constant initializers) shall be acyclic in any
 *        polarity; cycles are diagnosed. Finally, same-name declarations that
 *        both survive are diagnosed as redefinitions (§10.4 Effect) — before
 *        gating they are intentional alternatives.
 */
void semantic_decide_gates(SemanticRegistry *registry, Arena *arena);
