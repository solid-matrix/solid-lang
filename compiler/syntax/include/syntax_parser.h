/**
 * @file syntax_parser.h
 * @brief Scannerless parser: source span -> AST.
 * @author solid-matrix
 * @version 0.0.5
 */

#pragma once

#include <stddef.h>

#include "arena.h"
#include "source.h"

/**
 * @brief Immutable scanning context.
 *
 * Parse functions receive it by const pointer and never mutate it: the
 * source is read-only through the const member, and any future shared
 * state (string interning, recursion-depth limits) would live here
 * without changing call sites.
 */
typedef struct {
  const Source *source;
  Arena *arena; // backs every syntax node of the parses driven here
} SyntaxParser;

/**
 * @brief Creates a heap-allocated Parser over @p source.
 * @param source The text to parse; the Source must outlive the Parser.
 * @return The new Parser, owned by the caller; released exactly once
 *         with parser_destroy().
 */
SyntaxParser *syntax_parser_create(const Source *source);

/**
 * @brief Frees a Parser created by parser_create(), together with its
 *        arena — and through it, every syntax node the parse produced.
 * @param parser The Parser to destroy; must come from parser_create()
 *               and be destroyed exactly once. The Source is not
 *               touched: it is not owned by the Parser.
 */
void syntax_parser_destroy(SyntaxParser *parser);
