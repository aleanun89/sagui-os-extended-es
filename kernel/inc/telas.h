// SPDX-License-Identifier: GPL-3.0-only
/* telas.h - varias pantallas (salidas de vídeo) y escritorio extendido o en espejo (mejora 060, D290).
 *
 * Cada salida de vídeo es una "cabeza". La 0 es la que dejó el arranque (VESA, video.c) y puede
 * cambiar de modo en caliente con la tarjeta de Bochs/QEMU (D059). Las demás las registra el
 * driver de su tarjeta con telas_registrar_saida() (hoy bga_extra.c: segundas tarjetas de Bochs
 * de QEMU; los drivers de las GPU de la v8.3/v11 usarán la misma interfaz).
 *
 * El escritorio es un único rectángulo (el búfer de atrás de video.c):
 *   - EXTENDER: las cabezas una al lado de la otra (a la derecha o debajo de la principal, que
 *     siempre queda en (0,0): la barra de arriba vive en ella);
 *   - ESPEJO: todas muestran la esquina (0,0) del escritorio, que tiene el tamaño de la principal.
 * Con una sola cabeza todo es como antes. */
#ifndef SAGUI_TELAS_H
#define SAGUI_TELAS_H
#include "tipos.h"
#include "abi.h"   /* TELAS_MODO_*, TELAS_LADO_*, info_telas_t */

#define TELAS_MAX TELAS_NUM_MAX

/* Una salida registrada por un driver. Siempre 32 bits por píxel 0x00RRGGBB. */
typedef struct saida {
    char     nome[24];
    uint8_t *fb;              /* framebuffer de la salida (ventana del kernel) */
    uint32_t pitch;           /* bytes por línea del modo actual */
    uint32_t largura, altura; /* modo actual */
    uint32_t max_l, max_a, memoria;   /* máximos del modo y bytes del framebuffer */
    /* Cambia el modo (actualiza pitch, largura y altura); false = la salida lo rechazó y sigue como estaba. */
    bool   (*mudar_modo)(struct saida *s, uint32_t l, uint32_t a);
    void    *priv;
} saida_t;

/* Para los drivers: añade una salida (la copia) y devuelve su cabeza (>= 1), o -1 si no cabe. */
int  telas_registrar_saida(const saida_t *s);
/* Busca las salidas extra de las tarjetas conocidas y aplica la configuración guardada. Después
 * de gui_iniciar: el escritorio cambia de tamaño sin que nadie lo pida. */
void telas_iniciar(void);

/* ---- consultas (el escritorio mira solo esto) */
int  telas_qtd(void);                       /* cabezas presentes (1 = pantalla única) */
int  telas_modo(void);                      /* TELAS_MODO_EXTENDER o TELAS_MODO_ESPELHO */
int  telas_lado(void);                      /* TELAS_LADO_* (solo EXTENDER) */
int  telas_principal(void);                 /* cabeza que lleva la barra, en (0,0) del escritorio */
/* Rectángulo de una cabeza dentro del escritorio y su modo; false si no existe. */
bool telas_rect(int cab, int *x, int *y, int *l, int *a);
void telas_principal_rect(int *l, int *a);  /* tamaño de la principal (con una pantalla, todo el escritorio) */
/* La pantalla que contiene el punto (o la más cercana, para pegar el cursor y las ventanas). */
int  telas_da_posicao(int x, int y);
void telas_prender(int *x, int *y);         /* si (x, y) cae fuera de toda pantalla, lo acerca a la más cercana */
/* Tamaño del escritorio calculado con la cabeza `cab` en l x a (cab < 0: como está). */
void telas_desktop(int cab, uint32_t l, uint32_t a, uint32_t *dl, uint32_t *da);
/* Para video.c: la cabeza 0 pone aquí su modo. */
void telas_cabeca0(uint32_t l, uint32_t a);
const saida_t *telas_saida(int cab);        /* cabeza >= 1, NULL si no existe */

/* ---- configuración: devuelve 0 o un error. La resolución de una cabeza es un modo del driver. */
int  telas_configurar(int modo, int lado, int principal);
int  telas_resolucao(int cab, uint32_t l, uint32_t a);
int  telas_guardar(void);                   /* /home/.config/pantallas.conf */

/* Segundas tarjetas de Bochs/QEMU (bga_extra.c): registra las que encuentra; devuelve cuántas. */
int  bga_extra_detectar(void);

/* SIS_TELAS (abi.h). */
int32_t sis_telas(uint32_t op, uint32_t a, uint32_t b, uint32_t c, uint32_t e);

#endif
