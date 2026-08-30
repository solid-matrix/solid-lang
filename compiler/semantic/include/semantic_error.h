#pragma once

#include "syntax_node.h"

typedef enum {
  SEMANTIC_OK = 0x0000,
} SemanticErrorCode;

typedef struct {
  SemanticErrorCode code;
  SyntaxNode *node;
} SemanticError;

typedef struct SemanticErrorList SemanticErrorList;
struct SemanticErrorList {
  SemanticError error;
  SemanticErrorList *next;
};