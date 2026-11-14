/* alc_logica.h - la parte pura del driver alc (Atheros AR813x/AR815x, D200):
 * modelos, MAC, descriptores, estado del PHY y valores de los registros de
 * configuración. Sin acceso al hardware, para probarla en el anfitrión
 * (tests/recursos/alc_anfitriao.c, tests/h165_red_alc.py) con el mismo
 * código que compila el kernel. Los registros y sus bits vienen del alc(4)
 * de FreeBSD (kernel/terceros/bsd/alc/alc_regs.h, licencia BSD). */
#ifndef SAGUI_ALC_LOGICA_H
#define SAGUI_ALC_LOGICA_H
#ifdef SAGUI_KERNEL
#include "tipos.h"
#else
#include <stdbool.h>
#include <stdint.h>
#endif
#include "../terceros/bsd/alc/alc_regs.h"

/* ---------------------------------------------------------- modelos */

#define ALC_F_RAPIDA   0x01   /* Fast Ethernet: el PHY no sabe hacer 1000 */
#define ALC_F_APS      0x02   /* ahorro de energía de las AR815x (L1D/L2C nuevas) */
#define ALC_F_CRC32    0x04   /* MAC_CFG con HASH_ALG_CRC32 y SPEED_MODE_SW */
#define ALC_F_SERDES   0x08   /* ralentizar relojes del SERDES tras el reinicio */

typedef struct {
    uint16_t    modelo;
    uint8_t     flags;
    uint16_t    max_trama;      /* lo que va en ALC_FRAME_SIZE */
    const char *nome;
} alc_modelo_t;

static const alc_modelo_t alc_modelos[] = {
    { DEVICEID_ATHEROS_AR8152_B,  ALC_F_RAPIDA | ALC_F_APS | ALC_F_SERDES, ALC_MAX_FRAMELEN_815X,
      "Atheros AR8152 v1.1 Fast Ethernet" },
    { DEVICEID_ATHEROS_AR8152_B2, ALC_F_RAPIDA | ALC_F_APS | ALC_F_CRC32,  ALC_MAX_FRAMELEN_815X,
      "Atheros AR8152 v2.0 Fast Ethernet" },
    { DEVICEID_ATHEROS_AR8151,    ALC_F_APS | ALC_F_CRC32,                ALC_MAX_FRAMELEN_815X,
      "Atheros AR8151 v1.0 Gigabit" },
    { DEVICEID_ATHEROS_AR8151_V2, ALC_F_APS | ALC_F_CRC32 | ALC_F_SERDES, ALC_MAX_FRAMELEN_815X,
      "Atheros AR8151 v2.0 Gigabit" },
    { DEVICEID_ATHEROS_AR8131,    0,                                      ALC_MAX_FRAMELEN_813X,
      "Atheros AR8131 Gigabit" },
    { DEVICEID_ATHEROS_AR8132,    ALC_F_RAPIDA,                           ALC_MAX_FRAMELEN_813X,
      "Atheros AR8132 Fast Ethernet" },
};
#define ALC_N_MODELOS (sizeof alc_modelos / sizeof alc_modelos[0])

static inline const alc_modelo_t *alc_modelo(uint16_t fabricante, uint16_t modelo)
{
    if (fabricante != VENDORID_ATHEROS) return 0;
    for (unsigned i = 0; i < ALC_N_MODELOS; i++)
        if (alc_modelos[i].modelo == modelo) return &alc_modelos[i];
    return 0;
}

/* La 2060 con revisión PCI 0xC0 es la v1.0 (necesita sus propios ajustes del SERDES). */
static inline bool alc_8152_v10(uint16_t modelo, uint8_t revisao)
{
    return modelo == DEVICEID_ATHEROS_AR8152_B && revisao == ATHEROS_AR8152_B_V10;
}

/* ---------------------------------------------------------- MAC */

/* PAR1 tiene los dos primeros bytes; PAR0, los cuatro últimos. */
static inline void alc_mac_de_par(uint32_t par0, uint32_t par1, uint8_t mac[6])
{
    mac[0] = (uint8_t)(par1 >> 8); mac[1] = (uint8_t)par1;
    mac[2] = (uint8_t)(par0 >> 24); mac[3] = (uint8_t)(par0 >> 16);
    mac[4] = (uint8_t)(par0 >> 8); mac[5] = (uint8_t)par0;
}

static inline void alc_mac_para_par(const uint8_t mac[6], uint32_t *par0, uint32_t *par1)
{
    *par0 = (uint32_t)mac[2] << 24 | (uint32_t)mac[3] << 16 | (uint32_t)mac[4] << 8 | mac[5];
    *par1 = (uint32_t)mac[0] << 8 | mac[1];
}

/* Una MAC de verdad: ni todo ceros, ni todo unos, ni de grupo (multicast). */
static inline bool alc_mac_valida(const uint8_t mac[6])
{
    uint8_t o = 0, a = 0xFF;
    for (int i = 0; i < 6; i++) { o |= mac[i]; a &= mac[i]; }
    return o != 0 && a != 0xFF && !(mac[0] & 1);
}

/* ---------------------------------------------------------- descriptores */

/* Un TPD de un solo fragmento: toda la trama en un búfer, con el EOP. */
static inline void alc_tpd_montar(volatile uint32_t d[4], uint32_t fisico, uint32_t n)
{
    d[0] = n & TD_BUFLEN_MASK;          /* sin etiqueta VLAN */
    d[1] = TD_ETHERNET | TD_EOP;        /* sin sumas de control por hardware */
    d[2] = fisico;
    d[3] = 0;                           /* la memoria del SaguiOS está debajo de 4 GB */
}

/* Lo que dice un RRD (rdinfo, status) leído por la CPU. */
typedef enum {
    ALC_RRD_VACIO = 0,     /* la tarjeta todavía no lo escribió */
    ALC_RRD_BIEN,          /* trama buena: bytes (sin el CRC) en *n */
    ALC_RRD_ERROR_CRC,
    ALC_RRD_ERROR_ALINEACION,
    ALC_RRD_ERROR_TRUNCADA,
    ALC_RRD_ERROR_CORTA,   /* runt */
    ALC_RRD_FRAGMENTADA,   /* ocupa más de un búfer (tramas gigantes): se descarta */
    ALC_RRD_TAMANO,        /* tamaño imposible */
    ALC_RRD_SIN_BUFERES,   /* cuenta de búferes 0: el anillo está roto (hay que reiniciar) */
} alc_rrd_t;

typedef struct {
    alc_rrd_t estado;
    uint32_t  indice;      /* primer búfer (RFD) de la trama */
    uint32_t  nbuf;        /* cuántos búferes usa */
    uint32_t  n;           /* bytes de la trama sin el CRC */
} alc_rrd_info_t;

static inline alc_rrd_info_t alc_rrd_decodificar(uint32_t rdinfo, uint32_t status, uint32_t max_trama)
{
    alc_rrd_info_t r = { ALC_RRD_VACIO, 0, 0, 0 };
    if (!(status & RRD_VALID)) return r;
    r.indice = (rdinfo & RRD_RD_IDX_MASK) >> RRD_RD_IDX_SHIFT;
    r.nbuf = (rdinfo & RRD_RD_CNT_MASK) >> RRD_RD_CNT_SHIFT;
    uint32_t bytes = status & RRD_LEN_MASK;   /* incluye los 4 del CRC: la L1C no sabe quitarlo */
    if (r.nbuf == 0) { r.estado = ALC_RRD_SIN_BUFERES; return r; }
    /* Como alc(4): un error de suma o de longitud de protocolo no tira la
     * trama (la pila comprueba las sumas); CRC, alineación, truncada o corta sí. */
    if (status & (RRD_ERR_SUM | RRD_ERR_LENGTH)) {
        if (status & RRD_ERR_CRC)   { r.estado = ALC_RRD_ERROR_CRC; return r; }
        if (status & RRD_ERR_ALIGN) { r.estado = ALC_RRD_ERROR_ALINEACION; return r; }
        if (status & RRD_ERR_TRUNC) { r.estado = ALC_RRD_ERROR_TRUNCADA; return r; }
        if (status & RRD_ERR_RUNT)  { r.estado = ALC_RRD_ERROR_CORTA; return r; }
    }
    if (r.nbuf > 1 || bytes > RX_BUF_SIZE_MAX) { r.estado = ALC_RRD_FRAGMENTADA; return r; }
    if (bytes < 4 + 14 || bytes - 4 > max_trama) { r.estado = ALC_RRD_TAMANO; return r; }
    r.n = bytes - 4;
    r.estado = ALC_RRD_BIEN;
    return r;
}

/* Índices de los anillos: el productor de búferes libres (RFD) es el
 * índice del último búfer entregado a la tarjeta; avanza con lo consumido. */
static inline uint32_t alc_avanzar(uint32_t i, uint32_t k, uint32_t n) { return (i + k) % n; }

/* Descriptores de envío ocupados (entre el consumidor de la tarjeta y nuestro productor). */
static inline uint32_t alc_tx_ocupados(uint32_t prod, uint32_t cons, uint32_t n)
{
    return (prod + n - cons) % n;
}

/* ---------------------------------------------------------- PHY */

#define ALC_MII_BMCR     0x00
#define ALC_MII_BMSR     0x01
#define ALC_MII_PHYID1   0x02
#define ALC_MII_PHYID2   0x03
#define ALC_MII_ANAR     0x04
#define ALC_MII_ANLPAR   0x05
#define ALC_MII_GTCR     0x09   /* control 1000BASE-T */
#define BMCR_RESET       0x8000
#define BMCR_AUTOEN      0x1000
#define BMCR_STARTNEG    0x0200
#define BMSR_LINK        0x0004
#define BMSR_ACOMP       0x0020
#define ANAR_10          0x0020
#define ANAR_10_FD       0x0040
#define ANAR_TX          0x0080
#define ANAR_TX_FD       0x0100
#define ANAR_CSMA        0x0001
#define GTCR_ADV_1000TFDX 0x0200
#define GTCR_ADV_1000THDX 0x0100

typedef struct {
    bool     enlace;
    uint16_t velocidade;   /* 10, 100, 1000; 0 = sin resolver */
    bool     completo;     /* full-duplex */
} alc_enlace_t;

/* Velocidad y dúplex: del registro de estado del PHY F1 (lo que hace atphy)
 * y, si el PHY no dice que está resuelto, de lo común entre lo anunciado
 * (ANAR) y lo del otro lado (ANLPAR), como cualquier PHY 802.3. */
static inline alc_enlace_t alc_phy_resolver(uint16_t bmsr, uint16_t ssr, uint16_t anar, uint16_t anlpar, bool rapida)
{
    alc_enlace_t e = { false, 0, false };
    if (!(bmsr & BMSR_LINK)) return e;
    if (ssr != 0xFFFF && (ssr & ATPHY_SSR_SPD_DPLX_RESOLVED)) {
        switch (ssr & ATPHY_SSR_SPEED_MASK) {
        case ATPHY_SSR_1000MBS: e.velocidade = 1000; break;
        case ATPHY_SSR_100MBS:  e.velocidade = 100; break;
        case ATPHY_SSR_10MBS:   e.velocidade = 10; break;
        default:                e.velocidade = 0; break;
        }
        e.completo = (ssr & ATPHY_SSR_DUPLEX) != 0;
    } else {
        uint16_t comum = anar & anlpar;
        if (comum & ANAR_TX_FD)      { e.velocidade = 100; e.completo = true; }
        else if (comum & ANAR_TX)    { e.velocidade = 100; }
        else if (comum & ANAR_10_FD) { e.velocidade = 10; e.completo = true; }
        else if (comum & ANAR_10)    { e.velocidade = 10; }
    }
    /* La AR8132/AR8152 usan el modelo del PHY gigabit pero no pueden hacer 1000. */
    if (e.velocidade == 1000 && rapida) e.velocidade = 0;
    e.enlace = e.velocidade != 0;
    return e;
}

/* Lo que se anuncia en la autonegociación (sin pausa: el MAC no la usa). */
static inline uint16_t alc_anar(void) { return ANAR_TX_FD | ANAR_TX | ANAR_10_FD | ANAR_10 | ANAR_CSMA; }
static inline uint16_t alc_gtcr(bool rapida) { return rapida ? 0 : GTCR_ADV_1000TFDX | GTCR_ADV_1000THDX; }

/* ---------------------------------------------------------- registros */

/* MAC_CFG inicial (alc_init_locked): CRC y relleno automático; la
 * velocidad de verdad se pone al subir el enlace. Con difusión (ARP, DHCP)
 * y sin multicast. */
static inline uint32_t alc_mac_cfg_inicial(uint8_t flags)
{
    uint32_t r = MAC_CFG_TX_CRC_ENB | MAC_CFG_TX_AUTO_PAD | MAC_CFG_FULL_DUPLEX |
                 ((MAC_CFG_PREAMBLE_DEFAULT << MAC_CFG_PREAMBLE_SHIFT) & MAC_CFG_PREAMBLE_MASK);
    if (flags & ALC_F_CRC32) r |= MAC_CFG_HASH_ALG_CRC32 | MAC_CFG_SPEED_MODE_SW;
    r |= (flags & ALC_F_RAPIDA) ? MAC_CFG_SPEED_10_100 : MAC_CFG_SPEED_1000;
    r |= MAC_CFG_BCAST;
    return r;
}

/* MAC_CFG con el enlace resuelto (alc_mac_config), sin control de flujo. */
static inline uint32_t alc_mac_cfg_enlace(uint32_t r, uint8_t flags, alc_enlace_t e)
{
    r &= ~(uint32_t)(MAC_CFG_FULL_DUPLEX | MAC_CFG_TX_FC | MAC_CFG_RX_FC | MAC_CFG_SPEED_MASK);
    if (flags & ALC_F_CRC32) r |= MAC_CFG_HASH_ALG_CRC32 | MAC_CFG_SPEED_MODE_SW;
    r |= e.velocidade == 1000 ? MAC_CFG_SPEED_1000 : MAC_CFG_SPEED_10_100;
    if (e.completo) r |= MAC_CFG_FULL_DUPLEX;
    return r;
}

/* Tamaño de ráfaga DMA de PCIe (DevCtl): 0=128 ... 5=4096; alc(4) se queda en 1024. */
static inline uint32_t alc_rafaga(uint32_t codigo) { return codigo > 3 ? 3 : codigo; }
static inline uint32_t alc_rafaga_bytes(uint32_t codigo) { return codigo <= 5 ? 128u << codigo : 0; }

static inline uint32_t alc_txq_cfg(uint16_t modelo, uint32_t rd_rafaga)
{
    uint32_t r = (alc_rafaga_bytes(rd_rafaga) << TXQ_CFG_TX_FIFO_BURST_SHIFT) & TXQ_CFG_TX_FIFO_BURST_MASK;
    if (modelo == DEVICEID_ATHEROS_AR8152_B || modelo == DEVICEID_ATHEROS_AR8152_B2) r >>= 1;
    r |= (TXQ_CFG_TD_BURST_DEFAULT << TXQ_CFG_TD_BURST_SHIFT) & TXQ_CFG_TD_BURST_MASK;
    return r | TXQ_CFG_IP_OPTION_ENB | TXQ_CFG_8023_ENB | TXQ_CFG_ENHANCED_MODE;
}

static inline uint32_t alc_rxq_cfg(uint16_t modelo, uint8_t flags)
{
    uint32_t r = (RXQ_CFG_RD_BURST_DEFAULT << RXQ_CFG_RD_BURST_SHIFT) & RXQ_CFG_RD_BURST_MASK;
    r |= RXQ_CFG_RSS_MODE_DIS;
    if (!(flags & ALC_F_RAPIDA) && modelo != DEVICEID_ATHEROS_AR8151_V2) r |= RXQ_CFG_ASPM_THROUGHPUT_LIMIT_100M;
    return r;
}

static inline uint32_t alc_dma_cfg(bool rcb128, uint32_t rd_rafaga, uint32_t wr_rafaga)
{
    uint32_t r = DMA_CFG_OUT_ORDER | DMA_CFG_RD_REQ_PRI | (rcb128 ? DMA_CFG_RCB_128 : DMA_CFG_RCB_64);
    r |= DMA_CFG_SMB_DIS;   /* los bloques de estadísticas y de mensajes tienen fallos en el silicio */
    r |= (rd_rafaga & DMA_CFG_RD_BURST_MASK) << DMA_CFG_RD_BURST_SHIFT;
    r |= (wr_rafaga & DMA_CFG_WR_BURST_MASK) << DMA_CFG_WR_BURST_SHIFT;
    r |= (DMA_CFG_RD_DELAY_CNT_DEFAULT << DMA_CFG_RD_DELAY_CNT_SHIFT) & DMA_CFG_RD_DELAY_CNT_MASK;
    r |= (DMA_CFG_WR_DELAY_CNT_DEFAULT << DMA_CFG_WR_DELAY_CNT_SHIFT) & DMA_CFG_WR_DELAY_CNT_MASK;
    return r;
}

/* PM_CFG sin ASPM, antes de saber nada del enlace (alc_disable_l0s_l1). */
static inline uint32_t alc_pm_sin_aspm(uint32_t pm)
{
    pm &= ~(uint32_t)(PM_CFG_L1_ENTRY_TIMER_MASK | PM_CFG_CLK_SWH_L1 | PM_CFG_ASPM_L0S_ENB |
                      PM_CFG_ASPM_L1_ENB | PM_CFG_MAC_ASPM_CHK | PM_CFG_SERDES_PD_EX_L1);
    return pm | PM_CFG_SERDES_BUDS_RX_L1_ENB | PM_CFG_SERDES_PLL_L1_ENB | PM_CFG_SERDES_L1_ENB;
}

/* PM_CFG con el enlace arriba (alc_aspm_813x) con L0s y L1 SIEMPRE
 * apagados: es lo que hace alc(4) cuando la BIOS no los encendió. Con APS
 * (AR815x) los relojes del SERDES se gestionan aparte. */
static inline uint32_t alc_pm_enlace(uint32_t pm, uint16_t modelo, uint8_t flags, uint16_t velocidade)
{
    pm &= ~(uint32_t)PM_CFG_SERDES_PD_EX_L1;
    pm &= ~(uint32_t)(PM_CFG_L1_ENTRY_TIMER_MASK | PM_CFG_LCKDET_TIMER_MASK);
    pm |= PM_CFG_MAC_ASPM_CHK;
    pm |= (uint32_t)PM_CFG_LCKDET_TIMER_DEFAULT << PM_CFG_LCKDET_TIMER_SHIFT;
    pm &= ~(uint32_t)(PM_CFG_ASPM_L1_ENB | PM_CFG_ASPM_L0S_ENB);
    if (flags & ALC_F_APS) {
        pm &= ~(uint32_t)(PM_CFG_EN_BUFS_RX_L0S | PM_CFG_SA_DLY_ENB | PM_CFG_HOTRST);
        pm |= (uint32_t)PM_CFG_L1_ENTRY_TIMER_DEFAULT << PM_CFG_L1_ENTRY_TIMER_SHIFT;
        pm &= ~(uint32_t)PM_CFG_PM_REQ_TIMER_MASK;
        pm |= (uint32_t)PM_CFG_PM_REQ_TIMER_DEFAULT << PM_CFG_PM_REQ_TIMER_SHIFT;
        pm |= PM_CFG_SERDES_PD_EX_L1 | PM_CFG_PCIE_RECV;
        pm &= ~(uint32_t)(PM_CFG_SERDES_L1_ENB | PM_CFG_SERDES_PLL_L1_ENB | PM_CFG_SERDES_BUDS_RX_L1_ENB);
        pm |= PM_CFG_CLK_SWH_L1;
        if (velocidade == 100 || velocidade == 1000) {
            pm &= ~(uint32_t)PM_CFG_L1_ENTRY_TIMER_MASK;
            uint32_t t = modelo == DEVICEID_ATHEROS_AR8152_B ? 7 :
                         (modelo == DEVICEID_ATHEROS_AR8152_B2 || modelo == DEVICEID_ATHEROS_AR8151_V2) ? 4 : 15;
            pm |= t << PM_CFG_L1_ENTRY_TIMER_SHIFT;
        }
    } else {
        pm |= PM_CFG_SERDES_L1_ENB | PM_CFG_SERDES_PLL_L1_ENB | PM_CFG_SERDES_BUDS_RX_L1_ENB;
        pm &= ~(uint32_t)(PM_CFG_CLK_SWH_L1 | PM_CFG_ASPM_L1_ENB | PM_CFG_ASPM_L0S_ENB);
    }
    return pm;
}

#endif
