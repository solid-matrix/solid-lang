/**
 * @file namepath.c
 * @brief Name path chain operations.
 * @author solid-matrix
 * @version 0.0.5
 */

#include <assert.h>

#include "namepath.h"

SemanticNamePath *semantic_namepath_empty(void) { return NULL; }

SemanticNamePath *semantic_namepath_from_array(Arena *arena, const Strview *names, size_t count) {
  assert(arena != NULL);
  assert(count == 0 || names != NULL);

  SemanticNamePath *path = NULL;
  for (size_t i = count; i > 0; i--) // build back-to-front: O(n)
    path = semantic_namepath_prepend(arena, path, names[i - 1]);
  return path;
}

SemanticNamePath *semantic_namepath_prepend(Arena *arena, SemanticNamePath *path, Strview name) {
  assert(arena != NULL);

  SemanticNamePath *cell = arena_alloc(arena, sizeof(SemanticNamePath)); // OOM is fatal
  cell->name = name;
  cell->next = path;
  return cell;
}

SemanticNamePath *semantic_namepath_append(Arena *arena, SemanticNamePath *path, Strview name) {
  assert(arena != NULL);

  if (path == NULL)
    return semantic_namepath_prepend(arena, NULL, name);

  // Copy every cell so the source path stays valid and unchanged.
  SemanticNamePath *head = semantic_namepath_prepend(arena, NULL, path->name);
  SemanticNamePath *tail = head;
  for (const SemanticNamePath *it = path->next; it != NULL; it = it->next) {
    tail->next = semantic_namepath_prepend(arena, NULL, it->name);
    tail = tail->next;
  }
  tail->next = semantic_namepath_prepend(arena, NULL, name);
  return head;
}

Strview semantic_namepath_head(SemanticNamePath *path) {
  assert(!semantic_namepath_is_empty(path));

  return path->name;
}

SemanticNamePath *semantic_namepath_tail(SemanticNamePath *path) {
  assert(!semantic_namepath_is_empty(path));

  return path->next;
}

Strview semantic_namepath_at(SemanticNamePath *path, size_t n) {
  const SemanticNamePath *it = path;
  while (n-- > 0) {
    assert(!semantic_namepath_is_empty(it)); // out of range

    it = it->next;
  }
  assert(!semantic_namepath_is_empty(it));

  return it->name;
}

bool semantic_namepath_is_empty(const SemanticNamePath *path) { return path == NULL; }

SemanticNamePath *semantic_namepath_reverse(Arena *arena, SemanticNamePath *path) {
  assert(arena != NULL);

  SemanticNamePath *result = NULL;
  for (const SemanticNamePath *it = path; it != NULL; it = it->next)
    result = semantic_namepath_prepend(arena, result, it->name);
  return result;
}

SemanticNamePath *semantic_namepath_concat(Arena *arena, SemanticNamePath *path_a, SemanticNamePath *path_b) {
  assert(arena != NULL);

  if (semantic_namepath_is_empty(path_a))
    return path_b; // shares b wholesale

  SemanticNamePath *head = semantic_namepath_prepend(arena, NULL, path_a->name);
  SemanticNamePath *tail = head;
  for (const SemanticNamePath *it = path_a->next; it != NULL; it = it->next) {
    tail->next = semantic_namepath_prepend(arena, NULL, it->name);
    tail = tail->next;
  }
  tail->next = path_b; // share b wholesale
  return head;
}

size_t semantic_namepath_length(SemanticNamePath *path) {
  size_t n = 0;
  for (const SemanticNamePath *it = path; it != NULL; it = it->next)
    n++;
  return n;
}
