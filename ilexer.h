/**
 * Implemetnace lexikálního analyzátoru pro jazyk IFJ26
 * @file ilexer.h
 * @brief Hlavičkový soubor lexikálního analyzátoru pro interface
 * @author Daniela Sladká, xsladkd00
 */

#ifndef ILEXER_H
#define ILEXER_H

#include "token.h"

#include <stdio.h>

/**
 * @brief Inicializace lexeru
 * @param file Zdrojový soubor s kódem
 */
void lexer_init(FILE *f);

/**
 * @brief Vytvoření a poslání následujícího tokenu
 */
tToken give_token();

/**
 * @brief Uvolnění paměti pro lexer
 */
void lexer_free();

#endif /*ILEXER_H*/