#include "syntax_error.h"

SyntaxError syntax_error_create(SyntaxErrorCode code, Span span) { return (SyntaxError){.code = code, .span = span}; }
