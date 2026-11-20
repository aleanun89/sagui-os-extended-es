// SPDX-License-Identifier: GPL-3.0-only
/* painel_logica.h - la parte pura de los paneles táctiles PS/2 (D407): reconocer el modelo por
 * sus respuestas, decodificar los paquetes absolutos de Synaptics, ALPS y Elantech y convertir
 * los toques en movimiento, clics y rueda (los gestos). Sin acceso al hardware: lo usa
 * kernel/dispositivos/painel.c y lo prueba el PC (tests/painel_unidad.c, h408 unidad) con el
 * mismo código.
 *
 * Fuentes del protocolo (solo hechos: órdenes, formatos de paquete y constantes; el código es
 * propio):
 *   - Synaptics: «Synaptics PS/2 TouchPad Interfacing Guide», PN 511-000275-01, revisión B
 *     (documentación pública): órdenes especiales (cuatro E8 con dos bits cada una), consultas
 *     0x00 (identificar), 0x01 (modos), 0x02 (capacidades), 0x03 (modelo), 0x09/0x0C (capacidades
 *     extendidas), 0x0D (coordenadas máximas), byte de modo (absoluto, 80 paquetes/s, sin gestos
 *     del firmware, modo W) y el paquete absoluto de 6 bytes con W.
 *   - Synaptics AGM (dos dedos), ALPS y Elantech: no hay guía pública completa; los hechos
 *     (firmas de identificación, registros, formatos) se han contrastado con lo que documenta el
 *     kernel Linux (Documentation/input/devices/alps.rst y elantech.rst, y las constantes de
 *     drivers/input/mouse/), que es GPL-2.0-only: no se ha copiado su código.
 *
 * Coordenadas: todo se pasa a «unidades del panel» con la y creciendo HACIA ABAJO, como en la
 * pantalla (Synaptics y Elantech la dan hacia arriba: y = ymax - y_crudo). */
#ifndef SAGUI_PAINEL_LOGICA_H
#define SAGUI_PAINEL_LOGICA_H
#ifdef SAGUI_KERNEL
#include "tipos.h"
#else
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#endif

#define PNL_ESQ   1   /* los mismos bits que BOTAO_* de mouse.h */
#define PNL_DIR   2
#define PNL_MEIO  4

enum { PAINEL_NENHUM, PAINEL_SYNAPTICS, PAINEL_ALPS, PAINEL_ELANTECH };

/* ------------------------------------------------------------ el modelo reconocido */

typedef struct {
    int      tipo;            /* PAINEL_* */
    int      versao;          /* Synaptics: versión mayor del firmware; ALPS: 2; Elantech: 2, 3 o 4 (hardware) */
    int      menor;           /* Synaptics: versión menor */
    int      xmin, xmax;      /* rango útil, ya con la y hacia abajo */
    int      ymin, ymax;
    int      yinv;            /* si > 0: y = yinv - y_crudo (Synaptics, Elantech) */
    int      z_toque;         /* presión para empezar a contar un dedo (0 = el panel cuenta los dedos) */
    int      z_soltar;        /* y para dejar de contarlo (histéresis) */
    int      z_palma;         /* presión desde la que es la palma (0 = no se mira) */
    int      larg_palma;      /* ancho (W de Synaptics) desde el que es la palma (0 = no se mira) */
    bool     largura;         /* el paquete trae el ancho del contacto */
    bool     multidedo;       /* cuenta dedos (dos y tres) */
    bool     agm;             /* Synaptics: paquetes del segundo dedo (W = 2) */
    bool     boton_medio;     /* Synaptics: botón central */
    bool     passthrough;     /* Synaptics: palanca (trackpoint) por el puerto de paso (W = 3) */
    bool     crc;             /* Elantech v3/v4: variante de paquete «con CRC» */
    uint8_t  alps_b0, alps_m0;   /* ALPS v2: valor y máscara del primer byte */
    bool     alps_palanca;    /* ALPS v2 «dualpoint»: z = 127 es la palanca */
    uint32_t firmware;        /* Elantech: versión del firmware (3 bytes) */
    uint32_t capacidades;     /* Synaptics: capacidades (3 bytes) */
    uint32_t ext0c;           /* Synaptics: capacidades extendidas 0x0C */
} painel_modelo_t;

/* Synaptics: respuesta a «identificar» (orden especial 0x00 + E9). byte 1 = 0x47 si es Synaptics;
 * byte 0 = versión menor; byte 2: bits 0-3 versión mayor, 4-7 código de modelo. */
static inline bool syn_identificar(const uint8_t r[3], int *maior, int *menor)
{
    if (r[1] != 0x47) return false;
    *maior = r[2] & 0x0F;
    *menor = r[0];
    return true;
}

/* Capacidades (orden especial 0x02): los tres bytes como un número de 24 bits. */
#define SYN_CAP_EXTENDIDA(c)   (((c) >> 23) & 1)
#define SYN_CAP_N_CONSULTAS(c) (((c) >> 20) & 7)
#define SYN_CAP_MEIO(c)        (((c) >> 18) & 1)
#define SYN_CAP_PASSO(c)       (((c) >> 7) & 1)
#define SYN_CAP_MULTIDEDO(c)   (((c) >> 1) & 1)
#define SYN_CAP_PALMA(c)       ((c) & 1)
/* Capacidades extendidas 0x0C. */
#define SYN_E0C_AGM(c)         (((c) >> 19) & 1)
#define SYN_E0C_MAXIMOS(c)     (((c) >> 17) & 1)
#define SYN_E0C_IMAGEM(c)      (((c) >> 11) & 1)

/* Byte de modo: absoluto, 80 paquetes/s, sin gestos del firmware (los hacemos aquí) y modo W. */
#define SYN_MODO_ABSOLUTO 0x80
#define SYN_MODO_RAPIDO   0x40
#define SYN_MODO_SEM_GEST 0x04
#define SYN_MODO_W        0x01
#define SYN_MODO_SAGUI    (SYN_MODO_ABSOLUTO | SYN_MODO_RAPIDO | SYN_MODO_SEM_GEST | SYN_MODO_W)

/* Rango nominal de la guía cuando el panel no dice sus máximos. */
#define SYN_XMIN 1472
#define SYN_XMAX 5472
#define SYN_YMIN 1408
#define SYN_YMAX 4448

/* Los cuatro argumentos de dos bits de una orden especial (cada uno va tras un E8). */
static inline void syn_argumentos(uint8_t arg, uint8_t a[4])
{
    a[0] = (arg >> 6) & 3; a[1] = (arg >> 4) & 3; a[2] = (arg >> 2) & 3; a[3] = arg & 3;
}

/* Rellena el modelo de un Synaptics. maximos = respuesta a 0x0D (NULL si no se pudo pedir). */
static inline void syn_modelo(painel_modelo_t *m, int maior, int menor, uint32_t cap, uint32_t e0c, const uint8_t *maximos)
{
    m->tipo = PAINEL_SYNAPTICS;
    m->versao = maior; m->menor = menor;
    m->capacidades = cap; m->ext0c = e0c;
    int xmin = SYN_XMIN, xmax = SYN_XMAX, ymin = SYN_YMIN, ymax = SYN_YMAX;
    if (maximos) {
        int x = (maximos[0] << 5) | ((maximos[1] & 0x0F) << 1);
        int y = (maximos[2] << 5) | ((maximos[1] & 0xF0) >> 3);
        if (x > xmin + 1000 && x < 8192) xmax = x;
        if (y > ymin + 1000 && y < 8192) ymax = y;
    }
    m->yinv = ymax;                      /* y hacia abajo: ymax - y */
    m->xmin = xmin; m->xmax = xmax;
    m->ymin = 0; m->ymax = ymax - ymin;
    m->z_toque = 30; m->z_soltar = 25;
    m->z_palma = 200; m->larg_palma = 11;
    m->largura = true;
    m->multidedo = SYN_CAP_MULTIDEDO(cap) != 0;
    m->agm = SYN_E0C_AGM(e0c) || SYN_E0C_IMAGEM(e0c);
    m->boton_medio = SYN_CAP_MEIO(cap) != 0;
    m->passthrough = SYN_CAP_PASSO(cap) != 0;
}

/* ALPS: «informe E6» (E6 x3 + E9) = 00 00 0A o 00 00 64. */
static inline bool alps_e6(const uint8_t r[3])
{
    return r[0] == 0 && r[1] == 0 && (r[2] == 0x0A || r[2] == 0x64);
}

/* ALPS: la firma del «informe E7» (E7 x3 + E9) de los modelos con protocolo v2 que se usan aquí,
 * con el valor y la máscara del primer byte de sus paquetes. Se dejan fuera (vuelven al ratón PS/2
 * de siempre) los v1, los v3 en adelante (Rushmore, Pinnacle, Dolphin: firma 73 02 64 y otras,
 * reconocibles por el modo de órdenes), los que tienen palanca por el puerto de paso y los que
 * mezclan paquetes PS/2 en medio (Dell E6400/E6500: 62 02 14). */
typedef struct { uint8_t e7[3]; uint8_t b0, m0; bool palanca; } alps_firma_t;
static const alps_firma_t alps_firmas_v2[] = {
    { { 0x53, 0x02, 0x0A }, 0xF8, 0xF8, false },
    { { 0x53, 0x02, 0x14 }, 0xF8, 0xF8, false },
    { { 0x60, 0x03, 0xC8 }, 0xF8, 0xF8, false },   /* HP ze1115 */
    { { 0x63, 0x02, 0x0A }, 0xF8, 0xF8, false },
    { { 0x63, 0x02, 0x14 }, 0xF8, 0xF8, false },
    { { 0x63, 0x02, 0x28 }, 0xF8, 0xF8, false },   /* Fujitsu Siemens S6010 */
    { { 0x63, 0x02, 0x3C }, 0x8F, 0x8F, false },   /* Toshiba Satellite S2400 */
    { { 0x63, 0x02, 0x50 }, 0xEF, 0xEF, false },   /* NEC Versa L320 */
    { { 0x63, 0x02, 0x64 }, 0xF8, 0xF8, false },
    { { 0x73, 0x00, 0x0A }, 0xF8, 0xF8, true  },   /* ThinkPad R61 (palanca en el mismo paquete) */
    { { 0x73, 0x02, 0x0A }, 0xF8, 0xF8, false },
    { { 0x73, 0x02, 0x14 }, 0xF8, 0xF8, false },
    { { 0x73, 0x02, 0x50 }, 0xCF, 0xCF, false },   /* Dell Vostro 1400 */
};

static inline bool alps_modelo(painel_modelo_t *m, const uint8_t e7[3])
{
    for (size_t i = 0; i < sizeof alps_firmas_v2 / sizeof alps_firmas_v2[0]; i++) {
        const alps_firma_t *f = &alps_firmas_v2[i];
        if (f->e7[0] != e7[0] || f->e7[1] != e7[1] || f->e7[2] != e7[2]) continue;
        m->tipo = PAINEL_ALPS;
        m->versao = 2;
        m->alps_b0 = f->b0; m->alps_m0 = f->m0; m->alps_palanca = f->palanca;
        m->xmin = 0; m->xmax = 1023; m->ymin = 0; m->ymax = 767;
        m->yinv = 0;
        m->z_toque = 1; m->z_soltar = 1;   /* z > 0: dedo */
        m->z_palma = 0; m->larg_palma = 0;
        return true;
    }
    return false;
}

/* Elantech: «golpe mágico» (E6 x3 + E9) = 3C 03 C8 (o 3C 03 00). */
static inline bool elan_magico(const uint8_t r[3])
{
    return r[0] == 0x3C && r[1] == 0x03 && (r[2] == 0xC8 || r[2] == 0x00);
}

/* Versión del hardware a partir del firmware (consulta 0x01): 2, 3, 4, o 0 si no se usa (v1 y
 * desconocidos vuelven al ratón PS/2). */
static inline int elan_versao_hw(uint32_t fw)
{
    if (fw < 0x020030 || fw == 0x020600) return 0;   /* v1: no lo tratamos */
    switch ((fw >> 16) & 0x0F) {
    case 2: case 4: return 2;
    case 5: return 3;
    case 6: case 7: case 8: case 9: case 10: case 11: case 12: case 13: case 14: case 15: return 4;
    default: return 0;
    }
}

/* Rellena el modelo de un Elantech. id = respuesta a la consulta 0x00 (máximos) en v3/v4, NULL en v2. */
static inline bool elan_modelo(painel_modelo_t *m, uint32_t fw, const uint8_t *id)
{
    int hw = elan_versao_hw(fw);
    if (!hw) return false;
    /* los v2 de antes del firmware 0x020800 no dan la presión y sus paquetes tienen otros bits fijos: no se tratan */
    if (hw == 2 && fw < 0x020800) return false;
    m->tipo = PAINEL_ELANTECH;
    m->versao = hw;
    m->firmware = fw;
    m->crc = (fw & 0x4000) != 0;
    int xmax = 1152, ymax = 768;   /* v2 */
    if (hw >= 3) {
        if (!id) return false;
        xmax = ((id[0] & 0x0F) << 8) | id[1];
        ymax = ((id[0] & 0xF0) << 4) | id[2];
        if (xmax < 256 || ymax < 128) return false;   /* respuesta rara: mejor el ratón de siempre */
    }
    m->xmin = 0; m->xmax = xmax; m->ymin = 0; m->ymax = ymax;
    m->yinv = ymax;
    m->z_toque = 0; m->z_soltar = 0;   /* el panel cuenta los dedos */
    m->z_palma = 0; m->larg_palma = 0;
    m->multidedo = true;
    m->largura = hw == 2;
    return true;
}

/* ------------------------------------------------------------ los paquetes */

/* Un «cuadro»: lo que dice el panel en un paquete, ya normalizado. */
typedef struct {
    int     dedos;        /* 0 = ninguno */
    int     x, y, z;      /* primer dedo, unidades del panel, y hacia abajo */
    int     largura;      /* ancho del contacto, o -1 si no se sabe */
    bool    tem2;         /* hay posición del segundo dedo */
    int     x2, y2;
    uint8_t botoes;       /* botones físicos (PNL_*) */
    bool    relativo;     /* paquete de la palanca: rdx, rdy (y hacia abajo) */
    int     rdx, rdy;
} painel_quadro_t;

typedef struct {
    painel_modelo_t m;
    bool    dedo;                  /* histéresis de la presión */
    bool    agm_valido; int agm_x, agm_y;   /* Synaptics: el último segundo dedo */
    uint8_t bot_palanca;           /* botones de la palanca */
    uint8_t bot_painel;            /* botones del panel en el último paquete con dedo (para la palanca) */
    int     v3_x, v3_y; bool v3_cabeca;     /* Elantech v3: primera mitad de dos dedos */
    uint8_t v4_dedos;              /* Elantech v4: máscara de los 5 dedos */
    uint8_t v4_conhecidos;         /* y de los que ya mandaron su posición (cabecera) */
    int     v4_x[5], v4_y[5];
    uint32_t invalidos;            /* paquetes rechazados (diagnóstico) */
} painel_decod_t;

static inline void painel_decod_iniciar(painel_decod_t *d, const painel_modelo_t *m)
{
    painel_decod_t z = { 0 };
    *d = z;
    d->m = *m;
}

/* ¿Puede ir el byte b en la posición pos (0-5) de un paquete? Sirve para no perder la sincronía:
 * un paquete con un byte que no encaja se descarta desde su comienzo. */
static inline bool painel_byte_ok(const painel_modelo_t *m, int pos, uint8_t b)
{
    switch (m->tipo) {
    case PAINEL_SYNAPTICS:
        if (pos == 0) return (b & 0xC8) == 0x80;
        if (pos == 3) return (b & 0xC8) == 0xC0;
        return true;
    case PAINEL_ALPS:
        if (pos == 0) return (b & m->alps_m0) == m->alps_b0;
        return (b & 0x80) == 0;
    case PAINEL_ELANTECH:
        if (m->versao == 2) {
            if (pos == 0) return (b & 0x0C) == 0x04;
            if (pos == 3) return (b & 0x0F) == 0x02;
            return true;
        }
        if (m->versao == 3) {
            if (m->crc) return pos != 3 || (b & 0x08) == 0x08;
            if (pos == 0) return (b & 0x0C) == 0x04 || (b & 0x0C) == 0x0C;
            if (pos == 3) return (b & 0xCF) == 0x02 || (b & 0xCE) == 0x0C;
            return true;
        }
        if (m->crc) return pos != 3 || (b & 0x08) == 0x00;
        if (pos == 0) return (b & 0x08) == 0x00;
        if (pos == 3) return (b & 0x1C) == 0x10;
        return true;
    default:
        return false;
    }
}

static inline int pnl_conta_bits(unsigned v)
{
    int n = 0;
    for (; v; v &= v - 1) n++;
    return n;
}

/* Dedo presente según la presión, con histéresis (paneles que no cuentan dedos). */
static inline bool pnl_dedo_por_z(painel_decod_t *d, int z)
{
    if (d->dedo) d->dedo = z >= d->m.z_soltar;
    else d->dedo = z >= d->m.z_toque;
    return d->dedo;
}

static inline int synaptics_decodificar(painel_decod_t *d, const uint8_t p[6], painel_quadro_t *q)
{
    const painel_modelo_t *m = &d->m;
    int w = ((p[0] & 0x30) >> 2) | ((p[0] & 0x04) >> 1) | ((p[3] & 0x04) >> 2);
    if (w == 3 && m->passthrough) {
        /* la palanca: su paquete PS/2 de 3 bytes va en los bytes 1, 4 y 5 */
        int dx = p[4] - ((p[1] & 0x10) ? 256 : 0);
        int dy = p[5] - ((p[1] & 0x20) ? 256 : 0);
        d->bot_palanca = p[1] & 7;
        q->relativo = true;
        q->rdx = dx; q->rdy = -dy;
        q->botoes = d->bot_palanca | d->bot_painel;
        return 1;
    }
    int x = ((p[3] & 0x10) << 8) | ((p[1] & 0x0F) << 8) | p[4];
    int y = ((p[3] & 0x20) << 7) | ((p[1] & 0xF0) << 4) | p[5];
    int z = p[2];
    if (w == 2 && m->agm) {
        /* paquete del segundo dedo, a media resolución */
        d->agm_x = (((p[4] & 0x0F) << 8) | p[1]) << 1;
        d->agm_y = m->yinv - ((((p[4] & 0xF0) << 4) | p[2]) << 1);
        d->agm_valido = true;
        return 0;
    }
    uint8_t b = p[0] & 3;   /* bit 0 izquierdo, bit 1 derecho */
    if (m->boton_medio && ((p[0] ^ p[3]) & 1)) b |= PNL_MEIO;
    d->bot_painel = b;
    q->botoes = b | d->bot_palanca;
    q->x = x; q->y = m->yinv - y; q->z = z;
    q->largura = -1;
    bool dedo = pnl_dedo_por_z(d, z);
    if (!dedo) { q->dedos = 0; d->agm_valido = false; return 1; }
    if (w >= 4) { q->dedos = 1; q->largura = w; d->agm_valido = false; }
    else if (w == 0 && m->multidedo) q->dedos = 2;
    else if (w == 1 && m->multidedo) q->dedos = 3;
    else q->dedos = 1;
    if (q->dedos >= 2 && d->agm_valido) { q->tem2 = true; q->x2 = d->agm_x; q->y2 = d->agm_y; }
    return 1;
}

static inline int alps_decodificar(painel_decod_t *d, const uint8_t p[6], painel_quadro_t *q)
{
    const painel_modelo_t *m = &d->m;
    int x = p[1] | ((p[2] & 0x78) << 4);
    int y = p[4] | ((p[3] & 0x70) << 3);
    int z = p[5];
    uint8_t b = p[3] & 7;
    if (m->alps_palanca && z == 127) {
        /* la palanca: x e y son desplazamientos con signo (10 y 9 bits) */
        q->relativo = true;
        q->rdx = x > 383 ? x - 768 : x;
        q->rdy = y > 255 ? y - 512 : y;
        d->bot_palanca = b;
        q->botoes = b | d->bot_painel;
        return 1;
    }
    bool ges = p[2] & 1, fin = p[2] & 2;
    if (ges && !fin) z = 40;   /* toque del firmware: un contacto corto */
    d->bot_painel = b;
    q->botoes = b | d->bot_palanca;
    q->x = x; q->y = y; q->z = z;
    q->largura = -1;
    q->dedos = pnl_dedo_por_z(d, z) ? 1 : 0;
    return 1;
}

static inline int elan_v2_decodificar(painel_decod_t *d, const uint8_t p[6], painel_quadro_t *q)
{
    const painel_modelo_t *m = &d->m;
    int dedos = (p[0] & 0xC0) >> 6;
    q->botoes = p[0] & 3;
    q->largura = -1;
    q->dedos = dedos;
    if (dedos == 1 || dedos == 3) {
        q->x = ((p[1] & 0x0F) << 8) | p[2];
        q->y = m->yinv - (((p[4] & 0x0F) << 8) | p[5]);
        q->z = (p[1] & 0xF0) | ((p[4] & 0xF0) >> 4);
        q->largura = ((p[0] & 0x30) >> 2) | ((p[3] & 0x30) >> 4);
    } else if (dedos == 2) {
        /* dos dedos: cada uno a un cuarto de resolución */
        q->x = (((p[0] & 0x10) << 4) | p[1]) << 2;
        q->y = m->yinv - ((((p[0] & 0x20) << 3) | p[2]) << 2);
        q->x2 = (((p[3] & 0x10) << 4) | p[4]) << 2;
        q->y2 = m->yinv - ((((p[3] & 0x20) << 3) | p[5]) << 2);
        q->tem2 = true;
        q->z = 0;
    }
    return 1;
}

static inline int elan_v3_decodificar(painel_decod_t *d, const uint8_t p[6], painel_quadro_t *q)
{
    const painel_modelo_t *m = &d->m;
    static const uint8_t rebote[6] = { 0xC4, 0xFF, 0xFF, 0x02, 0xFF, 0xFF };
    bool igual = true;
    for (int i = 0; i < 6; i++) if (p[i] != rebote[i]) igual = false;
    if (igual) return 0;   /* paquete de rebote del firmware: no dice nada */
    bool cabeca, cauda;
    if (m->crc) { cabeca = (p[3] & 0x09) == 0x08; cauda = (p[3] & 0x09) == 0x09; }
    else { cabeca = (p[0] & 0x0C) == 0x04 && (p[3] & 0xCF) == 0x02; cauda = (p[0] & 0x0C) == 0x0C && (p[3] & 0xCE) == 0x0C; }
    if (!cabeca && !cauda) { d->invalidos++; return -1; }
    int dedos = (p[0] & 0xC0) >> 6;
    int x = ((p[1] & 0x0F) << 8) | p[2];
    int y = m->yinv - (((p[4] & 0x0F) << 8) | p[5]);
    q->botoes = p[0] & 3;
    q->largura = -1;
    if (dedos == 2) {
        if (cabeca) { d->v3_x = x; d->v3_y = y; d->v3_cabeca = true; return 0; }
        if (!d->v3_cabeca) return 0;
        d->v3_cabeca = false;
        q->dedos = 2;
        q->x = d->v3_x; q->y = d->v3_y;
        q->tem2 = true; q->x2 = x; q->y2 = y;
        return 1;
    }
    if (!cabeca) return 0;
    d->v3_cabeca = false;
    q->dedos = dedos;
    q->x = x; q->y = y;
    q->z = (p[1] & 0xF0) | ((p[4] & 0xF0) >> 4);
    return 1;
}

static inline int elan_v4_decodificar(painel_decod_t *d, const uint8_t p[6], painel_quadro_t *q)
{
    const painel_modelo_t *m = &d->m;
    int tipo = p[3] & 3;
    if (tipo == 0) {                          /* estado: qué dedos tocan */
        d->v4_dedos = p[1] & 0x1F;
        d->v4_conhecidos &= d->v4_dedos;
    } else if (tipo == 1) {                   /* cabecera: la posición de un dedo */
        int id = ((p[3] & 0xE0) >> 5) - 1;
        if (id < 0 || id > 4) { d->invalidos++; return -1; }
        d->v4_x[id] = ((p[1] & 0x0F) << 8) | p[2];
        d->v4_y[id] = m->yinv - (((p[4] & 0x0F) << 8) | p[5]);
        d->v4_dedos |= (uint8_t)(1u << id);
        d->v4_conhecidos |= (uint8_t)(1u << id);
    } else if (tipo == 2) {                   /* movimiento de uno o dos dedos, en relativo */
        int peso = (p[0] & 0x10) ? 5 : 1;
        int id = ((p[0] & 0xE0) >> 5) - 1;
        if (id >= 0 && id <= 4) { d->v4_x[id] += (int8_t)p[1] * peso; d->v4_y[id] -= (int8_t)p[2] * peso; }
        id = ((p[3] & 0xE0) >> 5) - 1;
        if (id >= 0 && id <= 4) { d->v4_x[id] += (int8_t)p[4] * peso; d->v4_y[id] -= (int8_t)p[5] * peso; }
    } else { d->invalidos++; return -1; }
    /* un dedo nuevo cuya cabecera aún no llegó: su posición no se sabe; se espera (sin cuadro) */
    if (d->v4_dedos & ~d->v4_conhecidos) return 0;
    q->botoes = p[0] & 3;
    q->largura = -1;
    q->dedos = pnl_conta_bits(d->v4_dedos);
    int n = 0;
    for (int i = 0; i < 5 && n < 2; i++) {
        if (!(d->v4_dedos & (1u << i))) continue;
        if (n == 0) { q->x = d->v4_x[i]; q->y = d->v4_y[i]; }
        else { q->tem2 = true; q->x2 = d->v4_x[i]; q->y2 = d->v4_y[i]; }
        n++;
    }
    return 1;
}

/* Un paquete de 6 bytes. 1 = hay cuadro en q; 0 = consumido sin cuadro (mitad de algo); -1 = inválido. */
static inline int painel_decodificar(painel_decod_t *d, const uint8_t p[6], painel_quadro_t *q)
{
    painel_quadro_t z = { 0 };
    *q = z;
    for (int i = 0; i < 6; i++)
        if (!painel_byte_ok(&d->m, i, p[i])) { d->invalidos++; return -1; }
    switch (d->m.tipo) {
    case PAINEL_SYNAPTICS: return synaptics_decodificar(d, p, q);
    case PAINEL_ALPS:      return alps_decodificar(d, p, q);
    case PAINEL_ELANTECH:
        if (d->m.versao == 2) return elan_v2_decodificar(d, p, q);
        if (d->m.versao == 3) return elan_v3_decodificar(d, p, q);
        return elan_v4_decodificar(d, p, q);
    default: return -1;
    }
}

/* ------------------------------------------------------------ los gestos */

typedef struct {
    bool toque;          /* tocar = clic (dos dedos = derecho, tres = central) */
    bool dois_dedos;     /* desplazamiento con dos dedos (vertical y horizontal) */
    bool borda;          /* desplazamiento por el borde derecho (vertical) y el inferior (horizontal) */
    bool natural;        /* sentido natural: el contenido sigue a los dedos */
    int  sensib;         /* 1-10 (5 = normal) */
    int  ms_teclado;     /* no hacer caso del panel durante estos ms tras una tecla (0 = nunca) */
    bool desligado;      /* el panel no mueve ni pulsa (ratón USB enchufado, o apagado) */
} painel_conf_t;

static inline void painel_conf_padrao(painel_conf_t *c)
{
    c->toque = true; c->dois_dedos = true; c->borda = false; c->natural = false;
    c->sensib = 5; c->ms_teclado = 300; c->desligado = false;
}

/* Tiempos (ms) y distancias (en milésimas del ancho o del alto del panel). */
#define PNL_MS_TOQUE      180   /* un toque dura menos que esto */
#define PNL_MS_DUPLO      220   /* espera de un segundo toque (doble toque = arrastrar o doble clic) */
#define PNL_MIL_TOQUE      40   /* y se mueve menos que esto */
#define PNL_MIL_BORDA_D    70   /* franja del borde derecho */
#define PNL_MIL_BORDA_I    90   /* franja del borde inferior */
#define PNL_MIL_PASSO_V    55   /* desplazamiento por muesca de rueda */
#define PNL_MIL_PASSO_H    45

enum { PNL_MOVER, PNL_ROLAR2, PNL_BORDA_V, PNL_BORDA_H };
enum { PNL_T_NADA, PNL_T_PENDENTE, PNL_T_ARRASTE };

typedef struct {
    int     dx, dy;      /* píxeles, y hacia abajo */
    int     rv, rh;      /* muescas de rueda: + hacia abajo / hacia la derecha */
    uint8_t botoes;      /* estado de los botones tras este evento */
} painel_evento_t;

#define PNL_EVENTOS_MAX 6
typedef struct {
    int n;
    painel_evento_t e[PNL_EVENTOS_MAX];
} painel_saida_t;

typedef struct {
    bool     contato;          /* hay dedos */
    bool     ignorar;          /* este contacto no cuenta (palma, al escribir, panel apagado) */
    int      modo;             /* PNL_MOVER, ... */
    bool     rolou;            /* hubo desplazamiento en este contacto */
    bool     fisico;           /* se pulsó un botón físico durante este contacto (no es un toque) */
    uint32_t t_ini, t_ult;     /* comienzo del contacto y último cuadro */
    int      x_ini, y_ini, x_ant, y_ant;
    int      desvio;           /* lo más lejos que llegó del punto inicial (|dx| + |dy|) */
    int      dedos_ant, dedos_max;
    int      resto_x, resto_y; /* fracciones de píxel (x256) */
    int      acc_v, acc_h;     /* desplazamiento acumulado sin muesca */
    int      toque;            /* PNL_T_* */
    uint32_t t_toque;
    uint8_t  bot_toque;        /* botón que mantiene un toque */
    uint8_t  bot_fis;          /* botones físicos */
    uint8_t  bot_enviados;     /* lo último que se envió */
    uint32_t toques, rolagens, palmas, teclado;   /* contadores (diagnóstico) */
} painel_gestos_t;

static inline void painel_gestos_iniciar(painel_gestos_t *g)
{
    painel_gestos_t z = { 0 };
    *g = z;
}

static inline void pnl_emitir(painel_gestos_t *g, painel_saida_t *s, int dx, int dy, int rv, int rh)
{
    uint8_t b = g->bot_fis | g->bot_toque;
    if (!dx && !dy && !rv && !rh && b == g->bot_enviados) return;
    /* juntar con el anterior si los botones no cambian (o si ya no cabe otro) */
    if (s->n > 0 && (s->e[s->n - 1].botoes == b || s->n == PNL_EVENTOS_MAX)) {
        painel_evento_t *e = &s->e[s->n - 1];
        e->dx += dx; e->dy += dy; e->rv += rv; e->rh += rh;
        e->botoes = b;
    } else {
        painel_evento_t e = { dx, dy, rv, rh, b };
        s->e[s->n++] = e;
    }
    g->bot_enviados = b;
}

static inline int pnl_abs(int v) { return v < 0 ? -v : v; }

/* Movimiento con aceleración: lento, el ancho del panel son unos 700 píxeles; rápido, hasta el triple. */
static inline void pnl_acelerar(painel_gestos_t *g, const painel_modelo_t *m, const painel_conf_t *c,
                                int dx, int dy, int *px, int *py)
{
    int larg = m->xmax - m->xmin;
    if (larg < 100) larg = 100;
    int v = (pnl_abs(dx) + pnl_abs(dy)) * 1000 / larg;   /* milésimas del ancho por paquete */
    int f = v <= 5 ? 256 : v >= 40 ? 768 : 256 + (v - 5) * 512 / 35;
    int s = c->sensib < 1 ? 1 : c->sensib > 10 ? 10 : c->sensib;
    /* píxeles x256 por el ancho entero: 700 * 256, por el factor (f / 256) y la sensibilidad (s / 5).
     * Solo cuentas de 32 bits (el kernel no tiene división de 64): todo /16. */
    int ganho = 700 * f * s / 5 / 16;
    int l16 = larg / 16;
    if (dx > 2000) dx = 2000;
    if (dx < -2000) dx = -2000;
    if (dy > 2000) dy = 2000;
    if (dy < -2000) dy = -2000;
    int ax = dx * ganho / l16 + g->resto_x;
    int ay = dy * ganho / l16 + g->resto_y;
    *px = ax / 256; *py = ay / 256;
    g->resto_x = ax - *px * 256;
    g->resto_y = ay - *py * 256;
}

static inline void pnl_fim_contato(painel_gestos_t *g, const painel_conf_t *c, uint32_t agora, painel_saida_t *s, int larg)
{
    bool foi_toque = !g->ignorar && c->toque && !g->rolou && !g->fisico && g->bot_fis == 0 &&
                     agora - g->t_ini <= PNL_MS_TOQUE && g->desvio <= larg * PNL_MIL_TOQUE / 1000;
    if (foi_toque) {
        g->toques++;
        uint8_t bt = g->dedos_max >= 3 ? PNL_MEIO : g->dedos_max == 2 ? PNL_DIR : PNL_ESQ;
        if (g->toque == PNL_T_ARRASTE) {
            /* segundo toque corto: doble clic (soltar el primero, y otro clic) */
            g->bot_toque = 0; pnl_emitir(g, s, 0, 0, 0, 0);
            g->bot_toque = PNL_ESQ; pnl_emitir(g, s, 0, 0, 0, 0);
            g->toque = PNL_T_PENDENTE; g->t_toque = agora;
        } else if (bt == PNL_ESQ) {
            g->bot_toque = PNL_ESQ; pnl_emitir(g, s, 0, 0, 0, 0);
            g->toque = PNL_T_PENDENTE; g->t_toque = agora;   /* se suelta al acabar la espera */
        } else {
            g->bot_toque = bt; pnl_emitir(g, s, 0, 0, 0, 0);
            g->bot_toque = 0; pnl_emitir(g, s, 0, 0, 0, 0);
            g->toque = PNL_T_NADA;
        }
    } else if (g->toque == PNL_T_ARRASTE) {
        g->bot_toque = 0; g->toque = PNL_T_NADA;
        pnl_emitir(g, s, 0, 0, 0, 0);
    }
    g->contato = false;
}

/* Un cuadro del panel. agora y tecla en ms (tecla = cuándo se pulsó la última tecla). */
static inline void painel_gestos_quadro(painel_gestos_t *g, const painel_modelo_t *m, const painel_conf_t *c,
                                        const painel_quadro_t *q, uint32_t agora, uint32_t tecla, painel_saida_t *s)
{
    s->n = 0;
    g->t_ult = agora;
    int larg = m->xmax - m->xmin, alt = m->ymax - m->ymin;
    if (larg < 100) larg = 100;
    if (alt < 100) alt = 100;
    if (c->desligado) {
        /* el panel no hace nada; se sueltan los botones que quedasen */
        g->bot_fis = 0; g->bot_toque = 0; g->toque = PNL_T_NADA; g->contato = false;
        pnl_emitir(g, s, 0, 0, 0, 0);
        return;
    }
    if (q->relativo) {   /* la palanca: tal cual */
        g->bot_fis = q->botoes;
        pnl_emitir(g, s, q->rdx, q->rdy, 0, 0);
        return;
    }
    g->bot_fis = q->botoes;
    if (q->botoes && g->contato) g->fisico = true;   /* un clic de verdad (clickpad): al levantar no hay toque */
    /* un toque pendiente cuya espera ya pasó: se suelta antes de nada */
    if (g->toque == PNL_T_PENDENTE && agora - g->t_toque > PNL_MS_DUPLO) {
        g->bot_toque = 0; g->toque = PNL_T_NADA;
        pnl_emitir(g, s, 0, 0, 0, 0);
    }
    if (q->dedos == 0) {
        if (g->contato) pnl_fim_contato(g, c, agora, s, larg);
        pnl_emitir(g, s, 0, 0, 0, 0);   /* botones físicos */
        g->dedos_ant = 0;
        return;
    }
    bool palma = (m->larg_palma && q->largura >= m->larg_palma) || (m->z_palma && q->z >= m->z_palma);
    if (!g->contato) {
        g->contato = true;
        g->t_ini = agora;
        g->x_ini = g->x_ant = q->x; g->y_ini = g->y_ant = q->y;
        g->desvio = 0; g->dedos_max = q->dedos; g->rolou = false; g->fisico = q->botoes != 0;
        g->resto_x = g->resto_y = 0; g->acc_v = g->acc_h = 0;
        g->ignorar = false;
        if (palma) { g->ignorar = true; g->palmas++; }
        else if (c->ms_teclado > 0 && agora - tecla < (uint32_t)c->ms_teclado) { g->ignorar = true; g->teclado++; }
        g->modo = PNL_MOVER;
        if (g->toque == PNL_T_PENDENTE && q->dedos == 1 && !g->ignorar) g->toque = PNL_T_ARRASTE;
        else if (g->toque == PNL_T_PENDENTE) { g->bot_toque = 0; g->toque = PNL_T_NADA; }
        if (q->dedos >= 2 && c->dois_dedos) g->modo = PNL_ROLAR2;
        else if (q->dedos == 1 && c->borda && g->toque != PNL_T_ARRASTE) {
            if (q->x >= m->xmax - larg * PNL_MIL_BORDA_D / 1000) g->modo = PNL_BORDA_V;
            else if (q->y >= m->ymax - alt * PNL_MIL_BORDA_I / 1000) g->modo = PNL_BORDA_H;
        }
        g->dedos_ant = q->dedos;
        pnl_emitir(g, s, 0, 0, 0, 0);
        return;
    }
    if (palma && !g->ignorar && !g->rolou && g->desvio <= larg * PNL_MIL_TOQUE / 1000) { g->ignorar = true; g->palmas++; }
    if (q->dedos > g->dedos_max) g->dedos_max = q->dedos;
    if (q->dedos != g->dedos_ant) {
        /* cambió el número de dedos: la posición del «primer dedo» puede saltar; se toma de nuevo */
        g->x_ant = q->x; g->y_ant = q->y;
        g->resto_x = g->resto_y = 0;
        if (q->dedos >= 2 && c->dois_dedos && g->toque != PNL_T_ARRASTE) { g->modo = PNL_ROLAR2; g->acc_v = g->acc_h = 0; }
        g->dedos_ant = q->dedos;
        pnl_emitir(g, s, 0, 0, 0, 0);
        return;
    }
    /* siempre el primer dedo (también con dos: el punto medio saltaría cuando llega o se pierde la
     * posición del segundo, que en Synaptics viene en paquetes aparte) */
    int x = q->x, y = q->y;
    int dx = x - g->x_ant, dy = y - g->y_ant;
    g->x_ant = x; g->y_ant = y;
    int dv = pnl_abs(q->x - g->x_ini) + pnl_abs(q->y - g->y_ini);
    if (dv > g->desvio) g->desvio = dv;
    if (g->ignorar) { pnl_emitir(g, s, 0, 0, 0, 0); return; }
    int sinal = c->natural ? -1 : 1;
    int pv = alt * PNL_MIL_PASSO_V / 1000, ph = larg * PNL_MIL_PASSO_H / 1000;
    if (pv < 1) pv = 1;
    if (ph < 1) ph = 1;
    switch (g->modo) {
    case PNL_ROLAR2: {
        g->acc_v += dy; g->acc_h += dx;
        /* un eje cada vez: el que más se movió */
        int nv = 0, nh = 0;
        if (pnl_abs(g->acc_v) >= pnl_abs(g->acc_h)) { nv = g->acc_v / pv; g->acc_v -= nv * pv; if (nv) g->acc_h = 0; }
        else { nh = g->acc_h / ph; g->acc_h -= nh * ph; if (nh) g->acc_v = 0; }
        if (nv || nh) { g->rolou = true; g->rolagens++; }
        pnl_emitir(g, s, 0, 0, nv * sinal, nh * sinal);
        break;
    }
    case PNL_BORDA_V: {
        g->acc_v += dy;
        int nv = g->acc_v / pv; g->acc_v -= nv * pv;
        if (nv) { g->rolou = true; g->rolagens++; }
        pnl_emitir(g, s, 0, 0, nv * sinal, 0);
        break;
    }
    case PNL_BORDA_H: {
        g->acc_h += dx;
        int nh = g->acc_h / ph; g->acc_h -= nh * ph;
        if (nh) { g->rolou = true; g->rolagens++; }
        pnl_emitir(g, s, 0, 0, 0, nh * sinal);
        break;
    }
    default: {
        int px, py;
        pnl_acelerar(g, m, c, dx, dy, &px, &py);
        pnl_emitir(g, s, px, py, 0, 0);
        break;
    }
    }
}

/* Llamado a menudo (cada tic): suelta el clic de un toque cuando ya no puede llegar el segundo, y
 * da por levantado un contacto del que no llega nada hace rato (sin clic: no se sabe qué fue). */
#define PNL_MS_SILENCIO 400
static inline void painel_gestos_tique(painel_gestos_t *g, uint32_t agora, painel_saida_t *s)
{
    s->n = 0;
    if (g->contato && agora - g->t_ult > PNL_MS_SILENCIO) {
        g->contato = false; g->dedos_ant = 0;
        if (g->toque == PNL_T_ARRASTE) { g->bot_toque = 0; g->toque = PNL_T_NADA; }
        pnl_emitir(g, s, 0, 0, 0, 0);
    }
    if (g->toque == PNL_T_PENDENTE && !g->contato && agora - g->t_toque > PNL_MS_DUPLO) {
        g->bot_toque = 0; g->toque = PNL_T_NADA;
        pnl_emitir(g, s, 0, 0, 0, 0);
    }
}

#endif
