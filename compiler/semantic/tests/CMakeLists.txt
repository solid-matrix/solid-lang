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
