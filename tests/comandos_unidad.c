// SPDX-License-Identifier: GPL-2.0-or-later
/* comandos_unidad.c - pruebas de unidad en el PC de la lógica de los comandos nuevos (D232, h232).
 *
 *   gcc -O2 -Wall -Wextra -DSAGUI_HOST -I userland/inc -idirafter kernel/inc -I userland/programas \
 *       -o comandos_unidad tests/comandos_unidad.c userland/libsagui/{regex,texto,formatar,utilitarios}.c
 *
 * Cubre sed, tr, expr, bc, cal, base64, od, md5 y sha1 con salidas conocidas (las mismas que da Linux).
 * Escribe "todo bien" y sale con 0 si pasa todo. */
#include <stdio.h>
#include <stdlib.h>
#include "sed_logica.h"
#include "tr_logica.h"
#include "expr_logica.h"
#include "bc_logica.h"
#include "cal_logica.h"
#include "base64_logica.h"
#include "od_logica.h"
#include "resumos_logica.h"
#include "file_logica.h"

/* lo que la libsagui pide al kernel y estas pruebas no usan */
int escrever(int fd, const void *b, unsigned n) { (void)fd; (void)b; return (int)n; }
int registrar_texto(const char *t) { (void)t; return 0; }
int ler(int fd, void *b, unsigned n) { (void)fd; (void)b; (void)n; return 0; }
int abrir(const char *c, int m) { (void)c; (void)m; return -1; }
int fechar(int fd) { (void)fd; return 0; }
int tipo_descritor(int fd) { (void)fd; return 0; }
int info_arquivo(const char *c, info_arquivo_t *i) { (void)c; (void)i; return -1; }
int listar(const char *c, int i, entrada_pasta_t *e) { (void)c; (void)i; (void)e; return 0; }
void *aloca(size_t n) { return malloc(n); }
void libera(void *p) { free(p); }
void *realoca(void *p, size_t n) { return realloc(p, n); }

static int fallos, total;
#define COMPROBAR(c, ...) do { total++; if (!(c)) { fallos++; printf("FALLO %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

/* ------------------------------------------------------------ sed */

typedef struct { const char *p; } ent_t;
static int ent_linea(void *ctx, char *buf, int max)
{
    ent_t *e = ctx;
    if (!*e->p) return -1;
    int n = 0;
    while (*e->p && *e->p != '\n' && n + 1 < max) buf[n++] = *e->p++;
    if (*e->p == '\n') e->p++;
    buf[n] = 0;
    return n;
}
typedef struct { char b[4096]; int n; } sal_t;
static int sal_esc(void *ctx, const char *s, int n) { sal_t *o = ctx; memcpy(o->b + o->n, s, (size_t)n); o->n += n; o->b[o->n] = 0; return 0; }

static const char *sed_prueba(const char *guion, bool n_opt, bool ext, const char *entrada)
{
    static sed_t s;
    static sal_t o;
    char err[160];
    sed_iniciar(&s, ext, n_opt);
    o.n = 0; o.b[0] = 0;
    if (sed_compilar(&s, guion, err, sizeof err) < 0) { snprintf(o.b, sizeof o.b, "ERROR: %s", err); return o.b; }
    ent_t e = { entrada };
    sed_executar(&s, ent_linea, &e, sal_esc, &o);
    return o.b;
}
#define SED(g, n, ext, ent, esperado) do { const char *r_ = sed_prueba(g, n, ext, ent); \
    COMPROBAR(strcmp(r_, esperado) == 0, "sed '%s': salió [%s], esperado [%s]", g, r_, esperado); } while (0)

static void prueba_sed(void)
{
    const char *t = "uno dos\ndos tres\ntres cuatro\ncuatro cinco\n";
    SED("s/dos/2/", false, false, t, "uno 2\n2 tres\ntres cuatro\ncuatro cinco\n");
    SED("s/o/0/g", false, false, "foo boo\n", "f00 b00\n");
    SED("s/o/0/2", false, false, "foo boo\n", "fo0 boo\n");
    SED("s/o/0/2g", false, false, "foo boo\n", "fo0 b00\n");
    SED("s/O/0/I", false, false, "foo\n", "f0o\n");
    SED("s/o/&&/g", false, false, "ro\n", "roo\n");
    SED("s|/usr|/opt|", false, false, "/usr/bin\n", "/opt/bin\n");
    SED("s/a\\.b/X/", false, false, "a.b axb\n", "X axb\n");
    SED("s/x*/-/g", false, false, "abc\n", "-a-b-c-\n");
    SED("s/ /\\n/", false, false, "a b\n", "a\nb\n");
    SED("2d", false, false, t, "uno dos\ntres cuatro\ncuatro cinco\n");
    SED("$d", false, false, t, "uno dos\ndos tres\ntres cuatro\n");
    SED("2,3d", false, false, t, "uno dos\ncuatro cinco\n");
    SED("2!d", false, false, t, "dos tres\n");
    SED("/tres/d", false, false, t, "uno dos\ncuatro cinco\n");
    SED("/dos/,/cuatro/d", false, false, t, "cuatro cinco\n");
    SED("2,/cuatro/p", true, false, t, "dos tres\ntres cuatro\n");
    SED("3,1p", true, false, t, "tres cuatro\n");
    SED("2p", true, false, t, "dos tres\n");
    SED("2,3p", true, false, t, "dos tres\ntres cuatro\n");
    SED("p", false, false, "a\n", "a\na\n");
    SED("2q", false, false, t, "uno dos\ndos tres\n");
    SED("=", true, false, "a\nb\n", "1\n2\n");
    SED("$=", true, false, t, "4\n");
    SED("1i\\inicio", false, false, "a\n", "inicio\na\n");
    SED("$a fin", false, false, "a\nb\n", "a\nb\nfin\n");
    SED("2c cambiada", false, false, "a\nb\nc\n", "a\ncambiada\nc\n");
    SED("2,3c bloque", false, false, "a\nb\nc\nd\n", "a\nbloque\nd\n");
    SED("s/a/b/;s/b/c/", false, false, "a\n", "c\n");
    SED("/b/s/b/X/p", true, false, "a\nb\n", "X\n");
    SED("s/(a|b)+/X/", false, true, "aabbc\n", "Xc\n");
    SED("s/\\(a\\|b\\)\\+/X/", false, false, "aabbc\n", "Xc\n");
    SED("s/^/> /", false, false, "a\nb\n", "> a\n> b\n");
    SED("s/$/;/", false, false, "a\n", "a;\n");
    SED("/^$/d", false, false, "a\n\nb\n", "a\nb\n");
    SED("s/a/b", false, false, "a\n", "ERROR: s: falta un delimitador (s/viejo/nuevo/)");
    SED("k", false, false, "a\n", "ERROR: orden desconocida (\"k\")");
    SED("s/(a)/\\1/", false, true, "a\n", "ERROR: s: las referencias \\1 no están disponibles (el motor no tiene subexpresiones)");
}

/* ------------------------------------------------------------ tr */

static const char *tr_prueba(const char *c1, const char *c2, bool del, bool sq, bool comp, const char *entrada)
{
    static tr_t t;
    static char sal[512];
    char err[96];
    if (tr_preparar(&t, c1, c2, del, sq, comp, err, sizeof err) < 0) { snprintf(sal, sizeof sal, "ERROR: %s", err); return sal; }
    int n = 0;
    for (const char *p = entrada; *p; p++) {
        int c = tr_byte(&t, (unsigned char)*p);
        if (c >= 0) sal[n++] = (char)c;
    }
    sal[n] = 0;
    return sal;
}
#define TR(c1, c2, d, s, cp, ent, esperado) do { const char *r_ = tr_prueba(c1, c2, d, s, cp, ent); \
    COMPROBAR(strcmp(r_, esperado) == 0, "tr '%s' '%s': salió [%s], esperado [%s]", c1, c2 ? c2 : "", r_, esperado); } while (0)

static void prueba_tr(void)
{
    TR("a-z", "A-Z", false, false, false, "Hola mundo", "HOLA MUNDO");
    TR("abc", "xyz", false, false, false, "aabbcc d", "xxyyzz d");
    TR("abc", "x", false, false, false, "abc", "xxx");
    TR("[:lower:]", "[:upper:]", false, false, false, "abc1", "ABC1");
    TR("[:digit:]", "#", false, false, false, "a1b22", "a#b##");
    TR("aeiou", NULL, true, false, false, "murcielago", "mrclg");
    TR("0-9", NULL, true, false, false, "a1b2c3", "abc");
    TR("a-z", NULL, true, false, true, "ab12cd", "abcd");
    TR(" ", NULL, false, true, false, "a   b  c", "a b c");
    TR("a-z", "*", false, true, false, "aabb  cc", "*  *");
    TR("\\n", " ", false, false, false, "a\nb", "a b");
    TR("\\t", "-", false, false, false, "a\tb", "a-b");
    TR("a-c", "A-C", false, false, false, "abcd", "ABCd");
    TR("a-z", "n-za-m", false, false, false, "hola", "ubyn");
    TR("a-z", NULL, false, false, false, "x", "ERROR: falta el segundo conjunto (tr C1 C2)");
    TR("z-a", "x", false, false, false, "x", "ERROR: intervalo no válido: z-a");
}

/* ------------------------------------------------------------ expr */

static const char *expr_prueba(int n, ...)
{
    static char sal[200];
    char *a[16];
    va_list ap;
    va_start(ap, n);
    for (int i = 0; i < n; i++) a[i] = va_arg(ap, char *);
    va_end(ap);
    char err[96];
    int r = expr_avaliar(n, a, sal, sizeof sal, err, sizeof err);
    if (r == 2) snprintf(sal, sizeof sal, "ERROR: %s", err);
    return sal;
}
#define EXPR(esperado, ...) do { const char *r_ = expr_prueba(__VA_ARGS__); \
    COMPROBAR(strcmp(r_, esperado) == 0, "expr %s: salió [%s], esperado [%s]", #__VA_ARGS__, r_, esperado); } while (0)

static void prueba_expr(void)
{
    EXPR("5", 3, "2", "+", "3");
    EXPR("-1", 3, "2", "-", "3");
    EXPR("6", 3, "2", "*", "3");
    EXPR("3", 3, "10", "/", "3");
    EXPR("1", 3, "10", "%", "3");
    EXPR("14", 5, "2", "+", "3", "*", "4");       /* la precedencia: 2 + (3*4) */
    EXPR("20", 7, "(", "2", "+", "3", ")", "*", "4");
    EXPR("1", 3, "3", ">", "2");
    EXPR("0", 3, "3", "<", "2");
    EXPR("1", 3, "abc", "=", "abc");
    EXPR("1", 3, "a", "!=", "b");
    EXPR("1", 3, "3", ">=", "3");
    EXPR("3", 3, "abc", ":", "...");
    EXPR("0", 3, "abc", ":", "x");
    EXPR("2", 3, "abc", ":", "ab");
    EXPR("4", 2, "length", "hola");
    EXPR("ol", 4, "substr", "hola", "2", "2");
    EXPR("3", 3, "index", "hola", "l");
    EXPR("7", 3, "0", "|", "7");
    EXPR("0", 3, "0", "&", "7");
    EXPR("5", 3, "5", "&", "7");
    EXPR("ERROR: división entre cero", 3, "1", "/", "0");
    EXPR("ERROR: expresión sintácticamente incorrecta", 2, "1", "+");
    EXPR("ERROR: argumento no numérico", 3, "a", "+", "1");
}

/* ------------------------------------------------------------ bc */

static const char *bc_prueba(const char *entrada)
{
    static char sal[1024];
    bc_t b;
    bc_iniciar(&b);
    sal[0] = 0;
    char linea[256];
    const char *p = entrada;
    while (*p) {
        int n = 0;
        while (*p && *p != '\n' && n < 255) linea[n++] = *p++;
        if (*p == '\n') p++;
        linea[n] = 0;
        char t[128];
        int r = bc_linea(&b, linea, t, sizeof t);
        if (r == 1) { strcat(sal, t); strcat(sal, "\n"); }
        else if (r < 0) { strcat(sal, "ERR:"); strcat(sal, t); strcat(sal, "\n"); }
    }
    return sal;
}
#define BC(ent, esperado) do { const char *r_ = bc_prueba(ent); \
    COMPROBAR(strcmp(r_, esperado) == 0, "bc [%s]: salió [%s], esperado [%s]", ent, r_, esperado); } while (0)

static void prueba_bc(void)
{
    BC("2+3", "5\n");
    BC("2+3*4", "14\n");
    BC("(2+3)*4", "20\n");
    BC("10/3", "3\n");
    BC("scale=2; 10/3", "3.33\n");
    BC("scale=4; 1/3", ".3333\n");
    BC("10%3", "1\n");
    BC("2^10", "1024\n");
    BC("-5+2", "-3\n");
    BC("1.5+2.25", "3.75\n");
    BC("scale=2; 1.5*2.25", "3.37\n");
    BC("a=5; b=a*2; b+1", "11\n");
    BC("a=3\na*a", "9\n");
    BC("scale=3; sqrt(2)", "1.414\n");
    BC("sqrt(16)", "4\n");
    BC("scale=2; 7/2", "3.50\n");
    BC("1/0", "ERR:división entre cero\n");
    BC("2+", "ERR:expresión no válida\n");
    BC("scale=1; .5+.25", ".75\n");
    BC("100000*100000", "10000000000\n");
    BC("2^0", "1\n");
    BC("5-3-1", "1\n");
    BC("2^3^2", "512\n");
}

/* ------------------------------------------------------------ cal */

static void prueba_cal(void)
{
    COMPROBAR(cal_dias_mes(2024, 2) == 29, "febrero 2024");
    COMPROBAR(cal_dias_mes(2023, 2) == 28, "febrero 2023");
    COMPROBAR(cal_dias_mes(1900, 2) == 28, "1900 no es bisiesto");
    COMPROBAR(cal_dias_mes(2000, 2) == 29, "2000 es bisiesto");
    COMPROBAR(cal_dias_mes(2026, 11) == 30, "noviembre");
    COMPROBAR(cal_dia_semana(2026, 11, 1) == 0, "1 nov 2026 es domingo (%d)", cal_dia_semana(2026, 11, 1));
    COMPROBAR(cal_dia_semana(2024, 2, 29) == 4, "29 feb 2024 es jueves");
    COMPROBAR(cal_dia_semana(1970, 1, 1) == 4, "1 ene 1970 es jueves");
    COMPROBAR(cal_dia_semana(2000, 1, 1) == 6, "1 ene 2000 es sábado");
    char t[512];
    cal_mes(t, sizeof t, 2026, 11, true, 1);
    const char *e = "   noviembre 2026\nlu ma mi ju vi sá do\n                   1\n"
                    " 2  3  4  5  6  7  8\n 9 10 11 12 13 14 15\n16 17 18 19 20 21 22\n23 24 25 26 27 28 29\n30\n";
    COMPROBAR(strcmp(t, e) == 0, "cal 11 2026 (lunes primero) salió:\n%s\nesperado:\n%s", t, e);
    cal_mes(t, sizeof t, 2024, 2, false, 1);
    e = "    febrero 2024\ndo lu ma mi ju vi sá\n             1  2  3\n 4  5  6  7  8  9 10\n11 12 13 14 15 16 17\n18 19 20 21 22 23 24\n25 26 27 28 29\n";
    COMPROBAR(strcmp(t, e) == 0, "cal 2 2024 (domingo primero) salió:\n%s\nesperado:\n%s", t, e);
}

/* ------------------------------------------------------------ base64 */

static void prueba_base64(void)
{
    char s[128];
    uint8_t b[128];
    struct { const char *claro, *cod; } v[] = { { "", "" }, { "f", "Zg==" }, { "fo", "Zm8=" }, { "foo", "Zm9v" },
        { "foob", "Zm9vYg==" }, { "fooba", "Zm9vYmE=" }, { "foobar", "Zm9vYmFy" }, { "Hola, mundo\n", "SG9sYSwgbXVuZG8K" } };
    for (unsigned i = 0; i < sizeof v / sizeof v[0]; i++) {
        int n = b64_codificar((const uint8_t *)v[i].claro, (int)strlen(v[i].claro), s, sizeof s);
        COMPROBAR(n == (int)strlen(v[i].cod) && strcmp(s, v[i].cod) == 0, "base64 de [%s]: [%s]", v[i].claro, s);
        int m = b64_decodificar(v[i].cod, (int)strlen(v[i].cod), b, sizeof b, false);
        COMPROBAR(m == (int)strlen(v[i].claro) && memcmp(b, v[i].claro, (size_t)m) == 0, "decodificar [%s]: %d", v[i].cod, m);
    }
    COMPROBAR(b64_decodificar("Zm9v\nYmFy\n", 10, b, sizeof b, false) == 6, "decodifica ignorando saltos");
    COMPROBAR(b64_decodificar("Zm9v!", 5, b, sizeof b, false) < 0, "carácter inválido");
    COMPROBAR(b64_decodificar("Zm9v!", 5, b, sizeof b, true) == 3, "-i ignora basura");
    uint8_t bin[256];
    for (int i = 0; i < 256; i++) bin[i] = (uint8_t)i;
    char *g = malloc(1024);
    int n = b64_codificar(bin, 256, g, 1024);
    uint8_t back[256];
    COMPROBAR(b64_decodificar(g, n, back, 256, false) == 256 && memcmp(back, bin, 256) == 0, "ida y vuelta de los 256 bytes");
    free(g);
}

/* ------------------------------------------------------------ od / xxd */

static void prueba_od(void)
{
    char t[256];
    const uint8_t d[] = "Hola, mundo!\n\x01\xff";
    od_linea_hex_c(t, sizeof t, 0, d, 15);
    const char *e = "00000000  48 6f 6c 61 2c 20 6d 75  6e 64 6f 21 0a 01 ff     |Hola, mundo!...|";
    COMPROBAR(strcmp(t, e) == 0, "hexdump -C salió [%s] esperado [%s]", t, e);
    od_linea_xxd(t, sizeof t, 0, (const uint8_t *)"Hola, mundo!\n", 13);
    e = "00000000: 486f 6c61 2c20 6d75 6e64 6f21 0a         Hola, mundo!.";
    COMPROBAR(strcmp(t, e) == 0, "xxd salió [%s] esperado [%s]", t, e);
    od_linea_od(t, sizeof t, 0, (const uint8_t *)"abc", 3, 'o', 1, 8);
    COMPROBAR(strcmp(t, "0000000 141 142 143") == 0, "od -t o1 salió [%s]", t);
    od_linea_od(t, sizeof t, 0, (const uint8_t *)"abc", 3, 'x', 1, 8);
    COMPROBAR(strcmp(t, "0000000 61 62 63") == 0, "od -t x1 salió [%s]", t);
    od_linea_od(t, sizeof t, 16, (const uint8_t *)"a\n", 2, 'c', 1, 8);
    COMPROBAR(strcmp(t, "0000020   a  \\n") == 0, "od -c salió [%s]", t);
    od_linea_od(t, sizeof t, 0, (const uint8_t *)"\x01\x02\x03\x04", 4, 'x', 2, 8);
    COMPROBAR(strcmp(t, "0000000 0201 0403") == 0, "od -t x2 salió [%s]", t);
    od_linea_od(t, sizeof t, 0, (const uint8_t *)"\x01\x02\x03\x04", 4, 'd', 2, 8);
    COMPROBAR(strcmp(t, "0000000    513   1027") == 0, "od -t d2 salió [%s]", t);
    od_linea_od(t, sizeof t, 0, (const uint8_t *)"\x01\x02\x03\x04", 4, 'u', 1, 0);
    COMPROBAR(strcmp(t, "   1   2   3   4") == 0, "od -An -t u1 salió [%s]", t);
    od_linea_od(t, sizeof t, 2, (const uint8_t *)"a\n", 2, 'c', 1, 16);
    COMPROBAR(strcmp(t, "000002   a  \\n") == 0, "od -A x -c salió [%s]", t);
}

/* ------------------------------------------------------------ md5 / sha1 */

static void prueba_resumos(void)
{
    char h[48];
    md5_hex("", 0, h);
    COMPROBAR(strcmp(h, "d41d8cd98f00b204e9800998ecf8427e") == 0, "md5 vacío: %s", h);
    md5_hex("abc", 3, h);
    COMPROBAR(strcmp(h, "900150983cd24fb0d6963f7d28e17f72") == 0, "md5 abc: %s", h);
    md5_hex("The quick brown fox jumps over the lazy dog", 43, h);
    COMPROBAR(strcmp(h, "9e107d9d372bb6826bd81d3542a419d6") == 0, "md5 zorro: %s", h);
    char largo[1000];
    memset(largo, 'a', sizeof largo);
    md5_hex(largo, 1000, h);
    COMPROBAR(strcmp(h, "cabe45dcc9ae5b66ba86600cca6b8ba8") == 0, "md5 1000 a: %s", h);
    sha1_hex("", 0, h);
    COMPROBAR(strcmp(h, "da39a3ee5e6b4b0d3255bfef95601890afd80709") == 0, "sha1 vacío: %s", h);
    sha1_hex("abc", 3, h);
    COMPROBAR(strcmp(h, "a9993e364706816aba3e25717850c26c9cd0d89d") == 0, "sha1 abc: %s", h);
    sha1_hex("The quick brown fox jumps over the lazy dog", 43, h);
    COMPROBAR(strcmp(h, "2fd4e1c67a2d28fced849ee1bb76e7391b93eb12") == 0, "sha1 zorro: %s", h);
    sha1_hex(largo, 1000, h);
    COMPROBAR(strcmp(h, "291e9a6c66994949b57ba5e650361e98fc36b1ba") == 0, "sha1 1000 a: %s", h);
}

/* ------------------------------------------------------------ file */

static const char *tipo(const void *d, int n) { static char t[200]; file_tipo(d, n, t, sizeof t); return t; }
#define TIPO(d, n, esperado) do { const char *r_ = tipo(d, n); \
    COMPROBAR(strcmp(r_, esperado) == 0, "file: salió [%s], esperado [%s]", r_, esperado); } while (0)

static void prueba_file(void)
{
    TIPO("", 0, "vacío");
    TIPO("hola\nmundo\n", 11, "texto ASCII");
    TIPO("a\r\nb\r\n", 6, "texto ASCII, con terminadores de línea CRLF");
    TIPO("camión\n", 8, "texto UTF-8");
    TIPO("cami\xf3n\n", 7, "texto ISO-8859");
    TIPO("#!/bin/sh\necho\n", 15, "guion ejecutable (/bin/sh)");
    TIPO("<?xml version=\"1.0\"?>\n", 22, "documento XML");
    TIPO("<!DOCTYPE html>\n<html>", 22, "documento HTML");
    TIPO("%PDF-1.7\n", 9, "documento PDF, versión 1.7");
    TIPO("\x1f\x8b\x08\x00", 4, "datos comprimidos con gzip");
    TIPO("PK\x03\x04xxxx", 8, "archivo ZIP");
    TIPO("\xff\xd8\xff\xe0", 4, "imagen JPEG");
    TIPO("ID3\x03\x00", 5, "audio MP3 con etiquetas ID3");
    TIPO("SQLite format 3\0", 16, "base de datos SQLite 3");
    TIPO("a\0b", 3, "datos");
    const uint8_t png[] = { 0x89, 'P', 'N', 'G', 13, 10, 0x1a, 10, 0, 0, 0, 13, 'I', 'H', 'D', 'R', 0, 0, 1, 0, 0, 0, 0, 0x80 };
    TIPO(png, (int)sizeof png, "imagen PNG, 256 x 128");
    uint8_t elf[20] = { 0x7f, 'E', 'L', 'F', 1, 1, 1 };
    elf[16] = 2; elf[18] = 3;
    TIPO(elf, 20, "ELF 32-bit LSB, ejecutable, Intel 80386");
    TIPO("RIFF\x24\0\0\0WAVEfmt ", 16, "audio WAV");
    TIPO("MThd\0\0\0\6", 8, "música MIDI");
    TIPO("GIF89a\x10\0\x20\0", 10, "imagen GIF, 16 x 32");
}

int main(void)
{
    prueba_sed();
    prueba_tr();
    prueba_expr();
    prueba_bc();
    prueba_cal();
    prueba_base64();
    prueba_od();
    prueba_resumos();
    prueba_file();
    if (fallos) { printf("%d de %d comprobaciones fallan\n", fallos, total); return 1; }
    printf("todo bien (%d comprobaciones)\n", total);
    return 0;
}
