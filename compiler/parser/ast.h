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
 * Standalone nodes (program, fields, annotations, parameters) use plain
 * values in the low byte; they have no category:
 * (kind & SYNTAX_KIND_CATEGORY_MASK) == 0.
 *
 * Categorized nodes (type/expr/stmt/decl) are built as
 * <CATEGORY_MASK> | <ordinal 0x01..0xFF>. Category membership is tested
 * with (kind & SYNTAX_KIND_CATEGORY_MASK) == <CATEGORY_MASK>, or the bit
 * test (kind & <CATEGORY_MASK>) != 0. Never compare a kind to a mask
 * with ==: ordinal 0x00 is reserved for the masks themselves.
 */
typedef enum
{
  SYNTAX_KIND_INVALID = 0x0000,

  /* standalone nodes */
  SYNTAX_KIND_PROGRAM = 0x0001,

  SYNTAX_KIND_CT_ANNOTATION = 0x0002,

  SYNTAX_KIND_STRUCT_FIELD = 0x0003,

  SYNTAX_KIND_ENUM_FIELD = 0x0004,

  SYNTAX_KIND_UNION_FIELD = 0x0005,

  SYNTAX_KIND_VARIANT_FIELD = 0x0006,

  SYNTAX_KIND_GENERIC_PARAMETER = 0x0007,

  SYNTAX_KIND_CALL_PARAMETER = 0x0008,

  SYNTAX_KIND_CONTRACT_PARAMETER = 0x0009,

  SYNTAX_KIND_STRUCT_LIT_FIELD = 0x000A,

  SYNTAX_KIND_CONTRACT_ARGUMENT = 0x000B,

  /* category masks: one bit per category in the high byte */
  SYNTAX_KIND_CATEGORY_MASK = 0xFF00,

  SYNTAX_KIND_TYPE_MASK = 0x0100,

  SYNTAX_KIND_EXPR_MASK = 0x0200,

  SYNTAX_KIND_STMT_MASK = 0x0400,

  SYNTAX_KIND_DECL_MASK = 0x0800,

  /* type nodes */
  SYNTAX_KIND_CONST_TYPE = SYNTAX_KIND_TYPE_MASK | 0x01,

  SYNTAX_KIND_NAMED_TYPE = SYNTAX_KIND_TYPE_MASK | 0x02,

  SYNTAX_KIND_REF_TYPE = SYNTAX_KIND_TYPE_MASK | 0x03,

  SYNTAX_KIND_ARRAY_TYPE = SYNTAX_KIND_TYPE_MASK | 0x04,

  SYNTAX_KIND_FUNC_TYPE = SYNTAX_KIND_TYPE_MASK | 0x05,

  /* statement nodes */
  SYNTAX_KIND_BODY_STMT = SYNTAX_KIND_STMT_MASK | 0x01,

  SYNTAX_KIND_LET_STMT = SYNTAX_KIND_STMT_MASK | 0x02,

  SYNTAX_KIND_ASSIGN_STMT = SYNTAX_KIND_STMT_MASK | 0x03,

  SYNTAX_KIND_EXPR_STMT = SYNTAX_KIND_STMT_MASK | 0x04,

  SYNTAX_KIND_IF_STMT = SYNTAX_KIND_STMT_MASK | 0x05,

  SYNTAX_KIND_LOOP_STMT = SYNTAX_KIND_STMT_MASK | 0x06,

  SYNTAX_KIND_BREAK_STMT = SYNTAX_KIND_STMT_MASK | 0x07,

  SYNTAX_KIND_CONTINUE_STMT = SYNTAX_KIND_STMT_MASK | 0x08,

  SYNTAX_KIND_RETURN_STMT = SYNTAX_KIND_STMT_MASK | 0x09,

  SYNTAX_KIND_WHILE_STMT = SYNTAX_KIND_STMT_MASK | 0x0A,

  /* expression nodes */
  SYNTAX_KIND_INT_LIT_EXPR = SYNTAX_KIND_EXPR_MASK | 0x01,

  SYNTAX_KIND_FLOAT_LIT_EXPR = SYNTAX_KIND_EXPR_MASK | 0x02,

  SYNTAX_KIND_RUNE_LIT_EXPR = SYNTAX_KIND_EXPR_MASK | 0x03,

  SYNTAX_KIND_STRING_LIT_EXPR = SYNTAX_KIND_EXPR_MASK | 0x04,

  SYNTAX_KIND_STRUCT_LIT_EXPR = SYNTAX_KIND_EXPR_MASK | 0x05,

  SYNTAX_KIND_ARRAY_LIT_EXPR = SYNTAX_KIND_EXPR_MASK | 0x06,

  SYNTAX_KIND_NAMED_EXPR = SYNTAX_KIND_EXPR_MASK | 0x07,

  SYNTAX_KIND_UNARY_EXPR = SYNTAX_KIND_EXPR_MASK | 0x08,

  SYNTAX_KIND_BINARY_EXPR = SYNTAX_KIND_EXPR_MASK | 0x09,

  SYNTAX_KIND_DOT_EXPR = SYNTAX_KIND_EXPR_MASK | 0x0A,

  SYNTAX_KIND_INDEX_EXPR = SYNTAX_KIND_EXPR_MASK | 0x0B,

  SYNTAX_KIND_CALL_EXPR = SYNTAX_KIND_EXPR_MASK | 0x0C,

  SYNTAX_KIND_CT_OPERATION_EXPR = SYNTAX_KIND_EXPR_MASK | 0x0D,

  /* declaration nodes */
  SYNTAX_KIND_NAMESPACE_DECL = SYNTAX_KIND_DECL_MASK | 0x01,

  SYNTAX_KIND_USING_DECL = SYNTAX_KIND_DECL_MASK | 0x02,

  SYNTAX_KIND_LET_DECL = SYNTAX_KIND_DECL_MASK | 0x03,

  SYNTAX_KIND_STRUCT_DECL = SYNTAX_KIND_DECL_MASK | 0x04,

  SYNTAX_KIND_ENUM_DECL = SYNTAX_KIND_DECL_MASK | 0x05,

  SYNTAX_KIND_UNION_DECL = SYNTAX_KIND_DECL_MASK | 0x06,

  SYNTAX_KIND_VARIANT_DECL = SYNTAX_KIND_DECL_MASK | 0x07,

  SYNTAX_KIND_CONTRACT_DECL = SYNTAX_KIND_DECL_MASK | 0x08,

  SYNTAX_KIND_FUNC_DECL = SYNTAX_KIND_DECL_MASK | 0x09,

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

typedef enum
{
  SYNTAX_OPERATOR_PLUS,  // +
  SYNTAX_OPERATOR_MINUS, // -

  SYNTAX_OPERATOR_ADD, // +
  SYNTAX_OPERATOR_SUB, // -
  SYNTAX_OPERATOR_MUL, // *
  SYNTAX_OPERATOR_DIV, // /
  SYNTAX_OPERATOR_MOD, // %

  SYNTAX_OPERATOR_EQ,  // ==
  SYNTAX_OPERATOR_NEQ, // !=
  SYNTAX_OPERATOR_LT,  // <
  SYNTAX_OPERATOR_GT,  // >
  SYNTAX_OPERATOR_LTE, // <=
  SYNTAX_OPERATOR_GTE, // >=

  SYNTAX_OPERATOR_LNOT, // !
  SYNTAX_OPERATOR_LAND, // &&
  SYNTAX_OPERATOR_LOR,  // ||
  SYNTAX_OPERATOR_LXOR, // ^^

  SYNTAX_OPERATOR_BNOT, // ~
  SYNTAX_OPERATOR_BAND, // &
  SYNTAX_OPERATOR_BOR,  // |
  SYNTAX_OPERATOR_BXOR, // ^

  SYNTAX_OPERATOR_SHL, // <<
  SYNTAX_OPERATOR_SHR, // >>
} SyntaxOperator;

// typedef union SyntaxNode SyntaxNode;

typedef union SyntaxType SyntaxType;

typedef union SyntaxExpr SyntaxExpr;

typedef union SyntaxStmt SyntaxStmt;

typedef union SyntaxDecl SyntaxDecl;

#pragma region SHARED

typedef struct
{
  SyntaxKind kind;
  Span span;

  size_t top_level_count;
  SyntaxDecl *top_levels;

} SyntaxProgram;

typedef struct
{
  SyntaxKind kind;
  Span span;

  StringView name;

  size_t argument_count;
  SyntaxExpr *arguments;

} SyntaxCtAnnotation;

typedef struct
{
  SyntaxKind kind;
  Span span;

  size_t annotation_count;
  SyntaxCtAnnotation *annotations;

  StringView name;

  SyntaxType *type;

} SyntaxStructField;

typedef struct
{
  SyntaxKind kind;
  Span span;

  size_t annotation_count;
  SyntaxCtAnnotation *annotations;

  StringView name;

  SyntaxExpr *expr;

} SyntaxEnumField;

typedef struct
{
  SyntaxKind kind;
  Span span;

  size_t annotation_count;
  SyntaxCtAnnotation *annotations;

  StringView name;

  SyntaxType *type;

} SyntaxUnionField;

typedef struct
{
  SyntaxKind kind;
  Span span;

  size_t annotation_count;
  SyntaxCtAnnotation *annotations;

  StringView name;

  SyntaxType *type;

} SyntaxVariantField;

typedef struct
{
  SyntaxKind kind;
  Span span;

  size_t annotation_count;
  SyntaxCtAnnotation *annotations;

  StringView name;

  SyntaxType *type;

} SyntaxGenericParameter;

typedef struct
{
  SyntaxKind kind;
  Span span;

  size_t annotation_count;
  SyntaxCtAnnotation *annotations;

  StringView name;

  SyntaxType *type;

} SyntaxCallParameter;

typedef struct
{
  SyntaxKind kind;
  Span span;

  size_t annotation_count;
  SyntaxCtAnnotation *annotations;

  StringView name;
  SyntaxType *type;
} SyntaxContractParameter;

typedef struct
{
  SyntaxKind kind;
  Span span;

  StringView name;

  SyntaxExpr *expr;

} SyntaxStructLitField;

typedef struct
{
  SyntaxKind kind;
  Span span;

  StringView name;

  SyntaxExpr *expr;

} SyntaxContractArgument;

#pragma endregion

#pragma region TYPE

typedef struct
{
  SyntaxKind kind;
  Span span;

  SyntaxExpr *expr;

} SyntaxConstType;

typedef struct
{
  SyntaxKind kind;
  Span span;

  size_t path_count;
  StringView *paths;

  size_t generic_argument_count;
  SyntaxType *generic_arguments;

} SyntaxNamedType;

typedef struct
{
  SyntaxKind kind;
  Span span;

  SyntaxRefKind ref_kind;

  SyntaxType *inner;

} SyntaxRefType;

typedef struct
{
  SyntaxKind kind;
  Span span;

  SyntaxConstType *len;

  SyntaxType *inner;

} SyntaxArrayType;

typedef struct
{
  SyntaxKind kind;
  Span span;

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

#pragma endregion

#pragma region STATEMENT

typedef struct
{
  SyntaxKind kind;
  Span span;

  size_t stmt_count;
  SyntaxStmt *stmts;

} SyntaxBodyStmt;

typedef struct
{
  SyntaxKind kind;
  Span span;

  StringView name;

  SyntaxType *type;

  SyntaxExpr *expr;

} SyntaxLetStmt;

typedef struct
{
  SyntaxKind kind;
  Span span;

  SyntaxExpr *left;

  SyntaxExpr *right;

} SyntaxAssignStmt;

typedef struct
{
  SyntaxKind kind;
  Span span;

  SyntaxExpr *expr;

} SyntaxExprStmt;

typedef struct
{
  SyntaxKind kind;
  Span span;

  SyntaxExpr *condition;

  SyntaxBodyStmt *then_body;

  SyntaxStmt *else_stmt;

} SyntaxIfStmt;

typedef struct
{
  SyntaxKind kind;
  Span span;

  SyntaxBodyStmt *body;

} SyntaxLoopStmt;

typedef struct
{
  SyntaxKind kind;
  Span span;

} SyntaxBreakStmt;

typedef struct
{
  SyntaxKind kind;
  Span span;

} SyntaxContinueStmt;

typedef struct
{
  SyntaxKind kind;
  Span span;

  SyntaxExpr *expr;

} SyntaxReturnStmt;

typedef struct
{
  SyntaxKind kind;
  Span span;

  SyntaxExpr *condition;

  SyntaxBodyStmt *body;

} SyntaxWhileStmt;

union SyntaxStmt
{
  SyntaxKind kind;

  SyntaxBodyStmt as_body_stmt;
  SyntaxLetStmt as_let_stmt;
  SyntaxAssignStmt as_assign_stmt;
  SyntaxExprStmt as_expr_stmt;
  SyntaxIfStmt as_if_stmt;
  SyntaxLoopStmt as_loop_stmt;
  SyntaxBreakStmt as_break_stmt;
  SyntaxContinueStmt as_continue_stmt;
  SyntaxReturnStmt as_return_stmt;
  SyntaxWhileStmt as_while_stmt;
};

#pragma endregion

#pragma region EXPRESSION

typedef struct
{
  SyntaxKind kind;
  Span span;

  StringView value;

  StringView suffix;

} SyntaxIntLitExpr;

typedef struct
{
  SyntaxKind kind;
  Span span;

  StringView value;

  StringView suffix;

} SyntaxFloatLitExpr;

typedef struct
{
  SyntaxKind kind;
  Span span;

  StringView value;

} SyntaxRuneLitExpr;

typedef struct
{
  SyntaxKind kind;
  Span span;

  StringView value;

} SyntaxStringLitExpr;

typedef struct
{
  SyntaxKind kind;
  Span span;

  SyntaxNamedType *type;

  size_t field_count;
  SyntaxStructLitField *fields;

} SyntaxStructLitExpr;

typedef struct
{
  SyntaxKind kind;
  Span span;

  SyntaxArrayType *type;

  size_t element_count;
  SyntaxExpr *elements;

} SyntaxArrayLitExpr;

typedef struct
{
  SyntaxKind kind;
  Span span;

  size_t path_count;
  StringView *paths;

  size_t generic_argument_count;
  SyntaxType *generic_arguments;

  size_t contract_argument_count;
  SyntaxContractArgument *contract_arguments;

} SyntaxNamedExpr;

typedef struct
{
  SyntaxKind kind;
  Span span;

  SyntaxOperator operator;

  SyntaxExpr *operand;

} SyntaxUnaryExpr;

typedef struct
{
  SyntaxKind kind;
  Span span;

  SyntaxOperator operator;

  SyntaxExpr *left;

  SyntaxExpr *right;

} SyntaxBinaryExpr;

typedef struct
{
  SyntaxKind kind;
  Span span;

  SyntaxExpr *receiver;

  StringView name;

} SyntaxDotExpr;

typedef struct
{
  SyntaxKind kind;
  Span span;

  SyntaxExpr *receiver;

  SyntaxExpr *index;

} SyntaxIndexExpr;

typedef struct
{
  SyntaxKind kind;
  Span span;

  SyntaxExpr *callee;

  size_t argument_count;
  SyntaxExpr *arguments;

} SyntaxCallExpr;

typedef struct
{
  SyntaxKind kind;
  Span span;

  StringView name;

  size_t argument_count;
  SyntaxExpr *arguments;

} SyntaxCtOperationExpr;

union SyntaxExpr
{
  SyntaxKind kind;
  SyntaxIntLitExpr as_int_lit_expr;
  SyntaxFloatLitExpr as_float_lit_expr;
  SyntaxRuneLitExpr as_rune_lit_expr;
  SyntaxStringLitExpr as_string_lit_expr;
  SyntaxStructLitExpr as_struct_lit_expr;
  SyntaxArrayLitExpr as_array_lit_expr;
  SyntaxNamedExpr as_named_expr;
  SyntaxUnaryExpr as_unary_expr;
  SyntaxBinaryExpr as_binary_expr;
  SyntaxDotExpr as_dot_expr;
  SyntaxIndexExpr as_index_expr;
  SyntaxCallExpr as_call_expr;
  SyntaxCtOperationExpr as_ct_operation_expr;
};

#pragma endregion

#pragma region DECLARATION

typedef struct
{
  SyntaxKind kind;
  Span span;

  size_t path_count;
  StringView *paths;

} SyntaxNamespaceDecl;

typedef struct
{
  SyntaxKind kind;
  Span span;

  size_t path_count;
  StringView *paths;

} SyntaxUsingDecl;

typedef struct
{
  SyntaxKind kind;
  Span span;

  size_t annotation_count;
  SyntaxCtAnnotation *annotations;

  StringView name;

  SyntaxType *type;

  SyntaxExpr *expr;

} SyntaxLetDecl;

typedef struct
{
  SyntaxKind kind;
  Span span;

  size_t annotation_count;
  SyntaxCtAnnotation *annotations;

  StringView name;

  size_t generic_parameter_count;
  SyntaxGenericParameter *generic_parameters;

  size_t field_count;
  SyntaxStructField *fields;

} SyntaxStructDecl;

typedef struct
{
  SyntaxKind kind;
  Span span;

  size_t annotation_count;
  SyntaxCtAnnotation *annotations;

  StringView name;
  SyntaxType *behind_type;

  size_t field_count;
  SyntaxEnumField *fields;

} SyntaxEnumDecl;

typedef struct
{
  SyntaxKind kind;
  Span span;

  size_t annotation_count;
  SyntaxCtAnnotation *annotations;

  StringView name;

  size_t generic_parameter_count;
  SyntaxGenericParameter *generic_parameters;

  size_t field_count;
  SyntaxUnionField *fields;

} SyntaxUnionDecl;

typedef struct
{
  SyntaxKind kind;
  Span span;

  size_t annotation_count;
  SyntaxCtAnnotation *annotations;

  StringView name;

  SyntaxType *behind_type;

  size_t generic_parameter_count;
  SyntaxGenericParameter *generic_parameters;

  size_t field_count;
  SyntaxVariantField *fields;

} SyntaxVariantDecl;

typedef struct
{
  SyntaxKind kind;
  Span span;

  size_t annotation_count;
  SyntaxCtAnnotation *annotations;

  StringView name;

  size_t generic_parameter_count;
  SyntaxGenericParameter *generic_parameters;

  size_t call_parameter_count;
  SyntaxCallParameter *call_parameters;

  SyntaxType *return_type;

} SyntaxContractDecl;

typedef struct
{
  SyntaxKind kind;
  Span span;

  size_t annotation_count;
  SyntaxCtAnnotation *annotations;

  StringView name; // 16

  size_t generic_parameter_count;
  SyntaxGenericParameter *generic_parameters;

  size_t contract_parameter_count;
  SyntaxContractParameter *contract_parameters;

  size_t call_parameter_count;
  SyntaxCallParameter *call_parameters;

  SyntaxCallConv callconv;
  SyntaxType *return_type;

  size_t fulfill_count;
  SyntaxType *fulfills;

  SyntaxBodyStmt *body;

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
  SyntaxContractDecl as_contract_decl;
  SyntaxFuncDecl as_func_decl;
};

#pragma endregion

// /**
//  * @brief The union of every AST node; used for top-level routing.
//  *
//  * Peer of the category unions (SyntaxType/SyntaxExpr/SyntaxStmt/
//  * SyntaxDecl); unlike them it also covers the standalone nodes
//  * (program, fields, annotations, parameters). All node structs share
//  * the common initial sequence starting with `kind`.
//  */
// union SyntaxNode
// {
//   SyntaxKind kind;

//   /* standalone nodes */
//   SyntaxProgram as_program;

//   SyntaxCtAnnotation as_ct_annotation;
//   SyntaxStructField as_struct_field;
//   SyntaxEnumField as_enum_field;
//   SyntaxUnionField as_union_field;
//   SyntaxVariantField as_variant_field;
//   SyntaxGenericParameter as_generic_parameter;
//   SyntaxCallParameter as_call_parameter;
//   SyntaxContractParameter as_contract_parameter;
//   SyntaxStructLitField as_struct_lit_field;
//   SyntaxContractArgument as_contract_argument;

//   /* type nodes */
//   SyntaxConstType as_const_type;
//   SyntaxNamedType as_named_type;
//   SyntaxRefType as_ref_type;
//   SyntaxArrayType as_array_type;
//   SyntaxFuncType as_func_type;

//   /* statement nodes */
//   SyntaxBodyStmt as_body_stmt;
//   SyntaxLetStmt as_let_stmt;
//   SyntaxAssignStmt as_assign_stmt;
//   SyntaxExprStmt as_expr_stmt;
//   SyntaxIfStmt as_if_stmt;
//   SyntaxLoopStmt as_loop_stmt;
//   SyntaxBreakStmt as_break_stmt;
//   SyntaxContinueStmt as_continue_stmt;
//   SyntaxReturnStmt as_return_stmt;
//   SyntaxWhileStmt as_while_stmt;

//   /* expression nodes */
//   SyntaxIntLitExpr as_int_lit_expr;
//   SyntaxFloatLitExpr as_float_lit_expr;
//   SyntaxRuneLitExpr as_rune_lit_expr;
//   SyntaxStringLitExpr as_string_lit_expr;
//   SyntaxStructLitExpr as_struct_lit_expr;
//   SyntaxArrayLitExpr as_array_lit_expr;
//   SyntaxNamedExpr as_named_expr;
//   SyntaxUnaryExpr as_unary_expr;
//   SyntaxBinaryExpr as_binary_expr;
//   SyntaxDotExpr as_dot_expr;
//   SyntaxIndexExpr as_index_expr;
//   SyntaxCallExpr as_call_expr;
//   SyntaxCtOperationExpr as_ct_operation_expr;

//   /* declaration nodes */
//   SyntaxNamespaceDecl as_namespace_decl;
//   SyntaxUsingDecl as_using_decl;
//   SyntaxLetDecl as_let_decl;
//   SyntaxStructDecl as_struct_decl;
//   SyntaxEnumDecl as_enum_decl;
//   SyntaxUnionDecl as_union_decl;
//   SyntaxVariantDecl as_variant_decl;
//   SyntaxContractDecl as_contract_decl;
//   SyntaxFuncDecl as_func_decl;
// };

#endif /* SOLID_AST_H */
