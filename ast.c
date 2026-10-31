/**
 * @file ast.c
 * @brief Implementace AST.
 * @author František Bilavčík (<xbilavf00>)
 *
 * (AI) Při programování byl použit Github Copilot,
 * který jsem používal jako inline suggestion.
 */

#include "ast.h"

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
