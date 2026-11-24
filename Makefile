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
CFLAGS = -Wall -Wextra -Werror -pedantic -std=c11 -g
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
TEST_INPUT = factorial.scala

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

server-test:
	@test "$(FIT_LOGIN)" != "xlogin00" || \
		{ echo "❌ CHYBA: Zadejte FIT_LOGIN (např. make server-test FIT_LOGIN=xnovak00)"; exit 1; }
	@echo "--> Odesílám soubory na server Merlin..."
	ssh $(REMOTE) "mkdir -p $(REMOTE_DIR)"
	scp Makefile $(SRCS) $(HDRS) $(wildcard $(TEST_INPUT)) $(REMOTE):$(REMOTE_DIR)/
	@echo "--> Spouštím překlad a test na serveru Merlin..."
	ssh $(REMOTE) "cd $(REMOTE_DIR) && make clean build && ./$(TARGET) < $(MEM_INPUT) > /dev/null"
	@echo "✅ Test překladu na serveru Merlin úspěšně prošel!"

# ==============================================================================
# 5. GOOGLE TEST (Jednotkové a izolované testy)
# ==============================================================================
CXX ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -g -I.
GTEST_LIBS = -lgtest -lgtest_main -pthread

TEST_DIR = tests
TEST_SRCS = $(wildcard $(TEST_DIR)/test_*.cpp)
TEST_BINS = $(patsubst $(TEST_DIR)/test_%.cpp, bin_test_%, $(TEST_SRCS))

# Spuštění všech existujících testů najednou: make test
test: $(TEST_BINS)
	@if [ -z "$(TEST_BINS)" ]; then \
		echo "⚠️  Žádné testy k dispozici (v '$(TEST_DIR)/' nebyl nalezen žádný 'test_*.cpp')."; \
	else \
		for bin in $(TEST_BINS); do \
			echo "--> Spouštím test: $$bin"; \
			./$$bin || exit 1; \
		done; \
		echo "✅ Všechny jednotkové testy úspěšně prošly!"; \
	fi

# Vnitřní překlad testovací binárky: bin_test_<modul>
bin_test_%: $(TEST_DIR)/test_%.cpp
	$(CXX) $(CXXFLAGS) $^ $(GTEST_LIBS) -o $@

# Spuštění jednoho testu (funguje jak 'make test-stack', tak 'make test_stack')
test-%: bin_test_%
	./$< $(if $(FILTER),--gtest_filter="$(FILTER)",)

test_%: bin_test_%
	./$< $(if $(FILTER),--gtest_filter="$(FILTER)",)

# Kontrola paměti izolovaného testu pod Valgrindem: make memcheck-<nazev>
memcheck-%: bin_test_%
	@echo "--> Spouštím Valgrind nad testem $<..."
	$(VALGRIND) ./$< $(if $(FILTER),--gtest_filter="$(FILTER)",)
	@echo "✅ Modul $< prošel bez úniků paměti!"

# ------------------------------------------------------------------------------
# NÁVOD PRO TÝM: JAK PŘIDAT TEST PRO NOVÝ MODUL
# ------------------------------------------------------------------------------
# 1. Ve složce tests/ vytvořte soubor test_<nazev>.cpp (např. tests/test_stack.cpp).
#    Uvnitř includujte hlavičky překladače v bloku: extern "C" { #include "stack.h" }
# 2. Sem dolů připište jeden řádek se seznamem .o souborů, které modul potřebuje:
#       bin_test_<nazev>: <modul>.o error.o
# 3. Spusťte: make test-<nazev> nebo make memcheck-<nazev>
# ------------------------------------------------------------------------------

# ZDE PŘIPISUJTE ZÁVISLOSTI PRO VAŠE TESTY:
# bin_test_dynamic_string: dynamic_string.o error.o
# bin_test_stack:          stack.o error.o

# Příklady pro další vrstvy (odkomentujte a upravte při implementaci):
# bin_test_token:        token.o dynamic_string.o error.o
# bin_test_lexer:        lexer.o token.o dynamic_string.o error.o
# bin_test_symtable:     symtable.o dynamic_string.o error.o
# bin_test_expression:   expression.o stack.o token.o ast.o dynamic_string.o error.o
# bin_test_parser:       parser.o expression.o ast.o symtable.o lexer.o token.o dynamic_string.o error.o
# bin_test_generator:    generator.o ast.o symtable.o dynamic_string.o error.o

# ==============================================================================
# 6. ZABALENÍ PROJEKTU K ODEVZDÁNÍ (dle sekcí 5 a 6 zadání)
# ==============================================================================
ARCHIVE ?= $(FIT_LOGIN).zip
INCLUDE_TESTS ?= 1

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
# 7. END TO END TESTY
# ==============================================================================
IC26INT ?= ic26int

# Pomocný výpis tokenů pro testy lexeru (tests/tokdump.c, token_dump z token.c).
# Překlad: make tokdump, použití: ./tokdump < program.ifj
TOKDUMP_OBJS = lexer.o token.o dynamic_string.o error.o

tokdump: $(TEST_DIR)/tokdump.c $(TOKDUMP_OBJS)
	$(CC) $(CFLAGS) -I. $^ -o $@

# End-to-end testy: make e2e, jen jedna skupina: make e2e FILTER=tok_
e2e: build tokdump
	COMPILER=./$(TARGET) IC26INT=$(IC26INT) TOKDUMP=./tokdump $(TEST_DIR)/run_e2e.sh $(FILTER)

# Totéž, ale překladač i tokdump běží pod valgrindem
e2e-valgrind: build tokdump
	VALGRIND=1 COMPILER=./$(TARGET) IC26INT=$(IC26INT) TOKDUMP=./tokdump $(TEST_DIR)/run_e2e.sh $(FILTER)

.PHONY: e2e e2e-valgrind

# ==============================================================================
# 8. ÚKLID PŘELOŽENÝCH SOUBORŮ
# ==============================================================================
clean:
	rm -f $(OBJS) $(DEPS) $(TARGET) $(TEST_BINS) tokdump


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
	@echo "  make clean                 Smaže všechny .o, .d a přeložené binárky"
	@echo "  make build-check           Nejprve provede kontrolu kvality, poté překlad"
	@echo ""
	@echo "2. JEDNOTKOVÉ TESTY (Google Test):"
	@echo "  make test                  Zkompiluje a spustí všechny testy v tests/"
	@echo "  make test-<modul>          Izolovaně přeloží a spustí test jednoho modulu"
	@echo "                             (Příklad: make test-stack nebo make test-lexer)"
	@echo "  make test-<modul> FILTER=\"<vzor>\""
	@echo "                             Spustí jen konkrétní test z daného modulu"
	@echo "                             (Příklad: make test-stack FILTER=\"StackTest.Push*\")"
	@echo ""
	@echo "3. KONTROLY KVALITY KÓDU:"
	@echo "  make check                 Spustí clang-format, clang-tidy a cppcheck"
	@echo "  make check FILE=<soubor>   Zkontroluje pouze jeden soubor (např. FILE=stack.c)"
	@echo "  make format                Automaticky naformátuje všechny soubory"
	@echo "  make format FILE=<soubor>  Naformátuje pouze jeden zadaný soubor"
	@echo ""
	@echo "4. KONTROLA PAMĚTI (Valgrind):"
	@echo "  make memcheck              Zkontroluje celý překladač se vstupem <soubor>.scala"
	@echo "  make memcheck-<modul>      Otestuje úniky paměti v izolovaném testu modulu"
	@echo "                             (Příklad: make memcheck-stack)"
	@echo ""
	@echo "5. ŠKOLNÍ SERVER MERLIN:"
	@echo "  make server-test FIT_LOGIN=<váš_login>"
	@echo "                             Odešle kód na Merlin a ověří čistý překlad"
	@echo "======================================================================"

-include $(DEPS)

