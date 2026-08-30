#pragma once

#include <stddef.h>

#include "arena.h"
#include "semantic_package.h"
#include "semantic_param.h"

typedef struct {

} SemanticAnalyzeResult;

SemanticAnalyzeResult semantic_analyze(const SemanticPackage *package, const SemanticParamList *params, Arena *arena);