add_executable(test_parser test_parser.c)
target_link_libraries(test_parser PRIVATE solid-lang-parser
                                          solid-lang-test-util)

set(PARSER_CASES int_valid float_valid malformed_prefixes splits boundaries)
foreach(case_name IN LISTS PARSER_CASES)
  add_test(NAME parser.${case_name} COMMAND test_parser ${case_name})
endforeach()
