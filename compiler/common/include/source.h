/**
 * @file source.h
 * @brief Owned source text with a per-line offset index, and span accessors.
 * @author solid-matrix
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "span.h"
#include "strview.h"

/**
 * @brief A zero-based line/column position within a Source.
 */
typedef struct {
  size_t row; // 0-based line
  size_t col; // 0-based BYTE column within the line. This is not a
              // character or display column: multi-byte UTF-8 sequences
              // count as multiple columns and tabs are not expanded.
} SourcePosition;

/**
 * @brief Loaded source text plus a per-line offset index.
 *
 * Source is opaque: its fields are private and reachable only through
 * the functions below. A Source owns its text: every constructor copies
 * the bytes into memory owned by the Source, so caller-side buffers may
 * be released as soon as construction returns. Each Source is created
 * by a constructor, used by reference through a pointer, and released
 * exactly once with source_destroy().
 */
typedef struct Source Source;

/**
 * @brief Builds a Source that owns a copy of @p sv, indexing its lines.
 * @param sv The text to copy and index.
 * @return The new Source, owned by the caller. A leading UTF-8 BOM
 *         (EF BB BF) is dropped from the stored text so spans and
 *         positions are BOM-free.
 */
Source *source_from_strview(Strview sv);

/**
 * @brief Convenience: source_from_strview(strview_from_cstr(str)).
 * @param str NUL-terminated text; it is copied, so it need not outlive
 *            the Source.
 * @return The new Source, owned by the caller. A leading UTF-8 BOM
 *         (EF BB BF) is dropped from the stored text so spans and
 *         positions are BOM-free.
 */
Source *source_from_cstr(const char *str);

/**
 * @brief Reads the whole file at @p path into a new Source and indexes
 *        its lines.
 *
 * The file is opened in binary mode, so the Source holds its exact
 * bytes; a leading UTF-8 BOM (EF BB BF) is dropped as above.
 * @param path Path of the file to read.
 * @return The new Source, owned by the caller, or NULL when the file
 *         cannot be opened or read.
 */
Source *source_from_file(const char *path);

/**
 * @brief Frees a Source created by a constructor, including its owned
 *        text and line index.
 * @param source The Source to destroy; must come from a constructor and
 *               be destroyed exactly once.
 */
void source_destroy(Source *source);

/**
 * @brief Number of lines in the text; at least 1.
 * @param source The Source to query.
 * @return The line count.
 */
size_t source_line_count(const Source *source);

/**
 * @brief Position of byte @p offset as 0-based row/col.
 * @param source The Source to query.
 * @param offset Byte offset. May equal the text length (one past the last
 *               byte); such a position lies on the final line.
 *               Asserts: offset <= the text length.
 * @return The position.
 */
SourcePosition source_get_position(const Source *source, size_t offset);

/**
 * @brief Start offset of line @p line (0-based).
 * @param source The Source to query.
 * @param line 0-based line number. Asserts: line < source_line_count(source).
 * @return The offset of the line start.
 */
size_t source_get_line_start(const Source *source, size_t line);

/**
 * @brief Offset of the start of line @p line's terminator.
 *
 * For the last line (no terminator) it is the end of the text.
 * @param source The Source to query.
 * @param line 0-based line number. Asserts: line < source_line_count(source).
 * @return The offset just past the line content.
 */
size_t source_get_line_end(const Source *source, size_t line);

/**
 * @brief Span of line @p line's content, excluding the line terminator.
 * @param source The Source to query.
 * @param line 0-based line number. Asserts: line < source_line_count(source).
 * @return The line content span.
 */
Span source_get_line_span(const Source *source, size_t line);

/**
 * @brief Span of the whole source text.
 * @param source The Source to query.
 * @return The whole-text span.
 */
Span source_get_span(const Source *source);

/**
 * @brief The whole source text, as a view into the Source.
 * @param source The Source to view.
 * @return A Strview over the full text; it stays valid for the
 *         Source's lifetime and is never NULL-data.
 */
Strview source_get_strview(const Source *source);

/**
 * @brief The text covered by @p span, as a view into the Source.
 * @param source The Source the span refers to.
 * @param span Offset range within the source text.
 * @return A Strview over the covered text.
 * @note Asserts (via strview_slice) that the span lies within the text.
 */
Strview source_strview_at(const Source *source, Span span);

/**
 * @brief Byte at absolute offset @p pos in the source text.
 * @param source The Source to read from.
 * @param pos Absolute byte offset. Asserts: pos < the text length.
 * @return The byte.
 */
uint8_t source_byte_at(const Source *source, size_t pos);
