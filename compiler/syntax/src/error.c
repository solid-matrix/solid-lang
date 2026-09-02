/**
 * @file error.c
 * @brief SyntaxError construction & Diagnostic chain operations.
 * @author solid-matrix
 */

#include <assert.h>

#include "syntax_error.h"

SyntaxError syntax_error_create(SyntaxErrorCode code, Span span) { return (SyntaxError){.code = code, .span = span}; }

LIST_DEFINE(SyntaxErrorList, syntax_errorlist, SyntaxError)
