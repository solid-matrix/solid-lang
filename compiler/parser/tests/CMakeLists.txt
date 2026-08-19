# Tests for the parser subproject: AST helpers.
# Registered with CTest. Run with: ctest --test-dir out/build/x64-debug -R parser
add_executable(test_ast test_ast.c)
target_link_libraries(test_ast PRIVATE solid-lang-parser)
add_test(NAME parser.ast COMMAND test_ast)
