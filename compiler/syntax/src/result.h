#pragma once

#include "syntax_error.h"
#include "syntax_node.h"

/**
 * @brief Outcome of parsing one construct; the contract of every
 *        parse_XXX(const SyntaxParser *, Span span).
 * @details Every parse function never consumes trivia itself:
 *          span.start sits on a non-trivia byte (or the span is
 *          empty), and on success rem points just past the consumed
 *          text while trailing trivia stays with the enclosing
 *          sequence.
 *
 *          matched == false: the construct does not start at span.
 *          Nothing is consumed and nothing is recorded — node == NULL,
 *          errors == NULL, rem == span — so the caller may backtrack.
 *
 *          matched == true: the construct was recognized and consumed.
 *          node is the AST node, or NULL for a dropped recovery frame;
 *          errors is NULL when there are no diagnostics, otherwise the
 *          newest diagnostic is at the head of the chain.
 */
typedef struct {
  bool matched;
  Span rem;
  SyntaxNode *node;
  SyntaxErrorList *errors;
} SyntaxNodeResult;

/**
 * @brief Outcome of a list helper: the parsed node chain, the consumed
 *        position and any diagnostics.
 */
typedef struct {
  Span rem;
  SyntaxNodeList *list;
  SyntaxErrorList *errors;
} SyntaxListResult;

/**
 * @brief Outcome of a byte-level match attempt: matched, the position
 *        just past the matched bytes, and any diagnostics.
 */
typedef struct {
  bool matched;
  Span rem;
  SyntaxErrorList *errors;
} SyntaxMatchResult;

/**
 * @brief The not-matched outcome for @p span: nothing consumed,
 *        nothing recorded.
 * @param span The span that was tested.
 * @return The not-matched outcome (rem == span).
 */
SyntaxNodeResult syntax_node_result_not_match(Span span);

/**
 * @brief The matched outcome carrying @p rem, @p node and @p errors.
 * @param rem Position just past the consumed text.
 * @param node The parsed node, or NULL for a recovery frame.
 * @param errors Diagnostics, newest at head; NULL when silent.
 * @return The matched outcome.
 */
SyntaxNodeResult syntax_node_result_matched(Span rem, SyntaxNode *node, SyntaxErrorList *errors);

/**
 * @brief True when the attempt succeeded and produced no diagnostics.
 * @param result The outcome to test.
 * @return True for matched == true with errors == NULL.
 */
bool syntax_node_result_is_ok(SyntaxNodeResult result);