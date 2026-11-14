// SPDX-License-Identifier: GPL-2.0-or-later
/* tr_logica.h - la lógica de tr (D232): expande los conjuntos (a-z, [:lower:], \n...) y traduce, borra o aprieta bytes. */
#ifndef TR_LOGICA_H
#define TR_LOGICA_H
#include "sagui.h"

#define TR_MAX 2048

typedef struct {
    unsigned char mapa[256];
    bool borra[256], aprieta[256];
    int  ultimo;     /* último byte emitido (para apretar repetidos); -1 = ninguno */
} tr_t;

static bool tr_clase(const char *nombre, int n, int c)
{
    if (n == 5 && comeca_com(nombre, "lower")) return c >= 'a' && c <= 'z';
    if (n == 5 && comeca_com(nombre, "upper")) return c >= 'A' && c <= 'Z';
    if (n == 5 && comeca_com(nombre, "digit")) return c >= '0' && c <= '9';
    if (n == 5 && comeca_com(nombre, "alpha")) return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
    if (n == 5 && comeca_com(nombre, "alnum")) return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
    if (n == 5 && comeca_com(nombre, "space")) return c == ' ' || (c >= 9 && c <= 13);
    if (n == 5 && comeca_com(nombre, "blank")) return c == ' ' || c == '\t';
    if (n == 5 && comeca_com(nombre, "punct")) return (c > 32 && c < 127) && !((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'));
    if (n == 6 && comeca_com(nombre, "xdigit")) return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
    if (n == 5 && comeca_com(nombre, "cntrl")) return c < 32 || c == 127;
    if (n == 5 && comeca_com(nombre, "print")) return c >= 32 && c < 127;
    if (n == 5 && comeca_com(nombre, "graph")) return c > 32 && c < 127;
    return false;
}

static bool tr_clase_valida(const char *nombre, int n)
{
    for (int c = 0; c < 256; c++) if (tr_clase(nombre, n, c)) return true;
    return false;
}

/* Un carácter del conjunto (con \n \t \r \\ y \NNN octales); avanza *p. */
static int tr_caracter(const char **p)
{
    const char *q = *p;
    if (*q == '\\' && q[1]) {
        q++;
        int c = *q++;
        if (c == 'n') c = '\n'; else if (c == 't') c = '\t'; else if (c == 'r') c = '\r';
        else if (c == 'a') c = 7; else if (c == 'f') c = 12; else if (c == 'v') c = 11; else if (c == 'b') c = 8;
        else if (c >= '0' && c <= '7') {
            int v = c - '0', k = 1;
            while (k < 3 && *q >= '0' && *q <= '7') { v = v * 8 + (*q++ - '0'); k++; }
            c = v & 255;
        }
        *p = q;
        return c;
    }
    *p = q + 1;
    return (unsigned char)*q;
}

static int tr_expandir(const char *s, unsigned char *out, int max, char *err, int emax)
{
    int n = 0;
    while (*s) {
        if (s[0] == '[' && s[1] == ':') {
            const char *f = acha_char(s + 2, ':');
            if (f && f[1] == ']') {
                int len = (int)(f - (s + 2));
                if (!tr_clase_valida(s + 2, len)) { copia_texto(err, "clase de caracteres no válida", (size_t)emax); return -1; }
                for (int c = 0; c < 256; c++)
                    if (tr_clase(s + 2, len, c)) { if (n >= max) goto largo; out[n++] = (unsigned char)c; }
                s = f + 2;
                continue;
            }
        }
        int a = tr_caracter(&s);
        if (*s == '-' && s[1]) {
            const char *q = s + 1;
            int b = tr_caracter(&q);
            if (b < a) { formatar_texto(err, (size_t)emax, "intervalo no válido: %c-%c", a, b); return -1; }
            for (int c = a; c <= b; c++) { if (n >= max) goto largo; out[n++] = (unsigned char)c; }
            s = q;
        } else { if (n >= max) goto largo; out[n++] = (unsigned char)a; }
    }
    return n;
largo:
    copia_texto(err, "conjunto demasiado largo", (size_t)emax);
    return -1;
}

/* c2 puede ser NULL con del o con squeeze. */
static int tr_preparar(tr_t *t, const char *c1, const char *c2, bool del, bool sq, bool comp, char *err, int emax)
{
    static unsigned char s1[TR_MAX], s2[TR_MAX];
    int n1, n2 = 0;
    memset(t, 0, sizeof *t);
    t->ultimo = -1;
    for (int i = 0; i < 256; i++) t->mapa[i] = (unsigned char)i;
    n1 = tr_expandir(c1, s1, TR_MAX, err, emax);
    if (n1 < 0) return -1;
    if (c2) { n2 = tr_expandir(c2, s2, TR_MAX, err, emax); if (n2 < 0) return -1; }
    if (comp) {
        bool esta[256] = { false };
        for (int i = 0; i < n1; i++) esta[s1[i]] = true;
        n1 = 0;
        for (int c = 0; c < 256; c++) if (!esta[c]) s1[n1++] = (unsigned char)c;
    }
    if (del) {
        for (int i = 0; i < n1; i++) t->borra[s1[i]] = true;
        if (sq) {
            if (!c2) { copia_texto(err, "con -d y -s hace falta un segundo conjunto", (size_t)emax); return -1; }
            for (int i = 0; i < n2; i++) t->aprieta[s2[i]] = true;
        }
        return 0;
    }
    if (!c2) {
        if (!sq) { copia_texto(err, "falta el segundo conjunto (tr C1 C2)", (size_t)emax); return -1; }
        for (int i = 0; i < n1; i++) t->aprieta[s1[i]] = true;
        return 0;
    }
    if (n2 == 0) { copia_texto(err, "el segundo conjunto está vacío", (size_t)emax); return -1; }
    for (int i = 0; i < n1; i++) {
        unsigned char d = i < n2 ? s2[i] : s2[n2 - 1];
        t->mapa[s1[i]] = d;
    }
    if (sq) for (int i = 0; i < n2; i++) t->aprieta[s2[i]] = true;
    return 0;
}

/* Procesa un byte: devuelve el byte que sale, o -1 si no sale nada. */
static int tr_byte(tr_t *t, int c)
{
    if (t->borra[c]) return -1;
    int d = t->mapa[c];
    if (t->aprieta[d] && t->ultimo == d) return -1;
    t->ultimo = d;
    return d;
}

#endif
