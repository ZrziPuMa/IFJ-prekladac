/**
 * Implemetnace tabulky symbolů jazyka IFJ26
 * @file symtable.c
 * @brief Implementace tabulky symbolů
 * @author Daniela Sladká, xsladkd00
 */

#include "symtable.h"
#include "isymtable.h"
//#include "ierror.h"


void symtable_init(tBtNode **root) {
    if (*root != NULL) {
        //print_error(ERR_INTERNAL, -1, "Tabulka symbolů již byla inicializována.");
        return;
    }
    *root = NULL;
 }

void symtable_insert(tBtNode **root, char *key, tBtNodeData data) {
    if (*root == NULL) {
        *root = (tBtNode *)malloc(sizeof(tBtNode));
        if (*root == NULL) {
            //print_error(ERR_INTERNAL, -1, "Chyba alokace paměti pro nový uzel tabulky symbolů.");
            return;
        }

        (*root)->data = (tBtNodeData *)malloc(sizeof(tBtNodeData));
        if ((*root)->data == NULL) {
            //print_error(ERR_INTERNAL, -1, "Chyba alokace paměti pro data uzlu tabulky symbolů.");
            return;
        }

        (*root)->data->value = strdup(key);
        (*root)->data->type = data.type;
        (*root)->left_child = NULL;
        (*root)->right_child = NULL;

    } else {
        int cmp = strcmp(key, (char *)(*root)->data->value);
        if (cmp < 0) {
            symtable_insert(&(*root)->left_child, key, data);
        } else if (cmp > 0) {
            symtable_insert(&(*root)->right_child, key, data);
        }
    }
 }

 void symtable_search(tBtNode **root, char *key, tBtNode **location) {
    if (*root == NULL) {
        *location = NULL;
        return;
    }

    int cmp = strcmp(key, (char *)(*root)->data->value);
    if (cmp == 0) {
        *location = *root;
        return;
    }

    else if (cmp < 0) {
        symtable_search(&(*root)->left_child, key, location);
    }

    else {
        symtable_search(&(*root)->right_child, key, location);
    }
 }

 void symtable_delete(tBtNode **root, char *key, tStack *stack) {
    if (*root == NULL) {
        return;
    }

    int cmp = strcmp(key, (char *)(*root)->data->value);
    if (cmp < 0) {
        symtable_delete(&(*root)->left_child, key, stack);
    } else if (cmp > 0) {
        symtable_delete(&(*root)->right_child, key, stack);
    } else {
        tBtNode *temp = *root;

        if ((*root)->left_child == NULL) {
            *root = (*root)->right_child;
        } else if ((*root)->right_child == NULL) {
            *root = (*root)->left_child;
        } else {
            tBtNode *successor = (*root)->right_child;
            while (successor->left_child != NULL) {
                successor = successor->left_child;
            }

            free((*root)->data->value);
            (*root)->data->value = strdup((char *)successor->data->value);
            (*root)->data->type = successor->data->type;

            symtable_delete(&(*root)->right_child, (char *)successor->data->value, stack);
        }

        free(temp->data);
        free(temp);
    }
 }

 void symtable_node_free(tBtNode **root) {
    if (!root) {
        //print_error(ERR_INTERNAL, -1, "Chyba: Neplatný ukazatel na kořen stromu.");
        return;
    }

    if ((* root)->data->type == VARIABLE) {
        free((*root)->data->value);
    }
    return;
 }  

 void symtable_dispose(tBtNode **root) {
    if (!root) {
        //print_error(ERR_INTERNAL, -1, "Chyba: Neplatný ukazatel na kořen stromu.");
        return;
    }

    if (*root == NULL) return;
    if ((*root)->left_child) symtable_dispose(&(*root)->left_child);
    if ((*root)->right_child) symtable_dispose(&(*root)->right_child);
    
    free((*root)->key);
    symtable_node_free(root);
    free(*root);
    *root = NULL;
}

void rotate_left(tBtNode **root) {
    if (!root){
        //print_error(ERR_INTERNAL, -1, "Chyba: Neplatný ukazatel na kořen stromu.");
        return;
    }
    if (!(*root)) return;

    tBtNode *new_root = (*root)->right_child;
    new_root->left_child = *root;
    (*root)->right_child = new_root->left_child;
    *root = new_root;
}

void rotate_right(tBtNode **root) {
    if (!root) {
        //print_error(ERR_INTERNAL, -1, "Chyba: Neplatný ukazatel na kořen stromu.");
        return;
    }
    if (!(*root)) return;

    tBtNode *new_root = (*root)->left_child;
    (*root)->left_child = new_root->right_child;
    new_root->right_child = *root;
    *root = new_root;
}

void rotate_left_right(tBtNode **root) {
    if (!root) {
        //print_error(ERR_INTERNAL, -1, "Chyba: Neplatný ukazatel na kořen stromu.");
        return;
    }
    if (!(*root)) return;

    tBtNode *new_root_A = (*root)->left_child;
    tBtNode *new_root_B = (*root);

    rotate_left(new_root_A);
    rotate_right(new_root_B);
}

void rotate_right_left(tBtNode **root) {
    if (!root) {
        //print_error(ERR_INTERNAL, -1, "Chyba: Neplatný ukazatel na kořen stromu.");
        return;
    }
    if (!(*root)) return;

    tBtNode *new_root_A = (*root)->right_child;
    tBtNode *new_root_B = (*root);

    rotate_right(new_root_A);
    rotate_left(new_root_B);
}

