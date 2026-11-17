/**
 * @file semantics.h
 * @brief Interní rozhraní sémantické analýzy překladače IFJ26
 * @author Natalie Morávková (<xmoravn00>)
 *
 * Při návrhu byla použita umělá inteligence (ChatGPT) pro vysvětlení
 * průběhu sémantické analýzy a návrhu potřebných funkcí
 */

#ifndef SEMANTICS_H
#define SEMANTICS_H

#include "ast.h"
#include "istack.h"
#include "isymtable.h"

#include <stdbool.h>

/**
 * @brief Stav zpracování definice funkce během sémantické analýzy
 *
 * Stav je uložen u symbolu příslušné funkce v tabulce symbolů
 */
typedef enum {
    FUNC_UNRESOLVED, // Funkce je registrována, její tělo ještě není analyzováno
    FUNC_RESOLVING,  // Právě probíhá analýza funkce, její výsledek nemusí být znám
    FUNC_RESOLVED,   // Výsledný typ je známý, kontrola těla ještě není dokončena
    FUNC_CHECKED     // Tělo bylo kompletně zkontrolováno a AST obohaceno
} SemanticFunctionState;

typedef struct SemanticFunctionSymbol {
    ASTNode *function;  // AST_FUNC_DEF
    ASTNode *extension; // AST_EXTENSION_DEF, NULL pro běžnou funkci

    SemanticFunctionState state;
    struct SemanticFunctionSymbol *next;
    bool is_buildin;
    TypeInfo *builtin_signatures[2];

} SemanticFunctionSymbol;

typedef struct AliasPath {
    char *name;
    struct AliasPath *previous;
} AliasPath;

/**
 * @brief Sdílený kontext jednoho běhu sémantické analýzy
 *
 * Uchovává stav potřebný při rekurzivním průchodu AST. Při dočasném
 * přechodu do jiné funkce nebo lambdy je nutné příslušné položky uložit
 * a po návratu obnovit. Kontext nevlastní AST
 */
typedef struct SemanticContext {
    tBtNode *symbols; // Tabulka symbolů
    tBtNode *ext_symbols;
    tStack *scope_stack;       // Zásobník lokálních rozsahů platnosti (scope) pro aktuální funkci
    ASTNode *current_function; // Aktuálně analyzovaná funkce (AST_FUNC_DEF); mimo funkci NULL
    int next_var_id;           // Další volné ID pro přidělení proměnné
    bool inside_lambda;        // True, pokud jsme uvnitř funkce lambda
    bool last_in_block;
} SemanticContext;

/* ==================================================
 * 1 PRŮCHOD – SBĚR GLOBÁLNÍCH DEKLARACÍ
 * ================================================== */

/**
 * @brief Registruje globální deklarace programu, ještě neanalyzuje těla funkcí
 *
 * Prochází definice v AST_PROGRAM a připraví symboly funkcí, datových
 * tříd, typových aliasů a rozšiřujících metod. U funkcí uloží dostupné
 * informace o parametrech a explicitním návratovém typu
 *
 * @param program Kořenový uzel AST obsahujícího program k analýze
 * @param ctx Ukazatel na strukturu SemanticContext, do které se v tabulce symbolů ukládají globální
 * symboly
 * @return 0 při úspěchu, jinak chybový kód IFJ26
 */
int semantic_collect_globals(ASTNode *program, SemanticContext *ctx);

/* ==================================================
 * 2 PRŮCHOD – ANALÝZA TĚL FUNKCÍ A OBOHACENÍ AST
 * ================================================== */

/**
 * @brief Probíhá kontrola sémantiky AST a jeho obohacení
 *
 * Analyzuje těla funkcí, odvozuje chybějící návratové typy a kontroluje
 * sémantiku globálních definic. Při řešení závislostí může analyzovat
 * funkce v jiném než zdrojovém pořadí, již zkontrolované funkce znovu neprochází
 *
 * @param program Kořenový uzel AST_PROGRAM; jeho potomci se mohou měnit
 * @param ctx Kontext analýzy s registrovanými globálními symboly
 * @return 0 při úspěchu, jinak chybový kód IFJ26
 */
int semantic_check_program(ASTNode *program, SemanticContext *ctx);

/* ==================================================
 * POMOCNÉ FUNKCE PRO PRŮCHOD AST
 * ================================================== */

/**
 * @brief Zpracuje jeden uzel AST a doplní jeho informace.
 *
 * @param node Adresa ukazatele v AST na uzel, který se analyzuje. Může být nahrazen novým uzlem
 * @param ctx Kontext sémantické analýzy
 * @param expected_type Očekávaný typ výrazu, nebo NULL, pokud není známý
 * @return 0 při úspěchu, jinak chybový kód IFJ26
 */
int semantic_visit(ASTNode **node, SemanticContext *ctx, const TypeInfo *expected_type);

/**
 * @brief Zkontroluje danou definici funkce a její tělo
 *
 * @param function Uzel typu AST_FUNC_DEF
 * @param ctx Kontext sémantické analýzy
 * @return 0 při úspěchu, jinak chybový kód IFJ26
 */
int semantic_check_function(ASTNode *function, SemanticContext *ctx);

/**
 * @brief Zkontroluje příkazy daného bloku a určí jeho výsledný typ
 *
 * @param block Uzel typu AST_BLOCK
 * @param ctx Kontext sémantické analýzy
 * @param expected_type Očekávaný typ hodnoty bloku, nebo NULL
 * @return 0 při úspěchu, jinak chybový kód IFJ26
 */
int semantic_check_block(ASTNode *block, SemanticContext *ctx, const TypeInfo *expected_type,
                         bool used_as_value);

/* ==================================================
 * FUNKCE PRO VÝRAZOVÉ UZLY AST
 * ================================================== */

int semantic_func_def(SemanticFunctionSymbol *func_sym, SemanticContext *ctx);

/**
 * @brief Určí typ literálu podle jeho druhu a uloží jej do AST
 *
 * @param node Uzel typu AST_LITERAL
 * @return 0 při úspěchu, jinak chybový kód IFJ26
 */
int semantic_literal(ASTNode *node);

/**
 * @brief Vyhledá identifikátor a určí jeho typ a vazbu na deklaraci
 *
 * @param node Adresa ukazatele v AST na uzel typu AST_IDENTIFIER. Může být nahrazen uzlem
 * AST_FUNC_CALL, pokud se jedná o volání funkce.
 * @param ctx Kontext sémantické analýzy
 * @return 0 při úspěchu, jinak chybový kód IFJ26
 */
int semantic_identifier(ASTNode **node, SemanticContext *ctx);

/**
 * @brief Zkontroluje binární operátor a typy obou operandů
 *
 * @param node Uzel typu AST_BINARY_OP
 * @param ctx Kontext sémantické analýzy
 * @return 0 při úspěchu, jinak chybový kód IFJ26
 */
int semantic_binary_op(ASTNode *node, SemanticContext *ctx);

/**
 * @brief Ověří unární operátor a typ jeho operandu
 *
 * @param node Uzel typu AST_UNARY_OP
 * @param ctx Kontext sémantické analýzy
 * @return 0 při úspěchu, jinak chybový kód IFJ26
 */
int semantic_unary_op(ASTNode *node, SemanticContext *ctx);

/**
 * @brief Zkontroluje volání funkce, konstruktoru nebo funkční hodnoty
 *
 * @param node Uzel typu AST_FUNC_CALL
 * @param ctx Kontext sémantické analýzy
 * @param expected_type Očekávaný typ výsledku, nebo NULL
 * @return 0 při úspěchu, jinak chybový kód IFJ26
 */
int semantic_func_call(ASTNode *node, SemanticContext *ctx, const TypeInfo *expected_type);

/**
 * @brief Zkontroluje přístup ke členu objektu nebo hodnoty
 *
 * @param node Adresa ukazatele v AST na uzel typu AST_MEMBER_ACCESS. Může být nahrazen uzlem
 * AST_FUNC_CALL, pokud se jedná o volání metody bez parametrů.
 * @param ctx Kontext sémantické analýzy
 * @return 0 při úspěchu, jinak chybový kód IFJ26
 */
int semantic_member_access(ASTNode **node, SemanticContext *ctx);

/**
 * @brief Určí typ literálu seznamu
 *
 * @param node Uzel typu AST_LIST_LITERAL
 * @param ctx Kontext sémantické analýzy
 * @param expected_type Očekávaný typ List[T], nebo NULL
 * @return 0 při úspěchu, jinak chybový kód IFJ26
 */
int semantic_list_literal(ASTNode *node, SemanticContext *ctx, const TypeInfo *expected_type);

/* ==================================================
 * DEKLARACE
 * ================================================== */

/**
 * @brief Zkontroluje deklaraci proměnné a určí její typ
 *
 * @param node Uzel typu AST_VAR_DEF
 * @param ctx Kontext sémantické analýzy
 * @return 0 při úspěchu, jinak chybový kód IFJ26
 */
int semantic_var_def(tStack *scope, ASTNode *node, SemanticContext *ctx, bool is_param);

/**
 * @brief Zkontroluje definici datové třídy a typy jejích atributů
 *
 * @param node Uzel typu AST_CLASS_DEF
 * @param ctx Kontext sémantické analýzy
 * @return 0 při úspěchu, jinak chybový kód IFJ26
 */
int semantic_class_def(ASTNode *node, SemanticContext *ctx);

/**
 * @brief Ověří definici typového aliasu a rozvine jeho cílový typ
 *
 * @param node Uzel typu AST_TYPE_ALIAS
 * @param ctx Kontext sémantické analýzy
 * @return 0 při úspěchu, jinak chybový kód IFJ26
 */
int semantic_type_alias(ASTNode *node, SemanticContext *ctx);

/**
 * @brief Zkontroluje definici extension
 *
 * @param node Uzel typu AST_EXTENSION_DEF
 * @param ctx Kontext sémantické analýzy
 * @return 0 při úspěchu, jinak chybový kód IFJ26
 */
int semantic_extension_def(ASTNode *node, SemanticContext *ctx);

/**
 * @brief Zkontroluje lambda výraz a sestaví jeho funkční typ
 *
 * @param node Uzel typu AST_LAMBDA
 * @param ctx Kontext sémantické analýzy
 * @param expected_type Očekávaný funkční typ, nebo NULL
 * @return 0 při úspěchu, jinak chybový kód IFJ26
 */
int semantic_lambda(ASTNode *node, SemanticContext *ctx, const TypeInfo *expected_type);

/* ==================================================
 * PŘÍKAZY A ŘÍDICÍ STRUKTURY
 * ================================================== */

/**
 * @brief Zkontroluje přiřazení do existující modifikovatelné proměnné
 *
 * @param node Uzel typu AST_ASSIGNMENT
 * @param ctx Kontext sémantické analýzy
 * @return 0 při úspěchu, jinak chybový kód IFJ26
 */
int semantic_assignment(ASTNode *node, SemanticContext *ctx);

/**
 * @brief Zkontroluje podmíněný příkaz nebo výraz if
 *
 * @param node Uzel typu AST_IF_EXPR
 * @param ctx Kontext sémantické analýzy
 * @param expected_type Očekávaný typ výsledku if, nebo NULL
 * @return 0 při úspěchu, jinak chybový kód IFJ26
 */
int semantic_if_expr(ASTNode *node, SemanticContext *ctx, const TypeInfo *expected_type,
                     bool used_as_value);

/**
 * @brief Zkontroluje podmínku a tělo cyklu while
 *
 * @param node Uzel typu AST_WHILE
 * @param ctx Kontext sémantické analýzy
 * @return 0 při úspěchu, jinak chybový kód IFJ26
 */
int semantic_while(ASTNode *node, SemanticContext *ctx);

/**
 * @brief Zkontroluje cyklus for, jeho generátor, stráže a tělo
 *
 * @param node Uzel typu AST_FOR
 * @param ctx Kontext sémantické analýzy
 * @return 0 při úspěchu, jinak chybový kód IFJ26
 */
int semantic_for(ASTNode *node, SemanticContext *ctx);

/**
 * @brief Zkontroluje výraz match a jeho jednotlivé větve case
 *
 * @param node Uzel typu AST_MATCH_EXPR
 * @param ctx Kontext sémantické analýzy
 * @param expected_type Očekávaný typ výsledku match, nebo NULL
 * @return 0 při úspěchu, jinak chybový kód IFJ26
 */
int semantic_match(ASTNode *node, SemanticContext *ctx, const TypeInfo *expected_type);

/**
 * @brief Zkontroluje příkaz return vůči aktuální funkci
 *
 * @param node Uzel typu AST_RETURN
 * @param ctx Kontext sémantické analýzy
 * @return 0 při úspěchu, jinak chybový kód IFJ26
 */
int semantic_return(ASTNode *node, SemanticContext *ctx);

/* ==================================================
 * POMOCNÉ FUNKCE PRO STRUKTURÁLNÍ UZLY
 * ================================================== */

/**
 * @brief Zkontroluje jednu skupinu argumentů volání
 *
 * @param arg_list Uzel typu AST_ARG_LIST
 * @param ctx Kontext sémantické analýzy
 * @param param_types Seznam očekávaných typů parametrů propojený přes next
 * @return 0 při úspěchu, jinak chybový kód IFJ26
 */
int semantic_check_agr_list(ASTNode *node, SemanticContext *ctx, const TypeInfo *param_types);

/**
 * @brief Zaregistruje parametry jedné skupiny v aktuálním scope
 *
 * @param param_list Uzel typu AST_PARAM_LIST
 * @param ctx Kontext sémantické analýzy
 * @return 0 při úspěchu, jinak chybový kód IFJ26
 */
int semantic_register_param_list(ASTNode *node, SemanticContext *ctx);

/**
 * @brief Zpracuje jednu větev case výrazu match
 *
 * @param node Uzel typu AST_CASE_BRANCH
 * @param ctx Kontext sémantické analýzy
 * @param matched_type Typ hodnoty porovnávané výrazem match
 * @return 0 při úspěchu, jinak chybový kód IFJ26
 */
int semantic_case_branch(ASTNode *node, SemanticContext *ctx, const TypeInfo *matched_type);

/**
 * @brief Zkontroluje jeden vzor a zavede případně vázané proměnné
 *
 * @param node Uzel typu AST_PATTERN
 * @param ctx Kontext sémantické analýzy
 * @param matched_type Typ hodnoty analyzované výrazem match
 * @return 0 při úspěchu, jinak chybový kód IFJ26
 */
int semantic_pattern(ASTNode *node, SemanticContext *ctx, const TypeInfo *matched_type);

/**
 * @brief Zkontroluje číselný rozsah to/until/by
 *
 * @param node Uzel typu AST_RANGE
 * @param ctx Kontext sémantické analýzy
 * @return 0 při úspěchu, jinak chybový kód IFJ26
 */
int semantic_range(ASTNode *node, SemanticContext *ctx);

/* ==================================================
 * POMOCNÉ FUNKCE PRO PRÁCI S TYPY
 * ================================================== */

/**
 * @brief Porovná rovnost dvou typů
 *
 * @param a První porovnávaný typ
 * @param b Druhý porovnávaný typ
 * @return true, pokud jsou typy stejné, jinak false
 */
bool semantic_types_equal(const TypeInfo *a, const TypeInfo *b);

/**
 * @brief Ověří typovou kompatibilitu hodnoty typů actual a expected
 *
 * @param expected Požadovaný cílový typ
 * @param actual Skutečný typ hodnoty
 * @return true při typové kompatibilitě, jinak false
 */
bool semantic_types_compatible(const TypeInfo *expected, const TypeInfo *actual);

/**
 * @brief Určí společný typ dvou výrazů podle pravidel IFJ26
 *
 * @param a Typ prvního výrazu
 * @param b Typ druhého výrazu
 * @param result Adresa ukazatele, kam se uloží nově alokovaný společný typ
 * @return 0 při úspěchu, jinak chybový kód IFJ26
 */
int semantic_common_type(const TypeInfo *a, const TypeInfo *b, TypeInfo **result);

/**
 * @brief Ověří typ výrazu a případně označí implicitní konverzi
 *
 * @param expression Uzel výrazu, jehož typ se má ověřit
 * @param expected_type Očekávaný typ výrazu
 * @return 0 při úspěchu, jinak chybový kód IFJ26
 */
int semantic_check_conversion(ASTNode *expression, const TypeInfo *expected_type);

/**
 * @brief Rozvine typové aliasy v zadaném typu
 *
 * @param type Typ, v němž se mají rozvinout aliasy
 * @param ctx Kontext sémantické analýzy, obsahující tabulku symbolů s aliasy
 * @return 0 při úspěchu, jinak chybový kód IFJ26
 */
int semantic_resolve_type_alias(const TypeInfo *type, SemanticContext *ctx, TypeInfo **result);

bool semantic_always_returns(const ASTNode *node);

tBtNode *semantic_find_symbol(tStack *stack, char *name, SemanticContext *ctx);

bool compare_unions(const TypeInfo *a, const TypeInfo *b);

int is_mem_acc_allowed(ASTNode *node, SemanticContext *ctx, TypeInfo **result,
                       SemanticFunctionSymbol **matched_method);

bool pattern_types_overlap(const TypeInfo *match_type, const TypeInfo *case_type);

/**
 * @brief vytvoří návratový typ vestavěné funkce
 *
 * @param built_def vestavěná funkce
 * @return TypeInfo* návratový typ
 */
TypeInfo *builtin_ret_type(BuiltinDef *built_def);

/**
 * @brief vytvoří celý typ vestavěné funkce
 *
 * @param built_def vestavěná funkce
 * @return TypeInfo* typ funkce
 */
TypeInfo *builtin_type(BuiltinDef *built_def);

/**
 * @brief zaregistruje vestavěné funkce
 *
 * @param ctx kontext sémantické analýzy
 * @return int chybový kód
 */
int register_buildin_func(SemanticContext *ctx);

int semantic_register_global(SemanticContext *ctx, char *name, tBtContentType type,
                             ASTNode *declaration);

int register_ext_func(SemanticContext *ctx, ASTNode *declaration, ASTNode *ext);
int check_globals(ASTNode *program, SemanticContext *ctx);
int semantic_build_function_type(const ASTNode *param_group, const TypeInfo *return_type,
                                 TypeInfo **result);
int semantic_ensure_func_type(SemanticFunctionSymbol *func_sym, SemanticContext *ctx);

int call_param_group_count(const ASTNode *group);
int call_builtin_type(const SemanticFunctionSymbol *func, const ASTNode *args,
                      const TypeInfo **result);
int call_list_method_type(const char *name, const TypeInfo *list, TypeInfo **out);

int call_constructor_type(ASTNode *class_def, TypeInfo **res_type);
int call_prepare_target(ASTNode *call, SemanticContext *ctx, CallKind *kind, int *own_groups);
int call_apply_group(ASTNode *group, SemanticContext *ctx, const TypeInfo *current,
                     TypeInfo **next);
int semantic_extension_member(ASTNode *node, SemanticContext *ctx, TypeInfo **result,
                              SemanticFunctionSymbol **matched_method);
int is_mem_acc_allowed(ASTNode *node, SemanticContext *ctx, TypeInfo **result,
                       SemanticFunctionSymbol **matched_method);

int semantic_fill_unit_else(ASTNode *node);
#endif // SEMANTICS_H
