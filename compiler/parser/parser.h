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
#include "syntax_error.h"
#include <stddef.h>

// parse不匹配时， node 为 NULL, rem与parse_XXX函数传入的span保持一致;
// parse 匹配且成功时，node 不为NULL，rem为剩余span;
// parse 匹配但失败时，node 不为NULL，rem为剩余span, 但在Parser 写入错误信息;
typedef struct {
  SyntaxNode *node;
  SourceSpan rem;
} ParseResult;

typedef struct SyntaxErrorList SyntaxErrorList;

struct SyntaxErrorList {
  SyntaxError error;
  SyntaxErrorList *next;
};

typedef struct {
  SyntaxErrorList *errors;
  Arena *arena;
} Parser;

bool parser_init(Parser *parser, Arena *arena);

void parser_destroy(Parser *parser);

ParseResult parse_program(Parser *parser, SourceSpan span);

ParseResult parse_expr(Parser *parser, SourceSpan span);

ParseResult parse_decl(Parser *parser, SourceSpan span);

ParseResult parse_stmt(Parser *parser, SourceSpan span);

ParseResult parse_type(Parser *parser, SourceSpan span);

#endif /* SOLID_PARSER_H */
