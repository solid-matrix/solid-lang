/**
 * @file parser.c
 * @brief Parser implementation.
 * @author solid-matrix
 * @version 0.0.5
 */

#include "parser.h"

SyntaxError syntax_error_create(SyntaxErrorCode code, SourceSpan span)
{
    return (SyntaxError){.code = code, .span = span};
}

Parser parser_create(Arena *arena)
{
    return (Parser){.errors = NULL, .arena = arena};
}

void parser_append_error(Parser *parser, SyntaxError error)
{
    SyntaxErrorList *en = arena_alloc(parser->arena, sizeof(SyntaxErrorList));
    en->error = error;
    en->next = parser->errors;
    parser->errors = en;
}

static SourceSpan skip_trivia(SourceSpan span)
{
    size_t i = 0;
    size_t len = span_len(span);

    while (i < len)
    {
        char c = span_get_char(span, i);

        if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
        {
            i += 1;
            continue;
        }

        if (c == '/' && i + 1 < len && span_get_char(span, i + 1) == '/')
        {
            i += 2;
            while (i < len && span_get_char(span, i) != '\n' && span_get_char(span, i) != '\r')
                i += 1;

            continue;
        }

        break;
    }

    return span_slice(span, i, len);
}