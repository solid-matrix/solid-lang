/**
 * @file source.c
 * @brief Implementation of the Source line index and SourceSpan.
 * @author solid-matrix
 * @version 0.0.5
 */

#include "source.h"

#include <assert.h>
#include <stdlib.h>

Source source_from_string_view(StringView sv)
{
  // Count lines: both '\n' and '\r' start a new line, but "\r\n" is a
  // single terminator and only counts once.
  size_t count = 1;
  for (size_t i = 0; i < sv.len; i++) {
    char c = sv.data[i];
    if (c == '\n') {
      count++;
    } else if (c == '\r') {
      if (i + 1 >= sv.len || sv.data[i + 1] != '\n') {
        count++;
      }
    }
  }

  Source source;
  source.str = sv;
  source.line_count = count;
  source.line_offsets = malloc(sizeof(size_t) * count);
  if (source.line_offsets == NULL) {
    abort();
  }

  size_t line = 0;
  source.line_offsets[0] = 0;
  for (size_t i = 0; i < sv.len; i++) {
    char c = sv.data[i];
    if (c == '\n') {
      line++;
      source.line_offsets[line] = i + 1;
    } else if (c == '\r') {
      line++;
      if (i + 1 < sv.len && sv.data[i + 1] == '\n') {
        source.line_offsets[line] = i + 2; // CRLF: next line starts after "\r\n"
        i++;                               // skip the '\n'
      } else {
        source.line_offsets[line] = i + 1;
      }
    }
  }
  return source;
}

Source source_from_cstr(const char *str)
{
  return source_from_string_view(sv_from_cstr(str));
}

void source_destroy(Source *source)
{
  free(source->line_offsets);
  source->line_offsets = NULL;
  source->line_count = 0;
}

Position source_get_position(const Source *source, size_t offset)
{
  assert(offset <= source->str.len);
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
  Position pos;
  pos.row = lo;
  pos.col = offset - source->line_offsets[lo];
  return pos;
}

size_t source_get_line_start(const Source *source, size_t line)
{
  assert(line < source->line_count);
  return source->line_offsets[line];
}

size_t source_get_line_end(const Source *source, size_t line)
{
  assert(line < source->line_count);
  if (line + 1 >= source->line_count) {
    return source->str.len;
  }
  size_t end = source->line_offsets[line + 1] - 1; // last terminator byte
  if (end > 0 && source->str.data[end] == '\n' && source->str.data[end - 1] == '\r') {
    end--; // CRLF: the whole "\r\n" is excluded from the line content
  }
  return end;
}

SourceSpan source_get_line_span(const Source *source, size_t line)
{
  SourceSpan span;
  span.src = source;
  span.start = source_get_line_start(source, line);
  span.end = source_get_line_end(source, line);
  return span;
}

SourceSpan source_to_span(const Source *source)
{
  SourceSpan span;
  span.src = source;
  span.start = 0;
  span.end = source->str.len;
  return span;
}

StringView span_to_string_view(SourceSpan span)
{
  StringView sv;
  sv.data = span.src->str.data + span.start;
  sv.len = span.end - span.start;
  return sv;
}

size_t span_len(SourceSpan span)
{
  return span.end - span.start;
}

bool span_is_empty(SourceSpan span)
{
  return span.start == span.end;
}

char span_get_char(SourceSpan span, size_t rel)
{
  assert(rel < span_len(span));
  return span.src->str.data[span.start + rel];
}

SourceSpan span_slice(SourceSpan span, size_t rel_start, size_t rel_end)
{
  assert(rel_start <= rel_end && rel_end <= span_len(span));
  SourceSpan sub;
  sub.src = span.src;
  sub.start = span.start + rel_start;
  sub.end = span.start + rel_end;
  return sub;
}
