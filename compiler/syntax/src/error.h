#pragma once

#include "arena.h"
#include "syntax_error.h"
/**
 * @brief Makes a diagnostic value.
 * @param code The diagnostic code.
 * @param span The source text the diagnostic refers to.
 * @return The diagnostic value.
 */
SyntaxError syntax_error_create(SyntaxErrorCode code, Span span);

/**
 * @brief The empty list. Returns NULL; exists for explicit call sites.
 * @return NULL.
 */
SyntaxErrorList *syntax_errorlist_empty(void);

/**
 * @brief A list holding @p count array elements, preserving order.
 * @param arena Backs the new cells.
 * @param errors The elements, in head-to-tail order.
 * @param count Number of elements; zero yields NULL.
 * @return The new list.
 */
SyntaxErrorList *syntax_errorlist_from_array(Arena *arena, const SyntaxError *errors, size_t count);

/**
 * @brief A list with @p error followed by all of @p list.
 * @param arena Backs the new cell.
 * @param list The tail; shared wholesale.
 * @param error The new head error.
 * @return The extended list. O(1).
 */
SyntaxErrorList *syntax_errorlist_prepend(Arena *arena, SyntaxErrorList *list, SyntaxError error);

/**
 * @brief A list with all elements of @p list followed by @p error.
 * @param arena Backs the copied cells.
 * @param list The prefix; stays valid and unchanged.
 * @param error The new last error.
 * @return The extended list.
 */
SyntaxErrorList *syntax_errorlist_append(Arena *arena, SyntaxErrorList *list, SyntaxError error);

/**
 * @brief The first error.
 * @param list A non-empty list.
 * @return The head error. Asserts non-empty.
 */
SyntaxError syntax_errorlist_head(SyntaxErrorList *list);

/**
 * @brief Every element except the first.
 * @param list A non-empty list.
 * @return The list without its head; NULL when the length is one.
 *         Asserts non-empty.
 */
SyntaxErrorList *syntax_errorlist_tail(SyntaxErrorList *list);

/**
 * @brief The error at zero-based position @p n.
 * @param list The list to index.
 * @param n Zero-based position.
 * @return The error. Asserts in range.
 */
SyntaxError syntax_errorlist_at(SyntaxErrorList *list, size_t n);

/**
 * @brief True when the list holds no errors.
 * @param list The list to test.
 * @return True for NULL or an empty list.
 */
bool syntax_errorlist_is_empty(const SyntaxErrorList *list);

/**
 * @brief Fresh cells holding @p list's errors in reverse order.
 * @param arena Backs the new cells.
 * @param list The source; stays valid and unchanged.
 * @return The reversed list.
 */
SyntaxErrorList *syntax_errorlist_reverse(Arena *arena, SyntaxErrorList *list);

/**
 * @brief All errors of @p list_a followed by all of @p list_b.
 * @param arena Backs the copied cells.
 * @param list_a The prefix; copied, stays valid and unchanged.
 * @param list_b The suffix; shared wholesale.
 * @return The joined list.
 */
SyntaxErrorList *syntax_errorlist_concat(Arena *arena, SyntaxErrorList *list_a, SyntaxErrorList *list_b);

/**
 * @brief Number of errors.
 * @param list The list to measure.
 * @return The length. O(n).
 */
size_t syntax_errorlist_length(SyntaxErrorList *list);