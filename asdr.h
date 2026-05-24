/*
 * Matheus Gabriel Viana Araujo - 10420444
 * Luis Fernando de Mesquita Pereira - 10410686
 *
 * asdr.h - Analisador Sintático com Descida Recursiva (ASDR)
 *
 * Implementa análise sintática, semântica e geração de código MEPA
 * para a linguagem SAL (Tradução Dirigida à Sintaxe).
 */

#ifndef ASDR_H
#define ASDR_H

#include <stdio.h>

/**
 * Ponto de entrada da análise.
 * Realiza análise léxica, sintática, semântica e gera código MEPA.
 *
 * Retorna  0 em sucesso.
 * Retorna -1 em caso de erro (mensagem no stderr).
 */
int parse_ini(void);

/**
 * Ativa dump da tabela de símbolos (--symtab) ao final da compilação.
 * Deve ser chamado antes de parse_ini(). Passa NULL para desativar.
 */
void parse_set_arq_ts(FILE *fp);

/**
 * Ativa log de tokens (--tokens) durante a análise léxica.
 * Deve ser chamado antes de parse_ini(). Passa NULL para desativar.
 */
void parse_set_arq_tk(FILE *fp);

#endif /* ASDR_H */
