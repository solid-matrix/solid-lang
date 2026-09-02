# One CTest entry per unit-test case; case names must stay in sync with
# the TestDispatchEntry tables in the corresponding test_*.c sources.
# Test files mirror common/src one-to-one.

set(COMMON_TEST_MODULES arena strview xmem span source)

add_library(solid-lang-test-support INTERFACE)
target_include_directories(solid-lang-test-support
                           INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(solid-lang-test-support INTERFACE unity::framework)

foreach(module IN LISTS COMMON_TEST_MODULES)
  add_executable(test_${module} test_${module}.c)
  target_link_libraries(test_${module} PRIVATE solid-lang-common
                                               solid-lang-test-support)
  # The MSVC CRT deprecates standard functions (e.g. tmpfile) that the
  # tests use deliberately; silence those diagnostics only for tests.
  target_compile_definitions(test_${module} PRIVATE _CRT_SECURE_NO_WARNINGS)
endforeach()

set(ARENA_CASES alloc_distinct_and_writable realloc_grow_preserves_content
                realloc_null_is_alloc destroy_cycles destroy_null_safe)
foreach(case_name IN LISTS ARENA_CASES)
  add_test(NAME common.arena.${case_name} COMMAND test_arena ${case_name})
endforeach()

set(STRVIEW_CASES
    create_macro create_functions is_empty equals compare starts_with
    ends_with slice byte_at write copy)
foreach(case_name IN LISTS STRVIEW_CASES)
  add_test(NAME common.strview.${case_name}
           COMMAND test_strview ${case_name})
endforeach()

set(XMEM_CASES malloc calloc_zeroes realloc_grow realloc_shrink
               realloc_null_is_malloc free_null_noop)
foreach(case_name IN LISTS XMEM_CASES)
  add_test(NAME common.xmem.${case_name} COMMAND test_xmem ${case_name})
endforeach()

set(SPAN_CASES empty len_and_is_empty slice)
foreach(case_name IN LISTS SPAN_CASES)
  add_test(NAME common.span.${case_name} COMMAND test_span ${case_name})
endforeach()

set(SOURCE_CASES lines_lf lines_crlf lines_lone_cr lines_mixed
                 empty_source bom_stripped get_position accessors owns_bytes
                 destroy_cycles from_file from_file_bom_and_empty)
foreach(case_name IN LISTS SOURCE_CASES)
  add_test(NAME common.source.${case_name} COMMAND test_source ${case_name})
endforeach()

