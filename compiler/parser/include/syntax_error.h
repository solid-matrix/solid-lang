#pragma once

#include <stdbool.h>

#include "span.h"

typedef enum {
  SYNTAX_OK = 0x0000,
  SYNTAX_EXPECTED_EOF = 0x0001,
  SYNTAX_MALFORMED_NUMBER = 0x0002,
} SyntaxErrorCode;

typedef struct {
  SyntaxErrorCode code;
  Span span;
} SyntaxError;

SyntaxError syntax_error_create(SyntaxErrorCode code, Span span);

/**
 * @brief One diagnostic in a SyntaxErrorList.
 */
typedef struct SyntaxErrorListNode SyntaxErrorListNode;
struct SyntaxErrorListNode {
  SyntaxError error;
  SyntaxErrorListNode *next;
};

/**
 * @brief A heap-allocated handle owning a chain of error nodes.
 *
 * The stable address of the handle is what lets append/merge/destroy
 * take a single pointer: they mutate the chain through it. An empty
 * list has head == NULL; the handle itself is never NULL while alive.
 */
typedef struct {
  SyntaxErrorListNode *head; // first node, or NULL when empty
} SyntaxErrorList;

/**
 * @brief Allocates an empty list.
 * @return The new list, owned by the caller; released exactly once
 *         with syntax_errorlist_destroy().
 */
SyntaxErrorList *syntax_errorlist_create(void);

/**
 * @brief Tests whether the list holds no nodes.
 * @param list The list to test.
 * @return True when head == NULL.
 */
bool syntax_errorlist_is_empty(const SyntaxErrorList *list);

/**
 * @brief Appends @p error as the last node.
 * @param list The list to extend.
 * @param error The diagnostic to append.
 */
void syntax_errorlist_append(SyntaxErrorList *list, SyntaxError error);

/**
 * @brief Moves every node of @p src to the end of @p dst.
 *
 * Only ownership of the NODES moves: @p src is left as an empty list
 * and its handle stays valid; release it separately with
 * syntax_errorlist_destroy().
 * @param dst The list that receives the nodes.
 * @param src The list whose nodes are moved away; must differ from dst.
 */
void syntax_errorlist_merge(SyntaxErrorList *dst, SyntaxErrorList *src);

/**
 * @brief Frees every node, then the handle itself.
 * @param list The list to destroy; NULL is allowed and is a no-op, so
 *             callers may release lists unconditionally.
 */
void syntax_errorlist_destroy(SyntaxErrorList *list);
