# Registers one CTest entry per unit-test case so failures are reported
# individually. Case names must stay in sync with the TestEntry tables in
# the corresponding test_*.c sources.

set(COMMON_TEST_MODULES strview xmem span source)

foreach(module IN LISTS COMMON_TEST_MODULES)
  add_executable(test_${module} test_${module}.c)
  target_link_libraries(test_${module} PRIVATE solid-lang-common)
  # The MSVC CRT deprecates standard functions (e.g. tmpfile) that the
  # tests use deliberately; silence those diagnostics only for tests.
  target_compile_definitions(test_${module} PRIVATE _CRT_SECURE_NO_WARNINGS)
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
                 destroy from_file from_file_bom_and_empty)
foreach(case_name IN LISTS SOURCE_CASES)
  add_test(NAME common.source.${case_name} COMMAND test_source ${case_name})
endforeach()
