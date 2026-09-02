#include "internal.h"
#include "semantic_analyze.h"
#include "semantic_error.h"

// TODO
SemanticAnalyzeResult semantic_analyze(Arena *arena, const SemanticModuleList *modules,
                                       const SemanticParamList *params) {

  SemanticAnalyzer analyzer = {.arena = arena, .modules = modules, .params = params};
  SemanticErrorList *errors = semantic_errorlist_empty();

  // pass 1: collect
  SemanticCollectResult collect = semantic_collect(&analyzer);
  errors = semantic_errorlist_concat(arena, collect.errors, errors);
  SemanticSymbolTable *symbol_table = collect.symbol_table;
  SemanticNamePathTable *namepath_table = collect.namepath_table;

  // pass 2: resolve
  SemanticResolveResult resolve = semantic_resolve(&analyzer, symbol_table);
  errors = semantic_errorlist_concat(arena, resolve.errors, errors);
  SemanticBindingTable *binding_table = resolve.binding_table;

  // pass 3: check
  SemanticCheckResult check = semantic_check(&analyzer, symbol_table, namepath_table, binding_table);
  errors = semantic_errorlist_concat(arena, check.errors, errors);

  return (SemanticAnalyzeResult){
      .errors = semantic_errorlist_reverse(arena, errors),
  };
}