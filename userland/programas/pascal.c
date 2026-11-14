// SPDX-License-Identifier: GPL-2.0-or-later
/* pascal.c - el compilador de Pascal de SaguiOS (D085-D087).
 *
 *   pascal programa.pas              -> genera el ejecutable "programa"
 *   pascal programa.pas -o nombre    -> genera "nombre"
 *   pascal -x87 / -sse2 programa.pas -> real con la x87 o con SSE2 (como el cc, D076)
 *
 * Compila un subconjunto de Pascal al estilo de Turbo Pascal directamente a
 * código de máquina i386 y escribe un ejecutable ELF, como el cc: sin
 * ensamblador ni enlazador. La parte que genera bytes (saltos con parche, punto
 * flotante, principio y fin del programa) y la escritura del ELF son las del cc, en
 * gerador_x86.h y elf_saida.h.
 *
 * Cómo funciona, al estilo de Wirth:
 *   1. El analizador léxico transforma el texto en fichas (no distingue
 *      mayúsculas y minúsculas, lee {comentarios}, (* *), //, 'textos', #13 y las
 *      directivas {$R+}, {$I archivo}, {$IFDEF}...). Cada ficha guarda la
 *      línea y la columna, para los mensajes de error.
 *   2. Descenso recursivo generando código en el mismo momento. Cada expresión
 *      se convierte en un "item" (constante, variable en memoria o valor en el registro):
 *      las constantes se unen sin generar código (const N = 10 * 2), y una
 *      variable sólo se lee cuando hace falta el valor, con el direccionamiento propio
 *      del x86 ([ebp+d], [dirección], [eax+d]).
 *   3. Los procedimientos anidados acceden a las variables de los niveles de fuera por
 *      un enlace estático: [ebp+8] guarda el marco del procedimiento que contiene
 *      éste (sólo en los de nivel 2 o más; el nivel 1 sólo ve las globales, que tienen
 *      dirección fija).
 *   4. La biblioteca (write, read, strings, conjuntos, archivos, crt y la
 *      unidad gráfica sagui) está escrita en el propio Pascal
 *      (pascal_biblioteca.h) y se compila junto. Como en el cc (D039), el
 *      programa se compila dos veces: la primera descubre qué rutinas
 *      de la biblioteca son alcanzables; la segunda deja las otras fuera.
 *
 * Convención de llamada (la misma para la biblioteca): los argumentos se apilan de
 * izquierda a derecha; quien es llamado quita los argumentos de la
 * pila ("ret n"), como en el Pascal de siempre. Ordinales y punteros
 * ocupan 4 bytes; real, 8; parámetros var y valores grandes (string,
 * record, array, set) pasan la dirección, y el llamado copia los de valor
 * a su marco. Una función que devuelve string, set o record recibe
 * primero la dirección donde guardar el resultado. Resultado ordinal en
 * EAX; real en XMM0 (SSE2) o ST(0) (x87).
 *
 * "-linux" (sólo para desarrollo) genera el programa para el Linux i386,
 * para probar el compilador en la computadora donde se construye SaguiOS. */
#include "sagui.h"

#define GX_NOME "pascal"
#define GX_MSG_SSE2 "este programa usa SSE2 y la CPU no lo tiene (compílalo con pascal -x87)\n"
#include "gerador_x86.h"
#include "elf_saida.h"
#include "pascal_biblioteca.h"

extern const unsigned char fonte_sagui[256][16];   /* la letra de la libsagui: va al programa que usa texto() */

/* ------------------------------------------------------------ errores */

static const char *arq_erro = "?";
static int linha_erro, coluna_erro;
static int nerros;
#define MAX_ERROS 20

static void mensagem(const char *fmt, va_list ap)
{
    char msg[300];
    formatar_lista(msg, sizeof msg, fmt, ap);
    imprimir_em(ERROS, "%s:%d:%d: error: %s\n", arq_erro, linha_erro, coluna_erro, msg);
}

/* Error de sintaxis: no se puede continuar. */
static void falhar(const char *fmt, ...) __attribute__((noreturn));
static void falhar(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    mensagem(fmt, ap);
    va_end(ap);
    sair(1);
}

/* Error de tipos, nombre no declarado...: el análisis sigue y muestra los
 * siguientes (el tipo "error" combina con todo, para no tener errores en
 * cascada). */
static void erro(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    mensagem(fmt, ap);
    va_end(ap);
    if (++nerros >= MAX_ERROS) {
        imprimir_em(ERROS, "%s: demasiados errores\n", arq_erro);
        sair(1);
    }
}

/* ------------------------------------------------------------ nombres */

enum {
    F_FIM = 0, F_NOME = 256, F_INT, F_REAL, F_TEXTO, F_ATRIB, F_DIF, F_MENIG, F_MAIIG, F_PP,
    /* palavras reservadas */
    P_AND, P_ARRAY, P_BEGIN, P_CASE, P_CONST, P_DIV, P_DO, P_DOWNTO, P_ELSE, P_END, P_FILE,
    P_FOR, P_FUNCTION, P_GOTO, P_IF, P_IN, P_LABEL, P_MOD, P_NIL, P_NOT, P_OF, P_OR, P_PACKED,
    P_PROCEDURE, P_PROGRAM, P_RECORD, P_REPEAT, P_SET, P_SHL, P_SHR, P_STRING, P_THEN, P_TO,
    P_TYPE, P_UNTIL, P_USES, P_VAR, P_WHILE, P_WITH, P_XOR,
};

static const char *const reservadas[] = {
    "and", "array", "begin", "case", "const", "div", "do", "downto", "else", "end", "file",
    "for", "function", "goto", "if", "in", "label", "mod", "nil", "not", "of", "or", "packed",
    "procedure", "program", "record", "repeat", "set", "shl", "shr", "string", "then", "to",
    "type", "until", "uses", "var", "while", "with", "xor",
};

struct simbolo;

typedef struct nome {
    char           *s;          /* en minúsculas: Pascal no distingue */
    int             reservada;  /* P_* o 0 */
    struct simbolo *sim;        /* el significado visible ahora (el más interno) */
    struct nome    *prox;
    bool            definido;   /* {$DEFINE} */
} nome_t;

#define HASH_NOMES 1024
static nome_t *nomes[HASH_NOMES];

static int minuscula(int c)
{
    if ((c >= 'A' && c <= 'Z') || (c >= 0xC0 && c <= 0xDE && c != 0xD7)) return c + 32;
    return c;
}

static nome_t *nome_de(const char *s, int n)
{
    char b[128];
    if (n > 127) n = 127;
    uint32_t h = 5381;
    for (int i = 0; i < n; i++) {
        b[i] = (char)minuscula((uint8_t)s[i]);
        h = h * 33 + (uint8_t)b[i];
    }
    h %= HASH_NOMES;
    for (nome_t *x = nomes[h]; x; x = x->prox)
        if ((int)tam_texto(x->s) == n && memcmp(x->s, b, n) == 0) return x;
    nome_t *x = memoria(sizeof *x);
    x->s = duplicar_texto(b, n);
    x->prox = nomes[h];
    nomes[h] = x;
    return x;
}

static nome_t *nome_c(const char *s) { return nome_de(s, (int)tam_texto(s)); }

/* ------------------------------------------------------------ fichas */

typedef struct {
    int         tipo;
    int32_t     val;          /* F_INT; F_TEXTO: índice del literal */
    double      real;         /* F_REAL */
    nome_t     *nome;
    const char *arq;
    int         linha, coluna;
    bool        faixa;        /* {$R+} valía aquí */
} ficha_t;

typedef struct { char *b; int n; } literal_t;
static literal_t *literais;
static int nliterais, capliterais;

static int novo_literal(const char *b, int n)
{
    for (int i = 0; i < nliterais; i++)
        if (literais[i].n == n && !memcmp(literais[i].b, b, n)) return i;
    garantir((void **)&literais, &capliterais, nliterais + 1, sizeof(literal_t));
    literais[nliterais].b = duplicar_texto(b, n);
    literais[nliterais].n = n;
    return nliterais++;
}

typedef struct {
    const char *texto;
    int         pos, n, linha, ini_linha;
    const char *nome;
} fonte_t;

#define MAX_FONTES 8
static fonte_t fontes[MAX_FONTES];
static int nfontes;
static bool faixa_lex = true;    /* {$R+} por defecto (D086) */

/* {$IFDEF}: pila de condiciones; "activo" = el texto actual vale. */
#define MAX_COND 16
static bool cond_pai[MAX_COND], cond_valeu[MAX_COND];
static int ncond;
static bool cond_ativo = true;

static ficha_t *fichas;
static int nfichas, capfichas;

static fonte_t *F(void) { return &fontes[nfontes - 1]; }
static int car(int k) { fonte_t *f = F(); return f->pos + k < f->n ? (uint8_t)f->texto[f->pos + k] : 0; }
static bool eh_letra(int c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || (c >= 0xC0 && c != 0xD7 && c != 0xF7); }
static bool eh_digito(int c) { return c >= '0' && c <= '9'; }
static bool eh_hex(int c) { return eh_digito(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }

static void abrir_fonte(const char *nome, const char *texto, int n)
{
    if (nfontes == MAX_FONTES) falhar("{$I} anidados demasiado");
    fonte_t *f = &fontes[nfontes++];
    f->texto = texto;
    f->n = n;
    f->pos = 0;
    f->linha = 1;
    f->ini_linha = 0;
    f->nome = nome;
}

static char *ler_arquivo(const char *caminho, int *tam)
{
    int fd = abrir(caminho, ABRIR_LER);
    if (fd < 0) return NULL;
    int cap = 4096, n = 0;
    char *b = memoria(cap + 1);
    for (;;) {
        if (n == cap) {
            char *c = memoria(cap * 2 + 1);
            memcpy(c, b, n);
            libera(b);
            b = c;
            cap *= 2;
        }
        int r = ler(fd, b + n, (unsigned)(cap - n));
        if (r <= 0) break;
        n += r;
    }
    fechar(fd);
    b[n] = 0;
    *tam = n;
    return b;
}

/* Posición actual del texto (para los mensajes del léxico). */
static void marcar_posicao(void)
{
    fonte_t *f = F();
    arq_erro = f->nome;
    linha_erro = f->linha;
    coluna_erro = f->pos - f->ini_linha + 1;
}

static void avancar_car(void)
{
    fonte_t *f = F();
    if (f->pos < f->n && f->texto[f->pos] == '\n') { f->linha++; f->ini_linha = f->pos + 1; }
    f->pos++;
}

/* {$I nombre}: el archivo incluido queda en la misma carpeta que el que lo incluye. */
static void incluir(const char *nome)
{
    char caminho[160];
    const char *atual = F()->nome;
    const char *barra = acha_ultimo_char(atual, '/');
    if (nome[0] != '/' && barra) {
        int k = (int)(barra - atual) + 1;
        if (k > 100) k = 100;
        memcpy(caminho, atual, k);
        copia_texto(caminho + k, nome, sizeof caminho - k);
    } else {
        copia_texto(caminho, nome, sizeof caminho);
    }
    int tam;
    char *texto = ler_arquivo(caminho, &tam);
    if (!texto) falhar("no se puede abrir el archivo incluido '%s'", caminho);
    abrir_fonte(duplicar_texto(caminho, (int)tam_texto(caminho)), texto, tam);
}

/* Directiva {$...} (el texto entre "{$" y "}"). */
static void diretiva(const char *d, int n)
{
    char nome[16], arg[128];
    int i = 0, k = 0;
    while (i < n && eh_letra((uint8_t)d[i]) && k < 15) nome[k++] = (char)minuscula((uint8_t)d[i++]);
    nome[k] = 0;
    char sinal = i < n ? d[i] : 0;
    while (i < n && d[i] == ' ') i++;
    k = 0;
    while (i < n && d[i] != ' ' && d[i] != '}' && k < 127) arg[k++] = d[i++];
    arg[k] = 0;
    bool eh = !compara_texto(nome, "ifdef"), nao = !compara_texto(nome, "ifndef");
    if (eh || nao) {
        if (ncond == MAX_COND) falhar("{$IFDEF} anidados demasiado");
        bool v = nome_c(arg)->definido == eh;
        cond_pai[ncond] = cond_ativo;
        cond_valeu[ncond] = v;
        ncond++;
        cond_ativo = cond_ativo && v;
        return;
    }
    if (!compara_texto(nome, "else")) {
        if (!ncond) falhar("{$ELSE} sin {$IFDEF}");
        cond_ativo = cond_pai[ncond - 1] && !cond_valeu[ncond - 1];
        cond_valeu[ncond - 1] = true;
        return;
    }
    if (!compara_texto(nome, "endif")) {
        if (!ncond) falhar("{$ENDIF} sin {$IFDEF}");
        cond_ativo = cond_pai[--ncond];
        return;
    }
    if (!cond_ativo) return;
    if (!compara_texto(nome, "define")) { nome_c(arg)->definido = true; return; }
    if (!compara_texto(nome, "undef")) { nome_c(arg)->definido = false; return; }
    if (!compara_texto(nome, "r") && (sinal == '+' || sinal == '-')) { faixa_lex = sinal == '+'; return; }
    if (!compara_texto(nome, "i") && sinal != '+' && sinal != '-') { incluir(arg); return; }
    /* Las otras ({$MODE}, {$H+}, {$I-}...) no cambian nada aquí. */
}

/* Salta espacios y comentarios. Devuelve false al final del archivo actual. */
static void pular_espacos(void)
{
    for (;;) {
        int c = car(0);
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f') { avancar_car(); continue; }
        if (c == '/' && car(1) == '/') {
            while (car(0) && car(0) != '\n') avancar_car();
            continue;
        }
        if (c == '{' || (c == '(' && car(1) == '*')) {
            marcar_posicao();
            bool chave = c == '{';
            avancar_car();
            if (!chave) avancar_car();
            int ini = F()->pos;
            fonte_t *f = F();
            for (;;) {
                if (!car(0)) falhar("comentario sin cerrar");
                if (chave && car(0) == '}') break;
                if (!chave && car(0) == '*' && car(1) == ')') break;
                avancar_car();
            }
            int fim = f->pos;
            avancar_car();
            if (!chave) avancar_car();
            if (f->texto[ini] == '$') diretiva(f->texto + ini + 1, fim - ini - 1);
            continue;
        }
        if (!cond_ativo && c) {   /* dentro de un {$IFDEF} falso: sólo busca la próxima directiva */
            if (c == '\'') { avancar_car(); while (car(0) && car(0) != '\'' && car(0) != '\n') avancar_car(); }
            avancar_car();
            continue;
        }
        return;
    }
}

static double potencia10(int e)
{
    long double r = 1, b = 10;
    bool neg = e < 0;
    if (neg) e = -e;
    while (e) { if (e & 1) r *= b; b *= b; e >>= 1; }
    return (double)(neg ? 1 / r : r);
}

static void ler_ficha(ficha_t *t)
{
    for (;;) {
        pular_espacos();
        if (car(0) || nfontes == 1) break;
        nfontes--;           /* fin de un {$I}: vuelve a quien lo incluyó */
    }
    memset(t, 0, sizeof *t);
    marcar_posicao();
    t->arq = arq_erro;
    t->linha = linha_erro;
    t->coluna = coluna_erro;
    t->faixa = faixa_lex;
    int c = car(0);
    if (!c) { t->tipo = F_FIM; return; }
    if (eh_letra(c)) {
        fonte_t *f = F();
        int ini = f->pos;
        while (eh_letra(car(0)) || eh_digito(car(0))) avancar_car();
        t->nome = nome_de(f->texto + ini, f->pos - ini);
        t->tipo = t->nome->reservada ? t->nome->reservada : F_NOME;
        return;
    }
    if (eh_digito(c) || (c == '$' && eh_hex(car(1)))) {
        if (c == '$') {
            avancar_car();
            uint32_t v = 0;
            while (eh_hex(car(0))) {
                int d = car(0);
                v = v * 16 + (uint32_t)(eh_digito(d) ? d - '0' : (d | 32) - 'a' + 10);
                avancar_car();
            }
            t->tipo = F_INT;
            t->val = (int32_t)v;
            return;
        }
        /* Entero o real: "1..10" es un entero seguido de "..". */
        uint64_t m = 0;
        int nd = 0, e = 0;
        bool real = false, grande = false;
        while (eh_digito(car(0))) {
            if (nd < 19) { m = m * 10 + (uint64_t)(car(0) - '0'); if (m) nd++; } else e++;
            avancar_car();
        }
        if (car(0) == '.' && eh_digito(car(1))) {
            real = true;
            avancar_car();
            while (eh_digito(car(0))) {
                if (nd < 19) { m = m * 10 + (uint64_t)(car(0) - '0'); if (m) nd++; e--; }
                avancar_car();
            }
        }
        if ((car(0) | 32) == 'e' && (eh_digito(car(1)) || ((car(1) == '+' || car(1) == '-') && eh_digito(car(2))))) {
            real = true;
            avancar_car();
            bool neg = car(0) == '-';
            if (car(0) == '+' || car(0) == '-') avancar_car();
            int x = 0;
            while (eh_digito(car(0))) { if (x < 10000) x = x * 10 + car(0) - '0'; avancar_car(); }
            e += neg ? -x : x;
        }
        if (!real && (e || m > 0x7FFFFFFFu)) grande = true;
        if (real || grande) {
            long double v = (long double)m;
            if (e >= 0) v *= potencia10(e);
            else if (e >= -300) v /= potencia10(-e);
            else v = v / potencia10(-e - 300) / 1e300L;
            t->tipo = F_REAL;
            t->real = (double)v;
            if (grande) erro("entero demasiado grande (el máximo es 2147483647)");
            return;
        }
        t->tipo = F_INT;
        t->val = (int32_t)m;
        return;
    }
    if (c == '\'' || c == '#') {
        /* 'texto', 'it''s', #13#10, 'a'#9'b': todo se convierte en un solo literal. */
        char buf[256];
        int n = 0;
        for (;;) {
            if (car(0) == '\'') {
                avancar_car();
                for (;;) {
                    if (!car(0) || car(0) == '\n') falhar("texto sin cerrar");
                    if (car(0) == '\'') {
                        if (car(1) != '\'') break;
                        avancar_car();
                    }
                    if (n == 255) falhar("texto demasiado largo (máximo 255 caracteres)");
                    buf[n++] = (char)car(0);
                    avancar_car();
                }
                avancar_car();
            } else if (car(0) == '#') {
                avancar_car();
                uint32_t v = 0;
                if (car(0) == '$') {
                    avancar_car();
                    if (!eh_hex(car(0))) falhar("se esperaba un número después de #$");
                    while (eh_hex(car(0))) { int d = car(0); v = v * 16 + (uint32_t)(eh_digito(d) ? d - '0' : (d | 32) - 'a' + 10); avancar_car(); }
                } else {
                    if (!eh_digito(car(0))) falhar("se esperaba un número después de #");
                    while (eh_digito(car(0))) { v = v * 10 + (uint32_t)(car(0) - '0'); avancar_car(); }
                }
                if (v > 255) falhar("carácter #%u fuera de rango", v);
                if (n == 255) falhar("texto demasiado largo (máximo 255 caracteres)");
                buf[n++] = (char)v;
            } else {
                break;
            }
        }
        t->tipo = F_TEXTO;
        t->val = novo_literal(buf, n);
        return;
    }
    avancar_car();
    int d = car(0);
    switch (c) {
    case ':': if (d == '=') { avancar_car(); t->tipo = F_ATRIB; return; } break;
    case '<':
        if (d == '>') { avancar_car(); t->tipo = F_DIF; return; }
        if (d == '=') { avancar_car(); t->tipo = F_MENIG; return; }
        break;
    case '>': if (d == '=') { avancar_car(); t->tipo = F_MAIIG; return; } break;
    case '.':
        if (d == '.') { avancar_car(); t->tipo = F_PP; return; }
        if (d == ')') { avancar_car(); t->tipo = ']'; return; }   /* (. .) = [ ] */
        break;
    case '(': if (d == '.') { avancar_car(); t->tipo = '['; return; } break;
    case '+': case '-': case '*': case '/': case '=': case ')': case '[': case ']': case ',':
    case ';': case '^': case '@':
        break;
    default:
        falhar("carácter inesperado '%c'", c);
    }
    t->tipo = c;
}

static void ler_todas_as_fichas(void)
{
    for (;;) {
        garantir((void **)&fichas, &capfichas, nfichas + 1, sizeof(ficha_t));
        ficha_t *t = &fichas[nfichas++];
        ler_ficha(t);
        if (t->tipo == F_FIM) break;
    }
    if (ncond) falhar("falta {$ENDIF}");
}

/* ------------------------------------------------------------ tipos */

enum { T_ERRO, T_INT, T_CHAR, T_BOOL, T_ENUM, T_REAL, T_STRING, T_ARRAY, T_RECORD, T_SET, T_PTR, T_NIL, T_FILE };

typedef struct campo {
    nome_t       *nome;
    struct tipo  *t;
    int           desloc;
    struct campo *prox;
} campo_t;

typedef struct tipo {
    int           k;
    int           tam;
    int32_t       lo, hi;      /* ordinales: el rango; T_STRING: hi = capacidad */
    struct tipo  *raiz;        /* ordinales: el tipo del que éste es subrango (enum: el propio enum) */
    struct tipo  *base;        /* T_ARRAY: elemento; T_SET: elemento; T_PTR: apontado (NULL = pointer); T_FILE: registro (NULL = text) */
    struct tipo  *indice;      /* T_ARRAY: el tipo del índice (NULL: array abierto, "array of T") */
    campo_t      *campos;      /* T_RECORD */
    nome_t       *pendente;    /* T_PTR: ^T con T declarado más adelante */
    nome_t       *nome;        /* nombre declarado (para los mensajes) */
} tipo_t;

#define INT_MIN32 (-2147483647 - 1)
static tipo_t t_erro = {T_ERRO, 4, INT_MIN32, 2147483647, NULL, NULL, NULL, NULL, NULL, NULL};
static tipo_t t_integer = {T_INT, 4, INT_MIN32, 2147483647, NULL, NULL, NULL, NULL, NULL, NULL};
static tipo_t t_byte = {T_INT, 1, 0, 255, &t_integer, NULL, NULL, NULL, NULL, NULL};
static tipo_t t_shortint = {T_INT, 1, -128, 127, &t_integer, NULL, NULL, NULL, NULL, NULL};
static tipo_t t_word = {T_INT, 2, 0, 65535, &t_integer, NULL, NULL, NULL, NULL, NULL};
static tipo_t t_smallint = {T_INT, 2, -32768, 32767, &t_integer, NULL, NULL, NULL, NULL, NULL};
static tipo_t t_char = {T_CHAR, 1, 0, 255, NULL, NULL, NULL, NULL, NULL, NULL};
static tipo_t t_boolean = {T_BOOL, 1, 0, 1, NULL, NULL, NULL, NULL, NULL, NULL};
static tipo_t t_real = {T_REAL, 8, 0, 0, NULL, NULL, NULL, NULL, NULL, NULL};
static tipo_t t_string = {T_STRING, 256, 0, 255, NULL, NULL, NULL, NULL, NULL, NULL};
static tipo_t t_pointer = {T_PTR, 4, 0, 0, NULL, NULL, NULL, NULL, NULL, NULL};
static tipo_t t_nil = {T_NIL, 4, 0, 0, NULL, NULL, NULL, NULL, NULL, NULL};
static tipo_t t_conj_vazio = {T_SET, 32, 0, 0, NULL, NULL, NULL, NULL, NULL, NULL};   /* [] */
static tipo_t *t_text;          /* text (el registro __texto de la biblioteca) */

static bool eh_ordinal(tipo_t *t) { return t->k >= T_INT && t->k <= T_ENUM; }
static tipo_t *raiz(tipo_t *t) { return t->raiz ? t->raiz : t; }
static bool eh_grande(tipo_t *t) { return t->k == T_STRING || t->k == T_ARRAY || t->k == T_RECORD || t->k == T_SET || t->k == T_FILE; }

static tipo_t *novo_tipo(int k, int tam)
{
    tipo_t *t = memoria(sizeof *t);
    t->k = k;
    t->tam = tam;
    return t;
}

/* Tamaño de un subrango: el menor que cabe (como en Turbo Pascal). */
static tipo_t *subfaixa(tipo_t *de, int32_t lo, int32_t hi)
{
    tipo_t *t = novo_tipo(de->k, 4);
    t->lo = lo;
    t->hi = hi;
    t->raiz = raiz(de);
    if ((lo >= 0 && hi <= 255) || (lo >= -128 && hi <= 127)) t->tam = 1;
    else if ((lo >= 0 && hi <= 65535) || (lo >= -32768 && hi <= 32767)) t->tam = 2;
    return t;
}

static tipo_t *tipo_string(int cap)
{
    if (cap == 255) return &t_string;
    tipo_t *t = novo_tipo(T_STRING, cap + 1);
    t->hi = cap;
    return t;
}

static tipo_t *ponteiro_para(tipo_t *b)
{
    tipo_t *t = novo_tipo(T_PTR, 4);
    t->base = b;
    return t;
}

/* Nombre legible del tipo, para los mensajes. */
static const char *nome_tipo(tipo_t *t)
{
    static char b[4][64];
    static int k;
    char *s = b[k++ & 3];
    if (t->nome) { copia_texto(s, t->nome->s, 64); return s; }
    switch (t->k) {
    case T_INT: return t == &t_integer ? "integer" : t == &t_byte ? "byte" : t == &t_word ? "word" :
                       t == &t_shortint ? "shortint" : t == &t_smallint ? "smallint" : "subrango de integer";
    case T_CHAR: return t == &t_char ? "char" : "subrango de char";
    case T_BOOL: return "boolean";
    case T_ENUM: return "enumerado";
    case T_REAL: return "real";
    case T_STRING: if (t->hi == 255) return "string"; formatar_texto(s, 64, "string[%d]", t->hi); return s;
    case T_ARRAY: return "array";
    case T_RECORD: return "record";
    case T_SET: return "set";
    case T_PTR: return t->base ? "puntero" : "pointer";
    case T_NIL: return "nil";
    case T_FILE: return t->base ? "file" : "text";
    default: return "?";
    }
}

/* ------------------------------------------------------------ símbolos */

enum { S_VAR, S_CONST, S_TIPO, S_PROC, S_CAMPO, S_ROTULO };
enum { B_ABS, B_EBP, B_EAX, B_ECX, B_PILHA, B_ESP };   /* dónde está la dirección de una variable */

typedef struct param {
    nome_t *nome;
    tipo_t *t;
    int     modo;           /* 0 = valor, 1 = var, 2 = const */
} param_t;

typedef struct proc {
    int      id;            /* orden de declaración: igual en las dos pasadas */
    nome_t  *nome;
    int      nivel;         /* nivel léxico del cuerpo: 1 = declarado en el programa */
    int      np;
    param_t *params;
    tipo_t  *ret;           /* NULL = procedure */
    bool     definida, adiantada, da_biblioteca;
    int      pos;           /* posición del código */
    int      chamadas;      /* lista de "call" esperando la definición */
    int      tam_args;      /* bytes de argumentos (ret n) */
    int      res_desloc;    /* resultado: local [ebp+d], o la dirección oculta (ret_ref) */
    bool     ret_ref;       /* resultado por dirección oculta (string, set, record, array) */
    int      saidas;        /* "exit": saltos al fin */
    bool     em_curso;      /* compilando el cuerpo (el nombre de la función es la variable del resultado) */
    struct simbolo *res;    /* a variável do resultado */
} proc_t;

typedef struct simbolo {
    nome_t         *nome;
    int             classe;
    tipo_t         *t;
    int             nivel;          /* S_VAR: nivel léxico (0 = global, dirección fija) */
    int32_t         valor;          /* S_VAR: dirección (nivel 0) o [ebp+valor]; S_CONST ordinal: el valor */
    double          real;           /* S_CONST real */
    int             literal;        /* S_CONST string: índice del literal */
    uint8_t        *conj;           /* S_CONST set */
    bool            por_ref;        /* S_VAR: el lugar guarda la dirección (parámetro var, array abierto) */
    int             alto;           /* array abierto: [ebp+alto] = high */
    proc_t         *p;              /* S_PROC */
    campo_t        *campo;          /* S_CAMPO (with): o campo... */
    int             with_base;      /* ... y dónde está el registro: B_ABS/B_EBP con with_desloc, o B_PILHA = puntero en [ebp+with_desloc] */
    int32_t         with_desloc;
    int             rotulo_pos, rotulo_lista;   /* S_ROTULO */
    bool            rotulo_def;
    struct simbolo *sombra;         /* el significado anterior del mismo nombre */
    struct simbolo *prox;           /* lista del ámbito */
    int             escopo;
} simbolo_t;

#define MAX_ESCOPOS 64
static simbolo_t *escopos[MAX_ESCOPOS];
static int escopo_id[MAX_ESCOPOS];
static int nescopos, prox_escopo_id;

static void abrir_escopo(void)
{
    if (nescopos == MAX_ESCOPOS) falhar("anidamiento demasiado profundo");
    escopos[nescopos] = NULL;
    escopo_id[nescopos++] = ++prox_escopo_id;
}

static void fechar_escopo(void)
{
    simbolo_t *s = escopos[--nescopos];
    for (; s; s = s->prox) s->nome->sim = s->sombra;
}

static simbolo_t *declarar(nome_t *n, int classe, tipo_t *t)
{
    simbolo_t *a = n->sim;
    if (a && a->escopo == escopo_id[nescopos - 1]) {
        if (!(a->classe == S_PROC && a->p->adiantada && classe == S_PROC))
            erro("'%s' ya está declarado", n->s);
    }
    simbolo_t *s = memoria(sizeof *s);
    s->nome = n;
    s->classe = classe;
    s->t = t;
    s->sombra = n->sim;
    s->escopo = escopo_id[nescopos - 1];
    s->prox = escopos[nescopos - 1];
    escopos[nescopos - 1] = s;
    n->sim = s;
    return s;
}

/* Las unidades usadas ("uses crt, sagui"): sus nombres están en la biblioteca
 * con prefijo (__crt_clrscr), para no ocupar el espacio de nombres de quien no
 * las usa. Las rutinas comunes son __sys_*. */
static bool usa_crt, usa_sagui, usa_hilos;

static simbolo_t *procurar_prefixo(const char *pre, nome_t *n)
{
    char b[160];
    formatar_texto(b, sizeof b, "%s%s", pre, n->s);
    return nome_c(b)->sim;
}

static simbolo_t *procurar(nome_t *n)
{
    if (n->sim) return n->sim;
    simbolo_t *s = NULL;
    if (usa_crt) s = procurar_prefixo("__crt_", n);
    if (!s && usa_sagui) s = procurar_prefixo("__sagui_", n);
    if (!s && usa_hilos) s = procurar_prefixo("__hilos_", n);
    if (!s) s = procurar_prefixo("__sys_", n);
    return s;
}

/* ------------------------------------------------------------ procedimientos usados (D039 del cc) */

static int passada;                 /* 1: descubre las rutinas alcanzables; 2: genera el programa */
static int nprocs;                  /* ids dados en esta pasada (0 = el programa principal) */
static int **refs;                  /* refs[id]: las rutinas que ésta llama */
static int *nrefs, *caprefs;
static bool *alcancavel;
static int caprocs;
static proc_t *proc_atual;          /* NULL = cuerpo del programa principal */

static void registrar_ref(proc_t *p)
{
    if (passada != 1 || descartando) return;
    int de = proc_atual ? proc_atual->id : 0;
    for (int i = 0; i < nrefs[de]; i++) if (refs[de][i] == p->id) return;
    garantir((void **)&refs[de], &caprefs[de], nrefs[de] + 1, sizeof(int));
    refs[de][nrefs[de]++] = p->id;
}

static proc_t *novo_proc(nome_t *n)
{
    proc_t *p = memoria(sizeof *p);
    p->nome = n;
    p->id = ++nprocs;
    p->chamadas = -1;
    p->saidas = -1;
    p->da_biblioteca = compilando_biblioteca;
    if (passada == 1 && nprocs >= caprocs) {
        int c1 = caprocs, c2 = caprocs, c3 = caprocs, c4 = caprocs;
        garantir((void **)&refs, &c1, nprocs + 1, sizeof(int *));
        garantir((void **)&nrefs, &c2, nprocs + 1, sizeof(int));
        garantir((void **)&caprefs, &c3, nprocs + 1, sizeof(int));
        garantir((void **)&alcancavel, &c4, nprocs + 1, sizeof(bool));
        caprocs = c1;
    }
    return p;
}

static void alcancar(int id)
{
    if (alcancavel[id]) return;
    alcancavel[id] = true;
    for (int i = 0; i < nrefs[id]; i++) alcancar(refs[id][i]);
}

static void chamar_proc(proc_t *p)
{
    registrar_ref(p);
    e1(0xE8);
    if (p->definida) { e4((uint32_t)(p->pos - (ncod + 4))); return; }
    int pos = ncod;
    e4(descartando ? 0 : (uint32_t)p->chamadas);
    if (!descartando) p->chamadas = pos;
}

/* ------------------------------------------------------------ fichas en el análisis */

static int pf;
static ficha_t *tk(void) { return &fichas[pf]; }
static int tt(void) { return fichas[pf].tipo; }
static int tt_a(int k) { return pf + k < nfichas ? fichas[pf + k].tipo : F_FIM; }

static void posicionar_erro(ficha_t *t)
{
    arq_erro = t->arq;
    linha_erro = t->linha;
    coluna_erro = t->coluna;
}

static void avancar(void)
{
    if (fichas[pf].tipo != F_FIM) pf++;
    posicionar_erro(&fichas[pf]);
}

static bool aceitar(int t)
{
    if (tt() != t) return false;
    avancar();
    return true;
}

static const char *descrever(int t)
{
    static char b[2][32];
    static int k;
    char *s = b[k++ & 1];
    switch (t) {
    case F_FIM: return "el fin del archivo";
    case F_NOME: return "un nombre";
    case F_INT: return "un número";
    case F_REAL: return "un número real";
    case F_TEXTO: return "un texto";
    case F_ATRIB: return "':='";
    case F_DIF: return "'<>'";
    case F_MENIG: return "'<='";
    case F_MAIIG: return "'>='";
    case F_PP: return "'..'";
    }
    if (t >= P_AND && t <= P_XOR) { formatar_texto(s, 32, "'%s'", reservadas[t - P_AND]); return s; }
    formatar_texto(s, 32, "'%c'", t);
    return s;
}

static const char *descrever_atual(void)
{
    static char b[160];
    ficha_t *t = tk();
    if (t->tipo == F_NOME) { formatar_texto(b, sizeof b, "'%s'", t->nome->s); return b; }
    if (t->tipo == F_INT) { formatar_texto(b, sizeof b, "'%d'", t->val); return b; }
    return descrever(t->tipo);
}

static void esperar_ficha(int t)
{
    if (aceitar(t)) return;
    if (pf > 0) {
        /* El ";" (o ")") que falta es del fin de la línea anterior: apunta allí. */
        ficha_t *a = &fichas[pf - 1];
        if (a->linha < tk()->linha) {
            arq_erro = a->arq;
            linha_erro = a->linha;
            coluna_erro = a->coluna + 1;
        }
    }
    falhar("se esperaba %s y se encontró %s", descrever(t), descrever_atual());
}

static nome_t *esperar_nome(void)
{
    if (tt() != F_NOME) falhar("se esperaba un nombre y se encontró %s", descrever_atual());
    nome_t *n = tk()->nome;
    avancar();
    return n;
}

/* ------------------------------------------------------------ ítems */

enum { M_CONST, M_VAR, M_REG, M_PILHA };

/* Un operando: constante (ningún código todavía), variable en la memoria (la
 * dirección es [base+desloc]) o valor ya calculado (EAX, o XMM0/ST(0) si es
 * real; string/set/record/array en EAX es la dirección). M_PILHA: el valor (o
 * la dirección, si es grande) está en la cima de la pila del procesador. */
typedef struct {
    int      modo;
    tipo_t  *t;
    int32_t  val;           /* M_CONST ordinal/puntero */
    double   real;          /* M_CONST real */
    int      literal;       /* M_CONST string */
    uint8_t *conj;          /* M_CONST set: 32 bytes */
    int      base;          /* M_VAR: B_ABS, B_EBP, B_EAX, B_ECX o B_PILHA (dirección en la pila) */
    int32_t  desloc;
    bool     temp;          /* M_VAR: temporal (string, set) que puede reaprovecharse */
} item_t;

static void item_const(item_t *x, tipo_t *t, int32_t v)
{
    memset(x, 0, sizeof *x);
    x->modo = M_CONST;
    x->t = t;
    x->val = v;
}

static void item_var(item_t *x, tipo_t *t, int base, int32_t d)
{
    memset(x, 0, sizeof *x);
    x->modo = M_VAR;
    x->t = t;
    x->base = base;
    x->desloc = d;
}

/* ------------------------------------------------------------ código: básico */

static bool faixa_ativa(void) { return tk()->faixa && !compilando_biblioteca; }

static void invalidar_ultimas(void)
{
    comparacao_fim = -1;
    logico_fim = -1;
}

static void remendar_aqui(int lista)
{
    remendar(lista, ncod);
    invalidar_ultimas();
}

static bool cabe8(int32_t d) { return d >= -128 && d <= 127; }

/* modrm + desplazamiento para el operando x (base B_ABS, B_EBP, B_EAX o
 * B_ECX) con el registro reg (o /n de la instrucción). */
static void modrm(int reg, item_t *x, int32_t extra)
{
    int32_t d = x->desloc + extra;
    switch (x->base) {
    case B_ABS: e1(0x05 | reg << 3); e4((uint32_t)d); return;
    case B_EBP:
        if (cabe8(d)) { e2(0x45 | reg << 3, d & 0xFF); return; }
        e1(0x85 | reg << 3); e4((uint32_t)d); return;
    case B_EAX: case B_ECX: {
        int rm = x->base == B_EAX ? 0 : 1;
        if (d == 0) { e1(rm | reg << 3); return; }
        if (cabe8(d)) { e2(0x40 | rm | reg << 3, d & 0xFF); return; }
        e1(0x80 | rm | reg << 3); e4((uint32_t)d); return;
    }
    case B_ESP:
        if (d == 0) { e2(0x04 | reg << 3, 0x24); return; }
        e3(0x44 | reg << 3, 0x24, d & 0xFF); return;
    default: falhar("error interno: modrm con base %d", x->base);
    }
}

/* Dirección en la pila -> ECX (antes de usar la variable). */
static void base_utilizavel(item_t *x)
{
    if (x->modo == M_VAR && x->base == B_PILHA) { pop_ecx(); x->base = B_ECX; }
}

static bool sem_sinal(tipo_t *t) { return t->lo >= 0; }

/* EAX = valor ordinal (o puntero) en la memoria. */
/* El último "mov [mem], eax" de 32 bits (donde empieza y donde acaba): una
 * lectura de la misma memoria justo después no necesita cargar (D108). */
static int guarda_ini = -1, guarda_fim = -1;

static void carregar_ord_mem(item_t *x)
{
    base_utilizavel(x);
    tipo_t *t = x->t;
    int tam = t->k == T_PTR || t->k == T_NIL ? 4 : t->tam;
    int ini = ncod;
    if (tam == 1) e2(0x0F, sem_sinal(t) ? 0xB6 : 0xBE);
    else if (tam == 2) e2(0x0F, sem_sinal(t) ? 0xB7 : 0xBF);
    else e1(0x8B);
    modrm(0, x, 0);
    int n = ncod - ini;
    if (tam == 4 && otim(OTIM_MIRILLA) && guarda_fim == ini && barreira < ini && guarda_ini >= 0 &&
        guarda_fim - guarda_ini == n && cod[guarda_ini] == 0x89 && !memcmp(cod + guarda_ini + 1, cod + ini + 1, (size_t)(n - 1)))
        ncod = ini;       /* el valor ya está en EAX: acaba de grabarse en ese mismo lugar */
}

/* [mem] = EAX (reg 0) o EDX (reg 2), con el tamaño del tipo. */
static void guardar_ord_mem(item_t *x, int reg)
{
    int tam = x->t->k == T_PTR || x->t->k == T_NIL ? 4 : x->t->tam;
    int ini = ncod;
    if (tam == 1) e1(0x88);
    else if (tam == 2) e2(0x66, 0x89);
    else e1(0x89);
    modrm(reg, x, 0);
    if (tam == 4 && reg == 0) { guarda_ini = ini; guarda_fim = ncod; }
}

/* lea eax, [mem] (o mov eax, dirección). */
static void endereco_em_eax(item_t *x)
{
    base_utilizavel(x);
    if (x->base == B_ABS) { mov_eax((uint32_t)x->desloc); return; }
    if (x->base == B_EAX && x->desloc == 0) return;
    e1(0x8D);
    modrm(0, x, 0);
}

/* Real en la memoria -> XMM0 / ST(0). */
static void carregar_real_mem(item_t *x)
{
    base_utilizavel(x);
    if (usar_sse) { x87(); e3(0xF2, 0x0F, 0x10); modrm(0, x, 0); }   /* movsd xmm0, [mem] */
    else { x87(); e1(0xDD); modrm(0, x, 0); }                                            /* fld qword [mem] */
}

/* XMM0 / ST(0) -> [mem] (la x87 queda vacía). */
static void guardar_real_mem(item_t *x)
{
    if (usar_sse) { x87(); e3(0xF2, 0x0F, 0x11); modrm(0, x, 0); }   /* movsd [mem], xmm0 */
    else { x87(); e1(0xDD); modrm(3, x, 0); }                         /* fstp qword [mem] */
}

/* Real en XMM0/ST(0) -> pila del procesador (8 bytes). */
static void empilhar_real(void)
{
    esp_menos(8);
    if (usar_sse) { sse(0xF2, 0x11, 0x04); e1(0x24); }   /* movsd [esp], xmm0 */
    else f3(0xDD, 0x1C, 0x24);                           /* fstp qword [esp] */
}

static void desempilhar_real(void)
{
    if (usar_sse) { sse(0xF2, 0x10, 0x04); e1(0x24); }   /* movsd xmm0, [esp] */
    else f3(0xDD, 0x04, 0x24);                           /* fld qword [esp] */
    esp_mais(8);
}

/* EAX (inteiro) -> real. */
static void inteiro_para_real(void)
{
    if (usar_sse) sse(0xF2, 0x2A, 0xC0);   /* cvtsi2sd xmm0, eax */
    else { push_eax(); f3(0xDB, 0x04, 0x24); esp_mais(4); }   /* fild dword [esp] */
}

/* Descarta un real que nadie va a usar. */
static void descartar_real(void)
{
    if (!usar_sse) fstp_st0();
}

/* ------------------------------------------------------------ datos constantes */

/* Literales y conjuntos constantes que ya fueron a los datos en esta
 * pasada. Cuando se tira una rutina de la biblioteca, también sus datos:
 * las entradas posteriores se olvidan (ver descartar_rotina). */
typedef struct { int literal; int off; } lit_dados_t;
static lit_dados_t *lit_dados;
static int nlit_dados, caplit_dados;
static int off_fonte = -1;

static void esquecer_dados(void)
{
    while (nlit_dados && lit_dados[nlit_dados - 1].off >= ndados) nlit_dados--;
    if (off_fonte >= ndados) off_fonte = -1;
}

/* Dirección de un literal como string corta (el tamaño en el byte 0). */
static uint32_t endereco_literal(int k)
{
    for (int i = 0; i < nlit_dados; i++)
        if (lit_dados[i].literal == k) return DADOS_BASE + (uint32_t)lit_dados[i].off;
    int n = literais[k].n;
    int off = aloca_dados(n + 1, 1);
    dados[off] = (uint8_t)n;
    memcpy(dados + off + 1, literais[k].b, n);
    garantir((void **)&lit_dados, &caplit_dados, nlit_dados + 1, sizeof(lit_dados_t));
    lit_dados[nlit_dados].literal = k;
    lit_dados[nlit_dados++].off = off;
    return DADOS_BASE + (uint32_t)off;
}

static uint32_t endereco_conj(const uint8_t *c)
{
    int off = aloca_dados(32, 4);
    memcpy(dados + off, c, 32);
    return DADOS_BASE + (uint32_t)off;
}

/* ------------------------------------------------------------ marco y temporales */

static int nivel_atual;            /* 0 = corpo do programa principal */
static int local_desloc, local_max;

static int novo_local(int tam)
{
    local_desloc -= (tam + 3) & ~3;
    if (-local_desloc > local_max) local_max = -local_desloc;
    return local_desloc;
}

/* Temporal de string (255) o set en el marco actual. */
static void nova_temp(item_t *x, tipo_t *t)
{
    item_var(x, t, B_EBP, novo_local(t->tam));
    x->temp = true;
}

/* ------------------------------------------------------------ valores */

static void push_imm(int32_t v)
{
    if (cabe8(v)) e2(0x6A, v & 0xFF);
    else { e1(0x68); e4((uint32_t)v); }
}

/* Constante real -> ítem en la memoria (datos). */
static void real_na_memoria(item_t *x)
{
    uint32_t a = endereco_constante(x->real, false);
    item_var(x, &t_real, B_ABS, (int32_t)a);
}

/* Constante grande (string, set) -> variable en los datos. */
static void const_na_memoria(item_t *x)
{
    if (x->t->k == T_STRING) { int k = x->literal; tipo_t *t = x->t; item_var(x, t, B_ABS, (int32_t)endereco_literal(k)); return; }
    if (x->t->k == T_SET) { uint32_t a = endereco_conj(x->conj); tipo_t *t = x->t; item_var(x, t, B_ABS, (int32_t)a); return; }
    if (x->t->k == T_REAL) real_na_memoria(x);
}

/* Valor -> EAX (ordinal, puntero; string/set/record/array: la dirección) o
 * XMM0/ST(0) (real). */
static void carregar(item_t *x)
{
    switch (x->modo) {
    case M_REG: return;
    case M_CONST:
        if (x->t->k == T_REAL) { real_na_memoria(x); carregar_real_mem(x); break; }
        if (eh_grande(x->t)) { const_na_memoria(x); endereco_em_eax(x); break; }
        if (x->val == 0) e2(0x31, 0xC0);   /* xor eax, eax */
        else mov_eax((uint32_t)x->val);
        break;
    case M_VAR:
        if (x->t->k == T_REAL) carregar_real_mem(x);
        else if (eh_grande(x->t)) endereco_em_eax(x);
        else carregar_ord_mem(x);
        break;
    case M_PILHA:
        if (x->t->k == T_REAL) desempilhar_real();
        else e1(0x58);   /* pop eax */
        break;
    }
    x->modo = M_REG;
}

/* Valor (o dirección, si es grande) -> cima de la pila. */
static void empilhar(item_t *x)
{
    if (x->modo == M_PILHA) return;
    if (x->modo == M_CONST && !eh_grande(x->t)) {
        if (x->t->k == T_REAL) {
            uint32_t w[2];
            memcpy(w, &x->real, 8);
            push_imm((int32_t)w[1]);
            push_imm((int32_t)w[0]);
        } else {
            push_imm(x->val);
        }
    } else if (x->modo == M_CONST || (x->modo == M_VAR && x->base == B_ABS && eh_grande(x->t))) {
        if (x->modo == M_CONST) const_na_memoria(x);
        e1(0x68); e4((uint32_t)x->desloc);       /* push dirección */
    } else if (x->modo == M_VAR && x->t->k == T_REAL && x->base != B_PILHA) {
        e1(0xFF); modrm(6, x, 4);                /* push dword [mem+4] */
        e1(0xFF); modrm(6, x, 0);                /* push dword [mem] */
    } else if (x->modo == M_VAR && !eh_grande(x->t) && x->t->k != T_REAL && x->base != B_PILHA &&
               (x->t->tam == 4 || x->t->k == T_PTR)) {
        e1(0xFF); modrm(6, x, 0);                /* push dword [mem] */
    } else {
        carregar(x);
        if (x->t->k == T_REAL) empilhar_real();
        else push_eax();
    }
    x->modo = M_PILHA;
}

/* Rutina de la biblioteca llamada por el compilador (los argumentos ya están en la
 * pila, en el orden de sus parámetros). */
static proc_t *rotina_biblioteca(const char *nome)
{
    simbolo_t *s = nome_c(nome)->sim;
    if (!s || s->classe != S_PROC) falhar("error interno: falta la rutina '%s' en la biblioteca", nome);
    return s->p;
}

static void chamar_rotina(const char *nome) { chamar_proc(rotina_biblioteca(nome)); invalidar_ultimas(); }

/* Error de ejecución con la línea: "push línea; call rutina". */
static void erro_execucao(const char *rot)
{
    push_imm(tk()->linha);
    chamar_rotina(rot);
}

/* Comprueba el rango de EAX ({$R+}): fuera de [lo, hi], error 201 con la línea. */
static void conferir_faixa(int32_t lo, int32_t hi)
{
    if (!faixa_ativa()) return;
    if (lo == INT_MIN32 && hi == 2147483647) return;
    int p;
    if (lo == 0) {
        e1(0x3D); e4((uint32_t)hi);                     /* cmp eax, hi */
    } else {
        e2(0x8D, 0x88); e4((uint32_t)-lo);              /* lea ecx, [eax-lo] */
        e2(0x81, 0xF9); e4((uint32_t)hi - (uint32_t)lo);/* cmp ecx, hi-lo */
    }
    e2(0x76, 0);                                        /* jbe ok */
    p = ncod;
    erro_execucao("__erro_faixa");
    cod[p - 1] = (uint8_t)(ncod - p);
    invalidar_ultimas();
}

/* ------------------------------------------------------------ compatibilidade */

static bool mesmo_ordinal(tipo_t *a, tipo_t *b)
{
    if (a->k != b->k) return false;
    if (a->k == T_INT || a->k == T_CHAR || a->k == T_BOOL) return true;
    return raiz(a) == raiz(b);
}

/* ¿Un valor del tipo b puede guardarse en una variable del tipo a? */
static bool compativel(tipo_t *a, tipo_t *b)
{
    if (a->k == T_ERRO || b->k == T_ERRO) return true;
    if (a == b) return a->k != T_FILE;
    if (eh_ordinal(a)) return eh_ordinal(b) && mesmo_ordinal(a, b);
    switch (a->k) {
    case T_REAL: return b->k == T_REAL || b->k == T_INT;
    case T_STRING: return b->k == T_STRING || b->k == T_CHAR;
    case T_PTR:
        if (b->k == T_NIL) return true;
        return b->k == T_PTR && (!a->base || !b->base || a->base == b->base);
    case T_SET:
        if (b == &t_conj_vazio) return true;
        return b->k == T_SET && (a->base == NULL || b->base == NULL || mesmo_ordinal(a->base, b->base));
    case T_ARRAY:
        /* Dos "array[1..n] of T" declarados por separado son tipos distintos en
         * Pascal; se acepta si son iguales en la forma (como hace FPC con
         * los arrays abiertos). */
        return b->k == T_ARRAY && b->indice && a->indice && a->tam == b->tam && a->indice->lo == b->indice->lo &&
               compativel(a->base, b->base) && compativel(b->base, a->base);
    default: return false;
    }
}

static void exigir_compativel(tipo_t *a, tipo_t *b, const char *onde)
{
    if (!compativel(a, b)) erro("tipos incompatibles en %s: se esperaba %s y se encontró %s", onde, nome_tipo(a), nome_tipo(b));
}

/* lea ecx, [ebp+d]; push ecx (la dirección de una temporal, sin tocar EAX). */
static void empilhar_temp(item_t *t)
{
    if (cabe8(t->desloc)) { e3(0x8D, 0x4D, t->desloc & 0xFF); }
    else { e2(0x8D, 0x8D); e4((uint32_t)t->desloc); }
    e1(0x51);
}

/* String: char -> string. Constante: se convierte en literal; valor: temporal. */
static void para_string(item_t *x)
{
    if (x->t->k != T_CHAR) return;
    if (x->modo == M_CONST) {
        char c = (char)x->val;
        int k = novo_literal(&c, 1);
        item_const(x, &t_string, 0);
        x->literal = k;
        return;
    }
    empilhar(x);
    item_t t;
    nova_temp(&t, &t_string);
    empilhar_temp(&t);
    chamar_rotina("__str_de_char");   /* (c, destino) */
    *x = t;
}

/* Entero -> real (constante o valor). */
static void para_real(item_t *x)
{
    if (x->t->k != T_INT) return;
    if (x->modo == M_CONST) { x->real = (double)x->val; x->t = &t_real; return; }
    carregar(x);
    inteiro_para_real();
    x->t = &t_real;
}

/* Comprueba y convierte el valor x para guardarlo en el tipo t. */
static void converter_para(item_t *x, tipo_t *t, const char *onde)
{
    exigir_compativel(t, x->t, onde);
    if (t->k == T_REAL) para_real(x);
    else if (t->k == T_STRING) para_string(x);
    else if (eh_ordinal(t) && eh_ordinal(x->t) && x->t->k != T_ERRO) {
        if (x->modo == M_CONST) {
            if (x->val < t->lo || x->val > t->hi) erro("constante fuera de rango (%d no está en %d..%d)", x->val, t->lo, t->hi);
        } else if (x->t->lo < t->lo || x->t->hi > t->hi) {
            carregar(x);
            conferir_faixa(t->lo, t->hi);
        }
    }
}

/* Dirección del ítem grande -> pila (para las rutinas de la biblioteca). */
static void empilhar_endereco(item_t *x)
{
    if (x->modo == M_PILHA) return;
    if (x->modo == M_CONST) { empilhar(x); return; }
    if (x->modo == M_VAR && x->base == B_ABS) { e1(0x68); e4((uint32_t)x->desloc); x->modo = M_PILHA; return; }
    if (x->modo == M_VAR) endereco_em_eax(x);
    push_eax();
    x->modo = M_PILHA;
}


/* Copia de memoria: ESI = origen, EDI = destino, n bytes. */
static void copiar_bloco(int n)
{
    if (n % 4 == 0) { e1(0xB9); e4((uint32_t)(n / 4)); e2(0xF3, 0xA5); }   /* mov ecx, n/4; rep movsd */
    else { e1(0xB9); e4((uint32_t)n); e2(0xF3, 0xA4); }                     /* mov ecx, n; rep movsb */
}

/* Temporal que guarda una copia de x (string o set), para operar en ella. */
static void copiar_para_temp(item_t *x)
{
    if (x->modo == M_VAR && x->temp) return;
    item_t t;
    if (x->t->k == T_SET) {
        nova_temp(&t, x->t == &t_conj_vazio ? &t_conj_vazio : x->t);
        if (x->modo == M_CONST && x->conj) {
            /* conjunto constante: copia desde los datos */
            const_na_memoria(x);
        }
        endereco_em_eax(x);
        e2(0x89, 0xC6);                               /* mov esi, eax */
        if (cabe8(t.desloc)) e3(0x8D, 0x7D, t.desloc & 0xFF);
        else { e2(0x8D, 0xBD); e4((uint32_t)t.desloc); }   /* lea edi, [ebp+t] */
        copiar_bloco(32);
        *x = t;
        return;
    }
    para_string(x);
    if (x->modo == M_VAR && x->temp) return;
    nova_temp(&t, &t_string);
    empilhar_endereco(x);
    empilhar_temp(&t);
    push_imm(255);
    chamar_rotina("__str_atrib");   /* (origem, destino, capacidade) */
    *x = t;
}

/* ------------------------------------------------------------ código descartado */

/* Trozo cuyo código no vale (la rama muerta de "false and x", rutina de la
 * biblioteca no usada): analiza normalmente y después devuelve el código y los
 * datos a lo que eran. */
typedef struct { int cod, dados, bss, consts; bool desc; } marca_t;

static marca_t marcar(void)
{
    marca_t m = { ncod, ndados, nbss, nconstantes, descartando };
    descartando = true;
    return m;
}

static void voltar(marca_t m)
{
    ncod = m.cod;
    ndados = m.dados;
    nbss = m.bss;
    nconstantes = m.consts;
    descartando = m.desc;
    esquecer_dados();
    invalidar_ultimas();
}

/* ------------------------------------------------------------ expresiones */

static void expressao(item_t *x);
static void seletores(item_t *x);
static void chamada(proc_t *p, item_t *x);
static bool funcao_embutida(nome_t *n, item_t *x);

/* EAX = marco del nivel "alvo" (por el enlace estático). */
static void carregar_elo(int alvo)
{
    int h = nivel_atual - alvo;
    e3(0x8B, 0x45, 0x08);                         /* mov eax, [ebp+8] */
    for (int i = 1; i < h; i++) e3(0x8B, 0x40, 0x08);   /* mov eax, [eax+8] */
}

static void empilhar_elo(int alvo)
{
    if (alvo == nivel_atual) { e1(0x55); return; }   /* push ebp */
    carregar_elo(alvo);
    push_eax();
}

/* Ítem de una variable, de un parámetro o de un campo de "with". */
static void var_de_simbolo(simbolo_t *s, item_t *x)
{
    if (s->classe == S_CAMPO) {
        if (s->with_base == B_PILHA) {
            item_t p;
            item_var(&p, &t_pointer, B_EBP, s->with_desloc);
            carregar_ord_mem(&p);
            item_var(x, s->campo->t, B_EAX, s->campo->desloc);
        } else {
            item_var(x, s->campo->t, s->with_base, s->with_desloc + s->campo->desloc);
        }
        return;
    }
    if (s->nivel == 0) item_var(x, s->t, B_ABS, s->valor);
    else if (s->nivel == nivel_atual) item_var(x, s->t, B_EBP, s->valor);
    else {
        carregar_elo(s->nivel);
        item_var(x, s->t, B_EAX, s->valor);
    }
    if (s->por_ref) {
        e1(0x8B); modrm(0, x, 0);                 /* mov eax, [lugar]: la dirección */
        item_var(x, s->t, B_EAX, 0);
    }
}

/* El "high" de un array abierto (parámetro "array of T") -> ECX. */
static simbolo_t *aberto_atual;   /* el último array abierto leído por var_de_simbolo */

static void carregar_alto_ecx(simbolo_t *s)
{
    if (s->nivel == nivel_atual) { e2(0x8B, 0x8D); e4((uint32_t)s->alto); }   /* mov ecx, [ebp+alto] */
    else { carregar_elo(s->nivel); e2(0x8B, 0x88); e4((uint32_t)s->alto); }    /* mov ecx, [eax+alto] */
}

/* ECX = valor ordinal (constante, variable simple o cima de la pila). */
static void carregar_em_ecx(item_t *x)
{
    if (x->modo == M_CONST) { e1(0xB9); e4((uint32_t)x->val); return; }
    if (x->modo == M_PILHA) { pop_ecx(); return; }
    if (x->modo == M_REG) { e2(0x89, 0xC1); return; }   /* mov ecx, eax */
    tipo_t *t = x->t;
    int tam = t->k == T_PTR || t->k == T_NIL ? 4 : t->tam;
    if (tam == 1) e2(0x0F, sem_sinal(t) ? 0xB6 : 0xBE);
    else if (tam == 2) e2(0x0F, sem_sinal(t) ? 0xB7 : 0xBF);
    else e1(0x8B);
    modrm(1, x, 0);
}

static int log2_exato(int n)
{
    for (int k = 0; k < 31; k++) if (n == 1 << k) return k;
    return -1;
}

/* x[i]: x es array o string (en la memoria). */
static void indexar(item_t *x, simbolo_t *aberto)
{
    tipo_t *t = x->t;
    int32_t lo = 0, hi = 0;
    tipo_t *el, *ti = &t_integer;
    if (t->k == T_STRING) { hi = t->hi; el = &t_char; }
    else if (t->k == T_ARRAY) {
        el = t->base;
        if (t->indice) { lo = t->indice->lo; hi = t->indice->hi; ti = t->indice; }
    } else {
        if (t->k != T_ERRO) erro("'[' en algo que no es un array ni un string");
        item_t i;
        expressao(&i);
        x->t = &t_erro;
        return;
    }
    if (x->modo == M_CONST) const_na_memoria(x);
    if (x->modo == M_REG) item_var(x, t, B_EAX, 0);
    item_t salvo = *x;
    int antes = ncod, depois = -1;
    if (x->base == B_EAX || x->base == B_ECX) {
        endereco_em_eax(x);
        push_eax();
        x->base = B_PILHA;
        x->desloc = 0;
        depois = ncod;
    }
    item_t i;
    expressao(&i);
    if (!eh_ordinal(i.t) || !mesmo_ordinal(ti, i.t)) {
        if (i.t->k != T_ERRO) erro("índice de tipo %s, se esperaba %s", nome_tipo(i.t), nome_tipo(ti));
        x->t = &t_erro;
        return;
    }
    int tam = el->tam;
    if (i.modo == M_CONST && (t->k != T_ARRAY || t->indice)) {
        if (i.val < lo || i.val > hi) erro("índice %d fuera de rango (%d..%d)", i.val, lo, hi);
        if (ncod == depois) { ncod = antes; *x = salvo; }   /* no hacía falta guardar la dirección */
        if (x->modo == M_VAR && x->base == B_PILHA) { pop_ecx(); x->base = B_ECX; }
        x->desloc += (i.val - lo) * tam;
        x->t = el;
        return;
    }
    carregar(&i);
    if (t->k == T_ARRAY && !t->indice) {
        if (faixa_ativa()) {
            /* array abierto: 0..high, con el high en el marco de quien lo recibió */
            e1(0x52);                                    /* push edx */
            e2(0x89, 0xC2);                              /* mov edx, eax */
            carregar_alto_ecx(aberto);
            e2(0x89, 0xD0);                              /* mov eax, edx */
            e1(0x5A);                                    /* pop edx */
            e2(0x39, 0xC8);                              /* cmp eax, ecx */
            e2(0x76, 0);
            int p = ncod;
            erro_execucao("__erro_faixa");
            cod[p - 1] = (uint8_t)(ncod - p);
        }
    } else if (i.t->lo < lo || i.t->hi > hi) {
        conferir_faixa(lo, hi);
    }
    int k = log2_exato(tam);
    /* tamaño 1, 2, 4 u 8: el lea hace la escala y la suma de una vez (D108) */
    bool lea = otim(OTIM_IMEDIATOS) && k >= 1 && k <= 3 && (x->base == B_PILHA || x->base == B_EBP);
    if (!lea) {
        if (k > 0) e3(0xC1, 0xE0, k);                    /* shl eax, k */
        else if (k < 0) { e2(0x69, 0xC0); e4((uint32_t)tam); }   /* imul eax, eax, tam */
    }
    switch (x->base) {
    case B_ABS: break;
    case B_EBP:
        if (lea) e3(0x8D, 0x44, k << 6 | 0x05), e1(0);   /* lea eax, [ebp+eax*tam] */
        else e2(0x01, 0xE8);                             /* add eax, ebp */
        break;
    case B_PILHA:
        pop_ecx();
        if (lea) e3(0x8D, 0x04, k << 6 | 0x01);          /* lea eax, [ecx+eax*tam] */
        else e2(0x01, 0xC8);                             /* add eax, ecx */
        break;
    }
    x->base = B_EAX;
    x->desloc -= lo * tam;
    x->t = el;
}

static void seletores(item_t *x)
{
    simbolo_t *aberto = aberto_atual;
    aberto_atual = NULL;
    for (;;) {
        if (tt() == '[') {
            avancar();
            for (;;) {
                indexar(x, aberto);
                aberto = NULL;
                if (!aceitar(',')) break;
            }
            esperar_ficha(']');
            continue;
        }
        if (tt() == '.' && tt_a(1) == F_NOME) {
            avancar();
            nome_t *n = esperar_nome();
            if (x->t->k != T_RECORD) {
                if (x->t->k != T_ERRO) erro("'.%s' en algo que no es un record", n->s);
                x->t = &t_erro;
                continue;
            }
            campo_t *c = x->t->campos;
            while (c && c->nome != n) c = c->prox;
            if (!c) { erro("el record no tiene el campo '%s'", n->s); x->t = &t_erro; continue; }
            if (x->modo == M_REG) item_var(x, x->t, B_EAX, 0);
            x->desloc += c->desloc;
            x->t = c->t;
            continue;
        }
        if (tt() == '^') {
            avancar();
            if (x->t->k != T_PTR || !x->t->base) {
                if (x->t->k != T_ERRO) erro("'^' en algo que no es un puntero con tipo");
                x->t = &t_erro;
                continue;
            }
            tipo_t *b = x->t->base;
            carregar(x);
            item_var(x, b, B_EAX, 0);
            continue;
        }
        return;
    }
}

/* Typecast "T(x)" entre ordinales y punteros. */
static void conversao(tipo_t *t, item_t *x)
{
    esperar_ficha('(');
    expressao(x);
    esperar_ficha(')');
    bool ok = (eh_ordinal(t) || t->k == T_PTR) && (eh_ordinal(x->t) || x->t->k == T_PTR || x->t->k == T_NIL);
    if (t->k == T_REAL && (x->t->k == T_INT || x->t->k == T_REAL)) { para_real(x); return; }
    if (!ok) {
        if (x->t->k != T_ERRO) erro("no se puede convertir %s a %s", nome_tipo(x->t), nome_tipo(t));
        x->t = &t_erro;
        return;
    }
    if (x->modo != M_CONST) carregar(x);
    if (x->modo == M_REG && eh_ordinal(t) && t->tam < 4) {
        /* corta al tamaño del tipo, como Turbo Pascal */
        if (t->tam == 1) e3(0x0F, sem_sinal(t) ? 0xB6 : 0xBE, 0xC0);
        else e3(0x0F, sem_sinal(t) ? 0xB7 : 0xBF, 0xC0);
    } else if (x->modo == M_CONST && eh_ordinal(t) && t->tam < 4) {
        x->val = t->tam == 1 ? (sem_sinal(t) ? (uint8_t)x->val : (int8_t)x->val)
                             : (sem_sinal(t) ? (uint16_t)x->val : (int16_t)x->val);
    }
    x->t = t;
}

static void conjunto(item_t *x);

static void identificador(item_t *x)
{
    nome_t *n = tk()->nome;
    simbolo_t *s = n->sim;
    if (!s && funcao_embutida(n, x)) return;
    if (!s) s = procurar(n);
    if (!s) {
        erro("'%s' no está declarado", n->s);
        avancar();
        item_const(x, &t_erro, 0);
        if (aceitar('(')) { int nivel = 1; while (nivel && tt() != F_FIM) { if (tt() == '(') nivel++; if (tt() == ')') nivel--; avancar(); } }
        return;
    }
    avancar();
    switch (s->classe) {
    case S_CONST:
        item_const(x, s->t, s->valor);
        x->real = s->real;
        x->literal = s->literal;
        x->conj = s->conj;
        return;
    case S_VAR: case S_CAMPO:
        var_de_simbolo(s, x);
        if (s->classe == S_VAR && s->t->k == T_ARRAY && !s->t->indice) aberto_atual = s;
        seletores(x);
        return;
    case S_TIPO:
        if (tt() != '(') { erro("uso inesperado del tipo '%s'", n->s); item_const(x, &t_erro, 0); return; }
        conversao(s->t, x);
        seletores(x);
        return;
    case S_PROC:
        if (!s->p->ret) {
            erro("'%s' es un procedimiento: no devuelve valor", n->s);
            item_t lixo;
            chamada(s->p, &lixo);
            item_const(x, &t_erro, 0);
            return;
        }
        chamada(s->p, x);
        seletores(x);
        return;
    default:
        erro("uso inesperado de '%s'", n->s);
        item_const(x, &t_erro, 0);
    }
}

static void fator(item_t *x);

/* "not x" */
static void negar_logico(item_t *x)
{
    if (x->t->k == T_BOOL) {
        if (x->modo == M_CONST) { x->val = !x->val; return; }
        carregar(x);
        e3(0x83, 0xF0, 0x01);              /* xor eax, 1 */
        x->t = &t_boolean;
        return;
    }
    if (x->t->k == T_INT) {
        if (x->modo == M_CONST) { x->val = ~x->val; x->t = &t_integer; return; }
        carregar(x);
        e2(0xF7, 0xD0);                    /* not eax */
        x->t = &t_integer;
        return;
    }
    if (x->t->k != T_ERRO) erro("'not' necesita un boolean o un entero");
    x->t = &t_erro;
}

/* "-x" */
static void negar_num(item_t *x)
{
    if (x->t->k == T_INT) {
        if (x->modo == M_CONST) { x->val = (int32_t)(0u - (uint32_t)x->val); x->t = &t_integer; return; }
        carregar(x);
        e2(0xF7, 0xD8);                    /* neg eax */
        x->t = &t_integer;
        return;
    }
    if (x->t->k == T_REAL) {
        if (x->modo == M_CONST) { x->real = -x->real; return; }
        carregar(x);
        if (usar_sse) {
            sse(0xF2, 0x10, 0x3D); e4(endereco_bits(0, 0x80000000u, false));   /* movsd xmm7, [-0.0] */
            sse(0x66, 0x57, XMM(0, 7));                                        /* xorpd xmm0, xmm7 */
        } else {
            f2(0xD9, 0xE0);                                                    /* fchs */
        }
        return;
    }
    if (x->t->k != T_ERRO) erro("'-' necesita un número");
    x->t = &t_erro;
}

static tipo_t *tipo_conj(tipo_t *el)
{
    tipo_t *t = novo_tipo(T_SET, 32);
    t->base = el;
    return t;
}

/* [a, b..c, ...] */
static void conjunto(item_t *x)
{
    avancar();
    uint8_t *c = memoria(32);
    tipo_t *el = NULL;
    bool tem_temp = false, tem_const = false;
    item_t t;
    if (!aceitar(']')) {
        for (;;) {
            item_t a, b;
            expressao(&a);
            if (!eh_ordinal(a.t)) { if (a.t->k != T_ERRO) erro("los elementos de un conjunto deben ser ordinales"); a.t = &t_erro; }
            else if (el && !mesmo_ordinal(el, a.t)) erro("elemento de tipo %s en un conjunto de %s", nome_tipo(a.t), nome_tipo(el));
            else if (!el) el = raiz(a.t);
            bool faixa = aceitar(F_PP);
            if (faixa && a.modo != M_CONST) empilhar(&a);
            if (faixa) expressao(&b);
            else b = a;
            if (a.modo == M_CONST && b.modo == M_CONST) {
                if (a.val < 0 || a.val > 255 || b.val < 0 || b.val > 255) erro("elemento fuera de 0..255 en un conjunto");
                else for (int v = a.val; v <= b.val; v++) c[v >> 3] |= (uint8_t)(1 << (v & 7));
                tem_const = true;
            } else {
                if (!tem_temp) {
                    /* lo que esté en EAX va a la pila antes de poner a cero la temporal */
                    item_t *v = faixa ? &b : &a;
                    if (v->modo == M_REG || (v->modo == M_VAR && v->base != B_ABS && v->base != B_EBP)) empilhar(v);
                    nova_temp(&t, &t_conj_vazio);
                    if (cabe8(t.desloc)) e3(0x8D, 0x7D, t.desloc & 0xFF);
                    else { e2(0x8D, 0xBD); e4((uint32_t)t.desloc); }   /* lea edi, [ebp+t] */
                    e2(0x31, 0xC0); e1(0xB9); e4(8); e2(0xF3, 0xAB);  /* xor eax, eax; mov ecx, 8; rep stosd */
                    tem_temp = true;
                }
                if (faixa) {
                    if (a.modo == M_CONST) {
                        /* la constante no se apiló antes de b: pone en el orden (a, b) */
                        empilhar(&b);
                        e1(0x59);                       /* pop ecx (b) */
                        push_imm(a.val);
                        e1(0x51);                       /* push ecx */
                    } else {
                        empilhar(&b);
                    }
                    empilhar_temp(&t);
                    chamar_rotina("__conj_faixa");     /* (a, b, destino) */
                } else {
                    carregar(&a);
                    e1(0x3D); e4(255);                  /* cmp eax, 255 */
                    e2(0x77, 0);                        /* ja fin */
                    int p = ncod;
                    e2(0x0F, 0xAB); modrm(0, &t, 0);    /* bts [ebp+t], eax */
                    cod[p - 1] = (uint8_t)(ncod - p);
                }
            }
            if (!aceitar(',')) break;
        }
        esperar_ficha(']');
    }
    tipo_t *tc = el ? tipo_conj(el) : &t_conj_vazio;
    if (!tem_temp) {
        item_const(x, tc, 0);
        x->conj = c;
        return;
    }
    if (tem_const) {
        item_t k;
        item_const(&k, tc, 0);
        k.conj = c;
        empilhar(&k);
        empilhar_temp(&t);
        push_imm(0);
        chamar_rotina("__conj_operar");   /* destino := destino + origem */
    }
    t.t = tc;
    *x = t;
}

static void fator(item_t *x)
{
    ficha_t *t = tk();
    switch (t->tipo) {
    case F_INT: item_const(x, &t_integer, t->val); avancar(); return;
    case F_REAL: item_const(x, &t_real, 0); x->real = t->real; avancar(); return;
    case F_TEXTO:
        if (literais[t->val].n == 1) item_const(x, &t_char, (uint8_t)literais[t->val].b[0]);
        else { item_const(x, &t_string, 0); x->literal = t->val; }
        avancar();
        return;
    case P_NIL: item_const(x, &t_nil, 0); avancar(); return;
    case '(':
        avancar();
        expressao(x);
        esperar_ficha(')');
        if (tt() == '^' || tt() == '[' || tt() == '.') seletores(x);
        return;
    case P_NOT: avancar(); fator(x); negar_logico(x); return;
    case '@': {
        avancar();
        /* D171: @Rutina = la direccion de un procedimiento de nivel 1 con un
         * parametro (la entrada de un hilo); queda como Pointer. */
        if (tt() == F_NOME) {
            simbolo_t *sp = tk()->nome->sim;
            if (!sp) sp = procurar(tk()->nome);
            if (sp && sp->classe == S_PROC) {
                proc_t *rp = sp->p;
                if (rp->ret || rp->np != 1 || rp->nivel > 1)
                    erro("@%s: solo se puede tomar la direccion de un procedimiento de primer nivel con un parametro", sp->nome->s);
                else if (!rp->definida && passada == 2)
                    erro("@%s: el procedimiento debe estar definido antes", sp->nome->s);
                avancar();
                registrar_ref(rp);
                item_const(x, &t_pointer, rp->definida ? (int32_t)(COD_BASE + (uint32_t)rp->pos) : 0);
                return;
            }
        }
        fator(x);
        if (x->modo != M_VAR) { if (x->t->k != T_ERRO) erro("'@' necesita una variable"); x->t = &t_erro; return; }
        tipo_t *p = &t_pointer;   /* como en Turbo Pascal ($T-): puntero sin tipo */
        if (x->base == B_ABS) item_const(x, p, x->desloc);
        else { endereco_em_eax(x); x->modo = M_REG; x->t = p; }
        return;
    }
    case '[': conjunto(x); return;
    case F_NOME: identificador(x); return;
    default:
        falhar("se esperaba una expresión y se encontró %s", descrever_atual());
    }
}

/* ---- operaciones ---- */

static bool eh_comparacao(int op) { return op == '=' || op == F_DIF || op == '<' || op == '>' || op == F_MENIG || op == F_MAIIG; }

static int cc_comparacao(int op, bool sem)   /* setcc */
{
    switch (op) {
    case '=': return 0x94;
    case F_DIF: return 0x95;
    case '<': return sem ? 0x92 : 0x9C;
    case F_MENIG: return sem ? 0x96 : 0x9E;
    case '>': return sem ? 0x97 : 0x9F;
    default: return sem ? 0x93 : 0x9D;
    }
}

static int trocar_comparacao(int op)   /* a < b  ==  b > a */
{
    switch (op) {
    case '<': return '>';
    case '>': return '<';
    case F_MENIG: return F_MAIIG;
    case F_MAIIG: return F_MENIG;
    default: return op;
    }
}

static void resultado_bool(item_t *x, int cc)
{
    setcc(cc);
    x->modo = M_REG;
    x->t = &t_boolean;
}

/* Salta si EAX (boolean) es falso, sumando el salto a la lista; un &&/|| recién montado salta directo. */
static int saltar_se_falso(int lista)
{
    if (logico_saltar_se_falso(&lista)) return lista;
    test_eax();
    return saltar(0x84, lista);
}

static void conferir_divisor_ecx(void)
{
    if (compilando_biblioteca) return;
    e2(0x85, 0xC9);                       /* test ecx, ecx */
    e2(0x75, 0);                          /* jnz ok */
    int p = ncod;
    erro_execucao("__erro_div0");
    cod[p - 1] = (uint8_t)(ncod - p);
}

/* EAX = EAX op ECX (esquerdo em EAX) ou ECX op EAX (esquerdo em ECX). */
static void op_regs(int op, bool esq_ecx, item_t *x)
{
    switch (op) {
    case '+': e2(0x01, 0xC8); break;
    case '*': e3(0x0F, 0xAF, 0xC1); break;
    case P_AND: e2(0x21, 0xC8); break;
    case P_OR: e2(0x09, 0xC8); break;
    case P_XOR: e2(0x31, 0xC8); break;
    case '-':
        if (esq_ecx) { e2(0x29, 0xC1); e2(0x89, 0xC8); }   /* sub ecx, eax; mov eax, ecx */
        else e2(0x29, 0xC8);                               /* sub eax, ecx */
        break;
    case P_DIV: case P_MOD:
        if (esq_ecx) e1(0x91);                             /* xchg eax, ecx */
        conferir_divisor_ecx();
        e1(0x99); e2(0xF7, 0xF9);                          /* cdq; idiv ecx */
        if (op == P_MOD) e2(0x89, 0xD0);                   /* mov eax, edx */
        break;
    case P_SHL: case P_SHR:
        if (esq_ecx) e1(0x91);
        e2(0xD3, op == P_SHL ? 0xE0 : 0xE8);               /* shl/shr eax, cl */
        break;
    default:
        if (esq_ecx) e2(0x39, 0xC1);                       /* cmp ecx, eax */
        else e2(0x39, 0xC8);                               /* cmp eax, ecx */
        resultado_bool(x, cc_comparacao(op, false));
        return;
    }
    x->modo = M_REG;
}

/* EAX = EAX op v. Con OTIM_REDUCAO, x*2^k se convierte en desplazamiento, x*0, x*1 y
 * x+0 no generan la cuenta, y div/mod por constante no prueban el divisor. */
static void op_imediato(int op, int32_t v, item_t *x)
{
    bool red = otim(OTIM_REDUCAO);
    switch (op) {
    case '+': if (v) aplicar_imediato(0, 0, v, 0x05); break;
    case '-': if (v) aplicar_imediato(5, 0, v, 0x2D); break;
    case '*':
        if (red) {
            int k = v > 0 ? pot2((uint32_t)v) : -1;
            if (v == 1) break;
            if (v == 0) { e2(0x31, 0xC0); break; }
            if (k > 0) { e3(0xC1, 0xE0, k); break; }
            if (v == 3 || v == 5 || v == 9) { e3(0x8D, 0x04, (v == 3 ? 0x40 : v == 5 ? 0x80 : 0xC0)); break; }
        }
        if (otim(OTIM_IMEDIATOS) && v >= -128 && v <= 127) e3(0x6B, 0xC0, v & 0xFF);
        else { e2(0x69, 0xC0); e4((uint32_t)v); }
        break;
    case P_AND:
        if (red && v == -1) break;
        if (red && v == 0) { e2(0x31, 0xC0); break; }
        aplicar_imediato(4, 0, v, 0x25);
        break;
    case P_OR: if (!(red && v == 0)) aplicar_imediato(1, 0, v, 0x0D); break;
    case P_XOR: if (!(red && v == 0)) aplicar_imediato(6, 0, v, 0x35); break;
    case P_SHL: case P_SHR:
        if (red && (v & 31) == 0) break;
        e3(0xC1, op == P_SHL ? 0xE0 : 0xE8, v & 31);
        break;
    case P_DIV: case P_MOD:
        dividir_constante(op == P_MOD, false, v);
        break;
    default:
        aplicar_imediato(7, 0, v, 0x3D);           /* cmp eax, v */
        resultado_bool(x, cc_comparacao(op, false));
        return;
    }
    x->modo = M_REG;
}

/* EAX = EAX op [y], con y una variable de 32 bits simple ("add eax, [mem]",
 * "cmp eax, [mem]"). Devuelve false si no es el caso (el llamador usa ECX). */
static bool op_memoria(int op, item_t *y, item_t *x)
{
    if (!otim(OTIM_IMEDIATOS) || y->modo != M_VAR || !(y->base == B_ABS || y->base == B_EBP)) return false;
    tipo_t *t = y->t;
    if (!(t->k == T_INT && t->tam == 4 && !sem_sinal(t))) return false;
    switch (op) {
    case '+': e1(0x03); break;
    case '-': e1(0x2B); break;
    case P_AND: e1(0x23); break;
    case P_OR: e1(0x0B); break;
    case P_XOR: e1(0x33); break;
    case '*': e2(0x0F, 0xAF); break;
    default:
        if (!eh_comparacao(op)) return false;
        e1(0x3B);
        modrm(0, y, 0);
        resultado_bool(x, cc_comparacao(op, false));
        return true;
    }
    modrm(0, y, 0);
    x->modo = M_REG;
    return true;
}

static int32_t dobrar_ord(int op, int32_t a, int32_t b, bool *ok)
{
    uint32_t ua = (uint32_t)a, ub = (uint32_t)b;
    *ok = true;
    switch (op) {
    case '+': return (int32_t)(ua + ub);
    case '-': return (int32_t)(ua - ub);
    case '*': return (int32_t)(ua * ub);
    case P_DIV: case P_MOD:
        if (!b) { erro("división por cero"); return 0; }
        if (a == INT_MIN32 && b == -1) return op == P_DIV ? a : 0;
        return op == P_DIV ? a / b : a % b;
    case P_AND: return a & b;
    case P_OR: return a | b;
    case P_XOR: return a ^ b;
    case P_SHL: return (int32_t)(ua << (b & 31));
    case P_SHR: return (int32_t)(ua >> (b & 31));
    case '=': return a == b;
    case F_DIF: return a != b;
    case '<': return a < b;
    case '>': return a > b;
    case F_MENIG: return a <= b;
    case F_MAIIG: return a >= b;
    }
    *ok = false;
    return 0;
}

static bool eh_simples(item_t *x) { return x->modo == M_CONST || (x->modo == M_VAR && (x->base == B_ABS || x->base == B_EBP)); }

static void op_ordinal(int op, item_t *x, item_t *y)
{
    bool comp = eh_comparacao(op);
    tipo_t *r = comp ? &t_boolean : (x->t->k == T_BOOL ? &t_boolean : &t_integer);
    if (x->modo == M_CONST && y->modo == M_CONST) {
        bool ok;
        int32_t v = dobrar_ord(op, x->val, y->val, &ok);
        item_const(x, r, v);
        return;
    }
    if (op == P_DIV || op == P_MOD) {
        if (y->modo == M_CONST && y->val == 0) erro("división por cero");
    }
    if (x->modo == M_REG && eh_simples(y)) {
        if (y->modo == M_CONST && (y->val != 0 || (op != P_DIV && op != P_MOD)) && (otim(OTIM_REDUCAO) || (op != P_DIV && op != P_MOD)))
            op_imediato(op, y->val, x);
        else if (!op_memoria(op, y, x)) { carregar_em_ecx(y); op_regs(op, false, x); }
    } else if (otim(OTIM_IMEDIATOS) && x->modo == M_VAR && eh_simples(x) && y->modo == M_CONST &&
               eh_ordinal(x->t) && (y->val != 0 || (op != P_DIV && op != P_MOD)) &&
               (otim(OTIM_REDUCAO) || (op != P_DIV && op != P_MOD))) {
        /* variable op constante: carga la variable y opera con el inmediato */
        carregar(x);
        op_imediato(op, y->val, x);
    } else if (otim(OTIM_IMEDIATOS) && x->modo == M_VAR && eh_simples(x) && y->modo == M_VAR && eh_simples(y) &&
               eh_ordinal(x->t)) {
        carregar(x);
        if (!op_memoria(op, y, x)) { carregar_em_ecx(y); op_regs(op, false, x); }
    } else if (x->modo == M_CONST && (op == '+' || op == '*' || op == P_AND || op == P_OR || op == P_XOR || comp)) {
        int32_t v = x->val;
        carregar(y);
        op_imediato(comp ? trocar_comparacao(op) : op, v, x);
    } else {
        carregar(y);
        carregar_em_ecx(x);
        op_regs(op, true, x);
    }
    if (!comp) x->t = r;
}

/* Real: x (izquierdo) en la memoria, en la pila o constante; y ya calculado. */
static void op_real(int op, item_t *x, item_t *y)
{
    bool comp = eh_comparacao(op);
    if (x->modo == M_CONST && y->modo == M_CONST) {
        double a = x->t->k == T_INT ? (double)x->val : x->real, b = y->t->k == T_INT ? (double)y->val : y->real;
        if (comp) {
            int v = op == '=' ? a == b : op == F_DIF ? a != b : op == '<' ? a < b : op == '>' ? a > b : op == F_MENIG ? a <= b : a >= b;
            item_const(x, &t_boolean, v);
            return;
        }
        double r = op == '+' ? a + b : op == '-' ? a - b : op == '*' ? a * b : a / b;
        if (op == '/' && b == 0) erro("división por cero");
        item_const(x, &t_real, 0);
        x->real = r;
        return;
    }
    /* derecho -> XMM0/ST(0) */
    if (y->t->k == T_INT) {
        if (y->modo == M_CONST) { y->real = (double)y->val; y->t = &t_real; }
        else { carregar(y); inteiro_para_real(); y->t = &t_real; }
    }
    carregar(y);
    int esp = 0;   /* bytes a quitar de la pila al final */
    item_t m;
    bool registrador = false;   /* izquierdo en XMM1 / ST(0) (con el derecho en ST(1)) */
    if (x->t->k == T_INT) {
        if (x->modo == M_CONST) { x->real = (double)x->val; x->t = &t_real; real_na_memoria(x); m = *x; }
        else {
            if (x->modo == M_PILHA) {
                item_var(&m, &t_integer, B_ESP, 0);
                if (usar_sse) { x87(); e3(0xF2, 0x0F, 0x2A); modrm(1, &m, 0); }   /* cvtsi2sd xmm1, [esp] */
                else { x87(); e1(0xDB); modrm(0, &m, 0); }                       /* fild dword [esp] */
                esp_mais(4);
            } else {
                carregar_em_ecx(x);
                if (usar_sse) sse(0xF2, 0x2A, 0xC9);           /* cvtsi2sd xmm1, ecx */
                else { e1(0x51); f3(0xDB, 0x04, 0x24); esp_mais(4); }
            }
            registrador = true;
        }
    } else if (x->modo == M_CONST) {
        real_na_memoria(x);
        m = *x;
    } else if (x->modo == M_PILHA) {
        item_var(&m, &t_real, B_ESP, 0);
        esp = 8;
    } else {
        m = *x;
        base_utilizavel(&m);
    }
    if (registrador) {
        /* SSE: izquierdo en XMM1, derecho en XMM0. x87: ST0 = izquierdo, ST1 = derecho. */
        if (usar_sse) {
            switch (op) {
            case '+': sse(0xF2, 0x58, XMM(0, 1)); break;
            case '*': sse(0xF2, 0x59, XMM(0, 1)); break;
            case '-': sse(0xF2, 0x5C, XMM(1, 0)); sse(0, 0x28, XMM(0, 1)); break;
            case '/': sse(0xF2, 0x5E, XMM(1, 0)); sse(0, 0x28, XMM(0, 1)); break;
            default: sse(0x66, 0x2E, XMM(1, 0)); break;   /* ucomisd xmm1, xmm0 */
            }
        } else {
            switch (op) {
            case '+': f2(0xDE, 0xC1); break;   /* faddp */
            case '*': f2(0xDE, 0xC9); break;   /* fmulp */
            case '-': f2(0xDE, 0xE1); break;   /* fsubrp: ST1 = ST0 - ST1 */
            case '/': f2(0xDE, 0xF1); break;   /* fdivrp */
            default: f2(0xDF, 0xE9); fstp_st0(); break;   /* fucomip; fstp */
            }
        }
    } else if (usar_sse) {
        switch (op) {
        case '+': x87(); e3(0xF2, 0x0F, 0x58); modrm(0, &m, 0); break;   /* addsd xmm0, [m] */
        case '*': x87(); e3(0xF2, 0x0F, 0x59); modrm(0, &m, 0); break;
        case '-': case '/':
            x87(); e3(0xF2, 0x0F, 0x10); modrm(1, &m, 0);                 /* movsd xmm1, [m] */
            sse(0xF2, op == '-' ? 0x5C : 0x5E, XMM(1, 0));
            sse(0, 0x28, XMM(0, 1));
            break;
        default:
            x87(); e3(0xF2, 0x0F, 0x10); modrm(1, &m, 0);
            sse(0x66, 0x2E, XMM(1, 0));                                  /* ucomisd xmm1, xmm0 */
        }
    } else {
        switch (op) {
        case '+': x87(); e1(0xDC); modrm(0, &m, 0); break;   /* fadd qword [m] */
        case '*': x87(); e1(0xDC); modrm(1, &m, 0); break;   /* fmul */
        case '-': x87(); e1(0xDC); modrm(5, &m, 0); break;   /* fsubr: ST0 = [m] - ST0 */
        case '/': x87(); e1(0xDC); modrm(7, &m, 0); break;   /* fdivr */
        default:
            x87(); e1(0xDD); modrm(0, &m, 0);                /* fld [m]: ST0 = izquierdo */
            f2(0xDF, 0xE9); fstp_st0();                      /* fucomip; fstp */
        }
    }
    if (esp) { if (comp) { e3(0x8D, 0x64, 0x24); e1(esp); } else esp_mais(esp); }   /* lea esp, [esp+n]: no cambia las flags */
    if (comp) resultado_bool(x, cc_comparacao(op, true));
    else { x->modo = M_REG; x->t = &t_real; }
}

/* Dirección de un ítem que no está en registro -> pila, sin tocar
 * EAX (constante o variable global/local). */
static void empilhar_end_simples(item_t *x)
{
    if (x->modo == M_CONST) const_na_memoria(x);
    if (x->base == B_ABS) { e1(0x68); e4((uint32_t)x->desloc); }
    else empilhar_temp(x);   /* lea ecx, [ebp+d]; push ecx */
    x->modo = M_PILHA;
}

/* Las direcciones de x e y en la pila, para una rutina (a, b). Si x todavía no
 * se apiló, y si y está en EAX, va primero: devuelve true (cambiado). */
static bool empilhar_par(item_t *x, item_t *y)
{
    if (x->modo == M_PILHA) { empilhar_endereco(y); return false; }
    if (y->modo == M_REG || (y->modo == M_VAR && (y->base == B_EAX || y->base == B_ECX || y->base == B_PILHA))) {
        empilhar_endereco(y);
        empilhar_end_simples(x);
        return true;
    }
    empilhar_end_simples(x);
    if (y->modo == M_CONST || (y->modo == M_VAR && (y->base == B_ABS || y->base == B_EBP))) empilhar_end_simples(y);
    else empilhar_endereco(y);
    return false;
}

/* String: concatenación (x ya es temporal) o comparación. */
static void op_string(int op, item_t *x, item_t *y)
{
    if (op == '+') {
        if (x->modo == M_CONST && y->modo == M_CONST) {
            para_string(x);
            para_string(y);
            char b[512];
            int na = literais[x->literal].n, nb = literais[y->literal].n;
            memcpy(b, literais[x->literal].b, na);
            memcpy(b + na, literais[y->literal].b, nb);
            int n = na + nb > 255 ? 255 : na + nb;
            x->literal = novo_literal(b, n);
            return;
        }
        if (y->t->k == T_CHAR) {
            empilhar(y);
            if (!(x->modo == M_VAR && x->temp)) copiar_para_temp(x);
            empilhar_temp(x);
            chamar_rotina("__str_juntar_c");   /* (c, destino) */
        } else {
            empilhar_endereco(y);
            if (!(x->modo == M_VAR && x->temp)) copiar_para_temp(x);
            empilhar_temp(x);
            chamar_rotina("__str_juntar");     /* (origem, destino) */
        }
        x->t = &t_string;
        return;
    }
    if (x->modo == M_CONST && y->modo == M_CONST) {
        para_string(x);
        para_string(y);
        literal_t *a = &literais[x->literal], *b = &literais[y->literal];
        int i = 0, c = 0;
        while (i < a->n && i < b->n && a->b[i] == b->b[i]) i++;
        if (i < a->n && i < b->n) c = (uint8_t)a->b[i] < (uint8_t)b->b[i] ? -1 : 1;
        else c = a->n < b->n ? -1 : a->n > b->n;
        bool ok;
        item_const(x, &t_boolean, dobrar_ord(op, c, 0, &ok));
        return;
    }
    /* Comparación: las dos strings en la pila (en el orden que sea; si y se
     * apiló primero, la comparación se cambia) y __str_comparar. */
    bool trocado = false;
    if (x->t->k == T_CHAR && x->modo == M_PILHA) {
        /* char (valor en la pila) con string: y primero, después x se convierte en string */
        empilhar_endereco(y);
        item_t t;
        nova_temp(&t, &t_string);
        e2(0xFF, 0x74); e2(0x24, 0x04);       /* push dword [esp+4]: o char */
        empilhar_temp(&t);
        chamar_rotina("__str_de_char");
        empilhar_temp(&t);
        chamar_rotina("__str_comparar");       /* (y, x) */
        esp_mais(4);
        e3(0x83, 0xF8, 0x00);
        resultado_bool(x, cc_comparacao(trocar_comparacao(op), false));
        return;
    }
    para_string(x);
    para_string(y);
    trocado = empilhar_par(x, y);
    chamar_rotina("__str_comparar");   /* (a, b) -> -1, 0, 1 */
    e3(0x83, 0xF8, 0x00);              /* cmp eax, 0 */
    resultado_bool(x, cc_comparacao(trocado ? trocar_comparacao(op) : op, false));
}

static void op_conj(int op, item_t *x, item_t *y)
{
    if (op == '+' || op == '-' || op == '*') {
        if (x->modo == M_CONST && y->modo == M_CONST) {
            uint8_t *c = memoria(32);
            for (int i = 0; i < 32; i++)
                c[i] = op == '+' ? x->conj[i] | y->conj[i] : op == '-' ? x->conj[i] & ~y->conj[i] : x->conj[i] & y->conj[i];
            x->conj = c;
            if (x->t == &t_conj_vazio) x->t = y->t;
            return;
        }
        tipo_t *t = x->t == &t_conj_vazio ? y->t : x->t;
        empilhar_endereco(y);
        if (!(x->modo == M_VAR && x->temp)) copiar_para_temp(x);
        empilhar_temp(x);
        push_imm(op == '+' ? 0 : op == '-' ? 1 : 2);
        chamar_rotina("__conj_operar");   /* (origem, destino, op): destino := destino op origem */
        x->t = t;
        return;
    }
    if (op != '=' && op != F_DIF && op != F_MENIG && op != F_MAIIG) {
        erro("operador no válido con conjuntos");
        x->t = &t_erro;
        return;
    }
    if (x->modo == M_CONST && y->modo == M_CONST) {
        bool sub = true, sup = true, igual = true;
        for (int i = 0; i < 32; i++) {
            if (x->conj[i] != y->conj[i]) igual = false;
            if (x->conj[i] & ~y->conj[i]) sub = false;
            if (y->conj[i] & ~x->conj[i]) sup = false;
        }
        item_const(x, &t_boolean, op == '=' ? igual : op == F_DIF ? !igual : op == F_MENIG ? sub : sup);
        return;
    }
    bool trocado = empilhar_par(x, y);
    if (trocado) op = trocar_comparacao(op);
    push_imm(op == '=' ? 0 : op == F_DIF ? 1 : op == F_MENIG ? 2 : 3);
    chamar_rotina("__conj_comparar");   /* (a, b, op) -> boolean */
    x->modo = M_REG;
    x->t = &t_boolean;
}

/* x in y */
static void op_in(item_t *x, item_t *y)
{
    if (!eh_ordinal(x->t) || y->t->k != T_SET) {
        if (x->t->k != T_ERRO && y->t->k != T_ERRO) erro("'in' necesita un ordinal y un conjunto");
        x->t = &t_erro;
        return;
    }
    if (y->t->base && !mesmo_ordinal(x->t, y->t->base)) erro("'in' con tipos distintos (%s en set de %s)", nome_tipo(x->t), nome_tipo(y->t->base));
    if (x->modo == M_CONST && y->modo == M_CONST) {
        int v = x->val >= 0 && x->val <= 255 && (y->conj[x->val >> 3] >> (x->val & 7) & 1);
        item_const(x, &t_boolean, v);
        return;
    }
    /* x es constante, variable simple o está en la pila (preparar_esquerdo) */
    if (y->modo == M_CONST) const_na_memoria(y);
    if (y->modo != M_REG) endereco_em_eax(y);
    carregar_em_ecx(x);
    e2(0x31, 0xD2);                           /* xor edx, edx */
    e2(0x81, 0xF9); e4(255);                  /* cmp ecx, 255 */
    e2(0x77, 6);                              /* ja fin */
    e3(0x0F, 0xA3, 0x08);                     /* bt [eax], ecx */
    e3(0x0F, 0x92, 0xC2);                     /* setc dl */
    e2(0x89, 0xD0);                           /* fin: mov eax, edx */
    x->modo = M_REG;
    x->t = &t_boolean;
}

static bool eh_numero(tipo_t *t) { return t->k == T_INT || t->k == T_REAL; }
static bool eh_textual(tipo_t *t) { return t->k == T_STRING || t->k == T_CHAR; }

static const char *nome_op(int op)
{
    switch (op) {
    case P_DIV: return "div"; case P_MOD: return "mod"; case P_AND: return "and"; case P_OR: return "or";
    case P_XOR: return "xor"; case P_SHL: return "shl"; case P_SHR: return "shr"; case P_IN: return "in";
    case F_DIF: return "<>"; case F_MENIG: return "<="; case F_MAIIG: return ">=";
    }
    static char b[2];
    b[0] = (char)op;
    return b;
}

static void operar(int op, item_t *x, item_t *y)
{
    tipo_t *a = x->t, *b = y->t;
    if (a->k == T_ERRO || b->k == T_ERRO) { item_const(x, &t_erro, 0); return; }
    if (op == P_IN) { op_in(x, y); return; }
    bool comp = eh_comparacao(op);
    if (a->k == T_STRING || b->k == T_STRING || (op == '+' && a->k == T_CHAR && b->k == T_CHAR)) {
        if (eh_textual(a) && eh_textual(b) && (op == '+' || comp)) { op_string(op, x, y); return; }
    } else if (a->k == T_SET || b->k == T_SET) {
        if (a->k == T_SET && b->k == T_SET && (a->base == NULL || b->base == NULL || mesmo_ordinal(a->base, b->base))) {
            op_conj(op, x, y);
            return;
        }
    } else if (a->k == T_REAL || b->k == T_REAL || op == '/') {
        if (eh_numero(a) && eh_numero(b) && (op == '+' || op == '-' || op == '*' || op == '/' || comp)) {
            op_real(op, x, y);
            return;
        }
    } else if (eh_ordinal(a) && eh_ordinal(b)) {
        bool ok = mesmo_ordinal(a, b);
        if (!comp) {
            if (op == P_AND || op == P_OR || op == P_XOR) ok = ok && (a->k == T_INT || a->k == T_BOOL);
            else ok = ok && a->k == T_INT;
        }
        if (ok) { op_ordinal(op, x, y); return; }
    } else if ((a->k == T_PTR || a->k == T_NIL) && (b->k == T_PTR || b->k == T_NIL) && (op == '=' || op == F_DIF)) {
        op_ordinal(op, x, y);
        return;
    }
    erro("operador '%s' no válido entre %s y %s", nome_op(op), nome_tipo(a), nome_tipo(b));
    item_const(x, &t_erro, 0);
}

/* Antes de leer el operando derecho: el izquierdo no puede quedar en EAX. La
 * concatenación y las cuentas de conjuntos trabajan en una temporal. */
typedef struct { int antes, depois; item_t salvo; } esquerdo_t;

static void preparar_esquerdo(item_t *x, int op, esquerdo_t *e)
{
    e->depois = -1;
    if (op == '+' && eh_textual(x->t)) { if (x->modo != M_CONST) copiar_para_temp(x); return; }
    if ((op == '+' || op == '-' || op == '*') && x->t->k == T_SET) { if (x->modo != M_CONST) copiar_para_temp(x); return; }
    if (x->modo == M_REG || (x->modo == M_VAR && (x->base == B_EAX || x->base == B_ECX || x->base == B_PILHA))) {
        e->salvo = *x;
        e->antes = ncod;
        if (eh_grande(x->t)) empilhar_endereco(x);
        else empilhar(x);
        e->depois = ncod;
    }
}

/* Después de leer el derecho: si es simple (constante o variable) y
 * ningún código vino después del "push", el izquierdo vuelve a EAX. */
static void depois_do_direito(item_t *x, item_t *y, esquerdo_t *e)
{
    if (e->depois == ncod && eh_ordinal(x->t) && eh_ordinal(y->t) && eh_simples(y)) {
        ncod = e->antes;
        *x = e->salvo;
        carregar(x);
    }
}

/* "x and y" / "x or y" con booleanos: en cortocircuito, como en Turbo
 * Pascal ($B-): el derecho sólo se calcula si hace falta. */
static void logico(item_t *x, int op, void (*prox)(item_t *))
{
    item_t y;
    if (x->modo == M_CONST) {
        if ((op == P_AND && !x->val) || (op == P_OR && x->val)) {
            marca_t m = marcar();
            prox(&y);
            voltar(m);
        } else {
            prox(&y);
            *x = y;
        }
        if (y.t->k != T_BOOL && y.t->k != T_ERRO) erro("'%s' entre boolean y %s", nome_op(op), nome_tipo(y.t));
        return;
    }
    int cc = op == P_AND ? 0x84 : 0x85;
    int l = -1;
    carregar(x);
    if (op == P_AND) l = saltar_se_falso(-1);
    else { test_eax(); l = saltar(cc, -1); }
    prox(&y);
    if (y.t->k != T_BOOL && y.t->k != T_ERRO) erro("'%s' entre boolean y %s", nome_op(op), nome_tipo(y.t));
    carregar(&y);
    if (op == P_AND) l = saltar_se_falso(l);
    else { test_eax(); l = saltar(cc, l); }
    logico_gravar(l, op == P_AND);     /* si una condición viene justo después, el 0/1 ni se monta */
    mov_eax(op == P_AND ? 1 : 0);
    e2(0xEB, 5);                       /* jmp fin */
    remendar_aqui(l);
    mov_eax(op == P_AND ? 0 : 1);
    invalidar_ultimas();
    logico_fechar();
    x->modo = M_REG;
    x->t = &t_boolean;
}

static void termo(item_t *x)
{
    fator(x);
    for (;;) {
        int op = tt();
        if (op != '*' && op != '/' && op != P_DIV && op != P_MOD && op != P_AND && op != P_SHL && op != P_SHR) return;
        avancar();
        if (op == P_AND && x->t->k == T_BOOL) { logico(x, op, fator); continue; }
        esquerdo_t e;
        preparar_esquerdo(x, op, &e);
        item_t y;
        fator(&y);
        depois_do_direito(x, &y, &e);
        operar(op, x, &y);
    }
}

static void simples(item_t *x)
{
    int sinal = 0;
    if (tt() == '-' || tt() == '+') { sinal = tt(); avancar(); }
    termo(x);
    if (sinal == '-') negar_num(x);
    else if (sinal == '+' && !eh_numero(x->t) && x->t->k != T_ERRO) erro("'+' necesita un número");
    for (;;) {
        int op = tt();
        if (op != '+' && op != '-' && op != P_OR && op != P_XOR) return;
        avancar();
        if (op == P_OR && x->t->k == T_BOOL) { logico(x, op, termo); continue; }
        esquerdo_t e;
        preparar_esquerdo(x, op, &e);
        item_t y;
        termo(&y);
        depois_do_direito(x, &y, &e);
        operar(op, x, &y);
    }
}

static void expressao(item_t *x)
{
    simples(x);
    int op = tt();
    if (!eh_comparacao(op) && op != P_IN) return;
    avancar();
    esquerdo_t e;
    preparar_esquerdo(x, op, &e);
    item_t y;
    simples(&y);
    depois_do_direito(x, &y, &e);
    operar(op, x, &y);
}

/* Expresión constante (declaraciones, rangos, etiquetas del case). */
static void constante(item_t *x)
{
    marca_t m = marcar();
    expressao(x);
    voltar(m);
    if (x->modo != M_CONST && x->t->k != T_ERRO) { erro("se esperaba una constante"); item_const(x, &t_erro, 0); }
}

static int32_t constante_ordinal(tipo_t **t)
{
    item_t x;
    constante(&x);
    if (!eh_ordinal(x.t) && x.t->k != T_ERRO) erro("se esperaba una constante ordinal");
    if (t) *t = x.t->k == T_ERRO ? &t_integer : x.t;
    return x.val;
}

/* ------------------------------------------------------------ llamadas */

static bool tipos_identicos(tipo_t *a, tipo_t *b)
{
    if (a == b || a->k == T_ERRO || b->k == T_ERRO) return true;
    if (a->k != b->k) return false;
    switch (a->k) {
    case T_INT: case T_CHAR: case T_BOOL: return a->lo == b->lo && a->hi == b->hi && a->tam == b->tam;
    case T_REAL: return true;
    case T_STRING: return a->hi == b->hi;
    case T_PTR: return a->base == b->base || !a->base || !b->base;
    case T_ARRAY: return compativel(a, b);
    case T_FILE: return (a->base == NULL) == (b->base == NULL) && (!a->base || tipos_identicos(a->base, b->base));
    default: return false;
    }
}

static bool eh_variavel(item_t *x) { return x->modo == M_VAR && !x->temp; }

static void argumento(proc_t *p, int i)
{
    param_t *pa = &p->params[i];
    item_t y;
    bool aberto = pa->t->k == T_ARRAY && !pa->t->indice;
    ficha_t *ini = tk();
    expressao(&y);
    posicionar_erro(ini);
    if (y.t->k == T_ERRO) { empilhar(&y); if (aberto) push_imm(0); return; }
    if (aberto) {
        /* array abierto: dirección y "high" (n - 1) */
        if (y.t->k != T_ARRAY || !compativel(pa->t->base, y.t->base) || !compativel(y.t->base, pa->t->base)) {
            erro("argumento %d de '%s': se esperaba un array de %s", i + 1, p->nome->s, nome_tipo(pa->t->base));
            empilhar(&y); push_imm(0);
            return;
        }
        if (pa->modo == 1 && !eh_variavel(&y)) erro("argumento %d de '%s': se esperaba una variable", i + 1, p->nome->s);
        if (y.t->indice) {
            empilhar_endereco(&y);
            push_imm(y.t->indice->hi - y.t->indice->lo);
        } else {
            /* un array abierto pasado adelante: el mismo high */
            simbolo_t *s = aberto_atual;
            empilhar_endereco(&y);
            if (s) { carregar_alto_ecx(s); e1(0x51); } else push_imm(0);
        }
        return;
    }
    if (pa->modo == 1) {
        if (!eh_variavel(&y)) erro("argumento %d de '%s': se esperaba una variable (parámetro var)", i + 1, p->nome->s);
        else if (!tipos_identicos(pa->t, y.t))
            erro("argumento %d de '%s': el parámetro var es de tipo %s y la variable de tipo %s", i + 1, p->nome->s, nome_tipo(pa->t), nome_tipo(y.t));
        empilhar_endereco(&y);
        return;
    }
    char onde[160];
    formatar_texto(onde, sizeof onde, "el argumento %d de '%s'", i + 1, p->nome->s);
    converter_para(&y, pa->t, onde);
    posicionar_erro(tk());
    if (eh_grande(pa->t)) empilhar_endereco(&y);
    else empilhar(&y);
}

static void chamada(proc_t *p, item_t *x)
{
    item_t res;
    memset(&res, 0, sizeof res);
    if (p->ret_ref) {
        nova_temp(&res, p->ret);
        empilhar_temp(&res);
    }
    int n = 0;
    if (aceitar('(')) {
        if (!aceitar(')')) {
            for (;;) {
                if (n < p->np) argumento(p, n);
                else { item_t lixo; expressao(&lixo); if (n == p->np) erro("demasiados argumentos para '%s' (espera %d)", p->nome->s, p->np); }
                n++;
                if (aceitar(')')) break;
                esperar_ficha(',');
            }
        }
    }
    if (n < p->np) erro("faltan argumentos para '%s' (espera %d y recibe %d)", p->nome->s, p->np, n);
    if (p->nivel >= 2) empilhar_elo(p->nivel - 1);
    chamar_proc(p);
    invalidar_ultimas();
    if (p->ret_ref) { *x = res; return; }
    memset(x, 0, sizeof *x);
    x->modo = M_REG;
    x->t = p->ret ? p->ret : &t_erro;
}

/* ------------------------------------------------------------ funciones integradas */

static void um_argumento(item_t *x)
{
    esperar_ficha('(');
    expressao(x);
    esperar_ficha(')');
}

/* Real en XMM0/ST(0) -> una de las secuencias de la x87 de gerador_x86.h. */
static void aplicar_x87(const char *nome, bool exp)
{
    int k = 0;
    while (k < N_EMBUTIDAS && compara_texto(embutidas[k].nome, nome)) k++;
    if (usar_sse) { esp_menos(8); sse(0xF2, 0x11, 0x04); e1(0x24); f3(0xDD, 0x04, 0x24); }   /* xmm0 -> [esp] -> ST(0) */
    if (exp) { f2(0xD9, 0xEA); f2(0xDE, 0xC9); }   /* fldl2e; fmulp: x * log2(e) */
    for (int i = 0; i < embutidas[k].n; i++) e1(embutidas[k].cod[i]);
    if (exp) e2(0xDD, 0xD9);                       /* fstp st1 */
    if (usar_sse) { f3(0xDD, 0x1C, 0x24); sse(0xF2, 0x10, 0x04); e1(0x24); esp_mais(8); }
}

static void exigir_real(item_t *x, const char *f)
{
    if (x->t->k == T_INT) para_real(x);
    else if (x->t->k != T_REAL) { if (x->t->k != T_ERRO) erro("%s necesita un número", f); item_const(x, &t_real, 0); }
}

/* high/low/sizeof: el argumento es un tipo o una expresión (sin generar código). */
static tipo_t *tipo_do_argumento(simbolo_t **aberto)
{
    *aberto = NULL;
    esperar_ficha('(');
    tipo_t *t;
    if (tt() == F_NOME && procurar(tk()->nome) && procurar(tk()->nome)->classe == S_TIPO && tt_a(1) == ')') {
        t = procurar(tk()->nome)->t;
        avancar();
    } else if (tt() == P_STRING && tt_a(1) == ')') {
        t = &t_string;
        avancar();
    } else {
        if (tt() == F_NOME && tt_a(1) == ')') {
            simbolo_t *s = procurar(tk()->nome);
            if (s && s->classe == S_VAR && s->t->k == T_ARRAY && !s->t->indice) *aberto = s;
        }
        item_t x;
        marca_t m = marcar();
        expressao(&x);
        voltar(m);
        t = x.t;
    }
    esperar_ficha(')');
    return t;
}

static const char *const nomes_funcoes[] = {
    "abs", "sqr", "sqrt", "sin", "cos", "arctan", "ln", "exp", "trunc", "round", "odd", "ord", "chr",
    "succ", "pred", "length", "high", "low", "sizeof", "concat", "eof", "eoln", "random", "pi",
    "assigned", "__syscall", "__fonte8x16", "frac", "int", "ioresult", "__rint", "__troca", "__cas", "__chamar1", NULL,
};
enum { E_ABS, E_SQR, E_SQRT, E_SIN, E_COS, E_ARCTAN, E_LN, E_EXP, E_TRUNC, E_ROUND, E_ODD, E_ORD, E_CHR,
       E_SUCC, E_PRED, E_LENGTH, E_HIGH, E_LOW, E_SIZEOF, E_CONCAT, E_EOF, E_EOLN, E_RANDOM, E_PI,
       E_ASSIGNED, E_SYSCALL, E_FONTE, E_FRAC, E_INT, E_IORESULT, E_RINT, E_TROCA, E_CAS, E_CHAMAR1 };

static void empilhar_arquivo(item_t *f);

static bool funcao_embutida(nome_t *n, item_t *x)
{
    int k = 0;
    while (nomes_funcoes[k] && compara_texto(nomes_funcoes[k], n->s)) k++;
    if (!nomes_funcoes[k]) return false;
    avancar();
    item_t a;
    simbolo_t *aberto;
    switch (k) {
    case E_ABS: case E_SQR:
        um_argumento(x);
        if (x->t->k == T_INT) {
            if (x->modo == M_CONST) { x->val = k == E_ABS ? (x->val < 0 ? -x->val : x->val) : x->val * x->val; x->t = &t_integer; return true; }
            carregar(x);
            if (k == E_ABS) { e1(0x99); e2(0x31, 0xD0); e2(0x29, 0xD0); }   /* cdq; xor eax, edx; sub eax, edx */
            else e3(0x0F, 0xAF, 0xC0);                                      /* imul eax, eax */
            x->t = &t_integer;
            return true;
        }
        exigir_real(x, n->s);
        if (x->modo == M_CONST) { x->real = k == E_ABS ? (x->real < 0 ? -x->real : x->real) : x->real * x->real; return true; }
        carregar(x);
        if (k == E_SQR) { if (usar_sse) sse(0xF2, 0x59, XMM(0, 0)); else f2(0xD8, 0xC8); }   /* mulsd / fmul st0, st0 */
        else if (usar_sse) {
            sse(0xF2, 0x10, 0x3D); e4(endereco_bits(0xFFFFFFFFu, 0x7FFFFFFFu, false));   /* movsd xmm7, [máscara] */
            sse(0x66, 0x54, XMM(0, 7));                                                  /* andpd xmm0, xmm7 */
        } else f2(0xD9, 0xE1);                                                           /* fabs */
        return true;
    case E_SQRT: case E_SIN: case E_COS: case E_ARCTAN: case E_LN: case E_EXP:
        um_argumento(x);
        exigir_real(x, n->s);
        if (x->modo == M_CONST && k == E_SQRT && x->real >= 0) {
            /* raíz de constante: Newton en long double basta para el double */
            long double r = x->real, g = r > 1 ? r : 1;
            if (r > 0) for (int i = 0; i < 200; i++) g = (g + r / g) / 2;
            x->real = r > 0 ? (double)g : 0;
            return true;
        }
        carregar(x);
        if (k == E_SQRT) { if (usar_sse) sse(0xF2, 0x51, XMM(0, 0)); else f2(0xD9, 0xFA); }
        else aplicar_x87(k == E_SIN ? "__builtin_sin" : k == E_COS ? "__builtin_cos" : k == E_ARCTAN ? "__builtin_atan" :
                         k == E_LN ? "__builtin_log" : "__builtin_exp2", k == E_EXP);
        return true;
    case E_TRUNC: case E_ROUND: case E_FRAC: case E_INT:
        um_argumento(x);
        if (x->t->k == T_INT && (k == E_TRUNC || k == E_ROUND)) return true;
        exigir_real(x, n->s);
        if (x->modo == M_CONST && (k == E_TRUNC || k == E_ROUND)) {
            double v = x->real;
            if (v >= 2147483648.0 || v <= -2147483649.0) erro("%s: valor fuera del rango de integer", n->s);
            double r = k == E_TRUNC ? v : (v >= 0 ? v + 0.5 : v - 0.5);
            item_const(x, &t_integer, (int32_t)r);
            return true;
        }
        if (k == E_TRUNC) {
            carregar(x);
            if (usar_sse) sse(0xF2, 0x2C, 0xC0);   /* cvttsd2si eax, xmm0 */
            else chamar_para_inteiro();
            x->t = &t_integer;
            x->modo = M_REG;
            return true;
        }
        empilhar(x);
        chamar_rotina(k == E_ROUND ? "__arredondar" : k == E_FRAC ? "__parte_frac" : "__parte_int");
        x->modo = M_REG;
        x->t = k == E_ROUND ? &t_integer : &t_real;
        return true;
    case E_ODD:
        um_argumento(x);
        if (x->t->k != T_INT) { if (x->t->k != T_ERRO) erro("odd necesita un entero"); }
        if (x->modo == M_CONST) { item_const(x, &t_boolean, x->val & 1); return true; }
        carregar(x);
        e3(0x83, 0xE0, 0x01);                   /* and eax, 1 */
        x->t = &t_boolean;
        return true;
    case E_ORD:
        um_argumento(x);
        if (!eh_ordinal(x->t) && x->t->k != T_PTR) { if (x->t->k != T_ERRO) erro("ord necesita un ordinal"); }
        if (x->modo != M_CONST) carregar(x);
        x->t = &t_integer;
        return true;
    case E_CHR:
        um_argumento(x);
        if (x->t->k != T_INT && x->t->k != T_ERRO) erro("chr necesita un entero");
        if (x->modo == M_CONST) { x->val &= 255; x->t = &t_char; return true; }
        carregar(x);
        e3(0x0F, 0xB6, 0xC0);                   /* movzx eax, al */
        x->t = &t_char;
        return true;
    case E_SUCC: case E_PRED:
        um_argumento(x);
        if (!eh_ordinal(x->t)) { if (x->t->k != T_ERRO) erro("%s necesita un ordinal", n->s); return true; }
        if (x->modo == M_CONST) { x->val += k == E_SUCC ? 1 : -1; return true; }
        carregar(x);
        e1(k == E_SUCC ? 0x40 : 0x48);          /* inc/dec eax */
        if (x->t->k != T_INT) conferir_faixa(x->t->lo, x->t->hi);
        else x->t = &t_integer;
        return true;
    case E_LENGTH:
        um_argumento(x);
        if (x->t->k == T_CHAR) { item_const(x, &t_integer, 1); return true; }
        if (x->t->k == T_ARRAY && x->t->indice) { item_const(x, &t_integer, x->t->indice->hi - x->t->indice->lo + 1); return true; }
        if (x->t->k != T_STRING) { if (x->t->k != T_ERRO) erro("length necesita un string"); item_const(x, &t_integer, 0); return true; }
        if (x->modo == M_CONST) { item_const(x, &t_integer, literais[x->literal].n); return true; }
        if (x->modo == M_REG) item_var(x, x->t, B_EAX, 0);
        x->t = &t_byte;
        carregar(x);
        x->t = &t_integer;
        return true;
    case E_HIGH: case E_LOW: case E_SIZEOF: {
        tipo_t *t = tipo_do_argumento(&aberto);
        if (k == E_SIZEOF) { item_const(x, &t_integer, t->tam); if (aberto) erro("sizeof de un array abierto"); return true; }
        if (aberto) {
            if (k == E_LOW) { item_const(x, &t_integer, 0); return true; }
            carregar_alto_ecx(aberto);
            e2(0x89, 0xC8);                     /* mov eax, ecx */
            memset(x, 0, sizeof *x);
            x->modo = M_REG;
            x->t = &t_integer;
            return true;
        }
        if (t->k == T_ARRAY && t->indice) { item_const(x, t->indice, k == E_HIGH ? t->indice->hi : t->indice->lo); return true; }
        if (t->k == T_STRING) { item_const(x, &t_integer, k == E_HIGH ? t->hi : 0); return true; }
        if (eh_ordinal(t)) { item_const(x, t, k == E_HIGH ? t->hi : t->lo); return true; }
        if (t->k != T_ERRO) erro("%s necesita un array, un string o un ordinal", n->s);
        item_const(x, &t_integer, 0);
        return true;
    }
    case E_CONCAT:
        esperar_ficha('(');
        expressao(x);
        if (!eh_textual(x->t) && x->t->k != T_ERRO) erro("concat necesita strings");
        while (aceitar(',')) {
            esquerdo_t e;
            preparar_esquerdo(x, '+', &e);
            item_t y;
            expressao(&y);
            operar('+', x, &y);
        }
        esperar_ficha(')');
        para_string(x);
        return true;
    case E_EOF: case E_EOLN: {
        item_t f;
        if (aceitar('(')) {
            expressao(&f);
            if (f.t->k != T_FILE) { if (f.t->k != T_ERRO) erro("%s necesita un archivo", n->s); }
            esperar_ficha(')');
        } else {
            simbolo_t *s = nome_c("input")->sim;
            var_de_simbolo(s, &f);
        }
        empilhar_arquivo(&f);
        chamar_rotina(k == E_EOF ? "__arq_eof" : "__arq_eoln");
        memset(x, 0, sizeof *x);
        x->modo = M_REG;
        x->t = &t_boolean;
        return true;
    }
    case E_RANDOM:
        memset(x, 0, sizeof *x);
        x->modo = M_REG;
        if (aceitar('(')) {
            expressao(&a);
            esperar_ficha(')');
            if (a.t->k != T_INT && a.t->k != T_ERRO) erro("random necesita un entero");
            empilhar(&a);
            chamar_rotina("__aleatorio_n");
            x->t = &t_integer;
        } else {
            chamar_rotina("__aleatorio_real");
            x->t = &t_real;
        }
        return true;
    case E_PI:
        item_const(x, &t_real, 0);
        x->real = 3.14159265358979323846;
        return true;
    case E_ASSIGNED:
        um_argumento(x);
        if (x->t->k != T_PTR && x->t->k != T_ERRO) erro("assigned necesita un puntero");
        carregar(x);
        test_eax();
        resultado_bool(x, 0x95);
        return true;
    case E_SYSCALL:
        /* __syscall(n, a, b, c): int 0x80 directo (sólo la biblioteca lo usa) */
        esperar_ficha('(');
        for (int i = 0; i < 4; i++) {
            expressao(&a);
            empilhar(&a);
            if (i < 3) esperar_ficha(',');
        }
        esperar_ficha(')');
        e1(0x5A); e1(0x59); e1(0x5B); e1(0x58);   /* pop edx; pop ecx; pop ebx; pop eax */
        e2(0xCD, 0x80);                           /* int 0x80 */
        memset(x, 0, sizeof *x);
        x->modo = M_REG;
        x->t = &t_integer;
        return true;
    case E_TROCA: case E_CAS: case E_CHAMAR1: {
        /* D171, para los hilos (solo la biblioteca los usa):
         *   __troca(p, v): *p := v, devuelve el valor anterior (xchg);
         *   __cas(p, esperado, nuevo): si *p = esperado, *p := nuevo; devuelve
         *     el valor anterior (lock cmpxchg);
         *   __chamar1(rutina, arg): llama a la rutina con un argumento. */
        int na = k == E_CAS ? 3 : 2;
        esperar_ficha('(');
        for (int i = 0; i < na; i++) {
            expressao(&a);
            empilhar(&a);
            if (i < na - 1) esperar_ficha(',');
        }
        esperar_ficha(')');
        if (k == E_TROCA) {
            e1(0x58); e1(0x59);                    /* pop eax (v); pop ecx (p) */
            e2(0x87, 0x01);                        /* xchg [ecx], eax */
        } else if (k == E_CAS) {
            e1(0x5A); e1(0x58); e1(0x59);          /* pop edx (nuevo); pop eax (esperado); pop ecx (p) */
            e2(0xF0, 0x0F); e2(0xB1, 0x11);        /* lock cmpxchg [ecx], edx */
        } else {
            e1(0x58); e1(0x5A); e1(0x50);          /* pop eax (arg); pop edx (rutina); push eax */
            e2(0xFF, 0xD2);                        /* call edx (la rutina saca el argumento) */
            invalidar_ultimas();
        }
        memset(x, 0, sizeof *x);
        x->modo = M_REG;
        x->t = &t_integer;
        return true;
    }
    case E_FONTE:
        /* La letra 8x16 de la libsagui, en los datos del programa (sólo si se usa texto()). */
        if (off_fonte < 0 || descartando) {
            int off = aloca_dados(4096, 4);
            memcpy(dados + off, fonte_sagui, 4096);
            if (!descartando) off_fonte = off;
            item_const(x, &t_integer, (int32_t)(DADOS_BASE + off));
            return true;
        }
        item_const(x, &t_integer, (int32_t)(DADOS_BASE + off_fonte));
        return true;
    case E_IORESULT:
        item_const(x, &t_integer, 0);
        return true;
    case E_RINT:
        um_argumento(x);
        exigir_real(x, n->s);
        carregar(x);
        aplicar_x87("__builtin_rint", false);
        return true;
    }
    return false;
}

/* ------------------------------------------------------------ archivos y E/S */

/* El archivo de write/read: las rutinas de E/S reciben la dirección del registro
 * por último. En una variable simple la dirección es constante ("push
 * dirección" en cada uso); si no, se calcula una vez y se guarda en una
 * temporal. */
static void preparar_arquivo(item_t *f)
{
    if (f->modo == M_VAR && (f->base == B_ABS || f->base == B_EBP)) return;
    endereco_em_eax(f);
    int t = novo_local(4);
    e2(0x89, 0x85); e4((uint32_t)t);    /* mov [ebp+t], eax */
    item_var(f, f->t, B_EBP, t);
    f->temp = true;                      /* marca: la dirección está guardada en [ebp+t] */
}

static void empilhar_arquivo_de_novo(item_t *f)
{
    if (f->temp) { e2(0xFF, 0xB5); e4((uint32_t)f->desloc); return; }   /* push dword [ebp+t] */
    item_t c = *f;
    empilhar_end_simples(&c);
}

static void empilhar_arquivo(item_t *f)
{
    preparar_arquivo(f);
    empilhar_arquivo_de_novo(f);
}

/* Primer argumento de write/read: si es un archivo, queda en f y devuelve
 * true; si no, f = output/input y el ítem ya leído vuelve al llamador. */
static item_t primeiro_item;
static bool tem_primeiro;

static bool argumento_arquivo(item_t *f)
{
    tem_primeiro = false;
    expressao(&primeiro_item);
    if (primeiro_item.t->k == T_FILE) { *f = primeiro_item; return true; }
    tem_primeiro = true;
    return false;
}

static void arquivo_padrao(item_t *f, const char *nome)
{
    var_de_simbolo(nome_c(nome)->sim, f);
}

static bool eh_texto(tipo_t *t) { return t->k == T_FILE && !t->base; }

static void escrever_um(item_t *x, item_t *f)
{
    item_t w, d;
    bool tem_w = false, tem_d = false;
    if (eh_texto(f->t) && x->t->k != T_ERRO) {
        if (!(eh_ordinal(x->t) && x->t->k != T_ENUM) && x->t->k != T_REAL && x->t->k != T_STRING) {
            erro("no se puede escribir un valor de tipo %s", nome_tipo(x->t));
            x->t = &t_erro;
        }
    }
    if (!eh_texto(f->t)) {
        /* archivo con tipo: graba el valor como bytes */
        if (!compativel(f->t->base, x->t)) erro("tipos incompatibles: el archivo es de %s", nome_tipo(f->t->base));
        converter_para(x, f->t->base, "write");
        if (!eh_variavel(x)) {
            item_t t;
            item_var(&t, f->t->base, B_EBP, novo_local(f->t->base->tam));
            if (eh_grande(t.t)) {
                empilhar_endereco(x);
                empilhar_temp(&t);
                push_imm(t.t->tam);
                chamar_rotina("__mem_mover");
            } else if (t.t->k == T_REAL) { carregar(x); guardar_real_mem(&t); }
            else { carregar(x); guardar_ord_mem(&t, 0); }
            *x = t;
        }
        empilhar_endereco(x);
        push_imm(f->t->base->tam);
        empilhar_arquivo_de_novo(f);
        chamar_rotina("__arq_escrever_bloco");   /* (dirección, tamaño, f) */
        return;
    }
    if (x->t->k == T_REAL) empilhar(x);
    else if (x->t->k == T_STRING) empilhar_endereco(x);
    else empilhar(x);
    if (aceitar(':')) {
        expressao(&w);
        if (w.t->k != T_INT && w.t->k != T_ERRO) erro("el ancho debe ser un entero");
        tem_w = true;
        if (aceitar(':')) {
            expressao(&d);
            if (d.t->k != T_INT && d.t->k != T_ERRO) erro("los decimales deben ser un entero");
            if (x->t->k != T_REAL && x->t->k != T_ERRO) erro("':decimales' solo con valores reales");
            tem_d = true;
        }
    }
    if (tem_w) empilhar(&w);
    else push_imm(x->t->k == T_REAL ? -1 : 0);
    if (x->t->k == T_REAL) {
        if (tem_d) empilhar(&d);
        else push_imm(-1);
    }
    empilhar_arquivo_de_novo(f);
    switch (x->t->k) {
    case T_REAL: chamar_rotina("__esc_real"); break;     /* (v, ancho, decimales, f) */
    case T_STRING: chamar_rotina("__esc_str"); break;    /* (dirección, ancho, f) */
    case T_CHAR: chamar_rotina("__esc_char"); break;
    case T_BOOL: chamar_rotina("__esc_bool"); break;
    case T_ERRO: break;
    default: chamar_rotina("__esc_int"); break;
    }
}

static void instr_escrever(bool linha)
{
    item_t f;
    bool algum = false;
    /* D171: con hilos, cada write/writeln es atomico (cerrojo de la salida;
     * sin hilos las dos rutinas no hacen nada). */
    if (!compilando_biblioteca) chamar_rotina("__io_trava");
    if (aceitar('(')) {
        if (!aceitar(')')) {
            bool arq = argumento_arquivo(&f);
            if (!arq) arquivo_padrao(&f, "output");
            if (arq) preparar_arquivo(&f);
            if (tem_primeiro) { escrever_um(&primeiro_item, &f); algum = true; }
            while (arq ? aceitar(',') : (algum ? aceitar(',') : false)) {
                item_t x;
                expressao(&x);
                escrever_um(&x, &f);
                algum = true;
            }
            esperar_ficha(')');
        }
    } else {
        arquivo_padrao(&f, "output");
    }
    if (linha) {
        if (!eh_texto(f.t)) erro("writeln solo con archivos de texto");
        empilhar_arquivo_de_novo(&f);
        chamar_rotina("__esc_nl");
    }
    if (!compilando_biblioteca) chamar_rotina("__io_largar");
}

/* Lee para la variable x. */
static void ler_um(item_t *x, item_t *f)
{
    if (!eh_variavel(x)) { if (x->t->k != T_ERRO) erro("read necesita una variable"); return; }
    if (!eh_texto(f->t)) {
        if (!tipos_identicos(f->t->base, x->t)) erro("tipos incompatibles: el archivo es de %s", nome_tipo(f->t->base));
        empilhar_endereco(x);
        push_imm(f->t->base->tam);
        empilhar_arquivo_de_novo(f);
        chamar_rotina("__arq_ler_bloco");
        return;
    }
    tipo_t *t = x->t;
    if (t->k == T_STRING) {
        empilhar_endereco(x);
        push_imm(t->hi);
        empilhar_arquivo_de_novo(f);
        chamar_rotina("__ler_str");   /* (destino, capacidad, f) */
        return;
    }
    if (t->k != T_INT && t->k != T_CHAR && t->k != T_REAL) {
        erro("no se puede leer un valor de tipo %s", nome_tipo(t));
        return;
    }
    if (x->base == B_EAX || x->base == B_ECX) { endereco_em_eax(x); push_eax(); x->base = B_PILHA; x->desloc = 0; }
    empilhar_arquivo_de_novo(f);
    chamar_rotina(t->k == T_INT ? "__ler_int" : t->k == T_CHAR ? "__ler_char" : "__ler_real");
    if (t->k == T_REAL) { base_utilizavel(x); guardar_real_mem(x); return; }
    if (t->lo > INT_MIN32 || t->hi < 2147483647) conferir_faixa(t->lo, t->hi);
    base_utilizavel(x);
    guardar_ord_mem(x, 0);
}

static void instr_ler(bool linha)
{
    item_t f;
    if (aceitar('(')) {
        if (!aceitar(')')) {
            bool arq = argumento_arquivo(&f);
            if (!arq) arquivo_padrao(&f, "input");
            else preparar_arquivo(&f);
            if (tem_primeiro) ler_um(&primeiro_item, &f);
            while (aceitar(',')) {
                item_t x;
                expressao(&x);
                ler_um(&x, &f);
            }
            esperar_ficha(')');
        }
    } else {
        arquivo_padrao(&f, "input");
    }
    if (linha) {
        empilhar_arquivo_de_novo(&f);
        chamar_rotina("__ler_nl");
    }
}

/* Designador que tiene que ser variable (argumento de inc, new, str...). */
static void variavel(item_t *x, const char *quem)
{
    expressao(x);
    if (!eh_variavel(x) && x->t->k != T_ERRO) { erro("%s necesita una variable", quem); x->t = &t_erro; }
}

/* La dirección de x ya apilada o guardada en ECX, sin depender de EAX. */
static void guardar_endereco_var(item_t *x)
{
    if (x->modo == M_VAR && (x->base == B_EAX || x->base == B_ECX)) {
        endereco_em_eax(x);
        push_eax();
        x->base = B_PILHA;
        x->desloc = 0;
    }
}

/* ------------------------------------------------------------ instrucciones */

typedef struct { int quebra, continua, alvo_continua; } laco_t;
static laco_t lacos[64];
static int nlacos;
static int saidas_principal = -1;

static void instrucao(void);

static void sequencia(int fim)
{
    for (;;) {
        instrucao();
        if (tt() == fim) return;
        if (tt() == P_ELSE && fichas[pf - 1].tipo == ';') falhar("no se puede poner ';' antes de 'else'");
        if (tt() == F_FIM) falhar("falta '%s'", fim == P_END ? "end" : "until");
        esperar_ficha(';');
    }
}

static void condicao(item_t *c, const char *onde)
{
    expressao(c);
    if (c->t->k != T_BOOL && c->t->k != T_ERRO) erro("la condición de '%s' debe ser boolean (es %s)", onde, nome_tipo(c->t));
}

static laco_t *abrir_laco(int alvo)
{
    if (nlacos == 64) falhar("bucles anidados demasiado");
    laco_t *l = &lacos[nlacos++];
    l->quebra = l->continua = -1;
    l->alvo_continua = alvo;
    return l;
}

static void atribuir(item_t *d)
{
    tipo_t *t = d->t;
    if (!eh_variavel(d) && t->k != T_ERRO) erro("no se puede asignar a esto");
    if (t->k == T_FILE) erro("no se puede asignar un archivo");
    guardar_endereco_var(d);
    item_t v;
    ficha_t *ini = tk();
    expressao(&v);
    posicionar_erro(ini);
    converter_para(&v, t, "la asignación");
    posicionar_erro(tk());
    if (t->k == T_ERRO || v.t->k == T_ERRO) return;
    if (t->k == T_STRING) {
        if (d->base == B_PILHA) {
            empilhar_endereco(&v);
            e1(0x58); e3(0x87, 0x04, 0x24); push_eax();   /* intercambio: [origen][destino] */
        } else {
            empilhar_endereco(&v);
            empilhar_end_simples(d);
        }
        push_imm(t->hi);
        chamar_rotina("__str_atrib");   /* (origen, destino, capacidad) */
        return;
    }
    if (eh_grande(t)) {
        if (v.modo == M_CONST) const_na_memoria(&v);
        endereco_em_eax(&v);
        e2(0x89, 0xC6);                 /* mov esi, eax */
        if (d->base == B_PILHA) e1(0x5F);   /* pop edi */
        else { e1(0x8D); modrm(7, d, 0); }  /* lea edi, [destino] */
        copiar_bloco(t->tam);
        return;
    }
    if (t->k == T_REAL) {
        carregar(&v);
        base_utilizavel(d);
        guardar_real_mem(d);
        return;
    }
    if (v.modo == M_CONST && d->base != B_PILHA) {
        int tam = t->k == T_PTR ? 4 : t->tam;
        if (tam == 1) { e1(0xC6); modrm(0, d, 0); e1(v.val & 0xFF); }
        else if (tam == 2) { e2(0x66, 0xC7); modrm(0, d, 0); e2(v.val & 0xFF, (v.val >> 8) & 0xFF); }
        else { e1(0xC7); modrm(0, d, 0); e4((uint32_t)v.val); }
        return;
    }
    carregar(&v);
    base_utilizavel(d);
    guardar_ord_mem(d, 0);
}

/* inc(x [, n]) / dec(x [, n]) */
static void instr_inc(bool mais)
{
    esperar_ficha('(');
    item_t x, n;
    variavel(&x, mais ? "inc" : "dec");
    bool ptr = x.t->k == T_PTR && x.t->base;
    if (!eh_ordinal(x.t) && !ptr && x.t->k != T_ERRO) erro("%s necesita una variable ordinal", mais ? "inc" : "dec");
    int passo = ptr ? x.t->base->tam : 1;
    item_const(&n, &t_integer, 1);
    guardar_endereco_var(&x);
    if (aceitar(',')) { expressao(&n); if (n.t->k != T_INT && n.t->k != T_ERRO) erro("el incremento debe ser un entero"); }
    esperar_ficha(')');
    if (x.t->k == T_ERRO) return;
    int tam = ptr ? 4 : x.t->tam;
    if (n.modo == M_CONST && (!faixa_ativa() || (x.t->lo == INT_MIN32 && x.t->hi == 2147483647))) {
        int32_t v = n.val * passo;
        base_utilizavel(&x);
        if (!mais) v = -v;
        if (tam == 2) e1(0x66);
        if (cabe8(v)) { e1(tam == 1 ? 0x80 : 0x83); modrm(0, &x, 0); e1(v & 0xFF); }   /* add [m], v */
        else { e1(tam == 1 ? 0x80 : 0x81); modrm(0, &x, 0); if (tam == 1) e1(v & 0xFF); else if (tam == 2) e2(v & 0xFF, (v >> 8) & 0xFF); else e4((uint32_t)v); }
        return;
    }
    carregar(&n);
    if (passo != 1) { e2(0x69, 0xC0); e4((uint32_t)passo); }
    if (!mais) e2(0xF7, 0xD8);              /* neg eax */
    base_utilizavel(&x);
    if (faixa_ativa() && !ptr && !(x.t->lo == INT_MIN32 && x.t->hi == 2147483647)) {
        /* con {$R+}: suma en EAX, comprueba el rango y guarda */
        item_t c = x;
        if (c.base == B_ECX) e1(0x51);      /* push ecx */
        e2(0x89, 0xC2);                     /* mov edx, eax */
        carregar_ord_mem(&c);
        e2(0x01, 0xD0);                     /* add eax, edx */
        conferir_faixa(x.t->lo, x.t->hi);
        if (x.base == B_ECX) e1(0x59);      /* pop ecx */
        guardar_ord_mem(&x, 0);
        return;
    }
    if (tam == 2) e1(0x66);
    e1(tam == 1 ? 0x00 : 0x01);             /* add [m], al/ax/eax */
    modrm(0, &x, 0);
}

static void instr_new(bool novo)
{
    esperar_ficha('(');
    item_t p;
    variavel(&p, novo ? "new" : "dispose");
    if (p.t->k != T_PTR || !p.t->base) { if (p.t->k != T_ERRO) erro("%s necesita un puntero con tipo", novo ? "new" : "dispose"); }
    esperar_ficha(')');
    if (p.t->k != T_PTR || !p.t->base) return;
    if (!novo) {
        empilhar(&p);
        chamar_rotina("__mem_liberar");
        return;
    }
    guardar_endereco_var(&p);
    push_imm(p.t->base->tam);
    chamar_rotina("__mem_alocar");
    base_utilizavel(&p);
    guardar_ord_mem(&p, 0);
}

static void instr_getmem(bool pegar)
{
    esperar_ficha('(');
    item_t p, n;
    variavel(&p, pegar ? "getmem" : "freemem");
    if (p.t->k != T_PTR && p.t->k != T_ERRO) erro("%s necesita un puntero", pegar ? "getmem" : "freemem");
    if (!pegar) {
        empilhar(&p);
        if (aceitar(',')) { expressao(&n); }
        esperar_ficha(')');
        chamar_rotina("__mem_liberar");
        return;
    }
    guardar_endereco_var(&p);
    esperar_ficha(',');
    expressao(&n);
    esperar_ficha(')');
    if (n.t->k != T_INT && n.t->k != T_ERRO) erro("getmem: el tamaño debe ser un entero");
    empilhar(&n);
    chamar_rotina("__mem_alocar");
    base_utilizavel(&p);
    guardar_ord_mem(&p, 0);
}

/* str(x[:w[:d]], s) */
static void instr_str(void)
{
    esperar_ficha('(');
    item_t x, w, d, s;
    expressao(&x);
    bool real = x.t->k == T_REAL;
    if (!real && x.t->k != T_INT && x.t->k != T_ERRO) erro("str necesita un número");
    empilhar(&x);
    if (aceitar(':')) {
        expressao(&w);
        empilhar(&w);
        if (aceitar(':')) { expressao(&d); if (!real) erro("':decimales' solo con valores reales"); empilhar(&d); }
        else if (real) push_imm(-1);
    } else {
        push_imm(real ? -1 : 0);
        if (real) push_imm(-1);
    }
    esperar_ficha(',');
    variavel(&s, "str");
    if (s.t->k != T_STRING && s.t->k != T_ERRO) erro("str: el destino debe ser un string");
    esperar_ficha(')');
    empilhar_endereco(&s);
    push_imm(s.t->k == T_STRING ? s.t->hi : 255);
    chamar_rotina(real ? "__str_real" : "__str_int");   /* (v, ancho, [decimales], destino, capacidad) */
}

/* val(s, v, codigo) */
static void instr_val(void)
{
    esperar_ficha('(');
    item_t s, v, c;
    expressao(&s);
    para_string(&s);
    if (s.t->k != T_STRING && s.t->k != T_ERRO) erro("val necesita un string");
    empilhar_endereco(&s);
    esperar_ficha(',');
    variavel(&v, "val");
    guardar_endereco_var(&v);
    esperar_ficha(',');
    variavel(&c, "val");
    if (c.t->k != T_INT && c.t->k != T_ERRO) erro("val: el código debe ser una variable entera");
    esperar_ficha(')');
    if (c.modo == M_VAR && (c.base == B_EAX || c.base == B_ECX)) {
        /* la dirección del código se calcula ahora; v ya está guardado */
    }
    empilhar_endereco(&c);
    bool real = v.t->k == T_REAL;
    if (!real && v.t->k != T_INT && v.t->k != T_ERRO) erro("val: la variable debe ser numérica");
    chamar_rotina(real ? "__val_real" : "__val_int");   /* (s, &codigo) -> valor */
    if (v.t->k == T_ERRO) return;
    base_utilizavel(&v);
    if (real) guardar_real_mem(&v);
    else guardar_ord_mem(&v, 0);
}

/* Procedimientos integrados. */
static const char *const nomes_procs[] = {
    "write", "writeln", "read", "readln", "inc", "dec", "new", "dispose", "getmem", "freemem",
    "halt", "exit", "break", "continue", "str", "val", "delete", "insert", "assign", "reset",
    "rewrite", "append", "close", "fillchar", "move", "include", "exclude", NULL,
};
enum { I_WRITE, I_WRITELN, I_READ, I_READLN, I_INC, I_DEC, I_NEW, I_DISPOSE, I_GETMEM, I_FREEMEM,
       I_HALT, I_EXIT, I_BREAK, I_CONTINUE, I_STR, I_VAL, I_DELETE, I_INSERT, I_ASSIGN, I_RESET,
       I_REWRITE, I_APPEND, I_CLOSE, I_FILLCHAR, I_MOVE, I_INCLUDE, I_EXCLUDE };

static void atribuir_resultado(proc_t *p, item_t *v);

static void arquivo_arg(item_t *f, const char *quem)
{
    variavel(f, quem);
    if (f->t->k != T_FILE && f->t->k != T_ERRO) erro("%s necesita una variable de archivo", quem);
}

static bool procedimento_embutido(nome_t *n)
{
    int k = 0;
    while (nomes_procs[k] && compara_texto(nomes_procs[k], n->s)) k++;
    if (!nomes_procs[k]) return false;
    avancar();
    item_t a, b, c;
    switch (k) {
    case I_WRITE: case I_WRITELN: instr_escrever(k == I_WRITELN); break;
    case I_READ: case I_READLN: instr_ler(k == I_READLN); break;
    case I_INC: case I_DEC: instr_inc(k == I_INC); break;
    case I_NEW: case I_DISPOSE: instr_new(k == I_NEW); break;
    case I_GETMEM: case I_FREEMEM: instr_getmem(k == I_GETMEM); break;
    case I_HALT:
        if (aceitar('(')) { expressao(&a); esperar_ficha(')'); if (a.t->k != T_INT && a.t->k != T_ERRO) erro("halt necesita un entero"); empilhar(&a); }
        else push_imm(0);
        chamar_rotina("__sys_halt");
        break;
    case I_EXIT:
        if (aceitar('(')) {
            expressao(&a);
            esperar_ficha(')');
            if (!proc_atual || !proc_atual->ret) erro("exit con valor fuera de una función");
            else atribuir_resultado(proc_atual, &a);
        }
        if (proc_atual) proc_atual->saidas = saltar(0xE9, proc_atual->saidas);
        else saidas_principal = saltar(0xE9, saidas_principal);
        break;
    case I_BREAK:
        if (!nlacos) { erro("break fuera de un bucle"); break; }
        lacos[nlacos - 1].quebra = saltar(0xE9, lacos[nlacos - 1].quebra);
        break;
    case I_CONTINUE: {
        if (!nlacos) { erro("continue fuera de un bucle"); break; }
        laco_t *l = &lacos[nlacos - 1];
        if (l->alvo_continua >= 0) saltar_para(0xE9, l->alvo_continua);
        else l->continua = saltar(0xE9, l->continua);
        break;
    }
    case I_STR: instr_str(); break;
    case I_VAL: instr_val(); break;
    case I_DELETE:
        esperar_ficha('(');
        variavel(&a, "delete");
        if (a.t->k != T_STRING && a.t->k != T_ERRO) erro("delete necesita un string");
        empilhar_endereco(&a);
        esperar_ficha(','); expressao(&b); empilhar(&b);
        esperar_ficha(','); expressao(&c); empilhar(&c);
        esperar_ficha(')');
        chamar_rotina("__str_apagar");   /* (s, inicio, cuántos) */
        break;
    case I_INSERT:
        esperar_ficha('(');
        expressao(&a);
        para_string(&a);
        if (a.t->k != T_STRING && a.t->k != T_ERRO) erro("insert necesita un string");
        empilhar_endereco(&a);
        esperar_ficha(','); variavel(&b, "insert");
        if (b.t->k != T_STRING && b.t->k != T_ERRO) erro("insert necesita un string");
        empilhar_endereco(&b);
        push_imm(b.t->k == T_STRING ? b.t->hi : 255);
        esperar_ficha(','); expressao(&c); empilhar(&c);
        esperar_ficha(')');
        chamar_rotina("__str_inserir");   /* (origen, s, capacidad, posición) */
        break;
    case I_ASSIGN:
        esperar_ficha('(');
        arquivo_arg(&a, "assign");
        empilhar_endereco(&a);
        esperar_ficha(','); expressao(&b); para_string(&b);
        if (b.t->k != T_STRING && b.t->k != T_ERRO) erro("assign: el nombre debe ser un string");
        empilhar_endereco(&b);
        esperar_ficha(')');
        chamar_rotina("__arq_assign");   /* (f, nombre) */
        break;
    case I_RESET: case I_REWRITE: case I_APPEND: case I_CLOSE:
        esperar_ficha('(');
        arquivo_arg(&a, nomes_procs[k]);
        esperar_ficha(')');
        empilhar_endereco(&a);
        if (k == I_CLOSE) { chamar_rotina("__arq_close"); break; }
        if (k == I_APPEND && a.t->k == T_FILE && a.t->base) erro("append solo con archivos de texto");
        push_imm(a.t->k == T_FILE && a.t->base ? a.t->base->tam : 0);
        chamar_rotina(k == I_RESET ? "__arq_reset" : k == I_REWRITE ? "__arq_rewrite" : "__arq_append");   /* (f, tamaño del registro) */
        break;
    case I_FILLCHAR:
        esperar_ficha('(');
        variavel(&a, "fillchar");
        empilhar_endereco(&a);
        esperar_ficha(','); expressao(&b); if (b.t->k != T_INT && b.t->k != T_ERRO) erro("fillchar: la cantidad debe ser un entero");
        empilhar(&b);
        esperar_ficha(','); expressao(&c);
        if (!eh_ordinal(c.t) && c.t->k != T_ERRO) erro("fillchar: el valor debe ser ordinal");
        empilhar(&c);
        esperar_ficha(')');
        chamar_rotina("__mem_encher");   /* (p, n, valor) */
        break;
    case I_MOVE:
        esperar_ficha('(');
        variavel(&a, "move"); empilhar_endereco(&a);
        esperar_ficha(','); variavel(&b, "move"); empilhar_endereco(&b);
        esperar_ficha(','); expressao(&c); if (c.t->k != T_INT && c.t->k != T_ERRO) erro("move: la cantidad debe ser un entero");
        empilhar(&c);
        esperar_ficha(')');
        chamar_rotina("__mem_mover");    /* (origen, destino, n) */
        break;
    case I_INCLUDE: case I_EXCLUDE:
        esperar_ficha('(');
        variavel(&a, nomes_procs[k]);
        if (a.t->k != T_SET && a.t->k != T_ERRO) erro("%s necesita un conjunto", nomes_procs[k]);
        guardar_endereco_var(&a);
        esperar_ficha(','); expressao(&b);
        if (a.t->k == T_SET && a.t->base && !mesmo_ordinal(a.t->base, b.t) && b.t->k != T_ERRO) erro("%s: elemento de otro tipo", nomes_procs[k]);
        esperar_ficha(')');
        carregar(&b);
        base_utilizavel(&a);
        e1(0x3D); e4(255);                 /* cmp eax, 255 */
        e2(0x77, 0);                       /* ja fin */
        {
            int p = ncod;
            e2(0x0F, k == I_INCLUDE ? 0xAB : 0xB3);   /* bts/btr [m], eax */
            modrm(0, &a, 0);
            cod[p - 1] = (uint8_t)(ncod - p);
        }
        break;
    }
    return true;
}

/* El resultado de la función p (desde dentro de ella o de una anidada en ella). */
static simbolo_t *resultado_de(proc_t *p);

static void atribuir_resultado(proc_t *p, item_t *v)
{
    item_t d;
    var_de_simbolo(resultado_de(p), &d);
    guardar_endereco_var(&d);
    tipo_t *t = d.t;
    converter_para(v, t, "el resultado");
    if (v->t->k == T_ERRO) return;
    if (t->k == T_STRING) {
        if (d.base == B_PILHA) { empilhar_endereco(v); e1(0x58); e3(0x87, 0x04, 0x24); push_eax(); }
        else { empilhar_endereco(v); empilhar_end_simples(&d); }
        push_imm(t->hi);
        chamar_rotina("__str_atrib");
    } else if (eh_grande(t)) {
        if (v->modo == M_CONST) const_na_memoria(v);
        endereco_em_eax(v);
        e2(0x89, 0xC6);
        if (d.base == B_PILHA) e1(0x5F);
        else { e1(0x8D); modrm(7, &d, 0); }
        copiar_bloco(t->tam);
    } else if (t->k == T_REAL) {
        carregar(v);
        base_utilizavel(&d);
        guardar_real_mem(&d);
    } else {
        carregar(v);
        base_utilizavel(&d);
        guardar_ord_mem(&d, 0);
    }
}

static void instr_for(void)
{
    nome_t *n = esperar_nome();
    simbolo_t *s = procurar(n);
    item_t v, a, b;
    if (!s || s->classe != S_VAR || !eh_ordinal(s->t) || s->por_ref || (s->nivel != nivel_atual && s->nivel != 0)) {
        if (!s) erro("'%s' no está declarado", n->s);
        else erro("la variable de control del for debe ser una variable ordinal local");
        item_var(&v, &t_erro, B_ABS, 0);
    } else var_de_simbolo(s, &v);
    esperar_ficha(F_ATRIB);
    expressao(&a);
    converter_para(&a, v.t, "el for");
    bool desce = tt() == P_DOWNTO;
    if (!aceitar(P_TO) && !aceitar(P_DOWNTO)) falhar("se esperaba 'to' o 'downto' y se encontró %s", descrever_atual());
    if (v.t->k != T_ERRO) { carregar(&a); guardar_ord_mem(&v, 0); }
    expressao(&b);
    if (!compativel(v.t, b.t)) erro("el límite del for es de tipo %s y la variable de %s", nome_tipo(b.t), nome_tipo(v.t));
    esperar_ficha(P_DO);
    if (v.t->k == T_ERRO) { instrucao(); return; }
    item_t lim;
    if (b.modo == M_CONST) lim = b;
    else {
        carregar(&b);
        item_var(&lim, &t_integer, B_EBP, novo_local(4));
        guardar_ord_mem(&lim, 0);
    }
    /* comienzo: si ya pasó del límite, no entra */
    carregar_ord_mem(&v);
    if (lim.modo == M_CONST) { e1(0x3D); e4((uint32_t)lim.val); }   /* cmp eax, lim */
    else { e1(0x3B); modrm(0, &lim, 0); }                          /* cmp eax, [lim] */
    int fim = saltar(desce ? 0x8C : 0x8F, -1);                      /* jl / jg fin */
    int topo = ncod;
    marcar_rotulo();
    invalidar_ultimas();
    laco_t *l = abrir_laco(-1);
    instrucao();
    remendar_aqui(l->continua);
    carregar_ord_mem(&v);
    if (lim.modo == M_CONST) { e1(0x3D); e4((uint32_t)lim.val); }
    else { e1(0x3B); modrm(0, &lim, 0); }
    fim = saltar(0x84, fim);                                        /* je fin */
    e1(desce ? 0x48 : 0x40);                                        /* dec/inc eax */
    guardar_ord_mem(&v, 0);
    saltar_para(0xE9, topo);
    remendar_aqui(fim);
    remendar_aqui(lacos[nlacos - 1].quebra);
    nlacos--;
}

static void instr_case(void)
{
    item_t sel;
    expressao(&sel);
    if (!eh_ordinal(sel.t) && sel.t->k != T_ERRO) erro("el selector del case debe ser ordinal (es %s)", nome_tipo(sel.t));
    esperar_ficha(P_OF);
    carregar(&sel);
    int fim = -1;
    while (tt() != P_END && tt() != P_ELSE && !(tt() == F_NOME && !compara_texto(tk()->nome->s, "otherwise"))) {
        int corpo = -1;
        for (;;) {
            tipo_t *ta, *tb;
            int32_t a = constante_ordinal(&ta);
            if (sel.t->k != T_ERRO && !mesmo_ordinal(sel.t, ta)) erro("etiqueta de tipo %s en un case de %s", nome_tipo(ta), nome_tipo(sel.t));
            if (aceitar(F_PP)) {
                int32_t b = constante_ordinal(&tb);
                if (b < a) erro("rango vacío en el case (%d..%d)", a, b);
                e2(0x8D, 0x88); e4((uint32_t)-a);                 /* lea ecx, [eax-a] */
                e2(0x81, 0xF9); e4((uint32_t)b - (uint32_t)a);    /* cmp ecx, b-a */
                corpo = saltar(0x86, corpo);                      /* jbe corpo */
            } else {
                if (a == 0) e2(0x85, 0xC0);                       /* test eax, eax */
                else { e1(0x3D); e4((uint32_t)a); }               /* cmp eax, a */
                corpo = saltar(0x84, corpo);                      /* je corpo */
            }
            if (!aceitar(',')) break;
        }
        esperar_ficha(':');
        int prox = saltar(0xE9, -1);
        remendar_aqui(corpo);
        instrucao();
        fim = saltar(0xE9, fim);
        remendar_aqui(prox);
        if (!aceitar(';')) break;
    }
    if (aceitar(P_ELSE) || (tt() == F_NOME && !compara_texto(tk()->nome->s, "otherwise") && (avancar(), true))) {
        while (tt() != P_END) {
            instrucao();
            if (tt() == P_END) break;
            esperar_ficha(';');
        }
    }
    esperar_ficha(P_END);
    remendar_aqui(fim);
}

static void instr_with(void)
{
    int abertos = 0;
    for (;;) {
        item_t r;
        expressao(&r);
        if (r.t->k != T_RECORD) {
            if (r.t->k != T_ERRO) erro("with necesita un record");
        } else {
            if (r.modo != M_VAR) { erro("with necesita una variable"); }
            int base = B_PILHA;
            int32_t d;
            if (r.modo == M_VAR && (r.base == B_ABS || r.base == B_EBP)) { base = r.base; d = r.desloc; }
            else {
                endereco_em_eax(&r);
                d = novo_local(4);
                e2(0x89, 0x85); e4((uint32_t)d);   /* mov [ebp+d], eax */
            }
            abrir_escopo();
            abertos++;
            for (campo_t *c = r.t->campos; c; c = c->prox) {
                simbolo_t *s = declarar(c->nome, S_CAMPO, c->t);
                s->campo = c;
                s->with_base = base;
                s->with_desloc = d;
            }
        }
        if (!aceitar(',')) break;
    }
    esperar_ficha(P_DO);
    instrucao();
    while (abertos--) fechar_escopo();
}

static simbolo_t *rotulo(nome_t *n)
{
    simbolo_t *s = n->sim;
    if (!s || s->classe != S_ROTULO) { erro("la etiqueta '%s' no está declarada (falta 'label %s;')", n->s, n->s); return NULL; }
    if (s->nivel != nivel_atual) { erro("goto a una etiqueta de otro procedimiento"); return NULL; }
    return s;
}

static nome_t *nome_rotulo(void)
{
    if (tt() == F_INT) {
        char b[16];
        formatar_texto(b, sizeof b, "%d", tk()->val);
        avancar();
        return nome_c(b);
    }
    return esperar_nome();
}

static void instrucao(void)
{
    int salvo_local = local_desloc;
    /* etiqueta "10: x := 1" */
    while ((tt() == F_INT || tt() == F_NOME) && tt_a(1) == ':') {
        nome_t *n = nome_rotulo();
        avancar();
        simbolo_t *s = rotulo(n);
        if (s) {
            if (s->rotulo_def) erro("etiqueta '%s' repetida", n->s);
            s->rotulo_def = true;
            s->rotulo_pos = ncod;
            marcar_rotulo();
            remendar_aqui(s->rotulo_lista);
            s->rotulo_lista = -1;
            invalidar_ultimas();
        }
    }
    item_t c, x;
    switch (tt()) {
    case P_BEGIN:
        avancar();
        if (tt() != P_END) sequencia(P_END);
        esperar_ficha(P_END);
        break;
    case P_IF: {
        avancar();
        condicao(&c, "if");
        esperar_ficha(P_THEN);
        if (c.modo == M_CONST) {
            /* condición constante: sólo la rama que vale genera código */
            marca_t m;
            if (!c.val) m = marcar();
            if (tt() != P_ELSE) instrucao();
            if (!c.val) voltar(m);
            if (aceitar(P_ELSE)) {
                if (c.val) m = marcar();
                instrucao();
                if (c.val) voltar(m);
            }
            break;
        }
        carregar(&c);
        int senao = saltar_se_falso(-1);
        if (tt() != P_ELSE) instrucao();
        if (aceitar(P_ELSE)) {
            int fim = saltar(0xE9, -1);
            remendar_aqui(senao);
            instrucao();
            remendar_aqui(fim);
        } else {
            remendar_aqui(senao);
        }
        break;
    }
    case P_WHILE: {
        avancar();
        int topo = ncod;
        marcar_rotulo();
        invalidar_ultimas();
        condicao(&c, "while");
        esperar_ficha(P_DO);
        int fim = -1;
        if (c.modo != M_CONST) { carregar(&c); fim = saltar_se_falso(-1); }
        else if (!c.val) fim = saltar(0xE9, -1);
        abrir_laco(topo);
        instrucao();
        saltar_para(0xE9, topo);
        remendar_aqui(fim);
        remendar_aqui(lacos[nlacos - 1].quebra);
        nlacos--;
        break;
    }
    case P_REPEAT: {
        avancar();
        int topo = ncod;
        marcar_rotulo();
        invalidar_ultimas();
        abrir_laco(-1);
        if (tt() != P_UNTIL) sequencia(P_UNTIL);
        esperar_ficha(P_UNTIL);
        remendar_aqui(lacos[nlacos - 1].continua);
        condicao(&c, "until");
        if (c.modo == M_CONST) { if (!c.val) saltar_para(0xE9, topo); }
        else { carregar(&c); test_eax(); saltar_para(0x84, topo); }
        remendar_aqui(lacos[nlacos - 1].quebra);
        nlacos--;
        break;
    }
    case P_FOR: avancar(); instr_for(); break;
    case P_CASE: avancar(); instr_case(); break;
    case P_WITH: avancar(); instr_with(); break;
    case P_GOTO: {
        avancar();
        simbolo_t *s = rotulo(nome_rotulo());
        if (!s) break;
        if (s->rotulo_def) saltar_para(0xE9, s->rotulo_pos);
        else s->rotulo_lista = saltar(0xE9, s->rotulo_lista);
        break;
    }
    case F_NOME: {
        nome_t *n = tk()->nome;
        simbolo_t *s = n->sim;
        if (!s && procedimento_embutido(n)) break;
        if (!s && funcao_embutida(n, &x)) { if (x.modo == M_REG && x.t->k == T_REAL) descartar_real(); break; }
        if (!s) s = procurar(n);
        if (!s) {
            erro("'%s' no está declarado", n->s);
            while (tt() != ';' && tt() != P_END && tt() != F_FIM && tt() != P_ELSE && tt() != P_UNTIL) avancar();
            break;
        }
        if (s->classe == S_PROC && tt_a(1) == F_ATRIB) {
            avancar();
            avancar();
            if (!s->p->ret || !resultado_de(s->p)) { erro("no se puede asignar a '%s'", n->s); expressao(&x); break; }
            expressao(&x);
            atribuir_resultado(s->p, &x);
            break;
        }
        if (s->classe == S_PROC && resultado_de(s->p) && (tt_a(1) == '.' || tt_a(1) == '[')) {
            /* f.campo := ... / f[i] := ... dentro de la función f: el resultado */
            avancar();
            var_de_simbolo(resultado_de(s->p), &x);
            seletores(&x);
            esperar_ficha(F_ATRIB);
            atribuir(&x);
            break;
        }
        if (s->classe == S_PROC) {
            avancar();
            chamada(s->p, &x);
            if (x.t->k == T_REAL) descartar_real();
            break;
        }
        if (s->classe != S_VAR && s->classe != S_CAMPO) {
            erro(s->classe == S_CONST ? "no se puede asignar a la constante '%s'" : "uso inesperado de '%s'", n->s);
            while (tt() != ';' && tt() != P_END && tt() != F_FIM && tt() != P_ELSE && tt() != P_UNTIL) avancar();
            break;
        }
        avancar();
        var_de_simbolo(s, &x);
        if (s->classe == S_VAR && s->t->k == T_ARRAY && !s->t->indice) aberto_atual = s;
        seletores(&x);
        if (tt() == '=') falhar("se esperaba ':=' (en Pascal la asignación es ':=' y '=' compara)");
        if (tt() != F_ATRIB) {
            erro("se esperaba ':=' o una llamada y se encontró %s", descrever_atual());
            break;
        }
        avancar();
        atribuir(&x);
        break;
    }
    case ';': case P_END: case P_UNTIL: case P_ELSE:
        break;   /* instrucción vacía */
    default:
        falhar("se esperaba una instrucción y se encontró %s", descrever_atual());
    }
    local_desloc = salvo_local;
}

static simbolo_t *resultado_de(proc_t *p)
{
    return p->em_curso ? p->res : NULL;
}

/* ------------------------------------------------------------ tipos (declaraciones) */

/* ^T con T todavía no declarado: se resuelven al final de la sección type. */
static tipo_t *pendentes[256];
static int npendentes;

static tipo_t *tipo(void);

static tipo_t *tipo_ordinal(void)
{
    tipo_t *t = tipo();
    if (!eh_ordinal(t) && t->k != T_ERRO) { erro("se esperaba un tipo ordinal (el índice de un array, un subrango...)"); t = &t_erro; }
    return t;
}

static tipo_t *tipo_array(void)
{
    tipo_t *ix[16];
    int n = 0;
    esperar_ficha('[');
    do {
        if (n == 16) falhar("demasiadas dimensiones");
        ix[n++] = tipo_ordinal();
    } while (aceitar(','));
    esperar_ficha(']');
    esperar_ficha(P_OF);
    tipo_t *t = tipo();
    for (int i = n - 1; i >= 0; i--) {
        tipo_t *a = novo_tipo(T_ARRAY, 0);
        a->indice = ix[i];
        a->base = t;
        int64_t tam = ((int64_t)ix[i]->hi - ix[i]->lo + 1) * t->tam;
        if (ix[i]->k == T_ERRO) tam = 4;
        if (tam > 0x10000000) { erro("array demasiado grande"); tam = 4; }
        a->tam = (int)tam;
        t = a;
    }
    return t;
}

/* Campos de un record (hasta "end" o ")" de una variante); desloc = dónde
 * empiezan. Devuelve el fin. */
static int campos_record(tipo_t *r, campo_t ***fimlista, int desloc, int fecha)
{
    int maximo = desloc;
    while (tt() != fecha && tt() != P_CASE) {
        nome_t *ns[64];
        int n = 0;
        do {
            if (n == 64) falhar("demasiados campos en una línea");
            ns[n++] = esperar_nome();
        } while (aceitar(','));
        esperar_ficha(':');
        tipo_t *t = tipo();
        for (int i = 0; i < n; i++) {
            for (campo_t *c = r->campos; c; c = c->prox)
                if (c->nome == ns[i]) erro("campo '%s' repetido", ns[i]->s);
            campo_t *c = memoria(sizeof *c);
            c->nome = ns[i];
            c->t = t;
            c->desloc = desloc;
            desloc += t->tam;
            **fimlista = c;
            *fimlista = &c->prox;
        }
        if (desloc > maximo) maximo = desloc;
        if (!aceitar(';')) break;
    }
    if (tt() == P_CASE) {
        /* parte variante: case [tag:] tipo of k: (campos); ... — las
         * variantes empiezan todas en el mismo lugar (una sobre otra) */
        avancar();
        if (tt() == F_NOME && tt_a(1) == ':') {
            nome_t *tag = esperar_nome();
            avancar();
            tipo_t *tt_ = tipo_ordinal();
            campo_t *c = memoria(sizeof *c);
            c->nome = tag;
            c->t = tt_;
            c->desloc = desloc;
            desloc += tt_->tam;
            **fimlista = c;
            *fimlista = &c->prox;
        } else {
            tipo_ordinal();
        }
        esperar_ficha(P_OF);
        maximo = desloc;
        while (tt() != fecha) {
            do { item_t k; constante(&k); if (aceitar(F_PP)) constante(&k); } while (aceitar(','));
            esperar_ficha(':');
            esperar_ficha('(');
            int f = campos_record(r, fimlista, desloc, ')');
            esperar_ficha(')');
            if (f > maximo) maximo = f;
            if (!aceitar(';')) break;
        }
    }
    return maximo;
}

static tipo_t *tipo(void)
{
    aceitar(P_PACKED);
    switch (tt()) {
    case '^': {
        avancar();
        nome_t *n = esperar_nome();
        simbolo_t *s = procurar(n);
        tipo_t *t = ponteiro_para(NULL);
        if (s && s->classe == S_TIPO && s->escopo == escopo_id[nescopos - 1]) t->base = s->t;
        else {
            /* puede declararse más adelante en la misma sección (lista enlazada) */
            t->pendente = n;
            if (npendentes == 256) falhar("demasiados punteros adelantados");
            pendentes[npendentes++] = t;
        }
        return t;
    }
    case '(': {
        avancar();
        tipo_t *t = novo_tipo(T_ENUM, 1);
        int n = 0;
        do {
            nome_t *nm = esperar_nome();
            simbolo_t *s = declarar(nm, S_CONST, t);
            s->valor = n++;
        } while (aceitar(','));
        esperar_ficha(')');
        t->lo = 0;
        t->hi = n - 1;
        if (n > 256) t->tam = 2;
        return t;
    }
    case P_ARRAY:
        avancar();
        if (tt() == P_OF) falhar("'array of' solo vale en parámetros (array abierto)");
        return tipo_array();
    case P_RECORD: {
        avancar();
        tipo_t *t = novo_tipo(T_RECORD, 0);
        campo_t **fim = &t->campos;
        t->tam = campos_record(t, &fim, 0, P_END);
        esperar_ficha(P_END);
        return t;
    }
    case P_SET: {
        avancar();
        esperar_ficha(P_OF);
        tipo_t *e = tipo_ordinal();
        if (e->k != T_ERRO && (e->lo < 0 || e->hi > 255)) erro("un conjunto solo puede tener elementos de 0 a 255 (este va de %d a %d)", e->lo, e->hi);
        return tipo_conj(e);
    }
    case P_STRING:
        avancar();
        if (aceitar('[')) {
            int32_t n = constante_ordinal(NULL);
            esperar_ficha(']');
            if (n < 1 || n > 255) { erro("la longitud de un string debe ir de 1 a 255"); n = 255; }
            return tipo_string(n);
        }
        return &t_string;
    case P_FILE: {
        avancar();
        tipo_t *t = novo_tipo(T_FILE, t_text->tam);
        if (aceitar(P_OF)) {
            t->base = tipo();
            if (t->base->k == T_FILE) erro("archivo de archivos");
        } else {
            erro("el archivo sin tipo ('file' a secas) no está soportado: usa 'file of byte' o 'text'");
            t->base = &t_byte;
        }
        return t;
    }
    case F_NOME: {
        simbolo_t *s = procurar(tk()->nome);
        if (s && s->classe == S_TIPO) { avancar(); return s->t; }
        if (!s && tt_a(1) != F_PP) {
            erro("tipo '%s' no declarado", tk()->nome->s);
            avancar();
            return &t_erro;
        }
        break;
    }
    }
    /* subrango: constante .. constante */
    tipo_t *ta, *tb;
    int32_t lo = constante_ordinal(&ta);
    esperar_ficha(F_PP);
    int32_t hi = constante_ordinal(&tb);
    if (!mesmo_ordinal(ta, tb)) erro("los límites del subrango son de tipos distintos");
    if (lo > hi) { erro("subrango vacío (%d..%d)", lo, hi); hi = lo; }
    return subfaixa(ta, lo, hi);
}

static void resolver_pendentes(void)
{
    for (int i = 0; i < npendentes; i++) {
        tipo_t *t = pendentes[i];
        simbolo_t *s = procurar(t->pendente);
        if (!s || s->classe != S_TIPO) erro("tipo '%s' no declarado (en ^%s)", t->pendente->s, t->pendente->s);
        else t->base = s->t;
        t->pendente = NULL;
    }
    npendentes = 0;
}

/* ------------------------------------------------------------ declaraciones */

static void inicializar(tipo_t *t, int off);

static void secao_const(void)
{
    avancar();
    do {
        nome_t *n = esperar_nome();
        if (aceitar(':')) {
            /* constante con tipo (Turbo Pascal): una variable con valor inicial en los datos */
            tipo_t *t = tipo();
            esperar_ficha('=');
            simbolo_t *s = declarar(n, S_VAR, t);
            int off = aloca_dados(t->tam, 4);
            s->nivel = 0;
            s->valor = (int32_t)(DADOS_BASE + off);
            inicializar(t, off);
            esperar_ficha(';');
            continue;
        }
        esperar_ficha('=');
        item_t x;
        constante(&x);
        simbolo_t *s = declarar(n, S_CONST, x.t);
        s->valor = x.val;
        s->real = x.real;
        s->literal = x.literal;
        s->conj = x.conj;
        if (x.t->k == T_STRING && x.modo == M_CONST) { /* literal */ }
        esperar_ficha(';');
    } while (tt() == F_NOME);
}

/* Valor inicial (constante con tipo, var x: T = valor) en dados[off]. */
static void inicializar(tipo_t *t, int off)
{
    if (t->k == T_ARRAY) {
        if (t->base->k == T_CHAR && tt() == F_TEXTO) {
            literal_t *l = &literais[tk()->val];
            int n = t->indice->hi - t->indice->lo + 1;
            if (l->n != n) erro("el texto tiene %d caracteres y el array %d", l->n, n);
            memcpy(dados + off, l->b, l->n < n ? l->n : n);
            avancar();
            return;
        }
        esperar_ficha('(');
        int n = t->indice->hi - t->indice->lo + 1;
        for (int i = 0; i < n; i++) {
            inicializar(t->base, off + i * t->base->tam);
            if (i < n - 1 && !aceitar(',')) { erro("faltan valores: el array tiene %d elementos", n); break; }
        }
        if (tt() == ',') erro("sobran valores: el array tiene %d elementos", n);
        while (tt() != ')' && tt() != F_FIM) avancar();
        esperar_ficha(')');
        return;
    }
    if (t->k == T_RECORD) {
        esperar_ficha('(');
        while (tt() == F_NOME) {
            nome_t *n = esperar_nome();
            campo_t *c = t->campos;
            while (c && c->nome != n) c = c->prox;
            esperar_ficha(':');
            if (!c) { erro("el record no tiene el campo '%s'", n->s); item_t lixo; constante(&lixo); }
            else inicializar(c->t, off + c->desloc);
            if (!aceitar(';')) break;
        }
        esperar_ficha(')');
        return;
    }
    item_t x;
    constante(&x);
    exigir_compativel(t, x.t, "el valor inicial");
    if (x.t->k == T_ERRO) return;
    switch (t->k) {
    case T_REAL: { double v = x.t->k == T_INT ? (double)x.val : x.real; memcpy(dados + off, &v, 8); return; }
    case T_STRING: {
        if (x.t->k == T_CHAR) { dados[off] = 1; dados[off + 1] = (uint8_t)x.val; return; }
        literal_t *l = &literais[x.literal];
        int n = l->n > t->hi ? t->hi : l->n;
        dados[off] = (uint8_t)n;
        memcpy(dados + off + 1, l->b, n);
        return;
    }
    case T_SET: memcpy(dados + off, x.conj, 32); return;
    case T_PTR: return;   /* nil */
    default:
        if (eh_ordinal(t)) {
            if (x.val < t->lo || x.val > t->hi) erro("valor inicial fuera de rango (%d..%d)", t->lo, t->hi);
            memcpy(dados + off, &x.val, t->tam);
            return;
        }
        erro("no se puede dar valor inicial a un %s", nome_tipo(t));
    }
}

static void secao_type(void)
{
    avancar();
    do {
        nome_t *n = esperar_nome();
        esperar_ficha('=');
        tipo_t *t = tipo();
        if (!t->nome && t->k != T_ERRO && t != &t_string && t->k != T_INT && t->k != T_CHAR && t->k != T_BOOL && t->k != T_REAL) t->nome = n;
        declarar(n, S_TIPO, t);
        esperar_ficha(';');
    } while (tt() == F_NOME);
    resolver_pendentes();
}

static int novo_global(int tam)
{
    return (int)(BSS_BASE + (uint32_t)aloca_bss(tam, 4));
}

static void secao_var(void)
{
    avancar();
    do {
        nome_t *ns[64];
        int n = 0;
        do {
            if (n == 64) falhar("demasiadas variables en una línea");
            ns[n++] = esperar_nome();
        } while (aceitar(','));
        esperar_ficha(':');
        tipo_t *t = tipo();
        if (npendentes) resolver_pendentes();
        if (tt() == '=' && n == 1 && nivel_atual == 0) {
            /* var x: integer = 5; (FPC): en los datos */
            avancar();
            simbolo_t *s = declarar(ns[0], S_VAR, t);
            int off = aloca_dados(t->tam, 4);
            s->valor = (int32_t)(DADOS_BASE + off);
            inicializar(t, off);
        } else {
            for (int i = 0; i < n; i++) {
                simbolo_t *s = declarar(ns[i], S_VAR, t);
                s->nivel = nivel_atual;
                s->valor = nivel_atual == 0 ? novo_global(t->tam) : novo_local(t->tam);
            }
        }
        esperar_ficha(';');
    } while (tt() == F_NOME);
}

static void secao_label(void)
{
    avancar();
    do {
        nome_t *n = nome_rotulo();
        simbolo_t *s = declarar(n, S_ROTULO, &t_erro);
        s->nivel = nivel_atual;
        s->rotulo_lista = -1;
    } while (aceitar(','));
    esperar_ficha(';');
}

static void declaracao_rotina(bool funcao);

static void declaracoes(void)
{
    for (;;) {
        switch (tt()) {
        case P_CONST: secao_const(); break;
        case P_TYPE: secao_type(); break;
        case P_VAR: secao_var(); break;
        case P_LABEL: secao_label(); break;
        case P_PROCEDURE: declaracao_rotina(false); break;
        case P_FUNCTION: declaracao_rotina(true); break;
        default: return;
        }
    }
}

/* ------------------------------------------------------------ procedimentos */

static tipo_t *tipo_parametro(void)
{
    if (aceitar(P_ARRAY)) {
        esperar_ficha(P_OF);
        tipo_t *t = novo_tipo(T_ARRAY, 0);
        t->base = tipo();
        return t;   /* índice NULL: array abierto */
    }
    if (tt() == P_STRING && tt_a(1) != '[') { avancar(); return &t_string; }
    return tipo();
}

static int lista_parametros(param_t **ps)
{
    param_t buf[64];
    int n = 0;
    if (!aceitar('(')) { *ps = NULL; return 0; }
    if (!aceitar(')')) {
        do {
            int modo = aceitar(P_VAR) ? 1 : aceitar(P_CONST) ? 2 : 0;
            nome_t *ns[32];
            int k = 0;
            do {
                if (k == 32) falhar("demasiados parámetros");
                ns[k++] = esperar_nome();
            } while (aceitar(','));
            esperar_ficha(':');
            tipo_t *t = tipo_parametro();
            for (int i = 0; i < k; i++) {
                if (n == 64) falhar("demasiados parámetros");
                buf[n].nome = ns[i];
                buf[n].t = t;
                buf[n++].modo = modo;
            }
        } while (aceitar(';'));
        esperar_ficha(')');
    }
    *ps = memoria(sizeof(param_t) * (n ? n : 1));
    memcpy(*ps, buf, sizeof(param_t) * n);
    return n;
}

static void corpo_rotina(proc_t *p);

static void declaracao_rotina(bool funcao)
{
    avancar();
    nome_t *n = esperar_nome();
    param_t *ps;
    int np = lista_parametros(&ps);
    tipo_t *ret = NULL;
    if (funcao) {
        if (aceitar(':')) {
            ret = tipo_parametro();
            if (ret->k == T_FILE || (ret->k == T_ARRAY && !ret->indice)) erro("una función no puede devolver este tipo");
        } else if (!(n->sim && n->sim->classe == S_PROC && n->sim->p->adiantada)) {
            erro("falta el tipo del resultado de la función '%s'", n->s);
            ret = &t_erro;
        }
    }
    esperar_ficha(';');
    simbolo_t *a = n->sim;
    proc_t *p;
    if (a && a->classe == S_PROC && a->p->adiantada && !a->p->definida && a->escopo == escopo_id[nescopos - 1]) {
        p = a->p;
        if (np && np != p->np) erro("los parámetros de '%s' no coinciden con los de la declaración forward", n->s);
        p->adiantada = false;
    } else {
        p = novo_proc(n);
        p->np = np;
        p->params = ps;
        p->ret = ret;
        p->nivel = nivel_atual + 1;
        p->ret_ref = ret && eh_grande(ret);
        simbolo_t *s = declarar(n, S_PROC, NULL);
        s->p = p;
    }
    if (tt() == F_NOME && !compara_texto(tk()->nome->s, "forward")) {
        avancar();
        esperar_ficha(';');
        p->adiantada = true;
        return;
    }
    corpo_rotina(p);
    esperar_ficha(';');
}

/* push ebp; mov ebp, esp; sub esp, N; pone a cero N bytes (N parcheado al final). */
static int prologo(void)
{
    e1(0x55);
    e2(0x89, 0xE5);
    e2(0x81, 0xEC);
    int pos = ncod;
    e4(0);
    e2(0x89, 0xE7);          /* mov edi, esp */
    e2(0x31, 0xC0);          /* xor eax, eax */
    e1(0xB9); e4(0);         /* mov ecx, N/4 */
    e2(0xF3, 0xAB);          /* rep stosd */
    return pos;
}

static void remendar_prologo(int pos)
{
    int n = (local_max + 3) & ~3;
    por4(pos, (uint32_t)n);
    por4(pos + 4 + 2 + 2 + 1, (uint32_t)(n / 4));
}

static void conferir_rotulos(void)
{
    for (simbolo_t *s = escopos[nescopos - 1]; s; s = s->prox)
        if (s->classe == S_ROTULO && s->rotulo_lista != -1 && !s->rotulo_def) erro("la etiqueta '%s' se usa pero no está definida", s->nome->s);
}

static void corpo_rotina(proc_t *p)
{
    bool pular = passada == 2 && p->da_biblioteca && !alcancavel[p->id];
    marca_t m;
    if (pular) m = marcar();
    proc_t *ant_proc = proc_atual;
    int ant_nivel = nivel_atual, ant_desloc = local_desloc, ant_max = local_max, ant_lacos = nlacos;
    proc_atual = p;
    nivel_atual = p->nivel;
    local_desloc = local_max = 0;
    nlacos = 0;
    abrir_escopo();
    /* parámetros: el último apilado queda más cerca de EBP */
    int off = 8 + (p->nivel >= 2 ? 4 : 0);
    int desl[64];
    for (int i = p->np - 1; i >= 0; i--) {
        param_t *pa = &p->params[i];
        desl[i] = off;
        if (pa->t->k == T_ARRAY && !pa->t->indice) off += 8;
        else if (pa->modo == 0 && pa->t->k == T_REAL) off += 8;
        else off += 4;
    }
    if (p->ret_ref) { p->res_desloc = off; off += 4; }
    p->tam_args = off - 8;
    int copias[64], ncopias = 0;
    for (int i = 0; i < p->np; i++) {
        param_t *pa = &p->params[i];
        simbolo_t *s = declarar(pa->nome, S_VAR, pa->t);
        s->nivel = p->nivel;
        s->valor = desl[i];
        if (pa->t->k == T_ARRAY && !pa->t->indice) {
            s->por_ref = true;
            s->valor = desl[i] + 4;
            s->alto = desl[i];
        } else if (pa->modo == 1 || (pa->modo == 2 && eh_grande(pa->t))) {
            s->por_ref = true;
        } else if (eh_grande(pa->t)) {
            /* valor grande: el llamado copia a su marco */
            if (pa->t->k == T_FILE) erro("un archivo debe pasarse como parámetro var");
            s->valor = novo_local(pa->t->tam);
            s->alto = desl[i];        /* dónde está la dirección del original */
            copias[ncopias++] = i;
        }
    }
    simbolo_t *res = NULL;
    if (p->ret) {
        res = declarar(nome_c("result"), S_VAR, p->ret);
        res->nivel = p->nivel;
        if (p->ret_ref) { res->valor = p->res_desloc; res->por_ref = true; }
        else { p->res_desloc = novo_local(p->ret->tam < 4 ? 4 : p->ret->tam); res->valor = p->res_desloc; }
        p->res = res;
    }
    declaracoes();
    p->definida = true;
    p->pos = ncod;
    marcar_rotulo();
    remendar(p->chamadas, ncod);
    p->chamadas = -1;
    invalidar_ultimas();
    int pq = prologo();
    for (int k = 0; k < ncopias; k++) {
        param_t *pa = &p->params[copias[k]];
        simbolo_t *s = pa->nome->sim;
        if (pa->t->k == T_STRING) {
            e2(0xFF, 0xB5); e4((uint32_t)s->alto);   /* push dword [ebp+param] (origen) */
            item_t l;
            item_var(&l, pa->t, B_EBP, s->valor);
            empilhar_temp(&l);
            push_imm(pa->t->hi);
            chamar_rotina("__str_atrib");
        } else {
            e2(0x8B, 0xB5); e4((uint32_t)s->alto);   /* mov esi, [ebp+param] */
            e2(0x8D, 0xBD); e4((uint32_t)s->valor);  /* lea edi, [ebp+local] */
            copiar_bloco(pa->t->tam);
        }
    }
    p->em_curso = true;
    esperar_ficha(P_BEGIN);
    if (tt() != P_END) sequencia(P_END);
    esperar_ficha(P_END);
    p->em_curso = false;
    remendar_aqui(p->saidas);
    p->saidas = -1;
    if (p->ret && !p->ret_ref) {
        item_t r;
        item_var(&r, p->ret, B_EBP, p->res_desloc);
        if (p->ret->k == T_REAL) carregar_real_mem(&r);
        else carregar_ord_mem(&r);
    }
    e1(0xC9);                                        /* leave */
    if (p->tam_args) { e1(0xC2); e2(p->tam_args & 0xFF, p->tam_args >> 8); }   /* ret n */
    else e1(0xC3);
    remendar_prologo(pq);
    conferir_rotulos();
    fechar_escopo();
    proc_atual = ant_proc;
    nivel_atual = ant_nivel;
    local_desloc = ant_desloc;
    local_max = ant_max;
    nlacos = ant_lacos;
    if (pular) voltar(m);
}

/* ------------------------------------------------------------ programa */

static const char NOME_BIBLIOTECA[] = "<biblioteca>";
static int inicio_programa;   /* primera ficha del programa (después de la biblioteca) */
static int salto_principal;

static void declarar_tipo(const char *nome, tipo_t *t)
{
    declarar(nome_c(nome), S_TIPO, t);
}

static void declarar_const(const char *nome, tipo_t *t, int32_t v)
{
    declarar(nome_c(nome), S_CONST, t)->valor = v;
}

/* El "sistema": tipos y constantes predefinidos, después la biblioteca. */
static void sistema(void)
{
    declarar_tipo("integer", &t_integer);
    declarar_tipo("longint", &t_integer);
    declarar_tipo("byte", &t_byte);
    declarar_tipo("shortint", &t_shortint);
    declarar_tipo("word", &t_word);
    declarar_tipo("smallint", &t_smallint);
    declarar_tipo("char", &t_char);
    declarar_tipo("boolean", &t_boolean);
    declarar_tipo("real", &t_real);
    declarar_tipo("double", &t_real);
    declarar_tipo("extended", &t_real);
    declarar_tipo("pointer", &t_pointer);
    declarar_const("true", &t_boolean, 1);
    declarar_const("false", &t_boolean, 0);
    declarar_const("maxint", &t_integer, 2147483647);
    declarar_const("maxlongint", &t_integer, 2147483647);
    /* la biblioteca */
    pf = 0;
    posicionar_erro(&fichas[0]);
    compilando_biblioteca = true;
    declaracoes();
    if (tt() != F_FIM) falhar("error interno: la biblioteca no termina bien");
    /* text: un registro __texto de la biblioteca, que el programa ve como archivo */
    simbolo_t *r = nome_c("__texto")->sim;
    t_text = novo_tipo(T_FILE, r->t->tam);
    t_text->nome = nome_c("text");
    declarar_tipo("text", t_text);
    simbolo_t *e = declarar(nome_c("input"), S_VAR, t_text);
    e->valor = nome_c("__entrada")->sim->valor;
    e = declarar(nome_c("output"), S_VAR, t_text);
    e->valor = nome_c("__saida")->sim->valor;
    simbolo_t *a = nome_c("__argc")->sim;
    if (!a || a->valor != (int32_t)BSS_BASE) falhar("error interno: __argc debe ser la primera variable de la biblioteca");
    compilando_biblioteca = false;
}

static void programa(void)
{
    pf = inicio_programa;
    posicionar_erro(tk());
    usa_crt = usa_sagui = usa_hilos = false;
    if (aceitar(P_PROGRAM)) {
        esperar_nome();
        if (aceitar('(')) { while (tt() != ')' && tt() != F_FIM) avancar(); esperar_ficha(')'); }
        esperar_ficha(';');
    }
    if (aceitar(P_USES)) {
        do {
            nome_t *u = esperar_nome();
            if (!compara_texto(u->s, "crt")) usa_crt = true;
            else if (!compara_texto(u->s, "sagui")) usa_sagui = true;
            else if (!compara_texto(u->s, "hilos")) usa_hilos = true;
            else if (!compara_texto(u->s, "graph")) erro("la unidad 'graph' (BGI) no existe en SaguiOS: usa 'sagui' (mira 'ayuda pascal')");
            else if (compara_texto(u->s, "dos") && compara_texto(u->s, "sysutils") && compara_texto(u->s, "math"))
                erro("unidad '%s' desconocida (hay crt, sagui y hilos)", u->s);
            else erro("la unidad '%s' no está disponible en SaguiOS (hay crt, sagui y hilos)", u->s);
        } while (aceitar(','));
        esperar_ficha(';');
    }
    abrir_escopo();
    nivel_atual = 0;
    declaracoes();
    /* cuerpo principal */
    remendar_aqui(salto_principal);
    proc_atual = NULL;
    local_desloc = local_max = 0;
    int pq = prologo();
    chamar_rotina("__sys_iniciar");
    if (tt() != P_BEGIN) falhar("se esperaba 'begin' (el programa principal) y se encontró %s", descrever_atual());
    avancar();
    if (tt() != P_END) sequencia(P_END);
    esperar_ficha(P_END);
    if (tt() != '.') falhar("se esperaba '.' al final del programa y se encontró %s", descrever_atual());
    avancar();
    remendar_aqui(saidas_principal);
    saidas_principal = -1;
    push_imm(0);
    chamar_rotina("__sys_halt");
    remendar_prologo(pq);
    conferir_rotulos();
    fechar_escopo();
}

/* _inicio: guarda argc/argv en las dos primeras variables de la biblioteca
 * (__argc y __argv, al principio de la BSS) y salta al programa principal. */
static void emitir_inicio(void)
{
    reservar_iniciar_fpu();
    e3(0x8B, 0x04, 0x24);                       /* mov eax, [esp] */
    e1(0xA3); e4(BSS_BASE);                     /* mov [__argc], eax */
    /* argv: en SaguiOS, [esp+4] apunta al vector; en Linux, el vector empieza ahí */
    e1(modo_linux ? 0x8D : 0x8B); e3(0x44, 0x24, 0x04);   /* mov (lea) eax, [esp+4] */
    e1(0xA3); e4(BSS_BASE + 4);                 /* mov [__argv], eax */
    e2(0x31, 0xED);                             /* xor ebp, ebp */
    salto_principal = saltar(0xE9, -1);
}

static void compilar(void)
{
    ncod = ndados = nbss = nconstantes = 0;
    nlit_dados = 0;
    off_fonte = -1;
    nprocs = 0;
    usou_real = false;
    lista_para_inteiro = -1;
    prox_escopo_id = 0;
    nescopos = 0;
    nlacos = 0;
    saidas_principal = -1;
    for (int h = 0; h < HASH_NOMES; h++)
        for (nome_t *x = nomes[h]; x; x = x->prox) x->sim = NULL;
    abrir_escopo();
    emitir_inicio();
    sistema();
    programa();
    if (tt() != F_FIM) {
        posicionar_erro(tk());
        erro("texto después del 'end.' final");
    }
    emitir_rotinas_finais();
}

/* ¿Aparece la palabra "crt" (sin distinguir mayúsculas) en el texto? */
static bool menciona_crt(const char *t, int n)
{
    for (int i = 0; i + 3 <= n; i++) {
        if ((t[i] | 32) != 'c' || (t[i + 1] | 32) != 'r' || (t[i + 2] | 32) != 't') continue;
        char a = i ? t[i - 1] : ' ', d = i + 3 < n ? t[i + 3] : ' ';
        bool la = (a | 32) >= 'a' && (a | 32) <= 'z', ld = (d | 32) >= 'a' && (d | 32) <= 'z';
        if (!la && !ld && a != '_' && d != '_' && !(a >= '0' && a <= '9') && !(d >= '0' && d <= '9')) return true;
    }
    return false;
}

int principal(int argc, char **argv)
{
    /* D236: el kernel da la x87 a 53 bits; el Pascal calcula las constantes
     * reales (potencia10, la raíz por Newton) en long double con 64 bits, como antes. */
    x87_precisao(X87_64_BITS);
    const char *entrada = NULL, *saida = NULL;
    int fpu = 0;   /* 0 = según la CPU, 1 = -sse2, 2 = -x87 */
    for (int i = 1; i < argc; i++) {
        if (!compara_texto(argv[i], "-o") && i + 1 < argc) saida = argv[++i];
        else if (!compara_texto(argv[i], "-linux")) modo_linux = true;
        else if (!compara_texto(argv[i], "-x87")) fpu = 2;
        else if (!compara_texto(argv[i], "-sse2")) fpu = 1;
        else if (opcao_otimizacao(argv[i])) continue;
        else if (argv[i][0] == '-') { entrada = NULL; break; }
        else entrada = argv[i];
    }
    if (!entrada) {
        imprimir_em(ERROS, "uso: pascal programa.pas [-o nombre] [-x87 | -sse2] [-O0 | -O1] [-fno-mirilla|inmediatos|saltos|reduccion]\n");
        return 2;
    }
    /* D076: sin opción, el programa sale para la CPU donde se compila. */
    bool tem_sse2 = cpu_tem_sse2();
    usar_sse = fpu == 1 || (fpu == 0 && tem_sse2);
    if (fpu == 1 && !tem_sse2)
        imprimir_em(ERROS, "pascal: aviso: esta CPU no tiene SSE2; el programa compilado con -sse2 no funcionará en ella "
                           "(sin -sse2, pascal usa la FPU x87)\n");
    registrar("pascal: coma flotante con %s (%s)", usar_sse ? "SSE2" : "x87",
              fpu == 1 ? "-sse2" : fpu == 2 ? "-x87" : tem_sse2 ? "la CPU tiene SSE2" : "la CPU no tiene SSE2");
    char nome_saida[128];
    if (!saida) {   /* "carpeta/prog.pas" -> "prog", en la carpeta actual */
        const char *base = acha_ultimo_char(entrada, '/');
        base = base ? base + 1 : entrada;
        copia_texto(nome_saida, base, sizeof nome_saida);
        char *ponto = acha_ultimo_char(nome_saida, '.');
        if (ponto && ponto != nome_saida) *ponto = 0;
        else copia_texto(nome_saida + tam_texto(nome_saida), ".out", sizeof nome_saida - tam_texto(nome_saida));
        saida = nome_saida;
    }
    for (unsigned i = 0; i < sizeof reservadas / sizeof reservadas[0]; i++)
        nome_c(reservadas[i])->reservada = P_AND + (int)i;
    nome_c("SAGUIOS")->definido = true;
    if (modo_linux) nome_c("LINUX")->definido = true;

    int tam;
    char *texto = ler_arquivo(entrada, &tam);
    if (!texto) {
        imprimir_em(ERROS, "pascal: no se puede abrir %s\n", entrada);
        return 1;
    }
    /* La biblioteca solo lleva la pantalla virtual del crt (los desvíos en
     * __descarregar y en la lectura) si el programa nombra crt: el "uses crt"
     * aún no se ha leído. */
    if (menciona_crt(texto, tam)) nome_c("CRT")->definido = true;

    /* fichas: la biblioteca y después el programa */
    abrir_fonte(NOME_BIBLIOTECA, BIBLIOTECA_PASCAL, (int)tam_texto(BIBLIOTECA_PASCAL));
    faixa_lex = false;
    ler_todas_as_fichas();
    nfichas--;   /* el F_FIM de la biblioteca se convierte en el separador de abajo */
    fichas[nfichas].tipo = F_FIM;
    nfichas++;
    inicio_programa = nfichas;
    nfontes = 0;
    abrir_fonte(entrada, texto, tam);
    faixa_lex = true;
    ler_todas_as_fichas();
    if (nerros) return 1;

    /* 1ª pasada: descubre qué rutinas usa cada una. */
    passada = 1;
    compilar();
    if (nerros) return 1;
    alcancar(0);
    /* 2ª pasada: todo de nuevo, sin las rutinas de la biblioteca no usadas. */
    passada = 2;
    compilar();
    if (nerros) return 1;
    int r = escrever_elf(saida);
    if (r < 0) {
        imprimir_em(ERROS, "pascal: no se puede escribir %s: %s\n", saida, nome_erro(r));
        return 1;
    }
    return 0;
}
