/**
 * @file error.c
 * @brief SyntaxError construction.
 * @author solid-matrix
 * @version 0.0.5
 */

#include "syntax_error.h"

SyntaxError syntax_error_create(SyntaxErrorCode code, Span span) { return (SyntaxError){.code = code, .span = span}; }
