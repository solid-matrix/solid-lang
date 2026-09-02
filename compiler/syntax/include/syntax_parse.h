/**
 * @file syntax_parse.h
 * @brief The public parse entry point and its outcome.
 * @author solid-matrix
 */

#pragma once

#include "arena.h"
#include "source.h"
#include "syntax_error.h"
#include "syntax_node.h"

/**
 * @brief One parsed translation unit plus its diagnostics.
 * @details @p program is never NULL: parse_program always matches, and a
 *          junk tail surfaces as diagnostics instead of a missing tree.
 *          @p errors is in source order — the first diagnostic is the
 *          first in the source — and NULL when the parse was clean.
 */
typedef struct {
  SyntaxProgram *program;
  SyntaxErrorList *errors;
} SyntaxParseResult;

/**
 * @brief Parses @p source into a translation unit.
 * @param source The text to parse; must outlive the result.
 * @param arena Backs the program tree and every diagnostic; must outlive
 *              the result.
 * @return The program and its diagnostics in source order.
 */
SyntaxParseResult syntax_parse(const Source *source, Arena *arena);
