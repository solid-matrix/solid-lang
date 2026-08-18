/**
 * @file sema.h
 * @brief Semantic analyzer: symbol resolution and type checking.
 * @author solid-matrix
 * @version 0.0.5
 */

#ifndef SOLID_SEMA_SEMA_H
#define SOLID_SEMA_SEMA_H

#include "ast.h"

/**
 * @brief Semantic analyzer state; opaque to callers.
 */
typedef struct SolidSema SolidSema;

// TODO: SolidSemaCreate(SolidSema **out, ...)
// TODO: SolidSemaRun(SolidSema *sema, SolidAstNode *root, ...)
// TODO: SolidSemaDestroy(SolidSema *sema)

#endif /* SOLID_SEMA_SEMA_H */
