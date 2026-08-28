/**
 * @file parser.c
 * @brief Parser implementation.
 * @author solid-matrix
 * @version 0.0.5
 */

#include <assert.h>
#include <stdbool.h>

#include "arena.h"
#include "syntax_parser.h"
#include "xmem.h"

#pragma region PARSER

SyntaxParser *syntax_parser_create(const Source *source) {
  Arena *arena = arena_create();

  SyntaxParser *parser = xmalloc(sizeof(SyntaxParser));
  *parser = (SyntaxParser){.source = source, .arena = arena};
  return parser;
}

void syntax_parser_destroy(SyntaxParser *parser) {
  assert(parser != NULL);

  arena_destroy(parser->arena); // reclaims every node of this parse
  xfree(parser);
}

#pragma endregion
