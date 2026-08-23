#pragma once

#include "xmem.h"
#include "parser.h"

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