/**
 * @file symbol_table.c
 * @brief Namespace tree of name-keyed treap levels.
 * @author solid-matrix
 */

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "symbol_table.h"

struct SemanticSymbolTable {
  Strview name;               // the entry's own name
  SyntaxNode *decl;           // NULL = namespace (content in down); else symbol
  SemanticSymbolTable *down;  // the namespace's content level
  SemanticSymbolTable *left;  // treap: names ordered before name
  SemanticSymbolTable *right; // treap: names ordered after name
  uint32_t priority;          // heap key: FNV-1a of name
};

static inline int compare_name(Strview a, Strview b) { return strview_compare(a, b); }

// Kind discriminator: what the entry carries. Never stored — a namespace
// keeps its content in down, a symbol its declaration in decl.
static bool is_namespace(const SemanticSymbolTable *entry) { return entry->decl == NULL; }

static uint32_t make_priority(Strview name) {
  uint32_t hash = 2166136261u;
  for (size_t i = 0; i < name.len; i++)
    hash = (hash ^ (uint32_t)name.data[i]) * 16777619u;
  return hash;
}

static SemanticSymbolTable *node_new(Arena *arena, Strview name, SyntaxNode *decl) {
  SemanticSymbolTable *node = arena_alloc(arena, sizeof *node);
  node->name = name;
  node->decl = decl;
  node->down = NULL;
  node->left = NULL;
  node->right = NULL;
  node->priority = make_priority(name);
  return node;
}

static SemanticSymbolTable *node_copy(Arena *arena, const SemanticSymbolTable *src) {
  SemanticSymbolTable *dst = arena_alloc(arena, sizeof *dst);
  *dst = *src;
  return dst;
}

static bool outranks(const SemanticSymbolTable *child, const SemanticSymbolTable *parent) {
  if (child->priority != parent->priority)
    return child->priority > parent->priority;
  return compare_name(child->name, parent->name) < 0;
}

static SemanticSymbolTable *rotate_right(SemanticSymbolTable *x) {
  SemanticSymbolTable *y = x->left;
  x->left = y->right;
  y->right = x;
  return y;
}

static SemanticSymbolTable *rotate_left(SemanticSymbolTable *x) {
  SemanticSymbolTable *y = x->right;
  x->right = y->left;
  y->left = x;
  return y;
}

static SemanticSymbolTable *level_lookup(SemanticSymbolTable *level, Strview name) {
  SemanticSymbolTable *node = level;
  while (node != NULL) {
    int cmp = compare_name(name, node->name);
    if (cmp == 0)
      return node;
    node = cmp < 0 ? node->left : node->right;
  }
  return NULL;
}

static SemanticSymbolTable *insert_node(Arena *arena, SemanticSymbolTable *root, SemanticSymbolTable *node,
                                        bool *inserted) {
  if (root == NULL) {
    *inserted = true;
    return node;
  }

  int cmp = compare_name(node->name, root->name);
  if (cmp == 0) {
    *inserted = false;
    return root;
  }

  if (cmp < 0) {
    SemanticSymbolTable *new_left = insert_node(arena, root->left, node, inserted);
    if (!*inserted)
      return root;

    if (new_left == root->left && !outranks(new_left, root))
      return root;

    SemanticSymbolTable *copy = node_copy(arena, root);
    copy->left = new_left;
    if (outranks(copy->left, copy))
      copy = rotate_right(copy);
    return copy;
  } else {
    SemanticSymbolTable *new_right = insert_node(arena, root->right, node, inserted);
    if (!*inserted)
      return root;

    if (new_right == root->right && !outranks(new_right, root))
      return root;

    SemanticSymbolTable *copy = node_copy(arena, root);
    copy->right = new_right;
    if (outranks(copy->right, copy))
      copy = rotate_left(copy);
    return copy;
  }
}

static SemanticSymbolTable *replace_node(Arena *arena, SemanticSymbolTable *root, SemanticSymbolTable *fresh) {
  int cmp = compare_name(fresh->name, root->name);
  if (cmp == 0) {
    fresh->left = root->left;
    fresh->right = root->right;
    return fresh;
  }

  if (cmp < 0) {
    SemanticSymbolTable *new_left = replace_node(arena, root->left, fresh);
    if (new_left == root->left)
      return root;
    SemanticSymbolTable *copy = node_copy(arena, root);
    copy->left = new_left;
    return copy;
  } else {
    SemanticSymbolTable *new_right = replace_node(arena, root->right, fresh);
    if (new_right == root->right)
      return root;
    SemanticSymbolTable *copy = node_copy(arena, root);
    copy->right = new_right;
    return copy;
  }
}

static SemanticSymbolTable *define_walk(Arena *arena, SemanticSymbolTable *level, const SemanticNamePath *path,
                                        SyntaxNode *node) {
  bool symbol_here = path->tail == NULL && node != NULL;

  SemanticSymbolTable *root = level;
  SemanticSymbolTable *hit = level_lookup(root, path->head);
  SemanticSymbolTable *mine;

  if (hit == NULL) {
    mine = node_new(arena, path->head, symbol_here ? node : NULL);
    bool inserted = false;
    root = insert_node(arena, root, mine, &inserted);
    assert(inserted);
  } else if (!symbol_here && is_namespace(hit)) {
    mine = hit;
  } else {
    return NULL;
  }

  if (path->tail != NULL) {
    SemanticSymbolTable *inner_root = define_walk(arena, mine->down, path->tail, node);
    if (inner_root == NULL)
      return NULL;

    if (inner_root != mine->down) {
      if (hit == NULL) {
        mine->down = inner_root;
      } else {
        SemanticSymbolTable *updated = node_copy(arena, mine);
        updated->down = inner_root;
        root = replace_node(arena, root, updated);
      }
    }
  }

  return root;
}

// Walks @p path from @p level and returns the tail entry, or NULL when a
// segment is missing or a non-tail segment names a symbol. The tail's kind
// is the caller's business.
static SemanticSymbolTable *walk_levels(SemanticSymbolTable *level, const SemanticNamePath *path) {
  SemanticSymbolTable *hit = NULL;
  for (const SemanticNamePath *p = path; p != NULL; p = p->tail) {
    hit = level_lookup(level, p->head);
    if (hit == NULL || (p->tail != NULL && is_namespace(hit) == false))
      return NULL;
    level = hit->down;
  }
  return hit;
}

// Counts the entries of one level: the treap nodes above @p entry,
// excluding everything nested under namespace downs.
static size_t count_entries(const SemanticSymbolTable *entry) {
  if (entry == NULL)
    return 0;
  return 1 + count_entries(entry->left) + count_entries(entry->right);
}

SemanticSymbolTable *semantic_symbol_table_empty(void) { return NULL; }

SemanticSymbolTable *semantic_symbol_table_insert(Arena *arena, SemanticSymbolTable *table,
                                                  const SemanticNamePath *path, SyntaxNode *node) {
  assert(arena != NULL);
  assert(path != NULL);

  return define_walk(arena, table, path, node);
}

SyntaxNode *semantic_symbol_table_lookup(const SemanticSymbolTable *table, const SemanticNamePath *path) {
  if (table == NULL)
    return NULL;

  SemanticSymbolTable *hit = walk_levels((SemanticSymbolTable *)table, path);
  return hit != NULL && !is_namespace(hit) ? hit->decl : NULL;
}

SemanticSymbolTable *semantic_symbol_table_subtable(const SemanticSymbolTable *table, const SemanticNamePath *path) {
  if (table == NULL)
    return NULL;

  if (path == NULL)
    return (SemanticSymbolTable *)table; // tables are immutable values

  SemanticSymbolTable *ns = walk_levels((SemanticSymbolTable *)table, path);
  return ns != NULL && is_namespace(ns) ? ns->down : NULL;
}

bool semantic_symbol_table_contains(const SemanticSymbolTable *table, const SemanticNamePath *path) {
  if (table == NULL)
    return false;

  if (path == NULL)
    return true; // the level itself is always a namespace

  return walk_levels((SemanticSymbolTable *)table, path) != NULL;
}

size_t semantic_symbol_table_length(const SemanticSymbolTable *table) { return count_entries(table); }
