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

/**
 * @brief Generic node handle: a pointer to the `kind` first member of
 * any node struct. Dereferencing yields the node's kind; cast to the
 * concrete struct after dispatching on it.
 */
typedef SyntaxKind SyntaxNode;

typedef struct
{
  size_t len;
  size_t cap;
  SyntaxNode **nodes;

} SyntaxNodeList;

typedef struct
{
  size_t len;
  size_t cap;
  StringView *names;

} SyntaxPath;

typedef struct
{
  SyntaxKind kind;
  Span span;

  SyntaxNodeList top_levels; // decl nodes

} SyntaxProgram;

typedef struct
{
  SyntaxKind kind;
  Span span;

  StringView name;
  SyntaxNodeList arguments; // expr nodes

} SyntaxCtAnnotation;

typedef struct
{
  SyntaxKind kind;
  Span span;

  SyntaxNodeList annotations; // SyntaxCtAnnotation nodes
  StringView name;
  SyntaxNode *type; // type node

} SyntaxStructField;

typedef struct
{
  SyntaxKind kind;
  Span span;

  SyntaxNodeList annotations; // SyntaxCtAnnotation nodes
  StringView name;
  SyntaxNode *value; // expr node

} SyntaxEnumField;

typedef struct
{
  SyntaxKind kind;
  Span span;

  SyntaxNodeList annotations; // SyntaxCtAnnotation nodes
  StringView name;
  SyntaxNode *type; // type node

} SyntaxUnionField;

typedef struct
{
  SyntaxKind kind;
  Span span;

  SyntaxNodeList annotations; // SyntaxCtAnnotation nodes
  StringView name;
  SyntaxNode *type; // type node

} SyntaxVariantField;

typedef struct
{
  SyntaxKind kind;
  Span span;

  SyntaxNodeList annotations; // SyntaxCtAnnotation nodes
  StringView name;
  SyntaxNode *type; // type node

} SyntaxGenericParameter;

typedef struct
{
  SyntaxKind kind;
  Span span;

  SyntaxNodeList annotations; // SyntaxCtAnnotation nodes
  StringView name;
  SyntaxNode *type; // type node

} SyntaxCallParameter;

typedef struct
{
  SyntaxKind kind;
  Span span;

  SyntaxNodeList annotations; // SyntaxCtAnnotation nodes
  StringView name;
  SyntaxNode *type; // type node

} SyntaxContractParameter;

typedef struct
{
  SyntaxKind kind;
  Span span;

  StringView name;
  SyntaxNode *value; // expr node

} SyntaxStructLitField;

typedef struct
{
  SyntaxKind kind;
  Span span;

  StringView name;
  SyntaxNode *value; // expr node

} SyntaxContractArgument;

#pragma region TYPE

typedef struct
{
  SyntaxKind kind;
  Span span;

  SyntaxNode *value; // expr node

} SyntaxConstType;

typedef struct
{
  SyntaxKind kind;
  Span span;

  SyntaxPath paths;
  SyntaxNodeList generic_arguments; // type nodes

} SyntaxNamedType;

typedef struct
{
  SyntaxKind kind;
  Span span;

  SyntaxRefKind ref_kind;
  SyntaxNode *inner_type; // type node

} SyntaxRefType;

typedef struct
{
  SyntaxKind kind;
  Span span;

  SyntaxNode *len;        // SyntaxConstType
  SyntaxNode *inner_type; // type node

} SyntaxArrayType;

typedef struct
{
  SyntaxKind kind;
  Span span;

  SyntaxNodeList call_params; // SyntaxCallParameter nodes
  SyntaxCallConv callconv;
  SyntaxNode *return_type; // type node

} SyntaxFuncType;

#pragma endregion

#pragma region STATEMENT

typedef struct
{
  SyntaxKind kind;
  Span span;

  SyntaxNodeList stmts; // stmt nodes

} SyntaxBodyStmt;

typedef struct
{
  SyntaxKind kind;
  Span span;

  StringView name;
  SyntaxNode *type;  // type node
  SyntaxNode *value; // expr node

} SyntaxLetStmt;

typedef struct
{
  SyntaxKind kind;
  Span span;

  SyntaxNode *left;  // expr node
  SyntaxNode *right; // expr node

} SyntaxAssignStmt;

typedef struct
{
  SyntaxKind kind;
  Span span;

  SyntaxNode *expr; // expr node

} SyntaxExprStmt;

typedef struct
{
  SyntaxKind kind;
  Span span;

  SyntaxNode *condition; // expr node
  SyntaxNode *then_stmt; // SyntaxBodyStmt
  SyntaxNode *else_stmt; // SyntaxIfStmt | SyntaxBodyStmt

} SyntaxIfStmt;

typedef struct
{
  SyntaxKind kind;
  Span span;

  SyntaxNode *stmt; // SyntaxBodyStmt

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

  SyntaxNode *expr; // expr node

} SyntaxReturnStmt;

typedef struct
{
  SyntaxKind kind;
  Span span;

  SyntaxNode *condition; // expr
  SyntaxNode *stmt;      // SyntaxBodyStmt node

} SyntaxWhileStmt;

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

  SyntaxNode *type;      // SyntaxNamedType
  SyntaxNodeList fields; // SyntaxStructLitField

} SyntaxStructLitExpr;

typedef struct
{
  SyntaxKind kind;
  Span span;

  SyntaxNode *type;        // SyntaxArrayType
  SyntaxNodeList elements; // SyntaxExpr

} SyntaxArrayLitExpr;

typedef struct
{
  SyntaxKind kind;
  Span span;

  SyntaxPath paths;
  SyntaxNodeList generic_arguments;  // type
  SyntaxNodeList contract_arguments; // SyntaxContractArgument

} SyntaxNamedExpr;

typedef struct
{
  SyntaxKind kind;
  Span span;

  SyntaxOperator operator;
  SyntaxNode *operand; // expr

} SyntaxUnaryExpr;

typedef struct
{
  SyntaxKind kind;
  Span span;

  SyntaxOperator operator;
  SyntaxNode *left;  // expr
  SyntaxNode *right; // expr

} SyntaxBinaryExpr;

typedef struct
{
  SyntaxKind kind;
  Span span;

  SyntaxNode *receiver; // expr
  StringView name;

} SyntaxDotExpr;

typedef struct
{
  SyntaxKind kind;
  Span span;

  SyntaxNode *receiver; // expr
  SyntaxNode *index;    // expr

} SyntaxIndexExpr;

typedef struct
{
  SyntaxKind kind;
  Span span;

  SyntaxNode *callee;       // expr
  SyntaxNodeList arguments; // expr nodes

} SyntaxCallExpr;

typedef struct
{
  SyntaxKind kind;
  Span span;

  StringView name;
  SyntaxNodeList arguments; // expr nodes

} SyntaxCtOperationExpr;

#pragma endregion

#pragma region DECLARATION

typedef struct
{
  SyntaxKind kind;
  Span span;

  SyntaxPath paths;

} SyntaxNamespaceDecl;

typedef struct
{
  SyntaxKind kind;
  Span span;

  SyntaxPath paths;

} SyntaxUsingDecl;

typedef struct
{
  SyntaxKind kind;
  Span span;

  SyntaxNodeList annotations; // SyntaxCtAnnotation nodes
  StringView name;
  SyntaxNode *type;  // type node
  SyntaxNode *value; // expr node

} SyntaxLetDecl;

typedef struct
{
  SyntaxKind kind;
  Span span;

  SyntaxNodeList annotations; // SyntaxCtAnnotation nodes
  StringView name;
  SyntaxNodeList generic_params; // SyntaxGenericParameter nodes
  SyntaxNodeList fields;         // SyntaxStructField nodes

} SyntaxStructDecl;

typedef struct
{
  SyntaxKind kind;
  Span span;

  SyntaxNodeList annotations; // SyntaxCtAnnotation nodes
  StringView name;
  SyntaxNode *behind_type; // type node
  SyntaxNodeList fields;   // SyntaxEnumField nodes

} SyntaxEnumDecl;

typedef struct
{
  SyntaxKind kind;
  Span span;

  SyntaxNodeList annotations; // SyntaxCtAnnotation nodes
  StringView name;
  SyntaxNodeList generic_params; // SyntaxGenericParameter nodes
  SyntaxNodeList fields;         // SyntaxUnionField nodes

} SyntaxUnionDecl;

typedef struct
{
  SyntaxKind kind;
  Span span;

  SyntaxNodeList annotations; // SyntaxCtAnnotation nodes
  StringView name;
  SyntaxNode *behind_type;       // type node
  SyntaxNodeList generic_params; // SyntaxGenericParameter nodes
  SyntaxNodeList fields;         // SyntaxVariantField nodes

} SyntaxVariantDecl;

typedef struct
{
  SyntaxKind kind;
  Span span;

  SyntaxNodeList annotations; // SyntaxCtAnnotation nodes
  StringView name;
  SyntaxNodeList generic_params; // SyntaxGenericParameter nodes
  SyntaxNodeList call_params;    // SyntaxCallParameter nodes
  SyntaxNode *return_type;       // type node

} SyntaxContractDecl;

typedef struct
{
  SyntaxKind kind;
  Span span;

  SyntaxNodeList annotations; // SyntaxCtAnnotation nodes
  StringView name;
  SyntaxNodeList generic_params;  // SyntaxGenericParameter nodes
  SyntaxNodeList contract_params; // SyntaxContractParameter nodes
  SyntaxNodeList call_params;     // SyntaxCallParameter nodes
  SyntaxCallConv callconv;
  SyntaxNode *return_type; // type node
  SyntaxNodeList fulfills; // type nodes
  SyntaxNode *body;        // SyntaxBodyStmt

} SyntaxFuncDecl;

#pragma endregion

#endif /* SOLID_AST_H */
