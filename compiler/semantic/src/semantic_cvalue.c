/**
 * @file semantic_cvalue.c
 * @brief Constant values and the foldable operator evaluator.
 * @author solid-matrix
 */

#include "semantic_cvalue.h"

#include <string.h>

SemanticCValue semantic_cvalue_int(SemanticIntType type, uint64_t bits) {
  return (SemanticCValue){.kind = SEMANTIC_CV_INT, .int_type = type, .bits = bits};
}

SemanticCValue semantic_cvalue_bool(uint64_t bits) {
  return (SemanticCValue){.kind = SEMANTIC_CV_BOOL, .bits = bits != 0};
}

SemanticCValue semantic_cvalue_string(Strview bytes) {
  return (SemanticCValue){.kind = SEMANTIC_CV_STRING, .text = bytes};
}

SemanticCValue semantic_cvalue_rune(uint64_t scalar) {
  return (SemanticCValue){.kind = SEMANTIC_CV_RUNE, .bits = scalar};
}

SemanticCValue semantic_cvalue_float(Strview raw) { return (SemanticCValue){.kind = SEMANTIC_CV_FLOAT, .text = raw}; }

SemanticCValue semantic_cvalue_deferred(void) { return (SemanticCValue){.kind = SEMANTIC_CV_DEFERRED}; }

static SemanticEvalOutcome ok(SemanticCValue v) {
  return (SemanticEvalOutcome){.status = SEMANTIC_EVAL_OK, .value = v};
}

static SemanticEvalOutcome fail(SemanticEvalStatus s) { return (SemanticEvalOutcome){.status = s}; }

static uint32_t width_of(const SemanticCValue *v) { return semantic_int_width(v->int_type); }

static uint64_t mask_of(uint32_t w) { return w >= 64 ? UINT64_MAX : ((1ull << w) - 1); }

static int64_t signed_of(uint64_t bits, uint32_t w) { return (int64_t)(bits << (64 - w)) >> (64 - w); }

static int both_int(const SemanticCValue *l, const SemanticCValue *r) {
  return l->kind == SEMANTIC_CV_INT && r->kind == SEMANTIC_CV_INT && l->int_type == r->int_type;
}

static SemanticEvalOutcome eval_int_cmp(SyntaxOperator op, const SemanticCValue *l, const SemanticCValue *r) {
  uint32_t w = width_of(l);
  int is_signed = l->int_type <= SEMANTIC_INT_ISIZE;
  int result;
  if (is_signed) {
    int64_t a = signed_of(l->bits, w);
    int64_t b = signed_of(r->bits, w);
    result = op == SYNTAX_OPERATOR_EQ    ? a == b
             : op == SYNTAX_OPERATOR_NEQ ? a != b
             : op == SYNTAX_OPERATOR_LT  ? a < b
             : op == SYNTAX_OPERATOR_GT  ? a > b
             : op == SYNTAX_OPERATOR_LTE ? a <= b
                                         : a >= b;
  } else {
    uint64_t a = l->bits;
    uint64_t b = r->bits;
    result = op == SYNTAX_OPERATOR_EQ    ? a == b
             : op == SYNTAX_OPERATOR_NEQ ? a != b
             : op == SYNTAX_OPERATOR_LT  ? a < b
             : op == SYNTAX_OPERATOR_GT  ? a > b
             : op == SYNTAX_OPERATOR_LTE ? a <= b
                                         : a >= b;
  }
  return ok(semantic_cvalue_bool((uint64_t)result));
}

static SemanticEvalOutcome eval_int_binary(SyntaxOperator op, const SemanticCValue *l, const SemanticCValue *r) {
  if (!both_int(l, r))
    return fail(SEMANTIC_EVAL_TYPE);

  uint32_t w = width_of(l);
  uint64_t mask = mask_of(w);
  int is_signed = l->int_type <= SEMANTIC_INT_ISIZE;

  switch (op) {
  case SYNTAX_OPERATOR_EQ:
  case SYNTAX_OPERATOR_NEQ:
    return eval_int_cmp(op, l, r);
  case SYNTAX_OPERATOR_ADD:
    return ok(semantic_cvalue_int(l->int_type, (l->bits + r->bits) & mask));
  case SYNTAX_OPERATOR_SUB:
    return ok(semantic_cvalue_int(l->int_type, (l->bits - r->bits) & mask));
  case SYNTAX_OPERATOR_MUL:
    return ok(semantic_cvalue_int(l->int_type, (l->bits * r->bits) & mask));
  case SYNTAX_OPERATOR_DIV:
  case SYNTAX_OPERATOR_MOD: {
    if (r->bits == 0)
      return fail(SEMANTIC_EVAL_DIV_ZERO);
    if (is_signed) {
      int64_t a = signed_of(l->bits, w);
      int64_t b = signed_of(r->bits, w);
      int64_t min = w == 64 ? INT64_MIN : -(1ll << (w - 1));
      if (a == min && b == -1)
        return fail(SEMANTIC_EVAL_DIV_OVERFLOW);
      int64_t q = a / b;
      int64_t m = a % b;
      return ok(semantic_cvalue_int(l->int_type, (uint64_t)(op == SYNTAX_OPERATOR_DIV ? q : m) & mask));
    }
    return ok(semantic_cvalue_int(l->int_type, op == SYNTAX_OPERATOR_DIV ? l->bits / r->bits : l->bits % r->bits));
  }
  case SYNTAX_OPERATOR_SHL:
  case SYNTAX_OPERATOR_SHR: {
    if (r->bits >= w)
      return fail(SEMANTIC_EVAL_SHIFT_RANGE);
    if (op == SYNTAX_OPERATOR_SHL)
      return ok(semantic_cvalue_int(l->int_type, (l->bits << r->bits) & mask));
    if (!is_signed)
      return ok(semantic_cvalue_int(l->int_type, l->bits >> r->bits));
    return ok(semantic_cvalue_int(l->int_type, (uint64_t)(signed_of(l->bits, w) >> r->bits) & mask));
  }
  case SYNTAX_OPERATOR_BAND:
    return ok(semantic_cvalue_int(l->int_type, l->bits & r->bits));
  case SYNTAX_OPERATOR_BOR:
    return ok(semantic_cvalue_int(l->int_type, l->bits | r->bits));
  case SYNTAX_OPERATOR_BXOR:
    return ok(semantic_cvalue_int(l->int_type, l->bits ^ r->bits));
  case SYNTAX_OPERATOR_LT:
  case SYNTAX_OPERATOR_GT:
  case SYNTAX_OPERATOR_LTE:
  case SYNTAX_OPERATOR_GTE:
    return eval_int_cmp(op, l, r);
  default:
    return fail(SEMANTIC_EVAL_TYPE);
  }
}

static SemanticEvalOutcome eval_eq_neq(SyntaxOperator op, const SemanticCValue *l, const SemanticCValue *r) {
  int equal;
  if (l->kind == SEMANTIC_CV_STRING)
    equal = l->text.len == r->text.len && memcmp(l->text.data, r->text.data, l->text.len) == 0;
  else
    equal = l->bits == r->bits;
  return ok(semantic_cvalue_bool((uint64_t)(op == SYNTAX_OPERATOR_EQ ? equal : !equal)));
}

SemanticEvalOutcome semantic_cvalue_binary(SyntaxOperator op, SemanticCValue l, SemanticCValue r) {
  switch (op) {
  case SYNTAX_OPERATOR_LAND:
  case SYNTAX_OPERATOR_LOR:
    if (l.kind != SEMANTIC_CV_BOOL || r.kind != SEMANTIC_CV_BOOL)
      return fail(SEMANTIC_EVAL_TYPE);
    return ok(semantic_cvalue_bool(op == SYNTAX_OPERATOR_LAND ? l.bits & r.bits : l.bits | r.bits));
  case SYNTAX_OPERATOR_EQ:
  case SYNTAX_OPERATOR_NEQ:
    if (l.kind != r.kind)
      return fail(SEMANTIC_EVAL_TYPE);
    if (l.kind == SEMANTIC_CV_INT && l.int_type != r.int_type)
      return fail(SEMANTIC_EVAL_TYPE); // strictly same integer type (§10.1)
    return eval_eq_neq(op, &l, &r);
  default:
    break;
  }

  if (l.kind == SEMANTIC_CV_INT && r.kind == SEMANTIC_CV_INT)
    return eval_int_binary(op, &l, &r);
  return fail(SEMANTIC_EVAL_TYPE);
}

SemanticEvalOutcome semantic_cvalue_unary(SyntaxOperator op, SemanticCValue v) {
  switch (op) {
  case SYNTAX_OPERATOR_LNOT:
    if (v.kind != SEMANTIC_CV_BOOL)
      return fail(SEMANTIC_EVAL_TYPE);
    return ok(semantic_cvalue_bool(!v.bits));
  case SYNTAX_OPERATOR_MINUS:
    if (v.kind != SEMANTIC_CV_INT)
      return fail(SEMANTIC_EVAL_TYPE);
    return ok(semantic_cvalue_int(v.int_type, (0 - v.bits) & mask_of(width_of(&v))));
  case SYNTAX_OPERATOR_PLUS:
    if (v.kind != SEMANTIC_CV_INT)
      return fail(SEMANTIC_EVAL_TYPE);
    return ok(v);
  case SYNTAX_OPERATOR_BNOT:
    if (v.kind != SEMANTIC_CV_INT)
      return fail(SEMANTIC_EVAL_TYPE);
    return ok(semantic_cvalue_int(v.int_type, ~v.bits & mask_of(width_of(&v))));
  default:
    return fail(SEMANTIC_EVAL_TYPE);
  }
}
