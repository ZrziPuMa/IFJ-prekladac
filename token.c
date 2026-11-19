/**
 * Implementace překladače imperativního jazyka IFJ26
 * @file token.c
 * @brief Zdrojový soubor tokenů
 * @author Daniela Sladká, xsladkd00
 */

#include "token.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

void token_free(tToken *t) {
    if (t->val.str != NULL) {
        free(t->val.str);
    }
    free(t);
}

const char *token_type_name(tTokenType type) {
    switch (type) {
        case TOK_EOF:
            return "EOF";
        case TOK_NEWLINE:
            return "NEWLINE";
        case TOK_INDENT:
            return "INDENT";
        case TOK_DEDENT:
            return "DEDENT";
        case TOK_ID:
            return "ID";
        case TOK_INT:
            return "INT";
        case TOK_DOUBLE:
            return "DOUBLE";
        case TOK_BOOL:
            return "BOOL";
        case TOK_STRING:
            return "STRING";
        case TOK_TYPE_LIT:
            return "TYPE_LIT";
        case TOK_TYPE:
            return "TYPE";

        // Kličová slova
        case TOK_DEF:
            return "DEF";
        case TOK_VAL:
            return "VAL";
        case TOK_VAR:
            return "VAR";
        case TOK_CASE:
            return "CASE";
        case TOK_CLASS:
            return "CLASS";
        case TOK_EXTENSION:
            return "EXTENSION";
        case TOK_IMPORT:
            return "IMPORT";
        case TOK_IF:
            return "IF";
        case TOK_THEN:
            return "THEN";
        case TOK_ELSE:
            return "ELSE";
        case TOK_MATCH:
            return "MATCH";
        case TOK_WHILE:
            return "WHILE";
        case TOK_FOR:
            return "FOR";
        case TOK_DO:
            return "DO";
        case TOK_RETURN:
            return "RETURN";
        case TOK_LIST:
            return "LIST";
        case TOK_UNTIL:
            return "UNTIL";
        case TOK_TO:
            return "TO";
        case TOK_BY:
            return "BY";

        // Operace
        case TOK_PLUS:
            return "PLUS";
        case TOK_MINUS:
            return "MINUS";
        case TOK_MUL:
            return "MUL";
        case TOK_DIV:
            return "DIV";
        case TOK_NOT:
            return "NOT";
        case TOK_CONCAT:
            return "CONCAT";
        case TOK_LESSTHEN:
            return "LESSTHEN";
        case TOK_LESSEQ:
            return "LESSEQ";
        case TOK_GREATERTHEN:
            return "GREATERTHEN";
        case TOK_GREATEREQ:
            return "GREATEREQ";
        case TOK_EQ:
            return "EQ";
        case TOK_NOTEQ:
            return "NOTEQ";
        case TOK_AND:
            return "AND";
        case TOK_OR:
            return "OR";
        case TOK_ASSIGN:
            return "ASSIGN";
        case TOK_LEFT_ARROW:
            return "LEFT_ARROW";
        case TOK_RIGHT_ARROW:
            return "RIGHT_ARROW";
        case TOK_PIPE:
            return "PIPE";
        case TOK_LPAR:
            return "LPAR";
        case TOK_RPAR:
            return "RPAR";
        case TOK_LBRACKET:
            return "LBRACKET";
        case TOK_RBRACKET:
            return "RBRACKET";
        case TOK_COMMA:
            return "COMMA";
        case TOK_COLON:
            return "COLON";
        case TOK_SEMICOLON:
            return "SEMICOLON";
        case TOK_DOT:
            return "DOT";
        case TOK_AT_MAIN:
            return "AT_MAIN";
        case TOK_UNDERSCORE:
            return "UNDERSCORE";
        case TOK_INTERP_BEGIN:
            return "INTERP_BEGIN";
        case TOK_INTERP_END:
            return "INTERP_END";
        case TOK_INTERP_EXPR_BEGIN:
            return "INTERP_EXPR_BEGIN";
        case TOK_INTERP_EXPR_END:
            return "INTERP_EXPR_END";
        case TOK_INTERP_STRING_PART:
            return "INTERP_STRING_PART";
        case TOK_ERROR:
            return "ERROR";
        default:
            return "UNKNOWN";
    }
}

void token_dump(const tToken *t, FILE *out) {
    fprintf(out, "Token: %s, line: %d, value: ", token_type_name(t->type), t->line);
    switch (t->type) {
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
        case TOK_ID:
            fprintf(out, "%s", t->val.str);
            break;
        case TOK_TYPE:
            fprintf(out, "%s", t->val.str);
            break;
        case TOK_TYPE_LIT:
            fprintf(out, "type");
            break;
        case TOK_NEWLINE:
            fprintf(out, "newline");
            break;
        case TOK_INDENT:
            fprintf(out, "%lldx větší odsazení", t->val.i_val);
            break;
        case TOK_DEDENT:
            fprintf(out, "%lldx menší odsazení", t->val.i_val);
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
