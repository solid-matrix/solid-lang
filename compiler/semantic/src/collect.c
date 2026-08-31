/**
 * @file collect.c
 * @brief Collect pass: gathers every module's top-level declarations into the world table.
 * @author solid-matrix
 * @version 0.0.5
 */

#include <assert.h>
#include <stdbool.h>

#include "arena.h"
#include "collect.h"
#include "error.h"
#include "namepath.h"
#include "semantic_error.h"
#include "symboltable.h"
#include "syntax_node.h"

// A SemanticNamePath from a source-order chain of SyntaxIdentifier nodes.
static SemanticNamePath *path_from_identifiers(Arena *arena, const SyntaxNodeList *ids) {
  if (ids == NULL)
    return semantic_namepath_empty();
  return semantic_namepath_prepend(arena, path_from_identifiers(arena, ids->next),
                                   ((SyntaxIdentifier *)ids->node)->value);
}

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
// skips them.
static SemanticCollectResult collect_program(Arena *arena, SemanticSymbolTable *symbols, const SemanticModule *module,
                                             SyntaxProgram *program) {

  if (program->top_levels == NULL)
    return (SemanticCollectResult){.errors = NULL, .symbols = symbols};

  SemanticErrorList *errors = semantic_errorlist_empty();

  SyntaxNodeList *decls = program->top_levels;
  SemanticNamePath *prefix = module->path;

  // process the optional topmost namespace declaration
  if (decls->node->kind == SYNTAX_KIND_NAMESPACE_DECL) {
    SyntaxNamespaceDecl *decl = (SyntaxNamespaceDecl *)decls->node;

    SemanticNamePath *ns = path_from_identifiers(arena, decl->path);
    prefix = semantic_namepath_concat(arena, prefix, ns);

    SemanticSymbolTable *defined = semantic_symboltable_define(symbols, prefix, NULL);
    if (defined == NULL) {
      errors = semantic_errorlist_prepend(arena, errors,
                                          semantic_error_create(SEMANTIC_SYMBOL_NAMESPACE_CLASH, decl->header.span));
    } else {
      symbols = defined;
    }

    decls = decls->next;
  }

  // skip the following consecutive using declarations
  while (decls != NULL && decls->node->kind == SYNTAX_KIND_USING_DECL)
    decls = decls->next;

  for (SyntaxNodeList *it = decls; it != NULL; it = it->next) {
    SyntaxNode *decl = it->node;

    if (!decl_enabled(decl))
      continue;

    SemanticNamePath *tail = semantic_namepath_prepend(arena, semantic_namepath_empty(), decl_name(decl));
    SemanticNamePath *path = semantic_namepath_concat(arena, prefix, tail);

    SemanticError error;
    if (semantic_symboltable_lookup(symbols, path) != NULL) {
      error = semantic_error_create(SEMANTIC_SYMBOL_REDEFINED, decl->span);
      errors = semantic_errorlist_prepend(arena, errors, error);
      continue;
    }

    SemanticSymbolTable *defined = semantic_symboltable_define(symbols, path, decl);
    if (defined == NULL) {
      error = semantic_error_create(SEMANTIC_SYMBOL_NAMESPACE_CLASH, decl->span);
      errors = semantic_errorlist_prepend(arena, errors, error);
      continue;
    }

    symbols = defined;
  }

  return (SemanticCollectResult){.errors = errors, .symbols = symbols};
}

SemanticCollectResult semantic_collect(Arena *arena, const SemanticModuleList *modules,
                                       const SemanticParamList *params) {
  (void)params; // consumed by the @when gate once it lands

  SemanticSymbolTable *symbols = semantic_symboltable_create(arena);
  SemanticErrorList *errors = semantic_errorlist_empty();

  for (const SemanticModuleList *it = modules; it != NULL; it = it->next)
    for (SemanticProgramList *unit = it->module->programs; unit != NULL; unit = unit->next) {
      SemanticCollectResult res = collect_program(arena, symbols, it->module, unit->program);

      symbols = res.symbols;
      errors = semantic_errorlist_concat(arena, res.errors, errors);
    }

  return (SemanticCollectResult){.symbols = symbols, .errors = errors};
}
