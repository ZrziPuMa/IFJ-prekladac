/**
 * Projekt: Implementace překladače imperativního jazyka IFJ26
 * @file generator.c
 * @brief Implementační soubor genrátoru cílového kódu
 * @author Martin Chlubna (<xchlubm00>), pomocný autor pro ALGEBDT Frantisek Bilavčík (<xbilavf00>),
 * který používal Github Copilot Inline suggestion a Claude Opus 5.5 pro pomoc opravením.
 *
 */

#include "generator.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void create_prolog(const ASTNode *main_def) {
    printf(".IFJcode26\n");
    printf("DEFVAR GF@%%tmp1\n");
    printf("DEFVAR GF@%%tmp2\n");
    printf("DEFVAR GF@%%tmp3\n");
    printf("DEFVAR GF@%%fstack\n");
    printf("MOVE GF@%%fstack nil@nil\n");
    printf("CALL $func_%s\n", main_def->data.func_def.name);
    printf("EXIT int@0\n");
    printf("\n");
}

// AI help
/* Napis mi pomocnou funkci create_ext_func_label, ktera vypise Label prave pro metody z
 *extension. nedelej to static, protoze to budu mit v hlavickovem souboru. a nad funkci napis
 *komentar trto formy: // AI help <prompt co jsem ti napsal>
 */
void create_ext_func_label(const TypeInfo *receiver_type, const char *method_name) {
    printf("$ext_");

    const TypeInfo *t = receiver_type;
    while (t != NULL && t->type_class == TYPE_LIST) {
        printf("List_");
        t = t->data.list_element_type;
    }

    if (t == NULL) {
        printf("Unknown");
    } else {
        switch (t->type_class) {
            case TYPE_INT:
                printf("Int");
                break;
            case TYPE_DOUBLE:
                printf("Double");
                break;
            case TYPE_STRING:
                printf("String");
                break;
            case TYPE_BOOLEAN:
                printf("Bool");
                break;
            case TYPE_UNIT:
                printf("Unit");
                break;
            case TYPE_ANY:
                printf("Any");
                break;
            case TYPE_NOTHING:
                printf("Nothing");
                break;
            case TYPE_CUSTOM:
                printf("%s", t->data.custom_name ? t->data.custom_name : "Custom");
                break;
            case TYPE_LIST:
                printf("List");
                break;
            case TYPE_UNION:
                printf("Union");
                break;
            case TYPE_FUNCTION:
                printf("Fn");
                break;
        }
    }

    printf("_%s", method_name);
}

void create_func_def(const ASTNode *func, const ASTNode *receiver, GenState *stt) {
    if (receiver != NULL) {
        printf("LABEL ");
        create_ext_func_label(receiver->expr_type, func->data.func_def.name);
        printf("\n");

    } else {
        printf("LABEL $func_%s\n", func->data.func_def.name);
    }

    printf("CREATEFRAME\n");
    printf("PUSHFRAME\n");

    // Pokud se jedná o methodu z extension, musíme deklarovat i receiver
    if (receiver != NULL) {
        printf("DEFVAR LF@%s$%d\n", receiver->data.var_def.identifier,
               receiver->data.var_def.var_id);
    }

    define_param_groups(func->data.func_def.param_groups);

    declare_local_vars(func->data.func_def.body);

    if (receiver != NULL) {
        printf("POPS LF@%s$%d\n", receiver->data.var_def.identifier, receiver->data.var_def.var_id);
    }
    // AI help
    /*
     * Spolupráce s AI při návrhu předávání návratové hodnoty z těla funkce:
     * S pomocí AI byla navržena detekce návratového typu TYPE_UNIT z uzel->expr_type
     * a její propojení s parametrem keep_result v gen_block tak, aby každá funkce
     * zanechala na datovém zásobníku právě jednu návratovou hodnotu (u Unit nil@nil).
     */
    const TypeInfo *fn_type = func->expr_type;
    stt->func_name = func->data.func_def.name;
    stt->stack_on_extra =
        0; // F.B. pro ALGEBDT: počet prvků na zásobníku, které se mají uklidit po návratu z funkce
    stt->is_unit = (fn_type != NULL && fn_type->type_class == TYPE_FUNCTION &&
                    fn_type->data.func_type.return_type != NULL &&
                    fn_type->data.func_type.return_type->type_class == TYPE_UNIT);

    gen_block(func->data.func_def.body->data.block.statements, stt, !stt->is_unit);
    if (stt->is_unit) {
        printf("PUSHS nil@nil\n");
    } else if (func->data.func_def.body->to_double) {
        printf("INT2FLOATS\n");
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
void gen_expr(const ASTNode *expr, GenState *stt) {

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

        case AST_IDENTIFIER: {
            // AI help
            /*
             * Prompt: Uprav mi case AST_IDENTIFIER v gen_expr() tak, aby pokud se jedna o funkci
             * pouzitou jako hodnota (var_id je -1), tak se na zasobnik pushne nazev navesti te
             * funkce ve stringu aby se funkce pak mohla zavolat
             */

            if (expr->data.identifier.var_id == -1) {
                printf("PUSHS string@$func_%s\n", expr->data.identifier.name);
            } else {
                printf("PUSHS LF@%s$%d\n", expr->data.identifier.name,
                       expr->data.identifier.var_id);
            }
            break;
        }

            // AI help
            /* Prompt: switch (expr->data.unary_op.operator_token) {
             * } dopis mi tento switch, kde pokud to bude +, tak se nestane nic, pokud minus, tak
             * udelame ten odecet od nuly na zasobniku, a pokud tam bude negace, tak udelame
             * instrukci NOTS nad tim co je na zasobniku
             */
        case AST_UNARY_OP:
            switch (expr->data.unary_op.operator_token) {
                case OP_PLUS:
                    gen_expr(expr->data.unary_op.operand, stt);
                    break;

                case OP_MINUS:
                    if (expr->expr_type != NULL && expr->expr_type->type_class == TYPE_DOUBLE) {
                        printf("PUSHS float@%a\n", 0.0);
                    } else {
                        printf("PUSHS int@0\n");
                    }
                    gen_expr(expr->data.unary_op.operand, stt);
                    printf("SUBS\n");
                    break;

                case OP_NOT:
                    gen_expr(expr->data.unary_op.operand, stt);
                    printf("NOTS\n");
                    break;

                default:
                    break;
            }
            break;

        case AST_BINARY_OP:

            // BOOLTHEN operátory:
            if (expr->data.bin_op.operator_token == OP_AND) {
                int id = stt->l_cnt++;
                gen_expr(expr->data.bin_op.left, stt);
                printf("PUSHS bool@false\n");
                printf("JUMPIFEQS $and_false_%d\n", id);

                gen_expr(expr->data.bin_op.right, stt);

                printf("JUMP $and_end_%d\n", id);
                printf("LABEL $and_false_%d\n", id);
                printf("PUSHS bool@false\n");
                printf("LABEL $and_end_%d\n", id);
                break;

            } else if (expr->data.bin_op.operator_token == OP_OR) {
                int id = stt->l_cnt++;
                gen_expr(expr->data.bin_op.left, stt);
                printf("PUSHS bool@true\n");
                printf("JUMPIFEQS $or_true_%d\n", id);

                gen_expr(expr->data.bin_op.right, stt);

                printf("JUMP $or_end_%d\n", id);
                printf("LABEL $or_true_%d\n", id);
                printf("PUSHS bool@true\n");
                printf("LABEL $or_end_%d\n", id);
                break;
            }

            // Normální operátory:
            gen_expr(expr->data.bin_op.left, stt);
            gen_expr(expr->data.bin_op.right, stt);

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
            // Rozšíření EXTKW: pokud se jedná o metodu z extension, musíme na zásobník nejdříve
            // pushnout hodnotu příjemce (Hodnota nalevo od tečky)
            const ASTNode *target = expr->data.func_call.target;

            const ASTNode *curr_arg_group = expr->data.func_call.arg_groups;

            // AI help
            if (expr->data.func_call.own_groups == 0) {
                gen_expr(target, stt);
            } else {

                if (target != NULL && expr->data.func_call.call_kind == CALL_EXTENSION) {
                    gen_expr(target->data.member_access.object, stt);
                }

                for (int i = 0; i < expr->data.func_call.own_groups && curr_arg_group != NULL;
                     i++) {
                    const ASTNode *curr_item = curr_arg_group->data.list.items;

                    while (curr_item != NULL) {
                        gen_expr(curr_item, stt);
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

                    case CALL_EXTENSION:
                        if (target != NULL && target->type == AST_MEMBER_ACCESS) {
                            printf("CALL ");
                            create_ext_func_label(target->data.member_access.object->expr_type,
                                                  target->data.member_access.member_name);
                            printf("\n");
                        }
                        break;

                    case CALL_CONSTRUCTOR: // F.B. ALGEBDT: Point(10, 20)
                        gen_constructor(expr, stt);
                        break;

                    default:
                        break;
                }
            }
            // AI help
            while (curr_arg_group != NULL) {
                printf("POPS GF@%%tmp1\n");
                printf("NEWARRAY GF@%%tmp2 int@2\n");
                printf("ARRAYSET GF@%%tmp2 int@0 GF@%%fstack\n");
                printf("ARRAYSET GF@%%tmp2 int@1 GF@%%tmp1\n");
                printf("MOVE GF@%%fstack GF@%%tmp2\n");

                const ASTNode *curr_item = curr_arg_group->data.list.items;
                while (curr_item != NULL) {
                    gen_expr(curr_item, stt);
                    curr_item = curr_item->next;
                }

                printf("ARRAYGET GF@%%tmp1 GF@%%fstack int@1\n");
                printf("ARRAYGET GF@%%fstack GF@%%fstack int@0\n");
                printf("CALL GF@%%tmp1\n");

                curr_arg_group = curr_arg_group->next;
            }

            break;
        }

        // 2. Rozšíření (konstrukce vystupující jako výrazy)
        // AI help
        /*
         * Spolupráce s AI při implementaci AST_IF_EXPR a AST_BLOCK v gen_expr():
         * Prompt (kontext): "pro dokonceni control mi pak uz jen bude stacit podporovat if ve
         * vyrazu, ale to uz bude rychlovka ne? Jen doplnim par radku v gen_expr u ast_if_expr"
         * Na základě konzultace s AI byla větev rozšířena o ošetření implicitní konverze
         * to_double na uzlech then_block a else_block a o vyhodnocení AST_BLOCK na místě
         * výrazu.
         */
        case AST_IF_EXPR: {
            int id = stt->l_cnt++;
            const ASTNode *then_blk = expr->data.if_expr.then_block;
            const ASTNode *else_blk = expr->data.if_expr.else_block;

            gen_expr(expr->data.if_expr.condition, stt);
            printf("PUSHS bool@false\n");
            printf("JUMPIFEQS $if_else_%d\n", id);

            gen_block(then_blk->data.block.statements, stt, true);
            if (then_blk->to_double) {
                printf("INT2FLOATS\n");
            }
            printf("JUMP $if_end_%d\n", id);

            printf("LABEL $if_else_%d\n", id);
            if (else_blk != NULL) {
                gen_block(else_blk->data.block.statements, stt, true);
                if (else_blk->to_double) {
                    printf("INT2FLOATS\n");
                }
            }
            printf("LABEL $if_end_%d\n", id);
            break;
        }

        case AST_BLOCK:
            gen_block(expr->data.block.statements, stt, true);
            break;

        case AST_MATCH_EXPR: // F.B. ALGEBDT
            gen_match(expr, stt);
            break;

        case AST_LAMBDA: {

            // AI help
            /*
             * Prompt: vytvoř mi blok kodu do AST_LAMBDA v gen_expr, ktery vytiskne instrukci
             * pro pushnuti nazvu lambdy ve stringu na zasobnik id zjisti pomoci  get_lambda_id
             */
            int id = get_lambda_id(stt, expr);
            if (id >= 0) {
                printf("PUSHS string@$lambda_%d\n", id);
            }
            break;
        }

        break;
        case AST_LIST_LITERAL:
            break;
        // AI help
        /*
         * Prompty:
         * 1. Napis mi tedy switch pro case AST_MEMBER_ACCESS, ktery bude rozhodovat podle
         * member kind a bude obsahovat vsechny case z enumu member kind
         *
         * 2. napis mi blok kodu, ktery bude vybirat, ktereou builtin funkci zavola podle
         * member_name
         */
        case AST_MEMBER_ACCESS: {
            switch (expr->data.member_access.member_kind) {
                case MEMBER_VALUE_BUILTIN:
                    gen_expr(expr->data.member_access.object, stt);

                    const char *member_name = expr->data.member_access.member_name;
                    if (strcmp(member_name, "toString") == 0) {
                        printf("CALL $builtin_toStr\n");
                    } else if (strcmp(member_name, "toInt") == 0) {
                        printf("CALL $builtin_toInt\n");
                    } else if (strcmp(member_name, "floor") == 0) {
                        printf("CALL $builtin_floor\n");
                    } else if (strcmp(member_name, "length") == 0) {
                        printf("CALL $builtin_strLength\n");
                    }
                    break;

                case MEMBER_FIELD: // F.B. ALGEBDT: p.x
                    gen_field_access(expr, stt);
                    break;

                case MEMBER_LIST_BUILTIN:
                    break;

                case MEMBER_RANGE_TOLIST:
                    break;

                case MEMBER_EXTENSION:
                    break;

                case MEMBER_UNRESOLVED:
                default:
                    break;
            }
            break;
        }

        case AST_RANGE:
            // Dostaneme hodnoty start, end a step na zásobník:
            gen_expr(expr->data.range.start, stt);
            gen_expr(expr->data.range.end, stt);

            if (expr->data.range.step != NULL) {
                gen_expr(expr->data.range.step, stt);
            } else {
                printf("PUSHS int@1\n");
            }
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
         * napis mi nyni kostru prikazu switch, ktery bude rozhodovat podle statement->type,
         * jaky vybere case: AST_VAR_DEF, AST_ASSIGNMENT a zbytek jako je if a while vcetne
         * rozsireni
         */
        bool val_on_stack = keep_result && (statement->next == NULL);

        switch (statement->type) {
            // 1. Základní zadání (příkazy)
            case AST_VAR_DEF: {
                if (statement->data.var_def.initializer != NULL) {
                    gen_expr(statement->data.var_def.initializer, stt);
                    printf("POPS LF@%s$%d\n", statement->data.var_def.identifier,
                           statement->data.var_def.var_id);
                }
                if (val_on_stack) {
                    printf("PUSHS nil@nil\n");
                }
                break;
            }

            case AST_ASSIGNMENT:
                gen_expr(statement->data.assignment.value, stt);
                printf("POPS LF@%s$%d\n", statement->data.assignment.target,
                       statement->data.assignment.var_id);
                if (val_on_stack) {
                    printf("PUSHS nil@nil\n");
                }
                break;

            case AST_RETURN:
                if (statement->data.return_stmt.value != NULL) {
                    gen_expr(statement->data.return_stmt.value, stt);
                    if (stt->is_unit) {
                        printf("POPS GF@%%tmp1\n");
                        printf("PUSHS nil@nil\n");
                    }
                } else {
                    printf("PUSHS nil@nil\n");
                }
                if (stt->stack_on_extra > 0) {
                    printf("POPS GF@%%tmp1\n");
                    for (int k = 0; k < stt->stack_on_extra; k++) {
                        printf("POPS GF@%%tmp2\n");
                    }
                    printf("PUSHS GF@%%tmp1\n");
                }
                printf("JUMP $func_%s$end\n", stt->func_name);

                break;
            // AI help
            /*
             * Úprava větve AST_IF_EXPR v gen_block() na základě code review s AI:
             * AI upozornila na případ, kdy funkce končí if-výrazem s větví else (keep_result ==
             * true), kde původní kód vždy vracel nil@nil místo výsledku větve. Podle návrhu AI
             * byla přidána propagace příznaku keep_branch_val a kontrola to_double na obou
             * větvích.
             */
            case AST_IF_EXPR: {
                int id = stt->l_cnt++;
                const ASTNode *then_blk = statement->data.if_expr.then_block;
                const ASTNode *else_blk = statement->data.if_expr.else_block;
                bool keep_branch_val = val_on_stack && (else_blk != NULL);

                gen_expr(statement->data.if_expr.condition, stt);
                printf("PUSHS bool@false\n");
                printf("JUMPIFEQS $if_else_%d\n", id);

                gen_block(then_blk->data.block.statements, stt, keep_branch_val);
                if (keep_branch_val && then_blk->to_double) {
                    printf("INT2FLOATS\n");
                }
                printf("JUMP $if_end_%d\n", id);

                printf("LABEL $if_else_%d\n", id);
                if (else_blk != NULL) {
                    gen_block(else_blk->data.block.statements, stt, keep_branch_val);
                    if (keep_branch_val && else_blk->to_double) {
                        printf("INT2FLOATS\n");
                    }
                }
                printf("LABEL $if_end_%d\n", id);

                if (val_on_stack && else_blk == NULL) {
                    printf("PUSHS nil@nil\n");
                }
                break;
            }
            case AST_WHILE: {
                int id = stt->l_cnt++;
                printf("LABEL $while_start_%d\n", id);
                gen_expr(statement->data.while_loop.condition, stt);
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
            case AST_FOR: {
                ASTNode *source = statement->data.for_loop.source;
                if (source != NULL && source->type == AST_RANGE) {
                    int id = stt->l_cnt++;
                    ASTNode *iter_var = statement->data.for_loop.iter_var;

                    gen_expr(source, stt);
                    printf("POPS GF@%%tmp3\n");

                    // Kontrola nuloveho step
                    printf("JUMPIFNEQ $for_step_ok_%d GF@%%tmp3 int@0\n", id);
                    printf("EXIT int@40\n");
                    printf("LABEL $for_step_ok_%d\n", id);

                    printf("POPS GF@%%tmp2\n");
                    printf("POPS LF@%s$%d\n", iter_var->data.var_def.identifier,
                           iter_var->data.var_def.var_id);
                    printf("PUSHS GF@%%tmp2\n");
                    printf("PUSHS GF@%%tmp3\n");

                    printf("LABEL $for_start_%d\n", id);

                    // Nahrán end a step do pomocných proměnných
                    printf("POPS GF@%%tmp3\n");
                    printf("POPS GF@%%tmp2\n");
                    printf("PUSHS GF@%%tmp2\n");
                    printf("PUSHS GF@%%tmp3\n");
                    printf("LT GF@%%tmp1 GF@%%tmp3 int@0\n");
                    printf("JUMPIFEQ $for_neg_%d GF@%%tmp1 bool@true\n", id);

                    // Zjištění, jestli se jedná o <= nebo <
                    if (!source->data.range.is_until) {
                        printf("GT GF@%%tmp1 LF@%s$%d GF@%%tmp2\n",
                               iter_var->data.var_def.identifier, iter_var->data.var_def.var_id);
                        printf("JUMPIFEQ $for_end_%d GF@%%tmp1 bool@true\n", id);
                    } else {
                        printf("LT GF@%%tmp1 LF@%s$%d GF@%%tmp2\n",
                               iter_var->data.var_def.identifier, iter_var->data.var_def.var_id);
                        printf("JUMPIFEQ $for_end_%d GF@%%tmp1 bool@false\n", id);
                    }

                    // Přeskočení větve pro záporný krok:
                    printf("JUMP $for_body_%d\n", id);

                    printf("LABEL $for_neg_%d\n", id);

                    // Zjištění, jestli se jedná o <= nebo <
                    if (!source->data.range.is_until) {
                        printf("LT GF@%%tmp1 LF@%s$%d GF@%%tmp2\n",
                               iter_var->data.var_def.identifier, iter_var->data.var_def.var_id);
                        printf("JUMPIFEQ $for_end_%d GF@%%tmp1 bool@true\n", id);
                    } else {
                        printf("GT GF@%%tmp1 LF@%s$%d GF@%%tmp2\n",
                               iter_var->data.var_def.identifier, iter_var->data.var_def.var_id);
                        printf("JUMPIFEQ $for_end_%d GF@%%tmp1 bool@false\n", id);
                    }

                    printf("LABEL $for_body_%d\n", id);

                    // Kontrola stráží (Guards):
                    const ASTNode *curr_guard = statement->data.for_loop.guards;
                    while (curr_guard != NULL) {
                        gen_expr(curr_guard, stt);
                        printf("PUSHS bool@false\n");
                        printf("JUMPIFEQS $for_next_%d\n", id);
                        curr_guard = curr_guard->next;
                    }

                    gen_block(statement->data.for_loop.body->data.block.statements, stt, false);

                    // AI help
                    /*
                     * Prompt: Napis mi blok vypisu instrukci pro IFJCODE26, ktere inkrementuje
                     * řídící proměnnou a skočí na začátek cyklu (navesti for_next_id) a pod to
                     * navesti for_end_id, ktere ukidi zasobnik.
                     */
                    printf("LABEL $for_next_%d\n", id);
                    printf("POPS GF@%%tmp3\n");
                    printf("PUSHS GF@%%tmp3\n");
                    printf("ADD LF@%s$%d LF@%s$%d GF@%%tmp3\n", iter_var->data.var_def.identifier,
                           iter_var->data.var_def.var_id, iter_var->data.var_def.identifier,
                           iter_var->data.var_def.var_id);
                    printf("JUMP $for_start_%d\n", id);

                    printf("LABEL $for_end_%d\n", id);
                    printf("POPS GF@%%tmp1\n");
                    printf("POPS GF@%%tmp1\n");

                } else {
                    // TODO: LISTS
                }

                if (val_on_stack) {
                    printf("PUSHS nil@nil\n");
                }

                break;
            }
            // 3. Samostatně stojící výrazy a volání funkcí (vč. výrazů z rozšíření)
            default:
                gen_expr(statement, stt);
                if (!val_on_stack) {
                    printf("POPS GF@%%tmp1\n");
                }
                break;
        }
        statement = statement->next;
    }
}

// AI help
/*
 * Prompt: Vytvoř mi funkce pro správu id ukazatelů na lambdy, které budou využívat proměnných
 * ze struktury stt. ukazatele se budou uchovavat v const ASTNode **lambdas v stt. jedna funkce
 * vytvori novy zaznam v dynamickem poli a pripadne pole nafoukne. dalsi bude pro ziskani
 * hodnoty id za ucelem vypsani navesti a posledni na uvolneni dynamickeho pole
 */

int register_lambda(GenState *stt, const ASTNode *lambda) {
    if (stt == NULL || lambda == NULL) {
        return -1;
    }

    if (stt->lambda_cnt >= stt->lambda_cap) {
        int new_cap = (stt->lambda_cap == 0) ? 16 : stt->lambda_cap * 2;
        const ASTNode **new_arr = realloc(stt->lambdas, (size_t)new_cap * sizeof(const ASTNode *));
        if (new_arr == NULL) {
            return -1;
        }
        stt->lambdas = new_arr;
        stt->lambda_cap = new_cap;
    }

    int id = stt->lambda_cnt++;
    stt->lambdas[id] = lambda;
    return id;
}

int get_lambda_id(const GenState *stt, const ASTNode *lambda) {
    if (stt == NULL || lambda == NULL) {
        return -1;
    }

    for (int i = 0; i < stt->lambda_cnt; i++) {
        if (stt->lambdas[i] == lambda) {
            return i;
        }
    }
    return -1;
}

void free_lambdas(GenState *stt) {
    if (stt == NULL) {
        return;
    }

    free(stt->lambdas);
    stt->lambdas = NULL;
    stt->lambda_cnt = 0;
    stt->lambda_cap = 0;
}

// AI help
/*
 * Prompt: Vytvoř mi tedy funkci pro vytvoření definice lambda funkce, kde se nejdrive vypise
 * navesti se spravnym id, ktere zjistime zavolanim get_lambda_id() a vytvori se lokalni ramec.
 * nasledne vyzvedne parametry pomoci define_params a nadeklaruje lokalni promenne pomoci
 * declare_local_vars. pote zavola gen_block() nad telem lambdy a osetri pripadny navratovy typ
 * unit nakonec provede uklid lokalniho ramce
 */
void create_lambda_def(const ASTNode *lambda, GenState *stt) {
    if (lambda == NULL || stt == NULL) {
        return;
    }

    int id = get_lambda_id(stt, lambda);
    if (id < 0) {
        return;
    }

    printf("LABEL $lambda_%d\n", id);
    printf("CREATEFRAME\n");
    printf("PUSHFRAME\n");

    if (lambda->data.lambda.param_list != NULL) {
        define_params(lambda->data.lambda.param_list->data.list.items);
    }

    declare_local_vars(lambda->data.lambda.body);

    const TypeInfo *fn_type = lambda->expr_type;
    bool prev_is_unit = stt->is_unit;
    stt->is_unit = (fn_type != NULL && fn_type->type_class == TYPE_FUNCTION &&
                    fn_type->data.func_type.return_type != NULL &&
                    fn_type->data.func_type.return_type->type_class == TYPE_UNIT);

    // AI help
    char lambda_name[32];
    snprintf(lambda_name, sizeof(lambda_name), "lambda_%d", id);
    const char *prev_func_name = stt->func_name;
    stt->func_name = lambda_name;

    if (lambda->data.lambda.body != NULL) {
        gen_block(lambda->data.lambda.body->data.block.statements, stt, !stt->is_unit);
    }

    if (stt->is_unit) {
        printf("PUSHS nil@nil\n");
    } else if (lambda->data.lambda.body != NULL && lambda->data.lambda.body->to_double) {
        printf("INT2FLOATS\n");
    }

    // AI help
    printf("LABEL $func_%s$end\n", lambda_name);
    stt->func_name = prev_func_name;

    stt->is_unit = prev_is_unit;
    printf("POPFRAME\n");
    printf("RETURN\n\n");
}

// AI help
/*
 * Prompt: Vytvoř mi pomocnou funkci create_all_lambdas ktera dostane ukazatel na prvni definici
 * nejvyssi urovne. Pomoci cyklu while() projde vsechny definice a rekurzivne projde vsechny
 * uzly ktere mohou obsahovat vyrazy. pokud narazi na AST_LAMBDA, přidělí jí unikátní id pomocí
 * register_lambda(). Pro přiípad, že by v těle lambdy byla dalsi lambda funkce projde
 * rekurzivne i jeji telo. Nakonec vygeneruje jeji definici pomoci create_lambda_def()
 */
void create_all_lambdas(const ASTNode *node, GenState *stt) {
    if (stt == NULL) {
        return;
    }

    while (node != NULL) {
        switch (node->type) {
            case AST_FUNC_DEF:
                create_all_lambdas(node->data.func_def.body, stt);
                break;

            case AST_EXTENSION_DEF:
                create_all_lambdas(node->data.extension_def.methods, stt);
                break;

            case AST_BLOCK:
                create_all_lambdas(node->data.block.statements, stt);
                break;

            case AST_VAR_DEF:
                create_all_lambdas(node->data.var_def.initializer, stt);
                break;

            case AST_ASSIGNMENT:
                create_all_lambdas(node->data.assignment.value, stt);
                break;

            case AST_RETURN:
                create_all_lambdas(node->data.return_stmt.value, stt);
                break;

            case AST_IF_EXPR:
                create_all_lambdas(node->data.if_expr.condition, stt);
                create_all_lambdas(node->data.if_expr.then_block, stt);
                create_all_lambdas(node->data.if_expr.else_block, stt);
                break;

            case AST_WHILE:
                create_all_lambdas(node->data.while_loop.condition, stt);
                create_all_lambdas(node->data.while_loop.body, stt);
                break;

            case AST_FOR:
                create_all_lambdas(node->data.for_loop.source, stt);
                create_all_lambdas(node->data.for_loop.guards, stt);
                create_all_lambdas(node->data.for_loop.body, stt);
                break;

            case AST_MATCH_EXPR:
                create_all_lambdas(node->data.match_expr.matched_expr, stt);
                create_all_lambdas(node->data.match_expr.cases, stt);
                break;

            case AST_CASE_BRANCH:
                create_all_lambdas(node->data.case_branch.guard, stt);
                create_all_lambdas(node->data.case_branch.body, stt);
                break;

            case AST_BINARY_OP:
                create_all_lambdas(node->data.bin_op.left, stt);
                create_all_lambdas(node->data.bin_op.right, stt);
                break;

            case AST_UNARY_OP:
                create_all_lambdas(node->data.unary_op.operand, stt);
                break;

            case AST_FUNC_CALL:
                create_all_lambdas(node->data.func_call.target, stt);
                create_all_lambdas(node->data.func_call.arg_groups, stt);
                break;

            case AST_ARG_LIST:
            case AST_LIST_LITERAL:
                create_all_lambdas(node->data.list.items, stt);
                break;

            case AST_MEMBER_ACCESS:
                create_all_lambdas(node->data.member_access.object, stt);
                break;

            case AST_RANGE:
                create_all_lambdas(node->data.range.start, stt);
                create_all_lambdas(node->data.range.end, stt);
                create_all_lambdas(node->data.range.step, stt);
                break;

            case AST_LAMBDA: {
                int id = register_lambda(stt, node);
                create_all_lambdas(node->data.lambda.body, stt);
                if (id >= 0) {
                    create_lambda_def(node, stt);
                }
                break;
            }

            default:
                break;
        }
        node = node->next;
    }
}

void create_builtin_functions() {
    // toInt():
    printf("LABEL $builtin_toInt\n");
    printf("FLOAT2INTS\n");
    printf("RETURN\n\n");

    // chr():
    printf("LABEL $builtin_chr\n");
    printf("INT2CHARS\n");
    printf("RETURN\n\n");

    // strLength():
    printf("LABEL $builtin_strLength\n");
    printf("POPS GF@%%tmp1\n");
    printf("STRLEN GF@%%tmp2 GF@%%tmp1\n");
    printf("PUSHS GF@%%tmp2\n");
    printf("RETURN\n\n");

    // readLine():
    printf("LABEL $builtin_readLine\n");
    printf("READ GF@%%tmp1 string\n");
    printf("JUMPIFNEQ $builtin_readLine_end GF@%%tmp1 nil@nil\n");
    printf("MOVE GF@%%tmp1 string@\n");
    printf("LABEL $builtin_readLine_end\n");
    printf("PUSHS GF@%%tmp1\n");
    printf("RETURN\n\n");

    // readInt():
    printf("LABEL $builtin_readInt\n");
    printf("READ GF@%%tmp1 int\n");
    printf("JUMPIFNEQ $builtin_readInt_end GF@%%tmp1 nil@nil\n");
    printf("MOVE GF@%%tmp1 int@0\n");
    printf("LABEL $builtin_readInt_end\n");
    printf("PUSHS GF@%%tmp1\n");
    printf("RETURN\n\n");

    // readDouble():
    printf("LABEL $builtin_readDouble\n");
    printf("READ GF@%%tmp1 float\n");
    printf("JUMPIFNEQ $builtin_readDouble_end GF@%%tmp1 nil@nil\n");
    printf("MOVE GF@%%tmp1 float@0x0p+0\n");
    printf("LABEL $builtin_readDouble_end\n");
    printf("PUSHS GF@%%tmp1\n");
    printf("RETURN\n\n");

    // readBoolean():
    printf("LABEL $builtin_readBoolean\n");
    printf("READ GF@%%tmp1 bool\n");
    printf("JUMPIFNEQ $builtin_readBoolean_end GF@%%tmp1 nil@nil\n");
    printf("MOVE GF@%%tmp1 bool@false\n");
    printf("LABEL $builtin_readBoolean_end\n");
    printf("PUSHS GF@%%tmp1\n");
    printf("RETURN\n\n");

    // F.B. ALGEBDT readIntMaybe(), readDoubleMaybe() : při chybě i EOF vrací (), => nil
    printf("LABEL $builtin_readIntMaybe\n");
    printf("READ GF@%%tmp1 int\n");
    printf("PUSHS GF@%%tmp1\n");
    printf("RETURN\n\n");

    printf("LABEL $builtin_readDoubleMaybe\n");
    printf("READ GF@%%tmp1 float\n");
    printf("PUSHS GF@%%tmp1\n");
    printf("RETURN\n\n");

    // print():
    printf("LABEL $builtin_print\n");
    printf("POPS GF@%%tmp1\n");
    printf("TYPE GF@%%tmp2 GF@%%tmp1\n");
    printf("JUMPIFNEQ $builtin_print_end GF@%%tmp2 string@nil\n");
    printf("WRITE string@()\n");
    printf("PUSHS nil@nil\n");
    printf("RETURN\n");
    printf("LABEL $builtin_print_end\n");
    printf("WRITE GF@%%tmp1\n");
    printf("PUSHS nil@nil\n");
    printf("RETURN\n\n");

    // ord():
    printf("LABEL $builtin_ord\n");
    printf("POPS GF@%%tmp2\n");
    printf("POPS GF@%%tmp1\n");
    printf("LT GF@%%tmp3 GF@%%tmp2 int@0\n");
    printf("JUMPIFEQ $builtin_ord_err GF@%%tmp3 bool@true\n");

    printf("STRLEN GF@%%tmp3 GF@%%tmp1\n");
    printf("LT GF@%%tmp3 GF@%%tmp2 GF@%%tmp3\n");
    printf("JUMPIFEQ $builtin_ord_err GF@%%tmp3 bool@false\n");

    printf("STRI2INT GF@%%tmp3 GF@%%tmp1 GF@%%tmp2\n");
    printf("PUSHS GF@%%tmp3\n");
    printf("RETURN\n");

    printf("LABEL $builtin_ord_err\n");
    printf("PUSHS int@-1\n");
    printf("RETURN\n\n");

    // toStr():
    printf("LABEL $builtin_toStr\n");
    printf("POPS GF@%%tmp1\n");
    printf("TYPE GF@%%tmp2 GF@%%tmp1\n");

    printf("JUMPIFEQ $builtin_toStr_end GF@%%tmp2 string@string\n");
    printf("JUMPIFEQ $builtin_toStr_int GF@%%tmp2 string@int\n");
    printf("JUMPIFEQ $builtin_toStr_float GF@%%tmp2 string@float\n");
    printf("JUMPIFEQ $builtin_toStr_bool GF@%%tmp2 string@bool\n");
    printf("JUMPIFEQ $builtin_toStr_nil GF@%%tmp2 string@nil\n");

    printf("LABEL $builtin_toStr_int\n");
    printf("INT2STR GF@%%tmp1 GF@%%tmp1\n");
    printf("JUMP $builtin_toStr_end\n");

    printf("LABEL $builtin_toStr_float\n");
    printf("FLOAT2STR GF@%%tmp1 GF@%%tmp1\n");
    printf("JUMP $builtin_toStr_end\n");

    printf("LABEL $builtin_toStr_bool\n");
    printf("JUMPIFEQ $builtin_toStr_bool_true GF@%%tmp1 bool@true\n");
    printf("MOVE GF@%%tmp1 string@false\n");
    printf("JUMP $builtin_toStr_end\n");
    printf("LABEL $builtin_toStr_bool_true\n");
    printf("MOVE GF@%%tmp1 string@true\n");
    printf("JUMP $builtin_toStr_end\n");

    printf("LABEL $builtin_toStr_nil\n");
    printf("MOVE GF@%%tmp1 string@()\n");

    printf("LABEL $builtin_toStr_end\n");
    printf("PUSHS GF@%%tmp1\n");
    printf("RETURN\n\n");

    // subStr():
    printf("LABEL $builtin_subStr\n");
    printf("CREATEFRAME\n");
    printf("PUSHFRAME\n");

    printf("DEFVAR LF@str\n");
    printf("DEFVAR LF@start\n");
    printf("DEFVAR LF@end\n");
    printf("DEFVAR LF@len\n");
    printf("DEFVAR LF@res\n");
    printf("DEFVAR LF@ch\n");
    printf("DEFVAR LF@cmp\n");

    printf("POPS LF@end\n");
    printf("POPS LF@start\n");
    printf("POPS LF@str\n");

    printf("STRLEN LF@len LF@str\n");

    // AI help
    /*
     * Prompt: Napis mi blok kodu, ktery provede vsechny pozadovane kontroly a v pripade
     * neuspechu skoci na navesti $builtin_subStr_err ktere uz mam v kodu napsane
     */

    // 1. start < 0
    printf("LT LF@cmp LF@start int@0\n");
    printf("JUMPIFEQ $builtin_subStr_err LF@cmp bool@true\n");

    // 2. end < 0
    printf("LT LF@cmp LF@end int@0\n");
    printf("JUMPIFEQ $builtin_subStr_err LF@cmp bool@true\n");

    // 3. start > end
    printf("GT LF@cmp LF@start LF@end\n");
    printf("JUMPIFEQ $builtin_subStr_err LF@cmp bool@true\n");

    // 4. start >= len
    printf("LT LF@cmp LF@start LF@len\n");
    printf("JUMPIFEQ $builtin_subStr_err LF@cmp bool@false\n");

    // 5. end > len
    printf("GT LF@cmp LF@end LF@len\n");
    printf("JUMPIFEQ $builtin_subStr_err LF@cmp bool@true\n");

    printf("MOVE LF@res string@\n");
    printf("LABEL $builtin_subStr_loop\n");
    printf("LT LF@cmp LF@start LF@end\n");
    printf("JUMPIFEQ $builtin_subStr_end LF@cmp bool@false\n");
    printf("GETCHAR LF@ch LF@str LF@start\n");
    printf("CONCAT LF@res LF@res LF@ch\n");
    printf("ADD LF@start LF@start int@1\n");
    printf("JUMP $builtin_subStr_loop\n");

    printf("LABEL $builtin_subStr_err\n");
    printf("EXIT int@40\n");

    printf("LABEL $builtin_subStr_end\n");
    printf("PUSHS LF@res\n");
    printf("POPFRAME\n");
    printf("RETURN\n\n");

    // AI help
    /*
     * Prompt: Pridej mi do moji funkce s vestavenymi funkcemi funkci floor, ktera bude fungovat
     * tak, ze orizne hodnotu typu double smerem k minus nekonecnu.
     */
    // floor (rozšíření VALMEM)
    printf("LABEL $builtin_floor\n");
    printf("POPS GF@%%tmp1\n");
    printf("FLOAT2INT GF@%%tmp2 GF@%%tmp1\n");
    printf("INT2FLOAT GF@%%tmp2 GF@%%tmp2\n");
    printf("GT GF@%%tmp3 GF@%%tmp2 GF@%%tmp1\n");
    printf("JUMPIFNEQ $builtin_floor_end GF@%%tmp3 bool@true\n");
    printf("SUB GF@%%tmp2 GF@%%tmp2 float@0x1p+0\n");
    printf("LABEL $builtin_floor_end\n");
    printf("PUSHS GF@%%tmp2\n");
    printf("RETURN\n\n");
}

ErrorCode generate_code(const ASTNode *root) {
    if (root == NULL) {
        return print_error(ERR_INTERNAL, -1, "Generátor přijal neplatný kořen stromu.");
    }

    if (root->type != AST_PROGRAM) {
        return print_error(ERR_INTERNAL, -1, "Kořenový uzel AST je nesprávného typu.");
    }

    GenState stt = {.l_cnt = 0};
    stt.program = root; // F.B. pro ALGEBDT

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

    // Registrace a vygenerování lambd před funkcemi, aby znaly jejich ID návěští
    create_all_lambdas(curr_def, &stt);

    // Procházení všech definic nejvyšší úrovně
    while (curr_def != NULL) {

        switch (curr_def->type) {

            case AST_FUNC_DEF:
                create_func_def(curr_def, NULL, &stt);
                break;

                // TODO: Dodělat zbylé definice
            case AST_EXTENSION_DEF: {
                ASTNode *curr_method = curr_def->data.extension_def.methods;
                while (curr_method != NULL) {
                    create_func_def(curr_method, curr_def->data.extension_def.receiver, &stt);
                    curr_method = curr_method->next;
                }
                break;
            }
            default:
                break;
        }
        curr_def = curr_def->next;
    }

    // Vytvoření těl všech vestavěných funkcí
    create_builtin_functions();

    // Úklid dynamického pole s ukazateli na lambda funkce
    free_lambdas(&stt);

    return ERR_OK;
}

// ==== Rozsireni ALGEBDT ==== //
// Claude (Opus 5.5), prompt: „Navrhni potřebné pomocné funkce pro implementaci ALGEBDT,
// používám 3 funkce: gen_match, gen_field_access a gen_constructor navržené v generator.h. Jaké
// funkce budu potřebovat a co bude jejich cílem?“ AI help, použit AI při navrhování pomocných
// funkcí pro rozšíření a opravování chyb ve psaní kodu

/**
 * @brief Pomocná funkce pro vyhledání definice třídy podle jména v rámci programu.
 *
 * @param stt Ukazatel na stav generátoru kódu.
 * @param name Jméno třídy, kterou hledáme.
 * @return Ukazatel na uzel AST reprezentující definici třídy, pokud byla nalezena, jinak NULL.
 */
static const ASTNode *adt_find_class(const GenState *stt, const char *name) {
    if (stt->program == NULL || name == NULL) {
        return NULL;
    }
    for (const ASTNode *def = stt->program->data.program.definitions; def != NULL;
         def = def->next) {
        if (def->type == AST_CLASS_DEF && strcmp(def->data.class_def.name, name) == 0) {
            return def;
        }
    }
    return NULL;
}

/**
 * @brief Pomocná funkce pro získání počtu polí v třídě.
 *
 * @param class_def Ukazatel na uzel AST reprezentující definici třídy.
 * @return Počet polí v třídě.
 */
static int adt_field_count(const ASTNode *class_def) {
    int count = 0;
    for (const ASTNode *f = class_def->data.class_def.fields; f != NULL; f = f->next) {
        count++;
    }
    return count;
}

/**
 * @brief Pomocná funkce pro získání indexu pole v třídě podle jeho jména.
 *
 * @param class_def Ukazatel na uzel AST reprezentující definici třídy.
 * @param field_name Jméno pole, jehož index hledáme.
 * @return Index pole v třídě, pokud bylo nalezeno, jinak -1.
 */
static int adt_field_index(const ASTNode *class_def, const char *field_name) {
    int index = 1;
    for (const ASTNode *f = class_def->data.class_def.fields; f != NULL; f = f->next) {
        if (f->data.var_def.identifier != NULL &&
            strcmp(f->data.var_def.identifier, field_name) == 0) {
            return index;
        }
        index++;
    }
    return -1;
}

/**
 * @brief Pomocná funkce pro získání jména třídy z typu výrazu.
 *
 * @param type Ukazatel na strukturu TypeInfo reprezentující typ výrazu.
 * @return Jméno třídy, pokud typ odpovídá vlastnímu typu (TYPE_CUSTOM), jinak NULL.
 */
static const char *adt_class_name(const TypeInfo *type) {
    if (type != NULL && type->type_class == TYPE_CUSTOM) {
        return type->data.custom_name;
    }
    return NULL;
}

void gen_constructor(const ASTNode *expr, GenState *stt) {
    const char *name =
        adt_class_name(expr->expr_type); // F.B. ALGEBDT: získání jména třídy z typu výrazu
    const ASTNode *target =
        expr->data.func_call
            .target; // F.B. ALGEBDT: získání cílového uzlu volání funkce (konstruktoru)

    // Pokud jméno třídy není přímo dostupné, zkusíme ho získat z cílového uzlu volání funkce
    if (name == NULL && target != NULL && target->type == AST_IDENTIFIER) {
        name = target->data.identifier.name;
    }

    // Pokud jméno třídy stále není dostupné, nelze pokračovat
    const ASTNode *class_def = adt_find_class(stt, name);

    // Pokud definice třídy nebyla nalezena, nelze pokračovat
    if (class_def == NULL) {
        return;
    }

    // Argumenty jsou na zásobníku v pořadí atributů, nahoře je poslední
    const int n = adt_field_count(class_def);

    // Vytvoření pole pro uložení hodnot atributů a inicializace prvního prvku na jméno třídy
    printf("NEWARRAY GF@%%tmp1 int@%d\n", n + 1);
    printf("ARRAYSET GF@%%tmp1 int@0 string@%s\n", name);

    // Uložení hodnot atributů do pole v opačném pořadí, protože jsou na zásobníku v opačném
    // pořadí
    for (int i = n; i >= 1; i--) {
        printf("POPS GF@%%tmp2\n");
        printf("ARRAYSET GF@%%tmp1 int@%d GF@%%tmp2\n", i);
    }

    // Uložení pole do GF@%tmp1, které bude reprezentovat instanci třídy
    printf("PUSHS GF@%%tmp1\n");
}

void gen_field_access(const ASTNode *expr, GenState *stt) {
    const ASTNode *object = expr->data.member_access.object;
    const ASTNode *class_def = adt_find_class(stt, adt_class_name(object->expr_type));

    // Pokud definice třídy nebyla nalezena, nelze pokračovat
    if (class_def == NULL) {
        return;
    }
    // Získání indexu pole podle jména člena
    const int index = adt_field_index(class_def, expr->data.member_access.member_name);

    // Generování kódu pro vyhodnocení objektu (instance třídy) a uložení do GF@%tmp1
    gen_expr(object, stt);
    // Uložení objektu (pole) do GF@%tmp1
    printf("POPS GF@%%tmp1\n");
    printf("ARRAYGET GF@%%tmp2 GF@%%tmp1 int@%d\n", index);
    printf("PUSHS GF@%%tmp2\n");
}

void gen_match(const ASTNode *expr, GenState *stt) {
    // TODO
    (void)expr;
    (void)stt;
}
