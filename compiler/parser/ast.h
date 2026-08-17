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
typedef enum {
  SYNTAX_KIND_UNKNOWN = 0x0000,

  SYNTAX_KIND_PROGRAM = 0x0001,

  SYNTAX_KIND_TYPE_MASK = 0x0100,

  SYNTAX_KIND_EXPR_MASK = 0x0200,

  SYNTAX_KIND_STMT_MASK = 0x0400,

  SYNTAX_KIND_DECL_MASK = 0x0800,

} SyntaxKind;

typedef union SyntaxNode SyntaxNode;

typedef union SyntaxTypeNode SyntaxTypeNode;

typedef union SyntaxExprNode SyntaxExprNode;

typedef union SyntaxStmtNode SyntaxStmtNode;

typedef union SyntaxDeclNode SyntaxDeclNode;

typedef struct SyntaxCtAnnotation SyntaxCtAnnotation;

typedef struct SyntaxGenericParam SyntaxGenericParam;

typedef struct SyntaxProgram SyntaxProgram;

union SyntaxNode {};

union SyntaxTypeNode {};

union SyntaxExprNode {};

union SyntaxStmtNode {};

union SyntaxDeclNode {};

/**
 * @brief The root of an AST: a program with a list of top-level decls.
 */
struct SyntaxProgram {
  SyntaxKind kind;
  SourceSpan span;
  size_t top_level_count;
  SyntaxDeclNode *top_levels;
};

struct SyntaxCtAnnotation {
  SyntaxKind kind;
  SourceSpan span;
  StringView name;
  size_t argument_count;
  SyntaxExprNode *arguments;
};

struct SyntaxGenericParam {
  SyntaxKind kind;
  SourceSpan span;
  StringView name;
  SyntaxTypeNode type;
  size_t annotation_count;
  SyntaxCtAnnotation *annotations;
};

#endif /* SOLID_AST_H */
