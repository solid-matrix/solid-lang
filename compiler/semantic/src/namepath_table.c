/**
 * @file namepath_table.c
 * @brief Declaration to qualified path reverse table over a persistent
 *        address-ordered treap.
 * @author solid-matrix
 */

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>

#include "namepath_table.h"

// One keyed entry: a declaration maps to the qualified path it was defined
// under. Treap order: left/right subtrees by address, ancestors above
// descendants by priority. The node is the table: the treap root doubles as
// the table handle, and NULL is the empty table.
struct SemanticNamePathTable {
  SyntaxNode *decl;       // key: the declaration
  SemanticNamePath *path; // the declaration's full world path
  SemanticNamePathTable *left;
  SemanticNamePathTable *right;
  uint32_t priority;
};

static inline int cmp_decl_ptr(const SyntaxNode *a, const SyntaxNode *b) {
  uintptr_t ua = (uintptr_t)a;
  uintptr_t ub = (uintptr_t)b;
  return (ua > ub) - (ua < ub);
}

static uint32_t make_priority(const SyntaxNode *key) {
  uint32_t hash = 2166136261u;
  const unsigned char *bytes = (const unsigned char *)&key;
  for (size_t i = 0; i < sizeof key; i++)
    hash = (hash ^ (uint32_t)bytes[i]) * 16777619u;
  return hash;
}

static SemanticNamePathTable *node_new(Arena *arena, SyntaxNode *decl, SemanticNamePath *path) {
  SemanticNamePathTable *node = arena_alloc(arena, sizeof *node);
  node->decl = decl;
  node->path = path;
  node->left = NULL;
  node->right = NULL;
  node->priority = make_priority(decl);
  return node;
}

static SemanticNamePathTable *node_copy(Arena *arena, const SemanticNamePathTable *src) {
  SemanticNamePathTable *dst = arena_alloc(arena, sizeof *dst);
  *dst = *src;
  return dst;
}

static SemanticNamePathTable *rotate_right(SemanticNamePathTable *x) {
  SemanticNamePathTable *y = x->left;
  x->left = y->right;
  y->right = x;
  return y;
}

static SemanticNamePathTable *rotate_left(SemanticNamePathTable *x) {
  SemanticNamePathTable *y = x->right;
  x->right = y->left;
  y->left = x;
  return y;
}

static bool outranks(const SemanticNamePathTable *child, const SemanticNamePathTable *parent) {
  if (child->priority != parent->priority)
    return child->priority > parent->priority;
  return cmp_decl_ptr(child->decl, parent->decl) < 0;
}

static SemanticNamePathTable *insert_node(Arena *arena, SemanticNamePathTable *root, SemanticNamePathTable *node,
                                          bool *inserted) {
  if (root == NULL) {
    *inserted = true;
    return node;
  }

  int cmp = cmp_decl_ptr(node->decl, root->decl);
  if (cmp == 0) {
    *inserted = false;
    return root;
  }

  if (cmp < 0) {
    SemanticNamePathTable *new_left = insert_node(arena, root->left, node, inserted);
    if (!*inserted)
      return root;

    if (new_left == root->left && !outranks(new_left, root))
      return root;

    SemanticNamePathTable *copy = node_copy(arena, root);
    copy->left = new_left;
    if (outranks(copy->left, copy))
      copy = rotate_right(copy);
    return copy;
  } else {
    SemanticNamePathTable *new_right = insert_node(arena, root->right, node, inserted);
    if (!*inserted)
      return root;

    if (new_right == root->right && !outranks(new_right, root))
      return root;

    SemanticNamePathTable *copy = node_copy(arena, root);
    copy->right = new_right;
    if (outranks(copy->right, copy))
      copy = rotate_left(copy);
    return copy;
  }
}

// Counts the entries of one subtree.
static size_t count_entries(const SemanticNamePathTable *entry) {
  if (entry == NULL)
    return 0;
  return 1 + count_entries(entry->left) + count_entries(entry->right);
}

SemanticNamePathTable *semantic_namepath_table_empty(void) { return NULL; }

SemanticNamePathTable *semantic_namepath_table_insert(Arena *arena, SemanticNamePathTable *table, SyntaxNode *decl,
                                                      SemanticNamePath *path) {
  assert(arena != NULL);
  assert(decl != NULL);
  assert(path != NULL);
  assert(semantic_namepath_table_lookup(table, decl) == NULL); // duplicate key: caller bug

  bool inserted = false;
  SemanticNamePathTable *root = insert_node(arena, table, node_new(arena, decl, path), &inserted);
  assert(inserted);
  return root;
}

SemanticNamePath *semantic_namepath_table_lookup(const SemanticNamePathTable *table, const SyntaxNode *decl) {
  const SemanticNamePathTable *it = table;
  while (it != NULL) {
    int cmp = cmp_decl_ptr(decl, it->decl);
    if (cmp == 0)
      return it->path;
    it = cmp < 0 ? it->left : it->right;
  }
  return NULL;
}

size_t semantic_namepath_table_length(const SemanticNamePathTable *table) { return count_entries(table); }
