#pragma once

#include "arena.h"
#include "semantic_paramlist.h"
#include <stddef.h>

typedef struct {
  const SemanticParamList *params;
  Arena *arena;
} SemanticAnalyzer;