/**
 * @file semantic_literal.h
 * @brief Literal-token classification and decoding for compile-time consumers.
 * @author solid-matrix
 *
 * The parser stores literal tokens as raw source text. Compile-time stages
 * (the constant world, guard evaluation) consume typed values; this header
 * turns well-formed literal token text into those values. Precondition: the
 * token was produced by the parser and is well-formed as a literal.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "arena.h"
#include "strview.h"

/**
 * @brief The type an integer literal's suffix denotes; unsuffixed literals
 *        default to I32 (spec §4.4).
 */
typedef enum {
  SEMANTIC_INT_I8,
  SEMANTIC_INT_I16,
  SEMANTIC_INT_I32,
  SEMANTIC_INT_I64,
  SEMANTIC_INT_ISIZE,
  SEMANTIC_INT_U8,
  SEMANTIC_INT_U16,
  SEMANTIC_INT_U32,
  SEMANTIC_INT_U64,
  SEMANTIC_INT_USIZE,
} SemanticIntType;

/**
 * @brief Literal evaluation status. RANGE means the magnitude exceeds the
 *        type's range as written; a leading minus is applied through
 *        semantic_int_negate, which re-checks the negated form.
 */
typedef enum {
  SEMANTIC_LIT_OK = 0,
  SEMANTIC_LIT_RANGE,
} SemanticLiteralStatus;

/**
 * @brief An evaluated integer literal. Signed types store the positive
 *        magnitude in @p bits; apply semantic_int_negate for a leading minus.
 *        Unsigned types store the value directly.
 */
typedef struct {
  SemanticLiteralStatus status;
  SemanticIntType type;
  uint64_t bits;
  bool suffixed; ///< the token carried an explicit type suffix (§4.4)
} SemanticIntLiteral;

/**
 * @brief Parses an integer literal token — decimal/hex/octal/binary digits,
 *        separators and suffix included — into type and magnitude.
 */
SemanticIntLiteral semantic_int_literal(Strview text);

/**
 * @brief Two's-complement negation in the type's width. Signed: fits iff the
 *        magnitude is at most 2^(width-1); unsigned: wraps (0 negates to 0).
 */
SemanticIntLiteral semantic_int_negate(SemanticIntLiteral literal);

/**
 * @brief The type's width in bits. ISIZE/USIZE follow the compilation
 *        target's pointer width (64 in the current host configuration).
 */
uint32_t semantic_int_width(SemanticIntType type);

/**
 * @brief Decodes a string literal token (quotes stripped, escapes resolved)
 *        into bytes backed by @p arena. Raw text length is an upper bound of
 *        the decoded length.
 */
Strview semantic_string_content(Strview text, Arena *arena);

/**
 * @brief Decodes a rune literal token into its Unicode scalar value.
 */
uint32_t semantic_rune_scalar(Strview text);
