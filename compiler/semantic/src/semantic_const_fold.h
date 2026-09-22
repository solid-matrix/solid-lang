/**
 * @file semantic_const_fold.h
 * @brief The constant-expression folder: classifies and evaluates every
 *        foldable top-level initializer over the registry (spec §10.1).
 * @author solid-matrix
 */

#pragma once

#include "semantic_registry.h"

/**
 * @brief Folds the constant world: walks every registered let, classifies it
 *        (foldable / unfoldable-FP / layout / deferred / symbolic), builds
 *        the dependency order implicitly through DFS with memoization, and
 *        fills the folded values. Diagnoses cycles, out-of-range literals,
 *        and panic-class operations in constant context.
 */
void semantic_fold_world(SemanticRegistry *registry, Arena *arena);

/**
 * @brief Folds one `@when` condition against the (unpruned) constant world
 *        (§10.4). Values come from the memoized world fold; constant
 *        references made by the condition are recorded on @p owner as
 *        guard edges for the gating-graph acyclicity check.
 * @param not_foldable Set when the condition reaches a non-foldable operand
 *                     (layout query, symbolic binding, unknown name, or
 *                     floating-point arithmetic); the numeric diagnostics of
 *                     a failing evaluation are emitted here as a side effect.
 */
SemanticEvalOutcome semantic_fold_guard(SemanticRegistry *registry, SemanticRegistryEntry *owner,
                                        SyntaxNode *guard, Arena *arena, int *not_foldable);
