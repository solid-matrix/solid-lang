/**
 * @file source.c
 * @brief Implementation of the Source line index and Span accessors.
 * @author solid-matrix
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "source.h"
#include "xmem.h"

/**
 * @brief The private representation of a Source.
 *
 * The Source owns both allocations: @p bytes holds the (BOM-stripped)
 * text and @p line_offsets indexes it; both are freed by
 * source_destroy(), together with the Source itself.
 */
struct Source {
  uint8_t *bytes;       // owned copy of the text; never NULL
  size_t len;           // text length in bytes
  size_t line_count;    // number of lines, at least 1
  size_t *line_offsets; // start offset of each line (indexed by 0-based line)
};

/**
 * @brief Builds a Source by adopting ownership of @p bytes.
 * @param bytes A heap buffer from one of the x* allocators, holding
 *              exactly max(@p len, 1) bytes; ownership passes to the
 *              returned Source.
 * @param len Number of text bytes in the buffer.
 * @return The new Source with its line index built.
 */
static Source *source_adopt(uint8_t *bytes, size_t len) {
  // A leading UTF-8 BOM (EF BB BF) is not source text; drop it so that
  // spans, line/column positions, and string views stay BOM-free.
  if (len >= 3 && bytes[0] == 0xEF && bytes[1] == 0xBB && bytes[2] == 0xBF) {
    memmove(bytes, bytes + 3, len - 3);
    len -= 3;
  }

  // Count lines: both '\n' and '\r' start a new line, but "\r\n" is a
  // single terminator and only counts once.
  size_t count = 1;
  for (size_t i = 0; i < len; i++) {
    uint8_t c = bytes[i];
    if (c == '\n') {
      count++;
    } else if (c == '\r') {
      if (i + 1 >= len || bytes[i + 1] != '\n') {
        count++;
      }
    }
  }

  Source *source = xmalloc(sizeof(Source));
  *source = (Source){.bytes = bytes,
                     .len = len,
                     .line_count = count,
                     .line_offsets = xmalloc(sizeof(size_t) * count)};

  size_t line = 0;
  source->line_offsets[0] = 0;
  for (size_t i = 0; i < len; i++) {
    uint8_t c = bytes[i];
    if (c == '\n') {
      line++;
      source->line_offsets[line] = i + 1;
    } else if (c == '\r') {
      line++;
      if (i + 1 < len && bytes[i + 1] == '\n') {
        source->line_offsets[line] =
            i + 2; // CRLF: next line starts after "\r\n"
        i++;       // skip the '\n'
      } else {
        source->line_offsets[line] = i + 1;
      }
    }
  }
  return source;
}

Source *source_from_strview(Strview sv) {
  // Copy so that the Source owns its text and outlives the caller's
  // buffer; the x* allocators require a positive size.
  uint8_t *bytes = xmalloc(sv.len > 0 ? sv.len : 1);
  if (sv.len > 0)
    memcpy(bytes, sv.data, sv.len);
  return source_adopt(bytes, sv.len);
}

Source *source_from_cstr(const char *str) {
  return source_from_strview(strview_from_cstr(str));
}

Source *source_from_file(const char *path) {
  FILE *file = fopen(path, "rb");
  if (file == NULL)
    return NULL;

  // Stream the file in and grow the buffer as needed: this avoids
  // fseek/ftell portability pitfalls and works for inputs of unknown
  // size. The buffer starts at a page-ish granularity and doubles.
  size_t len = 0;
  size_t capacity = 4096;
  uint8_t *bytes = xmalloc(capacity);

  size_t n;
  while ((n = fread(bytes + len, 1, capacity - len, file)) > 0) {
    len += n;
    if (len == capacity) {
      capacity *= 2;
      bytes = xrealloc(bytes, capacity);
    }
  }

  bool failed = ferror(file) != 0;
  fclose(file);

  if (failed) {
    xfree(bytes);
    return NULL;
  }

  // Shrink to the exact size so no slack is kept for the Source's
  // lifetime; keep the allocation non-empty for xfree/xrealloc rules.
  bytes = xrealloc(bytes, len > 0 ? len : 1);
  return source_adopt(bytes, len);
}

void source_destroy(Source *source) {
  assert(source != NULL);
  assert(source->bytes != NULL);
  assert(source->line_offsets != NULL);

  xfree(source->bytes);
  xfree(source->line_offsets);
  xfree(source);
}

size_t source_line_count(const Source *source) { return source->line_count; }

SourcePosition source_get_position(const Source *source, size_t offset) {
  assert(offset <= source->len);
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
    return source->len;
  }
  size_t end = source->line_offsets[line + 1] - 1; // last terminator byte
  if (end > 0 && source->bytes[end] == '\n' && source->bytes[end - 1] == '\r') {
    end--; // CRLF: the whole "\r\n" is excluded from the line content
  }
  return end;
}

Span source_get_line_span(const Source *source, size_t line) {
  return (Span){.start = source_get_line_start(source, line),
                .end = source_get_line_end(source, line)};
}

Span source_get_span(const Source *source) {
  return (Span){.start = 0, .end = source->len};
}

Strview source_get_strview(const Source *source) {
  return strview_create(source->bytes, source->len);
}

Strview source_strview_at(const Source *source, Span span) {
  Strview sv = source_get_strview(source);
  return strview_slice(sv, span.start, span_len(span));
}

uint8_t source_byte_at(const Source *source, size_t pos) {
  assert(pos < source->len);
  return source->bytes[pos];
}
