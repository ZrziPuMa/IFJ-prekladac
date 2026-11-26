/**
 * Projekt: Implementace překladače imperativního jazyka IFJ26
 * @file parser.h
 * @brief Interní rozhraní pro syntaktický analyzátor (parser) (Top-down, rekurzivní sestup)
 * @author František Bilavčík (<xbilavf00>)
 *
 * (AI) Používal jsem Claude Opus 5.5 na funkce uvedené v souboru parser.c
 */

#ifndef PARSER_H
#define PARSER_H

#include "ast.h"
#include "ierror.h"
#include "ilexer.h"
#include "token.h"

#include <stdbool.h>

/*===========================
 * Vnitřní datový typ pro stav parseru
 * ===========================
 */
// (AI) Claude Opus 5.5 – prompt: „Vytvoř mi strukturu pro filtraci NEWLINE a DEDENT tak, aby
// korespondoval s naší dohodou o přijímání tokenů a byl v pořádku s naší implementací.“
/**
 * @brief Struktura pro filtraci tokenů. Stojí mezi lexerem a parserem.
 * Zajišťuje správný sled tokenů kolem NEWLINE, tak, aby korespondoval s LL gramatikou
 * Upraví proud z give_token(): NEWLINE za každým
 * DEDENT, bez NEWLINE před ELSE/THEN/DO, bez zdvojených NEWLINE, na konci souboru NEWLINE
 * a DEDENT NEWLINE za každou otevřenou úroveň, pak EOF i při každém dalším volání.
 * Tokeny v held a out už patří parseru, uvolní je free_parser_state.
 * Vynulovaný stav (init_parser_state) = začátek vstupu.
 */
typedef struct {
    tToken held;     ///< token přečtený z lexeru dopředu (pro nahlédnutí)
    bool has_held;   ///< true: held obsahuje platný token, jinak false
    tToken out[3];   ///< tokeny připravené pro parser (nejvýš NEWLINE DEDENT NEWLINE)
    int out_n;       ///< počet tokenů v out
    tTokenType last; ///< typ posledního tokenu přidaného do out, TOK_EMPTY = zatím žádný
    int depth;       ///< počet otevřených úrovní odsazení (INDENT − DEDENT)
} TokenFilter;

/**
 * @brief Reprezentuje stav parseru
 */
typedef struct {
    tToken current_token; ///< Aktualní token
    tToken ahead[2];      ///< Pole pro nutné nahlédnutí
    int ahead_count;      ///< Počet tokenů v ahead
    ErrorCode error_code; ///< první zaregistrovaná chyba, 0 = bez chyby
    TokenFilter filter;   ///< Filtr pro tokeny (pro správné zpracování odsazení a nových řádků)
} ParserState;

/*============================
 * Funkce pro práci s příchozím tokenem
 * ============================
 */

/**
 * @brief Inicializuje stav Parseru
 *
 * @param pstate Ukazatel na parser
 */
void init_parser_state(ParserState *pstate);

/**
 * @brief Uvolní stav parseru, uvolní řetězce tokenů
 *
 * @param pstate Ukazatel na parser
 */
void free_parser_state(ParserState *pstate);

/**
 * @brief Dá další token, prvně z ahead, případně volá filtr read_token(). Uvolní řetězec starého
 * current_token.val.str. Pokud dostane TOK_ERROR, nastaví pstate->error_code = val.i_val
 *
 * @param pstate Ukazatel na parser
 */
void next_token(ParserState *pstate);

/**
 * @brief Naplní ahead, na 2 tokeny. Volá ji jen parse_expr na LPAR.
 *
 * @param pstate Ukazatel na parser
 */
void fill_ahead(ParserState *pstate);

/**
 * @brief Očekává token typu type, pokud je jiný, nahlásí chybu 2. Pokud je správný, dá next_token()
 *
 * @param pstate Ukazatel na parser
 * @param type Očekávaný typ tokenu
 * @return true, pokud je token správný, jinak false
 */
bool expect_token(ParserState *pstate, tTokenType type);

/**
 * @brief Vezme pstate->current_token.val.str a nastaví ho na null
 *
 * @param pstate Ukazatel na parser
 * @return Vrací pstate->current_token.val.str
 */
char *take_string(ParserState *pstate);

/**
 * @brief Zkontroluje, zda je aktuální token identifikátor s textem text
 *
 * @param pstate Ukazatel na parser
 * @param text Text, který se má porovnat s aktuálním tokenem
 * @return true, pokud je aktuální token identifikátor s textem text, jinak false
 */
bool is_id_text(const ParserState *pstate, const char *text);

// ============================
// Funkce pro Filtraci tokenů (TokenFilter)
// ===========================

/**
 * @brief Přečte token z lexeru a aplikuje filtraci tokenů (TokenFilter)
 *
 * @param filter Ukazatel na filtr
 * @return Přečtený a filtrovaný token
 */
tToken read_token(TokenFilter *filter);

/* ===========================
 * Funkce pro hlášení chyb
 * ===========================
 */

/**
 * @brief Chyba 2: error_report a pstate->error_code = 2, jen když pstate->error_code == 0.
 * @param pstate Ukazatel na parser
 * @param msg Zpráva o chybě
 */
void syntax_error(ParserState *pstate, const char *msg);

/**
 * @brief Chyba 99: error_report a pstate->error_code = 99, jen když pstate->error_code == 0.
 * @param pstate Ukazatel na parser
 */
void out_of_memory(ParserState *pstate);

/* ===========================
 * Hlavní funkce parseru
 * ===========================
 */

/**
 * @brief Spustí se jako první, ověří, zda došlo k povinnému prologu. Prolog: IMPORT ID DOT MUL
 * NEWLINE, ID musí být "ifj26" ==> pravidlo 2 <prolog>
 *
 * @param pstate Ukazatel na parser
 * @note Výsledek je v pstate->error_code
 */
void parse_prolog(ParserState *pstate);

/**
 * @brief Pravidla 4.1 - 4.5 <decl>, <main_def> 5, <opt_new_line> 6.1/6.2 Přechází se pak do
 * FUNC_DEF / TYPE_ALIAS / EXTENSION_DEF / CLASS_DEF
 *
 * @param pstate Ukazatel na parser
 * @return Úspešně sestavný podstrom, při erroru vrací NULL, chyba je v pstate->error_code.
 */
ASTNode *parse_declaration(ParserState *pstate);

/**
 * @brief Pravidlo 7 <type_def> TYPE ID ASSIGN <type>
 *
 * @param pstate Ukazatel na parser
 * @return Úspešně sestavný podstrom, při erroru vrací NULL, chyba je v pstate->error_code.
 */
ASTNode *parse_type_def(ParserState *pstate);

/**
 * @brief Pravidlo 8 <extension>, 9.1/9.2 <extension_body>, 10 <methd_list>, 11.1/11.2 <method_rest>
 *
 * @param pstate Ukazatel na parser
 * @return Úspešně sestavný podstrom, při erroru vrací NULL, chyba je v pstate->error_code.
 */
ASTNode *parse_extension(ParserState *pstate);

/**
 * @brief Pravidlo 12 DEF ID <param_list> <ret_type> ASSIGN <body> -> FUNC_DEF, tělo bude AST_BLOCK
 *
 * @param pstate Ukazatel na parser
 * @param is_main true, pokud se jedná o funkci main, jinak false
 * @return Úspešně sestavný podstrom, při erroru vrací NULL, chyba je v pstate->error_code.
 */
ASTNode *parse_func_def(ParserState *pstate, bool is_main);

/**
 * @brief Pravidlo 13 <case_class>: CASE CLASS ID LPAR <params> RPAR, vede na CLASS_DEF
 *
 * @param pstate Ukazatel na parser
 * @return Úspešně sestavný podstrom, při erroru vrací NULL, chyba je v pstate->error_code.
 */
ASTNode *parse_case_class(ParserState *pstate);

/**
 * @brief pravidlo 14.1/14.2 <param_list>. Vytvoří řetězec PARAM_LIST přes ukazatele next. NULL
 * značí bez závorek, například (def do)
 *
 * @param pstate Ukazatel na parser
 * @return Úspešně sestavný podstrom, nebo NULL pokud je funkce bez závorek.
 * Při erroru také NULL, o chybě rozhoduje pstate->error_code.
 */
ASTNode *parse_param_list(ParserState *pstate);

/**
 * @brief pravidlo 15.1/15.2 <params>, 16.1/16.2 <param_next>,17 <param>.
 *
 * @param pstate Ukazatel na parser
 * @return Úspešně sestavný podstrom, nebo NULL pokud je prázdný ().
 * Při erroru také NULL, o chybě rozhoduje pstate->error_code.
 */
ASTNode *parse_params(ParserState *pstate);

/**
 * @brief  pravidlo 18.1/18.2 <ret_type>, napsaný typ, pokud není napsán tak = NULL
 *
 * @param pstate Ukazatel na parser
 * @return Úspešně sestavný podstrom typu TypeInfo, nebo NULL pokud není napsaný typ.
 * Při erroru také NULL, o chybě rozhoduje pstate->error_code.
 */
TypeInfo *parse_ret_type(ParserState *pstate);

/**
 * @brief pravidlo 19.1/19.2 <type_list>, 20.1/20.2 <type_next>. Vytvoří posloupnost typů přes
 * ukazatele next. NULL značí prázdný seznam.
 *
 * @param pstate Ukazatel na parser
 * @return Úspešně sestavný podstrom typu TypeInfo, nebo NULL u ().
 * Při erroru také NULL, o chybě rozhoduje pstate->error_code.
 */
TypeInfo *parse_type_list(ParserState *pstate);

/**
 * @brief 21.1/21.2 <type>, 22.1/22.2 <type_rest>.
 *
 * @param pstate Ukazatel na parser
 * @return Úspešně sestavný podstrom typu TypeInfo, při erroru vrací NULL, chyba je v
 * pstate->error_code.
 */
TypeInfo *parse_type(ParserState *pstate);

/**
 * @brief 23.1-23.7 <simple_type>.
 *
 * @param pstate Ukazatel na parser
 * @return Úspešně sestavný podstrom typu TypeInfo, při erroru vrací NULL, chyba je v
 * pstate->error_code.
 */
TypeInfo *parse_simple_type(ParserState *pstate);

/**
 * @brief 24.1/24.2 <union_rest>. převezme left a vytvoří union
 *
 * @param pstate Ukazatel na parser
 * @param left Levý operand
 * @return Úspešně sestavný podstrom typu TypeInfo, při epsylon vrací left beze změny, u erroru
 * uvolní left a vrací NULL, chyba je v pstate->error_code.
 */
TypeInfo *parse_union_rest(ParserState *pstate, TypeInfo *left);

/**
 * @brief 25.1/25.2 <union_member>.
 *
 * @param pstate Ukazatel na parser
 * @return Úspešně sestavný podstrom typu TypeInfo, při erroru vrací NULL, chyba je v
 * pstate->error_code.
 */
TypeInfo *parse_union_member(ParserState *pstate);

/**
 * @brief 26.1/26.2 <body>. 26.1 vrací výraz, 26.2 vrací blok.
 *
 * @param pstate Ukazatel na parser
 * @return Úspešně sestavný podstrom, při erroru vrací NULL, pstate->error_code
 */
ASTNode *parse_body(ParserState *pstate);

/**
 * @brief 27 <block>, 28 <statement_list>, 29.1/29.2 <statement_rest>, 30.1/30.2 <separate>.
 *
 * @param pstate Ukazatel na parser
 * @return Úspešně sestavný podstrom, při erroru vrací NULL, pstate->error_code
 */
ASTNode *parse_block(ParserState *pstate);

/**
 * @brief 31.1-31.5 <statement. Kontrola před zpracováním, že řádek nezačíná PLUS, MINUS nebo NOT
 *
 * @param pstate Ukazatel na parser
 * @return Úspešně sestavný podstrom, při erroru vrací NULL, pstate->error_code
 */
ASTNode *parse_statement(ParserState *pstate);

/**
 * @brief 32 <val> 33 <var>
 *
 * @param pstate Ukazatel na parser
 * @param is_mutable true = var, false = val
 * @return Úspešně sestavný podstrom, při erroru vrací NULL, pstate->error_code
 */
ASTNode *parse_val_var_def(ParserState *pstate, bool is_mutable);

/**
 * @brief 35 <while>: WHILE <op_expr DO <branch>
 *
 * @param pstate Ukazatel na parser
 * @return Úspešně sestavný podstrom, při erroru vrací NULL, pstate->error_code
 */
ASTNode *parse_while(ParserState *pstate);

/**
 * @brief 36 <return>, 37.1/37.2 <return_val>, value = NULL pokud je samotné return.
 *
 * @param pstate Ukazatel na parser
 * @return Úspešně sestavný podstrom, při erroru vrací NULL, pstate->error_code
 */
ASTNode *parse_return(ParserState *pstate);

/**
 * @brief 38 <if>, 39.1/39.2 <else_part>. Kontflikt, pokud přijde ELSE, vždy se použije 39.1.
 * else_block = NULL pokud je bez else.
 *
 * @param pstate Ukazatel na parser
 * @return Úspešně sestavný podstrom, při erroru vrací NULL, pstate->error_code
 */
ASTNode *parse_if(ParserState *pstate);

/**
 * @brief 40.1/40.2 <branch>. Vždy do AST_BLOCK, i jeden příkaz.
 *
 * @param pstate Ukazatel na parser
 * @return Úspešně sestavný podstrom, při erroru vrací NULL, pstate->error_code
 */
ASTNode *parse_branch(ParserState *pstate);

/**
 * @brief 41.1/41.2 <line_expr>, 34.1/34.2 <assign_rest>. Konflikt, pokud přijde ASSIGN, vždy se
 * použije 34.1. Nalevo od ASSIGN může být jen ID, jinak chyba 2 (chyba syntaxe).
 *
 * @param pstate Ukazatel na parser
 * @return Úspešně sestavný podstrom, při erroru vrací NULL, pstate->error_code
 */
ASTNode *parse_line_expr(ParserState *pstate);

/**
 * @brief 42.1-42.3 <expr> (U LPAR vyjímka, výhled na 2 tokeny, chvilkové porušení čísté LL1
 * gram.), 60.1/60.2 <match_part>.
 *
 * @param pstate Ukazatel na parser
 * @return Úspešně sestavný podstrom, při erroru vrací NULL, pstate->error_code
 */
ASTNode *parse_expr(ParserState *pstate);

/**
 * @brief 43 <lambda_def>. Vrací LAMBDA s jedním PARAM_LIST a AST_BLOCK
 *
 * @param pstate Ukazatel na parser
 * @return Úspešně sestavný podstrom, při erroru vrací NULL, pstate->error_code
 */
ASTNode *parse_lambda(ParserState *pstate);

/**
 * @brief 47 <for>, 48.1/48.2 <dor_condition>, 44 <generator>, 49.1/49.2 <guards>, 50.1/50.2
 * <guard_next>.
 *
 * @param pstate Ukazatel na parser
 * @return Úspešně sestavný podstrom, při erroru vrací NULL, chyba je v pstate->error_code.
 */
ASTNode *parse_for(ParserState *pstate);

/**
 * @brief  45.1 - 45.3 <generator>_opt>, 46.1/46.2 <by>.
 *
 * @param pstate Ukazatel na parser
 * @param start_expr Počáteční výraz, přebírá vlastnictvé. 45.1/45.2 vznikne range.start. při 45.3
 * ho vrátí beze změny. Při chybě ho funkce uvolní.
 * @return Úspešně sestavný podstrom, při erroru je chyba v pstate->error_code a vrací NULL.
 */
ASTNode *parse_generator_opt(ParserState *pstate, ASTNode *start_expr);

/**
 * @brief 51 <term>, 52.1-52.3 <term_rest>
 *
 * @param pstate Ukazatel na parser
 * @return Úspešně sestavný podstrom, při erroru vrací NULL, chyba je v pstate->error_code.
 */
ASTNode *parse_term(ParserState *pstate);

/**
 * @brief 53.1-53.9 <element>, 59.1/59.2 <list_rest>
 *
 * @param pstate Ukazatel na parser
 * @return Úspešně sestavný podstrom, při erroru vrací NULL, chyba je v pstate->error_code.
 */
ASTNode *parse_element(ParserState *pstate);

/**
 * @brief 54.1/54.2 <arguments>, 55.1/55.2 <arguments_next>. items NULL = ()
 *
 * @param pstate Ukazatel na parser
 * @return Úspešně sestavný podstrom, při erroru vrací NULL, chyba je v pstate->error_code.
 */
ASTNode *parse_arguments(ParserState *pstate);

/**
 * @brief 56 <interpol>, 57.1-57.3 <interpol_section>. Přepis na string
 *
 * @param pstate Ukazatel na parser
 * @return Úspešně sestavný podstrom, při erroru vrací NULL, chyba je v pstate->error_code.
 */
ASTNode *parse_interpol(ParserState *pstate);

/**
 * @brief 58.1/58.2 <after_parenth>. Po volá se po LPAR použitou parse_element,
 *
 * @param pstate Ukazatel na parser
 * @return UNIT při RPAR, výraz v závorce nebo AST_RANGE, při erroru vrací NULL, chyba je v
 * pstate->error_code.
 */
ASTNode *parse_after_parenth(ParserState *pstate);

/**
 * @brief pravidlo 60.1 <match_part>, 61 <case_list>, 62.1/62.2 <case_rest>. převezme matched_expr.
 *
 * @param pstate Ukazatel na parser
 * @param matched_expr Výraz, který se má porovnávat
 * @return Úspešně sestavný podstrom, při erroru vrací NULL, chyba je v pstate->error_code.
 */
ASTNode *parse_match(ParserState *pstate, ASTNode *matched_expr);

/**
 * @brief 63 <case_branch>, 64.1-64.2 <case_guard>
 *
 * @param pstate Ukazatel na parser
 * @return Úspešně sestavný podstrom, při erroru vrací NULL, chyba je v pstate->error_code.
 */
ASTNode *parse_case_branch(ParserState *pstate);

/**
 * @brief 65.1/65.2 <pattern>, 67.1/67.2 <id_pattern_rest>, 73 <pattern_type>
 *
 * @param pstate Ukazatel na parser
 * @return Úspešně sestavný podstrom, při erroru vrací NULL, chyba je v pstate->error_code.
 */
ASTNode *parse_pattern(ParserState *pstate);

/**
 * @brief 66.1 - 66.3 <simple_pattern>
 *
 * @param pstate Ukazatel na parser
 * @return Úspešně sestavný podstrom, při erroru vrací NULL, chyba je v pstate->error_code.
 */
ASTNode *parse_simple_pattern(ParserState *pstate);

/**
 * @brief 68.1/68.2 <sub_patterns>, 69.1/69.2 <sub_pattern>, 70.1/70.2 <sub_pattern_next>
 *
 * @param pstate Ukazatel na parser
 * @return Úspešně sestavný podstrom, nebo NULL u Point().
 * Při erroru také NULL, o chybě rozhoduje pstate->error_code.
 */
ASTNode *parse_sub_patterns(ParserState *pstate);

/**
 * @brief 71.1 - 71.4 <literal>, 72.1/72.2 <num_lit>, ==> AST_LITERAL
 *
 * @param pstate Ukazatel na parser
 * @return Úspešně sestavný podstrom, při erroru vrací NULL, chyba je v pstate->error_code.
 */
ASTNode *parse_literal(ParserState *pstate);

/* ==========================
 * Funkce pro precedenční SA
 * ==========================
 */

/**
 * @brief <op_expr>: Precedenční SA, Volají pravidla 35, 38, 42.3, 44, 45.1, 45.2, 46.1, 49.1, 50.1
 * a 64.1. Operandy i dělá parse_term. Končí na prvním tokenu, kter není BINARY_OP, UNARY_OP,
 * operand
 *
 * @param pstate Ukazatel na parser
 * @return Úspešně sestavný podstrom, při erroru vrací NULL, chyba je v pstate->error_code.
 */
ASTNode *parse_op_expr(ParserState *pstate);

#endif // PARSER_H
