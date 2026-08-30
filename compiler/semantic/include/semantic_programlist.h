#pragma once

#include "syntax_nodes.h"

typedef struct SemanticProgramList SemanticProgramList;
struct SemanticProgramList {
  SyntaxProgram *program;
  SemanticProgramList *next;
};