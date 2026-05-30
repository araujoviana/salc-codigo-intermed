/*
 * Matheus Gabriel Viana Araujo - 10420444
 * Luis Fernando de Mesquita Pereira - 10410686
 *
 * asdr.c - Analisador Sintático com Descida Recursiva + Geração MEPA
 *
 * Estratégia: Tradução Dirigida à Sintaxe (SDT).
 * As instruções MEPA são emitidas no momento em que as construções
 * sintáticas são reconhecidas, sem necessidade de árvore intermediária.
 *
 * Simplificação (conforme especificação do projeto 2):
 *   - Vetores: análise semântica mantida; nenhum código MEPA gerado.
 *   - Sub-rotinas (proc/fn ≠ main): análise semântica mantida; sem MEPA.
 *   - Todas as variáveis usam nível de aninhamento 0 (sem sub-rotinas ativas).
 *   - Um slot extra é reservado ao final da memória para uso do 'match'.
 */

#include "analex.h"  /* léxico completo (header-only) */
#include "asdr.h"
#include "gerador.h"
#include "tabsimb.h"

#include <setjmp.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================
 *  Estado global do parser
 * ============================================================ */

#define MAX_PARAMS  64
#define MAX_DECL_ID 64
#define NIVEL_MEPA   0  /* nível de aninhamento: sempre 0 neste projeto */

static TInfoAtomo tk;        /* token atual                              */
static TInfoAtomo prox_tk;   /* lookahead (token seguinte)              */
static jmp_buf    jmp_erro;  /* ponto de retorno em caso de erro        */

/* Arquivos opcionais de saída auxiliar (NULL = desativado) */
static FILE *arq_ts = NULL;  /* --symtab: dump da tabela de símbolos    */
static FILE *arq_tk = NULL;  /* --tokens: log dos átomos léxicos        */

static int  prox_addr      = 0;  /* próximo endereço de variável disponível */
static int  match_temp_addr = -1; /* endereço do slot temporário para match  */
static int  prox_escopo_sub = ESCOPO_MAIN + 1; /* contador de escopos únicos para sub-rotinas */

/* Contexto da sub-rotina atual (para verificação de 'ret') */
static bool      em_funcao       = false;
static bool      fn_tem_ret      = false;
static TipoAtomo tipo_fn_atual   = TIPO_NENHUM;

/* Indica se estamos dentro de uma sub-rotina (suprime geração de código) */
static bool em_subrotina = false;

/* ============================================================
 *  Protótipos antecipados
 * ============================================================ */

static TipoAtomo   parse_expr(void);
static void        parse_comando(void);
static void        parse_bloco(void);
static TInfoAtomo  next_token_log(void);  /* log léxico (--tokens) */

/* ============================================================
 *  Helpers de análise léxica
 * ============================================================ */

/* Copia rótulo gerado para buffer local (novo_rotulo usa buf estático) */
#define SALVA_ROT(var) char var[16]; strncpy(var, novo_rotulo(), 15); var[15] = '\0'

/* Formata "nivel,disp" para uso imediato em gera_instr_mepa */
static char s_addr[32];
static char *addr_of(const RegistroTS *r) {
    snprintf(s_addr, sizeof(s_addr), "%d,%d", r->nivel, r->endereco);
    return s_addr;
}

/* Avança a janela de dois tokens (registra no log de tokens se ativo) */
static void avanca(void) {
    tk      = prox_tk;
    prox_tk = next_token_log();
}

/* Consome token atual se for da categoria s; retorna true se consumiu */
static bool aceita(Simb s) {
    if (tk.simb == s) { avanca(); return true; }
    return false;
}

/* Exige a categoria s; consome e avança. Aborta se errar. */
static void verifica(Simb s, const char *esperado) {
    if (tk.simb != s) {
        fprintf(stderr,
            "Erro sintatico [linha %d]: esperado '%s', encontrado '%s'\n",
            tk.linha, esperado, tk.lexema);
        longjmp(jmp_erro, 1);
    }
    avanca();
}

/* ============================================================
 *  Helpers de análise semântica
 * ============================================================ */

static const char *tipo_nome(TipoAtomo t) {
    switch (t) {
    case TIPO_INT:    return "int";
    case TIPO_BOOL:   return "bool";
    case TIPO_CHAR:   return "char";
    case TIPO_NENHUM: return "nenhum";
    default:          return "?";
    }
}

static void erro_semantico(const char *esperado, const char *encontrado,
                           int linha) {
    fprintf(stderr,
        "Erro semantico [linha %d]: esperado '%s', encontrado '%s'\n",
        linha, esperado, encontrado);
    longjmp(jmp_erro, 1);
}

static void exige_tipo(TipoAtomo atual, TipoAtomo esperado,
                       const char *desc, int linha) {
    if (atual != esperado)
        erro_semantico(desc, tipo_nome(atual), linha);
}

static void exige_mesmo_tipo(TipoAtomo a, TipoAtomo b,
                             const char *desc, int linha) {
    if (a != b)
        erro_semantico(desc, tipo_nome(b), linha);
}

static bool e_logico(TipoAtomo t)   { return t == TIPO_BOOL || t == TIPO_INT; }
static bool e_condicao(TipoAtomo t) { return e_logico(t); }

/* Macro para emitir instrução somente fora de sub-rotinas */
#define GERA(...) do { if (!em_subrotina) gera_instr_mepa(__VA_ARGS__); } while(0)

/* ============================================================
 *  Setters públicos para flags de saída auxiliar
 * ============================================================ */

void parse_set_arq_ts(FILE *fp) { arq_ts = fp; }
void parse_set_arq_tk(FILE *fp) { arq_tk = fp; }

/* ============================================================
 *  Log de tokens (--tokens)
 *  Mapeia cada Simb para um nome legível e registra no arq_tk.
 * ============================================================ */

static const char *simb_nome(Simb s) {
    switch (s) {
    case sIDENTIF:      return "IDENTIF";
    case sCTEINT:       return "CTEINT";
    case sCTECHAR:      return "CTECHAR";
    case sSTRING:       return "STRING";
    case sMODULE:       return "module";
    case sGLOBALS:      return "globals";
    case sLOCALS:       return "locals";
    case sSTART:        return "start";
    case sEND:          return "end";
    case sINT:          return "int";
    case sBOOL:         return "bool/true/false";
    case sCHAR:         return "char";
    case sFN:           return "fn";
    case sPROC:         return "proc";
    case sMAIN:         return "main";
    case sRETURN:       return "ret";
    case sPRINT:        return "print";
    case sSCAN:         return "scan";
    case sIF:           return "if";
    case sELSE:         return "else";
    case sMATCH:        return "match";
    case sWHEN:         return "when";
    case sOTHERWISE:    return "otherwise";
    case sFOR:          return "for";
    case sSTEP:         return "step";
    case sTO:           return "to";
    case sLOOP:         return "loop";
    case sWHILE:        return "while";
    case sUNTIL:        return "until";
    case sDO:           return "do";
    case sATRIB:        return ":=";
    case sIMPLIC:       return "=>";
    case sPTOPTO:       return "..";
    case sSOMA:         return "+";
    case sSUBRAT:       return "-";
    case sMULT:         return "*";
    case sDIV:          return "/";
    case sIGUAL:        return "=";
    case sDIFERENTE:    return "<>";
    case sMAIOR:        return ">";
    case sMAIORIG:      return ">=";
    case sMENOR:        return "<";
    case sMENORIG:      return "<=";
    case sOR:           return "v";
    case sAND:          return "^";
    case sNEG:          return "~";
    case sPTO_VIRG:     return ";";
    case sDOIS_PTOS:    return ":";
    case sVIRGULA:      return ",";
    case sABRE_PARENT:  return "(";
    case sFECHA_PARENT: return ")";
    case sABRE_COLCH:   return "[";
    case sFECHA_COLCH:  return "]";
    case sERRO:         return "ERRO";
    case sEOF:          return "EOF";
    default:            return "?";
    }
}

/*
 * Obtém o próximo token do léxico e, se arq_tk estiver ativo,
 * registra: número de linha, nome do símbolo e lexema.
 */
static TInfoAtomo next_token_log(void) {
    TInfoAtomo t = obter_atomo();
    if (arq_tk) {
        fprintf(arq_tk, "L%-4d  %-16s  %s\n",
                t.linha, simb_nome(t.simb), t.lexema);
    }
    return t;
}

/* ============================================================
 *  Parsing de tipos e declarações
 * ============================================================ */

/* <tipo> ::= sINT | sBOOL | sCHAR [vetor ignorado na codegen] */
static TipoAtomo parse_tipo(int *tam_out) {
    TipoAtomo tipo;
    if      (tk.simb == sINT)                                    { tipo = TIPO_INT;  avanca(); }
    else if (tk.simb == sBOOL && strcmp(tk.lexema,"bool") == 0) { tipo = TIPO_BOOL; avanca(); }
    else if (tk.simb == sCHAR)                                   { tipo = TIPO_CHAR; avanca(); }
    else {
        fprintf(stderr, "Erro sintatico [linha %d]: tipo esperado (int|bool|char), encontrado '%s'\n",
                tk.linha, tk.lexema);
        longjmp(jmp_erro, 1);
        tipo = TIPO_INT; /* não alcançado */
    }
    /* Sufixo de vetor: [tamanho] */
    int tam = 0;
    if (aceita(sABRE_COLCH)) {
        tam = atoi(tk.lexema);
        verifica(sCTEINT, "tamanho do vetor");
        verifica(sFECHA_COLCH, "]");
    }
    if (tam_out) *tam_out = tam;
    return tipo;
}

/*
 * Lê uma linha de declarações: id (, id)* : tipo [vetor] ;
 * Insere cada variável na tabela de símbolos com endereço sequencial.
 * Retorna a quantidade de variáveis declaradas na linha.
 */
static int parse_dcls(void) {
    typedef struct { char nome[LEX_MAX]; int linha; } DeclItem;
    DeclItem items[MAX_DECL_ID];
    int n = 0;

    /* Primeiro identificador */
    strncpy(items[n].nome, tk.lexema, LEX_MAX - 1);
    items[n].linha = tk.linha;
    n++;
    verifica(sIDENTIF, "identificador");

    while (aceita(sVIRGULA)) {
        if (n >= MAX_DECL_ID) {
            erro_semantico("limite de ids por declaracao", tk.lexema, tk.linha);
        }
        strncpy(items[n].nome, tk.lexema, LEX_MAX - 1);
        items[n].linha = tk.linha;
        n++;
        verifica(sIDENTIF, "identificador");
    }

    verifica(sDOIS_PTOS, ":");
    int tam_vec = 0;
    TipoAtomo tipo = parse_tipo(&tam_vec);
    verifica(sPTO_VIRG, ";");

    /* Insere cada identificador na tabela */
    for (int i = 0; i < n; i++) {
        Categoria cat = (tam_vec > 0) ? CAT_VETOR : CAT_VARIAVEL;
        RegistroTS *r = ts_inserir(items[i].nome, cat, tipo, prox_addr);
        if (!r) {
            erro_semantico("identificador unico no escopo",
                           items[i].nome, items[i].linha);
        }
        /* Vetores ocupam (tam+1) slots: 1 para o tamanho + tam para elementos */
        if (cat == CAT_VETOR) {
            r->extra = tam_vec;
            prox_addr += tam_vec + 1; /* slot 0 = tamanho lógico */
        } else {
            prox_addr++;
        }
    }
    return n;
}

/* Seção 'globals': lê uma ou mais linhas de declaração */
static void parse_globais(void) {
    verifica(sGLOBALS, "globals");
    do {
        parse_dcls();
    } while (tk.simb == sIDENTIF);
}

/* Seção 'locals' (dentro de proc/fn/main): idem */
static void parse_locals(void) {
    verifica(sLOCALS, "locals");
    do {
        parse_dcls();
    } while (tk.simb == sIDENTIF);
}

/* ============================================================
 *  Parsing de sub-rotinas (análise semântica; sem geração MEPA)
 * ============================================================ */

typedef struct {
    char      nome[LEX_MAX];
    TipoAtomo tipo;
    int       tam;
    int       linha;
} InfoParam;

static int parse_param_lista(InfoParam params[], int max) {
    int n = 0;
    do {
        if (n >= max) {
            erro_semantico("limite de parametros", tk.lexema, tk.linha);
        }
        strncpy(params[n].nome, tk.lexema, LEX_MAX - 1);
        params[n].linha = tk.linha;
        n++;
        verifica(sIDENTIF, "identificador de parametro");
        verifica(sDOIS_PTOS, ":");
        params[n-1].tipo = parse_tipo(&params[n-1].tam);
    } while (aceita(sVIRGULA));
    return n;
}

static void parse_subrotina(bool e_funcao) {
    InfoParam params[MAX_PARAMS];
    char      nome[LEX_MAX];
    int       nome_linha;
    int       param_count = 0;
    TipoAtomo tipo_ret    = TIPO_NENHUM;

    /* Salva contexto anterior (incluindo escopo) */
    bool      prev_em_funcao  = em_funcao;
    bool      prev_tem_ret    = fn_tem_ret;
    TipoAtomo prev_tipo_fn    = tipo_fn_atual;
    bool      prev_sub        = em_subrotina;
    int       prev_escopo     = ts_get_escopo();

    em_subrotina = true; /* suprime geração de código MEPA */

    if (e_funcao) verifica(sFN,   "fn");
    else          verifica(sPROC, "proc");

    nome_linha = tk.linha;
    strncpy(nome, tk.lexema, LEX_MAX - 1);
    verifica(sIDENTIF, "nome da sub-rotina");

    verifica(sABRE_PARENT, "(");
    if (tk.simb == sIDENTIF) {
        param_count = parse_param_lista(params, MAX_PARAMS);
    }
    verifica(sFECHA_PARENT, ")");

    if (e_funcao) {
        verifica(sDOIS_PTOS, ":");
        tipo_ret = parse_tipo(NULL);
    }

    /* Registra sub-rotina no escopo corrente (normalmente global) */
    RegistroTS *r = ts_inserir(nome,
                               e_funcao ? CAT_FUNCAO : CAT_PROCEDIMENTO,
                               tipo_ret, -1);
    if (!r) {
        erro_semantico(e_funcao ? "funcao unica" : "procedimento unico",
                       nome, nome_linha);
    }

    /* Escopo exclusivo para esta sub-rotina via contador global */
    int escopo_sub = prox_escopo_sub++;
    ts_set_escopo(escopo_sub);
    r->extra = escopo_sub; /* escopo dos parâmetros - usado por ts_param */

    em_funcao    = e_funcao;
    fn_tem_ret   = false;
    tipo_fn_atual = tipo_ret;

    /* Insere parâmetros como CAT_PARAMETRO no escopo exclusivo da sub */
    for (int i = 0; i < param_count; i++) {
        RegistroTS *p = ts_inserir(params[i].nome, CAT_PARAMETRO,
                                   params[i].tipo, i);
        if (!p) {
            erro_semantico("parametro unico", params[i].nome, params[i].linha);
        }
        p->extra = params[i].tam;
    }
    /* r->extra = escopo dos parâmetros; ts_param usa esse valor para buscá-los */

    if (aceita(sLOCALS)) {
        do { parse_dcls(); } while (tk.simb == sIDENTIF);
    }
    parse_bloco();

    if (e_funcao && !fn_tem_ret) {
        erro_semantico("funcao com ret", nome, nome_linha);
    }

    /* Restaura contexto ao escopo anterior (não fixa em ESCOPO_MAIN) */
    ts_set_escopo(prev_escopo);
    em_funcao    = prev_em_funcao;
    fn_tem_ret   = prev_tem_ret;
    tipo_fn_atual = prev_tipo_fn;
    em_subrotina = prev_sub;
}

/* ============================================================
 *  Parsing de expressões (com geração de código MEPA)
 * ============================================================ */

static bool e_literal_bool(void) {
    return tk.simb == sBOOL &&
           (strcmp(tk.lexema, "true") == 0 || strcmp(tk.lexema, "false") == 0);
}


/* parse_elem: literal | id | vetor[idx] | chamada(args) */
static TipoAtomo parse_elem(void) {
    /* Constante inteira */
    if (tk.simb == sCTEINT) {
        char cte[LEX_MAX]; strncpy(cte, tk.lexema, LEX_MAX - 1);
        avanca();
        GERA(NULL, "CRCT", cte, NULL);
        return TIPO_INT;
    }
    /* Constante char */
    if (tk.simb == sCTECHAR) {
        char cte[LEX_MAX]; strncpy(cte, tk.lexema, LEX_MAX - 1);
        avanca();
        GERA(NULL, "CRCT", cte, NULL);
        return TIPO_CHAR;
    }
    /* Literal booleano */
    if (e_literal_bool()) {
        int val = (strcmp(tk.lexema, "true") == 0) ? 1 : 0;
        char cte[8]; snprintf(cte, sizeof(cte), "%d", val);
        avanca();
        GERA(NULL, "CRCT", cte, NULL);
        return TIPO_BOOL;
    }
    /* String */
    if (tk.simb == sSTRING) {
        char str[LEX_MAX]; strncpy(str, tk.lexema, LEX_MAX - 1);
        avanca();
        GERA(NULL, "CRST", str, NULL); /* instrução CRST para constante string */
        return TIPO_CHAR; /* tratamos string como char para compatibilidade */
    }
    /* Identificador: variável, vetor ou chamada */
    if (tk.simb == sIDENTIF) {
        char nome[LEX_MAX]; int ln = tk.linha;
        strncpy(nome, tk.lexema, LEX_MAX - 1);
        avanca();

        /* Chamada de sub-rotina: id( ... ) */
        if (tk.simb == sABRE_PARENT) {
            avanca(); /* consume '(' */
            RegistroTS *sub = ts_buscar(nome);
            if (!sub ||
                (sub->cat != CAT_PROCEDIMENTO && sub->cat != CAT_FUNCAO)) {
                erro_semantico("sub-rotina declarada", nome, ln);
            }
            int n_args = 0;
            if (tk.simb != sFECHA_PARENT) {
                do {
                    int aln = tk.linha;
                    TipoAtomo ta = parse_expr();
                    const RegistroTS *p = ts_param(sub, n_args);
                    if (p) exige_tipo(ta, p->tipo, "tipo de argumento compativel", aln);
                    n_args++;
                } while (aceita(sVIRGULA));
            }
            verifica(sFECHA_PARENT, ")");
            /* Conta parâmetros esperados via ts_param (extra agora é o escopo) */
            int n_esp = 0;
            while (ts_param(sub, n_esp) != NULL) n_esp++;
            if (n_esp != n_args) {
                erro_semantico("quantidade correta de parametros", nome, ln);
            }
            /* Sem geração de código MEPA para chamadas (simplificação) */
            return sub->tipo;
        }

        /* Acesso a vetor: id[idx] */
        if (tk.simb == sABRE_COLCH) {
            avanca(); /* consume '[' */
            RegistroTS *v = ts_buscar(nome);
            if (!v || v->cat != CAT_VETOR) {
                erro_semantico("vetor declarado", nome, ln);
            }
            int iln = tk.linha;
            TipoAtomo tidx = parse_expr();
            exige_tipo(tidx, TIPO_INT, "indice inteiro", iln);
            verifica(sFECHA_COLCH, "]");
            /* Sem geração de código para vetores (simplificação) */
            return v->tipo;
        }

        /* Variável escalar */
        RegistroTS *v = ts_buscar(nome);
        if (!v || (v->cat != CAT_VARIAVEL && v->cat != CAT_PARAMETRO)) {
            erro_semantico("variavel ou parametro declarado", nome, ln);
        }
        GERA(NULL, "CRVL", addr_of(v), NULL);
        return v->tipo;
    }

    fprintf(stderr, "Erro sintatico [linha %d]: elemento esperado, encontrado '%s'\n",
            tk.linha, tk.lexema);
    longjmp(jmp_erro, 1);
    return TIPO_NENHUM; /* não alcançado */
}

/* parse_fator: elem | ~fator | -fator | (expr) */
static TipoAtomo parse_fator(void) {
    /* Negação lógica: ~ */
    if (tk.simb == sNEG) {
        int ln = tk.linha;
        avanca();
        TipoAtomo t = parse_fator();
        if (!e_logico(t)) erro_semantico("operando logico", tipo_nome(t), ln);
        GERA(NULL, "NEGA", NULL, NULL);
        return TIPO_BOOL;
    }
    /* Menos unário: - */
    if (tk.simb == sSUBRAT) {
        int ln = tk.linha;
        avanca();
        /* Emite CRCT 0 ANTES da expressão para que SUBT faça 0 - expr */
        GERA(NULL, "CRCT", "0", NULL);
        TipoAtomo t = parse_fator();
        exige_tipo(t, TIPO_INT, "operando inteiro no menos unario", ln);
        GERA(NULL, "SUBT", NULL, NULL);
        return TIPO_INT;
    }
    /* Parênteses */
    if (aceita(sABRE_PARENT)) {
        TipoAtomo t = parse_expr();
        verifica(sFECHA_PARENT, ")");
        return t;
    }
    return parse_elem();
}

/* Nível multiplicativo: fator (* | /) fator ... */
static TipoAtomo parse_exarp(void) {
    TipoAtomo esq = parse_fator();
    while (tk.simb == sMULT || tk.simb == sDIV) {
        int ln  = tk.linha;
        bool mult = (tk.simb == sMULT);
        avanca();
        exige_tipo(esq, TIPO_INT, "operando inteiro", ln);
        TipoAtomo dir = parse_fator();
        exige_tipo(dir, TIPO_INT, "operando inteiro", ln);
        GERA(NULL, mult ? "MULT" : "DIVI", NULL, NULL);
        esq = TIPO_INT;
    }
    return esq;
}

/* Nível aditivo: exarp (+ | -) exarp ... */
static TipoAtomo parse_exari(void) {
    TipoAtomo esq = parse_exarp();
    while (tk.simb == sSOMA || tk.simb == sSUBRAT) {
        int ln   = tk.linha;
        bool soma = (tk.simb == sSOMA);
        avanca();
        exige_tipo(esq, TIPO_INT, "operando inteiro", ln);
        TipoAtomo dir = parse_exarp();
        exige_tipo(dir, TIPO_INT, "operando inteiro", ln);
        GERA(NULL, soma ? "SOMA" : "SUBT", NULL, NULL);
        esq = TIPO_INT;
    }
    return esq;
}

static bool e_rel(Simb s) {
    return s == sMAIOR || s == sMAIORIG || s == sIGUAL ||
           s == sMENOR || s == sMENORIG || s == sDIFERENTE;
}

/* Nível relacional: exari op_rel exari */
static TipoAtomo parse_exrel(void) {
    TipoAtomo esq = parse_exari();
    while (e_rel(tk.simb)) {
        Simb op  = tk.simb;
        int  ln  = tk.linha;
        avanca();
        TipoAtomo dir = parse_exari();

        /* Verificação de tipo por operador */
        if (op == sMAIOR || op == sMAIORIG || op == sMENOR || op == sMENORIG) {
            exige_tipo(esq, TIPO_INT, "operando inteiro em relacional", ln);
            exige_tipo(dir, TIPO_INT, "operando inteiro em relacional", ln);
        } else {
            exige_mesmo_tipo(esq, dir, "operandos do mesmo tipo em relacional", ln);
        }

        /* Emite instrução MEPA de comparação */
        switch (op) {
        case sIGUAL:    GERA(NULL, "CMIG",  NULL, NULL); break;
        case sDIFERENTE:GERA(NULL, "CMDIF", NULL, NULL); break;
        case sMAIOR:    GERA(NULL, "CMMA",  NULL, NULL); break;
        case sMAIORIG:  GERA(NULL, "CMMAG", NULL, NULL); break;
        case sMENOR:    GERA(NULL, "CMME",  NULL, NULL); break;
        case sMENORIG:  GERA(NULL, "CMMEG", NULL, NULL); break;
        default: break;
        }
        esq = TIPO_BOOL;
    }
    return esq;
}

/* Nível lógico AND (^): exrel ^ exrel ... */
static TipoAtomo parse_exlog(void) {
    TipoAtomo esq = parse_exrel();
    while (tk.simb == sAND) {
        int ln = tk.linha;
        avanca();
        if (!e_logico(esq)) erro_semantico("operando logico", tipo_nome(esq), ln);
        TipoAtomo dir = parse_exrel();
        if (!e_logico(dir)) erro_semantico("operando logico", tipo_nome(dir), ln);
        GERA(NULL, "CONJ", NULL, NULL);
        esq = TIPO_BOOL;
    }
    return esq;
}

/* Nível lógico OR (v): exlog v exlog ... */
static TipoAtomo parse_expr(void) {
    TipoAtomo esq = parse_exlog();
    while (tk.simb == sOR) {
        int ln = tk.linha;
        avanca();
        if (!e_logico(esq)) erro_semantico("operando logico", tipo_nome(esq), ln);
        TipoAtomo dir = parse_exlog();
        if (!e_logico(dir)) erro_semantico("operando logico", tipo_nome(dir), ln);
        GERA(NULL, "DISJ", NULL, NULL);
        esq = TIPO_BOOL;
    }
    return esq;
}

/* ============================================================
 *  Comandos
 * ============================================================ */

/* print(elem, ...) */
static void parse_print(void) {
    verifica(sPRINT, "print");
    verifica(sABRE_PARENT, "(");
    parse_expr();
    GERA(NULL, "IMPR", NULL, NULL);
    while (aceita(sVIRGULA)) {
        parse_expr();
        GERA(NULL, "IMPR", NULL, NULL);
    }
    verifica(sFECHA_PARENT, ")");
}

/* scan(id) */
static void parse_scan(void) {
    verifica(sSCAN, "scan");
    verifica(sABRE_PARENT, "(");

    if (tk.simb == sIDENTIF && prox_tk.simb == sABRE_COLCH) {
        /* Acesso a vetor (simplificação: sem código) */
        char nome[LEX_MAX]; strncpy(nome, tk.lexema, LEX_MAX - 1);
        int ln = tk.linha;
        avanca(); avanca(); /* id e [ */
        int iln = tk.linha;
        TipoAtomo tidx = parse_expr();
        exige_tipo(tidx, TIPO_INT, "indice inteiro", iln);
        verifica(sFECHA_COLCH, "]");
        RegistroTS *v = ts_buscar(nome);
        if (!v || v->cat != CAT_VETOR)
            erro_semantico("vetor declarado", nome, ln);
    } else if (tk.simb == sIDENTIF) {
        char nome[LEX_MAX]; int ln = tk.linha;
        strncpy(nome, tk.lexema, LEX_MAX - 1);
        avanca();
        RegistroTS *v = ts_buscar(nome);
        if (!v || (v->cat != CAT_VARIAVEL && v->cat != CAT_PARAMETRO))
            erro_semantico("variavel declarada", nome, ln);
        GERA(NULL, "LEIT", NULL, NULL);
        GERA(NULL, "ARMZ", addr_of(v), NULL);
    } else {
        fprintf(stderr, "Erro sintatico [linha %d]: identificador esperado em scan\n",
                tk.linha);
        longjmp(jmp_erro, 1);
    }

    verifica(sFECHA_PARENT, ")");
}

/* Atribuição: id := expr  OU  id[idx] := expr */
static void parse_atrib(void) {
    char nome[LEX_MAX]; int ln = tk.linha;
    strncpy(nome, tk.lexema, LEX_MAX - 1);
    verifica(sIDENTIF, "identificador");

    if (tk.simb == sABRE_COLCH) {
        /* Vetor: sem geração de código (simplificação) */
        avanca();
        int iln = tk.linha;
        TipoAtomo tidx = parse_expr();
        exige_tipo(tidx, TIPO_INT, "indice inteiro", iln);
        verifica(sFECHA_COLCH, "]");
        RegistroTS *v = ts_buscar(nome);
        if (!v || v->cat != CAT_VETOR) erro_semantico("vetor declarado", nome, ln);
        verifica(sATRIB, ":=");
        int eln = tk.linha;
        TipoAtomo te = parse_expr();
        exige_tipo(te, v->tipo, "tipo compativel na atribuicao", eln);
        return;
    }

    RegistroTS *v = ts_buscar(nome);
    if (!v || (v->cat != CAT_VARIAVEL && v->cat != CAT_PARAMETRO))
        erro_semantico("variavel ou parametro declarado", nome, ln);

    verifica(sATRIB, ":=");
    int eln = tk.linha;
    TipoAtomo te = parse_expr();
    exige_tipo(te, v->tipo, "tipo compativel na atribuicao", eln);
    GERA(NULL, "ARMZ", addr_of(v), NULL);
}

/* if ( expr ) cmd [ else cmd ] */
static void parse_if(void) {
    SALVA_ROT(L_else);

    verifica(sIF, "if");
    verifica(sABRE_PARENT, "(");
    int ln = tk.linha;
    TipoAtomo tc = parse_expr();
    if (!e_condicao(tc)) erro_semantico("condicao valida", tipo_nome(tc), ln);
    verifica(sFECHA_PARENT, ")");

    GERA(NULL, "DSVF", L_else, NULL);
    parse_comando();

    if (aceita(sELSE)) {
        SALVA_ROT(L_fim);
        GERA(NULL, "DSVS", L_fim, NULL);
        GERA(L_else, "NADA", NULL, NULL);
        parse_comando();
        GERA(L_fim, "NADA", NULL, NULL);
    } else {
        GERA(L_else, "NADA", NULL, NULL);
    }
}

/* for id := e1 to e2 [step s] do cmd */
static void parse_for(void) {
    SALVA_ROT(L_test);
    SALVA_ROT(L_exit);
    SALVA_ROT(L_body);
    SALVA_ROT(L_after);

    verifica(sFOR, "for");

    char ctrl_nome[LEX_MAX]; int ctrl_ln = tk.linha;
    strncpy(ctrl_nome, tk.lexema, LEX_MAX - 1);
    verifica(sIDENTIF, "variavel de controle");

    RegistroTS *ctrl = ts_buscar(ctrl_nome);
    if (!ctrl || (ctrl->cat != CAT_VARIAVEL && ctrl->cat != CAT_PARAMETRO))
        erro_semantico("variavel de controle declarada", ctrl_nome, ctrl_ln);
    exige_tipo(ctrl->tipo, TIPO_INT, "variavel de controle inteira", ctrl_ln);

    char a_ctrl[32]; snprintf(a_ctrl, sizeof(a_ctrl), "%d,%d",
                              ctrl->nivel, ctrl->endereco);

    verifica(sATRIB, ":=");
    int e1_ln = tk.linha;
    exige_tipo(parse_expr(), TIPO_INT, "expressao inteira em for", e1_ln);
    GERA(NULL, "ARMZ", a_ctrl, NULL);   /* ctrl := e1 */

    verifica(sTO, "to");

    /* Test label + condição reavaliada em cada iteração */
    GERA(L_test, "NADA", NULL, NULL);
    GERA(NULL, "CRVL", a_ctrl, NULL);  /* empilha ctrl */
    int e2_ln = tk.linha;
    exige_tipo(parse_expr(), TIPO_INT, "expressao inteira em for", e2_ln);

    /* Step: parsear antes de emitir a comparação */
    bool step_neg    = false;
    bool step_e_id   = false;
    int  step_val    = 1;
    char step_id[LEX_MAX] = {0};

    if (tk.simb == sSTEP) {
        avanca();
        if (tk.simb == sSUBRAT) { step_neg = true; avanca(); }
        if (tk.simb == sIDENTIF) {
            step_e_id = true;
            strncpy(step_id, tk.lexema, LEX_MAX - 1);
            avanca();
        } else {
            int step_ln = tk.linha;
            step_val = atoi(tk.lexema);
            verifica(sCTEINT, "constante inteira para step");
            if (step_val == 0)
                erro_semantico("passo diferente de zero", "0", step_ln);
        }
    }

    /* Comparação: <= (passo positivo) ou >= (passo negativo) */
    GERA(NULL, step_neg ? "CMMAG" : "CMMEG", NULL, NULL);
    GERA(NULL, "DSVF", L_exit, NULL);
    GERA(NULL, "DSVS", L_body, NULL);

    /* Pós-corpo: incremento/decremento */
    GERA(L_after, "NADA", NULL, NULL);
    GERA(NULL, "CRVL", a_ctrl, NULL);
    if (step_e_id) {
        RegistroTS *sv = ts_buscar(step_id);
        if (!sv) erro_semantico("variavel de step declarada", step_id, tk.linha);
        char a_step[32]; snprintf(a_step, sizeof(a_step), "%d,%d",
                                  sv->nivel, sv->endereco);
        GERA(NULL, "CRVL", a_step, NULL);
        GERA(NULL, step_neg ? "SUBT" : "SOMA", NULL, NULL);
    } else {
        int abs_s = (step_val < 0) ? -step_val : step_val;
        if (abs_s == 0) abs_s = 1;
        char cs[16]; snprintf(cs, sizeof(cs), "%d", abs_s);
        GERA(NULL, "CRCT", cs, NULL);
        GERA(NULL, step_neg ? "SUBT" : "SOMA", NULL, NULL);
    }
    GERA(NULL, "ARMZ", a_ctrl, NULL);
    GERA(NULL, "DSVS", L_test, NULL);

    /* Corpo */
    GERA(L_body, "NADA", NULL, NULL);
    verifica(sDO, "do");
    parse_comando();
    GERA(NULL, "DSVS", L_after, NULL);
    GERA(L_exit, "NADA", NULL, NULL);
}

/* loop while ( expr ) cmd */
static void parse_while(void) {
    SALVA_ROT(L_test);
    SALVA_ROT(L_exit);
    SALVA_ROT(L_body);
    SALVA_ROT(L_after);

    verifica(sLOOP, "loop");
    verifica(sWHILE, "while");
    verifica(sABRE_PARENT, "(");

    GERA(L_test, "NADA", NULL, NULL);
    int ln = tk.linha;
    TipoAtomo tc = parse_expr();
    if (!e_condicao(tc)) erro_semantico("condicao valida", tipo_nome(tc), ln);
    verifica(sFECHA_PARENT, ")");

    GERA(NULL, "DSVF", L_exit,  NULL);
    GERA(NULL, "DSVS", L_body,  NULL);
    GERA(L_after, "NADA", NULL, NULL);
    GERA(NULL, "DSVS", L_test,  NULL);
    GERA(L_body, "NADA", NULL, NULL);

    parse_comando();

    GERA(NULL, "DSVS", L_after, NULL);
    GERA(L_exit, "NADA", NULL, NULL);
}

/* loop cmd* until ( expr ) */
static void parse_until(void) {
    SALVA_ROT(L_ini);

    verifica(sLOOP, "loop");
    GERA(L_ini, "NADA", NULL, NULL);

    /* Comandos antes do until */
    while (tk.simb != sUNTIL) {
        parse_comando();
        verifica(sPTO_VIRG, ";");
    }

    verifica(sUNTIL, "until");
    verifica(sABRE_PARENT, "(");
    int ln = tk.linha;
    TipoAtomo tc = parse_expr();
    if (!e_condicao(tc)) erro_semantico("condicao valida", tipo_nome(tc), ln);
    verifica(sFECHA_PARENT, ")");

    /* Se a condição for FALSA, volta para o início */
    GERA(NULL, "DSVF", L_ini, NULL);
}

/* ------------------------------------------------
 *  match: análise e geração para when/otherwise
 * ------------------------------------------------ */

/* Lê um inteiro possivelmente negativo de uma cláusula when */
static int parse_wint(void) {
    bool neg = aceita(sSUBRAT);
    int  val = atoi(tk.lexema);
    verifica(sCTEINT, "constante inteira no when");
    return neg ? -val : val;
}

/*
 * Gera código para um witem: val único ou intervalo val1..val2
 * Caso haja correspondência → DSVS L_corpo.
 * Caso não haja → queda para próximo item (via etiqueta local de skip).
 */
static void gera_witem(char *L_corpo, char *addr_temp) {
    int v1 = parse_wint();

    if (tk.simb == sPTOPTO) {
        /* Intervalo: v1..v2 */
        avanca();
        int v2 = parse_wint();

        SALVA_ROT(L_skip);
        char sv1[32], sv2[32];
        snprintf(sv1, sizeof(sv1), "%d", v1);
        snprintf(sv2, sizeof(sv2), "%d", v2);

        GERA(NULL, "CRVL", addr_temp, NULL);
        GERA(NULL, "CRCT", sv1, NULL);
        GERA(NULL, "CMMAG", NULL, NULL);  /* temp >= v1? */
        GERA(NULL, "DSVF", L_skip, NULL); /* não → pula */
        GERA(NULL, "CRVL", addr_temp, NULL);
        GERA(NULL, "CRCT", sv2, NULL);
        GERA(NULL, "CMMEG", NULL, NULL);  /* temp <= v2? */
        GERA(NULL, "DSVF", L_skip, NULL); /* não → pula */
        GERA(NULL, "DSVS", L_corpo, NULL);
        GERA(L_skip, "NADA", NULL, NULL);
    } else {
        /* Valor único */
        SALVA_ROT(L_skip);
        char sv[32]; snprintf(sv, sizeof(sv), "%d", v1);

        GERA(NULL, "CRVL", addr_temp, NULL);
        GERA(NULL, "CRCT", sv, NULL);
        GERA(NULL, "CMIG", NULL, NULL);   /* temp == v1? */
        GERA(NULL, "DSVF", L_skip, NULL); /* não → pula */
        GERA(NULL, "DSVS", L_corpo, NULL);
        GERA(L_skip, "NADA", NULL, NULL);
    }
}

/*
 * match ( expr )
 *   when cond => cmd ;
 *   ...
 *   [otherwise => cmd ;]
 * end
 */
static void parse_match(void) {
    if (match_temp_addr < 0) {
        fprintf(stderr, "Erro interno [linha %d]: slot para match nao alocado\n",
                tk.linha);
        longjmp(jmp_erro, 1);
    }
    char addr_temp[32];
    snprintf(addr_temp, sizeof(addr_temp), "0,%d", match_temp_addr);

    verifica(sMATCH, "match");
    verifica(sABRE_PARENT, "(");
    int ln = tk.linha;
    TipoAtomo te = parse_expr();
    exige_tipo(te, TIPO_INT, "expressao inteira em match", ln);
    verifica(sFECHA_PARENT, ")");

    GERA(NULL, "ARMZ", addr_temp, NULL); /* salva expr no temp */

    SALVA_ROT(L_fim);

    /* Cláusulas when */
    while (tk.simb == sWHEN) {
        avanca(); /* consume 'when' */

        SALVA_ROT(L_corpo);
        SALVA_ROT(L_prox);

        /* Lista de condições: witem (, witem)* */
        gera_witem(L_corpo, addr_temp);
        while (aceita(sVIRGULA)) {
            gera_witem(L_corpo, addr_temp);
        }
        /* Nenhuma condição casou → pula para próximo when */
        GERA(NULL, "DSVS", L_prox, NULL);

        GERA(L_corpo, "NADA", NULL, NULL);
        verifica(sIMPLIC, "=>");
        parse_comando();
        GERA(NULL, "DSVS", L_fim, NULL);

        GERA(L_prox, "NADA", NULL, NULL);
        verifica(sPTO_VIRG, ";");
    }

    /* Cláusula otherwise opcional */
    if (tk.simb == sOTHERWISE) {
        avanca();
        verifica(sIMPLIC, "=>");
        parse_comando();
        verifica(sPTO_VIRG, ";");
    }

    verifica(sEND, "end");
    GERA(L_fim, "NADA", NULL, NULL);
}

/* ret expr  (somente dentro de função) */
static void parse_ret(void) {
    if (!em_funcao) {
        erro_semantico("ret somente dentro de funcao", tk.lexema, tk.linha);
    }
    verifica(sRETURN, "ret");
    fn_tem_ret = true;
    int ln = tk.linha;
    TipoAtomo tr = parse_expr();
    exige_tipo(tr, tipo_fn_atual, "tipo compativel com retorno da funcao", ln);
    /* Sem geração MEPA para sub-rotinas */
}

/* Identifica se o token pode iniciar um comando */
static bool inicia_comando(Simb s) {
    return s == sPRINT || s == sSCAN  || s == sIF    || s == sMATCH  ||
           s == sFOR   || s == sLOOP  || s == sRETURN || s == sSTART  ||
           s == sIDENTIF;
}

/* parse_comando: despacha para o parser do comando correto */
static void parse_comando(void) {
    switch (tk.simb) {
    case sPRINT:  parse_print(); break;
    case sSCAN:   parse_scan();  break;
    case sIF:     parse_if();    break;
    case sMATCH:  parse_match(); break;
    case sFOR:    parse_for();   break;
    case sLOOP:
        if (prox_tk.simb == sWHILE) parse_while();
        else                         parse_until();
        break;
    case sRETURN: parse_ret();   break;
    case sSTART:  parse_bloco(); break;
    case sIDENTIF:
        if (prox_tk.simb == sABRE_PARENT) {
            /* Chamada de procedimento como comando (descarta retorno) */
            char nome[LEX_MAX]; int ln = tk.linha;
            strncpy(nome, tk.lexema, LEX_MAX - 1);
            avanca(); avanca(); /* id ( */
            RegistroTS *sub = ts_buscar(nome);
            if (!sub || (sub->cat != CAT_PROCEDIMENTO && sub->cat != CAT_FUNCAO))
                erro_semantico("sub-rotina declarada", nome, ln);
            int na = 0;
            if (tk.simb != sFECHA_PARENT) {
                do {
                    int aln = tk.linha;
                    TipoAtomo ta = parse_expr();
                    const RegistroTS *p = ts_param(sub, na);
                    if (p) exige_tipo(ta, p->tipo, "tipo de argumento", aln);
                    na++;
                } while (aceita(sVIRGULA));
            }
            verifica(sFECHA_PARENT, ")");
            /* Conta parâmetros esperados via ts_param (extra agora é o escopo) */
            int n_esp2 = 0;
            while (ts_param(sub, n_esp2) != NULL) n_esp2++;
            if (n_esp2 != na) {
                erro_semantico("quantidade correta de parametros", nome, ln);
            }
        } else {
            parse_atrib();
        }
        break;
    default:
        fprintf(stderr, "Erro sintatico [linha %d]: comando esperado, encontrado '%s'\n",
                tk.linha, tk.lexema);
        longjmp(jmp_erro, 1);
        break;
    }
}

/* start cmd ; cmd ; ... end */
static void parse_bloco(void) {
    verifica(sSTART, "start");
    while (inicia_comando(tk.simb)) {
        parse_comando();
        verifica(sPTO_VIRG, ";");
    }
    verifica(sEND, "end");
}

/* ============================================================
 *  Procedimento principal (proc main)
 * ============================================================ */

/*
 * Varre o restante do arquivo-fonte buscando a palavra "match".
 * Usa fseek para restaurar a posição do lexer após a varredura.
 * Falso positivo (ex: "match" em comentário) apenas aloca um slot
 * extra de memória — não causa erro de execução.
 */
static bool fonte_tem_match(void) {
    long saved = ftell(fonte_sal);
    if (saved < 0) return true;

    char word[8];
    int  n = 0, c;
    bool found = false;

    while (!found && (c = fgetc(fonte_sal)) != EOF) {
        if (isalpha((unsigned char)c) || c == '_') {
            if (n < (int)sizeof(word) - 1) word[n++] = (char)c;
        } else {
            word[n] = '\0';
            if (strcmp(word, "match") == 0) found = true;
            n = 0;
        }
    }
    if (!found && n > 0) {
        word[n] = '\0';
        found = (strcmp(word, "match") == 0);
    }

    fseek(fonte_sal, saved, SEEK_SET);
    return found;
}

static void parse_principal(int n_globais) {
    verifica(sPROC, "proc");
    verifica(sMAIN, "main");
    verifica(sABRE_PARENT, "(");
    verifica(sFECHA_PARENT, ")");

    if (ts_inserir("main", CAT_PROCEDIMENTO, TIPO_NENHUM, -1) == NULL) {
        erro_semantico("procedimento unico", "main", tk.linha);
    }

    ts_set_escopo(ESCOPO_MAIN);
    prox_addr = n_globais; /* endereços locais continuam após os globais */

    int n_locals = 0;
    if (tk.simb == sLOCALS) {
        int addr_antes = prox_addr;
        parse_locals();
        n_locals = prox_addr - addr_antes;
    }

    int total_vars = n_globais + n_locals;

    /*
     * Aloca slot extra para o temporário do 'match' somente se o bloco
     * principal de fato usa a instrução. A varredura antecipada da fonte
     * evita alocar memória desnecessária em programas sem match.
     */
    bool usa_match   = fonte_tem_match();
    match_temp_addr  = usa_match ? total_vars : -1;
    int  alloc       = total_vars + (usa_match ? 1 : 0);

    if (alloc > 0) {
        char ct[16]; snprintf(ct, sizeof(ct), "%d", alloc);
        GERA(NULL, "AMEM", ct, NULL);
    }

    parse_bloco();

    if (alloc > 0) {
        char ct[16]; snprintf(ct, sizeof(ct), "%d", alloc);
        GERA(NULL, "DMEM", ct, NULL);
    }

    GERA(NULL, "PARA", NULL, NULL);
}

/* ============================================================
 *  Ponto de entrada: parse_ini
 * ============================================================ */

int parse_ini(void) {
    /* Configura ponto de retorno para erros */
    if (setjmp(jmp_erro) != 0) return -1;

    /* Inicializa janela de dois tokens (já registra no log, se ativo) */
    tk      = next_token_log();
    prox_tk = next_token_log();
    if (tk.simb == sERRO || prox_tk.simb == sERRO) return -1;

    /* Inicializa tabela de símbolos */
    ts_init();
    ts_set_escopo(ESCOPO_GLOBAL);
    prox_addr        = 0;
    prox_escopo_sub  = ESCOPO_MAIN + 1; /* reinicia escopos de sub-rotinas */
    em_subrotina     = false;
    em_funcao        = false;
    fn_tem_ret       = false;

    /* module <id> ; */
    verifica(sMODULE, "module");
    char nome_modulo[LEX_MAX];
    strncpy(nome_modulo, tk.lexema, LEX_MAX - 1);
    verifica(sIDENTIF, "nome do modulo");
    verifica(sPTO_VIRG, ";");

    /* Registra o programa na tabela */
    ts_inserir(nome_modulo, CAT_PROGRAMA, TIPO_NENHUM, -1);

    /* Instrução inicial MEPA */
    GERA(NULL, "INPP", NULL, NULL);

    /* Seção globals (opcional) */
    if (tk.simb == sGLOBALS) {
        parse_globais();
    }
    int n_globais = prox_addr;

    /* Sub-rotinas (proc/fn antes da main) - apenas análise semântica */
    while (tk.simb == sFN ||
           (tk.simb == sPROC && prox_tk.simb != sMAIN)) {
        parse_subrotina(tk.simb == sFN);
    }

    /* Procedimento principal obrigatório */
    parse_principal(n_globais);

    /* Fim do arquivo */
    if (tk.simb != sEOF) {
        fprintf(stderr,
            "Erro sintatico [linha %d]: fim de arquivo esperado, encontrado '%s'\n",
            tk.linha, tk.lexema);
        return -1;
    }

    GERA(NULL, "FIM", NULL, NULL);

    /* Dump da tabela de símbolos (--symtab), antes de destruí-la */
    if (arq_ts) {
        ts_dump(arq_ts);
        arq_ts = NULL;
    }

    ts_destroy();
    return 0;
}
