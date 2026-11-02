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
typedef enum
{
    TOK_EOF,                // konec vstupu
    TOK_NEWLINE,            // konec logického řádku (kromě těch uvnitř závorek a ${})
    TOK_INDENT,             // zvýšení odsazení
    TOK_DEDENT,             // snižení odsazení
    TOK_ID,                 // identifikátor
    TOK_INT,                // celočíselný literál
    TOK_DOUBLE,             // desetinný literál
    TOK_BOOL,               // boolean
    TOK_STRING,             // řetezec
    TOK_UNIT,               // jednotka (prázdný výraz)
    TOK_FUNCTION,           // klíčové slovo vestavěné funkce

    // Kličová slova
    TOK_DEF,
    TOK_VAL,
    TOK_VAR,
    TOK_TYPE,
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
    TOK_TRUE,
    TOK_FALSE,
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
    TOK_LEFT_ARROW,          // <-
    TOK_RIGHT_ARROW,        //  =>
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
    TOK_INTERP_EXPR_BEGIN,  //${
    TOK_INTERP_EXPR_END,    //}
    TOK_INTERP_STRING_PART, // prostý text
    TOKEN__COUNT            // počet tokenů (pro iterace cyklů)
} tTokenType;


/**
 *  @brief Typ pro vestavěné funkce 
 */
typedef enum {
    BF_print,
    BF_readLine,
    BF_readInt,
    BF_readIntMaybe,
    BF_readDouble,
    BF_readDoubleMaybe,
    BF_readBoolean,
    BF_toStr,
    BF_toInt,
    BF_strLength,
    BF_subStr,
    BF_ord,
    BF_chr,
} bBuiltinFuncs;

/**
 * @brief Datový typ pro hodnotu tokenu
 */
typedef struct {
    long long i_val;
    double d_val;
    char *str;
    bool isBuiltinFunc;
    bBuiltinFuncs BuiltInFunc;
} tTokenVal;

/**
 * @brief Datový typ pro token
 */
typedef struct
{
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


#endif // TOKEN_H