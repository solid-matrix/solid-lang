/**
 * @file ast.h
 * @brief AST node types produced by the parser.
 * @author solid-matrix
 * @version 0.0.5
 */

#ifndef SOLID_AST_H
#define SOLID_AST_H

#include "source.h"
#include "string_view.h"
#include <stddef.h>
#include <stdint.h>

/**
 * @brief Kind of an AST node.
 *
 * Values are grouped by category (type/expr/stmt/decl) so that
 * category membership can be tested with a mask.
 */
typedef enum
{
  SYNTAX_KIND_UNKNOWN = 0x0000,

  SYNTAX_KIND_PROGRAM = 0x0001,

  SYNTAX_KIND_TYPE_MASK = 0x0100,

  SYNTAX_KIND_EXPR_MASK = 0x0200,

  SYNTAX_KIND_STMT_MASK = 0x0400,

  SYNTAX_KIND_DECL_MASK = 0x0800,

} SyntaxKind;

typedef enum
{
  SYNTAX_REF_KIND_READWRITE = 0,

  SYNTAX_REF_KIND_READONLY,

  SYNTAX_REF_KIND_WRITEONLY,
} SyntaxRefKind;

typedef enum
{
  SYNTAX_CALLCONV_UNDEFINED = 0,

  SYNTAX_CALLCONV_CDECL,

  SYNTAX_CALLCONV_STDCALL,

  SYNTAX_CALLCONV_WINAPI,

  SYNTAX_CALLCONV_THISCALL,

  SYNTAX_CALLCONV_FASTCALL,
} SyntaxCallConv;

typedef union SyntaxNode SyntaxNode;

typedef union SyntaxType SyntaxType;

typedef union SyntaxExpr SyntaxExpr;

typedef union SyntaxStmt SyntaxStmt;

typedef union SyntaxDecl SyntaxDecl;

typedef struct
{
  SyntaxKind kind;
  SourceSpan span;
  size_t top_level_count;
  SyntaxDecl *top_levels;
} SyntaxProgram;

typedef struct
{
  SyntaxKind kind;
  SourceSpan span;
  StringView name;
  size_t argument_count;
  SyntaxExpr *arguments;
} SyntaxCtAnnotation;

typedef struct
{
  SyntaxKind kind;
  SourceSpan span;
  size_t annotation_count;
  SyntaxCtAnnotation *annotations;
  StringView name;
  SyntaxType *type;
} SyntaxGenericParameter;

typedef struct
{
  SyntaxKind kind;
  SourceSpan span;
  size_t annotation_count;
  SyntaxCtAnnotation *annotations;
  StringView name;
  SyntaxType *type;
} SyntaxGenericArgument;

typedef struct
{
  SyntaxKind kind;
  SourceSpan span;
  size_t annotation_count;
  SyntaxCtAnnotation *annotations;
  StringView name;
  SyntaxType *type;
} SyntaxContractParameter;

typedef struct
{
  SyntaxKind kind;
  SourceSpan span;
  size_t annotation_count;
  SyntaxCtAnnotation *annotations;
  StringView name;
  SyntaxType *type;
} SyntaxContractArgument;

typedef struct
{
  SyntaxKind kind;
  SourceSpan span;
  size_t annotation_count;
  SyntaxCtAnnotation *annotations;
  StringView name;
  SyntaxType *type;
} SyntaxCallParameter;

typedef struct
{
  SyntaxKind kind;
  SourceSpan span;
  size_t annotation_count;
  SyntaxCtAnnotation *annotations;
  SyntaxExpr *expr;
} SyntaxCallArgument;

typedef struct
{
  SyntaxKind kind;
  SourceSpan span;
  size_t annotation_count;
  SyntaxCtAnnotation *annotations;
  StringView name;
  SyntaxType *type;
} SyntaxStructField;

typedef struct
{
  SyntaxKind kind;
  SourceSpan span;
  size_t annotation_count;
  SyntaxCtAnnotation *annotations;
  StringView name;
  SyntaxExpr *expr;
} SyntaxEnumField;

typedef struct
{
  SyntaxKind kind;
  SourceSpan span;
  size_t annotation_count;
  SyntaxCtAnnotation *annotations;
  StringView name;
  SyntaxType *type;
} SyntaxUnionField;

typedef struct
{
  SyntaxKind kind;
  SourceSpan span;
  size_t annotation_count;
  SyntaxCtAnnotation *annotations;
  StringView name;
  SyntaxType *type;
} SyntaxVariantField;

// -- type --
typedef struct
{
  SyntaxKind kind;
  SourceSpan span;
  SyntaxExpr *expr;
} SyntaxConstType;

typedef struct
{
  SyntaxKind kind;
  SourceSpan span;

  size_t path_count;
  StringView *paths;

  size_t generic_argument_count;
  SyntaxType *generic_arguments;

  size_t contract_argument_count;
  SyntaxType *contract_arguments;

} SyntaxNamedType;

typedef struct
{
  SyntaxKind kind;
  SourceSpan span;
  SyntaxRefKind ref_kind;
  SyntaxType *inner;
} SyntaxRefType;

typedef struct
{
  SyntaxKind kind;
  SourceSpan span;
  SyntaxConstType *len;
  SyntaxType *inner;
} SyntaxArrayType;

typedef struct
{
  SyntaxKind kind;
  SourceSpan span;

  size_t call_param_count;
  SyntaxCallParameter *call_params;

  SyntaxCallConv callconv;
  SyntaxType *return_type;
} SyntaxFuncType;

union SyntaxType
{
  SyntaxKind kind;
  SyntaxNamedType as_named_type;
  SyntaxRefType as_ref_type;
  SyntaxArrayType as_array_type;
  SyntaxFuncType as_func_type;
  SyntaxConstType as_const_type;
};

// -- decl --
typedef struct
{
  SyntaxKind kind;
  // TODO
} SyntaxNamespaceDecl;

typedef struct
{
  SyntaxKind kind;
  // TODO
} SyntaxUsingDecl;

typedef struct
{
  SyntaxKind kind;
  // TODO
} SyntaxLetDecl;

typedef struct
{
  SyntaxKind kind;
  // TODO
} SyntaxStructDecl;

typedef struct
{
  SyntaxKind kind;
  // TODO
} SyntaxEnumDecl;

typedef struct
{
  SyntaxKind kind;
  // TODO
} SyntaxUnionDecl;

typedef struct
{
  SyntaxKind kind;
  // TODO
} SyntaxVariantDecl;

typedef struct
{
  SyntaxKind kind;
  // TODO
} SyntaxContractDecl;

typedef struct
{
  SyntaxKind kind;
  // TODO
} SyntaxFuncDecl;

union SyntaxDecl
{
  SyntaxKind kind;
  SyntaxNamespaceDecl as_namespace_decl;
  SyntaxUsingDecl as_using_decl;
  SyntaxLetDecl as_let_decl;
  SyntaxStructDecl as_struct_decl;
  SyntaxEnumDecl as_enum_decl;
  SyntaxUnionDecl as_union_decl;
  SyntaxVariantDecl as_variant_decl;
  SyntaxConstType as_contract_decl;
  SyntaxFuncDecl as_func_decl;
};

// -- stmt --

union SyntaxStmt
{
  SyntaxKind kind;
  // TODO
};

// -- expr --

union SyntaxExpr
{
  SyntaxKind kind;
  // TODO
};

// ----
union SyntaxNode
{
  SyntaxKind kind;
  // TODO
};

#endif /* SOLID_AST_H */
