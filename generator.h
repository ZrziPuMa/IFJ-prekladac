/**
 * Projekt: Implementace překladače imperativního jazyka IFJ26
 * @file generator.h
 * @brief
 * @author Martin Chlubna (<xchlubm00>)
 *
 */

#ifndef GENERATOR_H
#define GENERATOR_H

#include "igenerator.h"

// AI help
/*
 * Prompt: Napis mi enum, ktery vlozim do generator.h a ktery bude slouzit pro prirazeni
 * spravnych ciselnych hodnot pro jednotlive operatory, ktere pouzivame. ciselne hodnoty
 * operatoru musi byt uplne stejne jako v token.h
 */
typedef enum {
    OP_PLUS = 32,        // +
    OP_MINUS = 33,       // -
    OP_MUL = 34,         // *
    OP_DIV = 35,         // /
    OP_NOT = 36,         // !
    OP_CONCAT = 37,      // ++
    OP_LESSTHEN = 38,    // <
    OP_LESSEQ = 39,      // <=
    OP_GREATERTHEN = 40, // >
    OP_GREATEREQ = 41,   // >=
    OP_EQ = 42,          // ==
    OP_NOTEQ = 43,       // !=
    OP_AND = 44,         // &&
    OP_OR = 45           // ||
} OperatorToken;

/**
 * @brief Struktura pro uchovávání aktualního stavu generátoru
 *
 */
typedef struct {
    int l_cnt;             // Čítač návěští
    const char *func_name; // Název aktuální funkce se kterou se pracuje
    bool is_unit;
} GenState;

/**
 * @brief Create a prolog object
 *
 * @param main_def
 */
void create_prolog(const ASTNode *main_def);

/**
 * @brief Create a func def object
 *
 * @param func
 * @param stt
 */
void create_func_def(const ASTNode *func, GenState *stt);

/**
 * @brief Funkce pro prochazání všech param_groups a zavolání define_params() nad nimi
 *
 * @param param_groups
 */
void define_param_groups(const ASTNode *param_groups);

/**
 * @brief Připraví proměnné v LF pro argumenty funkce a vloží do nich hodnoty ze zásobníku
 *
 * @param items
 */
void define_params(const ASTNode *items);

/**
 * @brief Vypíše deklarace všech lokálních proměnných ve funkci (hoisting)
 *
 * @param command
 */
void declare_local_vars(const ASTNode *command);

/**
 * @brief Vyhodnotí zadaný výraz a nechá ho na vrcholu zásobníku
 *
 * @param expr
 */
void gen_expr(const ASTNode *expr, GenState *stt);

/**
 * @brief Pomocná funkce pro výpis instrukce PUSHS pro literál typu string
 *
 * @param string
 */
void string_lit_pushs(const char *string);

/**
 * @brief
 *
 * @param statement
 * @param stt
 */
void gen_block(const ASTNode *statement, GenState *stt, bool keep_result);

/**
 * @brief Vytvoří návěští pro všechny vestavěné funkce z IFJ26
 *
 */
void create_builtin_functions();

#endif