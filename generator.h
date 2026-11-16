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

/**
 * @brief Struktura pro uchovávání aktualního stavu generátoru
 *
 */
typedef struct {
    int l_cnt; // Čítač návěští
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
 * @brief Vypíše definice všech lokálních proměnných ve funkci (hoisting)
 *
 * @param command
 */
void define_local_vars(const ASTNode *command);

#endif