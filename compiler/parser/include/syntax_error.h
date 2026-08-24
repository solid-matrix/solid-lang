#pragma once

#include "span.h"

typedef enum {
  SYNTAX_OK = 0x0000,
  SYNTAX_EXPECTED_EOF = 0x0001,
  SYNTAX_MALFORMED_NUMBER = 0x0002,
  SYNTAX_MALFORMED_RUNE = 0x0003,
  SYNTAX_MALFORMED_STRING = 0x0004,
  SYNTAX_MALFORMED_NAMEPATH = 0x0005,
  SYNTAX_EXPECTED_SEMICOLON = 0x0006,
} SyntaxErrorCode;

typedef struct {
  SyntaxErrorCode code;
  Span span;
} SyntaxError;

SyntaxError syntax_error_create(SyntaxErrorCode code, Span span);

/**
 * @brief One diagnostic; a chain of these nodes IS the error list.
 *
 * The list is represented by its head pointer, so "no errors" is
 * simply NULL and an empty list costs nothing.
 */
typedef struct SyntaxErrorList SyntaxErrorList;
struct SyntaxErrorList {
  SyntaxError error;
  SyntaxErrorList *next;
};

/**
 * @brief Appends @p error as the last node.
 * @param list The list slot to extend; may point at NULL (the empty
 *             list), which this call fills.
 */
void syntax_errorlist_append(SyntaxErrorList **list, SyntaxError error);

/**
 * @brief Moves every node of @p src to the end of @p dst.
 *
 * Ownership of the nodes moves: *src is set to NULL, so the source
 * slot is left as the empty list and cannot be double-released.
 * @param dst The list that receives the nodes.
 * @param src The list whose nodes are moved away; must differ from dst.
 */
void syntax_errorlist_merge(SyntaxErrorList **dst, SyntaxErrorList **src);

/**
 * @brief Frees every node and clears the slot.
 * @param list The list slot to destroy; NULL is allowed and only
 *             normalizes the slot, matching free()-style tolerance.
 */
void syntax_errorlist_destroy(SyntaxErrorList **list);
