/**
 * @file parser.h
 * @brief Parser: token stream -> AST.
 * @author solid-matrix
 * @version 0.0.5
 */

#pragma once

#include "ast.h"
#include "source.h"
#include "syntax_error.h"
#include <stddef.h>

typedef struct SyntaxErrorLinkedList SyntaxErrorLinkedList;
struct SyntaxErrorLinkedList
{
  SyntaxError error;
  SyntaxErrorLinkedList *next;
};

typedef struct
{
  Source *source;
  SyntaxErrorLinkedList *errors;
} Parser;

typedef struct
{
  bool matched;
  Span rem;
  SyntaxNode *node;
} ParserResult;

Parser parser_create(Source *source);

void parser_destroy(Parser *parser);

void parser_append_error(Parser *parser, Span span, SyntaxErrorCode code);

SyntaxProgram *parse_program(Parser *parser);

ParserResult parse_expr(Parser *parser, Span span);

ParserResult parse_decl(Parser *parser, Span span);

ParserResult parse_stmt(Parser *parser, Span span);

ParserResult parse_type(Parser *parser, Span span);
