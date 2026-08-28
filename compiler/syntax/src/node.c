#include "syntax_node.h"

SyntaxNode syntax_node_create(SyntaxKind kind, Span span) { return (SyntaxNode){.kind = kind, .span = span}; }

bool syntax_node_is_type(SyntaxNode *node) {
  return node->kind == SYNTAX_KIND_NAMED || node->kind == SYNTAX_KIND_REF_TYPE ||
         node->kind == SYNTAX_KIND_ARRAY_TYPE || node->kind == SYNTAX_KIND_FUNC_TYPE;
}

bool syntax_node_is_decl(SyntaxNode *node) {
  return node->kind == SYNTAX_KIND_NAMESPACE_DECL || node->kind == SYNTAX_KIND_USING_DECL ||
         node->kind == SYNTAX_KIND_LET_DECL || node->kind == SYNTAX_KIND_STRUCT_DECL ||
         node->kind == SYNTAX_KIND_ENUM_DECL || node->kind == SYNTAX_KIND_UNION_DECL ||
         node->kind == SYNTAX_KIND_VARIANT_DECL || node->kind == SYNTAX_KIND_CONTRACT_DECL ||
         node->kind == SYNTAX_KIND_FUNC_DECL;
}

bool syntax_node_is_stmt(SyntaxNode *node) {
  return node->kind == SYNTAX_KIND_EMPTY_STMT || node->kind == SYNTAX_KIND_BODY_STMT ||
         node->kind == SYNTAX_KIND_LET_STMT || node->kind == SYNTAX_KIND_SET_STMT ||
         node->kind == SYNTAX_KIND_EXPR_STMT || node->kind == SYNTAX_KIND_IF_STMT ||
         node->kind == SYNTAX_KIND_LOOP_STMT || node->kind == SYNTAX_KIND_BREAK_STMT ||
         node->kind == SYNTAX_KIND_CONTINUE_STMT || node->kind == SYNTAX_KIND_RETURN_STMT ||
         node->kind == SYNTAX_KIND_WHILE_STMT;
}

bool syntax_node_is_expr(SyntaxNode *node) {
  return node->kind == SYNTAX_KIND_NAMED || node->kind == SYNTAX_KIND_BINARY_EXPR ||
         node->kind == SYNTAX_KIND_UNARY_EXPR || node->kind == SYNTAX_KIND_DOT_EXPR ||
         node->kind == SYNTAX_KIND_INDEX_EXPR || node->kind == SYNTAX_KIND_CALL_EXPR ||
         node->kind == SYNTAX_KIND_INT_LIT_EXPR || node->kind == SYNTAX_KIND_FLOAT_LIT_EXPR ||
         node->kind == SYNTAX_KIND_RUNE_LIT_EXPR || node->kind == SYNTAX_KIND_STRING_LIT_EXPR ||
         node->kind == SYNTAX_KIND_STRUCT_LIT_EXPR || node->kind == SYNTAX_KIND_ARRAY_LIT_EXPR;
}