// SPDX-License-Identifier: GPL-3.0-only
/* analise.c - léxico y analizador de sagsh (D037).
 *
 * El texto se convierte en un árbol (no_t) por descenso recursivo, siguiendo la
 * gramática del sh POSIX:
 *
 *   linea      : e_ou { (';' | '&') e_ou } (nueva línea | fin)
 *   e_ou       : pipeline { ('&&' | '||') pipeline }
 *   pipeline   : ['!'] comando { '|' comando }
 *   comando    : simple | compuesto {redir} | nombre '(' ')' compuesto
 *   compuesto  : '(' lista ')' | '{' lista '}' | if | while | until | for | case
 *
 * Las palabras quedan "crudas" (con comillas, $, \): quien quita las comillas y expande
 * es expansao.c, al momento de ejecutar. El léxico solo necesita saber dónde termina cada
 * palabra: comillas, $( ... ), ${ ... }, $(( ... )) y `...` pueden tener
 * espacios y operadores dentro. Para encontrar el ")" de un $( ... ), un
 * analizador nuevo analiza lo que está dentro (así un "case" con "a)" allí
 * dentro no confunde el conteo).
 *
 * Los errores no usan longjmp: el estado va a "estado" y cada función devuelve
 * NULL. Cuando el texto termina en medio de algo (comillas abiertas, if sin fi,
 * here-doc sin delimitador), el resultado es A_INCOMPLETO y el shell
 * interactivo pide una línea más (PS2). */
#include "sagsh.h"

enum {
    T_PALAVRA, T_NUM_IO, T_NL, T_FIM, T_PV, T_PVPV, T_FUNDO, T_E, T_OU, T_PIPE, T_ABRE, T_FECHA,
    T_MENOR, T_MAIOR, T_MAIOR2, T_MENOR_E, T_MAIOR_E, T_MENOR_MAIOR, T_HD, T_HD_TAB, T_HS,
    T_MAIOR_PIPE, T_TUDO, T_TUDO2
};

static const char *nome_tok[] = {
    "", "", "nueva línea", "", ";", ";;", "&", "&&", "||", "|", "(", ")",
    "<", ">", ">>", "<&", ">&", "<>", "<<", "<<-", "<<<", ">|", "&>", "&>>"
};

#define MAX_PEND 32
#define MAX_PROF 60

typedef struct {
    arena_t    *ar;
    const char *s;
    int         pos, tam, linha;
    bool        fim_entrada;
    int         estado, linha_erro;
    char        perto[48];
    int         tok, tok_ini, tok_linha;
    char       *texto;
    redir_t    *pend[MAX_PEND];
    int         npend;
    int         prof;
} an_t;

static int pular_dolar(const char *s, int i, int tam, int prof);
static int fecha_subst(const char *s, int ini, int tam, int prof);

/* ------------------------------------------------------------ errores */

static void incompleto(an_t *p)
{
    if (p->estado != A_OK) return;
    p->estado = p->fim_entrada ? A_ERRO : A_INCOMPLETO;
    p->linha_erro = p->linha;
    p->perto[0] = 0;   /* vacío = el texto terminó */
}

static void erro_em(an_t *p, const char *perto)
{
    if (p->estado != A_OK) return;
    p->estado = A_ERRO;
    p->linha_erro = p->tok_linha;
    copia_texto(p->perto, perto, sizeof p->perto);
}

static void erro_tok(an_t *p)
{
    if (p->tok == T_FIM) { incompleto(p); return; }
    erro_em(p, (p->tok == T_PALAVRA || p->tok == T_NUM_IO) ? p->texto : nome_tok[p->tok]);
}

/* ------------------------------------------------------------ fin de las estructuras */

static int pular_simples(const char *s, int i, int tam)   /* s[i] = '\'' */
{
    for (i++; i < tam && s[i] != '\''; i++) ;
    return i < tam ? i + 1 : -1;
}

static int pular_crase(const char *s, int i, int tam)     /* s[i] = '`' */
{
    for (i++; i < tam && s[i] != '`'; i++)
        if (s[i] == '\\') i++;
    return i < tam ? i + 1 : -1;
}

static int pular_duplas(const char *s, int i, int tam, int prof)   /* s[i] = '"' */
{
    i++;
    while (i < tam && s[i] != '"') {
        if (s[i] == '\\') { i += 2; continue; }
        if (s[i] == '$' || s[i] == '`') {
            int r = pular_dolar(s, i, tam, prof);
            if (r < 0) return r;
            i = r > i ? r : i + 1;
            continue;
        }
        i++;
    }
    return i < tam ? i + 1 : -1;
}

/* ${ ... }: s[i] es el primer carácter después de la "{". */
static int pular_chaves(const char *s, int i, int tam, int prof)
{
    while (i < tam) {
        char c = s[i];
        int r = -3;
        if (c == '\\') { i += 2; continue; }
        if (c == '}') return i + 1;
        if (c == '\'') r = pular_simples(s, i, tam);
        else if (c == '"') r = pular_duplas(s, i, tam, prof);
        else if (c == '$' || c == '`') r = pular_dolar(s, i, tam, prof);
        if (r == -3) { i++; continue; }
        if (r < 0) return r;
        i = r > i ? r : i + 1;
    }
    return -1;
}

/* $(( ... )): s[i] es el primer carácter después del "$((". -3 = no era
 * aritmética (ej.: "$( (cd x; ls) )"). */
static int pular_aritmetica(const char *s, int i, int tam, int prof)
{
    int nivel = 0;
    while (i < tam) {
        char c = s[i];
        if (c == '(') nivel++;
        else if (c == ')') {
            if (nivel == 0) {
                if (i + 1 >= tam) return -1;
                return s[i + 1] == ')' ? i + 2 : -3;
            }
            nivel--;
        } else if (c == '\\') {
            i++;
        } else if (c == '\'' || c == '"' || c == '$' || c == '`') {
            int r = c == '\'' ? pular_simples(s, i, tam) : c == '"' ? pular_duplas(s, i, tam, prof)
                                                                    : pular_dolar(s, i, tam, prof);
            if (r < 0) return r;
            if (r > i) { i = r; continue; }
        }
        i++;
    }
    return -1;
}

/* s[i] es '$' o '`'. Devuelve la posición después de la estructura, o i + 1 si el
 * $ es solo un carácter (el nombre que viene después es texto común para el léxico). */
static int pular_dolar(const char *s, int i, int tam, int prof)
{
    if (prof > MAX_PROF) return -2;
    if (s[i] == '`') return pular_crase(s, i, tam);
    if (i + 1 >= tam) return i + 1;
    char c = s[i + 1];
    if (c == '(') {
        if (i + 2 < tam && s[i + 2] == '(') {
            int r = pular_aritmetica(s, i + 3, tam, prof + 1);
            if (r != -3) return r;
        }
        if (i + 2 >= tam) return -1;
        int f = fecha_subst(s, i + 2, tam, prof + 1);
        return f < 0 ? f : f + 1;
    }
    if (c == '{') return pular_chaves(s, i + 2, tam, prof + 1);
    return i + 1;
}

static bool eh_meta(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == ';' || c == '&' || c == '|' ||
           c == '(' || c == ')' || c == '<' || c == '>';
}

/* Fin de una palabra que empieza en i. -1 = incompleta, -2 = error. */
static int fim_palavra(an_t *p, int i)
{
    const char *s = p->s;
    while (i < p->tam) {
        char c = s[i];
        if (eh_meta(c)) break;
        int r;
        if (c == '\\') {
            if (i + 1 >= p->tam) return -1;
            i += 2;
            continue;
        } else if (c == '\'') {
            r = pular_simples(s, i, p->tam);
        } else if (c == '"') {
            r = pular_duplas(s, i, p->tam, p->prof);
        } else if (c == '$' || c == '`') {
            r = pular_dolar(s, i, p->tam, p->prof);
        } else {
            i++;
            continue;
        }
        if (r < 0) return r;
        i = r > i ? r : i + 1;
    }
    return i;
}

/* ------------------------------------------------------------ léxico */

static void tirar_aspas_delim(const char *w, texto_t *t, bool *tinha)
{
    *tinha = false;
    for (const char *c = w; *c; c++) {
        if (*c == '\'' || *c == '"') { *tinha = true; continue; }
        if (*c == '\\' && c[1]) { *tinha = true; c++; }
        tx_car(t, *c);
    }
    tx_texto(t);
}

/* Lee los cuerpos de los here-docs pendientes, que empiezan en la línea siguiente. */
static void ler_heredocs(an_t *p)
{
    for (int k = 0; k < p->npend && p->estado == A_OK; k++) {
        redir_t *r = p->pend[k];
        texto_t delim = {0}, corpo = {0};
        bool tinha;
        tirar_aspas_delim(r->alvo, &delim, &tinha);
        r->expandir = !tinha;
        for (;;) {
            if (p->pos >= p->tam) {
                if (!p->fim_entrada) incompleto(p);
                break;   /* al final del script el here-doc termina aquí (como bash, sin drama) */
            }
            int ini = p->pos, e = ini;
            while (e < p->tam && p->s[e] != '\n') e++;
            if (e >= p->tam && !p->fim_entrada) { incompleto(p); break; }
            p->pos = e < p->tam ? e + 1 : e;
            p->linha++;
            int a = ini;
            if (r->tira_tab) while (a < e && p->s[a] == '\t') a++;
            if (e - a == delim.n && memcmp(p->s + a, delim.s, (size_t)delim.n) == 0) break;
            tx_junta_n(&corpo, p->s + a, e - a);
            tx_car(&corpo, '\n');
        }
        r->corpo = arena_dup_n(p->ar, tx_texto(&corpo), corpo.n);
        tx_liberar(&delim);
        tx_liberar(&corpo);
    }
    p->npend = 0;
}

/* Copia la palabra cruda, sin las continuaciones de línea (\ + nueva línea). */
static char *copiar_palavra(an_t *p, int ini, int fim)
{
    char *w = arena_aloca(p->ar, (size_t)(fim - ini) + 1);
    int k = 0;
    char aspa = 0;
    for (int i = ini; i < fim; i++) {
        char c = p->s[i];
        if (c == '\n') p->linha++;
        if (aspa == '\'') { if (c == '\'') aspa = 0; w[k++] = c; continue; }
        if (c == '\'' && !aspa) { aspa = '\''; w[k++] = c; continue; }
        if (c == '"') aspa = aspa ? 0 : '"';
        if (c == '\\' && i + 1 < fim) {
            if (p->s[i + 1] == '\n') { i++; p->linha++; continue; }
            w[k++] = c;
            w[k++] = p->s[++i];
            continue;
        }
        w[k++] = c;
    }
    w[k] = 0;
    return w;
}

static void lex(an_t *p, bool heredocs)
{
    if (p->estado != A_OK) { p->tok = T_FIM; return; }
    const char *s = p->s;
    for (;;) {
        while (p->pos < p->tam && (s[p->pos] == ' ' || s[p->pos] == '\t')) p->pos++;
        if (p->pos < p->tam && s[p->pos] == '\\' && p->pos + 1 < p->tam && s[p->pos + 1] == '\n') {
            p->pos += 2;
            p->linha++;
            continue;
        }
        if (p->pos < p->tam && s[p->pos] == '\\' && p->pos + 1 >= p->tam) {
            incompleto(p);   /* "\" al final: continúa en la línea siguiente */
            p->tok = T_FIM;
            return;
        }
        if (p->pos < p->tam && s[p->pos] == '#') {
            while (p->pos < p->tam && s[p->pos] != '\n') p->pos++;
            continue;
        }
        break;
    }
    p->tok_ini = p->pos;
    p->tok_linha = p->linha;
    p->texto = NULL;
    if (p->pos >= p->tam) {
        if (heredocs && p->npend) ler_heredocs(p);
        p->tok = T_FIM;
        return;
    }
    char c = s[p->pos], d = p->pos + 1 < p->tam ? s[p->pos + 1] : 0;
    char e = p->pos + 2 < p->tam ? s[p->pos + 2] : 0;
    int n = 1;
    switch (c) {
    case '\n':
        p->pos++;
        p->linha++;
        p->tok = T_NL;
        if (heredocs && p->npend) ler_heredocs(p);
        return;
    case ';': if (d == ';') { p->tok = T_PVPV; n = 2; } else p->tok = T_PV; break;
    case '&':
        if (d == '&') { p->tok = T_E; n = 2; }
        else if (d == '>' && e == '>') { p->tok = T_TUDO2; n = 3; }
        else if (d == '>') { p->tok = T_TUDO; n = 2; }
        else p->tok = T_FUNDO;
        break;
    case '|': if (d == '|') { p->tok = T_OU; n = 2; } else p->tok = T_PIPE; break;
    case '(': p->tok = T_ABRE; break;
    case ')': p->tok = T_FECHA; break;
    case '<':
        if (d == '<' && e == '<') { p->tok = T_HS; n = 3; }
        else if (d == '<' && e == '-') { p->tok = T_HD_TAB; n = 3; }
        else if (d == '<') { p->tok = T_HD; n = 2; }
        else if (d == '&') { p->tok = T_MENOR_E; n = 2; }
        else if (d == '>') { p->tok = T_MENOR_MAIOR; n = 2; }
        else p->tok = T_MENOR;
        break;
    case '>':
        if (d == '>') { p->tok = T_MAIOR2; n = 2; }
        else if (d == '&') { p->tok = T_MAIOR_E; n = 2; }
        else if (d == '|') { p->tok = T_MAIOR_PIPE; n = 2; }
        else p->tok = T_MAIOR;
        break;
    default: {
        int fim = fim_palavra(p, p->pos);
        if (fim == -1) { incompleto(p); p->tok = T_FIM; return; }
        if (fim < 0) { erro_em(p, "$("); p->tok = T_FIM; return; }
        p->texto = copiar_palavra(p, p->pos, fim);
        p->tok = T_PALAVRA;
        bool digitos = true;
        for (int i = p->pos; i < fim; i++)
            if (s[i] < '0' || s[i] > '9') { digitos = false; break; }
        if (digitos && fim - p->pos < 3 && fim < p->tam && (s[fim] == '<' || s[fim] == '>'))
            p->tok = T_NUM_IO;
        p->pos = fim;
        return;
    }
    }
    p->pos += n;
}

static void avancar(an_t *p) { lex(p, true); }

/* Mira el próximo símbolo sin consumirlo (sin leer here-docs). */
static int espiar(an_t *p)
{
    an_t copia = *p;
    lex(p, false);
    int t = p->tok;
    *p = copia;
    return t;
}

/* ------------------------------------------------------------ analizador */

static const char *reservadas[] = {
    "if", "then", "else", "elif", "fi", "do", "done", "case", "esac", "while", "until",
    "for", "in", "{", "}", "!", "function", NULL
};

bool eh_palavra_reservada(const char *s)
{
    for (int i = 0; reservadas[i]; i++)
        if (compara_texto(s, reservadas[i]) == 0) return true;
    return false;
}

static bool eh(an_t *p, const char *w) { return p->tok == T_PALAVRA && compara_texto(p->texto, w) == 0; }

static bool fim_de_lista(an_t *p)
{
    if (p->tok == T_FIM || p->tok == T_FECHA || p->tok == T_PVPV) return true;
    return eh(p, "then") || eh(p, "else") || eh(p, "elif") || eh(p, "fi") || eh(p, "do") ||
           eh(p, "done") || eh(p, "esac") || eh(p, "}");
}

static void pular_nl(an_t *p)
{
    while (p->tok == T_NL) avancar(p);
}

static bool exigir(an_t *p, const char *w)
{
    if (p->estado != A_OK) return false;
    if (eh(p, w)) { avancar(p); return true; }
    erro_tok(p);
    return false;
}

static no_t *novo(an_t *p, int tipo)
{
    no_t *n = arena_aloca(p->ar, sizeof(no_t));
    n->tipo = tipo;
    n->linha = p->tok_linha;
    return n;
}

/* Vector temporal de punteros, copiado a la arena al final. */
typedef struct { void **v; int n, cap; } vet_t;

static void vet_poe(vet_t *v, void *x)
{
    if (v->n == v->cap) {
        v->cap = v->cap ? v->cap * 2 : 8;
        v->v = sh_realoca(v->v, (size_t)v->cap * sizeof(void *));
    }
    v->v[v->n++] = x;
}

static void **vet_arena(an_t *p, vet_t *v)
{
    void **r = arena_aloca(p->ar, (size_t)(v->n + 1) * sizeof(void *));
    for (int i = 0; i < v->n; i++) r[i] = v->v[i];
    r[v->n] = NULL;
    libera(v->v);
    v->v = NULL;
    return r;
}

static no_t *e_ou(an_t *p);
static no_t *comando(an_t *p);

static bool eh_tok_redir(int t)
{
    return t == T_NUM_IO || (t >= T_MENOR && t <= T_TUDO2);
}

static bool redirecao(an_t *p, redir_t **lista)
{
    int fd = -1;
    if (p->tok == T_NUM_IO) { fd = para_int(p->texto); avancar(p); }
    int t = p->tok;
    avancar(p);
    if (p->tok != T_PALAVRA) { erro_tok(p); return false; }
    redir_t *r = arena_aloca(p->ar, sizeof(redir_t));
    r->fd = fd;
    r->alvo = p->texto;
    switch (t) {
    case T_MENOR:       r->tipo = R_ENTRADA; break;
    case T_MAIOR:       r->tipo = R_SAIDA; break;
    case T_MAIOR_PIPE:  r->tipo = R_FORCAR; break;
    case T_MAIOR2:      r->tipo = R_ACRESCENTA; break;
    case T_MENOR_MAIOR: r->tipo = R_LER_ESCREVER; break;
    case T_MENOR_E:     r->tipo = R_DUP_ENTRADA; break;
    case T_MAIOR_E:     r->tipo = R_DUP_SAIDA; break;
    case T_HS:          r->tipo = R_HERESTRING; break;
    case T_TUDO:        r->tipo = R_TUDO; break;
    case T_TUDO2:       r->tipo = R_TUDO_ACRESCENTA; break;
    default:            r->tipo = R_HEREDOC; r->tira_tab = t == T_HD_TAB; break;
    }
    if (r->tipo == R_HEREDOC) {
        if (p->npend == MAX_PEND) { erro_em(p, "<<"); return false; }
        p->pend[p->npend++] = r;
    }
    while (*lista) lista = &(*lista)->prox;
    *lista = r;
    avancar(p);
    return true;
}

static void redirecoes_depois(an_t *p, no_t *n)
{
    while (p->estado == A_OK && eh_tok_redir(p->tok))
        if (!redirecao(p, &n->redir)) return;
}

/* Lista dentro de un comando compuesto, hasta un terminador. */
static no_t *lista_composta(an_t *p, bool pode_vazia)
{
    vet_t itens = {0}, fundos = {0};
    no_t *l = novo(p, N_LISTA);
    pular_nl(p);
    while (p->estado == A_OK && !fim_de_lista(p)) {
        no_t *c = e_ou(p);
        if (!c) break;
        bool fundo = false;
        if (p->tok == T_FUNDO) { fundo = true; avancar(p); }
        else if (p->tok == T_PV) avancar(p);
        else if (p->tok != T_NL && !fim_de_lista(p)) { erro_tok(p); break; }
        vet_poe(&itens, c);
        vet_poe(&fundos, (void *)(size_t)fundo);
        pular_nl(p);
    }
    if (p->estado != A_OK) { libera(itens.v); libera(fundos.v); return NULL; }
    if (itens.n == 0 && !pode_vazia) {
        libera(itens.v); libera(fundos.v);
        erro_tok(p);
        return NULL;
    }
    if (itens.n == 1 && !fundos.v[0]) {
        no_t *c = itens.v[0];
        libera(itens.v); libera(fundos.v);
        return c;
    }
    l->nfilhos = itens.n;
    l->fundo = arena_aloca(p->ar, (size_t)itens.n + 1);
    for (int i = 0; i < itens.n; i++) l->fundo[i] = fundos.v[i] != NULL;
    libera(fundos.v);
    l->filhos = (no_t **)vet_arena(p, &itens);
    return l;
}

static no_t *se_corpo(an_t *p)   /* después de "if" o "elif" */
{
    no_t *n = novo(p, N_SE);
    if (!(n->a = lista_composta(p, false)) || !exigir(p, "then")) return NULL;
    if (!(n->b = lista_composta(p, false))) return NULL;
    if (eh(p, "elif")) {
        avancar(p);
        n->c = se_corpo(p);   /* el "fi" se consume allí dentro */
        return n->c ? n : NULL;
    }
    if (eh(p, "else")) {
        avancar(p);
        if (!(n->c = lista_composta(p, false))) return NULL;
    }
    return exigir(p, "fi") ? n : NULL;
}

static no_t *enquanto(an_t *p, int tipo)
{
    no_t *n = novo(p, tipo);
    avancar(p);
    if (!(n->a = lista_composta(p, false)) || !exigir(p, "do")) return NULL;
    if (!(n->b = lista_composta(p, false)) || !exigir(p, "done")) return NULL;
    return n;
}

static no_t *para(an_t *p)
{
    no_t *n = novo(p, N_PARA);
    avancar(p);
    if (p->tok != T_PALAVRA || !eh_nome(p->texto)) { erro_tok(p); return NULL; }
    n->nome = p->texto;
    avancar(p);
    pular_nl(p);
    if (eh(p, "in")) {
        n->com_lista = true;
        avancar(p);
        vet_t pal = {0};
        while (p->tok == T_PALAVRA) { vet_poe(&pal, p->texto); avancar(p); }
        n->npal = pal.n;
        n->palavras = (char **)vet_arena(p, &pal);
        if (p->tok == T_PV) avancar(p);
        else if (p->tok != T_NL) { erro_tok(p); return NULL; }
    } else if (p->tok == T_PV) {
        avancar(p);
    }
    pular_nl(p);
    if (!exigir(p, "do")) return NULL;
    if (!(n->b = lista_composta(p, false)) || !exigir(p, "done")) return NULL;
    return n;
}

static no_t *caso(an_t *p)
{
    no_t *n = novo(p, N_CASO);
    avancar(p);
    if (p->tok != T_PALAVRA) { erro_tok(p); return NULL; }
    n->nome = p->texto;
    avancar(p);
    pular_nl(p);
    if (!exigir(p, "in")) return NULL;
    pular_nl(p);
    item_caso_t **fim = &n->itens;
    while (p->estado == A_OK && !eh(p, "esac")) {
        if (p->tok == T_ABRE) avancar(p);
        vet_t pads = {0};
        for (;;) {
            if (p->tok != T_PALAVRA) { libera(pads.v); erro_tok(p); return NULL; }
            vet_poe(&pads, p->texto);
            avancar(p);
            if (p->tok != T_PIPE) break;
            avancar(p);
        }
        if (p->tok != T_FECHA) { libera(pads.v); erro_tok(p); return NULL; }
        avancar(p);
        item_caso_t *it = arena_aloca(p->ar, sizeof *it);
        it->npadroes = pads.n;
        it->padroes = (char **)vet_arena(p, &pads);
        it->corpo = lista_composta(p, true);
        if (p->estado != A_OK) return NULL;
        *fim = it;
        fim = &it->prox;
        if (p->tok == T_PVPV) { avancar(p); pular_nl(p); }
        else if (!eh(p, "esac")) { erro_tok(p); return NULL; }
    }
    return exigir(p, "esac") ? n : NULL;
}

static no_t *definir_funcao(an_t *p, char *nome)
{
    no_t *n = novo(p, N_FUNCAO);
    n->nome = nome;
    n->arena = p->ar;
    pular_nl(p);
    no_t *corpo = comando(p);
    if (!corpo) return NULL;
    if (corpo->tipo == N_SIMPLES || corpo->tipo == N_FUNCAO) { erro_em(p, nome); return NULL; }
    n->a = corpo;
    return n;
}

static bool eh_atribuicao(const char *w)
{
    const char *igual = acha_char(w, '=');
    if (!igual || igual == w) return false;
    int n = (int)(igual - w);
    if (w[n - 1] == '+') n--;
    return eh_nome_n(w, n);
}

static no_t *simples(an_t *p)
{
    no_t *n = novo(p, N_SIMPLES);
    vet_t pal = {0}, atr = {0};
    for (;;) {
        if (p->estado != A_OK) break;
        if (p->tok == T_PALAVRA) {
            if (pal.n == 0 && eh_atribuicao(p->texto)) {
                vet_poe(&atr, p->texto);
            } else {
                if (pal.n == 0 && atr.n == 0 && !n->redir && eh_nome(p->texto) && espiar(p) == T_ABRE) {
                    char *nome = p->texto;
                    avancar(p);
                    avancar(p);
                    if (p->tok != T_FECHA) { libera(pal.v); libera(atr.v); erro_tok(p); return NULL; }
                    avancar(p);
                    libera(pal.v); libera(atr.v);
                    return definir_funcao(p, nome);
                }
                vet_poe(&pal, p->texto);
            }
            avancar(p);
        } else if (eh_tok_redir(p->tok)) {
            if (!redirecao(p, &n->redir)) break;
        } else {
            break;
        }
    }
    if (p->estado != A_OK) { libera(pal.v); libera(atr.v); return NULL; }
    n->npal = pal.n;
    n->natr = atr.n;
    n->palavras = (char **)vet_arena(p, &pal);
    n->atribs = (char **)vet_arena(p, &atr);
    return n;
}

static no_t *comando(an_t *p)
{
    if (p->estado != A_OK) return NULL;
    if (++p->prof > MAX_PROF) { erro_em(p, "(demasiado anidado)"); return NULL; }
    no_t *n = NULL;
    if (p->tok == T_ABRE) {
        n = novo(p, N_SUBSHELL);
        avancar(p);
        if ((n->a = lista_composta(p, false)) && p->tok == T_FECHA) avancar(p);
        else { erro_tok(p); n = NULL; }
    } else if (p->tok == T_PALAVRA && eh_palavra_reservada(p->texto) && !eh(p, "in")) {
        if (eh(p, "if")) { avancar(p); n = se_corpo(p); }
        else if (eh(p, "while")) n = enquanto(p, N_ENQUANTO);
        else if (eh(p, "until")) n = enquanto(p, N_ATE);
        else if (eh(p, "for")) n = para(p);
        else if (eh(p, "case")) n = caso(p);
        else if (eh(p, "{")) {
            n = novo(p, N_GRUPO);
            avancar(p);
            if (!(n->a = lista_composta(p, false)) || !exigir(p, "}")) n = NULL;
        } else if (eh(p, "function")) {
            avancar(p);
            if (p->tok != T_PALAVRA || !eh_nome(p->texto)) { erro_tok(p); p->prof--; return NULL; }
            char *nome = p->texto;
            avancar(p);
            if (p->tok == T_ABRE) {
                avancar(p);
                if (p->tok != T_FECHA) { erro_tok(p); p->prof--; return NULL; }
                avancar(p);
            }
            p->prof--;
            return definir_funcao(p, nome);
        } else {
            erro_tok(p);   /* then, fi, done... fuera de lugar */
        }
    } else if (p->tok == T_PALAVRA || eh_tok_redir(p->tok)) {
        p->prof--;
        return simples(p);
    } else {
        erro_tok(p);
    }
    p->prof--;
    if (!n || p->estado != A_OK) return NULL;
    redirecoes_depois(p, n);
    return p->estado == A_OK ? n : NULL;
}

static no_t *pipeline(an_t *p)
{
    bool negar = false;
    if (eh(p, "!")) { negar = true; avancar(p); }
    no_t *c = comando(p);
    if (!c) return NULL;
    if (p->tok != T_PIPE && !negar) return c;
    vet_t et = {0};
    vet_poe(&et, c);
    while (p->tok == T_PIPE) {
        avancar(p);
        pular_nl(p);
        if (!(c = comando(p))) { libera(et.v); return NULL; }
        vet_poe(&et, c);
    }
    no_t *n = novo(p, N_PIPE);
    n->negar = negar;
    n->nfilhos = et.n;
    n->filhos = (no_t **)vet_arena(p, &et);
    return n;
}

static no_t *e_ou(an_t *p)
{
    no_t *esq = pipeline(p);
    while (esq && (p->tok == T_E || p->tok == T_OU)) {
        int t = p->tok;
        avancar(p);
        pular_nl(p);
        no_t *dir = pipeline(p);
        if (!dir) return NULL;
        no_t *n = novo(p, t == T_E ? N_E : N_OU);
        n->a = esq;
        n->b = dir;
        esq = n;
    }
    return esq;
}

resultado_t analisar(arena_t *ar, const char *s, int ini, int linha, bool fim_entrada)
{
    an_t p;
    memset(&p, 0, sizeof p);
    p.ar = ar;
    p.s = s;
    p.pos = ini;
    p.tam = (int)tam_texto(s);
    p.linha = linha;
    p.fim_entrada = fim_entrada;
    resultado_t r;
    memset(&r, 0, sizeof r);
    avancar(&p);
    pular_nl(&p);
    if (p.estado == A_OK && p.tok == T_FIM) {
        r.estado = A_VAZIO;
        r.fim = p.pos;
        return r;
    }
    vet_t itens = {0}, fundos = {0};
    int linha_ini = p.tok_linha;
    while (p.estado == A_OK) {
        no_t *c = e_ou(&p);
        if (!c) break;
        bool fundo = false, sep = false;
        if (p.tok == T_FUNDO) { fundo = sep = true; avancar(&p); }
        else if (p.tok == T_PV) { sep = true; avancar(&p); }
        vet_poe(&itens, c);
        vet_poe(&fundos, (void *)(size_t)fundo);
        if (p.tok == T_NL || p.tok == T_FIM) break;
        if (!sep || fim_de_lista(&p) || (p.tok != T_PALAVRA && p.tok != T_ABRE && !eh_tok_redir(p.tok))) {
            erro_tok(&p);
            break;
        }
    }
    if (p.estado != A_OK) {
        libera(itens.v); libera(fundos.v);
        r.estado = p.estado;
        r.linha_erro = p.linha_erro;
        copia_texto(r.perto, p.perto, sizeof r.perto);
        r.fim = p.pos;
        return r;
    }
    no_t *l;
    if (itens.n == 1 && !fundos.v[0]) {
        l = itens.v[0];
        libera(itens.v);
    } else {
        l = arena_aloca(ar, sizeof(no_t));
        l->tipo = N_LISTA;
        l->nfilhos = itens.n;
        l->fundo = arena_aloca(ar, (size_t)itens.n + 1);
        for (int i = 0; i < itens.n; i++) l->fundo[i] = fundos.v[i] != NULL;
        l->filhos = (no_t **)vet_arena(&p, &itens);
    }
    l->linha = linha_ini;
    libera(fundos.v);
    r.estado = A_OK;
    r.no = l;
    r.fim = p.pos;
    return r;
}

static int fecha_subst(const char *s, int ini, int tam, int prof)
{
    an_t q;
    memset(&q, 0, sizeof q);
    q.ar = arena_nova();
    q.s = s;
    q.pos = ini;
    q.tam = tam;
    q.prof = prof;
    avancar(&q);
    lista_composta(&q, true);
    int r;
    if (q.estado == A_INCOMPLETO) r = -1;
    else if (q.estado != A_OK) r = -2;
    else if (q.tok == T_FECHA) r = q.tok_ini;
    else if (q.tok == T_FIM) r = -1;
    else r = -2;
    arena_liberar(q.ar);
    return r;
}

int achar_fecha_subst(const char *s, int ini)
{
    return fecha_subst(s, ini, (int)tam_texto(s), 0);
}
