/**
 * @file semantic_cvalue.h
 * @brief Constant values and the foldable operator evaluator (spec §10.1).
 * @author solid-matrix
 *
 * The evaluator implements the closed foldable operator set: strictly
 * same-type integer arithmetic/bitwise/comparison with wrap and panic-class
 * diagnostics, `bool` logic (short-circuit is the caller's decision — a
 * short-circuited operand is simply never passed here), and `String8`/`Rune`
 * equality. Floating-point and deferred (layout-dependent) values are never
 * folded: they reach this module only as raw/deferred payloads and every
 * operator over them evaluates to SEMANTIC_EVAL_TYPE.
 */

#pragma once

#include <stdint.h>

#include "semantic_literal.h"
#include "strview.h"
#include "syntax_node.h"

/**
 * @brief The kind of a compile-time value.
 */
typedef enum {
  SEMANTIC_CV_BOOL,     ///< @p bits carries 0 or 1
  SEMANTIC_CV_INT,      ///< @p int_type + @p bits (two's complement in width)
  SEMANTIC_CV_FLOAT,    ///< unevaluated: @p text holds the raw literal expression
  SEMANTIC_CV_STRING,   ///< @p text holds the decoded bytes
  SEMANTIC_CV_RUNE,     ///< @p bits carries the scalar value
  SEMANTIC_CV_DEFERRED, ///< layout-dependent; resolved at P6
} SemanticCValueKind;

/**
 * @brief A folded compile-time value.
 */
typedef struct {
  SemanticCValueKind kind;
  SemanticIntType int_type; // SEMANTIC_CV_INT
  uint64_t bits;            // int payload / bool 0|1 / rune scalar
  Strview text;             // float raw expression / string decoded bytes
} SemanticCValue;

/**
 * @brief Evaluation outcome.
 */
typedef enum {
  SEMANTIC_EVAL_OK = 0,
  SEMANTIC_EVAL_DIV_ZERO,     ///< integer division/remainder by zero
  SEMANTIC_EVAL_DIV_OVERFLOW, ///< signed `INT_MIN / -1`
  SEMANTIC_EVAL_SHIFT_RANGE,  ///< shift count ≥ bit width
  SEMANTIC_EVAL_TYPE,         ///< operand types do not support the operator
} SemanticEvalStatus;

typedef struct {
  SemanticEvalStatus status;
  SemanticCValue value; // valid when status == SEMANTIC_EVAL_OK
} SemanticEvalOutcome;

SemanticCValue semantic_cvalue_int(SemanticIntType type, uint64_t bits);
SemanticCValue semantic_cvalue_bool(uint64_t bits);
SemanticCValue semantic_cvalue_string(Strview bytes);
SemanticCValue semantic_cvalue_rune(uint64_t scalar);
SemanticCValue semantic_cvalue_float(Strview raw);
SemanticCValue semantic_cvalue_deferred(void);

/**
 * @brief Folds a unary operator. `SYNTAX_OPERATOR_LNOT` accepts `bool` only.
 */
SemanticEvalOutcome semantic_cvalue_unary(SyntaxOperator op, SemanticCValue v);

/**
 * @brief Folds a binary operator. `SYNTAX_OPERATOR_LAND`/`LOR` accept `bool`
 *        only and do not short-circuit here (the caller skips a
 *        short-circuited operand before ever folding it).
 */
SemanticEvalOutcome semantic_cvalue_binary(SyntaxOperator op, SemanticCValue l, SemanticCValue r);
