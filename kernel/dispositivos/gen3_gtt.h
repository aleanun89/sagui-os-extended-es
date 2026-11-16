// SPDX-License-Identifier: GPL-2.0-or-later
/* gen3_gtt.h - RAM del sistema en el GTT de Intel Gen3 (915, 945, G33, Pineview), D358.
 *
 * Lógica pura (sin registros ni memoria): se compila igual en el kernel y en la
 * prueba del anfitrión (tests/gen3_gtt_unidad.c). gpu3d_intel.c la usa para:
 *   - saber cuánta memoria «robada» tiene la gráfica (GMCH_CTRL del puente 0:0.0);
 *   - elegir una ventana de direcciones gráficas libre por encima de la robada y
 *     de lo que anunció el VBE, dentro de los primeros 128 MB (la apertura más
 *     pequeña de Gen3: así la ventana existe en el GTT de cualquier modelo);
 *   - comprobar, leyendo, que esa ventana no la usa nadie: todas sus entradas
 *     iguales (inválidas o apuntando a la misma página de relleno de la BIOS);
 *   - componer las entradas nuevas (dirección física + válida).
 *
 * Fuentes (solo valores; código propio, docs/terceros.md): formato de la entrada
 * del GTT y códigos GMS de GMCH_CTRL de la documentación de Intel de los 915/945/
 * G33 y de las cabeceras de Linux (drivers/char/agp/intel-agp.h,
 * include/drm/intel-gtt.h, v6.6), de donde solo se toman números. */
#ifndef SAGUI_GEN3_GTT_H
#define SAGUI_GEN3_GTT_H
#include "tipos.h"

#define G3_GTT_VALIDA        1u
#define G3_GTT_LIMITE        (128u * 1024 * 1024)   /* la ventana termina por debajo de esto */
#define G3_GTT_GUARDA        (1u * 1024 * 1024)     /* hueco entre lo usado y la ventana */
#define G3_GMCH_CTRL         0x52                   /* en el puente 0:0.0, 16 bits */
#define G3_IFP_915           0x60                   /* página de vaciado del chipset (915/945) */
#define G3_IFP_G33           0x70                   /* ... y G33, Q35, Q33, Pineview (con 0x74: parte alta) */
#define G3_PCI_GTTADR        0x1C                   /* BAR 3 de la gráfica: ventana de la CPU sobre el GTT */

/* Los modelos con el GTT y la página de vaciado «de G33». */
static inline bool g3_gtt_tipo_g33(uint16_t modelo)
{
    return modelo == 0x29B2 || modelo == 0x29C2 || modelo == 0x29D2 || modelo == 0xA001 || modelo == 0xA011;
}

/* Memoria robada que dice GMCH_CTRL (bits 7:4). 0xFFFFFFFF: código que no se
 * conoce (quien llama no usa entonces la RAM en el GTT). */
static inline uint32_t g3_gtt_robada(uint16_t gmch_ctrl, bool g33)
{
    static const uint16_t mb[16] = { 0, 1, 4, 8, 16, 32, 48, 64, 128, 256, 96, 160, 224, 352, 0xFFFF, 0xFFFF };
    uint32_t c = (gmch_ctrl >> 4) & 0xF;
    if (!g33 && c > 7) return 0xFFFFFFFFu;   /* 915/945: como mucho 64 MB */
    if (mb[c] == 0xFFFF) return 0xFFFFFFFFu;
    return (uint32_t)mb[c] * 1024u * 1024u;
}

/* La ventana: los «bytes» de arriba de los primeros 128 MB. 0 si no cabe por
 * encima de lo usado (robada, memoria del VBE y una guarda de 1 MB). */
static inline uint32_t g3_gtt_janela(uint32_t robada, uint32_t vbe, uint32_t bytes)
{
    if (robada == 0xFFFFFFFFu || !bytes || (bytes & 0xFFFFFu) || bytes > G3_GTT_LIMITE) return 0;
    uint32_t usado = robada > vbe ? robada : vbe;
    if (usado > G3_GTT_LIMITE) return 0;
    usado = (usado + 0xFFFFFu) & ~0xFFFFFu;
    if (usado + G3_GTT_GUARDA + bytes > G3_GTT_LIMITE) return 0;
    return G3_GTT_LIMITE - bytes;
}

/* ¿Está libre la ventana? Todas las entradas iguales. Devuelve n si lo está, o
 * el índice de la primera distinta. */
static inline uint32_t g3_gtt_livre(const volatile uint32_t *pte, uint32_t n)
{
    for (uint32_t i = 1; i < n; i++)
        if (pte[i] != pte[0]) return i;
    return n;
}

/* La entrada de una página física (por debajo de 64 GB): bits 31:12 de la
 * dirección, bits 35:32 en 7:4 (G33 y Pineview; en 915/945 son 0) y válida. */
static inline uint32_t g3_gtt_entrada(uint64_t fis)
{
    return ((uint32_t)fis & 0xFFFFF000u) | ((uint32_t)(fis >> 28) & 0xF0u) | G3_GTT_VALIDA;
}

/* El tamaño de la zona de RAM según las páginas libres del kernel (de 4 KB):
 * 16 MB con 64 MB libres o más, 8 MB con 32 MB o más; si no, 0 (no se usa). */
static inline uint32_t g3_gtt_tamanho(uint32_t paginas_livres)
{
    if (paginas_livres >= 16384) return 16u * 1024 * 1024;
    if (paginas_livres >= 8192) return 8u * 1024 * 1024;
    return 0;
}

#endif
