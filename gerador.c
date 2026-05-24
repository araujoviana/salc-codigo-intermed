/*
 * Matheus Gabriel Viana Araujo - 10420444
 * Luis Fernando de Mesquita Pereira - 10410686
 *
 * gerador.c - Implementação do Gerador de Código MEPA
 */

#include "gerador.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static FILE *arquivo_saida  = NULL;
static FILE *arquivo_trace  = NULL;
static int   rotulo_contador = 0;

/* Buffers estáticos para evitar alocação dinâmica */
static char buf_rotulo[32];
static char buf_addr[32];

void gerador_init(FILE *saida) {
    arquivo_saida    = saida;
    arquivo_trace    = NULL;
    rotulo_contador  = 0;
}

void gerador_set_trace(FILE *trace) {
    arquivo_trace = trace;
}

/**
 * Emite instrução no formato:
 *   "L1:  MNEM p1,p2\n"   (com rótulo)
 *   "     MNEM p1,p2\n"   (sem rótulo)
 *
 * Importante: quando param1 e param2 são ambos não-NULL, são unidos
 * por ',' sem espaços (requisito do interpretador MEPA).
 */
void gera_instr_mepa(char *rotulo, char *mnem,
                     char *param1, char *param2) {
    if (!arquivo_saida) return;

    /* Coluna de rótulo: 5 caracteres (ex: "L1:  " ou "     ") */
    if (rotulo) {
        char col[32];
        snprintf(col, sizeof(col), "%s:", rotulo);
        fprintf(arquivo_saida, "%-5s", col);
    } else {
        fprintf(arquivo_saida, "     ");
    }

    /* Mnemônico */
    fprintf(arquivo_saida, "%s", mnem);

    /* Parâmetros */
    if (param1 && param2) {
        /* Dois endereços: sem espaço entre eles → "0,0" */
        fprintf(arquivo_saida, " %s,%s", param1, param2);
    } else if (param1) {
        fprintf(arquivo_saida, " %s", param1);
    }

    fprintf(arquivo_saida, "\n");

    /* Espelha instrução no arquivo de rastreamento (--trace), se ativo */
    if (arquivo_trace) {
        if (rotulo) {
            char col[32];
            snprintf(col, sizeof(col), "%s:", rotulo);
            fprintf(arquivo_trace, "%-5s", col);
        } else {
            fprintf(arquivo_trace, "     ");
        }
        fprintf(arquivo_trace, "%s", mnem);
        if (param1 && param2)
            fprintf(arquivo_trace, " %s,%s", param1, param2);
        else if (param1)
            fprintf(arquivo_trace, " %s", param1);
        fprintf(arquivo_trace, "\n");
    }
}

/**
 * Gera e retorna um novo rótulo único ("L1", "L2", ...).
 * O ponteiro retornado é válido até a próxima chamada.
 */
char *novo_rotulo(void) {
    snprintf(buf_rotulo, sizeof(buf_rotulo), "L%d", ++rotulo_contador);
    return buf_rotulo;
}

/**
 * Formata "nivel,disp" num buffer estático.
 * Uso: gera_instr_mepa(NULL, "CRVL", fmt_addr(0, sym->endereco), NULL)
 */
char *fmt_addr(int nivel, int disp) {
    snprintf(buf_addr, sizeof(buf_addr), "%d,%d", nivel, disp);
    return buf_addr;
}
