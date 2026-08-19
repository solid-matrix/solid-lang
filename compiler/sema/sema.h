/**
 * @file sema.h
 * @brief Semantic analyzer: symbol resolution and type checking.
 * @author solid-matrix
 * @version 0.0.5
 */

#pragma once

#include "ast.h"

/**
 * @brief Semantic analyzer state; opaque to callers.
 */
typedef struct SolidSema SolidSema;

// TODO: SolidSemaCreate(SolidSema **out, ...)
// TODO: SolidSemaRun(SolidSema *sema, SolidAstNode *root, ...)
// TODO: SolidSemaDestroy(SolidSema *sema)
