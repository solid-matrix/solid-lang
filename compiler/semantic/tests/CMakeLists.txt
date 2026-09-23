# One CTest entry per unit-test case; case names must stay in sync with
# the TestDispatchEntry table in test_symbol_table.c.

add_executable(test_symbol_table test_symbol_table.c)
target_link_libraries(test_symbol_table PRIVATE solid-lang-semantic
                                                solid-lang-semantic-tests-support)

set(SYMBOLTABLE_CASES
    empty_table_lookup_miss insert_into_null_grows define_and_lookup_hit
    define_duplicate_returns_null_table_unchanged define_persistence_old_table_unchanged
    define_nested_path_scoping collision_symbol_and_namespace namespace_materializes_empty
    namespace_path_through_symbol_fails namespace_redefine_reuses_entry
    sub_view_lookup_probes sub_unknown_segment_returns_null sub_rejects_symbol_leaf
    sub_chained_views subtable_empty_path_returns_table contains_accepts_namespace_and_symbol
    length_counts_level_entries
    define_many_orders_all_lookups deterministic_behavior_across_orders)

foreach(case_name IN LISTS SYMBOLTABLE_CASES)
  add_test(NAME semantic.symbol_table.${case_name} COMMAND test_symbol_table ${case_name})
endforeach()

add_executable(test_namepath_table test_namepath_table.c)
target_link_libraries(test_namepath_table PRIVATE solid-lang-semantic
                                                  solid-lang-semantic-tests-support)

set(NAMEPATH_TABLE_CASES
    empty_table_lookup_miss insert_into_null_grows insert_and_lookup_hit
    insert_persistence_old_table_unchanged lookup_unknown_decl_miss
    multiple_entries_all_found)

foreach(case_name IN LISTS NAMEPATH_TABLE_CASES)
  add_test(NAME semantic.namepath_table.${case_name} COMMAND test_namepath_table ${case_name})
endforeach()

add_executable(test_binding_table test_binding_table.c)
target_link_libraries(test_binding_table PRIVATE solid-lang-semantic
                                                 solid-lang-semantic-tests-support)

set(BINDING_TABLE_CASES
    empty_table_lookup_miss insert_into_null_grows insert_and_lookup_hit
    insert_persistence_old_table_unchanged lookup_unknown_site_miss
    multiple_entries_all_found)

foreach(case_name IN LISTS BINDING_TABLE_CASES)
  add_test(NAME semantic.binding_table.${case_name} COMMAND test_binding_table ${case_name})
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
    namepath_concat namepath_predicates)
foreach(case_name IN LISTS NAMEPATH_CASES)
  add_test(NAME semantic.namepath.${case_name} COMMAND test_namepath ${case_name})
endforeach()

add_executable(test_resolve test_resolve.c)
target_link_libraries(test_resolve PRIVATE solid-lang-semantic
                                           solid-lang-semantic-tests-support)

set(RESOLVE_CASES
    types_resolved forward_reference_across_files
    using_import_bare_name using_import_namespace_skips_prefix using_target_not_namespace
    qualified_member_binds_enum_field member_unknown_reports_no_member
    qualified_path_world_absolute param_shadows_global_symbol
    duplicate_generic_params duplicate_call_params duplicate_fields
    fulfills_binds_contract generic_arity_mismatch unknown_name_anchored
    using_name_ambiguity_reports_at_use duplicate_using_is_harmless
    param_shadows_generic_param_reports outer_block_let_vs_param_reports
    nested_block_shadows_param implicit_core_merge)

foreach(case_name IN LISTS RESOLVE_CASES)
  add_test(NAME semantic.resolve.${case_name} COMMAND test_resolve ${case_name})
endforeach()

add_executable(test_collect test_collect.c)
target_link_libraries(test_collect PRIVATE solid-lang-semantic
                                           solid-lang-semantic-tests-support)

set(COLLECT_CASES
    symbols_at_root_and_namespaced namespace_context_from_prologue nested_namespace_segments
    two_files_merge_namespace distinct_modules_no_collision symbol_redefined_reports_and_keeps_first
    clash_symbol_then_namespace clash_namespace_then_symbol enum_fields_not_registered
    all_decl_kinds_defined using_declarations_skipped reverse_table_maps_symbols_to_paths
    reverse_table_skips_redefined_symbols reverse_table_excludes_namespaces
    errors_newest_first_order_contract)

foreach(case_name IN LISTS COLLECT_CASES)
  add_test(NAME semantic.collect.${case_name} COMMAND test_collect ${case_name})
endforeach()

# add_executable(test_resolve test_resolve.c)
# target_link_libraries(test_resolve PRIVATE solid-lang-semantic
#                                            solid-lang-semantic-tests-support)

# set(RESOLVE_CASES
#     types_resolved forward_reference_across_files using_import_bare_name
#     using_import_namespace_skips_prefix using_target_not_namespace
#     qualified_member_binds_enum_field member_unknown_reports_no_member
#     qualified_path_world_absolute param_shadows_global_symbol duplicate_generic_params
#     duplicate_call_params duplicate_fields fulfills_binds_contract
#     generic_arity_mismatch unknown_name_anchored using_name_conflict_reports
#     duplicate_using_conflicts param_shadows_generic_param_reports
#     outer_block_let_vs_param_reports nested_block_shadows_param implicit_core_merge)

# foreach(case_name IN LISTS RESOLVE_CASES)
#   add_test(NAME semantic.resolve.${case_name} COMMAND test_resolve ${case_name})
# endforeach()

add_executable(test_registry test_registry.c)
target_link_libraries(test_registry PRIVATE solid-lang-semantic
                                            solid-lang-semantic-tests-support)

set(REGISTRY_CASES registry_names_root_and_namespaced registry_two_files_merge_namespace
    registry_distinct_packages_prefix registry_core_prelude_bare registry_duplicate_keeps_first
    registry_kinds_classification registry_intrinsic_outside_core_diag
    registry_lookup_ambiguous registry_lookup_miss
    registry_guards_collected registry_knob_values)
foreach(case_name IN LISTS REGISTRY_CASES)
  add_test(NAME semantic.registry.${case_name} COMMAND test_registry ${case_name})
endforeach()

add_executable(test_literal test_literal.c)
target_link_libraries(test_literal PRIVATE solid-lang-semantic
                                           solid-lang-semantic-tests-support)

set(LITERAL_CASES literal_int_type_matrix literal_int_bases_and_separators
    literal_int_unsuffixed_default literal_int_range_signed literal_int_range_unsigned
    literal_int_negate_forms literal_int_suffix_flag literal_string_content_escapes
    literal_rune_scalars)
foreach(case_name IN LISTS LITERAL_CASES)
  add_test(NAME semantic.literal.${case_name} COMMAND test_literal ${case_name})
endforeach()

add_executable(test_const_fold test_const_fold.c)
target_link_libraries(test_const_fold PRIVATE solid-lang-semantic
                                              solid-lang-semantic-tests-support)

set(CONST_FOLD_CASES fold_chain_int fold_cycle_diagnostic fold_fp_unfoldable
    fold_layout_deferred fold_symbolic_and_address_alias fold_string_flags
    fold_div_zero_diag fold_unknown_name_deferred fold_knob_value
    fold_core_builtins fold_core_builtin_shadowing)
foreach(case_name IN LISTS CONST_FOLD_CASES)
  add_test(NAME semantic.const_fold.${case_name} COMMAND test_const_fold ${case_name})
endforeach()

add_executable(test_gate test_gate.c)
target_link_libraries(test_gate PRIVATE solid-lang-semantic
                                        solid-lang-semantic-tests-support)

set(GATE_CASES gate_flag_selects_declaration gate_guard_on_all_kinds
    gate_condition_must_be_bool gate_unknown_name_diag gate_symbolic_and_layout_banned
    gate_self_reference_cycle gate_mutual_gating_cycle gate_transitive_cycle
    gate_disjoint_alternatives_legal gate_feature_let_forbidden
    gate_when_true_literal
    gate_grammar_rejects_unsuffixed_literal gate_grammar_rejects_operators_outside_the_set
    gate_grammar_rejects_non_integer_literals gate_grammar_accepts_the_closed_set
    gate_guard_without_condition_diag
    gate_nested_positions_forbidden gate_nested_custom_annotations_allowed)
foreach(case_name IN LISTS GATE_CASES)
  add_test(NAME semantic.gate.${case_name} COMMAND test_gate ${case_name})
endforeach()

add_executable(test_prune test_prune.c)
target_link_libraries(test_prune PRIVATE solid-lang-semantic
                                         solid-lang-semantic-tests-support)

set(PRUNE_CASES prune_drops_pruned_declaration prune_pruned_decl_never_resolved
    prune_surviving_alternative_is_collected prune_both_surviving_alternatives_collide)
foreach(case_name IN LISTS PRUNE_CASES)
  add_test(NAME semantic.prune.${case_name} COMMAND test_prune ${case_name})
endforeach()

add_executable(test_const_eval test_const_eval.c)
target_link_libraries(test_const_eval PRIVATE solid-lang-semantic
                                              solid-lang-semantic-tests-support)

set(CONST_EVAL_CASES
    eval_int_arithmetic_wraps eval_int_div_semantics eval_div_zero_diag
    eval_div_overflow_diag eval_shift_semantics eval_bitwise_and_comparisons
    eval_bool_logic eval_string_and_rune_eq eval_type_mismatches_diag
    eval_unary_int_forms)
foreach(case_name IN LISTS CONST_EVAL_CASES)
  add_test(NAME semantic.const_eval.${case_name} COMMAND test_const_eval ${case_name})
endforeach()
