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

    // AI help
    /*
     * Prompt: AI HELP: udelej mi dynamicke pole ukazatelu na uzly AST_LAMBDA
     */
    const ASTNode **lambdas; // LAMBDAS: Ukazatel na AST_LAMBDA
    int lambda_cnt;          // LAMBDAS: Počet alok. ukazatelů na lambdy
    int lambda_cap;          // LAMBDAS: Kolik místa je vyhrazeno pro ukazatele na lambdy
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

/**
 * @brief Vytvoří instanci datové třídy, např. Point(10, 20) (ALGEBDT)
 *
 * @param expr Uzel AST_FUNC_CALL s call_kind == CALL_CONSTRUCTOR
 * @param stt Stav generátoru
 */
void gen_constructor(const ASTNode *expr, GenState *stt);

/**
 * @brief Přečte atribut datové třídy, např. p.x (ALGEBDT)
 *
 * @param expr Uzel AST_MEMBER_ACCESS s member_kind == MEMBER_FIELD
 * @param stt Stav generátoru
 */
void gen_field_access(const ASTNode *expr, GenState *stt);

/**
 * @brief Vygeneruje výraz match; výsledek nechá na vrcholu zásobníku (ALGEBDT)
 *
 * @param expr Uzel AST_MATCH_EXPR
 * @param stt Stav generátoru
 */
void gen_match(const ASTNode *expr, GenState *stt);

/**
 * @brief
 *
 * @param stt
 * @param lambda
 * @return int
 */
int register_lambda(GenState *stt, const ASTNode *lambda);

/**
 * @brief Get the lambda id object
 *
 * @param stt
 * @param lambda
 * @return int
 */
int get_lambda_id(const GenState *stt, const ASTNode *lambda);

/**
 * @brief
 *
 * @param stt
 */
void free_lambdas(GenState *stt);

/**
 * @brief Create a lambda def object
 *
 * @param lambda
 * @param stt
 */
void create_lambda_def(const ASTNode *lambda, GenState *stt);

/**
 * @brief Create a all lambdas object
 *
 * @param node
 * @param stt
 */
void create_all_lambdas(const ASTNode *node, GenState *stt);

#endif