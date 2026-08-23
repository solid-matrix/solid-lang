/**
 * @file parser.h
 * @brief Parser: token stream -> AST.
 * @author solid-matrix
 * @version 0.0.5
 */

#pragma once

#include <stddef.h>

#include "ast.h"
#include "source.h"

/**
 * @brief Immutable scanning context.
 *
 * Parse functions receive it by const pointer and never mutate it: the
 * source is read-only through the const member, and any future shared
 * state (string interning, recursion-depth limits) would live here
 * without changing call sites.
 */
typedef struct {
  const Source *source;
} Parser;

/**
 * @brief Creates a heap-allocated Parser over @p source.
 * @param source The text to parse; the Source must outlive the Parser.
 * @return The new Parser, owned by the caller; released exactly once
 *         with parser_destroy().
 */
Parser *parser_create(const Source *source);

/**
 * @brief Frees a Parser created by parser_create().
 * @param parser The Parser to destroy; must come from parser_create()
 *               and be destroyed exactly once. The Source is not
 *               touched: it is not owned by the Parser.
 */
void parser_destroy(Parser *parser);

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

typedef struct SyntaxErrorList SyntaxErrorList;

struct SyntaxErrorList {
  SyntaxError error;
  SyntaxErrorList *next;
};

SyntaxErrorList *syntax_error_list_create();

void syntax_error_list_append(SyntaxErrorList **list, SyntaxError error);

void syntax_error_list_merge(SyntaxErrorList **dst, SyntaxErrorList **src);

void syntax_error_list_free(SyntaxErrorList **list);

/**
 * @struct ParserResult
 * @brief Outcome of parsing one construct (see per-function contracts).
 *
 * Layout discipline (the parser is scannerless, so trivia is handled
 * explicitly at composition points):
 *
 *   - Precondition of every parse_XXX(const Parser *, Span): \p span.start
 *     sits on a non-trivia byte, or the span is empty. Leading trivia
 *     is consumed only by parse_program, at the start of the unit and
 *     before every top-level declaration.
 *   - Postcondition on success: \p rem points just past the consumed
 *     text. Trailing whitespace and comments are NOT stripped -> they
 *     belong to the enclosing sequence, which skips them between two
 *     juxtaposed elements (and re-tests separators or terminators on
 *     the skipped position). Consequences: rem.start - span.start is
 *     exactly the consumed length, longest-match selection is a pure
 *     length comparison, and a kept node's span satisfies
 *     node->span.end == rem.start.
 *   - Alternation needs no layout work: all branches start at the same
 *     span.
 *   - Adjacency-sensitive checks (literal suffixes, multi-character
 *     operators) must inspect raw bytes BEFORE any trivia skip.
 *   - A parse_XXX function must never consume trivia internally; doing
 *     so silently breaks the invariants above.
 *
 * Every parse_XXX(const Parser *, Span) follows the same contract:
 *
 *   matched == false -> the construct does not start at \p span. Nothing
 *   is consumed and nothing is recorded:
 *       node == NULL, errors == NULL, rem == span.
 *   rem is returned exactly as received: the position was tested and
 *   rejected, so it is meaningless to strip anything here (that would
 *   make keyword/position checks accept input that does not literally
 *   start at \p span, and would corrupt backtracking).
 *   The caller may backtrack and try another alternative, or treat this
 *   as the end of the enclosing construct.
 *
 *   matched == true -> the construct was recognized and its input was
 *   consumed (see the postconditions above).
 *       node != NULL -> a concrete AST node to keep (e.g. append it to
 *                       the enclosing node list);
 *       node == NULL -> nothing worth keeping (e.g. a dropped
 *                       construct); diagnostics, if any, still travel
 *                       in errors.
 *
 *   errors carries every SyntaxError produced while recognizing this
 *   construct, directly or by nested constructs. The list is owned by
 *   the result: a combinator must either merge it upward into its own
 *   result (sequential acceptance) or free it together with the losing
 *   branch (longest-match selection adopts the winner's list and
 *   releases the losers' -> winner-takes-errors). The top-level caller
 *   disposes of the final list with parser_result_free_errors().
 */
typedef struct {
  bool matched;
  Span rem;
  SyntaxNode *node;
  SyntaxErrorList *errors;
} ParserResult;

ParserResult parser_result_not_match(Span span);

ParserResult parser_result_matched(Span rem, SyntaxNode *node,
                                   SyntaxErrorList *errors);
