/**
 * Implementace tabulky symbolů jazyka IFJ26
 * @file idynamic_string.h
 * @brief Hlavičkový soubor ADT dynamic string pro interface
 * @author Daniela Sladká, xsladkd00
 */

 
#ifndef IDYNAMIC_STRING_H
#define IDYNAMIC_STRING_H

#include <string.h>
#include <stdbool.h>
#include "token.h"

#define MAX_LENGTH 128


/**
 * @brief Struktura pro dynamic string
 */
typedef struct {
    char *string;
    int length;
    int max_length;
} sStr;


/**
 * @brief Kontrola, zda se string shoduje s klíčovým slovem a nastavení tokenu
 * @param token Ukazatel na token
 * @param string Ukazatel na string
 */
void check_keyword(tToken *token, sStr *string);

/**
 * @brief Kontrola, zda se string shoduje s druhem proměnné a nastavení tokenu
 * @param token Ukazatel na token
 * @param string Ukazatel na string
 * @return 1 když nalezena shoda
 */
int check_var_type(tToken *token, sStr *string);

/**
 * @brief Inicializace datové struktury
 * @param str Ukazatel na string
 * @return 1 když selže alokace
 */
int string_init(sStr *str);

/**
 * @brief Uvolnění alokované paměti pro string
 * @param str Ukazatel na string
 */
void string_free(sStr *str);

/**
 * @brief Přidání znaku na konec stringu
 * @param str Ukazatel na string
 * @param character Znak pro přidání
 * @return 1 když selže alokace
 */
int string_add(sStr *str, char character);

#endif //IDYNAMIC_STRING_H