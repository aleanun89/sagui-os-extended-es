// SPDX-License-Identifier: GPL-2.0-or-later
/* uvc_nucleo.h - la parte pura del driver UVC 1.1 (clase vídeo USB, D277).
 *
 * Sin dependencias del kernel (solo stdint y memcpy): lo usan el driver (uvc.c) y las
 * pruebas del anfitrión (tests/h277_uvc.py, con ASan). Contiene:
 *   - el recorrido de los descriptores (VideoControl y VideoStreaming: formatos YUY2 y
 *     MJPEG, tamaños de cuadro, intervalos, unidad de procesamiento y alternativas del
 *     endpoint isócrono);
 *   - los bloques PROBE/COMMIT (26 bytes de UVC 1.0, 34 de UVC 1.1) y la elección de
 *     la alternativa que cabe en el ancho de banda negociado;
 *   - el montaje de cuadros a partir de la cabecera de carga útil de cada paquete
 *     (bit FID, EOF, ERR) y la validación del cuadro (YUY2: tamaño exacto; MJPEG: SOI/EOI);
 *   - un generador de patrón YUY2 para la fuente de prueba.
 * Solo se usan las especificaciones públicas (USB Video Class 1.1 y USB 2.0). */
#ifndef UVC_NUCLEO_H
#define UVC_NUCLEO_H

#ifdef UVC_ANFITRION
#include <stdint.h>
#include <stdbool.h>
#else
#include "tipos.h"
#endif

#define UVC_FMT_YUY2   1
#define UVC_FMT_MJPEG  2
#define UVC_MAX_FORMATOS 4
#define UVC_MAX_QUADROS  12
#define UVC_MAX_INTERVALOS 4
#define UVC_MAX_ALT      16

typedef struct {
    uint8_t  idx;                       /* bFrameIndex */
    uint16_t larg, alt;
    uint32_t tam_max;                   /* dwMaxVideoFrameBufferSize */
    uint32_t intervalo_def;             /* en unidades de 100 ns */
    uint8_t  n_int;
    uint32_t ints[UVC_MAX_INTERVALOS];  /* intervalos discretos (o min/max/paso si n_int = 0 y continuo) */
} uvc_quadro_t;

typedef struct {
    uint8_t  idx;                       /* bFormatIndex */
    uint8_t  tipo;                      /* UVC_FMT_* */
    uint8_t  bits;                      /* bits por píxel (sin comprimir) */
    uint8_t  nq;
    uvc_quadro_t q[UVC_MAX_QUADROS];
} uvc_formato_t;

typedef struct {
    uint8_t  alt, ep;                   /* alternativa de la interfaz de streaming y endpoint isócrono de entrada */
    uint8_t  mult;                      /* transacciones por microcuadro (1 a 3) */
    uint8_t  intervalo;                 /* bInterval del endpoint */
    uint16_t mps;                       /* bytes por transacción */
} uvc_alt_t;

typedef struct {
    uint16_t bcd;                       /* versión de UVC del VC_HEADER */
    uint8_t  iface_vc, iface_vs;
    uint8_t  pu_id;                     /* unidad de procesamiento (0 = ninguna) */
    uint16_t pu_controles;              /* bmControls: bit 0 brillo, bit 1 contraste */
    uint8_t  nf;
    uvc_formato_t f[UVC_MAX_FORMATOS];
    uint8_t  nalt;
    uvc_alt_t alt[UVC_MAX_ALT];
} uvc_info_t;

/* Recorre el descriptor de configuración completo. Devuelve el número de formatos útiles
 * (YUY2 o MJPEG con algún tamaño); 0 si no es una cámara UVC que se pueda usar. */
int uvc_analizar(const uint8_t *b, int n, uvc_info_t *o);

/* Bloque PROBE/COMMIT (apartado 4.3.1.1 de UVC 1.1). */
typedef struct {
    uint16_t hint;
    uint8_t  formato, quadro;
    uint32_t intervalo;
    uint16_t kf, pf, calidad, ventana, retardo;
    uint32_t tam_quadro, tam_payload;
    uint32_t reloj;
    uint8_t  framing, version_pref, version_min, version_max;
} uvc_ctl_t;

#define UVC_CTL_TAM_10 26
#define UVC_CTL_TAM_11 34
int  uvc_ctl_serializar(const uvc_ctl_t *c, uint8_t *o, int tam);          /* tam = 26 o 34; devuelve tam */
bool uvc_ctl_leer(const uint8_t *b, int n, uvc_ctl_t *c);                  /* false si n < 26 */

/* Procedimiento de negociación (4.3.1.1.1) contra un dispositivo, con las órdenes de
 * clase que da quien llama: pedido(ctx, tipo, req, valor, indice, dados, n) -> bytes o <0. */
typedef int (*uvc_pedido_f)(void *ctx, uint8_t tipo, uint8_t req, uint16_t valor, uint16_t indice, void *dados, uint16_t n);
/* Pide formato/cuadro/intervalo, hace PROBE (SET_CUR y GET_CUR) y COMMIT. En *res queda lo
 * que el dispositivo aceptó. 0 = bien, <0 = error. tam_ctl: lo que mide el bloque (26 o 34). */
int uvc_negociar(uvc_pedido_f pedido, void *ctx, uint8_t iface_vs, int tam_ctl,
                 uint8_t formato, uint8_t quadro, uint32_t intervalo, uvc_ctl_t *res);
/* Alternativa con el menor ancho que cabe en tam_payload (o la mayor, si ninguna cabe); NULL si no hay. */
const uvc_alt_t *uvc_elegir_alt(const uvc_info_t *i, uint32_t tam_payload);
/* Formato (por tipo) y cuadro (por tamaño) pedidos: el exacto o, si no está, el más cercano. */
bool uvc_buscar(const uvc_info_t *i, uint8_t tipo, uint16_t larg, uint16_t alt, const uvc_formato_t **f, const uvc_quadro_t **q);

/* ---- montaje de cuadros ---- */
typedef struct {
    uint8_t  *buf[2];                   /* [montaje, último completo] (los reserva quien llama) */
    uint32_t cap;
    uint32_t len;                       /* bytes ya montados del cuadro en curso */
    uint32_t ult_len;                   /* bytes del último cuadro completo */
    uint8_t  tipo;                      /* UVC_FMT_* */
    uint16_t larg, alt;
    int      fid;                       /* FID del cuadro en curso (-1 = ninguno aún) */
    bool     malo;                      /* el cuadro en curso trae un error */
    uint32_t seq;                       /* cuadros completos entregados */
    uint32_t descartados;               /* cuadros descartados (error, tamaño o MJPEG roto) */
    uint32_t paquetes, errores;         /* paquetes con cabecera mala o bit ERR */
} uvc_montador_t;

void uvc_montador_iniciar(uvc_montador_t *m, uint8_t *b0, uint8_t *b1, uint32_t cap, uint8_t tipo, uint16_t larg, uint16_t alt);
void uvc_montador_paquete(uvc_montador_t *m, const uint8_t *p, uint32_t n);
const uint8_t *uvc_montador_ultimo(const uvc_montador_t *m);                /* último cuadro completo */
bool uvc_cuadro_valido(uint8_t tipo, uint16_t larg, uint16_t alt, const uint8_t *d, uint32_t n);

/* ---- fuente de prueba ---- */
/* Cuadro YUY2 de larg x alt: barras de color, un cuadro blanco que se mueve con seq y un
 * contador en binario (en la franja de abajo). */
void uvc_gerar_padrao(uint8_t *yuy2, uint16_t larg, uint16_t alt, uint32_t seq);
/* Empaqueta el cuadro en paquetes con cabecera UVC de 12 bytes (FID alterno, EOF en el último);
 * "pos" avanza; devuelve los bytes del paquete (0 = cuadro acabado). */
uint32_t uvc_gerar_paquete(const uint8_t *quadro, uint32_t tam, uint32_t *pos, uint8_t fid, uint32_t mps, uint8_t *saida);

#endif
