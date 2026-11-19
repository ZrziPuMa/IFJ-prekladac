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

ErrorCode symtable_init(tBtNode **root) {
    if (!root) {
        return ERR_INTERNAL;
    }
    *root = NULL;
    return ERR_OK;
}

ErrorCode symtable_insert(tBtNode **root, char *key, tBtNodeData data) {
    if (!root || !key) {
        return ERR_INTERNAL;
    }

    if (!(*root)) {
        *root = (tBtNode *)malloc(sizeof(tBtNode));
        if (*root == NULL) {
            return ERR_INTERNAL;
        }

        (*root)->data = (tBtNodeData *)malloc(sizeof(tBtNodeData));
        if ((*root)->data == NULL) {
            free(*root);
            *root = NULL;
            return ERR_INTERNAL;
        }

        (*root)->key = strdup(key);
        if ((*root)->key == NULL) {
            free((*root)->data);
            free(*root);
            *root = NULL;
            return ERR_INTERNAL;
        }
        (*root)->data->value = data.value;
        (*root)->data->type = data.type;
        (*root)->left_child = NULL;
        (*root)->right_child = NULL;

    } else {
        int cmp = strcmp(key, (*root)->key);
        if (cmp < 0) {
            ErrorCode err = symtable_insert(&(*root)->left_child, key, data);
            if (err != ERR_OK)
                return err;
        } else if (cmp > 0) {
            ErrorCode err = symtable_insert(&(*root)->right_child, key, data);
            if (err != ERR_OK)
                return err;
        } else {
            (*root)->data->value = data.value;
            (*root)->data->type = data.type;
            return ERR_OK;
        }
    }
    return realign_tree(root);
}

ErrorCode symtable_search(tBtNode **root, char *key, tBtNode **location) {
    if (location == NULL)
        return ERR_INTERNAL;
    if (root == NULL || key == NULL) {
        *location = NULL;
        return ERR_INTERNAL;
    }
    if (*root == NULL) {
        *location = NULL;
        return ERR_OK;
    }

    int cmp = strcmp(key, (*root)->key);
    if (cmp == 0) {
        *location = *root;
    } else if (cmp < 0) {
        return symtable_search(&(*root)->left_child, key, location);
    } else {
        return symtable_search(&(*root)->right_child, key, location);
    }
    return ERR_OK;
}

ErrorCode symtable_delete(tBtNode **root, char *key) {
    if (!root || !key) {
        return ERR_INTERNAL;
    }
    if (!(*root))
        return ERR_OK;

    int cmp = strcmp(key, (*root)->key);
    if (cmp < 0) {
        ErrorCode err = symtable_delete(&(*root)->left_child, key);
        if (err != ERR_OK)
            return err;
    } else if (cmp > 0) {
        ErrorCode err = symtable_delete(&(*root)->right_child, key);
        if (err != ERR_OK)
            return err;
    } else {
        if ((*root)->left_child == NULL && (*root)->right_child == NULL) {
            ErrorCode err = symtable_node_free(root);
            if (err != ERR_OK)
                return err;
        } else if ((*root)->left_child == NULL) {
            tBtNode *temp = *root;
            *root = (*root)->right_child;
            ErrorCode err = symtable_node_free(&temp);
            if (err != ERR_OK)
                return err;
        } else if ((*root)->right_child == NULL) {
            tBtNode *temp = *root;
            *root = (*root)->left_child;
            ErrorCode err = symtable_node_free(&temp);
            if (err != ERR_OK)
                return err;
        } else {
            ErrorCode err = replace_most_right(*root, &(*root)->left_child);
            if (err != ERR_OK)
                return err;
        }
    }
    return realign_tree(root);
}

ErrorCode symtable_node_free(tBtNode **root) {
    if (!root) {
        return ERR_INTERNAL;
    }
    if (!(*root))
        return ERR_OK;

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
    return ERR_OK;
}

ErrorCode symtable_dispose(tBtNode **root) {
    if (!root) {
        return ERR_INTERNAL;
    }

    if (*root == NULL)
        return ERR_OK;
    if ((*root)->left_child) {
        ErrorCode err = symtable_dispose(&(*root)->left_child);
        if (err != ERR_OK)
            return err;
    }
    if ((*root)->right_child) {
        ErrorCode err = symtable_dispose(&(*root)->right_child);
        if (err != ERR_OK)
            return err;
    }

    return symtable_node_free(root);
}

ErrorCode rotate_left(tBtNode **root) {
    if (!root || !(*root) || !(*root)->right_child) {
        return ERR_INTERNAL;
    }

    tBtNode *new_root = (*root)->right_child;
    (*root)->right_child = new_root->left_child;
    new_root->left_child = *root;
    *root = new_root;
    return ERR_OK;
}

ErrorCode rotate_right(tBtNode **root) {
    if (!root || !(*root) || !(*root)->left_child) {
        return ERR_INTERNAL;
    }

    tBtNode *new_root = (*root)->left_child;
    (*root)->left_child = new_root->right_child;
    new_root->right_child = *root;
    *root = new_root;
    return ERR_OK;
}

ErrorCode rotate_left_right(tBtNode **root) {
    if (!root) {
        return ERR_INTERNAL;
    }
    if (!(*root))
        return ERR_OK;

    ErrorCode err = rotate_left(&((*root)->left_child));
    if (err != ERR_OK)
        return err;
    return rotate_right(root);
}

ErrorCode rotate_right_left(tBtNode **root) {
    if (!root) {
        return ERR_INTERNAL;
    }
    if (!(*root))
        return ERR_OK;

    ErrorCode err = rotate_right(&((*root)->right_child));
    if (err != ERR_OK)
        return err;
    return rotate_left(root);
}

ErrorCode replace_most_right(tBtNode *node, tBtNode **root) {
    if (!node || !root || !(*root)) {
        return ERR_INTERNAL;
    }

    if ((*root)->right_child != NULL) {
        ErrorCode err = replace_most_right(node, &(*root)->right_child);
        if (err != ERR_OK)
            return err;
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
        ErrorCode err = symtable_node_free(&temp);
        if (err != ERR_OK)
            return err;
    }

    if (*root != NULL)
        return realign_tree(root);
    return ERR_OK;
}

ErrorCode replace_most_left(tBtNode *node, tBtNode **root) {
    if (!node || !root || !(*root)) {
        return ERR_INTERNAL;
    }

    if ((*root)->left_child != NULL) {
        ErrorCode err = replace_most_left(node, &(*root)->left_child);
        if (err != ERR_OK)
            return err;
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
        ErrorCode err = symtable_node_free(&temp);
        if (err != ERR_OK)
            return err;
    }

    if (*root != NULL)
        return realign_tree(root);
    return ERR_OK;
}

int check_height(tBtNode **root, tBtNode ***first_unbalanced) {
    if (!root || !first_unbalanced) {
        return -2;
    }

    int left_height;
    int right_height;

    if ((*root) != NULL) {
        left_height = check_height(&(*root)->left_child, first_unbalanced);
        if (left_height == -2)
            return -2;
        if (left_height == -1)
            return -1;
        right_height = check_height(&(*root)->right_child, first_unbalanced);
        if (right_height == -2)
            return -2;
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

ErrorCode realign_tree(tBtNode **root) {
    if (!root)
        return ERR_INTERNAL;
    if (!(*root))
        return ERR_OK;

    tBtNode **first_unbalanced = NULL;
    int height = check_height(root, &first_unbalanced);
    if (height == -2)
        return ERR_INTERNAL;
    if (height == -1 && first_unbalanced != NULL) {
        int left_height = check_height(&(*first_unbalanced)->left_child, &first_unbalanced);
        int right_height = check_height(&(*first_unbalanced)->right_child, &first_unbalanced);
        if (left_height == -2 || right_height == -2)
            return ERR_INTERNAL;

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
            return ERR_OK;
        }
        if (left_height == -2 || right_height == -2)
            return ERR_INTERNAL;

        if (left_height > right_height) {
            cmp2 = 'l';
        } else {
            cmp2 = 'r';
        }

        switch (cmp1) {
            case 'l':
                if (cmp2 == 'l')
                    return rotate_right(first_unbalanced);
                else if (cmp2 == 'r')
                    return rotate_left_right(first_unbalanced);
                break;
            case 'r':
                if (cmp2 == 'l')
                    return rotate_right_left(first_unbalanced);
                else if (cmp2 == 'r')
                    return rotate_left(first_unbalanced);
                break;
        }
    }
    return ERR_OK;
}
