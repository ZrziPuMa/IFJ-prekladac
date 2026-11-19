/**
 * Implementace tabulky symbolů jazyka IFJ26
 * @file isymtable.h
 * @brief Hlavičkový soubor tabulky symbolů pro interface
 * @author Daniela Sladká, xsladkd00
 */

#ifndef ISYMTABLE_H
#define ISYMTABLE_H

#include "ierror.h"

#include <stdbool.h>
#include <string.h>

/**
 * @brief Datový typ pro uzel binárního stromu
 */
typedef struct bt_node tBtNode;

/**
 * @brief Výčet typů obsahu uzlů (funkce, proměnná, třída nebo alias)
 */
typedef enum { FUNCTION, VARIABLE, CLASS, ALIAS } tBtContentType;

/**
 * @brief Struktura pro uložení dat symbolu
 */
typedef struct node_data {
    void *value;
    tBtContentType type;
} tBtNodeData;

/**
 * @brief Struktura uzlu binárního stromu
 */
struct bt_node {
    tBtNodeData *data;
    char *key;
    tBtNode *left_child;
    tBtNode *right_child;
};

/**
 * @brief Inicializace prázdné tabulky symbolů
 * @param root Ukazatel, který bude ukazovat na kořen inicializovaného (pod)stromu
 * @return ErrorCode chybový kód
 */
ErrorCode symtable_init(tBtNode **root);

/**
 * @brief Vložení nového prvku do stromu
 * @param root Ukazatel na kořen (pod)stromu, do kterého se prvek vloží
 * @param key Klíč, pod kterým se prvek uloží
 * @param data Data uloženého prvku
 * @return ErrorCode chybový kód
 */
ErrorCode symtable_insert(tBtNode **root, char *key, tBtNodeData data);

/**
 * @brief Vyhledávání v tabulce symbolů
 * @param root Ukazatel na kořen (pod)stromu, ve kterém se bude vyhledávat
 * @param key Klíč vyhledávaného prvku
 * @param location Ukazatel, ve kterém se vrátí umístění nalezeného prvku
 * @return ErrorCode chybový kód
 */
ErrorCode symtable_search(tBtNode **root, char *key, tBtNode **location);

/**
 * @brief Odstranění prvku z tabulky symbolů
 * @param root Ukazatel na kořen (pod)stromu, ze kterého se prvek odstraní
 * @param key Klíč odstraňovaného prvku
 * @return ErrorCode chybový kód
 */
ErrorCode symtable_delete(tBtNode **root, char *key);

/**
 * @brief Uvolnění alokovaných zdrojů v uzlu
 * @param root Ukazatel na kořen (pod)stromu, který se bude uvolňovat
 * @return ErrorCode chybový kód
 */
ErrorCode symtable_node_free(tBtNode **root);

/**
 * @brief Systematické uvolnění alokovaných zdrojů v tabulce symbolů
 * @param root Ukazatel na kořen (pod)stromu, který se bude uvolňovat
 * @return ErrorCode chybový kód
 */
ErrorCode symtable_dispose(tBtNode **root);

#endif // ISYMTABLE_H
