/*
 * Matheus Gabriel Viana Araujo - 10420444
 * Luis Fernando de Mesquita Pereira - 10410686
 *
 * analex.h — Analisador Léxico da linguagem SAL
 * Interface fornecida: TInfoAtomo obter_atomo()
 *
 * Implementado como header-only (funções static) para que apenas
 * asdr.c precise incluí-lo, evitando definições duplicadas.
 */

#ifndef ANALEX_H
#define ANALEX_H

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ─── Tamanho máximo do lexema ─── */
#define LEX_MAX 1024

/* ─── Categorias de átomos (tokens) ─── */
typedef enum {
    sIDENTIF = 0,
    sCTEINT,
    sCTECHAR,
    sSTRING,
    sMODULE,
    sGLOBALS,
    sLOCALS,
    sSTART,
    sEND,
    sINT,
    sBOOL,
    sCHAR,
    sFN,
    sPROC,
    sMAIN,
    sRETURN,
    sPRINT,
    sSCAN,
    sIF,
    sELSE,
    sMATCH,
    sWHEN,
    sOTHERWISE,
    sFOR,
    sSTEP,
    sTO,
    sLOOP,
    sWHILE,
    sUNTIL,
    sDO,
    sATRIB,
    sIMPLIC,
    sPTOPTO,
    sSOMA,
    sSUBRAT,
    sMULT,
    sDIV,
    sIGUAL,
    sDIFERENTE,
    sMAIOR,
    sMAIORIG,
    sMENOR,
    sMENORIG,
    sOR,
    sAND,
    sNEG,
    sPTO_VIRG,
    sDOIS_PTOS,
    sVIRGULA,
    sABRE_PARENT,
    sFECHA_PARENT,
    sABRE_COLCH,
    sFECHA_COLCH,
    sERRO,
    sEOF
} Simb;

/* ─── Estrutura do átomo retornado pelo léxico ─── */
typedef struct {
    Simb  simb;
    char  lexema[LEX_MAX];
    int   linha;
} TInfoAtomo;

/* ─── Estado global do léxico (definido em main.c) ─── */
extern FILE *fonte_sal;   /* arquivo-fonte em leitura           */
extern int   linha_lex;   /* contador de linhas (inicia em 1)   */

/* ══════════════════════════════════════════════════════════════
 *  Implementação do autômato finito determinístico do léxico.
 *  Todas as funções são `static` para evitar múltiplas definições.
 * ══════════════════════════════════════════════════════════════ */

typedef struct { const char *texto; Simb simb; } MapReservada;

static const MapReservada reservadas[] = {
    {"bool",      sBOOL},      {"char",      sCHAR},
    {"do",        sDO},        {"else",      sELSE},
    {"end",       sEND},       {"false",     sBOOL},
    {"fn",        sFN},        {"for",       sFOR},
    {"globals",   sGLOBALS},   {"if",        sIF},
    {"int",       sINT},       {"locals",    sLOCALS},
    {"loop",      sLOOP},      {"main",      sMAIN},
    {"match",     sMATCH},     {"module",    sMODULE},
    {"otherwise", sOTHERWISE}, {"print",     sPRINT},
    {"proc",      sPROC},      {"ret",       sRETURN},
    {"scan",      sSCAN},      {"start",     sSTART},
    {"step",      sSTEP},      {"to",        sTO},
    {"true",      sBOOL},      {"until",     sUNTIL},
    {"when",      sWHEN},      {"while",     sWHILE},
};
#define N_RESERVADAS ((int)(sizeof(reservadas)/sizeof(reservadas[0])))

static Simb lex_categoria(const char *lex) {
    for (int i = 0; i < N_RESERVADAS; i++)
        if (strcmp(lex, reservadas[i].texto) == 0)
            return reservadas[i].simb;
    return sIDENTIF;
}

static TInfoAtomo lex_make(Simb s, const char *lex, int linha) {
    TInfoAtomo t;
    t.simb  = s;
    t.linha = linha;
    strncpy(t.lexema, lex, LEX_MAX - 1);
    t.lexema[LEX_MAX - 1] = '\0';
    return t;
}

static TInfoAtomo lex_erro(const char *msg, int linha) {
    fprintf(stderr, "Erro lexico [linha %d]: %s\n", linha, msg);
    return lex_make(sERRO, msg, linha);
}

static bool lex_append(char *buf, int *len, int c) {
    if (*len + 1 >= LEX_MAX) return false;
    buf[(*len)++] = (char)c;
    buf[*len]     = '\0';
    return true;
}

typedef enum {
    ST_INI, ST_IDENT, ST_NUM,
    ST_STRING, ST_CHAR_INI, ST_CHAR_ESC, ST_CHAR_MID,
    ST_COLON, ST_PONTO, ST_IGUAL, ST_MAIOR, ST_MENOR,
    ST_COMENT_AMBIG, ST_COMENT_LINHA, ST_COMENT_BLOCO, ST_COMENT_FIM,
    ST_OR, ST_AND
} EstadoLex;

/* Lê o próximo átomo do arquivo-fonte global */
static TInfoAtomo obter_atomo(void) {
    char buf[LEX_MAX];
    int  len   = 0;
    int  linha = linha_lex;
    EstadoLex estado = ST_INI;
    int c;

    buf[0] = '\0';

    while ((c = fgetc(fonte_sal)) != EOF) {
        switch (estado) {

        case ST_INI:
            linha = linha_lex; len = 0; buf[0] = '\0';
            if (isspace(c)) { if (c == '\n') linha_lex++; continue; }
            if (!lex_append(buf, &len, c))
                return lex_erro("lexema longo demais", linha_lex);

            if (c == '"')               { estado = ST_STRING;       break; }
            if (c == '\'')              { estado = ST_CHAR_INI;     break; }
            if (isdigit(c))             { estado = ST_NUM;           break; }
            if (c == 'v')               { estado = ST_OR;            break; }
            if (c == '^')               { estado = ST_AND;           break; }
            if (isalpha(c) || c == '_') { estado = ST_IDENT;         break; }
            if (c == '@')               { estado = ST_COMENT_AMBIG;  break; }
            if (c == ':')               { estado = ST_COLON;         break; }
            if (c == '.')               { estado = ST_PONTO;         break; }
            if (c == '=')               { estado = ST_IGUAL;         break; }
            if (c == '>')               { estado = ST_MAIOR;         break; }
            if (c == '<')               { estado = ST_MENOR;         break; }
            if (c == '+') return lex_make(sSOMA,         buf, linha);
            if (c == '-') return lex_make(sSUBRAT,       buf, linha);
            if (c == '*') return lex_make(sMULT,         buf, linha);
            if (c == '/') return lex_make(sDIV,          buf, linha);
            if (c == '~') return lex_make(sNEG,          buf, linha);
            if (c == ';') return lex_make(sPTO_VIRG,     buf, linha);
            if (c == ',') return lex_make(sVIRGULA,      buf, linha);
            if (c == '(') return lex_make(sABRE_PARENT,  buf, linha);
            if (c == ')') return lex_make(sFECHA_PARENT, buf, linha);
            if (c == '[') return lex_make(sABRE_COLCH,   buf, linha);
            if (c == ']') return lex_make(sFECHA_COLCH,  buf, linha);
            { char m[64]; snprintf(m, sizeof(m), "caractere invalido '%c'", c);
              return lex_erro(m, linha); }

        case ST_STRING:
            if (c == '"') {
                lex_append(buf, &len, c);
                return lex_make(sSTRING, buf, linha);
            }
            if (c == '\n') return lex_erro("string sem fechamento", linha);
            if (!lex_append(buf, &len, c)) return lex_erro("string longa", linha);
            break;

        case ST_CHAR_INI:
            if (c == '\'') return lex_erro("caractere vazio ''", linha);
            if (c == '\n') return lex_erro("char sem fechamento", linha);
            if (!lex_append(buf, &len, c)) return lex_erro("char longo", linha);
            estado = (c == '\\') ? ST_CHAR_ESC : ST_CHAR_MID;
            break;
        case ST_CHAR_ESC:
            if (!lex_append(buf, &len, c)) return lex_erro("char longo", linha);
            estado = ST_CHAR_MID;
            break;
        case ST_CHAR_MID:
            if (c != '\'') return lex_erro("char: esperado '", linha);
            lex_append(buf, &len, c);
            return lex_make(sCTECHAR, buf, linha);

        case ST_NUM:
            if (isdigit(c)) { lex_append(buf, &len, c); break; }
            ungetc(c, fonte_sal);
            return lex_make(sCTEINT, buf, linha);

        case ST_IDENT:
            if (isalnum(c) || c == '_') { lex_append(buf, &len, c); break; }
            ungetc(c, fonte_sal);
            return lex_make(lex_categoria(buf), buf, linha);

        case ST_OR:
            if (isalnum(c) || c == '_') { ungetc(c, fonte_sal); estado = ST_IDENT; break; }
            ungetc(c, fonte_sal);
            return lex_make(sOR, buf, linha);

        case ST_AND:
            if (isalnum(c) || c == '_') { ungetc(c, fonte_sal); estado = ST_IDENT; break; }
            ungetc(c, fonte_sal);
            return lex_make(sAND, buf, linha);

        case ST_COLON:
            if (c == '=') { lex_append(buf, &len, c); return lex_make(sATRIB, buf, linha); }
            ungetc(c, fonte_sal);
            return lex_make(sDOIS_PTOS, buf, linha);

        case ST_PONTO:
            if (c == '.') { lex_append(buf, &len, c); return lex_make(sPTOPTO, buf, linha); }
            ungetc(c, fonte_sal);
            return lex_erro("ponto simples invalido (use ..)", linha);

        case ST_IGUAL:
            if (c == '>') { lex_append(buf, &len, c); return lex_make(sIMPLIC, buf, linha); }
            ungetc(c, fonte_sal);
            return lex_make(sIGUAL, buf, linha);

        case ST_MAIOR:
            if (c == '=') { lex_append(buf, &len, c); return lex_make(sMAIORIG, buf, linha); }
            ungetc(c, fonte_sal);
            return lex_make(sMAIOR, buf, linha);

        case ST_MENOR:
            if (c == '=') { lex_append(buf, &len, c); return lex_make(sMENORIG, buf, linha); }
            if (c == '>') { lex_append(buf, &len, c); return lex_make(sDIFERENTE, buf, linha); }
            ungetc(c, fonte_sal);
            return lex_make(sMENOR, buf, linha);

        case ST_COMENT_AMBIG:
            estado = (c == '{') ? ST_COMENT_BLOCO : ST_COMENT_LINHA;
            if (estado == ST_COMENT_LINHA) ungetc(c, fonte_sal);
            break;
        case ST_COMENT_LINHA:
            if (c == '\n') { linha_lex++; estado = ST_INI; len = 0; buf[0] = '\0'; }
            break;
        case ST_COMENT_BLOCO:
            if (c == '\n') linha_lex++;
            else if (c == '}') estado = ST_COMENT_FIM;
            break;
        case ST_COMENT_FIM:
            if (c == '@') { estado = ST_INI; len = 0; buf[0] = '\0'; break; }
            estado = ST_COMENT_BLOCO;
            break;
        }
    }

    /* Fim de arquivo */
    switch (estado) {
    case ST_NUM:          return lex_make(sCTEINT,          buf, linha_lex);
    case ST_IDENT:        return lex_make(lex_categoria(buf),buf, linha_lex);
    case ST_OR:           return lex_make(sOR,               buf, linha_lex);
    case ST_AND:          return lex_make(sAND,              buf, linha_lex);
    case ST_COLON:        return lex_make(sDOIS_PTOS,        buf, linha_lex);
    case ST_IGUAL:        return lex_make(sIGUAL,            buf, linha_lex);
    case ST_MAIOR:        return lex_make(sMAIOR,            buf, linha_lex);
    case ST_MENOR:        return lex_make(sMENOR,            buf, linha_lex);
    case ST_STRING:
    case ST_CHAR_INI:
    case ST_CHAR_MID:
        return lex_erro("token nao fechado no fim do arquivo", linha_lex);
    case ST_COMENT_BLOCO:
    case ST_COMENT_FIM:
        return lex_erro("comentario de bloco nao fechado", linha_lex);
    default: break;
    }

    return lex_make(sEOF, "EOF", linha_lex);
}

#endif /* ANALEX_H */
