/**
 * Implemetnace tabulky symbolů jazyka IFJ26
 * @file isymtable.h
 * @brief Hlavičkový soubor tabulky symbolů
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
    tBtContentType type;
    tBtNode *left_child;
    tBtNode *right_child;
};

/**
 * @brief Inicializace prázdné tabulky symbolů
 * @param root Ukazatel, který bude ukazovat na kořen inicializovaného (pod)stromu
 */
void symtable_init(bt_node **root);

/**
 * @brief Vložení nového lokálního rozsahu platnosti (scope) na vrchol zásobníku
 * @param root Ukazatel na kořen stromu reprezentujícího nový lokální rozsah
 */
void symtable_push_scope(bt_node **root);

/**
 * @brief Odebrání aktuálního lokálního rozsahu platnosti z vrcholu zásobníku
 * @return Tabulka symbolů (uzel) opuštěného rozsahu platnosti
 */
tBtNode symtable_pop_scope();

/**
 * @brief Vložení nového prvku do stromu
 * @param root Ukazatel na kořen (pod)stromu, do kterého se prvek vloží
 * @param key Klíč, pod kterým se prvek uloží
 * @param content Data uloženého prvku
 */
void symtable_insert(bt_node **root, char *key, tBtContentType content);

/**
 * @brief Vyhledávání v tabulce symbolů
 * @param root Ukazatel na kořen (pod)stromu, ve kterém se bude vyhledávat
 * @param key Klíč vyhledávaného prvku
 * @param content Ukazatel, ve kterém se vrátí umístění nalezeného prvku
 */
void symtable_search(bt_node **root, char *key, tBtContentType **content);

/**
 * @brief Uvolnění alokovaných zdrojů
 * @param root Ukazatel na kořen (pod)stromu, který se bude uvolňovat
 */
void symtable_free(bt_node **root);

#endif //ISYMTABLE_H