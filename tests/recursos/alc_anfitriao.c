// SPDX-License-Identifier: GPL-2.0-or-later
/* alc_anfitriao.c - pruebas en el anfitrión de la lógica del driver alc
 * (Atheros AR813x/AR815x, D200): modelos, MAC de los PAR, TPD, RRD,
 * resolución del enlace del PHY y valores de los registros. Las compila y
 * ejecuta tests/h165_red_alc.py con el MISMO kernel/rede/alc_logica.h que
 * usa el kernel. Imprime "ok ..." o "FALHA ..." y, al final, "TUDO OK". */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../../kernel/rede/alc_logica.h"

static int falhas, oks;

static void confere(bool c, const char *o_que)
{
    if (c) { oks++; printf("ok %s\n", o_que); }
    else { falhas++; printf("FALHA %s\n", o_que); }
}

static void modelos(void)
{
    const alc_modelo_t *m = alc_modelo(0x1969, 0x2060);
    confere(m && strcmp(m->nome, "Atheros AR8152 v1.1 Fast Ethernet") == 0, "1969:2060 es la AR8152 v1.1");
    confere(m && (m->flags & ALC_F_RAPIDA) && (m->flags & ALC_F_APS) && (m->flags & ALC_F_SERDES) &&
            !(m->flags & ALC_F_CRC32), "AR8152 v1.1: rápida, APS, SERDES lento, sin CRC32 en MAC_CFG");
    confere(m && m->max_trama == 6 * 1024, "AR8152 v1.1: trama máxima 6 KB");
    m = alc_modelo(0x1969, 0x2062);
    confere(m && (m->flags & ALC_F_CRC32) && !(m->flags & ALC_F_SERDES), "AR8152 v2.0: CRC32, sin SERDES lento");
    m = alc_modelo(0x1969, 0x1073);
    confere(m && !(m->flags & ALC_F_RAPIDA) && (m->flags & ALC_F_APS), "AR8151: gigabit con APS");
    m = alc_modelo(0x1969, 0x1063);
    confere(m && m->flags == 0 && m->max_trama == 9 * 1024, "AR8131: gigabit sin APS, 9 KB");
    m = alc_modelo(0x1969, 0x1062);
    confere(m && m->flags == ALC_F_RAPIDA, "AR8132: rápida");
    confere(alc_modelo(0x1969, 0x1091) == 0, "AR8161 (familia 816x) no es de este driver");
    confere(alc_modelo(0x10EC, 0x2060) == 0, "otro fabricante con el mismo modelo: no");
    confere(alc_8152_v10(0x2060, 0xC0) && !alc_8152_v10(0x2060, 0xC1) && !alc_8152_v10(0x2062, 0xC0),
            "AR8152 v1.0 = 2060 con revisión C0");
}

static void mac(void)
{
    uint8_t m[6];
    alc_mac_de_par(0x33445566u, 0x00001122u, m);
    const uint8_t esperado[6] = { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66 };
    confere(memcmp(m, esperado, 6) == 0, "PAR1=1122 PAR0=33445566 -> 11:22:33:44:55:66");
    uint32_t p0, p1;
    alc_mac_para_par(esperado, &p0, &p1);
    confere(p0 == 0x33445566u && p1 == 0x1122u, "y de vuelta a los PAR");
    const uint8_t zero[6] = { 0 }, um[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }, grupo[6] = { 0x01, 0, 0x5E, 0, 0, 1 };
    const uint8_t toshiba[6] = { 0x00, 0x26, 0x6C, 0x12, 0x34, 0x56 };
    confere(!alc_mac_valida(zero) && !alc_mac_valida(um) && !alc_mac_valida(grupo), "ceros, unos y multicast no valen");
    confere(alc_mac_valida(toshiba) && !alc_mac_valida(esperado), "una MAC normal vale; 11:... es de grupo (bit 0)");
}

static void tpd(void)
{
    uint32_t d[4] = { 0xDEADBEEF, 0xDEADBEEF, 0xDEADBEEF, 0xDEADBEEF };
    alc_tpd_montar(d, 0x01234000u, 342);
    confere(d[0] == 342 && d[1] == 0x80000000u && d[2] == 0x01234000u && d[3] == 0,
            "TPD: longitud, solo EOP, dirección baja, alta a cero");
    alc_tpd_montar(d, 0x00100000u, 60);
    confere(d[0] == 60 && (d[1] & TD_EOP), "TPD de 60 bytes");
}

static void rrd(void)
{
    /* Trama buena de 64 bytes con CRC en el búfer 5, un solo búfer. */
    alc_rrd_info_t r = alc_rrd_decodificar(5u << 20 | 1u << 16 | 0x1234, RRD_VALID | 64, 1514);
    confere(r.estado == ALC_RRD_BIEN && r.indice == 5 && r.nbuf == 1 && r.n == 60, "RRD bueno: búfer 5, 60 bytes sin CRC");
    r = alc_rrd_decodificar(5u << 20 | 1u << 16, 64, 1514);
    confere(r.estado == ALC_RRD_VACIO, "sin VALID: nada todavía");
    r = alc_rrd_decodificar(63u << 20 | 1u << 16, RRD_VALID | 1518, 1514);
    confere(r.estado == ALC_RRD_BIEN && r.n == 1514 && r.indice == 63, "trama máxima (1518 con CRC) en el último búfer");
    r = alc_rrd_decodificar(1u << 16, RRD_VALID | RRD_ERR_SUM | RRD_ERR_CRC | 100, 1514);
    confere(r.estado == ALC_RRD_ERROR_CRC && r.nbuf == 1, "error de CRC: se descarta pero devuelve su búfer");
    r = alc_rrd_decodificar(1u << 16, RRD_VALID | RRD_ERR_SUM | RRD_ERR_RUNT | 30, 1514);
    confere(r.estado == ALC_RRD_ERROR_CORTA, "runt");
    r = alc_rrd_decodificar(1u << 16, RRD_VALID | RRD_ERR_SUM | RRD_ERR_TRUNC | 1000, 1514);
    confere(r.estado == ALC_RRD_ERROR_TRUNCADA, "truncada");
    r = alc_rrd_decodificar(1u << 16, RRD_VALID | RRD_ERR_SUM | RRD_ERR_ALIGN | 1000, 1514);
    confere(r.estado == ALC_RRD_ERROR_ALINEACION, "alineación");
    r = alc_rrd_decodificar(1u << 16, RRD_VALID | RRD_ERR_SUM | 200, 1514);
    confere(r.estado == ALC_RRD_BIEN && r.n == 196, "solo error de suma (IP/TCP): pasa, la pila comprueba");
    r = alc_rrd_decodificar(1u << 16, RRD_VALID | RRD_ERR_LENGTH | 200, 1514);
    confere(r.estado == ALC_RRD_BIEN, "longitud de protocolo distinta: pasa (como alc(4))");
    r = alc_rrd_decodificar(10u << 20 | 3u << 16, RRD_VALID | 4000, 1514);
    confere(r.estado == ALC_RRD_FRAGMENTADA && r.nbuf == 3 && r.indice == 10, "trama gigante en 3 búferes: descartada, devuelve 3");
    r = alc_rrd_decodificar(0, RRD_VALID | 64, 1514);
    confere(r.estado == ALC_RRD_SIN_BUFERES, "cuenta de búferes 0: anillo roto");
    r = alc_rrd_decodificar(1u << 16, RRD_VALID | 10, 1514);
    confere(r.estado == ALC_RRD_TAMANO, "más corta que una cabecera: tamaño imposible");
    r = alc_rrd_decodificar(1u << 16, RRD_VALID | 1522, 1514);
    confere(r.estado == ALC_RRD_TAMANO, "con etiqueta VLAN (1518 sin CRC) no cabe en ETH_MAX");
    r = alc_rrd_decodificar(0xFFFu << 20 | 1u << 16, RRD_VALID | 64, 1514);
    confere(r.indice == 0xFFF, "el índice ocupa 12 bits (el driver comprueba el rango)");
}

static void aneis(void)
{
    confere(alc_avanzar(63, 1, 64) == 0 && alc_avanzar(62, 3, 64) == 1, "avanzar da la vuelta");
    confere(alc_tx_ocupados(5, 5, 64) == 0 && alc_tx_ocupados(2, 60, 64) == 6 && alc_tx_ocupados(63, 0, 64) == 63,
            "descriptores de envío ocupados");
}

static void phy(void)
{
    alc_enlace_t e = alc_phy_resolver(0x7809, 0, 0x01E1, 0x45E1, true);
    confere(!e.enlace, "BMSR sin LINK: sin enlace");
    e = alc_phy_resolver(0x796D, ATPHY_SSR_SPD_DPLX_RESOLVED | ATPHY_SSR_100MBS | ATPHY_SSR_DUPLEX | 0x0400, 0x01E1, 0xC5E1, true);
    confere(e.enlace && e.velocidade == 100 && e.completo, "SSR resuelto: 100 full");
    e = alc_phy_resolver(0x796D, ATPHY_SSR_SPD_DPLX_RESOLVED | ATPHY_SSR_10MBS, 0x01E1, 0x0021, true);
    confere(e.enlace && e.velocidade == 10 && !e.completo, "SSR resuelto: 10 half");
    e = alc_phy_resolver(0x796D, ATPHY_SSR_SPD_DPLX_RESOLVED | ATPHY_SSR_1000MBS | ATPHY_SSR_DUPLEX, 0x01E1, 0xC5E1, true);
    confere(!e.enlace, "1000 en un PHY rápido: no vale (AR8132/AR8152)");
    e = alc_phy_resolver(0x796D, ATPHY_SSR_SPD_DPLX_RESOLVED | ATPHY_SSR_1000MBS | ATPHY_SSR_DUPLEX, 0x01E1, 0xC5E1, false);
    confere(e.enlace && e.velocidade == 1000 && e.completo, "1000 full en un gigabit");
    e = alc_phy_resolver(0x796D, 0, 0x01E1, 0x41E1, true);
    confere(e.enlace && e.velocidade == 100 && e.completo, "sin SSR resuelto: ANAR&ANLPAR = 100 full");
    e = alc_phy_resolver(0x796D, 0, 0x01E1, 0x0061, true);
    confere(e.enlace && e.velocidade == 10 && e.completo, "sin SSR: el otro solo hace 10: 10 full");
    e = alc_phy_resolver(0x796D, 0xFFFF, 0x01E1, 0x0081, true);
    confere(e.enlace && e.velocidade == 100 && !e.completo, "SSR 0xFFFF (sin MDIO) no cuenta: 100 half por ANLPAR");
    e = alc_phy_resolver(0x796D, 0, 0x01E1, 0x0001, true);
    confere(!e.enlace, "nada en común: sin enlace");
    confere(alc_anar() == 0x01E1, "ANAR: 10/100 half y full, sin pausa");
    confere(alc_gtcr(true) == 0 && alc_gtcr(false) == 0x0300, "GTCR: 0 en los rápidos, 1000 half+full en los gigabit");
}

static void registros(void)
{
    const alc_modelo_t *m = alc_modelo(0x1969, 0x2060);
    uint32_t r = alc_mac_cfg_inicial(m->flags);
    confere(r == (MAC_CFG_TX_CRC_ENB | MAC_CFG_TX_AUTO_PAD | MAC_CFG_FULL_DUPLEX | (7u << 10) | MAC_CFG_SPEED_10_100 |
                  MAC_CFG_BCAST), "MAC_CFG inicial de la AR8152 v1.1");
    confere(!(r & (MAC_CFG_TX_ENB | MAC_CFG_RX_ENB | MAC_CFG_PROMISC | MAC_CFG_ALLMULTI)),
            "MAC_CFG inicial: TX/RX apagados hasta el enlace, sin promiscuo");
    alc_enlace_t e = { true, 100, false };
    uint32_t c = alc_mac_cfg_enlace(r | MAC_CFG_TX_FC | MAC_CFG_RX_FC, m->flags, e);
    confere(!(c & MAC_CFG_FULL_DUPLEX) && (c & MAC_CFG_SPEED_MASK) == MAC_CFG_SPEED_10_100 && !(c & (MAC_CFG_TX_FC | MAC_CFG_RX_FC)),
            "MAC_CFG con 100 half: sin full-duplex ni control de flujo");
    e.completo = true;
    c = alc_mac_cfg_enlace(r, m->flags, e);
    confere((c & MAC_CFG_FULL_DUPLEX) && (c & MAC_CFG_BCAST), "MAC_CFG con 100 full, conserva la difusión");
    const alc_modelo_t *g = alc_modelo(0x1969, 0x1083);
    e.velocidade = 1000;
    c = alc_mac_cfg_enlace(alc_mac_cfg_inicial(g->flags), g->flags, e);
    confere((c & MAC_CFG_SPEED_MASK) == MAC_CFG_SPEED_1000 && (c & MAC_CFG_HASH_ALG_CRC32) && (c & MAC_CFG_SPEED_MODE_SW),
            "AR8151 v2 a 1000: velocidad 1000, CRC32 y SPEED_MODE_SW");

    /* TXQ: ráfaga de lectura 512 bytes (código 2) -> 512 << 16, la AR8152 lo divide por 2. */
    confere(alc_txq_cfg(0x2060, 2) == ((256u << 16) | 5 | TXQ_CFG_IP_OPTION_ENB | TXQ_CFG_8023_ENB | TXQ_CFG_ENHANCED_MODE),
            "TXQ_CFG de la AR8152 con lectura de 512 bytes");
    confere(alc_txq_cfg(0x1063, 2) == ((512u << 16) | 5 | TXQ_CFG_IP_OPTION_ENB | TXQ_CFG_8023_ENB | TXQ_CFG_ENHANCED_MODE),
            "TXQ_CFG de la AR8131 con 512 bytes");
    confere(alc_rafaga(5) == 3 && alc_rafaga(2) == 2 && alc_rafaga_bytes(3) == 1024, "ráfaga limitada a 1024 bytes");
    confere(alc_rxq_cfg(0x2060, m->flags) == (8u << 20), "RXQ_CFG de la AR8152: ráfaga 8, sin límite ASPM, colas apagadas");
    confere(alc_rxq_cfg(0x1063, 0) == (8u << 20 | 3), "RXQ_CFG de la AR8131: límite de ASPM a 100M");
    uint32_t dma = alc_dma_cfg(false, 2, 0);
    confere(dma == (DMA_CFG_OUT_ORDER | DMA_CFG_RD_REQ_PRI | DMA_CFG_SMB_DIS | 2u << 4 | 15u << 11 | 4u << 16),
            "DMA_CFG: fuera de orden, SMB apagado, ráfagas y retardos");
    confere(!(dma & (DMA_CFG_CMB_ENB | DMA_CFG_SMB_ENB)), "DMA_CFG sin CMB ni SMB (fallos del silicio)");
    confere(alc_dma_cfg(true, 0, 0) & DMA_CFG_RCB_128, "RCB de 128 bytes si el LnkCtl lo dice");

    /* PM_CFG: nunca L0s ni L1 encendidos. */
    uint32_t pm = alc_pm_sin_aspm(0xFFFFFFFFu);
    confere(!(pm & (PM_CFG_ASPM_L0S_ENB | PM_CFG_ASPM_L1_ENB | PM_CFG_MAC_ASPM_CHK | PM_CFG_CLK_SWH_L1)),
            "PM_CFG al iniciar: L0s, L1, chequeo ASPM y CLK_SWH_L1 apagados");
    confere((pm & (PM_CFG_SERDES_L1_ENB | PM_CFG_SERDES_PLL_L1_ENB | PM_CFG_SERDES_BUDS_RX_L1_ENB)) ==
            (PM_CFG_SERDES_L1_ENB | PM_CFG_SERDES_PLL_L1_ENB | PM_CFG_SERDES_BUDS_RX_L1_ENB), "y los relojes del SERDES encendidos");
    pm = alc_pm_enlace(0xFFFFFFFFu, 0x2060, m->flags, 100);
    confere(!(pm & (PM_CFG_ASPM_L0S_ENB | PM_CFG_ASPM_L1_ENB)), "PM_CFG con enlace (AR8152): ni L0s ni L1");
    confere(((pm & PM_CFG_L1_ENTRY_TIMER_MASK) >> 16) == 7 && ((pm & PM_CFG_LCKDET_TIMER_MASK) >> 24) == 12 &&
            ((pm & PM_CFG_PM_REQ_TIMER_MASK) >> 20) == 12, "AR8152 a 100M: temporizadores L1 7, LCKDET 12, PM_REQ 12");
    confere((pm & PM_CFG_CLK_SWH_L1) && (pm & PM_CFG_SERDES_PD_EX_L1) && !(pm & PM_CFG_SERDES_L1_ENB) &&
            !(pm & (PM_CFG_EN_BUFS_RX_L0S | PM_CFG_SA_DLY_ENB | PM_CFG_HOTRST)), "AR8152 con APS: bits del SERDES");
    pm = alc_pm_enlace(0, 0x2060, m->flags, 10);
    confere(((pm & PM_CFG_L1_ENTRY_TIMER_MASK) >> 16) == 1, "AR8152 a 10M: temporizador L1 por defecto (1)");
    pm = alc_pm_enlace(0xFFFFFFFFu, 0x1063, 0, 1000);
    confere(!(pm & (PM_CFG_ASPM_L0S_ENB | PM_CFG_ASPM_L1_ENB | PM_CFG_CLK_SWH_L1)) && (pm & PM_CFG_SERDES_L1_ENB),
            "AR8131 (sin APS): sin ASPM, SERDES encendido");

    /* Valores del fabricante para el PHY (D200: los que el header de FreeBSD da como «por defecto»). */
    confere(ALC_ANA_CFG0_VALOR == 0x02EF && ALC_ANA_CFG54_VALOR == 0xE12C && ALC_ANA_CFG18_VALOR == 0x4C04 &&
            ALC_ANA_CFG4_VALOR == 0x88BB && ALC_ANA_CFG5_VALOR == 0x2C42, "valores ANA_CFG del PHY");
    confere(ALC_INTRS == 0x0001C718u, "máscara de interrupciones de alc(4)");
}

int main(void)
{
    modelos();
    mac();
    tpd();
    rrd();
    aneis();
    phy();
    registros();
    if (falhas) { printf("%d FALHAS\n", falhas); return 1; }
    printf("TUDO OK\n");
    return 0;
}
