/*
 * Matheus Gabriel Viana Araujo - 10420444
 * Luis Fernando de Mesquita Pereira - 10410686
 *
 * main.c — Ponto de entrada do compilador SALc (Projeto 2)
 *
 * Uso:   salc <arquivo.sal> [--tokens] [--symtab] [--trace]
 *
 * Saídas geradas:
 *   <base>.mepa   — código MEPA (sempre)
 *   <base>.tk     — lista de tokens léxicos  (requer --tokens)
 *   <base>.ts     — tabela de símbolos       (requer --symtab)
 *   <base>.trc    — rastreamento de instruções MEPA (requer --trace)
 *
 * Fluxo:
 *   1. Analisa argumentos e derivar nomes de arquivos de saída.
 *   2. Abre arquivos necessários.
 *   3. Configura gerador e analisador com os arquivos auxiliares.
 *   4. Invoca parse_ini() — análise + geração de código (SDT).
 *   5. Fecha arquivos; em caso de erro remove saídas incompletas.
 */

#include "asdr.h"
#include "gerador.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ─── Globais requeridos pelo léxico (analex.h usa 'extern') ─── */
FILE *fonte_sal = NULL;   /* arquivo-fonte SAL aberto para leitura */
int   linha_lex = 1;      /* contador de linhas (inicia em 1)      */

/* ──────────────────────────────────────────────────────────────
 * derivar_ext
 *   Copia 'entrada' para 'saida', substituindo (ou adicionando)
 *   a extensão por 'ext'.
 *   Exemplo: "tests/fib.sal" + ".mepa" → "tests/fib.mepa"
 *            "programa"      + ".ts"   → "programa.ts"
 * ────────────────────────────────────────────────────────────── */
static void derivar_ext(const char *entrada, char *saida, size_t n,
                        const char *ext) {
    strncpy(saida, entrada, n - 1);
    saida[n - 1] = '\0';

    /* Remove extensão ".sal" se presente */
    char *ponto = strrchr(saida, '.');
    if (ponto && strcmp(ponto, ".sal") == 0)
        *ponto = '\0';

    /* Garante espaço para ext + '\0' */
    if (strlen(saida) + strlen(ext) >= n) {
        fprintf(stderr, "salc: caminho de arquivo muito longo: %s\n", entrada);
        exit(EXIT_FAILURE);
    }
    strcat(saida, ext);
}

/* ──────────────────────────────────────────────────────────────
 * main
 * ────────────────────────────────────────────────────────────── */
int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr,
            "Uso: %s <arquivo.sal> [--tokens] [--symtab] [--trace]\n",
            argv[0]);
        return EXIT_FAILURE;
    }

    const char *nome_entrada = argv[1];

    /* ── Analisa flags (a partir de argv[2]) ── */
    bool flag_tokens = false;
    bool flag_symtab = false;
    bool flag_trace  = false;

    for (int i = 2; i < argc; i++) {
        if      (strcmp(argv[i], "--tokens") == 0) flag_tokens = true;
        else if (strcmp(argv[i], "--symtab") == 0) flag_symtab = true;
        else if (strcmp(argv[i], "--trace")  == 0) flag_trace  = true;
        else {
            fprintf(stderr, "salc: flag desconhecida '%s'\n", argv[i]);
            fprintf(stderr,
                "Uso: %s <arquivo.sal> [--tokens] [--symtab] [--trace]\n",
                argv[0]);
            return EXIT_FAILURE;
        }
    }

    /* ── Deriva nomes dos arquivos de saída ── */
    char nome_mepa[4096], nome_tk[4096], nome_ts[4096], nome_trc[4096];
    derivar_ext(nome_entrada, nome_mepa, sizeof(nome_mepa), ".mepa");
    derivar_ext(nome_entrada, nome_tk,   sizeof(nome_tk),   ".tk");
    derivar_ext(nome_entrada, nome_ts,   sizeof(nome_ts),   ".ts");
    derivar_ext(nome_entrada, nome_trc,  sizeof(nome_trc),  ".trc");

    /* ── Abre arquivo de entrada ── */
    fonte_sal = fopen(nome_entrada, "r");
    if (!fonte_sal) {
        fprintf(stderr, "salc: erro ao abrir '%s' para leitura.\n",
                nome_entrada);
        return EXIT_FAILURE;
    }

    /* ── Abre arquivo de saída principal (.mepa) ── */
    FILE *arq_mepa = fopen(nome_mepa, "w");
    if (!arq_mepa) {
        fprintf(stderr, "salc: erro ao criar '%s'.\n", nome_mepa);
        fclose(fonte_sal);
        return EXIT_FAILURE;
    }

    /* ── Abre arquivos auxiliares, conforme flags ── */
    FILE *arq_tk  = NULL;
    FILE *arq_ts  = NULL;
    FILE *arq_trc = NULL;

    if (flag_tokens) {
        arq_tk = fopen(nome_tk, "w");
        if (!arq_tk) {
            fprintf(stderr, "salc: erro ao criar '%s'.\n", nome_tk);
            fclose(arq_mepa); fclose(fonte_sal);
            return EXIT_FAILURE;
        }
        fprintf(arq_tk, "%-6s  %-16s  %s\n", "LINHA", "TOKEN", "LEXEMA");
        fprintf(arq_tk, "%-6s  %-16s  %s\n",
                "------", "----------------", "------");
    }
    if (flag_symtab) {
        arq_ts = fopen(nome_ts, "w");
        if (!arq_ts) {
            fprintf(stderr, "salc: erro ao criar '%s'.\n", nome_ts);
            if (arq_tk) fclose(arq_tk);
            fclose(arq_mepa); fclose(fonte_sal);
            return EXIT_FAILURE;
        }
    }
    if (flag_trace) {
        arq_trc = fopen(nome_trc, "w");
        if (!arq_trc) {
            fprintf(stderr, "salc: erro ao criar '%s'.\n", nome_trc);
            if (arq_ts) fclose(arq_ts);
            if (arq_tk) fclose(arq_tk);
            fclose(arq_mepa); fclose(fonte_sal);
            return EXIT_FAILURE;
        }
        fprintf(arq_trc, "@ Rastreamento de instruções MEPA — %s\n\n",
                nome_entrada);
    }

    /* ── Configura gerador e analisador com os arquivos auxiliares ── */
    gerador_init(arq_mepa);
    if (flag_trace)  gerador_set_trace(arq_trc);
    if (flag_tokens) parse_set_arq_tk(arq_tk);
    if (flag_symtab) parse_set_arq_ts(arq_ts);

    /* ── Executa compilação ── */
    int resultado = parse_ini();   /* 0 = sucesso, -1 = erro */

    /* ── Fecha todos os arquivos ── */
    fclose(fonte_sal);
    fclose(arq_mepa);
    fonte_sal = NULL;

    if (arq_tk)  fclose(arq_tk);
    if (arq_ts)  fclose(arq_ts);
    if (arq_trc) fclose(arq_trc);

    if (resultado != 0) {
        /* Remove saídas incompletas para não confundir execuções futuras */
        remove(nome_mepa);
        if (flag_symtab) remove(nome_ts);
        /* Mantém .tk e .trc mesmo em erro — úteis para depuração */
        return EXIT_FAILURE;
    }

    /* ── Informa saídas geradas ── */
    fprintf(stderr, "salc: %s -> %s\n", nome_entrada, nome_mepa);
    if (flag_tokens) fprintf(stderr, "salc: tokens    -> %s\n", nome_tk);
    if (flag_symtab) fprintf(stderr, "salc: tabela    -> %s\n", nome_ts);
    if (flag_trace)  fprintf(stderr, "salc: rastreio  -> %s\n", nome_trc);

    return EXIT_SUCCESS;
}
