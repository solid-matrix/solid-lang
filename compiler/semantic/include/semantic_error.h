#pragma once

#include "span.h"

/**
 * @brief Diagnostic codes reported by the semantic analyzer.
 * @details Codes are grouped per pass: 0x01xx collect, 0x02xx resolve,
 *          0x03xx check.
 */
typedef enum {
  SEMANTIC_OK = 0x0000,

  // collect
  SEMANTIC_SYMBOL_REDEFINED = 0x0101,
  SEMANTIC_SYMBOL_NAMESPACE_CLASH = 0x0102,
} SemanticErrorCode;

/**
 * @brief One diagnostic: a code anchored to a source span.
 */
typedef struct {
  SemanticErrorCode code;
  Span span;
} SemanticError;

/**
 * @brief A chain cell. Lists are persistent: sharing cells is safe
 *        because they are never mutated after allocation.
 */
typedef struct SemanticErrorList SemanticErrorList;
struct SemanticErrorList {
  SemanticError error;
  SemanticErrorList *next;
};
