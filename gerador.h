/*
 * Matheus Gabriel Viana Araujo - 10420444
 * Luis Fernando de Mesquita Pereira - 10410686
 *
 * gerador.h — Gerador de código MEPA
 *
 * Fornece emissão formatada de instruções e gerenciamento de rótulos.
 * Formato dos dois endereços: sem espaço entre parâmetros (ex: CRVL 0,0).
 */

#ifndef GERADOR_H
#define GERADOR_H

#include <stdio.h>

/**
 * Inicializa o gerador, associando-o ao arquivo de saída.
 */
void gerador_init(FILE *saida);

/**
 * Ativa saída de rastreamento (--trace): cada instrução MEPA emitida
 * também é escrita em 'trace'. Passe NULL para desativar.
 */
void gerador_set_trace(FILE *trace);

/**
 * Emite uma instrução MEPA formatada.
 *
 *  rotulo   — rótulo (ex: "L1"), ou NULL para sem rótulo
 *  mnem     — mnemônico (ex: "CRVL", "ARMZ", "SOMA")
 *  param1   — primeiro parâmetro, ou NULL
 *  param2   — segundo parâmetro (se não NULL, unido a param1 por ',')
 *
 * Formato gerado:
 *   Com rótulo:   "L1:  MNEM p1,p2\n"
 *   Sem rótulo:   "     MNEM p1,p2\n"
 */
void gera_instr_mepa(char *rotulo, char *mnem,
                     char *param1, char *param2);

/**
 * Retorna um novo rótulo único ("L1", "L2", ...).
 * O buffer interno é sobrescrito a cada chamada — copie se precisar
 * manter o valor após outra chamada a novo_rotulo().
 */
char *novo_rotulo(void);

/**
 * Formata "nivel,disp" num buffer estático e retorna o ponteiro.
 * Uso: gera_instr_mepa(NULL, "CRVL", fmt_addr(0, sym->endereco), NULL)
 */
char *fmt_addr(int nivel, int disp);

#endif /* GERADOR_H */
