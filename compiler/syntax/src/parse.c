/**
 * @file parser.c
 * @brief The module's single public entry point.
 * @author solid-matrix
 * @version 0.0.5
 */

#include <assert.h>

#include "error.h"
#include "parse.h"
#include "syntax_parse.h"

SyntaxParseResult syntax_parse(const Source *source, Arena *arena) {
  SyntaxParser parser = {.source = source, .arena = arena};

  SyntaxNodeResult result = parse_program(&parser, source_get_span(source));
  assert(result.matched && result.node != NULL); // parse_program always matches

  // Internal accumulation is newest-at-head; the public contract is source
  // order, so the single exit reverses once.
  SyntaxErrorList *errors = syntax_errorlist_reverse(arena, result.errors);

  return (SyntaxParseResult){.program = (SyntaxProgram *)result.node, .errors = errors};
}
