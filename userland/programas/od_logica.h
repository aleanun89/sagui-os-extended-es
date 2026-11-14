// SPDX-License-Identifier: GPL-2.0-or-later
/* od_logica.h - las líneas de od, hexdump -C y xxd (D232): dirección, valores y, en los volcados, el texto. */
#ifndef OD_LOGICA_H
#define OD_LOGICA_H
#include "sagui.h"

/* formatar_texto no tiene %o: octal con relleno de ceros a ancho w */
static void od_octal(char *b, size_t max, uint32_t v, int w, bool ceros)
{
    char t[16];
    int n = 0;
    do { t[n++] = (char)('0' + (v & 7)); v >>= 3; } while (v && n < 15);
    int o = 0;
    for (int i = n; i < w && (size_t)o + 1 < max; i++) b[o++] = ceros ? '0' : ' ';
    while (n && (size_t)o + 1 < max) b[o++] = t[--n];
    b[o] = 0;
}

static void od_direccion(char *b, size_t max, uint32_t off, int base)
{
    if (base == 16) formatar_texto(b, max, "%06x", off);
    else if (base == 10) formatar_texto(b, max, "%07u", off);
    else if (base == 0) b[0] = 0;
    else od_octal(b, max, off, 7, true);
}

/* "hexdump -C": 16 bytes por línea con el texto entre barras. */
static void od_linea_hex_c(char *b, size_t max, uint32_t off, const uint8_t *d, int n)
{
    int o = formatar_texto(b, max, "%08x  ", off);
    for (int i = 0; i < 16; i++) {
        if (i < n) o += formatar_texto(b + o, max - (size_t)o, "%02x ", d[i]);
        else o += formatar_texto(b + o, max - (size_t)o, "   ");
        if (i == 7) o += formatar_texto(b + o, max - (size_t)o, " ");
    }
    o += formatar_texto(b + o, max - (size_t)o, " |");
    for (int i = 0; i < n; i++) b[o++] = (d[i] >= 32 && d[i] < 127) ? (char)d[i] : '.';
    b[o++] = '|';
    b[o] = 0;
}

/* "xxd": grupos de 2 bytes, 16 por línea, y el texto. */
static void od_linea_xxd(char *b, size_t max, uint32_t off, const uint8_t *d, int n)
{
    int o = formatar_texto(b, max, "%08x: ", off);
    for (int i = 0; i < 16; i++) {
        if (i < n) o += formatar_texto(b + o, max - (size_t)o, "%02x", d[i]);
        else o += formatar_texto(b + o, max - (size_t)o, "  ");
        if (i & 1) o += formatar_texto(b + o, max - (size_t)o, " ");
    }
    o += formatar_texto(b + o, max - (size_t)o, " ");
    for (int i = 0; i < n; i++) b[o++] = (d[i] >= 32 && d[i] < 127) ? (char)d[i] : '.';
    b[o] = 0;
}

static int od_ancho(char fmt, int tam)
{
    switch (fmt) {
    case 'o': return tam == 1 ? 3 : tam == 2 ? 6 : 11;
    case 'x': return tam * 2;
    case 'd': return tam == 1 ? 4 : tam == 2 ? 6 : 11;
    case 'u': return tam == 1 ? 3 : tam == 2 ? 5 : 10;
    default: return 3;   /* 'c' */
    }
}

static void od_caracter(char *s, int c)
{
    switch (c) {
    case 0: copia_texto(s, "\\0", 4); break;
    case 7: copia_texto(s, "\\a", 4); break;
    case 8: copia_texto(s, "\\b", 4); break;
    case 12: copia_texto(s, "\\f", 4); break;
    case 10: copia_texto(s, "\\n", 4); break;
    case 13: copia_texto(s, "\\r", 4); break;
    case 9: copia_texto(s, "\\t", 4); break;
    case 11: copia_texto(s, "\\v", 4); break;
    default:
        if (c >= 32 && c < 127) { s[0] = (char)c; s[1] = 0; }
        else od_octal(s, 4, (uint32_t)c, 3, true);
    }
}

/* Una línea de od: la dirección (base 8, 10, 16 o 0 = sin ella) y los valores de tamaño tam (1, 2 o 4 bytes). */
static void od_linea_od(char *b, size_t max, uint32_t off, const uint8_t *d, int n, char fmt, int tam, int base)
{
    od_direccion(b, max, off, base);
    int o = (int)tam_texto(b);
    if (fmt == 'c') tam = 1;
    int w = od_ancho(fmt, tam);
    for (int i = 0; i < n; i += tam) {
        uint32_t v = 0;
        for (int k = tam - 1; k >= 0; k--) v = (v << 8) | (i + k < n ? d[i + k] : 0);
        char t[24];
        if (fmt == 'c') od_caracter(t, (int)v);
        else if (fmt == 'x') formatar_texto(t, sizeof t, "%0*x", w, v);
        else if (fmt == 'o') od_octal(t, sizeof t, v, w, true);
        else if (fmt == 'u') formatar_texto(t, sizeof t, "%u", v);
        else {
            int32_t s = tam == 1 ? (int8_t)v : tam == 2 ? (int16_t)v : (int32_t)v;
            formatar_texto(t, sizeof t, "%d", s);
        }
        o += formatar_texto(b + o, max - (size_t)o, " %*s", w, t);
    }
}

#endif
