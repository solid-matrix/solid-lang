#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "parser.h"
#include "parser_result.h"
#include "source.h"
#include "span.h"
#include "syntax_node.h"

typedef struct {
  bool matched;
  Span rem;
} ParserMatchResult;

typedef struct {
  Span rem;
  SyntaxNodeList *list;
  SyntaxErrorList *errors;
} ParserListResult;

bool is_letter_or_underscore(uint8_t c);

bool is_letter_digit_or_underscore(uint8_t c);

bool is_decimal_digit(uint8_t c);

bool is_binary_digit(uint8_t c);

bool is_octal_digit(uint8_t c);

bool is_hex_digit(uint8_t c);

bool is_base_digit(uint8_t c, int base);

bool is_whitespace(uint8_t c);

Span skip_trivia(const Source *source, Span span);

Span span_consumed(Span span, Span rem);

// bool match_keyword(const Source *source, Span span, Strview keyword);

// bool match(const Source *source, Span span, Strview strview);

ParserResult complete_longest_match(ParserResult *results, size_t count);

ParserListResult parse_expr_list(const Parser *parser, Span span, Strview separator);

ParserListResult parse_identifier_list(const Parser *parser, Span span, Strview separator);

ParserMatchResult match_keyword(const Source *source, Span span, Strview keyword);

ParserMatchResult match(const Source *source, Span span, Strview strview);

static const Strview KEYWORD_NAMESPACE = STRVIEW("namespace");
static const Strview KEYWORD_USING = STRVIEW("using");

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

static const Strview PUNCTUATION_DOT = STRVIEW(".");
static const Strview PUNCTUATION_COMMA = STRVIEW(",");
static const Strview PUNCTUATION_LBRACKET = STRVIEW("[");
static const Strview PUNCTUATION_RBRACKET = STRVIEW("]");
static const Strview PUNCTUATION_LPAREN = STRVIEW("(");
static const Strview PUNCTUATION_RPAREN = STRVIEW(")");
static const Strview PUNCTUATION_AT = STRVIEW("@");
static const Strview PUNCTUATION_SINGLE_QUOTE = STRVIEW("'");
static const Strview PUNCTUATION_DOUBLE_QUOTE = STRVIEW("\"");
static const Strview PUNCTUATION_COLON = STRVIEW(":");
static const Strview PUNCTUATION_SCOPE = STRVIEW("::");
static const Strview PUNCTUATION_SEMICOLON = STRVIEW(";");
static const Strview PUNCTUATION_DOLLAR = STRVIEW("$");
static const Strview PUNCTUATION_NUMBER = STRVIEW("#");
static const Strview PUNCTUATION_AMP = STRVIEW("&");
static const Strview PUNCTUATION_QUESTION = STRVIEW("?");
