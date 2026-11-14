// SPDX-License-Identifier: GPL-3.0-only
/* paginacao.h - espacios de direcciones (directorios de páginas). Ver D010. */
#ifndef SAGUI_PAGINACAO_H
#define SAGUI_PAGINACAO_H
#include "tipos.h"

#define PG_PRESENTE  0x001
#define PG_ESCRITA   0x002
#define PG_USUARIO   0x004
#define PG_EMPRESTADA 0x200   /* bit libre para el sistema: página que no es del proceso (ej.: píxeles de ventana) */
/* Tipo de memoria de una página de 4 KB: índice en la PAT = PAT*4 + PCD*2 + PWT.
 * El wc.c reprograma la entrada 1 (solo PWT) para write-combining (D057). */
#define PG_PWT       0x008
#define PG_PCD       0x010
#define PG_PAT       0x080

/* Mapa del espacio de direcciones (D049):
 *   0          - fin de la RAM   kernel, 1:1 con la memoria física (como máximo hasta KERNEL_JANELAS)
 *   0x78000000 - 0x7FFFFFFF   ventanas del kernel para dispositivos (vídeo, USB): 128 MB
 *   0x80000000 - 0xDBFFFFFF   programa: código, datos y el área de aumentar_memoria
 *   0xDC000000 - 0xDFFFFFFF   pilas de los hilos: 64 huecos de 1 MB, con 64 KB de guarda abajo (D186)
 *   0xE0000000 - 0xEFFFFFFF   píxeles de las ventanas del programa (32 x 8 MB, D044, D059)
 *   0xF0000000 - 0xFEFFFFFF   mapas de dispositivos (los BAR del PCI) para el programa (D158)
 *   0xFF000000 - 0xFF3FFFFF   guarda de la pila: nunca se mapea (D186)
 *   0xFF400000 - 0xFFBFFFFF   pila del hilo principal: 8 MB reservados, asignados al tocarlos (D186)
 *   0xFFC00000 - 0xFFFFFFFF   APIC local e IOAPIC, solo para el kernel (D157, D166) */
#define KERNEL_JANELAS   0x78000000u
#define USUARIO_INICIO   0x80000000u
#define USUARIO_JANELAS  0xE0000000u
#define USUARIO_MMIO     0xF0000000u   /* los BAR del PCI se mapean aquí (D158) */
#define USUARIO_MMIO_FIM 0xFF000000u   /* antes de la pila, que está arriba */
#define USUARIO_FIM      0xFFC00000u

/* D186: las pilas crecen bajo demanda. La del hilo principal ocupa
 * [USUARIO_PILHA, USUARIO_FIM); por debajo, hasta USUARIO_MMIO_FIM, queda la
 * guarda. Las de los hilos van en huecos de 1 MB alineados: los 64 KB de
 * abajo de cada hueco son su guarda y el resto, su pila. */
#define USUARIO_PILHA    0xFF400000u
#define USUARIO_HILOS    0xDC000000u
#define HILO_VAGA        0x00100000u   /* 1 MB por hilo */
#define HILO_VAGAS       64u
#define HILO_GUARDA      0x00010000u   /* 64 KB */

/* Un espacio de direcciones se identifica por la dirección física de su
 * directorio de páginas (que, por el mapeo 1:1, también es el virtual). */
typedef uint32_t espaco_t;

void     paginacao_iniciar(void);
uint32_t paginacao_fim_mapa(void);   /* hasta dónde ve el kernel la RAM 1:1 */
bool     paginacao_pse(void);        /* mapa del kernel en páginas de 4 MB */
espaco_t espaco_kernel(void);
espaco_t espaco_criar(void);
void     espaco_destruir(espaco_t e);
void     espaco_ativar(espaco_t e);
espaco_t espaco_atual(void);
bool     espaco_mapear(espaco_t e, uint32_t virt, uint32_t fis, uint32_t flags);
uint32_t espaco_traduzir(espaco_t e, uint32_t virt, uint32_t *flags);   /* 0 si no está mapeado */
uint32_t espaco_paginas_usuario(espaco_t e);
void     espaco_desmapear(espaco_t e, uint32_t virt);
/* D209: quita una página propia del programa (no prestada) y devuelve su
 * dirección física para liberarla después; 0 si no había o es prestada. */
uint32_t espaco_tirar(espaco_t e, uint32_t virt);

/* Asigna páginas nuevas y a cero para [virt, virt+bytes) en el espacio del usuario. */
bool     espaco_alocar(espaco_t e, uint32_t virt, uint32_t bytes, uint32_t flags);

/* Mapea una franja física alta (vídeo, USB) en la ventana del kernel; devuelve la
 * dirección virtual. */
uint32_t janela_mapear(uint32_t fis, uint32_t bytes);
uint32_t janela_mapear_tipo(uint32_t fis, uint32_t bytes, uint32_t tipo);   /* tipo = PG_PWT/PCD/PAT */
/* D192: franja de ventanas sin mapear (devuelve la dirección virtual, 0 si no
 * cabe) y páginas sueltas dentro de ella, para quien mapea y desmapea a menudo. */
uint32_t janela_reservar(uint32_t bytes);
void     janela_pagina(uint32_t virt, uint32_t fis, uint32_t tipo);
void     janela_soltar(uint32_t virt);

#endif
