// SPDX-License-Identifier: MIT
/*
 * Extraído de xf86-video-nv (el driver «nv» de X.Org, escrito por NVIDIA):
 * src/nv_dma.h, src/nv_xaa.c (NVResetGraphics, NVDmaWait) y src/nv_hw.c
 * (NVLoadStateExt, la parte de PRAMIN, PGRAPH y PFIFO de la arquitectura
 * NV_ARCH_40), rama master de gitlab.freedesktop.org/xorg/driver/xf86-video-nv
 * (commit 8234bcee52a3, consultado el 2026-11-13).
 *
 * Copyright (c) 1993-2003 NVIDIA, Corporation
 * Copyright (c) 2003 NVIDIA, Corporation
 *
 * Y, solo para elegir qué registros de PGRAPH lleva cada chip (los números de
 * chip de PMC_BOOT_0 y los grupos de nv40_gr_init()), de nouveau
 * (drivers/gpu/drm/nouveau/nvkm/engine/gr/nv40.c de Linux v6.6):
 *
 * Copyright 2012 Red Hat Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * SaguiOS (D224): solo DEFINICIONES y DATOS, sin lógica. Los métodos de los
 * objetos 2D (con su subcanal), las 16 palabras que atan los objetos a los
 * subcanales, los objetos y la tabla hash de PRAMIN, y las escrituras de
 * PFIFO y PGRAPH que hace NVLoadStateExt() en una NV4x/G7x, como tablas
 * {registro, máscara, valor} en el orden del original. Cambios frente al
 * original:
 *   - fuera todo lo que no es el motor 2D: relojes (PRAMDAC), CRTC, PTIMER,
 *     las regiones de tiling y la configuración de PFB (el driver comprueba
 *     que no hay tiling y no lo toca), PMC 0x1700-0x170C (el driver comprueba
 *     que ya vale lo que escribe el original) y PRAMDAC 0x608;
 *   - el reinicio de PMC_ENABLE (0x200) no se copia: el driver reinicia solo
 *     PFIFO y PGRAPH (bits 8 y 12) y deja los demás como estaban;
 *   - los grupos por chip usan el número de PMC_BOOT_0 (como nouveau) y no la
 *     máscara del identificador PCI; donde los dos drivers no coinciden
 *     (C61, C67, C68 y C73) se escriben las dos copias de la configuración de
 *     la memoria (0x4009F0 y 0x400DF0), ver D224;
 *   - el búfer de órdenes no va al final de la memoria de vídeo sino debajo de
 *     la pantalla (lo coloca el driver: NV40_PRAMIN_PB_ENDERECO);
 *   - la C73 (GeForce 7050/7100/7150 con nForce 610i/620i/630i, chip 0x63) no
 *     está en el original: va en el grupo de las C67/C68.
 */
#ifndef SAGUI_TERCEROS_NV_2D_H
#define SAGUI_TERCEROS_NV_2D_H

/* ---------------------------------------------- métodos (nv_dma.h) */
/* El subcanal va en los bits 15:13 del método: 0 SURFACE, 1 ROP, 2 PATTERN,
 * 3 CLIP, 4 LINE, 5 BLIT, 6 RECT, 7 STRETCH. */
#define SURFACE_FORMAT                  0x00000300
#define SURFACE_FORMAT_DEPTH24          0x00000006
#define SURFACE_PITCH                   0x00000304
#define SURFACE_OFFSET_SRC              0x00000308
#define SURFACE_OFFSET_DST              0x0000030C
#define ROP_SET                         0x00002300
#define PATTERN_FORMAT                  0x00004300
#define PATTERN_FORMAT_DEPTH24          0x00000003
#define PATTERN_COLOR_0                 0x00004310
#define LINE_FORMAT                     0x00008300
#define LINE_FORMAT_DEPTH24             0x00000003
#define BLIT_POINT_SRC                  0x0000A300
#define BLIT_POINT_DST                  0x0000A304
#define BLIT_SIZE                       0x0000A308
#define RECT_FORMAT                     0x0000C300
#define RECT_FORMAT_DEPTH24             0x00000003
#define RECT_SOLID_COLOR                0x0000C3FC
#define RECT_SOLID_RECTS(i)             (0x0000C400 + (i) * 8)

/* NVCopyROP[GXcopy] (nv_xaa.c): el destino es el origen (o el color). */
#define NV_ROP_GXCOPY                   0xCC

/* ---------------------------------------------- el búfer de órdenes (nv_xaa.c) */
/* «There is a HW race condition with videoram command buffers. You can't jump
 * to the location of your put offset. We write put at the jump offset + SKIPS
 * dwords with noop padding in between to solve this problem.» */
#define SKIPS                           8
#define NV_DMA_SALTO                    0x20000000u   /* NVDmaNext(pNv, 0x20000000): salto al byte 0 */
#define NV_DMA_MAX                      8191          /* pNv->dmaMax: 32 KB de búfer */

/* NVResetGraphics: atar los objetos 0x80000010..17 a los subcanales 0..7
 * (cabecera de 1 palabra con el método 0 del subcanal, y el identificador). */
static const uint32_t nv_dma_ligacoes[16] = {
    0x00040000, 0x80000010, 0x00042000, 0x80000011, 0x00044000, 0x80000012, 0x00046000, 0x80000013,
    0x00048000, 0x80000014, 0x0004A000, 0x80000015, 0x0004C000, 0x80000016, 0x0004E000, 0x80000017,
};

/* El canal 0 visto por la CPU (pNv->FIFO = BAR0 + 0x800000): PUT y GET en bytes. */
#define NV_USER_BASE                    0x00800000u
#define NV_USER_DMA_PUT                 0x00800040u   /* FIFO[0x0010] */
#define NV_USER_DMA_GET                 0x00800044u   /* FIFO[0x0011] */

/* ---------------------------------------------- PRAMIN (nv_hw.c, NV_ARCH_40) */
/* pNv->PRAMIN = BAR0 + 0x710000; índices en palabras de 32 bits. La tabla
 * hash (0x0000-0x000F) y los objetos (0x0800-0x084E). Dos valores dependen de
 * la tarjeta y los pone el driver: el límite del objeto DMA de toda la
 * memoria de vídeo (0x0801 = FbMapSize - 1) y la dirección del búfer de
 * órdenes (0x084E = desplazamiento | 2). */
#define NV40_PRAMIN_BASE                0x00710000u
#define NV40_PRAMIN_DMA_VRAM_LIMITE     0x0801
#define NV40_PRAMIN_PB_ENDERECO         0x084E
#define NV40_PRAMIN_PB_LIMITE           0x00007FFFu   /* 0x084D: 32 KB */
typedef struct { uint16_t indice; uint32_t valor; } nv_pramin_t;
static const nv_pramin_t nv40_pramin[] = {
    { 0x0000, 0x80000010 }, { 0x0001, 0x00101202 }, { 0x0002, 0x80000011 }, { 0x0003, 0x00101204 },
    { 0x0004, 0x80000012 }, { 0x0005, 0x00101206 }, { 0x0006, 0x80000013 }, { 0x0007, 0x00101208 },
    { 0x0008, 0x80000014 }, { 0x0009, 0x0010120A }, { 0x000A, 0x80000015 }, { 0x000B, 0x0010120C },
    { 0x000C, 0x80000016 }, { 0x000D, 0x0010120E }, { 0x000E, 0x80000017 }, { 0x000F, 0x00101210 },
    { 0x0800, 0x00003000 }, /* 0x0801: FbMapSize - 1 */ { 0x0802, 0x00000002 },
    { 0x0808, 0x02080062 }, { 0x0809, 0x00000000 }, { 0x080A, 0x00001200 }, { 0x080B, 0x00001200 },
    { 0x080C, 0x00000000 }, { 0x080D, 0x00000000 },
    { 0x0810, 0x02080043 }, { 0x0811, 0x00000000 }, { 0x0812, 0x00000000 }, { 0x0813, 0x00000000 },
    { 0x0814, 0x00000000 }, { 0x0815, 0x00000000 },
    { 0x0818, 0x02080044 }, { 0x0819, 0x02000000 }, { 0x081A, 0x00000000 }, { 0x081B, 0x00000000 },
    { 0x081C, 0x00000000 }, { 0x081D, 0x00000000 },
    { 0x0820, 0x02080019 }, { 0x0821, 0x00000000 }, { 0x0822, 0x00000000 }, { 0x0823, 0x00000000 },
    { 0x0824, 0x00000000 }, { 0x0825, 0x00000000 },
    { 0x0828, 0x020A005C }, { 0x0829, 0x00000000 }, { 0x082A, 0x00000000 }, { 0x082B, 0x00000000 },
    { 0x082C, 0x00000000 }, { 0x082D, 0x00000000 },
    { 0x0830, 0x0208009F }, { 0x0831, 0x00000000 }, { 0x0832, 0x00001200 }, { 0x0833, 0x00001200 },
    { 0x0834, 0x00000000 }, { 0x0835, 0x00000000 },
    { 0x0838, 0x0208004A }, { 0x0839, 0x02000000 }, { 0x083A, 0x00000000 }, { 0x083B, 0x00000000 },
    { 0x083C, 0x00000000 }, { 0x083D, 0x00000000 },
    { 0x0840, 0x02080077 }, { 0x0841, 0x00000000 }, { 0x0842, 0x00001200 }, { 0x0843, 0x00001200 },
    { 0x0844, 0x00000000 }, { 0x0845, 0x00000000 },
    { 0x084C, 0x00003002 }, { 0x084D, NV40_PRAMIN_PB_LIMITE }, /* 0x084E: búfer | 2 */
};

/* ---------------------------------------------- PGRAPH y PFIFO (nv_hw.c) */
/* Cada escritura: v = (máscara ? (leer(reg) & máscara) : 0) | valor. Registros
 * como desplazamientos en el BAR 0 (PGRAPH en 0x400000, PFIFO en 0x2000). */
typedef struct { uint32_t reg, mascara, valor; } nv_escrita_t;

/* Lo común de NV_ARCH_40, antes de lo de cada chip. */
static const nv_escrita_t nv40_pgraph_inicio[] = {
    { 0x400080, 0, 0xFFFFFFFF }, { 0x400080, 0, 0x00000000 },
    { 0x400140, 0, 0x00000000 },                 /* interrupciones de PGRAPH apagadas */
    { 0x400100, 0, 0xFFFFFFFF },                 /* y las pendientes, borradas */
    { 0x400144, 0, 0x10010100 }, { 0x400714, 0, 0xFFFFFFFF }, { 0x400720, 0, 0x00000001 },
    { 0x400710, 0x0007FF00, 0x00020100 },
    { 0x400084, 0, 0x401287C0 }, { 0x40008C, 0, 0x60DE8051 }, { 0x400090, 0, 0x00008000 },
    { 0x400610, 0, 0x00BE3C5F }, { 0x400BC4, 0xFFFFFFFF, 0x00008000 },
};
/* (después: 0x405000 = el primer bit de PMC 0x1540 & 0xFF) */
static const nv_escrita_t nv40_pgraph_nv40[] = {      /* chip 0x40 */
    { 0x4009B0, 0, 0x83280FFF }, { 0x4009B4, 0, 0x000000A0 },
};
static const nv_escrita_t nv40_pgraph_outros[] = {    /* el resto */
    { 0x400820, 0, 0x83280EFF }, { 0x400824, 0, 0x000000A0 },
};
static const nv_escrita_t nv40_pgraph_40_45[] = {     /* chips 0x40 y 0x45 (PCI 0x004x, 0x021x) */
    { 0x4009B8, 0, 0x0078E366 }, { 0x4009BC, 0, 0x0000014C },
};
static const nv_escrita_t nv40_pgraph_41_42[] = {     /* 0x41 y 0x42 (PCI 0x00Cx) */
    { 0x400828, 0, 0x007596FF }, { 0x40082C, 0, 0x00000108 },
};
static const nv_escrita_t nv40_pgraph_43[] = {        /* 0x43 (PCI 0x014x) */
    { 0x400828, 0, 0x0072CB77 }, { 0x40082C, 0, 0x00000108 },
};
static const nv_escrita_t nv40_pgraph_nv44[] = {      /* 0x44, 0x46, 0x4A, 0x4C, 0x4E, 0x63, 0x67, 0x68 */
    { 0x400860, 0, 0x00000000 }, { 0x400864, 0, 0x00000000 },
};
static const nv_escrita_t nv40_pgraph_g70[] = {       /* 0x47, 0x49, 0x4B (PCI 0x009x, 0x029x, 0x039x) */
    { 0x400828, 0, 0x07830610 }, { 0x40082C, 0, 0x0000016A },
};
static const nv_escrita_t nv40_pgraph_meio[] = {
    { 0x400B38, 0, 0x2FFFF800 }, { 0x400B3C, 0, 0x00006000 },
    { 0x40032C, 0, 0x01000000 }, { 0x400220, 0, 0x00001200 },
};
/* La configuración de la memoria: copia de PFB 0x100200/0x100204 en PGRAPH
 * (NV40: 0x4009A4/A8 y 0x4069A4/A8; el resto: 0x4009F0/F4 o 0x400DF0/F4, y
 * 0x4069F0/F4), los límites (NV40: 0x400864/68; el resto: 0x4008A0/A4 =
 * FbMapSize - 1) y 0x400820/24 (NV40) o 0x400840/44 (el resto) a cero. */
#define NV40_PFB_CFG0                   0x00100200u
#define NV40_PFB_CFG1                   0x00100204u
static const nv_escrita_t nv40_pgraph_fim[] = {
    { 0x400B20, 0, 0x00000000 }, { 0x400B04, 0, 0xFFFFFFFF },
    { 0x40053C, 0, 0x00000000 }, { 0x400540, 0, 0x00000000 },
    { 0x400544, 0, 0x00007FFF }, { 0x400548, 0, 0x00007FFF },
};

/* PFIFO: el canal 0 en modo DMA, con el búfer de órdenes en el objeto 0x1213
 * (PRAMIN 0x084C), tabla hash en 0x10000, RAMFC en 0x11000 y RAMRO en 0x11200. */
static const nv_escrita_t nv40_pfifo_inicio[] = {
    { 0x002500, 0, 0x00000000 }, { 0x002504, 0, 0x00000001 },
    { 0x003200, 0, 0x00000000 }, { 0x003250, 0, 0x00000000 },
    { 0x003204, 0, 0x00010000 },                 /* NV_ARCH_40 */
    { 0x003240, 0, 0x00000000 }, { 0x003244, 0, 0x00000000 },
    { 0x00322C, 0, 0x00001213 },                 /* NV_ARCH_40 */
    { 0x003000, 0, 0x00000000 }, { 0x003050, 0, 0x00000000 },
    { 0x002210, 0, 0x03000100 }, { 0x002214, 0, 0x00000110 }, { 0x002218, 0, 0x00000112 },
    { 0x00250C, 0, 0x0000FFFF }, { 0x003258, 0, 0x0000FFFF },
    { 0x002140, 0, 0x00000000 },                 /* interrupciones de PFIFO apagadas */
    { 0x002100, 0, 0xFFFFFFFF },                 /* y las pendientes, borradas */
    { 0x003054, 0, 0x00000001 }, { 0x003230, 0, 0x00000000 }, { 0x003280, 0, 0x00000000 },
    { 0x003224, 0, 0x000F0078 },                 /* little endian */
    { 0x003220, 0, 0x00000001 }, { 0x003200, 0, 0x00000001 }, { 0x003250, 0, 0x00000001 },
    { 0x003254, 0, 0x00000001 }, { 0x002500, 0, 0x00000001 },
};

/* NVSync: PGRAPH ocupado mientras PGRAPH[0x0700/4] no es cero. */
#define NV_PGRAPH_STATUS                0x00400700u

#endif
