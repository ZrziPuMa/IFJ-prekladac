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
#include <string.h>

void create_prolog(const ASTNode *main_def) {
    printf(".IFJcode26\n");
    printf("DEFVAR GF@%%tmp1\n");
    printf("DEFVAR GF@%%tmp2\n");
    printf("DEFVAR GF@%%tmp3\n");
    printf("CALL $func_%s\n", main_def->data.func_def.name);
    printf("EXIT int@0\n");
    printf("\n");
}

void create_func_def(const ASTNode *func, GenState *stt) {
    printf("LABEL $func_%s\n", func->data.func_def.name);
    printf("CREATEFRAME\n");
    printf("PUSHFRAME\n");

    define_param_groups(func->data.func_def.param_groups);

    declare_local_vars(func->data.func_def.body);

    // AI help (dovysvetlit)
    const TypeInfo *fn_type = func->expr_type;
    stt->func_name = func->data.func_def.name;
    stt->is_unit = (fn_type != NULL && fn_type->type_class == TYPE_FUNCTION &&
                    fn_type->data.func_type.return_type != NULL &&
                    fn_type->data.func_type.return_type->type_class == TYPE_UNIT);

    gen_block(func->data.func_def.body->data.block.statements, stt, !stt->is_unit);
    if (stt->is_unit) {
        printf("PUSHS nil@nil\n");
    }

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
void declare_local_vars(const ASTNode *command) {
    while (command != NULL) {

        switch (command->type) {
            // 1. Definice proměnných a vzorů
            case AST_VAR_DEF:
                if (command->data.var_def.var_id > 0) {
                    const char *identifier = command->data.var_def.identifier;
                    const int var_id = command->data.var_def.var_id;
                    printf("DEFVAR LF@%s$%d\n", identifier, var_id);
                }
                declare_local_vars(command->data.var_def.initializer);
                break;

            case AST_PATTERN:
                if (command->data.pattern.pat_type == PATTERN_TYPE_BINDING) {
                    if (command->data.pattern.var_id > 0) {
                        const char *identifier = command->data.pattern.bound_identifier;
                        const int var_id = command->data.pattern.var_id;
                        printf("DEFVAR LF@%s$%d\n", identifier, var_id);
                    }
                } else if (command->data.pattern.pat_type == PATTERN_CONSTRUCTOR) {
                    declare_local_vars(command->data.pattern.detail.constructor.args);
                }
                break;

            // 2. Bloky a řídicí konstrukce
            case AST_BLOCK:
                declare_local_vars(command->data.block.statements);
                break;

            case AST_IF_EXPR:
                declare_local_vars(command->data.if_expr.condition);
                declare_local_vars(command->data.if_expr.then_block);
                declare_local_vars(command->data.if_expr.else_block);
                break;

            case AST_WHILE:
                declare_local_vars(command->data.while_loop.condition);
                declare_local_vars(command->data.while_loop.body);
                break;

            case AST_FOR:
                declare_local_vars(command->data.for_loop.iter_var);
                declare_local_vars(command->data.for_loop.source);
                declare_local_vars(command->data.for_loop.guards);
                declare_local_vars(command->data.for_loop.body);
                break;

            case AST_MATCH_EXPR:
                declare_local_vars(command->data.match_expr.matched_expr);
                declare_local_vars(command->data.match_expr.cases);
                break;

            case AST_CASE_BRANCH:
                declare_local_vars(command->data.case_branch.pattern);
                declare_local_vars(command->data.case_branch.guard);
                declare_local_vars(command->data.case_branch.body);
                break;

            // 3. Příkazy, výrazy a seznamy s poduzly
            case AST_RETURN:
                declare_local_vars(command->data.return_stmt.value);
                break;

            case AST_ASSIGNMENT:
                declare_local_vars(command->data.assignment.value);
                break;

            case AST_BINARY_OP:
                declare_local_vars(command->data.bin_op.left);
                declare_local_vars(command->data.bin_op.right);
                break;

            case AST_UNARY_OP:
                declare_local_vars(command->data.unary_op.operand);
                break;

            case AST_FUNC_CALL:
                declare_local_vars(command->data.func_call.target);
                declare_local_vars(command->data.func_call.arg_groups);
                break;

            case AST_ARG_LIST:
            case AST_LIST_LITERAL:
                declare_local_vars(command->data.list.items);
                break;

            case AST_MEMBER_ACCESS:
                declare_local_vars(command->data.member_access.object);
                break;

            case AST_RANGE:
                declare_local_vars(command->data.range.start);
                declare_local_vars(command->data.range.end);
                declare_local_vars(command->data.range.step);
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

// AI help
/* Prompt:  Napiš mi pomocnou funkci: void string_lit_pushs(const char *string) {} tato funkce
 * vypise instrukci pushs pro literal typu string tak aby to splnovalo, ze tam mohou byt vsechny
 * znaky ascii, ale pro hodnoty ascii 000-032, 035 a 092 musi byt znak nahrazen escape sekvenci ve
 * tvaru \xyz.
 */
void string_lit_pushs(const char *string) {
    printf("PUSHS string@");

    if (string != NULL) {
        for (size_t i = 0; string[i] != '\0'; i++) {
            unsigned char c = (unsigned char)string[i];
            if (c <= 32 || c == 35 || c == 92) {
                printf("\\%03d", c);
            } else {
                putchar(c);
            }
        }
    }

    putchar('\n');
}

// AI help
/* Prompt: Vytvoř mi strukturu switche ve funkci gen_expr.
 * samotnou logiku funkce si pote doplnim sam
 */
void gen_expr(const ASTNode *expr) {

    if (expr == NULL) {
        return;
    }

    switch (expr->type) {
        // AI help
        /* Prompt: Napiš mi strukturu switche ktery vlozim do case AST_LITERAL a bude vybirat co se
         * pushne na zasobnik podle Literalkind. Ty printf si tam zatim napisu sam
         */

        // 1. Základní zadání (termy, operátory a volání funkcí)
        case AST_LITERAL:
            switch (expr->data.literal.kind) {
                case LIT_INT:
                    printf("PUSHS int@%lld\n", expr->data.literal.value.i_val);
                    break;

                case LIT_DOUBLE:
                    printf("PUSHS float@%a\n", expr->data.literal.value.d_val);
                    break;

                case LIT_STRING:
                    string_lit_pushs(expr->data.literal.value.s_val);
                    break;

                case LIT_BOOL:
                    printf("PUSHS bool@%s\n", expr->data.literal.value.b_val ? "true" : "false");
                    break;

                case LIT_UNIT:
                    printf("PUSHS nil@nil\n");
                    break;

                default:
                    break;
            }
            break;

        case AST_IDENTIFIER:
            printf("PUSHS LF@%s$%d\n", expr->data.identifier.name, expr->data.identifier.var_id);
            break;

            // AI help
            /* Prompt: switch (expr->data.unary_op.operator_token) {
             * } dopis mi tento switch, kde pokud to bude +, tak se nestane nic, pokud minus, tak
             * udelame ten odecet od nuly na zasobniku, a pokud tam bude negace, tak udelame
             * instrukci NOTS nad tim co je na zasobniku
             */
        case AST_UNARY_OP:
            switch (expr->data.unary_op.operator_token) {
                case OP_PLUS:
                    gen_expr(expr->data.unary_op.operand);
                    break;

                case OP_MINUS:
                    if (expr->expr_type != NULL && expr->expr_type->type_class == TYPE_DOUBLE) {
                        printf("PUSHS float@%a\n", 0.0);
                    } else {
                        printf("PUSHS int@0\n");
                    }
                    gen_expr(expr->data.unary_op.operand);
                    printf("SUBS\n");
                    break;

                case OP_NOT:
                    gen_expr(expr->data.unary_op.operand);
                    printf("NOTS\n");
                    break;

                default:
                    break;
            }
            break;

        case AST_BINARY_OP:
            // TODO: Dodelat BOOLTHEN operatory
            gen_expr(expr->data.bin_op.left);
            gen_expr(expr->data.bin_op.right);

            switch (expr->data.bin_op.operator_token) {
                case OP_PLUS:
                    if (expr->expr_type != NULL && expr->expr_type->type_class == TYPE_STRING) {
                        printf("POPS GF@%%tmp1\n");
                        printf("POPS GF@%%tmp2\n");
                        printf("CONCAT GF@%%tmp3 GF@%%tmp2 GF@%%tmp1\n");
                        printf("PUSHS GF@%%tmp3\n");
                    } else {
                        printf("ADDS\n");
                    }

                    break;

                case OP_MINUS:
                    printf("SUBS\n");
                    break;

                case OP_DIV:
                    if (expr->expr_type != NULL && expr->expr_type->type_class == TYPE_DOUBLE) {
                        printf("DIVS\n");

                    } else {
                        printf("IDIVS\n");
                    }
                    break;

                case OP_MUL:
                    printf("MULS\n");
                    break;

                // AI help
                /*
                 * Prompt: Napis mi bloky v tom switchi pro relacni operatory a dej tam zase stejnou
                 * AI hlavicku. AI hlavicky davej pokazde kdyz budes generovat nejaky blok kodu
                 */
                case OP_LESSTHEN:
                    printf("LTS\n");
                    break;

                case OP_GREATERTHEN:
                    printf("GTS\n");
                    break;

                case OP_EQ:
                    printf("EQS\n");
                    break;

                case OP_LESSEQ:
                    printf("GTS\n");
                    printf("NOTS\n");
                    break;

                case OP_GREATEREQ:
                    printf("LTS\n");
                    printf("NOTS\n");
                    break;

                case OP_NOTEQ:
                    printf("EQS\n");
                    printf("NOTS\n");
                    break;

                default:
                    break;
            }
            break;
        case AST_FUNC_CALL: {
            const ASTNode *curr_arg_group = expr->data.func_call.arg_groups;

            while (curr_arg_group != NULL) {
                const ASTNode *curr_item = curr_arg_group->data.list.items;

                while (curr_item != NULL) {
                    gen_expr(curr_item);
                    curr_item = curr_item->next;
                }
                curr_arg_group = curr_arg_group->next;
            }

            // AI help
            /*
             * Spolupráce s AI při návrhu větvení volání funkcí v AST_FUNC_CALL:
             * Na základě struktury AST (ast.h) jsme nejprve rozdělili volání uživatelských
             * a vestavěných funkcí podle položky call_kind. Původní větvení podle konkrétních
             * názvů vestavěných funkcí jsme následně zjednodušili na přímý výpis instrukce CALL
             * s prefixem $builtin_ podle jména identifikátoru, protože existenci i validitu
             * vestavěné funkce už ověřila sémantická analýza.
             */
            const ASTNode *target = expr->data.func_call.target;

            switch (expr->data.func_call.call_kind) {
                case CALL_USER_FUNC:
                    if (target != NULL && target->type == AST_IDENTIFIER) {
                        printf("CALL $func_%s\n", target->data.identifier.name);
                    }
                    break;

                case CALL_BUILTIN:
                    if (target != NULL && target->type == AST_IDENTIFIER) {
                        printf("CALL $builtin_%s\n", target->data.identifier.name);
                    }
                    break;

                default:
                    break;
            }

            break;
        }

        // TODO: dopsat rozsireni.
        // 2. Rozšíření (konstrukce vystupující jako výrazy)
        case AST_IF_EXPR:
            break;
        case AST_BLOCK:
            break;
        case AST_MATCH_EXPR:
            break;
        case AST_LAMBDA:
            break;
        case AST_LIST_LITERAL:
            break;
        case AST_MEMBER_ACCESS:
            break;
        case AST_RANGE:
            break;

        default:
            break;
    }

    if (expr->to_double == true) {
        printf("INT2FLOATS\n");
    }
}

// AI help
/*
 * Spolupráce s AI při návrhu a implementaci funkce gen_block:
 * Vzhledem k postupnému vývoji a ladění nelze uvést jeden izolovaný prompt. S AI byl
 * navržen mechanismus správy datového zásobníku na konci bloku (parametr keep_result
 * a příznak val_on_stack pro rozlišení návratové hodnoty funkce od mezivýsledků
 * zahazovaných do GF@%tmp1, včetně ošetření typu Unit). Následně byla s pomocí AI
 * vygenerována kostra větvení switch a implementovány jednotlivé větve základního
 * zadání (AST_VAR_DEF, AST_ASSIGNMENT, AST_RETURN se skokem na $end, AST_IF_EXPR,
 * AST_WHILE s generováním unikátních návěští přes l_cnt, AST_BLOCK a default).
 * Výsledný kód byl autorem zkontrolován a integrován do generátoru.
 */
void gen_block(const ASTNode *statement, GenState *stt, bool keep_result) {
    // Pokud je tělo funkce prázdné vracíme nil na zásobník
    if (statement == NULL && keep_result) {
        printf("PUSHS nil@nil\n");
        return;
    }

    while (statement != NULL) {
        // AI help
        /*
         * Prompt: Uz jsem si napsal while cyklus co projde vsechny prikazy v bloku az po NULL.
         * napis mi nyni kostru prikazu switch, ktery bude rozhodovat podle statement->type, jaky
         * vybere case: AST_VAR_DEF, AST_ASSIGNMENT a zbytek jako je if a while vcetne rozsireni
         */
        bool val_on_stack = keep_result && (statement->next == NULL);

        switch (statement->type) {
            // 1. Základní zadání (příkazy)
            case AST_VAR_DEF: {
                if (statement->data.var_def.initializer != NULL) {
                    gen_expr(statement->data.var_def.initializer);
                    printf("POPS LF@%s$%d\n", statement->data.var_def.identifier,
                           statement->data.var_def.var_id);
                }
                if (val_on_stack) {
                    printf("PUSHS nil@nil\n");
                }
                break;
            }

            case AST_ASSIGNMENT:
                gen_expr(statement->data.assignment.value);
                printf("POPS LF@%s$%d\n", statement->data.assignment.target,
                       statement->data.assignment.var_id);
                if (val_on_stack) {
                    printf("PUSHS nil@nil\n");
                }
                break;

            case AST_RETURN:
                if (statement->data.return_stmt.value != NULL) {
                    gen_expr(statement->data.return_stmt.value);
                    if (stt->is_unit) {
                        printf("POPS GF@%%tmp1\n");
                        printf("PUSHS nil@nil\n");
                    }
                } else {
                    printf("PUSHS nil@nil\n");
                }
                printf("JUMP $func_%s$end\n", stt->func_name);

                break;

            case AST_IF_EXPR: {
                int id = stt->l_cnt++;
                gen_expr(statement->data.if_expr.condition);
                printf("PUSHS bool@false\n");
                printf("JUMPIFEQS $if_else_%d\n", id);

                gen_block(statement->data.if_expr.then_block->data.block.statements, stt, false);
                printf("JUMP $if_end_%d\n", id);

                printf("LABEL $if_else_%d\n", id);
                if (statement->data.if_expr.else_block != NULL) {
                    gen_block(statement->data.if_expr.else_block->data.block.statements, stt,
                              false);
                }
                printf("LABEL $if_end_%d\n", id);

                if (val_on_stack) {
                    printf("PUSHS nil@nil\n");
                }
                break;
            }
            case AST_WHILE: {
                int id = stt->l_cnt++;
                printf("LABEL $while_start_%d\n", id);
                gen_expr(statement->data.while_loop.condition);
                printf("PUSHS bool@false\n");
                printf("JUMPIFEQS $while_end_%d\n", id);

                gen_block(statement->data.while_loop.body->data.block.statements, stt, false);
                printf("JUMP $while_start_%d\n", id);

                printf("LABEL $while_end_%d\n", id);
                if (val_on_stack) {
                    printf("PUSHS nil@nil\n");
                }
                break;
            }
            case AST_BLOCK:
                gen_block(statement->data.block.statements, stt, val_on_stack);
                break;

            // 2. Rozšíření (CONTROL)
            case AST_FOR:
                // TODO: dopsat rozsireni CONTROL
                break;

            // 3. Samostatně stojící výrazy a volání funkcí (vč. výrazů z rozšíření)
            default:
                gen_expr(statement);
                if (!val_on_stack) {
                    printf("POPS GF@%%tmp1\n");
                }
                break;
        }
        statement = statement->next;
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
