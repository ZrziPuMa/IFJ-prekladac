/**
 * Projekt: Implementace překladače imperativního jazyka IFJ26
 * @file igenerator.h
 * @brief
 * @author Martin Chlubna (<xchlubm00>)
 *
 */

#ifndef I_GENERATOR_H
#define I_GENERATOR_H

#include "ast.h"
#include "ierror.h"

ErrorCode generate_code(const ASTNode *root);

#endif