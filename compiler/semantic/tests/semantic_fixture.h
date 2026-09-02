/**
 * @file semantic_fixture.h
 * @brief Shared fixtures for the semantic pass tests.
 * @details Includes the public syntax entry point and the internal pass
 *          headers; helpers are plain (non-static) because every test
 *          executable has a single translation unit.
 */

#pragma once

#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>

#include "arena.h"
#include "internal.h"
#include "namepath.h"
#include "source.h"
#include "syntax_parse.h"
#include "test_support.h"

// Parses a syntactically clean unit; pass tests never feed broken sources,
// so fixture misuse aborts.
static inline SyntaxProgram *parse_unit(Arena *arena, const char *text) {
  Source *source = source_from_cstr(text);
  SyntaxParseResult r = syntax_parse(source, arena);
  if (r.errors != NULL)
    abort();
  return r.program;
}

// Builds a path from @p count C strings, in head-to-tail order.
static inline SemanticNamePath *path_vof(Arena *arena, int count, va_list args) {
  SemanticNamePath *head = NULL;
  SemanticNamePath *walk = NULL;
  for (int i = 0; i < count; i++) {
    SemanticNamePath *cell = arena_alloc(arena, sizeof *cell);
    cell->head = strview_from_cstr(va_arg(args, const char *));
    cell->tail = NULL;
    if (walk == NULL)
      head = cell;
    else
      walk->tail = cell;
    walk = cell;
  }
  return head;
}

static inline SemanticNamePath *path_of(Arena *arena, int count, ...) {
  va_list args;
  va_start(args, count);
  SemanticNamePath *path = path_vof(arena, count, args);
  va_end(args);
  return path;
}

// Translation units in source order, each parsed from a C string.
static inline SemanticProgramList *units_of(Arena *arena, int count, ...) {
  va_list args;
  va_start(args, count);
  SemanticProgramList *head = NULL;
  SemanticProgramList *tail = NULL;
  for (int i = 0; i < count; i++) {
    SemanticProgramList *cell = arena_alloc(arena, sizeof *cell);
    cell->program = parse_unit(arena, va_arg(args, const char *));
    cell->next = NULL;
    if (tail == NULL)
      head = cell;
    else
      tail->next = cell;
    tail = cell;
  }
  va_end(args);
  return head;
}

static inline SemanticModule *module_of(Arena *arena, SemanticNamePath *path, SemanticProgramList *units) {
  SemanticModule *module = arena_alloc(arena, sizeof *module);
  module->path = path;
  module->programs = units;
  return module;
}

static inline SemanticModuleList *modules_of(Arena *arena, int count, ...) {
  va_list args;
  va_start(args, count);
  SemanticModuleList *head = NULL;
  SemanticModuleList *tail = NULL;
  for (int i = 0; i < count; i++) {
    SemanticModuleList *cell = arena_alloc(arena, sizeof *cell);
    cell->module = va_arg(args, SemanticModule *);
    cell->next = NULL;
    if (tail == NULL)
      head = cell;
    else
      tail->next = cell;
    tail = cell;
  }
  va_end(args);
  return head;
}

static inline SemanticCollectResult run_collect(Arena *arena, const SemanticModuleList *modules) {
  SemanticAnalyzer analyzer = {.arena = arena, .modules = modules, .params = NULL};
  return semantic_collect(&analyzer);
}
