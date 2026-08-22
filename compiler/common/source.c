/**
 * @file source.c
 * @brief Implementation of the Source line index and Span accessors.
 * @author solid-matrix
 * @version 0.0.5
 */

#include <assert.h>

#include "mem.h"
#include "source.h"

Source source_from_string_view(StringView sv) {
  // A leading UTF-8 BOM (EF BB BF) is not source text; drop it so that
  // spans, line/column positions, and string views stay BOM-free.
  if (sv.len >= 3 && sv.data[0] == 0xEF && sv.data[1] == 0xBB &&
      sv.data[2] == 0xBF)
    sv = sv_slice(sv, 3, sv.len - 3);

  // Count lines: both '\n' and '\r' start a new line, but "\r\n" is a
  // single terminator and only counts once.
  size_t count = 1;
  for (size_t i = 0; i < sv.len; i++) {
    uint8_t c = sv.data[i];
    if (c == '\n') {
      count++;
    } else if (c == '\r') {
      if (i + 1 >= sv.len || sv.data[i + 1] != '\n') {
        count++;
      }
    }
  }

  Source source = {.string_view = sv,
                   .line_count = count,
                   .line_offsets = xmalloc(sizeof(size_t) * count)};

  size_t line = 0;
  source.line_offsets[0] = 0;
  for (size_t i = 0; i < sv.len; i++) {
    uint8_t c = sv.data[i];
    if (c == '\n') {
      line++;
      source.line_offsets[line] = i + 1;
    } else if (c == '\r') {
      line++;
      if (i + 1 < sv.len && sv.data[i + 1] == '\n') {
        source.line_offsets[line] =
            i + 2; // CRLF: next line starts after "\r\n"
        i++;       // skip the '\n'
      } else {
        source.line_offsets[line] = i + 1;
      }
    }
  }
  return source;
}

Source source_from_cstr(const char *str) {
  return source_from_string_view(sv_from_cstr(str));
}

void source_destroy(Source *source) {
  assert(source->line_offsets != NULL);

  xfree(source->line_offsets);
  source->line_offsets = NULL;
  source->line_count = 0;
}

SourcePosition source_get_position(const Source *source, size_t offset) {
  assert(offset <= source->string_view.len);
  // Binary search for the last line with line_offsets <= offset.
  size_t lo = 0;
  size_t hi = source->line_count - 1;
  while (lo < hi) {
    size_t mid = lo + (hi - lo + 1) / 2;
    if (source->line_offsets[mid] <= offset) {
      lo = mid;
    } else {
      hi = mid - 1;
    }
  }
  SourcePosition pos;
  pos.row = lo;
  pos.col = offset - source->line_offsets[lo];
  return pos;
}

size_t source_get_line_start(const Source *source, size_t line) {
  assert(line < source->line_count);

  return source->line_offsets[line];
}

size_t source_get_line_end(const Source *source, size_t line) {
  assert(line < source->line_count);

  if (line + 1 >= source->line_count) {
    return source->string_view.len;
  }
  size_t end = source->line_offsets[line + 1] - 1; // last terminator byte
  if (end > 0 && source->string_view.data[end] == '\n' &&
      source->string_view.data[end - 1] == '\r') {
    end--; // CRLF: the whole "\r\n" is excluded from the line content
  }
  return end;
}

Span source_get_line_span(const Source *source, size_t line) {
  return (Span){.start = source_get_line_start(source, line),
                .end = source_get_line_end(source, line)};
}

Span source_get_span(const Source *source) {
  return (Span){.start = 0, .end = source->string_view.len};
}

StringView source_string_view_at(const Source *source, Span span) {
  return sv_slice(source->string_view, span.start, span_len(span));
}

uint8_t source_byte_at(const Source *source, size_t pos) {
  return sv_byte_at(source->string_view, pos);
}
