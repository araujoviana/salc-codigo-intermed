CC      := gcc
CFLAGS  := -Wall -Wextra -std=c99
TARGET  := salc
OBJDIR  := dist
SRCS    := $(wildcard *.c)
OBJS    := $(patsubst %.c,$(OBJDIR)/%.o,$(SRCS))

.PHONY: all clean test test-verbose test-erros

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(TARGET)

$(OBJDIR)/%.o: %.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR):
	mkdir -p $(OBJDIR)

TESTS := fibonacci seq_while if_else loop_until match_test \
         aritmetica logica relacionais for_passo match_range subrotinas \
         nested_if char_const

TESTES_ERRO := var_nao_declarada duplicata ret_fora_funcao \
               args_errados tipo_incompativel fn_sem_ret arg_tipo_errado

test: $(TARGET)
	@set -e; \
	ok=0; fail=0; \
	for t in $(TESTS); do \
	    printf '%-20s' "$$t"; \
	    if ./$(TARGET) tests/$${t}.sal 2>/dev/null; then \
	        printf 'OK\n'; ok=$$((ok+1)); \
	    else \
	        printf 'FALHOU\n'; fail=$$((fail+1)); \
	    fi; \
	done; \
	printf '\nResultado: %d/%d testes passaram.\n' \
	    "$$ok" "$$((ok+fail))"

test-verbose: $(TARGET)
	@set -e; \
	for t in $(TESTS); do \
	    printf '\n========== %s ==========\n' "$$t"; \
	    ./$(TARGET) tests/$${t}.sal && cat tests/$${t}.mepa; \
	done; \
	printf '\nTodos os testes concluidos.\n'

test-erros: $(TARGET)
	@ok=0; fail=0; \
	for t in $(TESTES_ERRO); do \
	    printf '%-30s' "$$t"; \
	    if ! ./$(TARGET) tests/erros/$${t}.sal 2>/dev/null; then \
	        printf 'OK  (rejeitado corretamente)\n'; ok=$$((ok+1)); \
	    else \
	        printf 'FALHOU  (deveria ter rejeitado)\n'; fail=$$((fail+1)); \
	    fi; \
	done; \
	printf '\nErros detectados: %d/%d\n' "$$ok" "$$((ok+fail))"

clean:
	rm -f $(TARGET) *.mepa tests/*.mepa tests/erros/*.mepa
	rm -f tests/*.tk tests/*.ts tests/*.trc
	rm -rf $(OBJDIR)
