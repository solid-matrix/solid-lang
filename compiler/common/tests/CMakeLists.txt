# Tests for the common subproject: one executable per test, registered
# with CTest. Run with: ctest --test-dir out/build/x64-debug
# (or ctest -R common to run only this subproject).
add_executable(test_string_view test_string_view.c)
target_link_libraries(test_string_view PRIVATE solid-lang-common)
add_test(NAME common.string_view COMMAND test_string_view)

add_executable(test_arena test_arena.c)
target_link_libraries(test_arena PRIVATE solid-lang-common)
add_test(NAME common.arena COMMAND test_arena)

add_executable(test_source test_source.c)
target_link_libraries(test_source PRIVATE solid-lang-common)
add_test(NAME common.source COMMAND test_source)

add_executable(test_span test_span.c)
target_link_libraries(test_span PRIVATE solid-lang-common)
add_test(NAME common.span COMMAND test_span)
