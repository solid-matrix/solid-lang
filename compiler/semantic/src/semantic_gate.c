/**
 * @file semantic_gate.c
 * @brief The gate stage of the prune pass (see semantic_gate.h).
 * @author solid-matrix
 */

#include "semantic_gate.h"

#include <string.h>

#include "semantic_const_fold.h"
#include "semantic_literal.h"
#include "symbol_table.h"

typedef struct {
  Arena *arena;
  SemanticRegistry *registry; // read/write: entries receive decisions and edges
  SemanticErrorList *errors;  // accumulated diagnostics
  int epoch;                  ///< closure-walk stamp, distinct per guarded node
} Gate;

static void diag(Gate *g, SemanticErrorCode code, Span span) {
  g->errors = semantic_errorlist_prepend(g->arena, g->errors, semantic_error_create(code, span));
}

// The §10.4 condition grammar is closed: names, suffixed integer literals,
// the operators `! && || == != < > <= >= + - * /`, and parentheses. A guard
// that steps outside it is ill-formed, however foldable the general constant
// grammar (§10.1) would make it — strings, runes, floating-point arithmetic,
// remainders, shifts, bitwise operators and composite literals stay out.
static bool operator_allowed(SyntaxOperator op) {
  switch (op) {
  case SYNTAX_OPERATOR_LNOT:
  case SYNTAX_OPERATOR_LAND:
  case SYNTAX_OPERATOR_LOR:
  case SYNTAX_OPERATOR_EQ:
  case SYNTAX_OPERATOR_NEQ:
  case SYNTAX_OPERATOR_LT:
  case SYNTAX_OPERATOR_GT:
  case SYNTAX_OPERATOR_LTE:
  case SYNTAX_OPERATOR_GTE:
  case SYNTAX_OPERATOR_ADD:
  case SYNTAX_OPERATOR_SUB:
  case SYNTAX_OPERATOR_MUL:
  case SYNTAX_OPERATOR_DIV:
    return true;
  default:
    return false;
  }
}

// A sign directly on an integer literal spells a signed literal (§4.4); a
// unary sign on anything else is outside the closed set.
static bool signed_literal(const SyntaxNode *node) {
  return node->kind == SYNTAX_KIND_INT_LIT_EXPR;
}

static bool guard_condition_ok(Gate *g, const SyntaxNode *node) {
  switch (node->kind) {
  case SYNTAX_KIND_INT_LIT_EXPR: {
    // A `@when` condition is a typed slot: every integer literal in it shall
    // carry the suffix of exactly its type (§4.4) — no position adapts a
    // literal to a slot.
    SemanticIntLiteral literal = semantic_int_literal(((SyntaxIntLitExpr *)node)->value);
    if (!literal.suffixed) {
      diag(g, SEMANTIC_GUARD_CONDITION, node->span);
      return false;
    }
    return true;
  }
  case SYNTAX_KIND_NAMED:
    return true;
  case SYNTAX_KIND_UNARY_EXPR: {
    SyntaxUnaryExpr *un = (SyntaxUnaryExpr *)node;
    bool sign = un->operator == SYNTAX_OPERATOR_MINUS || un->operator == SYNTAX_OPERATOR_PLUS;
    if (un->operator != SYNTAX_OPERATOR_LNOT && !(sign && signed_literal(un->operand))) {
      diag(g, SEMANTIC_GUARD_CONDITION, node->span);
      return false;
    }
    return guard_condition_ok(g, un->operand);
  }
  case SYNTAX_KIND_BINARY_EXPR: {
    SyntaxBinaryExpr *bin = (SyntaxBinaryExpr *)node;
    if (!operator_allowed(bin->operator))
      diag(g, SEMANTIC_GUARD_CONDITION, node->span);
    bool left = guard_condition_ok(g, bin->left);
    bool right = guard_condition_ok(g, bin->right);
    return operator_allowed(bin->operator) && left && right;
  }
  default:
    diag(g, SEMANTIC_GUARD_CONDITION, node->span);
    return false;
  }
}

// Decides one guard: the condition folds to `bool` over the unpruned world,
// or the declaration is pruned with a diagnostic. Guard edges are recorded
// as a side effect of folding (§10.4).
static void decide_one(Gate *g, SemanticRegistryEntry *e) {
  if (e->kind == SEMANTIC_RK_LET_FEATURE) {
    diag(g, SEMANTIC_GUARD_FORBIDDEN, e->decl->span); // a knob gates, never is gated
    e->survive = 0;
    return;
  }
  if (!guard_condition_ok(g, e->guard)) {
    e->survive = 0; // outside the closed grammar: never folded, no second diagnostic
    return;
  }

  int not_foldable = 0;
  SemanticEvalOutcome r = semantic_fold_guard(g->registry, e, e->guard, g->arena, &not_foldable);

  int ok = 1;
  if (not_foldable) {
    diag(g, SEMANTIC_CONST_TYPE, e->guard->span);
    ok = 0;
  } else if (r.status != SEMANTIC_EVAL_OK) {
    ok = 0; // numeric diagnostics were already emitted while folding
  } else if (r.value.kind != SEMANTIC_CV_BOOL) {
    diag(g, SEMANTIC_CONST_TYPE, e->guard->span);
    ok = 0;
  }
  e->survive = ok && r.status == SEMANTIC_EVAL_OK && r.value.bits != 0;
}

// Walks the constant-initializer closure of @p root's guard, attaching a
// gating-graph edge for every guarded declaration reached; a guarded node
// ends the walk — its own dependencies hang off its guard (§10.4).
static void collect_visit(Gate *g, SemanticRegistryEntry *root, SemanticRegistryEntry *e) {
  if (e->reach_mark == g->epoch)
    return;
  e->reach_mark = g->epoch;

  if (e->guard != NULL) {
    SemanticGateEdge *edge = arena_alloc(g->arena, sizeof *edge);
    edge->to = e;
    edge->next = root->gate_edges;
    root->gate_edges = edge;
    return;
  }
  for (SemanticRefEdge *r = e->refs; r != NULL; r = r->next)
    collect_visit(g, root, r->to);
}

// DFS over the gating graph: every cycle — self-reference, mutual gating,
// or any longer loop, in any polarity — is diagnosed once (§10.4).
static void visit_gates(Gate *g, SemanticRegistryEntry *root) {
  if (root->gate_state == SEMANTIC_GVS_DONE)
    return;
  if (root->gate_state == SEMANTIC_GVS_EVALUATING) {
    diag(g, SEMANTIC_CONST_CYCLE, root->decl->span);
    return;
  }
  root->gate_state = SEMANTIC_GVS_EVALUATING;

  g->epoch++;
  for (SemanticRefEdge *r = root->guard_refs; r != NULL; r = r->next)
    collect_visit(g, root, r->to);

  for (SemanticGateEdge *edge = root->gate_edges; edge != NULL; edge = edge->next)
    visit_gates(g, edge->to); // a back edge lands on EVALUATING and is diagnosed

  root->gate_state = SEMANTIC_GVS_DONE;
}

// Same-name declarations that both survived were intentional alternatives
// until the guards were decided; now they collide (§10.4 Effect). The first
// declaration claims the name — the convention of the world table and the
// collect pass — and each later survivor is diagnosed and dropped, so the
// program the later passes see holds one spelling and reports one diagnostic.
static void resolve_name_collisions(Gate *g) {
  size_t count = 0;
  for (SemanticRegistryEntry *e = semantic_registry_entries(g->registry); e != NULL; e = e->next)
    count++;
  SemanticRegistryEntry **ordered = arena_alloc(g->arena, count * sizeof *ordered);
  size_t i = count;
  for (SemanticRegistryEntry *e = semantic_registry_entries(g->registry); e != NULL; e = e->next)
    ordered[--i] = e; // the chain is newest-first; fill back to front for source order

  SemanticSymbolTable *claimed = semantic_symbol_table_empty();
  for (i = 0; i < count; i++) {
    SemanticRegistryEntry *e = ordered[i];
    if (!e->survive)
      continue;
    if (semantic_symbol_table_lookup(claimed, e->path) != NULL) {
      diag(g, SEMANTIC_SYMBOL_REDEFINED, e->decl->span);
      e->survive = 0;
      continue;
    }
    claimed = semantic_symbol_table_insert(g->arena, claimed, e->path, e->decl);
  }
}

void semantic_decide_gates(SemanticRegistry *registry, Arena *arena) {
  Gate g = {.arena = arena, .registry = registry, .errors = registry->errors, .epoch = 0};

  for (SemanticRegistryEntry *e = semantic_registry_entries(registry); e != NULL; e = e->next) {
    e->survive = 1;
    e->gate_state = SEMANTIC_GVS_UNVISITED;
    e->reach_mark = 0;
  }
  for (SemanticRegistryEntry *e = semantic_registry_entries(registry); e != NULL; e = e->next) {
    if (e->guard != NULL)
      decide_one(&g, e);
    else if (e->guard_missing)
      e->survive = 0; // diagnosed at build: a guard with no condition decides nothing
  }
  for (SemanticRegistryEntry *e = semantic_registry_entries(registry); e != NULL; e = e->next)
    if (e->guard != NULL)
      visit_gates(&g, e);
  resolve_name_collisions(&g);

  registry->errors = g.errors;
}
