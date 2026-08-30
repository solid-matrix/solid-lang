/**
 * @file syntax_node.h
 * @brief Every AST node: kind tags, the common header, the chain type and all node structs.
 * @author solid-matrix
 * @version 0.0.5
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "span.h"
#include "strview.h"

/**
 * @brief Kind tag carried by every syntax node.
 * @details Kinds marked `auxilliary` are helper nodes owned by their
 *          enclosing node; they are never produced as a standalone
 *          parse result.
 */
typedef enum {
  SYNTAX_KIND_INVALID = 0,

  SYNTAX_KIND_IDENTIFIER,
  SYNTAX_KIND_COMPILE_TIME,
  SYNTAX_KIND_GENERIC_PARAM, // auxilliary
  SYNTAX_KIND_CALL_PARAM,    // auxilliary
  SYNTAX_KIND_GENERIC_ARG,   // auxilliary
  SYNTAX_KIND_PROGRAM,

  SYNTAX_KIND_NAMED,
  SYNTAX_KIND_REF_TYPE,
  SYNTAX_KIND_ARRAY_TYPE,
  SYNTAX_KIND_FUNC_TYPE,

  SYNTAX_KIND_NAMESPACE_DECL,
  SYNTAX_KIND_USING_DECL,
  SYNTAX_KIND_LET_DECL,
  SYNTAX_KIND_STRUCT_FIELD, // auxilliary
  SYNTAX_KIND_STRUCT_DECL,
  SYNTAX_KIND_ENUM_FIELD, // auxilliary
  SYNTAX_KIND_ENUM_DECL,
  SYNTAX_KIND_UNION_FIELD, // auxilliary
  SYNTAX_KIND_UNION_DECL,
  SYNTAX_KIND_VARIANT_FIELD, // auxilliary
  SYNTAX_KIND_VARIANT_DECL,
  SYNTAX_KIND_CONTRACT_DECL,
  SYNTAX_KIND_FUNC_DECL,

  SYNTAX_KIND_EMPTY_STMT,
  SYNTAX_KIND_BODY_STMT,
  SYNTAX_KIND_LET_STMT,
  SYNTAX_KIND_SET_STMT,
  SYNTAX_KIND_EXPR_STMT,
  SYNTAX_KIND_IF_STMT,
  SYNTAX_KIND_LOOP_STMT,
  SYNTAX_KIND_BREAK_STMT,
  SYNTAX_KIND_CONTINUE_STMT,
  SYNTAX_KIND_RETURN_STMT,
  SYNTAX_KIND_WHILE_STMT,

  SYNTAX_KIND_BINARY_EXPR,
  SYNTAX_KIND_UNARY_EXPR,
  SYNTAX_KIND_DOT_EXPR,
  SYNTAX_KIND_INDEX_EXPR,
  SYNTAX_KIND_CALL_EXPR,
  SYNTAX_KIND_INT_LIT_EXPR,
  SYNTAX_KIND_FLOAT_LIT_EXPR,
  SYNTAX_KIND_RUNE_LIT_EXPR,
  SYNTAX_KIND_STRING_LIT_EXPR,
  SYNTAX_KIND_STRUCT_LIT_FIELD, // auxilliary
  SYNTAX_KIND_STRUCT_LIT_EXPR,
  SYNTAX_KIND_ARRAY_LIT_EXPR,
} SyntaxKind;

/**
 * @brief Operator tag stored on unary and binary expression nodes.
 */
typedef enum {
  SYNTAX_OPERATOR_INVALID = 0,

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

  SYNTAX_OPERATOR_DEREF, // *

} SyntaxOperator;

/**
 * @brief Access mode of a reference type.
 */
typedef enum {
  SYNTAX_REF_KIND_READWRITE = 0,
  SYNTAX_REF_KIND_READONLY,
  SYNTAX_REF_KIND_WRITEONLY,
} SyntaxRefKind;

/**
 * @brief Common header of every AST node: kind plus source span.
 */
typedef struct {
  SyntaxKind kind;
  Span span;
} SyntaxNode;

/**
 * @brief A persistent chain cell, newest at head. Sharing cells is safe
 *        because they are never mutated after allocation.
 */
typedef struct SyntaxNodeList SyntaxNodeList;
struct SyntaxNodeList {
  SyntaxNode *node;
  SyntaxNodeList *next;
};

/**
 * @brief Identifier occurrence.
 */
typedef struct {
  SyntaxNode header;
  Strview value;
} SyntaxIdentifier;

/**
 * @brief Compile-time form `@name[(args)]`.
 */
typedef struct {
  SyntaxNode header;
  SyntaxIdentifier *id;
  SyntaxNodeList *args; // expr nodes
} SyntaxCompileTime;

/**
 * @brief Generic parameter `[annotations] name [: type]`.
 */
typedef struct {
  SyntaxNode header;
  SyntaxNodeList *annotations; // SyntaxCompileTime nodes
  SyntaxIdentifier *id;
  SyntaxNode *type; // type node
} SyntaxGenericParam;

/**
 * @brief Call parameter `[annotations] name : type`.
 */
typedef struct {
  SyntaxNode header;
  SyntaxNodeList *annotations; // SyntaxCompileTime nodes
  SyntaxIdentifier *id;
  SyntaxNode *type; // type node
} SyntaxCallParam;

/**
 * @brief Translation unit: top-level declarations, newest at head.
 */
typedef struct {
  SyntaxNode header;
  SyntaxNodeList *top_levels; // decl nodes
} SyntaxProgram;

/**
 * @brief Named path `a::b::c` with optional generic arguments.
 */
typedef struct {
  SyntaxNode header;
  SyntaxNodeList *path;         // SyntaxIdentifier nodes
  SyntaxNodeList *generic_args; // SyntaxGenericArg nodes
} SyntaxNamed;

/**
 * @brief Generic argument: a Type, or `id = PrimaryExpr`.
 */
typedef struct {
  SyntaxNode header;
  SyntaxIdentifier *id; // named form; NULL for the Type form
  SyntaxNode *value;    // type node (Type form) or PrimaryExpr node (named form)
} SyntaxGenericArg;

/**
 * @brief Reference type `&[readonly | writeonly] type`.
 */
typedef struct {
  SyntaxNode header;
  SyntaxRefKind ref_kind;
  SyntaxNode *inner_type; // type node
} SyntaxRefType;

/**
 * @brief Array type `[len] type`.
 */
typedef struct {
  SyntaxNode header;
  SyntaxNode *len;        // expr node
  SyntaxNode *inner_type; // type node
} SyntaxArrayType;

/**
 * @brief Function type `&func(params)[callconv][: type]`.
 */
typedef struct {
  SyntaxNode header;
  SyntaxNodeList *call_params; // type nodes
  SyntaxIdentifier *callconv;
  SyntaxNode *return_type; // type node
} SyntaxFuncType;

/**
 * @brief Namespace declaration `namespace path;`.
 */
typedef struct {
  SyntaxNode header;
  SyntaxNodeList *path; // SyntaxIdentifier nodes
} SyntaxNamespaceDecl;

/**
 * @brief Using declaration `using path;`.
 */
typedef struct {
  SyntaxNode header;
  SyntaxNodeList *path; // SyntaxIdentifier nodes
} SyntaxUsingDecl;

/**
 * @brief Let declaration `[annotations] let id[: type] [= value];`.
 */
typedef struct {
  SyntaxNode header;
  SyntaxNodeList *annotations; // SyntaxCompileTime nodes
  SyntaxIdentifier *id;
  SyntaxNode *type;  // type node
  SyntaxNode *value; // expr node
} SyntaxLetDecl;

/**
 * @brief Struct field `[annotations] name : type`.
 */
typedef struct {
  SyntaxNode header;
  SyntaxNodeList *annotations; // SyntaxCompileTime nodes
  SyntaxIdentifier *id;
  SyntaxNode *type; // type node
} SyntaxStructField;

/**
 * @brief Struct declaration `[annotations] struct Name[<params>] [; | { fields }]`.
 */
typedef struct {
  SyntaxNode header;
  SyntaxNodeList *annotations; // SyntaxCompileTime nodes
  SyntaxIdentifier *id;
  SyntaxNodeList *generic_params; // SyntaxGenericParameter nodes
  SyntaxNodeList *fields;         // SyntaxStructField nodes
} SyntaxStructDecl;

/**
 * @brief Enum field `[annotations] name [= value]`.
 */
typedef struct {
  SyntaxNode header;
  SyntaxNodeList *annotations; // SyntaxCompileTime nodes
  SyntaxIdentifier *id;
  SyntaxNode *value; // expr node
} SyntaxEnumField;

/**
 * @brief Enum declaration `[annotations] enum Name[: behind] { fields }`.
 */
typedef struct {
  SyntaxNode header;
  SyntaxNodeList *annotations; // SyntaxCompileTime nodes
  SyntaxIdentifier *id;
  SyntaxNode *behind_type; // type node
  SyntaxNodeList *fields;  // SyntaxEnumField nodes
} SyntaxEnumDecl;

/**
 * @brief Union field `[annotations] name : type`.
 */
typedef struct {
  SyntaxNode header;
  SyntaxNodeList *annotations; // SyntaxCompileTime nodes
  SyntaxIdentifier *id;
  SyntaxNode *type; // type node
} SyntaxUnionField;

/**
 * @brief Union declaration `[annotations] union Name[<params>] [; | { fields }]`.
 */
typedef struct {
  SyntaxNode header;
  SyntaxNodeList *annotations; // SyntaxCompileTime nodes
  SyntaxIdentifier *id;
  SyntaxNodeList *generic_params; // SyntaxGenericParameter nodes
  SyntaxNodeList *fields;         // SyntaxUnionField nodes
} SyntaxUnionDecl;

/**
 * @brief Variant field `[annotations] name [: type]`.
 */
typedef struct {
  SyntaxNode header;
  SyntaxNodeList *annotations; // SyntaxCompileTime nodes
  SyntaxIdentifier *id;
  SyntaxNode *type; // type node
} SyntaxVariantField;

/**
 * @brief Variant declaration `[annotations] variant Name[: behind][<params>] { fields }`.
 */
typedef struct {
  SyntaxNode header;
  SyntaxNodeList *annotations; // SyntaxCompileTime nodes
  SyntaxIdentifier *id;
  SyntaxNode *behind_type;        // type node
  SyntaxNodeList *generic_params; // SyntaxGenericParameter nodes
  SyntaxNodeList *fields;         // SyntaxVariantField nodes
} SyntaxVariantDecl;

/**
 * @brief Contract declaration `contract Name[<params>](call params)[: return];`.
 */
typedef struct {
  SyntaxNode header;
  SyntaxNodeList *annotations; // SyntaxCompileTime nodes
  SyntaxIdentifier *id;
  SyntaxNodeList *generic_params; // SyntaxGenericParameter nodes
  SyntaxNodeList *call_params;    // SyntaxCallParameter nodes
  SyntaxNode *return_type;        // type node
} SyntaxContractDecl;

/**
 * @brief Function declaration.
 */
typedef struct {
  SyntaxNode header;
  SyntaxNodeList *annotations; // SyntaxCompileTime nodes
  SyntaxIdentifier *id;
  SyntaxNodeList *generic_params; // SyntaxGenericParameter nodes
  SyntaxNodeList *call_params;    // SyntaxCallParameter nodes
  SyntaxIdentifier *callconv;
  SyntaxNode *return_type;  // type node
  SyntaxNodeList *fulfills; // Named nodes
  SyntaxNode *body;         // SyntaxEmptyStmt | SyntaxBodyStmt
} SyntaxFuncDecl;

/**
 * @brief Empty statement `;`.
 */
typedef struct {
  SyntaxNode header;
} SyntaxEmptyStmt;

/**
 * @brief Braced body `{ stmts }`.
 */
typedef struct {
  SyntaxNode header;
  SyntaxNodeList *stmts; // stmt nodes
} SyntaxBodyStmt;

/**
 * @brief Local binding `let id[: type] = value;`.
 */
typedef struct {
  SyntaxNode header;
  SyntaxIdentifier *id;
  SyntaxNode *type;  // type node
  SyntaxNode *value; // expr node
} SyntaxLetStmt;

/**
 * @brief Assignment `set left = right;`.
 */
typedef struct {
  SyntaxNode header;
  SyntaxNode *left;  // expr node
  SyntaxNode *right; // expr node
} SyntaxSetStmt;

/**
 * @brief Expression statement `expr;`.
 */
typedef struct {
  SyntaxNode header;
  SyntaxNode *expr; // expr node
} SyntaxExprStmt;

/**
 * @brief Conditional `if (condition) then [else else_stmt]`.
 */
typedef struct {
  SyntaxNode header;
  SyntaxNode *condition; // expr node
  SyntaxNode *then_stmt; // SyntaxEmptyStmt | SyntaxBodyStmt
  SyntaxNode *else_stmt; // SyntaxEmptyStmt | SyntaxIfStmt | SyntaxBodyStmt
} SyntaxIfStmt;

/**
 * @brief Infinite loop `loop stmt`.
 */
typedef struct {
  SyntaxNode header;
  SyntaxNode *stmt; // SyntaxEmptyStmt | SyntaxBodyStmt
} SyntaxLoopStmt;

/**
 * @brief Break statement `break;`.
 */
typedef struct {
  SyntaxNode header;
} SyntaxBreakStmt;

/**
 * @brief Continue statement `continue;`.
 */
typedef struct {
  SyntaxNode header;
} SyntaxContinueStmt;

/**
 * @brief Return statement `return [expr];`.
 */
typedef struct {
  SyntaxNode header;
  SyntaxNode *expr; // expr node
} SyntaxReturnStmt;

typedef struct {
  SyntaxNode header;
  SyntaxNode *condition; // expr node
  SyntaxNode *stmt;      // SyntaxEmptyStmt | SyntaxBodyStmt
} SyntaxWhileStmt;

typedef struct {
  SyntaxNode header;
  SyntaxOperator operator;
  SyntaxNode *left;  // expr
  SyntaxNode *right; // expr
} SyntaxBinaryExpr;

typedef struct {
  SyntaxNode header;
  SyntaxOperator operator;
  SyntaxNode *operand; // expr
} SyntaxUnaryExpr;

typedef struct {
  SyntaxNode header;
  SyntaxNode *receiver; // expr
  SyntaxIdentifier *id;
} SyntaxDotExpr;

typedef struct {
  SyntaxNode header;
  SyntaxNode *receiver; // expr
  SyntaxNode *index;    // expr
} SyntaxIndexExpr;

typedef struct {
  SyntaxNode header;
  SyntaxNode *receiver; // expr
  SyntaxNodeList *args; // expr nodes
} SyntaxCallExpr;

typedef struct {
  SyntaxNode header;
  Strview value;
} SyntaxIntLitExpr;

typedef struct {
  SyntaxNode header;
  Strview value;
} SyntaxFloatLitExpr;

typedef struct {
  SyntaxNode header;
  Strview value;
} SyntaxRuneLitExpr;

typedef struct {
  SyntaxNode header;
  Strview value;
} SyntaxStringLitExpr;

typedef struct {
  SyntaxNode header;
  SyntaxIdentifier *id;
  SyntaxNode *value; // expr node
} SyntaxStructLitField;

typedef struct {
  SyntaxNode header;
  SyntaxNamed *type;
  SyntaxNodeList *fields; // SyntaxStructLitField
} SyntaxStructLitExpr;

typedef struct {
  SyntaxNode header;
  SyntaxNode *type;         // SyntaxArrayType
  SyntaxNodeList *elements; // expr nodes
} SyntaxArrayLitExpr;

/**
 * @brief True when @p node's kind belongs to the type group.
 * @param node The node to test.
 * @return True for NAMED, REF_TYPE, ARRAY_TYPE or FUNC_TYPE.
 */
bool syntax_node_is_type(SyntaxNode *node);

/**
 * @brief True when @p node's kind belongs to the declaration group.
 * @param node The node to test.
 * @return True for the *_DECL kinds.
 */
bool syntax_node_is_decl(SyntaxNode *node);

/**
 * @brief True when @p node's kind belongs to the statement group.
 * @param node The node to test.
 * @return True for the *_STMT kinds.
 */
bool syntax_node_is_stmt(SyntaxNode *node);

/**
 * @brief True when @p node's kind belongs to the expression group.
 * @param node The node to test.
 * @return True for the *_EXPR kinds.
 */
bool syntax_node_is_expr(SyntaxNode *node);