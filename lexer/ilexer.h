/**
 * Implemetnace lexikálního analyzátoru pro jazyk IFJ26
 * @file ilexer.h
 * @brief Hlavičkový soubor lexikálního analyzátoru pro interface
 * @author Daniela Sladká, xsladkd00
 */

#ifndef ILEXER_H
#define ILEXER_H

#include "token.h"
// #include "ierror.h"

#include <stdio.h>

/**
 * @brief Datový typ pro prvek zásobníku
 */
typedef struct stack_elem {
    int count;
    struct stack_elem *next; // Ukazatel na další prvek zásobníku
} tLStackElem;

/**
 * @brief Datový typ pro zásobník lexeru
 */
typedef struct LStack {
    tLStackElem *top;
} tLStack;

/**
 * @brief Inicializace lexeru
 * @param file Zdrojový soubor s kódem
 */
void lexer_init(FILE *f);

/**
 * @brief Vytvoření a poslání následujícího tokenu
 */
tToken get_token();

/**
 * @brief Uvolnění paměti pro lexer
 */
void lexer_free();

#endif /*ILEXER_H*/