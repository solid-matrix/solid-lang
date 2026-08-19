#pragma once

#include "span.h"
#include "syntax_kind.h"

typedef struct
{
    SyntaxKind kind;
    Span span;
} SyntaxNode;