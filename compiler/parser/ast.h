/**
 * @file ast.h
 * @brief AST node types produced by the parser.
 * @author solid-matrix
 * @version 0.0.5
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "span.h"
#include "string_view.h"
#include "syntax_kind.h"

typedef enum {
  SYNTAX_REF_KIND_READWRITE = 0,

  SYNTAX_REF_KIND_READONLY,

  SYNTAX_REF_KIND_WRITEONLY,

} SyntaxRefKind;

typedef enum {
  SYNTAX_CALLCONV_UNDEFINED = 0,

  SYNTAX_CALLCONV_CDECL,

  SYNTAX_CALLCONV_STDCALL,

  SYNTAX_CALLCONV_WINAPI,

  SYNTAX_CALLCONV_THISCALL,

  SYNTAX_CALLCONV_FASTCALL,

} SyntaxCallConv;

typedef enum {
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

typedef struct {
  SyntaxKind kind;
  Span span;
} SyntaxNode;

typedef struct {
  size_t len;
  size_t cap;
  SyntaxNode **nodes;
} SyntaxNodeList;

SyntaxNodeList syntax_node_list_create(void);
void syntax_node_list_append(SyntaxNodeList *list, SyntaxNode *node);
void syntax_node_list_destroy(SyntaxNodeList *list);

typedef struct {
  SyntaxNode header;
  SyntaxNodeList top_levels; // decl nodes
} SyntaxProgram;

typedef struct {
  SyntaxNode header;
  StringView string_view;
} SyntaxIdentifier;

typedef struct {
  SyntaxNode header;
  SyntaxIdentifier *name;
  SyntaxNodeList arguments; // expr nodes
} SyntaxCtAnnotation;

typedef struct {
  SyntaxNode header;
  SyntaxNodeList annotations; // SyntaxCtAnnotation nodes
  SyntaxIdentifier *name;
  SyntaxNode *type; // type node
} SyntaxStructField;

typedef struct {
  SyntaxNode header;
  SyntaxNodeList annotations; // SyntaxCtAnnotation nodes
  SyntaxIdentifier *name;
  SyntaxNode *value; // expr node
} SyntaxEnumField;

typedef struct {
  SyntaxNode header;
  SyntaxNodeList annotations; // SyntaxCtAnnotation nodes
  SyntaxIdentifier *name;
  SyntaxNode *type; // type node
} SyntaxUnionField;

typedef struct {
  SyntaxNode header;
  SyntaxNodeList annotations; // SyntaxCtAnnotation nodes
  SyntaxIdentifier *name;
  SyntaxNode *type; // type node
} SyntaxVariantField;

typedef struct {
  SyntaxNode header;
  SyntaxNodeList annotations; // SyntaxCtAnnotation nodes
  SyntaxIdentifier *name;
  SyntaxNode *type; // type node
} SyntaxGenericParameter;

typedef struct {
  SyntaxNode header;
  SyntaxNodeList annotations; // SyntaxCtAnnotation nodes
  SyntaxIdentifier *name;
  SyntaxNode *type; // type node
} SyntaxCallParameter;

typedef struct {
  SyntaxNode header;
  SyntaxNodeList annotations; // SyntaxCtAnnotation nodes
  SyntaxIdentifier *name;
  SyntaxNode *type; // type node
} SyntaxContractParameter;

typedef struct {
  SyntaxNode header;
  SyntaxIdentifier *name;
  SyntaxNode *value; // expr node
} SyntaxStructLitField;

typedef struct {
  SyntaxNode header;
  SyntaxIdentifier *name;
  SyntaxNode *value; // expr node
} SyntaxContractArgument;

#pragma region TYPE

typedef struct {
  SyntaxNode header;
  SyntaxNode *value; // expr node
} SyntaxConstType;

typedef struct {
  SyntaxNode header;
  SyntaxNodeList paths;             // SyntaxIdentifier nodes
  SyntaxNodeList generic_arguments; // type nodes
} SyntaxNamedType;

typedef struct {
  SyntaxNode header;
  SyntaxRefKind ref_kind;
  SyntaxNode *inner_type; // type node
} SyntaxRefType;

typedef struct {
  SyntaxNode header;
  SyntaxNode *len;        // SyntaxConstType
  SyntaxNode *inner_type; // type node
} SyntaxArrayType;

typedef struct {
  SyntaxNode header;
  SyntaxNodeList call_params; // SyntaxCallParameter nodes
  SyntaxCallConv callconv;
  SyntaxNode *return_type; // type node
} SyntaxFuncType;

#pragma endregion

#pragma region STATEMENT

typedef struct {
  SyntaxNode header;
  SyntaxNodeList stmts; // stmt nodes
} SyntaxBodyStmt;

typedef struct {
  SyntaxNode header;
  SyntaxIdentifier *name;
  SyntaxNode *type;  // type node
  SyntaxNode *value; // expr node
} SyntaxLetStmt;

typedef struct {
  SyntaxNode header;
  SyntaxNode *left;  // expr node
  SyntaxNode *right; // expr node
} SyntaxSetStmt;

typedef struct {
  SyntaxNode header;
  SyntaxNode *expr; // expr node
} SyntaxExprStmt;

typedef struct {
  SyntaxNode header;
  SyntaxNode *condition; // expr node
  SyntaxNode *then_stmt; // SyntaxBodyStmt
  SyntaxNode *else_stmt; // SyntaxIfStmt | SyntaxBodyStmt
} SyntaxIfStmt;

typedef struct {
  SyntaxNode header;
  SyntaxNode *stmt; // SyntaxBodyStmt
} SyntaxLoopStmt;

typedef struct {
  SyntaxNode header;
} SyntaxBreakStmt;

typedef struct {
  SyntaxNode header;
} SyntaxContinueStmt;

typedef struct {
  SyntaxNode header;
  SyntaxNode *expr; // expr node
} SyntaxReturnStmt;

typedef struct {
  SyntaxNode header;
  SyntaxNode *condition; // expr
  SyntaxNode *stmt;      // SyntaxBodyStmt node
} SyntaxWhileStmt;

#pragma endregion

#pragma region EXPRESSION

typedef struct {
  SyntaxNode header;
  StringView value;
} SyntaxNumberLitExpr;

typedef struct {
  SyntaxNode header;
  StringView value;
} SyntaxRuneLitExpr;

typedef struct {
  SyntaxNode header;
  StringView value;
} SyntaxStringLitExpr;

typedef struct {
  SyntaxNode header;
  SyntaxNode *type;      // SyntaxNamedType
  SyntaxNodeList fields; // SyntaxStructLitField
} SyntaxStructLitExpr;

typedef struct {
  SyntaxNode header;
  SyntaxNode *type;        // SyntaxArrayType
  SyntaxNodeList elements; // SyntaxExpr
} SyntaxArrayLitExpr;

typedef struct {
  SyntaxNode header;
  SyntaxNodeList paths;              // SyntaxIdentifier nodes
  SyntaxNodeList generic_arguments;  // type
  SyntaxNodeList contract_arguments; // SyntaxContractArgument
} SyntaxNamedExpr;

typedef struct {
  SyntaxNode header;
  SyntaxOperator operator;
  SyntaxNode *operand; // expr
} SyntaxUnaryExpr;

typedef struct {
  SyntaxNode header;
  SyntaxOperator operator;
  SyntaxNode *left;  // expr
  SyntaxNode *right; // expr
} SyntaxBinaryExpr;

typedef struct {
  SyntaxNode header;
  SyntaxNode *receiver; // expr
  SyntaxIdentifier *name;
} SyntaxDotExpr;

typedef struct {
  SyntaxNode header;
  SyntaxNode *receiver; // expr
  SyntaxNode *index;    // expr
} SyntaxIndexExpr;

typedef struct {
  SyntaxNode header;
  SyntaxNode *callee;       // expr
  SyntaxNodeList arguments; // expr nodes
} SyntaxCallExpr;

typedef struct {
  SyntaxNode header;
  SyntaxIdentifier *name;
  SyntaxNodeList arguments; // expr nodes
} SyntaxCtOperationExpr;

#pragma endregion

#pragma region DECLARATION

typedef struct {
  SyntaxNode header;
  SyntaxNodeList paths; // SyntaxIdentifier nodes
} SyntaxNamespaceDecl;

typedef struct {
  SyntaxNode header;
  SyntaxNodeList paths; // SyntaxIdentifier nodes
} SyntaxUsingDecl;

typedef struct {
  SyntaxNode header;
  SyntaxNodeList annotations; // SyntaxCtAnnotation nodes
  SyntaxIdentifier *name;
  SyntaxNode *type;  // type node
  SyntaxNode *value; // expr node
} SyntaxLetDecl;

typedef struct {
  SyntaxNode header;
  SyntaxNodeList annotations; // SyntaxCtAnnotation nodes
  SyntaxIdentifier *name;
  SyntaxNodeList generic_params; // SyntaxGenericParameter nodes
  SyntaxNodeList fields;         // SyntaxStructField nodes
} SyntaxStructDecl;

typedef struct {
  SyntaxNode header;
  SyntaxNodeList annotations; // SyntaxCtAnnotation nodes
  SyntaxIdentifier *name;
  SyntaxNode *behind_type; // type node
  SyntaxNodeList fields;   // SyntaxEnumField nodes
} SyntaxEnumDecl;

typedef struct {
  SyntaxNode header;
  SyntaxNodeList annotations; // SyntaxCtAnnotation nodes
  SyntaxIdentifier *name;
  SyntaxNodeList generic_params; // SyntaxGenericParameter nodes
  SyntaxNodeList fields;         // SyntaxUnionField nodes
} SyntaxUnionDecl;

typedef struct {
  SyntaxNode header;
  SyntaxNodeList annotations; // SyntaxCtAnnotation nodes
  SyntaxIdentifier *name;
  SyntaxNode *behind_type;       // type node
  SyntaxNodeList generic_params; // SyntaxGenericParameter nodes
  SyntaxNodeList fields;         // SyntaxVariantField nodes
} SyntaxVariantDecl;

typedef struct {
  SyntaxNode header;
  SyntaxNodeList annotations; // SyntaxCtAnnotation nodes
  SyntaxIdentifier *name;
  SyntaxNodeList generic_params; // SyntaxGenericParameter nodes
  SyntaxNodeList call_params;    // SyntaxCallParameter nodes
  SyntaxNode *return_type;       // type node
} SyntaxContractDecl;

typedef struct {
  SyntaxNode header;
  SyntaxNodeList annotations; // SyntaxCtAnnotation nodes
  SyntaxIdentifier *name;
  SyntaxNodeList generic_params;  // SyntaxGenericParameter nodes
  SyntaxNodeList contract_params; // SyntaxContractParameter nodes
  SyntaxNodeList call_params;     // SyntaxCallParameter nodes
  SyntaxCallConv callconv;
  SyntaxNode *return_type; // type node
  SyntaxNodeList fulfills; // type nodes
  SyntaxNode *body;        // SyntaxBodyStmt
} SyntaxFuncDecl;

#pragma endregion
