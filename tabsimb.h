/*
 * Matheus Gabriel Viana Araujo - 10420444
 * Luis Fernando de Mesquita Pereira - 10410686
 *
 * tabsimb.h — Tabela de Símbolos para o compilador SALc
 *
 * Registra identificadores com categoria, tipo e endereço MEPA
 * (nível de aninhamento + deslocamento).
 */

#ifndef TABSIMB_H
#define TABSIMB_H

#include <stdio.h>   /* FILE*, para ts_dump */

/* LEX_MAX também está em analex.h; aqui apenas como fallback
 * para evitar que tabsimb.h dependa do léxico completo.       */
#ifndef LEX_MAX
#  define LEX_MAX 1024
#endif

#define TS_MAX       2048
#define ESCOPO_GLOBAL   0
#define ESCOPO_MAIN     1

typedef enum {
    CAT_PROGRAMA,
    CAT_VARIAVEL,
    CAT_VETOR,
    CAT_PROCEDIMENTO,
    CAT_FUNCAO,
    CAT_PARAMETRO
} Categoria;

typedef enum {
    TIPO_INT,
    TIPO_BOOL,
    TIPO_CHAR,
    TIPO_NENHUM
} TipoAtomo;

typedef struct {
    char      lexema[LEX_MAX];
    Categoria cat;
    TipoAtomo tipo;
    int       endereco; /* deslocamento d em CRVL n,d  */
    int       nivel;    /* nivel n em CRVL n,d          */
    int       escopo;   /* escopo de declaração         */
    int       extra;    /* params (proc/fn) ou tam (vet)*/
} RegistroTS;

void        ts_init(void);
void        ts_destroy(void);
void        ts_set_escopo(int escopo);
int         ts_get_escopo(void);
RegistroTS *ts_inserir(char *lexema, Categoria cat, TipoAtomo tipo, int endereco);
RegistroTS *ts_buscar(char *lexema);
RegistroTS *ts_buscar_local(char *lexema);
const RegistroTS *ts_param(const RegistroTS *sub, int i);
void        ts_dump(FILE *fp);

#endif /* TABSIMB_H */
