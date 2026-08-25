# One CTest entry per unit-test case; case names must stay in sync with
# the TestDispatchEntry tables in the corresponding test_*.c sources.
# Test files mirror parser/src one-to-one.

set(PARSER_TEST_MODULES
    parse
    parse_primary
    parse_shared
    parser
    parser_result
    syntax_error
    syntax_node)

foreach(module IN LISTS PARSER_TEST_MODULES)
  add_executable(test_${module} test_${module}.c)
  target_link_libraries(test_${module} PRIVATE solid-lang-parser
                                               solid-lang-parser-tests-support)
endforeach()

# parse_shared.h is a private header (lives in src/); its mirror test
# may reach it through the private path.
target_include_directories(test_parse_shared
                           PRIVATE ${CMAKE_SOURCE_DIR}/parser/src)

set(PARSE_CASES
    identifier_basic identifier_stops_at_non_word identifier_rejects_digit_start
    name_path_single name_path_multi_and_trivia
    name_path_trailing_separator_reports_identifier
    name_path_missing_first_segment_is_silent_not_match
    namespace_decl_valid_forms using_decl_valid_forms decl_keyword_boundary
    namespace_decl_malforms using_decl_malforms
    program_accumulates_decls_newest_first
    program_junk_tail_reports_expected_eof program_empty_and_trivia_only)
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
