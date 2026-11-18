// SPDX-License-Identifier: GPL-2.0-or-later
/* realce.c - colores del texto por lenguaje.
 *
 * Cada lenguaje es un bucle solo sobre la línea, que marca una clase por letra
 * (palabra clave, tipo, cadena, comentario, número...). No hay gramática:
 * son las reglas que usa un ojo humano para leer código rápido. Lo que
 * atraviesa líneas (comentario de bloque de C, cadenas de tres comillas de Python,
 * bloques ``` de Markdown) se convierte en un "estado" que la línea devuelve y la siguiente
 * recibe; el documento guarda ese estado por línea (documento.c).
 *
 * Con classes == NULL la función solo acompaña el estado: es lo que corre cuando
 * se salta al final de un archivo grande, línea a línea, sin dibujar. */
#include "mico.h"

const char *const nome_ling[QTD_LING] = { "Texto", "C", "Python", "sh", "Markdown", "CHIP-8" };
const char letra_classe[QTD_CLASSES + 1] = ".ktscnpfherlv";

static bool letra(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || (unsigned char)c >= 0xC0; }
static bool digito(char c) { return c >= '0' && c <= '9'; }
static bool alnum(char c) { return letra(c) || digito(c); }

static bool na_lista(const char *const *l, const char *s, int n, bool sem_caixa)
{
    for (; *l; l++) {
        int k = 0;
        while (k < n && (*l)[k]) {
            char a = s[k], b = (*l)[k];
            if (sem_caixa && a >= 'A' && a <= 'Z') a = (char)(a + 32);
            if (a != b) break;
            k++;
        }
        if (k == n && !(*l)[k]) return true;
    }
    return false;
}

static void pintar(uint8_t *cl, int a, int b, int c)
{
    if (cl) for (int i = a; i < b; i++) cl[i] = (uint8_t)c;
}

/* Número: 123, 0x1F, 1.5e3, 10u... */
static int numero(const char *s, int n, int i)
{
    if (i + 1 < n && s[i] == '0' && (s[i + 1] == 'x' || s[i + 1] == 'X' || s[i + 1] == 'b' || s[i + 1] == 'B')) i += 2;
    while (i < n && (alnum(s[i]) || s[i] == '.')) {
        if ((s[i] == 'e' || s[i] == 'E') && i + 1 < n && (s[i + 1] == '+' || s[i + 1] == '-')) i++;
        i++;
    }
    return i;
}

/* Cadena entre comillas q, con escapes \; devuelve la posición después del cierre
 * (o n, si la línea termina antes). */
static int cadeia(const char *s, int n, int i, char q, bool escapes)
{
    for (i++; i < n; i++) {
        if (escapes && s[i] == '\\') { i++; continue; }
        if (s[i] == q) return i + 1;
    }
    return n;
}

static int proximo_nao_espaco(const char *s, int n, int i)
{
    while (i < n && (s[i] == ' ' || s[i] == '\t')) i++;
    return i;
}

/* ------------------------------------------------------------ C */

static const char *const c_chaves[] = { "if", "else", "for", "while", "do", "switch", "case", "default", "break",
    "continue", "return", "goto", "sizeof", "typedef", "struct", "union", "enum", "static", "extern", "const",
    "volatile", "register", "inline", "auto", "restrict", NULL };
static const char *const c_tipos[] = { "void", "char", "short", "int", "long", "float", "double", "signed",
    "unsigned", "bool", "_Bool", "size_t", "uint8_t", "uint16_t", "uint32_t", "uint64_t", "int8_t", "int16_t",
    "int32_t", "int64_t", "FILE", NULL };
static const char *const c_consts[] = { "true", "false", "NULL", NULL };

static int realce_c(const char *s, int n, int estado, uint8_t *cl)
{
    int i = 0;
    if (estado == 1) {   /* dentro de un comentario de bloque */
        while (i < n && !(s[i] == '*' && i + 1 < n && s[i + 1] == '/')) i++;
        if (i >= n) { pintar(cl, 0, n, CL_COMENTARIO); return 1; }
        i += 2;
        pintar(cl, 0, i, CL_COMENTARIO);
    }
    int p = proximo_nao_espaco(s, n, i);
    if (p < n && s[p] == '#') {   /* preprocesador: la directiva y, en el #include, el <archivo> */
        int q = proximo_nao_espaco(s, n, p + 1);
        while (q < n && letra(s[q])) q++;
        pintar(cl, p, q, CL_PREPROC);
        i = q;
        q = proximo_nao_espaco(s, n, q);
        if (q < n && s[q] == '<') {
            int f = q;
            while (f < n && s[f] != '>') f++;
            if (f < n) f++;
            pintar(cl, q, f, CL_CADEIA);
            i = f;
        }
    }
    while (i < n) {
        char c = s[i];
        if (c == '/' && i + 1 < n && s[i + 1] == '/') { pintar(cl, i, n, CL_COMENTARIO); return 0; }
        if (c == '/' && i + 1 < n && s[i + 1] == '*') {
            int k = i + 2;
            while (k < n && !(s[k] == '*' && k + 1 < n && s[k + 1] == '/')) k++;
            if (k >= n) { pintar(cl, i, n, CL_COMENTARIO); return 1; }
            pintar(cl, i, k + 2, CL_COMENTARIO);
            i = k + 2;
            continue;
        }
        if (c == '"' || c == '\'') { int f = cadeia(s, n, i, c, true); pintar(cl, i, f, CL_CADEIA); i = f; continue; }
        if (digito(c) || (c == '.' && i + 1 < n && digito(s[i + 1]))) { int f = numero(s, n, i); pintar(cl, i, f, CL_NUMERO); i = f; continue; }
        if (letra(c)) {
            int f = i;
            while (f < n && alnum(s[f])) f++;
            int cls = CL_NORMAL;
            if (na_lista(c_chaves, s + i, f - i, false)) cls = CL_CHAVE;
            else if (na_lista(c_tipos, s + i, f - i, false)) cls = CL_TIPO;
            else if (na_lista(c_consts, s + i, f - i, false)) cls = CL_NUMERO;
            else { int q = proximo_nao_espaco(s, n, f); if (q < n && s[q] == '(') cls = CL_FUNCAO; }
            pintar(cl, i, f, cls);
            i = f;
            continue;
        }
        if (cl) cl[i] = CL_NORMAL;
        i++;
    }
    return 0;
}

/* ------------------------------------------------------------ Python */

static const char *const py_chaves[] = { "and", "as", "assert", "async", "await", "break", "class", "continue",
    "def", "del", "elif", "else", "except", "finally", "for", "from", "global", "if", "import", "in", "is",
    "lambda", "nonlocal", "not", "or", "pass", "raise", "return", "try", "while", "with", "yield", NULL };
static const char *const py_consts[] = { "True", "False", "None", NULL };
static const char *const py_tipos[] = { "print", "len", "range", "int", "str", "float", "list", "dict", "set",
    "tuple", "open", "input", "bool", "type", "isinstance", "super", "abs", "min", "max", "sum", "enumerate",
    "zip", "map", "filter", "sorted", "bytes", "repr", "ord", "chr", "self", NULL };

/* Cierre de las tres comillas q desde i; -1 si no cierra en esta línea. */
static int fecha_tripla(const char *s, int n, int i, char q)
{
    for (; i + 3 <= n; i++) {
        if (s[i] == '\\') { i++; continue; }
        if (s[i] == q && s[i + 1] == q && s[i + 2] == q) return i + 3;
    }
    return -1;
}

static int realce_py(const char *s, int n, int estado, uint8_t *cl)
{
    int i = 0;
    if (estado == 1 || estado == 2) {
        char q = estado == 1 ? '\'' : '"';
        int f = fecha_tripla(s, n, 0, q);
        if (f < 0) { pintar(cl, 0, n, CL_CADEIA); return estado; }
        pintar(cl, 0, f, CL_CADEIA);
        i = f;
    }
    int antes_def = 0;   /* 1 = la palabra anterior fue def/class */
    while (i < n) {
        char c = s[i];
        if (c == '#') { pintar(cl, i, n, CL_COMENTARIO); return 0; }
        /* prefijos de cadena: r'', b"", f"", rb''... */
        int p = i;
        while (p < n && p - i < 2 && (s[p] == 'r' || s[p] == 'b' || s[p] == 'f' || s[p] == 'u' ||
                                      s[p] == 'R' || s[p] == 'B' || s[p] == 'F' || s[p] == 'U')) p++;
        if (p < n && (s[p] == '"' || s[p] == '\'') && (p == i || i == 0 || !alnum(s[i - 1]))) {
            char q = s[p];
            if (p + 2 < n && s[p + 1] == q && s[p + 2] == q) {
                int f = fecha_tripla(s, n, p + 3, q);
                if (f < 0) { pintar(cl, i, n, CL_CADEIA); return q == '\'' ? 1 : 2; }
                pintar(cl, i, f, CL_CADEIA);
                i = f;
                continue;
            }
            int f = cadeia(s, n, p, q, true);
            pintar(cl, i, f, CL_CADEIA);
            i = f;
            continue;
        }
        if (c == '@' && proximo_nao_espaco(s, n, 0) == i) {
            int f = i + 1;
            while (f < n && (alnum(s[f]) || s[f] == '.')) f++;
            pintar(cl, i, f, CL_PREPROC);
            i = f;
            continue;
        }
        if (digito(c) || (c == '.' && i + 1 < n && digito(s[i + 1]))) { int f = numero(s, n, i); pintar(cl, i, f, CL_NUMERO); i = f; continue; }
        if (letra(c)) {
            int f = i;
            while (f < n && alnum(s[f])) f++;
            int cls = CL_NORMAL;
            if (antes_def) cls = CL_FUNCAO;
            else if (na_lista(py_chaves, s + i, f - i, false)) cls = CL_CHAVE;
            else if (na_lista(py_consts, s + i, f - i, false)) cls = CL_NUMERO;
            else if (na_lista(py_tipos, s + i, f - i, false)) cls = CL_TIPO;
            else { int q = proximo_nao_espaco(s, n, f); if (q < n && s[q] == '(') cls = CL_FUNCAO; }
            antes_def = (f - i == 3 && compara_texto_n(s + i, "def", 3) == 0) || (f - i == 5 && compara_texto_n(s + i, "class", 5) == 0);
            pintar(cl, i, f, cls);
            i = f;
            continue;
        }
        if (c != ' ' && c != '\t') antes_def = 0;
        if (cl) cl[i] = CL_NORMAL;
        i++;
    }
    return 0;
}

/* ------------------------------------------------------------ sh */

static const char *const sh_chaves[] = { "if", "then", "else", "elif", "fi", "for", "while", "until", "do",
    "done", "case", "esac", "in", "function", "return", "local", "export", "break", "continue", "exit",
    "readonly", NULL };
static const char *const sh_tipos[] = { "echo", "cd", "pwd", "read", "test", "printf", "set", "unset", "shift",
    "source", "eval", "exec", "trap", "alias", "true", "false", "ayuda", "ajuda", NULL };

static int variavel(const char *s, int n, int i)
{
    i++;
    if (i < n && s[i] == '{') { while (i < n && s[i] != '}') i++; return i < n ? i + 1 : n; }
    if (i < n && s[i] == '(') { int p = 1; for (i++; i < n && p; i++) { if (s[i] == '(') p++; else if (s[i] == ')') p--; } return i; }
    if (i < n && (digito(s[i]) || s[i] == '?' || s[i] == '#' || s[i] == '@' || s[i] == '*' || s[i] == '$' || s[i] == '!')) return i + 1;
    while (i < n && alnum(s[i])) i++;
    return i;
}

static int realce_sh(const char *s, int n, uint8_t *cl)
{
    int i = 0;
    while (i < n) {
        char c = s[i];
        if (c == '#' && (i == 0 || s[i - 1] == ' ' || s[i - 1] == '\t' || s[i - 1] == ';')) { pintar(cl, i, n, CL_COMENTARIO); return 0; }
        if (c == '\'') { int f = cadeia(s, n, i, c, false); pintar(cl, i, f, CL_CADEIA); i = f; continue; }
        if (c == '"') {
            int f = cadeia(s, n, i, c, true);
            pintar(cl, i, f, CL_CADEIA);
            for (int k = i + 1; k < f - 1; k++)   /* $VAR dentro de comillas dobles */
                if (s[k] == '$' && s[k - 1] != '\\') { int g = variavel(s, f - 1, k); pintar(cl, k, g, CL_VARIAVEL); k = g - 1; }
            i = f;
            continue;
        }
        if (c == '$') { int f = variavel(s, n, i); pintar(cl, i, f, CL_VARIAVEL); i = f; continue; }
        if (digito(c) && (i == 0 || !alnum(s[i - 1]))) { int f = numero(s, n, i); pintar(cl, i, f, CL_NUMERO); i = f; continue; }
        if (letra(c)) {
            int f = i;
            while (f < n && (alnum(s[f]) || s[f] == '-')) f++;
            int cls = CL_NORMAL;
            if (na_lista(sh_chaves, s + i, f - i, false)) cls = CL_CHAVE;
            else if (na_lista(sh_tipos, s + i, f - i, false)) cls = CL_TIPO;
            else {
                int q = proximo_nao_espaco(s, n, f);
                if (q + 1 < n && s[q] == '(' && s[q + 1] == ')') cls = CL_FUNCAO;
                else if (f < n && s[f] == '=') cls = CL_VARIAVEL;
            }
            pintar(cl, i, f, cls);
            i = f;
            continue;
        }
        if (cl) cl[i] = CL_NORMAL;
        i++;
    }
    return 0;
}

/* ------------------------------------------------------------ Markdown */

static int realce_md(const char *s, int n, int estado, uint8_t *cl)
{
    pintar(cl, 0, n, CL_NORMAL);
    int p = proximo_nao_espaco(s, n, 0);
    bool cerca = p + 2 < n && ((s[p] == '`' && s[p + 1] == '`' && s[p + 2] == '`') || (s[p] == '~' && s[p + 1] == '~' && s[p + 2] == '~'));
    if (estado == 1) { pintar(cl, 0, n, CL_CADEIA); return cerca ? 0 : 1; }
    if (cerca) { pintar(cl, 0, n, CL_CADEIA); return 1; }
    if (p < n && s[p] == '#') { pintar(cl, 0, n, CL_TITULO); return 0; }
    int i = p;
    if (p < n && s[p] == '>') { pintar(cl, p, p + 1, CL_CHAVE); i = p + 1; }
    else if (p + 1 < n && (s[p] == '-' || s[p] == '*' || s[p] == '+') && s[p + 1] == ' ') { pintar(cl, p, p + 1, CL_CHAVE); i = p + 1; }
    else if (p < n && digito(s[p])) {
        int f = p;
        while (f < n && digito(s[f])) f++;
        if (f < n && (s[f] == '.' || s[f] == ')')) { pintar(cl, p, f + 1, CL_CHAVE); i = f + 1; }
    }
    while (i < n) {
        char c = s[i];
        if (c == '\\' && i + 1 < n) { i += 2; continue; }
        if (c == '`') {
            int f = i + 1;
            while (f < n && s[f] != '`') f++;
            if (f < n) { pintar(cl, i, f + 1, CL_CADEIA); i = f + 1; continue; }
        }
        if (c == '*' || c == '_') {   /* *énfasis*, **fuerte**, _énfasis_ */
            int k = i;
            while (k < n && s[k] == c) k++;
            int q = k - i, f = k;
            while (f < n && !(s[f] == c && f + q <= n && compara_texto_n(s + f, s + i, (size_t)q) == 0)) f++;
            if (f < n && f > k) { pintar(cl, i, f + q, CL_ENFASE); i = f + q; continue; }
            i = k;
            continue;
        }
        if (c == '[' || (c == '!' && i + 1 < n && s[i + 1] == '[')) {   /* [texto](destino) */
            int a = c == '!' ? i + 1 : i, f = a;
            while (f < n && s[f] != ']') f++;
            if (f + 1 < n && s[f + 1] == '(') {
                int g = f + 1;
                while (g < n && s[g] != ')') g++;
                pintar(cl, i, f + 1, CL_VARIAVEL);
                pintar(cl, f + 1, g < n ? g + 1 : n, CL_COMENTARIO);
                i = g < n ? g + 1 : n;
                continue;
            }
        }
        i++;
    }
    return 0;
}

/* ------------------------------------------------------------ CHIP-8 (sintaxis de chip8asm, D071) */

static const char *const c8_mnem[] = { "cls", "ret", "sys", "jp", "call", "se", "sne", "ld", "add", "or", "and",
    "xor", "sub", "shr", "subn", "shl", "rnd", "drw", "skp", "sknp", "scd", "scr", "scl", "scu", "exit", "low",
    "high", "save", "load", "plane", "audio", "pitch", NULL };
static const char *const c8_dir[] = { "db", "dw", "ds", "org", "align", "include", "equ", "byte", "long", NULL };
static const char *const c8_reg[] = { "i", "dt", "st", "k", "f", "hf", "b", "r", NULL };

static int realce_c8(const char *s, int n, uint8_t *cl)
{
    int i = 0;
    pintar(cl, 0, n, CL_NORMAL);
    int p = proximo_nao_espaco(s, n, 0);
    if (p < n && letra(s[p])) {   /* "etiqueta:" o "nombre = valor" al comienzo */
        int f = p;
        while (f < n && alnum(s[f])) f++;
        int q = proximo_nao_espaco(s, n, f);
        if (f < n && s[f] == ':') { pintar(cl, p, f + 1, CL_ROTULO); i = f + 1; }
        else if (q < n && s[q] == '=') { pintar(cl, p, f, CL_ROTULO); i = f; }
        else if (q + 3 <= n && (s[q] == 'e' || s[q] == 'E') && na_lista(c8_dir, s + q, 3, true) && compara_texto_n(s + q, "equ", 3) == 0) { pintar(cl, p, f, CL_ROTULO); i = f; }
    }
    while (i < n) {
        char c = s[i];
        if (c == ';' || c == '#') { pintar(cl, i, n, CL_COMENTARIO); return 0; }
        if (c == '"' || c == '\'') { int f = cadeia(s, n, i, c, true); pintar(cl, i, f, CL_CADEIA); i = f; continue; }
        if (c == '%' && i + 1 < n && (s[i + 1] == '0' || s[i + 1] == '1' || s[i + 1] == '#' || s[i + 1] == '.')) {
            int f = i + 1;
            while (f < n && (s[f] == '0' || s[f] == '1' || s[f] == '#' || s[f] == '.')) f++;
            pintar(cl, i, f, CL_NUMERO);
            i = f;
            continue;
        }
        if (digito(c) || (c == '$' && (i + 1 >= n || !alnum(s[i + 1])))) { int f = c == '$' ? i + 1 : numero(s, n, i); pintar(cl, i, f, CL_NUMERO); i = f; continue; }
        if (letra(c)) {
            int f = i;
            while (f < n && alnum(s[f])) f++;
            int k = f - i, cls = CL_NORMAL;
            if (k == 2 && (s[i] == 'v' || s[i] == 'V') && ((s[i + 1] >= '0' && s[i + 1] <= '9') ||
                (s[i + 1] >= 'a' && s[i + 1] <= 'f') || (s[i + 1] >= 'A' && s[i + 1] <= 'F'))) cls = CL_REGISTRO;
            else if (na_lista(c8_mnem, s + i, k, true)) cls = CL_CHAVE;
            else if (na_lista(c8_dir, s + i, k, true)) cls = CL_PREPROC;
            else if (na_lista(c8_reg, s + i, k, true)) cls = CL_REGISTRO;
            pintar(cl, i, f, cls);
            i = f;
            continue;
        }
        i++;
    }
    return 0;
}

/* ------------------------------------------------------------ común */

int realce_linha(int ling, const char *s, int n, int estado, uint8_t *classes)
{
    switch (ling) {
    case LING_C:  return realce_c(s, n, estado, classes);
    case LING_PY: return realce_py(s, n, estado, classes);
    case LING_SH: return realce_sh(s, n, classes);
    case LING_MD: return realce_md(s, n, estado, classes);
    case LING_C8: return realce_c8(s, n, classes);
    default: pintar(classes, 0, n, CL_NORMAL); return 0;
    }
}

const char *comentario_de(int ling)
{
    switch (ling) {
    case LING_C: return "//";
    case LING_PY: case LING_SH: return "#";
    case LING_C8: return ";";
    default: return NULL;
    }
}

static bool termina(const char *c, const char *ext)
{
    int a = (int)tam_texto(c), b = (int)tam_texto(ext);
    if (a < b) return false;
    for (int i = 0; i < b; i++) {
        char x = c[a - b + i];
        if (x >= 'A' && x <= 'Z') x = (char)(x + 32);
        if (x != ext[i]) return false;
    }
    return true;
}

/* Por la extensión; sin extensión conocida, por la primera línea (#!/bin/sh). */
int ling_do_nome(const char *caminho, const char *inicio, int n)
{
    if (termina(caminho, ".c") || termina(caminho, ".h")) return LING_C;
    if (termina(caminho, ".py")) return LING_PY;
    if (termina(caminho, ".sh") || termina(caminho, "/.mico") || termina(caminho, "sagshrc")) return LING_SH;
    if (termina(caminho, ".md") || termina(caminho, ".markdown")) return LING_MD;
    if (termina(caminho, ".asm") || termina(caminho, ".c8") || termina(caminho, ".8o")) return LING_C8;
    if (n > 2 && inicio[0] == '#' && inicio[1] == '!') {
        char l[64];
        int k = 0;
        while (k < n && k < 63 && inicio[k] != '\n') { l[k] = inicio[k]; k++; }
        l[k] = 0;
        for (int i = 0; i + 6 <= k; i++) if (compara_texto_n(l + i, "python", 6) == 0) return LING_PY;
        for (int i = 0; i + 2 <= k; i++) if (l[i] == 's' && l[i + 1] == 'h') return LING_SH;
    }
    return LING_TEXTO;
}
