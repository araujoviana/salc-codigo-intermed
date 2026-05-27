/*
 * Matheus Gabriel Viana Araujo - 10420444
 * Luis Fernando de Mesquita Pereira - 10410686
 *
 * tabsimb.c - Implementação da Tabela de Símbolos
 */

#include "tabsimb.h"
#include <stdio.h>
#include <string.h>

/* --- Estado interno (vetor fixo) --- */
static RegistroTS tabela[TS_MAX];
static int        n_simbolos  = 0;
static int        escopo_atual = ESCOPO_GLOBAL;

/* Converte categoria para string legível */
static const char *cat_str(Categoria c) {
    switch (c) {
    case CAT_PROGRAMA:    return "prog";
    case CAT_VARIAVEL:    return "var";
    case CAT_VETOR:       return "vet";
    case CAT_PROCEDIMENTO:return "proc";
    case CAT_FUNCAO:      return "fn";
    case CAT_PARAMETRO:   return "param";
    default:              return "?";
    }
}

/* Converte tipo para string */
static const char *tipo_str(TipoAtomo t) {
    switch (t) {
    case TIPO_INT:    return "int";
    case TIPO_BOOL:   return "bool";
    case TIPO_CHAR:   return "char";
    case TIPO_NENHUM: return "nenhum";
    default:          return "?";
    }
}

/* --- Interface pública --- */

void ts_init(void) {
    n_simbolos   = 0;
    escopo_atual = ESCOPO_GLOBAL;
}

void ts_destroy(void) {
    n_simbolos   = 0;
    escopo_atual = ESCOPO_GLOBAL;
}

void ts_set_escopo(int e) { escopo_atual = e; }
int  ts_get_escopo(void)  { return escopo_atual; }

/**
 * Insere símbolo no escopo atual.
 * Retorna NULL se já existir no mesmo escopo (duplicata).
 */
RegistroTS *ts_inserir(char *lexema, Categoria cat, TipoAtomo tipo,
                       int endereco) {
    if (n_simbolos >= TS_MAX) return NULL;

    /* Verifica duplicata no escopo atual */
    for (int i = 0; i < n_simbolos; i++) {
        if (tabela[i].escopo == escopo_atual &&
            strcmp(tabela[i].lexema, lexema) == 0)
            return NULL;
    }

    RegistroTS *r = &tabela[n_simbolos++];
    strncpy(r->lexema, lexema, LEX_MAX - 1);
    r->lexema[LEX_MAX - 1] = '\0';
    r->cat      = cat;
    r->tipo     = tipo;
    r->endereco = endereco;
    r->nivel    = 0;   /* simplificado: nível 0 para todas as vars */
    r->escopo   = escopo_atual;
    r->extra    = 0;
    return r;
}

/**
 * Busca símbolo visível: pesquisa do escopo atual para cima
 * (ESCOPO_MAIN vê ESCOPO_GLOBAL; global vê só global).
 */
RegistroTS *ts_buscar(char *lexema) {
    /* Busca no escopo atual primeiro, depois no pai */
    for (int scope = escopo_atual; scope >= ESCOPO_GLOBAL; scope--) {
        for (int i = 0; i < n_simbolos; i++) {
            if (tabela[i].escopo == scope &&
                strcmp(tabela[i].lexema, lexema) == 0)
                return &tabela[i];
        }
    }
    return NULL;
}

/**
 * Busca apenas no escopo atual (unicidade).
 */
RegistroTS *ts_buscar_local(char *lexema) {
    for (int i = 0; i < n_simbolos; i++) {
        if (tabela[i].escopo == escopo_atual &&
            strcmp(tabela[i].lexema, lexema) == 0)
            return &tabela[i];
    }
    return NULL;
}

/**
 * Devolve o i-ésimo parâmetro (0-based) da sub-rotina.
 * Parâmetros têm cat == CAT_PARAMETRO e escopo == sub->extra.
 */
const RegistroTS *ts_param(const RegistroTS *sub, int idx) {
    if (!sub) return NULL;
    /* Parâmetros ficam no escopo da própria sub-rotina (escopo > ESCOPO_MAIN) */
    int count = 0;
    for (int i = 0; i < n_simbolos; i++) {
        if (tabela[i].cat == CAT_PARAMETRO &&
            tabela[i].escopo == sub->extra) {
            if (count == idx) return &tabela[i];
            count++;
        }
    }
    return NULL;
}

/** Imprime a tabela de símbolos (útil para debug). */
void ts_dump(FILE *fp) {
    fprintf(fp, "%-20s %-8s %-7s addr nivel escopo extra\n",
            "lexema", "cat", "tipo");
    fprintf(fp, "%-20s %-8s %-7s ---- ----- ------ -----\n",
            "--------------------","--------","-------");
    for (int i = 0; i < n_simbolos; i++) {
        fprintf(fp, "%-20s %-8s %-7s %4d %5d %6d %5d\n",
                tabela[i].lexema,
                cat_str(tabela[i].cat),
                tipo_str(tabela[i].tipo),
                tabela[i].endereco,
                tabela[i].nivel,
                tabela[i].escopo,
                tabela[i].extra);
    }
}
