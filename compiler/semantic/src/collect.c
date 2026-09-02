/**
 * @file collect.c
 * @brief Collect pass: gathers every module's top-level declarations into the world table.
 * @author solid-matrix
 */

#include <assert.h>
#include <stdbool.h>

#include "arena.h"
#include "internal.h"
#include "namepath.h"
#include "namepath_table.h"
#include "semantic_error.h"
#include "symbol_table.h"
#include "syntax_node.h"

// The declared name of a top-level declaration.
static Strview decl_name(SyntaxNode *decl) {
  switch (decl->kind) {
  case SYNTAX_KIND_LET_DECL:
    return ((SyntaxLetDecl *)decl)->id->value;
  case SYNTAX_KIND_STRUCT_DECL:
    return ((SyntaxStructDecl *)decl)->id->value;
  case SYNTAX_KIND_ENUM_DECL:
    return ((SyntaxEnumDecl *)decl)->id->value;
  case SYNTAX_KIND_UNION_DECL:
    return ((SyntaxUnionDecl *)decl)->id->value;
  case SYNTAX_KIND_VARIANT_DECL:
    return ((SyntaxVariantDecl *)decl)->id->value;
  case SYNTAX_KIND_CONTRACT_DECL:
    return ((SyntaxContractDecl *)decl)->id->value;
  case SYNTAX_KIND_FUNC_DECL:
    return ((SyntaxFuncDecl *)decl)->id->value;
  default:
    assert(!"top level holds a non-declaration node");
    return (Strview){0};
  }
}

// TODO(when): gate declarations on their @when annotations; every
// declaration is live until the gate lands.
static bool decl_enabled(const SyntaxNode *decl) {
  (void)decl;
  return true;
}

// Namespace declarations are relative to the module root: the prologue
// namespace is materialized into the table and extends the current context,
// declarations land under it. Using declarations belong to resolve; collect
// skips them. Every live symbol also lands in the reverse path table.
static SemanticCollectResult collect_program(Arena *arena, SemanticSymbolTable *symbols, SemanticNamePathTable *paths,
                                             const SemanticModule *module, SyntaxProgram *program) {

  if (program->top_levels == NULL)
    return (SemanticCollectResult){.errors = NULL, .symbol_table = symbols, .namepath_table = paths};

  SemanticErrorList *errors = semantic_errorlist_empty();

  SyntaxNodeList *decls = program->top_levels;
  SemanticNamePath *prefix = module->path;

  // process the optional topmost namespace declaration
  if (decls->head->kind == SYNTAX_KIND_NAMESPACE_DECL) {
    SyntaxNamespaceDecl *decl = (SyntaxNamespaceDecl *)decls->head;

    SemanticNamePath *ns = semantic_namepath_from_identifiers(arena, decl->path);
    prefix = semantic_namepath_concat(arena, prefix, ns);

    SemanticSymbolTable *defined = semantic_symbol_table_insert(arena, symbols, prefix, NULL);
    if (defined == NULL) {
      errors = semantic_errorlist_prepend(arena, errors,
                                          semantic_error_create(SEMANTIC_SYMBOL_NAMESPACE_CLASH, decl->header.span));
    } else {
      symbols = defined;
    }

    decls = decls->tail;
  }

  // skip the following consecutive using declarations
  while (decls != NULL && decls->head->kind == SYNTAX_KIND_USING_DECL)
    decls = decls->tail;

  for (SyntaxNodeList *it = decls; it != NULL; it = it->tail) {
    SyntaxNode *decl = it->head;

    if (!decl_enabled(decl))
      continue;

    SemanticNamePath *tail = semantic_namepath_prepend(arena, semantic_namepath_empty(), decl_name(decl));
    SemanticNamePath *path = semantic_namepath_concat(arena, prefix, tail);

    SemanticError error;
    if (semantic_symbol_table_lookup(symbols, path) != NULL) {
      error = semantic_error_create(SEMANTIC_SYMBOL_REDEFINED, decl->span);
      errors = semantic_errorlist_prepend(arena, errors, error);
      continue;
    }

    SemanticSymbolTable *defined = semantic_symbol_table_insert(arena, symbols, path, decl);
    if (defined == NULL) {
      error = semantic_error_create(SEMANTIC_SYMBOL_NAMESPACE_CLASH, decl->span);
      errors = semantic_errorlist_prepend(arena, errors, error);
      continue;
    }

    symbols = defined;
    paths = semantic_namepath_table_insert(arena, paths, decl, path);
  }

  return (SemanticCollectResult){.errors = errors, .symbol_table = symbols, .namepath_table = paths};
}

SemanticCollectResult semantic_collect(const SemanticAnalyzer *analyzer) {
  SemanticSymbolTable *symbols = semantic_symbol_table_empty();
  SemanticNamePathTable *paths = semantic_namepath_table_empty();
  SemanticErrorList *errors = semantic_errorlist_empty();

  for (const SemanticModuleList *it = analyzer->modules; it != NULL; it = it->next)
    for (SemanticProgramList *unit = it->module->programs; unit != NULL; unit = unit->next) {
      SemanticCollectResult res = collect_program(analyzer->arena, symbols, paths, it->module, unit->program);

      symbols = res.symbol_table;
      paths = res.namepath_table;
      errors = semantic_errorlist_concat(analyzer->arena, res.errors, errors);
    }

  return (SemanticCollectResult){.symbol_table = symbols, .namepath_table = paths, .errors = errors};
}
