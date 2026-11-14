// SPDX-License-Identifier: GPL-2.0-or-later
/* base64_logica.h - codificación y decodificación Base64 (RFC 4648) para el comando base64 (D232). */
#ifndef BASE64_LOGICA_H
#define BASE64_LOGICA_H
#include "sagui.h"

static const char b64_tabla[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/* Codifica n bytes; escribe el texto (con cero final) y devuelve su largo, o -1 si no cabe. */
static int b64_codificar(const uint8_t *in, int n, char *out, int max)
{
    int o = 0;
    if ((n + 2) / 3 * 4 + 1 > max) return -1;
    for (int i = 0; i < n; i += 3) {
        uint32_t v = (uint32_t)in[i] << 16;
        if (i + 1 < n) v |= (uint32_t)in[i + 1] << 8;
        if (i + 2 < n) v |= in[i + 2];
        out[o++] = b64_tabla[(v >> 18) & 63];
        out[o++] = b64_tabla[(v >> 12) & 63];
        out[o++] = i + 1 < n ? b64_tabla[(v >> 6) & 63] : '=';
        out[o++] = i + 2 < n ? b64_tabla[v & 63] : '=';
    }
    out[o] = 0;
    return o;
}

static int b64_valor(int c)
{
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

/* Decodifica; los saltos de línea se ignoran siempre y los demás caracteres ajenos, solo con ignorar.
 * Devuelve los bytes escritos o -1 si el texto no es Base64 válido. */
static int b64_decodificar(const char *in, int n, uint8_t *out, int max, bool ignorar)
{
    uint32_t acc = 0;
    int bits = 0, o = 0;
    for (int i = 0; i < n; i++) {
        int c = (unsigned char)in[i];
        if (c == '\n' || c == '\r') continue;
        if (c == '=') break;
        int v = b64_valor(c);
        if (v < 0) { if (ignorar) continue; return -1; }
        acc = (acc << 6) | (uint32_t)v;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            if (o >= max) return -1;
            out[o++] = (uint8_t)(acc >> bits);
        }
    }
    return o;
}

#endif
