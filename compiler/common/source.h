/**
 * @file source.h
 * @brief Loaded source text with a per-line offset index, and spans.
 * @author solid-matrix
 * @version 0.0.5
 */

#ifndef SOLID_SOURCE_H
#define SOLID_SOURCE_H

#include "string_view.h"
#include <stdbool.h>
#include <stddef.h>

/**
 * @brief A zero-based line/column position within a Source.
 */
typedef struct {
  size_t row; // 0-based line
  size_t col; // 0-based byte column within the line
} Position;

/**
 * @brief A loaded source text plus a per-line offset index.
 *
 * `str` is not owned: the underlying buffer must outlive the Source.
 * `line_offsets` is owned by the Source and freed by source_destroy().
 * A Source is used by reference in this API: always pass it as a pointer.
 */
typedef struct {
  StringView str;
  size_t line_count;    // number of lines, at least 1
  size_t *line_offsets; // start offset of each line (indexed by 0-based line)
} Source;

/**
 * @brief A half-open [start, end) slice of a Source, in absolute offsets.
 *
 * Line terminators are '\n' (LF), '\r' (CR), or "\r\n" (CRLF, counted as
 * a single terminator). Line spans exclude the terminator entirely.
 * `src` must outlive the span.
 */
typedef struct {
  const Source *src;
  size_t start;
  size_t end;
} SourceSpan;

/**
 * @brief Builds a Source over @p sv, indexing its lines.
 * @param sv The text to index; the underlying buffer must outlive the Source.
 * @return The indexed Source.
 */
Source source_from_string_view(StringView sv);

/**
 * @brief Convenience: source_from_string_view(sv_from_cstr(str)).
 * @param str NUL-terminated text; must outlive the Source.
 * @return The indexed Source.
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
 * @param offset Byte offset. Asserts: offset <= source->str.len.
 * @return The position.
 */
Position source_get_position(const Source *source, size_t offset);

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
SourceSpan source_get_line_span(const Source *source, size_t line);

/**
 * @brief Span of the whole source text.
 * @param source The Source to query.
 * @return The whole-text span.
 */
SourceSpan source_to_span(const Source *source);

/**
 * @brief The text covered by the span.
 * @param span The span to convert.
 * @return A StringView over the span's text.
 */
StringView span_to_string_view(SourceSpan span);

/**
 * @brief Number of bytes in the span.
 * @param span The span to measure.
 * @return end - start.
 */
size_t span_len(SourceSpan span);

/**
 * @brief True when the span covers no bytes.
 * @param span The span to test.
 * @return True if start == end.
 */
bool span_is_empty(SourceSpan span);

/**
 * @brief Byte at relative offset @p rel within the span.
 * @param span The span to read from.
 * @param rel Relative offset. Asserts: rel < span_len(span).
 * @return The byte.
 */
char span_get_char(SourceSpan span, size_t rel);

/**
 * @brief Sub-span at relative offsets within this span.
 * @param span The span to slice.
 * @param rel_start Relative start offset.
 * @param rel_end Relative end offset. Asserts: rel_start <= rel_end <=
 * span_len(span).
 * @return The sub-span.
 */
SourceSpan span_slice(SourceSpan span, size_t rel_start, size_t rel_end);

#endif /* SOLID_SOURCE_H */
