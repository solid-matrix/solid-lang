/**
 * @file parser.h
 * @brief Parser: token stream -> AST.
 * @author solid-matrix
 * @version 0.0.5
 */

#ifndef SOLID_PARSER_H
#define SOLID_PARSER_H

#include "ast.h"
#include "source.h"
#include "syntax_error.h"
#include <stddef.h>

typedef struct SyntaxErrorList SyntaxErrorList;
struct SyntaxErrorList
{
  SyntaxError error;
  SyntaxErrorList *next;
};

typedef struct
{
  Source *source;
  SyntaxErrorList *errors;
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

#endif /* SOLID_PARSER_H */
