#pragma once

#include "strview.h"

typedef struct {
  Strview name;
  Strview value;
} SemanticParam;

typedef struct SemanticParamList SemanticParamList;
struct SemanticParamList {
  SemanticParam param;
  SemanticParamList *next;
};
