# One CTest entry per unit-test case; case names must stay in sync with
# the TestDispatchEntry table in test_symboltable.c.

add_executable(test_symboltable test_symboltable.c)
target_link_libraries(test_symboltable PRIVATE solid-lang-semantic
                                               solid-lang-semantic-tests-support)

set(SYMBOLTABLE_CASES
    create_empty_lookup_miss define_and_lookup_hit
    define_duplicate_returns_null_table_unchanged define_persistence_old_table_unchanged
    define_nested_path_scoping collision_symbol_and_namespace namespace_materializes_empty
    namespace_path_through_symbol_fails namespace_redefine_reuses_entry
    sub_view_lookup_probes sub_unknown_segment_returns_null sub_rejects_symbol_leaf
    sub_chained_views define_many_orders_all_lookups deterministic_behavior_across_orders)

foreach(case_name IN LISTS SYMBOLTABLE_CASES)
  add_test(NAME semantic.symboltable.${case_name} COMMAND test_symboltable ${case_name})
endforeach()

# Executables carry a semantic_ prefix when the bare module name would
# collide with a syntax test target (test_error, test_errorlist).
add_executable(test_semantic_error test_error.c)
target_link_libraries(test_semantic_error PRIVATE solid-lang-semantic
                                                  solid-lang-semantic-tests-support)

set(ERROR_CASES error_carries_code_and_span)
foreach(case_name IN LISTS ERROR_CASES)
  add_test(NAME semantic.error.${case_name} COMMAND test_semantic_error ${case_name})
endforeach()

add_executable(test_semantic_errorlist test_errorlist.c)
target_link_libraries(test_semantic_errorlist PRIVATE solid-lang-semantic
                                                      solid-lang-semantic-tests-support)

set(ERRORLIST_CASES errorlist_empty errorlist_from_array errorlist_persistence errorlist_reverse
    errorlist_concat)
foreach(case_name IN LISTS ERRORLIST_CASES)
  add_test(NAME semantic.errorlist.${case_name} COMMAND test_semantic_errorlist ${case_name})
endforeach()

add_executable(test_namepath test_namepath.c)
target_link_libraries(test_namepath PRIVATE solid-lang-semantic
                                            solid-lang-semantic-tests-support)

set(NAMEPATH_CASES namepath_empty namepath_from_array namepath_persistence namepath_reverse
    namepath_concat)
foreach(case_name IN LISTS NAMEPATH_CASES)
  add_test(NAME semantic.namepath.${case_name} COMMAND test_namepath ${case_name})
endforeach()

add_executable(test_collect test_collect.c)
target_link_libraries(test_collect PRIVATE solid-lang-semantic
                                           solid-lang-semantic-tests-support)

set(COLLECT_CASES
    symbols_at_root_and_namespaced namespace_context_from_prologue nested_namespace_segments
    two_files_merge_namespace distinct_modules_no_collision symbol_redefined_reports_and_keeps_first
    clash_symbol_then_namespace clash_namespace_then_symbol enum_fields_not_registered
    all_decl_kinds_defined using_declarations_skipped errors_newest_first_order_contract)

foreach(case_name IN LISTS COLLECT_CASES)
  add_test(NAME semantic.collect.${case_name} COMMAND test_collect ${case_name})
endforeach()
