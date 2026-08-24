add_executable(test_parser test_parser.c)
target_link_libraries(test_parser PRIVATE solid-lang-parser
                                          solid-lang-test-util)

set(PARSER_CASES int_valid float_valid malformed_prefixes splits boundaries)
foreach(case_name IN LISTS PARSER_CASES)
  add_test(NAME parser.${case_name} COMMAND test_parser ${case_name})
endforeach()

add_executable(test_rune test_rune.c)
target_link_libraries(test_rune PRIVATE solid-lang-parser
                                        solid-lang-test-util)

set(RUNE_CASES valid invalid boundaries)
foreach(case_name IN LISTS RUNE_CASES)
  add_test(NAME parser.rune.${case_name} COMMAND test_rune ${case_name})
endforeach()

add_executable(test_string test_string.c)
target_link_libraries(test_string PRIVATE solid-lang-parser
                                          solid-lang-test-util)

set(STRING_CASES valid invalid boundaries)
foreach(case_name IN LISTS STRING_CASES)
  add_test(NAME parser.string.${case_name} COMMAND test_string ${case_name})
endforeach()

add_executable(test_decl test_decl.c)
target_link_libraries(test_decl PRIVATE solid-lang-parser
                                        solid-lang-test-util)

set(DECL_CASES namespace using malformed boundaries)
foreach(case_name IN LISTS DECL_CASES)
  add_test(NAME parser.decl.${case_name} COMMAND test_decl ${case_name})
endforeach()
