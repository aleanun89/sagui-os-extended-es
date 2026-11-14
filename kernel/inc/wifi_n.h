// SPDX-License-Identifier: GPL-2.0-or-later
/* wifi_n.h - rendimiento del Wi-Fi del pincho RTL8188EU (D238): el desglose de
 * una transferencia de recepción con varias tramas (agregación del chip), los
 * elementos de la 802.11n (HT) y de WMM, la respuesta a un ADDBA y la escalera
 * de tasas del control de tasa (b/g y HT, MCS 0-7 a 20 MHz).
 *
 * Como wpa.c, el mismo archivo (kernel/lib/wifi_n.c) se compila en el kernel y
 * en el anfitrión (tests/h215_wifi.py, con -DSAGUI_HOST): no usa nada de libc.
 *
 * Referencias (no se copió código): IEEE 802.11-2020 (9.4.2.55 HT Capabilities,
 * 9.4.2.56 HT Operation, 9.6.5.2/3 ADDBA, 19.5 tasas HT); WMM (Wi-Fi Alliance,
 * 2.2.1 «WMM Information Element»); el formato del descriptor de recepción del
 * RTL8188E, de r92creg.h de OpenBSD (ISC, kernel/terceros/bsd/rtwn/) y del
 * rtl8xxxu de Linux (rtl8xxxu_parse_rxdesc16: cada trama de una agregación
 * empieza alineada a 128 bytes, el tamaño de página del DMA de recepción). */
#ifndef SAGUI_WIFI_N_H
#define SAGUI_WIFI_N_H
#ifdef SAGUI_KERNEL
#include "tipos.h"
#else
#include <stdint.h>
#include <stdbool.h>
#endif

/* ------------------------------------------------------------ recepción */

#define WN_RXDESC        24      /* descriptor de recepción del RTL8188E */
#define WN_RX_ALINHAR    128     /* cada trama de una agregación empieza en múltiplo de 128 */

/* Bits del descriptor (r92creg.h: R92C_RXDW0_*, R92C_RXDW2_PKTCNT, R92C_RXDW3_*, R88E_RXDW3_RPT). */
#define WN_DW0_PKTLEN    0x00003FFFu
#define WN_DW0_CRCERR    0x00004000u
#define WN_DW0_ICVERR    0x00008000u
#define WN_DW0_INFOSZ_S  16              /* 4 bits, en unidades de 8 bytes */
#define WN_DW0_SHIFT_S   24              /* 2 bits: bytes de relleno antes de la trama */
#define WN_DW0_PHYST     0x04000000u
#define WN_DW0_SWDEC     0x08000000u     /* «hay que descifrar por software» (R92C_RXDW0_DECRYPTED) */
#define WN_DW2_PKTCNT_S  16              /* 8 bits: tramas de la agregación (en el primer descriptor) */
#define WN_DW3_RATE      0x0000003Fu
#define WN_DW3_HT        0x00000040u
#define WN_DW3_RPT_S     14              /* 2 bits: 0 = trama, 1 = informe de TX (CCX), 2 = informe TX2, 3 = otro */
#define WN_RPT_RX        0
#define WN_RPT_TX1       1

typedef struct {
    const uint8_t *quadro;       /* la trama 802.11 (sin FCS) */
    uint32_t       n;
    const uint8_t *phy;          /* el estado del PHY (NULL si no viene) */
    uint8_t        taxa;         /* índice de tasa del chip: 0-3 CCK, 4-11 OFDM, 12-19 MCS 0-7 */
    bool           ht;
    bool           decifrada;    /* el chip no pidió descifrado por software */
} wn_rx_t;

typedef struct {
    uint32_t quadros;            /* tramas entregadas */
    uint32_t relatorios;         /* informes del chip (no son tramas) */
    uint32_t erros;              /* con CRC o ICV malos (tiradas) */
    uint32_t cortadas;           /* descriptores que se salían de la transferencia */
    uint32_t max_por_transf;     /* el máximo de tramas en una transferencia */
} wn_rx_conta_t;

typedef void (*wn_quadro_f)(void *ctx, const wn_rx_t *q);
typedef void (*wn_relatorio_f)(void *ctx, const uint8_t *p, uint32_t n);

/* Recorre una transferencia de recepción (len bytes): por cada trama buena llama
 * a «quadro»; por cada informe de transmisión (CCX), a «relatorio» (puede ser
 * NULL). Devuelve las tramas entregadas. Nunca lee fuera de buf[0..len). */
int wn_rx_desglosar(const uint8_t *buf, uint32_t len, wn_quadro_f quadro, wn_relatorio_f relatorio, void *ctx,
                    wn_rx_conta_t *c);

/* ------------------------------------------------------------ elementos de las balizas */

typedef struct {
    bool    ht;                  /* HT Capabilities (45) */
    bool    sgi20;               /* recibe con GI corto a 20 MHz */
    uint8_t mcs;                 /* bits de los MCS 0-7 que recibe */
    bool    ht_op;               /* HT Operation (61) */
    uint8_t canal_ht;            /* el canal primario que dice el HT Operation */
    bool    wmm;                 /* el elemento WMM de parámetros o de información */
} wn_ap_t;

/* Lee los elementos (ies, n bytes) de una baliza o respuesta de sondeo. */
void wn_analisar_ies(const uint8_t *ies, uint32_t n, wn_ap_t *a);

#define WN_IE_HT_CAP_TAM 28
#define WN_IE_WMM_TAM    9
/* Nuestras HT Capabilities (1 flujo, 20 MHz, GI corto a 20 MHz, sin ahorro de
 * energía SM): escribe WN_IE_HT_CAP_TAM bytes. */
uint32_t wn_ie_ht_cap(uint8_t *o);
/* El elemento de información WMM de una estación (sin U-APSD): WN_IE_WMM_TAM bytes. */
uint32_t wn_ie_wmm(uint8_t *o);

/* ------------------------------------------------------------ Block Ack */

#define WN_CAT_BLOCK_ACK     3
#define WN_ADDBA_PEDIDO      0
#define WN_ADDBA_RESPOSTA    1
#define WN_DELBA             2
#define WN_ESTADO_RECUSADO   37      /* «The request has been declined» */
#define WN_ADDBA_RESP_TAM    9

/* El cuerpo de la respuesta (categoría, acción, token, estado, parámetros y
 * plazo) a un pedido ADDBA (cuerpo de la trama de acción, n bytes). 0 si el
 * pedido no es un ADDBA válido; si no, WN_ADDBA_RESP_TAM. */
uint32_t wn_addba_resposta(const uint8_t *pedido, uint32_t n, uint16_t estado, uint8_t *o);

/* ------------------------------------------------------------ tasas */

#define WN_TAXA_MCS0  12
#define WN_TAXA_MCS7  19
/* Décimas de Mb/s de un índice de tasa del chip (0 si no es válido). */
uint16_t wn_taxa_dec(uint8_t taxa, bool sgi);
/* La escalera del control de tasa. Sin HT, la de siempre (índices 0..11 uno a
 * uno, D215). Con HT: CCK 1, CCK 5,5, OFDM 6 y luego MCS 0..7 (solo los que
 * el punto de acceso recibe, «mcs_ap»). */
uint8_t wn_taxa_subir(uint8_t taxa, bool ht, uint8_t mcs_ap);
uint8_t wn_taxa_descer(uint8_t taxa, bool ht);
uint8_t wn_taxa_inicial(bool ht, uint8_t mcs_ap);

#endif
