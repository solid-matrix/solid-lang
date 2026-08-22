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
#include "syntax_error.h"

typedef struct SyntaxErrorLinkedList SyntaxErrorLinkedList;
struct SyntaxErrorLinkedList {
  SyntaxError error;
  SyntaxErrorLinkedList *next;
};

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

Parser parser_create(const Source *source);

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
 *     text. Trailing whitespace and comments are NOT stripped â€?they
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
 *   matched == false â€?the construct does not start at \p span. Nothing
 *   is consumed and nothing is recorded:
 *       node == NULL, errors == NULL, rem == span.
 *   rem is returned exactly as received: the position was tested and
 *   rejected, so it is meaningless to strip anything here (that would
 *   make keyword/position checks accept input that does not literally
 *   start at \p span, and would corrupt backtracking).
 *   The caller may backtrack and try another alternative, or treat this
 *   as the end of the enclosing construct.
 *
 *   matched == true â€?the construct was recognized and its input was
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
 *   releases the losers' â€?winner-takes-errors). The top-level caller
 *   disposes of the final list with parser_result_free_errors().
 */
typedef struct {
  bool matched;
  SyntaxErrorLinkedList *errors;
  Span rem;
  SyntaxNode *node;
} ParserResult;

/**
 * @brief Prepends one diagnostic to @p result's owned error list.
 */
void parser_result_push_error(ParserResult *result, Span span,
                              SyntaxErrorCode code);

/**
 * @brief Moves all nodes of @p src's error list onto @p dst's.
 * @param dst The adopting result.
 * @param src The donor result; its list is left empty.
 */
void parser_result_merge_errors(ParserResult *dst, ParserResult *src);

/**
 * @brief Releases every node in @p result's error list and empties it.
 */
void parser_result_free_errors(ParserResult *result);

/**
 * @brief Top-level entry: parses a whole translation unit.
 *
 * The only function that consumes leading trivia: once at the start of
 * the unit, and before every top-level declaration. Trailing trivia is
 * skipped before the final SYNTAX_EXPECTED_EOF check; any unconsumed
 * input is reported from there. The returned result owns both the
 * program node and the error list.
 */
ParserResult parse_program(const Parser *parser, Span span);

ParserResult parse_identifier(const Parser *parser, Span span);

ParserResult parse_expr(const Parser *parser, Span span);

ParserResult parse_decl(const Parser *parser, Span span);

ParserResult parse_stmt(const Parser *parser, Span span);

ParserResult parse_type(const Parser *parser, Span span);

/**
 * @brief Parses an int_lit or float_lit token.
 *
 * See the Number Literals section of doc/syntax.md. Produces a
 * SyntaxNumberLitExpr whose kind distinguishes the two forms and whose
 * value holds the full raw token text.
 *
 * @param parser The parser performing the scan.
 * @param span Position to test; leading trivia must already be skipped.
 * @return Standard ParserResult contract (see the struct docs).
 */
ParserResult parse_number_lit_expr(const Parser *parser, Span span);
