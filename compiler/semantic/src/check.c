#include "internal.h"
#include "semantic_error.h"

SemanticCheckResult semantic_check(const SemanticAnalyzer *analyzer, const SemanticSymbolTable *symbol_table,
                                   const SemanticNamePathTable *namepath_table,
                                   const SemanticBindingTable *binding_table) {

  SemanticErrorList *errors = semantic_errorlist_empty();

  return (SemanticCheckResult){.errors = errors};
}