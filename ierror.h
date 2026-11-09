/**
 * Projekt: Implementace překladače imperativního jazyka IFJ26
 * @file ierror.h
 * @brief Error modul pro výpis chyby na stderr
 * @author Martin Chlubna (<xchlubm00>)
 */

#ifndef I_ERROR_H
#define I_ERROR_H

/**
 * @brief Seznam chybových kódů překladače
 *
 */

typedef enum {
    ERR_OK = 0,
    ERR_LEX = 1,
    ERR_PARSE = 2,
    ERR_SEM_UNDEF = 3,
    ERR_SEM_REDEF = 4,
    ERR_SEM_ARGS = 5,
    ERR_SEM_TYPE = 6,
    ERR_SEM_UNAUTH_WRITE = 7,
    ERR_SEM_OTHER = 10,
    ERR_INTERNAL = 99,
} ErrorCode;

/**
 * @brief Makro pro přepis předávaných parametrů do struktury ErrInfo
 *
 * V kódu můžete funkci volat těmito způsoby:
 *
 * 1. print_error(ERR_SEM_UNAUTH_WRITE, line_number, "Částice z vesmíru otočila bity");
 *    V tomto případě, pokud se má nějaký parametr vynechat,
 *    napíše se např: print_error(ERR_INTERNAL)
 *    nebo print_error(ERR_INTERNAL, -1, "Překladač je v troskách :( ");
 *
 * 2. print_error(.code=ERR_LEX, .line=789, .msg="Nepovolený font na vstupu: Comic sans");
 *      Zde nezaleží na pořadí.
 *
 * Pravidla:
 *      1. Vždy předat alespoň ErrorCode
 *      2. Pokud je ErrorCode jiný než ERR_INTERNAL
 *      a nechcete předávat int line, tak int line nastavit na -1
 *
 */

#define print_error(...) print_error_impl((ErrInfo){__VA_ARGS__})

/**
 * @brief Struktura s informacemi pro vypsání chyby na stderr
 *
 */

typedef struct {
    ErrorCode code;  // Typ chyby
    int line;        // Číslo řádku chyby
    const char *msg; // Volitelná hláška k vypsání
} ErrInfo;

/**
 * @brief Interní funkce error modulu pro výpis na stderr
 *
 * @param err Struktura typu ErrInfo (viz výše)
 * @return ErrorCode Předání stejného chyb. kódu zpět pro předání do main()
 */

ErrorCode print_error_impl(ErrInfo err);

#endif
