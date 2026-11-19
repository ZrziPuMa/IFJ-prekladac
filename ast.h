/**
 * Projekt: Implementace překladače imperativního jazyka IFJ26
 * @file ast.h
 * @brief Abstraktní syntaktický strom (AST) a typy (TypeInfo): uzly,
 *        konstruktory, pomocné funkce pro sémantiku a destruktory.
 * @author František Bilavčík (<xbilavf00>)
 *
 *
 * AI - Při vývoji a návrhu byly použity Gemini Pro, Claude (Opus 5.5). Byly použity
 * pro vysvětlení pojmu AST, pomocí s návrhem, vzájemné opravě a řešení problémů při postupném
 * vývoji, pro verifikaci a testování use cases a hledání chyb. Nelze napsat určitý prompt, nicméně
 * kód byl napsán s využitím AI, minimálně ve své finální verzi (názvy, funkce, některé komentáře)
 * neměněn a následně zkontrolován autorem.
 */

#ifndef AST_H
#define AST_H

#include <stdbool.h>
#include <stdio.h>

/* ========================================================================== *
 *  TypeInfo: Typy a jejich reprezentace v AST (TypeClass, TypeInfo)
 * ========================================================================== */

typedef enum {
    TYPE_UNIT,   // jediná hodnota (), nenese informaci
    TYPE_INT,    // celočíselný typ
    TYPE_DOUBLE, // desetinný typ
    TYPE_STRING, // řetězec
    TYPE_BOOLEAN, // rozšíření BOOLTHEN: Logický typ, bez rozšíření jen výsledek porovnání
    TYPE_UNION, // rozšíření ALGEBDT: Sjednocení (např. Int | String). Porovnává se jako množina
    TYPE_FUNCTION, // k základu i rozšíření LAMBDAS: Funkční typ (např. (Int) => Boolean)
    TYPE_LIST,     // rozšíření LISTS: Homogenní seznam (např. List[Int])
    TYPE_CUSTOM,   // rozšíření ALGEBDT: Datové třídy nebo aliasy
    TYPE_ANY, // jakýkoliv typ, je používaný jen ve vestavěných funkcích (např. print(x: Any))
    TYPE_NOTHING // výraz, který nedoběhne (return, i s hodnotou); přenese se výš do bloku, if a
                 // match, pokud jím končí všechny cesty. Je kompatibilní s každým typem,
                 // lub(Nothing, T) = T (least upper bound)
} TypeClass;

typedef struct TypeInfo TypeInfo;

struct TypeInfo {
    // Typ uzlu.
    TypeClass type_class;

    // Zřetězení typů pro parametry funkčních typů
    TypeInfo *next;

    // Další data pro složitější typy (TYPE_UNION, TYPE_FUNCTION, TYPE_LIST, TYPE_CUSTOM).
    union {
        // TYPE_UNION: left | right, delší sjednocení skládá parser zleva
        struct {
            TypeInfo *left;  // levá část sjednocení
            TypeInfo *right; // pravá část sjednocení
        } union_type;

        // TYPE_FUNCTION: (param_types) => return_type
        struct {
            TypeInfo *param_types; // Spojový seznam parametrů
            TypeInfo *return_type;
        } func_type;

        // TYPE_LIST: List[element_type]
        TypeInfo *list_element_type;
        // TYPE_CUSTOM: jméno třídy nebo aliasu podle zápisu (vlastní kopie); význam řeší sémantika
        char *custom_name;
    } data;
};

/* ========================================================================== *
 *  VZORY PRO PATTERN MATCHING (rozšíření ALGEBDT)
 * ========================================================================== */

typedef enum {
    PATTERN_LITERAL,      // case 0; case "a", case -1
    PATTERN_TYPE_BINDING, // např. n: Int
    PATTERN_CONSTRUCTOR,  // např. Point(x, y)
    PATTERN_WILDCARD      // _
} PatternType;

/* ========================================================================== *
 *  STRUKTURA UZLŮ AST
 * ========================================================================== */

// Typy uzlů, u každého je člen ASTNode.data, ve kterém uzel obsahuje data.
typedef enum {
    AST_ARG_LIST,      // data.list
    AST_ASSIGNMENT,    // data.assignment
    AST_BINARY_OP,     // data.bin_op
    AST_BLOCK,         // data.block
    AST_CASE_BRANCH,   // data.case_branch
    AST_CLASS_DEF,     // data.class_def
    AST_EXTENSION_DEF, // data.extension_def
    AST_FOR,           // data.for_loop
    AST_FUNC_CALL,     // data.func_call
    AST_FUNC_DEF,      // data.func_def
    AST_IDENTIFIER,    // data.identifier
    AST_IF_EXPR,       // data.if_expr
    AST_LAMBDA,        // data.lambda
    AST_LIST_LITERAL,  // data.list: List(v1, v2, ...), List()
    AST_LITERAL,       // data.literal
    AST_MATCH_EXPR,    // data.match_expr
    AST_MEMBER_ACCESS, // data.member_access
    AST_PARAM_LIST,    // data.list
    AST_PATTERN,       // data.pattern
    AST_PROGRAM,       // data.program
    AST_RANGE,         // data.range: a to b , a until b , a to b by s
    AST_RETURN,        // data.return_stmt
    AST_TYPE_ALIAS,    // data.type_alias
    AST_UNARY_OP,      // data.unary_op
    AST_VAR_DEF,       // data.var_def
    AST_WHILE          // data.while_loop
} ASTNodeType;

typedef enum {
    LIT_INT,    // value.i_val
    LIT_DOUBLE, // value.d_val
    LIT_STRING, // value.s_val (vlastní kopie řetězce, uvolní ji ast_free)
    LIT_BOOL,   // value.b_val (rozšíření BOOLTHEN: true / false)
    LIT_UNIT    // () - hodnota se nepoužívá
} LiteralKind;

// Co znamená volání; vyplní sémantika, 0 = ještě neurčeno
typedef enum {
    CALL_UNRESOLVED = 0,
    CALL_USER_FUNC,   // pojmenovaná funkce: foo(1), add(1)(2)(3), answer, známe už za překladu
    CALL_BUILTIN,     // vestavěná funkce: print(x), List.range(1, 10)
    CALL_CONSTRUCTOR, // datová třída: Point(1, 2)
    CALL_EXTENSION,   // rozšiřující metoda: x.plus(12), x.twice
    CALL_LIST_METHOD, // metoda seznamu s argumenty: lst.drop(1), lst.filter(f)
    CALL_VALUE // cíl je hodnota, známe až za běhu programu, například funkce v proměnné nebo v
               // parametru
} CallKind;

// Co znamená příkaz výraz.jméno, vyplní sémantika, 0 = ještě neurčeno.
typedef enum {
    MEMBER_UNRESOLVED = 0,
    MEMBER_FIELD,         // rozšíření ALGEBDT: atribut datové třídy, p.x
    MEMBER_VALUE_BUILTIN, // rozšíření VALMEM: .toString .toInt .floor .length
    MEMBER_LIST_BUILTIN, // rozšíření LISTS: člen .head .tail .toString, nebo metoda .drop .take ...
                         // (cíl volání) .toString na hodnotě typu List je vždy MEMBER_LIST_BUILTIN
    MEMBER_RANGE_TOLIST, // rozšíření LISTS + CONTROL: (1 to 5).toList
    MEMBER_EXTENSION // rozšíření EXTKW: rozšiřující metoda; vždy jako cíl FUNC_CALL (x.twice
                     // sémantika zabalí)
} MemberKind;

typedef struct ASTNode ASTNode;

struct ASTNode {
    ASTNodeType type;

    // Metadata: číslo řádku v původním zdrojovém souboru. Lexer ho nastavuje v tokenu
    int line;

    // Sémantické obohacení (Attributed AST)
    // Po sémantice je vyplněn u všech uzlů kromě CASE_BRANCH, CLASS_DEF, EXTENSION_DEF, PARAM_LIST,
    // PATTERN jiného druhu než PATTERN_TYPE_BINDING, PROGRAM a RANGE (rozsah nemá vlastní typ)
    // Generátor čte jen expr_type, nikdy declared_type / return_type
    // Aliasy jsou v něm už nahrazené; TYPE_CUSTOM v expr_type vždy znamená třídu
    TypeInfo *expr_type;

    // to_double: vyplní sémantika. true = hodnotu tohoto výrazu převést před použitím z Int na
    // Double Výchozí je false
    bool to_double;

    // Lineární zřetězení uzlů
    ASTNode *next;

    // Data podle typu uzlu. Nad každou je uvedeno, které typy uzlů ji používají.
    union {
        // data.assignment: AST_ASSIGNMENT
        struct {
            char *target;   // jméno proměnné vlevo (spec: vlevo smí být jen id)
            ASTNode *value; // výraz nebo volání vpravo
            int var_id; // vyplní sémantika: var_id deklarace, do které se zapisuje (0 < var_id,
                        // cílem je vždy lokální var); 0 = sémantika ještě neproběhla. Stejné jméno
                        // může mít víc deklarací (vnořené bloky)
        } assignment;

        // data.bin_op: AST_BINARY_OP
        struct {
            int operator_token; // Typ operátoru podle tokenu
            ASTNode *left;      // levý operand
            ASTNode *right;     // pravý operand
        } bin_op;

        // data.block: AST_BLOCK
        struct {
            ASTNode *statements; // první příkaz, další přes next (blok nesmí být prázdný)
                                 // BLOCK smí stát i na místě výrazu (větve if-výrazu, inicializace
                                 // na dalším řádku); jeho hodnota = hodnota posledního příkazu
                                 // (Unit, pokud je posledním definice/přiřazení/cyklus)
        } block;

        // data.case_branch: AST_CASE_BRANCH
        struct {
            ASTNode *pattern; // Uzel typu AST_PATTERN
            ASTNode *guard;   // Volitelná if stráž, podmínka
            ASTNode *body;    // vždy AST_BLOCK
        } case_branch;

        // data.class_def: AST_CLASS_DEF
        struct {
            char *name;      // např. Point v: case class Point(x: Int, y: Int)
            ASTNode *fields; // seznam AST_VAR_DEF (is_mutable = false, initializer = NULL)
        } class_def;

        // data.extension_def: AST_EXTENSION_DEF
        struct {
            ASTNode *receiver; // příjemce jako parametr (AST_VAR_DEF): extension (value: Int) ->
                               // identifier "value", declared_type Int (vždy napsaný), is_mutable =
                               // false, initializer = NULL
            ASTNode *methods; // seznam AST_FUNC_DEF; volají se jen přes tečku, příjemce je jejich
                              // skrytý 1. argument
        } extension_def;

        // data.for_loop: AST_FOR
        struct {
            ASTNode *iter_var; // AST_VAR_DEF řídicí proměnné (NE AST_IDENTIFIER): is_mutable =
                               // false, initializer = NULL, declared_type = NULL (typ doplní
                               // sémantika do expr_type)
            ASTNode *source;   // zdroj generátoru: AST_RANGE, nebo výraz typu List[T] (LISTS)
            ASTNode *guards;   // CONTROL: if stráže (seznam výrazů)
            ASTNode *body;     // vždy AST_BLOCK
        } for_loop;

        // data.func_call: AST_FUNC_CALL
        struct {
            ASTNode *target; // co se volá: identifikátor, člen (x.plus), nebo libovolný výraz
                             // (LAMBDAS, LISTS)
            ASTNode *arg_groups; // VŠECHNY bezprostředně následující skupiny argumentů (...) jako
                                 // seznam AST_ARG_LIST přes next; NULL = volání bez závorek
                                 // (answer, x.twice). Parser je nedělí.
            int own_groups; // vyplní sémantika: kolik prvních skupin patří volané definici
                            // (add(a)(b)(c) -> 3, goo() -> 1, answer -> 0, volání hodnoty
                            // f(1)/lst(2) -> 0). Každá další skupina se aplikuje na dosavadní
                            // výsledek: funkce = volání, List = index. -1 = sémantika ještě
                            // neproběhla (nastaví konstruktor).
            CallKind call_kind; // vyplní sémantika
        } func_call;

        // data.func_def: AST_FUNC_DEF
        struct {
            char *name;   // jméno funkce, identifikátor (kopie řetězce, uvolní ast_free)
            bool is_main; // true = @main def ...; false = běžná funkce. Parser ho nastaví podle
                          // tokenu @main.
            ASTNode *param_groups; // PARAMLISTS: seznam AST_PARAM_LIST přes next. NULL = bez
                                   // seznamu (def answer) jeden PARAM_LIST s items == NULL =
                                   // prázdný seznam (def answer())
            TypeInfo *return_type; // napsaný návratový typ; NULL = nenapsán. Sémantika ho nemění;
                                   // odvozený nebo rozvinutý typ funkce je v expr_type. Na
                                   // NULL/ne-NULL závisí pravidla spec 2.3
            ASTNode *body; // vždy AST_BLOCK (jednořádkové tělo parser zabalí)
        } func_def;

        // data.identifier: AST_IDENTIFIER
        struct {
            char *name; // jméno proměnné, identifikátor (kopie řetězce, uvolní ast_free)
            int var_id; // vyplní sémantika: 0 < var_id = lokální proměnná nebo parametr (var_id
                        // její deklarace) -1 = nejde o lokální proměnnou (funkce, třída, vestavěná
                        // funkce) 0 = sémantika ještě neproběhla Holé jméno funkce: jedna a více
                        // seznamů parametrů = odkaz na funkci (var_id -1) žádný seznam = volání ->
                        // sémantika zabalí do FUNC_CALL (ast_wrap_in_call)
        } identifier;

        // data.if_expr: AST_IF_EXPR
        struct {
            ASTNode *condition;  // výraz podmínky
            ASTNode *then_block; // vždy AST_BLOCK
            ASTNode *else_block; // AST_BLOCK, nebo NULL = chybí else. Parser NEDOPLŇUJE ()
                                 // `else if` = AST_BLOCK s jediným příkazem AST_IF_EXPR
        } if_expr;

        // data.lambda: AST_LAMBDA
        struct {
            ASTNode *param_list; // vždy jeden AST_PARAM_LIST, i pro () => ...
            ASTNode *body;       // vždy AST_BLOCK, návratový typ lambda nemá,
                                 // její typ je jen v expr_type.
        } lambda;

        // data.list: AST_ARG_LIST, AST_LIST_LITERAL, AST_PARAM_LIST
        struct {
            ASTNode *items; // AST_ARG_LIST: výrazy, AST_LIST_LITERAL: prvky seznamu,
                            // AST_PARAM_LIST: VAR_DEF parametry; NULL = () / List()
        } list;

        // data.literal: AST_LITERAL
        struct {
            LiteralKind kind; // která položka z value je platná
            union {
                long long i_val; // literál má rozsah int z IFJcode26 = 64 bitů
                double d_val;    // double
                char *s_val;     // string
                bool b_val;      // boolean
            } value;
        } literal;

        // data.match_expr: AST_MATCH_EXPR
        struct {
            ASTNode *matched_expr; // např. value match
            ASTNode *cases;        // Seznam AST_CASE_BRANCH uzlů
        } match_expr;

        // data.member_access: AST_MEMBER_ACCESS
        struct {
            ASTNode *object;   // vlevo od tečky, object.something
            char *member_name; // jméno vpravo od tečky: atribut (x), člen (length, head, toList),
                               // metoda (plus)
            MemberKind member_kind; // vyplní sémantika
        } member_access;

        // data.pattern: AST_PATTERN
        struct {
            PatternType pat_type;   // Druh patternu
            char *bound_identifier; // jen PATTERN_TYPE_BINDING: je to n v case n: Int; jinak je to
                                    // NULL
            int var_id; // jen PATTERN_TYPE_BINDING: číslo deklarace n (podobně jako u
                        // var_def.var_id); jinak je to 0
            union {
                ASTNode *literal_val; // PATTERN_LITERAL: case 0 => , srovnávání
                TypeInfo *
                    type_bind; // PATTERN_TYPE_BINDING: typ podle zápisu (může být alias). Rozvinutá
                               // kopie = expr_type uzlu AST_PATTERN = typ vázané proměnné.
                struct {
                    TypeInfo *class_type; // TYPE_CUSTOM se jménem třídy podle zápisu; sémantika ho
                                          // nemění.
                    ASTNode *args; // argumenty podle pořadí atributů (přes next), každý je:
                                   // - AST_VAR_DEF: nová proměnná (a), is_mutable = false,
                    //   declared_type = NULL, initializer = NULL; var_id a typ
                    //   (expr_type) doplní sémantika,
                    // - AST_PATTERN s PATTERN_LITERAL (0, -1, "x"): porovnání,
                    // - AST_PATTERN s PATTERN_WILDCARD (_): hodnota se ignoruje.
                    // Jiný vzor (vnořený Point(...), n: T) v argumentu není.
                } constructor; // PATTERN_CONSTRUCTOR: např.: case Point(a, 0) =>, Point(_, y) =>
            } detail;
        } pattern;

        // data.program: AST_PROGRAM
        struct {
            ASTNode *definitions; // první definice nejvyšší úrovně, další přes next; PROGRAM je
                                  // kořen (jen jednou)
        } program;

        // data.range: AST_RANGE
        struct {
            ASTNode *start; // dolní mez (výraz Int)
            ASTNode *end;   // horní mez (výraz Int)
            ASTNode *step;  // "by" krok; NULL = krok o velikosti 1.
            bool is_until;  // true = horní mez se nezahrnuje
        } range; // AST_RANGE smí stát JEN jako for_loop.source nebo jako object u .toList;
                 // jinak sémantika hlásí chybu.

        // data.return_stmt: AST_RETURN
        struct {
            ASTNode *value; // NULL = samostatné `return` (vrací ())
        } return_stmt;

        // data.type_alias: AST_TYPE_ALIAS
        struct {
            char *name;        // Scalar u: type Scalar = Int | Double | String
            TypeInfo *aliased; // typ napravo od = přesně podle zápisu (smí obsahovat jiné aliasy).
                               // Rozvinutou kopii bez aliasů uloží sémantika do expr_type tohoto
                               // uzlu; tabulka symbolů si ji jen půjčuje.
        } type_alias;

        // data.unary_op: AST_UNARY_OP
        struct {
            int operator_token; // unární -, + nebo !
            ASTNode *operand;   // operand výrazu (napravo, např -x, +y, !z)
        } unary_op;

        // data.var_def: AST_VAR_DEF
        struct {
            int var_id; // vyplní sémantika: číslo této deklarace, jedinečné v celém programu (1, 2,
                        // ...) 0 = sémantika ještě neproběhla. Generátor z něj tvoří
                        // LF@<identifier>$<var_id> -1 = deklarace, která není lokální proměnnou:
                        // atribut třídy (čte se přes p.x). Po sémantice tedy žádný VAR_DEF nemá 0
            char *identifier; // jméno proměnné, identifikátor (kopie řetězce, uvolní ast_free)
            bool is_mutable;         // true = var, false = val
            TypeInfo *declared_type; // typ napsaný v kódu; NULL = nenapsán. Zapisuje parser,
                                     // sémantika ho NIKDY nemění (ani alias nenahrazuje). Skutečný
                                     // typ proměnné je v expr_type tohoto uzlu
            ASTNode *initializer; // výraz za = u val/var (povinný); jinak NULL
        } var_def;

        // data.while_loop: AST_WHILE
        struct {
            ASTNode *condition; // Podmínka, například binary_op, identifier, func_call, atd. (výraz
                                // typu Boolean)
            ASTNode *body; // vždy AST_BLOCK
        } while_loop;

    } data;
};

/* ========================================================================== *
 *  Konstruktory
 * ========================================================================== */

// Platí pro všechny konstruktory:
// - Při úspěchu převezmou předané podstromy a typy, řetězce si zkopírují.
// - Při chybě alokace vrátí NULL a nepřevezmou nic: předané podstromy dál patří volajícímu.
// - Anotace sémantiky nastaví na „neurčeno“: expr_type = NULL, to_double = false, var_id = 0,
//   member_kind = MEMBER_UNRESOLVED; next = NULL.
// - Tvar (např. že body je AST_BLOCK) nekontrolují, to je úkol ast_check.
// Chyby: nikde exit(). Každá funkce při chybě uvolní, co sama vlastní (goto fail + ast_free,
// ast_free(NULL) nic nedělá), a vrátí NULL / kód chyby. main uvolní kořen AST.

/**
 * @brief Vytvoří binární operaci.
 * @param op_token číslo operátoru z Tokenu.
 * @param left Levý operand.
 * @param right Pravý operand.
 * @param line Číslo řádku ve zdrojovém souboru.
 * @return Vytvořený uzel binární operace nebo NULL v případě chyby.
 */
ASTNode *ast_create_binop(int op_token, ASTNode *left, ASTNode *right, int line);

/**
 * @brief Vytvoří uzel volání funkce (AST_FUNC_CALL).
 * @param target Cíl volání: identifikátor, člen (x.plus) nebo libovolný výraz.
 * @param arg_groups Všechny skupiny (...) za cílem jako seznam AST_ARG_LIST přes next; NULL = bez
 * závorek. Parser je nedělí, kolik jich patří funkci, určí sémantika.
 * @param line Číslo řádku ve zdrojovém souboru.
 * @return Nový uzel, nebo NULL při chybě alokace; pak nepřevezme target ani arg_groups.
 * @note Anotace sémantiky nastaví na „neurčeno“: own_groups = -1, call_kind = CALL_UNRESOLVED,
 * expr_type = NULL.
 */
ASTNode *ast_create_func_call(ASTNode *target, ASTNode *arg_groups, int line);

/**
 * @brief Vytvoří uzel if-výrazu (AST_IF_EXPR).
 * @param cond Podmínka.
 * @param then_blk Blok pro případ, že podmínka platí.
 * @param else_blk Blok pro případ, že podmínka neplatí; NULL = else chybí (parser nedoplňuje ()).
 * @param line Číslo řádku ve zdrojovém souboru.
 * @return Nový uzel if-výrazu nebo NULL v případě chyby.
 */
ASTNode *ast_create_if_expr(ASTNode *cond, ASTNode *then_blk, ASTNode *else_blk, int line);

/**
 * @brief Vytvoří uzel match-výrazu (AST_MATCH_EXPR).
 * @param expr Výraz, který se má porovnávat.
 * @param cases Seznam větví (AST_CASE_BRANCH) přes next.
 * @param line Číslo řádku ve zdrojovém souboru.
 * @return Nový uzel match-výrazu nebo NULL v případě chyby.
 */
ASTNode *ast_create_match(ASTNode *expr, ASTNode *cases, int line);

/**
 * @brief Vytvoří uzel lambda-výrazu (AST_LAMBDA).
 * @param params Parametry lambda-výrazu.
 * @param body Tělo lambda-výrazu.
 * @param line Číslo řádku ve zdrojovém souboru.
 * @return Nový uzel lambda-výrazu nebo NULL v případě chyby.
 */
ASTNode *ast_create_lambda(ASTNode *params, ASTNode *body, int line);

/**
 * @brief Vytvoří uzel celočíselného literálu (AST_LITERAL).
 * @param value Hodnota celočíselného literálu.
 * @param line Číslo řádku ve zdrojovém souboru.
 * @return Nový uzel celočíselného literálu nebo NULL v případě chyby.
 */
ASTNode *ast_create_int_lit(long long value, int line);

/**
 * @brief Vytvoří uzel desetinného literálu (AST_LITERAL).
 * @param value Hodnota desetinného literálu.
 * @param line Číslo řádku ve zdrojovém souboru.
 * @return Nový uzel desetinného literálu nebo NULL v případě chyby.
 */
ASTNode *ast_create_double_lit(double value, int line);

/**
 * @brief Vytvoří uzel řetězcového literálu (AST_LITERAL).
 * @param value Hodnota řetězcového literálu. Dělá kopii, je to již dekódováno.
 * @param line Číslo řádku ve zdrojovém souboru.
 * @return Nový uzel řetězcového literálu nebo NULL v případě chyby.
 */
ASTNode *ast_create_string_lit(const char *value, int line);

/**
 * @brief Vytvoří uzel logického literálu (AST_LITERAL).
 * @param value Hodnota logického literálu (true nebo false).
 * @param line Číslo řádku ve zdrojovém souboru.
 * @return Nový uzel logického literálu nebo NULL v případě chyby.
 */
ASTNode *ast_create_bool_lit(bool value, int line);

/**
 * @brief Vytvoří uzel jednotkového literálu () (AST_LITERAL).
 * @param line Číslo řádku ve zdrojovém souboru.
 * @return Nový uzel jednotkového literálu nebo NULL v případě chyby.
 */
ASTNode *ast_create_unit_lit(int line);

/**
 * @brief Vytvoří uzel programu (AST_PROGRAM).
 * @param definitions Definice nejvyšší úrovně přes next (FUNC_DEF, CLASS_DEF, TYPE_ALIAS,
 * EXTENSION_DEF); NULL = prázdný program.
 * @param line Číslo řádku ve zdrojovém souboru.
 * @return Nový uzel programu nebo NULL v případě chyby.
 */
ASTNode *ast_create_program(ASTNode *definitions, int line);

/**
 * @brief Vytvoří uzel bloku (AST_BLOCK).
 * @param statements Seznam příkazů v bloku.
 * @param line Číslo řádku ve zdrojovém souboru.
 * @return Nový uzel bloku nebo NULL v případě chyby.
 */
ASTNode *ast_create_block(ASTNode *statements, int line);

/**
 * @brief Zajistí, že tělo je AST_BLOCK: jeden výraz
 * nebo příkaz na stejném řádku (def f(x: Int) = x* + 5)
 *  zabalí do nového bloku; už hotový AST_BLOCK (odsazené tělo na dalších řádcích) vrátí beze
 * změny.
 * @param stmt Jednořádkové tělo, nebo už hotový AST_BLOCK.
 * @param line Číslo řádku ve zdrojovém souboru.
 * @return Blok, nebo NULL při chybě alokace; pak stmt nepřevezme a volající ho uvolní sám.
 */
ASTNode *ast_wrap_block(ASTNode *stmt, int line);

/**
 * @brief Vytvoří uzel přiřazení (AST_ASSIGNMENT).
 * @param target Cíl přiřazení (jméno proměnné). Kopíruje se.
 * @param value Hodnota, která se má přiřadit.
 * @param line Číslo řádku ve zdrojovém souboru.
 * @return Nový uzel přiřazení nebo NULL v případě chyby.
 */
ASTNode *ast_create_assignment(const char *target, ASTNode *value, int line);

/**
 * @brief Vytvoří uzel while-cyklu (AST_WHILE).
 * @param condition Podmínka cyklu.
 * @param body Tělo cyklu (AST_BLOCK).
 * @param line Číslo řádku ve zdrojovém souboru.
 * @return Nový uzel while-cyklu nebo NULL v případě chyby.
 */
ASTNode *ast_create_while(ASTNode *condition, ASTNode *body, int line);

/**
 * @brief Vytvoří uzel návratového příkazu (AST_RETURN).
 * @param value Hodnota, která se má vrátit, NULL = samostatné return (vrací ()).
 * @param line Číslo řádku ve zdrojovém souboru.
 * @return Nový uzel návratového příkazu nebo NULL v případě chyby.
 */
ASTNode *ast_create_return(ASTNode *value, int line);

/**
 * @brief Vytvoří uzel unární operace (AST_UNARY_OP).
 * @param op_token Číslo operátoru z Tokenu (-, +, !).
 * @param operand Operand unární operace.
 * @param line Číslo řádku ve zdrojovém souboru.
 * @return Nový uzel unární operace nebo NULL v případě chyby.
 */
ASTNode *ast_create_unary_op(int op_token, ASTNode *operand, int line);

/**
 * @brief Vytvoří uzel definice třídy (AST_CLASS_DEF).
 * @param name Jméno třídy. Kopíruje se.
 * @param fields Atributy: seznam AST_VAR_DEF přes next.
 * @param line Číslo řádku ve zdrojovém souboru.
 * @return Nový uzel definice třídy nebo NULL v případě chyby.
 */
ASTNode *ast_create_class_def(const char *name, ASTNode *fields, int line);

/**
 * @brief Vytvoří uzel aliasu typu (AST_TYPE_ALIAS).
 * @param name Jméno aliasu.
 * @param aliased Typ, na který se alias vztahuje.
 * @param line Číslo řádku ve zdrojovém souboru.
 * @return Nový uzel aliasu typu nebo NULL v případě chyby.
 */
ASTNode *ast_create_type_alias(const char *name, TypeInfo *aliased, int line);

/**
 * @brief Vytvoří uzel definice rozšíření (AST_EXTENSION_DEF).
 * @param receiver Příjemce rozšíření.
 * @param methods Metody: seznam AST_FUNC_DEF přes next.
 * @param line Číslo řádku ve zdrojovém souboru.
 * @return Nový uzel definice rozšíření nebo NULL v případě chyby.
 */
ASTNode *ast_create_extension_def(ASTNode *receiver, ASTNode *methods, int line);

/**
 * @brief Vytvoří uzel rozsahu (AST_RANGE).
 * @param start Dolní mez rozsahu.
 * @param end Horní mez rozsahu.
 * @param step Krok rozsahu, NULL = krok 1.
 * @param is_until True = horní mez se nezahrnuje, false = horní mez se zahrnuje (Byl použit
 * until?).
 * @param line Číslo řádku ve zdrojovém souboru.
 * @return Nový uzel rozsahu nebo NULL v případě chyby.
 */
ASTNode *ast_create_range(ASTNode *start, ASTNode *end, ASTNode *step, bool is_until, int line);

/**
 * @brief Vytvoří uzel definice funkce (AST_FUNC_DEF).
 * @param name Jméno funkce.
 * @param is_main True = @main def ..., false = běžná funkce.
 * @param param_groups Seznam AST_PARAM_LIST přes next; NULL = bez závorek (def answer),
 *                     jeden PARAM_LIST bez položek = prázdné závorky (def answer()).
 * @param return_type Napsaný návratový typ; NULL = nenapsán.
 * @param body Tělo funkce (AST_BLOCK).
 * @param line Číslo řádku ve zdrojovém souboru.
 * @return Nový uzel definice funkce nebo NULL v případě chyby.
 */
ASTNode *ast_create_func_def(const char *name, bool is_main, ASTNode *param_groups,
                             TypeInfo *return_type, ASTNode *body, int line);

/**
 * @brief Vytvoří uzel deklarace (AST_VAR_DEF): proměnná val/var, parametr, řídicí proměnná for,
 *        atribut třídy, proměnná ve vzoru Point(a, b) nebo příjemce extension.
 * @param identifier Jméno (povinné, NULL = funkce vrátí NULL). `_` ve vzoru Point(_, y) není
 *                   VAR_DEF, ale vzor ast_create_pattern_wildcard.
 * @param is_mutable True = var, false = val i všechna ostatní použití.
 * @param declared_type Napsaný typ; NULL = nenapsán.
 * @param initializer Výraz za = u val/var (povinný); u ostatních použití NULL.
 * @param line Číslo řádku ve zdrojovém souboru.
 * @return Nový uzel, nebo NULL při chybě alokace.
 */
ASTNode *ast_create_var_def(const char *identifier, bool is_mutable, TypeInfo *declared_type,
                            ASTNode *initializer, int line);
/**
 * @brief Vytvoří uzel cyklu for (AST_FOR).
 * @param iter_var Řídicí proměnná (AST_VAR_DEF) s is_mutable = false.
 * @param source Zdroj generátoru (AST_RANGE nebo výraz typu List[T]).
 * @param guards Výrazy stráží přes next; NULL = bez stráží.
 * @param body Tělo cyklu (AST_BLOCK).
 * @param line Číslo řádku ve zdrojovém souboru.
 * @return Nový uzel cyklu for nebo NULL v případě chyby.
 */
ASTNode *ast_create_for(ASTNode *iter_var, ASTNode *source, ASTNode *guards, ASTNode *body,
                        int line);

/**
 * @brief Vytvoří uzel identifikátoru (AST_IDENTIFIER).
 * @param name Jméno (proměnná, parametr, funkce, třída); i "List.range" jako jeden identifikátor.
 * @param line Číslo řádku ve zdrojovém souboru.
 * @return Nový uzel identifikátoru nebo NULL v případě chyby.
 */
ASTNode *ast_create_identifier(const char *name, int line);

/**
 * @brief Vytvoří uzel přístupu k členu (AST_MEMBER_ACCESS): p.x, "abc".length, x.plus.
 * @param object Příjemce: výraz vlevo od tečky.
 * @param member_name Jméno vpravo od tečky.
 * @param line Číslo řádku ve zdrojovém souboru.
 * @return Nový uzel přístupu k členu nebo NULL v případě chyby.
 */
ASTNode *ast_create_member_access(ASTNode *object, const char *member_name, int line);

/**
 * @brief Vytvoří uzel větve case (AST_CASE_BRANCH).
 * @param pattern Vzor pro porovnání.
 * @param guard Volitelná if stráž; NULL = bez stráže.
 * @param body Tělo větve (AST_BLOCK).
 * @param line Číslo řádku ve zdrojovém souboru.
 * @return Nový uzel větve case nebo NULL v případě chyby.
 */
ASTNode *ast_create_case_branch(ASTNode *pattern, ASTNode *guard, ASTNode *body, int line);

/**
 * @brief Vytvoří vzor literálu (AST_PATTERN, PATTERN_LITERAL): case 0 =>.
 * @param literal Uzel AST_LITERAL, i pro záporné číslo (case -1 parser složí do jednoho literálu).
 * @param line Číslo řádku ve zdrojovém souboru.
 * @return Nový uzel literálu vzoru nebo NULL v případě chyby.
 */
ASTNode *ast_create_pattern_literal(ASTNode *literal, int line);

/**
 * @brief Vytvoří uzel vázání typu vzoru (AST_PATTERN).
 * @param name Jméno proměnné, do které se váže hodnota.
 * @param type_bind Typ podle zápisu (může být alias).
 * @param line Číslo řádku ve zdrojovém souboru.
 * @return Nový uzel vázání typu vzoru nebo NULL v případě chyby.
 */
ASTNode *ast_create_pattern_binding(const char *name, TypeInfo *type_bind, int line);

/**
 * @brief Vytvoří uzel konstruktoru vzoru (AST_PATTERN).
 * @param class_type Typ třídy podle zápisu (TYPE_CUSTOM).
 * @param args Argumenty přes next: AST_VAR_DEF (nová proměnná), nebo AST_PATTERN s literálem
 *             či _ (ast_create_pattern_literal, ast_create_pattern_wildcard); NULL = bez argumentů.
 * @param line Číslo řádku ve zdrojovém souboru.
 * @return Nový uzel konstruktoru vzoru nebo NULL v případě chyby.
 */
ASTNode *ast_create_pattern_constructor(TypeInfo *class_type, ASTNode *args, int line);

/**
 * @brief Vytvoří uzel zástupného znaku vzoru (AST_PATTERN).
 * @param line Číslo řádku ve zdrojovém souboru.
 * @return Nový uzel zástupného znaku vzoru nebo NULL v případě chyby.
 */
ASTNode *ast_create_pattern_wildcard(int line);

/**
 * @brief Vytvoří uzel seznamu parametrů (AST_PARAM_LIST).
 * @param params Seznam parametrů (AST_VAR_DEF) přes next; NULL = prázdný seznam ().
 * @param line Číslo řádku ve zdrojovém souboru.
 * @return Nový uzel seznamu parametrů nebo NULL v případě chyby.
 */
ASTNode *ast_create_param_list(ASTNode *params, int line);

/**
 * @brief Vytvoří uzel literálu seznamu (AST_LIST_LITERAL).
 * @param items Seznam prvků seznamu přes next; NULL = prázdný seznam.
 * @param line Číslo řádku ve zdrojovém souboru.
 * @return Nový uzel literálu seznamu nebo NULL v případě chyby.
 * @note List.range(...) není literál: parser vytvoří FUNC_CALL s cílem IDENTIFIER "List.range".
 */
ASTNode *ast_create_list_literal(ASTNode *items, int line);

/**
 * @brief Vytvoří uzel seznamu argumentů (AST_ARG_LIST).
 * @param args Argumenty (výrazy) přes next; NULL = prázdné závorky ().
 * @param line Číslo řádku ve zdrojovém souboru.
 * @return Nový uzel seznamu argumentů nebo NULL v případě chyby.
 */
ASTNode *ast_create_arg_list(ASTNode *args, int line);

/* ========================================================================== *
 *  Typy (TypeInfo) – Konstruktory
 * ========================================================================== */

/**
 * @brief Vytvoří jednoduchý typ bez dalších dat.
 * @param type_class TYPE_UNIT, TYPE_INT, TYPE_DOUBLE, TYPE_STRING, TYPE_BOOLEAN, TYPE_ANY nebo
 * TYPE_NOTHING; jiná hodnota je chyba volajícího a funkce vrátí NULL.
 * @return Nový TypeInfo pro jednoduchý typ nebo NULL v případě chyby.
 */
TypeInfo *type_create_basic(TypeClass type_class);

/**
 * @brief Vytvoří sjednocení left | right; delší sjednocení skládá parser zleva: (Int | Double) |
 * String.
 * @param left Levý typ.
 * @param right Pravý typ.
 * @return Nový TypeInfo pro sjednocení typů nebo NULL v případě chyby nebo NULL parametru.
 */
TypeInfo *type_create_union(TypeInfo *left, TypeInfo *right);

/**
 * @brief Vytvoří TypeInfo pro funkční typ.
 * @param param_types Seznam typů parametrů přes next; NULL = prázdný seznam ().
 * @param return_type Návratový typ funkce.
 * @return Nový TypeInfo pro funkční typ nebo NULL v případě chyby.
 */
TypeInfo *type_create_function(TypeInfo *param_types, TypeInfo *return_type);

/**
 * @brief Vytvoří TypeInfo pro seznam.
 * @param element_type Typ prvku seznamu.
 * @return Nový TypeInfo pro seznam nebo NULL v případě chyby.
 */
TypeInfo *type_create_list(TypeInfo *element_type);

/**
 * @brief Vytvoří TypeInfo pro vlastní typ (třídu nebo alias).
 * @param name Jméno třídy nebo aliasu.
 * @return Nový TypeInfo pro vlastní typ nebo NULL v případě chyby.
 */
TypeInfo *type_create_custom(const char *name);

/**
 * @brief Připojí item na konec řetězu typů parametrů (přes next), obdoba ast_append.
 * @param head Hlava seznamu TypeInfo.
 * @param item TypeInfo, který se má připojit na konec seznamu.
 * @return Hlava řetězu; při head == NULL vrátí item. Nic nealokuje, nemůže selhat.
 */
TypeInfo *type_append(TypeInfo *head, TypeInfo *item);

/**
 * @brief Připojí item na konec seznamu ASTNode (přes next).
 * @param head Hlava seznamu ASTNode.
 * @param item ASTNode, který se má připojit na konec seznamu.
 * @return Hlava řetězu; při head == NULL vrátí item.
 * @note Nic nealokuje, nemůže selhat.
 */
ASTNode *ast_append(ASTNode *head, ASTNode *item);

/**
 * @brief Sémantika: zabalí uzel na místě *slot (answer, x.twice) do nového AST_FUNC_CALL.
 * @param slot Ukazatel na místo ve stromu, kde uzel leží. Sloty počítat za průchodu z aktuálního
 * stromu, NIKDY je neukládat dopředu (přepis strom mění).
 * @return 0 při úspěchu, 99 při chybě alokace (strom zůstane beze změny).
 * @note Nový uzel: target = původní uzel, arg_groups = NULL, own_groups = 0, call_kind =
 * CALL_UNRESOLVED, expr_type = NULL, to_double = false; převezme next starého, starý dostane next =
 * NULL. Volat dřív, než se zabalený uzel otypuje.
 *
 */
int ast_wrap_in_call(ASTNode **slot);

/**
 * @brief Nastaví expr_type uzlu na owned a starý expr_type uvolní.
 * @param node Uzel, jehož expr_type se nastavuje.
 * @param owned Nový typ; funkce ho převezme (předávej vlastní kopii, např. z type_info_clone).
 * @note Jediný povolený zápis expr_type; přímé přiřazení by starý typ ztratilo (únik paměti).
 */
void ast_set_type(ASTNode *node, TypeInfo *owned);
/* ========================================================================== *
 * Destruktory a kopie
 * ========================================================================== */

/**
 * @brief Uvolní TypeInfo, jeho podstromy a CELÝ řetěz next (parametry funkčního typu).
 * @param root Kořen typu, který se má uvolnit. NULL nic nedělá.
 * @warning Typ uprostřed řetězu parametrů uvolní i se všemi dalšími; jeden prvek nejdřív odpoj
 * (next = NULL).
 */
void type_info_free(TypeInfo *root);

/**
 * @brief Hluboká kopie typu.
 * @param src Zdrojový typ, který se má zkopírovat.
 * @return Kopie typu, nebo NULL při chybě alokace (pak nic nealokuje).
 * @note Výsledek má next == NULL: kopíruje se jen tento typ, ne zbytek řetězu parametrů.
 */
TypeInfo *type_info_clone(const TypeInfo *src);

/**
 * @brief Nahradí typ na místě *slot typem owned a starý uvolní (např. rozvinutí aliasu uvnitř
 * typu).
 * @param slot Ukazatel na místo, kde se má typ nahradit.
 * @param owned Nový typ; funkce ho převezme (předávej vlastní kopii, např. z type_info_clone).
 * @note Zachová řetěz parametrů: owned->next = (*slot)->next, starý se před uvolněním odpojí.
 *       Používá se jen na kopiích v expr_type, nikdy na declared_type ani return_type.
 *      Pro slot == NULL neudělí nic a jen uvolní owned
 *      Pro owned == NULL neudělá nic
 *      Pro *slot == NULL přenese owned na místo *slot a nic neuvolní
 */
void type_replace(TypeInfo **slot, TypeInfo *owned);

/**
 * @brief Uvolní uzel, jeho podstromy, expr_type a CELÝ řetěz next. Přes next iterativně
 *        (dlouhý blok nezvětšuje zásobník), do potomků rekurzivně.
 * @param root Kořen stromu, který se má uvolnit. NULL nic nedělá.
 * @warning Uzel uprostřed seznamu uvolní i se všemi dalšími; jeden uzel nejdřív odpoj (next =
 * NULL).
 */
void ast_free(ASTNode *root);

/* ========================================================================== *
 *  LADĚNÍ A TESTY (AI)
 * ========================================================================== */
/**
 * @brief Ladicí kontrola stromu (testy a ladicí překlad); nic nemění ani neuvolňuje.
 * @param root Kořen stromu.
 * @param after_semantics false = volá se po parseru, true = po sémantice.
 * @return Počet porušení (každé vypíše na stderr s řádkem uzlu); 0 = strom v pořádku.
 * @details Vždy: těla jsou AST_BLOCK, blok není prázdný, v jednoduchém slotu není next, žádný uzel
 *          ani typ není sdílený, výčty jsou platné, napsané typy neobsahují TYPE_ANY ani
 * TYPE_NOTHING. Po parseru: anotace jsou ve stavu po konstruktoru (parser je nevyplňuje). Po
 * sémantice: expr_type všude kromě výjimek u expr_type; var_id != 0 u VAR_DEF a IDENTIFIER, > 0 u
 * ASSIGNMENT a vzoru n: T; own_groups od 0 do počtu skupin; call_kind a member_kind určené;
 * to_double jen u uzlu typu Int; MEMBER_EXTENSION jen jako cíl FUNC_CALL.
 */
int ast_check(const ASTNode *root, bool after_semantics);

/**
 * @brief Textový výpis stromu pro zlaté testy parseru; nic nemění.
 * @param root Kořen stromu; NULL nevypíše nic.
 * @param out Kam vypisovat (např. stdout nebo otevřený soubor).
 * @details Jeden uzel na řádek, odsazení 2 mezery na úroveň: pole u rodiče (např. else:), druh uzlu
 *          bez AST_, data uzlu (jména, literály, operátor, napsané typy jako v IFJ26) a v []
 * anotace, které nejsou ve stavu po konstruktoru.
 * @warning Formát je pevný; jeho změna znamená přegenerovat všechny zlaté testy.
 */
void ast_dump(const ASTNode *root, FILE *out);
#endif // AST_H
