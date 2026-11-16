/**
 * Implemetnace lexikálního analyzátoru pro jazyk IFJ26
 * @file lexer.c
 * @brief Zdrojový soubor lexikálního analyzátoru
 * @author Daniela Sladká, xsladkd00
 */

#include "ilexer.h"
// #include "istack.h"
#include "token.h"

FILE *file;

void lexer_init(FILE *f) {
    file = f;
    // tStack *indent_stack;
    // stack_init(*indent_stack);
    // tStack *par_stack;
    // stack_init(*par_stack);
}
