#pragma once

#include "semantic_common.h"

typedef struct {
  SemanticNamePath *path;
  SemanticProgramList *programs;
} SemanticModule;

typedef struct SemanticModuleList SemanticModuleList;
struct SemanticModuleList {
  SemanticModule *module;
  SemanticModuleList *next;
};