/**
 * Implementace překladače imperativního jazyka IFJ26
 * @file token.c
 * @brief Zdrojový soubor tokenů
 * @author Daniela Sladká, xsladkd00
*/

#include "token.h"

#include <stdbool.h>
#include <stdio.h>

void token_free(tToken *t)
{
    if (t->val.str != NULL)
    {
        free(t->val.str);
    }
    free(t);
}

void token_dump(const tToken *t, FILE *out)
{
    fprintf(out, "Token: %s, line: %d, value: ", token_type_name(t->type), t->line);
    switch (t->type)
    {
    case TOK_INT:
        fprintf(out, "%lld", t->val.i_val);
        break;
    case TOK_DOUBLE:
        fprintf(out, "%lf", t->val.d_val);
        break;
    case TOK_STRING:
        fprintf(out, "\"%s\"", t->val.str);
        break;
    case TOK_BOOL:
        fprintf(out, "%s", t->val.i_val ? "true" : "false");
        break;
    case TOK_FUNCTION:
        fprintf(out, "%d", t->val.BuiltInFunc);
        break;
    case TOK_ID:
        fprintf(out, "%s", t->val.str);
        break;
    case TOK_TYPE:
        fprintf(out, "%s", t->val.str);
        break;
    case TOK_TYPE_LIT:
        fprintf(out, "type");
        break;
    case TOK_UNIT:
        fprintf(out, "unit");
        break;
    case TOK_NEWLINE:
        fprintf(out, "newline");
        break;
    case TOK_INDENT:
        fprintf(out, "%ix větší odsazení", t->val.i_val);
        break;
    case TOK_DEDENT:
        fprintf(out, "%ix menší odsazení", t->val.i_val);
        break;
    case TOK_EOF:
        fprintf(out, "EOF");
        break;
    case TOK_ERROR:
        fprintf(out, "ERROR line: %d", t->line);
        break;
    default:
        fprintf(out, "N/A");
        break;
    }
    fprintf(out, "\n");
}

