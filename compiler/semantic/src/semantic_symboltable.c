/**
 * @file semantic_symboltable.c
 * @brief Namespace tree of name-keyed treap levels.
 * @author solid-matrix
 * @version 0.0.5
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "semantic_symboltable.h"

// One keyed entry within a single namespace level: a name maps to either a
// nested namespace (down) or a declaration (decl). Treap order: left/right
// subtrees by name, ancestors above descendants by priority.
typedef enum {
  SEMANTIC_ENTRY_NAMESPACE,
  SEMANTIC_ENTRY_SYMBOL,
} SemanticEntryKind;

typedef struct SemanticEntry SemanticEntry;
struct SemanticEntry {
  Strview name;
  SemanticEntryKind kind;
  SyntaxNode *decl;    // SYMBOL only
  SemanticEntry *down; // NAMESPACE only: the next level's tree root
  SemanticEntry *left;
  SemanticEntry *right;
  uint32_t priority;
};

struct SemanticSymbolTable {
  Arena *arena;
  SemanticEntry *root; // top level's tree root
};

// FNV-1a over the name bytes; the fixed constants keep tree shapes
// reproducible across runs and platforms.
static uint32_t entry_priority(Strview name) {
  uint32_t hash = 2166136261u;
  for (size_t i = 0; i < name.len; i++)
    hash = (hash ^ (uint32_t)name.data[i]) * 16777619u;
  return hash;
}

// True when @p node outranks @p other and belongs closer to the root. Equal
// priorities fall back to the name, so the order stays total (names are
// unique within a level) and independent of insertion order.
static bool entry_outranks(const SemanticEntry *node, const SemanticEntry *other) {
  if (node->priority != other->priority)
    return node->priority > other->priority;
  return strview_compare(node->name, other->name) < 0;
}

static SemanticEntry *entry_new(Arena *arena, Strview name, SemanticEntryKind kind) {
  SemanticEntry *entry = arena_alloc(arena, sizeof *entry);
  entry->name = name;
  entry->kind = kind;
  entry->decl = NULL;
  entry->down = NULL;
  entry->left = NULL;
  entry->right = NULL;
  entry->priority = entry_priority(name);
  return entry;
}

static SemanticEntry *entry_copy(Arena *arena, const SemanticEntry *entry) {
  SemanticEntry *copy = arena_alloc(arena, sizeof *copy);
  *copy = *entry;
  return copy;
}

// Searches one level by name; NULL when absent.
static SemanticEntry *level_find(SemanticEntry *level, Strview name) {
  SemanticEntry *node = level;
  while (node != NULL) {
    int order = strview_compare(name, node->name);
    if (order == 0)
      return node;
    node = order < 0 ? node->left : node->right;
  }
  return NULL;
}

// Persistent treap insert: copies the search spine and rotates on the way
// back up. The caller must not insert a name the level already contains.
static SemanticEntry *level_insert(Arena *arena, SemanticEntry *level, SemanticEntry *fresh) {
  if (level == NULL)
    return fresh;

  SemanticEntry *copy = entry_copy(arena, level);
  if (strview_compare(fresh->name, copy->name) < 0) {
    copy->left = level_insert(arena, copy->left, fresh);
    if (entry_outranks(copy->left, copy)) {
      SemanticEntry *up = copy->left; // fresh from the recursion
      copy->left = up->right;
      up->right = copy;
      return up;
    }
  } else {
    copy->right = level_insert(arena, copy->right, fresh);
    if (entry_outranks(copy->right, copy)) {
      SemanticEntry *up = copy->right; // fresh from the recursion
      copy->right = up->left;
      up->left = copy;
      return up;
    }
  }
  return copy;
}

// Same-key replacement: @p fresh takes over the existing entry's subtree arms.
// Key and priority are identical, so the heap order — and the shape — is
// unchanged; only the spine along the search path is copied. The key must
// exist in the level.
static SemanticEntry *level_replace(Arena *arena, SemanticEntry *level, SemanticEntry *fresh) {
  int order = strview_compare(fresh->name, level->name);
  if (order == 0) {
    fresh->left = level->left;
    fresh->right = level->right;
    return fresh;
  }

  SemanticEntry *copy = entry_copy(arena, level);
  if (order < 0)
    copy->left = level_replace(arena, copy->left, fresh);
  else
    copy->right = level_replace(arena, copy->right, fresh);
  return copy;
}

// Defines the chain from the outermost segment inward: each frame registers
// its segment in @p level, then recurses into the entry's down level. On the
// unwind every deeper level publishes its (possibly new) root upward — a
// shared entry whose level changed is rewritten (same key, same priority).
// Returns this level's new root, or NULL on collision — a symbol redefined,
// or a name taken by the other kind.
static SemanticEntry *define_walk(Arena *arena, SemanticEntry *level, const SemanticNamePath *path, SyntaxNode *node) {
  SemanticEntryKind kind = path->next == NULL && node != NULL ? SEMANTIC_ENTRY_SYMBOL : SEMANTIC_ENTRY_NAMESPACE;

  SemanticEntry *root = level;
  SemanticEntry *hit = level_find(root, path->name);
  SemanticEntry *mine;
  if (hit == NULL) {
    mine = entry_new(arena, path->name, kind);
    if (path->next == NULL && node != NULL)
      mine->decl = node;
    root = level_insert(arena, root, mine);
  } else if (kind == SEMANTIC_ENTRY_NAMESPACE && hit->kind == SEMANTIC_ENTRY_NAMESPACE) {
    mine = hit; // namespace redeclaration: share the in-tree entry
  } else {
    return NULL;
  }

  if (path->next != NULL) {
    SemanticEntry *inner_root = define_walk(arena, mine->down, path->next, node);
    if (inner_root == NULL)
      return NULL;
    if (inner_root != mine->down) { // a deeper level changed: republish it
      if (hit == NULL) {
        mine->down = inner_root; // freshly inserted this call — safe to mutate
      } else {
        SemanticEntry *updated = entry_copy(arena, mine);
        updated->down = inner_root;
        mine = updated;
        root = level_replace(arena, root, updated);
      }
    }
  }

  return root;
}

// Resolves the chain level by level: the tail segment must name an entry of
// @p head_kind, every other segment a namespace. Returns the tail entry, or
// NULL when a segment is missing or mistyped.
static SemanticEntry *resolve_levels(SemanticEntry *level, const SemanticNamePath *path, SemanticEntryKind head_kind) {
  SemanticEntry *hit = NULL;
  for (const SemanticNamePath *p = path; p != NULL; p = p->next) {
    SemanticEntryKind kind = p->next == NULL ? head_kind : SEMANTIC_ENTRY_NAMESPACE;
    hit = level_find(level, p->name);
    if (hit == NULL || hit->kind != kind)
      return NULL;
    level = hit->down;
  }
  return hit;
}

SemanticSymbolTable *semantic_symboltable_create(Arena *arena) {
  SemanticSymbolTable *table = arena_alloc(arena, sizeof *table);
  table->arena = arena;
  table->root = NULL;
  return table;
}

SemanticSymbolTable *semantic_symboltable_define(SemanticSymbolTable *table, const SemanticNamePath *path,
                                                 SyntaxNode *node) {
  if (table == NULL || path == NULL)
    return NULL;

  SemanticEntry *root = define_walk(table->arena, table->root, path, node);
  if (root == NULL)
    return NULL;

  SemanticSymbolTable *defined = arena_alloc(table->arena, sizeof *defined);
  defined->arena = table->arena;
  defined->root = root;
  return defined;
}

SyntaxNode *semantic_symboltable_lookup(const SemanticSymbolTable *table, const SemanticNamePath *path) {
  if (table == NULL || path == NULL)
    return NULL;
  SemanticEntry *hit = resolve_levels(table->root, path, SEMANTIC_ENTRY_SYMBOL);
  return hit != NULL ? hit->decl : NULL;
}

const SemanticSymbolTable *semantic_symboltable_sub(const SemanticSymbolTable *table, const SemanticNamePath *path) {
  if (table == NULL)
    return NULL;
  if (path == NULL)
    return table; // an empty chain views the whole table

  SemanticEntry *ns = resolve_levels(table->root, path, SEMANTIC_ENTRY_NAMESPACE);
  if (ns == NULL)
    return NULL;

  SemanticSymbolTable *view = arena_alloc(table->arena, sizeof *view);
  view->arena = table->arena;
  view->root = ns->down;
  return view;
}
