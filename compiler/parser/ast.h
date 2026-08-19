/**
 * @file ast.h
 * @brief AST node types produced by the parser.
 * @author solid-matrix
 * @version 0.0.5
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "source.h"
#include "string_view.h"
#include "syntax_kind.h"
#include "syntax_node.h"

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

typedef SyntaxNode SyntaxHeader;

typedef struct
{
  size_t len;
  size_t cap;
  SyntaxNode **nodes;

} SyntaxNodeList;

SyntaxNodeList syntax_node_list_create(void);
void syntax_node_list_append(SyntaxNodeList *list, SyntaxNode *node);
void syntax_node_list_destroy(SyntaxNodeList *list);

typedef struct
{
  size_t len;
  size_t cap;
  StringView *names;

} SyntaxPath;

SyntaxPath syntax_path_create(void);
void syntax_path_append(SyntaxPath *path, StringView name);
void syntax_path_destroy(SyntaxPath *path);

typedef struct
{
  SyntaxHeader header;
  SyntaxNodeList top_levels; // decl nodes
} SyntaxProgram;

typedef struct
{
  SyntaxHeader header;
  StringView string_view;
} SyntaxIdentifier;

typedef struct
{
  SyntaxHeader header;
  StringView name;
  SyntaxNodeList arguments; // expr nodes
} SyntaxCtAnnotation;

typedef struct
{
  SyntaxHeader header;
  SyntaxNodeList annotations; // SyntaxCtAnnotation nodes
  StringView name;
  SyntaxNode *type; // type node
} SyntaxStructField;

typedef struct
{
  SyntaxHeader header;
  SyntaxNodeList annotations; // SyntaxCtAnnotation nodes
  StringView name;
  SyntaxNode *value; // expr node
} SyntaxEnumField;

typedef struct
{
  SyntaxHeader header;
  SyntaxNodeList annotations; // SyntaxCtAnnotation nodes
  StringView name;
  SyntaxNode *type; // type node
} SyntaxUnionField;

typedef struct
{
  SyntaxHeader header;
  SyntaxNodeList annotations; // SyntaxCtAnnotation nodes
  StringView name;
  SyntaxNode *type; // type node
} SyntaxVariantField;

typedef struct
{
  SyntaxHeader header;
  SyntaxNodeList annotations; // SyntaxCtAnnotation nodes
  StringView name;
  SyntaxNode *type; // type node
} SyntaxGenericParameter;

typedef struct
{
  SyntaxHeader header;
  SyntaxNodeList annotations; // SyntaxCtAnnotation nodes
  StringView name;
  SyntaxNode *type; // type node
} SyntaxCallParameter;

typedef struct
{
  SyntaxHeader header;
  SyntaxNodeList annotations; // SyntaxCtAnnotation nodes
  StringView name;
  SyntaxNode *type; // type node
} SyntaxContractParameter;

typedef struct
{
  SyntaxHeader header;
  StringView name;
  SyntaxNode *value; // expr node
} SyntaxStructLitField;

typedef struct
{
  SyntaxHeader header;
  StringView name;
  SyntaxNode *value; // expr node
} SyntaxContractArgument;

#pragma region TYPE

typedef struct
{
  SyntaxHeader header;
  SyntaxNode *value; // expr node
} SyntaxConstType;

typedef struct
{
  SyntaxHeader header;
  SyntaxPath paths;
  SyntaxNodeList generic_arguments; // type nodes
} SyntaxNamedType;

typedef struct
{
  SyntaxHeader header;
  SyntaxRefKind ref_kind;
  SyntaxNode *inner_type; // type node
} SyntaxRefType;

typedef struct
{
  SyntaxHeader header;
  SyntaxNode *len;        // SyntaxConstType
  SyntaxNode *inner_type; // type node
} SyntaxArrayType;

typedef struct
{
  SyntaxHeader header;
  SyntaxNodeList call_params; // SyntaxCallParameter nodes
  SyntaxCallConv callconv;
  SyntaxNode *return_type; // type node
} SyntaxFuncType;

#pragma endregion

#pragma region STATEMENT

typedef struct
{
  SyntaxHeader header;
  SyntaxNodeList stmts; // stmt nodes
} SyntaxBodyStmt;

typedef struct
{
  SyntaxHeader header;
  StringView name;
  SyntaxNode *type;  // type node
  SyntaxNode *value; // expr node
} SyntaxLetStmt;

typedef struct
{
  SyntaxHeader header;
  SyntaxNode *left;  // expr node
  SyntaxNode *right; // expr node
} SyntaxAssignStmt;

typedef struct
{
  SyntaxHeader header;
  SyntaxNode *expr; // expr node
} SyntaxExprStmt;

typedef struct
{
  SyntaxHeader header;
  SyntaxNode *condition; // expr node
  SyntaxNode *then_stmt; // SyntaxBodyStmt
  SyntaxNode *else_stmt; // SyntaxIfStmt | SyntaxBodyStmt
} SyntaxIfStmt;

typedef struct
{
  SyntaxHeader header;
  SyntaxNode *stmt; // SyntaxBodyStmt
} SyntaxLoopStmt;

typedef struct
{
  SyntaxHeader header;
} SyntaxBreakStmt;

typedef struct
{
  SyntaxHeader header;
} SyntaxContinueStmt;

typedef struct
{
  SyntaxHeader header;
  SyntaxNode *expr; // expr node
} SyntaxReturnStmt;

typedef struct
{
  SyntaxHeader header;
  SyntaxNode *condition; // expr
  SyntaxNode *stmt;      // SyntaxBodyStmt node
} SyntaxWhileStmt;

#pragma endregion

#pragma region EXPRESSION

typedef struct
{
  SyntaxHeader header;
  StringView value;
  StringView suffix;
} SyntaxIntLitExpr;

typedef struct
{
  SyntaxHeader header;
  StringView value;
  StringView suffix;
} SyntaxFloatLitExpr;

typedef struct
{
  SyntaxHeader header;
  StringView value;
} SyntaxRuneLitExpr;

typedef struct
{
  SyntaxHeader header;
  StringView value;
} SyntaxStringLitExpr;

typedef struct
{
  SyntaxHeader header;
  SyntaxNode *type;      // SyntaxNamedType
  SyntaxNodeList fields; // SyntaxStructLitField
} SyntaxStructLitExpr;

typedef struct
{
  SyntaxHeader header;
  SyntaxNode *type;        // SyntaxArrayType
  SyntaxNodeList elements; // SyntaxExpr
} SyntaxArrayLitExpr;

typedef struct
{
  SyntaxHeader header;
  SyntaxPath paths;
  SyntaxNodeList generic_arguments;  // type
  SyntaxNodeList contract_arguments; // SyntaxContractArgument
} SyntaxNamedExpr;

typedef struct
{
  SyntaxHeader header;
  SyntaxOperator operator;
  SyntaxNode *operand; // expr
} SyntaxUnaryExpr;

typedef struct
{
  SyntaxHeader header;
  SyntaxOperator operator;
  SyntaxNode *left;  // expr
  SyntaxNode *right; // expr
} SyntaxBinaryExpr;

typedef struct
{
  SyntaxHeader header;
  SyntaxNode *receiver; // expr
  StringView name;
} SyntaxDotExpr;

typedef struct
{
  SyntaxHeader header;
  SyntaxNode *receiver; // expr
  SyntaxNode *index;    // expr
} SyntaxIndexExpr;

typedef struct
{
  SyntaxHeader header;
  SyntaxNode *callee;       // expr
  SyntaxNodeList arguments; // expr nodes
} SyntaxCallExpr;

typedef struct
{
  SyntaxHeader header;
  StringView name;
  SyntaxNodeList arguments; // expr nodes
} SyntaxCtOperationExpr;

#pragma endregion

#pragma region DECLARATION

typedef struct
{
  SyntaxHeader header;
  SyntaxPath paths;
} SyntaxNamespaceDecl;

typedef struct
{
  SyntaxHeader header;
  SyntaxPath paths;
} SyntaxUsingDecl;

typedef struct
{
  SyntaxHeader header;
  SyntaxNodeList annotations; // SyntaxCtAnnotation nodes
  StringView name;
  SyntaxNode *type;  // type node
  SyntaxNode *value; // expr node
} SyntaxLetDecl;

typedef struct
{
  SyntaxHeader header;
  SyntaxNodeList annotations; // SyntaxCtAnnotation nodes
  StringView name;
  SyntaxNodeList generic_params; // SyntaxGenericParameter nodes
  SyntaxNodeList fields;         // SyntaxStructField nodes
} SyntaxStructDecl;

typedef struct
{
  SyntaxHeader header;
  SyntaxNodeList annotations; // SyntaxCtAnnotation nodes
  StringView name;
  SyntaxNode *behind_type; // type node
  SyntaxNodeList fields;   // SyntaxEnumField nodes
} SyntaxEnumDecl;

typedef struct
{
  SyntaxHeader header;
  SyntaxNodeList annotations; // SyntaxCtAnnotation nodes
  StringView name;
  SyntaxNodeList generic_params; // SyntaxGenericParameter nodes
  SyntaxNodeList fields;         // SyntaxUnionField nodes
} SyntaxUnionDecl;

typedef struct
{
  SyntaxHeader header;
  SyntaxNodeList annotations; // SyntaxCtAnnotation nodes
  StringView name;
  SyntaxNode *behind_type;       // type node
  SyntaxNodeList generic_params; // SyntaxGenericParameter nodes
  SyntaxNodeList fields;         // SyntaxVariantField nodes
} SyntaxVariantDecl;

typedef struct
{
  SyntaxHeader header;
  SyntaxNodeList annotations; // SyntaxCtAnnotation nodes
  StringView name;
  SyntaxNodeList generic_params; // SyntaxGenericParameter nodes
  SyntaxNodeList call_params;    // SyntaxCallParameter nodes
  SyntaxNode *return_type;       // type node
} SyntaxContractDecl;

typedef struct
{
  SyntaxHeader header;
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
