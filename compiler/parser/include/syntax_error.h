#pragma once

#include "arena.h"
#include "span.h"

typedef enum {
  SYNTAX_OK = 0x0000,
  SYNTAX_EXPECTED_EOF = 0x0001,
  SYNTAX_MALFORMED_NUMBER = 0x0002,
  SYNTAX_MALFORMED_RUNE = 0x0003,
  SYNTAX_MALFORMED_STRING = 0x0004,
  SYNTAX_EXPECTED_SEMICOLON = 0x0006,
  SYNTAX_EXPECTED_IDENTIFIER = 0x0007,
  SYNTAX_EXPECTED_NAME_PATH = 0x0008,
  SYNTAX_EXPECTED_EXPR = 0x0009,
  SYNTAX_EXPECTED_RPAREN = 0x000A,
} SyntaxErrorCode;

typedef struct {
  SyntaxErrorCode code;
  Span span;
} SyntaxError;

SyntaxError syntax_error_create(SyntaxErrorCode code, Span span);

#pragma region SYNTAX ERROR LIST

typedef struct SyntaxErrorList SyntaxErrorList;
struct SyntaxErrorList {
  SyntaxError error;
  SyntaxErrorList *next;
};

/**
 * @brief The empty list. Returns NULL; exists for explicit call sites.
 */
SyntaxErrorList *syntax_errorlist_empty(void);

/**
 * @brief Builds a list holding @p count array elements, preserving
 *        order. Returns NULL when @p count is zero.
 */
SyntaxErrorList *syntax_errorlist_from_array(Arena *arena,
                                             const SyntaxError *errors,
                                             size_t count);

/**
 * @brief A list with @p error followed by all of @p list. O(1); shares
 *        the whole old spine.
 */
SyntaxErrorList *syntax_errorlist_prepend(Arena *arena, SyntaxErrorList *list,
                                          SyntaxError error);

/**
 * @brief A list with all elements of @p list followed by @p error.
 *        Copies @p list's cells; the source stays valid and unchanged.
 */
SyntaxErrorList *syntax_errorlist_append(Arena *arena, SyntaxErrorList *list,
                                         SyntaxError error);

/**
 * @brief The first error. Asserts non-empty.
 */
SyntaxError syntax_errorlist_head(SyntaxErrorList *list);

/**
 * @brief Every element except the first (NULL when length is one).
 *        Asserts non-empty.
 */
SyntaxErrorList *syntax_errorlist_tail(SyntaxErrorList *list);

/**
 * @brief The error at zero-based position @p n. Asserts in range.
 */
SyntaxError syntax_errorlist_at(SyntaxErrorList *list, size_t n);

/**
 * @brief True when the list holds no errors.
 */
bool syntax_errorlist_is_empty(const SyntaxErrorList *list);

/**
 * @brief Fresh cells holding @p list's errors in reverse order; the
 *        source stays valid and unchanged.
 */
SyntaxErrorList *syntax_errorlist_reverse(Arena *arena, SyntaxErrorList *list);

/**
 * @brief All errors of @p list_a followed by all of @p list_b. Copies
 *        @p list_a's cells and shares @p list_b wholesale.
 */
SyntaxErrorList *syntax_errorlist_concat(Arena *arena, SyntaxErrorList *list_a,
                                         SyntaxErrorList *list_b);

/**
 * @brief Number of errors. O(n).
 */
size_t syntax_errorlist_length(SyntaxErrorList *list);

#pragma endregion