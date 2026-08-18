/**
 * @file span.h
 * @brief A half-open [start, end) offset range, independent of Source.
 * @author solid-matrix
 * @version 0.0.5
 */

#ifndef SOLID_SPAN_H
#define SOLID_SPAN_H

#include <stddef.h>
#include <stdbool.h>

/**
 * @brief A half-open [start, end) byte offset range.
 */
typedef struct
{
    size_t start;
    size_t end;
} Span;

/**
 * @brief An empty span at offset 0.
 * @return The empty span.
 */
Span span_empty(void);

/**
 * @brief Length of the span.
 * @param span The span to measure.
 * @return end - start.
 */
size_t span_len(Span span);

/**
 * @brief True when the span covers zero length.
 * @param span The span to test.
 * @return True if start == end.
 */
bool span_is_empty(Span span);

/**
 * @brief Sub-span at relative offsets within this span.
 * @param span The span to slice.
 * @param rel_start Relative start offset.
 * @param rel_end Relative end offset. Asserts: rel_start <= rel_end <= span_len(span).
 * @return The sub-span.
 */
Span span_slice(Span span, size_t rel_start, size_t rel_end);

#endif /* SOLID_SPAN_H */
