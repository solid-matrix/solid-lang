/**
 * @file parser.h
 * @brief Parser: token stream -> AST.
 * @author solid-matrix
 * @version 0.0.5
 */

#pragma once

#include <stddef.h>

#include "arena.h"
#include "parser_result.h"
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
} Parser;

/**
 * @brief Creates a heap-allocated Parser over @p source.
 * @param source The text to parse; the Source must outlive the Parser.
 * @return The new Parser, owned by the caller; released exactly once
 *         with parser_destroy().
 */
Parser *parser_create(const Source *source);

/**
 * @brief Frees a Parser created by parser_create(), together with its
 *        arena — and through it, every syntax node the parse produced.
 * @param parser The Parser to destroy; must come from parser_create()
 *               and be destroyed exactly once. The Source is not
 *               touched: it is not owned by the Parser.
 */
void parser_destroy(Parser *parser);

#pragma region COMMON PARSE

ParserResult parse_identifier(const Parser *parser, Span span);

ParserResult parse_name_path(const Parser *parser, Span span);

ParserResult parse_compile_time(const Parser *parser, Span span);

ParserResult parse_program(const Parser *parser, Span span);

#pragma endregion

#pragma region TYPE PARSE

ParserResult parse_type(const Parser *parser, Span span);

ParserResult parse_named_type(const Parser *parser, Span span);

ParserResult parse_ref_type(const Parser *parser, Span span);

ParserResult parse_array_type(const Parser *parser, Span span);

ParserResult parse_func_type(const Parser *parser, Span span);

#pragma endregion

#pragma region DECL PARSE

ParserResult parse_decl(const Parser *parser, Span span);

ParserResult parse_namespace_decl(const Parser *parser, Span span);

ParserResult parse_using_decl(const Parser *parser, Span span);

ParserResult parse_let_decl(const Parser *parser, Span span);

ParserResult parse_struct_decl(const Parser *parser, Span span);

ParserResult parse_enum_decl(const Parser *parser, Span span);

ParserResult parse_union_decl(const Parser *parser, Span span);

ParserResult parse_variant_decl(const Parser *parser, Span span);

ParserResult parse_contract_decl(const Parser *parser, Span span);

ParserResult parse_func_decl(const Parser *parser, Span span);

#pragma endregion

#pragma region STMT PARSE

ParserResult parse_stmt(const Parser *parser, Span span);

ParserResult parse_body_stmt(const Parser *parser, Span span);

ParserResult parse_let_stmt(const Parser *parser, Span span);

ParserResult parse_set_stmt(const Parser *parser, Span span);

ParserResult parse_expr_stmt(const Parser *parser, Span span);

ParserResult parse_if_stmt(const Parser *parser, Span span);

ParserResult parse_loop_stmt(const Parser *parser, Span span);

ParserResult parse_break_stmt(const Parser *parser, Span span);

ParserResult parse_continue_stmt(const Parser *parser, Span span);

ParserResult parse_return_stmt(const Parser *parser, Span span);

ParserResult parse_while_stmt(const Parser *parser, Span span);

#pragma endregion

#pragma region EXPR PARSE

ParserResult parse_expr(const Parser *parser, Span span);

ParserResult parse_number_lit_expr(const Parser *parser, Span span);

ParserResult parse_rune_lit_expr(const Parser *parser, Span span);

ParserResult parse_string_lit_expr(const Parser *parser, Span span);

ParserResult parse_struct_lit_expr(const Parser *parser, Span span);

ParserResult parse_array_lit_expr(const Parser *parser, Span span);

ParserResult parse_named_expr(const Parser *parser, Span span);

ParserResult parse_sub_expr(const Parser *parser, Span span);

ParserResult parse_operand_expr(const Parser *parser, Span span);

#pragma endregion
