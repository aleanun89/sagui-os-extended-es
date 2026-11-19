// SPDX-License-Identifier: GPL-2.0-or-later
/* volcado_logica.h - la parte pura del volcado del búfer de atrás a la pantalla por la GPU
 * (D387): entradas de la GART PCIE de las Radeon R300-R5xx, dónde poner su ventana en el mapa
 * del controlador de memoria (MC) y el troceo de un rectángulo en franjas para el motor 2D.
 * Sin acceso al hardware: lo prueba el PC (tests/volcado_unidad.c, h394 unidad) con el mismo
 * código que compila el kernel.
 *
 * Cómo se usa (acel_radeon.c): el búfer de atrás (RAM del sistema) se apunta página a página en
 * la GART; la GPU lo ve a partir de «gtt_base» en su mapa (MC). El motor 2D copia de ahí a la
 * pantalla con SRC_PITCH_OFFSET = (paso / 64) << 22 | base >> 10: la base tiene que ir alineada a
 * 1 KB y el paso ser múltiplo de 64 bytes. El búfer no empieza en el principio de una página (el
 * kmalloc deja delante su cabecera): cada franja toma como base la dirección de su primera fila
 * bajada a 1 KB y suma lo que sobra a la x del origen (como mucho 255 píxeles).
 *
 * Referencia de los valores (solo números; el código es propio): Linux 6.6,
 * drivers/gpu/drm/radeon/r300.c (rv370_pcie_gart_get_page_entry, rv370_pcie_gart_enable) y
 * radeon_device.c (radeon_gtt_location). */
#ifndef SAGUI_VOLCADO_LOGICA_H
#define SAGUI_VOLCADO_LOGICA_H
#ifdef SAGUI_KERNEL
#include "tipos.h"
#else
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#endif

/* ------------------------------------------------------------ entradas de la GART */

/* Una entrada de 32 bits por página de 4 KB: bits 31:24 = bits 39:32 de la dirección física,
 * bits 23:4 = bits 31:12, y abajo las banderas. Sin SEM_SNOOP la lectura pasa por la caché de la
 * CPU (el puente la «fisgonea»): no hace falta vaciar nada antes de que la GPU lea el búfer. */
#define GART_R300_SEM_SNOOP  0x1u
#define GART_R300_ESCRITA    0x4u
#define GART_R300_LEITURA    0x8u
#define GART_R300_FIS_MAX    ((1ull << 40) - 1)   /* 40 bits: con PAE (36 bits) siempre cabe */

/* false si la dirección no es de una página o no cabe en 40 bits. */
static inline bool gart_r300_pte(uint64_t fis, uint32_t bandeiras, uint32_t *pte)
{
    if ((fis & 0xFFFu) || fis > GART_R300_FIS_MAX) return false;
    *pte = ((uint32_t)fis >> 8) | ((uint32_t)((fis >> 32) & 0xFFu) << 24) | (bandeiras & 0xFu);
    return true;
}

/* La dirección física de una entrada (para comprobarla al leerla de vuelta y en la prueba). */
static inline uint64_t gart_r300_fis(uint32_t pte)
{
    return ((uint64_t)(pte & 0x00FFFFF0u) << 8) | ((uint64_t)(pte >> 24) << 32);
}

/* ------------------------------------------------------------ la ventana en el MC */

/* Rango [ini, fim] de la VRAM en el MC a partir de MC_FB_LOCATION (bits 15:0 inicio >> 16,
 * 31:16 último >> 16). false si no tiene sentido. */
static inline bool mc_rango(uint32_t loc, uint32_t *ini, uint32_t *fim)
{
    uint32_t a = (loc & 0xFFFFu) << 16, b = (loc & 0xFFFF0000u) | 0xFFFFu;
    if (loc == 0xFFFFFFFFu || b <= a) return false;
    *ini = a; *fim = b;
    return true;
}

static inline bool rangos_pisan(uint64_t a0, uint64_t a1, uint64_t b0, uint64_t b1)   /* [a0,a1] y [b0,b1] */
{
    return a0 <= b1 && b0 <= a1;
}

/* Dónde va la ventana de la GART (tam bytes, potencia de 2) en el mapa del MC: como Linux
 * (radeon_gtt_location), delante de la VRAM si hay más sitio delante que detrás y si no detrás,
 * alineada a su tamaño. No puede pisar la VRAM ni la ventana AGP (MC_AGP_LOCATION, si está
 * encendida: inicio <= fin). Si el lado elegido pisa la AGP se prueba el otro. */
static inline bool gart_r300_janela(uint32_t fb_ini, uint32_t fb_fim, uint32_t agp_loc, uint32_t tam, uint32_t *ini)
{
    uint64_t t = tam, cand[2];
    int n = 0;
    uint64_t antes = ((uint64_t)fb_ini & ~(t - 1));
    uint64_t depois = (((uint64_t)fb_fim + 1 + t - 1) & ~(t - 1));
    bool antes_ok = antes >= t;
    bool depois_ok = depois + t - 1 <= 0xFFFFFFFFull;
    uint64_t sitio_antes = fb_ini, sitio_depois = 0xFFFFFFFFull - fb_fim;
    if (sitio_antes > sitio_depois) {
        if (antes_ok) cand[n++] = antes - t;
        if (depois_ok) cand[n++] = depois;
    } else {
        if (depois_ok) cand[n++] = depois;
        if (antes_ok) cand[n++] = antes - t;
    }
    uint32_t agp_ini = (agp_loc & 0xFFFFu) << 16, agp_fim = (agp_loc & 0xFFFF0000u) | 0xFFFFu;
    bool agp = agp_ini <= agp_fim;
    for (int i = 0; i < n; i++) {
        if (rangos_pisan(cand[i], cand[i] + t - 1, fb_ini, fb_fim)) continue;
        if (agp && rangos_pisan(cand[i], cand[i] + t - 1, agp_ini, agp_fim)) continue;
        *ini = (uint32_t)cand[i];
        return true;
    }
    return false;
}

/* ------------------------------------------------------------ el búfer y las franjas */

/* El búfer de origen tal como lo ve la GPU. */
typedef struct {
    uint32_t gtt_base;   /* dirección en el MC de la página donde empieza el búfer */
    uint32_t desl;       /* bytes desde el principio de esa página (múltiplo de 4) */
    uint32_t passo;      /* bytes por fila (múltiplo de 64, como mucho 255 * 64) */
    uint32_t larg, alt;  /* píxeles del búfer */
} volcado_fonte_t;

/* Páginas de la GART que ocupa un búfer. */
static inline uint32_t volcado_paginas(uint32_t desl, uint32_t passo, uint32_t alt)
{
    uint64_t b = (uint64_t)desl + (uint64_t)passo * alt;
    return (uint32_t)((b + 4095u) >> 12);
}

/* ¿Sirve el búfer para el motor 2D de las R5xx? (motivo en *porque si no) */
static inline bool volcado_fonte_ok(const volcado_fonte_t *f, const char **porque)
{
    if (f->desl & 3u || f->desl > 4095u) { *porque = "el bufer no empieza en un pixel"; return false; }
    if (f->passo & 63u) { *porque = "el ancho del escritorio en bytes no es multiplo de 64"; return false; }
    if (f->passo / 64u > 255u || f->passo == 0) { *porque = "el ancho del escritorio no cabe en el motor 2D"; return false; }
    if (f->passo < f->larg * 4u) { *porque = "el paso es menor que la fila"; return false; }
    if (f->larg + 255u > 8191u || f->alt > 8191u || f->alt == 0) { *porque = "el escritorio es demasiado grande"; return false; }
    if (f->gtt_base & 0xFFFu) { *porque = "la ventana de la GART no esta alineada a una pagina"; return false; }
    return true;
}

/* Una franja para el motor 2D: origen (base alineada a 1 KB, x e y desde ella), destino en la
 * pantalla y tamaño. */
typedef struct {
    uint32_t base;        /* dirección en el MC, múltiplo de 1024 */
    uint32_t pitch_offset;  /* SRC_PITCH_OFFSET ya montado */
    int sx, sy, dx, dy, l, a;
} volcado_faixa_t;

/* Trocea el rectángulo (x, y, l, a) del búfer (que va a (x, y) de la pantalla) en franjas de
 * como mucho max_filas filas. Devuelve cuántas (0 si el rectángulo está vacío) o -1 si no cabe en
 * «saida» o el rectángulo se sale del búfer. */
static inline int volcado_trocear(const volcado_fonte_t *f, int x, int y, int l, int a, int max_filas,
                                  volcado_faixa_t *saida, int max_saida)
{
    if (l <= 0 || a <= 0) return 0;
    if (x < 0 || y < 0 || (uint32_t)x + (uint32_t)l > f->larg || (uint32_t)y + (uint32_t)a > f->alt || max_filas <= 0) return -1;
    int n = 0;
    for (int j = 0; j < a; j += max_filas) {
        if (n >= max_saida) return -1;
        int h = a - j < max_filas ? a - j : max_filas;
        uint64_t fila = (uint64_t)f->gtt_base + f->desl + (uint64_t)f->passo * (uint32_t)(y + j);
        uint64_t base = fila & ~0x3FFull;
        if (base + (uint64_t)f->passo * (uint32_t)h + 4096u > 0x100000000ull) return -1;
        volcado_faixa_t *s = &saida[n++];
        s->base = (uint32_t)base;
        s->pitch_offset = ((f->passo / 64u) << 22) | (s->base >> 10);
        s->sx = x + (int)((fila - base) / 4u);
        s->sy = 0;
        s->dx = x; s->dy = y + j;
        s->l = l; s->a = h;
    }
    return n;
}

/* ------------------------------------------------------------ D391: la vuelta (VRAM -> RAM)
 *
 * El cuadro del 3D (en la VRAM) vuelve a la RAM escrito por el motor 2D a través de la GART (región
 * de escritura, solo mientras dura la copia): la CPU no lee la VRAM. El destino es el búfer del
 * programa (la ventana o su memoria), que puede tener cualquier paso: con un paso múltiplo de 64
 * bytes se copia en franjas de varias filas; si no, fila a fila (con una sola fila el paso del
 * motor no importa: se pone el menor múltiplo de 64 que cubre la fila). Cada franja toma como base
 * su primera fila bajada a 1 KB y suma lo que sobra a la x del destino (como mucho 255 píxeles). */
typedef struct {
    uint32_t gtt_base;   /* dirección en el MC de la página donde empieza el destino */
    uint32_t desl;       /* bytes desde el principio de esa página (múltiplo de 4) */
    uint32_t passo;      /* bytes por fila del destino (múltiplo de 4) */
} retorno_destino_t;

#define RETORNO_PASSO_MAX (255u * 64u)   /* el paso más grande que entiende el motor 2D */

/* Filas por franja: max_filas con un paso que el motor entiende; si no, una. */
static inline int retorno_filas(const retorno_destino_t *d, int max_filas)
{
    return (d->passo & 63u) || d->passo > RETORNO_PASSO_MAX ? 1 : max_filas;
}

/* ¿Se puede copiar un rectángulo de l x a píxeles a este destino? (motivo en *porque si no) */
static inline bool retorno_destino_ok(const retorno_destino_t *d, int l, int a, const char **porque)
{
    if (l <= 0 || a <= 0) { *porque = "rectangulo vacio"; return false; }
    if (d->desl & 3u || d->desl > 4095u) { *porque = "el destino no empieza en un pixel"; return false; }
    if (d->passo & 3u || d->passo < (uint32_t)l * 4u) { *porque = "el paso del destino es menor que la fila"; return false; }
    if (d->gtt_base & 0xFFFu) { *porque = "la region de la GART no esta alineada a una pagina"; return false; }
    if ((uint32_t)l + 255u > 8191u || a > 8191) { *porque = "el cuadro es demasiado grande para el motor 2D"; return false; }
    /* Fila a fila, el paso de la orden cubre lo que sobra (hasta 255 píxeles) y la fila. */
    if (retorno_filas(d, 2) == 1 && ((uint32_t)l + 255u) * 4u > RETORNO_PASSO_MAX) { *porque = "la fila no cabe en el motor 2D"; return false; }
    uint64_t fim = (uint64_t)d->gtt_base + d->desl + (uint64_t)d->passo * (uint32_t)(a - 1) + (uint32_t)l * 4u;
    if (fim > 0x100000000ull) { *porque = "el destino se sale del mapa de 32 bits"; return false; }
    return true;
}

/* Bytes (desde el principio de la primera página) que toca la copia: lo que hay que apuntar. */
static inline uint32_t retorno_bytes(const retorno_destino_t *d, int l, int a)
{
    return d->desl + d->passo * (uint32_t)(a - 1) + (uint32_t)l * 4u;
}

/* Cuántas franjas. */
static inline int retorno_n_faixas(const retorno_destino_t *d, int a, int max_filas)
{
    int f = retorno_filas(d, max_filas);
    return a <= 0 ? 0 : (a + f - 1) / f;
}

/* La franja k: destino (base alineada a 1 KB, su DST_PITCH_OFFSET y la x dentro de ella; y = 0)
 * y la fila del origen donde empieza (sy, desde la primera del cuadro). false si no hay franja k. */
static inline bool retorno_faixa(const retorno_destino_t *d, int l, int a, int max_filas, int k, volcado_faixa_t *s)
{
    int f = retorno_filas(d, max_filas);
    int j = k * f;
    if (k < 0 || j >= a || l <= 0) return false;
    int h = a - j < f ? a - j : f;
    uint64_t fila = (uint64_t)d->gtt_base + d->desl + (uint64_t)d->passo * (uint32_t)j;
    uint64_t base = fila & ~0x3FFull;
    uint32_t dx = (uint32_t)(fila - base) / 4u;
    uint32_t passo = f == 1 ? (((dx + (uint32_t)l) * 4u + 63u) & ~63u) : d->passo;
    if (passo > RETORNO_PASSO_MAX || base > 0xFFFFFFFFull) return false;
    s->base = (uint32_t)base;
    s->pitch_offset = ((passo / 64u) << 22) | (s->base >> 10);
    s->sx = 0; s->sy = j;
    s->dx = (int)dx; s->dy = 0;
    s->l = l; s->a = h;
    return true;
}

#endif
