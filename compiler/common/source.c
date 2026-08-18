/**
 * @file source.c
 * @brief Implementation of the Source line index and Span accessors.
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
  for (size_t i = 0; i < sv.len; i++)
  {
    char c = sv.data[i];
    if (c == '\n')
    {
      count++;
    }
    else if (c == '\r')
    {
      if (i + 1 >= sv.len || sv.data[i + 1] != '\n')
      {
        count++;
      }
    }
  }

  Source source = {
      .string_view = sv,
      .line_count = count,
      .line_offsets = malloc(sizeof(size_t) * count)};

  if (source.line_offsets == NULL)
  {
    abort();
  }

  size_t line = 0;
  source.line_offsets[0] = 0;
  for (size_t i = 0; i < sv.len; i++)
  {
    char c = sv.data[i];
    if (c == '\n')
    {
      line++;
      source.line_offsets[line] = i + 1;
    }
    else if (c == '\r')
    {
      line++;
      if (i + 1 < sv.len && sv.data[i + 1] == '\n')
      {
        source.line_offsets[line] = i + 2; // CRLF: next line starts after "\r\n"
        i++;                               // skip the '\n'
      }
      else
      {
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
  assert(source->line_offsets != NULL);

  free(source->line_offsets);
  source->line_offsets = NULL;
  source->line_count = 0;
}

Position source_get_position(const Source *source, size_t offset)
{
  assert(offset <= source->string_view.len);
  // Binary search for the last line with line_offsets <= offset.
  size_t lo = 0;
  size_t hi = source->line_count - 1;
  while (lo < hi)
  {
    size_t mid = lo + (hi - lo + 1) / 2;
    if (source->line_offsets[mid] <= offset)
    {
      lo = mid;
    }
    else
    {
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

  if (line + 1 >= source->line_count)
  {
    return source->string_view.len;
  }
  size_t end = source->line_offsets[line + 1] - 1; // last terminator byte
  if (end > 0 && source->string_view.data[end] == '\n' && source->string_view.data[end - 1] == '\r')
  {
    end--; // CRLF: the whole "\r\n" is excluded from the line content
  }
  return end;
}

Span source_get_line_span(const Source *source, size_t line)
{
  return (Span){.start = source_get_line_start(source, line), .end = source_get_line_end(source, line)};
}

Span source_get_span(const Source *source)
{
  return (Span){.start = 0, .end = source->string_view.len};
}

StringView source_get_string_view(const Source *source, Span span)
{
  return sv_slice(source->string_view, span.start, span_len(span));
}

char source_get_char(const Source *source, size_t pos)
{
  return sv_char_at(source->string_view, pos);
}