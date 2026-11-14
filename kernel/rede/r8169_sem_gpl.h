/* r8169_sem_gpl.h - lo mínimo del driver r8169 para la compilación sin GPL
 * (`make SEM_GPL=1`, mejora 057, D109).
 *
 * Sin la tabla de revisiones ni las secuencias de inicio de Linux
 * (kernel/terceros/linux/r8169/, GPL-2.0) el driver no sabe qué chip es: usa
 * siempre el inicio "clásico" propio (el que funciona en el HP dv7), sigue
 * siendo experimental (hay que pedirlo con "ifconfig eth0 experimental") y
 * las RTL8111G/H modernas quedan sin soporte. Las definiciones de aquí son
 * las del descriptor y de las interrupciones según las hojas de datos
 * públicas de Realtek, escritas para este proyecto. */
#ifndef SAGUI_R8169_SEM_GPL_H
#define SAGUI_R8169_SEM_GPL_H
#include "tipos.h"

#define R8169_XID_DESLOCAMENTO 20
#define R8169_XID_MASCARA      0xfcf

#define R8169_OWN (1u << 31)
#define R8169_EOR (1u << 30)
#define R8169_FS  (1u << 29)
#define R8169_LS  (1u << 28)
#define R8169_ETH_MAX 1514

/* Bits de opts1 de un descriptor de recepción devuelto por la tarjeta. */
#define R8169_RxRWT  (1u << 22)
#define R8169_RxRES  (1u << 21)
#define R8169_RxRUNT (1u << 20)
#define R8169_RxCRC  (1u << 19)
#define R8169_RxVlanTag (1u << 16)   /* en opts2 */

/* Bits del registro de interrupciones. */
#define R8169_SYSErr     0x8000
#define R8169_RxFIFOOver 0x0040
#define R8169_LinkChg    0x0020
#define R8169_RxOverflow 0x0010
#define R8169_RxErr      0x0002

/* Sin tabla: la revisión es siempre desconocida (0); solo se guarda el XID. */
static inline int r8169_versao_do_txconfig(uint32_t txconfig, uint32_t *xid)
{
    if (xid) *xid = (txconfig >> R8169_XID_DESLOCAMENTO) & R8169_XID_MASCARA;
    return 0;
}

static inline const char *r8169_nome_da_versao(int v)
{
    (void)v;
    return "desconocido (sin GPL: sin tabla de revisiones)";
}

enum { PERFIL_LEGADO, PERFIL_MODERNO, PERFIL_8168E };

/* Nombre del driver según el modelo PCI (código propio, D124). */
static inline const char *r8169_nome_driver(uint16_t modelo)
{
    return modelo == 0x8136 ? "rtl810x" : modelo == 0x8169 ? "r8169" : "r8168";
}

enum { RX_OK, RX_FRAGMENTADO, RX_ERRO, RX_CURTO, RX_LONGO };
static inline int r8169_classificar(uint32_t opts1, int *n)
{
    *n = (int)(opts1 & 0x3FFF) - 4;
    if ((opts1 & (R8169_FS | R8169_LS)) != (R8169_FS | R8169_LS)) return RX_FRAGMENTADO;
    if (opts1 & R8169_RxRES) return RX_ERRO;
    if (*n < 14) return RX_CURTO;
    if (*n > R8169_ETH_MAX) return RX_LONGO;
    return RX_OK;
}

#endif
