/**
 * @file semantic_const_fold.c
 * @brief The constant-expression folder (see semantic_const_fold.h).
 * @author solid-matrix
 */

#include "semantic_const_fold.h"

#include <string.h>

#include "semantic_cvalue.h"
#include "semantic_error.h"
#include "namepath.h"

typedef struct {
  Arena *arena;
  SemanticRegistry *registry;  // read-only: name resolution
  SemanticErrorList *errors;   // accumulated diagnostics
  int record_guard;            ///< 1 while folding a guard: references land on guard_refs
} Folder;

typedef struct {
  int fp;       ///< contains floating-point arithmetic
  int layout;   ///< references the `@sizeof` family (resolved at P6)
  int unknown;  ///< references names unresolvable at P1 (e.g. via `using`; P6 resolves)
  int symbolic; ///< references function names / address bindings
} FoldFlags;

static void diag(Folder *f, SemanticErrorCode code, Span span) {
  f->errors = semantic_errorlist_prepend(f->arena, f->errors, semantic_error_create(code, span));
}

static SemanticEvalOutcome ok_value(SemanticCValue v) {
  return (SemanticEvalOutcome){.status = SEMANTIC_EVAL_OK, .value = v};
}

static SemanticEvalOutcome fail_status(SemanticEvalStatus s) {
  return (SemanticEvalOutcome){.status = s};
}

static SemanticCValue deferred_value(void) {
  return (SemanticCValue){.kind = SEMANTIC_CV_DEFERRED};
}

static int is_placeholder(const SemanticCValue *v) {
  return v->kind == SEMANTIC_CV_DEFERRED || v->kind == SEMANTIC_CV_SYMBOLIC ||
         v->kind == SEMANTIC_CV_FLOAT;
}

static SemanticRegistryEntry *entry_of(SemanticRegistry *registry, SyntaxNode *decl) {
  for (SemanticRegistryEntry *e = semantic_registry_entries(registry); e != NULL; e = e->next)
    if (e->decl == decl) return e;
  return NULL;
}

static SemanticNamePath *named_path(Arena *arena, SyntaxNamed *named) {
  SemanticNamePath *head = NULL;
  SemanticNamePath *walk = NULL;
  for (SyntaxNodeList *it = named->path; it != NULL; it = it->tail) {
    SemanticNamePath *cell = arena_alloc(arena, sizeof *cell);
    cell->head = ((SyntaxIdentifier *)it->head)->value;
    cell->tail = NULL;
    if (walk == NULL) head = cell;
    else walk->tail = cell;
    walk = cell;
  }
  return head;
}

static void fold_entry(Folder *f, SemanticRegistryEntry *e);

// Resolves a name reference from @p owner's package: bare — own package then
// core prelude; qualified — package-relative then world-absolute (§10.4).
static SemanticRegistryEntry *resolve_name(Folder *f, SemanticRegistryEntry *owner,
                                           SyntaxNamed *named) {
  size_t segs = 0;
  for (SyntaxNodeList *it = named->path; it != NULL; it = it->tail) segs++;

  if (segs == 1) {
    SyntaxIdentifier *id = (SyntaxIdentifier *)named->path->head;
    SyntaxNode *decl = NULL;
    if (semantic_registry_lookup_bare(f->registry, owner->module, id->value, &decl) ==
        SEMANTIC_RLOOKUP_HIT)
      return entry_of(f->registry, decl);
    return NULL;
  }

  SemanticNamePath *path = named_path(f->arena, named);
  SyntaxNode *decl = NULL;
  if (semantic_registry_lookup_path(f->registry, owner->module, path, &decl) == SEMANTIC_RLOOKUP_HIT)
    return entry_of(f->registry, decl);
  return NULL;
}

static SemanticEvalOutcome fold_reference(Folder *f, SemanticRegistryEntry *owner, FoldFlags *fl,
                                          SemanticRegistryEntry *target, SemanticCValue *out) {
  if (target->kind != SEMANTIC_RK_LET && target->kind != SEMANTIC_RK_LET_FEATURE &&
      target->kind != SEMANTIC_RK_LET_INTRINSIC) {
    fl->symbolic = 1; // type entities are not constant values
    *out = deferred_value();
    return ok_value(*out);
  }
  // Record the constant reference edge: initializer edges during the world
  // fold, guard edges while a condition is being folded (§10.4). Edges are
  // their own nodes — the referenced declaration may be named from anywhere.
  SemanticRefEdge **chain = f->record_guard ? &owner->guard_refs : &owner->refs;
  SemanticRefEdge *edge = arena_alloc(f->arena, sizeof *edge);
  edge->to = target;
  edge->next = *chain;
  *chain = edge;
  fold_entry(f, target);

  switch (target->fold_class) {
  case SEMANTIC_FC_FOLDABLE:
    *out = target->value;
    return ok_value(*out);
  case SEMANTIC_FC_UNFOLDABLE_FP:
    fl->fp = 1;
    break;
  case SEMANTIC_FC_LAYOUT:
    fl->layout = 1;
    break;
  case SEMANTIC_FC_DEFERRED:
    fl->unknown = 1;
    break;
  case SEMANTIC_FC_SYMBOLIC:
  case SEMANTIC_FC_EXTERNAL:
    fl->symbolic = 1;
    break;
  }
  *out = deferred_value();
  return ok_value(*out);
}

// The declared type spelling of a knob/fact (the Named's tail segment).
static int knob_type_spelling(SyntaxNode *type, SemanticCValueKind *kind, Strview *spelling) {
  if (type == NULL || type->kind != SYNTAX_KIND_NAMED) return 0;
  Strview last = {0};
  for (const SyntaxNodeList *it = ((SyntaxNamed *)type)->path; it != NULL; it = it->tail)
    last = ((SyntaxIdentifier *)it->head)->value;
  *spelling = last;
  if (strview_equals(last, strview_from_cstr("bool"))) { *kind = SEMANTIC_CV_BOOL; return 1; }
  if (strview_equals(last, strview_from_cstr("String8"))) { *kind = SEMANTIC_CV_STRING; return 1; }
  static const char *const INTS[] = {"i8",  "i16", "i32", "i64", "isize",
                                     "u8",  "u16", "u32", "u64", "usize"};
  for (size_t i = 0; i < sizeof INTS / sizeof INTS[0]; i++) {
    if (strview_equals(last, strview_from_cstr(INTS[i]))) { *kind = SEMANTIC_CV_INT; return 1; }
  }
  return 0;
}

// core's own constants — `true` and `false` — are the language's values,
// provided by the toolchain rather than injected by the driver (§4.1, §12.1).
// They are the one `@intrinsic let` family with no driver-supplied value;
// shadowing declarations in other packages are ordinary lets (§10.4).
static int core_builtin_bits(const SemanticRegistryEntry *e, uint64_t *bits) {
  if (!semantic_module_is_core(e->module))
    return 0;
  if (strview_equals(e->name, STRVIEW("true"))) {
    *bits = 1;
    return 1;
  }
  if (strview_equals(e->name, STRVIEW("false"))) {
    *bits = 0;
    return 1;
  }
  return 0;
}

// Folds the injected value of a knob/fact: the manifest or driver supplies a
// plain string parsed against the declared type spelling (§12.3).
static SemanticEvalOutcome fold_extern_value(Folder *f, SemanticRegistryEntry *e,
                                             SemanticCValue *out) {
  uint64_t bits = 0;
  if (e->decl->kind == SYNTAX_KIND_LET_DECL && core_builtin_bits(e, &bits)) {
    SemanticCValueKind kind = SEMANTIC_CV_DEFERRED;
    Strview spelling;
    if (knob_type_spelling(((SyntaxLetDecl *)e->decl)->type, &kind, &spelling) &&
        kind == SEMANTIC_CV_BOOL) {
      *out = semantic_cvalue_bool(bits);
      return ok_value(*out);
    }
    // core declares them `bool`; a differently typed declaration is no
    // language constant and falls through to the driver path.
  }

  const Strview *raw = semantic_registry_knob_value(f->registry, e);
  SemanticCValueKind kind = SEMANTIC_CV_DEFERRED;
  Strview spelling;
  SyntaxNode *type_node =
      e->decl->kind == SYNTAX_KIND_LET_DECL ? ((SyntaxLetDecl *)e->decl)->type : NULL;
  if (!knob_type_spelling(type_node, &kind, &spelling) || raw == NULL) {
    diag(f, SEMANTIC_CONST_VALUE_MISSING, e->decl->span);
    return fail_status(SEMANTIC_EVAL_TYPE);
  }

  if (kind == SEMANTIC_CV_STRING) {
    *out = semantic_cvalue_string(*raw);
    return ok_value(*out);
  }

  uint64_t mag = 0;
  int negative = 0;
  size_t i = 0;
  if (raw->len > 0 && raw->data[0] == '-') {
    negative = 1;
    i = 1;
  }
  int bad = 0;
  for (; i < raw->len; i++) {
    uint8_t c = raw->data[i];
    if (c < '0' || c > '9') {
      bad = 1;
      break;
    }
    if (mag > (UINT64_MAX - (uint64_t)(c - '0')) / 10) bad = 1;
    mag = mag * 10 + (uint64_t)(c - '0');
  }

  SemanticIntType type = SEMANTIC_INT_I32;
  if (strview_equals(spelling, strview_from_cstr("i8"))) type = SEMANTIC_INT_I8;
  else if (strview_equals(spelling, strview_from_cstr("i16"))) type = SEMANTIC_INT_I16;
  else if (strview_equals(spelling, strview_from_cstr("i32"))) type = SEMANTIC_INT_I32;
  else if (strview_equals(spelling, strview_from_cstr("i64"))) type = SEMANTIC_INT_I64;
  else if (strview_equals(spelling, strview_from_cstr("isize"))) type = SEMANTIC_INT_ISIZE;
  else if (strview_equals(spelling, strview_from_cstr("u8"))) type = SEMANTIC_INT_U8;
  else if (strview_equals(spelling, strview_from_cstr("u16"))) type = SEMANTIC_INT_U16;
  else if (strview_equals(spelling, strview_from_cstr("u32"))) type = SEMANTIC_INT_U32;
  else if (strview_equals(spelling, strview_from_cstr("u64"))) type = SEMANTIC_INT_U64;
  else if (strview_equals(spelling, strview_from_cstr("usize"))) type = SEMANTIC_INT_USIZE;

  uint32_t w = semantic_int_width(type);
  int is_signed = type <= SEMANTIC_INT_ISIZE;
  uint64_t mask = w == 64 ? UINT64_MAX : ((1ull << w) - 1);
  if (kind == SEMANTIC_CV_BOOL) {
    if (mag > 1) bad = 1;
  } else if (is_signed) {
    if (negative && mag > (1ull << (w - 1))) bad = 1;
    if (!negative && mag > (1ull << (w - 1)) - 1) bad = 1;
  } else if (negative) {
    bad = 1;
  } else if (w < 64 && mag > mask) {
    bad = 1;
  }
  if (bad) {
    diag(f, SEMANTIC_CONST_VALUE_MISSING, e->decl->span);
    return fail_status(SEMANTIC_EVAL_TYPE);
  }

  if (kind == SEMANTIC_CV_BOOL) {
    *out = semantic_cvalue_bool(mag);
    return ok_value(*out);
  }
  if (negative) {
    *out = semantic_cvalue_int(type, (0 - mag) & mask);
  } else {
    *out = semantic_cvalue_int(type, mag);
  }
  return ok_value(*out);
}

static SemanticEvalOutcome fold_expr(Folder *f, SemanticRegistryEntry *owner, FoldFlags *fl,
                                     SyntaxNode *node, SemanticCValue *out) {
  switch (node->kind) {
  case SYNTAX_KIND_INT_LIT_EXPR: {
    SemanticIntLiteral lit = semantic_int_literal(((SyntaxIntLitExpr *)node)->value);
    if (lit.status == SEMANTIC_LIT_RANGE) {
      diag(f, SEMANTIC_CONST_RANGE, node->span);
      *out = deferred_value();
      return fail_status(SEMANTIC_EVAL_TYPE);
    }
    *out = semantic_cvalue_int(lit.type, lit.bits);
    return ok_value(*out);
  }
  case SYNTAX_KIND_FLOAT_LIT_EXPR:
    *out = semantic_cvalue_float(((SyntaxFloatLitExpr *)node)->value);
    return ok_value(*out);
  case SYNTAX_KIND_STRING_LIT_EXPR:
    *out =
        semantic_cvalue_string(semantic_string_content(((SyntaxStringLitExpr *)node)->value, f->arena));
    return ok_value(*out);
  case SYNTAX_KIND_RUNE_LIT_EXPR:
    *out = semantic_cvalue_rune(semantic_rune_scalar(((SyntaxRuneLitExpr *)node)->value));
    return ok_value(*out);
  case SYNTAX_KIND_NAMED: {
    SyntaxNamed *named = (SyntaxNamed *)node;
    SemanticRegistryEntry *target = resolve_name(f, owner, named);
    if (target == NULL) {
      fl->unknown = 1;
      *out = deferred_value();
      return ok_value(*out);
    }
    return fold_reference(f, owner, fl, target, out);
  }
  case SYNTAX_KIND_UNARY_EXPR: {
    SyntaxUnaryExpr *un = (SyntaxUnaryExpr *)node;
    SemanticCValue operand;
    SemanticEvalOutcome r = fold_expr(f, owner, fl, un->operand, &operand);
    if (r.status != SEMANTIC_EVAL_OK) return r;
    if (is_placeholder(&operand)) {
      if (operand.kind == SEMANTIC_CV_FLOAT) fl->fp = 1;
      if (operand.kind == SEMANTIC_CV_SYMBOLIC) fl->symbolic = 1;
      *out = operand;
      return ok_value(*out);
    }
    SemanticEvalOutcome res = semantic_cvalue_unary(un->operator, operand);
    if (res.status == SEMANTIC_EVAL_TYPE) diag(f, SEMANTIC_CONST_TYPE, node->span);
    if (res.status != SEMANTIC_EVAL_OK) return res;
    *out = res.value;
    return ok_value(*out);
  }
  case SYNTAX_KIND_BINARY_EXPR: {
    SyntaxBinaryExpr *bin = (SyntaxBinaryExpr *)node;

    if (bin->operator == SYNTAX_OPERATOR_LAND || bin->operator == SYNTAX_OPERATOR_LOR) {
      SemanticCValue lhs;
      SemanticEvalOutcome lr = fold_expr(f, owner, fl, bin->left, &lhs);
      if (lr.status != SEMANTIC_EVAL_OK) return lr;
      if (is_placeholder(&lhs)) {
        if (lhs.kind == SEMANTIC_CV_SYMBOLIC) fl->symbolic = 1;
        if (lhs.kind == SEMANTIC_CV_FLOAT) fl->fp = 1;
        *out = deferred_value();
        return ok_value(*out);
      }
      if (lhs.kind != SEMANTIC_CV_BOOL) {
        diag(f, SEMANTIC_CONST_TYPE, node->span);
        return fail_status(SEMANTIC_EVAL_TYPE);
      }
      if (bin->operator == SYNTAX_OPERATOR_LAND && lhs.bits == 0) {
        *out = semantic_cvalue_bool(0); // short-circuit: the right side never folds
        return ok_value(*out);
      }
      if (bin->operator == SYNTAX_OPERATOR_LOR && lhs.bits != 0) {
        *out = semantic_cvalue_bool(1);
        return ok_value(*out);
      }
      SemanticCValue rhs;
      SemanticEvalOutcome rr = fold_expr(f, owner, fl, bin->right, &rhs);
      if (rr.status != SEMANTIC_EVAL_OK) return rr;
      if (is_placeholder(&rhs)) {
        if (rhs.kind == SEMANTIC_CV_SYMBOLIC) fl->symbolic = 1;
        if (rhs.kind == SEMANTIC_CV_FLOAT) fl->fp = 1;
        *out = deferred_value();
        return ok_value(*out);
      }
      if (rhs.kind != SEMANTIC_CV_BOOL) {
        diag(f, SEMANTIC_CONST_TYPE, node->span);
        return fail_status(SEMANTIC_EVAL_TYPE);
      }
      *out = semantic_cvalue_bool(rhs.bits);
      return ok_value(*out);
    }

    SemanticCValue lhs;
    SemanticEvalOutcome lr = fold_expr(f, owner, fl, bin->left, &lhs);
    if (lr.status != SEMANTIC_EVAL_OK) return lr;
    SemanticCValue rhs;
    SemanticEvalOutcome rr = fold_expr(f, owner, fl, bin->right, &rhs);
    if (rr.status != SEMANTIC_EVAL_OK) return rr;

    if (is_placeholder(&lhs) || is_placeholder(&rhs)) {
      if (lhs.kind == SEMANTIC_CV_SYMBOLIC || rhs.kind == SEMANTIC_CV_SYMBOLIC)
        fl->symbolic = 1;
      if (lhs.kind == SEMANTIC_CV_FLOAT || rhs.kind == SEMANTIC_CV_FLOAT) fl->fp = 1;
      *out = deferred_value();
      return ok_value(*out);
    }

    SemanticEvalOutcome res = semantic_cvalue_binary(bin->operator, lhs, rhs);
    if (res.status == SEMANTIC_EVAL_DIV_ZERO) diag(f, SEMANTIC_CONST_DIV_ZERO, node->span);
    else if (res.status == SEMANTIC_EVAL_DIV_OVERFLOW)
      diag(f, SEMANTIC_CONST_DIV_OVERFLOW, node->span);
    else if (res.status == SEMANTIC_EVAL_SHIFT_RANGE) diag(f, SEMANTIC_CONST_SHIFT_RANGE, node->span);
    else if (res.status == SEMANTIC_EVAL_TYPE) diag(f, SEMANTIC_CONST_TYPE, node->span);
    if (res.status != SEMANTIC_EVAL_OK) return res;
    *out = res.value;
    return ok_value(*out);
  }
  case SYNTAX_KIND_STRUCT_LIT_EXPR: {
    for (SyntaxNodeList *it = ((SyntaxStructLitExpr *)node)->fields; it != NULL; it = it->tail) {
      SemanticCValue ignored;
      SemanticEvalOutcome r =
          fold_expr(f, owner, fl, ((SyntaxStructLitField *)it->head)->value, &ignored);
      if (r.status != SEMANTIC_EVAL_OK) return r;
    }
    *out = deferred_value();
    return ok_value(*out);
  }
  case SYNTAX_KIND_ARRAY_LIT_EXPR: {
    for (SyntaxNodeList *it = ((SyntaxArrayLitExpr *)node)->elements; it != NULL; it = it->tail) {
      SemanticCValue ignored;
      SemanticEvalOutcome r = fold_expr(f, owner, fl, it->head, &ignored);
      if (r.status != SEMANTIC_EVAL_OK) return r;
    }
    *out = deferred_value();
    return ok_value(*out);
  }
  case SYNTAX_KIND_COMPILE_TIME: {
    fl->layout = 1; // @sizeof family and any compile-time query: P6 resolves
    *out = deferred_value();
    return ok_value(*out);
  }
  default:
    diag(f, SEMANTIC_CONST_TYPE, node->span);
    return fail_status(SEMANTIC_EVAL_TYPE);
  }
}

static void fold_entry(Folder *f, SemanticRegistryEntry *e) {
  if (e->eval_state == SEMANTIC_EVS_DONE) return;
  if (e->eval_state == SEMANTIC_EVS_EVALUATING) {
    diag(f, SEMANTIC_CONST_CYCLE, e->decl->span);
    e->fold_class = SEMANTIC_FC_DEFERRED;
    e->eval_state = SEMANTIC_EVS_DONE;
    return;
  }

  e->eval_state = SEMANTIC_EVS_EVALUATING;

  switch (e->kind) {
  case SEMANTIC_RK_LET: {
    FoldFlags fl = {0, 0, 0, 0};
    SyntaxLetDecl *decl = (SyntaxLetDecl *)e->decl;
    SemanticCValue out = deferred_value();
    SemanticEvalOutcome r = fold_expr(f, e, &fl, decl->value, &out);
    if (r.status == SEMANTIC_EVAL_OK) e->value = out;

    if (fl.symbolic) e->fold_class = SEMANTIC_FC_SYMBOLIC;
    else if (fl.fp) e->fold_class = SEMANTIC_FC_UNFOLDABLE_FP;
    else if (fl.layout) e->fold_class = SEMANTIC_FC_LAYOUT;
    else if (fl.unknown) e->fold_class = SEMANTIC_FC_DEFERRED;
    else e->fold_class = SEMANTIC_FC_FOLDABLE;

    e->guard_usable = e->fold_class == SEMANTIC_FC_FOLDABLE && r.status == SEMANTIC_EVAL_OK &&
                      (e->value.kind == SEMANTIC_CV_BOOL || e->value.kind == SEMANTIC_CV_INT);
    break;
  }
  case SEMANTIC_RK_LET_FEATURE:
  case SEMANTIC_RK_LET_INTRINSIC: {
    SemanticCValue out;
    SemanticEvalOutcome r = fold_extern_value(f, e, &out);
    if (r.status == SEMANTIC_EVAL_OK) {
      e->value = out;
      e->fold_class = SEMANTIC_FC_FOLDABLE;
      e->guard_usable = e->value.kind == SEMANTIC_CV_BOOL || e->value.kind == SEMANTIC_CV_INT;
    } else {
      e->fold_class = SEMANTIC_FC_EXTERNAL;
    }
    break;
  }
  case SEMANTIC_RK_LET_ADDRESS:
    e->fold_class = SEMANTIC_FC_SYMBOLIC;
    break;
  case SEMANTIC_RK_LET_IMPORT:
  case SEMANTIC_RK_LET_NO_INIT:
    e->fold_class = SEMANTIC_FC_EXTERNAL;
    break;
  case SEMANTIC_RK_FUNC:
  case SEMANTIC_RK_STRUCT:
  case SEMANTIC_RK_ENUM:
  case SEMANTIC_RK_UNION:
  case SEMANTIC_RK_CONTRACT:
    e->fold_class = SEMANTIC_FC_SYMBOLIC;
    break;
  }

  e->eval_state = SEMANTIC_EVS_DONE;
}

void semantic_fold_world(SemanticRegistry *registry, Arena *arena) {
  Folder f = {.arena = arena, .registry = registry, .errors = registry->errors};
  for (SemanticRegistryEntry *e = semantic_registry_entries(registry); e != NULL; e = e->next)
    fold_entry(&f, e);
  registry->errors = f.errors;
}

SemanticEvalOutcome semantic_fold_guard(SemanticRegistry *registry, SemanticRegistryEntry *owner,
                                        SyntaxNode *guard, Arena *arena, int *not_foldable) {
  Folder f = {.arena = arena, .registry = registry, .errors = registry->errors, .record_guard = 1};
  FoldFlags fl = {0, 0, 0, 0};
  SemanticCValue out = deferred_value();
  SemanticEvalOutcome r = fold_expr(&f, owner, &fl, guard, &out);
  registry->errors = f.errors;
  *not_foldable = fl.layout || fl.symbolic || fl.unknown || fl.fp;
  return r;
}
