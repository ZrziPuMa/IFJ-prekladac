/**
 * @file ast.c
 * @brief Implementace AST.
 * @author František Bilavčík (<xbilavf00>)
 *
 * (AI) Při programování byl použit Github Copilot,
 * který jsem používal jako inline suggestion.
 * Na funkce a části označené (AI) byl použit Claude OPUS 5.5
 */

#include "ast.h"

#include "token.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief Pomocná funkce pro vytvoření nového uzlu AST
 * @param type Typ uzlu AST.
 * @param line Číslo řádku ve zdrojovém souboru.
 * @return Nový uzel AST nebo NULL v případě chyby alokace.
 */
static ASTNode *ast_new_node(ASTNodeType type, int line) {
    ASTNode *node = calloc(1, sizeof(ASTNode));
    if (!node)
        return NULL;

    node->type = type;
    node->line = line;
    node->expr_type = NULL;
    node->to_double = false;
    node->next = NULL;

    return node;
}

/**
 * @brief Pomocná funkce pro vytvoření kopie řetězce.
 * @param src Zdrojový řetězec.
 * @return Nová kopie řetězce nebo NULL v případě chyby alokace.
 *
 * (AI) prompt: Naimplementuj strdup
 */
static char *ast_strdup(const char *src) {
    size_t len = strlen(src) + 1; // včetně posledního charu '\0'
    char *copy = malloc(len);
    if (copy != NULL)
        memcpy(copy, src, len);
    return copy;
}

ASTNode *ast_create_binop(int op_token, ASTNode *left, ASTNode *right, int line) {

    ASTNode *node = ast_new_node(AST_BINARY_OP, line);
    if (node == NULL) {
        return NULL;
    }
    node->data.bin_op.operator_token = op_token;
    node->data.bin_op.left = left;
    node->data.bin_op.right = right;

    return node;
}

ASTNode *ast_create_func_call(ASTNode *target, ASTNode *arg_groups, int line) {
    ASTNode *node = ast_new_node(AST_FUNC_CALL, line);
    if (node == NULL) {
        return NULL;
    }
    node->data.func_call.target = target;
    node->data.func_call.arg_groups = arg_groups;
    node->data.func_call.own_groups = -1; // ještě neurčeno
    node->data.func_call.call_kind = CALL_UNRESOLVED;

    return node;
}

ASTNode *ast_create_if_expr(ASTNode *cond, ASTNode *then_blk, ASTNode *else_blk, int line) {
    ASTNode *node = ast_new_node(AST_IF_EXPR, line);
    if (node == NULL) {
        return NULL;
    }
    node->data.if_expr.condition = cond;
    node->data.if_expr.then_block = then_blk;
    node->data.if_expr.else_block = else_blk;

    return node;
}

ASTNode *ast_create_match(ASTNode *expr, ASTNode *cases, int line) {
    ASTNode *node = ast_new_node(AST_MATCH_EXPR, line);
    if (node == NULL) {
        return NULL;
    }
    node->data.match_expr.matched_expr = expr;
    node->data.match_expr.cases = cases;

    return node;
}

ASTNode *ast_create_lambda(ASTNode *params, ASTNode *body, int line) {
    ASTNode *node = ast_new_node(AST_LAMBDA, line);
    if (node == NULL) {
        return NULL;
    }
    node->data.lambda.param_list = params;
    node->data.lambda.body = body;

    return node;
}

ASTNode *ast_create_int_lit(long long value, int line) {
    ASTNode *node = ast_new_node(AST_LITERAL, line);
    if (node == NULL) {
        return NULL;
    }
    node->data.literal.kind = LIT_INT;
    node->data.literal.value.i_val = value;

    return node;
}

ASTNode *ast_create_double_lit(double value, int line) {
    ASTNode *node = ast_new_node(AST_LITERAL, line);
    if (node == NULL) {
        return NULL;
    }
    node->data.literal.kind = LIT_DOUBLE;
    node->data.literal.value.d_val = value;

    return node;
}

ASTNode *ast_create_string_lit(const char *value, int line) {
    if (value == NULL) {
        return NULL;
    }

    ASTNode *node = ast_new_node(AST_LITERAL, line);
    if (node == NULL) {
        return NULL;
    }

    node->data.literal.kind = LIT_STRING;
    node->data.literal.value.s_val = ast_strdup(value);
    if (node->data.literal.value.s_val == NULL) {
        free(node);
        return NULL;
    }

    return node;
}

ASTNode *ast_create_bool_lit(bool value, int line) {
    ASTNode *node = ast_new_node(AST_LITERAL, line);
    if (node == NULL) {
        return NULL;
    }
    node->data.literal.kind = LIT_BOOL;
    node->data.literal.value.b_val = value;

    return node;
}

ASTNode *ast_create_unit_lit(int line) {
    ASTNode *node = ast_new_node(AST_LITERAL, line);
    if (node == NULL) {
        return NULL;
    }
    node->data.literal.kind = LIT_UNIT;

    return node;
}

ASTNode *ast_create_program(ASTNode *definitions, int line) {
    ASTNode *node = ast_new_node(AST_PROGRAM, line);
    if (node == NULL) {
        return NULL;
    }
    node->data.program.definitions = definitions;

    return node;
}

ASTNode *ast_create_block(ASTNode *statements, int line) {
    if (statements == NULL) {
        return NULL;
    }
    ASTNode *node = ast_new_node(AST_BLOCK, line);
    if (node == NULL) {
        return NULL;
    }
    node->data.block.statements = statements;

    return node;
}

ASTNode *ast_wrap_block(ASTNode *stmt, int line) {
    if (stmt == NULL) {
        return NULL;
    }
    // (AI) prompt: Je podmínka správně? if (stmt->next ==NULL)
    if (stmt->type == AST_BLOCK && stmt->next == NULL) {
        return stmt;
    }
    return ast_create_block(stmt, line);
}

ASTNode *ast_create_assignment(const char *target, ASTNode *value, int line) {

    // (AI) Prompt: Je potřeba kontroly ještě něčeho?
    if (target == NULL || value == NULL) {
        return NULL;
    }

    ASTNode *node = ast_new_node(AST_ASSIGNMENT, line);
    if (node == NULL) {
        return NULL;
    }

    node->data.assignment.target = ast_strdup(target);
    if (node->data.assignment.target == NULL) {
        free(node);
        return NULL;
    }
    node->data.assignment.value = value;
    node->data.assignment.var_id = 0; // ještě neurčeno

    return node;
}

ASTNode *ast_create_while(ASTNode *condition, ASTNode *body, int line) {
    if (condition == NULL || body == NULL) {
        return NULL;
    }
    ASTNode *node = ast_new_node(AST_WHILE, line);
    if (node == NULL) {
        return NULL;
    }
    node->data.while_loop.condition = condition;
    node->data.while_loop.body = body;

    return node;
}

ASTNode *ast_create_return(ASTNode *value, int line) {
    ASTNode *node = ast_new_node(AST_RETURN, line);
    if (node == NULL) {
        return NULL;
    }
    node->data.return_stmt.value = value;

    return node;
}

ASTNode *ast_create_unary_op(int op_token, ASTNode *operand, int line) {
    if (operand == NULL) {
        return NULL;
    }
    ASTNode *node = ast_new_node(AST_UNARY_OP, line);
    if (node == NULL) {
        return NULL;
    }
    node->data.unary_op.operator_token = op_token;
    node->data.unary_op.operand = operand;

    return node;
}

ASTNode *ast_create_class_def(const char *name, ASTNode *fields, int line) {
    // fields může byt NULL, značí to např: case class Empty()
    if (name == NULL) {
        return NULL;
    }

    ASTNode *node = ast_new_node(AST_CLASS_DEF, line);
    if (node == NULL) {
        return NULL;
    }

    node->data.class_def.name = ast_strdup(name);
    if (node->data.class_def.name == NULL) {
        free(node);
        return NULL;
    }
    node->data.class_def.fields = fields;

    return node;
}

ASTNode *ast_create_type_alias(const char *name, TypeInfo *aliased, int line) {
    if (name == NULL || aliased == NULL) {
        return NULL;
    }

    ASTNode *node = ast_new_node(AST_TYPE_ALIAS, line);
    if (node == NULL) {
        return NULL;
    }

    node->data.type_alias.name = ast_strdup(name);
    if (node->data.type_alias.name == NULL) {
        free(node);
        return NULL;
    }
    node->data.type_alias.aliased = aliased;

    return node;
}

ASTNode *ast_create_extension_def(ASTNode *receiver, ASTNode *methods, int line) {
    if (receiver == NULL || methods == NULL) {
        return NULL;
    }

    ASTNode *node = ast_new_node(AST_EXTENSION_DEF, line);
    if (node == NULL) {
        return NULL;
    }

    node->data.extension_def.receiver = receiver;
    node->data.extension_def.methods = methods;

    return node;
}

ASTNode *ast_create_range(ASTNode *start, ASTNode *end, ASTNode *step, bool is_until, int line) {
    if (start == NULL || end == NULL) {
        return NULL;
    }

    ASTNode *node = ast_new_node(AST_RANGE, line);
    if (node == NULL) {
        return NULL;
    }

    node->data.range.is_until = is_until;
    node->data.range.start = start;
    node->data.range.end = end;
    node->data.range.step = step;

    return node;
}

ASTNode *ast_create_func_def(const char *name, bool is_main, ASTNode *param_groups,
                             TypeInfo *return_type, ASTNode *body, int line) {
    if (name == NULL || body == NULL) {
        return NULL;
    }

    ASTNode *node = ast_new_node(AST_FUNC_DEF, line);
    if (node == NULL) {
        return NULL;
    }

    node->data.func_def.name = ast_strdup(name);
    if (node->data.func_def.name == NULL) {
        free(node);
        return NULL;
    }
    node->data.func_def.is_main = is_main;
    node->data.func_def.param_groups = param_groups;
    node->data.func_def.return_type = return_type;
    node->data.func_def.body = body;

    return node;
}

ASTNode *ast_create_var_def(const char *identifier, bool is_mutable, TypeInfo *declared_type,
                            ASTNode *initializer, int line) {

    // "_" ve Point(_, y) je povolené, to značí, že i identifier i initializer mohou být NULL. Jinak
    // nelze mit proměnnou bez jména.
    if (identifier == NULL && initializer != NULL) {
        return NULL;
    }

    ASTNode *node = ast_new_node(AST_VAR_DEF, line);
    if (node == NULL) {
        return NULL;
    }

    if (identifier != NULL) {
        node->data.var_def.identifier = ast_strdup(identifier);
        if (node->data.var_def.identifier == NULL) {
            free(node);
            return NULL;
        }
    } else {
        node->data.var_def.identifier = NULL; // pro `_` ve vzoru Point(_, y)
    }
    node->data.var_def.is_mutable = is_mutable;
    node->data.var_def.declared_type = declared_type;
    node->data.var_def.initializer = initializer;
    node->data.var_def.var_id = 0; // ještě neurčeno

    return node;
}

ASTNode *ast_create_for(ASTNode *iter_var, ASTNode *source, ASTNode *guards, ASTNode *body,
                        int line) {
    if (iter_var == NULL || source == NULL || body == NULL) {
        return NULL;
    }

    ASTNode *node = ast_new_node(AST_FOR, line);
    if (node == NULL) {
        return NULL;
    }

    node->data.for_loop.iter_var = iter_var;
    node->data.for_loop.source = source;
    node->data.for_loop.guards = guards;
    node->data.for_loop.body = body;

    return node;
}

ASTNode *ast_create_identifier(const char *name, int line) {
    if (name == NULL) {
        return NULL;
    }

    ASTNode *node = ast_new_node(AST_IDENTIFIER, line);
    if (node == NULL) {
        return NULL;
    }

    node->data.identifier.name = ast_strdup(name);
    if (node->data.identifier.name == NULL) {
        free(node);
        return NULL;
    }
    node->data.identifier.var_id = 0; // ještě neurčeno

    return node;
}

ASTNode *ast_create_member_access(ASTNode *object, const char *member_name, int line) {
    if (object == NULL || member_name == NULL) {
        return NULL;
    }

    ASTNode *node = ast_new_node(AST_MEMBER_ACCESS, line);
    if (node == NULL) {
        return NULL;
    }

    node->data.member_access.object = object;
    node->data.member_access.member_name = ast_strdup(member_name);
    if (node->data.member_access.member_name == NULL) {
        free(node);
        return NULL;
    }
    node->data.member_access.member_kind = MEMBER_UNRESOLVED; // vyplní sémantika

    return node;
}

ASTNode *ast_create_case_branch(ASTNode *pattern, ASTNode *guard, ASTNode *body, int line) {
    if (pattern == NULL || body == NULL) {
        return NULL;
    }

    ASTNode *node = ast_new_node(AST_CASE_BRANCH, line);
    if (node == NULL) {
        return NULL;
    }

    node->data.case_branch.pattern = pattern;
    node->data.case_branch.guard = guard;
    node->data.case_branch.body = body;

    return node;
}

ASTNode *ast_create_pattern_literal(ASTNode *literal, int line) {
    if (literal == NULL) {
        return NULL;
    }

    ASTNode *node = ast_new_node(AST_PATTERN, line);
    if (node == NULL) {
        return NULL;
    }

    node->data.pattern.pat_type = PATTERN_LITERAL;
    node->data.pattern.detail.literal_val = literal;
    node->data.pattern.bound_identifier = NULL;
    node->data.pattern.var_id = 0; // ještě neurčeno

    return node;
}

ASTNode *ast_create_pattern_binding(const char *bound_identifier, TypeInfo *type_bind, int line) {
    if (bound_identifier == NULL || type_bind == NULL) {
        return NULL;
    }

    ASTNode *node = ast_new_node(AST_PATTERN, line);
    if (node == NULL) {
        return NULL;
    }

    node->data.pattern.pat_type = PATTERN_TYPE_BINDING;
    node->data.pattern.detail.type_bind = type_bind;
    node->data.pattern.bound_identifier = ast_strdup(bound_identifier);
    if (node->data.pattern.bound_identifier == NULL) {
        free(node);
        return NULL;
    }
    node->data.pattern.var_id = 0; // ještě neurčeno

    return node;
}

ASTNode *ast_create_pattern_constructor(TypeInfo *class_type, ASTNode *args, int line) {
    if (class_type == NULL) {
        return NULL;
    }

    ASTNode *node = ast_new_node(AST_PATTERN, line);
    if (node == NULL) {
        return NULL;
    }

    node->data.pattern.pat_type = PATTERN_CONSTRUCTOR;
    node->data.pattern.detail.constructor.class_type = class_type;
    node->data.pattern.detail.constructor.args = args;

    return node;
}

ASTNode *ast_create_pattern_wildcard(int line) {
    ASTNode *node = ast_new_node(AST_PATTERN, line);
    if (node == NULL) {
        return NULL;
    }

    node->data.pattern.pat_type = PATTERN_WILDCARD;
    node->data.pattern.bound_identifier = NULL;
    node->data.pattern.var_id = 0;

    return node;
}

ASTNode *ast_create_param_list(ASTNode *params, int line) {
    ASTNode *node = ast_new_node(AST_PARAM_LIST, line);
    if (node == NULL) {
        return NULL;
    }
    node->data.list.items = params;

    return node;
}

ASTNode *ast_create_list_literal(ASTNode *items, int line) {
    ASTNode *node = ast_new_node(AST_LIST_LITERAL, line);
    if (node == NULL) {
        return NULL;
    }
    node->data.list.items = items;

    return node;
}

ASTNode *ast_create_arg_list(ASTNode *args, int line) {
    ASTNode *node = ast_new_node(AST_ARG_LIST, line);
    if (node == NULL) {
        return NULL;
    }
    node->data.list.items = args;

    return node;
}

TypeInfo *type_create_basic(TypeClass type_class) {
    switch (type_class) {
        case TYPE_UNIT:
        case TYPE_INT:
        case TYPE_DOUBLE:
        case TYPE_STRING:
        case TYPE_BOOLEAN:
        case TYPE_ANY:
        case TYPE_NOTHING:
            break;
        default:
            return NULL;
    }

    TypeInfo *type_info = calloc(1, sizeof *type_info);
    if (type_info == NULL)
        return NULL;

    type_info->type_class = type_class;
    type_info->next = NULL;
    return type_info;
}

TypeInfo *type_create_union(TypeInfo *left, TypeInfo *right) {
    if (left == NULL || right == NULL) {
        return NULL;
    }

    TypeInfo *type_info = calloc(1, sizeof *type_info);
    if (type_info == NULL)
        return NULL;

    type_info->type_class = TYPE_UNION;
    type_info->next = NULL;
    type_info->data.union_type.left = left;
    type_info->data.union_type.right = right;

    return type_info;
}

TypeInfo *type_create_function(TypeInfo *param_types, TypeInfo *return_type) {
    if (return_type == NULL) {
        return NULL;
    }

    TypeInfo *type_info = calloc(1, sizeof *type_info);
    if (type_info == NULL)
        return NULL;

    type_info->type_class = TYPE_FUNCTION;
    type_info->next = NULL;
    type_info->data.func_type.param_types = param_types;
    type_info->data.func_type.return_type = return_type;

    return type_info;
}

TypeInfo *type_create_list(TypeInfo *element_type) {
    if (element_type == NULL) {
        return NULL;
    }

    TypeInfo *type_info = calloc(1, sizeof *type_info);
    if (type_info == NULL)
        return NULL;

    type_info->type_class = TYPE_LIST;
    type_info->next = NULL;
    type_info->data.list_element_type = element_type;

    return type_info;
}

TypeInfo *type_create_custom(const char *custom_name) {
    if (custom_name == NULL) {
        return NULL;
    }

    TypeInfo *type_info = calloc(1, sizeof *type_info);
    if (type_info == NULL)
        return NULL;

    type_info->type_class = TYPE_CUSTOM;
    type_info->next = NULL;
    type_info->data.custom_name = ast_strdup(custom_name);
    if (type_info->data.custom_name == NULL) {
        free(type_info);
        return NULL;
    }

    return type_info;
}

TypeInfo *type_append(TypeInfo *head, TypeInfo *item) {
    if (item == NULL) {
        return head;
    }
    if (head == NULL) {
        return item;
    }

    TypeInfo *current = head;
    while (current->next != NULL) {
        current = current->next;
    }
    current->next = item;

    return head;
}

ASTNode *ast_append(ASTNode *head, ASTNode *item) {
    if (item == NULL) {
        return head;
    }
    if (head == NULL) {
        return item;
    }

    ASTNode *current = head;
    while (current->next != NULL) {
        current = current->next;
    }
    current->next = item;

    return head;
}

int ast_wrap_in_call(ASTNode **slot) {
    if (slot == NULL || *slot == NULL)
        return 99;

    ASTNode *node = *slot;
    ASTNode *call_node = ast_create_func_call(node, NULL, node->line);
    if (call_node == NULL)
        return 99;

    call_node->data.func_call.own_groups = 0;
    call_node->next = node->next;
    node->next = NULL;

    *slot = call_node;
    return 0;
}

void ast_set_type(ASTNode *node, TypeInfo *owned) {
    if (node == NULL) {
        type_info_free(owned);
        return;
    }
    if (node->expr_type == owned)
        return;

    type_info_free(node->expr_type);
    node->expr_type = owned;
}

void type_info_free(TypeInfo *root) {
    while (root != NULL) {
        TypeInfo *next = root->next;

        switch (root->type_class) {
            case TYPE_UNION:
                type_info_free(root->data.union_type.left);
                type_info_free(root->data.union_type.right);
                break;
            case TYPE_FUNCTION:
                type_info_free(root->data.func_type.param_types);
                type_info_free(root->data.func_type.return_type);
                break;
            case TYPE_LIST:
                type_info_free(root->data.list_element_type);
                break;
            case TYPE_CUSTOM:
                free(root->data.custom_name);
                break;
            default:
                break;
        }

        free(root);
        root = next;
    }
}

static TypeInfo *type_chain_clone(const TypeInfo *src);

TypeInfo *type_info_clone(const TypeInfo *src) {
    if (src == NULL)
        return NULL;

    TypeInfo *copy = calloc(1, sizeof(TypeInfo));
    if (copy == NULL)
        return NULL;

    copy->type_class = src->type_class;

    switch (src->type_class) {
        case TYPE_UNION:
            copy->data.union_type.left = type_info_clone(src->data.union_type.left);
            if (copy->data.union_type.left == NULL) {
                type_info_free(copy);
                return NULL;
            }
            copy->data.union_type.right = type_info_clone(src->data.union_type.right);
            if (copy->data.union_type.right == NULL) {
                type_info_free(copy);
                return NULL;
            }
            break;
        case TYPE_FUNCTION:
            // (AI) prompt: Oprav kopii param_types v type_info_clone (celý řetěz, NULL = bez
            // parametrů)
            copy->data.func_type.param_types = type_chain_clone(src->data.func_type.param_types);
            if (src->data.func_type.param_types != NULL &&
                copy->data.func_type.param_types == NULL) {
                type_info_free(copy);
                return NULL;
            }
            copy->data.func_type.return_type = type_info_clone(src->data.func_type.return_type);
            if (copy->data.func_type.return_type == NULL) {
                type_info_free(copy);
                return NULL;
            }
            break;
        case TYPE_LIST:
            copy->data.list_element_type = type_info_clone(src->data.list_element_type);
            if (copy->data.list_element_type == NULL) {
                type_info_free(copy);
                return NULL;
            }
            break;
        case TYPE_CUSTOM:
            copy->data.custom_name = ast_strdup(src->data.custom_name);
            if (copy->data.custom_name == NULL) {
                type_info_free(copy);
                return NULL;
            }
            break;
        default:
            break;
    }

    return copy;
}

// (AI) prompt: Oprav kopii param_types v type_info_clone (celý řetěz, NULL = bez parametrů)
static TypeInfo *type_chain_clone(const TypeInfo *src) {
    TypeInfo *head = NULL;
    TypeInfo *tail = NULL;

    for (const TypeInfo *item = src; item != NULL; item = item->next) {
        TypeInfo *item_copy = type_info_clone(item); // kopie má next == NULL
        if (item_copy == NULL) {
            type_info_free(head); // uvolní celý dosud zkopírovaný řetěz
            return NULL;
        }
        if (head == NULL)
            head = item_copy;
        else
            tail->next = item_copy;
        tail = item_copy;
    }

    return head;
}

void type_replace(TypeInfo **slot, TypeInfo *owned) {
    if (slot == NULL) {
        type_info_free(owned);
        return;
    }
    if (owned == NULL) {
        return;
    }
    if (*slot == NULL) {
        *slot = owned;
        return;
    }

    TypeInfo *old_type = *slot;
    owned->next = old_type->next;
    old_type->next = NULL;
    *slot = owned;
    type_info_free(old_type);
}

void ast_free(ASTNode *root) {
    while (root != NULL) {
        ASTNode *next = root->next;

        switch (root->type) {
            case AST_BINARY_OP:
                ast_free(root->data.bin_op.left);
                ast_free(root->data.bin_op.right);
                break;
            case AST_FUNC_CALL:
                ast_free(root->data.func_call.target);
                ast_free(root->data.func_call.arg_groups);
                break;
            case AST_IF_EXPR:
                ast_free(root->data.if_expr.condition);
                ast_free(root->data.if_expr.then_block);
                ast_free(root->data.if_expr.else_block);
                break;
            case AST_MATCH_EXPR:
                ast_free(root->data.match_expr.matched_expr);
                ast_free(root->data.match_expr.cases);
                break;
            case AST_LAMBDA:
                ast_free(root->data.lambda.param_list);
                ast_free(root->data.lambda.body);
                break;
            case AST_ASSIGNMENT:
                free(root->data.assignment.target);
                ast_free(root->data.assignment.value);
                break;
            case AST_WHILE:
                ast_free(root->data.while_loop.condition);
                ast_free(root->data.while_loop.body);
                break;
            case AST_RETURN:
                ast_free(root->data.return_stmt.value);
                break;
            case AST_UNARY_OP:
                ast_free(root->data.unary_op.operand);
                break;
            case AST_CLASS_DEF:
                free(root->data.class_def.name);
                ast_free(root->data.class_def.fields);
                break;
            case AST_TYPE_ALIAS:
                free(root->data.type_alias.name);
                type_info_free(root->data.type_alias.aliased);
                break;
            case AST_EXTENSION_DEF:
                ast_free(root->data.extension_def.receiver);
                ast_free(root->data.extension_def.methods);
                break;
            case AST_RANGE:
                ast_free(root->data.range.start);
                ast_free(root->data.range.end);
                ast_free(root->data.range.step);
                break;
            case AST_FUNC_DEF:
                free(root->data.func_def.name);
                ast_free(root->data.func_def.param_groups);
                type_info_free(root->data.func_def.return_type);
                ast_free(root->data.func_def.body);
                break;
            case AST_VAR_DEF:
                free(root->data.var_def.identifier); // může být NULL pro `_` ve vzoru Point(_, y)
                type_info_free(root->data.var_def.declared_type);
                ast_free(root->data.var_def.initializer);
                break;
            case AST_FOR:
                ast_free(root->data.for_loop.iter_var);
                ast_free(root->data.for_loop.source);
                ast_free(root->data.for_loop.guards);
                ast_free(root->data.for_loop.body);
                break;
            case AST_IDENTIFIER:
                free(root->data.identifier.name);
                break;
            case AST_MEMBER_ACCESS:
                ast_free(root->data.member_access.object);
                free(root->data.member_access.member_name);
                break;
            case AST_CASE_BRANCH:
                ast_free(root->data.case_branch.pattern);
                ast_free(root->data.case_branch.guard);
                ast_free(root->data.case_branch.body);
                break;
            case AST_PATTERN:
                switch (root->data.pattern.pat_type) {
                    case PATTERN_LITERAL:
                        ast_free(root->data.pattern.detail.literal_val);
                        break;
                    case PATTERN_TYPE_BINDING:
                        free(root->data.pattern.bound_identifier);
                        type_info_free(root->data.pattern.detail.type_bind);
                        break;
                    case PATTERN_CONSTRUCTOR:
                        type_info_free(root->data.pattern.detail.constructor.class_type);
                        ast_free(root->data.pattern.detail.constructor.args);
                        break;
                    case PATTERN_WILDCARD:
                        break;
                    default:
                        break;
                }
                break;
            case AST_PARAM_LIST:
                ast_free(root->data.list.items);
                break;
            case AST_LIST_LITERAL:
                ast_free(root->data.list.items);
                break;
            case AST_ARG_LIST:
                ast_free(root->data.list.items);
                break;
            case AST_LITERAL:
                switch (root->data.literal.kind) {
                    case LIT_STRING:
                        free(root->data.literal.value.s_val);
                        break;
                    case LIT_INT:
                        break;
                    case LIT_DOUBLE:
                        break;
                    case LIT_BOOL:
                        break;
                    case LIT_UNIT:
                        break;
                }
                break;
            case AST_PROGRAM:
                ast_free(root->data.program.definitions);
                break;
            case AST_BLOCK:
                ast_free(root->data.block.statements);
                break;
        }
        type_info_free(root->expr_type);
        free(root);
        root = next;
    }
}

/*========================================================================== *
 *  LADĚNÍ A TESTY (AI)
 * ========================================================================== */

// (AI) Claude Opus 5.5, prompt: "Proveď implementaci verzovací a vypisovací funkce pro testování."
// Týká se všeho od této značky do konce souboru: ast_check, ast_dump a jejich pomocné funkce.

/**
 * @brief Jméno druhu uzlu bez předpony AST_ (pro ast_dump a hlášení ast_check).
 * @param type Druh uzlu.
 * @return Jméno, nebo "?" pro neplatnou hodnotu.
 */
static const char *node_kind_name(ASTNodeType type) {
    switch (type) {
        case AST_ARG_LIST:
            return "ARG_LIST";
        case AST_ASSIGNMENT:
            return "ASSIGNMENT";
        case AST_BINARY_OP:
            return "BINARY_OP";
        case AST_BLOCK:
            return "BLOCK";
        case AST_CASE_BRANCH:
            return "CASE_BRANCH";
        case AST_CLASS_DEF:
            return "CLASS_DEF";
        case AST_EXTENSION_DEF:
            return "EXTENSION_DEF";
        case AST_FOR:
            return "FOR";
        case AST_FUNC_CALL:
            return "FUNC_CALL";
        case AST_FUNC_DEF:
            return "FUNC_DEF";
        case AST_IDENTIFIER:
            return "IDENTIFIER";
        case AST_IF_EXPR:
            return "IF_EXPR";
        case AST_LAMBDA:
            return "LAMBDA";
        case AST_LIST_LITERAL:
            return "LIST_LITERAL";
        case AST_LITERAL:
            return "LITERAL";
        case AST_MATCH_EXPR:
            return "MATCH_EXPR";
        case AST_MEMBER_ACCESS:
            return "MEMBER_ACCESS";
        case AST_PARAM_LIST:
            return "PARAM_LIST";
        case AST_PATTERN:
            return "PATTERN";
        case AST_PROGRAM:
            return "PROGRAM";
        case AST_RANGE:
            return "RANGE";
        case AST_RETURN:
            return "RETURN";
        case AST_TYPE_ALIAS:
            return "TYPE_ALIAS";
        case AST_UNARY_OP:
            return "UNARY_OP";
        case AST_VAR_DEF:
            return "VAR_DEF";
        case AST_WHILE:
            return "WHILE";
    }
    return "?";
}

/**
 * @brief Zápis operátoru v IFJ26 podle typu tokenu.
 * @param op Typ tokenu z token.h (operator_token v uzlu).
 * @return Zápis operátoru, nebo NULL, když token není operátor výrazu.
 */
static const char *op_text(int op) {
    switch (op) {
        case TOK_PLUS:
            return "+";
        case TOK_MINUS:
            return "-";
        case TOK_MUL:
            return "*";
        case TOK_DIV:
            return "/";
        case TOK_NOT:
            return "!";
        case TOK_CONCAT:
            return "++";
        case TOK_LESSTHEN:
            return "<";
        case TOK_LESSEQ:
            return "<=";
        case TOK_GREATERTHEN:
            return ">";
        case TOK_GREATEREQ:
            return ">=";
        case TOK_EQ:
            return "==";
        case TOK_NOTEQ:
            return "!=";
        case TOK_AND:
            return "&&";
        case TOK_OR:
            return "||";
        default:
            return NULL;
    }
}

/** @brief true, když op smí stát v AST_BINARY_OP. */
static bool is_binary_op(int op) {
    return op_text(op) != NULL && op != TOK_NOT;
}

/** @brief true, když op smí stát v AST_UNARY_OP (-, + nebo !). */
static bool is_unary_op(int op) {
    return op == TOK_MINUS || op == TOK_PLUS || op == TOK_NOT;
}

/** @brief Jméno druhu volání bez předpony CALL_, nebo NULL pro neplatnou hodnotu. */
static const char *call_kind_name(CallKind kind) {
    switch (kind) {
        case CALL_UNRESOLVED:
            return "UNRESOLVED";
        case CALL_USER_FUNC:
            return "USER_FUNC";
        case CALL_BUILTIN:
            return "BUILTIN";
        case CALL_CONSTRUCTOR:
            return "CONSTRUCTOR";
        case CALL_EXTENSION:
            return "EXTENSION";
        case CALL_LIST_METHOD:
            return "LIST_METHOD";
        case CALL_VALUE:
            return "VALUE";
    }
    return NULL;
}

/** @brief Jméno druhu členu bez předpony MEMBER_, nebo NULL pro neplatnou hodnotu. */
static const char *member_kind_name(MemberKind kind) {
    switch (kind) {
        case MEMBER_UNRESOLVED:
            return "UNRESOLVED";
        case MEMBER_FIELD:
            return "FIELD";
        case MEMBER_VALUE_BUILTIN:
            return "VALUE_BUILTIN";
        case MEMBER_LIST_BUILTIN:
            return "LIST_BUILTIN";
        case MEMBER_RANGE_TOLIST:
            return "RANGE_TOLIST";
        case MEMBER_EXTENSION:
            return "EXTENSION";
    }
    return NULL;
}

/**
 * @brief var_id uzlu, který ho má (VAR_DEF, IDENTIFIER, ASSIGNMENT, PATTERN).
 * @param has Sem se zapíše, zda uzel pole var_id má.
 * @return Hodnota var_id, nebo 0, když ho uzel nemá.
 */
static int node_var_id(const ASTNode *n, bool *has) {
    *has = true;
    if (n->type == AST_VAR_DEF)
        return n->data.var_def.var_id;
    if (n->type == AST_IDENTIFIER)
        return n->data.identifier.var_id;
    if (n->type == AST_ASSIGNMENT)
        return n->data.assignment.var_id;
    if (n->type == AST_PATTERN)
        return n->data.pattern.var_id;
    *has = false;
    return 0;
}

/* -------------------------------------------------------------------------- *
 *  ast_dump
 * -------------------------------------------------------------------------- */

/**
 * @brief Vypíše typ zápisem IFJ26 (Int | String, (Int) => Int, List[Int], Point).
 * @param paren_func Funkční typ uzavřít do závorek (stojí ve sjednocení).
 * @param paren_union Sjednocení uzavřít do závorek (stojí vpravo ve sjednocení).
 */
static void dump_type(const TypeInfo *t, FILE *out, bool paren_func, bool paren_union) {
    if (t == NULL) {
        fputs("<null>", out);
        return;
    }
    switch (t->type_class) {
        case TYPE_UNIT:
            fputs("Unit", out);
            return;
        case TYPE_INT:
            fputs("Int", out);
            return;
        case TYPE_DOUBLE:
            fputs("Double", out);
            return;
        case TYPE_STRING:
            fputs("String", out);
            return;
        case TYPE_BOOLEAN:
            fputs("Boolean", out);
            return;
        case TYPE_ANY:
            fputs("Any", out);
            return;
        case TYPE_NOTHING:
            fputs("Nothing", out);
            return;
        case TYPE_UNION:
            if (paren_union)
                fputc('(', out);
            dump_type(t->data.union_type.left, out, true, false);
            fputs(" | ", out);
            dump_type(t->data.union_type.right, out, true, true);
            if (paren_union)
                fputc(')', out);
            return;
        case TYPE_FUNCTION:
            if (paren_func)
                fputc('(', out);
            fputc('(', out);
            for (const TypeInfo *p = t->data.func_type.param_types; p != NULL; p = p->next) {
                if (p != t->data.func_type.param_types)
                    fputs(", ", out);
                dump_type(p, out, false, false);
            }
            fputs(") => ", out);
            dump_type(t->data.func_type.return_type, out, false, false);
            if (paren_func)
                fputc(')', out);
            return;
        case TYPE_LIST:
            fputs("List[", out);
            dump_type(t->data.list_element_type, out, false, false);
            fputc(']', out);
            return;
        case TYPE_CUSTOM:
            fputs(t->data.custom_name != NULL ? t->data.custom_name : "<null>", out);
            return;
    }
    fprintf(out, "<type %d>", (int)t->type_class);
}

/** @brief Řetězec v uvozovkách; \ " a řídicí znaky jako escape, aby byl uzel na jednom řádku. */
static void dump_string(const char *s, FILE *out) {
    fputc('"', out);
    for (const unsigned char *p = (const unsigned char *)s; *p != '\0'; p++) {
        switch (*p) {
            case '"':
                fputs("\\\"", out);
                break;
            case '\\':
                fputs("\\\\", out);
                break;
            case '\n':
                fputs("\\n", out);
                break;
            case '\r':
                fputs("\\r", out);
                break;
            case '\t':
                fputs("\\t", out);
                break;
            default:
                if (*p < 0x20 || *p == 0x7f)
                    fprintf(out, "\\x%02X", (unsigned)*p);
                else
                    fputc(*p, out);
                break;
        }
    }
    fputc('"', out);
}

/** @brief Nejkratší zápis, který se přečte zpět na stejný double; vždy s . nebo e (2 -> 2.0). */
static void dump_double(double d, FILE *out) {
    char buf[64];
    for (int prec = 1; prec <= 17; prec++) {
        snprintf(buf, sizeof buf, "%.*g", prec, d);
        if (strtod(buf, NULL) == d)
            break;
    }
    fputs(buf, out);
    if (strpbrk(buf, ".eEn") == NULL) // n: inf a nan
        fputs(".0", out);
}

/** @brief Jméno z uzlu; NULL vypíše jako <null>. */
static void dump_name(const char *s, FILE *out) {
    fputs(s != NULL ? s : "<null>", out);
}

/** @brief Operátor jeho zápisem; neznámý token jako op?<číslo>. */
static void dump_op(int op, FILE *out) {
    const char *text = op_text(op);
    if (text != NULL)
        fputs(text, out);
    else
        fprintf(out, "op?%d", op);
}

/** @brief Před další anotací vypíše " [" (první) nebo ", ". */
static void dump_ann_sep(FILE *out, bool *open) {
    fputs(*open ? ", " : " [", out);
    *open = true;
}

/** @brief Anotace, které nejsou ve stavu po konstruktoru, jako [typ, to_double, var_id N, ...]. */
static void dump_annotations(const ASTNode *n, FILE *out) {
    bool open = false;
    bool has_var_id;
    int var_id = node_var_id(n, &has_var_id);

    if (n->expr_type != NULL) {
        dump_ann_sep(out, &open);
        dump_type(n->expr_type, out, false, false);
    }
    if (n->to_double) {
        dump_ann_sep(out, &open);
        fputs("to_double", out);
    }
    if (has_var_id && var_id != 0) {
        dump_ann_sep(out, &open);
        fprintf(out, "var_id %d", var_id);
    }
    if (n->type == AST_FUNC_CALL) {
        if (n->data.func_call.own_groups != -1) {
            dump_ann_sep(out, &open);
            fprintf(out, "own_groups %d", n->data.func_call.own_groups);
        }
        if (n->data.func_call.call_kind != CALL_UNRESOLVED) {
            const char *name = call_kind_name(n->data.func_call.call_kind);
            dump_ann_sep(out, &open);
            if (name != NULL)
                fputs(name, out);
            else
                fprintf(out, "call_kind %d", (int)n->data.func_call.call_kind);
        }
    }
    if (n->type == AST_MEMBER_ACCESS && n->data.member_access.member_kind != MEMBER_UNRESOLVED) {
        const char *name = member_kind_name(n->data.member_access.member_kind);
        dump_ann_sep(out, &open);
        if (name != NULL)
            fputs(name, out);
        else
            fprintf(out, "member_kind %d", (int)n->data.member_access.member_kind);
    }
    if (open)
        fputc(']', out);
}

/** @brief Data literálu za druhem uzlu: 1, 1.5, "text", true, (). */
static void dump_literal(const ASTNode *n, FILE *out) {
    fputc(' ', out);
    switch (n->data.literal.kind) {
        case LIT_INT:
            fprintf(out, "%lld", n->data.literal.value.i_val);
            return;
        case LIT_DOUBLE:
            dump_double(n->data.literal.value.d_val, out);
            return;
        case LIT_STRING:
            if (n->data.literal.value.s_val != NULL)
                dump_string(n->data.literal.value.s_val, out);
            else
                fputs("<null>", out);
            return;
        case LIT_BOOL:
            fputs(n->data.literal.value.b_val ? "true" : "false", out);
            return;
        case LIT_UNIT:
            fputs("()", out);
            return;
    }
    fprintf(out, "<literal %d>", (int)n->data.literal.kind);
}

/** @brief Data uzlu na jeho řádku (jména, literály, operátor, napsané typy). */
static void dump_node_data(const ASTNode *n, FILE *out) {
    switch (n->type) {
        case AST_ASSIGNMENT:
            fputc(' ', out);
            dump_name(n->data.assignment.target, out);
            break;
        case AST_BINARY_OP:
            fputc(' ', out);
            dump_op(n->data.bin_op.operator_token, out);
            break;
        case AST_CLASS_DEF:
            fputc(' ', out);
            dump_name(n->data.class_def.name, out);
            break;
        case AST_FUNC_DEF:
            fputs(n->data.func_def.is_main ? " @main " : " ", out);
            dump_name(n->data.func_def.name, out);
            if (n->data.func_def.return_type != NULL) {
                fputs(" : ", out);
                dump_type(n->data.func_def.return_type, out, false, false);
            }
            break;
        case AST_IDENTIFIER:
            fputc(' ', out);
            dump_name(n->data.identifier.name, out);
            break;
        case AST_LITERAL:
            dump_literal(n, out);
            break;
        case AST_MEMBER_ACCESS:
            fputc(' ', out);
            dump_name(n->data.member_access.member_name, out);
            break;
        case AST_PATTERN:
            switch (n->data.pattern.pat_type) {
                case PATTERN_LITERAL:
                    fputs(" literal", out);
                    break;
                case PATTERN_TYPE_BINDING:
                    fputs(" binding ", out);
                    dump_name(n->data.pattern.bound_identifier, out);
                    fputs(" : ", out);
                    dump_type(n->data.pattern.detail.type_bind, out, false, false);
                    break;
                case PATTERN_CONSTRUCTOR:
                    fputs(" constructor ", out);
                    dump_type(n->data.pattern.detail.constructor.class_type, out, false, false);
                    break;
                case PATTERN_WILDCARD:
                    fputs(" wildcard", out);
                    break;
                default:
                    fprintf(out, " <pattern %d>", (int)n->data.pattern.pat_type);
                    break;
            }
            break;
        case AST_RANGE:
            fputs(n->data.range.is_until ? " until" : " to", out);
            break;
        case AST_TYPE_ALIAS:
            fputc(' ', out);
            dump_name(n->data.type_alias.name, out);
            fputs(" = ", out);
            dump_type(n->data.type_alias.aliased, out, false, false);
            break;
        case AST_UNARY_OP:
            fputc(' ', out);
            dump_op(n->data.unary_op.operator_token, out);
            break;
        case AST_VAR_DEF:
            fputs(n->data.var_def.is_mutable ? " var " : " val ", out);
            fputs(n->data.var_def.identifier != NULL ? n->data.var_def.identifier : "_", out);
            if (n->data.var_def.declared_type != NULL) {
                fputs(" : ", out);
                dump_type(n->data.var_def.declared_type, out, false, false);
            }
            break;
        case AST_ARG_LIST:
        case AST_BLOCK:
        case AST_CASE_BRANCH:
        case AST_EXTENSION_DEF:
        case AST_FOR:
        case AST_FUNC_CALL:
        case AST_IF_EXPR:
        case AST_LAMBDA:
        case AST_LIST_LITERAL:
        case AST_MATCH_EXPR:
        case AST_PARAM_LIST:
        case AST_PROGRAM:
        case AST_RETURN:
        case AST_WHILE:
            break;
    }
}

static void dump_node(const ASTNode *n, FILE *out, int depth, const char *label);

/** @brief Vypíše uzel a všechny jeho sourozence přes next, každý se stejným popiskem. */
static void dump_chain(const ASTNode *head, FILE *out, int depth, const char *label) {
    for (const ASTNode *n = head; n != NULL; n = n->next)
        dump_node(n, out, depth, label);
}

/** @brief Jeden řádek uzlu (odsazení, popisek, druh, data, anotace) a pod ním jeho potomci. */
static void dump_node(const ASTNode *n, FILE *out, int depth, const char *label) {
    for (int i = 0; i < depth; i++)
        fputs("  ", out);
    if (label != NULL)
        fprintf(out, "%s: ", label);
    if ((unsigned)n->type > (unsigned)AST_WHILE) {
        fprintf(out, "?%d\n", (int)n->type);
        return;
    }
    fputs(node_kind_name(n->type), out);
    dump_node_data(n, out);
    dump_annotations(n, out);
    fputc('\n', out);

    depth++;
    switch (n->type) {
        case AST_ARG_LIST:
        case AST_LIST_LITERAL:
        case AST_PARAM_LIST:
            dump_chain(n->data.list.items, out, depth, NULL);
            break;
        case AST_ASSIGNMENT:
            dump_chain(n->data.assignment.value, out, depth, "value");
            break;
        case AST_BINARY_OP:
            dump_chain(n->data.bin_op.left, out, depth, "left");
            dump_chain(n->data.bin_op.right, out, depth, "right");
            break;
        case AST_BLOCK:
            dump_chain(n->data.block.statements, out, depth, NULL);
            break;
        case AST_CASE_BRANCH:
            dump_chain(n->data.case_branch.pattern, out, depth, "pattern");
            dump_chain(n->data.case_branch.guard, out, depth, "guard");
            dump_chain(n->data.case_branch.body, out, depth, "body");
            break;
        case AST_CLASS_DEF:
            dump_chain(n->data.class_def.fields, out, depth, "field");
            break;
        case AST_EXTENSION_DEF:
            dump_chain(n->data.extension_def.receiver, out, depth, "receiver");
            dump_chain(n->data.extension_def.methods, out, depth, "method");
            break;
        case AST_FOR:
            dump_chain(n->data.for_loop.iter_var, out, depth, "var");
            dump_chain(n->data.for_loop.source, out, depth, "source");
            dump_chain(n->data.for_loop.guards, out, depth, "guard");
            dump_chain(n->data.for_loop.body, out, depth, "body");
            break;
        case AST_FUNC_CALL:
            dump_chain(n->data.func_call.target, out, depth, "target");
            dump_chain(n->data.func_call.arg_groups, out, depth, "args");
            break;
        case AST_FUNC_DEF:
            dump_chain(n->data.func_def.param_groups, out, depth, "params");
            dump_chain(n->data.func_def.body, out, depth, "body");
            break;
        case AST_IF_EXPR:
            dump_chain(n->data.if_expr.condition, out, depth, "cond");
            dump_chain(n->data.if_expr.then_block, out, depth, "then");
            dump_chain(n->data.if_expr.else_block, out, depth, "else");
            break;
        case AST_LAMBDA:
            dump_chain(n->data.lambda.param_list, out, depth, "params");
            dump_chain(n->data.lambda.body, out, depth, "body");
            break;
        case AST_MATCH_EXPR:
            dump_chain(n->data.match_expr.matched_expr, out, depth, "expr");
            dump_chain(n->data.match_expr.cases, out, depth, "case");
            break;
        case AST_MEMBER_ACCESS:
            dump_chain(n->data.member_access.object, out, depth, "object");
            break;
        case AST_PATTERN:
            if (n->data.pattern.pat_type == PATTERN_LITERAL)
                dump_chain(n->data.pattern.detail.literal_val, out, depth, "value");
            else if (n->data.pattern.pat_type == PATTERN_CONSTRUCTOR)
                dump_chain(n->data.pattern.detail.constructor.args, out, depth, "arg");
            break;
        case AST_PROGRAM:
            dump_chain(n->data.program.definitions, out, depth, NULL);
            break;
        case AST_RANGE:
            dump_chain(n->data.range.start, out, depth, "start");
            dump_chain(n->data.range.end, out, depth, "end");
            dump_chain(n->data.range.step, out, depth, "by");
            break;
        case AST_RETURN:
            dump_chain(n->data.return_stmt.value, out, depth, "value");
            break;
        case AST_UNARY_OP:
            dump_chain(n->data.unary_op.operand, out, depth, "operand");
            break;
        case AST_VAR_DEF:
            dump_chain(n->data.var_def.initializer, out, depth, "init");
            break;
        case AST_WHILE:
            dump_chain(n->data.while_loop.condition, out, depth, "cond");
            dump_chain(n->data.while_loop.body, out, depth, "body");
            break;
        case AST_IDENTIFIER:
        case AST_LITERAL:
        case AST_TYPE_ALIAS:
            break;
    }
}

void ast_dump(const ASTNode *root, FILE *out) {
    if (out == NULL)
        return;
    dump_chain(root, out, 0, NULL);
}

/* -------------------------------------------------------------------------- *
 *  ast_check
 * -------------------------------------------------------------------------- */

/** @brief Množina navštívených ukazatelů (uzlů i typů) pro odhalení sdílení a cyklů. */
typedef struct {
    const void **slots; // otevřené adresování, kapacita je mocnina dvou
    size_t cap;
    size_t used;
    bool oom; // selhala alokace, kontrola není úplná
} PtrSet;

static size_t ptrset_index(const void *p, size_t cap) {
    size_t h = (size_t)((uintptr_t)p >> 3);
    h ^= h >> 16;
    h *= (size_t)0x45d9f3bu;
    h ^= h >> 16;
    return h & (cap - 1);
}

/**
 * @brief Přidá ukazatel do množiny.
 * @return 1 = nově přidán, 0 = už tam byl (sdílení nebo cyklus), -1 = chyba alokace.
 */
static int ptrset_add(PtrSet *set, const void *p) {
    if (set->oom)
        return -1;
    if ((set->used + 1) * 2 > set->cap) {
        size_t new_cap = set->cap != 0 ? set->cap * 2 : 256;
        const void **new_slots = calloc(new_cap, sizeof *new_slots);
        if (new_slots == NULL) {
            set->oom = true;
            return -1;
        }
        for (size_t i = 0; i < set->cap; i++) {
            if (set->slots[i] == NULL)
                continue;
            size_t j = ptrset_index(set->slots[i], new_cap);
            while (new_slots[j] != NULL)
                j = (j + 1) & (new_cap - 1);
            new_slots[j] = set->slots[i];
        }
        free(set->slots);
        set->slots = new_slots;
        set->cap = new_cap;
    }
    size_t i = ptrset_index(p, set->cap);
    while (set->slots[i] != NULL) {
        if (set->slots[i] == p)
            return 0;
        i = (i + 1) & (set->cap - 1);
    }
    set->slots[i] = p;
    set->used++;
    return 1;
}

/** @brief Místo ve stromu, podle kterého se kontroluje druh uzlu a pravidla VAR_DEF. */
typedef enum {
    SLOT_ROOT,        // kořen předaný ast_check (obvykle PROGRAM)
    SLOT_DEF,         // definice nejvyšší úrovně v PROGRAM
    SLOT_STMT,        // příkaz v BLOCK
    SLOT_EXPR,        // výraz
    SLOT_CALL_TARGET, // výraz jako cíl FUNC_CALL (jediné místo pro MEMBER_EXTENSION)
    SLOT_BLOCK,       // tělo: vždy AST_BLOCK
    SLOT_PARAM_LIST,  // AST_PARAM_LIST
    SLOT_ARG_LIST,    // AST_ARG_LIST
    SLOT_PARAM,       // VAR_DEF parametru
    SLOT_FIELD,       // VAR_DEF atributu třídy
    SLOT_ITER_VAR,    // VAR_DEF řídicí proměnné for
    SLOT_PAT_ARG,     // VAR_DEF ve vzoru Point(a, _)
    SLOT_RECEIVER,    // VAR_DEF příjemce extension
    SLOT_METHOD,      // FUNC_DEF v extension
    SLOT_CASE,        // CASE_BRANCH v match
    SLOT_PATTERN,     // PATTERN v case
    SLOT_LITERAL      // LITERAL ve vzoru case 0
} CheckSlot;

typedef struct {
    bool after_sem;
    int errors;
    PtrSet seen;
} AstChecker;

/** @brief Vypíše jedno porušení na stderr (řádek a druh uzlu) a započítá ho. */
static void chk_report(AstChecker *c, const ASTNode *n, const char *fmt, ...) {
    va_list ap;
    c->errors++;
    if (n != NULL && (unsigned)n->type <= (unsigned)AST_WHILE)
        fprintf(stderr, "ast_check: řádek %d, %s: ", n->line, node_kind_name(n->type));
    else if (n != NULL)
        fprintf(stderr, "ast_check: řádek %d, ?: ", n->line);
    else
        fputs("ast_check: ", stderr);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
}

/**
 * @brief Zapíše ukazatel jako navštívený.
 * @return true = první návštěva (pokračovat dovnitř), false = sdílený, cyklus nebo nedostatek
 * paměti.
 */
static bool chk_visit(AstChecker *c, const void *p, const ASTNode *owner, const char *what) {
    int r = ptrset_add(&c->seen, p);
    if (r == 0)
        chk_report(c, owner, "%s: sdílený ukazatel nebo cyklus", what);
    return r == 1;
}

/** @brief Uzly, které jsou jen součástí jiného uzlu a nesmí stát na místě výrazu ani příkazu. */
static bool is_structural(ASTNodeType t) {
    return t == AST_PROGRAM || t == AST_PARAM_LIST || t == AST_ARG_LIST || t == AST_CASE_BRANCH ||
           t == AST_PATTERN;
}

/** @brief Smí uzel druhu t stát na místě slot? */
static bool slot_allows(CheckSlot slot, ASTNodeType t) {
    switch (slot) {
        case SLOT_ROOT:
            return true;
        case SLOT_DEF:
            return t == AST_FUNC_DEF || t == AST_CLASS_DEF || t == AST_TYPE_ALIAS ||
                   t == AST_EXTENSION_DEF;
        case SLOT_STMT:
            return !is_structural(t) && t != AST_CLASS_DEF && t != AST_TYPE_ALIAS &&
                   t != AST_EXTENSION_DEF;
        case SLOT_EXPR:
        case SLOT_CALL_TARGET:
            return !is_structural(t) && t != AST_CLASS_DEF && t != AST_TYPE_ALIAS &&
                   t != AST_EXTENSION_DEF && t != AST_FUNC_DEF && t != AST_VAR_DEF;
        case SLOT_BLOCK:
            return t == AST_BLOCK;
        case SLOT_PARAM_LIST:
            return t == AST_PARAM_LIST;
        case SLOT_ARG_LIST:
            return t == AST_ARG_LIST;
        case SLOT_PARAM:
        case SLOT_FIELD:
        case SLOT_ITER_VAR:
        case SLOT_PAT_ARG:
        case SLOT_RECEIVER:
            return t == AST_VAR_DEF;
        case SLOT_METHOD:
            return t == AST_FUNC_DEF;
        case SLOT_CASE:
            return t == AST_CASE_BRANCH;
        case SLOT_PATTERN:
            return t == AST_PATTERN;
        case SLOT_LITERAL:
            return t == AST_LITERAL;
    }
    return false;
}

/**
 * @brief Kontrola typu a jeho podtypů.
 * @param written Napsaný typ (declared_type, return_type, ...): nesmí obsahovat Any ani Nothing.
 * @param in_chain Typ je prvek seznamu parametrů funkčního typu, smí mít next.
 * @return false = sdílený nebo zacyklený typ (volající přestane procházet řetěz).
 */
static bool chk_type(AstChecker *c, const ASTNode *owner, const TypeInfo *t, const char *field,
                     bool written, bool in_chain);

/** @brief Typ v jednoduchém místě (ne v seznamu parametrů); NULL je chyba jen u povinného. */
static void chk_type_one(AstChecker *c, const ASTNode *owner, const TypeInfo *t, const char *field,
                         bool written, bool required) {
    if (t == NULL) {
        if (required)
            chk_report(c, owner, "%s: chybí typ", field);
        return;
    }
    chk_type(c, owner, t, field, written, false);
}

static bool chk_type(AstChecker *c, const ASTNode *owner, const TypeInfo *t, const char *field,
                     bool written, bool in_chain) {
    if (!chk_visit(c, t, owner, field))
        return false;
    if ((unsigned)t->type_class > (unsigned)TYPE_NOTHING) {
        chk_report(c, owner, "%s: neplatná třída typu %d", field, (int)t->type_class);
        return true;
    }
    if (!in_chain && t->next != NULL)
        chk_report(c, owner, "%s: typ mimo seznam parametrů má next", field);
    if (written && (t->type_class == TYPE_ANY || t->type_class == TYPE_NOTHING))
        chk_report(c, owner, "%s: napsaný typ obsahuje Any nebo Nothing", field);

    switch (t->type_class) {
        case TYPE_UNION:
            chk_type_one(c, owner, t->data.union_type.left, field, written, true);
            chk_type_one(c, owner, t->data.union_type.right, field, written, true);
            break;
        case TYPE_FUNCTION:
            for (const TypeInfo *p = t->data.func_type.param_types; p != NULL; p = p->next) {
                if (!chk_type(c, owner, p, field, written, true))
                    break;
            }
            chk_type_one(c, owner, t->data.func_type.return_type, field, written, true);
            break;
        case TYPE_LIST:
            chk_type_one(c, owner, t->data.list_element_type, field, written, true);
            break;
        case TYPE_CUSTOM:
            if (t->data.custom_name == NULL)
                chk_report(c, owner, "%s: vlastní typ bez jména", field);
            break;
        case TYPE_UNIT:
        case TYPE_INT:
        case TYPE_DOUBLE:
        case TYPE_STRING:
        case TYPE_BOOLEAN:
        case TYPE_ANY:
        case TYPE_NOTHING:
            break;
    }
    return true;
}

static bool chk_node(AstChecker *c, const ASTNode *n, CheckSlot slot, const char *field);

/** @brief Jednoduché místo: nejvýš jeden uzel bez next; NULL je chyba jen u povinného. */
static void chk_one(AstChecker *c, const ASTNode *parent, const ASTNode *n, CheckSlot slot,
                    const char *field, bool required) {
    if (n == NULL) {
        if (required)
            chk_report(c, parent, "%s: chybí povinný uzel", field);
        return;
    }
    if (n->next != NULL)
        chk_report(c, parent, "%s: jednoduché místo, ale uzel má next", field);
    chk_node(c, n, slot, field);
}

/** @brief Seznam přes next; vrátí počet prošlých prvků. */
static int chk_list(AstChecker *c, const ASTNode *head, CheckSlot slot, const char *field) {
    int count = 0;
    for (const ASTNode *n = head; n != NULL; n = n->next) {
        if (!chk_node(c, n, slot, field))
            break;
        count++;
    }
    return count;
}

/** @brief Uzly, které po sémantice expr_type nemají (výjimky z komentáře u expr_type). */
static bool expr_type_exempt(const ASTNode *n) {
    switch (n->type) {
        case AST_CASE_BRANCH:
        case AST_CLASS_DEF:
        case AST_EXTENSION_DEF:
        case AST_PARAM_LIST:
        case AST_PROGRAM:
        case AST_RANGE:
            return true;
        case AST_PATTERN:
            return n->data.pattern.pat_type != PATTERN_TYPE_BINDING;
        default:
            return false;
    }
}

/** @brief Anotace sémantiky: po parseru ve stavu po konstruktoru, po sémantice vyplněné. */
static void chk_annotations(AstChecker *c, const ASTNode *n, CheckSlot slot) {
    bool has_var_id;
    int var_id = node_var_id(n, &has_var_id);

    if (!c->after_sem) {
        if (n->expr_type != NULL)
            chk_report(c, n, "expr_type je vyplněný už po parseru");
        if (n->to_double)
            chk_report(c, n, "to_double je true už po parseru");
        if (has_var_id && var_id != 0)
            chk_report(c, n, "var_id %d už po parseru (má být 0)", var_id);
        if (n->type == AST_FUNC_CALL && n->data.func_call.own_groups != -1)
            chk_report(c, n, "own_groups %d už po parseru (má být -1)",
                       n->data.func_call.own_groups);
        if (n->type == AST_FUNC_CALL && n->data.func_call.call_kind != CALL_UNRESOLVED)
            chk_report(c, n, "call_kind je určený už po parseru");
        if (n->type == AST_MEMBER_ACCESS && n->data.member_access.member_kind != MEMBER_UNRESOLVED)
            chk_report(c, n, "member_kind je určený už po parseru");
    } else {
        if (expr_type_exempt(n)) {
            if (n->expr_type != NULL)
                chk_report(c, n, "expr_type u uzlu, který ho nemá mít");
        } else if (n->expr_type == NULL) {
            chk_report(c, n, "chybí expr_type");
        }
        if (n->expr_type != NULL)
            chk_type(c, n, n->expr_type, "expr_type", false, false);
        if (n->to_double && (n->expr_type == NULL || n->expr_type->type_class != TYPE_INT))
            chk_report(c, n, "to_double u uzlu, který není typu Int");

        if (n->type == AST_VAR_DEF || n->type == AST_IDENTIFIER) {
            if (var_id == 0 || var_id < -1)
                chk_report(c, n, "var_id %d (má být > 0 nebo -1)", var_id);
        } else if (n->type == AST_ASSIGNMENT ||
                   (n->type == AST_PATTERN && n->data.pattern.pat_type == PATTERN_TYPE_BINDING)) {
            if (var_id <= 0)
                chk_report(c, n, "var_id %d (má být > 0)", var_id);
        }
        if (n->type == AST_FUNC_CALL && n->data.func_call.call_kind == CALL_UNRESOLVED)
            chk_report(c, n, "call_kind není určený");
        if (n->type == AST_MEMBER_ACCESS && n->data.member_access.member_kind == MEMBER_UNRESOLVED)
            chk_report(c, n, "member_kind není určený");
    }

    // Platí vždy.
    if (n->type == AST_PATTERN && n->data.pattern.pat_type != PATTERN_TYPE_BINDING && var_id != 0)
        chk_report(c, n, "var_id %d u vzoru, který nic neváže (má být 0)", var_id);
    if (n->type == AST_FUNC_CALL && call_kind_name(n->data.func_call.call_kind) == NULL)
        chk_report(c, n, "neplatný call_kind %d", (int)n->data.func_call.call_kind);
    if (n->type == AST_MEMBER_ACCESS) {
        MemberKind kind = n->data.member_access.member_kind;
        if (member_kind_name(kind) == NULL)
            chk_report(c, n, "neplatný member_kind %d", (int)kind);
        if (kind == MEMBER_EXTENSION && slot != SLOT_CALL_TARGET)
            chk_report(c, n, "MEMBER_EXTENSION mimo target FUNC_CALL");
    }
}

/** @brief VAR_DEF podle místa: příkaz v bloku má jméno a inicializaci, ostatní použití ne. */
static void chk_var_def(AstChecker *c, const ASTNode *n, CheckSlot slot) {
    bool in_block = slot == SLOT_STMT;

    if (n->data.var_def.identifier == NULL && slot != SLOT_PAT_ARG)
        chk_report(c, n, "chybí jméno (NULL smí být jen _ ve vzoru)");
    chk_type_one(c, n, n->data.var_def.declared_type, "declared_type", true, slot == SLOT_RECEIVER);

    if (in_block || slot == SLOT_ROOT) {
        chk_one(c, n, n->data.var_def.initializer, SLOT_EXPR, "initializer", in_block);
        return;
    }
    if (n->data.var_def.initializer != NULL)
        chk_report(c, n, "initializer smí mít jen val/var v bloku");
    if (n->data.var_def.is_mutable)
        chk_report(c, n, "var smí být jen proměnná v bloku");
    if ((slot == SLOT_ITER_VAR || slot == SLOT_PAT_ARG) && n->data.var_def.declared_type != NULL)
        chk_report(c, n, "declared_type má být NULL");
}

/** @brief Vzor podle druhu: co musí mít a co mít nesmí. */
static void chk_pattern(AstChecker *c, const ASTNode *n) {
    PatternType kind = n->data.pattern.pat_type;

    if (kind != PATTERN_TYPE_BINDING && n->data.pattern.bound_identifier != NULL)
        chk_report(c, n, "bound_identifier u vzoru, který nic neváže");
    switch (kind) {
        case PATTERN_LITERAL:
            chk_one(c, n, n->data.pattern.detail.literal_val, SLOT_LITERAL, "literal", true);
            return;
        case PATTERN_TYPE_BINDING:
            if (n->data.pattern.bound_identifier == NULL)
                chk_report(c, n, "vzor n: T bez jména");
            chk_type_one(c, n, n->data.pattern.detail.type_bind, "type_bind", true, true);
            return;
        case PATTERN_CONSTRUCTOR: {
            const TypeInfo *cls = n->data.pattern.detail.constructor.class_type;
            chk_type_one(c, n, cls, "class_type", true, true);
            if (cls != NULL && cls->type_class != TYPE_CUSTOM)
                chk_report(c, n, "class_type není TYPE_CUSTOM");
            chk_list(c, n->data.pattern.detail.constructor.args, SLOT_PAT_ARG, "args");
            return;
        }
        case PATTERN_WILDCARD:
            return;
    }
    chk_report(c, n, "neplatný druh vzoru %d", (int)kind);
}

/** @brief Potomci uzlu podle jeho druhu. */
static void chk_children(AstChecker *c, const ASTNode *n, CheckSlot slot) {
    switch (n->type) {
        case AST_ARG_LIST:
        case AST_LIST_LITERAL:
            chk_list(c, n->data.list.items, SLOT_EXPR, "items");
            break;
        case AST_PARAM_LIST:
            chk_list(c, n->data.list.items, SLOT_PARAM, "params");
            break;
        case AST_ASSIGNMENT:
            if (n->data.assignment.target == NULL)
                chk_report(c, n, "chybí jméno cíle");
            chk_one(c, n, n->data.assignment.value, SLOT_EXPR, "value", true);
            break;
        case AST_BINARY_OP:
            if (!is_binary_op(n->data.bin_op.operator_token))
                chk_report(c, n, "neplatný binární operátor %d", n->data.bin_op.operator_token);
            chk_one(c, n, n->data.bin_op.left, SLOT_EXPR, "left", true);
            chk_one(c, n, n->data.bin_op.right, SLOT_EXPR, "right", true);
            break;
        case AST_BLOCK:
            if (n->data.block.statements == NULL)
                chk_report(c, n, "prázdný blok");
            chk_list(c, n->data.block.statements, SLOT_STMT, "statements");
            break;
        case AST_CASE_BRANCH:
            chk_one(c, n, n->data.case_branch.pattern, SLOT_PATTERN, "pattern", true);
            chk_one(c, n, n->data.case_branch.guard, SLOT_EXPR, "guard", false);
            chk_one(c, n, n->data.case_branch.body, SLOT_BLOCK, "body", true);
            break;
        case AST_CLASS_DEF:
            if (n->data.class_def.name == NULL)
                chk_report(c, n, "chybí jméno třídy");
            chk_list(c, n->data.class_def.fields, SLOT_FIELD, "fields");
            break;
        case AST_EXTENSION_DEF:
            chk_one(c, n, n->data.extension_def.receiver, SLOT_RECEIVER, "receiver", true);
            if (n->data.extension_def.methods == NULL)
                chk_report(c, n, "extension bez metod");
            chk_list(c, n->data.extension_def.methods, SLOT_METHOD, "methods");
            break;
        case AST_FOR:
            chk_one(c, n, n->data.for_loop.iter_var, SLOT_ITER_VAR, "iter_var", true);
            chk_one(c, n, n->data.for_loop.source, SLOT_EXPR, "source", true);
            chk_list(c, n->data.for_loop.guards, SLOT_EXPR, "guards");
            chk_one(c, n, n->data.for_loop.body, SLOT_BLOCK, "body", true);
            break;
        case AST_FUNC_CALL: {
            int groups;
            chk_one(c, n, n->data.func_call.target, SLOT_CALL_TARGET, "target", true);
            groups = chk_list(c, n->data.func_call.arg_groups, SLOT_ARG_LIST, "arg_groups");
            if (c->after_sem) {
                int own = n->data.func_call.own_groups;
                if (own < 0 || own > groups)
                    chk_report(c, n, "own_groups %d mimo rozsah 0..%d", own, groups);
                if (n->data.func_call.call_kind == CALL_VALUE && own != 0)
                    chk_report(c, n, "CALL_VALUE má mít own_groups 0, má %d", own);
            }
            break;
        }
        case AST_FUNC_DEF:
            if (n->data.func_def.name == NULL)
                chk_report(c, n, "chybí jméno funkce");
            chk_list(c, n->data.func_def.param_groups, SLOT_PARAM_LIST, "param_groups");
            chk_type_one(c, n, n->data.func_def.return_type, "return_type", true, false);
            chk_one(c, n, n->data.func_def.body, SLOT_BLOCK, "body", true);
            break;
        case AST_IDENTIFIER:
            if (n->data.identifier.name == NULL)
                chk_report(c, n, "chybí jméno");
            break;
        case AST_IF_EXPR:
            chk_one(c, n, n->data.if_expr.condition, SLOT_EXPR, "condition", true);
            chk_one(c, n, n->data.if_expr.then_block, SLOT_BLOCK, "then_block", true);
            chk_one(c, n, n->data.if_expr.else_block, SLOT_BLOCK, "else_block", false);
            break;
        case AST_LAMBDA:
            chk_one(c, n, n->data.lambda.param_list, SLOT_PARAM_LIST, "param_list", true);
            chk_one(c, n, n->data.lambda.body, SLOT_BLOCK, "body", true);
            break;
        case AST_LITERAL:
            if (n->data.literal.kind == LIT_STRING && n->data.literal.value.s_val == NULL)
                chk_report(c, n, "řetězcový literál bez hodnoty");
            else if ((unsigned)n->data.literal.kind > (unsigned)LIT_UNIT)
                chk_report(c, n, "neplatný druh literálu %d", (int)n->data.literal.kind);
            break;
        case AST_MATCH_EXPR:
            chk_one(c, n, n->data.match_expr.matched_expr, SLOT_EXPR, "matched_expr", true);
            if (n->data.match_expr.cases == NULL)
                chk_report(c, n, "match bez větví");
            chk_list(c, n->data.match_expr.cases, SLOT_CASE, "cases");
            break;
        case AST_MEMBER_ACCESS:
            chk_one(c, n, n->data.member_access.object, SLOT_EXPR, "object", true);
            if (n->data.member_access.member_name == NULL)
                chk_report(c, n, "chybí jméno členu");
            break;
        case AST_PATTERN:
            chk_pattern(c, n);
            break;
        case AST_PROGRAM:
            chk_list(c, n->data.program.definitions, SLOT_DEF, "definitions");
            break;
        case AST_RANGE:
            chk_one(c, n, n->data.range.start, SLOT_EXPR, "start", true);
            chk_one(c, n, n->data.range.end, SLOT_EXPR, "end", true);
            chk_one(c, n, n->data.range.step, SLOT_EXPR, "step", false);
            break;
        case AST_RETURN:
            chk_one(c, n, n->data.return_stmt.value, SLOT_EXPR, "value", false);
            break;
        case AST_TYPE_ALIAS:
            if (n->data.type_alias.name == NULL)
                chk_report(c, n, "chybí jméno aliasu");
            chk_type_one(c, n, n->data.type_alias.aliased, "aliased", true, true);
            break;
        case AST_UNARY_OP:
            if (!is_unary_op(n->data.unary_op.operator_token))
                chk_report(c, n, "neplatný unární operátor %d", n->data.unary_op.operator_token);
            chk_one(c, n, n->data.unary_op.operand, SLOT_EXPR, "operand", true);
            break;
        case AST_VAR_DEF:
            chk_var_def(c, n, slot);
            break;
        case AST_WHILE:
            chk_one(c, n, n->data.while_loop.condition, SLOT_EXPR, "condition", true);
            chk_one(c, n, n->data.while_loop.body, SLOT_BLOCK, "body", true);
            break;
    }
}

/**
 * @brief Kontrola jednoho uzlu na daném místě a celého jeho podstromu.
 * @return false = uzel už byl navštíven (sdílení nebo cyklus); volající přestane procházet řetěz.
 */
static bool chk_node(AstChecker *c, const ASTNode *n, CheckSlot slot, const char *field) {
    if (!chk_visit(c, n, n, field))
        return false;
    if ((unsigned)n->type > (unsigned)AST_WHILE) {
        chk_report(c, n, "%s: neplatný druh uzlu %d", field, (int)n->type);
        return true;
    }
    if (!slot_allows(slot, n->type))
        chk_report(c, n, "%s: uzel tohoto druhu sem nepatří", field);
    chk_annotations(c, n, slot);
    chk_children(c, n, slot);
    return true;
}

int ast_check(const ASTNode *root, bool after_semantics) {
    AstChecker c = {after_semantics, 0, {NULL, 0, 0, false}};

    for (const ASTNode *n = root; n != NULL; n = n->next) {
        if (n->type == AST_PROGRAM && (n != root || n->next != NULL))
            chk_report(&c, n, "PROGRAM smí být jen jako jediný kořen");
        if (!chk_node(&c, n, SLOT_ROOT, "root"))
            break;
    }
    if (c.seen.oom) {
        fputs("ast_check: nedostatek paměti, kontrola není úplná\n", stderr);
        c.errors++;
    }
    free(c.seen.slots);
    return c.errors;
}