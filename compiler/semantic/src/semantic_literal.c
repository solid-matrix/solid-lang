/**
 * @file semantic_literal.c
 * @brief Literal-token classification and decoding (see semantic_literal.h).
 * @author solid-matrix
 */

#include <string.h>

#include "semantic_literal.h"

static int hex_value(uint8_t c) {
  if (c >= '0' && c <= '9')
    return c - '0';
  if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;
  if (c >= 'A' && c <= 'F')
    return c - 'A' + 10;
  return -1;
}

static uint32_t utf8_encode(uint8_t *out, uint32_t cp) {
  if (cp < 0x80) {
    out[0] = (uint8_t)cp;
    return 1;
  }
  if (cp < 0x800) {
    out[0] = (uint8_t)(0xC0 | (cp >> 6));
    out[1] = (uint8_t)(0x80 | (cp & 0x3F));
    return 2;
  }
  if (cp < 0x10000) {
    out[0] = (uint8_t)(0xE0 | (cp >> 12));
    out[1] = (uint8_t)(0x80 | ((cp >> 6) & 0x3F));
    out[2] = (uint8_t)(0x80 | (cp & 0x3F));
    return 3;
  }
  out[0] = (uint8_t)(0xF0 | (cp >> 18));
  out[1] = (uint8_t)(0x80 | ((cp >> 12) & 0x3F));
  out[2] = (uint8_t)(0x80 | ((cp >> 6) & 0x3F));
  out[3] = (uint8_t)(0x80 | (cp & 0x3F));
  return 4;
}

static uint32_t utf8_decode(const uint8_t *in, size_t len, size_t *consumed) {
  uint8_t c = in[0];
  if (c < 0x80) {
    *consumed = 1;
    return c;
  }
  size_t n = c >= 0xF0 ? 4 : c >= 0xE0 ? 3 : 2;
  if (len < n) {
    *consumed = 1;
    return c;
  }
  uint32_t cp = (uint32_t)(c & (0xFF >> (n + 1)));
  for (size_t k = 1; k < n; k++)
    cp = (cp << 6) | (in[k] & 0x3F);
  *consumed = n;
  return cp;
}

static const struct {
  const char *name;
  size_t len;
  SemanticIntType type;
} SUFFIXES[] = {
    {"isize", 5, SEMANTIC_INT_ISIZE}, {"usize", 5, SEMANTIC_INT_USIZE}, {"i64", 3, SEMANTIC_INT_I64},
    {"u64", 3, SEMANTIC_INT_U64},     {"i32", 3, SEMANTIC_INT_I32},     {"u32", 3, SEMANTIC_INT_U32},
    {"i16", 3, SEMANTIC_INT_I16},     {"u16", 3, SEMANTIC_INT_U16},     {"i8", 2, SEMANTIC_INT_I8},
    {"u8", 2, SEMANTIC_INT_U8},       {"i", 1, SEMANTIC_INT_ISIZE},     {"u", 1, SEMANTIC_INT_USIZE},
};

SemanticIntLiteral semantic_int_literal(Strview text) {
  size_t i = 0;
  int base = 10;
  if (text.len >= 2 && text.data[0] == '0') {
    uint8_t c = (uint8_t)(text.data[1] | 0x20);
    if (c == 'b') {
      base = 2;
      i = 2;
    } else if (c == 'o') {
      base = 8;
      i = 2;
    } else if (c == 'x') {
      base = 16;
      i = 2;
    }
  }

  uint64_t mag = 0;
  int overflow = 0;
  while (i < text.len) {
    uint8_t c = text.data[i];
    if (c == '_') {
      i++;
      continue;
    }
    int d = hex_value(c);
    if (d < 0 || d >= base)
      break;
    if (mag > (UINT64_MAX - (uint64_t)d) / (uint64_t)base)
      overflow = 1;
    else
      mag = mag * (uint64_t)base + (uint64_t)d;
    i++;
  }

  Strview tail = {.data = text.data + i, .len = text.len - i};
  SemanticIntType type = SEMANTIC_INT_I32; // unsuffixed default (§4.4)
  bool suffixed = false;
  for (size_t k = 0; k < sizeof SUFFIXES / sizeof SUFFIXES[0]; k++) {
    if (tail.len == SUFFIXES[k].len && memcmp(tail.data, SUFFIXES[k].name, tail.len) == 0) {
      type = SUFFIXES[k].type;
      suffixed = true;
      break;
    }
  }

  SemanticIntLiteral r = {.status = SEMANTIC_LIT_OK, .type = type, .bits = mag, .suffixed = suffixed};
  uint32_t w = semantic_int_width(type);
  int is_signed = type <= SEMANTIC_INT_ISIZE;
  uint64_t max = is_signed ? ((1ull << (w - 1)) - 1) : (w == 64 ? UINT64_MAX : ((1ull << w) - 1));
  if (overflow || mag > max)
    r.status = SEMANTIC_LIT_RANGE;
  return r;
}

SemanticIntLiteral semantic_int_negate(SemanticIntLiteral literal) {
  uint32_t w = semantic_int_width(literal.type);
  int is_signed = literal.type <= SEMANTIC_INT_ISIZE;
  uint64_t mask = w == 64 ? UINT64_MAX : ((1ull << w) - 1);
  SemanticIntLiteral r = {.status = SEMANTIC_LIT_OK, .type = literal.type, .bits = (0 - literal.bits) & mask};
  if (is_signed && literal.bits > (1ull << (w - 1)))
    r.status = SEMANTIC_LIT_RANGE;
  return r;
}

uint32_t semantic_int_width(SemanticIntType type) {
  switch (type) {
  case SEMANTIC_INT_I8:
  case SEMANTIC_INT_U8:
    return 8;
  case SEMANTIC_INT_I16:
  case SEMANTIC_INT_U16:
    return 16;
  case SEMANTIC_INT_I32:
  case SEMANTIC_INT_U32:
    return 32;
  case SEMANTIC_INT_I64:
  case SEMANTIC_INT_U64:
  case SEMANTIC_INT_ISIZE:
  case SEMANTIC_INT_USIZE:
  default:
    return 64;
  }
}

static size_t decode_escape(const uint8_t *in, size_t len, uint8_t *out, size_t *produced) {
  uint8_t e = in[1];
  *produced = 1;
  switch (e) {
  case 'n':
    out[0] = '\n';
    return 2;
  case 'r':
    out[0] = '\r';
    return 2;
  case 't':
    out[0] = '\t';
    return 2;
  case '0':
    out[0] = '\0';
    return 2;
  case '\\':
    out[0] = '\\';
    return 2;
  case '\'':
    out[0] = '\'';
    return 2;
  case '"':
    out[0] = '"';
    return 2;
  case 'x': {
    if (len < 4)
      return 0;
    int hi = hex_value(in[2]);
    int lo = hex_value(in[3]);
    if (hi < 0 || lo < 0)
      return 0;
    out[0] = (uint8_t)(hi * 16 + lo);
    return 4;
  }
  case 'u': {
    if (len < 4 || in[2] != '{')
      return 0;
    size_t i = 3;
    uint32_t cp = 0;
    while (i < len && in[i] != '}') {
      if (in[i] == '_') {
        i++;
        continue;
      }
      int h = hex_value(in[i]);
      if (h < 0)
        return 0;
      cp = cp * 16 + (uint32_t)h;
      i++;
    }
    if (i >= len || cp > 0x10FFFF)
      return 0;
    *produced = utf8_encode(out, cp);
    return i + 1;
  }
  default:
    return 0;
  }
}

// The parser stores string literal content WITHOUT the quotes; escapes are
// still encoded and resolved here.
Strview semantic_string_content(Strview text, Arena *arena) {
  uint8_t *out = arena_alloc(arena, text.len > 0 ? text.len : 1);
  size_t o = 0;
  size_t i = 0;
  while (i < text.len) {
    uint8_t c = text.data[i];
    if (c != '\\') {
      out[o++] = c;
      i++;
      continue;
    }
    size_t consumed = 0;
    size_t produced = 0;
    consumed = decode_escape(text.data + i, text.len - i, out + o, &produced);
    if (consumed == 0)
      break; // malformed escape: unreachable post-parse
    i += consumed;
    o += produced;
  }
  return (Strview){.data = out, .len = o};
}

// Rune literal content WITHOUT the quotes (the parser strips them).
uint32_t semantic_rune_scalar(Strview text) {
  if (text.len == 0) return 0;
  if (text.data[0] == '\\') {
    uint8_t out[4];
    size_t consumed = 0;
    size_t produced = 0;
    consumed = decode_escape(text.data, text.len, out, &produced);
    if (consumed == 0 || produced == 0) return 0;
    size_t advance = 0;
    return utf8_decode(out, produced, &advance);
  }
  size_t advance = 0;
  return utf8_decode(text.data, text.len, &advance);
}
