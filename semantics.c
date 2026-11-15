/**
 * @file semantics.C
 * @brief Implementace sémantické analýzy překladače z hlavičkových souborů isemantics.h a
 * semantics.h
 * @author Natalie Morávková (<xmoravn00>)
 *
 * při vývoji byla použita AI (ChatGPT, github copilot) pro kontrolu kódu, pomoc s opravami
 * funkcionality kódu a vyhledávání informací ke kontrole ze specifikace IFJ26
 */

#include "semantics.h"

#include "ast.h"
#include "isemantics.h"
#include "istack.h"
#include "isymtable.h"
#include "token.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

int check_semantics(ASTNode *root) {
    SemanticContext ctx = {0};

    symtable_init(&ctx.symbols);
    symtable_init(&ctx.ext_symbols);

    ctx.scope_stack = NULL;
    ctx.current_function = NULL;
    ctx.next_var_id = 1;
    ctx.inside_lambda = false;

    return 0;
}

/* ==================================================
 * FUNKCE PRO 1. A 2. PRŮCHOD AST
 * ================================================== */

static int semantic_register_global(SemanticContext *ctx, char *name, tBtContentType type,
                                    ASTNode *declaration) {
    if (ctx == NULL || declaration == NULL || name == NULL) {
        return 99;
    }

    // Kontrola duplicity metody.
    tBtNode *existing = NULL;
    symtable_search(&ctx->symbols, name, &existing);

    if (existing != NULL) {
        return 4; // duplicitní deklarace
    }

    tBtNodeData data = {.type = type, .value = declaration};

    if (type == FUNCTION) {
        SemanticFunctionSymbol *info = malloc(sizeof *info);
        if (info == NULL) {
            return 99;
        }

        info->function = declaration;
        info->extension = NULL;
        info->state = FUNC_UNRESOLVED;
        info->next = NULL;
        data.value = info;
    }
    symtable_insert(&ctx->symbols, name, data);
    return 0;
}

int register_ext_func(SemanticContext *ctx, ASTNode *declaration, ASTNode *ext) {
    tBtNode *existing = NULL;

    if (ctx == NULL || declaration == NULL || ext == NULL || declaration->type != AST_FUNC_DEF ||
        declaration->data.func_def.name == NULL || ext->type != AST_VAR_DEF ||
        ext->expr_type == NULL) {
        return 99;
    }

    // Pouzivame primo jmeno z AST!
    symtable_search(&ctx->ext_symbols, declaration->data.func_def.name, &existing);

    SemanticFunctionSymbol *last = NULL;

    if (existing != NULL) {
        if (existing->data == NULL || existing->data->type != FUNCTION ||
            existing->data->value == NULL) {
            return 99;
        }

        // Kontrola duplicit pro stejny typ.
        SemanticFunctionSymbol *pos = existing->data->value;

        while (pos != NULL) {
            ASTNode *pos_receiver = pos->extension;

            if (pos_receiver == NULL || pos_receiver->expr_type == NULL) {
                return 99;
            }
            if (semantic_types_equal(pos_receiver->expr_type, ext->expr_type)) {
                return 4;
            }

            last = pos;
            pos = pos->next;
        }
    }

    // Vytvoreni zaznamu metody.
    SemanticFunctionSymbol *info = malloc(sizeof *info);

    if (info == NULL) {
        return 99;
    }

    info->function = declaration;
    info->extension = ext;
    info->state = FUNC_UNRESOLVED;
    info->next = NULL;

    if (existing == NULL) {
        // Prvni metoda tohoto jmena.
        tBtNodeData data = {.type = FUNCTION, .value = info};

        symtable_insert(&ctx->ext_symbols, declaration->data.func_def.name, data);
    } else {
        // Dalsi metoda stejneho jmena,
        // ale pro jiny typ.
        last->next = info;
    }
    return 0;
}

int check_globals(ASTNode *program, SemanticContext *ctx) {
    if (program == NULL || ctx == NULL || program->type != AST_PROGRAM) {
        return 99;
    }

    ASTNode *definition = program->data.program.definitions;
    while (definition != NULL) {
        int result = 0;
        switch (definition->type) {
            case AST_FUNC_DEF:
                if (definition->data.func_def.is_main) {
                }

                if (result != 0) {
                    return result;
                }
                break;
            case AST_CLASS_DEF:

                if (result != 0) {
                    return result;
                }
                break;
            case AST_TYPE_ALIAS:
                result = semantic_type_alias(definition, ctx);
                if (result != 0) {
                    return result;
                }
                break;
            case AST_EXTENSION_DEF:
                break;
            default:
                // Neočekávaný typ uzlu na úrovni programu.
                return 99;
        }
        definition = definition->next;
    }
    return 0;
}

int semantic_collect_globals(ASTNode *program, SemanticContext *ctx) {
    if (program == NULL || ctx == NULL || program->type != AST_PROGRAM) {
        return 99;
    }

    ASTNode *definition = program->data.program.definitions;

    bool found_main = false;

    while (definition != NULL) {
        // Zpracování jedné globální deklarace.
        int result = 0;
        switch (definition->type) {
            case AST_FUNC_DEF:
                if (definition->data.func_def.is_main) {
                    if (found_main) {
                        return 4; // Opakovaná definice @main
                    }
                    found_main = true;
                }
                result = semantic_register_global(ctx, definition->data.func_def.name, FUNCTION,
                                                  definition);
                if (result != 0) {
                    return result;
                }
                break;
            case AST_CLASS_DEF:
                result = semantic_register_global(ctx, definition->data.class_def.name, CLASS,
                                                  definition);
                if (result != 0) {
                    return result;
                }
                break;
            case AST_TYPE_ALIAS:
                result = semantic_register_global(ctx, definition->data.type_alias.name, ALIAS,
                                                  definition);
                if (result != 0) {
                    return result;
                }
                break;
            case AST_EXTENSION_DEF:
                break;
            default:
                // Neočekávaný typ uzlu na úrovni programu.
                return 99;
        }
        definition = definition->next;
    }
    definition = program->data.program.definitions;

    while (definition != NULL) {
        if (definition->type == AST_EXTENSION_DEF) {
            ASTNode *pos = definition->data.extension_def.methods;
            ASTNode *reciever_node = definition->data.extension_def.receiver;
            TypeInfo *resolved = NULL;
            // dodělat, spousta věcí co musí sémantika řešit
            int res_alias = semantic_resolve_type_alias(

                reciever_node->data.var_def.declared_type, ctx, &resolved);
            if (res_alias != 0) {
                return res_alias;
            }
            ast_set_type(reciever_node, resolved);

            while (pos != NULL) {
                if (pos->type != AST_FUNC_DEF) {
                    return 99;
                }

                int result = register_ext_func(ctx, pos, definition->data.extension_def.receiver);
                if (result != 0) {
                    return result;
                }
                pos = pos->next;
            }
        }
        definition = definition->next;
    }

    if (found_main == false) { // v programu chybí funkce main
        return 3;
    }
    return 0;
}

int semantic_check_program(ASTNode *program, SemanticContext *ctx) {
}

/* ==================================================
 * POMOCNÉ FUNKCE PRO PRŮCHOD AST
 * ================================================== */

int semantic_visit(ASTNode **node, SemanticContext *ctx, const TypeInfo *expected_type) {
    switch ((*node)->type) {
        case AST_ARG_LIST: // hotovo
            // semantic_literal(...);
            break;

        case AST_ASSIGNMENT: // hotova
            // semantic_identifier(...);
            break;

        case AST_BINARY_OP: // hotova
            // semantic_binary_op(*node, ctx);
            break;

        case AST_BLOCK: // hotova
            // semantic_func_call(...);
            break;

        case AST_CASE_BRANCH:
            break;
        case AST_CLASS_DEF: // hotova
            break;
        case AST_EXTENSION_DEF:
            break;
        case AST_FOR: // hotovo
            break;
        case AST_FUNC_CALL:
            break;
        case AST_FUNC_DEF: // hotovo
            break;
        case AST_IDENTIFIER:
            break;
        case AST_IF_EXPR: // asi hotovo
            break;
        case AST_LAMBDA:
            break;
        case AST_LIST_LITERAL: // hotova
            break;
        case AST_LITERAL:
            // semantic_literal(*node);
            break;
        case AST_MATCH_EXPR:
            break;
        case AST_MEMBER_ACCESS: // asi hotovo
            break;
        case AST_PARAM_LIST: // hotovo
            break;
        case AST_PATTERN:
            break;
        case AST_PROGRAM:
            break;
        case AST_RANGE: // hotova
            break;

        case AST_RETURN: // hotova
            break;
        case AST_TYPE_ALIAS: // hotova
            break;
        case AST_UNARY_OP: // hotova
            // semantic_unary_op(*node, ctx);
            break;
        case AST_VAR_DEF: // hotova
            // semantic_var_def();
            break;
        case AST_WHILE: // hotovo
    }
}

// hotová
int semantic_check_block(ASTNode *block, SemanticContext *ctx, const TypeInfo *expected_type) {
    // úvodní kontroly
    if (block == NULL || ctx == NULL || block->type != AST_BLOCK || ctx->scope_stack == NULL) {
        return 99;
    }

    if (block->data.block.statements == NULL) {
        return 99;
    }

    // vytvoření nového scope
    tBtNode *new_scope = NULL;
    symtable_init(&new_scope);
    symtable_push_scope(&new_scope, ctx->scope_stack);

    // projde seznam příkazů v bloku, každý příkaz zpracuje
    ASTNode **act_statement = &block->data.block.statements;
    while ((*act_statement)->next != NULL) {
        int res = semantic_visit(act_statement, ctx, NULL);
        if (res != 0) {
            symtable_pop_scope(ctx->scope_stack);
            return res;
        }
        act_statement = &(*act_statement)->next;
    }

    // projde poslední příkaz
    int res = semantic_visit(act_statement, ctx, expected_type);
    if (res != 0) {
        symtable_pop_scope(ctx->scope_stack);
        return res;
    }

    // podle posledního příkazu určí typ bloku
    TypeInfo *block_type = NULL;
    if (semantic_always_returns(block)) {
        block_type = type_create_basic(TYPE_NOTHING);
    } else {
        switch ((*act_statement)->type) {
            case AST_VAR_DEF:
            case AST_ASSIGNMENT:
            case AST_WHILE:
            case AST_FOR:
                block_type = type_create_basic(TYPE_UNIT);
                break;
            default:
                block_type = type_info_clone((*act_statement)->expr_type);
        }
    }
    if (block_type == NULL) {
        symtable_pop_scope(ctx->scope_stack);
        return 99;
    }

    // doplní expr_type
    ast_set_type(block, block_type);

    // uvolní scope co si na začátku vytvořil
    symtable_pop_scope(ctx->scope_stack);
    return 0;
}

/* ==================================================
 * FUNKCE PRO VÝRAZOVÉ UZLY AST
 * ================================================== */
static int semantic_build_function_type(const ASTNode *param_group, const TypeInfo *return_type,
                                        TypeInfo **result) {
    if (return_type == NULL || result == NULL) {
        return 99;
    }

    *result = NULL;

    // Uz zadna dalsi skupina parametru.
    if (param_group == NULL) {
        *result = type_info_clone(return_type);
        return *result != NULL ? 0 : 99;
    }

    if (param_group->type != AST_PARAM_LIST) {
        return 99;
    }

    // Nejdrive vytvorime typ pro nasledujici skupiny.
    TypeInfo *inner_type = NULL;

    int error = semantic_build_function_type(param_group->next, return_type, &inner_type);

    if (error != 0) {
        return error;
    }

    // Sestavime seznam typu parametru aktualni skupiny.
    TypeInfo *params = NULL;

    for (const ASTNode *param = param_group->data.list.items; param != NULL; param = param->next) {

        if (param->type != AST_VAR_DEF || param->expr_type == NULL) {
            type_info_free(params);
            type_info_free(inner_type);
            return 99;
        }

        TypeInfo *copy = type_info_clone(param->expr_type);

        if (copy == NULL) {
            type_info_free(params);
            type_info_free(inner_type);
            return 99;
        }

        params = type_append(params, copy);
    }

    // Aktualni skupina parametru vraci inner_type.
    *result = type_create_function(params, inner_type);

    if (*result == NULL) {
        type_info_free(params);
        type_info_free(inner_type);
        return 99;
    }

    return 0;
}

int semantic_ensure_func_type(SemanticFunctionSymbol *func_sym, SemanticContext *ctx) {
    if (func_sym == NULL || ctx == NULL || func_sym->function == NULL ||
        func_sym->function->type != AST_FUNC_DEF) {
        return 99;
    }

    if (func_sym->state == FUNC_CHECKED || func_sym->state == FUNC_RESOLVED) {
        return func_sym->function->expr_type != NULL ? 0 : 99;
    }

    if (func_sym->state == FUNC_RESOLVING) {
        return 6;
    }

    return semantic_func_def(func_sym, ctx);
}

int semantic_func_def(SemanticFunctionSymbol *func_sym, SemanticContext *ctx) {
    if (func_sym == NULL || ctx == NULL || func_sym->function == NULL ||
        func_sym->function->type != AST_FUNC_DEF) {
        return 99;
    }

    if (func_sym->state == FUNC_CHECKED) {
        return 0;
    }

    if (func_sym->state == FUNC_RESOLVING) {
        return 6;
    }

    ASTNode *function = func_sym->function;
    if (func_sym->state == FUNC_RESOLVED && function->expr_type == NULL) {
        return 99;
    }

    if (function->data.func_def.body == NULL || function->data.func_def.body->type != AST_BLOCK) {
        return 99;
    }

    // přepnutí zásobníku
    tStack *saved_stack = ctx->scope_stack;
    ASTNode *saved_function = ctx->current_function;
    bool saved_lambda = ctx->inside_lambda;

    tStack function_stack = {0};
    ctx->scope_stack = &function_stack;
    ctx->current_function = function;
    ctx->inside_lambda = false;

    func_sym->state = FUNC_RESOLVING;

    // moje

    int res = 0;
    TypeInfo *return_type = NULL;
    TypeInfo *function_type = NULL;

    // 1. Vytvoreni zakladniho scope funkce.
    tBtNode *scope = NULL;
    symtable_init(&scope);

    symtable_push_scope(&scope, &function_stack);

    if (function_stack.top == NULL) {
        res = 99;
        goto cleanup;
    }

    // 2. Extension receiver.
    if (func_sym->extension != NULL) {
        ASTNode *receiver = func_sym->extension;

        if (receiver->type != AST_VAR_DEF || receiver->data.var_def.identifier == NULL ||
            receiver->data.var_def.declared_type == NULL) {
            res = 99;
            goto cleanup;
        }

        // Typ receiveru musi byt vyreseny.
        if (receiver->expr_type == NULL) {
            TypeInfo *resolved = NULL;

            res = semantic_resolve_type_alias(receiver->data.var_def.declared_type, ctx, &resolved);

            if (res != 0) {
                type_info_free(resolved);
                goto cleanup;
            }

            if (resolved == NULL) {
                res = 99;
                goto cleanup;
            }

            ast_set_type(receiver, resolved);
        }

        // ID se prideluje jen jednou,
        // receiver muze sdilet vice metod.
        if (receiver->data.var_def.var_id == 0) {
            receiver->data.var_def.var_id = ctx->next_var_id++;
        }

        tBtNodeData receiver_data = {.type = VARIABLE, .value = receiver};

        symtable_insert(&function_stack.top->scope, receiver->data.var_def.identifier,
                        receiver_data);
    }

    // 3. Registrace vsech parametru.
    for (ASTNode *group = function->data.func_def.param_groups; group != NULL;
         group = group->next) {

        if (group->type != AST_PARAM_LIST) {
            res = 99;
            goto cleanup;
        }

        res = semantic_register_param_list(group, ctx);

        if (res != 0) {
            goto cleanup;
        }
    }

    // 4. Explicitni navratovy typ.
    if (function->data.func_def.return_type != NULL) {

        res = semantic_resolve_type_alias(function->data.func_def.return_type, ctx, &return_type);

        if (res != 0) {
            goto cleanup;
        }

        if (return_type == NULL) {
            res = 99;
            goto cleanup;
        }

        // Signaturu vytvorime PRED kontrolou tela,
        // aby bylo mozne rekurzivni volani.
        res = semantic_build_function_type(function->data.func_def.param_groups, return_type,
                                           &function_type);

        if (res != 0) {
            goto cleanup;
        }

        ast_set_type(function, function_type);
        function_type = NULL;

        func_sym->state = FUNC_RESOLVED;
    }

    // 5. Analyza tela.
    // U funkce vracejici Unit se hodnota
    // tela vyhodnoti a zahodi.
    const TypeInfo *expected_body = NULL;

    if (return_type != NULL && return_type->type_class != TYPE_UNIT) {
        expected_body = return_type;
    }

    res = semantic_check_block(function->data.func_def.body, ctx, expected_body);

    if (res != 0) {
        goto cleanup;
    }

    ASTNode *body = function->data.func_def.body;

    if (body->expr_type == NULL) {
        res = 99;
        goto cleanup;
    }

    // 6. Navratovy typ funkce.
    if (return_type == NULL) {

        // Funkce bez explicitniho navratoveho typu:
        // odvodime ho z vysledku tela.
        return_type = type_info_clone(body->expr_type);

        if (return_type == NULL) {
            res = 99;
            goto cleanup;
        }

        res = semantic_build_function_type(function->data.func_def.param_groups, return_type,
                                           &function_type);

        if (res != 0) {
            goto cleanup;
        }

        ast_set_type(function, function_type);
        function_type = NULL;

    } else if (return_type->type_class != TYPE_UNIT) {

        // Explicitni navratovy typ:
        // vysledek tela musi byt kompatibilni.
        if (!semantic_types_compatible(return_type, body->expr_type)) {
            res = 6;
            goto cleanup;
        }

        // Pripadna konverze Int -> Double
        // na vysledku tela.
        res = semantic_check_conversion(body, return_type);

        if (res != 0) {
            goto cleanup;
        }
    }

    // 7. Funkce je kompletne zkontrolovana.
    func_sym->state = FUNC_CHECKED;

cleanup:
    // Uvolneni pripadnych docasnych typu.
    type_info_free(return_type);
    type_info_free(function_type);

    // Pri chybe se analyza prerusila.
    if (res != 0) {
        func_sym->state = FUNC_UNRESOLVED;
    }

    // Uvolneni vsech scope teto funkce.
    while (function_stack.top != NULL) {
        symtable_pop_scope(&function_stack);
    }

    // Obnoveni predchoziho kontextu.
    ctx->scope_stack = saved_stack;
    ctx->current_function = saved_function;
    ctx->inside_lambda = saved_lambda;

    return res;
}

int semantic_literal(ASTNode *node) {
    if (node == NULL || node->type != AST_LITERAL) {
        return 99;
    }
    TypeClass type;
    switch (node->data.literal.kind) {
        case LIT_INT:
            type = TYPE_INT;
            break;
        case LIT_DOUBLE:
            type = TYPE_DOUBLE;
            break;
        case LIT_STRING:
            type = TYPE_STRING;
            break;
        case LIT_BOOL:
            type = TYPE_BOOLEAN;
            break;
        case LIT_UNIT:
            type = TYPE_UNIT;
            break;
        default:
            return 99;
    }

    TypeInfo *result = type_create_basic(type);

    if (result == NULL) {
        return 99;
    }

    ast_set_type(node, result);

    return 0;
}

int semantic_identifier(ASTNode **node, SemanticContext *ctx) {
    if (node == NULL || *node == NULL || ctx == NULL || (*node)->type != AST_IDENTIFIER ||
        (*node)->data.identifier.name == NULL) {
        return 99;
    }
    tBtNode *var = semantic_find_symbol(ctx->scope_stack, (*node)->data.identifier.name, ctx);
    if (var == NULL) {
        return 3;
    }
    if (var->data->type != FUNCTION) {
        ASTNode *found_node = (ASTNode *)var->data->value;
        TypeInfo *type = NULL;
        int var_id = 0;
        if (found_node->type == AST_VAR_DEF) {
            type = found_node->expr_type;
            var_id = found_node->data.var_def.var_id;

        } else if (found_node->type == AST_PATTERN) {
            if (found_node->data.pattern.pat_type != PATTERN_TYPE_BINDING) {
                return 99;
            }

            type = found_node->expr_type;
            var_id = found_node->data.pattern.var_id;
        } else if (found_node->type == AST_CLASS_DEF || found_node->type == AST_TYPE_ALIAS) {
            return 6;
        } else {
            return 99;
        }
        TypeInfo *clone_type = type_info_clone(type);
        if (clone_type == NULL) {
            return 99;
        }
        ast_set_type(*node, clone_type);
        (*node)->data.identifier.var_id = var_id;
    } else {
        SemanticFunctionSymbol *found = (SemanticFunctionSymbol *)var->data->value;
        ASTNode *found_node = found->function;
        if (found_node->type != AST_FUNC_DEF) {
            return 99;
        }

        (*node)->data.identifier.var_id = -1;

        if (found_node->data.func_def.param_groups == NULL) {
            int result = ast_wrap_in_call(node);

            if (result != 0) {
                return result;
            }
            int res = semantic_func_call(*node, ctx, NULL);
            if (res != 0) {
                return res;
            }
        } else {
            int res = semantic_ensure_func_type(found, ctx);
            if (res != 0) {
                return res;
            }

            if (found_node->expr_type == NULL) {
                return 99;
            }

            TypeInfo *clone_type = type_info_clone(found_node->expr_type);

            if (clone_type == NULL) {
                return 99;
            }

            ast_set_type(*node, clone_type);
        }
    }
    return 0;
}

// hotová
int semantic_binary_op(ASTNode *node, SemanticContext *ctx) {
    if (node == NULL || ctx == NULL || node->type != AST_BINARY_OP ||
        node->data.bin_op.left == NULL || node->data.bin_op.right == NULL) {
        return 99;
    }

    int op = node->data.bin_op.operator_token;

    int res = semantic_visit(&node->data.bin_op.left, ctx, NULL);
    if (res != 0) {
        return res;
    }
    res = semantic_visit(&node->data.bin_op.right, ctx, NULL);
    if (res != 0) {
        return res;
    }

    TypeInfo *left = node->data.bin_op.left->expr_type;
    TypeInfo *right = node->data.bin_op.right->expr_type;

    if (left == NULL || right == NULL) {
        return 99;
    }
    TypeInfo *res_type;

    if (op == TOK_PLUS || op == TOK_MINUS || op == TOK_MUL || op == TOK_DIV) {
        if (op == TOK_PLUS && left->type_class == TYPE_STRING && right->type_class == TYPE_STRING) {
            res_type = type_create_basic(TYPE_STRING);
        } else if ((left->type_class == TYPE_INT && right->type_class == TYPE_INT) ||
                   (left->type_class == TYPE_DOUBLE && right->type_class == TYPE_DOUBLE)) {
            res_type = type_create_basic(left->type_class);
        } else if (left->type_class == TYPE_INT && right->type_class == TYPE_DOUBLE) {
            node->data.bin_op.left->to_double = true;
            res_type = type_create_basic(TYPE_DOUBLE);

        } else if (left->type_class == TYPE_DOUBLE && right->type_class == TYPE_INT) {
            node->data.bin_op.right->to_double = true;
            res_type = type_create_basic(TYPE_DOUBLE);

        } else {
            return 6;
        }

    } else if (op == TOK_LESSTHEN || op == TOK_LESSEQ || op == TOK_GREATERTHEN ||
               op == TOK_GREATEREQ) {
        if (((left->type_class == TYPE_INT || left->type_class == TYPE_DOUBLE) &&
             (right->type_class == TYPE_INT || right->type_class == TYPE_DOUBLE)) ||
            (left->type_class == TYPE_STRING && right->type_class == TYPE_STRING)) {
            res_type = type_create_basic(TYPE_BOOLEAN);
            if (left->type_class == TYPE_INT && right->type_class == TYPE_DOUBLE) {
                node->data.bin_op.left->to_double = true;
            } else if (left->type_class == TYPE_DOUBLE && right->type_class == TYPE_INT) {
                node->data.bin_op.right->to_double = true;
            }
        } else {
            return 6;
        }

    } else if (op == TOK_EQ || op == TOK_NOTEQ) {

        if (semantic_types_equal(left, right)) {

            res_type = type_create_basic(TYPE_BOOLEAN);

        } else if (left->type_class == TYPE_INT && right->type_class == TYPE_DOUBLE) {
            node->data.bin_op.left->to_double = true;
            res_type = type_create_basic(TYPE_BOOLEAN);
        } else if (left->type_class == TYPE_DOUBLE && right->type_class == TYPE_INT) {
            node->data.bin_op.right->to_double = true;
            res_type = type_create_basic(TYPE_BOOLEAN);

        } else {
            return 6;
        }
    } else if (op == TOK_AND || op == TOK_OR) {
        if (left->type_class == TYPE_BOOLEAN && right->type_class == TYPE_BOOLEAN) {

            res_type = type_create_basic(TYPE_BOOLEAN);
        } else {
            return 6;
        }
    } else if (op == TOK_CONCAT) {
        if (left->type_class == TYPE_LIST && right->type_class == TYPE_LIST) {
            if (!semantic_types_equal(left, right)) {
                return 6;
            }
            res_type = type_info_clone(left);
        } else {
            return 6;
        }
    } else {
        return 99;
    }
    if (res_type == NULL) {
        return 99;
    }
    ast_set_type(node, res_type);

    return 0;
}

// hotová
int semantic_unary_op(ASTNode *node, SemanticContext *ctx) {
    if (node == NULL || ctx == NULL || node->type != AST_UNARY_OP ||
        node->data.unary_op.operand == NULL) {
        return 99;
    }

    int res = semantic_visit(&node->data.unary_op.operand, ctx, NULL);
    if (res != 0) {
        return res;
    }

    if (node->data.unary_op.operand->expr_type == NULL) {
        return 99;
    }

    switch (node->data.unary_op.operand->expr_type->type_class) {
        case TYPE_INT:
        case TYPE_DOUBLE:
            if (node->data.unary_op.operator_token != TOK_PLUS &&
                node->data.unary_op.operator_token != TOK_MINUS) {
                return 6;
            }
            break;
        case TYPE_BOOLEAN:
            if (node->data.unary_op.operator_token != TOK_NOT) {
                return 6;
            }
            break;
        default:
            return 6;
    }
    TypeInfo *res_type = type_create_basic(node->data.unary_op.operand->expr_type->type_class);
    if (res_type == NULL) {
        return 99;
    }
    ast_set_type(node, res_type);
    return 0;
}

int semantic_func_call(ASTNode *node, SemanticContext *ctx, const TypeInfo *expected_type);

static int semantic_extension_member(ASTNode *node, SemanticContext *ctx, TypeInfo **result,
                                     SemanticFunctionSymbol **matched_method) {
    if (node == NULL || ctx == NULL || result == NULL || matched_method == NULL ||
        node->type != AST_MEMBER_ACCESS || node->data.member_access.object == NULL ||
        node->data.member_access.member_name == NULL) {
        return 99;
    }

    *result = NULL;
    *matched_method = NULL;

    char *name = node->data.member_access.member_name;
    const TypeInfo *object_type = node->data.member_access.object->expr_type;

    if (object_type == NULL) {
        return 99;
    }

    tBtNode *symbol = NULL;
    symtable_search(&ctx->ext_symbols, name, &symbol);

    if (symbol == NULL) {
        return 3;
    }

    if (symbol->data == NULL || symbol->data->type != FUNCTION || symbol->data->value == NULL) {
        return 99;
    }

    for (SemanticFunctionSymbol *method = symbol->data->value; method != NULL;
         method = method->next) {

        const ASTNode *receiver = method->extension;
        ASTNode *function = method->function;

        if (receiver == NULL || receiver->type != AST_VAR_DEF || function == NULL ||
            function->type != AST_FUNC_DEF) {
            return 99;
        }

        // Zjisteni typu prijemce.
        const TypeInfo *receiver_type = receiver->expr_type;
        TypeInfo *temporary_type = NULL;

        if (receiver_type == NULL) {
            if (receiver->data.var_def.declared_type == NULL) {
                return 99;
            }

            int error = semantic_resolve_type_alias(receiver->data.var_def.declared_type, ctx,
                                                    &temporary_type);

            if (error != 0) {
                return error;
            }

            if (temporary_type == NULL) {
                return 99;
            }

            receiver_type = temporary_type;
        }

        bool matches = semantic_types_equal(object_type, receiver_type);

        type_info_free(temporary_type);

        if (!matches) {
            continue;
        }

        // Metoda odpovida typu objektu.
        // Zajistime, ze ma vyresenou signaturu.
        int error = semantic_ensure_func_type(method, ctx);
        if (error != 0) {
            return error;
        }

        if (function->expr_type == NULL) {
            return 99;
        }

        // Kopirujeme CELOU signaturu funkce.
        *result = type_info_clone(function->expr_type);

        if (*result == NULL) {
            return 99;
        }

        *matched_method = method;

        node->data.member_access.member_kind = MEMBER_EXTENSION;

        return 0;
    }

    return 3;
}

static int is_mem_acc_allowed(ASTNode *node, SemanticContext *ctx, TypeInfo **result,
                              SemanticFunctionSymbol **matched_method) {
    if (node == NULL || ctx == NULL || result == NULL || matched_method == NULL ||
        node->type != AST_MEMBER_ACCESS || node->data.member_access.object == NULL ||
        node->data.member_access.member_name == NULL) {
        return 99;
    }

    *result = NULL;
    *matched_method = NULL;

    ASTNode *object = node->data.member_access.object;
    const char *name = node->data.member_access.member_name;
    const TypeInfo *object_type = object->expr_type;

    if (object->type == AST_RANGE) {
        if (strcmp(name, "toList") != 0) {
            return 3;
        }

        TypeInfo *element = type_create_basic(TYPE_INT);
        if (element == NULL) {
            return 99;
        }

        *result = type_create_list(element);
        if (*result == NULL) {
            type_info_free(element);
            return 99;
        }

        node->data.member_access.member_kind = MEMBER_RANGE_TOLIST;
        return 0;
    }

    if (object_type == NULL) {
        return 99;
    }

    TypeClass receiver = object_type->type_class;
    if (receiver == TYPE_CUSTOM) {
        tBtNode *symbol = NULL;
        symtable_search(&ctx->symbols, object_type->data.custom_name, &symbol);
        if (symbol == NULL) {
            return 3;
        }
        if (symbol->data == NULL || symbol->data->type != CLASS) {
            return 99;
        }

        const ASTNode *class_def = symbol->data->value;
        if (class_def == NULL || class_def->type != AST_CLASS_DEF) {
            return 99;
        }

        for (const ASTNode *field = class_def->data.class_def.fields; field != NULL;
             field = field->next) {
            if (field->type != AST_VAR_DEF || field->data.var_def.identifier == NULL) {
                return 99;
            }
            if (strcmp(name, field->data.var_def.identifier) != 0) {
                continue;
            }

            if (field->expr_type != NULL) {
                *result = type_info_clone(field->expr_type);
            } else {
                int error =
                    semantic_resolve_type_alias(field->data.var_def.declared_type, ctx, result);
                if (error != 0) {
                    return error;
                }
            }
            if (*result == NULL) {
                return 99;
            }

            node->data.member_access.member_kind = MEMBER_FIELD;
            return 0;
        }
    }

    MemberKind kind = MEMBER_VALUE_BUILTIN;
    if (receiver == TYPE_LIST) {
        kind = MEMBER_LIST_BUILTIN;
    }

    if (strcmp(name, "toString") == 0) {
        *result = type_create_basic(TYPE_STRING);
    } else if ((receiver == TYPE_DOUBLE && strcmp(name, "toInt") == 0) ||
               (receiver == TYPE_STRING && strcmp(name, "length") == 0)) {
        *result = type_create_basic(TYPE_INT);
    } else if (receiver == TYPE_DOUBLE && strcmp(name, "floor") == 0) {
        *result = type_create_basic(TYPE_DOUBLE);
    } else if (receiver == TYPE_LIST && strcmp(name, "head") == 0) {
        *result = type_info_clone(object_type->data.list_element_type);
    } else if (receiver == TYPE_LIST && strcmp(name, "tail") == 0) {
        *result = type_info_clone(object_type);
    } else {
        return semantic_extension_member(node, ctx, result, matched_method);
    }

    if (*result == NULL) {
        return 99;
    }

    node->data.member_access.member_kind = kind;
    return 0;
}

// asi hotovo
int semantic_member_access(ASTNode **node, SemanticContext *ctx) {
    if (node == NULL || *node == NULL || ctx == NULL || (*node)->type != AST_MEMBER_ACCESS) {
        return 99;
    }

    ASTNode *member = *node;

    // Pokud uz byl member access vyresen,
    // neprovadime semantickou analyzu znovu.
    if (member->data.member_access.member_kind != MEMBER_UNRESOLVED) {
        return member->expr_type != NULL ? 0 : 99;
    }

    ASTNode **object = &member->data.member_access.object;

    if (*object == NULL || member->data.member_access.member_name == NULL) {
        return 99;
    }

    int error;

    // RANGE nema expr_type a vyzaduje zvlastni zpracovani.
    if ((*object)->type == AST_RANGE) {
        error = semantic_range(*object, ctx);
    } else {
        error = semantic_visit(object, ctx, NULL);
    }

    if (error != 0) {
        return error;
    }

    TypeInfo *result = NULL;
    SemanticFunctionSymbol *method = NULL;

    error = is_mem_acc_allowed(member, ctx, &result, &method);

    if (error != 0) {
        type_info_free(result);
        return error;
    }

    if (result == NULL) {
        return 99;
    }

    // Atribut tridy nebo vestaveny clen.
    if (member->data.member_access.member_kind != MEMBER_EXTENSION) {
        ast_set_type(member, result);
        return 0;
    }

    // Extension musi mit prirazenou funkci.
    if (method == NULL || method->function == NULL) {
        type_info_free(result);
        return 99;
    }

    // Samostatny member access lze pouzit jen
    // pro extension metodu bez seznamu parametru.
    if (method->function->data.func_def.param_groups != NULL) {
        type_info_free(result);
        return 5;
    }

    // U metody bez seznamu parametru je
    // function->expr_type primo navratovy typ.
    TypeInfo *call_type = type_info_clone(result);

    if (call_type == NULL) {
        type_info_free(result);
        return 99;
    }

    // Prepis x.twice na FUNC_CALL.
    error = ast_wrap_in_call(node);

    if (error != 0) {
        type_info_free(result);
        type_info_free(call_type);
        return error;
    }

    ASTNode *call = *node;
    ASTNode *target = call->data.func_call.target;

    // Typ metody (bez seznamu parametru).
    ast_set_type(target, result);

    // Typ celeho volani.
    ast_set_type(call, call_type);

    call->data.func_call.call_kind = CALL_EXTENSION;
    call->data.func_call.own_groups = 0;

    return 0;
}

// hotova
int semantic_list_literal(ASTNode *node, SemanticContext *ctx, const TypeInfo *expected_type) {
    if (node == NULL || ctx == NULL || node->type != AST_LIST_LITERAL) {
        return 99;
    }

    if (expected_type != NULL && expected_type->type_class != TYPE_LIST) {
        return 6;
    }

    int res = 0;
    TypeInfo *type = NULL;
    if (node->data.list.items ==
        NULL) { // pokud je seznam prázdný, bude mít typ podle expected_type
        if (expected_type == NULL) {
            return 6;
        }
        type = type_info_clone(expected_type);
        if (type == NULL) {
            return 99;
        }
    } else {
        // projde všechny prvky seznamu
        ASTNode **actual = &node->data.list.items;
        TypeInfo *list_type = NULL;
        while (*actual != NULL) {
            // zpracuje prvek seznamu
            res = semantic_visit(
                actual, ctx, expected_type != NULL ? expected_type->data.list_element_type : NULL);
            if (res != 0) {
                if (list_type != NULL) {
                    type_info_free(list_type);
                }
                return res;
            }

            // pokud není zadaný expected type, určí si ho podle společného typu aktuálního a
            // předchozího prvku(pokud předchozí prvek není, je to typ pouze tohoto prvku)
            if (expected_type == NULL) {
                if (list_type == NULL) {
                    list_type = type_info_clone((*actual)->expr_type);

                    if (list_type == NULL) {
                        return 99;
                    }
                } else {
                    TypeInfo *comm_type = NULL;
                    res = semantic_common_type((*actual)->expr_type, list_type, &comm_type);
                    if (res != 0) {
                        type_info_free(list_type);
                        return res;
                    }
                    if (comm_type == NULL) {
                        type_info_free(list_type);
                        return 99;
                    }

                    type_info_free(list_type);
                    list_type = comm_type;
                }
            } else { // expected_type je zadán, stačí ověřit kompatibilitu prvku s ním
                if (!semantic_types_compatible(expected_type->data.list_element_type,
                                               (*actual)->expr_type)) {
                    return 6;
                }
            }
            actual = &(*actual)->next;
        }

        // doplní si typ, který by měl seznam mít
        if (expected_type != NULL) {
            type = type_info_clone(expected_type);
            if (type == NULL) {
                return 99;
            }

        } else {
            type = type_create_list(list_type);
            if (type == NULL) {
                type_info_free(list_type);
                return 99;
            }
        }

        // znovu projde seznam prvků, u každého zkontroluje kompatibilitu s type a případně nastaví
        // to_double
        actual = &node->data.list.items;
        while (*actual != NULL) {

            if (!semantic_types_compatible(type->data.list_element_type, (*actual)->expr_type)) {
                type_info_free(type);
                return 6;
            }
            res = semantic_check_conversion((*actual), type->data.list_element_type);
            if (res != 0) {
                type_info_free(type);
                return res;
            }
            actual = &(*actual)->next;
        }
    }

    // nastavý expr_type
    ast_set_type(node, type);
    return 0;
}

// hotovo, zkontrolovat si chybové kódy
int semantic_assignment(ASTNode *node, SemanticContext *ctx) {
    if (node == NULL || ctx == NULL) {
        return 99;
    }

    // najde definici proměnné na levé straně přiřazení, pokud není, chyba
    tBtNode *definition = semantic_find_symbol(ctx->scope_stack, node->data.assignment.target, ctx);
    if (definition == NULL || definition->data->type != VARIABLE) {
        return 3; // nedefinovaná proměnná
    }

    ASTNode *tar_var = (ASTNode *)definition->data->value;
    if (tar_var->type != AST_VAR_DEF) {
        return 3; // na levé straně není proměnná
    }

    if (tar_var->data.var_def.is_mutable != true) {
        return 7; // proměnná není modifikovatelná, nelze přiřadit
    }

    // yřeší pravou stranu přiazení
    int res = semantic_visit(&node->data.assignment.value, ctx, tar_var->expr_type);
    if (res != 0) {
        return res;
    }

    // kontrola kompatibility typů
    if (!semantic_types_compatible(tar_var->expr_type, node->data.assignment.value->expr_type)) {
        return 6;
    }
    // kontrola a případné nastavení konverze to_double
    res = semantic_check_conversion(node->data.assignment.value, tar_var->expr_type);
    if (res != 0) {
        return res;
    }

    // nastavení var_id podle proměnné a expr_type (unit - přiřazení typ nemá)
    node->data.assignment.var_id = tar_var->data.var_def.var_id;
    TypeInfo *type = type_create_basic(TYPE_UNIT);
    if (type == NULL) {
        return 99;
    }

    ast_set_type(node, type);
    return 0;
}

/* ==================================================
 * DEKLARACE
 * ================================================== */

int semantic_var_def(tStack *scope, ASTNode *node, SemanticContext *ctx, bool is_param) {

    if (node == NULL || ctx == NULL || node->type != AST_VAR_DEF ||
        (!is_param && node->data.var_def.initializer == NULL) ||
        (is_param && node->data.var_def.initializer != NULL) || scope == NULL ||
        stack_top(scope) == NULL) {
        return 99;
    }

    tBtNode *exists = NULL;
    symtable_search(&stack_top(scope)->scope, node->data.var_def.identifier, &exists);
    if (exists != NULL) {
        // nejake uvolnovani
        return 4;
    }
    if (node->data.var_def.declared_type != NULL) {

        TypeInfo *exp_type = NULL;
        int resolved_res =
            semantic_resolve_type_alias(node->data.var_def.declared_type, ctx, &exp_type);
        if (resolved_res != 0) {
            if (exp_type != NULL) {
                type_info_free(exp_type);
            }
            return resolved_res;
        }
        if (is_param) {
            ast_set_type(node, exp_type);
        } else {
            int result = semantic_visit(&node->data.var_def.initializer, ctx, exp_type);

            if (result != 0) {
                if (exp_type != NULL) {
                    type_info_free(exp_type);
                }
                return result;
            }

            TypeInfo *act_type = node->data.var_def.initializer->expr_type;

            if (!semantic_types_compatible(exp_type, act_type)) {
                type_info_free(exp_type);
                return 6;
            }
            result = semantic_check_conversion(node->data.var_def.initializer, exp_type);

            if (result != 0) {
                type_info_free(exp_type);
                return result;
            }

            ast_set_type(node, exp_type);
        }

    } else {
        if (is_param) {
            return 10;
        }
        int result = semantic_visit(&node->data.var_def.initializer, ctx, NULL);

        if (result != 0) {
            return result;
        }

        TypeInfo *act_type = type_info_clone(node->data.var_def.initializer->expr_type);
        if (act_type == NULL) {
            return 99;
        }
        ast_set_type(node, act_type);
    }
    node->data.var_def.var_id = ctx->next_var_id;
    ctx->next_var_id++;

    tBtNodeData node_symtab = {.value = node, .type = VARIABLE};

    symtable_insert(&stack_top(scope)->scope, node->data.var_def.identifier, node_symtab);

    return 0;
}

// hotova
int semantic_class_def(ASTNode *node, SemanticContext *ctx) {
    if (node == NULL || ctx == NULL || node->type != AST_CLASS_DEF) {
        return 99;
    }

    ASTNode *field = node->data.class_def.fields;
    while (field != NULL) {
        // Kontrola spravne struktury atributu.
        if (field->type != AST_VAR_DEF || field->data.var_def.declared_type == NULL ||
            field->data.var_def.initializer != NULL || field->data.var_def.is_mutable) {
            return 99;
        }

        // Kontrola duplicitnich nazvu atributu.
        ASTNode *previous = node->data.class_def.fields;

        while (previous != field) {
            if (previous->data.var_def.identifier != NULL &&
                field->data.var_def.identifier != NULL) {
                if (strcmp(previous->data.var_def.identifier, field->data.var_def.identifier) ==
                    0) {
                    return 4;
                }
            }

            previous = previous->next;
        }

        // Vyreseni deklarovaneho typu atributu.
        TypeInfo *resolved_type = NULL;

        int res =
            semantic_resolve_type_alias(field->data.var_def.declared_type, ctx, &resolved_type);

        if (res != 0) {
            if (resolved_type != NULL) {
                type_info_free(resolved_type);
            }
            return res;
        }

        if (resolved_type == NULL) {
            return 99;
        }

        // Ulozeni vyreseneho typu do AST.
        ast_set_type(field, resolved_type);

        // Atribut tridy neni lokalni promenna.
        field->data.var_def.var_id = -1;
        field = field->next;
    }

    return 0;
}

// hotovo
int semantic_type_alias(ASTNode *node, SemanticContext *ctx) {
    if (node == NULL || ctx == NULL || node->type != AST_TYPE_ALIAS) {
        return 99;
    }

    TypeInfo *res_type = NULL;
    int res = semantic_resolve_type_alias(node->data.type_alias.aliased, ctx, &res_type);
    if (res != 0) {
        return res;
    }

    ast_set_type(node, res_type);
    return 0;
}

static SemanticFunctionSymbol *find_extension_symbol(ASTNode *function, SemanticContext *ctx) {
    if (function == NULL || ctx == NULL || function->type != AST_FUNC_DEF ||
        function->data.func_def.name == NULL) {
        return NULL;
    }

    tBtNode *symbol = NULL;

    symtable_search(&ctx->ext_symbols, function->data.func_def.name, &symbol);

    if (symbol == NULL || symbol->data == NULL || symbol->data->type != FUNCTION ||
        symbol->data->value == NULL) {
        return NULL;
    }

    for (SemanticFunctionSymbol *method = symbol->data->value; method != NULL;
         method = method->next) {

        if (method->function == function) {
            return method;
        }
    }

    return NULL;
}

int semantic_extension_def(ASTNode *node, SemanticContext *ctx) {
    if (node == NULL || ctx == NULL || node->type != AST_EXTENSION_DEF) {
        return 99;
    }

    ASTNode *receiver = node->data.extension_def.receiver;
    ASTNode *methods = node->data.extension_def.methods;

    if (receiver == NULL || receiver->type != AST_VAR_DEF ||
        receiver->data.var_def.identifier == NULL || receiver->data.var_def.declared_type == NULL ||
        receiver->data.var_def.initializer != NULL || receiver->data.var_def.is_mutable ||
        methods == NULL) {
        return 99;
    }

    // 1. Vyresit typ receiveru, pokud jeste neni vyreseny.
    if (receiver->expr_type == NULL) {
        TypeInfo *resolved_type = NULL;

        int error =
            semantic_resolve_type_alias(receiver->data.var_def.declared_type, ctx, &resolved_type);

        if (error != 0) {
            type_info_free(resolved_type);
            return error;
        }

        if (resolved_type == NULL) {
            return 99;
        }

        ast_set_type(receiver, resolved_type);
    }

    // 2. Pridelit receiveru var_id, pokud jeste nema.
    if (receiver->data.var_def.var_id == 0) {
        receiver->data.var_def.var_id = ctx->next_var_id++;
    }

    // 3. Zpracovat vsechny metody.
    for (ASTNode *method = methods; method != NULL; method = method->next) {

        if (method->type != AST_FUNC_DEF || method->data.func_def.name == NULL ||
            method->data.func_def.body == NULL) {
            return 99;
        }

        // Najdeme symbol vytvoreny pomoci register_ext_func.
        SemanticFunctionSymbol *symbol = find_extension_symbol(method, ctx);

        if (symbol == NULL || symbol->extension != receiver) {
            return 99;
        }

        // Kompletní semanticka kontrola metody.
        int error = semantic_func_def(symbol, ctx);

        if (error != 0) {
            return error;
        }
    }

    return 0;
}

int semantic_lambda(ASTNode *node, SemanticContext *ctx, const TypeInfo *expected_type);

/* ==================================================
 * PŘÍKAZY A ŘÍDICÍ STRUKTURY
 * ================================================== */

bool semantic_always_returns(const ASTNode *node) {
    if (node == NULL) {
        return false;
    }

    switch (node->type) {
        case AST_RETURN:
            return true;

        case AST_BLOCK:
            for (const ASTNode *stmt = node->data.block.statements; stmt != NULL;
                 stmt = stmt->next) {

                if (semantic_always_returns(stmt)) {
                    return true;
                }
            }
            return false;

        case AST_IF_EXPR:
            if (node->data.if_expr.else_block == NULL) {
                return false;
            }

            return semantic_always_returns(node->data.if_expr.then_block) &&
                   semantic_always_returns(node->data.if_expr.else_block);

        default:
            return false;
    }
}

// asi hotovo
int semantic_if_expr(ASTNode *node, SemanticContext *ctx, const TypeInfo *expected_type,
                     bool used_as_value) {
    if (node == NULL || ctx == NULL || node->type != AST_IF_EXPR) {
        return 99;
    }

    // zpracuje podmínku
    int res = semantic_visit(&node->data.if_expr.condition, ctx, NULL);
    if (res != 0) {
        return res;
    }
    if (node->data.if_expr.condition->expr_type->type_class != TYPE_BOOLEAN) {
        return 6;
    }

    // zkontroluje blok if
    res = semantic_check_block(node->data.if_expr.then_block, ctx,
                               used_as_value ? expected_type : NULL);
    if (res != 0) {
        return res;
    }

    if (node->data.if_expr.else_block != NULL) {

        // zpracuje else větev
        res = semantic_check_block(node->data.if_expr.else_block, ctx,
                                   used_as_value ? expected_type : NULL);
        if (res != 0) {
            return res;
        }

    } else { // bez else povoleno pouze u příkazu (ne výrazu)
        if (used_as_value) {
            return 6;
        }
    }

    TypeInfo *ret_type = NULL;

    if (used_as_value) {
        // If jako vyraz: spolecny typ obou vetvi.
        res = semantic_common_type(node->data.if_expr.then_block->expr_type,
                                   node->data.if_expr.else_block->expr_type, &ret_type);
        if (res != 0) {
            return res;
        }

        if (ret_type == NULL) {
            return 99;
        }

        res = semantic_check_conversion(node->data.if_expr.then_block, ret_type);

        if (res != 0) {
            type_info_free(ret_type);
            return res;
        }

        res = semantic_check_conversion(node->data.if_expr.else_block, ret_type);

        if (res != 0) {
            type_info_free(ret_type);
            return res;
        }
    } else {

        ret_type = type_create_basic(semantic_always_returns(node) ? TYPE_NOTHING : TYPE_UNIT);

        if (ret_type == NULL) {
            return 99;
        }
    }

    // nastaví expr_type
    ast_set_type(node, ret_type);
    return 0;
}

// hotovo
int semantic_while(ASTNode *node, SemanticContext *ctx) {
    if (node == NULL || ctx == NULL || node->data.while_loop.condition == NULL) {
        return 99;
    }

    int res = semantic_visit(&node->data.while_loop.condition, ctx, NULL);
    if (res != 0) {
        return res;
    }

    if (node->data.while_loop.condition->expr_type->type_class != TYPE_BOOLEAN) {
        return 6;
    }

    res = semantic_check_block(node->data.while_loop.body, ctx, NULL);
    if (res != 0) {
        return res;
    }

    TypeInfo *type = type_create_basic(TYPE_UNIT);
    if (type == NULL) {
        return 99;
    }

    ast_set_type(node, type);
    return 0;
}

int semantic_for(ASTNode *node, SemanticContext *ctx) {
    if (node == NULL || ctx == NULL || node->type != AST_FOR) {
        return 99;
    }

    // zpracování a kontrola zdroje generátoru
    int res = semantic_visit(&node->data.for_loop.source, ctx, NULL);
    if (res != 0) {
        return res;
    }
    if (node->data.for_loop.source->type != AST_RANGE &&
        node->data.for_loop.source->expr_type->type_class != TYPE_LIST) {
        return 6;
    }

    // zpracování proměnné cyklu
    TypeInfo *var_type = NULL;
    if (node->data.for_loop.source->type == AST_RANGE) {
        var_type = type_create_basic(TYPE_INT);
        if (var_type == NULL) {
            return 99;
        }

    } else {
        var_type = type_info_clone(node->data.for_loop.source->expr_type->data.list_element_type);
        if (var_type == NULL) {
            return 99;
        }
    }
    ast_set_type(node->data.for_loop.iter_var, var_type);
    node->data.for_loop.iter_var->data.var_def.var_id = ctx->next_var_id;
    ctx->next_var_id++;

    // nový scope
    tBtNode *for_scope = NULL;
    symtable_init(&for_scope);
    tBtNodeData var_data = {.value = node->data.for_loop.iter_var, .type = VARIABLE};
    symtable_insert(&for_scope, node->data.for_loop.iter_var->data.var_def.identifier, var_data);

    symtable_push_scope(&for_scope, ctx->scope_stack);

    // zpracování a kontrola stráží
    ASTNode **act_guard = &node->data.for_loop.guards;
    while (*act_guard != NULL) {
        res = semantic_visit(act_guard, ctx, NULL);
        if (res != 0) {
            symtable_pop_scope(ctx->scope_stack);
            return res;
        }
        if ((*act_guard)->expr_type->type_class != TYPE_BOOLEAN) {
            symtable_pop_scope(ctx->scope_stack);
            return 6;
        }
        act_guard = &(*act_guard)->next;
    }

    // zpracování těla
    res = semantic_check_block(node->data.for_loop.body, ctx, NULL);
    if (res != 0) {
        symtable_pop_scope(ctx->scope_stack);
        return res;
    }

    // uvolní se scope
    symtable_pop_scope(ctx->scope_stack);

    // nastavení typu
    TypeInfo *type = type_create_basic(TYPE_UNIT);
    if (type == NULL) {
        return 99;
    }
    ast_set_type(node, type);

    return 0;
}

int semantic_match(ASTNode *node, SemanticContext *ctx, const TypeInfo *expected_type);

// hotova
int semantic_return(ASTNode *node, SemanticContext *ctx) {
    if (node == NULL || ctx == NULL || node->type != AST_RETURN || ctx->current_function == NULL) {
        return 99;
    }

    if (ctx->inside_lambda || ctx->current_function->data.func_def.return_type == NULL) {
        return 6;
    }

    TypeInfo *func_ret = NULL;

    int res = semantic_resolve_type_alias(ctx->current_function->data.func_def.return_type, ctx,
                                          &func_ret);
    if (res != 0) {
        return res;
    }
    if (func_ret == NULL) {
        return 99;
    }

    if (node->data.return_stmt.value == NULL) {
        if (func_ret->type_class != TYPE_UNIT) {

            type_info_free(func_ret);

            return 6;
        }

    } else {
        res = semantic_visit(&node->data.return_stmt.value, ctx,
                             func_ret->type_class == TYPE_UNIT ? NULL : func_ret);
        if (res != 0) {
            type_info_free(func_ret);
            return res;
        }

        if (func_ret->type_class != TYPE_UNIT) {
            if (!semantic_types_compatible(func_ret, node->data.return_stmt.value->expr_type)) {
                type_info_free(func_ret);
                return 6;
            }
            res = semantic_check_conversion(node->data.return_stmt.value, func_ret);
            if (res != 0) {
                type_info_free(func_ret);
                return res;
            }
        }
    }

    type_info_free(func_ret);

    TypeInfo *ret_type = type_create_basic(TYPE_NOTHING);
    if (ret_type == NULL) {
        return 99;
    }
    ast_set_type(node, ret_type);
    return 0;
}

/* ==================================================
 * POMOCNÉ FUNKCE PRO STRUKTURÁLNÍ UZLY
 * ================================================== */

// hotovo
int semantic_check_agr_list(ASTNode *node, SemanticContext *ctx, const TypeInfo *param_types) {
    if (node == NULL || ctx == NULL || node->type != AST_ARG_LIST) {
        return 99;
    }

    ASTNode **actual = &node->data.list.items;
    const TypeInfo *expected = param_types;

    while (*actual != NULL && expected != NULL) {
        int res = semantic_visit(actual, ctx, expected);
        if (res != 0) {
            return res;
        }

        if (!semantic_types_compatible(expected, (*actual)->expr_type)) {
            return 5;
        }

        res = semantic_check_conversion(*actual, expected);
        if (res != 0) {
            return res;
        }

        actual = &(*actual)->next;
        expected = expected->next;
    }

    if (*actual != NULL || expected != NULL) {
        return 5;
    }

    return 0;
}

// hotovo
int semantic_register_param_list(ASTNode *node, SemanticContext *ctx) {
    if (node == NULL || ctx == NULL || node->type != AST_PARAM_LIST) {
        return 99;
    }

    ASTNode *param = node->data.list.items;

    while (param != NULL) {
        int res = semantic_var_def(ctx->scope_stack, param, ctx, true);
        if (res != 0) {
            return res;
        }

        param = param->next;
    }

    return 0;
}

int semantic_case_branch(ASTNode *node, SemanticContext *ctx, const TypeInfo *matched_type);

int semantic_pattern(ASTNode *node, SemanticContext *ctx, const TypeInfo *matched_type) {
}

// hotovo
int semantic_range(ASTNode *node, SemanticContext *ctx) {
    if (node == NULL || ctx == NULL || node->type != AST_RANGE) {
        return 99;
    }

    if (node->data.range.start == NULL || node->data.range.end == NULL) {
        return 99;
    }

    int res = semantic_visit(&node->data.range.start, ctx, NULL);
    if (res != 0) {
        return res;
    }

    res = semantic_visit(&node->data.range.end, ctx, NULL);
    if (res != 0) {
        return res;
    }

    if (node->data.range.start->expr_type->type_class != TYPE_INT ||
        node->data.range.end->expr_type->type_class != TYPE_INT) {
        return 6;
    }

    if (node->data.range.step != NULL) {
        res = semantic_visit(&node->data.range.step, ctx, NULL);
        if (res != 0) {
            return res;
        }
        if (node->data.range.step->expr_type->type_class != TYPE_INT) {
            return 6;
        }
    }
    return 0;
}

/**
 * @brief prohledá scope na zásobníku a zjistí, jestli je proměnná už definovaná a kde
 *
 * @param stack
 * @param name
 * @return tBtNode*
 */
tBtNode *semantic_find_symbol(tStack *stack, char *name, SemanticContext *ctx) {
    if (stack == NULL || name == NULL || ctx == NULL) {
        return NULL;
    }

    tStackElem *scope = stack->top;

    while (scope != NULL) {
        tBtNode *found = NULL;

        symtable_search(&scope->scope, name, &found);

        if (found != NULL) {
            return found;
        }

        scope = scope->next;
    }

    tBtNode *found = NULL;
    symtable_search(&ctx->symbols, name, &found);
    return found;
}

/* ==================================================
 * POMOCNÉ FUNKCE PRO PRÁCI S TYPY
 * ================================================== */

/**
 * @brief Vyhledá, jestli parametru "un" obsahuje typ y parametru "type"
 *
 * @param un Typeinfo ve kterém se hledá typ
 * @param type Typeinfo, který se hledá v parametru "un"
 * @return true pokud je typ z parametru "type" obsažen v parametru "un", jinak false
 */
bool semantics_compare_union_with_type(const TypeInfo *un, const TypeInfo *type) {
    if (un == NULL || type == NULL) {
        return false;
    }

    bool type_in_union = false;
    // rekurentně prochází union a porovnává typy
    if (un->type_class == TYPE_UNION) {
        if (type->type_class == TYPE_UNION) {
            type_in_union = compare_unions(type, un);
        } else {
            type_in_union = semantic_types_equal(un->data.union_type.right, type);
            if (!type_in_union) { // pokud typ ještě nebyl nalezen, pokračuje se v levé části
                                  // union
                type_in_union = semantics_compare_union_with_type(un->data.union_type.left, type);
            }
        }
    } else {
        type_in_union = semantic_types_equal(un, type);
    }
    return type_in_union;
}

/**
 * @brief Porovná, jestli union parametru 'b' obsahuje všechny typy z union parametru 'a'
 *
 * @param a TypeInfo typu union, ze kterého se porovnávají typy
 * @param b TypeInfo typu union, ve kterém se hledají typy z parametru 'a'
 * @return true pokud všechny typy z parametru 'a' jsou obsaženy v parametru 'b', jinak false
 */
bool compare_unions(const TypeInfo *a, const TypeInfo *b) {
    if (a == NULL || b == NULL) {
        return false;
    }

    bool are_equal = true;
    TypeInfo *position_a = a;
    // Prochází všechny typy v union z parametru 'a' a kontroluje, zda jsou obsaženy v parametru
    // 'b'
    while (position_a != NULL && position_a->type_class == TYPE_UNION && are_equal) {
        are_equal = semantics_compare_union_with_type(b, position_a->data.union_type.right);
        position_a = position_a->data.union_type.left;
    }
    return are_equal && semantics_compare_union_with_type(
                            b, position_a); // pro porovnání posledního typu v union
}

bool semantic_types_equal(const TypeInfo *a, const TypeInfo *b) {
    // Pokud je nějaký z typů NULL vrátí false
    if (a == NULL || b == NULL) {
        return false;
    }

    // porovnání typů

    if (a->type_class == b->type_class) {
        bool return_value = true;
        if (a->type_class == TYPE_UNION) {
            return_value = compare_unions(a, b) && compare_unions(b, a);

        } else if (a->type_class == TYPE_FUNCTION) {
            TypeInfo *actual_param_a = a->data.func_type.param_types;
            TypeInfo *actual_param_b = b->data.func_type.param_types;

            // porovnání seznamu parametrů
            bool are_equal = true;
            while (actual_param_a != NULL && actual_param_b != NULL) {
                // porovnání typů parametrů
                if (!semantic_types_equal(actual_param_a, actual_param_b)) {
                    are_equal = false;
                    break;
                }
                // posun na další parametry v seznamu
                actual_param_a = actual_param_a->next;
                actual_param_b = actual_param_b->next;
            }
            // nastane když pouze jeden z ukazatelů je NULL (jinak by skončil while)
            if ((actual_param_a != NULL) || (actual_param_b != NULL)) {
                are_equal = false;
            }
            return_value =
                are_equal &&
                semantic_types_equal(a->data.func_type.return_type,
                                     b->data.func_type.return_type); // porovnání návratového typu
        } else if (a->type_class == TYPE_LIST) {
            return_value =
                semantic_types_equal(a->data.list_element_type, b->data.list_element_type);
        } else if (a->type_class == TYPE_CUSTOM) {
            if (a->data.custom_name == NULL || b->data.custom_name == NULL) {
                return false;
            }
            return_value = strcmp(a->data.custom_name, b->data.custom_name) == 0;
        } else {
            return_value = true;
        }
        return return_value;
    }
    return false;
}

bool semantic_types_compatible(const TypeInfo *expected, const TypeInfo *actual) {
    if (expected == NULL || actual == NULL) {
        return false;
    }

    bool compatible = false;
    if (expected->type_class == TYPE_DOUBLE && actual->type_class == TYPE_INT) {
        compatible = true; // implicitní konverze Int -> Double
    } else if (actual->type_class == TYPE_NOTHING) {
        compatible = true; // Nothing je kompatibilní s každým typem
    } else if (expected->type_class == TYPE_ANY) {
        compatible = true;
    } else if (semantic_types_equal(expected, actual)) {
        compatible = true;
    } else if (expected->type_class == TYPE_UNION && actual->type_class != TYPE_UNION) {
        compatible = semantics_compare_union_with_type(expected, actual);
        if (!compatible &&
            actual->type_class == TYPE_INT) { // pokud by byl typ int a nebyl nalezen v union,
                                              // zkontroluje se, jestli je v union double
                                              // (implicitně by se mohl převést int -> double)
            TypeInfo double_type = {.type_class = TYPE_DOUBLE, .next = NULL};
            compatible = semantics_compare_union_with_type(expected, &double_type);
        }
    }
    return compatible;
}

int semantic_common_type(const TypeInfo *a, const TypeInfo *b, TypeInfo **result) {
    if (a == NULL || b == NULL || result == NULL) {
        return 99; // Chyba: Neplatný vstup
    }
    *result = NULL;
    TypeInfo *ret_type = NULL;

    if (semantic_types_equal(a, b)) {
        ret_type = a; // Typy jsou stejné, vrací se jeden z nich
    } else if (a->type_class == TYPE_INT && b->type_class == TYPE_DOUBLE) {
        ret_type = b; // Int a Double, vrací se Double
    } else if (a->type_class == TYPE_DOUBLE && b->type_class == TYPE_INT) {
        ret_type = a; // taky double a int co vrací double, pro obrácené pořadí
    } else if (a->type_class == TYPE_NOTHING) {
        ret_type = b; // Nothing je kompatibilní s každým typem, vrací se druhý typ
    } else if (b->type_class == TYPE_NOTHING) {
        ret_type = a; // stejně jako předchozí, jen nothing ve druhém typu
    } else if (a->type_class == TYPE_ANY) {
        ret_type = b; // Any je kompatibilní s každým typem, vrací se druhý typ
    } else if (b->type_class == TYPE_ANY) {
        ret_type = a; // Any je kompatibilní s každým typem, vrací se první typ
    } else if (semantic_types_compatible(a, b)) {
        ret_type = a; // a je kompatibilní s b, vrací se a, (ANY v a být nemůže, už by prošlo výš)
    } else if (semantic_types_compatible(b, a)) {
        ret_type = b; // b je kompatibilní s a, vrací se b
    } else {
        return 6; // Chyba: Typy nejsou kompatibilní
    }

    // potřeba naklokovat výsledný typ
    *result = type_info_clone(ret_type);
    if (*result == NULL) {
        return 99;
    }

    return 0; // Úspěch
}

int semantic_check_conversion(ASTNode *expression, const TypeInfo *expected_type) {
    if (expression == NULL || expression->expr_type == NULL || expected_type == NULL) {
        return 99; // Chyba: Neplatný vstup
    }
    expression->to_double = false;
    if (expression->expr_type->type_class == TYPE_INT) {
        if (expected_type->type_class == TYPE_DOUBLE) {
            expression->to_double = true; // Nastavení příznaku pro implicitní konverzi
        } else if (expected_type->type_class == TYPE_UNION) {
            const TypeInfo int_type = {.type_class = TYPE_INT};

            const TypeInfo double_type = {.type_class = TYPE_DOUBLE};

            expression->to_double = !semantics_compare_union_with_type(expected_type, &int_type) &&
                                    semantics_compare_union_with_type(expected_type, &double_type);
        }
    }
    return 0; // Úspěch
}

void normalise_union_tree(TypeInfo *type) {
    if (type == NULL) {
        return;
    }

    TypeInfo *end_union = type;
    if (type->type_class == TYPE_UNION) {
        while (end_union->data.union_type.left->type_class == TYPE_UNION) {
            end_union = end_union->data.union_type.left;
        }
    }

    TypeInfo *pos = type;
    while (pos->type_class == TYPE_UNION) {
        TypeInfo *tmp = pos->data.union_type.right;
        switch (tmp->type_class) {
            case TYPE_UNION:
                pos->data.union_type.right = end_union->data.union_type.left;
                end_union->data.union_type.left = tmp;
                while (end_union->data.union_type.left->type_class == TYPE_UNION) {
                    end_union = end_union->data.union_type.left;
                }
                break;
            case TYPE_LIST:
                normalise_union_tree(tmp->data.list_element_type);
                break;
            case TYPE_FUNCTION:
                for (TypeInfo *param = tmp->data.func_type.param_types; param != NULL;
                     param = param->next) {

                    normalise_union_tree(param);
                }

                // Navratovy typ funkce.
                normalise_union_tree(tmp->data.func_type.return_type);
                break;
        }
        pos = pos->data.union_type.left;
    }
    switch (pos->type_class) {
        case TYPE_LIST:
            normalise_union_tree(pos->data.list_element_type);
            break;
        case TYPE_FUNCTION:
            for (TypeInfo *param = pos->data.func_type.param_types; param != NULL;
                 param = param->next) {

                normalise_union_tree(param);
            }

            // Navratovy typ funkce.
            normalise_union_tree(pos->data.func_type.return_type);
            break;
    }
}

static bool alias_in_path(const AliasPath *path, const char *name) {
    AliasPath *path_pos = path;
    while (path_pos != NULL) {
        if (strcmp(path_pos->name, name) == 0) {
            return true;
        }
        path_pos = path_pos->previous;
    }
    return false;
}

static int expand_aliases(TypeInfo **slot, SemanticContext *ctx, const AliasPath *path) {
    if (slot == NULL || *slot == NULL || ctx == NULL) {
        return 99;
    }

    TypeInfo *type = *slot;

    switch (type->type_class) {
        case TYPE_CUSTOM: {
            tBtNode *symbol = NULL;

            symtable_search(&ctx->symbols, type->data.custom_name, &symbol);

            if (symbol == NULL || symbol->data == NULL) {
                return 6; // Neznamy typ
            }

            // Datova trida zustava TYPE_CUSTOM.
            if (symbol->data->type == CLASS) {
                return 0;
            }

            if (symbol->data->type != ALIAS) {
                return 3;
            }

            // V globalni tabulce mas AST_TYPE_ALIAS.
            ASTNode *definition = symbol->data->value;

            if (definition == NULL || definition->type != AST_TYPE_ALIAS ||
                definition->data.type_alias.aliased == NULL) {
                return 99;
            }

            const char *name = definition->data.type_alias.name;

            // Alias uz se na aktualni ceste rozviji.
            if (alias_in_path(path, name)) {
                return 6;
            }

            // Vytvorime kopii typu, na ktery alias ukazuje.
            TypeInfo *replacement = type_info_clone(definition->data.type_alias.aliased);

            if (replacement == NULL) {
                return 99;
            }

            // Zaznam aktualne rozvijeneho aliasu.
            AliasPath current = {.name = name, .previous = path};

            // Nahradime TYPE_CUSTOM rozvinovanym typem.
            // type_replace zachova pripadne propojeni next.
            type_replace(slot, replacement);

            // Pokracujeme v rozvijeni nahradniho typu.
            return expand_aliases(slot, ctx, &current);
        }

        case TYPE_UNION: {
            int result = expand_aliases(&type->data.union_type.left, ctx, path);

            if (result != 0) {
                return result;
            }

            return expand_aliases(&type->data.union_type.right, ctx, path);
        }

        case TYPE_LIST:
            return expand_aliases(&type->data.list_element_type, ctx, path);

        case TYPE_FUNCTION: {
            // Rozvineme aliasy ve vsech parametrech.
            TypeInfo **param = &type->data.func_type.param_types;

            while (*param != NULL) {
                int result = expand_aliases(param, ctx, path);

                if (result != 0) {
                    return result;
                }

                param = &(*param)->next;
            }

            // Rozvineme alias v navratovem typu.
            return expand_aliases(&type->data.func_type.return_type, ctx, path);
        }

        case TYPE_INT:
        case TYPE_DOUBLE:
        case TYPE_STRING:
        case TYPE_BOOLEAN:
        case TYPE_UNIT:
        case TYPE_ANY:
        case TYPE_NOTHING:
            return 0;

        default:
            return 99;
    }
}

int semantic_resolve_type_alias(const TypeInfo *type, SemanticContext *ctx, TypeInfo **result) {
    if (type == NULL || ctx == NULL || result == NULL) {
        return 99;
    }

    *result = type_info_clone(type);

    if (*result == NULL) {
        return 99;
    }

    int error = expand_aliases(result, ctx, NULL);

    if (error != 0) {
        type_info_free(*result);
        *result = NULL;
        return error;
    }

    normalise_union_tree(*result);

    return 0;
}
