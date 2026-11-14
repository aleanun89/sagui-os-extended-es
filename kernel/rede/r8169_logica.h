/* r8169_logica.h - a parte do driver r8169 que não toca no hardware (D091):
 * revisão do chip pelo XID, perfil de inicialização e decodificação do
 * descritor de recepção. Fica num cabeçalho para ser testada no anfitrião
 * (tests/h70_realtek_moderno.py), já que o QEMU não emula a RTL8168. */
#ifndef SAGUI_R8169_LOGICA_H
#define SAGUI_R8169_LOGICA_H
#include "../terceros/linux/r8169/r8169_versoes.h"

#define R8169_OWN (1u << 31)
#define R8169_EOR (1u << 30)
#define R8169_FS  (1u << 29)
#define R8169_LS  (1u << 28)
#define R8169_ETH_MAX 1514   /* quadro sem o CRC (o mesmo ETH_MAX de rede.h) */

/* A versão MAC (RTL_GIGA_MAC_VER_xx) para o valor de TxConfig, pela tabela
 * do Linux: o primeiro par máscara/valor que bate; 0 = desconhecida. */
static inline int r8169_versao_do_txconfig(uint32_t txconfig, uint32_t *xid)
{
    uint32_t x = (txconfig >> R8169_XID_DESLOCAMENTO) & R8169_XID_MASCARA;
    const r8169_info_mac_t *t = r8169_tabela_mac;
    while ((x & t->mask) != t->val) t++;
    if (xid) *xid = x;
    return t->ver;
}

static inline const char *r8169_nome_da_versao(int v)
{
    for (uint32_t i = 0; i < sizeof r8169_nomes_chip / sizeof r8169_nomes_chip[0]; i++)
        if (r8169_nomes_chip[i].ver == v) return r8169_nomes_chip[i].name;
    return "desconocido";
}

/* Perfil de inicialização (ver r8169.c). D124: PERFIL_8168E para la familia
 * RTL8168E/8111E (versiones 32 y 33) y RTL8168E-VL/8111E-VL (34), con el
 * arranque de Linux (rtl_hw_start_8168e_1/_2); el clásico no recibía en la
 * 8168evl del Toshiba L850. */
enum { PERFIL_LEGADO, PERFIL_MODERNO, PERFIL_8168E };
static inline int r8169_perfil_da_versao(int v)
{
    if (v >= 40 && v <= 48) return PERFIL_MODERNO;
    if (v >= 32 && v <= 34) return PERFIL_8168E;
    return PERFIL_LEGADO;
}

/* Nombre del driver para el "ip" según el modelo PCI (antes todo era
 * "rtl810x", también las 8168). */
static inline const char *r8169_nome_driver(uint16_t modelo)
{
    return modelo == 0x8136 ? "rtl810x" : modelo == 0x8169 ? "r8169" : "r8168";
}

/* O que fazer com um descritor de recepção que a placa devolveu (sem OWN).
 * *n recebe o tamanho do quadro sem o CRC (o campo do descritor inclui os 4
 * bytes do CRC, com ou sem RxChkSum/RxVlan, em todas as revisões 8168). */
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
