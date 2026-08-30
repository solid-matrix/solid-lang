#pragma once

#include "strview.h"
#include "syntax_node.h"

typedef struct SemanticProgramList SemanticProgramList;
struct SemanticProgramList {
  SyntaxProgram *program;
  SemanticProgramList *next;
};

typedef struct {
  Strview name;
  SemanticProgramList *programs;
} SemanticPackage;