/**
 * Projekt: Implementace překladače imperativního jazyka IFJ26
 * @file generator.c
 * @brief Implementační soubor genrátoru cílového kódu
 * @author Martin Chlubna (<xchlubm00>)
 *
 */

#include "generator.h"

#include <stdbool.h>
#include <stdio.h>

void create_prolog(const ASTNode *main_def) {
    printf(".IFJcode26\n");
    printf("DEFVAR GF@%%tmp1\n");
    printf("DEFVAR GF@%%tmp2\n");
    printf("CALL $func_%s\n", main_def->data.func_def.name);
    printf("EXIT int@0\n");
    printf("\n");
}

void create_func_def(const ASTNode *func, GenState *stt) {
    (void)stt;
    printf("LABEL $func_%s\n", func->data.func_def.name);
    printf("CREATEFRAME\n");
    printf("PUSHFRAME\n");

    define_param_groups(func->data.func_def.param_groups);

    define_local_vars(func->data.func_def.body);

    // TODO: Doplnit tvorbu těla funkce

    printf("PUSHS nil@nil\n");
    printf("LABEL $func_%s$end\n", func->data.func_def.name);
    printf("POPFRAME\n");
    printf("RETURN\n");
    printf("\n");
}

void define_param_groups(const ASTNode *param_groups) {
    if (param_groups == NULL) {
        return;
    }
    define_param_groups(param_groups->next);

    define_params(param_groups->data.list.items);
}

void define_params(const ASTNode *items) {
    if (items == NULL) {
        return;
    }
    define_params(items->next);

    const char *identifier = items->data.var_def.identifier;
    const int var_id = items->data.var_def.var_id;

    printf("DEFVAR LF@%s$%d\n", identifier, var_id);
    printf("POPS LF@%s$%d\n", identifier, var_id);
}
// AI help
void define_local_vars(const ASTNode *command) {
    while (command != NULL) {

        switch (command->type) {
            // 1. Definice proměnných a vzorů
            case AST_VAR_DEF:
                if (command->data.var_def.var_id > 0) {
                    const char *identifier = command->data.var_def.identifier;
                    const int var_id = command->data.var_def.var_id;
                    printf("DEFVAR LF@%s$%d\n", identifier, var_id);
                }
                define_local_vars(command->data.var_def.initializer);
                break;

            case AST_PATTERN:
                if (command->data.pattern.pat_type == PATTERN_TYPE_BINDING) {
                    if (command->data.pattern.var_id > 0) {
                        const char *identifier = command->data.pattern.bound_identifier;
                        const int var_id = command->data.pattern.var_id;
                        printf("DEFVAR LF@%s$%d\n", identifier, var_id);
                    }
                } else if (command->data.pattern.pat_type == PATTERN_CONSTRUCTOR) {
                    define_local_vars(command->data.pattern.detail.constructor.args);
                }
                break;

            // 2. Bloky a řídicí konstrukce
            case AST_BLOCK:
                define_local_vars(command->data.block.statements);
                break;

            case AST_IF_EXPR:
                define_local_vars(command->data.if_expr.condition);
                define_local_vars(command->data.if_expr.then_block);
                define_local_vars(command->data.if_expr.else_block);
                break;

            case AST_WHILE:
                define_local_vars(command->data.while_loop.condition);
                define_local_vars(command->data.while_loop.body);
                break;

            case AST_FOR:
                define_local_vars(command->data.for_loop.iter_var);
                define_local_vars(command->data.for_loop.source);
                define_local_vars(command->data.for_loop.guards);
                define_local_vars(command->data.for_loop.body);
                break;

            case AST_MATCH_EXPR:
                define_local_vars(command->data.match_expr.matched_expr);
                define_local_vars(command->data.match_expr.cases);
                break;

            case AST_CASE_BRANCH:
                define_local_vars(command->data.case_branch.pattern);
                define_local_vars(command->data.case_branch.guard);
                define_local_vars(command->data.case_branch.body);
                break;

            // 3. Příkazy, výrazy a seznamy s poduzly
            case AST_RETURN:
                define_local_vars(command->data.return_stmt.value);
                break;

            case AST_ASSIGNMENT:
                define_local_vars(command->data.assignment.value);
                break;

            case AST_BINARY_OP:
                define_local_vars(command->data.bin_op.left);
                define_local_vars(command->data.bin_op.right);
                break;

            case AST_UNARY_OP:
                define_local_vars(command->data.unary_op.operand);
                break;

            case AST_FUNC_CALL:
                define_local_vars(command->data.func_call.target);
                define_local_vars(command->data.func_call.arg_groups);
                break;

            case AST_ARG_LIST:
            case AST_LIST_LITERAL:
                define_local_vars(command->data.list.items);
                break;

            case AST_MEMBER_ACCESS:
                define_local_vars(command->data.member_access.object);
                break;

            case AST_RANGE:
                define_local_vars(command->data.range.start);
                define_local_vars(command->data.range.end);
                define_local_vars(command->data.range.step);
                break;

            case AST_LAMBDA:
                // Lambda má při spuštění vlastní LF, dovnitř se neponořujeme
                break;

            default:
                break;
        }
        command = command->next;
    }
}

ErrorCode generate_code(const ASTNode *root) {
    if (root == NULL) {
        return print_error(ERR_INTERNAL, -1, "Generátor přijal neplatný kořen stromu.");
    }

    if (root->type != AST_PROGRAM) {
        return print_error(ERR_INTERNAL, -1, "Kořenový uzel AST je nesprávného typu.");
    }

    GenState stt = {.l_cnt = 0};

    ASTNode *curr_def = root->data.program.definitions;
    ASTNode *main_def = NULL;

    // Smyčka pro nalezení fce @main
    while (curr_def != NULL) {
        if (curr_def->type == AST_FUNC_DEF) {
            if (curr_def->data.func_def.is_main == true) {
                main_def = curr_def;
                break;
            }
        }
        curr_def = curr_def->next;
    }

    // Kontrola toho, zda fce @main byla v AST nalezena
    if (main_def == NULL) {
        return print_error(ERR_INTERNAL, -1, "Generátor nenalezl funkci main.");
    }

    // Vytvoření úvodního prologu, pomocných proměnných, zavolání @main
    create_prolog(main_def);

    // Vrácení ukazatele na aktuální definici zpět na začátek
    curr_def = root->data.program.definitions;

    // Procházení všech definic nejvyšší úrovně
    while (curr_def != NULL) {

        switch (curr_def->type) {

            case AST_FUNC_DEF:
                create_func_def(curr_def, &stt);
                break;

            // TODO: Dodělat zbylé definice
            default:
                break;
        }
        curr_def = curr_def->next;
    }

    return ERR_OK;
}
