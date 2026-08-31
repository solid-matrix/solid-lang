/**
 * @file syntax_error.h
 * @brief Diagnostic codes, values and the diagnostic chain type.
 * @author solid-matrix
 * @version 0.0.5
 */

#pragma once

#include "span.h"

/**
 * @brief Diagnostic codes reported by the parser.
 */
typedef enum {
  SYNTAX_OK = 0x0000,
  SYNTAX_EXPECTED_EOF = 0x0001,
  SYNTAX_EXPECTED_SEMICOLON = 0x0006,
  SYNTAX_EXPECTED_IDENTIFIER = 0x0007,
  SYNTAX_EXPECTED_EXPR = 0x0009,
  SYNTAX_EXPECTED_RPAREN = 0x000A,
  SYNTAX_EXPECTED_RBRACKET = 0x000B,
  SYNTAX_EXPECTED_TYPE = 0x000D,
  SYNTAX_EXPECTED_RBRACE = 0x000E,
  SYNTAX_EXPECTED_EQUALS = 0x000F,
  SYNTAX_EXPECTED_BODY = 0x0010,
  SYNTAX_EXPECTED_DECL_BODY = 0x0011,
  SYNTAX_EXPECTED_COLON = 0x0012,
  SYNTAX_EXPECTED_GT = 0x0013,
  SYNTAX_EXPECTED_LPAREN = 0x0014,
  SYNTAX_EXPECTED_CHARACTER = 0x0015,
  SYNTAX_EXPECTED_SINGLE_QUOTE = 0x0016,

  SYNTAX_INVALID_CHARACTER = 0x0017,
  SYNTAX_INVALID_ESCAPE = 0x0018,
  SYNTAX_EXPECTED_HEX_DIGIT = 0x0019,
  SYNTAX_EXPECTED_BRACE = 0x001A,
  SYNTAX_ESCAPE_OUT_OF_RANGE = 0x001B,
  SYNTAX_EXPECTED_DOUBLE_QUOTE = 0x001C,

  SYNTAX_INVALID_LEADING_ZERO = 0x001E,
  SYNTAX_EXPECTED_SUFFIX = 0x001F,
  SYNTAX_EXPECTED_DIGIT = 0x0020,

  SYNTAX_MISPLACED_NAMESPACE = 0x0021,
  SYNTAX_MISPLACED_USING = 0x0022,
} SyntaxErrorCode;

/**
 * @brief One diagnostic: a code anchored to a source span.
 */
typedef struct {
  SyntaxErrorCode code;
  Span span;
} SyntaxError;

/**
 * @brief A chain cell. Lists are persistent: sharing cells is safe
 *        because they are never mutated after allocation.
 */
typedef struct SyntaxErrorList SyntaxErrorList;
struct SyntaxErrorList {
  SyntaxError error;
  SyntaxErrorList *next;
};
