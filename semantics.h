/**
 * Projekt: Implementace překladače imperativního jazyka IFJ26
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
#include "ierror.h"
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

/**
 * @brief struktura pro ukládání funkcí do vyhledávací tabulky
 */
typedef struct SemanticFunctionSymbol {
    ASTNode *function;  // AST_FUNC_DEF
    ASTNode *extension; // AST_EXTENSION_DEF, NULL pro běžnou funkci

    SemanticFunctionState state;         // aktuální stav řešení funkce
    struct SemanticFunctionSymbol *next; // pro seznam funkcí u extemsion metod
    bool is_buildin;                     // pro vestavěné funkce
    TypeInfo *builtin_signatures[2];     // pro vestavěné funkce

} SemanticFunctionSymbol;

/**
 * @brief struktura pro zaznamenání cesty, využívá se u union stromu
 */
typedef struct AliasPath {
    const char *name;
    const struct AliasPath *previous;
} AliasPath;

/**
 * @brief struktura na vestavěné funkce
 */
typedef struct {
    char *name;
    TypeClass result;
    int argc;
    TypeClass args[3];
} BuiltinDef;

/**
 * @brief struktura pro vytvoření grafu volání funkcí pro odhalování rekurzí
 *
 */
typedef struct CallEdge {
    const ASTNode *calling_func; // uzel funkce, ze které se volá called_func
    const ASTNode *called_func;
    struct CallEdge *next; // další hrana grafu
} CallEdge;

/**
 * @brief struktura s aktuální procházenou cestou volání funkcí
 *
 */
typedef struct FunctionPath {
    const ASTNode *function;
    const struct FunctionPath *previous;
} FunctionPath;

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
    int error_line;            // pro záznam řádku, kde nastala chyba v průběhu sémantiky
    ErrorCode expected_type_error; // kvůli rozlišení typových a argumentových chyb
    CallEdge *call_edges;          // struktura pro odhalování rekurzí
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
 * @return ErrorCode chybový kód podle ierror.h
 */
ErrorCode semantic_collect_globals(ASTNode *program, SemanticContext *ctx);

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
 * @return ErrorCode chybový kód podle ierror.h
 */
ErrorCode semantic_check_program(ASTNode *program, SemanticContext *ctx);

/* ==================================================
 * POMOCNÉ FUNKCE PRO PRŮCHOD AST
 * ================================================== */

/**
 * @brief Zpracuje jeden uzel AST a doplní jeho informace.
 *
 * @param node Adresa ukazatele v AST na uzel, který se analyzuje. Může být nahrazen novým uzlem
 * @param ctx Kontext sémantické analýzy
 * @param expected_type Očekávaný typ výrazu, nebo NULL, pokud není známý
 * @return ErrorCode chybový kód podle ierror.h
 */
ErrorCode semantic_visit(ASTNode **node, SemanticContext *ctx, const TypeInfo *expected_type);

/**
 * @brief Zkontroluje příkazy daného bloku a určí jeho výsledný typ
 *
 * @param block Uzel typu AST_BLOCK
 * @param ctx Kontext sémantické analýzy
 * @param expected_type Očekávaný typ hodnoty bloku, nebo NULL
 * @param used_ad_value určuje, jestli se používá jako výraz nebo příkaz
 * @return ErrorCode chybový kód podle ierror.h
 */
ErrorCode semantic_check_block(ASTNode *block, SemanticContext *ctx, const TypeInfo *expected_type,
                               bool used_as_value);

/* ==================================================
 * FUNKCE PRO VÝRAZOVÉ UZLY AST
 * ================================================== */

/**
 * @brief zkontroluje deklaraci funkce a její tělo
 *
 * @param func_sym uzel funkce
 * @param ctx kontex sémantické analýzy
 * @return ErrorCode chybový kód podle ierror.h
 */
ErrorCode semantic_func_def(SemanticFunctionSymbol *func_sym, SemanticContext *ctx);

/**
 * @brief Určí typ literálu podle jeho druhu a uloží jej do AST
 *
 * @param node Uzel typu AST_LITERAL
 * @return ErrorCode chybový kód podle ierror.h
 */
ErrorCode semantic_literal(ASTNode *node);

/**
 * @brief Vyhledá identifikátor a určí jeho typ a vazbu na deklaraci
 *
 * @param node Adresa ukazatele v AST na uzel typu AST_IDENTIFIER. Může být nahrazen uzlem
 * AST_FUNC_CALL, pokud se jedná o volání funkce
 * @param ctx Kontext sémantické analýzy
 * @return ErrorCode chybový kód podle ierror.h
 */
ErrorCode semantic_identifier(ASTNode **node, SemanticContext *ctx);

/**
 * @brief Zkontroluje binární operátor a typy obou operandů
 *
 * @param node Uzel typu AST_BINARY_OP
 * @param ctx Kontext sémantické analýzy
 * @return ErrorCode chybový kód podle ierror.h
 */
ErrorCode semantic_binary_op(ASTNode *node, SemanticContext *ctx);

/**
 * @brief Ověří unární operátor a typ jeho operandu
 *
 * @param node Uzel typu AST_UNARY_OP
 * @param ctx Kontext sémantické analýzy
 * @return ErrorCode chybový kód podle ierror.h
 */
ErrorCode semantic_unary_op(ASTNode *node, SemanticContext *ctx);

/**
 * @brief zkontroluje volání funkce a určí jeho výsledný typ
 *
 * @param node Uzel typu AST_FUNC_CALL
 * @param ctx Kontext sémantické analýzy
 * @param expected_type Očekávaný typ výsledku, nebo NULL
 * @return ErrorCode chybový kód podle ierror.h
 */
ErrorCode semantic_func_call(ASTNode *node, SemanticContext *ctx, const TypeInfo *expected_type);

/**
 * @brief Zkontroluje přístup ke členu objektu nebo hodnoty
 *
 * @param node Adresa ukazatele v AST na uzel typu AST_MEMBER_ACCESS. Může být nahrazen uzlem
 * AST_FUNC_CALL, pokud se jedná o volání metody bez parametrů.
 * @param ctx Kontext sémantické analýzy
 * @return ErrorCode chybový kód podle ierror.h
 */
ErrorCode semantic_member_access(ASTNode **node, SemanticContext *ctx);

/**
 * @brief Určí typ literálu seznamu
 *
 * @param node Uzel typu AST_LIST_LITERAL
 * @param ctx Kontext sémantické analýzy
 * @param expected_type Očekávaný typ List[T], nebo NULL
 * @return ErrorCode chybový kód podle ierror.h
 */
ErrorCode semantic_list_literal(ASTNode *node, SemanticContext *ctx, const TypeInfo *expected_type);

/* ==================================================
 * DEKLARACE
 * ================================================== */

/**
 * @brief Zkontroluje deklaraci proměnné a určí její typ
 *
 * @param scope ukazatel na zásobník
 * @param node Uzel typu AST_VAR_DEF
 * @param ctx Kontext sémantické analýzy
 * @param is_param true, pokud je definice proměnné použita jako parametr
 * @return ErrorCode chybový kód podle ierror.h
 */
ErrorCode semantic_var_def(tStack *scope, ASTNode *node, SemanticContext *ctx, bool is_param);

/**
 * @brief Zkontroluje definici datové třídy a typy jejích atributů
 *
 * @param node Uzel typu AST_CLASS_DEF
 * @param ctx Kontext sémantické analýzy
 * @return ErrorCode chybový kód podle ierror.h
 */
ErrorCode semantic_class_def(ASTNode *node, SemanticContext *ctx);

/**
 * @brief Ověří definici typového aliasu a rozvine jeho cílový typ
 *
 * @param node Uzel typu AST_TYPE_ALIAS
 * @param ctx Kontext sémantické analýzy
 * @return ErrorCode chybový kód podle ierror.h
 */
ErrorCode semantic_type_alias(ASTNode *node, SemanticContext *ctx);

/**
 * @brief Zkontroluje definici extension
 *
 * @param node Uzel typu AST_EXTENSION_DEF
 * @param ctx Kontext sémantické analýzy
 * @return ErrorCode chybový kód podle ierror.h
 */
ErrorCode semantic_extension_def(ASTNode *node, SemanticContext *ctx);

/**
 * @brief Zkontroluje lambda výraz a sestaví jeho funkční typ
 *
 * @param node Uzel typu AST_LAMBDA
 * @param ctx Kontext sémantické analýzy
 * @param expected_type Očekávaný funkční typ, nebo NULL
 * @return ErrorCode chybový kód podle ierror.h
 */
ErrorCode semantic_lambda(ASTNode *node, SemanticContext *ctx, const TypeInfo *expected_type);

/* ==================================================
 * PŘÍKAZY A ŘÍDICÍ STRUKTURY
 * ================================================== */

/**
 * @brief Zkontroluje přiřazení do existující modifikovatelné proměnné
 *
 * @param node Uzel typu AST_ASSIGNMENT
 * @param ctx Kontext sémantické analýzy
 * @return ErrorCode chybový kód podle ierror.h
 */
ErrorCode semantic_assignment(ASTNode *node, SemanticContext *ctx);

/**
 * @brief Zkontroluje podmíněný příkaz nebo výraz if
 *
 * @param node Uzel typu AST_IF_EXPR
 * @param ctx Kontext sémantické analýzy
 * @param expected_type Očekávaný typ výsledku if, nebo NULL
 * @param used_as_value true, pokud se if používá jako výraz
 * @return ErrorCode chybový kód podle ierror.h
 */
ErrorCode semantic_if_expr(ASTNode *node, SemanticContext *ctx, const TypeInfo *expected_type,
                           bool used_as_value);

/**
 * @brief Zkontroluje podmínku a tělo cyklu while
 *
 * @param node Uzel typu AST_WHILE
 * @param ctx Kontext sémantické analýzy
 * @return ErrorCode chybový kód podle ierror.h
 */
ErrorCode semantic_while(ASTNode *node, SemanticContext *ctx);

/**
 * @brief Zkontroluje cyklus for, jeho generátor, stráže a tělo
 *
 * @param node Uzel typu AST_FOR
 * @param ctx Kontext sémantické analýzy
 * @return ErrorCode chybový kód podle ierror.h
 */
ErrorCode semantic_for(ASTNode *node, SemanticContext *ctx);

/**
 * @brief Zkontroluje výraz match a jeho jednotlivé větve case
 *
 * @param node Uzel typu AST_MATCH_EXPR
 * @param ctx Kontext sémantické analýzy
 * @param expected_type Očekávaný typ výsledku match, nebo NULL
 * @return ErrorCode chybový kód podle ierror.h
 */
ErrorCode semantic_match(ASTNode *node, SemanticContext *ctx, const TypeInfo *expected_type);

/**
 * @brief Zkontroluje příkaz return vůči aktuální funkci
 *
 * @param node Uzel typu AST_RETURN
 * @param ctx Kontext sémantické analýzy
 * @return ErrorCode chybový kód podle ierror.h
 */
ErrorCode semantic_return(ASTNode *node, SemanticContext *ctx);

/* ==================================================
 * POMOCNÉ FUNKCE PRO STRUKTURÁLNÍ UZLY
 * ================================================== */

/**
 * @brief Zkontroluje jednu skupinu argumentů volání
 *
 * @param arg_list Uzel typu AST_ARG_LIST
 * @param ctx Kontext sémantické analýzy
 * @param param_types Seznam očekávaných typů parametrů propojený přes next
 * @return ErrorCode chybový kód podle ierror.h
 */
ErrorCode semantic_check_agr_list(ASTNode *node, SemanticContext *ctx, const TypeInfo *param_types);

/**
 * @brief Zaregistruje parametry jedné skupiny v aktuálním scope
 *
 * @param param_list Uzel typu AST_PARAM_LIST
 * @param ctx Kontext sémantické analýzy
 * @return ErrorCode chybový kód podle ierror.h
 */
ErrorCode semantic_register_param_list(ASTNode *node, SemanticContext *ctx);

/**
 * @brief Zpracuje jednu větev case výrazu match
 *
 * @param node Uzel typu AST_CASE_BRANCH
 * @param ctx Kontext sémantické analýzy
 * @param matched_type Typ hodnoty porovnávané výrazem match
 * @param expected_type očekávaný typ
 * @return ErrorCode chybový kód podle ierror.h
 */
ErrorCode semantic_case_branch(ASTNode *node, SemanticContext *ctx, const TypeInfo *matched_type,
                               const TypeInfo *expected_type);

/**
 * @brief Zkontroluje jeden vzor a zavede případně vázané proměnné
 *
 * @param node Uzel typu AST_PATTERN
 * @param ctx Kontext sémantické analýzy
 * @param matched_type Typ hodnoty analyzované výrazem match
 * @return ErrorCode chybový kód podle ierror.h
 */
ErrorCode semantic_pattern(ASTNode *node, SemanticContext *ctx, const TypeInfo *matched_type);

/**
 * @brief Zkontroluje číselný rozsah to/until/by
 *
 * @param node Uzel typu AST_RANGE
 * @param ctx Kontext sémantické analýzy
 * @return ErrorCode chybový kód podle ierror.h
 */
ErrorCode semantic_range(ASTNode *node, SemanticContext *ctx);

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
 * @return ErrorCode chybový kód podle ierror.h
 */
ErrorCode semantic_common_type(const TypeInfo *a, const TypeInfo *b, TypeInfo **result);

/**
 * @brief Ověří typ výrazu a případně označí implicitní konverzi
 *
 * @param expression Uzel výrazu, jehož typ se má ověřit
 * @param expected_type Očekávaný typ výrazu
 * @return ErrorCode chybový kód podle ierror.h
 */
ErrorCode semantic_check_conversion(ASTNode *expression, const TypeInfo *expected_type);

/**
 * @brief Rozvine typové aliasy v zadaném typu
 *
 * @param type Typ, v němž se mají rozvinout aliasy
 * @param ctx Kontext sémantické analýzy, obsahující tabulku symbolů s aliasy
 * @param result pro vrácení výsledného typu
 * @return ErrorCode chybový kód podle ierror.h
 */
ErrorCode semantic_resolve_type_alias(const TypeInfo *type, SemanticContext *ctx,
                                      TypeInfo **result);

/**
 * @brief Vyhledá, jestli parametru "un" typu union obsahuje typ z parametru "type"
 *
 * @param un Typeinfo typu union, ve kterém se hledá typ
 * @param type Typeinfo, který se hledá v parametru "un"
 * @return true pokud je typ z parametru "type" obsažen v parametru "un", jinak false
 */
bool semantics_compare_union_with_type(const TypeInfo *un, const TypeInfo *type);

/**
 * @brief Porovná, jestli union parametru 'b' obsahuje všechny typy z union parametru 'a'
 *
 * @param a TypeInfo typu union, ze kterého se porovnávají typy
 * @param b TypeInfo typu union, ve kterém se hledají typy z parametru 'a'
 * @return true pokud všechny typy z parametru 'a' jsou obsaženy v parametru 'b', jinak false
 */
bool compare_unions(const TypeInfo *a, const TypeInfo *b);

/**
 * @brief ověří, zda se typ vzoru shoduje s některým z typů výrazu match
 *
 * @param match_type typ výrazu match, může být union
 * @param case_type typ vzoru v case
 * @return true pokud se typy překrývají, jinak false
 */
bool pattern_types_overlap(const TypeInfo *match_type, const TypeInfo *case_type);

/**
 * @brief rekurzivně rozvine typové aliasy a kontroluje jejich cykly
 *
 * @param slot typ, který řešíme
 * @param ctx kontext sémantické analýzy
 * @param path cesta rozvíjených aliasů pro kontrolu cyklů
 * @return ErrorCode chybový kód podle ierror.h
 */
ErrorCode expand_aliases(TypeInfo **slot, SemanticContext *ctx, const AliasPath *path);

/**
 * @brief upraví union strom do tvaru, který vytváří parser (další union uzly jsou vždy vlevo)
 *
 * @param type typ, se kterým pracujeme
 */
void normalise_union_tree(TypeInfo *type);

/* ==================================================
 * DALŠÍ POMOCNÉ FUNKCE
 * ================================================== */

/**
 * @brief kontroluje, jestli v blok vždy skončí returnem
 *
 * @param node uzel kontrolovaného bloku
 * @return true pokud všechny větve mají return, jinak false
 */
bool semantic_always_returns(const ASTNode *node);

/**
 * @brief prohledá všechny vyhledávací tabulky a vrátí ukazatel na hledaný uzel
 *
 * @param stack zásobník, ve kterém se hledá
 * @param name klíč do tabulky
 * @param ctx kontext sémantické analýzy
 * @param location ukazatel na adresu nalezeného uzlu
 * @return ErrorCode chybový kód podle ierror.h
 */
ErrorCode semantic_find_symbol(tStack *stack, char *name, SemanticContext *ctx, tBtNode **location);

/**
 * @brief ověří přístup ke členu objektu a určí jeho typ
 *
 * @param node ukazatel na uzel member_access
 * @param ctx kontext sémantické analýzy
 * @param result výstupní parametr, do kterého funkce uloží typ nalezeného členu
 * @param matched_method výstupní parametr pro nalezenou extension metodu, u ostatního NULL
 * @return ErrorCode chybový kód podle ierror.h
 */
ErrorCode is_mem_acc_allowed(ASTNode *node, SemanticContext *ctx, TypeInfo **result,
                             SemanticFunctionSymbol **matched_method);

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
 * @return ErrorCode chybový kód podle ierror.h
 */
ErrorCode register_buildin_func(SemanticContext *ctx);

/**
 * @brief vytvoří globální deklaraci a dá so tabulky globálních symbolů
 *
 * @param ctx kontext sémantické analýzy
 * @param name klíč pro tabulku
 * @param type typ uzlu
 * @param declaration uzel co se přidává
 * @return ErrorCode chybový kód podle ierror.h
 */
ErrorCode semantic_register_global(SemanticContext *ctx, char *name, tBtContentType type,
                                   ASTNode *declaration);

/**
 * @brief zaregistruje extension funkci
 *
 * @param ctx kontext sémantické analýzy
 * @param declaration uzel s funkcí
 * @param ext uzel s extension částí funkce
 * @return ErrorCode chybový kód podle ierror.h
 */
ErrorCode register_ext_func(SemanticContext *ctx, ASTNode *declaration, ASTNode *ext);

/**
 * @brief zpracuje globální deklarace aliasů a tříd
 *
 * @param program program který kontroluje
 * @param ctx kontext sémantické analýzy
 * @return ErrorCode chybový kód podle ierror.h
 */
ErrorCode check_global_alias_class(ASTNode *program, SemanticContext *ctx);

/**
 * @brief vytvoří typ funkce
 *
 * @param param_group skupina parametrů
 * @param return_type návratový typ
 * @param result výsledný typ funkce
 * @return ErrorCode chybový kód podle ierror.h
 */
ErrorCode semantic_build_function_type(const ASTNode *param_group, const TypeInfo *return_type,
                                       TypeInfo **result);

/**
 * @brief zkontroluje stav kontroly funkce
 *
 * @param func_sym struktura s funkcí
 * @param ctx kontext sémantické analýzy
 * @return ErrorCode chybový kód podle ierror.h
 */
ErrorCode semantic_ensure_func_type(SemanticFunctionSymbol *func_sym, SemanticContext *ctx);

/**
 * @brief zjistí počet parametrů
 *
 * @param group skupina parametrů
 * @return int počet parametrů
 */
int call_param_group_count(const ASTNode *group);

/**
 * @brief vrátí typ u vestavěné funkce a ověří, že v místě jejího volání má správný počet argumentů
 *
 * @param func symbol s deklaradí funkce
 * @param args počet argumentů volání
 * @param result výsledný typ
 * @return ErrorCode chybový kód podle ierror.h
 */
ErrorCode call_builtin_type(const SemanticFunctionSymbol *func, const ASTNode *args,
                            const TypeInfo **result);

/**
 * @brief vyřeší vestavěné funkce co pracují s List
 *
 * @param name název volané funkce
 * @param list typ listu
 * @param out výsledný typ
 * @return ErrorCode chybový kód podle ierror.h
 */
ErrorCode call_list_method_type(const char *name, const TypeInfo *list, TypeInfo **out);

/**
 * @brief vytvoří typ pro třídu
 *
 * @param class_def definice třídy
 * @param res_type výsledný typ
 * @return ErrorCode chybový kód podle ierror.h
 */
ErrorCode call_constructor_type(ASTNode *class_def, TypeInfo **res_type);

/**
 * @brief určí druh volání a připraví typ volaného cíle
 * @param call uzel s voláním
 * @param ctx kontext sémantické analýzy
 * @param kind druh volání
 * @param own_groups počet skupin argumentů patřících k volané metodě
 * @return ErrorCode chybový kód podle ierror.h
 */
ErrorCode call_prepare_target(ASTNode *call, SemanticContext *ctx, CallKind *kind, int *own_groups);

/**
 * @brief zkontroluje skupinu argumentů a určí typ po jejím použití
 *
 * @param group uzel AST_ARG_LIST
 * @param ctx kontext sémantické analýzy
 * @param current aktuální funkční typ nebo typ seznamu
 * @param next místo pro nově alokovaný výsledný typ
 * @return ErrorCode chybový kód podle ierror.h
 */
ErrorCode call_apply_group(ASTNode *group, SemanticContext *ctx, const TypeInfo *current,
                           TypeInfo **next);

/**
 * @brief řeší member acces když je extension
 *
 * @param node řešený uzel
 * @param ctx kontext sémantické analýzy
 * @param result výsledný typ
 * @param matched_method ukazatel na strukturu s nalezenou extension funkcí
 * @return ErrorCode chybový kód podle ierror.h
 */
ErrorCode semantic_extension_member(ASTNode *node, SemanticContext *ctx, TypeInfo **result,
                                    SemanticFunctionSymbol **matched_method);

/**
 * @brief doplní () do else pro případ, kdy je if na konci bloku jako příkaz bez else větve
 *
 * @param node uzel podmínky
 * @return ErrorCode chybový kód podle ierror.h
 */
ErrorCode semantic_fill_unit_else(ASTNode *node);

/**
 * @brief funkce na uložení čísla řádku při chybě, pokud je code ERR_OK, nic neprovede a vrátí code
 *
 * @param ctx  kontext sémantické analýzy
 * @param code chybový kód
 * @param node uzel, kde chyba nastala
 * @return ErrorCode vrací parametr code
 */
ErrorCode semantic_error(SemanticContext *ctx, ErrorCode code, const ASTNode *node);

/**
 * @brief uvolní SemanticFunctionSymbol struktury z uzlů ve vyhledávací tabulce
 *
 * @param node kořen vyhledávací tabulky
 */
void semantic_free_func_sym(tBtNode *node);

/**
 * @brief najde konkrétní extension metodu a vrátí ji
 *
 * @param function uzel definice hledané funkce
 * @param ctx kontext sémantické analýzy
 * @return nalezená metoda, jinak NULL
 */
SemanticFunctionSymbol *find_extension_symbol(ASTNode *function, SemanticContext *ctx);

/**
 * @brief projde cestu aliasů path a zjistí, jestli se nějaký alias na cestě neopakuje (vyskytly
 * by se cykly)
 *
 * @param path cesta aliasů
 * @param name jméno aliasu, který se hledá v path
 * @return true pokud se alias s jménem namu už na cestě vyskytuje, jinak false
 */
bool alias_in_path(const AliasPath *path, const char *name);

/**
 * @brief odstraní duplikátní typy z union (v levostranném tvaru). Pokud po odstranění duplikátních
 * typů zbývá pouze jeden typ, union se zruší a typ se nastaví na ten jeden
 *
 * @param slot typ který upravujeme
 */
void remove_union_duplicates(TypeInfo **slot);

/**
 * @brief funkce pro zapsání volání funkce called_func z aktuální funkce do grafu volání funkcí
 *
 * @param ctx kontext sémantické analýzy
 * @param called_func funkce, která se volá
 * @return ErrorCode chybový kód
 */
ErrorCode semantic_add_call_edge(SemanticContext *ctx, const ASTNode *called_func);

/**
 * @brief funkce pro kontrolu rekurze
 *
 * @param ctx kontext sémantické analýzy
 * @return ErrorCode
 */
ErrorCode semantic_check_recursion(SemanticContext *ctx);

/**
 * @brief funkce pro vyhledávání, zda existuje přes nějakou funkci rekurze
 *
 * @param ctx kontext sémantické analýzy
 * @param current aktuální uzel funkce
 * @param target uzel cílového volání
 * @param path aktuálí cesta mezi funkcemi
 * @return true pokud existuje rekurze, jinak false
 */
bool semantic_reaches_function(const SemanticContext *ctx, const ASTNode *current,
                               const ASTNode *target, const FunctionPath *path);
#endif // SEMANTICS_H
