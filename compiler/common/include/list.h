/**
 * @file list.h
 * @brief Persistent singly-linked list templates for value and pointer
 *        payloads.
 * @author solid-matrix
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "arena.h"

/**
 * @brief Declares a persistent singly-linked list: cells, a predicate and
 *        the full function surface.
 * @details The list handle is the head cell, and NULL is the empty list.
 *          head is the entry's payload, tail the rest of the list. Cells
 *          are never mutated after allocation — every generated function
 *          is pure: inputs stay valid and unchanged, and results share
 *          untouched cells with them.
 *
 *          The payload travels by value, so pointer and value payloads
 *          share one template: predicates receive the payload by value,
 *          and a NULL payload is an ordinary element. An empty tail is
 *          indistinguishable from a missing one.
 *
 * @param type_name The list type (e.g. SyntaxErrorList).
 * @param func_prefix The function prefix (e.g. syntax_errorlist); declares
 *                    a matching @p func_prefix##_predicate callback type.
 * @param type The payload type; pointers and values both work.
 */
#define LIST_DECLARE(type_name, func_prefix, type)                                                       \
  typedef struct type_name type_name;                                                                    \
                                                                                                         \
  struct type_name {                                                                                     \
    type head;              /* the entry's payload */                                                    \
    type_name *tail;        /* the rest of the list */                                                   \
  };                                                                                                     \
                                                                                                         \
  typedef bool (*func_prefix##_predicate)(type head, void *context);                                     \
                                                                                                         \
  type_name *func_prefix##_empty(void);                                                                  \
                                                                                                         \
  type_name *func_prefix##_from_array(Arena *arena, type const *items, size_t count);                    \
                                                                                                         \
  type_name *func_prefix##_prepend(Arena *arena, const type_name *list, type head);                      \
                                                                                                         \
  type_name *func_prefix##_append(Arena *arena, const type_name *list, type tail);                       \
                                                                                                         \
  type func_prefix##_at(const type_name *list, size_t n);                                                \
                                                                                                         \
  bool func_prefix##_is_empty(const type_name *list);                                                    \
                                                                                                         \
  type_name *func_prefix##_reverse(Arena *arena, const type_name *list);                                 \
                                                                                                         \
  bool func_prefix##_for_all(const type_name *list, func_prefix##_predicate predicate,                   \
                             void *context);                                                             \
                                                                                                         \
  bool func_prefix##_exists(const type_name *list, func_prefix##_predicate predicate, void *context);    \
                                                                                                         \
  type_name *func_prefix##_filter(Arena *arena, const type_name *list,                                   \
                                  func_prefix##_predicate predicate, void *context);                     \
                                                                                                         \
  type_name *func_prefix##_concat(Arena *arena, const type_name *list_a, const type_name *list_b);       \
                                                                                                         \
  size_t func_prefix##_length(const type_name *list)

/**
 * @brief Defines the functions declared by LIST_DECLARE.
 * @note The expansion needs assert.h and the declarations from
 *       LIST_DECLARE.
 * @param type_name The list type.
 * @param func_prefix The function prefix.
 * @param type The payload type.
 */
#define LIST_DEFINE(type_name, func_prefix, type)                                                        \
  type_name *func_prefix##_empty(void) { return NULL; }                                                  \
                                                                                                         \
  type_name *func_prefix##_from_array(Arena *arena, type const *items, size_t count) {                   \
    assert(arena != NULL);                                                                               \
    assert(count == 0 || items != NULL);                                                                 \
                                                                                                         \
    type_name *list = NULL;                                                                              \
    for (size_t i = count; i > 0; i--) /* build back-to-front: O(n) */                                   \
      list = func_prefix##_prepend(arena, list, items[i - 1]);                                           \
    return list;                                                                                         \
  }                                                                                                      \
                                                                                                         \
  type_name *func_prefix##_prepend(Arena *arena, const type_name *list, type head) {                     \
    assert(arena != NULL);                                                                               \
                                                                                                         \
    type_name *cell = arena_alloc(arena, sizeof(type_name)); /* OOM is fatal */                          \
    cell->head = head;                                                                                   \
    cell->tail = (type_name *)list; /* lists are immutable values */                                     \
    return cell;                                                                                         \
  }                                                                                                      \
                                                                                                         \
  type_name *func_prefix##_append(Arena *arena, const type_name *list, type tail) {                      \
    assert(arena != NULL);                                                                               \
                                                                                                         \
    if (func_prefix##_is_empty(list))                                                                    \
      return func_prefix##_prepend(arena, NULL, tail);                                                   \
                                                                                                         \
    type_name *head_cell = func_prefix##_prepend(arena, NULL, list->head);                               \
    type_name *walk = head_cell;                                                                         \
    for (const type_name *it = list->tail; it != NULL; it = it->tail) {                                  \
      walk->tail = func_prefix##_prepend(arena, NULL, it->head);                                         \
      walk = walk->tail;                                                                                 \
    }                                                                                                    \
    walk->tail = func_prefix##_prepend(arena, NULL, tail);                                               \
    return head_cell;                                                                                    \
  }                                                                                                      \
                                                                                                         \
  type func_prefix##_at(const type_name *list, size_t n) {                                               \
    const type_name *it = list;                                                                          \
    while (n-- > 0) {                                                                                    \
      assert(!func_prefix##_is_empty(it)); /* out of range */                                            \
      it = it->tail;                                                                                     \
    }                                                                                                    \
    assert(!func_prefix##_is_empty(it));                                                                 \
    return it->head;                                                                                     \
  }                                                                                                      \
                                                                                                         \
  bool func_prefix##_is_empty(const type_name *list) { return list == NULL; }                            \
                                                                                                         \
  type_name *func_prefix##_reverse(Arena *arena, const type_name *list) {                                \
    assert(arena != NULL);                                                                               \
                                                                                                         \
    type_name *result = NULL;                                                                            \
    for (const type_name *it = list; it != NULL; it = it->tail)                                          \
      result = func_prefix##_prepend(arena, result, it->head);                                           \
    return result;                                                                                       \
  }                                                                                                      \
                                                                                                         \
  bool func_prefix##_for_all(const type_name *list, func_prefix##_predicate predicate,                   \
                             void *context) {                                                            \
    assert(predicate != NULL);                                                                           \
                                                                                                         \
    for (const type_name *it = list; it != NULL; it = it->tail)                                          \
      if (!predicate(it->head, context))                                                                 \
        return false;                                                                                    \
    return true;                                                                                         \
  }                                                                                                      \
                                                                                                         \
  bool func_prefix##_exists(const type_name *list, func_prefix##_predicate predicate,                    \
                            void *context) {                                                            \
    assert(predicate != NULL);                                                                           \
                                                                                                         \
    for (const type_name *it = list; it != NULL; it = it->tail)                                          \
      if (predicate(it->head, context))                                                                  \
        return true;                                                                                     \
    return false;                                                                                        \
  }                                                                                                      \
                                                                                                         \
  type_name *func_prefix##_filter(Arena *arena, const type_name *list,                                   \
                                  func_prefix##_predicate predicate, void *context) {                    \
    assert(arena != NULL);                                                                               \
    assert(predicate != NULL);                                                                           \
                                                                                                         \
    type_name *result = NULL;                                                                            \
    type_name *walk = NULL;                                                                              \
    for (const type_name *it = list; it != NULL; it = it->tail) {                                        \
      if (!predicate(it->head, context))                                                                 \
        continue;                                                                                        \
      type_name *cell = func_prefix##_prepend(arena, NULL, it->head);                                     \
      if (walk == NULL)                                                                                  \
        result = cell;                                                                                   \
      else                                                                                               \
        walk->tail = cell;                                                                               \
      walk = cell;                                                                                       \
    }                                                                                                    \
    return result;                                                                                       \
  }                                                                                                      \
                                                                                                         \
  type_name *func_prefix##_concat(Arena *arena, const type_name *list_a,                                 \
                                  const type_name *list_b) {                                                             \
    assert(arena != NULL);                                                                               \
                                                                                                         \
    if (func_prefix##_is_empty(list_a))                                                                  \
      return (type_name *)list_b; /* shares b — lists are immutable values */                            \
                                                                                                         \
    type_name *head_cell = func_prefix##_prepend(arena, NULL, list_a->head);                             \
    type_name *walk = head_cell;                                                                         \
    for (const type_name *it = list_a->tail; it != NULL; it = it->tail) {                                \
      walk->tail = func_prefix##_prepend(arena, NULL, it->head);                                         \
      walk = walk->tail;                                                                                 \
    }                                                                                                    \
    walk->tail = (type_name *)list_b; /* shares b — lists are immutable values */                        \
    return head_cell;                                                                                    \
  }                                                                                                      \
                                                                                                         \
  size_t func_prefix##_length(const type_name *list) {                                                   \
    size_t n = 0;                                                                                        \
    for (const type_name *it = list; it != NULL; it = it->tail)                                          \
      n++;                                                                                               \
    return n;                                                                                            \
  }
