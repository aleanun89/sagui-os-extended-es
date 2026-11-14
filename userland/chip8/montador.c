// SPDX-License-Identifier: GPL-2.0-or-later
/* montador.c - o montador de CHIP-8 / SUPER-CHIP / XO-CHIP (D071).
 *
 * Duas passadas sobre o texto: a primeira só conta bytes e anota o
 * endereço das etiquetas; a segunda avalia as expressões e grava. Os
 * tamanhos nunca dependem de uma expressão (toda instrução tem 2 bytes, o
 * "ld i, long" tem 4, db/dw contam os itens), menos org/ds/align, que por
 * isso só aceitam símbolos já definidos.
 *
 * Sintaxe (ver /ayuda/chip8.md): mnemônicos da referência do Cowgod com as
 * extensões do SCHIP e do XO-CHIP, um comando por linha, comentário com ;
 * ou #, "etiqueta:", "nome = expr" (ou "nome equ expr"), números 255,
 * 0xFF, 0b1010, %1010 ou %#.#. (o ponto é 0 e X é 1, bom para sprites),
 * 'A', $ (endereço da linha), + - * / & | ^ << >> ~ e parênteses.
 * Diretivas: db, dw, ds, org, align, include.
 *
 * Os erros saem como "arquivo:linha: mensagem", em espanhol. */
#include "chip8.h"
#include "montador.h"

#define MAX_SIMBOLOS 1024
#define MAX_NOME 32
#define MAX_LINHA 256
#define MAX_ERROS 30
#define MAX_PROFUNDIDADE 8

typedef struct {
    char nome[MAX_NOME];
    int  valor;
    bool definido;
    bool constante;
} simbolo_t;

typedef struct {
    int        passo;
    int        pc;
    int        pc_linha;        /* $ */
    uint8_t   *saida;           /* 64 KB; a ROM é saida[0x200 .. fim) */
    int        fim;
    int        erros;
    simbolo_t *sim;
    int        qtd;
    const char *arq;
    int        linha;
    int        profundidade;
    bool       indefinido;      /* a expressão usou um símbolo ainda sem valor */
} montador_t;

static montador_t M;

#ifdef SAGUI_HOST
#include <stdarg.h>
#define formatar_lista vsnprintf
#endif

static void erro_lista(const char *fmt, va_list ap)
{
    M.erros++;
    if (M.erros > MAX_ERROS) return;
    char b[200];
    formatar_lista(b, sizeof b, fmt, ap);
    c8_errof("%s:%d: %s\n", M.arq, M.linha, b);
    if (M.erros == MAX_ERROS) c8_errof("%s: demasiados errores, no se muestran más\n", M.arq);
}

/* Quase todo erro aparece igual nas duas passadas: só a segunda mostra,
 * para cada um sair uma vez e todos de uma vez (inclusive os símbolos não
 * definidos, que a primeira passada ainda não pode saber). */
static void erro(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void erro(const char *fmt, ...)
{
    if (M.passo == 1) return;
    va_list ap;
    va_start(ap, fmt);
    erro_lista(fmt, ap);
    va_end(ap);
}

/* Os que só a primeira passada enxerga (etiqueta repetida, org com símbolo
 * ainda sem valor): mostrados na hora. */
static void erro1(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void erro1(const char *fmt, ...)
{
    if (M.passo != 1) return;
    va_list ap;
    va_start(ap, fmt);
    erro_lista(fmt, ap);
    va_end(ap);
}

/* ------------------------------------------------------------ texto */

static bool espaco(int c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }
static int minusc(int c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }
static bool letra_nome(int c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c == '.'; }
static bool digito(int c) { return c >= '0' && c <= '9'; }

static char *aparar(char *s)
{
    while (espaco(*s)) s++;
    int n = (int)tam_texto(s);
    while (n > 0 && espaco(s[n - 1])) s[--n] = 0;
    return s;
}

static bool igual_sem_caixa(const char *a, const char *b)
{
    while (*a && *b) if (minusc(*a++) != minusc(*b++)) return false;
    return *a == *b;
}

static void junta(char *d, const char *s, size_t max)
{
    size_t n = tam_texto(d);
    while (*s && n + 1 < max) d[n++] = *s++;
    d[n] = 0;
}

/* Corta o comentário (; ou #) que não esteja dentro de aspas. */
static void tirar_comentario(char *s)
{
    char aspa = 0;
    char *ini = s;
    for (; *s; s++) {
        if (aspa) {
            if (*s == '\\' && s[1]) s++;
            else if (*s == aspa) aspa = 0;
        } else if (*s == '"' || (*s == '\'' && s[1] && s[2] == '\'')) {
            aspa = *s;
        } else if (*s == ';' || *s == '#') {
            /* "%#.#." é número binário, não comentário */
            char *p = s - 1;
            while (p >= ini && (*p == '#' || *p == '.' || *p == 'X' || *p == '0' || *p == '1')) p--;
            if (*s == '#' && p >= ini && *p == '%') continue;
            *s = 0;
            return;
        }
    }
}

/* Separa os operandos pelas vírgulas de fora de aspas e parênteses. */
static int separar(char *s, char **ops, int max)
{
    int n = 0, nivel = 0;
    char aspa = 0;
    s = aparar(s);
    if (!*s) return 0;
    ops[n++] = s;
    for (; *s; s++) {
        if (aspa) { if (*s == '\\' && s[1]) s++; else if (*s == aspa) aspa = 0; continue; }
        if (*s == '"' || *s == '\'') aspa = *s;
        else if (*s == '(') nivel++;
        else if (*s == ')') nivel--;
        else if (*s == ',' && nivel == 0) {
            *s = 0;
            if (n == max) return -1;
            ops[n++] = s + 1;
        }
    }
    for (int k = 0; k < n; k++) ops[k] = aparar(ops[k]);
    return n;
}

/* ------------------------------------------------------------ símbolos */

static simbolo_t *achar(const char *nome)
{
    for (int k = 0; k < M.qtd; k++)
        if (compara_texto(M.sim[k].nome, nome) == 0) return &M.sim[k];
    return NULL;
}

static simbolo_t *novo(const char *nome)
{
    simbolo_t *s = achar(nome);
    if (s) return s;
    if (M.qtd == MAX_SIMBOLOS) { erro("demasiados símbolos"); return NULL; }
    s = &M.sim[M.qtd++];
    copia_texto(s->nome, nome, sizeof s->nome);
    s->definido = false;
    s->constante = false;
    return s;
}

static const char *const reservados[] = { "i", "dt", "st", "k", "f", "hf", "b", "r", "long", 0 };

static bool nome_reservado(const char *n)
{
    if ((n[0] == 'v' || n[0] == 'V') && n[1] && !n[2] &&
        (digito(n[1]) || (minusc(n[1]) >= 'a' && minusc(n[1]) <= 'f'))) return true;
    for (int k = 0; reservados[k]; k++) if (igual_sem_caixa(n, reservados[k])) return true;
    return false;
}

/* ------------------------------------------------------------ expressões */

static int expr_ou(const char **p);

static void pular_espaco(const char **p) { while (espaco(**p)) (*p)++; }

static int numero(const char **p)
{
    const char *s = *p;
    int v = 0;
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        s += 2;
        if (!((*s >= '0' && *s <= '9') || (minusc(*s) >= 'a' && minusc(*s) <= 'f'))) erro("número hexadecimal vacío");
        for (;; s++) {
            int c = minusc(*s);
            if (digito(c)) v = v * 16 + c - '0';
            else if (c >= 'a' && c <= 'f') v = v * 16 + c - 'a' + 10;
            else if (c == '_') continue;
            else break;
        }
    } else if (s[0] == '0' && (s[1] == 'b' || s[1] == 'B') && (s[2] == '0' || s[2] == '1')) {
        for (s += 2; *s == '0' || *s == '1' || *s == '_'; s++) if (*s != '_') v = v * 2 + (*s - '0');
    } else if (s[0] == '%') {
        s++;
        if (!(*s == '0' || *s == '1' || *s == '.' || *s == '#' || *s == 'X')) erro("número binario vacío");
        for (; *s == '0' || *s == '1' || *s == '.' || *s == '#' || *s == 'X'; s++)
            v = v * 2 + (*s == '1' || *s == '#' || *s == 'X');
    } else {
        while (digito(*s)) v = v * 10 + (*s++ - '0');
    }
    if (letra_nome(*s) || digito(*s)) erro("número mal escrito");
    while (letra_nome(*s) || digito(*s)) s++;
    *p = s;
    return v;
}

static int primario(const char **p)
{
    pular_espaco(p);
    const char *s = *p;
    if (*s == '(') {
        (*p)++;
        int v = expr_ou(p);
        pular_espaco(p);
        if (**p != ')') erro("falta ')'");
        else (*p)++;
        return v;
    }
    if (*s == '-') { (*p)++; return -primario(p); }
    if (*s == '+') { (*p)++; return primario(p); }
    if (*s == '~') { (*p)++; return ~primario(p); }
    if (*s == '$') { (*p)++; return M.pc_linha; }
    if (*s == '\'') {
        int c = (uint8_t)s[1];
        if (!c || s[2] != '\'') { erro("carácter mal escrito"); *p = s + 1; return 0; }
        *p = s + 3;
        return c;
    }
    if (digito(*s) || *s == '%') return numero(p);
    if (letra_nome(*s)) {
        char nome[MAX_NOME];
        int n = 0;
        while (letra_nome(*s) || digito(*s)) { if (n < MAX_NOME - 1) nome[n++] = *s; s++; }
        nome[n] = 0;
        *p = s;
        if (nome_reservado(nome)) { erro("'%s' no es un número ni una etiqueta", nome); return 0; }
        simbolo_t *sb = achar(nome);
        if (sb && sb->definido) return sb->valor;
        if (M.passo == 2) erro("símbolo no definido '%s'", nome);
        M.indefinido = true;
        return 0;
    }
    if (!*s) erro("falta un valor");
    else erro("no se entiende '%c'", *s);
    *p = s + (*s != 0);
    return 0;
}

static int expr_mul(const char **p)
{
    int v = primario(p);
    for (;;) {
        pular_espaco(p);
        if (**p == '*') { (*p)++; v *= primario(p); }
        else if (**p == '/') {
            (*p)++;
            int d = primario(p);
            if (d == 0) { if (!M.indefinido) erro("división por cero"); v = 0; }
            else v /= d;
        } else return v;
    }
}

static int expr_soma(const char **p)
{
    int v = expr_mul(p);
    for (;;) {
        pular_espaco(p);
        if (**p == '+') { (*p)++; v += expr_mul(p); }
        else if (**p == '-') { (*p)++; v -= expr_mul(p); }
        else return v;
    }
}

static int expr_desloc(const char **p)
{
    int v = expr_soma(p);
    for (;;) {
        pular_espaco(p);
        if ((*p)[0] == '<' && (*p)[1] == '<') { *p += 2; v <<= expr_soma(p); }
        else if ((*p)[0] == '>' && (*p)[1] == '>') { *p += 2; v >>= expr_soma(p); }
        else return v;
    }
}

static int expr_e(const char **p)
{
    int v = expr_desloc(p);
    for (;;) { pular_espaco(p); if (**p != '&') return v; (*p)++; v &= expr_desloc(p); }
}

static int expr_xou(const char **p)
{
    int v = expr_e(p);
    for (;;) { pular_espaco(p); if (**p != '^') return v; (*p)++; v ^= expr_e(p); }
}

static int expr_ou(const char **p)
{
    int v = expr_xou(p);
    for (;;) { pular_espaco(p); if (**p != '|') return v; (*p)++; v |= expr_xou(p); }
}

/* Avalia o texto inteiro; erro se sobrar algo. */
static int avaliar(const char *s)
{
    const char *p = s;
    int antes = M.erros;
    int v = expr_ou(&p);
    pular_espaco(&p);
    if (*p && M.erros == antes) erro("sobra texto en la expresión: '%s'", p);
    return v;
}

/* Valor dentro de [min, max] (só confere na segunda passada). */
static int valor(const char *s, int min, int max, const char *oque)
{
    int v = avaliar(s);
    if (M.passo == 2 && (v < min || v > max)) {
        erro("%s fuera de rango: %d (de %d a %d)", oque, v, min, max);
        return 0;
    }
    return v;
}

/* ------------------------------------------------------------ saída */

static void emitir(int b)
{
    if (M.pc < C8_INICIO || M.pc > 0xFFFF) {
        if (M.passo == 2) erro("dirección %x fuera de la memoria (de 200 a ffff)", (unsigned)M.pc);
        M.pc++;
        return;
    }
    if (M.passo == 2) M.saida[M.pc] = (uint8_t)b;
    M.pc++;
    if (M.pc > M.fim) M.fim = M.pc;
}

static void emitir16(int w) { emitir((w >> 8) & 0xFF); emitir(w & 0xFF); }

/* ------------------------------------------------------------ operandos */

enum { OP_VX, OP_I, OP_IND_I, OP_DT, OP_ST, OP_K, OP_F, OP_HF, OP_B, OP_R, OP_LONGO, OP_VALOR };

static int registro(const char *s)
{
    if ((s[0] == 'v' || s[0] == 'V') && s[1] && !s[2]) {
        int c = minusc(s[1]);
        if (digito(c)) return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    }
    return -1;
}

static int tipo_op(const char *s, int *reg)
{
    if ((*reg = registro(s)) >= 0) return OP_VX;
    static const struct { const char *t; int tipo; } esp[] = {
        { "i", OP_I }, { "[i]", OP_IND_I }, { "dt", OP_DT }, { "st", OP_ST }, { "k", OP_K },
        { "f", OP_F }, { "hf", OP_HF }, { "b", OP_B }, { "r", OP_R },
    };
    for (unsigned k = 0; k < sizeof esp / sizeof esp[0]; k++) if (igual_sem_caixa(s, esp[k].t)) return esp[k].tipo;
    if (s[0] == '[') {   /* "[ i ]" com espaços */
        char t[16]; int n = 0;
        for (const char *p = s; *p && n < 15; p++) if (!espaco(*p)) t[n++] = (char)minusc(*p);
        t[n] = 0;
        if (compara_texto(t, "[i]") == 0) return OP_IND_I;
    }
    if (compara_texto_n(s, "long", 4) == 0 && espaco(s[4])) return OP_LONGO;
    if (compara_texto_n(s, "LONG", 4) == 0 && espaco(s[4])) return OP_LONGO;
    return OP_VALOR;
}

/* ------------------------------------------------------------ instruções */

static bool qtd_ops(int n, int min, int max, const char *mn)
{
    if (n >= min && n <= max) return true;
    if (min == max) erro("'%s' necesita %d operando%s", mn, min, min == 1 ? "" : "s");
    else erro("'%s' necesita de %d a %d operandos", mn, min, max);
    return false;
}

static void exigir_reg(int tipo, const char *mn)
{
    if (tipo != OP_VX) erro("'%s': se esperaba un registro (v0 a vf)", mn);
}

/* "vx - vy" (ou "vx, vy") do save/load do XO-CHIP. */
static bool faixa(char **ops, int n, int *a, int *b)
{
    if (n == 2) { *a = registro(ops[0]); *b = registro(ops[1]); return *a >= 0 && *b >= 0; }
    if (n != 1) return false;
    char t[64];
    copia_texto(t, ops[0], sizeof t);
    char *h = t;
    while (*h && *h != '-') h++;
    if (!*h) return false;
    *h = 0;
    *a = registro(aparar(t));
    *b = registro(aparar(h + 1));
    return *a >= 0 && *b >= 0;
}

static bool instrucao(const char *mn, char **ops, int n)
{
    static const struct { const char *mn; int op; } simples[] = {
        { "cls", 0x00E0 }, { "ret", 0x00EE }, { "scr", 0x00FB }, { "scl", 0x00FC }, { "exit", 0x00FD },
        { "low", 0x00FE }, { "high", 0x00FF }, { "audio", 0xF002 },
    };
    for (unsigned k = 0; k < sizeof simples / sizeof simples[0]; k++)
        if (compara_texto(mn, simples[k].mn) == 0) {
            if (qtd_ops(n, 0, 0, mn)) emitir16(simples[k].op); else emitir16(0);
            return true;
        }

    int r[3] = { -1, -1, -1 }, t[3] = { OP_VALOR, OP_VALOR, OP_VALOR };
    for (int k = 0; k < n && k < 3; k++) t[k] = tipo_op(ops[k], &r[k]);
    int x = r[0] < 0 ? 0 : r[0], y = r[1] < 0 ? 0 : r[1];

    if (compara_texto(mn, "scd") == 0 || compara_texto(mn, "scu") == 0) {
        int v = qtd_ops(n, 1, 1, mn) ? valor(ops[0], 1, 15, "desplazamiento") : 0;
        emitir16((mn[2] == 'd' ? 0x00C0 : 0x00D0) | v);
        return true;
    }
    if (compara_texto(mn, "sys") == 0) {
        emitir16(qtd_ops(n, 1, 1, mn) ? valor(ops[0], 0, 0xFFF, "dirección") : 0);
        return true;
    }
    if (compara_texto(mn, "jp") == 0) {
        if (n == 2) {
            if (r[0] != 0) erro("'jp' con dos operandos es 'jp v0, dirección'");
            emitir16(0xB000 | valor(ops[1], 0, 0xFFF, "dirección"));
        } else emitir16(0x1000 | (qtd_ops(n, 1, 1, mn) ? valor(ops[0], 0, 0xFFF, "dirección") : 0));
        return true;
    }
    if (compara_texto(mn, "call") == 0) {
        emitir16(0x2000 | (qtd_ops(n, 1, 1, mn) ? valor(ops[0], 0, 0xFFF, "dirección") : 0));
        return true;
    }
    if (compara_texto(mn, "se") == 0 || compara_texto(mn, "sne") == 0) {
        bool ne = mn[1] == 'n';
        if (!qtd_ops(n, 2, 2, mn)) { emitir16(0); return true; }
        exigir_reg(t[0], mn);
        if (t[1] == OP_VX) emitir16((ne ? 0x9000 : 0x5000) | x << 8 | y << 4);
        else emitir16((ne ? 0x4000 : 0x3000) | x << 8 | (valor(ops[1], -128, 255, "byte") & 0xFF));
        return true;
    }
    if (compara_texto(mn, "ld") == 0) {
        if (!qtd_ops(n, 2, 2, mn)) { emitir16(0); return true; }
        if (t[0] == OP_VX) {
            switch (t[1]) {
            case OP_VX: emitir16(0x8000 | x << 8 | y << 4); return true;
            case OP_DT: emitir16(0xF007 | x << 8); return true;
            case OP_K: emitir16(0xF00A | x << 8); return true;
            case OP_IND_I: emitir16(0xF065 | x << 8); return true;
            case OP_R: emitir16(0xF085 | x << 8); return true;
            case OP_VALOR: emitir16(0x6000 | x << 8 | (valor(ops[1], -128, 255, "byte") & 0xFF)); return true;
            }
        } else if (t[1] == OP_VX) {
            static const int cod[] = { [OP_DT] = 0xF015, [OP_ST] = 0xF018, [OP_F] = 0xF029, [OP_HF] = 0xF030,
                                       [OP_B] = 0xF033, [OP_IND_I] = 0xF055, [OP_R] = 0xF075 };
            if (t[0] < (int)(sizeof cod / sizeof cod[0]) && cod[t[0]]) { emitir16(cod[t[0]] | y << 8); return true; }
        } else if (t[0] == OP_I && t[1] == OP_LONGO) {
            emitir16(0xF000);
            emitir16(valor(ops[1] + 5, 0, 0xFFFF, "dirección") & 0xFFFF);
            return true;
        } else if (t[0] == OP_I && t[1] == OP_VALOR) {
            emitir16(0xA000 | valor(ops[1], 0, 0xFFF, "dirección"));
            return true;
        }
        erro("combinación de operandos de 'ld' no válida");
        emitir16(0);
        return true;
    }
    if (compara_texto(mn, "add") == 0) {
        if (!qtd_ops(n, 2, 2, mn)) { emitir16(0); return true; }
        if (t[0] == OP_I && t[1] == OP_VX) emitir16(0xF01E | y << 8);
        else if (t[0] == OP_VX && t[1] == OP_VX) emitir16(0x8004 | x << 8 | y << 4);
        else if (t[0] == OP_VX && t[1] == OP_VALOR) emitir16(0x7000 | x << 8 | (valor(ops[1], -128, 255, "byte") & 0xFF));
        else { erro("combinación de operandos de 'add' no válida"); emitir16(0); }
        return true;
    }
    static const struct { const char *mn; int n; } logicas[] = {
        { "or", 1 }, { "and", 2 }, { "xor", 3 }, { "sub", 5 }, { "subn", 7 },
    };
    for (unsigned k = 0; k < sizeof logicas / sizeof logicas[0]; k++)
        if (compara_texto(mn, logicas[k].mn) == 0) {
            if (qtd_ops(n, 2, 2, mn)) { exigir_reg(t[0], mn); exigir_reg(t[1], mn); }
            emitir16(0x8000 | x << 8 | y << 4 | logicas[k].n);
            return true;
        }
    if (compara_texto(mn, "shr") == 0 || compara_texto(mn, "shl") == 0) {
        /* Sem o segundo registro, Y = X: dá o mesmo resultado com os dois quirks. */
        if (qtd_ops(n, 1, 2, mn)) { exigir_reg(t[0], mn); if (n == 2) exigir_reg(t[1], mn); }
        if (n < 2) y = x;
        emitir16(0x8000 | x << 8 | y << 4 | (mn[2] == 'r' ? 6 : 0xE));
        return true;
    }
    if (compara_texto(mn, "rnd") == 0) {
        int v = 0;
        if (qtd_ops(n, 2, 2, mn)) { exigir_reg(t[0], mn); v = valor(ops[1], -128, 255, "byte") & 0xFF; }
        emitir16(0xC000 | x << 8 | v);
        return true;
    }
    if (compara_texto(mn, "drw") == 0) {
        int v = 0;
        if (qtd_ops(n, 3, 3, mn)) { exigir_reg(t[0], mn); exigir_reg(t[1], mn); v = valor(ops[2], 0, 15, "altura"); }
        emitir16(0xD000 | x << 8 | y << 4 | v);
        return true;
    }
    if (compara_texto(mn, "skp") == 0 || compara_texto(mn, "sknp") == 0) {
        if (qtd_ops(n, 1, 1, mn)) exigir_reg(t[0], mn);
        emitir16(0xE000 | x << 8 | (mn[2] == 'p' ? 0x9E : 0xA1));
        return true;
    }
    if (compara_texto(mn, "pitch") == 0) {
        if (qtd_ops(n, 1, 1, mn)) exigir_reg(t[0], mn);
        emitir16(0xF03A | x << 8);
        return true;
    }
    if (compara_texto(mn, "plane") == 0) {
        emitir16(0xF001 | (qtd_ops(n, 1, 1, mn) ? valor(ops[0], 0, 15, "plano") : 0) << 8);
        return true;
    }
    if (compara_texto(mn, "save") == 0 || compara_texto(mn, "load") == 0) {
        int a = 0, b = 0;
        if (!faixa(ops, n, &a, &b)) erro("'%s' espera un rango de registros: %s vX - vY", mn, mn);
        emitir16(0x5000 | a << 8 | b << 4 | (mn[0] == 's' ? 2 : 3));
        return true;
    }
    return false;
}

/* ------------------------------------------------------------ diretivas */

static bool montar_arquivo(const char *caminho);

/* Texto entre aspas -> bytes (com \n \t \\ \" \0). */
static void emitir_texto(const char *s)
{
    const char *p = s + 1;
    for (; *p && *p != '"'; p++) {
        int c = (uint8_t)*p;
        if (c == '\\' && p[1]) {
            p++;
            c = *p == 'n' ? '\n' : *p == 't' ? '\t' : *p == '0' ? 0 : (uint8_t)*p;
        }
        emitir(c);
    }
    if (*p != '"') erro("texto sin cerrar");
    else if (*aparar((char *)p + 1)) erro("sobra algo después del texto");
}

static bool valor_ja_definido(const char *s, int *v, const char *diretiva)
{
    M.indefinido = false;
    *v = avaliar(s);
    if (M.indefinido) { erro1("'%s' solo acepta símbolos ya definidos", diretiva); return false; }
    return true;
}

static bool diretiva(const char *d, char *resto)
{
    char *ops[64];
    if (compara_texto(d, "db") == 0 || compara_texto(d, "dw") == 0) {
        int n = separar(resto, ops, 64);
        if (n <= 0) { erro(n < 0 ? "demasiados valores en una línea" : "'%s' sin valores", d); return true; }
        for (int k = 0; k < n; k++) {
            if (ops[k][0] == '"') { if (d[1] == 'b') emitir_texto(ops[k]); else erro("'dw' no acepta texto"); }
            else if (d[1] == 'b') emitir(valor(ops[k], -128, 255, "byte") & 0xFF);
            else emitir16(valor(ops[k], -32768, 65535, "palabra") & 0xFFFF);
        }
        return true;
    }
    if (compara_texto(d, "ds") == 0) {
        int n = separar(resto, ops, 2), v, enche = 0;
        if (n < 1) { erro("'ds' necesita el número de bytes"); return true; }
        if (!valor_ja_definido(ops[0], &v, d)) return true;
        if (n == 2) enche = valor(ops[1], -128, 255, "byte") & 0xFF;
        if (v < 0 || v > 65536) { erro("'ds' fuera de rango"); return true; }
        while (v--) emitir(enche);
        return true;
    }
    if (compara_texto(d, "org") == 0) {
        int v;
        if (!valor_ja_definido(resto, &v, d)) return true;
        if (v < C8_INICIO || v > 0xFFFF) { erro("'org' fuera de la memoria del programa (de 200 a ffff)"); return true; }
        M.pc = v;
        return true;
    }
    if (compara_texto(d, "align") == 0) {
        int v;
        if (!valor_ja_definido(resto, &v, d)) return true;
        if (v < 1 || v > 4096) { erro("'align' fuera de rango"); return true; }
        while (M.pc % v) emitir(0);
        return true;
    }
    if (compara_texto(d, "include") == 0) {
        char *s = aparar(resto);
        int n = (int)tam_texto(s);
        if (n < 2 || s[0] != '"' || s[n - 1] != '"') { erro("uso: include \"archivo.asm\""); return true; }
        s[n - 1] = 0;
        char cam[200];
        const char *barra = acha_ultimo_char(M.arq, '/');
        if (s[1] == '/' || !barra) copia_texto(cam, s + 1, sizeof cam);
        else {
            /* a pasta do arquivo que inclui (o formatador da libsagui não tem %.*s) */
            int np = (int)(barra - M.arq + 1);
            if (np > (int)sizeof cam - 1) np = (int)sizeof cam - 1;
            memcpy(cam, M.arq, (size_t)np);
            cam[np] = 0;
            junta(cam, s + 1, sizeof cam);
        }
        if (M.profundidade >= MAX_PROFUNDIDADE) { erro("demasiados include anidados"); return true; }
        const char *arq = M.arq;
        int linha = M.linha;
        if (!montar_arquivo(cam)) { M.arq = arq; M.linha = linha; erro("no se ha podido leer '%s'", cam); }
        M.arq = arq;
        M.linha = linha;
        return true;
    }
    return false;
}

/* ------------------------------------------------------------ linhas */

static void definir(const char *nome, int v, bool constante)
{
    if (nome_reservado(nome)) { erro("'%s' es una palabra reservada", nome); return; }
    simbolo_t *s = novo(nome);
    if (!s) return;
    if (M.passo == 1) {
        if (s->definido) { erro1("'%s' ya está definido", nome); return; }
        s->definido = !M.indefinido;
        s->constante = constante;
        s->valor = v;
        if (M.indefinido) s->definido = false;
    } else {
        /* com erros da primeira passada (etiqueta repetida, org...) a diferença já foi explicada */
        if (s->definido && s->valor != v && !constante && !M.erros) erro("'%s' cambió de valor entre pasadas", nome);
        s->valor = v;
        s->definido = true;
    }
}

static void linha(char *l)
{
    tirar_comentario(l);
    char *s = aparar(l);
    if (!*s) return;
    M.pc_linha = M.pc;

    /* etiqueta: (pode haver um comando depois) */
    char *p = s;
    if (letra_nome(*p)) {
        while (letra_nome(*p) || digito(*p)) p++;
        if (*p == ':') {
            *p = 0;
            if (p - s >= MAX_NOME) erro("nombre demasiado largo");
            M.indefinido = false;
            definir(s, M.pc, false);
            s = aparar(p + 1);
            if (!*s) return;
            p = s;
            while (letra_nome(*p) || digito(*p)) p++;
        }
    }
    /* primeira palavra */
    char pal[MAX_NOME];
    int n = 0;
    for (p = s; *p && !espaco(*p) && *p != '='; p++) if (n < MAX_NOME - 1) pal[n++] = *p;
    pal[n] = 0;
    char *resto = aparar(p);

    /* nome = expr  /  nome equ expr */
    bool eq = resto[0] == '=';
    if (!eq && minusc(resto[0]) == 'e' && minusc(resto[1]) == 'q' && minusc(resto[2]) == 'u' && espaco(resto[3])) {
        eq = true;
        resto += 2;
    }
    if (eq) {
        if (!letra_nome(pal[0])) { erro("nombre de constante no válido"); return; }
        M.indefinido = false;
        int v = avaliar(resto + 1);
        definir(pal, v, true);
        return;
    }

    char mn[MAX_NOME];
    for (int k = 0; k <= n; k++) mn[k] = (char)minusc(pal[k]);
    if (diretiva(mn, resto)) return;
    char *ops[4];
    int q = separar(resto, ops, 4);
    if (q < 0) { erro("demasiados operandos"); emitir16(0); return; }
    if (!instrucao(mn, ops, q)) erro("instrucción desconocida '%s'", pal);
}

static bool montar_texto(const char *nome, const char *texto)
{
    M.profundidade++;
    M.arq = nome;
    M.linha = 0;
    const char *p = texto;
    while (*p) {
        char l[MAX_LINHA];
        int n = 0;
        while (*p && *p != '\n') { if (n < MAX_LINHA - 1) l[n++] = *p; p++; }
        if (*p) p++;
        l[n] = 0;
        M.linha++;
        if (n == MAX_LINHA - 1) erro("línea demasiado larga");
        linha(l);
        M.arq = nome;
    }
    M.profundidade--;
    return true;
}

static bool montar_arquivo(const char *caminho)
{
    int tam;
    uint8_t *t = c8_ler_arquivo(caminho, &tam);
    if (!t) return false;
    /* O nome tem de durar até o fim (as mensagens apontam para ele). */
    char *nome = aloca(tam_texto(caminho) + 1);
    copia_texto(nome, caminho, tam_texto(caminho) + 1);
    montar_texto(nome, (const char *)t);
    libera(t);
    return true;
}

int c8_montar(const char *fonte, uint8_t **rom, int *tam)
{
    memset(&M, 0, sizeof M);
    M.saida = aloca_zerado(65536);
    M.sim = aloca_zerado(sizeof(simbolo_t) * MAX_SIMBOLOS);
    if (!M.saida || !M.sim) { c8_errof("%s: sin memoria\n", fonte); return -1; }
    for (M.passo = 1; M.passo <= 2; M.passo++) {
        M.pc = C8_INICIO;
        M.fim = C8_INICIO;
        M.profundidade = 0;
        M.arq = fonte;
        M.linha = 0;
        if (!montar_arquivo(fonte)) {
            c8_errof("%s: no se ha podido leer el archivo\n", fonte);
            libera(M.saida); libera(M.sim);
            return -1;
        }
    }
    int erros = M.erros;
    libera(M.sim);
    if (erros) { libera(M.saida); return erros; }
    *tam = M.fim - C8_INICIO;
    *rom = aloca(*tam > 0 ? (size_t)*tam : 1);
    memcpy(*rom, M.saida + C8_INICIO, (size_t)*tam);
    libera(M.saida);
    return 0;
}
