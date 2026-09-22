/**
 * @file semantic_registry.h
 * @brief The pre-collect constant world: every top-level declaration of the
 *        closure, registered and classified by shape (spec §10.3, §5.1).
 * @author solid-matrix
 *
 * A SemanticRegistry value is **immutable after
 * semantic_registry_build returns**: all queries are pure, and later passes
 * (folding, gating) fill their own result fields through their own APIs.
 */

#pragma once

#include "arena.h"
#include "semantic_cvalue.h"
#include "semantic_common.h"
#include "semantic_error.h"
#include "syntax_node.h"

typedef struct SemanticSymbolTable SemanticSymbolTable;
/**
 * @brief The declaration shape of a registered name.
 */
typedef enum {
  SEMANTIC_RK_LET,           ///< ordinary let; class decided from the init (§5.1)
  SEMANTIC_RK_LET_ADDRESS,   ///< init root is `@const(e)` / `@static(e)` (§6.2)
  SEMANTIC_RK_LET_FEATURE,   ///< `@feature let` — value injected from the manifest (§12.3)
  SEMANTIC_RK_LET_INTRINSIC, ///< `@intrinsic let` — value from the toolchain (§12.3)
  SEMANTIC_RK_LET_IMPORT,    ///< `@import let` — external symbol (§6.6)
  SEMANTIC_RK_LET_NO_INIT,   ///< let with no initializer and no external form (diagnosed)
  SEMANTIC_RK_FUNC,
  SEMANTIC_RK_STRUCT,
  SEMANTIC_RK_ENUM,
  SEMANTIC_RK_UNION,
  SEMANTIC_RK_CONTRACT,
} SemanticRegistryKind;

/**
 * @brief How a binding's value comes into existence (spec §5.1, §10.3).
 */
typedef enum {
  SEMANTIC_FC_FOLDABLE,      ///< constant expression; folded at P1; guard-usable
  SEMANTIC_FC_UNFOLDABLE_FP, ///< contains floating-point arithmetic; irgen resolves (IEEE)
  SEMANTIC_FC_LAYOUT,        ///< references `@sizeof` family; P6 resolves
  SEMANTIC_FC_DEFERRED,      ///< references names unresolvable at P1 (e.g. via using); P6 resolves
  SEMANTIC_FC_SYMBOLIC,      ///< function names / address bindings / composites over them
  SEMANTIC_FC_EXTERNAL,      ///< `@import let`
} SemanticFoldClass;

/**
 * @brief Per-entry fold states: the folder's DFS memoization scratch (the
 *        one deliberate mutation in the prune pass).
 */
typedef enum {
  SEMANTIC_EVS_UNVISITED = 0,
  SEMANTIC_EVS_EVALUATING = 1, ///< on the DFS stack: re-entry means a cycle
  SEMANTIC_EVS_DONE = 2,
} SemanticEvalState;

/**
 * @brief DFS states of the gating-graph walk (gate-stage scratch).
 */
typedef enum {
  SEMANTIC_GVS_UNVISITED = 0,
  SEMANTIC_GVS_EVALUATING = 1, ///< on the DFS stack: re-entry is a gating cycle
  SEMANTIC_GVS_DONE = 2,
} SemanticGateState;

typedef struct SemanticRegistryEntry SemanticRegistryEntry;

/**
 * @brief A constant reference made by an initializer or a guard. Edges are
 *        their own nodes: a declaration may be referenced from any number of
 *        places, so the link cannot live on the referenced entry (§10.4).
 */
typedef struct SemanticRefEdge SemanticRefEdge;
struct SemanticRefEdge {
  SemanticRegistryEntry *to;
  SemanticRefEdge *next;
};

/**
 * @brief One edge of the reduced gating graph: a guarded declaration
 *        depending on another guarded declaration (§10.4).
 */
typedef struct SemanticGateEdge SemanticGateEdge;
struct SemanticGateEdge {
  SemanticRegistryEntry *to;
  SemanticGateEdge *next;
};

/**
 * @brief One registered top-level declaration.
 */
struct SemanticRegistryEntry {
  SemanticNamePath *path;       ///< canonical: package :: namespace? :: name
  const SemanticModule *module; ///< the declaring package
  Strview name;
  SemanticRegistryKind kind;
  SyntaxNode *decl;
  SyntaxNode *guard;                 ///< the `@when` condition expression, or NULL
  int guard_missing;                 ///< a `@when` without a condition: ill-formed, pruned
  SemanticFoldClass fold_class;      ///< set by the constant-world fold (§10.1)
  SemanticCValue value;              ///< folded value (FOLDABLE scalars); DEFERRED otherwise
  int eval_state;                    ///< SemanticEvalState (fold scratch)
  int guard_usable;                  ///< foldable scalar of type `bool`/integer
  SemanticRegistryEntry *next;   ///< newest-first registry chain
  SemanticRefEdge *refs;         ///< constants named by the initializer (fold scratch)
  SemanticRefEdge *guard_refs;   ///< constants named by the guard (gate scratch)
  SemanticGateEdge *gate_edges;  ///< gated declarations this guard depends on
  int gate_state;                ///< SemanticGateState (gate DFS scratch)
  int reach_mark;                ///< closure-walk epoch stamp (gate scratch)
  int survive;                   ///< 1 when the declaration is in the program: its guard held
                                 ///< and no earlier same-name survivor claimed the name (§10.4)
};

/**
 * @brief The pre-collect constant world. Immutable after build: every field
 *        is construction output and every accessor is a pure query.
 */
typedef struct {
  SemanticSymbolTable *table;      ///< world shape: package :: namespace? :: name → decl
  SemanticRegistryEntry *entries;  ///< newest-first chain
  SemanticErrorList *errors;       ///< diagnostics: redefinitions, clashes, missing inits
  const SemanticParamList *params; ///< injected values (platform facts, knob values)
  const SemanticSymbolTable *core_table; ///< core prelude subtable (bare-name fallback)
} SemanticRegistry;

/**
 * @brief Registers every top-level declaration of @p modules (all
 *        translation units) under package :: namespace? :: name. Same-name
 *        declarations are all registered (the world table keeps the first
 *        spelling); whether a collision is ill-formed is decided after
 *        gating (§10.4). Symbol/namespace clashes are diagnosed here;
 *        `using` declarations are skipped.
 * @returns The immutable registry value.
 */
SemanticRegistry semantic_registry_build(Arena *arena, const SemanticModuleList *modules,
                                         const SemanticParamList *params);

/**
 * @brief The world-shaped table backing the registry (package :: namespace?
 *        :: name → declaration).
 */
SemanticSymbolTable *semantic_registry_table(const SemanticRegistry *registry);

/**
 * @brief The registry diagnostics, newest first.
 */
SemanticErrorList *semantic_registry_errors(const SemanticRegistry *registry);

/**
 * @brief All registered entries, newest first.
 */
SemanticRegistryEntry *semantic_registry_entries(const SemanticRegistry *registry);

/**
 * @brief Bare-name resolution for guards: the current package (all its
 *        namespaces) plus the core prelude. Several matches are ambiguous.
 */
typedef enum {
  SEMANTIC_RLOOKUP_MISS = 0,
  SEMANTIC_RLOOKUP_HIT,
  SEMANTIC_RLOOKUP_AMBIGUOUS,
} SemanticRegistryLookup;

SemanticRegistryLookup semantic_registry_lookup_bare(const SemanticRegistry *registry,
                                                     const SemanticModule *module, Strview name,
                                                     SyntaxNode **out_decl);

/**
 * @brief Path resolution: package-relative first, world-absolute second.
 */
SemanticRegistryLookup semantic_registry_lookup_path(const SemanticRegistry *registry,
                                                     const SemanticModule *module,
                                                     const SemanticNamePath *path,
                                                     SyntaxNode **out_decl);

/**
 * @brief The injected value of a knob (@feature let), or NULL when the
 *        manifest supplied none.
 */
const Strview *semantic_registry_knob_value(const SemanticRegistry *registry,
                                            const SemanticRegistryEntry *entry);
