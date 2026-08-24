/**
 * @file parser.h
 * @brief Parser: token stream -> AST.
 * @author solid-matrix
 * @version 0.0.5
 */

#pragma once

#include <stddef.h>

#include "parser_result.h"
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

/**
 * @brief Parses a rune_lit token: one Unicode code point (raw or
 *        escaped) between single quotes.
 *
 * See the Rune Literals section of doc/syntax.md. Produces a
 * SyntaxRuneLitExpr whose value holds the full raw token text; escape
 * decoding belongs to semantic analysis.
 *
 * A malformed form (empty or multi-character content, unknown or
 * out-of-range escape, unterminated quote, invalid UTF-8) is reported
 * as SYNTAX_MALFORMED_RUNE over a recovery span and consumes it: the
 * result is matched with node == NULL, so longest-match selection
 * treats the error path like any other alternative.
 *
 * @param parser The parser performing the scan.
 * @param span Position to test; leading trivia must already be skipped.
 * @return Standard ParserResult contract (see the struct docs).
 */
ParserResult parse_rune_lit_expr(const Parser *parser, Span span);

/**
 * @brief Parses a string_lit token: a UTF-8 encoded sequence of raw or
 *        escaped characters between double quotes.
 *
 * See the String Literals section of doc/syntax.md. The escapes are
 * those defined for rune literals. Produces a SyntaxStringLitExpr
 * whose value holds the full raw token text.
 *
 * A malformed form (unterminated or multi-line literal, unknown or
 * malformed escape, invalid UTF-8, raw tab/line feed/carriage return)
 * is reported as SYNTAX_MALFORMED_STRING over a recovery span and
 * consumes it: the result is matched with node == NULL.
 *
 * @param parser The parser performing the scan.
 * @param span Position to test; leading trivia must already be skipped.
 * @return Standard ParserResult contract (see the struct docs).
 */
ParserResult parse_string_lit_expr(const Parser *parser, Span span);

ParserResult parse_identifier(const Parser *parser, Span span);

/**
 * @brief Parses NamespaceDecl = "namespace" NamePath ";".
 *
 * See the Namespace Declarations section of doc/syntax.md. Produces a
 * SyntaxNamespaceDecl owning the path segment nodes.
 *
 * A keyword hit with a broken tail (missing or unterminated path)
 * reports SYNTAX_MALFORMED_NAMEPATH; a parsed path not followed by ";"
 * reports SYNTAX_EXPECTED_SEMICOLON. Both are recovered: the result is
 * matched with node == NULL and consumes up to the stray ";" or line
 * terminator, so parse_program keeps making progress.
 *
 * @param parser The parser performing the scan.
 * @param span Position to test; leading trivia must already be skipped.
 * @return Standard ParserResult contract (see the struct docs).
 */
ParserResult parse_namespace_decl(const Parser *parser, Span span);

/**
 * @brief Parses UsingDecl = "using" NamePath ";".
 *
 * Same shape, recovery semantics, and node ownership as
 * parse_namespace_decl, anchored on the "using" keyword and producing
 * a SyntaxUsingDecl.
 *
 * @param parser The parser performing the scan.
 * @param span Position to test; leading trivia must already be skipped.
 * @return Standard ParserResult contract (see the struct docs).
 */
ParserResult parse_using_decl(const Parser *parser, Span span);

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