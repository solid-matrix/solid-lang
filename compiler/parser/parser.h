/**
 * @file parser.h
 * @brief Parser: token stream -> AST.
 * @author solid-matrix
 * @version 0.0.5
 */

#pragma once

#include <stddef.h>

#include "source.h"
#include "syntax_error.h"
#include "syntax_node.h"

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
 *       errors is ALWAYS a valid (possibly empty) list owned by the
 *       result: consumers never test it for NULL, only for emptiness.
 *       node != NULL -> a concrete AST node to keep (e.g. append it to
 *                       the enclosing node list);
 *       node == NULL -> nothing worth keeping (e.g. a dropped
 *                       construct); diagnostics, if any, still travel
 *                       in errors.
 *
 *   errors carries every SyntaxError produced while recognizing this
 *   construct, directly or by nested constructs. The list is owned by
 *   the result: a combinator must either merge it upward into its own
 *   result (sequential acceptance; the emptied source handle is
 *   released separately) or free it together with the losing branch
 *   (longest-match selection adopts the winner's list and releases the
 *   losers' -> winner-takes-errors). The top-level caller disposes of
 *   the final list with syntax_errorlist_destroy().
 */
typedef struct {
  bool matched;
  Span rem;
  SyntaxNode *node;
  SyntaxErrorList *errors;
} ParserResult;

/**
 * @brief Builds the not-matched outcome: errors == NULL, node == NULL,
 *        rem == @p span.
 */
ParserResult parser_result_not_match(Span span);

/**
 * @brief Builds a matched outcome.
 * @param rem The position just past the consumed text.
 * @param node The AST node to keep, or NULL for a dropped construct.
 * @param errors Diagnostics to attach; NULL means "no diagnostics" and
 *               receives a fresh empty list, so the returned result
 *               always owns a valid (possibly empty) list.
 * @return The result.
 */
ParserResult parser_result_matched(Span rem, SyntaxNode *node,
                                   SyntaxErrorList *errors);

bool is_letter_or_underscore(uint8_t c);
bool is_letter_digit_or_underscore(uint8_t c);
bool is_decimal_digit(uint8_t c);
bool is_binary_digit(uint8_t c);
bool is_octal_digit(uint8_t c);
bool is_hex_digit(uint8_t c);
bool is_base_digit(uint8_t c, int base);
bool is_whitespace(uint8_t c);

Span skip_trivia(const Source *source, Span span);

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

ParserResult parse_identifier(const Parser *parser, Span span);

ParserResult parse_expr(const Parser *parser, Span span);

ParserResult parse_decl(const Parser *parser, Span span);

ParserResult parse_stmt(const Parser *parser, Span span);

ParserResult parse_type(const Parser *parser, Span span);

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