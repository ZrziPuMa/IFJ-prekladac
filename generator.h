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
    OP_PLUS = 31,        // +
    OP_MINUS = 32,       // -
    OP_MUL = 33,         // *
    OP_DIV = 34,         // /
    OP_NOT = 35,         // !
    OP_CONCAT = 36,      // ++
    OP_LESSTHEN = 37,    // <
    OP_LESSEQ = 38,      // <=
    OP_GREATERTHEN = 39, // >
    OP_GREATEREQ = 40,   // >=
    OP_EQ = 41,          // ==
    OP_NOTEQ = 42,       // !=
    OP_AND = 43,         // &&
    OP_OR = 44           // ||
} OperatorToken;

/**
 * @brief Struktura pro uchovávání aktualního stavu generátoru
 *
 */
typedef struct {
    int l_cnt;             // Čítač návěští
    const char *func_name; // Název aktuální funkce se kterou se pracuje
    bool is_unit;
    int stack_on_extra;     // Počet prvků na zásobníku, které se mají uklidit // AI help
    const ASTNode *program; // ALGEBDT: kořen programu, v něm se hledají definice tříd // AI help
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
void create_func_def(const ASTNode *func, const ASTNode *receiver, GenState *stt);

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

/**
 * @brief Vytvoří návěští pro metodu z extension (EXTKW)
 *
 * @param receiver_type
 * @param method_name
 */
void create_ext_func_label(const TypeInfo *receiver_type, const char *method_name);

#endif