#pragma once

#include "strview.h"
#include "syntax_node.h"

typedef struct SemanticNamePath SemanticNamePath; /* complete in namepath.h */

typedef struct SemanticProgramList SemanticProgramList;
struct SemanticProgramList {
  SyntaxProgram *program;
  SemanticProgramList *next;
};

typedef struct {
  SemanticNamePath *path;
  SemanticProgramList *programs;
} SemanticModule;

typedef struct SemanticModuleList SemanticModuleList;
struct SemanticModuleList {
  SemanticModule *module;
  SemanticModuleList *next;
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