/**
 * Implementace tabulky symbolů jazyka IFJ26
 * @file isymtable.h
 * @brief Hlavičkový soubor tabulky symbolů pro interface
 * @author Daniela Sladká, xsladkd00
 */

 
#ifndef ISYMTABLE_H
#define ISYMTABLE_H

#include <string.h>
#include <stdbool.h>


/**
 * @brief Datový typ pro uzel binárního stromu
 */
typedef struct bt_node tBtNode;

/**
 * @brief Výčet typů obsahu uzlů (funkce, nebo proměnná)
 */
typedef enum{
    FUNCTION,
    VARIABLE
}tBtContentType;

/**
 * @brief Struktura pro uložení dat symbolu
 */
typedef struct node_data{
    void *value;
    tBtContentType type;
}tBtNodeData;

/**
 * @brief Struktura uzlu binárního stromu
 */
struct bt_node{
    tBtNodeData *data;
    char *key;
    tBtNode *left_child;
    tBtNode *right_child;
};

/** 
 * @brief Datový typ pro zásobník
 */
typedef struct stack {
    tBtNode *top;
} tStack;

/**
 * @brief Inicializace prázdné tabulky symbolů
 * @param root Ukazatel, který bude ukazovat na kořen inicializovaného (pod)stromu
 */
void symtable_init(tBtNode **root);

/**
 * @brief Vložení nového prvku do stromu
 * @param root Ukazatel na kořen (pod)stromu, do kterého se prvek vloží
 * @param key Klíč, pod kterým se prvek uloží
 * @param data Data uloženého prvku
 */
void symtable_insert(tBtNode **root, char *key, tBtNodeData data);

/**
 * @brief Vyhledávání v tabulce symbolů
 * @param root Ukazatel na kořen (pod)stromu, ve kterém se bude vyhledávat
 * @param key Klíč vyhledávaného prvku
 * @param location Ukazatel, ve kterém se vrátí umístění nalezeného prvku
 */
void symtable_search(tBtNode **root, char *key, tBtNode **location);

/**
 * @brief Odstranění prvku z tabulky symbolů
 * @param root Ukazatel na kořen (pod)stromu, ze kterého se prvek odstraní
 * @param key Klíč odstraňovaného prvku
 * @param stack Ukazatel na zásobník, který obsahuje cestu k odstraněnému uzlu
 */
void symtable_delete(tBtNode **root, char *key, tStack *stack);

/**
 * @brief Uvolnění alokovaných zdrojů v uzlu
 * @param root Ukazatel na kořen (pod)stromu, který se bude uvolňovat
 */
void symtable_node_free(tBtNode **root);

/**
 * @brief Systematické uvolnění alokovaných zdrojů v tabulce symbolů
 * @param root Ukazatel na kořen (pod)stromu, který se bude uvolňovat
 */
void symtable_dispose(tBtNode **root);


#endif //ISYMTABLE_H