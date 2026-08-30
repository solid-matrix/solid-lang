#pragma once

#include "syntax_node.h"

typedef struct SemanticProgramList SemanticProgramList;
struct SemanticProgramList {
  SyntaxProgram *program;
  SemanticProgramList *next;
};