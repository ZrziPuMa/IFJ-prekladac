/**
 * Implemetnace tabulky symbolů jazyka IFJ26
 * @file symtable.c
 * @brief Implementace tabulky symbolů
 * @author Daniela Sladká, xsladkd00
 *
 * Použití AI: Dovysvětlení logiky rotací a podchycení
 * ztráty paměti pomocí Gemini.
 */

#include "symtable.h"

#include "ierror.h"
#include "isymtable.h"

#include <stdlib.h>

void symtable_init(tBtNode **root) {
    if (!root) {
        print_error(ERR_INTERNAL, -1, "Neplatný ukazatel.");
        return;
    }
    *root = NULL;
}

void symtable_insert(tBtNode **root, char *key, tBtNodeData data) {
    if (!root) {
        print_error(ERR_INTERNAL, -1, "Chyba: Neplatný ukazatel na kořen stromu.");
        return;
    }

    if (!(*root)) {
        *root = (tBtNode *)malloc(sizeof(tBtNode));
        if (*root == NULL) {
            print_error(ERR_INTERNAL, -1, "Chyba alokace paměti pro nový uzel.");
            return;
        }

        (*root)->data = (tBtNodeData *)malloc(sizeof(tBtNodeData));
        if ((*root)->data == NULL) {
            free(*root);
            *root = NULL;
            print_error(ERR_INTERNAL, -1, "Chyba alokace paměti pro data uzlu.");
            return;
        }

        (*root)->key = strdup(key);
        if ((*root)->key == NULL) {
            free((*root)->data);
            free(*root);
            *root = NULL;
            print_error(ERR_INTERNAL, -1, "Chyba alokace paměti pro klíč uzlu.");
            return;
        }
        (*root)->data->value = data.value;
        (*root)->data->type = data.type;
        (*root)->left_child = NULL;
        (*root)->right_child = NULL;

    } else {
        int cmp = strcmp(key, (*root)->key);
        if (cmp < 0) {
            symtable_insert(&(*root)->left_child, key, data);
        } else if (cmp > 0) {
            symtable_insert(&(*root)->right_child, key, data);
        } else {
            (*root)->data->value = data.value;
            (*root)->data->type = data.type;
            return;
        }
    }
    realign_tree(root);
}

void symtable_search(tBtNode **root, char *key, tBtNode **location) {
    if (root == NULL || *root == NULL) {
        *location = NULL;
        return;
    }

    int cmp = strcmp(key, (*root)->key);
    if (cmp == 0) {
        *location = *root;
    } else if (cmp < 0) {
        symtable_search(&(*root)->left_child, key, location);
    } else {
        symtable_search(&(*root)->right_child, key, location);
    }
}

void symtable_delete(tBtNode **root, char *key) {
    if (!root) {
        print_error(ERR_INTERNAL, -1, "Chyba: Neplatný ukazatel na kořen stromu.");
        return;
    }
    if (!(*root))
        return;

    int cmp = strcmp(key, (*root)->key);
    if (cmp < 0) {
        symtable_delete(&(*root)->left_child, key);
    } else if (cmp > 0) {
        symtable_delete(&(*root)->right_child, key);
    } else {
        if ((*root)->left_child == NULL && (*root)->right_child == NULL) {
            symtable_node_free(root);
        } else if ((*root)->left_child == NULL) {
            tBtNode *temp = *root;
            *root = (*root)->right_child;
            symtable_node_free(&temp);
        } else if ((*root)->right_child == NULL) {
            tBtNode *temp = *root;
            *root = (*root)->left_child;
            symtable_node_free(&temp);
        } else {
            replace_most_right(*root, &(*root)->left_child);
        }
    }
    realign_tree(root);
}

void symtable_node_free(tBtNode **root) {
    if (!root) {
        print_error(ERR_INTERNAL, -1, "Chyba: Neplatný ukazatel na kořen stromu.");
        return;
    }
    if (!(*root))
        return;

    if ((*root)->data) {
        free((*root)->data);
        (*root)->data = NULL;
    }
    if ((*root)->key) {
        free((*root)->key);
        (*root)->key = NULL;
    }
    free(*root);
    *root = NULL;
}

void symtable_dispose(tBtNode **root) {
    if (!root) {
        print_error(ERR_INTERNAL, -1, "Chyba: Neplatný ukazatel na kořen stromu.");
        return;
    }

    if (*root == NULL)
        return;
    if ((*root)->left_child)
        symtable_dispose(&(*root)->left_child);
    if ((*root)->right_child)
        symtable_dispose(&(*root)->right_child);

    symtable_node_free(root);
}

void rotate_left(tBtNode **root) {
    if (!root || !(*root) || !(*root)->right_child) {
        print_error(ERR_INTERNAL, -1,
                    "Chyba: Neplatný ukazatel na kořen stromu nebo pravého potomka.");
        return;
    }

    tBtNode *new_root = (*root)->right_child;
    (*root)->right_child = new_root->left_child;
    new_root->left_child = *root;
    *root = new_root;
}

void rotate_right(tBtNode **root) {
    if (!root || !(*root) || !(*root)->left_child) {
        print_error(ERR_INTERNAL, -1,
                    "Chyba: Neplatný ukazatel na kořen stromu nebo levého potomka.");
        return;
    }

    tBtNode *new_root = (*root)->left_child;
    (*root)->left_child = new_root->right_child;
    new_root->right_child = *root;
    *root = new_root;
}

void rotate_left_right(tBtNode **root) {
    if (!root) {
        print_error(ERR_INTERNAL, -1, "Chyba: Neplatný ukazatel na kořen stromu.");
        return;
    }
    if (!(*root))
        return;

    rotate_left(&((*root)->left_child));
    rotate_right(root);
}

void rotate_right_left(tBtNode **root) {
    if (!root) {
        print_error(ERR_INTERNAL, -1, "Chyba: Neplatný ukazatel na kořen stromu.");
        return;
    }
    if (!(*root))
        return;

    rotate_right(&((*root)->right_child));
    rotate_left(root);
}

void replace_most_right(tBtNode *node, tBtNode **root) {
    if (!node || !root || !(*root)) {
        print_error(ERR_INTERNAL, -1, "Chyba: Neplatný ukazatel na uzel nebo kořen stromu.");
        return;
    }

    if ((*root)->right_child != NULL) {
        replace_most_right(node, &(*root)->right_child);
    } else {
        if (node->key)
            free(node->key);

        node->key = (*root)->key;
        node->data->value = (*root)->data->value;
        node->data->type = (*root)->data->type;

        tBtNode *temp = *root;
        *root = (*root)->left_child;

        temp->key = NULL;
        if (temp->data)
            temp->data->value = NULL;
        symtable_node_free(&temp);
    }

    if (*root != NULL)
        realign_tree(root);
}

void replace_most_left(tBtNode *node, tBtNode **root) {
    if (!node || !root || !(*root)) {
        print_error(ERR_INTERNAL, -1, "Chyba: Neplatný ukazatel na uzel nebo kořen stromu.");
        return;
    }

    if ((*root)->left_child != NULL) {
        replace_most_left(node, &(*root)->left_child);
    } else {
        if (node->key)
            free(node->key);

        node->key = (*root)->key;
        node->data->value = (*root)->data->value;
        node->data->type = (*root)->data->type;

        tBtNode *temp = *root;
        *root = (*root)->right_child;

        temp->key = NULL;
        if (temp->data)
            temp->data->value = NULL;
        symtable_node_free(&temp);
    }

    if (*root != NULL)
        realign_tree(root);
}

int check_height(tBtNode **root, tBtNode ***first_unbalanced) {
    if (!root) {
        print_error(ERR_INTERNAL, -1, "Chyba: Neplatný ukazatel na kořen stromu.");
        return 0;
    }

    int left_height;
    int right_height;

    if ((*root) != NULL) {
        left_height = check_height(&(*root)->left_child, first_unbalanced);
        if (left_height == -1)
            return -1;
        right_height = check_height(&(*root)->right_child, first_unbalanced);
        if (right_height == -1)
            return -1;
        if (left_height > right_height) {
            if (left_height - right_height > 1) {
                *first_unbalanced = root;
                return -1;
            }
            return left_height + 1;
        } else {
            if (right_height - left_height > 1) {
                *first_unbalanced = root;
                return -1;
            }
            return right_height + 1;
        }
    }
    return 0;
}

void realign_tree(tBtNode **root) {
    if (!root || !(*root))
        return;

    tBtNode **first_unbalanced = NULL;
    int height = check_height(root, &first_unbalanced);
    if (height == -1 && first_unbalanced != NULL) {
        int left_height = check_height(&(*first_unbalanced)->left_child, &first_unbalanced);
        int right_height = check_height(&(*first_unbalanced)->right_child, &first_unbalanced);

        int cmp1, cmp2;

        if (left_height > right_height) {
            cmp1 = 'l';
            left_height =
                check_height(&(*first_unbalanced)->left_child->left_child, &first_unbalanced);
            right_height =
                check_height(&(*first_unbalanced)->left_child->right_child, &first_unbalanced);
        } else if (right_height > left_height) {
            cmp1 = 'r';
            left_height =
                check_height(&(*first_unbalanced)->right_child->left_child, &first_unbalanced);
            right_height =
                check_height(&(*first_unbalanced)->right_child->right_child, &first_unbalanced);
        } else {
            return;
        }

        if (left_height > right_height) {
            cmp2 = 'l';
        } else {
            cmp2 = 'r';
        }

        switch (cmp1) {
            case 'l':
                if (cmp2 == 'l')
                    rotate_right(first_unbalanced);
                else if (cmp2 == 'r')
                    rotate_left_right(first_unbalanced);
                break;
            case 'r':
                if (cmp2 == 'l')
                    rotate_right_left(first_unbalanced);
                else if (cmp2 == 'r')
                    rotate_left(first_unbalanced);
                break;
        }
    }
}
