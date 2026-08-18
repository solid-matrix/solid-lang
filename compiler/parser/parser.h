/**
 * @file parser.h
 * @brief Parser: token stream -> AST.
 * @author solid-matrix
 * @version 0.0.5
 */

#ifndef SOLID_PARSER_H
#define SOLID_PARSER_H

#include "arena.h"
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
  SourceSpan span;
} SyntaxError;

SyntaxError syntax_error_create(SyntaxErrorCode code, SourceSpan span);

// parse不匹配时，matched 为 false, node 为 NULL, rem与parse_XXX函数传入的span保持一致;
// parse 匹配且成功时，matched 为 true, node 不为NULL，rem为剩余span;
// parse 匹配但失败时，matched 为 true, node 不为NULL，rem为剩余span, Parser 写入错误信息;
typedef struct
{
  bool matched;
  SyntaxNode *node;
  SourceSpan rem;
} ParseResult;

typedef struct SyntaxErrorList SyntaxErrorList;

struct SyntaxErrorList
{
  SyntaxError error;
  SyntaxErrorList *next;
};

typedef struct
{
  SyntaxErrorList *errors;
  Arena *arena;
} Parser;

Parser parser_create(Arena *arena);

void parser_append_error(Parser *parser, SyntaxError error);

ParseResult parse_program(Parser *parser, SourceSpan span);

ParseResult parse_expr(Parser *parser, SourceSpan span);

ParseResult parse_decl(Parser *parser, SourceSpan span);

ParseResult parse_stmt(Parser *parser, SourceSpan span);

ParseResult parse_type(Parser *parser, SourceSpan span);

#endif /* SOLID_PARSER_H */
