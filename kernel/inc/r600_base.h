// SPDX-License-Identifier: GPL-2.0-or-later
/* r600_base.h - lo que comparten el 2D por DMA (acel_r600.c, D223) y el 3D por el
 * CP (gpu3d_r600.c, D284) de las Radeon R6xx/R7xx: la sonda de solo lectura de la
 * tarjeta (modelo exacto, BAR, MMIO, dónde está la pantalla en el controlador de
 * memoria, qué controlador de pantalla la muestra). */
#ifndef SAGUI_R600_BASE_H
#define SAGUI_R600_BASE_H
#include "acel2d.h"

typedef struct {
    volatile uint8_t *mmio;       /* ventana de 64 KB del BAR 2 */
    int         gen;              /* 6 = R6xx, 7 = R7xx */
    const char *nome;             /* "RV710 (M92)"... */
    uint16_t    modelo;
    bool        agp;
    uint32_t    bar0;             /* base física de la apertura de la CPU */
    uint32_t    desloc;           /* de la pantalla dentro del BAR 0 */
    uint32_t    mc_tela;          /* la pantalla en el controlador de memoria de la GPU (32 bits bajos) */
    uint8_t     mc_alto;          /* D408: bits 39:32 de mc_tela (0 por debajo de 4 GB; la HD 7600M del Toshiba: 0x0F) */
    uint64_t    vram_ini, vram_fim;   /* MC_VM_FB_LOCATION */
    uint32_t    fbloc, hdp, vm0;
    uint32_t    memsize, aper;    /* CONFIG_MEMSIZE, CONFIG_APER_SIZE */
    int         controlador;      /* 0 = D1, 1 = D2 */
} r600_base_t;

/* No escribe ningún registro de la GPU (solo, si hiciera falta, el bit de memoria
 * del PCI). false: no es un modelo de la lista o algo no cuadra (motivo en m). */
bool r600_sondar(const pci_disp_t *d, int geracao, const acel_tela_t *t, r600_base_t *b, char *m, int nm);
/* D402: VRAM real de la tarjeta (CONFIG_MEMSIZE, en bytes) tras la primera sonda; 0 si no se ha
 * sondado. Para la información del sistema (la del VBE solo cuenta lo que el VBE mapea). */
uint64_t r600_vram_real_bytes(void);
/* D406: la memoria virtual de la GPU (contexto 0) está encendida por la GART del 3D (gpu3d_r600.c): la
 * sonda no la toma por «de otro». */
bool gpu3d_r600_gart_nossa(void);

/* Bytes, desde el principio de la pantalla, que puede llegar a usar el driver 2D
 * (pantalla, zona de prueba, anillo del DMA, página de control, fila del color y
 * zona de paso): el 3D pone lo suyo detrás. */
uint32_t r600_fim_2d(const acel_tela_t *t);

/* Vacía la caché de escritura del HDP: lo que la CPU escribió por el BAR 0 llega a
 * la VRAM antes de que la GPU lo lea. «leitura» es una palabra cualquiera de la VRAM
 * vista por la CPU. */
void r600_hdp_vaciar(const r600_base_t *b, volatile uint32_t *leitura);

#endif
