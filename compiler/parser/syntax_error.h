/**
 * @file syntax_error.h
 * @brief Syntax error handling for the parser.
 * @author solid-matrix
 * @version 0.0.5
 */

#pragma once

#include <stddef.h>

#include "source.h"

typedef enum {
  SYNTAX_OK = 0x0000,
  SYNTAX_EXPECTED_EOF = 0x0001,
  SYNTAX_MALFORMED_NUMBER = 0x0002,
} SyntaxErrorCode;

typedef struct {
  SyntaxErrorCode code;
  Span span;
} SyntaxError;
