/**
 * @file source.h
 * @brief Loaded source text with a per-line offset index, and span accessors.
 * @author solid-matrix
 * @version 0.0.5
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "span.h"
#include "string_view.h"

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
 * @brief A loaded source text plus a per-line offset index.
 *
 * `str` is not owned: the underlying buffer must outlive the Source.
 * `line_offsets` is owned by the Source and freed by source_destroy().
 * A Source is used by reference in this API: always pass it as a pointer.
 */
typedef struct {
  StringView string_view;
  size_t line_count;    // number of lines, at least 1
  size_t *line_offsets; // start offset of each line (indexed by 0-based line)
} Source;

/**
 * @brief Builds a Source over @p sv, indexing its lines.
 * @param sv The text to index; the underlying buffer must outlive the Source.
 * @return The indexed Source. A leading UTF-8 BOM (EF BB BF) is dropped from
 *         the indexed text so spans and positions are BOM-free.
 */
Source source_from_string_view(StringView sv);

/**
 * @brief Convenience: source_from_string_view(sv_from_cstr(str)).
 * @param str NUL-terminated text; must outlive the Source.
 * @return The indexed Source. A leading UTF-8 BOM (EF BB BF) is dropped from
 *         the indexed text so spans and positions are BOM-free.
 */
Source source_from_cstr(const char *str);

/**
 * @brief Frees the internal line index.
 * @param source The Source to destroy. Safe to call once.
 */
void source_destroy(Source *source);

/**
 * @brief Position of byte @p offset as 0-based row/col.
 * @param source The Source to query.
 * @param offset Byte offset. May equal the text length (one past the last
 *               byte); such a position lies on the final line.
 *               Asserts: offset <= source->string_view.len.
 * @return The position.
 */
SourcePosition source_get_position(const Source *source, size_t offset);

/**
 * @brief Start offset of line @p line (0-based).
 * @param source The Source to query.
 * @param line 0-based line number. Asserts: line < source->line_count.
 * @return The offset of the line start.
 */
size_t source_get_line_start(const Source *source, size_t line);

/**
 * @brief Offset of the start of line @p line's terminator.
 *
 * For the last line (no terminator) it is the end of the text.
 * @param source The Source to query.
 * @param line 0-based line number. Asserts: line < source->line_count.
 * @return The offset just past the line content.
 */
size_t source_get_line_end(const Source *source, size_t line);

/**
 * @brief Span of line @p line's content, excluding the line terminator.
 * @param source The Source to query.
 * @param line 0-based line number. Asserts: line < source->line_count.
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
 * @brief The text covered by @p span, as a view into the Source.
 * @param source The Source the span refers to.
 * @param span Offset range within the source text.
 * @return A StringView over the covered text.
 * @note Asserts (via sv_slice) that the span lies within the text.
 */
StringView source_string_view_at(const Source *source, Span span);

/**
 * @brief Byte at absolute offset @p pos in the source text.
 * @param source The Source to read from.
 * @param pos Absolute byte offset. Asserts: pos < source->string_view.len.
 * @return The byte.
 */
uint8_t source_byte_at(const Source *source, size_t pos);
