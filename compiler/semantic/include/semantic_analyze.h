#pragma once

#include <stddef.h>

#include "arena.h"
#include "semantic_common.h"
#include "semantic_error.h"

typedef struct {
  // TODO
  SemanticErrorList *errors;
} SemanticAnalyzeResult;

SemanticAnalyzeResult semantic_analyze(Arena *arena, const SemanticModuleList *modules,
                                       const SemanticParamList *params);