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
#include <stddef.h>

typedef enum
{
  SYNTAX_OK = 0x0000,
  SYNTAX_EXPECTED_EOF = 0x0001,
} SyntaxErrorCode;

typedef struct
{
  SyntaxErrorCode code;
  Span span;
} SyntaxError;

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

Parser parser_create(Source *source);

void parser_append_error(Parser *parser, Span span, SyntaxErrorCode code);

bool parse_program(Parser *parser, Span span, Span *rem, SyntaxProgram **program);

bool parse_expr(Parser *parser, Span span, Span *rem, SyntaxExpr **expr);

bool parse_decl(Parser *parser, Span span, Span *rem, SyntaxDecl **decl);

bool parse_stmt(Parser *parser, Span span, Span *rem, SyntaxStmt **stmt);

bool parse_type(Parser *parser, Span span, Span *rem, SyntaxType **type);

#endif /* SOLID_PARSER_H */
