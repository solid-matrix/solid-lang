add_executable(test_ast test_ast.c)
target_link_libraries(test_ast PRIVATE solid-lang-parser)
add_test(NAME parser.ast COMMAND test_ast)
