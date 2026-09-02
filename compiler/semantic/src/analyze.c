#include "internal.h"
#include "semantic_analyze.h"
#include "semantic_error.h"

// TODO
SemanticAnalyzeResult semantic_analyze(Arena *arena, const SemanticModuleList *modules,
                                       const SemanticParamList *params) {

  SemanticAnalyzer analyzer = {.arena = arena, .modules = modules, .params = params};
  SemanticErrorList *errors = semantic_errorlist_empty();

  // pass 1: collect
  SemanticCollectResult cres = semantic_collect(&analyzer);
  errors = semantic_errorlist_concat(arena, cres.errors, errors);
  SemanticSymbolTable *symbol_table = cres.symbol_table;
  SemanticNamePathTable *namepath_table = cres.namepath_table;

  // pass 2: resolve
  SemanticResolveResult rres = semantic_resolve(&analyzer, symbol_table);
  errors = semantic_errorlist_concat(arena, rres.errors, errors);
  SemanticBindingTable *binding_table = rres.binding_table;

  return (SemanticAnalyzeResult){
      .errors = semantic_errorlist_reverse(arena, errors),
  };
}