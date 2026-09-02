/**
 * @file binding_table.c
 * @brief Use-site to entity binding table over a persistent address-ordered treap.
 * @author solid-matrix
 */

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>

#include "binding_table.h"

// One keyed entry: a use-site maps to the entity it binds to. Treap order:
// left/right subtrees by address, ancestors above descendants by priority.
// The node is the table: the treap root doubles as the table handle, and
// NULL is the empty table. check may grow per-use-site payload (inferred
// type, constant value) in its own milestone; the storage shape is decided
// there.
struct SemanticBindingTable {
  SyntaxNode *syntax; // key: the use-site
  SyntaxNode *decl;   // the bound entity
  SemanticBindingTable *left;
  SemanticBindingTable *right;
  uint32_t priority;
};

// FNV-1a over the pointer bytes. Shapes are stable within a run, which is
// all the table needs — no observable order depends on it.
static uint32_t make_priority(const SyntaxNode *key) {
  uint32_t hash = 2166136261u;
  const unsigned char *bytes = (const unsigned char *)&key;
  for (size_t i = 0; i < sizeof(size_t); i++)
    hash = (hash ^ (uint32_t)bytes[i]) * 16777619u;
  return hash;
}

// True when @p entry outranks @p other and belongs closer to the root.
// Addresses are unique, so the order stays total without a fallback.
static bool entry_outranks(const SemanticBindingTable *entry, const SemanticBindingTable *other) {
  if (entry->priority != other->priority)
    return entry->priority > other->priority;
  return entry->syntax < other->syntax;
}

static SemanticBindingTable *entry_new(Arena *arena, SyntaxNode *syntax, SyntaxNode *decl) {
  SemanticBindingTable *entry = arena_alloc(arena, sizeof *entry);
  entry->syntax = syntax;
  entry->decl = decl;
  entry->left = NULL;
  entry->right = NULL;
  entry->priority = make_priority(syntax);
  return entry;
}

static SemanticBindingTable *entry_copy(Arena *arena, const SemanticBindingTable *entry) {
  SemanticBindingTable *copy = arena_alloc(arena, sizeof *copy);
  *copy = *entry;
  return copy;
}

// Persistent insert: copies the search spine and rotates on the way back
// up. The caller must not insert a key the table already contains.
static SemanticBindingTable *insert_walk(Arena *arena, SemanticBindingTable *root, SemanticBindingTable *fresh) {
  if (root == NULL)
    return fresh;

  SemanticBindingTable *copy = entry_copy(arena, root);
  if (fresh->syntax < copy->syntax) {
    copy->left = insert_walk(arena, copy->left, fresh);
    if (entry_outranks(copy->left, copy)) {
      SemanticBindingTable *up = copy->left; // fresh from the recursion
      copy->left = up->right;
      up->right = copy;
      return up;
    }
  } else {
    copy->right = insert_walk(arena, copy->right, fresh);
    if (entry_outranks(copy->right, copy)) {
      SemanticBindingTable *up = copy->right; // fresh from the recursion
      copy->right = up->left;
      up->left = copy;
      return up;
    }
  }
  return copy;
}

// Counts the entries of one subtree.
static size_t count_entries(const SemanticBindingTable *entry) {
  if (entry == NULL)
    return 0;
  return 1 + count_entries(entry->left) + count_entries(entry->right);
}

SemanticBindingTable *semantic_binding_table_empty(void) { return NULL; }

SemanticBindingTable *semantic_binding_table_insert(Arena *arena, SemanticBindingTable *table, SyntaxNode *syntax,
                                                    SyntaxNode *decl) {
  assert(arena != NULL);
  assert(semantic_binding_table_lookup(table, syntax) == NULL); // duplicate key: caller bug

  return insert_walk(arena, table, entry_new(arena, syntax, decl));
}

SyntaxNode *semantic_binding_table_lookup(const SemanticBindingTable *table, const SyntaxNode *syntax) {
  const SemanticBindingTable *it = table;
  while (it != NULL) {
    if (syntax == it->syntax)
      return it->decl;
    it = syntax < it->syntax ? it->left : it->right;
  }
  return NULL;
}

size_t semantic_binding_table_length(const SemanticBindingTable *table) { return count_entries(table); }
