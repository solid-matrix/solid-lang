/**
 * @file resolve.c
 * @brief Resolve pass: binds every name use-site to the entity it refers to.
 * @author solid-matrix
 */

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>

#include "arena.h"
#include "binding_table.h"
#include "internal.h"
#include "namepath.h"
#include "semantic_common.h"
#include "semantic_error.h"
#include "strview.h"
#include "symbol_table.h"
#include "symbol_table_chain.h"
#include "symbol_table_list.h"
#include "syntax_node.h"

static const Strview CORE_NAMESPACE = STRVIEW("core");

// The walking frame: everything a resolver step sees. Passed by value —
// walkers update their local copy (folding sub-walk results back in,
// extending the chain with scope levels) and hand the updated frame back
// through the returned result. arena and chain never change for a whole
// file; scope levels live on the chain itself and die with the frame that
// opened them.
typedef struct {
  Arena *arena;
  SemanticSymbolTableChain *chain; // innermost scope level first
  SemanticBindingTable *binding_table;
  SemanticErrorList *errors; // newest-first
} Resolver;

static SemanticNamePath *path_single(Arena *arena, Strview name) {
  return semantic_namepath_prepend(arena, semantic_namepath_empty(), name);
}

// True when the declaration can appear in a type position.
static bool is_type_entity(const SyntaxNode *decl) {
  switch (decl->kind) {
  case SYNTAX_KIND_STRUCT_DECL:
  case SYNTAX_KIND_ENUM_DECL:
  case SYNTAX_KIND_UNION_DECL:
  case SYNTAX_KIND_VARIANT_DECL:
  case SYNTAX_KIND_CONTRACT_DECL:
  case SYNTAX_KIND_GENERIC_PARAM:
    return true;
  default:
    return false;
  }
}

// Number of generic parameters a declaration declares; generic parameters
// themselves never take arguments.
static size_t generic_param_count(const SyntaxNode *decl) {
  switch (decl->kind) {
  case SYNTAX_KIND_STRUCT_DECL:
    return syntax_nodelist_length(((const SyntaxStructDecl *)decl)->generic_params);
  case SYNTAX_KIND_UNION_DECL:
    return syntax_nodelist_length(((const SyntaxUnionDecl *)decl)->generic_params);
  case SYNTAX_KIND_VARIANT_DECL:
    return syntax_nodelist_length(((const SyntaxVariantDecl *)decl)->generic_params);
  case SYNTAX_KIND_CONTRACT_DECL:
    return syntax_nodelist_length(((const SyntaxContractDecl *)decl)->generic_params);
  case SYNTAX_KIND_FUNC_DECL:
    return syntax_nodelist_length(((const SyntaxFuncDecl *)decl)->generic_params);
  default:
    return 0;
  }
}

// The identifier of an enum/variant/struct/union field node.
static const SyntaxIdentifier *field_id(const SyntaxNode *field) {
  switch (field->kind) {
  case SYNTAX_KIND_ENUM_FIELD:
    return ((const SyntaxEnumField *)field)->id;
  case SYNTAX_KIND_VARIANT_FIELD:
    return ((const SyntaxVariantField *)field)->id;
  case SYNTAX_KIND_STRUCT_FIELD:
    return ((const SyntaxStructField *)field)->id;
  default:
    return ((const SyntaxUnionField *)field)->id;
  }
}

typedef enum {
  RESOLVE_PATH_SYMBOL,
  RESOLVE_PATH_NAMESPACE,
  RESOLVE_PATH_MISSING,
  RESOLVE_PATH_AMBIGUOUS,
} ResolvePathKind;

typedef struct {
  ResolvePathKind kind;
  size_t matched;   // segments consumed; the symbol segment counts
  SyntaxNode *decl; // SYMBOL: the bound entity
} ResolvePathMatch;

// Continues @p path (which may be NULL) through one table: lookup probes
// for a symbol (early stop), subtable descends into namespaces. MISSING
// reports how much of @p path matched.
static ResolvePathMatch walk_table(Arena *arena, const SemanticSymbolTable *table, const SemanticNamePath *path) {
  ResolvePathMatch match = {.kind = RESOLVE_PATH_MISSING, .matched = 0, .decl = NULL};

  for (const SemanticNamePath *seg = path; seg != NULL; seg = seg->tail) {
    SyntaxNode *decl = semantic_symbol_table_lookup(table, path_single(arena, seg->head));
    if (decl != NULL)
      return (ResolvePathMatch){.kind = RESOLVE_PATH_SYMBOL, .matched = match.matched + 1, .decl = decl};

    const SemanticSymbolTable *next = semantic_symbol_table_subtable(table, path_single(arena, seg->head));
    if (next == NULL)
      return match;

    table = next;
    match.matched++;
  }

  match.kind = RESOLVE_PATH_NAMESPACE;
  return match;
}

static ResolvePathMatch path_resolve(Arena *arena, SemanticSymbolTableChain *chain, const SemanticNamePath *path) {
  for (const SemanticSymbolTableChain *c = chain; c != NULL; c = c->tail) {
    SyntaxNode *decl = NULL;
    SemanticSymbolTable *ns = NULL;
    size_t hits = 0;

    for (const SemanticSymbolTableList *it = c->head; it != NULL; it = it->tail) {
      SyntaxNode *d = semantic_symbol_table_lookup(it->head, path_single(arena, path->head));
      if (d != NULL) {
        decl = d;
        hits++;
        continue;
      }
      SemanticSymbolTable *sub = semantic_symbol_table_subtable(it->head, path_single(arena, path->head));
      if (sub != NULL) {
        ns = sub;
        hits++;
      }
    }

    if (hits > 1)
      return (ResolvePathMatch){.kind = RESOLVE_PATH_AMBIGUOUS, .matched = 1, .decl = NULL};
    if (hits == 0)
      continue;

    if (decl != NULL)
      return (ResolvePathMatch){.kind = RESOLVE_PATH_SYMBOL, .matched = 1, .decl = decl};

    ResolvePathMatch match = walk_table(arena, ns, path->tail);
    match.matched++;
    return match;
  }

  return (ResolvePathMatch){.kind = RESOLVE_PATH_MISSING, .matched = 0, .decl = NULL};
}

static SemanticResolveResult resolve_named(Resolver r, SyntaxNamed *named, bool type_position) {
  SemanticNamePath *path = semantic_namepath_from_identifiers(r.arena, named->path);
  size_t depth = semantic_namepath_length(path);

  ResolvePathMatch match = path_resolve(r.arena, r.chain, path);

  if (match.kind == RESOLVE_PATH_NAMESPACE) {
    SemanticErrorCode code = type_position ? SEMANTIC_NOT_A_TYPE : SEMANTIC_NOT_A_VALUE;
    SemanticError error = semantic_error_create(code, named->header.span);
    r.errors = semantic_errorlist_prepend(r.arena, r.errors, error);
    return (SemanticResolveResult){.binding_table = r.binding_table, .errors = r.errors};
  }
  if (match.kind == RESOLVE_PATH_MISSING) {
    SyntaxNode *seg = syntax_nodelist_at(named->path, match.matched);
    SemanticError error = semantic_error_create(SEMANTIC_UNKNOWN_NAME, ((SyntaxIdentifier *)seg)->header.span);
    r.errors = semantic_errorlist_prepend(r.arena, r.errors, error);
    return (SemanticResolveResult){.binding_table = r.binding_table, .errors = r.errors};
  }
  if (match.kind == RESOLVE_PATH_AMBIGUOUS) {
    SemanticError error = semantic_error_create(SEMANTIC_AMBIGUOUS_NAME, named->header.span);
    r.errors = semantic_errorlist_prepend(r.arena, r.errors, error);
    return (SemanticResolveResult){.binding_table = r.binding_table, .errors = r.errors};
  }

  // member resolution: the walk stopped at a symbol with segments to spare
  SyntaxNode *decl = match.decl;
  if (match.matched < depth) {
    SyntaxNode *member_seg = syntax_nodelist_at(named->path, match.matched);
    Span member_span = ((SyntaxIdentifier *)member_seg)->header.span;

    SyntaxNode *field = NULL;
    bool supported = decl->kind == SYNTAX_KIND_ENUM_DECL || decl->kind == SYNTAX_KIND_VARIANT_DECL;
    if (supported && match.matched + 2 >= depth) {
      Strview name = ((SyntaxIdentifier *)member_seg)->value;
      const SyntaxNodeList *fields = decl->kind == SYNTAX_KIND_ENUM_DECL ? ((const SyntaxEnumDecl *)decl)->fields
                                                                         : ((const SyntaxVariantDecl *)decl)->fields;
      for (const SyntaxNodeList *it = fields; it != NULL; it = it->tail) {
        if (strview_compare(field_id(it->head)->value, name) == 0) {
          field = it->head;
          break;
        }
      }
    }

    if (field == NULL) {
      SemanticError error = semantic_error_create(SEMANTIC_NO_MEMBER, member_span);
      r.errors = semantic_errorlist_prepend(r.arena, r.errors, error);
      return (SemanticResolveResult){.binding_table = r.binding_table, .errors = r.errors};
    }
    decl = field;
  }

  if (syntax_nodelist_length(named->generic_args) != generic_param_count(decl)) {
    SemanticError error = semantic_error_create(SEMANTIC_GENERIC_ARITY_MISMATCH, named->header.span);
    r.errors = semantic_errorlist_prepend(r.arena, r.errors, error);
    return (SemanticResolveResult){.binding_table = r.binding_table, .errors = r.errors};
  }

  if (type_position && !is_type_entity(decl)) {
    SemanticError error = semantic_error_create(SEMANTIC_NOT_A_TYPE, named->header.span);
    r.errors = semantic_errorlist_prepend(r.arena, r.errors, error);
    return (SemanticResolveResult){.binding_table = r.binding_table, .errors = r.errors};
  }

  r.binding_table = semantic_binding_table_insert(r.arena, r.binding_table, (SyntaxNode *)named, decl);
  return (SemanticResolveResult){.binding_table = r.binding_table, .errors = r.errors};
}

static SemanticResolveResult resolve_type(Resolver r, SyntaxNode *type);
static SemanticResolveResult resolve_expr(Resolver r, SyntaxNode *expr) {
  if (expr == NULL)
    return (SemanticResolveResult){.binding_table = r.binding_table, .errors = r.errors};

  switch (expr->kind) {
  case SYNTAX_KIND_NAMED:
    return resolve_named(r, (SyntaxNamed *)expr, false);

  case SYNTAX_KIND_BINARY_EXPR: {
    SyntaxBinaryExpr *binary = (SyntaxBinaryExpr *)expr;
    SemanticResolveResult res = resolve_expr(r, binary->left);
    r.binding_table = res.binding_table;
    r.errors = res.errors;
    return resolve_expr(r, binary->right);
  }
  case SYNTAX_KIND_UNARY_EXPR:
    return resolve_expr(r, ((SyntaxUnaryExpr *)expr)->operand);
  case SYNTAX_KIND_DOT_EXPR: // the field name itself is check's business
    return resolve_expr(r, ((SyntaxDotExpr *)expr)->receiver);
  case SYNTAX_KIND_INDEX_EXPR: {
    SyntaxIndexExpr *index = (SyntaxIndexExpr *)expr;
    SemanticResolveResult res = resolve_expr(r, index->receiver);
    r.binding_table = res.binding_table;
    r.errors = res.errors;
    return resolve_expr(r, index->index);
  }
  case SYNTAX_KIND_CALL_EXPR: {
    SyntaxCallExpr *call = (SyntaxCallExpr *)expr;
    SemanticResolveResult res = resolve_expr(r, call->receiver);
    r.binding_table = res.binding_table;
    r.errors = res.errors;
    for (const SyntaxNodeList *it = call->args; it != NULL; it = it->tail) {
      res = resolve_expr(r, it->head);
      r.binding_table = res.binding_table;
      r.errors = res.errors;
    }
    return (SemanticResolveResult){.binding_table = r.binding_table, .errors = r.errors};
  }
  case SYNTAX_KIND_STRUCT_LIT_EXPR: {
    SyntaxStructLitExpr *struct_lit = (SyntaxStructLitExpr *)expr;

    SemanticResolveResult res;
    if (struct_lit->type != NULL) {
      SemanticResolveResult res = resolve_named(r, struct_lit->type, true);
      r.binding_table = res.binding_table;
      r.errors = res.errors;
    }

    for (const SyntaxNodeList *it = struct_lit->fields; it != NULL; it = it->tail) {
      res = resolve_expr(r, ((SyntaxStructLitField *)it->head)->value);
      r.binding_table = res.binding_table;
      r.errors = res.errors;
    }
    return (SemanticResolveResult){.binding_table = r.binding_table, .errors = r.errors};
  }
  case SYNTAX_KIND_ARRAY_LIT_EXPR: {
    SyntaxArrayLitExpr *array_lit = (SyntaxArrayLitExpr *)expr;

    if (array_lit->type != NULL) {
      SemanticResolveResult res = resolve_type(r, array_lit->type);
      r.binding_table = res.binding_table;
      r.errors = res.errors;
    }

    for (const SyntaxNodeList *it = ((SyntaxArrayLitExpr *)expr)->elements; it != NULL; it = it->tail) {
      SemanticResolveResult res = resolve_expr(r, it->head);
      r.binding_table = res.binding_table;
      r.errors = res.errors;
    }
    return (SemanticResolveResult){.binding_table = r.binding_table, .errors = r.errors};
  }

  default: // literals carry no names
    return (SemanticResolveResult){.binding_table = r.binding_table, .errors = r.errors};
  }
}

static SemanticResolveResult resolve_type(Resolver r, SyntaxNode *type) {
  if (type == NULL)
    return (SemanticResolveResult){.binding_table = r.binding_table, .errors = r.errors};

  switch (type->kind) {
  case SYNTAX_KIND_NAMED:
    return resolve_named(r, (SyntaxNamed *)type, true);
  case SYNTAX_KIND_REF_TYPE:
    return resolve_type(r, ((SyntaxRefType *)type)->inner_type);
  case SYNTAX_KIND_ARRAY_TYPE: {
    SyntaxArrayType *array = (SyntaxArrayType *)type;
    SemanticResolveResult res = resolve_expr(r, array->len);
    r.binding_table = res.binding_table;
    r.errors = res.errors;
    return resolve_type(r, array->inner_type);
  }
  case SYNTAX_KIND_FUNC_TYPE: {
    SyntaxFuncType *fn = (SyntaxFuncType *)type;
    SemanticResolveResult res = {.binding_table = r.binding_table, .errors = r.errors};
    for (const SyntaxNodeList *it = fn->call_params; it != NULL; it = it->tail) {
      res = resolve_type(r, it->head);
      r.binding_table = res.binding_table;
      r.errors = res.errors;
    }
    return resolve_type(r, fn->return_type);
  }
  default:
    return (SemanticResolveResult){.binding_table = r.binding_table, .errors = r.errors};
  }
}

static SemanticResolveResult resolve_stmt(Resolver r, SyntaxNode *stmt) {
  assert(stmt != NULL);

  switch (stmt->kind) {
  case SYNTAX_KIND_BODY_STMT: {
    SemanticSymbolTableList *level = semantic_symbol_table_list_empty();
    level = semantic_symbol_table_list_prepend(r.arena, level, NULL);
    r.chain = semantic_symbol_table_chain_prepend(r.arena, r.chain, level);

    for (const SyntaxNodeList *it = ((const SyntaxBodyStmt *)stmt)->stmts; it != NULL; it = it->tail) {
      SemanticResolveResult res = resolve_stmt(r, it->head);
      r.binding_table = res.binding_table;
      r.errors = res.errors;
    }
    return (SemanticResolveResult){.binding_table = r.binding_table, .errors = r.errors};
  }
  case SYNTAX_KIND_LET_STMT: {
    SyntaxLetStmt *let = (SyntaxLetStmt *)stmt;
    SemanticResolveResult res = resolve_type(r, let->type);
    r.binding_table = res.binding_table;
    r.errors = res.errors;

    res = resolve_expr(r, let->value);
    r.binding_table = res.binding_table;
    r.errors = res.errors;

    SemanticNamePath *id_name = semantic_namepath_from_identifier(r.arena, let->id);
    SemanticSymbolTable *level = r.chain->head->head;
    if (semantic_symbol_table_lookup(level, id_name) != NULL) {
      SemanticError error = semantic_error_create(SEMANTIC_DUPLICATE_LOCAL, stmt->span);
      r.errors = semantic_errorlist_prepend(r.arena, r.errors, error);
    } else {
      r.chain->head->head = semantic_symbol_table_insert(r.arena, level, id_name, stmt);
    }

    return (SemanticResolveResult){.binding_table = r.binding_table, .errors = r.errors};
  }
  case SYNTAX_KIND_SET_STMT: {
    SyntaxSetStmt *set = (SyntaxSetStmt *)stmt;
    SemanticResolveResult res = resolve_expr(r, set->left);
    r.binding_table = res.binding_table;
    r.errors = res.errors;
    return resolve_expr(r, set->right);
  }
  case SYNTAX_KIND_EXPR_STMT:
    return resolve_expr(r, ((SyntaxExprStmt *)stmt)->expr);
  case SYNTAX_KIND_IF_STMT: {
    SyntaxIfStmt *iff = (SyntaxIfStmt *)stmt;
    SemanticResolveResult res = resolve_expr(r, iff->condition);
    r.binding_table = res.binding_table;
    r.errors = res.errors;

    res = resolve_stmt(r, iff->then_stmt);
    r.binding_table = res.binding_table;
    r.errors = res.errors;
    return resolve_stmt(r, iff->else_stmt);
  }
  case SYNTAX_KIND_LOOP_STMT:
    return resolve_stmt(r, ((const SyntaxLoopStmt *)stmt)->stmt);
  case SYNTAX_KIND_WHILE_STMT: {
    SyntaxWhileStmt *while_stmt = (SyntaxWhileStmt *)stmt;
    SemanticResolveResult res = resolve_expr(r, while_stmt->condition);
    r.binding_table = res.binding_table;
    r.errors = res.errors;
    return resolve_stmt(r, while_stmt->stmt);
  }
  case SYNTAX_KIND_RETURN_STMT:
    return resolve_expr(r, ((SyntaxReturnStmt *)stmt)->expr);
  default: // empty, break and continue carry no names
    return (SemanticResolveResult){.binding_table = r.binding_table, .errors = r.errors};
  }
}

static SemanticResolveResult resolve_generic_params(Resolver r, const SyntaxNodeList *params) {
  for (; params != NULL; params = params->tail) {
    SyntaxGenericParam *param = (SyntaxGenericParam *)params->head;

    SemanticResolveResult res = resolve_type(r, param->type);
    r.binding_table = res.binding_table;
    r.errors = res.errors;

    SemanticNamePath *param_name = semantic_namepath_from_identifier(r.arena, param->id);
    SemanticSymbolTable *level = r.chain->head->head;
    if (semantic_symbol_table_lookup(level, param_name) != NULL) {
      SemanticError error = semantic_error_create(SEMANTIC_DUPLICATE_GENERIC_PARAM, param->header.span);
      r.errors = semantic_errorlist_prepend(r.arena, r.errors, error);
    } else {
      r.chain->head->head = semantic_symbol_table_insert(r.arena, level, param_name, params->head);
    }
  }
  return (SemanticResolveResult){.binding_table = r.binding_table, .errors = r.errors};
}

static SemanticResolveResult resolve_call_params(Resolver r, const SyntaxNodeList *params) {
  for (; params != NULL; params = params->tail) {
    SyntaxCallParam *param = (SyntaxCallParam *)params->head;

    SemanticNamePath *param_name = semantic_namepath_from_identifier(r.arena, param->id);
    SemanticSymbolTable *level = r.chain->head->head;
    if (semantic_symbol_table_lookup(level, param_name) != NULL) {
      SemanticError error = semantic_error_create(SEMANTIC_DUPLICATE_PARAM_NAME, param->header.span);
      r.errors = semantic_errorlist_prepend(r.arena, r.errors, error);
    } else {
      r.chain->head->head = semantic_symbol_table_insert(r.arena, level, param_name, params->head);
    }

    SemanticResolveResult res = resolve_type(r, param->type);
    r.binding_table = res.binding_table;
    r.errors = res.errors;
  }
  return (SemanticResolveResult){.binding_table = r.binding_table, .errors = r.errors};
}

static SemanticResolveResult resolve_let_decl(Resolver r, SyntaxLetDecl *decl) {
  SemanticResolveResult res = resolve_type(r, decl->type);
  r.binding_table = res.binding_table;
  r.errors = res.errors;

  res = resolve_expr(r, decl->value);
  r.binding_table = res.binding_table;
  r.errors = res.errors;

  return (SemanticResolveResult){.binding_table = r.binding_table, .errors = r.errors};
}

static SemanticResolveResult resolve_struct_decl(Resolver r, SyntaxStructDecl *decl) {
  SemanticSymbolTableList *level = semantic_symbol_table_list_empty();
  level = semantic_symbol_table_list_prepend(r.arena, level, semantic_symbol_table_empty());
  r.chain = semantic_symbol_table_chain_prepend(r.arena, r.chain, level);

  SemanticResolveResult res = resolve_generic_params(r, decl->generic_params);
  r.binding_table = res.binding_table;
  r.errors = res.errors;

  for (const SyntaxNodeList *it = decl->fields; it != NULL; it = it->tail) {
    const SyntaxStructField *field = (const SyntaxStructField *)it->head;
    res = resolve_type(r, field->type);
    r.binding_table = res.binding_table;
    r.errors = res.errors;
  }

  for (const SyntaxNodeList *it = decl->fields; it != NULL; it = it->tail) {
    Strview iname = ((const SyntaxStructField *)it->head)->id->value;

    for (const SyntaxNodeList *jt = it->tail; jt != NULL; jt = jt->tail) {
      Strview jname = ((const SyntaxStructField *)jt->head)->id->value;

      if (strview_equals(iname, jname)) {
        SemanticError error = semantic_error_create(SEMANTIC_DUPLICATE_FIELD_NAME, jt->head->span);
        r.errors = semantic_errorlist_prepend(r.arena, r.errors, error);
        break;
      }
    }
  }

  // Resolver checked = check_duplicate_fields(r, decl->fields);
  return (SemanticResolveResult){.binding_table = r.binding_table, .errors = r.errors};
}

static SemanticResolveResult resolve_union_decl(Resolver r, SyntaxUnionDecl *decl) {
  SemanticSymbolTableList *level = semantic_symbol_table_list_empty();
  level = semantic_symbol_table_list_prepend(r.arena, level, semantic_symbol_table_empty());
  r.chain = semantic_symbol_table_chain_prepend(r.arena, r.chain, level);

  SemanticResolveResult res = resolve_generic_params(r, decl->generic_params);
  r.binding_table = res.binding_table;
  r.errors = res.errors;

  for (const SyntaxNodeList *it = decl->fields; it != NULL; it = it->tail) {
    const SyntaxUnionField *field = (const SyntaxUnionField *)it->head;
    res = resolve_type(r, field->type);
    r.binding_table = res.binding_table;
    r.errors = res.errors;
  }

  for (const SyntaxNodeList *it = decl->fields; it != NULL; it = it->tail) {
    Strview iname = ((const SyntaxUnionField *)it->head)->id->value;

    for (const SyntaxNodeList *jt = it->tail; jt != NULL; jt = jt->tail) {
      Strview jname = ((const SyntaxUnionField *)jt->head)->id->value;

      if (strview_equals(iname, jname)) {
        SemanticError error = semantic_error_create(SEMANTIC_DUPLICATE_FIELD_NAME, jt->head->span);
        r.errors = semantic_errorlist_prepend(r.arena, r.errors, error);
        break;
      }
    }
  }

  return (SemanticResolveResult){.binding_table = r.binding_table, .errors = r.errors};
}

static SemanticResolveResult resolve_variant_decl(Resolver r, SyntaxVariantDecl *decl) {
  SemanticResolveResult res = resolve_type(r, decl->behind_type);
  r.binding_table = res.binding_table;
  r.errors = res.errors;

  SemanticSymbolTableList *level = semantic_symbol_table_list_empty();
  level = semantic_symbol_table_list_prepend(r.arena, level, semantic_symbol_table_empty());
  r.chain = semantic_symbol_table_chain_prepend(r.arena, r.chain, level);

  res = resolve_generic_params(r, decl->generic_params);
  r.binding_table = res.binding_table;
  r.errors = res.errors;

  for (const SyntaxNodeList *it = decl->fields; it != NULL; it = it->tail) {
    const SyntaxVariantField *field = (const SyntaxVariantField *)it->head;
    res = resolve_type(r, field->type);
    r.binding_table = res.binding_table;
    r.errors = res.errors;
  }

  for (const SyntaxNodeList *it = decl->fields; it != NULL; it = it->tail) {
    Strview iname = ((const SyntaxVariantField *)it->head)->id->value;

    for (const SyntaxNodeList *jt = it->tail; jt != NULL; jt = jt->tail) {
      Strview jname = ((const SyntaxVariantField *)jt->head)->id->value;

      if (strview_equals(iname, jname)) {
        SemanticError error = semantic_error_create(SEMANTIC_DUPLICATE_FIELD_NAME, jt->head->span);
        r.errors = semantic_errorlist_prepend(r.arena, r.errors, error);
        break;
      }
    }
  }

  return (SemanticResolveResult){.binding_table = r.binding_table, .errors = r.errors};
}

static SemanticResolveResult resolve_enum_decl(Resolver r, SyntaxEnumDecl *decl) {
  SemanticResolveResult res = resolve_type(r, decl->behind_type);
  r.binding_table = res.binding_table;
  r.errors = res.errors;

  for (const SyntaxNodeList *it = decl->fields; it != NULL; it = it->tail) {
    res = resolve_expr(r, ((SyntaxEnumField *)it->head)->value);
    r.binding_table = res.binding_table;
    r.errors = res.errors;
  }

  for (const SyntaxNodeList *it = decl->fields; it != NULL; it = it->tail) {
    Strview iname = ((const SyntaxEnumField *)it->head)->id->value;

    for (const SyntaxNodeList *jt = it->tail; jt != NULL; jt = jt->tail) {
      Strview jname = ((const SyntaxEnumField *)jt->head)->id->value;

      if (strview_equals(iname, jname)) {
        SemanticError error = semantic_error_create(SEMANTIC_DUPLICATE_FIELD_NAME, jt->head->span);
        r.errors = semantic_errorlist_prepend(r.arena, r.errors, error);
        break;
      }
    }
  }

  return (SemanticResolveResult){.binding_table = r.binding_table, .errors = r.errors};
}

static SemanticResolveResult resolve_contract_decl(Resolver r, SyntaxContractDecl *decl) {
  SemanticSymbolTableList *level = semantic_symbol_table_list_empty();
  level = semantic_symbol_table_list_prepend(r.arena, level, semantic_symbol_table_empty());
  r.chain = semantic_symbol_table_chain_prepend(r.arena, r.chain, level);

  SemanticResolveResult res = resolve_generic_params(r, decl->generic_params);
  r.binding_table = res.binding_table;
  r.errors = res.errors;

  res = resolve_call_params(r, decl->call_params);
  r.binding_table = res.binding_table;
  r.errors = res.errors;

  res = resolve_type(r, decl->return_type);
  r.binding_table = res.binding_table;
  r.errors = res.errors;

  return (SemanticResolveResult){.binding_table = r.binding_table, .errors = r.errors};
}

static SemanticResolveResult resolve_func_decl(Resolver r, SyntaxFuncDecl *decl) {
  SemanticSymbolTableList *level = semantic_symbol_table_list_empty();
  level = semantic_symbol_table_list_prepend(r.arena, level, semantic_symbol_table_empty());
  r.chain = semantic_symbol_table_chain_prepend(r.arena, r.chain, level);

  SemanticResolveResult res = resolve_generic_params(r, decl->generic_params);
  r.binding_table = res.binding_table;
  r.errors = res.errors;

  res = resolve_call_params(r, decl->call_params);
  r.binding_table = res.binding_table;
  r.errors = res.errors;

  res = resolve_type(r, decl->return_type);
  r.binding_table = res.binding_table;
  r.errors = res.errors;

  for (const SyntaxNodeList *it = decl->fulfills; it != NULL; it = it->tail) {
    SyntaxNamed *contract = (SyntaxNamed *)it->head;

    res = resolve_named(r, contract, false);
    r.binding_table = res.binding_table;
    r.errors = res.errors;

    SyntaxNode *bound = semantic_binding_table_lookup(r.binding_table, (const SyntaxNode *)contract);
    if (bound != NULL && bound->kind != SYNTAX_KIND_CONTRACT_DECL) {
      SemanticError error = semantic_error_create(SEMANTIC_EXPECT_A_CONTRACT, contract->header.span);
      r.errors = semantic_errorlist_prepend(r.arena, r.errors, error);
    }
  }

  if (decl->body != NULL && decl->body->kind == SYNTAX_KIND_BODY_STMT)
    for (const SyntaxNodeList *it = ((const SyntaxBodyStmt *)decl->body)->stmts; it != NULL; it = it->tail) {
      res = resolve_stmt(r, it->head);
      r.binding_table = res.binding_table;
      r.errors = res.errors;
    }

  return (SemanticResolveResult){.binding_table = r.binding_table, .errors = r.errors};
}

static SemanticResolveResult resolve_decl(Resolver r, SyntaxNode *decl) {
  switch (decl->kind) {
  case SYNTAX_KIND_LET_DECL:
    return resolve_let_decl(r, (SyntaxLetDecl *)decl);
  case SYNTAX_KIND_STRUCT_DECL:
    return resolve_struct_decl(r, (SyntaxStructDecl *)decl);
  case SYNTAX_KIND_UNION_DECL:
    return resolve_union_decl(r, (SyntaxUnionDecl *)decl);
  case SYNTAX_KIND_VARIANT_DECL:
    return resolve_variant_decl(r, (SyntaxVariantDecl *)decl);
  case SYNTAX_KIND_ENUM_DECL:
    return resolve_enum_decl(r, (SyntaxEnumDecl *)decl);
  case SYNTAX_KIND_CONTRACT_DECL:
    return resolve_contract_decl(r, (SyntaxContractDecl *)decl);
  case SYNTAX_KIND_FUNC_DECL:
    return resolve_func_decl(r, (SyntaxFuncDecl *)decl);
  default:
    assert(!"unexpected decl kind");
    return (SemanticResolveResult){.binding_table = r.binding_table, .errors = r.errors};
  }
}

static SemanticResolveResult resolve_program(Resolver r, const SemanticModule *module, SyntaxProgram *program,
                                             const SemanticSymbolTable *global_symbol_table) {

  assert(module->path != NULL);

  // the global layer: the world root, home of every package name
  SemanticSymbolTable *global = semantic_symbol_table_subtable(global_symbol_table, NULL);
  SemanticSymbolTableList *global_list =
      semantic_symbol_table_list_prepend(r.arena, semantic_symbol_table_list_empty(), global);
  r.chain = semantic_symbol_table_chain_prepend(r.arena, r.chain, global_list);

  // the core layer: the implicit prelude, seen by every module but core
  static const Strview CORE_NAMESPACE = STRVIEW("core");
  SemanticNamePath *core_namepath = semantic_namepath_from_array(r.arena, &CORE_NAMESPACE, 1);
  if (!semantic_namepath_equals(module->path, core_namepath)) {
    SemanticSymbolTable *core = semantic_symbol_table_subtable(global_symbol_table, core_namepath);

    if (core != NULL) {
      SemanticSymbolTableList *core_list =
          semantic_symbol_table_list_prepend(r.arena, semantic_symbol_table_list_empty(), core);
      r.chain = semantic_symbol_table_chain_prepend(r.arena, r.chain, core_list);
    }
  }

  // the current list: the module-relative view, then every using target
  SyntaxNodeList *decl_list = program->top_levels;

  SemanticSymbolTableList *current_list = semantic_symbol_table_list_empty();

  SemanticNamePath *self_namepath = module->path;
  if (decl_list != NULL && decl_list->head->kind == SYNTAX_KIND_NAMESPACE_DECL) {
    SyntaxNamespaceDecl *ns_node = (SyntaxNamespaceDecl *)decl_list->head;
    SemanticNamePath *ns_path = semantic_namepath_from_identifiers(r.arena, ns_node->path);

    self_namepath = semantic_namepath_concat(r.arena, module->path, ns_path);
    decl_list = decl_list->tail;
  }
  SemanticSymbolTable *self = semantic_symbol_table_subtable(global_symbol_table, self_namepath);
  if (self != NULL)
    current_list = semantic_symbol_table_list_prepend(r.arena, current_list, self);

  for (; decl_list != NULL && decl_list->head->kind == SYNTAX_KIND_USING_DECL; decl_list = decl_list->tail) {
    SyntaxUsingDecl *using_node = (SyntaxUsingDecl *)decl_list->head;

    SemanticNamePath *using_namepath = semantic_namepath_from_identifiers(r.arena, using_node->path);
    SemanticNamePath *module_using_namepath = semantic_namepath_concat(r.arena, module->path, using_namepath);

    // the target resolves module-relative first, world-absolute second. A
    // using target is a namespace: a symbol occupying the name does not
    // block the search, an empty namespace imports nothing, and re-using
    // an already imported table is harmless
    SemanticSymbolTable *using = NULL;
    bool exists = false;
    const SemanticNamePath *candidates[2] = {module_using_namepath, using_namepath};
    for (int i = 0; i < 2 && !exists; i++) {
      if (!semantic_symbol_table_contains(global_symbol_table, candidates[i]))
        continue;
      if (semantic_symbol_table_lookup(global_symbol_table, candidates[i]) != NULL)
        continue; // the name is a symbol here — not a namespace
      exists = true;
      using = semantic_symbol_table_subtable(global_symbol_table, candidates[i]);
    }

    if (!exists) {
      SemanticError error = semantic_error_create(SEMANTIC_EXPECT_NAMESPACE, using_node->header.span);
      r.errors = semantic_errorlist_prepend(r.arena, r.errors, error);
      continue;
    }

    bool imported = false;
    for (const SemanticSymbolTableList *it = current_list; it != NULL && !imported; it = it->tail)
      imported = it->head == using;
    if (using != NULL && !imported)
      current_list = semantic_symbol_table_list_prepend(r.arena, current_list, using);
  }

  r.chain = semantic_symbol_table_chain_prepend(r.arena, r.chain, current_list);

  for (; decl_list != NULL; decl_list = decl_list->tail) {
    SemanticResolveResult res = resolve_decl(r, decl_list->head);
    r.binding_table = res.binding_table;
    r.errors = res.errors;
  }

  return (SemanticResolveResult){.binding_table = r.binding_table, .errors = r.errors};
}

SemanticResolveResult semantic_resolve(const SemanticAnalyzer *analyzer, const SemanticSymbolTable *global_symbols) {

  Resolver r = {
      .arena = analyzer->arena,
      .chain = semantic_symbol_table_chain_empty(),
      .binding_table = semantic_binding_table_empty(),
      .errors = semantic_errorlist_empty(),
  };

  for (const SemanticModuleList *mit = analyzer->modules; mit != NULL; mit = mit->next)
    for (SemanticProgramList *pit = mit->module->programs; pit != NULL; pit = pit->next) {
      SemanticResolveResult res = resolve_program(r, mit->module, pit->program, global_symbols);
      r.binding_table = res.binding_table;
      r.errors = res.errors;
    }

  return (SemanticResolveResult){.binding_table = r.binding_table, .errors = r.errors};
}
