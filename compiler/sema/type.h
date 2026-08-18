/**
 * @file type.h
 * @brief Type system: built-in kinds and type descriptors.
 * @author solid-matrix
 * @version 0.0.5
 */

#ifndef SOLID_SEMA_TYPE_H
#define SOLID_SEMA_TYPE_H

 /**
  * @brief Built-in type kinds.
  */
typedef enum SolidTypeKind {
  SOLID_TYPE_VOID = 0,
  SOLID_TYPE_I32,
  SOLID_TYPE_F64,
  SOLID_TYPE_BOOL,
  SOLID_TYPE_UNKNOWN
} SolidTypeKind;

/**
 * @brief Type descriptor; opaque to callers.
 */
typedef struct SolidType SolidType;

#endif /* SOLID_SEMA_TYPE_H */
