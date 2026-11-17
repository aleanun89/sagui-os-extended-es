// SPDX-License-Identifier: GPL-2.0-or-later
/* disco_ram.c - un disco en la RAM con la copia de un volumen ("ram0"; D379, fase T1: toram).
 *
 * En el modo «todo a RAM» (el CD por defecto, o una live que lo pida) la raíz SAGUI-RAIZ
 * del medio se copia entera, bloque a bloque, a páginas de memoria (la alta si la hay,
 * D348), y se monta encima el mismo ext2 de siempre: un solo formato en los tres modos y
 * el medio se puede quitar. Lo que se escribe se queda en la RAM (se pierde al apagar).
 *
 * El disco no pasa por la caché de bloques (sería tener dos veces lo mismo en la RAM) y
 * no se puede crear si no cabe dejando una reserva para el sistema: entonces raiz.c
 * avisa y arranca leyendo del medio (live), nunca se queda sin memoria a medias. */
#include "disco_ram.h"
#include "abi.h"
#include "console.h"
#include "memalta.h"
#include "memoria.h"
#include "relogio.h"
#include "texto.h"

#define PAG 4096u
#define SET_POR_PAG (PAG / 512u)
#define RESERVA_PAGINAS (32u * 256u)   /* 32 MB que siempre quedan para el sistema y los programas */

static uint64_t *paginas;
static uint32_t n_paginas;

static int r_ler(disco_t *d, uint32_t lba, uint32_t n, void *buf)
{
    (void)d;
    uint8_t *b = buf;
    for (uint32_t i = 0; i < n; i++, lba++) {
        uint32_t p = lba / SET_POR_PAG;
        if (p >= n_paginas) return -1;
        fisica_ler(paginas[p] + (lba % SET_POR_PAG) * 512u, b + i * 512u, 512);
    }
    return 0;
}

static int r_escrever(disco_t *d, uint32_t lba, uint32_t n, const void *buf)
{
    (void)d;
    const uint8_t *b = buf;
    for (uint32_t i = 0; i < n; i++, lba++) {
        uint32_t p = lba / SET_POR_PAG;
        if (p >= n_paginas) return -1;
        fisica_escrever(paginas[p] + (lba % SET_POR_PAG) * 512u, b + i * 512u, 512);
    }
    return 0;
}

static void soltar(uint32_t ate)
{
    for (uint32_t i = 0; i < ate; i++) memfisica_liberar(paginas[i]);
    kfree(paginas);
    paginas = NULL;
    n_paginas = 0;
}

disco_t *disco_ram_copiar(disco_t *origem, int *erro)
{
    *erro = 0;
    if (paginas) { *erro = ERRO_OCUPADO; return NULL; }
    uint32_t setores = origem->setores;
    uint32_t n = (setores + SET_POR_PAG - 1) / SET_POR_PAG;
    uint32_t livres = memfisica_livres() + memfisica_livres_alta();
    if (!n || livres < n + RESERVA_PAGINAS) {
        klog("[RAM0] %s: hacen falta %u MB y hay %u MB libres (con %u MB de reserva): no se copia\n", origem->nome,
             n / 256, livres / 256, RESERVA_PAGINAS / 256);
        *erro = ERRO_SEM_MEMORIA;
        return NULL;
    }
    paginas = kcalloc(n * sizeof *paginas);
    uint8_t *buf = kmalloc(64 * 1024);
    if (!paginas || !buf) {
        if (buf) kfree(buf);
        if (paginas) kfree(paginas);
        paginas = NULL;
        *erro = ERRO_SEM_MEMORIA;
        return NULL;
    }
    for (uint32_t i = 0; i < n; i++) {
        paginas[i] = memfisica_alocar_usuario();
        if (!paginas[i]) { soltar(i); kfree(buf); *erro = ERRO_SEM_MEMORIA; return NULL; }
    }
    n_paginas = n;
    /* La copia, de 64 KB en 64 KB (lectura secuencial: el CD y el pendrive van a su velocidad). */
    uint32_t t0 = relogio_tiques();
    for (uint32_t lba = 0; lba < setores; lba += 128) {
        uint32_t k = setores - lba < 128 ? setores - lba : 128;
        int r = disco_ler(origem, lba, k, buf);
        if (r < 0) {
            klog("[RAM0] error al leer %s en el sector %u (%d): no se copia\n", origem->nome, lba, r);
            soltar(n);
            kfree(buf);
            *erro = r;
            return NULL;
        }
        r_escrever(NULL, lba, k, buf);
    }
    kfree(buf);
    uint32_t ms = (relogio_tiques() - t0) * (1000u / TIQUES_POR_SEGUNDO);
    klog("[RAM0] %s copiado a la RAM: %u MB en %u ms\n", origem->nome, n / 256, ms);
    disco_t *d = disco_registrar("ram", "SaguiOS", "Disco en RAM", setores, 512, NULL, false, r_ler, r_escrever);
    if (!d) { soltar(n); *erro = ERRO_LIMITE; return NULL; }
    return d;
}
