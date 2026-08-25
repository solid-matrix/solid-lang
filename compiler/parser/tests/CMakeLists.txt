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

add_executable(test_syntaxerrorlist test_syntaxerrorlist.c)
target_link_libraries(test_syntaxerrorlist PRIVATE solid-lang-parser
                                                   solid-lang-test-util)

set(ERRORLIST_CASES empty from_array persistence access reverse concat length)
foreach(case_name IN LISTS ERRORLIST_CASES)
  add_test(NAME parser.errorlist.${case_name}
           COMMAND test_syntaxerrorlist ${case_name})
endforeach()

add_executable(test_syntaxnodelist test_syntaxnodelist.c)
target_link_libraries(test_syntaxnodelist PRIVATE solid-lang-parser
                                                  solid-lang-test-util)

set(NODELIST_CASES empty from_array persistence access reverse concat length)
foreach(case_name IN LISTS NODELIST_CASES)
  add_test(NAME parser.nodelist.${case_name}
           COMMAND test_syntaxnodelist ${case_name})
endforeach()

add_executable(test_namepath test_namepath.c)
target_link_libraries(test_namepath PRIVATE solid-lang-parser
                                            solid-lang-test-util)

set(NAMEPATH_CASES single multi trivia trailing_sep trailing_sep_trivia no_first_segment)
foreach(case_name IN LISTS NAMEPATH_CASES)
  add_test(NAME parser.namepath.${case_name}
           COMMAND test_namepath ${case_name})
endforeach()
