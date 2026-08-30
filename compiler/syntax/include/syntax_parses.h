/**
 * @file syntax_parses.h
 * @brief Public parse entry points, one per grammar construct.
 * @author solid-matrix
 * @version 0.0.5
 */

#pragma once

#include "syntax_parser.h"
#include "syntax_result.h"

#pragma region COMMON

/**
 * @brief Parses one identifier.
 * @param parser Parsing context.
 * @param span Where the construct starts.
 * @return Parse outcome; see SyntaxNodeResult.
 */
SyntaxNodeResult parse_identifier(const SyntaxParser *parser, Span span);

/**
 * @brief Parses `@name[(expr, ...)]`.
 * @param parser Parsing context.
 * @param span Where the construct starts.
 * @return Parse outcome; see SyntaxNodeResult.
 */
SyntaxNodeResult parse_compile_time(const SyntaxParser *parser, Span span);

/**
 * @brief Parses a run of zero or more compile-time forms.
 * @param parser Parsing context.
 * @param span Where the run starts.
 * @return List outcome; see SyntaxListResult.
 */
SyntaxListResult parse_annotations(const SyntaxParser *parser, Span span);

/**
 * @brief Parses `[annotations] name [: type]`.
 * @param parser Parsing context.
 * @param span Where the construct starts.
 * @return Parse outcome; see SyntaxNodeResult.
 */
SyntaxNodeResult parse_generic_param(const SyntaxParser *parser, Span span);

/**
 * @brief Parses `[annotations] name : type`.
 * @param parser Parsing context.
 * @param span Where the construct starts.
 * @return Parse outcome; see SyntaxNodeResult.
 */
SyntaxNodeResult parse_call_param(const SyntaxParser *parser, Span span);

/**
 * @brief Parses a generic argument: a Type, or `id = PrimaryExpr`.
 * @param parser Parsing context.
 * @param span Where the construct starts.
 * @return Parse outcome; see SyntaxNodeResult.
 */
SyntaxNodeResult parse_generic_arg(const SyntaxParser *parser, Span span);

/**
 * @brief Parses a translation unit of top-level declarations.
 * @param parser Parsing context.
 * @param span Where the unit starts; leading trivia is consumed.
 * @return Parse outcome; see SyntaxNodeResult.
 */
SyntaxNodeResult parse_program(const SyntaxParser *parser, Span span);

#pragma endregion

#pragma region TYPE

/**
 * @brief Dispatches to the matching type parser.
 * @param parser Parsing context.
 * @param span Where the construct starts.
 * @return Parse outcome; see SyntaxNodeResult.
 */
SyntaxNodeResult parse_type(const SyntaxParser *parser, Span span);

/**
 * @brief Parses `&[readonly | writeonly] type`.
 * @param parser Parsing context.
 * @param span Where the construct starts.
 * @return Parse outcome; see SyntaxNodeResult.
 */
SyntaxNodeResult parse_ref_type(const SyntaxParser *parser, Span span);

/**
 * @brief Parses `[len] type`.
 * @param parser Parsing context.
 * @param span Where the construct starts.
 * @return Parse outcome; see SyntaxNodeResult.
 */
SyntaxNodeResult parse_array_type(const SyntaxParser *parser, Span span);

/**
 * @brief Parses `&func(params)[callconv][: type]`.
 * @param parser Parsing context.
 * @param span Where the construct starts.
 * @return Parse outcome; see SyntaxNodeResult.
 */
SyntaxNodeResult parse_func_type(const SyntaxParser *parser, Span span);

/**
 * @brief Parses `path[<generic args>]`; type position, so generic
 *        arguments need no follow-set gating.
 * @param parser Parsing context.
 * @param span Where the construct starts.
 * @return Parse outcome; see SyntaxNodeResult.
 */
SyntaxNodeResult parse_named_type(const SyntaxParser *parser, Span span);

#pragma endregion

#pragma region DECL

/**
 * @brief Dispatches to the matching declaration parser.
 * @param parser Parsing context.
 * @param span Where the construct starts.
 * @return Parse outcome; see SyntaxNodeResult.
 */
SyntaxNodeResult parse_decl(const SyntaxParser *parser, Span span);

/**
 * @brief Parses `namespace path;`.
 * @param parser Parsing context.
 * @param span Where the construct starts.
 * @return Parse outcome; see SyntaxNodeResult.
 */
SyntaxNodeResult parse_namespace_decl(const SyntaxParser *parser, Span span);

/**
 * @brief Parses `using path;`.
 * @param parser Parsing context.
 * @param span Where the construct starts.
 * @return Parse outcome; see SyntaxNodeResult.
 */
SyntaxNodeResult parse_using_decl(const SyntaxParser *parser, Span span);

/**
 * @brief Parses `[annotations] let id[: type] [= value];`.
 * @param parser Parsing context.
 * @param span Where the construct starts.
 * @return Parse outcome; see SyntaxNodeResult.
 */
SyntaxNodeResult parse_let_decl(const SyntaxParser *parser, Span span);

/**
 * @brief Parses `[annotations] struct Name[<params>] [; | { fields }]`.
 * @param parser Parsing context.
 * @param span Where the construct starts.
 * @return Parse outcome; see SyntaxNodeResult.
 */
SyntaxNodeResult parse_struct_decl(const SyntaxParser *parser, Span span);

/**
 * @brief Parses `[annotations] enum Name[: behind] [; | { fields }]`.
 * @param parser Parsing context.
 * @param span Where the construct starts.
 * @return Parse outcome; see SyntaxNodeResult.
 */
SyntaxNodeResult parse_enum_decl(const SyntaxParser *parser, Span span);

/**
 * @brief Parses `[annotations] union Name[<params>] [; | { fields }]`.
 * @param parser Parsing context.
 * @param span Where the construct starts.
 * @return Parse outcome; see SyntaxNodeResult.
 */
SyntaxNodeResult parse_union_decl(const SyntaxParser *parser, Span span);

/**
 * @brief Parses `[annotations] variant Name[: behind][<params>] { fields }`.
 * @param parser Parsing context.
 * @param span Where the construct starts.
 * @return Parse outcome; see SyntaxNodeResult.
 */
SyntaxNodeResult parse_variant_decl(const SyntaxParser *parser, Span span);

/**
 * @brief Parses `contract Name[<params>](call params)[: return];`.
 * @param parser Parsing context.
 * @param span Where the construct starts.
 * @return Parse outcome; see SyntaxNodeResult.
 */
SyntaxNodeResult parse_contract_decl(const SyntaxParser *parser, Span span);

/**
 * @brief Parses `func Name[<params>](call params)[callconv][: return][fulfills ...] body`.
 * @param parser Parsing context.
 * @param span Where the construct starts.
 * @return Parse outcome; see SyntaxNodeResult.
 */
SyntaxNodeResult parse_func_decl(const SyntaxParser *parser, Span span);

#pragma endregion

#pragma region STMT

/**
 * @brief Dispatches to the matching statement parser.
 * @param parser Parsing context.
 * @param span Where the construct starts.
 * @return Parse outcome; see SyntaxNodeResult.
 */
SyntaxNodeResult parse_stmt(const SyntaxParser *parser, Span span);

/**
 * @brief Parses `;`.
 * @param parser Parsing context.
 * @param span Where the construct starts.
 * @return Parse outcome; see SyntaxNodeResult.
 */
SyntaxNodeResult parse_empty_stmt(const SyntaxParser *parser, Span span);

/**
 * @brief Parses `{ stmts }`.
 * @param parser Parsing context.
 * @param span Where the construct starts.
 * @return Parse outcome; see SyntaxNodeResult.
 */
SyntaxNodeResult parse_body_stmt(const SyntaxParser *parser, Span span);

/**
 * @brief Parses `let id[: type] = value;`.
 * @param parser Parsing context.
 * @param span Where the construct starts.
 * @return Parse outcome; see SyntaxNodeResult.
 */
SyntaxNodeResult parse_let_stmt(const SyntaxParser *parser, Span span);

/**
 * @brief Parses `set left = right;`.
 * @param parser Parsing context.
 * @param span Where the construct starts.
 * @return Parse outcome; see SyntaxNodeResult.
 */
SyntaxNodeResult parse_set_stmt(const SyntaxParser *parser, Span span);

/**
 * @brief Parses `expr;`.
 * @param parser Parsing context.
 * @param span Where the construct starts.
 * @return Parse outcome; see SyntaxNodeResult.
 */
SyntaxNodeResult parse_expr_stmt(const SyntaxParser *parser, Span span);

/**
 * @brief Parses `if (condition) then [else else_stmt]`.
 * @param parser Parsing context.
 * @param span Where the construct starts.
 * @return Parse outcome; see SyntaxNodeResult.
 */
SyntaxNodeResult parse_if_stmt(const SyntaxParser *parser, Span span);

/**
 * @brief Parses `loop stmt`.
 * @param parser Parsing context.
 * @param span Where the construct starts.
 * @return Parse outcome; see SyntaxNodeResult.
 */
SyntaxNodeResult parse_loop_stmt(const SyntaxParser *parser, Span span);

/**
 * @brief Parses `break;`.
 * @param parser Parsing context.
 * @param span Where the construct starts.
 * @return Parse outcome; see SyntaxNodeResult.
 */
SyntaxNodeResult parse_break_stmt(const SyntaxParser *parser, Span span);

/**
 * @brief Parses `continue;`.
 * @param parser Parsing context.
 * @param span Where the construct starts.
 * @return Parse outcome; see SyntaxNodeResult.
 */
SyntaxNodeResult parse_continue_stmt(const SyntaxParser *parser, Span span);

/**
 * @brief Parses `return [expr];`.
 * @param parser Parsing context.
 * @param span Where the construct starts.
 * @return Parse outcome; see SyntaxNodeResult.
 */
SyntaxNodeResult parse_return_stmt(const SyntaxParser *parser, Span span);

/**
 * @brief Parses `while (condition) stmt`.
 * @param parser Parsing context.
 * @param span Where the construct starts.
 * @return Parse outcome; see SyntaxNodeResult.
 */
SyntaxNodeResult parse_while_stmt(const SyntaxParser *parser, Span span);

#pragma endregion

#pragma region EXPR

/**
 * @brief Entry of the expression ladder; parses the logical-or level.
 * @param parser Parsing context.
 * @param span Where the construct starts.
 * @return Parse outcome; see SyntaxNodeResult.
 */
SyntaxNodeResult parse_expr(const SyntaxParser *parser, Span span);

/**
 * @brief Parses left-associative `||` over the tighter level.
 * @param parser Parsing context.
 * @param span Where the construct starts.
 * @return Parse outcome; see SyntaxNodeResult.
 */
SyntaxNodeResult parse_logical_or_expr(const SyntaxParser *parser, Span span);

/**
 * @brief Parses left-associative `^^` over the tighter level.
 * @param parser Parsing context.
 * @param span Where the construct starts.
 * @return Parse outcome; see SyntaxNodeResult.
 */
SyntaxNodeResult parse_logical_xor_expr(const SyntaxParser *parser, Span span);

/**
 * @brief Parses left-associative `&&` over the tighter level.
 * @param parser Parsing context.
 * @param span Where the construct starts.
 * @return Parse outcome; see SyntaxNodeResult.
 */
SyntaxNodeResult parse_logical_and_expr(const SyntaxParser *parser, Span span);

/**
 * @brief Parses left-associative `== != < > <= >=` over the tighter level.
 * @param parser Parsing context.
 * @param span Where the construct starts.
 * @return Parse outcome; see SyntaxNodeResult.
 */
SyntaxNodeResult parse_relational_expr(const SyntaxParser *parser, Span span);

/**
 * @brief Parses left-associative `| ^ &` over the tighter level.
 * @param parser Parsing context.
 * @param span Where the construct starts.
 * @return Parse outcome; see SyntaxNodeResult.
 */
SyntaxNodeResult parse_bitwise_expr(const SyntaxParser *parser, Span span);

/**
 * @brief Parses left-associative `<< >>` over the tighter level.
 * @param parser Parsing context.
 * @param span Where the construct starts.
 * @return Parse outcome; see SyntaxNodeResult.
 */
SyntaxNodeResult parse_shift_expr(const SyntaxParser *parser, Span span);

/**
 * @brief Parses left-associative `+ -` over the tighter level.
 * @param parser Parsing context.
 * @param span Where the construct starts.
 * @return Parse outcome; see SyntaxNodeResult.
 */
SyntaxNodeResult parse_additive_expr(const SyntaxParser *parser, Span span);

/**
 * @brief Parses left-associative `* / %` over the tighter level.
 * @param parser Parsing context.
 * @param span Where the construct starts.
 * @return Parse outcome; see SyntaxNodeResult.
 */
SyntaxNodeResult parse_multiplicative_expr(const SyntaxParser *parser, Span span);

/**
 * @brief Parses prefix operators (`- + ! ~ *`) over the postfix level.
 * @param parser Parsing context.
 * @param span Where the construct starts.
 * @return Parse outcome; see SyntaxNodeResult.
 */
SyntaxNodeResult parse_unary_expr(const SyntaxParser *parser, Span span);

/**
 * @brief Parses a primary plus `.name`, `[index]`, `(args)` chains.
 * @param parser Parsing context.
 * @param span Where the construct starts.
 * @return Parse outcome; see SyntaxNodeResult.
 */
SyntaxNodeResult parse_postfix_expr(const SyntaxParser *parser, Span span);

/**
 * @brief Dispatches to the matching primary parser.
 * @param parser Parsing context.
 * @param span Where the construct starts.
 * @return Parse outcome; see SyntaxNodeResult.
 */
SyntaxNodeResult parse_primary_expr(const SyntaxParser *parser, Span span);

/**
 * @brief Parses `path[<generic args>]`; expression position, so the
 *        generic form is accepted only when a FOLLOW-set token follows
 *        `>`, and the parse otherwise falls back to the bare path.
 * @param parser Parsing context.
 * @param span Where the construct starts.
 * @return Parse outcome; see SyntaxNodeResult.
 */
SyntaxNodeResult parse_named_expr(const SyntaxParser *parser, Span span);

/**
 * @brief Parses an integer literal: decimal, binary, octal or hex.
 * @param parser Parsing context.
 * @param span Where the construct starts.
 * @return Parse outcome; see SyntaxNodeResult.
 */
SyntaxNodeResult parse_int_lit_expr(const SyntaxParser *parser, Span span);

/**
 * @brief Parses a float literal: exponent, dot or suffix form.
 * @param parser Parsing context.
 * @param span Where the construct starts.
 * @return Parse outcome; see SyntaxNodeResult.
 */
SyntaxNodeResult parse_float_lit_expr(const SyntaxParser *parser, Span span);

/**
 * @brief Parses a single-quoted character literal.
 * @param parser Parsing context.
 * @param span Where the construct starts.
 * @return Parse outcome; see SyntaxNodeResult.
 */
SyntaxNodeResult parse_rune_lit_expr(const SyntaxParser *parser, Span span);

/**
 * @brief Parses a double-quoted string literal.
 * @param parser Parsing context.
 * @param span Where the construct starts.
 * @return Parse outcome; see SyntaxNodeResult.
 */
SyntaxNodeResult parse_string_lit_expr(const SyntaxParser *parser, Span span);

/**
 * @brief Parses `type { [field, ...] }`.
 * @param parser Parsing context.
 * @param span Where the construct starts.
 * @return Parse outcome; see SyntaxNodeResult.
 */
SyntaxNodeResult parse_struct_lit_expr(const SyntaxParser *parser, Span span);

/**
 * @brief Parses an array type followed by `{ [element, ...] }`.
 * @param parser Parsing context.
 * @param span Where the construct starts.
 * @return Parse outcome; see SyntaxNodeResult.
 */
SyntaxNodeResult parse_array_lit_expr(const SyntaxParser *parser, Span span);

/**
 * @brief Parses `(expr)`; the parens are transparent, the inner node
 *        surfaces unchanged.
 * @param parser Parsing context.
 * @param span Where the construct starts.
 * @return Parse outcome; see SyntaxNodeResult.
 */
SyntaxNodeResult parse_sub_expr(const SyntaxParser *parser, Span span);

#pragma endregion
