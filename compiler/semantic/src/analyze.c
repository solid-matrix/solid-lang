#include "collect.h"
#include "error.h"
#include "semantic_analyze.h"
#include "semantic_error.h"

// TODO
SemanticAnalyzeResult semantic_analyze(Arena *arena, const SemanticModuleList *modules,
                                       const SemanticParamList *params) {

  SemanticErrorList *errors = semantic_errorlist_empty();

  // pass 1: collect
  SemanticCollectResult cres = semantic_collect(arena, modules, params);
  errors = semantic_errorlist_concat(arena, cres.errors, errors);
  SemanticSymbolTable *symbol_table = cres.symbols;

  // TODO

  return (SemanticAnalyzeResult){
      .errors = semantic_errorlist_reverse(arena, errors),
  };
}