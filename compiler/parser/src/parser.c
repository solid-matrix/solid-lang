/**
 * @file parser.c
 * @brief Parser implementation.
 * @author solid-matrix
 * @version 0.0.5
 */

#include <assert.h>
#include <stdbool.h>

#include "arena.h"
#include "parser.h"
#include "xmem.h"

#pragma region PARSER

Parser *parser_create(const Source *source) {
  Arena *arena = arena_create();

  Parser *parser = xmalloc(sizeof(Parser));
  *parser = (Parser){.source = source, .arena = arena};
  return parser;
}

void parser_destroy(Parser *parser) {
  assert(parser != NULL);

  arena_destroy(parser->arena); // reclaims every node of this parse
  xfree(parser);
}

#pragma endregion
