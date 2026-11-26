/**
 * Projekt: Implementace překladače imperativního jazyka IFJ26
 * @file generator.h
 * @brief Interní hlavičkový soubor pro generátor kódu
 * @author Martin Chlubna (<xchlubm00>)
 *
 *
 * Využití AI: viz generator.c
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
    int l_cnt;              // Čítač návěští
    const char *func_name;  // Název aktuální funkce se kterou se pracuje
    bool is_unit;           // Příznak, zda má aktuální funkce návratový typ Unit
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
 * @brief Vypíše úvodní prolog + deklarace pom. proměnných, vstup do @main
 *
 * @param main_def Ukazatel na uzel s funkcí main
 */
void create_prolog(const ASTNode *main_def);

/**
 * @brief Vypíše definici předané funkce
 *
 * @param func Ukazatel na uzel AST_FUNC_DEF
 * @param receiver Ukazatel na příjemce metody (pro extension, jinak NULL)
 * @param stt Struktura pro aktualní stav generátoru
 */
void create_func_def(const ASTNode *func, const ASTNode *receiver, GenState *stt);

/**
 * @brief Funkce pro prochazání všech param_groups a zavolání define_params() nad nimi
 *
 * @param param_groups Ukazatel na seznam skupin parametrů funkce
 */
void define_param_groups(const ASTNode *param_groups);

/**
 * @brief Připraví proměnné v LF pro argumenty funkce a vloží do nich hodnoty ze zásobníku
 *
 * @param items Ukazatel na první parametr v rámci jedné skupiny parametrů
 */
void define_params(const ASTNode *items);

/**
 * @brief Vypíše deklarace všech lokálních proměnných ve funkci (hoisting)
 *
 * @param command Ukazatel na první uzel v těle funkce
 */
void declare_local_vars(const ASTNode *command);

/**
 * @brief Vyhodnotí zadaný výraz a nechá ho na vrcholu zásobníku
 *
 * @param expr Ukazatel na uzel výrazu v AST
 * @param stt Struktura pro aktuální stav generátoru
 */
void gen_expr(const ASTNode *expr, GenState *stt);

/**
 * @brief Pomocná funkce pro výpis instrukce PUSHS pro literál typu string
 *
 * @param string Vstupní řetězec k převedení do formátu IFJcode26
 */
void string_lit_pushs(const char *string);

/**
 * @brief Vygeneruje cílový kód pro posloupnost příkazů v bloku
 *
 * @param statement Ukazatel na první příkaz v bloku
 * @param stt Struktura pro aktuální stav generátoru
 * @param keep_result Určuje, zda má výsledek posledního výrazu zůstat na vrcholu zásobníku
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
 * @param receiver_type Typ příjemce metody (hodnoty nalevo od tečky)
 * @param method_name Název volané metody
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
 * @brief Zaregistruje uzel lambda funkce do dynamického pole a přidělí jí unikátní ID
 *
 * @param stt Struktura pro aktuální stav generátoru
 * @param lambda Ukazatel na uzel AST_LAMBDA
 * @return Přidělené ID lambda funkce, nebo -1 při chybě
 */
int register_lambda(GenState *stt, const ASTNode *lambda);

/**
 * @brief Vyhledá a vrátí přidělené ID pro zadaný uzel lambda funkce
 *
 * @param stt Struktura pro aktuální stav generátoru
 * @param lambda Ukazatel na uzel AST_LAMBDA
 * @return ID lambda funkce, nebo -1 pokud nebyla nalezena
 */
int get_lambda_id(const GenState *stt, const ASTNode *lambda);

/**
 * @brief Uvolní dynamické pole s ukazateli na lambda funkce
 *
 * @param stt Struktura pro aktuální stav generátoru
 */
void free_lambdas(GenState *stt);

/**
 * @brief Vypíše definici zadané lambda funkce v IFJcode26
 *
 * @param lambda Ukazatel na uzel AST_LAMBDA
 * @param stt Struktura pro aktuální stav generátoru
 */
void create_lambda_def(const ASTNode *lambda, GenState *stt);

/**
 * @brief Projde celý program, zaregistruje všechny lambda funkce a vygeneruje jejich těla
 *
 * @param node Ukazatel na první definici nejvyšší úrovně v AST
 * @param stt Struktura pro aktuální stav generátoru
 */
void create_all_lambdas(const ASTNode *node, GenState *stt);

#endif