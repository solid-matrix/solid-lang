# One CTest entry per unit-test case; case names must stay in sync with
# the TestDispatchEntry tables in the corresponding test_*.c sources.
# Test files mirror syntax/src one-to-one.

set(SYNTAX_TEST_MODULES
    parse
    parse_primary
    parse_shared
    parser
    parser_result
    syntax_error
    syntax_node)

foreach(module IN LISTS SYNTAX_TEST_MODULES)
  add_executable(test_${module} test_${module}.c)
  target_link_libraries(test_${module} PRIVATE solid-lang-syntax
                                               solid-lang-syntax-tests-support)
endforeach()

set(PARSE_CASES
    identifier_basic identifier_stops_at_non_word identifier_rejects_digit_start
    identifier_list_single identifier_list_multi_and_trivia
    identifier_list_trailing_separator_reports_identifier
    identifier_list_missing_first_is_silent
    namespace_decl_valid_forms using_decl_valid_forms decl_keyword_boundary
    namespace_decl_malforms using_decl_malforms
    named_type_single_and_path named_type_trivia_between_segments
    named_type_not_match_on_non_identifier
    ref_type_readwrite_named_inner ref_type_readonly_and_writeonly
    ref_type_keyword_word_boundary ref_type_nested_and_trivia
    ref_type_missing_inner_reports_type ref_type_not_match_without_amp
    type_dispatch_prefers_ref_for_amp
    program_accumulates_decls_newest_first
    program_junk_tail_reports_expected_eof program_empty_and_trivia_only
    empty_stmt_bare body_stmt_empty_and_stmts body_stmt_nested
    body_stmt_trivia body_stmt_missing_rbrace_frame
    let_stmt_typed_and_inferred let_stmt_malforms set_stmt_basic
    set_stmt_malforms expr_stmt_basic expr_stmt_missing_semicolon_frame
    break_continue_stmts return_with_and_without_expr return_malform
    if_else_if_chain if_empty_bodies if_missing_body_reports_body
    if_struct_lit_condition if_missing_lparen
    loop_stmt while_stmt stmt_keyword_boundaries stmt_dispatch_ladder
    annotations_none annotations_single_and_multi annotations_error_frame
    generic_param_bare_and_bound generic_param_annotated
    call_param_requires_colon_and_type call_param_annotated
    let_decl_three_forms let_decl_malforms
    struct_decl_bare_and_empty_body struct_decl_fields_and_trailing_comma
    struct_decl_generics struct_decl_generics_malforms struct_field_frames
    struct_decl_body_malforms union_decl_forms
    enum_decl_forms enum_decl_trailing_comma enum_decl_field_malform
    variant_decl_forms
    contract_decl_forms contract_decl_malforms
    func_decl_full_ladder func_decl_body_forms
    func_decl_callconv_and_fulfills program_func_sample_end_to_end
    array_type_forms array_type_malform func_type_forms
    func_type_word_boundary func_type_malform struct_lit_forms
    struct_lit_not_a_literal struct_lit_field_frame array_lit_forms
    array_lit_malform
    expr_precedence_mul_over_add expr_parens_override_grouping
    expr_left_associativity expr_unary_binds_tighter_than_mul
    expr_all_unary_operators expr_postfix_chain_on_literal
    expr_call_empty_args expr_float_dot_beats_member_access
    expr_malformed_missing_right_hand_side expr_malformed_unclosed_paren
    expr_malformed_missing_rhs_logical_or expr_malformed_dangling_call_comma
    expr_malformed_empty_parens expr_malformed_dangling_unary expr_relational_two_byte_forms
    expr_call_args_reverse_order expr_dot_missing_identifier_frame
    expr_index_frames expr_call_missing_rparen_frame ct_bare
    ct_with_args ct_multi_string_args_reversed ct_missing_name_frame
    ct_unclosed_args_frame expr_ct_operand_position expr_full_ladder_shape)
foreach(case_name IN LISTS PARSE_CASES)
  add_test(NAME parser.parse.${case_name} COMMAND test_parse ${case_name})
endforeach()

set(PRIMARY_CASES
    int_decimal int_base_prefixed malformed_base_prefixes float_exponent
    float_dot splits_int splits_float boundaries_number
    rune_simple_and_numeric rune_malformed rune_not_match
    string_simple_and_escapes string_malformed string_not_match)
foreach(case_name IN LISTS PRIMARY_CASES)
  add_test(NAME parser.parse_primary.${case_name}
           COMMAND test_parse_primary ${case_name})
endforeach()

set(SHARED_CASES
    digit_classes letter_classes whitespace_class skip_trivia_consumes_runs
    skip_trivia_on_code_is_noop skip_trivia_at_end
    span_consumed_measures_progress match_keyword_positive_and_negative
    complete_longest_match_picks_furthest_rem)
foreach(case_name IN LISTS SHARED_CASES)
  add_test(NAME parser.parse_shared.${case_name}
           COMMAND test_parse_shared ${case_name})
endforeach()

set(PARSER_CASES create_returns_usable_parser create_distinct_instances
                 destroy_cycle_is_repeatable)
foreach(case_name IN LISTS PARSER_CASES)
  add_test(NAME parser.parser.${case_name} COMMAND test_parser ${case_name})
endforeach()

set(RESULT_CASES matched_carries_fields not_match_resets_everything
                 is_ok_requires_match_and_silence)
foreach(case_name IN LISTS RESULT_CASES)
  add_test(NAME parser.parser_result.${case_name}
           COMMAND test_parser_result ${case_name})
endforeach()

set(SYNTAX_ERROR_CASES error_carries_code_and_span errorlist_empty
                       errorlist_from_array errorlist_persistence
                       errorlist_reverse errorlist_concat)
foreach(case_name IN LISTS SYNTAX_ERROR_CASES)
  add_test(NAME parser.syntax_error.${case_name}
           COMMAND test_syntax_error ${case_name})
endforeach()

set(SYNTAX_NODE_CASES header_carries_kind_and_span nodelist_empty
                      nodelist_from_array nodelist_persistence nodelist_tail
                      nodelist_reverse nodelist_concat nodelist_length)
foreach(case_name IN LISTS SYNTAX_NODE_CASES)
  add_test(NAME parser.syntax_node.${case_name}
           COMMAND test_syntax_node ${case_name})
endforeach()
