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

void parser_destroy(Parser *parser)
{
  SyntaxErrorList *en = parser->errors;
  while (en != NULL)
  {
    SyntaxErrorList *next = en->next;
    free(en);
    en = next;
  }
}

void parser_append_error(Parser *parser, Span span, SyntaxErrorCode code)
{
  SyntaxErrorList *en = malloc(sizeof(SyntaxErrorList));
  en->error = (SyntaxError){.code = code, .span = span};
  en->next = parser->errors;
  parser->errors = en;
}

static Span skip_trivia(const Source *source, Span span)
{
  size_t i = span.start;

  while (i < span.end)
  {
    char c = source_get_char(source, i);

    if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
    {
      i += 1;
      continue;
    }

    if (c == '/' && i + 1 < span.end && source_get_char(source, i + 1) == '/')
    {
      i += 2;
      while (i < span.end && source_get_char(source, i) != '\n' && source_get_char(source, i) != '\r')
        i += 1;

      continue;
    }

    break;
  }

  return (Span){.start = i, .end = span.end};
}

ParseResult parse_program(Parser *parser, Span span)
{
}

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
