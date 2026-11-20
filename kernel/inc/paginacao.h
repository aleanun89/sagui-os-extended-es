// SPDX-License-Identifier: GPL-3.0-only
/* paginacao.h - espacios de direcciones (directorios de páginas). Ver D010. */
#ifndef SAGUI_PAGINACAO_H
#define SAGUI_PAGINACAO_H
#include "tipos.h"

#define PG_PRESENTE  0x001
#define PG_ESCRITA   0x002
#define PG_USUARIO   0x004
#define PG_EMPRESTADA 0x200   /* bit libre para el sistema: página que no es del proceso (ej.: píxeles de ventana) */
#define PG_ACESSADA  0x020   /* A: el hardware la pone al usar la entrada (reloj de la swap, D352) */
#define PG_SUJA      0x040   /* D: el hardware la pone al escribir */
/* D352: bit libre para el sistema (11): la página no comprimió bien la última vez que se
 * intentó sacarla a la swap. Vale mientras D siga a cero (no se ha escrito desde entonces). */
#define PG_INCOMPRIMIVEL 0x800
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

/* Un espacio de direcciones se identifica por lo que va en el CR3: sin PAE, la
 * dirección física de su directorio de páginas; con PAE (D349), la de su PDPT.
 * Los dos están en la memoria baja, así que el kernel los ve 1:1. */
typedef uint32_t espaco_t;

/* D349: una dirección física. Con PAE llega a 64 GB (36 bits): las páginas de
 * los programas pueden estar por encima de 4 GB. Nunca convertir en puntero una
 * que no sea de la zona baja (ver memalta.h). */
typedef uint64_t fis_t;

/* D348/D349: el valor de una entrada de tabla de páginas en los dos modos. Sin
 * PAE la entrada mide 4 bytes (solo cuenta la mitad baja); con PAE, 8 (bit 63 =
 * NX). Quien escribe entradas fuera de paginacao.c lo hace con pte_escrever(),
 * que sabe el tamaño y, con PAE, el orden seguro de las dos mitades. */
typedef uint64_t pte_t;

/* D349: bandera del API, no del hardware: página que no se puede ejecutar. Con
 * NX activo se convierte en el bit 63 de la entrada; sin NX (o sin PAE) se
 * ignora. espaco_traduzir() la devuelve si la página la tiene. */
#define PG_NX        0x400

extern bool paginacao_com_pae;    /* D349: tablas de 3 niveles con entradas de 8 bytes */
extern uint64_t paginacao_bit_nx; /* bit 63 si NX está activo; 0 si no */

/* Escribe una entrada (de tabla o de directorio que apunta a tabla). Con PAE,
 * en tres pasos para que ninguna otra CPU vea nunca una entrada presente a
 * medias: primero la mitad baja a cero (sin P), después la alta, y al final la
 * baja (que lleva P). */
static inline void pte_escrever(void *e, fis_t fis, uint32_t flags)
{
    uint32_t baixa = ((uint32_t)fis & 0xFFFFF000u) | (flags & 0xFFFu & ~(uint32_t)PG_NX);
    if (paginacao_com_pae) {
        volatile uint32_t *m = (volatile uint32_t *)e;
        uint32_t alta = (uint32_t)(fis >> 32) & 0x000FFFFFu;
        if (flags & PG_NX) alta |= (uint32_t)(paginacao_bit_nx >> 32);
        m[0] = 0;
        __asm__ volatile("" ::: "memory");
        m[1] = alta;
        __asm__ volatile("" ::: "memory");
        m[0] = baixa;
    } else {
        *(volatile uint32_t *)e = baixa;
    }
}
static inline void pagina_invalidar(uint32_t virt)
{
    __asm__ volatile("invlpg (%0)" : : "r"(virt) : "memory");
}

/* D349: elige el modo (PAE o no, NX o no) según la CPU y la cabecera de
 * arranque. Antes de memfisica_iniciar(), que necesita saber si hay PAE. */
void     paginacao_escolher_modo(void);
bool     paginacao_pae_cpu(void);     /* la CPU anuncia PAE (aunque no se use) */
bool     paginacao_nx_cpu(void);      /* la CPU anuncia NX */
bool     paginacao_nx(void);          /* NX activo (solo con PAE) */
bool     paginacao_xd_liberado(void); /* la principal quitó XD_DISABLE de IA32_MISC_ENABLE (Intel) */
uint64_t paginacao_limite_fisico(void);   /* hasta dónde puede llegar la RAM usada (4 GB sin PAE) */
void     paginacao_iniciar(void);
uint32_t paginacao_fim_mapa(void);   /* hasta dónde ve el kernel la RAM 1:1 */
bool     paginacao_pse(void);        /* mapa del kernel en páginas grandes (4 MB, o 2 MB con PAE) */
espaco_t espaco_kernel(void);
espaco_t espaco_criar(void);
void     espaco_destruir(espaco_t e);
void     espaco_ativar(espaco_t e);
espaco_t espaco_atual(void);
bool     espaco_mapear(espaco_t e, uint32_t virt, fis_t fis, uint32_t flags);
fis_t    espaco_traduzir(espaco_t e, uint32_t virt, uint32_t *flags);   /* 0 si no está mapeado */
/* D349: cambia los permisos de las páginas propias ya asignadas de [virt,
 * virt+bytes) del programa: añade las banderas "por" y quita las de "tirar"
 * (solo PG_ESCRITA y PG_NX). Lo que no está asignado se deja (quien llama
 * comprueba antes, si le importa). Atómico por entrada: nunca pasa por P=0 y
 * conserva A/D. */
void     espaco_ajustar(espaco_t e, uint32_t virt, uint32_t bytes, uint32_t por, uint32_t tirar);
uint32_t espaco_paginas_usuario(espaco_t e);
void     espaco_desmapear(espaco_t e, uint32_t virt);
/* D209: quita una página propia del programa (no prestada) y devuelve su
 * dirección física para liberarla después; 0 si no había o es prestada. */
fis_t    espaco_tirar(espaco_t e, uint32_t virt);

/* D401: copia al escribir (fork). espaco_clonar() comparte las páginas propias de "de" con
 * "para" (recién creado); las escribibles quedan de copia al escribir en los dos. "copiar_ja"
 * (o NULL) elige las que se copian ya. 0 o ERRO_SEM_MEMORIA (entonces se destruye "para").
 * espaco_cow_falta() atiende el fallo de escritura en una página presente sin W: 0 no era de
 * copia al escribir, 1 resuelto (repetir), -1 sin memoria. espaco_romper_cow() la rompe si lo es
 * (para que el kernel escriba ahí); espaco_traduzir_escrita() traduce rompiéndola antes (quien
 * escribe por la dirección física). pte_partilhada(): la entrada es de una página compartida
 * (la swap no la saca). espaco_paginas_cow_alheias(): la parte de las compartidas que no se le
 * cuenta a "e" (cada uno paga 1/referencias). */
int      espaco_clonar(espaco_t de, espaco_t para, bool (*copiar_ja)(fis_t));
int      espaco_cow_falta(espaco_t e, uint32_t virt);
bool     espaco_romper_cow(espaco_t e, uint32_t virt);
fis_t    espaco_traduzir_escrita(espaco_t e, uint32_t virt);
bool     pte_partilhada(pte_t v);
uint32_t espaco_paginas_cow_alheias(espaco_t e);

/* Asigna páginas nuevas y a cero para [virt, virt+bytes) en el espacio del usuario. */
bool     espaco_alocar(espaco_t e, uint32_t virt, uint32_t bytes, uint32_t flags);

/* D352: la página existe para el programa (presente o sacada a la swap comprimida). Para
 * contar o comprobar sin traerla: espaco_traduzir() trae de la swap la que está fuera. */
bool     espaco_existe(espaco_t e, uint32_t virt);
uint32_t espaco_paginas_troca(espaco_t e);   /* D352: páginas del programa en la swap */

/* D352: acceso a las entradas de tabla de páginas de los programas para la swap (troca.c).
 * espaco_pte() da la entrada de "virt" (zona del programa) si su tabla existe, o NULL; las
 * entradas se leen y escriben con estas funciones, que saben el tamaño (4 u 8 bytes). */
void    *espaco_pte(espaco_t e, uint32_t virt);
uint32_t paginacao_bloco(void);              /* lo que cubre una tabla: 4 MB, o 2 MB con PAE */
pte_t    pte_ler(const void *p);
void     pte_gravar(void *p, pte_t v);       /* el valor ya hecho (pte_valor o una entrada de swap) */
pte_t    pte_valor(fis_t fis, uint32_t flags);
fis_t    pte_endereco(pte_t v);
uint32_t pte_flags(pte_t v);                 /* banderas del API (PG_NX incluida) */
void     pte_mudar(void *p, uint32_t por, uint32_t tirar);   /* bits bajos de una entrada presente, atómico */
pte_t    pte_esvaziar(void *p);              /* la pone a 0 de forma atómica y devuelve lo que tenía */

/* Mapea una franja física alta (vídeo, USB) en la ventana del kernel; devuelve la
 * dirección virtual. */
uint32_t janela_mapear(uint32_t fis, uint32_t bytes);
uint32_t janela_mapear_tipo(uint32_t fis, uint32_t bytes, uint32_t tipo);   /* tipo = PG_PWT/PCD/PAT */
/* D192: franja de ventanas sin mapear (devuelve la dirección virtual, 0 si no
 * cabe) y páginas sueltas dentro de ella, para quien mapea y desmapea a menudo. */
uint32_t janela_reservar(uint32_t bytes);
void     janela_pagina(uint32_t virt, fis_t fis, uint32_t tipo);
void     janela_soltar(uint32_t virt);
/* D348: la entrada de la tabla de páginas de una dirección de la franja de
 * ventanas (NULL si no es de ella), para las ventanas temporales de memalta.c.
 * Se escribe con pte_escrever(): mide 4 u 8 bytes según el modo (D349). */
void    *janela_entrada(uint32_t virt);

#endif
