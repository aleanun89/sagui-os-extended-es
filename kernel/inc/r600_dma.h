/* r600_dma.h - paquetes del motor DMA de las Radeon R6xx/R7xx y el reparto de
 * un rectángulo en copias lineales (D223).
 *
 * Es lógica pura, sin registros ni memoria de vídeo: la usa el driver
 * (kernel/dispositivos/acel_r600.c) y también se compila en el PC para la
 * prueba de unidad (tests/r600_dma_unidad.c, con -DR600_ANFITRION).
 *
 * El motor DMA asíncrono de las R6xx/R7xx lee paquetes de un anillo en la
 * memoria de vídeo. Cabecera de un paquete:
 *   bits 31:28  la orden (2 escribir, 3 copiar, 6 valla, 15 nada);
 *   bit 23      «t» (con mosaico; aquí siempre 0: memoria lineal);
 *   bit 22      «s» (siempre 0);
 *   bits 15:0   cuántas palabras de 32 bits (copiar, escribir).
 * Las direcciones son del controlador de memoria de la GPU (MC), de 40 bits:
 * los 32 bits bajos van en su palabra y los bits 39:32 («alto») en otra. D408:
 * el driver pone todo lo que toca el DMA (pantalla, anillo, zona de paso) en un
 * mismo tramo de 4 GB del MC, así que un solo byte alto vale para el destino y
 * el origen de todos los paquetes; las funciones sin «_alto» son las de siempre
 * (byte alto 0) y dan exactamente las mismas palabras que antes de la D408.
 *
 * La copia lineal NO es igual en las dos generaciones (r600_copy_dma y
 * rv770_copy_dma de Linux, y r600_dma_cs_parse):
 *   R6xx: cabecera, destino, origen, (bits altos del destino << 16 | los del origen): 4 palabras;
 *   R7xx: cabecera, destino, origen, bits altos del destino, bits altos del origen: 5 palabras.
 * Un paquete con el tamaño equivocado desalinea todo el anillo (el motor leería
 * la cabecera siguiente como una dirección): por eso el formato va por
 * generación y la prueba de unidad comprueba los dos.
 * Escribir: cabecera (n), destino, bits altos, n palabras de datos. Valla:
 * cabecera, destino, bits altos, valor (4 palabras en las dos).
 * Referencia de los números: r600d.h, r600_dma.c, rv770_dma.c y r600_cs.c del
 * driver radeon de Linux (cabeceras MIT/X11); el código es propio. */
#ifndef SAGUI_R600_DMA_H
#define SAGUI_R600_DMA_H

#ifdef R600_ANFITRION
#include <stdint.h>
#include <stdbool.h>
#else
#include "tipos.h"
#endif

#define R600_DMA_ESCREVER   0x2u
#define R600_DMA_COPIAR     0x3u
#define R600_DMA_VALLA      0x6u
#define R600_DMA_NADA       0xFu
#define R600_DMA_CABECA(op, n)  ((((uint32_t)(op) & 0xFu) << 28) | ((uint32_t)(n) & 0xFFFFu))
#define R600_DMA_NOP        R600_DMA_CABECA(R600_DMA_NADA, 0)
#define R600_DMA_MAX_DW     0xFFFEu     /* palabras por copia (como el driver radeon) */

/* Cada paquete escribe en p y devuelve cuántas palabras usó (R600_PKT_MAX como mucho). */
#define R600_PKT_MAX        5
int r600_pkt_copiar(uint32_t *p, uint32_t dst, uint32_t src, uint32_t ndw, bool r7xx);   /* 4 o 5 */
int r600_pkt_escrever(uint32_t *p, uint32_t dst, uint32_t valor);   /* una palabra */
int r600_pkt_valla(uint32_t *p, uint32_t dst, uint32_t valor);      /* escribe valor al acabar lo anterior */
/* D408: lo mismo con los bits 39:32 de las direcciones (el mismo byte para destino y origen), como
 * r600_copy_dma/rv770_copy_dma, r600_dma_ring_test y r600_dma_fence_ring_emit de Linux:
 *   R6xx copiar: palabra 3 = alto << 16 | alto;  R7xx: palabras 3 y 4 = alto;
 *   escribir y valla: palabra 2 = alto. */
int r600_pkt_copiar_alto(uint32_t *p, uint32_t dst, uint32_t src, uint32_t ndw, bool r7xx, uint8_t alto);
int r600_pkt_escrever_alto(uint32_t *p, uint32_t dst, uint32_t valor, uint8_t alto);
int r600_pkt_valla_alto(uint32_t *p, uint32_t dst, uint32_t valor, uint8_t alto);

/* La pantalla vista por el motor. */
typedef struct {
    uint32_t mc_tela;        /* dirección en el MC del píxel (0,0) */
    uint32_t pitch;          /* bytes por fila */
    uint32_t mc_rascunho;    /* zona de paso para las copias en la misma fila */
    int      filas_rascunho; /* cuántas filas de pitch bytes caben en ella (>= 1) */
} r600_geo_t;

/* Adónde van las copias lineales. copia() pone un paquete en el anillo;
 * barreira() espera a que el motor termine todo lo anterior (con plazo). Las
 * dos devuelven false si no se pudo: entonces se para todo y quien llama vuelve
 * a la CPU. */
typedef struct {
    void *ctx;
    bool (*copia)(void *ctx, uint32_t dst, uint32_t src, uint32_t ndw);
    bool (*barreira)(void *ctx);
} r600_saida_t;

/* Copia un rectángulo de la pantalla con el resultado de memmove, solape como
 * solape. Lo único que se supone del motor es que hace los paquetes en orden y
 * que cada uno lee su origen antes de escribir su destino:
 *   - filas distintas: una copia por fila, empezando por la fila que no pisa lo
 *     que falta por leer (de abajo arriba si el destino está más abajo); el
 *     origen y el destino de cada paquete no se tocan;
 *   - misma fila con solape (mover en horizontal): no se sabe en qué orden lee
 *     el motor dentro de un paquete, así que se pasa por la zona de paso en
 *     franjas: origen -> paso, barrera, paso -> destino, barrera.
 * Ningún paquete lee lo que escribió un paquete anterior sin una barrera en
 * medio. */
bool r600_copiar_ret(const r600_geo_t *g, const r600_saida_t *s, int ox, int oy, int dx, int dy, int l, int a);

/* Rellena un rectángulo copiando a cada fila una fila ya pintada del color
 * (mc_linha_cor, al menos l píxeles). Las filas no se solapan. */
bool r600_preencher_ret(const r600_geo_t *g, const r600_saida_t *s, uint32_t mc_linha_cor, int x, int y, int l, int a);

#endif
