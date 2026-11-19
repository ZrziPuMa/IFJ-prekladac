/**
 *
 * Projekt: Implementace překladače imperativního jazyka IFJ26
 *@file main.c
 *@brief Hlavní program překladače IFJ26 -> IFJcode26.
 *@author František Bilavčík (<xbilavf00>)
 */

#include "ast.h"
#include "ierror.h"
#include "igenerator.h"
#include "ilexer.h"
#include "iparser.h"
#include "isemantics.h"

#include <stdio.h>

int main(void) {
    ASTNode *root = NULL;

    lexer_init(stdin);
    ErrorCode err = parse_program(&root); // 0, 1, 2, 99 při chybě root == NULL, nic nezůstává
    lexer_free();
    if (err == ERR_OK)
        err = check_semantics(
            root); // 0, 3–7, 10, 99 strom při chybě zůstává celý, vytváří i maže symbtable
    if (err == ERR_OK)
        err = generate_code(root); // 0, 99 tiskne na stdout, ast jen čte
    ast_free(root);                // vždy
    return err;
}