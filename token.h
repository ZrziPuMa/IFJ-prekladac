/**
 * Implementace překladače imperativního jazyka IFJ26
 * @file token.h
 * @brief Hlavičkový soubor tokenů
 * @author Daniela Sladká, xsladkd00
 * @author František Bilavčík, xbilavf00
 */

#ifndef TOKEN_H
#define TOKEN_H

#include <stdbool.h>
#include <stdio.h>

/**
 *  @brief Typ pro typy tokenů
 */
typedef enum {
    TOK_EMPTY,    // počáteční stav
    TOK_EOF,      // konec vstupu
    TOK_NEWLINE,  // konec logického řádku (kromě těch uvnitř závorek a ${})
    TOK_INDENT,   // zvýšení odsazení
    TOK_DEDENT,   // snižení odsazení
    TOK_ID,       // identifikátor
    TOK_INT,      // celočíselný literál
    TOK_DOUBLE,   // desetinný literál
    TOK_BOOL,     // boolean
    TOK_STRING,   // řetezec
    TOK_TYPE_LIT, // klíčové slovo typ
    TOK_TYPE,     // datový typ (int, double, bool, string, ...)

    // Kličová slova
    TOK_DEF,
    TOK_VAL,
    TOK_VAR,
    TOK_CASE,
    TOK_CLASS,
    TOK_EXTENSION,
    TOK_IMPORT,
    TOK_IF,
    TOK_THEN,
    TOK_ELSE,
    TOK_MATCH,
    TOK_WHILE,
    TOK_FOR,
    TOK_DO,
    TOK_RETURN,
    TOK_LIST,
    TOK_UNTIL,
    TOK_TO,
    TOK_BY,

    // Operace
    TOK_PLUS,               // +
    TOK_MINUS,              // -
    TOK_MUL,                // *
    TOK_DIV,                // /
    TOK_NOT,                // !
    TOK_CONCAT,             // ++
    TOK_LESSTHEN,           // <
    TOK_LESSEQ,             // <=
    TOK_GREATERTHEN,        // >
    TOK_GREATEREQ,          // >=
    TOK_EQ,                 // ==
    TOK_NOTEQ,              // !=
    TOK_AND,                // &&
    TOK_OR,                 // ||
    TOK_ASSIGN,             // =
    TOK_LEFT_ARROW,         // <-
    TOK_RIGHT_ARROW,        // =>
    TOK_PIPE,               // |
    TOK_LPAR,               // (
    TOK_RPAR,               // )
    TOK_LBRACKET,           // [
    TOK_RBRACKET,           // ]
    TOK_COMMA,              // ,
    TOK_COLON,              // :
    TOK_SEMICOLON,          // ;
    TOK_DOT,                // .
    TOK_AT_MAIN,            // @main
    TOK_UNDERSCORE,         // _
    TOK_INTERP_BEGIN,       // záčátek interpolace řetězce (s"...")
    TOK_INTERP_END,         // "
    TOK_INTERP_EXPR_BEGIN,  // ${
    TOK_INTERP_EXPR_END,    // }
    TOK_INTERP_STRING_PART, // prostý text
    TOKEN_COUNT,            // počet tokenů (pro iterace cyklů)
    TOK_ERROR               // chybový token
} tTokenType;

/**
 * @brief Datový typ pro hodnotu tokenu
 */
typedef struct {
    long long i_val;
    double d_val;
    char *str;
} tTokenVal;

/**
 * @brief Výčet stavů FSM
 */
typedef enum {
    state_start,
    state_plus,
    state_concatenation,
    state_pipe,
    state_or,
    state_ampersand,
    state_and,
    state_slash,
    state_com_line,
    state_com_block,
    state_com_block_2,
    state_com_end,
    state_underscore,
    state_ID,

    state_string_check,
    state_empty_str,

    state_chain_lit,
    state_cl1,
    state_cl2,
    state_chain_esc,
    state_string_end,
    state_string,
    state_string_esc,

    state_interpolateds,
    state_interp_ID,

    state_dot,
    state_int_unders,
    state_dec_unders,
    state_exp_unders,
    state_exp_sign,
    state_exp_end,
    state_exclam,

    state_zero,
    state_int,
    state_decimal,
    state_exponent,
    state_exp_d,

    state_greater_than,
    state_greater_equal,
    state_lesser_than,
    state_lesser_equal,
    state_left_arrow,

    state_at,
    state_main1,
    state_main2,
    state_main3,

    state_not,
    state_not_equal,
    state_equal,
    state_is_equal,
    state_right_arrow,

    state_eol
} sState;

/**
 * @brief Datový typ pro token
 */
typedef struct {
    sState state;
    tTokenType type;
    tTokenVal val;
    int line;
    bool newline_before;
} tToken;

/**
 * @brief Uvolní token
 * @param t Ukazatel na token
 */
void token_free(tToken *t);

/**
 * @brief Vrátí název tokenu jako řetězec
 * @param type Typ tokenu
 * @return Název tokenu jako řetězec ("ASSIGN", "ID", …)
 */
const char *token_type_name(tTokenType type);

/**
 * @brief Vypíše token na výstup pro účely ladění
 * @param t Ukazatel na token
 * @param out Ukazatel na výstupní soubor
 */
void token_dump(const tToken *t, FILE *out);

#endif /*TOKEN_H*/
