/**
 * Implemetnace lexikálního analyzátoru pro jazyk IFJ26
 * @file ilexer.h
 * @brief Hlavičkový soubor lexikálního analyzátoru pro interface
 * @author Daniela Sladká, xsladkd00
 */

#ifndef ILEXER_H
#define ILEXER_H

#include "token.h"
// #include "ierror.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LENGTH 128

/**
 * @brief Výčet stavů FSM
 */
typedef enum {
    state_start,
    state_plus,
    state_minus,
    state_concatenation,
    state_multiply,
    state_pipe,
    state_or,
    state_ampersand,
    state_and,
    state_slash,
    state_com_line,
    state_com_block,
    state_com_end,
    state_underscore,
    state_ID,

    state_string_check,
    state_empty_str,
    state_chain_end,
    state_string_end,
    state_string,
    state_string_escape,

    state_interpolateds,
    state_interp_ID,

    state_Lpar,
    state_Rpar,
    state_Lbrack,
    state_Rbrack,

    state_colon,
    state_semicolon,
    state_dot,
    state_comma,

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
    state_at_main,
    state_not,
    state_not_equal,
    state_equal,
    state_is_equal,
    state_right_arrow,

    state_EOL,
    state_EOF,
} sState;

#endif /*ILEXER_H*/
