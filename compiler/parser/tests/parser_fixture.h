/**
 * @file parser_fixture.h
 * @brief Source/Parser lifecycle fixture for the parser's unit tests.
 * @author solid-matrix
 * @version 0.0.6
 */

#pragma once

#include "parser.h"
#include "source.h"

static Source *fx_source;
static Parser *fx_parser;

/** Points the fixture at @p text, releasing any previous parse. */
static void fx_begin(const char *text) {
  if (fx_parser != NULL) {
    parser_destroy(fx_parser);
    source_destroy(fx_source);
  }
  fx_source = source_from_cstr(text);
  fx_parser = parser_create(fx_source);
}

/** Releases the current parse; safe to call repeatedly. */
static void fx_release(void) {
  if (fx_parser != NULL) {
    parser_destroy(fx_parser);
    fx_parser = NULL;
  }
  if (fx_source != NULL) {
    source_destroy(fx_source);
    fx_source = NULL;
  }
}
