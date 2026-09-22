/**
 * @file semantic_registry.c
 * @brief The pre-collect constant world (see semantic_registry.h).
 * @author solid-matrix
 */

#include "semantic_registry.h"

#include <string.h>

#include "namepath.h"
#include "symbol_table.h"

static const char *ANNOT_FEATURE = "feature";
static const char *ANNOT_INTRINSIC = "intrinsic";
static const char *ANNOT_IMPORT = "import";

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
  case SYNTAX_KIND_CONTRACT_DECL:
    return ((SyntaxContractDecl *)decl)->id->value;
  case SYNTAX_KIND_FUNC_DECL:
    return ((SyntaxFuncDecl *)decl)->id->value;
  default:
    return (Strview){0};
  }
}

// The annotations of any node that can carry them: the top-level declaration
// kinds and the nested positions (fields, formal and generic parameters).
static SyntaxNodeList *decl_annotations(SyntaxNode *decl) {
  switch (decl->kind) {
  case SYNTAX_KIND_LET_DECL:
    return ((SyntaxLetDecl *)decl)->annotations;
  case SYNTAX_KIND_STRUCT_DECL:
    return ((SyntaxStructDecl *)decl)->annotations;
  case SYNTAX_KIND_ENUM_DECL:
    return ((SyntaxEnumDecl *)decl)->annotations;
  case SYNTAX_KIND_UNION_DECL:
    return ((SyntaxUnionDecl *)decl)->annotations;
  case SYNTAX_KIND_CONTRACT_DECL:
    return ((SyntaxContractDecl *)decl)->annotations;
  case SYNTAX_KIND_FUNC_DECL:
    return ((SyntaxFuncDecl *)decl)->annotations;
  case SYNTAX_KIND_STRUCT_FIELD:
    return ((SyntaxStructField *)decl)->annotations;
  case SYNTAX_KIND_UNION_FIELD:
    return ((SyntaxUnionField *)decl)->annotations;
  case SYNTAX_KIND_ENUM_FIELD:
    return ((SyntaxEnumField *)decl)->annotations;
  case SYNTAX_KIND_CALL_PARAM:
    return ((SyntaxCallParam *)decl)->annotations;
  case SYNTAX_KIND_GENERIC_PARAM:
    return ((SyntaxGenericParam *)decl)->annotations;
  default:
    return NULL;
  }
}

// Counts the `@when` annotations of an annotation list.
static int count_when(SyntaxNodeList *annotations) {
  Strview when = strview_from_cstr("when");
  int count = 0;
  for (SyntaxNodeList *it = annotations; it != NULL; it = it->tail) {
    SyntaxCompileTime *annotation = (SyntaxCompileTime *)it->head;
    if (annotation->header.kind != SYNTAX_KIND_COMPILE_TIME)
      continue;
    if (strview_equals(annotation->id->value, when))
      count++;
  }
  return count;
}

// Attaches the single `@when` condition to @p entry. A second guard is
// diagnosed and the first wins; a `@when` that carries no condition at all is
// ill-formed — a guard with nothing to decide would silently keep the
// declaration forever (§10.4).
static void extract_guard(Arena *arena, SemanticErrorList **errors, SemanticRegistryEntry *entry) {
  Strview when = strview_from_cstr("when");
  SyntaxNode *decl = entry->decl;
  SyntaxNode *guard = NULL;
  int count = 0;
  for (SyntaxNodeList *it = decl_annotations(decl); it != NULL; it = it->tail) {
    SyntaxCompileTime *annotation = (SyntaxCompileTime *)it->head;
    if (annotation->header.kind != SYNTAX_KIND_COMPILE_TIME)
      continue;
    if (!strview_equals(annotation->id->value, when))
      continue;
    count++;
    if (guard == NULL && annotation->args != NULL)
      guard = annotation->args->head;
  }
  if (count > 1) {
    SemanticError error = semantic_error_create(SEMANTIC_MULTIPLE_GUARDS, decl->span);
    *errors = semantic_errorlist_prepend(arena, *errors, error);
  }
  if (count > 0 && guard == NULL) {
    SemanticError error = semantic_error_create(SEMANTIC_GUARD_CONDITION, decl->span);
    *errors = semantic_errorlist_prepend(arena, *errors, error);
    entry->guard_missing = 1;
  }
  entry->guard = guard;
}

// Diagnoses `@when` written where guards are forbidden (§10.4 Position):
// fields and formal/generic parameters. The parser accepts annotations in
// those positions; only the six top-level declaration kinds may be guarded.
static void diagnose_nested_guards(Arena *arena, SemanticErrorList **errors, SyntaxNodeList *sites) {
  for (SyntaxNodeList *it = sites; it != NULL; it = it->tail) {
    SyntaxNode *site = it->head;
    if (site == NULL || count_when(decl_annotations(site)) == 0)
      continue;
    SemanticError error = semantic_error_create(SEMANTIC_GUARD_FORBIDDEN, site->span);
    *errors = semantic_errorlist_prepend(arena, *errors, error);
  }
}

static void check_nested_guards(Arena *arena, SemanticErrorList **errors, SyntaxNode *decl) {
  switch (decl->kind) {
  case SYNTAX_KIND_STRUCT_DECL: {
    SyntaxStructDecl *d = (SyntaxStructDecl *)decl;
    diagnose_nested_guards(arena, errors, d->fields);
    diagnose_nested_guards(arena, errors, d->generic_params);
    break;
  }
  case SYNTAX_KIND_UNION_DECL: {
    SyntaxUnionDecl *d = (SyntaxUnionDecl *)decl;
    diagnose_nested_guards(arena, errors, d->fields);
    diagnose_nested_guards(arena, errors, d->generic_params);
    break;
  }
  case SYNTAX_KIND_ENUM_DECL: {
    SyntaxEnumDecl *d = (SyntaxEnumDecl *)decl;
    diagnose_nested_guards(arena, errors, d->fields);
    break;
  }
  case SYNTAX_KIND_CONTRACT_DECL: {
    SyntaxContractDecl *d = (SyntaxContractDecl *)decl;
    diagnose_nested_guards(arena, errors, d->call_params);
    diagnose_nested_guards(arena, errors, d->generic_params);
    break;
  }
  case SYNTAX_KIND_FUNC_DECL: {
    SyntaxFuncDecl *d = (SyntaxFuncDecl *)decl;
    diagnose_nested_guards(arena, errors, d->call_params);
    diagnose_nested_guards(arena, errors, d->generic_params);
    break;
  }
  default:
    break;
  }
}

static int annotation_is(SyntaxCompileTime *a, const char *name) {
  return a->header.kind == SYNTAX_KIND_COMPILE_TIME &&
         strview_equals(a->id->value, strview_from_cstr(name));
}

static int init_is_address_binding(SyntaxNode *init) {
  return init->kind == SYNTAX_KIND_COMPILE_TIME &&
         (annotation_is((SyntaxCompileTime *)init, "const") ||
          annotation_is((SyntaxCompileTime *)init, "static"));
}

static SemanticRegistryKind classify_shape(SyntaxLetDecl *decl) {
  int has_feature = 0;
  int has_intrinsic = 0;
  int has_import = 0;
  for (SyntaxNodeList *it = decl->annotations; it != NULL; it = it->tail) {
    if (annotation_is((SyntaxCompileTime *)it->head, ANNOT_FEATURE)) has_feature = 1;
    if (annotation_is((SyntaxCompileTime *)it->head, ANNOT_INTRINSIC)) has_intrinsic = 1;
    if (annotation_is((SyntaxCompileTime *)it->head, ANNOT_IMPORT)) has_import = 1;
  }
  if (has_import) return SEMANTIC_RK_LET_IMPORT;
  if (has_feature) return SEMANTIC_RK_LET_FEATURE;
  if (has_intrinsic) return SEMANTIC_RK_LET_INTRINSIC;
  if (decl->value == NULL) return SEMANTIC_RK_LET_NO_INIT;
  if (init_is_address_binding(decl->value)) return SEMANTIC_RK_LET_ADDRESS;
  return SEMANTIC_RK_LET;
}

static SemanticRegistryKind classify_decl(SyntaxNode *decl) {
  if (decl->kind == SYNTAX_KIND_LET_DECL)
    return classify_shape((SyntaxLetDecl *)decl);
  switch (decl->kind) {
  case SYNTAX_KIND_FUNC_DECL:
    return SEMANTIC_RK_FUNC;
  case SYNTAX_KIND_STRUCT_DECL:
    return SEMANTIC_RK_STRUCT;
  case SYNTAX_KIND_ENUM_DECL:
    return SEMANTIC_RK_ENUM;
  case SYNTAX_KIND_UNION_DECL:
    return SEMANTIC_RK_UNION;
  case SYNTAX_KIND_CONTRACT_DECL:
    return SEMANTIC_RK_CONTRACT;
  default:
    return SEMANTIC_RK_LET; // unreachable: the walk feeds known kinds
  }
}

static int module_is_core(const SemanticModule *module) {
  return module->path != NULL && module->path->tail == NULL &&
         strview_equals(module->path->head, strview_from_cstr("core"));
}

SemanticRegistry semantic_registry_build(Arena *arena, const SemanticModuleList *modules,
                                         const SemanticParamList *params) {
  SemanticRegistry r = {.table = semantic_symbol_table_empty(),
                        .entries = NULL,
                        .errors = semantic_errorlist_empty(),
                        .params = params};

  for (const SemanticModuleList *it = modules; it != NULL; it = it->next) {
    const SemanticModule *module = it->module;
    int is_core = module_is_core(module);

    for (SemanticProgramList *unit = module->programs; unit != NULL; unit = unit->next) {
      SyntaxNodeList *decls = unit->program->top_levels;
      SemanticNamePath *prefix = module->path;

      if (decls != NULL && decls->head->kind == SYNTAX_KIND_NAMESPACE_DECL) {
        SyntaxNamespaceDecl *decl = (SyntaxNamespaceDecl *)decls->head;
        SemanticNamePath *ns = semantic_namepath_from_identifiers(arena, decl->path);
        prefix = semantic_namepath_concat(arena, prefix, ns);

        SemanticSymbolTable *defined =
            semantic_symbol_table_insert(arena, r.table, prefix, NULL);
        if (defined == NULL) {
          SemanticError error =
              semantic_error_create(SEMANTIC_SYMBOL_NAMESPACE_CLASH, decl->header.span);
          r.errors = semantic_errorlist_prepend(arena, r.errors, error);
        } else {
          r.table = defined;
        }
        decls = decls->tail;
      }

      while (decls != NULL && decls->head->kind == SYNTAX_KIND_USING_DECL)
        decls = decls->tail;

      for (SyntaxNodeList *it = decls; it != NULL; it = it->tail) {
        SyntaxNode *decl = it->head;
        Strview name = decl_name(decl);

        SemanticNamePath *tail =
            semantic_namepath_prepend(arena, semantic_namepath_empty(), name);
        SemanticNamePath *path = semantic_namepath_concat(arena, prefix, tail);

        SyntaxNode *existing = semantic_symbol_table_lookup(r.table, path);
        if (existing == NULL) {
          SemanticSymbolTable *defined =
              semantic_symbol_table_insert(arena, r.table, path, decl);
          if (defined == NULL) {
            SemanticError error =
                semantic_error_create(SEMANTIC_SYMBOL_NAMESPACE_CLASH, decl->span);
            r.errors = semantic_errorlist_prepend(arena, r.errors, error);
            continue;
          }
          r.table = defined;
        }
        // else: same-name declaration — registered as an alternative below;
        // the world table keeps the first spelling, and the gate stage
        // decides the collision (§10.4 Effect).

        SemanticRegistryEntry *entry = arena_alloc(arena, sizeof *entry);
        entry->path = path;
        entry->module = module;
        entry->name = name;
        entry->kind = classify_decl(decl);
        if (entry->kind == SEMANTIC_RK_LET_NO_INIT) {
          SemanticError error = semantic_error_create(SEMANTIC_LET_MISSING_INIT, decl->span);
          r.errors = semantic_errorlist_prepend(arena, r.errors, error);
        }
        entry->decl = decl;
        entry->guard = NULL;
        entry->guard_missing = 0;
        extract_guard(arena, &r.errors, entry);
        check_nested_guards(arena, &r.errors, decl);
        entry->fold_class = SEMANTIC_FC_DEFERRED; // pending: classified while folding
        entry->value = (SemanticCValue){.kind = SEMANTIC_CV_DEFERRED};
        entry->eval_state = SEMANTIC_EVS_UNVISITED;
        entry->guard_usable = 0;
        entry->refs = NULL;
        entry->guard_refs = NULL;
        entry->gate_edges = NULL;
        entry->gate_state = SEMANTIC_GVS_UNVISITED;
        entry->reach_mark = 0;
        entry->survive = 1;
        entry->next = r.entries;
        r.entries = entry;
      }
    }

    if (is_core) {
      // The core prelude: every package's bare-name fallback.
      size_t segs = 0;
      for (SemanticNamePath *w = module->path; w != NULL; w = w->tail) segs++;
      SemanticNamePath *ns = semantic_namepath_from_array(arena, &module->path->head, segs);
      r.core_table = semantic_symbol_table_subtable(r.table, ns);
    }
  }

  return r;
}

SemanticSymbolTable *semantic_registry_table(const SemanticRegistry *registry) {
  return registry->table;
}

SemanticErrorList *semantic_registry_errors(const SemanticRegistry *registry) {
  return registry->errors;
}

SemanticRegistryEntry *semantic_registry_entries(const SemanticRegistry *registry) {
  return registry->entries;
}

SemanticRegistryLookup semantic_registry_lookup_bare(const SemanticRegistry *registry,
                                                     const SemanticModule *module, Strview name,
                                                     SyntaxNode **out_decl) {
  int hits = 0;
  SyntaxNode *hit = NULL;
  for (const SemanticRegistryEntry *it = registry->entries; it != NULL; it = it->next) {
    if (it->module == module && strview_equals(it->name, name)) {
      hits++;
      hit = it->decl;
    }
  }
  if (hits > 1) return SEMANTIC_RLOOKUP_AMBIGUOUS;
  if (hits == 1) {
    *out_decl = hit;
    return SEMANTIC_RLOOKUP_HIT;
  }

  if (registry->core_table != NULL && !module_is_core(module)) {
    for (const SemanticRegistryEntry *it = registry->entries; it != NULL; it = it->next) {
      if (module_is_core(it->module) && strview_equals(it->name, name)) {
        hits++;
        hit = it->decl;
      }
    }
    if (hits > 1) return SEMANTIC_RLOOKUP_AMBIGUOUS;
    if (hits == 1) {
      *out_decl = hit;
      return SEMANTIC_RLOOKUP_HIT;
    }
  }

  return SEMANTIC_RLOOKUP_MISS;
}

SemanticRegistryLookup semantic_registry_lookup_path(const SemanticRegistry *registry,
                                                     const SemanticModule *module,
                                                     const SemanticNamePath *path,
                                                     SyntaxNode **out_decl) {
  // package-relative first, world-absolute second (§6.1)
  SemanticSymbolTable *from_module =
      semantic_symbol_table_subtable(registry->table, module->path);
  SyntaxNode *decl = semantic_symbol_table_lookup(from_module, path);
  if (decl != NULL) {
    *out_decl = decl;
    return SEMANTIC_RLOOKUP_HIT;
  }
  decl = semantic_symbol_table_lookup(registry->table, path);
  if (decl != NULL) {
    *out_decl = decl;
    return SEMANTIC_RLOOKUP_HIT;
  }
  return SEMANTIC_RLOOKUP_MISS;
}

const Strview *semantic_registry_knob_value(const SemanticRegistry *registry,
                                            const SemanticRegistryEntry *entry) {
  for (const SemanticParamList *it = registry->params; it != NULL; it = it->next)
    if (strview_equals(it->param.name, entry->name)) return &it->param.value;
  return NULL;
}
