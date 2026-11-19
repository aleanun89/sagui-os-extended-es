// SPDX-License-Identifier: GPL-2.0-or-later
/* vram_zona_logica.h - dónde poner una zona de trabajo de la GPU (cola de órdenes,
 * búferes del 3D) en la memoria de vídeo sin pisar nada de lo que muestra la
 * tarjeta (D386). La parte pura, sin acceso al hardware, para probarla en el
 * anfitrión (tests/vram_zona_unidad.c, tests/h287_gpu3d_r500.py) con el mismo
 * código que compila el kernel.
 *
 * Por qué: el Amilo (Mobility X1400, RV515) arranca con DOS controladores AVIVO
 * con la imagen activa: D1 con la pantalla del VBE (MC c0000000, 1024x768) y D2,
 * que deja la BIOS, en c0100000 con pitch 1280. La zona 3D iba siempre justo
 * detrás de la pantalla de D1 (c0400000) y, como D2 caía cerca, el driver se
 * negaba a seguir aunque quedaban 120 MB libres. Ahora se apuntan todas las
 * regiones ocupadas (superficies de D1 y D2 con su tamaño real o un margen
 * conservador, cursores) y la zona va al primer hueco libre detrás de todas. */
#ifndef SAGUI_VRAM_ZONA_LOGICA_H
#define SAGUI_VRAM_ZONA_LOGICA_H
#ifdef SAGUI_KERNEL
#include "tipos.h"
#else
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#endif

/* Una región ocupada de la VRAM, en bytes desde su principio: [ini, fim). */
typedef struct {
    uint64_t ini, fim;
} vram_ocupado_t;

/* Margen cuando no se sabe cuánto mide una superficie (registros a cero o raros). */
#define VRAM_MARGEM_DESCONHECIDA (8u << 20)

/* Bytes que recorre una superficie AVIVO (R5xx/R6xx/R7xx) al mostrarse:
 *   controle   D1GRPH_CONTROL (bits 1:0 profundidad: 0 = 8, 1 = 16, 2 = 32, 3 = 64 bits);
 *   pitch_px   D1GRPH_PITCH, en píxeles;
 *   y_fim      D1GRPH_Y_END (línea final de la superficie);
 *   desl_y     D1GRPH_SURFACE_OFFSET_Y;
 *   vp_ini     D1MODE_VIEWPORT_START (x << 16 | y), vp_tam D1MODE_VIEWPORT_SIZE (ancho << 16 | alto).
 * Las líneas son las mayores de Y_END y de lo que barre la ventana visible (como lo
 * programa avivo_crtc_do_set_base de Linux, drivers/gpu/drm/radeon/atombios_crtc.c).
 * Si nada de eso tiene sentido (cero o más de 8192 líneas, pitch cero o de más
 * de 8192), devuelve el margen conservador de 8 MB. */
static inline uint64_t avivo_superficie_bytes(uint32_t controle, uint32_t pitch_px, uint32_t y_fim, uint32_t desl_y,
                                              uint32_t vp_ini, uint32_t vp_tam)
{
    uint32_t bpp = 1u << (controle & 3u);
    uint32_t linhas = y_fim & 0x3FFFu;
    uint32_t vp = (desl_y & 0x3FFFu) + (vp_ini & 0x3FFFu) + (vp_tam & 0x3FFFu);
    if (vp > linhas) linhas = vp;
    pitch_px &= 0x3FFFu;
    if (linhas == 0 || linhas > 8192 || pitch_px == 0 || pitch_px > 8192) return VRAM_MARGEM_DESCONHECIDA;
    return (uint64_t)pitch_px * bpp * linhas;
}

static inline uint64_t vram_alinhar(uint64_t v, uint32_t alin) { return (v + alin - 1) & ~(uint64_t)(alin - 1); }

/* El primer hueco de al menos 'min' bytes desde 'desde' (alineado a 'alin', potencia
 * de 2) que no toca ninguna región ocupada y acaba antes de 'lim'. Su tamaño llega
 * hasta la siguiente región ocupada o 'lim', como mucho 'max', y es múltiplo de 'alin'.
 * Devuelve false si no hay. */
static inline bool vram_zona_buscar(const vram_ocupado_t *oc, int n, uint64_t desde, uint64_t lim, uint32_t min,
                                    uint32_t max, uint32_t alin, uint64_t *ini, uint32_t *tam)
{
    uint64_t c = vram_alinhar(desde, alin);
    /* Cada pasada salta como mucho detrás de una región: con n regiones, n + 1 pasadas bastan. */
    for (int pasada = 0; pasada <= n; pasada++) {
        bool mudou = false;
        for (int i = 0; i < n; i++) {
            if (oc[i].fim <= oc[i].ini) continue;
            if (oc[i].ini < c + min && oc[i].fim > c) {
                c = vram_alinhar(oc[i].fim, alin);
                mudou = true;
            }
        }
        if (!mudou) break;
        if (pasada == n) return false;
    }
    if (c + min > lim) return false;
    uint64_t fim = lim;
    if (fim > c + max) fim = c + max;
    for (int i = 0; i < n; i++)
        if (oc[i].fim > oc[i].ini && oc[i].ini >= c + min && oc[i].ini < fim) fim = oc[i].ini;
    uint64_t t = (fim - c) & ~(uint64_t)(alin - 1);
    if (t < min) return false;
    *ini = c;
    *tam = (uint32_t)t;
    return true;
}

/* ¿Pisa la zona [z, z + tam) alguna región ocupada? (la comprobación final, aparte). */
static inline int vram_zona_pisa(const vram_ocupado_t *oc, int n, uint64_t z, uint64_t tam)
{
    for (int i = 0; i < n; i++)
        if (oc[i].fim > oc[i].ini && oc[i].ini < z + tam && oc[i].fim > z) return i;
    return -1;
}

/* D387 (cuarta tanda, 2026-11-19): el tamaño de la zona 3D de las R5xx (gpu3d_r500.c). Antes, como mucho
 * 16 MB; ahora todo el primer hueco libre detrás de lo que la tarjeta muestra, hasta el final de la VRAM útil
 * MENOS R5_ZONA3D_COLA: en esos últimos 8 MB van la tabla de la GART del volcado (16 KB, acel_radeon.c, que
 * busca su sitio detrás de la zona con esta misma función), el destino de los experimentos del volcado
 * (una pantalla) y el margen para lo que el firmware ATOM pueda usar al final de la VRAM. En el Amilo (128 MB,
 * pantalla y D2 hasta los 5 MB): zona de 115 MB, de 5 a 120 MB. Las dos partes (3D y GART) la llaman con los
 * mismos datos, así que ven la misma zona. */
#define R5_ZONA3D_MIN   (2u << 20)
#define R5_ZONA3D_COLA  (8u << 20)
static inline bool r5_zona3d_buscar(const vram_ocupado_t *oc, int n, uint64_t desde, uint64_t vram, uint64_t *ini, uint32_t *tam)
{
    if (vram <= R5_ZONA3D_COLA) return false;
    uint64_t lim = vram - R5_ZONA3D_COLA;
    uint64_t max = lim > 0xFFF00000ull ? 0xFFF00000ull : lim;
    return vram_zona_buscar(oc, n, desde, lim, R5_ZONA3D_MIN, (uint32_t)max, 1u << 20, ini, tam);
}

/* Amilo (2026-11-19): dónde va la tabla de la GART PCIE (acel_radeon.c): «bytes» alineados a 4 KB detrás de
 * TODAS las regiones ocupadas (las vacías no cuentan). En 'ini', bytes desde el principio de la VRAM. (La
 * primera versión buscaba con alineación de 64 KB y como mucho 16 KB: el tamaño, redondeado a 64 KB, salía 0
 * y nunca cabía.) */
static inline bool vram_tabela_gart(const vram_ocupado_t *oc, int n, uint64_t lim, uint32_t bytes, uint64_t *ini)
{
    uint64_t desde = 0;
    uint32_t tam;
    for (int i = 0; i < n; i++)
        if (oc[i].fim > oc[i].ini && oc[i].fim > desde) desde = oc[i].fim;
    if (!vram_zona_buscar(oc, n, desde, lim, bytes, bytes, 0x1000u, ini, &tam)) return false;
    return vram_zona_pisa(oc, n, *ini, bytes) < 0;
}

#endif
