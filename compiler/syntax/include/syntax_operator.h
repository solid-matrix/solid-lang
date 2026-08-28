#pragma once

#include "strview.h"

typedef enum {
  SYNTAX_OPERATOR_INVALID = 0,

  SYNTAX_OPERATOR_PLUS,  // +
  SYNTAX_OPERATOR_MINUS, // -

  SYNTAX_OPERATOR_ADD, // +
  SYNTAX_OPERATOR_SUB, // -
  SYNTAX_OPERATOR_MUL, // *
  SYNTAX_OPERATOR_DIV, // /
  SYNTAX_OPERATOR_MOD, // %

  SYNTAX_OPERATOR_EQ,  // ==
  SYNTAX_OPERATOR_NEQ, // !=
  SYNTAX_OPERATOR_LT,  // <
  SYNTAX_OPERATOR_GT,  // >
  SYNTAX_OPERATOR_LTE, // <=
  SYNTAX_OPERATOR_GTE, // >=

  SYNTAX_OPERATOR_LNOT, // !
  SYNTAX_OPERATOR_LAND, // &&
  SYNTAX_OPERATOR_LOR,  // ||
  SYNTAX_OPERATOR_LXOR, // ^^

  SYNTAX_OPERATOR_BNOT, // ~
  SYNTAX_OPERATOR_BAND, // &
  SYNTAX_OPERATOR_BOR,  // |
  SYNTAX_OPERATOR_BXOR, // ^

  SYNTAX_OPERATOR_SHL, // <<
  SYNTAX_OPERATOR_SHR, // >>

  SYNTAX_OPERATOR_DEREF, // *

} SyntaxOperator;

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
