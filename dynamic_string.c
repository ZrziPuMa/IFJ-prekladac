/**
 * Implementace tabulky symbolů jazyka IFJ26
 * @file dynamic_string.c
 * @brief Zdrojový soubor ADT dynamic string
 * @author Daniela Sladká, xsladkd00
 */

#include "idynamic_string.h"
#include "ierror.h"
#include "token.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

void check_keyword(tToken *token, sStr *str) {
    if (strcmp(str->string, "def") == 0) {
        token->type = TOK_DEF;
    } else if (strcmp(str->string, "do") == 0) {
        token->type = TOK_DO;
    } else if (strcmp(str->string, "else") == 0) {
        token->type = TOK_ELSE;
    } else if (strcmp(str->string, "if") == 0) {
        token->type = TOK_IF;
    } else if (strcmp(str->string, "import") == 0) {
        token->type = TOK_IMPORT;
    } else if (strcmp(str->string, "return") == 0) {
        token->type = TOK_RETURN;
    } else if (strcmp(str->string, "then") == 0) {
        token->type = TOK_THEN;
    } else if (strcmp(str->string, "val") == 0) {
        token->type = TOK_VAL;
    } else if (strcmp(str->string, "var") == 0) {
        token->type = TOK_VAR;
    } else if (strcmp(str->string, "case") == 0) {
        token->type = TOK_CASE;
    } else if (strcmp(str->string, "while") == 0) {
        token->type = TOK_WHILE;
    } else if (strcmp(str->string, "class") == 0) {
        token->type = TOK_CLASS;
    } else if (strcmp(str->string, "extension") == 0) {
        token->type = TOK_EXTENSION;
    } else if (strcmp(str->string, "for") == 0) {
        token->type = TOK_FOR;
    } else if (strcmp(str->string, "match") == 0) {
        token->type = TOK_MATCH;
    } else if (strcmp(str->string, "type") == 0) {
        token->type = TOK_TYPE_LIT;
    }
}

int check_var_type(tToken *token, sStr *str) {
    if (strcmp(str->string, "Int") == 0) {
        token->type = TOK_TYPE;
        token->val.str = "Int";
        return 1;
    } else if (strcmp(str->string, "Double") == 0) {
        token->type = TOK_TYPE;
        token->val.str = "Double";
        return 1;
    } else if (strcmp(str->string, "String") == 0) {
        token->type = TOK_TYPE;
        token->val.str = "String";
        return 1;
    } else if (strcmp(str->string, "Unit") == 0) {
        token->type = TOK_TYPE;
        token->val.str = "Unit";
        return 1;
    } else if (strcmp(str->string, "Boolean") == 0) {
        token->type = TOK_TYPE;
        token->val.str = "Boolean";
        return 1;
    } else if (strcmp(str->string, "List") == 0) {
        token->type = TOK_TYPE;
        token->val.str = "List";
        return 1;
    }
    return 0;
}

int string_init(sStr *str) {
    str->string = (char *)malloc(MAX_LENGTH);
    if (str->string == NULL) {
        print_error(ERR_LEX, -1, "Chyba: Selhání alokace.");
        return 1;
    } else {
        str->length = 0;
        str->max_length = MAX_LENGTH;
        str->string[0] = '\0';
        return 0;
    }
}

void string_free(sStr *str) {
    if (str == NULL || str->string == NULL) {
        return;
    }
    free(str->string);
    str->string = NULL;
    str->length = 0;
    str->max_length = 0;
}

int string_add(sStr *str, char c) {
    if ((str->length + 1) >= str->max_length) {
        char *temp = realloc(str->string, str->max_length + MAX_LENGTH);
        if (temp == NULL) {
            print_error(ERR_LEX, -1, "Chyba: Selhání realokace.");
            return 1;
        } else {
            str->string = temp;
            str->max_length += MAX_LENGTH;
        }
    }
    str->string[str->length] = c;
    str->length++;
    str->string[str->length] = '\0';
    return 0;
}
