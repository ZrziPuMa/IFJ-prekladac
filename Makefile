# file: Makefile
# brief: Soubor s direktivy pro překlad a spuštění nástrojů pro vývoj
# author: Martin Chlubna (<xchlubm00>)

# UPOZORNĚNÍ NA VYUŽITÍ AI
# Tento Makefile byl navržen, strukturován a optimalizován za pomoci nástroje 
# umělé inteligence (Google Gemini). Vzhledem k iterativnímu vývoji a velkému 
# množství dílčích promptů (odladění závislostí, testů a Valgrindu) nejsou 
# konkrétní dotazy v komentářích detailně vypisovány.
# ==============================================================================

# Explicitní určení výchozího cíle (zaručí spuštění 'build' i při změně pořadí cílů)
.DEFAULT_GOAL := build

# --- Základní nastavení překladu ---
CC = gcc
CFLAGS = -Wall -Wextra -pedantic -std=c11 -g
# Poznámka: -Werror doporučujeme odstranit před finálním odevzdáním

DEPFLAGS = -MMD -MP
TARGET = prekladac

# Automatické načtení všech .c a .h souborů v projektu (není nutné nic ručně dopisovat)
SRCS = $(wildcard *.c)
HDRS = $(wildcard *.h)

OBJS = $(SRCS:.c=.o)
DEPS = $(SRCS:.c=.d)

# ==============================================================================
# NASTAVENÍ NÁSTROJŮ PRO KONTROLU, VALGRIND A ŠKOLNÍ SERVER
# ==============================================================================

FORMAT_STYLE = file

# Nastavení pro testování na školním serveru Merlin:
FIT_LOGIN = xlogin00
SERVER_HOST = merlin.fit.vutbr.cz
REMOTE_DIR = ifj26_test_build
REMOTE = $(FIT_LOGIN)@$(SERVER_HOST)

# Společné přepínače pro Valgrind:
VALGRIND = valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --error-exitcode=99

# Vstup pro Valgrind a Merlin: pokud existuje TEST_INPUT, použije se, jinak /dev/null
MEM_INPUT = $(if $(wildcard $(TEST_INPUT)),$(TEST_INPUT),/dev/null)

# ==============================================================================
# 1. HLAVNÍ CÍLE PRO PŘEKLAD (výchozí cíl 'build' pro vyučující a Merlin)
# ==============================================================================

.PHONY: build all check check-format check-tidy check-cppcheck build-check \
        format memcheck server-test test clean help pack zip

build: $(TARGET)

all: build

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@

%.o: %.c
	$(CC) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

# ==============================================================================
# 2. KONTROLY KVALITY KÓDU (pro lokální vývoj a CI/CD)
# ==============================================================================
# Volitelně lze předat soubor: make check FILE=stack.c
CHECK_FILES ?= $(if $(FILE),$(FILE),$(SRCS) $(HDRS))
CHECK_C_SRCS = $(filter %.c,$(CHECK_FILES))

check: check-format check-tidy check-cppcheck
	@echo "✅ Všechny kontroly kvality kódu dokončeny!"

# Kontrola formátování (pouze ověřuje, nic nepřepisuje)
check-format:
	@if [ -n "$(CHECK_FILES)" ]; then \
		echo "--> Kontrola formátování (clang-format) pro: $(CHECK_FILES)..."; \
		clang-format --style=$(FORMAT_STYLE) --dry-run --Werror $(CHECK_FILES); \
	fi

# Statická analýza clang-tidy (spouští se pouze na .c soubory, hlavičky analyzuje přes include)
check-tidy:
	@if [ -n "$(CHECK_C_SRCS)" ]; then \
		echo "--> Statická analýza (clang-tidy) pro: $(CHECK_C_SRCS)..."; \
		clang-tidy $(CHECK_C_SRCS) -- $(CFLAGS); \
	else \
		echo "--> Přeskakuji clang-tidy (žádné .c soubory k analýze)."; \
	fi

# Statická analýza cppcheck
check-cppcheck:
	@if [ -n "$(CHECK_FILES)" ]; then \
		echo "--> Statická analýza (cppcheck) pro: $(CHECK_FILES)..."; \
		cppcheck --enable=warning,style,performance,portability \
		         --suppress=missingIncludeSystem \
		         --std=c11 $(CHECK_FILES); \
	fi

# Automatické naformátování zadaného souboru nebo celého projektu
format:
	@if [ -n "$(CHECK_FILES)" ]; then \
		echo "--> Formátuji zdrojové soubory (clang-format): $(CHECK_FILES)..."; \
		clang-format --style=$(FORMAT_STYLE) -i $(CHECK_FILES); \
	fi

# ==============================================================================
# 3. KOMBINOVANÝ CÍL PRO VÝVOJÁŘE (nejdříve check, poté build)
# ==============================================================================
build-check:
	$(MAKE) check
	$(MAKE) build

# ==============================================================================
# 4. KONTROLA PAMĚTI CELÉHO PŘEKLADAČE A SERVER MERLIN
# ==============================================================================

memcheck: build
	@echo "--> Valgrind: testuji celý překladač se vstupem '$(MEM_INPUT)'..."
	$(VALGRIND) ./$(TARGET) < $(MEM_INPUT) > /dev/null
	@echo "✅ Valgrind nezjistil žádné úniky paměti v překladači!"

# ==============================================================================
# 5. ZABALENÍ PROJEKTU K ODEVZDÁNÍ (dle sekcí 5 a 6 zadání)
# ==============================================================================
ARCHIVE ?= $(FIT_LOGIN).zip
INCLUDE_TESTS ?= 1
TEST_DIR ?= tests

zip: pack

pack:
	@test "$(FIT_LOGIN)" != "xlogin00" || \
		{ echo "❌ CHYBA: Nastavte FIT_LOGIN vedoucího týmu (např. make pack FIT_LOGIN=xnovak00)"; exit 1; }
	@echo "--> Kontroluji povinné soubory v kořeni projektu..."
	@for f in Makefile dokumentace.pdf rozdeleni rozsireni; do \
		test -f "$$f" || { echo "❌ CHYBA: Chybí povinný soubor '$$f'!"; exit 1; }; \
	done
	@test -n "$(SRCS)" || { echo "❌ CHYBA: Nebyly nalezeny žádné zdrojové soubory (*.c)!"; exit 1; }
	@echo "--> Kontroluji formát 'rozdeleni' a 'rozsireni' (LF konce řádků, součet 100 %)..."
	@for f in rozdeleni rozsireni; do \
		if ! tr -d '\r' < "$$f" | cmp -s - "$$f"; then \
			echo "❌ CHYBA: Soubor '$$f' obsahuje windowsovské konce řádků (CRLF)! Převeďte jej na LF."; \
			exit 1; \
		fi; \
		if [ -s "$$f" ] && [ "$$(tail -c 1 "$$f" | wc -l)" -eq 0 ]; then \
			echo "❌ CHYBA: Soubor '$$f' nekončí znakem nového řádku (LF)!"; \
			exit 1; \
		fi; \
	done
	@awk -F: ' \
		!/^[0-9]+:[a-z0-9]+:[0-9]+$$/ { print "❌ CHYBA: Neplatný formát řádku v rozdeleni: " $$0; exit 1 } \
		{ sum += $$3 } \
		END { if (sum != 100) { printf "❌ CHYBA: Součet procent v rozdeleni je %d %% (musí být 100 %%)!\n", sum; exit 1 } } \
	' rozdeleni
	@echo "--> Sestavuji čistý archiv '$(ARCHIVE)'..."
	@rm -f "$(ARCHIVE)"
	@TMP_DIR=$$(mktemp -d); \
	trap 'rm -rf "$$TMP_DIR"' EXIT; \
	cp Makefile $(SRCS) $(HDRS) dokumentace.pdf rozdeleni rozsireni "$$TMP_DIR/"; \
	if [ "$(INCLUDE_TESTS)" = "1" ] && [ -d "$(TEST_DIR)" ]; then \
		mkdir -p "$$TMP_DIR/$(TEST_DIR)"; \
		find "$(TEST_DIR)" -maxdepth 1 -type f ! -name '.*' ! -name '*.o' ! -name '*.d' \
			! -name '*.exe' ! -name '*.out' ! -name 'bin_test_*' \
			-exec cp {} "$$TMP_DIR/$(TEST_DIR)/" \; ; \
	fi; \
	BAD_FILES=$$(cd "$$TMP_DIR" && find . -mindepth 1 \( -name '.*' -o -name '__MACOSX' \)); \
	if [ -n "$$BAD_FILES" ]; then \
		echo "❌ CHYBA: Archiv obsahuje zakázané skryté soubory nebo __MACOSX:"; \
		echo "$$BAD_FILES"; \
		exit 1; \
	fi; \
	BAD_NAMES=$$(cd "$$TMP_DIR" && find . -mindepth 1 | grep -Ev '^\./[a-zA-Z0-9._/-]+$$'); \
	if [ -n "$$BAD_NAMES" ]; then \
		echo "❌ CHYBA: Nepovolené znaky v názvech souborů/adresářů:"; \
		echo "$$BAD_NAMES"; \
		exit 1; \
	fi; \
	OUT_ABS="$$(pwd)/$(ARCHIVE)"; \
	(cd "$$TMP_DIR" && zip -rq "$$OUT_ABS" .)
	@echo "✅ Archiv '$(ARCHIVE)' je připraven k odevzdání!"
	@if [ -x "./is_it_ok.sh" ]; then \
		echo "--> Spouštím ověřovací skript is_it_ok.sh..."; \
		./is_it_ok.sh "$(ARCHIVE)"; \
	else \
		echo "ℹ️  Tip: Ověřte výsledný archiv skriptem is_it_ok.sh z e-learningu IFJ."; \
	fi

# ==============================================================================
# 6. END TO END TESTY
# ==============================================================================
IC26INT ?= ic26int

# End-to-end testy: make e2e, jen jedna skupina: make e2e FILTER=sem_
e2e: build
	COMPILER=./$(TARGET) IC26INT=$(IC26INT) tests/run_e2e.sh $(FILTER)

# Totéž, ale překladač běží pod valgrindem
e2e-valgrind: build
	VALGRIND=1 COMPILER=./$(TARGET) IC26INT=$(IC26INT) tests/run_e2e.sh $(FILTER)

.PHONY: e2e e2e-valgrind

# ==============================================================================
# 7. ÚKLID PŘELOŽENÝCH SOUBORŮ
# ==============================================================================
clean:
	rm -f $(OBJS) $(DEPS) $(TARGET) $(TEST_BINS)


# ------------------------------------------------------------------------------
# INTERAKTIVNÍ NÁPOVĚDA V TERMINÁLU: make help
# ------------------------------------------------------------------------------
help:
	@echo "======================================================================"
	@echo "             PŘÍRUČKA PRO VÝVOJ PŘEKLADAČE (Makefile)                "
	@echo "======================================================================"
	@echo ""
	@echo "1. PŘEKLAD A SESTAVENÍ:"
	@echo "  make (nebo make build)     Přeloží celý překladač (výchozí cíl)"
	@echo "  make all                   Alias pro make build"
	@echo "  make clean                 Smaže všechny .o, .d soubory a binárku prekladac"
	@echo "  make build-check           Nejprve provede kontrolu kvality, poté překlad"
	@echo ""
	@echo "2. END-TO-END TESTY:"
	@echo "  make e2e                   Spustí všechny E2E testy (tests/run_e2e.sh)"
	@echo "  make e2e FILTER=<vzor>     Spustí jen vybranou skupinu testů (např. FILTER=sem_)"
	@echo "  make e2e-valgrind          Spustí E2E testy pod nástrojem Valgrind"
	@echo "                             (lze kombinovat s FILTER=<vzor> a IC26INT=<cesta>)"
	@echo ""
	@echo "3. KONTROLY KVALITY KÓDU:"
	@echo "  make check                 Spustí clang-format, clang-tidy a cppcheck"
	@echo "  make check-format          Pouze ověří formátování (clang-format --dry-run)"
	@echo "  make check-tidy            Pouze statická analýza clang-tidy"
	@echo "  make check-cppcheck        Pouze statická analýza cppcheck"
	@echo "  make format                Automaticky naformátuje všechny soubory"
	@echo "  (Všechny cíle v sekci 3 podporují omezení na jeden soubor: FILE=<soubor>)"
	@echo ""
	@echo "4. KONTROLA PAMĚTI (Valgrind):"
	@echo "  make memcheck              Spustí překladač pod Valgrindem (výchozí vstup /dev/null)"
	@echo "  make memcheck TEST_INPUT=<soubor>"
	@echo "                             Otestuje překladač pod Valgrindem se zadaným vstupem"
	@echo ""
	@echo "5. ZABALENÍ K ODEVZDÁNÍ:"
	@echo "  make pack FIT_LOGIN=<login>"
	@echo "                             Ověří povinné soubory a vytvoří archiv <login>.zip"
	@echo "                             (volitelně: INCLUDE_TESTS=0 pro vynechání testů)"
	@echo "  make zip FIT_LOGIN=<login> Alias pro make pack"
	@echo "======================================================================"

-include $(DEPS)

