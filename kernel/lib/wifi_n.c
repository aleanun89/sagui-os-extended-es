// SPDX-License-Identifier: GPL-2.0-or-later
/* wifi_n.c - rendimiento del Wi-Fi del pincho RTL8188EU (D238). Ver wifi_n.h.
 *
 * Código propio. Las fuentes de cada formato están en wifi_n.h; las pruebas del
 * anfitrión (tests/recursos/wifi_n_anfitriao.c, desde tests/h215_wifi.py) lo
 * recorren con transferencias armadas a mano, cortadas y con basura. */
#include "wifi_n.h"

static uint32_t le32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

/* ------------------------------------------------------------ recepción */

/* Con la agregación de recepción del chip (RXDMA_AGG_EN en TRXDMA_CTRL, umbral
 * en RXDMA_AGG_PG_TH), una transferencia trae varias tramas: el primer
 * descriptor dice cuántas (PKTCNT) y cada una va detrás de la anterior, con su
 * descriptor de 24 bytes, el estado del PHY (INFOSZ * 8 bytes), un relleno de
 * SHIFT bytes y la trama; la siguiente empieza en el próximo múltiplo de 128.
 * Si el primer descriptor es un informe de transmisión, la transferencia
 * entera lo es (como lo trataba la D215). */
int wn_rx_desglosar(const uint8_t *buf, uint32_t len, wn_quadro_f quadro, wn_relatorio_f relatorio, void *ctx,
                    wn_rx_conta_t *c)
{
    if (!buf || len < WN_RXDESC) return 0;
    uint32_t rpt0 = (le32(buf + 12) >> WN_DW3_RPT_S) & 3;
    if (rpt0 != WN_RPT_RX) {
        c->relatorios++;
        if (rpt0 == WN_RPT_TX1 && relatorio) relatorio(ctx, buf + WN_RXDESC, len - WN_RXDESC);
        return 0;
    }
    uint32_t npkts = (le32(buf + 8) >> WN_DW2_PKTCNT_S) & 0xFF;
    if (npkts == 0) npkts = 1;
    const uint8_t *p = buf;
    uint32_t resto = len;
    int entregues = 0;
    while (npkts-- > 0 && resto >= WN_RXDESC) {
        uint32_t dw0 = le32(p), dw3 = le32(p + 12);
        uint32_t pktlen = dw0 & WN_DW0_PKTLEN;
        uint32_t info = ((dw0 >> WN_DW0_INFOSZ_S) & 0xF) * 8;
        uint32_t shift = (dw0 >> WN_DW0_SHIFT_S) & 3;
        if (pktlen == 0) break;
        uint32_t total = WN_RXDESC + info + shift + pktlen;
        if (total > resto) { c->cortadas++; break; }
        uint32_t rpt = (dw3 >> WN_DW3_RPT_S) & 3;
        const uint8_t *q = p + WN_RXDESC + info + shift;
        if (rpt != WN_RPT_RX) {
            c->relatorios++;
            if (rpt == WN_RPT_TX1 && relatorio) relatorio(ctx, q, pktlen);
        } else if (dw0 & (WN_DW0_CRCERR | WN_DW0_ICVERR)) {
            c->erros++;
        } else if (pktlen >= 10) {
            wn_rx_t r;
            r.quadro = q;
            r.n = pktlen;
            r.phy = (info && (dw0 & WN_DW0_PHYST)) ? p + WN_RXDESC : (const uint8_t *)0;
            r.taxa = (uint8_t)(dw3 & WN_DW3_RATE);
            r.ht = (dw3 & WN_DW3_HT) != 0;
            r.decifrada = (dw0 & WN_DW0_SWDEC) == 0;
            quadro(ctx, &r);
            entregues++;
            c->quadros++;
        }
        total = (total + WN_RX_ALINHAR - 1) & ~(uint32_t)(WN_RX_ALINHAR - 1);
        if (total >= resto) break;
        p += total;
        resto -= total;
    }
    if ((uint32_t)entregues > c->max_por_transf) c->max_por_transf = (uint32_t)entregues;
    return entregues;
}

/* ------------------------------------------------------------ elementos */

void wn_analisar_ies(const uint8_t *ies, uint32_t n, wn_ap_t *a)
{
    uint8_t *z = (uint8_t *)a;
    for (uint32_t k = 0; k < sizeof *a; k++) z[k] = 0;
    uint32_t i = 0;
    while (i + 2 <= n) {
        uint8_t id = ies[i], l = ies[i + 1];
        if (i + 2 + l > n) break;
        const uint8_t *b = ies + i + 2;
        if (id == 45 && l >= 26) {                        /* HT Capabilities */
            uint16_t cap = (uint16_t)(b[0] | b[1] << 8);
            a->ht = true;
            a->sgi20 = (cap & 0x0020) != 0;
            a->mcs = b[3];                                /* Supported MCS Set: máscara de recepción, MCS 0-7 */
        } else if (id == 61 && l >= 22) {                 /* HT Operation */
            a->ht_op = true;
            a->canal_ht = b[0];
        } else if (id == 221 && l >= 7 && b[0] == 0x00 && b[1] == 0x50 && b[2] == 0xF2 && b[3] == 2 &&
                   (b[4] == 0 || b[4] == 1) && b[5] == 1) {   /* WMM: información (0) o parámetros (1), versión 1 */
            a->wmm = true;
        }
        i += 2u + l;
    }
    if (!a->mcs) a->ht = false;                           /* un HT que no recibe ningún MCS de 1 flujo no sirve */
}

uint32_t wn_ie_ht_cap(uint8_t *o)
{
    uint32_t k = 0;
    o[k++] = 45;
    o[k++] = 26;
    /* HT Capability Information: SM Power Save desactivado (bits 2-3 = 3) y GI
     * corto a 20 MHz (bit 5). Solo 20 MHz, sin LDPC, sin STBC, A-MSDU de 3839. */
    uint16_t cap = 0x000C | 0x0020;
    o[k++] = (uint8_t)cap;
    o[k++] = (uint8_t)(cap >> 8);
    /* A-MPDU Parameters: tamaño máximo 2^(13+2)-1 (32 KB) y espaciado de 8 us
     * (6). No se aceptan acuerdos de Block Ack (D238), así que el punto de acceso
     * no llega a mandarnos A-MPDU; el campo es obligatorio. */
    o[k++] = (uint8_t)(0x02 | (6 << 2));
    /* Supported MCS Set: recibimos MCS 0-7; el resto, cero (la tasa más alta
     * sin indicar, el conjunto de transmisión igual al de recepción). */
    o[k++] = 0xFF;
    for (int i = 1; i < 16; i++) o[k++] = 0;
    o[k++] = 0; o[k++] = 0;                               /* HT Extended Capabilities */
    o[k++] = 0; o[k++] = 0; o[k++] = 0; o[k++] = 0;       /* Transmit Beamforming */
    o[k++] = 0;                                           /* ASEL */
    return k;
}

uint32_t wn_ie_wmm(uint8_t *o)
{
    static const uint8_t ie[WN_IE_WMM_TAM] = { 221, 7, 0x00, 0x50, 0xF2, 2, 0, 1, 0 };
    for (uint32_t i = 0; i < WN_IE_WMM_TAM; i++) o[i] = ie[i];
    return WN_IE_WMM_TAM;
}

/* ------------------------------------------------------------ Block Ack */

/* ADDBA Request (9.6.5.2): categoría, acción, token, parámetros (2), plazo (2),
 * número de secuencia inicial (2). La respuesta (9.6.5.3) repite el token, los
 * parámetros y el plazo, con el estado delante. */
uint32_t wn_addba_resposta(const uint8_t *pedido, uint32_t n, uint16_t estado, uint8_t *o)
{
    if (n < 9 || pedido[0] != WN_CAT_BLOCK_ACK || pedido[1] != WN_ADDBA_PEDIDO) return 0;
    o[0] = WN_CAT_BLOCK_ACK;
    o[1] = WN_ADDBA_RESPOSTA;
    o[2] = pedido[2];
    o[3] = (uint8_t)estado;
    o[4] = (uint8_t)(estado >> 8);
    o[5] = pedido[3];
    o[6] = pedido[4];
    o[7] = pedido[5];
    o[8] = pedido[6];
    return WN_ADDBA_RESP_TAM;
}

/* ------------------------------------------------------------ tasas */

uint16_t wn_taxa_dec(uint8_t taxa, bool sgi)
{
    static const uint16_t leg[12] = { 10, 20, 55, 110, 60, 90, 120, 180, 240, 360, 480, 540 };
    static const uint16_t ht[8] = { 65, 130, 195, 260, 390, 520, 585, 650 };       /* 20 MHz, GI de 800 ns */
    static const uint16_t ht_sgi[8] = { 72, 144, 217, 289, 433, 578, 650, 722 };   /* 20 MHz, GI de 400 ns */
    if (taxa < 12) return leg[taxa];
    if (taxa <= WN_TAXA_MCS7) return sgi ? ht_sgi[taxa - WN_TAXA_MCS0] : ht[taxa - WN_TAXA_MCS0];
    return 0;
}

static bool mcs_ok(uint8_t taxa, uint8_t mcs_ap)
{
    return taxa >= WN_TAXA_MCS0 && taxa <= WN_TAXA_MCS7 && (mcs_ap & (1u << (taxa - WN_TAXA_MCS0)));
}

uint8_t wn_taxa_subir(uint8_t taxa, bool ht, uint8_t mcs_ap)
{
    if (!ht) return taxa < 11 ? (uint8_t)(taxa + 1) : 11;
    if (taxa < 2) return 2;                               /* CCK 1 -> CCK 5,5 */
    if (taxa < 4) return 4;                               /* -> OFDM 6 */
    if (taxa < WN_TAXA_MCS0) return mcs_ok(WN_TAXA_MCS0, mcs_ap) ? WN_TAXA_MCS0 : taxa;
    if (taxa < WN_TAXA_MCS7 && mcs_ok((uint8_t)(taxa + 1), mcs_ap)) return (uint8_t)(taxa + 1);
    return taxa > WN_TAXA_MCS7 ? WN_TAXA_MCS7 : taxa;
}

uint8_t wn_taxa_descer(uint8_t taxa, bool ht)
{
    if (!ht) return taxa > 0 ? (uint8_t)(taxa - 1) : 0;
    if (taxa > WN_TAXA_MCS7) return WN_TAXA_MCS7;
    if (taxa > WN_TAXA_MCS0) return (uint8_t)(taxa - 1);
    if (taxa == WN_TAXA_MCS0 || taxa > 4) return 4;       /* MCS 0 -> OFDM 6 */
    if (taxa > 2) return 2;                               /* -> CCK 5,5 */
    return 0;                                             /* -> CCK 1 */
}

uint8_t wn_taxa_inicial(bool ht, uint8_t mcs_ap)
{
    /* Sin HT, OFDM 24 Mb/s (la de la D215). Con HT, MCS 3 (26 Mb/s) o el más
     * alto por debajo que el punto de acceso reciba. */
    if (!ht) return 8;
    for (int m = 3; m >= 0; m--)
        if (mcs_ap & (1u << m)) return (uint8_t)(WN_TAXA_MCS0 + m);
    return 8;
}
