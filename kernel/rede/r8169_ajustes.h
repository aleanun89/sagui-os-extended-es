/* r8169_ajustes.h - ajustes en caliente del driver r8169 (D124): RxConfig,
 * RxMaxSize, tamaño del búfer en el descriptor y "multi" (bit 14 de
 * RxConfig), pedidos con "ifconfig eth0 r8169 ..." para probar variantes en
 * el hardware real sin recompilar. Código propio, sin nada de Linux: lo usa
 * también la compilación sin GPL (D109). Es un cabecero para probarlo en el
 * anfitrión (tests/h96_realtek_8168e.py). */
#ifndef SAGUI_R8169_AJUSTES_H
#define SAGUI_R8169_AJUSTES_H

#define R8169_MULTI        (1u << 14)   /* RxConfig: leer varios descriptores de una vez */
#define R8169_ACEITAR      0x0Eu        /* RxConfig: difusión, multicast y nuestra MAC */
#define R8169_DESC_MAX     0x3FFFu      /* el tamaño cabe en 14 bits del descriptor */

typedef struct {
    uint32_t rxcfg;      /* lo que se escribe en RxConfig */
    uint32_t rms;        /* RxMaxSize */
    uint32_t buf;        /* tamaño del búfer en el descriptor */
} r8169_ajustes_t;

/* Aplica a *a lo pedido en *p (abi.h, ajuste_driver_t). "paso" es la
 * distancia entre búferes (lo que hay reservado para cada uno). Devuelve
 * 0 o -1 si un valor no vale; en ese caso *a no cambia. */
static inline int r8169_aplicar_ajuste(r8169_ajustes_t *a, const ajuste_driver_t *p, uint32_t paso)
{
    r8169_ajustes_t n = *a;
    if (p->mascara & AJUSTE_RXCFG) {
        n.rxcfg = p->rxcfg;
        if (!(n.rxcfg & 0x3F)) n.rxcfg |= R8169_ACEITAR;   /* sin bits de aceptar no entraría nada */
    }
    if (p->mascara & AJUSTE_MULTI) n.rxcfg = p->multi ? (n.rxcfg | R8169_MULTI) : (n.rxcfg & ~R8169_MULTI);
    if (p->mascara & AJUSTE_RMS) {
        if (p->rms < 64 || p->rms > 0x4000) return -1;
        n.rms = p->rms;
    }
    if (p->mascara & AJUSTE_BUF) {
        uint32_t lim = paso < R8169_DESC_MAX ? paso : R8169_DESC_MAX;
        if (p->buf < 1536 || p->buf > lim) return -1;   /* tiene que caber una trama entera */
        n.buf = p->buf;
    }
    *a = n;
    return 0;
}

#endif
