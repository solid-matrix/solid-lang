#pragma once

#include "syntax_parser.h"
#include "syntax_result.h"

#pragma region AUXILLIARY

typedef SyntaxNodeResult (*SyntaxFieldFn)(const SyntaxParser *, Span);

bool is_letter_or_underscore(uint8_t c);

bool is_letter_digit_or_underscore(uint8_t c);

bool is_decimal_digit(uint8_t c);

bool is_binary_digit(uint8_t c);

bool is_octal_digit(uint8_t c);

bool is_hex_digit(uint8_t c);

bool is_base_digit(uint8_t c, int base);

bool is_whitespace(uint8_t c);

Span skip_trivia(const Source *source, Span span);

Span span_consumed(Span span, Span rem);

SyntaxNodeResult complete_longest_match(SyntaxNodeResult *results, size_t count);

SyntaxListResult parse_expr_list(const SyntaxParser *parser, Span span, Strview separator);

SyntaxListResult parse_identifier_list(const SyntaxParser *parser, Span span, Strview separator);

SyntaxListResult parse_generic_param_list(const SyntaxParser *parser, Span span);

SyntaxListResult parse_call_param_list(const SyntaxParser *parser, Span span);

SyntaxListResult parse_generic_arg_list(const SyntaxParser *parser, Span span);

SyntaxListResult parse_field_list(const SyntaxParser *parser, Span span, SyntaxFieldFn parse_field,
                                  SyntaxErrorCode missing_code);

SyntaxMatchResult match_keyword(const Source *source, Span span, Strview keyword);

SyntaxMatchResult match(const Source *source, Span span, Strview strview);

SyntaxMatchResult match_escape(const SyntaxParser *parser, Span span);

SyntaxMatchResult match_utf8_char(const SyntaxParser *parser, Span span);

#pragma endregion

#pragma region COMMON

SyntaxNodeResult parse_identifier(const SyntaxParser *parser, Span span);

SyntaxNodeResult parse_compile_time(const SyntaxParser *parser, Span span);

SyntaxListResult parse_annotations(const SyntaxParser *parser, Span span);

SyntaxNodeResult parse_generic_param(const SyntaxParser *parser, Span span);

SyntaxNodeResult parse_call_param(const SyntaxParser *parser, Span span);

SyntaxNodeResult parse_generic_arg(const SyntaxParser *parser, Span span);

SyntaxNodeResult parse_program(const SyntaxParser *parser, Span span);

SyntaxNodeResult parse_named_type(const SyntaxParser *parser, Span span);

SyntaxNodeResult parse_named_expr(const SyntaxParser *parser, Span span);

#pragma endregion

#pragma region TYPE

SyntaxNodeResult parse_type(const SyntaxParser *parser, Span span);

SyntaxNodeResult parse_ref_type(const SyntaxParser *parser, Span span);

SyntaxNodeResult parse_array_type(const SyntaxParser *parser, Span span);

SyntaxNodeResult parse_func_type(const SyntaxParser *parser, Span span);

#pragma endregion

#pragma region DECL

SyntaxNodeResult parse_decl(const SyntaxParser *parser, Span span);

SyntaxNodeResult parse_namespace_decl(const SyntaxParser *parser, Span span);

SyntaxNodeResult parse_using_decl(const SyntaxParser *parser, Span span);

SyntaxNodeResult parse_let_decl(const SyntaxParser *parser, Span span);

SyntaxNodeResult parse_struct_decl(const SyntaxParser *parser, Span span);

SyntaxNodeResult parse_enum_decl(const SyntaxParser *parser, Span span);

SyntaxNodeResult parse_union_decl(const SyntaxParser *parser, Span span);

SyntaxNodeResult parse_variant_decl(const SyntaxParser *parser, Span span);

SyntaxNodeResult parse_contract_decl(const SyntaxParser *parser, Span span);

SyntaxNodeResult parse_func_decl(const SyntaxParser *parser, Span span);

#pragma endregion

#pragma region STMT

SyntaxNodeResult parse_stmt(const SyntaxParser *parser, Span span);

SyntaxNodeResult parse_empty_stmt(const SyntaxParser *parser, Span span);

SyntaxNodeResult parse_body_stmt(const SyntaxParser *parser, Span span);

SyntaxNodeResult parse_let_stmt(const SyntaxParser *parser, Span span);

SyntaxNodeResult parse_set_stmt(const SyntaxParser *parser, Span span);

SyntaxNodeResult parse_expr_stmt(const SyntaxParser *parser, Span span);

SyntaxNodeResult parse_if_stmt(const SyntaxParser *parser, Span span);

SyntaxNodeResult parse_loop_stmt(const SyntaxParser *parser, Span span);

SyntaxNodeResult parse_break_stmt(const SyntaxParser *parser, Span span);

SyntaxNodeResult parse_continue_stmt(const SyntaxParser *parser, Span span);

SyntaxNodeResult parse_return_stmt(const SyntaxParser *parser, Span span);

SyntaxNodeResult parse_while_stmt(const SyntaxParser *parser, Span span);

#pragma endregion

#pragma region EXPR

SyntaxNodeResult parse_expr(const SyntaxParser *parser, Span span);

SyntaxNodeResult parse_logical_or_expr(const SyntaxParser *parser, Span span);

SyntaxNodeResult parse_logical_xor_expr(const SyntaxParser *parser, Span span);

SyntaxNodeResult parse_logical_and_expr(const SyntaxParser *parser, Span span);

SyntaxNodeResult parse_relational_expr(const SyntaxParser *parser, Span span);

SyntaxNodeResult parse_bitwise_expr(const SyntaxParser *parser, Span span);

SyntaxNodeResult parse_shift_expr(const SyntaxParser *parser, Span span);

SyntaxNodeResult parse_additive_expr(const SyntaxParser *parser, Span span);

SyntaxNodeResult parse_multiplicative_expr(const SyntaxParser *parser, Span span);

SyntaxNodeResult parse_unary_expr(const SyntaxParser *parser, Span span);

SyntaxNodeResult parse_postfix_expr(const SyntaxParser *parser, Span span);

SyntaxNodeResult parse_primary_expr(const SyntaxParser *parser, Span span);

SyntaxNodeResult parse_int_lit_expr(const SyntaxParser *parser, Span span);

SyntaxNodeResult parse_float_lit_expr(const SyntaxParser *parser, Span span);

SyntaxNodeResult parse_rune_lit_expr(const SyntaxParser *parser, Span span);

SyntaxNodeResult parse_string_lit_expr(const SyntaxParser *parser, Span span);

SyntaxNodeResult parse_struct_lit_expr(const SyntaxParser *parser, Span span);

SyntaxNodeResult parse_array_lit_expr(const SyntaxParser *parser, Span span);

SyntaxNodeResult parse_sub_expr(const SyntaxParser *parser, Span span);

#pragma endregion
