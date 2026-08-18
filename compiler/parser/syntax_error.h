/**
 * @file syntax_error.h
 * @brief Syntax error handling for the parser.
 * @author solid-matrix
 * @version 0.0.5
 */

#ifndef SOLID_SYNTAX_ERROR_H
#define SOLID_SYNTAX_ERROR_H

#include "source.h"
#include <stddef.h>

typedef enum
{
  SYNTAX_OK = 0x0000,
  SYNTAX_EXPECTED_EOF = 0x0001,
} SyntaxErrorCode;

typedef struct
{
  SyntaxErrorCode code;
  Span span;
} SyntaxError;

#endif /* SOLID_SYNTAX_ERROR_H */
