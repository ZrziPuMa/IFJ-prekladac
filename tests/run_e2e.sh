#!/usr/bin/env bash
# End-to-end testy překladače IFJ26: program.ifj -> ./prekladac -> ic26int -> porovnání.
#
# Použití (z kořene repozitáře nebo přes make e2e):
#   tests/run_e2e.sh          všechny testy
#   tests/run_e2e.sh sem_     jen testy, jejichž jméno obsahuje "sem_"
# Proměnné prostředí:
#   COMPILER=./prekladac  IC26INT=ic26int  TIMEOUT=5  VALGRIND=1  E2E_OUT=/tmp/ifj26_e2e
#
# Test = soubor tests/<jméno>.ifj, bez podsložek. Očekávání určuje první řádek:
#   // expect: N        překladač skončí kódem N (1-7, 10, 99), nic se nespouští
#   // expect: run N    překlad projde (0), interpret skončí kódem N (40, 5x)
#   bez hlavičky        překlad i interpretace skončí kódem 0
# Volitelné soubory vedle testu:
#   tests/<jméno>.in    standardní vstup programu (jinak prázdný)
#   tests/<jméno>.out   očekávaný standardní výstup (porovnává se přesně)
# Výstupy posledního běhu se ukládají mimo repozitář: $E2E_OUT/<jméno>.{code,stdout,stderr}
#
# [AI] Claude (Opus 5.5), prompt: "Vytvoř mi shell skript, který spustí e2e testy pro překladač IFJ26"

set -u
cd "$(dirname "$0")/.." || exit 2

COMPILER=${COMPILER:-./prekladac}
IC26INT=${IC26INT:-ic26int}
TIMEOUT=${TIMEOUT:-5}
OUT=${E2E_OUT:-/tmp/ifj26_e2e}
FILTER=${1:-}
CASES=tests

if [[ ! -x $COMPILER ]]; then
    echo "Chybí překladač $COMPILER (spusťte make)." >&2
    exit 2
fi
if ! command -v "$IC26INT" >/dev/null 2>&1; then
    echo "Chybí interpret $IC26INT (nastavte IC26INT=/cesta/ic26int)." >&2
    exit 2
fi
mkdir -p "$OUT" || exit 2

describe_rc() {
    case $1 in
        124) echo "timeout po ${TIMEOUT} s" ;;
        125) echo "valgrind našel chybu paměti" ;;
        13[0-9] | 14[0-9]) echo "pád (signál $(($1 - 128)))" ;;
        *) echo "$1" ;;
    esac
}

shopt -s nullglob
pass=0
fail=0
for src in "$CASES"/*.ifj; do
    name=$(basename "$src" .ifj)
    [[ $name == *"$FILTER"* ]] || continue
    base=${src%.ifj}
    res=$OUT/$name

    want_comp=0
    want_run=0
    header=$(head -n 1 "$src")
    if [[ $header =~ ^//\ *expect:\ *run\ +([0-9]+) ]]; then
        want_run=${BASH_REMATCH[1]}
    elif [[ $header =~ ^//\ *expect:\ *([0-9]+) ]]; then
        want_comp=${BASH_REMATCH[1]}
    fi

    # 1. Překlad
    # Subshell s "exit $?" a 2>/dev/null skryje hlášky bashe typu "Aborted (core dumped)".
    if [[ ${VALGRIND:-0} == 1 ]]; then
        (timeout $((TIMEOUT * 20)) valgrind -q --leak-check=full \
            --errors-for-leak-kinds=definite,indirect --error-exitcode=125 \
            "$COMPILER" <"$src" >"$res.code" 2>"$res.stderr"; exit $?) 2>/dev/null
    else
        (timeout "$TIMEOUT" "$COMPILER" <"$src" >"$res.code" 2>"$res.stderr"; exit $?) 2>/dev/null
    fi
    rc=$?
    : >"$res.stdout"

    msg=""
    if [[ $rc -ne $want_comp ]]; then
        msg="překladač vrátil $(describe_rc "$rc"), čekáno $want_comp"
    elif [[ $want_comp -eq 0 ]]; then
        # 2. Interpretace vygenerovaného kódu
        input=/dev/null
        [[ -f $base.in ]] && input=$base.in
        (timeout "$TIMEOUT" "$IC26INT" "$res.code" <"$input" >"$res.stdout" 2>>"$res.stderr"; exit $?) 2>/dev/null
        rrc=$?
        if [[ $rrc -ne $want_run ]]; then
            msg="interpret vrátil $(describe_rc "$rrc"), čekáno $want_run"
        elif [[ -f $base.out ]] && ! cmp -s "$base.out" "$res.stdout"; then
            msg="výstup se liší od $base.out"
        fi
    fi

    if [[ -z $msg ]]; then
        pass=$((pass + 1))
        echo "OK    $name"
    else
        fail=$((fail + 1))
        echo "FAIL  $name: $msg"
        if [[ $msg == výstup* ]]; then
            diff "$base.out" "$res.stdout" | head -n 8 | sed 's/^/        /'
        else
            head -n 3 "$res.stderr" | sed 's/^/        /'
        fi
    fi
done

echo
if [[ $((pass + fail)) -eq 0 ]]; then
    echo "Žádný test: v $CASES/ není soubor *.ifj${FILTER:+ se jménem obsahujícím \"$FILTER\"}."
    exit 1
fi
echo "Prošlo $pass, neprošlo $fail."
[[ $fail -eq 0 ]] || echo "Výstupy posledního běhu: $OUT/<jméno>.{code,stdout,stderr}"
[[ $fail -eq 0 ]]