/**
 * @file string_view.h
 * @brief Non-owning, length-bounded views over character buffers.
 * @author solid-matrix
 * @version 0.0.5
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>

/**
 * @brief Builds a StringView from a string literal at compile time.
 * @note The argument MUST be a string literal (sizeof is applied to it).
 * Usage: StringView v = SV("hello"); or in a static table.
 */
#define SV(s) ((StringView){.data = s, .len = sizeof(s) - 1})

/**
 * @brief A non-owning, length-bounded view over a character buffer.
 *
 * Never allocates, never NUL-terminates, and does not copy the data.
 */
typedef struct
{
  const uint8_t *data;
  size_t len;
} StringView;

/**
 * @brief Creates a view over @p str with explicit @p len bytes.
 * @param str The buffer to view.
 * @param len Number of bytes to view.
 * @return The view.
 * @note Requires: str != NULL, or len == 0.
 */
StringView sv_create(const uint8_t *str, size_t len);

/**
 * @brief Returns an empty view (data == NULL, len == 0).
 * @return The empty view.
 */
StringView sv_empty(void);

/**
 * @brief Creates a view over a NUL-terminated string, using strlen.
 * @param str The NUL-terminated string.
 * @return The view.
 * @note Requires: str != NULL.
 */
StringView sv_from_cstr(const char *str);

/**
 * @brief Returns true when the view contains no bytes.
 * @param sv The view to test.
 * @return True if len == 0.
 */
bool sv_is_empty(StringView sv);

/**
 * @brief Byte-wise equality. Safe for views containing embedded NULs.
 * @param a First view.
 * @param b Second view.
 * @return True if both length and bytes are equal.
 */
bool sv_equals(StringView a, StringView b);

/**
 * @brief Lexicographic comparison (memcmp-based).
 * @param a First view.
 * @param b Second view.
 * @return Negative, zero, or positive.
 */
int sv_compare(StringView a, StringView b);

/**
 * @brief Returns the sub-view [start, start + len).
 * @param sv The view to slice.
 * @param start Relative start offset.
 * @param len Number of bytes.
 * @return The sub-view.
 * @note Asserts: start <= sv.len and len <= sv.len - start.
 */
StringView sv_slice(StringView sv, size_t start, size_t len);

/**
 * @brief Returns the byte at position @p pos.
 * @param sv The view to read from.
 * @param pos Position within the view.
 * @return The byte.
 * @note Asserts: pos < sv.len.
 */
uint8_t sv_byte_at(StringView sv, size_t pos);

/**
 * @brief Writes the view bytes to @p stream (no trailing NUL is written).
 * @param sv The view to write.
 * @param stream The output stream.
 */
void sv_write(StringView sv, FILE *stream);

/**
 * @brief Copies the view into @p dst as a NUL-terminated string.
 *
 * Truncates silently when the view does not fit in @p dst_size bytes.
 * @param sv The view to copy.
 * @param dst The destination buffer.
 * @param dst_size Size of the destination buffer.
 * @note Requires: dst != NULL, or dst_size == 0.
 */
void sv_copy(StringView sv, uint8_t *dst, size_t dst_size);
