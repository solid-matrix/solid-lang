/**
 * @file error.c
 * @brief SemanticError construction.
 * @author solid-matrix
 */

#include <assert.h>

#include "semantic_error.h"

SemanticError semantic_error_create(SemanticErrorCode code, Span span) {
  return (SemanticError){.code = code, .span = span};
}

LIST_DEFINE(SemanticErrorList, semantic_errorlist, SemanticError)
