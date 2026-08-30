/**
 * @file parse_aux.h
 * @brief Internal helpers: character classes, trivia, matching and
 *        shared list composition.
 * @author solid-matrix
 * @version 0.0.5
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "source.h"
#include "span.h"
#include "strview.h"
#include "syntax_error.h"
#include "syntax_parser.h"
#include "syntax_result.h"

/** Length of a fixed-size array. */
#define COUNT_OF(a) (sizeof(a) / sizeof((a)[0]))

/**
 * @brief Element parser callback for parse_field_list.
 */
typedef SyntaxNodeResult (*SyntaxFieldFn)(const SyntaxParser *, Span);

/**
 * @brief True for `A-Z`, `a-z` or `_`.
 * @param c The byte to test.
 * @return The test result.
 */
bool is_letter_or_underscore(uint8_t c);

/**
 * @brief True for identifier-continuation bytes: letters, digits or `_`.
 * @param c The byte to test.
 * @return The test result.
 */
bool is_letter_digit_or_underscore(uint8_t c);

/**
 * @brief True for `0-9`.
 * @param c The byte to test.
 * @return The test result.
 */
bool is_decimal_digit(uint8_t c);

/**
 * @brief True for `0` or `1`.
 * @param c The byte to test.
 * @return The test result.
 */
bool is_binary_digit(uint8_t c);

/**
 * @brief True for `0-7`.
 * @param c The byte to test.
 * @return The test result.
 */
bool is_octal_digit(uint8_t c);

/**
 * @brief True for `0-9`, `a-f` or `A-F`.
 * @param c The byte to test.
 * @return The test result.
 */
bool is_hex_digit(uint8_t c);

/**
 * @brief True when @p c is a digit of @p base.
 * @param c The byte to test.
 * @param base 2, 8, 10 or 16; other bases fall back to decimal.
 * @return The test result.
 */
bool is_base_digit(uint8_t c, int base);

/**
 * @brief True for blank, tab, vertical tab, form feed, carriage return
 *        or newline.
 * @param c The byte to test.
 * @return The test result.
 */
bool is_whitespace(uint8_t c);

/**
 * @brief Skips whitespace and `//` line comments.
 * @param source The source to read.
 * @param span Where to start skipping.
 * @return The span from the first non-trivia byte to @p span's end.
 */
Span skip_trivia(const Source *source, Span span);

/**
 * @brief The text consumed by advancing from @p span to @p rem.
 * @param span The span the parse started at.
 * @param rem The position the parse stopped at.
 * @return span.start .. rem.start.
 */
Span span_consumed(Span span, Span rem);

/**
 * @brief Picks the outcome with the furthest rem.start; ties keep the
 *        earliest entry.
 * @param results The candidate outcomes, in preference order.
 * @param count Number of candidates; must be positive.
 * @return The selected outcome.
 */
SyntaxNodeResult complete_longest_match(SyntaxNodeResult *results, size_t count);

/**
 * @brief Parses expr elements separated by @p separator.
 * @param parser Parsing context.
 * @param span Where the list starts.
 * @param separator The separator token.
 * @return List outcome; see SyntaxListResult.
 */
SyntaxListResult parse_expr_list(const SyntaxParser *parser, Span span, Strview separator);

/**
 * @brief Parses identifier elements separated by @p separator.
 * @param parser Parsing context.
 * @param span Where the list starts.
 * @param separator The separator token.
 * @return List outcome; see SyntaxListResult.
 */
SyntaxListResult parse_identifier_list(const SyntaxParser *parser, Span span, Strview separator);

/**
 * @brief Parses `<generic param, ...>`; silent when `<` is absent.
 * @param parser Parsing context.
 * @param span Where the list starts.
 * @return List outcome; see SyntaxListResult.
 */
SyntaxListResult parse_generic_param_list(const SyntaxParser *parser, Span span);

/**
 * @brief Parses `(call param, ...)`.
 * @param parser Parsing context.
 * @param span Where the list starts.
 * @return List outcome; see SyntaxListResult.
 */
SyntaxListResult parse_call_param_list(const SyntaxParser *parser, Span span);

/**
 * @brief Parses generic arguments separated by commas.
 * @param parser Parsing context.
 * @param span Where the list starts.
 * @return List outcome; see SyntaxListResult.
 */
SyntaxListResult parse_generic_arg_list(const SyntaxParser *parser, Span span);

/**
 * @brief Parses field elements separated by commas, closed by `;` or
 *        `{`; reports @p missing_code when the first field is absent.
 * @param parser Parsing context.
 * @param span Where the list starts.
 * @param parse_field Parses one element.
 * @param missing_code Diagnostic when the list is empty.
 * @return List outcome; see SyntaxListResult.
 */
SyntaxListResult parse_field_list(const SyntaxParser *parser, Span span, SyntaxFieldFn parse_field,
                                  SyntaxErrorCode missing_code);

/**
 * @brief Parses a statement body at @p span: `{ stmts }` or `;`.
 * @param parser Parsing context.
 * @param span Where the body starts.
 * @return Parse outcome; see SyntaxNodeResult.
 * @note Always matched: a missing body reports EXPECTED_BODY and the
 *       body node stays NULL.
 */
SyntaxNodeResult parse_body_position(const SyntaxParser *parser, Span span);

/**
 * @brief Matches @p keyword only when it does not continue into a word
 *        character.
 * @param parser Parsing context.
 * @param span Where to match.
 * @param keyword The keyword text.
 * @return Match outcome; see SyntaxMatchResult.
 */
SyntaxMatchResult match_keyword(const SyntaxParser *parser, Span span, Strview keyword);

/**
 * @brief Matches @p strview at the span start.
 * @param parser Parsing context.
 * @param span Where to match.
 * @param strview The bytes to match.
 * @return Match outcome; see SyntaxMatchResult.
 */
SyntaxMatchResult match(const SyntaxParser *parser, Span span, Strview strview);

/**
 * @brief The scalar value of a hex digit, case-insensitive.
 * @param d The byte to convert.
 * @return 0-15 for hex digits; meaningless otherwise.
 */
uint32_t hex_value(uint8_t d);

/**
 * @brief Matches one escape sequence.
 * @param parser Parsing context.
 * @param span Where the escape starts (on `\`).
 * @return Match outcome; see SyntaxMatchResult.
 * @details escape = quote_escape | ascii_escape | unicode_escape; a
 *          span starting with `\` is always consumed: an invalid
 *          escape returns matched == true with the specific
 *          diagnostic so the caller's recovery resumes right after
 *          it. matched == false only when the span does not start
 *          with `\`.
 */
SyntaxMatchResult match_escape(const SyntaxParser *parser, Span span);

/**
 * @brief Matches one UTF-8 encoded scalar value, validated strictly:
 *        no overlong encodings, surrogates or values above U+10FFFF.
 * @param parser Parsing context.
 * @param span Where the character starts.
 * @return Match outcome; see SyntaxMatchResult.
 * @note An invalid byte sequence consumes its lead byte and reports
 *       INVALID_CHARACTER so recovery resumes right after it;
 *       matched == false only for an empty span.
 */
SyntaxMatchResult match_utf8_char(const SyntaxParser *parser, Span span);

/* ---- keywords ---------------------------------------------------------- */

static const Strview KEYWORD_NAMESPACE = STRVIEW("namespace");
static const Strview KEYWORD_USING = STRVIEW("using");
static const Strview KEYWORD_READONLY = STRVIEW("readonly");
static const Strview KEYWORD_WRITEONLY = STRVIEW("writeonly");
static const Strview KEYWORD_LET = STRVIEW("let");
static const Strview KEYWORD_SET = STRVIEW("set");
static const Strview KEYWORD_IF = STRVIEW("if");
static const Strview KEYWORD_ELSE = STRVIEW("else");
static const Strview KEYWORD_LOOP = STRVIEW("loop");
static const Strview KEYWORD_WHILE = STRVIEW("while");
static const Strview KEYWORD_BREAK = STRVIEW("break");
static const Strview KEYWORD_CONTINUE = STRVIEW("continue");
static const Strview KEYWORD_RETURN = STRVIEW("return");
static const Strview KEYWORD_STRUCT = STRVIEW("struct");
static const Strview KEYWORD_UNION = STRVIEW("union");
static const Strview KEYWORD_ENUM = STRVIEW("enum");
static const Strview KEYWORD_VARIANT = STRVIEW("variant");
static const Strview KEYWORD_CONTRACT = STRVIEW("contract");
static const Strview KEYWORD_FUNC = STRVIEW("func");
static const Strview KEYWORD_FULFILLS = STRVIEW("fulfills");

/* ---- operators --------------------------------------------------------- */

static const Strview OPERATOR_LOR = STRVIEW("||");
static const Strview OPERATOR_LXOR = STRVIEW("^^");
static const Strview OPERATOR_LAND = STRVIEW("&&");

static const Strview OPERATOR_EQ = STRVIEW("==");
static const Strview OPERATOR_NEQ = STRVIEW("!=");
static const Strview OPERATOR_LT = STRVIEW("<");
static const Strview OPERATOR_GT = STRVIEW(">");
static const Strview OPERATOR_LTE = STRVIEW("<=");
static const Strview OPERATOR_GTE = STRVIEW(">=");

static const Strview OPERATOR_BOR = STRVIEW("|");
static const Strview OPERATOR_BXOR = STRVIEW("^");
static const Strview OPERATOR_BAND = STRVIEW("&");

static const Strview OPERATOR_SHL = STRVIEW("<<");
static const Strview OPERATOR_SHR = STRVIEW(">>");

static const Strview OPERATOR_ADD = STRVIEW("+");
static const Strview OPERATOR_SUB = STRVIEW("-");
static const Strview OPERATOR_MUL = STRVIEW("*");
static const Strview OPERATOR_DIV = STRVIEW("/");
static const Strview OPERATOR_MOD = STRVIEW("%");

static const Strview OPERATOR_PLUS = STRVIEW("+");
static const Strview OPERATOR_MINUS = STRVIEW("-");
static const Strview OPERATOR_LNOT = STRVIEW("!");
static const Strview OPERATOR_BNOT = STRVIEW("~");
static const Strview OPERATOR_DEREF = STRVIEW("*");

/* ---- punctuation ------------------------------------------------------- */

static const Strview PUNCTUATION_DOT = STRVIEW(".");
static const Strview PUNCTUATION_COMMA = STRVIEW(",");
static const Strview PUNCTUATION_LBRACKET = STRVIEW("[");
static const Strview PUNCTUATION_RBRACKET = STRVIEW("]");
static const Strview PUNCTUATION_LBRACE = STRVIEW("{");
static const Strview PUNCTUATION_RBRACE = STRVIEW("}");
static const Strview PUNCTUATION_LPAREN = STRVIEW("(");
static const Strview PUNCTUATION_RPAREN = STRVIEW(")");
static const Strview PUNCTUATION_AT = STRVIEW("@");
static const Strview PUNCTUATION_SINGLE_QUOTE = STRVIEW("'");
static const Strview PUNCTUATION_DOUBLE_QUOTE = STRVIEW("\"");
static const Strview PUNCTUATION_COLON = STRVIEW(":");
static const Strview PUNCTUATION_EQUALS = STRVIEW("=");
static const Strview PUNCTUATION_LT = STRVIEW("<");
static const Strview PUNCTUATION_GT = STRVIEW(">");
static const Strview PUNCTUATION_SCOPE = STRVIEW("::");
static const Strview PUNCTUATION_SEMICOLON = STRVIEW(";");
static const Strview PUNCTUATION_AMP = STRVIEW("&");

/* ---- literal suffixes (longest first) ---------------------------------- */

static const Strview INT_SUFFIXES[] = {
    STRVIEW("isize"), STRVIEW("usize"), STRVIEW("i128"), STRVIEW("u128"), STRVIEW("i64"),
    STRVIEW("u64"),   STRVIEW("i32"),   STRVIEW("u32"),  STRVIEW("i16"),  STRVIEW("u16"),
    STRVIEW("i8"),    STRVIEW("u8"),    STRVIEW("i"),    STRVIEW("u"),
};

static const Strview FLOAT_SUFFIXES[] = {STRVIEW("f32"), STRVIEW("f64"), STRVIEW("f"), STRVIEW("d")};
