/**
 * @file ast.h
 * @brief AST node types produced by the parser.
 * @author solid-matrix
 * @version 0.0.5
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "arena.h"
#include "span.h"
#include "strview.h"

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
typedef enum {
  SYNTAX_KIND_INVALID = 0x0000,

  /* standalone nodes */
  SYNTAX_KIND_PROGRAM = 0x0001,

  SYNTAX_KIND_COMPILE_TIME = 0x0002,

  SYNTAX_KIND_STRUCT_FIELD = 0x0003,

  SYNTAX_KIND_ENUM_FIELD = 0x0004,

  SYNTAX_KIND_UNION_FIELD = 0x0005,

  SYNTAX_KIND_VARIANT_FIELD = 0x0006,

  SYNTAX_KIND_GENERIC_PARAMETER = 0x0007,

  SYNTAX_KIND_CALL_PARAMETER = 0x0008,

  SYNTAX_KIND_CONTRACT_PARAMETER = 0x0009,

  SYNTAX_KIND_STRUCT_LIT_FIELD = 0x000A,

  SYNTAX_KIND_CONTRACT_ARGUMENT = 0x000B,

  SYNTAX_KIND_IDENTIFIER = 0x000C,

  SYNTAX_KIND_NAME_PATH = 0x000D,

  SYNTAX_KIND_CALL_ARGUMENTS = 0x000E,

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

  SYNTAX_OPERATOR_DEREF, // *

} SyntaxOperator;

typedef struct SyntaxNode {
  SyntaxKind kind;
  Span span;
} SyntaxNode;

/**
 * @brief Builds the common header shared by every AST node.
 * @param kind The node kind.
 * @param span The source range covered by the node.
 * @return The initialized header.
 */
SyntaxNode syntax_node_header(SyntaxKind kind, Span span);

#pragma region SYNTAX NODE LIST

typedef struct SyntaxNodeList SyntaxNodeList;
struct SyntaxNodeList {
  SyntaxNode *node;
  SyntaxNodeList *next;
};

/**
 * @brief The empty list. Returns NULL; exists for explicit call sites.
 */
SyntaxNodeList *syntax_nodelist_empty(void);

/**
 * @brief Builds a list holding @p count array elements, preserving
 *        order. Returns NULL when @p count is zero.
 */
SyntaxNodeList *syntax_nodelist_from_array(Arena *arena, SyntaxNode *const *nodes, size_t count);

/**
 * @brief A list with @p node followed by all of @p list. O(1); shares
 *        the whole old spine.
 */
SyntaxNodeList *syntax_nodelist_prepend(Arena *arena, SyntaxNodeList *list, SyntaxNode *node);

/**
 * @brief A list with all elements of @p list followed by @p node.
 *        Copies @p list's cells; the source stays valid and unchanged.
 */
SyntaxNodeList *syntax_nodelist_append(Arena *arena, SyntaxNodeList *list, SyntaxNode *node);

/**
 * @brief The first node. Asserts non-empty.
 */
SyntaxNode *syntax_nodelist_head(SyntaxNodeList *list);

/**
 * @brief Every element except the first (NULL when length is one).
 *        Asserts non-empty.
 */
SyntaxNodeList *syntax_nodelist_tail(SyntaxNodeList *list);

/**
 * @brief The node at zero-based position @p n. Asserts in range.
 */
SyntaxNode *syntax_nodelist_at(SyntaxNodeList *list, size_t n);

/**
 * @brief True when the list holds no nodes.
 */
bool syntax_nodelist_is_empty(const SyntaxNodeList *list);

/**
 * @brief Fresh cells holding @p list's nodes in reverse order; the
 *        source stays valid and unchanged.
 */
SyntaxNodeList *syntax_nodelist_reverse(Arena *arena, SyntaxNodeList *list);

/**
 * @brief All nodes of @p list_a followed by all of @p list_b. Copies
 *        @p list_a's cells and shares @p list_b wholesale.
 */
SyntaxNodeList *syntax_nodelist_concat(Arena *arena, SyntaxNodeList *list_a, SyntaxNodeList *list_b);

/**
 * @brief Number of nodes. O(n).
 */
size_t syntax_nodelist_length(SyntaxNodeList *list);

#pragma endregion

#pragma region COMMON NODES
typedef struct {
  SyntaxNode header;
  SyntaxNodeList *top_levels; // decl nodes
} SyntaxProgram;

typedef struct {
  SyntaxNode header;
  Strview strview;
} SyntaxIdentifier;

typedef struct {
  SyntaxNode header;
  SyntaxNodeList *segments; // SyntaxIdentifier nodes
} SyntaxNamePath;

typedef struct {
  SyntaxNode header;
  SyntaxIdentifier *name;
  SyntaxNodeList *args; // expr nodes
} SyntaxCompileTime;

typedef struct {
  SyntaxNode header;
  SyntaxNodeList *annotations; // SyntaxCompileTime nodes
  SyntaxIdentifier *name;
  SyntaxNode *type; // type node
} SyntaxStructField;

typedef struct {
  SyntaxNode header;
  SyntaxNodeList *annotations; // SyntaxCompileTime nodes
  SyntaxIdentifier *name;
  SyntaxNode *value; // expr node
} SyntaxEnumField;

typedef struct {
  SyntaxNode header;
  SyntaxNodeList *annotations; // SyntaxCompileTime nodes
  SyntaxIdentifier *name;
  SyntaxNode *type; // type node
} SyntaxUnionField;

typedef struct {
  SyntaxNode header;
  SyntaxNodeList *annotations; // SyntaxCompileTime nodes
  SyntaxIdentifier *name;
  SyntaxNode *type; // type node
} SyntaxVariantField;

typedef struct {
  SyntaxNode header;
  SyntaxNodeList *annotations; // SyntaxCompileTime nodes
  SyntaxIdentifier *name;
  SyntaxNode *type; // type node
} SyntaxGenericParameter;

typedef struct {
  SyntaxNode header;
  SyntaxNodeList *annotations; // SyntaxCompileTime nodes
  SyntaxIdentifier *name;
  SyntaxNode *type; // type node
} SyntaxCallParameter;

typedef struct {
  SyntaxNode header;
  SyntaxNodeList *annotations; // SyntaxCompileTime nodes
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

#pragma endregion

#pragma region TYPE NODES

typedef struct {
  SyntaxNode header;
  SyntaxNode *value; // expr node
} SyntaxConstType;

typedef struct {
  SyntaxNode header;
  SyntaxNamePath *path;              // SyntaxIdentifier nodes
  SyntaxNodeList *generic_arguments; // type nodes
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
  SyntaxNodeList *call_params; // SyntaxCallParameter nodes
  SyntaxCallConv callconv;
  SyntaxNode *return_type; // type node
} SyntaxFuncType;

#pragma endregion

#pragma region DECL NODES

typedef struct {
  SyntaxNode header;
  SyntaxNamePath *path;
} SyntaxNamespaceDecl;

typedef struct {
  SyntaxNode header;
  SyntaxNamePath *path;
} SyntaxUsingDecl;

typedef struct {
  SyntaxNode header;
  SyntaxNodeList *annotations; // SyntaxCompileTime nodes
  SyntaxIdentifier *name;
  SyntaxNode *type;  // type node
  SyntaxNode *value; // expr node
} SyntaxLetDecl;

typedef struct {
  SyntaxNode header;
  SyntaxNodeList *annotations; // SyntaxCompileTime nodes
  SyntaxIdentifier *name;
  SyntaxNodeList *generic_params; // SyntaxGenericParameter nodes
  SyntaxNodeList *fields;         // SyntaxStructField nodes
} SyntaxStructDecl;

typedef struct {
  SyntaxNode header;
  SyntaxNodeList *annotations; // SyntaxCompileTime nodes
  SyntaxIdentifier *name;
  SyntaxNode *behind_type; // type node
  SyntaxNodeList *fields;  // SyntaxEnumField nodes
} SyntaxEnumDecl;

typedef struct {
  SyntaxNode header;
  SyntaxNodeList *annotations; // SyntaxCompileTime nodes
  SyntaxIdentifier *name;
  SyntaxNodeList *generic_params; // SyntaxGenericParameter nodes
  SyntaxNodeList *fields;         // SyntaxUnionField nodes
} SyntaxUnionDecl;

typedef struct {
  SyntaxNode header;
  SyntaxNodeList *annotations; // SyntaxCompileTime nodes
  SyntaxIdentifier *name;
  SyntaxNode *behind_type;        // type node
  SyntaxNodeList *generic_params; // SyntaxGenericParameter nodes
  SyntaxNodeList *fields;         // SyntaxVariantField nodes
} SyntaxVariantDecl;

typedef struct {
  SyntaxNode header;
  SyntaxNodeList *annotations; // SyntaxCompileTime nodes
  SyntaxIdentifier *name;
  SyntaxNodeList *generic_params; // SyntaxGenericParameter nodes
  SyntaxNodeList *call_params;    // SyntaxCallParameter nodes
  SyntaxNode *return_type;        // type node
} SyntaxContractDecl;

typedef struct {
  SyntaxNode header;
  SyntaxNodeList *annotations; // SyntaxCompileTime nodes
  SyntaxIdentifier *name;
  SyntaxNodeList *generic_params;  // SyntaxGenericParameter nodes
  SyntaxNodeList *contract_params; // SyntaxContractParameter nodes
  SyntaxNodeList *call_params;     // SyntaxCallParameter nodes
  SyntaxCallConv callconv;
  SyntaxNode *return_type;  // type node
  SyntaxNodeList *fulfills; // type nodes
  SyntaxNode *body;         // SyntaxBodyStmt
} SyntaxFuncDecl;

#pragma endregion

#pragma region STMT NODES

typedef struct {
  SyntaxNode header;
  SyntaxNodeList *stmts; // stmt nodes
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

#pragma region EXPR NODES

typedef struct {
  SyntaxNode header;
  Strview value;
} SyntaxNumberLitExpr;

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
  SyntaxNode *type;       // SyntaxNamedType
  SyntaxNodeList *fields; // SyntaxStructLitField
} SyntaxStructLitExpr;

typedef struct {
  SyntaxNode header;
  SyntaxNode *type;         // SyntaxArrayType
  SyntaxNodeList *elements; // SyntaxExpr
} SyntaxArrayLitExpr;

typedef struct {
  SyntaxNode header;
  SyntaxNamePath *path;               // SyntaxIdentifier nodes
  SyntaxNodeList *generic_arguments;  // type
  SyntaxNodeList *contract_arguments; // SyntaxContractArgument nodes
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
  SyntaxNode *receiver; // expr
  SyntaxNodeList *args; // expr nodes
} SyntaxCallExpr;

#pragma endregion
