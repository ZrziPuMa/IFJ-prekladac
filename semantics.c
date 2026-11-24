/**
 * @file semantics.C
 * @brief Implementace sémantické analýzy překladače z hlavičkových souborů isemantics.h a
 * semantics.h
 * @author Natalie Morávková (<xmoravn00>)
 *
 * při vývoji byla použita AI (ChatGPT, github copilot) pro kontrolu kódu, pomoc s opravami
 * funkcionality kódu, pomoc s implementací složitějších částí a vyhledávání informací ke kontrole
 * ze specifikace IFJ26
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

ErrorCode semantic_error(SemanticContext *ctx, ErrorCode code, const ASTNode *node) {
    // nastaví řádek, kde nastala chyba
    if (code != ERR_OK && ctx != NULL && ctx->error_line == -1 && node != NULL) {
        ctx->error_line = node->line;
    }

    return code;
}

void semantic_free_func_sym(tBtNode *node) {
    if (node == NULL) {
        return;
    }

    // rekurzivně prochází strom tabulky
    semantic_free_func_sym(node->left_child);
    semantic_free_func_sym(node->right_child);

    // když to není funkce, nic se nedělá
    if (node->data == NULL || node->data->type != FUNCTION) {
        return;
    }

    SemanticFunctionSymbol *func = node->data->value;

    // uvolní strukturu a všechny její funkce nebo metody, které obsahuje
    while (func != NULL) {
        SemanticFunctionSymbol *next = func->next;

        // řeší uvolnění vestavěných funkcí
        for (int i = 0; i < 2; i++) {
            if (func->builtin_signatures[i] != NULL) {
                type_info_free(func->builtin_signatures[i]);
            }
        }

        free(func);
        func = next;
    }

    node->data->value = NULL;
}

ErrorCode check_semantics(ASTNode *root) {
    // vytvoží kontext pro sémantiku
    SemanticContext ctx = {0};
    ctx.error_line = -1;

    // inicializuje struktury v něm
    ErrorCode res = symtable_init(&ctx.symbols);
    if (res != ERR_OK) {
        goto cleanup;
    }
    res = symtable_init(&ctx.ext_symbols);
    if (res != ERR_OK) {
        goto cleanup;
    }

    ctx.scope_stack = NULL;
    ctx.current_function = NULL;
    ctx.next_var_id = 1;
    ctx.inside_lambda = false;

    // registrace vestavěných funkcí
    res = register_buildin_func(&ctx);
    if (res != ERR_OK) {
        goto cleanup;
    }

    // registrace globálních deklarací
    res = semantic_collect_globals(root, &ctx);
    if (res != ERR_OK) {
        goto cleanup;
    }

    // zpracování aliasů a tříd
    res = check_global_alias_class(root, &ctx);
    if (res != ERR_OK) {
        goto cleanup;
    }

    // analýza funkcí a extension metod
    res = semantic_check_program(root, &ctx);

cleanup:
    // potřeba uvolnit zvlášť SemanticFunctionSymbol struktury
    semantic_free_func_sym(ctx.ext_symbols);
    semantic_free_func_sym(ctx.symbols);

    // uvolnění zbytku
    ErrorCode dispose_res = symtable_dispose(&ctx.ext_symbols);
    if (res == ERR_OK) {
        res = dispose_res;
    }
    dispose_res = symtable_dispose(&ctx.symbols);
    if (res == ERR_OK) {
        res = dispose_res;
    }

    // pokud nastala chyba, vypíše chybové hlášení
    if (res != ERR_OK) {
        return print_error(res, ctx.error_line);
    }

    return res;
}

/* ==================================================
 * FUNKCE PRO 1. A 2. PRŮCHOD AST
 * ================================================== */

TypeInfo *builtin_ret_type(BuiltinDef *built_def) {
    if (built_def == NULL) {
        return NULL;
    }

    // nastavení pro List.range
    if (strcmp(built_def->name, "List.range") == 0) {
        // vytvoří typ list[int] a vrátí ho
        TypeInfo *int_type = type_create_basic(TYPE_INT);
        if (!int_type) {
            return NULL;
        }

        TypeInfo *list = type_create_list(int_type);
        if (!list) {
            if (int_type != NULL) {
                type_info_free(int_type);
            }
            return NULL;
        }
        return list;
    }

    // vyřeší případy kdy vrací union
    if (strcmp(built_def->name, "readIntMaybe") == 0 ||
        strcmp(built_def->name, "readDoubleMaybe") == 0) {
        // vytvoří union UNIT | INT nebo DOUBLE podle built_def
        TypeInfo *value = type_create_basic(built_def->result);
        if (value == NULL) {
            return NULL;
        }
        TypeInfo *unit = type_create_basic(TYPE_UNIT);
        if (unit == NULL) {
            type_info_free(value);
            return NULL;
        }
        TypeInfo *res_union = type_create_union(value, unit);
        if (!res_union) {
            type_info_free(value);
            type_info_free(unit);
        }
        return res_union;
    }
    // ostatní případy už mají jednoduché typy
    return type_create_basic(built_def->result);
}

TypeInfo *builtin_type(BuiltinDef *built_def) {
    TypeInfo *params = NULL; // seznam parametrů
    // vytvoří seznam typeinfo parametrů podle typů argunemtů
    for (int i = 0; i < built_def->argc; i++) {
        TypeInfo *param_type = type_create_basic(built_def->args[i]);
        if (param_type == NULL) {
            if (params != NULL) {
                type_info_free(params);
            }
            return NULL;
        }
        params = type_append(params, param_type);
    }

    // vytvoří návratový typ funkce
    TypeInfo *ret_type = builtin_ret_type(built_def);
    if (ret_type == NULL) {
        type_info_free(params);
        return NULL;
    }

    // z předchozích vytvoří celý typ funkce
    TypeInfo *func_type = type_create_function(params, ret_type);
    if (func_type == NULL) {
        if (params != NULL) {
            type_info_free(params);
        }
        if (ret_type != NULL) {
            type_info_free(ret_type);
        }
    }
    return func_type;
}

ErrorCode register_buildin_func(SemanticContext *ctx) {
    if (ctx == NULL) {
        return semantic_error(ctx, ERR_INTERNAL, NULL);
    }
    // seznam vestavěných funkcí
    BuiltinDef func_def[] = {
        {"print", TYPE_UNIT, 1, {TYPE_ANY}},
        {"readLine", TYPE_STRING, 0, {0}},
        {"readInt", TYPE_INT, 0, {0}},
        {"readDouble", TYPE_DOUBLE, 0, {0}},
        {"toStr", TYPE_STRING, 1, {TYPE_ANY}},
        {"toInt", TYPE_INT, 1, {TYPE_DOUBLE}},
        {"strLength", TYPE_INT, 1, {TYPE_STRING}},
        {"subStr", TYPE_STRING, 3, {TYPE_STRING, TYPE_INT, TYPE_INT}},
        {"ord", TYPE_INT, 2, {TYPE_STRING, TYPE_INT}},
        {"chr", TYPE_STRING, 1, {TYPE_INT}},
        {"readBoolean", TYPE_BOOLEAN, 0, {0}},
        {"readIntMaybe", TYPE_INT, 0, {0}},                // vrací TYPE_INT | TYPE_UNIT
        {"readDoubleMaybe", TYPE_DOUBLE, 0, {0}},          // vrací TYPE_DOUBLE | TYPE_UNIT
        {"List.range", TYPE_INT, 2, {TYPE_INT, TYPE_INT}}, // vrací list
        {"List.range", TYPE_INT, 3, {TYPE_INT, TYPE_INT, TYPE_INT}}}; // vrací list

    // projde seznam vestavěných funkcí, každou zpracuje, převede do správného tvaru a dá do
    // vyhledávací tabulky
    for (int i = 0; i < 15; i++) {
        // zjistí, jestli už existuje jiná funkce stejného jména
        BuiltinDef *built_def = &func_def[i];
        tBtNode *found = NULL;
        ErrorCode err = symtable_search(&ctx->symbols, built_def->name, &found);
        if (err != ERR_OK) {
            return semantic_error(ctx, err, NULL);
        }

        // jen List.range může mít dvě varianty, jinak chyba
        if (found && strcmp(built_def->name, "List.range") != 0) {
            return semantic_error(ctx, ERR_SEM_REDEF, NULL);
        }

        // vytvoří typ funkce
        TypeInfo *func_type = builtin_type(built_def);
        if (func_type == NULL) {
            return semantic_error(ctx, ERR_INTERNAL, NULL);
        }

        if (found != NULL) { // je to druhý list.range
            if (found->data == NULL || found->data->type != FUNCTION ||
                found->data->value == NULL) {
                type_info_free(func_type);
                return semantic_error(ctx, ERR_SEM_REDEF, NULL);
            }
            SemanticFunctionSymbol *func_sym = found->data->value;
            if (!func_sym->is_buildin || func_sym->builtin_signatures[0] == NULL ||
                func_sym->builtin_signatures[1] != NULL) {
                type_info_free(func_type);
                return semantic_error(ctx, ERR_SEM_REDEF, NULL);
            }
            func_sym->builtin_signatures[1] = func_type;
            continue;
        }

        // vytvoří si nový symbol pro funkci a vloží ke globálním deklaracím
        SemanticFunctionSymbol *func_sym = calloc(1, sizeof *func_sym);
        if (func_sym == NULL) {
            type_info_free(func_type);
            return semantic_error(ctx, ERR_INTERNAL, NULL);
        }
        func_sym->is_buildin = true;
        func_sym->state = FUNC_CHECKED;
        func_sym->builtin_signatures[0] = func_type;
        tBtNodeData data = {.type = FUNCTION, .value = func_sym};
        err = symtable_insert(&ctx->symbols, built_def->name, data);
        if (err != ERR_OK) {
            type_info_free(func_type);
            free(func_sym);
            return semantic_error(ctx, err, NULL);
        }
    }
    return ERR_OK;
}

ErrorCode semantic_register_global(SemanticContext *ctx, char *name, tBtContentType type,
                                   ASTNode *declaration) {
    if (ctx == NULL || declaration == NULL || name == NULL) {
        return semantic_error(ctx, ERR_INTERNAL, declaration);
    }

    // Kontrola duplicity metody.
    tBtNode *existing = NULL;
    ErrorCode err = symtable_search(&ctx->symbols, name, &existing);
    if (err != ERR_OK) {
        return semantic_error(ctx, err, declaration);
    }

    if (existing != NULL) {
        return semantic_error(ctx, ERR_SEM_REDEF, declaration); // duplicitní deklarace
    }

    // vytvoří potřebný uzel do tabuldy
    tBtNodeData data = {.type = type, .value = declaration};

    if (type == FUNCTION) {
        SemanticFunctionSymbol *info = calloc(1, sizeof *info);
        if (info == NULL) {
            return semantic_error(ctx, ERR_INTERNAL, declaration);
        }

        info->function = declaration;
        info->extension = NULL;
        info->state = FUNC_UNRESOLVED;
        info->next = NULL;
        data.value = info;
        info->is_buildin = false;
    }

    // vloží uzel do tabulky
    err = semantic_error(ctx, symtable_insert(&ctx->symbols, name, data), declaration);
    if (type == FUNCTION && err != ERR_OK && data.value != NULL) {
        free(data.value);
    }
    return err;
}

ErrorCode register_ext_func(SemanticContext *ctx, ASTNode *declaration, ASTNode *ext) {
    if (ctx == NULL || declaration == NULL || ext == NULL || declaration->type != AST_FUNC_DEF) {
        return semantic_error(ctx, ERR_INTERNAL, declaration);
    }

    // najde jestli existuje funkce se stejným jménem
    tBtNode *found = NULL;
    ErrorCode err = symtable_search(&ctx->ext_symbols, declaration->data.func_def.name, &found);
    if (err != ERR_OK) {
        return semantic_error(ctx, err, declaration);
    }

    SemanticFunctionSymbol *last = NULL; // pro uložení posledního prvku
    if (found != NULL) {
        if (found->data == NULL || found->data->type != FUNCTION || found->data->value == NULL) {
            return semantic_error(ctx, ERR_INTERNAL, declaration);
        }

        // projde seznam extension metod stejného jména a podle parametu reciever určí, jestli je
        // nebo není problém
        SemanticFunctionSymbol *pos = found->data->value;
        while (pos != NULL) {
            if (pos->extension == NULL || pos->extension->expr_type == NULL) {
                return semantic_error(ctx, ERR_INTERNAL, declaration);
            }
            if (semantic_types_equal(pos->extension->expr_type,
                                     ext->expr_type)) { // je to opakovaná deklarace
                return semantic_error(ctx, ERR_SEM_REDEF, declaration);
            }
            last = pos;
            pos = pos->next;
        }
    }

    // vytvoření symbolu funkce
    SemanticFunctionSymbol *info = calloc(1, sizeof *info);
    if (info == NULL) {
        return semantic_error(ctx, ERR_INTERNAL, declaration);
    }

    info->function = declaration;
    info->extension = ext;
    info->state = FUNC_UNRESOLVED;
    info->next = NULL;
    info->is_buildin = false;

    if (found == NULL) {
        // první metoda tohoto jména
        tBtNodeData data = {.type = FUNCTION, .value = info};
        err = symtable_insert(&ctx->ext_symbols, declaration->data.func_def.name, data);
        if (err != ERR_OK) {
            if (info != NULL) {
                free(info);
            }
            return semantic_error(ctx, err, declaration);
        }
    } else {
        // není první metoda tohoto jména, dám na konec seznamu těch metod
        last->next = info;
    }
    return ERR_OK;
}

ErrorCode check_global_alias_class(ASTNode *program, SemanticContext *ctx) {
    if (program == NULL || ctx == NULL || program->type != AST_PROGRAM) {
        return semantic_error(ctx, ERR_INTERNAL, program);
    }

    ASTNode *definition = program->data.program.definitions;

    // projde seznam definic
    while (definition != NULL) {
        ErrorCode result = ERR_OK;

        // kontrola probíhá u tříd a aliasů, u main pouze zkontroluje deklaraci main
        switch (definition->type) {

            case AST_FUNC_DEF:
                // u @main zkontroluje, že nemá žádné parametry
                if (definition->data.func_def.is_main) {
                    ASTNode *group = definition->data.func_def.param_groups;

                    while (group != NULL) {
                        if (group->type != AST_PARAM_LIST) {
                            return semantic_error(ctx, ERR_INTERNAL, definition);
                        }

                        // každá skupina parametrů musí být prázdná
                        if (group->data.list.items != NULL) {
                            return semantic_error(ctx, ERR_SEM_ARGS, definition);
                        }

                        group = group->next;
                    }
                }
                break;

            case AST_CLASS_DEF:
                // kontrola atributů třídy a jejich typů
                result = semantic_class_def(definition, ctx);
                break;

            case AST_TYPE_ALIAS:
                // rozvinutí a kontrola aliasu
                result = semantic_type_alias(definition, ctx);
                break;

            case AST_EXTENSION_DEF:
                // extension metody se zpracují později
                break;

            default:
                return semantic_error(ctx, ERR_INTERNAL, definition);
        }

        if (result != ERR_OK) {
            return semantic_error(ctx, result, definition);
        }

        definition = definition->next;
    }

    return ERR_OK;
}

ErrorCode semantic_collect_globals(ASTNode *program, SemanticContext *ctx) {
    if (program == NULL || ctx == NULL || program->type != AST_PROGRAM) {
        return semantic_error(ctx, ERR_INTERNAL, program);
    }

    ASTNode *definition = program->data.program.definitions;

    bool found_main = false;

    while (definition != NULL) {
        // zpracování jedné globální deklarace
        ErrorCode result = ERR_OK;

        // definice registruje
        switch (definition->type) {
            case AST_FUNC_DEF:
                if (definition->data.func_def.is_main) {
                    if (found_main) {
                        return semantic_error(ctx, ERR_SEM_REDEF,
                                              definition); // opakovaná definice @main
                    }
                    found_main = true;
                }
                result = semantic_register_global(ctx, definition->data.func_def.name, FUNCTION,
                                                  definition);
                if (result != ERR_OK) {
                    return semantic_error(ctx, result, definition);
                }
                break;

            case AST_CLASS_DEF:
                result = semantic_register_global(ctx, definition->data.class_def.name, CLASS,
                                                  definition);
                if (result != ERR_OK) {
                    return semantic_error(ctx, result, definition);
                }
                break;

            case AST_TYPE_ALIAS:
                result = semantic_register_global(ctx, definition->data.type_alias.name, ALIAS,
                                                  definition);
                if (result != ERR_OK) {
                    return semantic_error(ctx, result, definition);
                }
                break;

            case AST_EXTENSION_DEF:
                break;

            default:
                // neočekávaný typ uzlu na úrovni programu
                return semantic_error(ctx, ERR_INTERNAL, definition);
        }
        definition = definition->next;
    }

    // vrátí se s ukazatelem na začátek definic a kpracuje deklarace extension metod
    definition = program->data.program.definitions;

    while (definition != NULL) {
        if (definition->type == AST_EXTENSION_DEF) {
            ASTNode *pos = definition->data.extension_def.methods;
            ASTNode *reciever_node = definition->data.extension_def.receiver;
            TypeInfo *resolved = NULL;

            // řeší reciever část
            // udělá kopii typu a případně rozloží alias
            ErrorCode res_alias = semantic_resolve_type_alias(
                reciever_node->data.var_def.declared_type, ctx, &resolved);

            if (res_alias != ERR_OK) {
                return semantic_error(ctx, res_alias, program);
            }
            ast_set_type(reciever_node, resolved);

            // řeší seznam metod
            while (pos != NULL) {
                if (pos->type != AST_FUNC_DEF) {
                    return semantic_error(ctx, ERR_INTERNAL, definition);
                }

                ErrorCode result =
                    register_ext_func(ctx, pos, definition->data.extension_def.receiver);
                if (result != ERR_OK) {
                    return semantic_error(ctx, result, definition);
                }
                pos = pos->next;
            }
        }
        definition = definition->next;
    }

    if (!found_main) { // v programu chybí funkce main
        return semantic_error(ctx, ERR_SEM_UNDEF, NULL);
    }
    return ERR_OK;
}

ErrorCode semantic_check_program(ASTNode *program, SemanticContext *ctx) {
    if (program == NULL || ctx == NULL || program->type != AST_PROGRAM) {
        return semantic_error(ctx, ERR_INTERNAL, program);
    }

    ASTNode *definition = program->data.program.definitions;

    // projde globální deklarace a zpracuje je
    while (definition != NULL) {
        ErrorCode res = ERR_OK;

        switch (definition->type) {

            case AST_FUNC_DEF: {
                // najde symbol funkce
                tBtNode *symbol = NULL;
                res = symtable_search(&ctx->symbols, definition->data.func_def.name, &symbol);
                if (res != ERR_OK) {
                    return semantic_error(ctx, res, definition);
                }

                if (symbol == NULL || symbol->data->type != FUNCTION) {
                    return semantic_error(ctx, ERR_INTERNAL, definition);
                }

                SemanticFunctionSymbol *func_sym = symbol->data->value;
                // zkontroluje, že ho našel správně
                if (func_sym->is_buildin || func_sym->function != definition) {
                    return semantic_error(ctx, ERR_INTERNAL, definition);
                }

                // zpracuje tu funkci
                res = semantic_func_def(func_sym, ctx);
                if (res != ERR_OK) {
                    break;
                }

                // dodatečné kontroly pro main
                if (definition->data.func_def.is_main) {
                    const TypeInfo *ret_type = definition->expr_type;

                    for (ASTNode *group = definition->data.func_def.param_groups; group != NULL;
                         group = group->next) {

                        if (ret_type == NULL || ret_type->type_class != TYPE_FUNCTION) {
                            return semantic_error(ctx, ERR_INTERNAL, definition);
                        }

                        ret_type = ret_type->data.func_type.return_type;
                    }

                    if (ret_type == NULL) {
                        return semantic_error(ctx, ERR_INTERNAL, definition);
                    }

                    if (ret_type->type_class != TYPE_UNIT) {
                        return semantic_error(ctx, ERR_SEM_TYPE, definition);
                    }
                }
                break;
            }

            case AST_EXTENSION_DEF:
                // zpracuje estension metody
                res = semantic_extension_def(definition, ctx);
                break;

            case AST_CLASS_DEF:
            case AST_TYPE_ALIAS:
                // už se kontrolovaly dřív
                break;

            default:
                return semantic_error(ctx, ERR_INTERNAL, definition);
        }

        if (res != ERR_OK) {
            return semantic_error(ctx, res, definition);
        }

        definition = definition->next;
    }

    return ERR_OK;
}

/* ==================================================
 * POMOCNÉ FUNKCE PRO PRŮCHOD AST
 * ================================================== */

ErrorCode semantic_visit(ASTNode **node, SemanticContext *ctx, const TypeInfo *expected_type) {
    if (node == NULL || *node == NULL || ctx == NULL) {
        return semantic_error(ctx, ERR_INTERNAL, node != NULL ? *node : NULL);
    }
    ErrorCode res = ERR_OK;
    //  podle typu uzlu zvolí, kterou funkci zavolat
    switch ((*node)->type) {
        case AST_ASSIGNMENT:
            res = semantic_assignment(*node, ctx);
            break;

        case AST_BINARY_OP:
            res = semantic_binary_op(*node, ctx);
            break;

        case AST_BLOCK:
            res = semantic_check_block(*node, ctx, expected_type, true, true);
            break;

        case AST_EXTENSION_DEF:
            res = semantic_extension_def(*node, ctx);
            break;

        case AST_FOR:
            res = semantic_for(*node, ctx);
            break;

        case AST_FUNC_CALL:
            res = semantic_func_call(*node, ctx, expected_type);
            break;

        case AST_IDENTIFIER:
            res = semantic_identifier(node, ctx);
            break;

        case AST_IF_EXPR:
            res = semantic_if_expr(*node, ctx, expected_type, true);
            break;

        case AST_LAMBDA:
            res = semantic_lambda(*node, ctx, expected_type);
            break;

        case AST_LIST_LITERAL:
            res = semantic_list_literal(*node, ctx, expected_type);
            break;

        case AST_LITERAL:
            res = semantic_literal(*node);
            break;

        case AST_MATCH_EXPR:
            res = semantic_match(*node, ctx, expected_type);
            break;

        case AST_MEMBER_ACCESS:
            res = semantic_member_access(node, ctx);
            break;

        case AST_RANGE:
            return semantic_error(
                ctx, ERR_SEM_TYPE,
                node != NULL ? *node : NULL); // půjde řešit jen z určitých míst, nemůže být všude

        case AST_RETURN:
            res = semantic_return(*node, ctx);
            break;

        case AST_TYPE_ALIAS:
            res = semantic_type_alias(*node, ctx);
            break;

        case AST_UNARY_OP:
            res = semantic_unary_op(*node, ctx);
            break;

        case AST_VAR_DEF:
            res = semantic_var_def(ctx->scope_stack, *node, ctx,
                                   false); // pro is_param true se volá přímo
            break;

        case AST_WHILE:
            res = semantic_while(*node, ctx);
            break;

        default:
            return semantic_error(ctx, ERR_INTERNAL, node != NULL ? *node : NULL);
    }
    return semantic_error(ctx, res, node != NULL ? *node : NULL);
}

ErrorCode semantic_check_block(ASTNode *block, SemanticContext *ctx, const TypeInfo *expected_type,
                               bool used_as_value, bool create_scope) {
    // úvodní kontroly
    if (block == NULL || ctx == NULL || block->type != AST_BLOCK || ctx->scope_stack == NULL) {
        return semantic_error(ctx, ERR_INTERNAL, block);
    }

    if (block->data.block.statements == NULL) {
        return semantic_error(ctx, ERR_INTERNAL, block);
    }

    ErrorCode res = ERR_OK;

    // vytvoření nového scope pokud se má tvořit
    if (create_scope) {
        tBtNode *new_scope = NULL;

        res = symtable_init(&new_scope);
        if (res != ERR_OK) {
            return semantic_error(ctx, res, block);
        }

        res = symtable_push_scope(&new_scope, ctx->scope_stack);
        if (res != ERR_OK) {
            return semantic_error(ctx, res, block);
        }
    }

    // projde seznam příkazů v bloku, každý příkaz zpracuje
    ASTNode **act_statement = &block->data.block.statements;
    while ((*act_statement)->next != NULL) {
        if ((*act_statement)->type == AST_IF_EXPR) {
            res = semantic_if_expr(*act_statement, ctx, NULL, false);
        } else if ((*act_statement)->type == AST_BLOCK) {
            res = semantic_check_block(*act_statement, ctx, NULL, false, true);
        } else {
            res = semantic_visit(act_statement, ctx, NULL);
        }
        if (res != ERR_OK) {
            if (create_scope) {
                symtable_pop_scope(ctx->scope_stack);
            }
            return semantic_error(ctx, res, block);
        }
        act_statement = &(*act_statement)->next;
    }

    // projde poslední příkaz
    const TypeInfo *last_expected = used_as_value ? expected_type : NULL;

    if ((*act_statement)->type == AST_IF_EXPR) {
        res = semantic_if_expr(*act_statement, ctx, last_expected, used_as_value);
    } else if ((*act_statement)->type == AST_BLOCK) {
        res = semantic_check_block(*act_statement, ctx, last_expected, used_as_value, true);
    } else {
        res = semantic_visit(act_statement, ctx, last_expected);
    }

    if (res != ERR_OK) {
        if (create_scope) {
            symtable_pop_scope(ctx->scope_stack);
        }
        return semantic_error(ctx, res, block);
    }

    // podle posledního příkazu určí typ bloku
    TypeInfo *block_type = NULL;
    if (semantic_always_returns(block)) {
        block_type = type_create_basic(TYPE_NOTHING);
    } else if (!used_as_value) {
        block_type = type_create_basic(TYPE_UNIT);

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
        if (create_scope) {
            symtable_pop_scope(ctx->scope_stack);
        }
        return semantic_error(ctx, ERR_INTERNAL, block);
    }

    // doplní expr_type
    ast_set_type(block, block_type);

    // uvolní scope co si na začátku vytvořil (pokud ho vytvořit)
    if (create_scope) {
        res = symtable_pop_scope(ctx->scope_stack);
    }

    return semantic_error(ctx, res, block);
}

/* ==================================================
 * FUNKCE PRO VÝRAZOVÉ UZLY AST
 * ================================================== */
ErrorCode semantic_build_function_type(const ASTNode *param_group, const TypeInfo *return_type,
                                       TypeInfo **result) {
    if (return_type == NULL || result == NULL) {
        return ERR_INTERNAL;
    }

    *result = NULL;
    // pokud nemá funkce parametry, je její typ návratový typ
    if (param_group == NULL) {
        *result = type_info_clone(return_type);
        return *result != NULL ? ERR_OK : ERR_INTERNAL;
    }

    if (param_group->type != AST_PARAM_LIST) {
        return ERR_INTERNAL;
    }

    // vytvoří typ následující skupiny
    TypeInfo *inner_type = NULL;
    ErrorCode res = semantic_build_function_type(param_group->next, return_type, &inner_type);
    if (res != ERR_OK) {
        return res;
    }

    // vytvoří seznam typů aktuální skupiny parametrů
    TypeInfo *params = NULL;
    for (ASTNode *param = param_group->data.list.items; param != NULL; param = param->next) {
        // naklonuje typ a přidá na konec seznamu typů
        if (param->type != AST_VAR_DEF || param->expr_type == NULL) {
            if (params != NULL) {
                type_info_free(params);
            }
            if (inner_type != NULL) {
                type_info_free(inner_type);
            }
            return ERR_INTERNAL;
        }

        TypeInfo *copy = type_info_clone(param->expr_type);
        if (copy == NULL) {
            if (params != NULL) {
                type_info_free(params);
            }
            if (inner_type != NULL) {
                type_info_free(inner_type);
            }
            return ERR_INTERNAL;
        }
        params = type_append(params, copy);
    }

    // vytvoří typ funkce z aktuální skupiny
    *result = type_create_function(params, inner_type);

    if (*result == NULL) {
        if (params != NULL) {
            type_info_free(params);
        }
        if (inner_type != NULL) {
            type_info_free(inner_type);
        }
        return ERR_INTERNAL;
    }

    return ERR_OK;
}

ErrorCode semantic_ensure_func_type(SemanticFunctionSymbol *func_sym, SemanticContext *ctx) {
    if (func_sym == NULL || ctx == NULL || func_sym->function == NULL ||
        func_sym->function->type != AST_FUNC_DEF) {
        return semantic_error(ctx, ERR_INTERNAL, func_sym != NULL ? func_sym->function : NULL);
    }

    // zkontroluje stav řešení funkce kvůli chybám a rekurzím a zavolá zpracování funkce
    if (func_sym->state == FUNC_CHECKED || func_sym->state == FUNC_RESOLVED) {
        return func_sym->function->expr_type != NULL ? ERR_OK : ERR_INTERNAL;
    }

    if (func_sym->state == FUNC_RESOLVING) {
        return semantic_error(ctx, ERR_SEM_TYPE, func_sym != NULL ? func_sym->function : NULL);
    }

    return semantic_func_def(func_sym, ctx);
}

ErrorCode semantic_func_def(SemanticFunctionSymbol *func_sym, SemanticContext *ctx) {
    if (func_sym == NULL || ctx == NULL || func_sym->function == NULL ||
        func_sym->function->type != AST_FUNC_DEF) {
        return semantic_error(ctx, ERR_INTERNAL, func_sym != NULL ? func_sym->function : NULL);
    }

    // není co řešit
    if (func_sym->state == FUNC_CHECKED) {
        return ERR_OK;
    }

    // rekurze - problém
    if (func_sym->state == FUNC_RESOLVING) {
        return semantic_error(ctx, ERR_SEM_TYPE, func_sym != NULL ? func_sym->function : NULL);
    }

    ASTNode *function = func_sym->function;
    if (func_sym->state == FUNC_RESOLVED && function->expr_type == NULL) {
        return semantic_error(ctx, ERR_INTERNAL, func_sym != NULL ? func_sym->function : NULL);
    }

    if (function->data.func_def.body == NULL || function->data.func_def.body->type != AST_BLOCK) {
        return semantic_error(ctx, ERR_INTERNAL, func_sym != NULL ? func_sym->function : NULL);
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

    ErrorCode res = ERR_OK;
    TypeInfo *return_type = NULL;
    TypeInfo *function_type = NULL;

    // vytvoření nového scope
    tBtNode *scope = NULL;
    res = symtable_init(&scope);
    if (res != ERR_OK) {
        goto cleanup;
    }
    res = symtable_push_scope(&scope, &function_stack);
    if (res != ERR_OK) {
        goto cleanup;
    }
    if (stack_empty(&function_stack)) {
        res = ERR_INTERNAL;
        goto cleanup;
    }

    // vyřeší extension část
    if (func_sym->extension != NULL) {
        ASTNode *receiver = func_sym->extension;

        if (receiver->type != AST_VAR_DEF || receiver->data.var_def.identifier == NULL ||
            receiver->data.var_def.declared_type == NULL) {
            res = ERR_INTERNAL;
            goto cleanup;
        }

        // vyřeší reciever pokud ještě není vyřešený
        if (receiver->expr_type == NULL) {
            TypeInfo *resolved = NULL;
            res = semantic_resolve_type_alias(receiver->data.var_def.declared_type, ctx, &resolved);
            if (res != ERR_OK) {
                if (resolved != NULL) {
                    type_info_free(resolved);
                }
                goto cleanup;
            }

            if (resolved == NULL) {
                res = ERR_INTERNAL;
                goto cleanup;
            }

            ast_set_type(receiver, resolved);
        }

        // nastaví mu var_id pokud ho nemá
        if (receiver->data.var_def.var_id == 0) {
            receiver->data.var_def.var_id = ctx->next_var_id++;
        }

        // vloží do tabulky
        tBtNodeData receiver_data = {.type = VARIABLE, .value = receiver};
        res = symtable_insert(&stack_top(&function_stack)->scope, receiver->data.var_def.identifier,
                              receiver_data);
        if (res != ERR_OK) {
            goto cleanup;
        }
    }

    // registrace všech skupin parametrů
    for (ASTNode *group = function->data.func_def.param_groups; group != NULL;
         group = group->next) {

        if (group->type != AST_PARAM_LIST) {
            res = ERR_INTERNAL;
            goto cleanup;
        }

        // zaregistruje skupinu parametrů
        res = semantic_register_param_list(group, ctx);
        if (res != ERR_OK) {
            goto cleanup;
        }
    }

    // pokud je dán návratový typ, vyřeší ho a nastaví celkový typ funkce
    if (function->data.func_def.return_type != NULL) {

        res = semantic_resolve_type_alias(function->data.func_def.return_type, ctx, &return_type);
        if (res != ERR_OK) {
            goto cleanup;
        }

        if (return_type == NULL) {
            res = ERR_INTERNAL;
            goto cleanup;
        }

        // vytvoří typ funkce a uloží do expr_type
        res = semantic_build_function_type(function->data.func_def.param_groups, return_type,
                                           &function_type);
        if (res != ERR_OK) {
            goto cleanup;
        }

        ast_set_type(function, function_type);
        function_type = NULL;

        func_sym->state = FUNC_RESOLVED;
    }

    // zpracuje tělo, a pokud nečeká unit zjistí návratový typ
    TypeInfo *expected_body = NULL;

    if (return_type != NULL && return_type->type_class != TYPE_UNIT) {
        expected_body = return_type;
    }

    bool body_used_as_value = return_type == NULL || return_type->type_class != TYPE_UNIT;

    // nastaví pro případ, že blok končí na if bez else
    if (return_type == NULL) {
        res = semantic_fill_unit_else(function->data.func_def.body);
        if (res != ERR_OK) {
            goto cleanup;
        }
    }
    res = semantic_check_block(function->data.func_def.body, ctx, expected_body, body_used_as_value,
                               false);

    if (res != ERR_OK) {
        goto cleanup;
    }

    ASTNode *body = function->data.func_def.body;

    if (body->expr_type == NULL) {
        res = ERR_INTERNAL;
        goto cleanup;
    }

    // urší návratový typ funkce, pokud jsme předtím neznali návratový typ
    if (return_type == NULL) {

        // návratový typ určí z návratu těla
        return_type = type_info_clone(body->expr_type);
        if (return_type == NULL) {
            res = ERR_INTERNAL;
            goto cleanup;
        }

        // vytvoří typ a dá do expr_type
        res = semantic_build_function_type(function->data.func_def.param_groups, return_type,
                                           &function_type);
        if (res != ERR_OK) {
            goto cleanup;
        }

        ast_set_type(function, function_type);
        function_type = NULL;

    } else if (return_type->type_class != TYPE_UNIT) {

        // zontroluje kompatibilitu návratového typu funkce a co vrací body
        if (!semantic_types_compatible(return_type, body->expr_type)) {
            res = ERR_SEM_TYPE;
            goto cleanup;
        }

        // nastaví případnou konverzi
        res = semantic_check_conversion(body, return_type);
        if (res != ERR_OK) {
            goto cleanup;
        }
    }

    // hotovo
    func_sym->state = FUNC_CHECKED;

cleanup:

    // uvolní co je potřeba
    if (return_type != NULL) {
        type_info_free(return_type);
    }
    if (function_type != NULL) {
        type_info_free(function_type);
    }

    while (!stack_empty(&function_stack)) {
        ErrorCode pop_res = symtable_pop_scope(&function_stack);
        if (res == ERR_OK) {
            res = pop_res;
        }
    }

    // obnoví zásobník
    ctx->scope_stack = saved_stack;
    ctx->current_function = saved_function;
    ctx->inside_lambda = saved_lambda;

    return semantic_error(ctx, res, func_sym != NULL ? func_sym->function : NULL);
}

ErrorCode semantic_literal(ASTNode *node) {
    if (node == NULL || node->type != AST_LITERAL) {
        return ERR_INTERNAL;
    }
    TypeClass type;
    // určí typ literálu
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
            return ERR_INTERNAL;
    }

    // vytvoří podle toho typ
    TypeInfo *result = type_create_basic(type);

    if (result == NULL) {
        return ERR_INTERNAL;
    }

    // nastavý do expr_type
    ast_set_type(node, result);

    return ERR_OK;
}

ErrorCode semantic_identifier(ASTNode **node, SemanticContext *ctx) {
    if (node == NULL || *node == NULL || ctx == NULL || (*node)->type != AST_IDENTIFIER ||
        (*node)->data.identifier.name == NULL) {
        return semantic_error(ctx, ERR_INTERNAL, node != NULL ? *node : NULL);
    }

    // najde deklaraci toho identifikátoru
    tBtNode *var = NULL;
    ErrorCode search_res =
        semantic_find_symbol(ctx->scope_stack, (*node)->data.identifier.name, ctx, &var);
    if (search_res != ERR_OK) {
        return semantic_error(ctx, search_res, *node);
    }
    if (var == NULL) {
        return semantic_error(ctx, ERR_SEM_UNDEF, node != NULL ? *node : NULL);
    }

    // pokud to není funkce
    if (var->data->type != FUNCTION) {

        ASTNode *found_node = (ASTNode *)var->data->value;
        TypeInfo *type = NULL;
        int var_id = 0;

        // podle typu to určí type a var_id
        if (found_node->type == AST_VAR_DEF) {
            type = found_node->expr_type;
            var_id = found_node->data.var_def.var_id;

        } else if (found_node->type == AST_PATTERN) {
            if (found_node->data.pattern.pat_type != PATTERN_TYPE_BINDING) {
                return semantic_error(ctx, ERR_INTERNAL, node != NULL ? *node : NULL);
            }

            type = found_node->expr_type;
            var_id = found_node->data.pattern.var_id;

        } else if (found_node->type == AST_CLASS_DEF || found_node->type == AST_TYPE_ALIAS) {
            return semantic_error(ctx, ERR_SEM_TYPE, node != NULL ? *node : NULL);
        } else {
            return semantic_error(ctx, ERR_INTERNAL, node != NULL ? *node : NULL);
        }

        // vytvoří typ a dá do expr_type
        TypeInfo *clone_type = type_info_clone(type);
        if (clone_type == NULL) {
            return semantic_error(ctx, ERR_INTERNAL, node != NULL ? *node : NULL);
        }
        ast_set_type(*node, clone_type);
        (*node)->data.identifier.var_id = var_id;

    } else {
        SemanticFunctionSymbol *found = (SemanticFunctionSymbol *)var->data->value;

        if (found->is_buildin) { // pro vestavěné funkce
            TypeInfo *signature = found->builtin_signatures[0];

            if (signature == NULL) {
                return semantic_error(ctx, ERR_INTERNAL, node != NULL ? *node : NULL);
            }

            // List.range má dvě signatury
            // Bez kontextu nelze určit, kterou použít
            if (found->builtin_signatures[1] != NULL) {
                return semantic_error(ctx, ERR_SEM_TYPE, node != NULL ? *node : NULL);
            }

            // vytvoří typ a doplní
            TypeInfo *type = type_info_clone(signature);
            if (type == NULL) {
                return semantic_error(ctx, ERR_INTERNAL, node != NULL ? *node : NULL);
            }

            ast_set_type(*node, type);
            (*node)->data.identifier.var_id = -1;

            return ERR_OK;
        }

        ASTNode *found_node = found->function;
        if (found_node->type != AST_FUNC_DEF) {
            return semantic_error(ctx, ERR_INTERNAL, node != NULL ? *node : NULL);
        }

        (*node)->data.identifier.var_id = -1;
        ErrorCode res = ERR_OK;
        // vyřeší pro ostatní funkce
        if (found_node->data.func_def.param_groups == NULL) { // nemá parametry
            res = ast_wrap_in_call(node);
            if (res != ERR_OK) {
                return semantic_error(ctx, res, node != NULL ? *node : NULL);
            }

            res = semantic_func_call(*node, ctx, NULL);
            if (res != ERR_OK) {
                return semantic_error(ctx, res, node != NULL ? *node : NULL);
            }
        } else {
            res = semantic_ensure_func_type(found, ctx);
            if (res != ERR_OK) {
                return semantic_error(ctx, res, node != NULL ? *node : NULL);
            }

            if (found_node->expr_type == NULL) {
                return semantic_error(ctx, ERR_INTERNAL, node != NULL ? *node : NULL);
            }

            TypeInfo *clone_type = type_info_clone(found_node->expr_type);
            if (clone_type == NULL) {
                return semantic_error(ctx, ERR_INTERNAL, node != NULL ? *node : NULL);
            }

            ast_set_type(*node, clone_type);
        }
    }
    return ERR_OK;
}

ErrorCode semantic_binary_op(ASTNode *node, SemanticContext *ctx) {
    if (node == NULL || ctx == NULL || node->type != AST_BINARY_OP ||
        node->data.bin_op.left == NULL || node->data.bin_op.right == NULL) {
        return semantic_error(ctx, ERR_INTERNAL, node);
    }

    int op = node->data.bin_op.operator_token;

    // zpracuje uzly na pravé a levé straně operátoru
    ErrorCode res = semantic_visit(&node->data.bin_op.left, ctx, NULL);
    if (res != ERR_OK) {
        return semantic_error(ctx, res, node);
    }
    res = semantic_visit(&node->data.bin_op.right, ctx, NULL);
    if (res != ERR_OK) {
        return semantic_error(ctx, res, node);
    }

    TypeInfo *left = node->data.bin_op.left->expr_type;
    TypeInfo *right = node->data.bin_op.right->expr_type;

    if (left == NULL || right == NULL) {
        return semantic_error(ctx, ERR_INTERNAL, node);
    }
    TypeInfo *res_type;

    // podle typu operátoru a typů operandů určí, jestli je operace v pořádku a jaký je výsledný typ
    if (op == TOK_PLUS || op == TOK_MINUS || op == TOK_MUL || op == TOK_DIV) { // řeší + - * /
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
            return semantic_error(ctx, ERR_SEM_TYPE, node);
        }

    } else if (op == TOK_LESSTHEN || op == TOK_LESSEQ || op == TOK_GREATERTHEN ||
               op == TOK_GREATEREQ) { // řeší < > <= >=
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
            return semantic_error(ctx, ERR_SEM_TYPE, node);
        }

    } else if (op == TOK_EQ || op == TOK_NOTEQ) { // řeší == !=

        if (semantic_types_equal(left, right)) {

            res_type = type_create_basic(TYPE_BOOLEAN);

        } else if (left->type_class == TYPE_INT && right->type_class == TYPE_DOUBLE) {
            node->data.bin_op.left->to_double = true;
            res_type = type_create_basic(TYPE_BOOLEAN);
        } else if (left->type_class == TYPE_DOUBLE && right->type_class == TYPE_INT) {
            node->data.bin_op.right->to_double = true;
            res_type = type_create_basic(TYPE_BOOLEAN);

        } else {
            return semantic_error(ctx, ERR_SEM_TYPE, node);
        }
    } else if (op == TOK_AND || op == TOK_OR) { // řeší && ||
        if (left->type_class == TYPE_BOOLEAN && right->type_class == TYPE_BOOLEAN) {

            res_type = type_create_basic(TYPE_BOOLEAN);
        } else {
            return semantic_error(ctx, ERR_SEM_TYPE, node);
        }
    } else if (op == TOK_CONCAT) { // řeší ++
        if (left->type_class == TYPE_LIST && right->type_class == TYPE_LIST) {
            if (!semantic_types_equal(left, right)) {
                return semantic_error(ctx, ERR_SEM_TYPE, node);
            }
            res_type = type_info_clone(left);
        } else {
            return semantic_error(ctx, ERR_SEM_TYPE, node);
        }
    } else { // neznámý operátor
        return semantic_error(ctx, ERR_INTERNAL, node);
    }
    // zkontroluje, jestli má návratový typ (jeho tvorba proběhla v pořádku)
    if (res_type == NULL) {
        return semantic_error(ctx, ERR_INTERNAL, node);
    }
    // nastaví expr_type
    ast_set_type(node, res_type);

    return ERR_OK;
}

ErrorCode semantic_unary_op(ASTNode *node, SemanticContext *ctx) {
    if (node == NULL || ctx == NULL || node->type != AST_UNARY_OP ||
        node->data.unary_op.operand == NULL) {
        return semantic_error(ctx, ERR_INTERNAL, node);
    }

    // zpracuje uzel operandu
    ErrorCode res = semantic_visit(&node->data.unary_op.operand, ctx, NULL);
    if (res != ERR_OK) {
        return semantic_error(ctx, res, node);
    }

    if (node->data.unary_op.operand->expr_type == NULL) {
        return semantic_error(ctx, ERR_INTERNAL, node);
    }

    // podle druhu operátoru a typu operandu určí, jestli je operace povolená
    switch (node->data.unary_op.operand->expr_type->type_class) {
        case TYPE_INT:
        case TYPE_DOUBLE:
            if (node->data.unary_op.operator_token != TOK_PLUS &&
                node->data.unary_op.operator_token != TOK_MINUS) {
                return semantic_error(ctx, ERR_SEM_TYPE, node);
            }
            break;
        case TYPE_BOOLEAN:
            if (node->data.unary_op.operator_token != TOK_NOT) {
                return semantic_error(ctx, ERR_SEM_TYPE, node);
            }
            break;
        default:
            return semantic_error(ctx, ERR_SEM_TYPE, node);
    }
    // vytvoří výsledný typ (operátor ho nezmění, určí se podle operandu)
    TypeInfo *res_type = type_create_basic(node->data.unary_op.operand->expr_type->type_class);
    if (res_type == NULL) {
        return semantic_error(ctx, ERR_INTERNAL, node);
    }
    // nastavení expr_type
    ast_set_type(node, res_type);
    return ERR_OK;
}

int call_param_group_count(const ASTNode *group) {
    int count = 0;
    // počítá skupiny paramtrů
    for (; group; group = group->next) {
        if (group->type != AST_PARAM_LIST) {
            return -1;
        }
        count++;
    }
    return count;
}

ErrorCode call_builtin_type(const SemanticFunctionSymbol *func, const ASTNode *args,
                            const TypeInfo **result) {
    if (func == NULL || result == NULL || !func->is_buildin) {
        return ERR_INTERNAL;
    }

    *result = NULL;
    if (args == NULL || args->type != AST_ARG_LIST) {
        return ERR_SEM_ARGS;
    }

    // zjistí počet argumentů
    int argc = 0;
    for (const ASTNode *a = args->data.list.items; a; a = a->next) {
        argc++;
    }

    // pro vestavěné funkce, do dvou kvůli List.range
    for (int i = 0; i < 2; ++i) {
        TypeInfo *built_type = func->builtin_signatures[i];
        if (built_type == NULL) {
            continue;
        }
        if (built_type->type_class != TYPE_FUNCTION) {
            return ERR_INTERNAL;
        }

        // spočítá parametry deklarované funkce
        int param_count = 0;
        for (const TypeInfo *param = built_type->data.func_type.param_types; param;
             param = param->next) {
            param_count++;
        }

        // porovná, jestli mají stejný počet parametrů
        if (param_count == argc) {
            *result = built_type;
            return ERR_OK;
        }
    }
    return ERR_SEM_ARGS;
}

// řeší určité vestavěné funkce
ErrorCode call_list_method_type(const char *name, const TypeInfo *list, TypeInfo **out) {
    if (!name || !list || !out || list->type_class != TYPE_LIST || !list->data.list_element_type) {
        return ERR_INTERNAL;
    }
    *out = NULL;

    // rozdělí metody na 2 skupiny - jestli se očekává predikát (funkce vrací boolean) nebo kolik
    // parametrů int očekává funkce
    bool predicate = strcmp(name, "filter") == 0 || strcmp(name, "takeWhile") == 0 ||
                     strcmp(name, "dropWhile") == 0;
    int int_count =
        !strcmp(name, "slice") ? 2 : (!strcmp(name, "drop") || !strcmp(name, "take") ? 1 : 0);

    if (!predicate && !int_count)
        return ERR_SEM_UNDEF;

    TypeInfo *params = NULL;
    TypeInfo *ret = NULL;

    if (predicate) {
        // vytvoří typ funkce, kde je návratový typ boolean
        TypeInfo *arg = type_info_clone(list->data.list_element_type);
        if (!arg)
            return ERR_INTERNAL;
        TypeInfo *boolean = type_create_basic(TYPE_BOOLEAN);
        if (!boolean) {
            type_info_free(arg);
            return ERR_INTERNAL;
        }
        params = type_create_function(arg, boolean);
        if (!params) {
            type_info_free(arg);
            type_info_free(boolean);
            return ERR_INTERNAL;
        }
    } else {
        // vytvoří se daný počet int typů do seznamu
        for (int i = 0; i < int_count; i++) {
            TypeInfo *param_type = type_create_basic(TYPE_INT);
            if (!param_type) {
                type_info_free(params);
                type_info_free(ret);
                return ERR_INTERNAL;
            }
            params = type_append(params, param_type);
        }
    }

    // sestaví celý typ a vrátí v out
    ret = type_info_clone(list);
    if (!ret) {
        type_info_free(params);
        type_info_free(ret);
        return ERR_INTERNAL;
    }
    *out = type_create_function(params, ret);
    if (!*out) {
        type_info_free(params);
        type_info_free(ret);
        return ERR_INTERNAL;
    }
    return ERR_OK;
}

ErrorCode call_constructor_type(ASTNode *class_def, TypeInfo **res_type) {
    if (!class_def || !res_type || class_def->type != AST_CLASS_DEF) {
        return ERR_INTERNAL;
    }
    // projde parametry třídy a vytvoří seznam jejich typů
    *res_type = NULL;
    TypeInfo *params = NULL;
    for (const ASTNode *field = class_def->data.class_def.fields; field != NULL;
         field = field->next) {
        if (field->type != AST_VAR_DEF || !field->expr_type) {
            type_info_free(params);
            return ERR_INTERNAL;
        }
        TypeInfo *param_type = type_info_clone(field->expr_type);
        if (!param_type) {
            type_info_free(params);
            return ERR_INTERNAL;
        }
        params = type_append(params, param_type);
    }

    // vytvoří typ třídy
    TypeInfo *ret = type_create_custom(class_def->data.class_def.name);
    if (ret == NULL) {
        type_info_free(params);
        return ERR_INTERNAL;
    }

    // pomocí těch dvou vytvoří typ pro funkci
    *res_type = type_create_function(params, ret);
    if (*res_type == NULL) {
        type_info_free(params);
        type_info_free(ret);
        return ERR_INTERNAL;
    }
    return ERR_OK;
}

ErrorCode call_prepare_target(ASTNode *call, SemanticContext *ctx, CallKind *kind,
                              int *own_groups) {
    if (call == NULL || ctx == NULL || kind == NULL || own_groups == NULL ||
        call->type != AST_FUNC_CALL || !call->data.func_call.target) {
        return semantic_error(ctx, ERR_INTERNAL, call);
    }
    *kind = CALL_UNRESOLVED;
    *own_groups = 0;
    ASTNode **slot = &call->data.func_call.target;
    ASTNode *target = *slot;
    TypeInfo *owned = NULL;
    ErrorCode err;

    // když je cílem identifikátor
    if (target->type == AST_IDENTIFIER) {
        char *name = target->data.identifier.name;
        if (!name) {
            return semantic_error(ctx, ERR_INTERNAL, call);
        }

        // vyhledá v tabulce symbol
        tBtNode *entry = NULL;
        if (ctx->scope_stack) {
            err = semantic_find_symbol(ctx->scope_stack, name, ctx, &entry);
        } else {
            err = symtable_search(&ctx->symbols, name, &entry);
        }

        // kontroly
        if (err != ERR_OK) {
            return semantic_error(ctx, err, target);
        }
        if (!entry) {
            return semantic_error(ctx, ERR_SEM_UNDEF, call);
        }
        if (!entry->data || !entry->data->value) {
            return semantic_error(ctx, ERR_INTERNAL, call);
        }

        // podle typu nalezeného symbolu se určí co dál
        if (entry->data->type == FUNCTION) {
            SemanticFunctionSymbol *func = entry->data->value;
            const TypeInfo *signature = NULL;

            // pro vestavěné funkce vytvoří typ
            if (func->is_buildin) {
                err = call_builtin_type(func, call->data.func_call.arg_groups, &signature);
                if (err) {
                    return semantic_error(ctx, err, call);
                }
                *kind = CALL_BUILTIN;
                *own_groups = 1;
            } else {
                if (!func->function || func->function->type != AST_FUNC_DEF) {
                    return semantic_error(ctx, ERR_INTERNAL, call);
                }

                // zjistí se typ
                err = semantic_ensure_func_type(func, ctx);
                if (err) {
                    return semantic_error(ctx, err, call);
                }
                signature = func->function->expr_type;

                // zjistí počet skupin parametrů
                *own_groups = call_param_group_count(func->function->data.func_def.param_groups);
                if (*own_groups < 0) {
                    return semantic_error(ctx, ERR_INTERNAL, call);
                }

                // nastaví typ volání
                *kind = CALL_USER_FUNC;
            }

            if (!signature) {
                return semantic_error(ctx, ERR_INTERNAL, call);
            }
            // udělá se kopie typu a uloží do expr_type
            owned = type_info_clone(signature);
            if (!owned) {
                return semantic_error(ctx, ERR_INTERNAL, call);
            }
            ast_set_type(target, owned);
            target->data.identifier.var_id = -1;
            return ERR_OK;
        }

        // pro třídu vytvoří její typ a doplní
        if (entry->data->type == CLASS) {
            ASTNode *class_node = entry->data->value;

            err = call_constructor_type(class_node, &owned);
            if (err) {
                return semantic_error(ctx, err, call);
            }
            ast_set_type(target, owned);
            target->data.identifier.var_id = -1;
            *kind = CALL_CONSTRUCTOR;
            *own_groups = 1;
            return ERR_OK;
        }

        // alias ani proměnná nejde volat jako funkci
        if (entry->data->type == ALIAS) {
            return semantic_error(ctx, ERR_SEM_TYPE, call);
        }
        if (entry->data->type != VARIABLE) {
            return semantic_error(ctx, ERR_INTERNAL, call);
        }
    }

    if (target->type == AST_MEMBER_ACCESS) {
        ASTNode *member = target;
        ASTNode **object = &member->data.member_access.object;
        if (!*object || !member->data.member_access.member_name) {
            return semantic_error(ctx, ERR_INTERNAL, call);
        }

        // zpracuje se objekt před tečkou
        err = (*object)->type == AST_RANGE ? semantic_range(*object, ctx)
                                           : semantic_visit(object, ctx, NULL);
        if (err) {
            return semantic_error(ctx, err, call);
        }

        SemanticFunctionSymbol *method = NULL;
        const TypeInfo *object_type = (*object)->expr_type;

        if (object_type && object_type->type_class == TYPE_LIST) {
            // pro list zkouší vestavěné funkce, pokud není vestavěná pokračuje dál
            err =
                call_list_method_type(member->data.member_access.member_name, object_type, &owned);
            if (err == ERR_OK) {
                member->data.member_access.member_kind = MEMBER_LIST_BUILTIN;
                *kind = CALL_LIST_METHOD;
                *own_groups = 1;
            } else if (err != ERR_SEM_UNDEF) {
                return semantic_error(ctx, err, call);
            }
        }

        if (*kind != CALL_LIST_METHOD) {
            // zjistí jestli je člen povolený, jaký má typ a jestli je to extension metoda
            err = is_mem_acc_allowed(member, ctx, &owned, &method);
            if (err) {
                type_info_free(owned);
                return semantic_error(ctx, err, call);
            }

            // pokud je to extension, spočítá počet skupin parametrů, nastaví kind
            if (member->data.member_access.member_kind == MEMBER_EXTENSION) {
                if (!method || !method->function || method->function->type != AST_FUNC_DEF) {
                    type_info_free(owned);
                    return semantic_error(ctx, ERR_INTERNAL, call);
                }

                *own_groups = call_param_group_count(method->function->data.func_def.param_groups);
                if (*own_groups < 0) {
                    type_info_free(owned);
                    return semantic_error(ctx, ERR_INTERNAL, call);
                }
                *kind = CALL_EXTENSION;
            } else {
                *kind = CALL_VALUE;
            }
        }
        if (!owned) {
            return semantic_error(ctx, ERR_INTERNAL, call);
        }

        // typ se uloží
        ast_set_type(member, owned);
        return ERR_OK;
    }

    // proměnná, lambda, nebo jiný výraz hodnoty
    err = semantic_visit(slot, ctx, NULL);
    if (err) {
        return semantic_error(ctx, err, call);
    }
    if (!(*slot)->expr_type) {
        return semantic_error(ctx, ERR_INTERNAL, call);
    }
    *kind = CALL_VALUE;
    return ERR_OK;
}

ErrorCode call_apply_group(ASTNode *group, SemanticContext *ctx, const TypeInfo *current,
                           TypeInfo **next) {
    if (!group || group->type != AST_ARG_LIST || !ctx || !current || !next) {
        return ERR_INTERNAL;
    }

    *next = NULL;
    const TypeInfo *ret = NULL;
    ErrorCode err;

    // pokud je to typ funkce, zkontroluje seznam argumentů
    if (current->type_class == TYPE_FUNCTION) {
        err = semantic_check_agr_list(group, ctx, current->data.func_type.param_types);
        ret = current->data.func_type.return_type;

    } else if (current->type_class == TYPE_LIST) {
        // kontroluje seznam indexů, a že jsou typu int
        TypeInfo index = {.type_class = TYPE_INT};
        err = semantic_check_agr_list(group, ctx, &index);
        ret = current->data.list_element_type;

    } else {
        return ERR_SEM_ARGS;
    }

    if (err != ERR_OK) {
        return err;
    }
    if (!ret) {
        return ERR_INTERNAL;
    }

    // vytvoří výsledný typ a dá do expr_type
    TypeInfo *copy = type_info_clone(ret); // k uložení
    if (!copy) {
        return ERR_INTERNAL;
    }

    *next = type_info_clone(ret); // k předání pro další zpracování
    if (!*next) {
        type_info_free(copy);
        return ERR_INTERNAL;
    }

    ast_set_type(group, copy);
    return ERR_OK;
}

ErrorCode semantic_func_call(ASTNode *node, SemanticContext *ctx, const TypeInfo *expected_type) {

    (void)expected_type; // kontrolu kompatibility provádí rodič

    if (!node || !ctx || node->type != AST_FUNC_CALL || !node->data.func_call.target) {
        return semantic_error(ctx, ERR_INTERNAL, node);
    }
    if (node->data.func_call.call_kind != CALL_UNRESOLVED) {
        return node->expr_type ? ERR_OK : ERR_INTERNAL;
    }

    CallKind kind;
    int own_groups;

    // připraví se cíl
    ErrorCode err = call_prepare_target(node, ctx, &kind, &own_groups);
    if (err) {
        return semantic_error(ctx, err, node);
    }

    // udělá kopie typu cíle volání
    TypeInfo *current = type_info_clone(node->data.func_call.target->expr_type);
    if (!current) {
        return semantic_error(ctx, ERR_INTERNAL, node);
    }

    // volání funkční hodnoty musí obsahovat alespoň jednu skupinu argumentů
    ASTNode *group = node->data.func_call.arg_groups;
    if (kind == CALL_VALUE && !group) {
        err = ERR_SEM_ARGS;
        goto cleanup;
    }

    // zpracování skupin argumentů
    int used = 0;
    for (; group; group = group->next, used++) {
        if (group->type != AST_ARG_LIST) {
            err = ERR_INTERNAL;
            goto cleanup;
        }

        if (used < own_groups && current->type_class != TYPE_FUNCTION) {
            err = ERR_INTERNAL;
            goto cleanup;
        }

        TypeInfo *next = NULL;
        err = call_apply_group(group, ctx, current, &next);

        if (err) {
            type_info_free(next);
            goto cleanup;
        }

        type_info_free(current);
        current = next;
    }

    // kontrola počtu skupin
    if (used < own_groups) {
        err = ERR_SEM_ARGS;
        goto cleanup;
    }

    // nastaví typ
    ast_set_type(node, current);
    node->data.func_call.call_kind = kind;
    node->data.func_call.own_groups = own_groups;
    return ERR_OK;

cleanup:
    type_info_free(current);
    return semantic_error(ctx, err, node);
}

ErrorCode semantic_extension_member(ASTNode *node, SemanticContext *ctx, TypeInfo **result,
                                    SemanticFunctionSymbol **matched_method) {
    if (node == NULL || ctx == NULL || result == NULL || matched_method == NULL ||
        node->type != AST_MEMBER_ACCESS || node->data.member_access.object == NULL ||
        node->data.member_access.member_name == NULL) {
        return semantic_error(ctx, ERR_INTERNAL, node);
    }

    *result = NULL;
    *matched_method = NULL;

    char *name = node->data.member_access.member_name;
    const TypeInfo *object_type = node->data.member_access.object->expr_type;

    if (object_type == NULL) {
        return semantic_error(ctx, ERR_INTERNAL, node);
    }

    // najde metodu podle názvu
    tBtNode *symbol = NULL;
    ErrorCode search_res = symtable_search(&ctx->ext_symbols, name, &symbol);
    if (search_res != ERR_OK) {
        return semantic_error(ctx, search_res, node);
    }

    if (symbol == NULL) {
        return semantic_error(ctx, ERR_SEM_UNDEF, node);
    }

    if (symbol->data == NULL || symbol->data->type != FUNCTION || symbol->data->value == NULL) {
        return semantic_error(ctx, ERR_INTERNAL, node);
    }

    // u extension může být víc metod stejného jména (v různých extension) - musí projít spojový
    // seznam těchto metod
    for (SemanticFunctionSymbol *method = symbol->data->value; method != NULL;
         method = method->next) {

        const ASTNode *receiver = method->extension;
        ASTNode *function = method->function;

        if (receiver == NULL || receiver->type != AST_VAR_DEF || function == NULL ||
            function->type != AST_FUNC_DEF) {
            return semantic_error(ctx, ERR_INTERNAL, node);
        }

        // zjistí typ příjemce
        const TypeInfo *receiver_type = receiver->expr_type;
        TypeInfo *temporary_type = NULL;

        // pokud nebyl vyplněný, rozvine alias z deklarovaného typu
        if (receiver_type == NULL) {
            if (receiver->data.var_def.declared_type == NULL) {
                return semantic_error(ctx, ERR_INTERNAL, node);
            }

            ErrorCode error = semantic_resolve_type_alias(receiver->data.var_def.declared_type, ctx,
                                                          &temporary_type);

            if (error != ERR_OK) {
                return semantic_error(ctx, error, node);
            }

            if (temporary_type == NULL) {
                return semantic_error(ctx, ERR_INTERNAL, node);
            }

            receiver_type = temporary_type;
        }

        bool matches = semantic_types_equal(object_type, receiver_type);

        // pokud se vytvářel typ (po rozvinutí aliasu), uvolní se
        if (temporary_type != NULL) {
            type_info_free(temporary_type);
        }

        // když to není hledaná metoda, pokračuje do dalšího kola cyklu
        if (!matches) {
            continue;
        }

        // zjistí signaturu metody
        ErrorCode error = semantic_ensure_func_type(method, ctx);
        if (error != ERR_OK) {
            return semantic_error(ctx, error, node);
        }

        if (function->expr_type == NULL) {
            return semantic_error(ctx, ERR_INTERNAL, node);
        }

        // naklonuje typ signatury metody a dá ro result, co se vrací
        *result = type_info_clone(function->expr_type);

        if (*result == NULL) {
            return semantic_error(ctx, ERR_INTERNAL, node);
        }

        // vrací ukazatel na SemanticFunctionSymbol
        *matched_method = method;

        node->data.member_access.member_kind = MEMBER_EXTENSION;

        return ERR_OK;
    }

    // nenašlo odpovídající metodu
    return semantic_error(ctx, ERR_SEM_UNDEF, node);
}

ErrorCode is_mem_acc_allowed(ASTNode *node, SemanticContext *ctx, TypeInfo **result,
                             SemanticFunctionSymbol **matched_method) {
    if (node == NULL || ctx == NULL || result == NULL || matched_method == NULL ||
        node->type != AST_MEMBER_ACCESS || node->data.member_access.object == NULL ||
        node->data.member_access.member_name == NULL) {
        return semantic_error(ctx, ERR_INTERNAL, node);
    }

    *result = NULL;
    *matched_method = NULL;

    ASTNode *object = node->data.member_access.object;
    const char *name = node->data.member_access.member_name;
    const TypeInfo *object_type = object->expr_type;

    // vyřeší případ vestavěných funkcí, kdy jde o AST_RANGE (object_type== NULL)
    if (object->type == AST_RANGE) {
        // pouze jedna možnost toList, vrací list typu int
        if (strcmp(name, "toList") != 0) {
            return semantic_error(ctx, ERR_SEM_UNDEF, node);
        }

        // vytvoří výsledný typ a dosadí do result
        TypeInfo *element = type_create_basic(TYPE_INT);
        if (element == NULL) {
            return semantic_error(ctx, ERR_INTERNAL, node);
        }

        *result = type_create_list(element);
        if (*result == NULL) {
            type_info_free(element);
            return semantic_error(ctx, ERR_INTERNAL, node);
        }

        // nastaví member_kind a vrátí se
        node->data.member_access.member_kind = MEMBER_RANGE_TOLIST;
        return ERR_OK;
    }

    if (object_type == NULL) {
        return semantic_error(ctx, ERR_INTERNAL, node);
    }

    // vyřeší, když je object typu custom
    TypeClass receiver = object_type->type_class;
    if (receiver == TYPE_CUSTOM) {
        // najde jeho definici v globálních symbolech
        tBtNode *symbol = NULL;
        ErrorCode search_res =
            symtable_search(&ctx->symbols, object_type->data.custom_name, &symbol);
        if (search_res != ERR_OK) {
            return semantic_error(ctx, search_res, node);
        }
        if (symbol == NULL) {
            return semantic_error(ctx, ERR_SEM_UNDEF, node);
        }
        if (symbol->data == NULL || symbol->data->type != CLASS) {
            return semantic_error(ctx, ERR_INTERNAL, node);
        }

        const ASTNode *class_def = symbol->data->value; // čeká, že to bude třída
        if (class_def == NULL || class_def->type != AST_CLASS_DEF) {
            return semantic_error(ctx, ERR_INTERNAL, node);
        }

        // projde atributy třídy a hledá, jestli má proměnnou názvu jako name
        for (const ASTNode *field = class_def->data.class_def.fields; field != NULL;
             field = field->next) {
            if (field->type != AST_VAR_DEF || field->data.var_def.identifier == NULL) {
                return semantic_error(ctx, ERR_INTERNAL, node);
            }
            // když to není správný parametr, pokračuj do dalšího cyklu
            if (strcmp(name, field->data.var_def.identifier) != 0) {
                continue;
            }

            // zž mám správný parametr, naklonuje se jeho typ pokud ho má
            if (field->expr_type != NULL) {
                *result = type_info_clone(field->expr_type);
            } else {
                // pokud ho v expr_type nemá, zjistí se z deklarovaného typu
                ErrorCode error =
                    semantic_resolve_type_alias(field->data.var_def.declared_type, ctx, result);
                if (error != ERR_OK) {
                    return semantic_error(ctx, error, node);
                }
            }
            if (*result == NULL) {
                return semantic_error(ctx, ERR_INTERNAL, node);
            }

            // nastaví member_kind
            node->data.member_access.member_kind = MEMBER_FIELD;
            return ERR_OK;
        }
    }

    // projde vestavěné metody
    MemberKind kind = MEMBER_VALUE_BUILTIN;
    if (receiver == TYPE_LIST) {
        kind = MEMBER_LIST_BUILTIN;
    }
    node->data.member_access.member_kind = kind;

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
        // pokud to není mezi vestavěnými funkcemi, zkontroluje extension metody
        return semantic_extension_member(node, ctx, result, matched_method);
    }

    if (*result == NULL) {
        return semantic_error(ctx, ERR_INTERNAL, node);
    }
    return ERR_OK;
}

ErrorCode semantic_member_access(ASTNode **node, SemanticContext *ctx) {
    if (node == NULL || *node == NULL || ctx == NULL || (*node)->type != AST_MEMBER_ACCESS) {
        return semantic_error(ctx, ERR_INTERNAL, node != NULL ? *node : NULL);
    }

    ASTNode *member = *node;

    // pokud už byl member_access vyřešen, nekontroluje se znovu
    if (member->data.member_access.member_kind != MEMBER_UNRESOLVED) {
        return member->expr_type != NULL ? ERR_OK : ERR_INTERNAL;
    }

    ASTNode **object = &member->data.member_access.object;

    if (*object == NULL || member->data.member_access.member_name == NULL) {
        return semantic_error(ctx, ERR_INTERNAL, node != NULL ? *node : NULL);
    }

    ErrorCode error;

    // vyřešení range (nemá expr_type)
    if ((*object)->type == AST_RANGE) {
        error = semantic_range(*object, ctx);
    } else {
        // pokud to není range, zpracuje část před .
        error = semantic_visit(object, ctx, NULL);
    }

    if (error != ERR_OK) {
        return semantic_error(ctx, error, node != NULL ? *node : NULL);
    }

    TypeInfo *result = NULL;
    SemanticFunctionSymbol *method = NULL;

    // zjistí o jaký druh členu jde a jaký má typ
    error = is_mem_acc_allowed(member, ctx, &result, &method);

    if (error != ERR_OK) {
        type_info_free(result);
        return semantic_error(ctx, error, node != NULL ? *node : NULL);
    }

    if (result == NULL) {
        return semantic_error(ctx, ERR_INTERNAL, node != NULL ? *node : NULL);
    }

    // pokud to není metoda extension
    if (member->data.member_access.member_kind != MEMBER_EXTENSION) {
        ast_set_type(member, result);
        return ERR_OK;
    }

    // extension musí mít přiřazenou funci
    if (method == NULL || method->function == NULL) {
        type_info_free(result);
        return semantic_error(ctx, ERR_INTERNAL, node != NULL ? *node : NULL);
    }

    // pouze pro extension metody bez parametrů
    if (method->function->data.func_def.param_groups != NULL) {
        type_info_free(result);
        return semantic_error(ctx, ERR_SEM_ARGS, node != NULL ? *node : NULL);
    }

    // vytvoří klon typu result, typ result bude potřeba pro 2 různé uzly
    TypeInfo *call_type = type_info_clone(result);

    if (call_type == NULL) {
        type_info_free(result);
        return semantic_error(ctx, ERR_INTERNAL, node != NULL ? *node : NULL);
    }

    // zabalí metodu jako volání funkce
    error = ast_wrap_in_call(node);

    if (error != ERR_OK) {
        type_info_free(result);
        type_info_free(call_type);
        return semantic_error(ctx, error, node != NULL ? *node : NULL);
    }

    ASTNode *call = *node;
    ASTNode *target = call->data.func_call.target;

    // nastaví typ metody
    ast_set_type(target, result);

    // nastaví typ celého volání
    ast_set_type(call, call_type);

    call->data.func_call.call_kind = CALL_EXTENSION;
    call->data.func_call.own_groups = ERR_OK;

    return ERR_OK;
}

// hotova
ErrorCode semantic_list_literal(ASTNode *node, SemanticContext *ctx,
                                const TypeInfo *expected_type) {
    if (node == NULL || ctx == NULL || node->type != AST_LIST_LITERAL) {
        return semantic_error(ctx, ERR_INTERNAL, node);
    }

    const TypeInfo *expected_list = NULL;

    if (expected_type != NULL && expected_type->type_class == TYPE_LIST) {
        expected_list = expected_type;

        if (expected_list->data.list_element_type == NULL) {
            return semantic_error(ctx, ERR_INTERNAL, node);
        }
    }

    ErrorCode res = ERR_OK;
    TypeInfo *type = NULL;
    if (node->data.list.items == NULL) {
        // pokud je seznam prázdný, bude mít typ podle expected_list
        if (expected_list == NULL) {
            return semantic_error(ctx, ERR_SEM_TYPE, node);
        }
        type = type_info_clone(expected_list);
        if (type == NULL) {
            return semantic_error(ctx, ERR_INTERNAL, node);
        }
    } else {
        // projde všechny prvky seznamu
        ASTNode **actual = &node->data.list.items;
        TypeInfo *list_type = NULL;
        while (*actual != NULL) {
            // zpracuje prvek seznamu
            res = semantic_visit(
                actual, ctx, expected_list != NULL ? expected_list->data.list_element_type : NULL);
            if (res != ERR_OK) {
                if (list_type != NULL) {
                    type_info_free(list_type);
                }
                return semantic_error(ctx, res, node);
            }

            // pokud není zadaný expected type, určí si ho podle společného typu aktuálního a
            // předchozího prvku(pokud předchozí prvek není, je to typ pouze tohoto prvku)
            if (expected_list == NULL) {
                if (list_type == NULL) {
                    list_type = type_info_clone((*actual)->expr_type);

                    if (list_type == NULL) {
                        return semantic_error(ctx, ERR_INTERNAL, node);
                    }
                } else {
                    TypeInfo *comm_type = NULL;
                    res = semantic_common_type((*actual)->expr_type, list_type, &comm_type);
                    if (res != ERR_OK) {
                        type_info_free(list_type);
                        return semantic_error(ctx, res, node);
                    }
                    if (comm_type == NULL) {
                        type_info_free(list_type);
                        return semantic_error(ctx, ERR_INTERNAL, node);
                    }

                    type_info_free(list_type);
                    list_type = comm_type;
                }
            } else { // expected_list  je zadán, stačí ověřit kompatibilitu prvku s ním
                if (!semantic_types_compatible(expected_list->data.list_element_type,
                                               (*actual)->expr_type)) {
                    return semantic_error(ctx, ERR_SEM_TYPE, node);
                }
            }
            actual = &(*actual)->next;
        }

        // doplní si typ, který by měl seznam mít
        if (expected_list != NULL) {
            type = type_info_clone(expected_list);
            if (type == NULL) {
                return semantic_error(ctx, ERR_INTERNAL, node);
            }

        } else {
            type = type_create_list(list_type);
            if (type == NULL) {
                type_info_free(list_type);
                return semantic_error(ctx, ERR_INTERNAL, node);
            }
        }

        // znovu projde seznam prvků, u každého zkontroluje kompatibilitu s type a případně nastaví
        // to_double
        actual = &node->data.list.items;
        while (*actual != NULL) {

            if (!semantic_types_compatible(type->data.list_element_type, (*actual)->expr_type)) {
                type_info_free(type);
                return semantic_error(ctx, ERR_SEM_TYPE, node);
            }
            res = semantic_check_conversion((*actual), type->data.list_element_type);
            if (res != ERR_OK) {
                type_info_free(type);
                return semantic_error(ctx, res, node);
            }
            actual = &(*actual)->next;
        }
    }

    // nastavý expr_type
    ast_set_type(node, type);
    return ERR_OK;
}

// hotovo, zkontrolovat si chybové kódy
ErrorCode semantic_assignment(ASTNode *node, SemanticContext *ctx) {
    if (node == NULL || ctx == NULL) {
        return semantic_error(ctx, ERR_INTERNAL, node);
    }

    // najde definici proměnné na levé straně přiřazení, pokud není, chyba
    tBtNode *definition = NULL;
    ErrorCode search_res =
        semantic_find_symbol(ctx->scope_stack, node->data.assignment.target, ctx, &definition);
    if (search_res != ERR_OK) {
        return semantic_error(ctx, search_res, node);
    }
    if (definition == NULL || definition->data->type != VARIABLE) {
        return semantic_error(ctx, ERR_SEM_UNAUTH_WRITE, node); // nedefinovaná proměnná
    }

    ASTNode *tar_var = (ASTNode *)definition->data->value;
    if (tar_var->type != AST_VAR_DEF) {
        return semantic_error(ctx, ERR_SEM_UNAUTH_WRITE, node); // na levé straně není proměnná
    }

    if (tar_var->data.var_def.is_mutable != true) {
        return semantic_error(ctx, ERR_SEM_UNAUTH_WRITE,
                              node); // proměnná není modifikovatelná, nelze přiřadit
    }

    // vyřeší pravou stranu přiazení
    ErrorCode res = semantic_visit(&node->data.assignment.value, ctx, tar_var->expr_type);
    if (res != ERR_OK) {
        return semantic_error(ctx, res, node);
    }

    // kontrola kompatibility typů
    if (!semantic_types_compatible(tar_var->expr_type, node->data.assignment.value->expr_type)) {
        return semantic_error(ctx, ERR_SEM_TYPE, node);
    }
    // případné nastavení konverze to_double
    res = semantic_check_conversion(node->data.assignment.value, tar_var->expr_type);
    if (res != ERR_OK) {
        return semantic_error(ctx, res, node);
    }

    // nastavení var_id podle proměnné a expr_type (unit - přiřazení typ nemá)
    node->data.assignment.var_id = tar_var->data.var_def.var_id;
    TypeInfo *type = type_create_basic(TYPE_UNIT);
    if (type == NULL) {
        return semantic_error(ctx, ERR_INTERNAL, node);
    }

    ast_set_type(node, type);
    return ERR_OK;
}

/* ==================================================
 * DEKLARACE
 * ================================================== */

ErrorCode semantic_var_def(tStack *scope, ASTNode *node, SemanticContext *ctx, bool is_param) {
    // úvodní kontroly
    if (node == NULL || ctx == NULL || node->type != AST_VAR_DEF ||
        (!is_param && node->data.var_def.initializer == NULL) ||
        (is_param && node->data.var_def.initializer != NULL) || scope == NULL ||
        stack_top(scope) == NULL) {
        return semantic_error(ctx, ERR_INTERNAL, node);
    }

    tBtNode *exists = NULL;
    // zkontroluje, jestli v aktuálním scope už není definice proměnné stejného jména
    ErrorCode search_res =
        symtable_search(&stack_top(scope)->scope, node->data.var_def.identifier, &exists);
    if (search_res != ERR_OK) {
        return semantic_error(ctx, search_res, node);
    }
    if (exists != NULL) {
        // nejake uvolnovani
        return semantic_error(ctx, ERR_SEM_REDEF, node);
    }

    if (node->data.var_def.declared_type != NULL) {
        // rozvine alias deklarovaného typu
        TypeInfo *exp_type = NULL;
        ErrorCode resolved_res =
            semantic_resolve_type_alias(node->data.var_def.declared_type, ctx, &exp_type);
        if (resolved_res != ERR_OK) {
            if (exp_type != NULL) {
                type_info_free(exp_type);
            }
            return semantic_error(ctx, resolved_res, node);
        }

        // pokud je definice proměnné parametrem, nastaví typ na deklarovaný
        if (is_param) {
            ast_set_type(node, exp_type);
        } else {
            // zpracuje inicializér proměnné
            ErrorCode result = semantic_visit(&node->data.var_def.initializer, ctx, exp_type);

            if (result != ERR_OK) {
                if (exp_type != NULL) {
                    type_info_free(exp_type);
                }
                return semantic_error(ctx, result, node);
            }

            TypeInfo *act_type = node->data.var_def.initializer->expr_type;

            // ověří kompatibilitu tzpů inicializéru a deklarovaného typu
            if (!semantic_types_compatible(exp_type, act_type)) {
                type_info_free(exp_type);
                return semantic_error(ctx, ERR_SEM_TYPE, node);
            }
            // provede případnou konverzi
            result = semantic_check_conversion(node->data.var_def.initializer, exp_type);

            if (result != ERR_OK) {
                type_info_free(exp_type);
                return semantic_error(ctx, result, node);
            }

            // nastaví expr_type
            ast_set_type(node, exp_type);
        }

    } else {            // pokud nemám deklarovaný typ
        if (is_param) { // definice parametru musí mít deklarovaný typ
            return semantic_error(ctx, ERR_SEM_TYPE, node);
        }
        // zpracuje inicializer
        ErrorCode result = semantic_visit(&node->data.var_def.initializer, ctx, NULL);

        if (result != ERR_OK) {
            return semantic_error(ctx, result, node);
        }

        ASTNode *initializer = node->data.var_def.initializer;
        if (initializer->type == AST_IDENTIFIER && initializer->data.identifier.var_id == -1 &&
            initializer->expr_type != NULL && initializer->expr_type->type_class == TYPE_FUNCTION) {
            return semantic_error(ctx, ERR_SEM_TYPE, node);
        }

        // jeho typ nastaví jako expr_type
        TypeInfo *act_type = type_info_clone(node->data.var_def.initializer->expr_type);
        if (act_type == NULL) {
            return semantic_error(ctx, ERR_INTERNAL, node);
        }
        ast_set_type(node, act_type);
    }
    // nastaví var_id
    node->data.var_def.var_id = ctx->next_var_id;
    ctx->next_var_id++;

    // vloží do aktuálního scope
    tBtNodeData node_symtab = {.value = node, .type = VARIABLE};

    return semantic_error(
        ctx, symtable_insert(&stack_top(scope)->scope, node->data.var_def.identifier, node_symtab),
        node);
}

// hotova
ErrorCode semantic_class_def(ASTNode *node, SemanticContext *ctx) {
    if (node == NULL || ctx == NULL || node->type != AST_CLASS_DEF) {
        return semantic_error(ctx, ERR_INTERNAL, node);
    }

    ASTNode *field = node->data.class_def.fields;
    while (field != NULL) {
        // kontrola jestli má uzel správný tvar
        if (field->type != AST_VAR_DEF || field->data.var_def.declared_type == NULL ||
            field->data.var_def.initializer != NULL || field->data.var_def.is_mutable) {
            return semantic_error(ctx, ERR_INTERNAL, node);
        }

        // zkonroluje, jestli už před ním nebylo v této třídě jméno takovéto proměnné použito
        ASTNode *previous = node->data.class_def.fields;
        while (previous != field) {
            if (previous->data.var_def.identifier != NULL &&
                field->data.var_def.identifier != NULL) {
                if (strcmp(previous->data.var_def.identifier, field->data.var_def.identifier) ==
                    0) {
                    return semantic_error(ctx, ERR_SEM_REDEF, node);
                }
            }

            previous = previous->next;
        }

        TypeInfo *resolved_type = NULL;
        // zjistí a rozvine deklarovaný tvar proměnné
        ErrorCode res =
            semantic_resolve_type_alias(field->data.var_def.declared_type, ctx, &resolved_type);

        if (res != ERR_OK) {
            if (resolved_type != NULL) {
                type_info_free(resolved_type);
            }
            return semantic_error(ctx, res, node);
        }

        if (resolved_type == NULL) {
            return semantic_error(ctx, ERR_INTERNAL, node);
        }

        // oložího do expr_type proměnné
        ast_set_type(field, resolved_type);

        // atributy třídy mají var_id -1
        field->data.var_def.var_id = -1;
        field = field->next;
    }

    return ERR_OK;
}

// hotovo
ErrorCode semantic_type_alias(ASTNode *node, SemanticContext *ctx) {
    if (node == NULL || ctx == NULL || node->type != AST_TYPE_ALIAS) {
        return semantic_error(ctx, ERR_INTERNAL, node);
    }

    // rozloží alias
    TypeInfo *res_type = NULL;
    ErrorCode res = semantic_resolve_type_alias(node->data.type_alias.aliased, ctx, &res_type);
    if (res != ERR_OK) {
        return semantic_error(ctx, res, node);
    }

    // nastaví typ na rozložený alias
    ast_set_type(node, res_type);
    return ERR_OK;
}

SemanticFunctionSymbol *find_extension_symbol(ASTNode *function, SemanticContext *ctx) {
    if (function == NULL || ctx == NULL || function->type != AST_FUNC_DEF ||
        function->data.func_def.name == NULL) {
        return NULL;
    }

    tBtNode *symbol = NULL;

    // prohledá tabulku extension metod, jestli už obsahuje metodu s jménem této funkce
    if (symtable_search(&ctx->ext_symbols, function->data.func_def.name, &symbol) != ERR_OK) {
        semantic_error(ctx, ERR_INTERNAL, function);
        return NULL;
    }

    if (symbol == NULL || symbol->data == NULL || symbol->data->type != FUNCTION ||
        symbol->data->value == NULL) {
        return NULL;
    }

    // projde seznam extension metod se stejným jménem (můžou mít stejné jméno, pokud je v extension
    // s různými recievery)
    for (SemanticFunctionSymbol *method = symbol->data->value; method != NULL;
         method = method->next) {

        if (method->function ==
            function) { // porovnám ukazatele, jestli se skutečně jedná o hledanou metodu
            return method;
        }
    }

    return NULL;
}

ErrorCode semantic_extension_def(ASTNode *node, SemanticContext *ctx) {
    if (node == NULL || ctx == NULL || node->type != AST_EXTENSION_DEF) {
        return semantic_error(ctx, ERR_INTERNAL, node);
    }

    ASTNode *receiver = node->data.extension_def.receiver;
    ASTNode *methods = node->data.extension_def.methods;

    if (receiver == NULL || receiver->type != AST_VAR_DEF ||
        receiver->data.var_def.identifier == NULL || receiver->data.var_def.declared_type == NULL ||
        receiver->data.var_def.initializer != NULL || receiver->data.var_def.is_mutable ||
        methods == NULL) {
        return semantic_error(ctx, ERR_INTERNAL, node);
    }

    // vyřeší typ receiveru, pokud ještě není vyřešený
    if (receiver->expr_type == NULL) {
        TypeInfo *resolved_type = NULL;

        ErrorCode error =
            semantic_resolve_type_alias(receiver->data.var_def.declared_type, ctx, &resolved_type);

        if (error != ERR_OK) {
            type_info_free(resolved_type);
            return semantic_error(ctx, error, node);
        }

        if (resolved_type == NULL) {
            return semantic_error(ctx, ERR_INTERNAL, node);
        }

        ast_set_type(receiver, resolved_type);
    }

    // přidělí mu (pořád reciever) var_id, pokud ho ještě nemá
    if (receiver->data.var_def.var_id == 0) {
        receiver->data.var_def.var_id = ctx->next_var_id++;
    }

    // zpracuje všechny metody
    for (ASTNode *method = methods; method != NULL; method = method->next) {

        if (method->type != AST_FUNC_DEF || method->data.func_def.name == NULL ||
            method->data.func_def.body == NULL) {
            return semantic_error(ctx, ERR_INTERNAL, node);
        }

        // Najdeme extension symbol metody, kterou zpracováváme
        SemanticFunctionSymbol *symbol = find_extension_symbol(method, ctx);

        if (symbol == NULL || symbol->extension != receiver) {
            return semantic_error(ctx, ERR_INTERNAL, node);
        }

        // zpracování funkce metody
        ErrorCode error = semantic_func_def(symbol, ctx);

        if (error != ERR_OK) {
            return semantic_error(ctx, error, node);
        }
    }

    return ERR_OK;
}

ErrorCode semantic_lambda(ASTNode *node, SemanticContext *ctx, const TypeInfo *expected_type) {
    if (node == NULL || ctx == NULL || node->type != AST_LAMBDA) {
        return semantic_error(ctx, ERR_INTERNAL, node);
    }
    // používám až později, ale potřeba inicializovat před prvním goto cleanup
    TypeInfo *function_type = NULL;
    TypeInfo *param_list = NULL;
    TypeInfo *ret_type = NULL;

    // vytvoří nový zásobník pro lambda funkci
    tStack lambda_stack = {0};
    ErrorCode return_value = ERR_OK;
    // uloží stav předchozího zásobníku a nastaví nový
    tStack *former_stack = ctx->scope_stack;
    bool former_lambda = ctx->inside_lambda;

    ctx->scope_stack = &lambda_stack;
    ctx->inside_lambda = true;

    // vytvoří nový scope a dá ho na vrchol zásobníku
    tBtNode *scope = NULL;
    return_value = symtable_init(&scope);
    if (return_value != ERR_OK) {
        goto cleanup;
    }
    return_value = symtable_push_scope(&scope, &lambda_stack);
    if (return_value != ERR_OK) {
        goto cleanup;
    }
    if (stack_empty(&lambda_stack)) {
        return_value = ERR_INTERNAL;
        goto cleanup;
    }

    // ypracuje parametry a dá do scope
    return_value = semantic_register_param_list(node->data.lambda.param_list, ctx);
    if (return_value != ERR_OK) {
        goto cleanup;
    }

    // vytvoří z parametů seznam typů

    ASTNode *act_param = node->data.lambda.param_list->data.list.items;
    while (act_param != NULL) {
        if (act_param->type != AST_VAR_DEF || !act_param->expr_type) {
            return_value = ERR_INTERNAL;
            goto cleanup;
        }
        TypeInfo *copy = type_info_clone(act_param->expr_type);
        if (!copy) {
            return_value = ERR_INTERNAL;
            goto cleanup;
        }
        param_list = type_append(param_list, copy);
        act_param = act_param->next;
    }

    TypeInfo *expected_return = NULL;
    if (expected_type != NULL && expected_type->type_class == TYPE_FUNCTION) {
        const TypeInfo *param = param_list;
        const TypeInfo *expect = expected_type->data.func_type.param_types;
        while (param != NULL && expect != NULL && semantic_types_equal(param, expect)) {
            param = param->next;
            expect = expect->next;
        }
        if (!param && !expect) {
            expected_return = expected_type->data.func_type.return_type;
            if (!expected_return) {
                return_value = ERR_INTERNAL;
                goto cleanup;
            }
        }
    }

    const TypeInfo *body_expected = expected_return;
    if (body_expected != NULL && body_expected->type_class == TYPE_UNIT) {
        body_expected =
            NULL; // pokud má být typ unit, je nám to jedno, protože se nic vracet nebude
    }

    // zpracuje tělo funkce
    bool body_used_as_value = expected_return == NULL || expected_return->type_class != TYPE_UNIT;

    if (expected_return == NULL) {
        return_value = semantic_fill_unit_else(node->data.lambda.body);
        if (return_value != ERR_OK) {
            goto cleanup;
        }
    }
    return_value =
        semantic_check_block(node->data.lambda.body, ctx, body_expected, body_used_as_value, false);

    if (return_value != ERR_OK) {
        goto cleanup;
    }
    if (node->data.lambda.body->expr_type == NULL) {
        return_value = ERR_INTERNAL;
        goto cleanup;
    }

    // zkontroluje správnost typů a vytvoří návratový typ

    if (expected_return != NULL && expected_return->type_class == TYPE_UNIT) {

        ret_type = type_create_basic(TYPE_UNIT);

    } else if (expected_return != NULL &&
               semantic_types_compatible(expected_return, node->data.lambda.body->expr_type)) {

        return_value = semantic_check_conversion(node->data.lambda.body, expected_return);
        if (return_value != ERR_OK) {
            goto cleanup;
        }

        ret_type = type_info_clone(expected_return);
    } else {
        // kompatibilitu bude řešit nadřazený uzel, tady řešit nemusím
        ret_type = type_info_clone(node->data.lambda.body->expr_type);
    }

    if (ret_type == NULL) {
        return_value = ERR_INTERNAL;
        goto cleanup;
    }

    // vytvoří typ funkce a dosadí do expr_type lambda funkce
    function_type = type_create_function(param_list, ret_type);
    if (function_type == NULL) {
        return_value = ERR_INTERNAL;
        goto cleanup;
    }
    param_list = NULL; /* vlastnictvi preslo do function_type */
    ret_type = NULL;
    ast_set_type(node, function_type);
    function_type = NULL;

// závěrečný úklid a zrácení zásobníku do původního stavu
cleanup:
    if (param_list != NULL) {
        type_info_free(param_list);
    }
    if (ret_type != NULL) {
        type_info_free(ret_type);
    }
    if (function_type != NULL) {
        type_info_free(function_type);
    }
    while (!stack_empty(&lambda_stack)) {
        ErrorCode pop_res = symtable_pop_scope(&lambda_stack);
        if (return_value == ERR_OK) {
            return_value = pop_res;
        }
    }
    ctx->scope_stack = former_stack;
    ctx->inside_lambda = former_lambda;
    return semantic_error(ctx, return_value, node);
}

/* ==================================================
 * PŘÍKAZY A ŘÍDICÍ STRUKTURY
 * ================================================== */

bool semantic_always_returns(const ASTNode *node) {
    if (node == NULL) {
        return false;
    }

    switch (node->type) {
        case AST_RETURN:
            // uzel je typu return, vrací true
            return true;

        case AST_BLOCK:
            // projde všechny příkazy bloku, když najde return vrací true
            for (const ASTNode *stmt = node->data.block.statements; stmt != NULL;
                 stmt = stmt->next) {

                if (semantic_always_returns(stmt)) {
                    return true;
                }
            }
            return false;

        case AST_IF_EXPR:
            // když nemá větev else nemuže mít if vždy return
            if (node->data.if_expr.else_block == NULL) {
                return false;
            }
            // zjistí jestli if then i else větev obsahují return
            return semantic_always_returns(node->data.if_expr.then_block) &&
                   semantic_always_returns(node->data.if_expr.else_block);

        default:
            // ostatní typy vrací false
            return false;
    }
}
ErrorCode semantic_fill_unit_else(ASTNode *node) {
    if (node == NULL) {
        return ERR_OK;
    }

    // pokud jde o blok, dojdeme na poslední příkaz tohoto bloku
    if (node->type == AST_BLOCK) {
        ASTNode *last = node->data.block.statements;

        if (last == NULL) {
            return ERR_INTERNAL;
        }

        while (last->next != NULL) {
            last = last->next;
        }

        return semantic_fill_unit_else(last);
    }

    // zajímá nás pouze if
    if (node->type != AST_IF_EXPR) {
        return ERR_OK;
    }

    // chybějící else doplníme jako else ()
    if (node->data.if_expr.else_block == NULL) {
        ASTNode *unit = ast_create_unit_lit(node->line);
        if (unit == NULL) {
            return ERR_INTERNAL;
        }

        ASTNode *else_block = ast_create_block(unit, node->line);
        if (else_block == NULL) {
            ast_free(unit);
            return ERR_INTERNAL;
        }

        node->data.if_expr.else_block = else_block;
    }

    // to stenjé se zkontroluje pro blok uvnitř if a else
    ErrorCode res = semantic_fill_unit_else(node->data.if_expr.then_block);
    if (res != ERR_OK) {
        return res;
    }

    return semantic_fill_unit_else(node->data.if_expr.else_block);
}

ErrorCode semantic_if_expr(ASTNode *node, SemanticContext *ctx, const TypeInfo *expected_type,
                           bool used_as_value) {
    if (node == NULL || ctx == NULL || node->type != AST_IF_EXPR) {
        return semantic_error(ctx, ERR_INTERNAL, node);
    }

    // zpracuje podmínku
    ErrorCode res = semantic_visit(&node->data.if_expr.condition, ctx, NULL);
    if (res != ERR_OK) {
        return semantic_error(ctx, res, node);
    }
    // podmínka musí mít typ boolean
    if (node->data.if_expr.condition->expr_type->type_class != TYPE_BOOLEAN) {
        return semantic_error(ctx, ERR_SEM_TYPE, node->data.if_expr.condition);
    }

    // zpracuje blok po if
    res = semantic_check_block(node->data.if_expr.then_block, ctx,
                               used_as_value ? expected_type : NULL, used_as_value, true);
    if (res != ERR_OK) {
        return semantic_error(ctx, res, node);
    }

    if (node->data.if_expr.else_block != NULL) {

        // zpracuje else větev pokud ji if má
        res = semantic_check_block(node->data.if_expr.else_block, ctx,
                                   used_as_value ? expected_type : NULL, used_as_value, true);
        if (res != ERR_OK) {
            return semantic_error(ctx, res, node);
        }

    } else { // if bez else povoleno pouze u příkazu (ne výrazu)
        if (used_as_value) {
            return semantic_error(ctx, ERR_SEM_TYPE, node);
        }
    }

    TypeInfo *ret_type = NULL;
    // určí návratový typ
    if (used_as_value) {
        // if jako výraz: musí být společný typ obou větví
        // společný typ se nastaví jako ret_type
        res = semantic_common_type(node->data.if_expr.then_block->expr_type,
                                   node->data.if_expr.else_block->expr_type, &ret_type);
        if (res != ERR_OK) {
            return semantic_error(ctx, res, node);
        }

        if (ret_type == NULL) {
            return semantic_error(ctx, ERR_INTERNAL, node);
        }

        res = semantic_check_conversion(node->data.if_expr.then_block, ret_type);

        if (res != ERR_OK) {
            type_info_free(ret_type);
            return semantic_error(ctx, res, node);
        }

        res = semantic_check_conversion(node->data.if_expr.else_block, ret_type);

        if (res != ERR_OK) {
            type_info_free(ret_type);
            return semantic_error(ctx, res, node);
        }
    } else {
        // pokud má ve všech větvích return TYPE_NOTHING, jinak TYPE_UNIT
        ret_type = type_create_basic(semantic_always_returns(node) ? TYPE_NOTHING : TYPE_UNIT);

        if (ret_type == NULL) {
            return semantic_error(ctx, ERR_INTERNAL, node);
        }
    }

    // nastaví expr_type
    ast_set_type(node, ret_type);
    return ERR_OK;
}

// hotovo
ErrorCode semantic_while(ASTNode *node, SemanticContext *ctx) {
    if (node == NULL || ctx == NULL || node->data.while_loop.condition == NULL) {
        return semantic_error(ctx, ERR_INTERNAL, node);
    }

    // zpracuje uzel s podmínkou
    ErrorCode res = semantic_visit(&node->data.while_loop.condition, ctx, NULL);
    if (res != ERR_OK) {
        return semantic_error(ctx, res, node);
    }

    // podmínka musí být typu boolean
    if (node->data.while_loop.condition->expr_type->type_class != TYPE_BOOLEAN) {
        return semantic_error(ctx, ERR_SEM_TYPE, node->data.while_loop.condition);
    }

    // zpracuje tělo cyklu
    res = semantic_check_block(node->data.while_loop.body, ctx, NULL, false, true);
    if (res != ERR_OK) {
        return semantic_error(ctx, res, node);
    }

    // nastaví návratový typ na TYPE_UNIT
    TypeInfo *type = type_create_basic(TYPE_UNIT);
    if (type == NULL) {
        return semantic_error(ctx, ERR_INTERNAL, node);
    }

    ast_set_type(node, type);
    return ERR_OK;
}

ErrorCode semantic_for(ASTNode *node, SemanticContext *ctx) {
    if (node == NULL || ctx == NULL || node->type != AST_FOR) {
        return semantic_error(ctx, ERR_INTERNAL, node);
    }

    // zpracování a kontrola zdroje generátoru
    ErrorCode res = ERR_OK;
    if (node->data.for_loop.source->type == AST_RANGE) {
        // pokud je to range
        res = semantic_range(node->data.for_loop.source, ctx);
    } else {
        // ostatní případy
        res = semantic_visit(&node->data.for_loop.source, ctx, NULL);
    }
    if (res != ERR_OK) {
        return semantic_error(ctx, res, node);
    }

    // kontrola, jestli tam jsou správné typy
    if (node->data.for_loop.source->type != AST_RANGE &&
        node->data.for_loop.source->expr_type->type_class != TYPE_LIST) {
        return semantic_error(ctx, ERR_SEM_TYPE, node);
    }

    // zpracování proměnné cyklu
    TypeInfo *var_type = NULL;
    if (node->data.for_loop.source->type == AST_RANGE) {
        var_type = type_create_basic(TYPE_INT);
        if (var_type == NULL) {
            return semantic_error(ctx, ERR_INTERNAL, node);
        }

    } else {
        var_type = type_info_clone(node->data.for_loop.source->expr_type->data.list_element_type);
        if (var_type == NULL) {
            return semantic_error(ctx, ERR_INTERNAL, node);
        }
    }
    ast_set_type(node->data.for_loop.iter_var, var_type);
    node->data.for_loop.iter_var->data.var_def.var_id = ctx->next_var_id;
    ctx->next_var_id++;

    // vytvoří nový scope
    tBtNode *for_scope = NULL;
    res = symtable_init(&for_scope);
    if (res != ERR_OK) {
        return semantic_error(ctx, res, node);
    }
    // dá do něj proměnnou cyklu
    tBtNodeData var_data = {.value = node->data.for_loop.iter_var, .type = VARIABLE};
    res = symtable_insert(&for_scope, node->data.for_loop.iter_var->data.var_def.identifier,
                          var_data);
    if (res != ERR_OK) {
        symtable_dispose(&for_scope);
        return semantic_error(ctx, res, node);
    }

    res = symtable_push_scope(&for_scope, ctx->scope_stack);
    if (res != ERR_OK) {
        symtable_dispose(&for_scope);
        return semantic_error(ctx, res, node);
    }

    // zpracování a kontrola stráží
    ASTNode **act_guard = &node->data.for_loop.guards;
    while (*act_guard != NULL) {
        res = semantic_visit(act_guard, ctx, NULL);
        if (res != ERR_OK) {
            symtable_pop_scope(ctx->scope_stack);
            return semantic_error(ctx, res, node);
        }
        if ((*act_guard)->expr_type->type_class != TYPE_BOOLEAN) {
            symtable_pop_scope(ctx->scope_stack);
            return semantic_error(ctx, ERR_SEM_TYPE, node);
        }
        act_guard = &(*act_guard)->next;
    }

    // zpracování těla
    res = semantic_check_block(node->data.for_loop.body, ctx, NULL, false, true);
    if (res != ERR_OK) {
        symtable_pop_scope(ctx->scope_stack);
        return semantic_error(ctx, res, node);
    }

    // uvolní se scope
    res = symtable_pop_scope(ctx->scope_stack);
    if (res != ERR_OK) {
        return semantic_error(ctx, res, node);
    }

    // nastavení typu
    TypeInfo *type = type_create_basic(TYPE_UNIT);
    if (type == NULL) {
        return semantic_error(ctx, ERR_INTERNAL, node);
    }
    ast_set_type(node, type);

    return ERR_OK;
}

ErrorCode semantic_match(ASTNode *node, SemanticContext *ctx, const TypeInfo *expected_type) {
    if (node == NULL || ctx == NULL || node->type != AST_MATCH_EXPR) {
        return semantic_error(ctx, ERR_INTERNAL, node);
    }

    ErrorCode res = ERR_OK;
    bool has_wildcard = false;
    TypeInfo *ret_type = NULL;

    // zpracuje výraz, který v match porovnáváme
    res = semantic_visit(&node->data.match_expr.matched_expr, ctx, NULL);
    if (res != ERR_OK) {
        return semantic_error(ctx, res, node);
    }

    // typ toho výrazu
    TypeInfo *matched_type = node->data.match_expr.matched_expr->expr_type;

    if (matched_type == NULL) {
        return semantic_error(ctx, ERR_INTERNAL, node);
    }

    // projde case větve
    ASTNode *act_branch = node->data.match_expr.cases;

    while (act_branch != NULL) {
        // zjistí, jestli je v pattern _ (PATTERN_WILDCARD) - kvůli kontrole vyčerpání
        ASTNode *pattern = act_branch->data.case_branch.pattern;

        if (pattern->data.pattern.pat_type == PATTERN_WILDCARD &&
            act_branch->data.case_branch.guard == NULL) {
            has_wildcard = true;
        }

        // zpracuje větev
        res = semantic_case_branch(act_branch, ctx, matched_type, expected_type);
        if (res != ERR_OK) {
            if (ret_type != NULL) {
                type_info_free(ret_type);
            }
            return semantic_error(ctx, res, node);
        }

        // získá typ těla větve
        TypeInfo *branch_type = act_branch->data.case_branch.body->expr_type;

        if (branch_type == NULL) {
            if (ret_type != NULL) {
                type_info_free(ret_type);
            }
            return semantic_error(ctx, ERR_INTERNAL, node);
        }

        // nastavení ret_type u první větve
        if (ret_type == NULL) {
            ret_type = type_info_clone(branch_type);

            if (ret_type == NULL) {
                return semantic_error(ctx, ERR_INTERNAL, node);
            }
        } else {
            // větve musí být vzájemně kompatibilní
            TypeInfo *new_ret_type = NULL;

            res = semantic_common_type(ret_type, branch_type, &new_ret_type);
            if (res != ERR_OK) {
                type_info_free(ret_type);
                return semantic_error(ctx, res, node);
            }

            if (new_ret_type == NULL) {
                type_info_free(ret_type);
                return semantic_error(ctx, ERR_INTERNAL, node);
            }

            type_info_free(ret_type);
            ret_type = new_ret_type;
        }

        act_branch = act_branch->next;
    }

    // match musí mít aspoň jednu větev - nastavila by ret_type
    if (ret_type == NULL) {
        return semantic_error(ctx, ERR_INTERNAL, node);
    }

    // kontrola jestli je vyčerpávající - má _
    if (!has_wildcard) {
        type_info_free(ret_type);
        return semantic_error(ctx, ERR_SEM_TYPE, node);
    }

    // kontrola kompatibility s expected
    if (expected_type != NULL && !semantic_types_compatible(expected_type, ret_type)) {
        type_info_free(ret_type);
        return semantic_error(ctx, ERR_SEM_TYPE, node);
    }

    // znovu projdu všechny větve a ověřím, jestli nejsou potřeba konverze
    act_branch = node->data.match_expr.cases;
    while (act_branch != NULL) {
        res = semantic_check_conversion(act_branch->data.case_branch.body, ret_type);
        if (res != ERR_OK) {
            type_info_free(ret_type);
            return semantic_error(ctx, res, node);
        }

        act_branch = act_branch->next;
    }

    // nastavení typu match
    ast_set_type(node, ret_type);

    // Případná konverze výsledku match na očekávaný typ.
    if (expected_type != NULL) {
        res = semantic_check_conversion(node, expected_type);
    }

    return semantic_error(ctx, res, node);
}

// hotova
ErrorCode semantic_return(ASTNode *node, SemanticContext *ctx) {
    if (node == NULL || ctx == NULL || node->type != AST_RETURN || ctx->current_function == NULL) {
        return semantic_error(ctx, ERR_INTERNAL, node);
    }

    // lambda funkce nemůže mít return
    // když je kdekoli ve funkci return, musí mít funkce napsaný v definici návratový typ
    if (ctx->inside_lambda || ctx->current_function->data.func_def.return_type == NULL) {
        return semantic_error(ctx, ERR_SEM_TYPE, node);
    }

    TypeInfo *func_ret = NULL;

    // vezme si návratový typ funkce
    ErrorCode res = semantic_resolve_type_alias(ctx->current_function->data.func_def.return_type,
                                                ctx, &func_ret);
    if (res != ERR_OK) {
        return semantic_error(ctx, res, node);
    }
    if (func_ret == NULL) {
        return semantic_error(ctx, ERR_INTERNAL, node);
    }

    // pokud je return bez hodnoty, musí být návratový typ unit
    if (node->data.return_stmt.value == NULL) {
        if (func_ret->type_class != TYPE_UNIT) {

            type_info_free(func_ret);

            return semantic_error(ctx, ERR_SEM_TYPE, node);
        }

    } else {
        // jinak zpracuje hodnotu u return
        res = semantic_visit(&node->data.return_stmt.value, ctx,
                             func_ret->type_class == TYPE_UNIT ? NULL : func_ret);
        if (res != ERR_OK) {
            type_info_free(func_ret);
            return semantic_error(ctx, res, node);
        }

        // pokud nemá funkce návratový typ unit, ověří kompatibilitu a konverzi typů u return a
        // návratového typu funkce
        if (func_ret->type_class != TYPE_UNIT) {
            if (!semantic_types_compatible(func_ret, node->data.return_stmt.value->expr_type)) {
                type_info_free(func_ret);
                return semantic_error(ctx, ERR_SEM_TYPE, node);
            }
            res = semantic_check_conversion(node->data.return_stmt.value, func_ret);
            if (res != ERR_OK) {
                type_info_free(func_ret);
                return semantic_error(ctx, res, node);
            }
        }
    }

    // uvolní návratový typ funkce
    type_info_free(func_ret);

    // nastaví typ u return na TYPE_NOTHING
    TypeInfo *ret_type = type_create_basic(TYPE_NOTHING);
    if (ret_type == NULL) {
        return semantic_error(ctx, ERR_INTERNAL, node);
    }
    ast_set_type(node, ret_type);
    return ERR_OK;
}

/* ==================================================
 * POMOCNÉ FUNKCE PRO STRUKTURÁLNÍ UZLY
 * ================================================== */

// hotovo
ErrorCode semantic_check_agr_list(ASTNode *node, SemanticContext *ctx,
                                  const TypeInfo *param_types) {
    if (node == NULL || ctx == NULL || node->type != AST_ARG_LIST) {
        return semantic_error(ctx, ERR_INTERNAL, node);
    }

    ASTNode **actual = &node->data.list.items;
    const TypeInfo *expected = param_types;

    // projde seznam argumentů
    while (*actual != NULL && expected != NULL) {
        // každý argument zpracuje
        ErrorCode res = semantic_visit(actual, ctx, expected);
        if (res != ERR_OK) {
            return semantic_error(ctx, res, node);
        }

        // porovná, jestli je kompatibilní s očekávaným typem
        if (!semantic_types_compatible(expected, (*actual)->expr_type)) {
            return semantic_error(ctx, ERR_SEM_ARGS, *actual);
        }

        // pokud je potřeba, nastaví konverzi
        res = semantic_check_conversion(*actual, expected);
        if (res != ERR_OK) {
            return semantic_error(ctx, res, node);
        }

        actual = &(*actual)->next;
        expected = expected->next;
    }

    // pokud byly seznamy různě dlouhé, hlásí chybu
    if (*actual != NULL || expected != NULL) {
        return semantic_error(ctx, ERR_SEM_ARGS, node);
    }

    return ERR_OK;
}

// hotovo
ErrorCode semantic_register_param_list(ASTNode *node, SemanticContext *ctx) {
    if (node == NULL || ctx == NULL || node->type != AST_PARAM_LIST) {
        return semantic_error(ctx, ERR_INTERNAL, node);
    }

    ASTNode *param = node->data.list.items;

    // projde seznam parametrů a každý uzel zpracuje
    while (param != NULL) {
        ErrorCode res = semantic_var_def(ctx->scope_stack, param, ctx, true);
        if (res != ERR_OK) {
            return semantic_error(ctx, res, node);
        }

        param = param->next;
    }

    return ERR_OK;
}

ErrorCode semantic_case_branch(ASTNode *node, SemanticContext *ctx, const TypeInfo *matched_type,
                               const TypeInfo *expected_type) {
    if (node == NULL || ctx == NULL || matched_type == NULL || node->type != AST_CASE_BRANCH ||
        ctx->scope_stack == NULL) {
        return semantic_error(ctx, ERR_INTERNAL, node);
    }

    ErrorCode res = ERR_OK;

    // vytvoří nový scope pro case větev
    tBtNode *case_scope = NULL;
    res = symtable_init(&case_scope);
    if (res != ERR_OK) {
        return semantic_error(ctx, res, node);
    }
    res = symtable_push_scope(&case_scope, ctx->scope_stack);
    if (res != ERR_OK) {
        return semantic_error(ctx, res, node);
    }

    // zpracuje vzor
    res = semantic_pattern(node->data.case_branch.pattern, ctx, matched_type);
    if (res != ERR_OK) {
        symtable_pop_scope(ctx->scope_stack);
        return semantic_error(ctx, res, node);
    }

    // zpracuje stráž, pokud je
    if (node->data.case_branch.guard != NULL) {
        res = semantic_visit(&node->data.case_branch.guard, ctx, NULL);
        if (res != ERR_OK) {
            symtable_pop_scope(ctx->scope_stack);
            return semantic_error(ctx, res, node);
        }

        // stráž musí mít typ boolean
        if (node->data.case_branch.guard->expr_type == NULL ||
            node->data.case_branch.guard->expr_type->type_class != TYPE_BOOLEAN) {
            symtable_pop_scope(ctx->scope_stack);
            return semantic_error(ctx, ERR_SEM_TYPE, node);
        }
    }

    // zpracuje tělo case větve
    res = semantic_check_block(node->data.case_branch.body, ctx, expected_type, true, true);
    if (res != ERR_OK) {
        symtable_pop_scope(ctx->scope_stack);
        return semantic_error(ctx, res, node);
    }
    // odstraní (a tím uvolní) scope této větve
    res = symtable_pop_scope(ctx->scope_stack);

    return semantic_error(ctx, res, node);
}

ErrorCode semantic_pattern(ASTNode *node, SemanticContext *ctx, const TypeInfo *matched_type) {
    if (node == NULL || ctx == NULL || matched_type == NULL || node->type != AST_PATTERN) {
        return semantic_error(ctx, ERR_INTERNAL, node);
    }
    ErrorCode res = ERR_OK;
    switch (node->data.pattern.pat_type) {
        case PATTERN_LITERAL: { // př. case 0
            // Zkontrolovat literál a kompatibilitu typů.
            ASTNode *literal = node->data.pattern.detail.literal_val;
            res = semantic_literal(literal);
            if (res != ERR_OK) {
                return semantic_error(ctx, res, node);
            }
            if (!semantic_types_compatible(matched_type, literal->expr_type)) {
                return semantic_error(ctx, ERR_SEM_TYPE, node);
            }

            // Případná implicitní konverze Int -> Double
            return semantic_check_conversion(literal, matched_type);
        }

        case PATTERN_TYPE_BINDING: { // př. n: Int
            // zjistí typ proměnné v pattern a rozvine ho
            TypeInfo *resolved = NULL;
            res = semantic_resolve_type_alias(node->data.pattern.detail.type_bind, ctx, &resolved);
            if (res != ERR_OK) {
                return semantic_error(ctx, res, node);
            }

            if (resolved == NULL) {
                return semantic_error(ctx, ERR_INTERNAL, node);
            }

            // kontrola, jestli je typ kompatibilní s tím, u kterého děláme match
            if (!pattern_types_overlap(matched_type, resolved)) {
                type_info_free(resolved);
                return semantic_error(ctx, ERR_SEM_TYPE, node);
            }

            // projde aktuální scope a zjistí, jestli proměnná už nebyla deklarována dřív
            tBtNode **scope = &stack_top(ctx->scope_stack)->scope;
            tBtNode *found = NULL;
            res = symtable_search(scope, node->data.pattern.bound_identifier, &found);
            if (res != ERR_OK) {
                type_info_free(resolved);
                return semantic_error(ctx, res, node);
            }
            if (found != NULL) {
                type_info_free(resolved);
                return semantic_error(ctx, ERR_SEM_REDEF, node);
            }

            // vytvoří uzel pro vložení do scope a vloží ho
            tBtNodeData data = {.type = VARIABLE, .value = node};
            res = symtable_insert(scope, node->data.pattern.bound_identifier, data);
            if (res != ERR_OK) {
                type_info_free(resolved);
                return semantic_error(ctx, res, node);
            }

            // zkontroluje, že nenastala chyba a vložil se
            tBtNode *inserted_node = NULL;
            res = symtable_search(scope, node->data.pattern.bound_identifier, &inserted_node);
            if (res != ERR_OK) {
                type_info_free(resolved);
                return semantic_error(ctx, res, node);
            }

            if (inserted_node == NULL || inserted_node->data->type != VARIABLE ||
                inserted_node->data->value != node) {
                type_info_free(resolved);
                return semantic_error(ctx, ERR_INTERNAL, node);
            }

            // nastaví expr_type a var_id
            ast_set_type(node, resolved);
            node->data.pattern.var_id = ctx->next_var_id++;

            break;
        }
        case PATTERN_CONSTRUCTOR: {
            //  projde tabulku globálních dymbolů a najde deklaraci třídy
            tBtNode *symbol = NULL;
            TypeInfo *class_type = node->data.pattern.detail.constructor.class_type;

            res = symtable_search(&ctx->symbols, class_type->data.custom_name, &symbol);
            if (res != ERR_OK) {
                return semantic_error(ctx, res, node);
            }
            if (symbol == NULL) {
                return semantic_error(ctx, ERR_SEM_UNDEF, node);
            }

            if (symbol->data->type != CLASS) {
                return semantic_error(ctx, ERR_SEM_TYPE, node);
            }
            ASTNode *class_def = symbol->data->value;
            if (class_def->type != AST_CLASS_DEF) {
                return semantic_error(ctx, ERR_INTERNAL, node);
            }

            // ověří kompatibilitu s matched_type
            if (!pattern_types_overlap(matched_type, class_type)) {
                return semantic_error(ctx, ERR_SEM_TYPE, node);
            }

            ASTNode *act_field = class_def->data.class_def.fields;
            ASTNode *arg = node->data.pattern.detail.constructor.args;
            // projde argumenty
            while (act_field != NULL && arg != NULL) {
                if (act_field->type != AST_VAR_DEF || act_field->expr_type == NULL) {
                    return semantic_error(ctx, ERR_INTERNAL, node);
                }

                if (arg->type != AST_VAR_DEF) {
                    return semantic_error(ctx, ERR_SEM_TYPE, node);
                }
                char *name = arg->data.var_def.identifier;

                if (name == NULL || arg->data.var_def.declared_type != NULL ||
                    arg->data.var_def.initializer != NULL) {
                    return semantic_error(ctx, ERR_INTERNAL, node);
                }

                // zjistí, jestli se už proměnná tohoto názvu v aktuálním scope nevyskytuje
                tBtNode **scope = &stack_top(ctx->scope_stack)->scope;
                tBtNode *found = NULL;
                res = symtable_search(scope, name, &found);
                if (res != ERR_OK) {
                    return semantic_error(ctx, res, arg);
                }

                if (found != NULL) {
                    return semantic_error(ctx, ERR_SEM_REDEF, node);
                }

                // naklonuje typ podle typu z deklarace třídy a nastaví do expr_type proměnné
                TypeInfo *type = type_info_clone(act_field->expr_type);
                if (type == NULL) {
                    return semantic_error(ctx, ERR_INTERNAL, node);
                }
                ast_set_type(arg, type);

                // vloží proměnnou do aktuálního scope
                tBtNodeData data = {.type = VARIABLE, .value = arg};
                res = symtable_insert(scope, name, data);
                if (res != ERR_OK) {
                    return semantic_error(ctx, res, arg);
                }

                // zkontroluje, jestli se v pořádku vložil
                tBtNode *inserted = NULL;
                res = symtable_search(scope, name, &inserted);
                if (res != ERR_OK) {
                    return semantic_error(ctx, res, arg);
                }
                if (inserted == NULL || inserted->data == NULL || inserted->data->value != arg) {
                    return semantic_error(ctx, ERR_INTERNAL, node);
                }

                // nastaví var_id proměnné
                arg->data.var_def.var_id = ctx->next_var_id++;

                act_field = act_field->next;
                arg = arg->next;
            }

            // kontrola, jestli měly seznamy stejnou délku
            if (act_field != NULL || arg != NULL) {
                return semantic_error(ctx, ERR_SEM_TYPE, node);
            }
            break;
        }
        case PATTERN_WILDCARD: // _ ostatní případy
            return ERR_OK;

        default:
            return semantic_error(ctx, ERR_INTERNAL, node);
    }

    return ERR_OK;
}

ErrorCode semantic_range(ASTNode *node, SemanticContext *ctx) {
    if (node == NULL || ctx == NULL || node->type != AST_RANGE) {
        return semantic_error(ctx, ERR_INTERNAL, node);
    }

    if (node->data.range.start == NULL || node->data.range.end == NULL) {
        return semantic_error(ctx, ERR_INTERNAL, node);
    }

    // zpracuje uzly uvnitř range
    ErrorCode res = semantic_visit(&node->data.range.start, ctx, NULL);
    if (res != ERR_OK) {
        return semantic_error(ctx, res, node);
    }

    res = semantic_visit(&node->data.range.end, ctx, NULL);
    if (res != ERR_OK) {
        return semantic_error(ctx, res, node);
    }

    // start i end musí být typu int, jinak chyba
    if (node->data.range.start->expr_type->type_class != TYPE_INT ||
        node->data.range.end->expr_type->type_class != TYPE_INT) {
        return semantic_error(ctx, ERR_SEM_TYPE, node);
    }

    // pokud má range i step, zpracuje ho a zkontroluje, jestli je také int
    if (node->data.range.step != NULL) {
        res = semantic_visit(&node->data.range.step, ctx, NULL);
        if (res != ERR_OK) {
            return semantic_error(ctx, res, node);
        }
        if (node->data.range.step->expr_type->type_class != TYPE_INT) {
            return semantic_error(ctx, ERR_SEM_TYPE, node);
        }
    }
    return ERR_OK;
}

ErrorCode semantic_find_symbol(tStack *stack, char *name, SemanticContext *ctx,
                               tBtNode **location) {
    if (stack == NULL || name == NULL || ctx == NULL || location == NULL) {
        return ERR_INTERNAL;
    }
    *location = NULL;

    tStackElem *scope = stack_top(stack);

    // projde všechny tabulky symbolů v zásobníku, pokud najde odpovídající uzel, vrátí ho
    while (scope != NULL) {
        ErrorCode res = symtable_search(&scope->scope, name, location);
        if (res != ERR_OK) {
            return res;
        }
        if (*location != NULL) {
            return ERR_OK;
        }

        scope = scope->next;
    }

    // uzel se zatím nenašel, zkusí ještě prohledat tabulku globálních symbolů
    return symtable_search(&ctx->symbols, name, location);
}

/* ==================================================
 * POMOCNÉ FUNKCE PRO PRÁCI S TYPY
 * ================================================== */

bool pattern_types_overlap(const TypeInfo *match_type, const TypeInfo *case_type) {
    if (match_type == NULL || case_type == NULL)
        return false;

    if (match_type->type_class == TYPE_UNION) {
        return pattern_types_overlap(match_type->data.union_type.left, case_type) ||
               pattern_types_overlap(match_type->data.union_type.right, case_type);
    }

    return semantic_types_equal(match_type, case_type);
}

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

bool compare_unions(const TypeInfo *a, const TypeInfo *b) {
    if (a == NULL || b == NULL) {
        return false;
    }

    bool are_equal = true;
    const TypeInfo *position_a = a;
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
        if (a->type_class == TYPE_UNION) { // pokud je union, porovná, jestli obsahují stejné typy
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
        } else if (a->type_class == TYPE_LIST) {                     // porovný typy listů
            return_value =
                semantic_types_equal(a->data.list_element_type, b->data.list_element_type);
        } else if (a->type_class == TYPE_CUSTOM) {
            if (a->data.custom_name == NULL || b->data.custom_name == NULL) {
                return false;
            }
            // porovná jména
            return_value = strcmp(a->data.custom_name, b->data.custom_name) == 0;
        } else { // pro zbylé typy není třeba dodatečných kontrol
            return_value = true;
        }
        return return_value;
    }
    // nastane když se type_class nerovnají
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
        compatible = true; // stejné typy
    } else if (expected->type_class == TYPE_UNION && actual->type_class != TYPE_UNION) {
        // zkontroluje, jestli je actual typ v union
        compatible = semantics_compare_union_with_type(expected, actual);
        if (!compatible &&
            actual->type_class == TYPE_INT) { // pokud by byl typ int a nebyl nalezen v union,
                                              // zkontroluje se, jestli je v union double
                                              // (implicitně by se mohl převést int -> double)
            TypeInfo double_type = {.type_class = TYPE_DOUBLE, .next = NULL};
            compatible = semantics_compare_union_with_type(expected, &double_type);
        }
    } else if (expected->type_class == TYPE_UNION && actual->type_class == TYPE_UNION) {
        compatible = compare_unions(actual, expected);
    }
    return compatible;
}

ErrorCode semantic_common_type(const TypeInfo *a, const TypeInfo *b, TypeInfo **result) {
    if (a == NULL || b == NULL || result == NULL) {
        return ERR_INTERNAL; // Chyba: Neplatný vstup
    }
    *result = NULL;
    const TypeInfo *ret_type = NULL;

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
        return ERR_SEM_TYPE; // Chyba: Typy nejsou kompatibilní
    }

    // potřeba naklokovat výsledný typ
    *result = type_info_clone(ret_type);
    if (*result == NULL) {
        return ERR_INTERNAL;
    }

    return ERR_OK; // Úspěch
}

ErrorCode semantic_check_conversion(ASTNode *expression, const TypeInfo *expected_type) {
    if (expression == NULL || expression->expr_type == NULL || expected_type == NULL) {
        return ERR_INTERNAL; // Chyba: Neplatný vstup
    }

    // zjistí, jestli je potřeba konverze to_double a případně ji nastaví
    expression->to_double = false;
    if (expression->expr_type->type_class == TYPE_INT) {
        if (expected_type->type_class == TYPE_DOUBLE) {
            expression->to_double = true; // máme typ int, očekáváme doble - nastaví konverzi
        } else if (expected_type->type_class == TYPE_UNION) {
            const TypeInfo int_type = {.type_class = TYPE_INT};

            const TypeInfo double_type = {.type_class = TYPE_DOUBLE};

            // nastaví konverzi, pokud v očekávaném union je pouze type double ale int ne (a
            // máme typ int)
            expression->to_double = !semantics_compare_union_with_type(expected_type, &int_type) &&
                                    semantics_compare_union_with_type(expected_type, &double_type);
        }
    }
    return ERR_OK; // Úspěch
}

void remove_union_duplicates(TypeInfo **slot) {
    if (slot == NULL || *slot == NULL) {
        return;
    }

    TypeInfo *type = *slot;
    switch (type->type_class) {
        case TYPE_UNION:
            // odstraní duplicity z obou podstromů
            remove_union_duplicates(&type->data.union_type.left);
            remove_union_duplicates(&type->data.union_type.right);

            // po normalizaci je pravá větev jediný člen unionu
            if (type->data.union_type.right->type_class != TYPE_UNION &&
                semantics_compare_union_with_type(type->data.union_type.left,
                                                  type->data.union_type.right)) {

                // ponechá levou větev a odstraní nadbytečný pravý člen
                TypeInfo *left = type->data.union_type.left;
                type->data.union_type.left = NULL;
                type_replace(slot, left);
            }
            break;

        case TYPE_LIST:
            // vyřeší vnitřek listu
            remove_union_duplicates(&type->data.list_element_type);
            break;

        case TYPE_FUNCTION:
            for (TypeInfo **param = &type->data.func_type.param_types; *param != NULL;
                 param = &(*param)->next) {
                remove_union_duplicates(param);
            }
            remove_union_duplicates(&type->data.func_type.return_type);
            break;

        default:
            break;
    }
}

void normalise_union_tree(TypeInfo *type) {
    if (type == NULL) {
        return;
    }

    // vytvoří a nastaví ukazatel end_union na poslední union uzel levé diagonály (pokud je type
    // typ UNION)
    TypeInfo *end_union = type;
    if (type->type_class == TYPE_UNION) {
        while (end_union->data.union_type.left->type_class == TYPE_UNION) {
            end_union = end_union->data.union_type.left;
        }
    }

    // prochází celý unoin
    TypeInfo *pos = type;
    while (pos->type_class == TYPE_UNION) {
        // vezme si prvek na pravé straně
        TypeInfo *tmp = pos->data.union_type.right;
        switch (tmp->type_class) {
            case TYPE_UNION:
                // pokud tam je union, vyměního s prvkem na konci levé diagonály (co už není
                // union) a aktualizije end_union, aby opět ukazoval na poslední union prvek
                // levé diagonály
                pos->data.union_type.right = end_union->data.union_type.left;
                end_union->data.union_type.left = tmp;
                while (end_union->data.union_type.left->type_class == TYPE_UNION) {
                    end_union = end_union->data.union_type.left;
                }
                break;
            case TYPE_LIST:
                // pokud tam je list, zavolá tuto funkci nad jeho parametrem
                normalise_union_tree(tmp->data.list_element_type);
                break;
            case TYPE_FUNCTION:
                // pokud je tam funkce, zavolá to tuto funkci nad všemi jeho parametry i
                // návratovým typem
                for (TypeInfo *param = tmp->data.func_type.param_types; param != NULL;
                     param = param->next) {

                    normalise_union_tree(param);
                }

                // návratový typ
                normalise_union_tree(tmp->data.func_type.return_type);
                break;
            default:
                break;
        }
        // posune se po levé diagonále o prvek dál
        pos = pos->data.union_type.left;
    }
    switch (pos->type_class) {
        case TYPE_LIST:
            // pokud mám list, zavolá tuto funci nad typem parametru
            normalise_union_tree(pos->data.list_element_type);
            break;
        case TYPE_FUNCTION:
            // u funkce ji zavolá na včechny parametry a návratový typ
            for (TypeInfo *param = pos->data.func_type.param_types; param != NULL;
                 param = param->next) {

                normalise_union_tree(param);
            }

            // Navratovy typ funkce.
            normalise_union_tree(pos->data.func_type.return_type);
            break;
        default:
            break;
    }
}

bool alias_in_path(const AliasPath *path, const char *name) {
    const AliasPath *path_pos = path;
    while (path_pos != NULL) {
        if (strcmp(path_pos->name, name) == 0) {
            return true;
        }
        path_pos = path_pos->previous;
    }
    return false;
}

ErrorCode expand_aliases(TypeInfo **slot, SemanticContext *ctx, const AliasPath *path) {
    if (slot == NULL || *slot == NULL || ctx == NULL) {
        return semantic_error(ctx, ERR_INTERNAL, NULL);
    }

    TypeInfo *type = *slot;

    switch (type->type_class) {
        case TYPE_CUSTOM: {
            tBtNode *symbol = NULL;

            // najde v tabulce symbolů definici aliasu
            ErrorCode search_res = symtable_search(&ctx->symbols, type->data.custom_name, &symbol);
            if (search_res != ERR_OK) {
                return search_res;
            }

            if (symbol == NULL || symbol->data == NULL) {
                return semantic_error(ctx, ERR_SEM_TYPE, NULL);
            }

            // Datová třída zůstava TYPE_CUSTOM (nerozvíjí se)
            if (symbol->data->type == CLASS) {
                return ERR_OK;
            }

            if (symbol->data->type != ALIAS) {
                return semantic_error(ctx, ERR_SEM_UNDEF, NULL);
            }

            // získám si z value uzel aliasu AST_TYPE_ALIAS
            ASTNode *definition = (ASTNode *)symbol->data->value;

            if (definition == NULL || definition->type != AST_TYPE_ALIAS ||
                definition->data.type_alias.aliased == NULL) {
                return semantic_error(ctx, ERR_INTERNAL, NULL);
            }

            const char *name = definition->data.type_alias.name;

            // Alias už se na aktuální cestě rozvíjí (je to cyklický alias)
            if (alias_in_path(path, name)) {
                return semantic_error(ctx, ERR_SEM_TYPE, NULL);
            }

            // Vytvoří kopii typu, na který alias ukazuje
            TypeInfo *replacement = type_info_clone(definition->data.type_alias.aliased);

            if (replacement == NULL) {
                return semantic_error(ctx, ERR_INTERNAL, NULL);
            }

            // záznam o aliasu, kvůli hledání cyklů
            AliasPath current = {.name = name, .previous = path};

            // nahradí TYPE_CUSTOM novým rozvinutým typem
            type_replace(slot, replacement);

            // pokračuje v hledání dalších aliasů
            return expand_aliases(slot, ctx, &current);
        }

        case TYPE_UNION: {
            // zavolá rozložení aliasů nad pravým i levým podstromem
            ErrorCode result = expand_aliases(&type->data.union_type.left, ctx, path);

            if (result != ERR_OK) {
                return semantic_error(ctx, result, NULL);
            }

            return expand_aliases(&type->data.union_type.right, ctx, path);
        }

        case TYPE_LIST:
            // zavolá rozvinutí aliasů nad seznamem prvků listu
            return expand_aliases(&type->data.list_element_type, ctx, path);

        case TYPE_FUNCTION: {
            // Rozvine aliasy ve všech parametrech
            TypeInfo **param = &type->data.func_type.param_types;

            while (*param != NULL) {
                ErrorCode result = expand_aliases(param, ctx, path);

                if (result != ERR_OK) {
                    return semantic_error(ctx, result, NULL);
                }

                param = &(*param)->next;
            }

            // Rozvine alias v návratovém typu
            return expand_aliases(&type->data.func_type.return_type, ctx, path);
        }

        case TYPE_INT:
        case TYPE_DOUBLE:
        case TYPE_STRING:
        case TYPE_BOOLEAN:
        case TYPE_UNIT:
        case TYPE_ANY:
        case TYPE_NOTHING:
            // u ostatních typů není co rozvíjet
            return ERR_OK;

        default:
            return semantic_error(ctx, ERR_INTERNAL, NULL);
    }
}

ErrorCode semantic_resolve_type_alias(const TypeInfo *type, SemanticContext *ctx,
                                      TypeInfo **result) {
    if (type == NULL || ctx == NULL || result == NULL) {
        return semantic_error(ctx, ERR_INTERNAL, NULL);
    }

    // vytvoří si kopii typu, který má rozvinout
    *result = type_info_clone(type);

    if (*result == NULL) {
        return semantic_error(ctx, ERR_INTERNAL, NULL);
    }

    // zavolá pomocnou funkci na rozvinutí aliasů
    ErrorCode error = expand_aliases(result, ctx, NULL);

    if (error != ERR_OK) {
        type_info_free(*result);
        *result = NULL;
        return semantic_error(ctx, error, NULL);
    }

    // rozvinutím se můžou objevit množiny v jjiném tvaru než je očekáván podle ast, funkce ho
    // upraví do správného tvaru
    normalise_union_tree(*result);
    remove_union_duplicates(result);

    return ERR_OK;
}
