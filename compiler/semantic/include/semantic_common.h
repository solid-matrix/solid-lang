#pragma once

#include "strview.h"
#include "syntax_node.h"

typedef struct SemanticNamePath SemanticNamePath;
struct SemanticNamePath {
  Strview name;
  SemanticNamePath *next;
};

typedef struct SemanticProgramList SemanticProgramList;
struct SemanticProgramList {
  SyntaxProgram *program;
  SemanticProgramList *next;
};

typedef struct {
  Strview name;
  Strview value;
} SemanticParam;

typedef struct SemanticParamList SemanticParamList;
struct SemanticParamList {
  SemanticParam param;
  SemanticParamList *next;
};