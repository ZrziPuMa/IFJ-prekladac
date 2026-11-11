/**
 * Projekt: Implementace překladače imperativního jazyka IFJ26
 * @file error.c
 * @brief Implementace error modulu pro výpis chyb na stderr
 * @author Martin Chlubna (<xchlubm00>)
 *
 * Využití AI:
 * Při práci byli využity LLM modely z rodiny Gemini.
 * Použití:
 * 1. Vysvětlení syntaxe jazyka C
 * 2. Pomoc s vymyšlením struktury podmínek switch a if
 * 3. Generování chybových hlášek
 *
 */

#include "ierror.h"

#include <stdio.h>

ErrorCode print_error_impl(ErrInfo err) {
    const char *general_msg;
    const char *extra_msg = err.msg;
    ErrorCode err_code = err.code;

    // Výběr správné obecné hlášky na základě chybového kódu:

    switch (err_code) {
        case ERR_OK:
            // Pro případ, že by někdo zapomněl předat ErrorCode nebo by předal ERR_OK.
            general_msg = "Interní chyba překladače – neočekávané selhání vnitřního stavu!";
            extra_msg = "Error modul neobdržel chybový kód nebo byl zavolán s kódem ERR_OK.";
            err_code = ERR_INTERNAL;
            break;

        case ERR_LEX:
            general_msg = "Lexikální chyba – neplatná struktura lexému, komentáře nebo odsazení!";
            break;

        case ERR_PARSE:
            general_msg =
                "Syntaktická chyba – neplatná syntaxe, chybějící prolog nebo porušení bloků!";
            break;

        case ERR_SEM_UNDEF:
            general_msg =
                "Sémantická chyba – použití nedefinovaného prvku nebo chybějící funkce @main!";
            break;

        case ERR_SEM_REDEF:
            general_msg = "Sémantická chyba – nepovolená redefinice funkce, proměnné nebo typu!";
            break;

        case ERR_SEM_ARGS:
            general_msg =
                "Sémantická chyba – neplatný počet či typ argumentů nebo parametry funkce @main!";
            break;

        case ERR_SEM_TYPE:
            general_msg = "Sémantická chyba – typová nekompatibilita nebo neplatné odvození typu!";
            break;

        case ERR_SEM_UNAUTH_WRITE:
            general_msg =
                "Sémantická chyba – pokus o zápis do nemodifikovatelné proměnné nebo parametru!";
            break;

        case ERR_SEM_OTHER:
            general_msg = "Jiná sémantická chyba – neplatná konstrukce, nepovolený návrat nebo "
                          "nulový krok rozsahu!";
            break;

        case ERR_INTERNAL:
            general_msg = "Interní chyba překladače – neočekávané selhání vnitřního stavu!";
            break;

        default:
            general_msg = "Interní chyba překladače – neočekávané selhání vnitřního stavu!";
            extra_msg = "Error modul byl zavolán s neexistující chybou.";
            err_code = ERR_INTERNAL;
            break;
    }

    // Výpis výsledné kompletní hlášky na základě předaných hodnot:

    fprintf(stderr, "[Error %d] %s", err_code, general_msg); // Základní chyb. hláška

    if (err.line > 0) {
        fprintf(stderr, " (Řádek: %d)", err.line); // Číslo chybného řádku
    }

    if (extra_msg != NULL) {
        fprintf(stderr, ": %s", extra_msg); // Doplňující chyb. hláška
    }

    fprintf(stderr, "\n");

    return err_code;
}