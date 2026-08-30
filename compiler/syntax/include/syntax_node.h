/**
 * @file syntax_node.h
 * @brief Common node header and kind tags.
 * @author solid-matrix
 * @version 0.0.5
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "span.h"

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
 * @brief Common header of every AST node: kind plus source span.
 */
typedef struct SyntaxNode {
  SyntaxKind kind;
  Span span;
} SyntaxNode;

/**
 * @brief Initializes a node header.
 * @param kind The node's kind tag.
 * @param span The source text the node was parsed from.
 * @return The initialized header value.
 */
SyntaxNode syntax_node_create(SyntaxKind kind, Span span);

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
