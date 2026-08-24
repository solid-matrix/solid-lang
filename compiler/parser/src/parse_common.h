/**
 * @file parse_common.h
 * @brief Parsers for the common productions of doc/syntax.md.
 * @author solid-matrix
 * @version 0.0.5
 *
 * Hosts the grammar pieces shared by declarations, statements, types,
 * and expressions: NamePath today; compile-time annotations, generic/
 * call/contract parameter lists, and CallConv as those waves land.
 * Unlike parse_shared, the helpers here build syntax nodes.
 */

#pragma once

#include "parser.h"

/**
 * @brief Parses NamePath = identifier { "::" identifier }.
 *
 * Best-prefix semantics: a "::" not followed by an identifier ends the
 * path BEFORE the separator (trivia rolled back with it), leaving it
 * unconsumed for the enclosing construct.
 *
 * Trivia is skipped at the junctions between "::" and the segments;
 * the leading trivia of @p span itself must already be skipped.
 *
 * @param parser The parser providing the source text.
 * @param span Position to test; leading trivia must already be skipped.
 * @param paths Receives one SyntaxIdentifier node per segment. The list
 *              must be created by the caller; on success it owns the
 *              appended nodes and is left untouched when false is
 *              returned.
 * @param end Receives the position just past the path.
 * @return True when at least one identifier was consumed.
 */
bool parse_name_path(const Parser *parser, Span span, SyntaxNodeList *paths,
                     size_t *end);
