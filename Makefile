# Autor: Martin Chlubna (xchlubm00)

# UPOZORNĚNÍ NA VYUŽITÍ AI
# Tento Makefile byl rozšířen a doplněn pomocí nástroje umělé inteligence (Gemini).
#
# Použité prompty:
# 1. "Chtel bych Makefile, ve kterem budou 3 hlavni cile: build (vychozi, kterym
#    budou prekladat ucitele), check (provede nasledujici kontroly: clang-tidy,
#    cpp-check, clang-format check,) a nakonec build-check, ktery nejdriv udela
#    check a potom build. nasledne bych pak chtel jeste udelat dalsi cile jako
#    memcheck, ktery projede program valgrindem a pak jeste server-test, ktery
#    otestuje kod na skolnim serveru. muzu ti poslat muj aktualni Makefile a ze
#    ty bys tam tyto veci doplnil?"
# 2. "Nahral jsem ti i zadani projektu i Makefile. dopln ho tedy a nezapomen se
#    na zacatku Makefile zminit, ze to bylo doplneno pomoci AI"
# 3. "pomoz mi tedy upravit ten makefile pro tento setup"
# ==============================================================================

# --- Základní nastavení překladu ---
CC = gcc
CFLAGS = -Wall -Wextra -Werror -pedantic -std=c11 -g
# -Werror raději odstranit před odevzdáním

DEPFLAGS = -MMD -MP
TARGET = prekladac
# Sem přidejte jakýkoli .c soubor, co nově přidáte:
SRCS = main.c 
# Automatické načtení hlavičkových souborů pro formátování a statickou analýzu:
HDRS = $(wildcard *.h)

OBJS = $(SRCS:.c=.o)
DEPS = $(SRCS:.c=.d)

# ==============================================================================
# NASTAVENÍ NÁSTROJŮ PRO KONTROLU, VALGRIND A ŠKOLNÍ SERVER (ZDE UPRAVTE)
# ==============================================================================

# Styl pro clang-format (povinný dle sekce 2.5 zadání: např. Google, LLVM, nebo file)
FORMAT_STYLE = file

# Vstupní soubor pro Valgrind (překladač čte ze stdin):
TEST_INPUT = test.ifj26

# Nastavení pro testování na školním serveru Merlin:
FIT_LOGIN = xlogin00
SERVER_HOST = merlin.fit.vutbr.cz
REMOTE_DIR = ~/ifj26_test_build

# ==============================================================================
# 1. HLAVNÍ CÍLE PRO PŘEKLAD (výchozí cíl 'build' pro vyučující a Merlin)
# ==============================================================================

.PHONY: build all check check-format check-tidy check-cppcheck build-check \
        format memcheck server-test clean

# Výchozí cíl při spuštění samotného příkazu 'make'
build: $(TARGET)

all: build

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@

%.o: %.c
	$(CC) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

# ==============================================================================
# 2. KONTROLY KVALITY KÓDU (pro lokální vývoj a CI/CD)
# ==============================================================================
# Poznámka: Tyto cíle jsou oddělené od 'build', aby překlad nezkolaboval na
# stroji, kde tyto nástroje nejsou nainstalované (viz sekce 2.5 a 2.7 zadání).

check: check-format check-tidy check-cppcheck
	@echo "KONEC KONTROL"

# Kontrola formátování (nic nepřepisuje, pouze vrátí chybu při špatném formátu)
check-format:
	@echo "--> Spouštím kontrolu formátování (clang-format)..."
	clang-format --style=$(FORMAT_STYLE) --dry-run --Werror $(SRCS) $(HDRS)

# Statická analýza pomocí clang-tidy (využívá soubor .clang-tidy v repozitáři)
check-tidy:
	@echo "--> Spouštím statickou analýzu (clang-tidy)..."
	-clang-tidy $(SRCS) $(HDRS) -- $(CFLAGS)

# Statická analýza pomocí cppcheck
check-cppcheck:
	@echo "--> Spouštím statickou analýzu (cppcheck)..."
	-cppcheck --enable=warning,style,performance,portability \
	         --suppress=missingIncludeSystem \
	         --std=c11 $(SRCS) $(HDRS)

# Pomocný cíl: Automaticky naformátuje všechny .c a .h soubory
format:
	@echo "--> Formátuji zdrojové soubory pomocí clang-format..."
	clang-format --style=$(FORMAT_STYLE) -i $(SRCS) $(HDRS)

# ==============================================================================
# 3. KOMBINOVANÝ CÍL PRO VÝVOJÁŘE (nejdříve check, poté build)
# ==============================================================================
# Sekvenční volání $(MAKE) zaručí, že se 'check' provede striktně před 'build'
# i v případě, že někdo spustí paralelní překlad (např. make -j4 build-check).

build-check:
	$(MAKE) check
	$(MAKE) build

# ==============================================================================
# 4. KONTROLA PAMĚTI (Valgrind) A TESTOVÁNÍ NA SERVERU MERLIN
# ==============================================================================

# Spustí překladač pod Valgrindem. Jelikož je překladač filtr (čte ze stdin),
# přesměruje do něj soubor $(TEST_INPUT), nebo prázdný vstup, pokud soubor neexistuje.
memcheck: build
	@echo "--> Spouštím kontrolu paměti (Valgrind)..."
	@if [ -f "$(TEST_INPUT)" ]; then \
		valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes \
		         --error-exitcode=99 ./$(TARGET) < "$(TEST_INPUT)" > /dev/null; \
	else \
		echo "Upozornění: Soubor $(TEST_INPUT) nenalezen, testuji s prázdným vstupem."; \
		valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes \
		         --error-exitcode=99 ./$(TARGET) < /dev/null > /dev/null; \
	fi
	@echo "✅ Valgrind nezjistil žádné úniky paměti ani chyby!"

# Nahraje zdrojové kódy na školní server Merlin, přeloží projekt výchozím cílem
# 'build' a ověří, že výsledná binárka jde spustit.
server-test:
	@if [ "$(FIT_LOGIN)" = "xlogin00" ]; then \
		echo "❌ CHYBA: Nejprve nastavte svůj FIT_LOGIN (např. make server-test FIT_LOGIN=xnovak00)"; \
		exit 1; \
	fi
	@echo "--> Kopíruji soubory na $(FIT_LOGIN)@$(SERVER_HOST):$(REMOTE_DIR)..."
	ssh $(FIT_LOGIN)@$(SERVER_HOST) "mkdir -p $(REMOTE_DIR)"
	scp Makefile $(SRCS) $(HDRS) $(FIT_LOGIN)@$(SERVER_HOST):$(REMOTE_DIR)/
	@echo "--> Spouštím čistý překlad a test na serveru Merlin..."
	ssh $(FIT_LOGIN)@$(SERVER_HOST) "cd $(REMOTE_DIR) && make clean && make build && ./$(TARGET) < /dev/null; echo 'Návratový kód překladače:' \$$?"
	@echo "✅ Test překladu na serveru Merlin dokončen!"

# ==============================================================================
# 5. ÚKLID PŘELOŽENÝCH SOUBORŮ
# ==============================================================================
# Důležité před zabalením odevzdávaného archivu (sekce 6 zadání zakazuje binárky).

clean:
	rm -f $(OBJS) $(DEPS) $(TARGET)

-include $(DEPS)