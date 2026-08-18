/**
 * @file span.c
 * @brief Implementation of Span operations.
 * @author solid-matrix
 * @version 0.0.5
 */

#include "span.h"
#include <assert.h>

Span span_empty(void)
{
    return (Span){.start = 0, .end = 0};
}

size_t span_len(Span span)
{
    assert(span.start <= span.end);
    return span.end - span.start;
}

bool span_is_empty(Span span)
{
    return span.start == span.end;
}

Span span_slice(Span span, size_t rel_start, size_t rel_end)
{
    assert(rel_start <= rel_end && rel_end <= span_len(span));
    return (Span){.start = span.start + rel_start, .end = span.start + rel_end};
}
