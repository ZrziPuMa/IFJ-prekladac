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
 * @brief Create a param groups object
 *
 * @param param_groups
 */
void create_param_groups(const ASTNode *param_groups);

/**
 * @brief Create a param list object
 *
 * @param items
 */
void create_param_list(const ASTNode *items);

#endif