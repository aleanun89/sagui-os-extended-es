/* web.h - a API que o motor da web usa do SaguiOS-Extended (melhoria 081).
 *
 * O motor (motor.c, js.c, canvas2d.c, svg.c, webgl.c, fonte.c, quebra.c) veio do
 * repositorio https://github.com/robson-apk/sagui-os, ramo "cipo-web" (ver
 * userland/web/ORIGEM.md). Esse motor foi escrito contra um sagui.h diferente do
 * nosso: a API de desenho e a de memoria sao as mesmas, mas faltavam as fontes da
 * web, a quebra de linha, o UTF-8 e a API do Canvas 2D e do JavaScript.
 *
 * Este ficheiro junta a nossa sagui.h e a nossa gui.h com essas APIs novas. O
 * codigo do motor fica assim byte a byte como estava la, so mudando o include.
 */
#ifndef SAGUI_WEB_H
#define SAGUI_WEB_H

#include "sagui.h"    /* a nossa: memoria, texto, arquivos, rede */
#include "gui.h"      /* a nossa: janelas e desenho (o motor nao a usa, mas o browser sim) */
#include "imagem.h"   /* a nossa: PNG, JPEG, BMP */

/* ---- fontes (TrueType, OpenType/CFF, WOFF, WOFF2): o motor e a libfontes (D196) ---- */
#include "fontes.h"
/* Fontes do sistema (DejaVu em /fontes): g 0 sans-serif, 1 serif, 2 monospace.
 * Uma vez por processo. */
fonte_t *fonte_sistema(int g, int negrito, int italico);

/* ---- quebra de linha (UAX #14) ----
 * r[i] antes do caractere i: 0 proibido, 1 permitido, 2 obrigatorio. */
void quebra_linha(const uint32_t *cp, int n, uint8_t *r);

/* ---- UTF-8: le um caractere (byte invalido vale por si: cp = byte) e escreve um. ---- */
static inline int utf8_ler(const char *p, int resto, uint32_t *cp)
{
    uint8_t c = (uint8_t)p[0];
    int n = c < 0x80 ? 1 : (c & 0xE0) == 0xC0 ? 2 : (c & 0xF0) == 0xE0 ? 3 : (c & 0xF8) == 0xF0 ? 4 : 0;
    if (!n || n > resto) { *cp = c; return 1; }
    uint32_t v = n == 1 ? c : n == 2 ? (c & 0x1Fu) : n == 3 ? (c & 0x0Fu) : (c & 0x07u);
    for (int k = 1; k < n; k++) {
        if (((uint8_t)p[k] & 0xC0) != 0x80) { *cp = c; return 1; }
        v = v << 6 | ((uint8_t)p[k] & 0x3Fu);
    }
    *cp = v;
    return n;
}

static inline int utf8_por(char *d, uint32_t cp)
{
    if (cp < 0x80) { d[0] = (char)cp; return 1; }
    if (cp < 0x800) { d[0] = (char)(0xC0 | cp >> 6); d[1] = (char)(0x80 | (cp & 63)); return 2; }
    if (cp < 0x10000) {
        d[0] = (char)(0xE0 | cp >> 12);
        d[1] = (char)(0x80 | ((cp >> 6) & 63));
        d[2] = (char)(0x80 | (cp & 63));
        return 3;
    }
    d[0] = (char)(0xF0 | cp >> 18);
    d[1] = (char)(0x80 | ((cp >> 12) & 63));
    d[2] = (char)(0x80 | ((cp >> 6) & 63));
    d[3] = (char)(0x80 | (cp & 63));
    return 4;
}

#endif
