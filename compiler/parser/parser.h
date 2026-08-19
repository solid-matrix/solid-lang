/**
 * @file parser.h
 * @brief Parser: token stream -> AST.
 * @author solid-matrix
 * @version 0.0.5
 */

#pragma once

#include <stddef.h>

#include "ast.h"
#include "source.h"
#include "syntax_error.h"

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

Parser parser_create(Source *source);

void parser_destroy(Parser *parser);

void parser_append_error(Parser *parser, Span span, SyntaxErrorCode code);

/**
 * @struct ParserResult
 * @brief Outcome of parsing one construct (see per-function contracts).
 *
 * Every parse_XXX(Parser *, Span) follows the same contract:
 *
 *   matched == false — the construct does not start at \p span. Nothing is
 *   consumed and no error is recorded:
 *       node == NULL, rem == span.
 *   rem is returned exactly as received: the position was tested and
 *   rejected, so it is meaningless to strip trivia here (that would make
 *   keyword/position checks accept input that does not literally start at
 *   \p span, and would corrupt backtracking). Only a successful match
 *   normalizes rem.
 *   The caller may backtrack and try another alternative, or treat this as
 *   the end of the enclosing construct.
 *
 *   matched == true — the construct was recognized and its input was
 *   consumed, so rem is always advanced past what was parsed and normalized
 *   (leading trivia skipped, pointing at the next token). Callers may
 *   unconditionally continue with res.rem and may inspect the first byte of
 *   it directly (this keeps loops in parse_program etc. from spinning on the
 *   same span). Errors are reported through
 *   parser->errors alone and never drive control flow here:
 *       node != NULL -> a concrete AST node to keep (e.g. append it to the
 *                       enclosing node list); parse errors may still have
 *                       been recorded in parser->errors;
 *       node == NULL -> nothing worth keeping (e.g. a bare or dropped
 *                       construct); error info, if any, is likewise already
 *                       in parser->errors.
 *   After matched == true a caller must not try alternatives: the enclosing
 *   construct was already recognized.
 */
typedef struct
{
  bool matched;
  Span rem;
  SyntaxNode *node;
} ParserResult;

SyntaxProgram *parse(Parser *parser);

ParserResult parse_identifier(Parser *parser, Span span);

ParserResult parse_program(Parser *parser, Span span);

ParserResult parse_expr(Parser *parser, Span span);

ParserResult parse_decl(Parser *parser, Span span);

ParserResult parse_stmt(Parser *parser, Span span);

ParserResult parse_type(Parser *parser, Span span);
