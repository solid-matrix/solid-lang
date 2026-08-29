#pragma once

#include "strview.h"
#include "syntax_node.h"
#include "syntax_nodelist.h"
#include "syntax_operator.h"

typedef enum {
  SYNTAX_REF_KIND_READWRITE = 0,
  SYNTAX_REF_KIND_READONLY,
  SYNTAX_REF_KIND_WRITEONLY,
} SyntaxRefKind;

typedef struct {
  SyntaxNode header; // SYNTAX_KIND_IDENTIFIER
  Strview value;     //
} SyntaxIdentifier;

typedef struct {
  SyntaxNode header;    // SYNTAX_KIND_COMPILE_TIME
  SyntaxIdentifier *id; //
  SyntaxNodeList *args; // expr nodes
} SyntaxCompileTime;

typedef struct {
  SyntaxNode header;           // SYNTAX_KIND_GENERIC_PARAMETER
  SyntaxNodeList *annotations; // SyntaxCompileTime nodes
  SyntaxIdentifier *id;        //
  SyntaxNode *type;            // type node
} SyntaxGenericParam;

typedef struct {
  SyntaxNode header;           // SYNTAX_KIND_CALL_PARAMETER
  SyntaxNodeList *annotations; // SyntaxCompileTime nodes
  SyntaxIdentifier *id;        //
  SyntaxNode *type;            // type node
} SyntaxCallParam;

typedef struct {
  SyntaxNode header;          // SYNTAX_KIND_PROGRAM
  SyntaxNodeList *top_levels; // decl nodes
} SyntaxProgram;

typedef struct {
  SyntaxNode header;            // SYNTAX_KIND_NAMED
  SyntaxNodeList *path;         // SyntaxIdentifier nodes
  SyntaxNodeList *generic_args; // SyntaxGenericArg nodes
} SyntaxNamed;

typedef struct {
  SyntaxNode header;    // SYNTAX_KIND_GENERIC_ARG
  SyntaxIdentifier *id; // named form; NULL for the Type form
  SyntaxNode *value;    // type node (Type form) or PrimaryExpr node (named form)
} SyntaxGenericArg;

typedef struct {
  SyntaxNode header;      // SYNTAX_KIND_REF_TYPE
  SyntaxRefKind ref_kind; //
  SyntaxNode *inner_type; // type node
} SyntaxRefType;

typedef struct {
  SyntaxNode header;      // SYNTAX_KIND_ARRAY_TYPE
  SyntaxNode *len;        // expr node
  SyntaxNode *inner_type; // type node
} SyntaxArrayType;

typedef struct {
  SyntaxNode header;           // SYNTAX_KIND_FUNC_TYPE
  SyntaxNodeList *call_params; // type nodes
  SyntaxIdentifier *callconv;  //
  SyntaxNode *return_type;     // type node
} SyntaxFuncType;

typedef struct {
  SyntaxNode header;    // SYNTAX_KIND_NAMESPACE_DECL
  SyntaxNodeList *path; // SyntaxIdentifier nodes
} SyntaxNamespaceDecl;

typedef struct {
  SyntaxNode header;    // SYNTAX_KIND_USING_DECL
  SyntaxNodeList *path; // SyntaxIdentifier nodes
} SyntaxUsingDecl;

typedef struct {
  SyntaxNode header;           // SYNTAX_KIND_LET_DECL
  SyntaxNodeList *annotations; // SyntaxCompileTime nodes
  SyntaxIdentifier *id;        //
  SyntaxNode *type;            // type node
  SyntaxNode *value;           // expr node
} SyntaxLetDecl;

typedef struct {
  SyntaxNode header;           // SYNTAX_KIND_STRUCT_FIELD
  SyntaxNodeList *annotations; // SyntaxCompileTime nodes
  SyntaxIdentifier *id;        //
  SyntaxNode *type;            // type node
} SyntaxStructField;

typedef struct {
  SyntaxNode header;              // SYNTAX_KIND_STRUCT_DECL
  SyntaxNodeList *annotations;    // SyntaxCompileTime nodes
  SyntaxIdentifier *id;           //
  SyntaxNodeList *generic_params; // SyntaxGenericParameter nodes
  SyntaxNodeList *fields;         // SyntaxStructField nodes
} SyntaxStructDecl;

typedef struct {
  SyntaxNode header;           // SYNTAX_KIND_ENUM_FIELD
  SyntaxNodeList *annotations; // SyntaxCompileTime nodes
  SyntaxIdentifier *id;        //
  SyntaxNode *value;           // expr node
} SyntaxEnumField;

typedef struct {
  SyntaxNode header;           // SYNTAX_KIND_ENUM_DECL
  SyntaxNodeList *annotations; // SyntaxCompileTime nodes
  SyntaxIdentifier *id;        //
  SyntaxNode *behind_type;     // type node
  SyntaxNodeList *fields;      // SyntaxEnumField nodes
} SyntaxEnumDecl;

typedef struct {
  SyntaxNode header;           // SYNTAX_KIND_UNION_FIELD
  SyntaxNodeList *annotations; // SyntaxCompileTime nodes
  SyntaxIdentifier *id;        //
  SyntaxNode *type;            // type node
} SyntaxUnionField;

typedef struct {
  SyntaxNode header;              // SYNTAX_KIND_UNION_DECL
  SyntaxNodeList *annotations;    // SyntaxCompileTime nodes
  SyntaxIdentifier *id;           //
  SyntaxNodeList *generic_params; // SyntaxGenericParameter nodes
  SyntaxNodeList *fields;         // SyntaxUnionField nodes
} SyntaxUnionDecl;

typedef struct {
  SyntaxNode header;           // SYNTAX_KIND_VARIANT_FIELD
  SyntaxNodeList *annotations; // SyntaxCompileTime nodes
  SyntaxIdentifier *id;        //
  SyntaxNode *type;            // type node
} SyntaxVariantField;

typedef struct {
  SyntaxNode header;              // SYNTAX_KIND_VARIANT_DECL
  SyntaxNodeList *annotations;    // SyntaxCompileTime nodes
  SyntaxIdentifier *id;           //
  SyntaxNode *behind_type;        // type node
  SyntaxNodeList *generic_params; // SyntaxGenericParameter nodes
  SyntaxNodeList *fields;         // SyntaxVariantField nodes
} SyntaxVariantDecl;

typedef struct {
  SyntaxNode header;              // SYNTAX_KIND_CONTRACT_DECL
  SyntaxNodeList *annotations;    // SyntaxCompileTime nodes
  SyntaxIdentifier *id;           //
  SyntaxNodeList *generic_params; // SyntaxGenericParameter nodes
  SyntaxNodeList *call_params;    // SyntaxCallParameter nodes
  SyntaxNode *return_type;        // type node
} SyntaxContractDecl;

typedef struct {
  SyntaxNode header;              // SYNTAX_KIND_FUNC_DECL
  SyntaxNodeList *annotations;    // SyntaxCompileTime nodes
  SyntaxIdentifier *id;           //
  SyntaxNodeList *generic_params; // SyntaxGenericParameter nodes
  SyntaxNodeList *call_params;    // SyntaxCallParameter nodes
  SyntaxIdentifier *callconv;     //
  SyntaxNode *return_type;        // type node
  SyntaxNodeList *fulfills;       // Named nodes
  SyntaxNode *body;               // SyntaxEmptyStmt | SyntaxBodyStmt
} SyntaxFuncDecl;

typedef struct {
  SyntaxNode header; // SYNTAX_KIND_EMPTY_STMT
} SyntaxEmptyStmt;

typedef struct {
  SyntaxNode header;     // SYNTAX_KIND_BODY_STMT
  SyntaxNodeList *stmts; // stmt nodes
} SyntaxBodyStmt;

typedef struct {
  SyntaxNode header;    // SYNTAX_KIND_LET_STMT
  SyntaxIdentifier *id; //
  SyntaxNode *type;     // type node
  SyntaxNode *value;    // expr node
} SyntaxLetStmt;

typedef struct {
  SyntaxNode header; // SYNTAX_KIND_SET_STMT
  SyntaxNode *left;  // expr node
  SyntaxNode *right; // expr node
} SyntaxSetStmt;

typedef struct {
  SyntaxNode header; // SYNTAX_KIND_EXPR_STMT
  SyntaxNode *expr;  // expr node
} SyntaxExprStmt;

typedef struct {
  SyntaxNode header;     // SYNTAX_KIND_IF_STMT
  SyntaxNode *condition; // expr node
  SyntaxNode *then_stmt; // SyntaxEmptyStmt | SyntaxBodyStmt
  SyntaxNode *else_stmt; // SyntaxEmptyStmt | SyntaxIfStmt | SyntaxBodyStmt
} SyntaxIfStmt;

typedef struct {
  SyntaxNode header; // SYNTAX_KIND_LOOP_STMT
  SyntaxNode *stmt;  // SyntaxEmptyStmt | SyntaxBodyStmt
} SyntaxLoopStmt;

typedef struct {
  SyntaxNode header; // SYNTAX_KIND_BREAK_STMT
} SyntaxBreakStmt;

typedef struct {
  SyntaxNode header; // SYNTAX_KIND_CONTINUE_STMT
} SyntaxContinueStmt;

typedef struct {
  SyntaxNode header; // SYNTAX_KIND_RETURN_STMT
  SyntaxNode *expr;  // expr node
} SyntaxReturnStmt;

typedef struct {
  SyntaxNode header;     // SYNTAX_KIND_WHILE_STMT
  SyntaxNode *condition; // expr node
  SyntaxNode *stmt;      // SyntaxEmptyStmt | SyntaxBodyStmt
} SyntaxWhileStmt;

typedef struct {
  SyntaxNode header;       // SYNTAX_KIND_BINARY_EXPR
  SyntaxOperator operator; //
  SyntaxNode *left;        // expr
  SyntaxNode *right;       // expr
} SyntaxBinaryExpr;

typedef struct {
  SyntaxNode header;       // SYNTAX_KIND_UNARY_EXPR
  SyntaxOperator operator; //
  SyntaxNode *operand;     // expr
} SyntaxUnaryExpr;

typedef struct {
  SyntaxNode header;    // SYNTAX_KIND_DOT_EXPR
  SyntaxNode *receiver; // expr
  SyntaxIdentifier *id; //
} SyntaxDotExpr;

typedef struct {
  SyntaxNode header;    // SYNTAX_KIND_INDEX_EXPR
  SyntaxNode *receiver; // expr
  SyntaxNode *index;    // expr
} SyntaxIndexExpr;

typedef struct {
  SyntaxNode header;    // SYNTAX_KIND_CALL_EXPR
  SyntaxNode *receiver; // expr
  SyntaxNodeList *args; // expr nodes
} SyntaxCallExpr;

typedef struct {
  SyntaxNode header; // SYNTAX_KIND_INT_LIT_EXPR
  Strview value;     //
} SyntaxIntLitExpr;

typedef struct {
  SyntaxNode header; // SYNTAX_KIND_FLOAT_LIT_EXPR
  Strview value;     //
} SyntaxFloatLitExpr;

typedef struct {
  SyntaxNode header; // SYNTAX_KIND_RUNE_LIT_EXPR
  Strview value;     //
} SyntaxRuneLitExpr;

typedef struct {
  SyntaxNode header; // SYNTAX_KIND_STRING_LIT_EXPR
  Strview value;     //
} SyntaxStringLitExpr;

typedef struct {
  SyntaxNode header;    // SYNTAX_KIND_STRUCT_LIT_FIELD
  SyntaxIdentifier *id; //
  SyntaxNode *value;    // expr node
} SyntaxStructLitField;

typedef struct {
  SyntaxNode header;      // SYNTAX_KIND_STRUCT_LIT_EXPR
  SyntaxNamed *type;      //
  SyntaxNodeList *fields; // SyntaxStructLitField
} SyntaxStructLitExpr;

typedef struct {
  SyntaxNode header;        // SYNTAX_KIND_ARRAY_LIT_EXPR
  SyntaxNode *type;         // SyntaxArrayType
  SyntaxNodeList *elements; // expr nodes
} SyntaxArrayLitExpr;