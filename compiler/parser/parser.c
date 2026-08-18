/**
 * @file parser.c
 * @brief Parser implementation.
 * @author solid-matrix
 * @version 0.0.5
 */

#include "parser.h"
#include <stdlib.h>

Parser parser_create(Source *source)
{
    return (Parser){.source = source, .errors = NULL};
}

void parser_append_error(Parser *parser, Span span, SyntaxErrorCode code)
{
    SyntaxErrorList *en = malloc(sizeof(SyntaxErrorList));
    en->error = (SyntaxError){.code = code, .span = span};
    en->next = parser->errors;
    parser->errors = en;
}

// static SourceSpan skip_trivia(SourceSpan span)
// {
//     size_t i = 0;
//     size_t len = span_len(span);

//     while (i < len)
//     {
//         char c = span_get_char(span, i);

//         if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
//         {
//             i += 1;
//             continue;
//         }

//         if (c == '/' && i + 1 < len && span_get_char(span, i + 1) == '/')
//         {
//             i += 2;
//             while (i < len && span_get_char(span, i) != '\n' && span_get_char(span, i) != '\r')
//                 i += 1;

//             continue;
//         }

//         break;
//     }

//     return span_slice(span, i, len);
// }

// const size_t a0 = sizeof(SyntaxNode);

// bool parse_program(Parser *parser, SourceSpan span, SourceSpan *rem, SyntaxProgram **program)
// {
//     SourceSpan r = span;
//     SyntaxProgram p = {
//         .kind = SYNTAX_KIND_PROGRAM,
//         .top_level_count = 0,
//         .top_levels = NULL,
//         .span = {.src = span.src}};

//     r = skip_trivia(r);

//     SyntaxDecl *decl;
//     while (parse_decl(parser, r, &r, &decl))
//     {
//         if (decl == NULL)
//             continue;

//         if (p.top_level_count == 0)
//         {
//             p.top_level_count++;
//             p.top_levels = arena_alloc(parser->arena, sizeof(SyntaxDecl *));
//         }
//         else
//         {
//             p.top_level_count++;
//         }
//     }

//     r = skip_trivia(r);

//     *rem = r;
//     *program = arena_alloc(parser->arena, sizeof(SyntaxProgram));
//     **program = p;
//     return true;
// }