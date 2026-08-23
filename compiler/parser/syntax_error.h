#pragma once

#include "span.h"

typedef enum {
  SYNTAX_OK = 0x0000,
  SYNTAX_EXPECTED_EOF = 0x0001,
  SYNTAX_MALFORMED_NUMBER = 0x0002,
} SyntaxErrorCode;

typedef struct {
  SyntaxErrorCode code;
  Span span;
} SyntaxError;

SyntaxError syntax_error_create(SyntaxErrorCode code, Span span);

typedef struct SyntaxErrorList SyntaxErrorList;
struct SyntaxErrorList {
  SyntaxError error;
  SyntaxErrorList *next;
};

SyntaxErrorList *syntax_errorlist_create();

void syntax_errorlist_append(SyntaxErrorList **list, SyntaxError error);

void syntax_errorlist_merge(SyntaxErrorList **dst, SyntaxErrorList **src);

void syntax_errorlist_free(SyntaxErrorList **list);