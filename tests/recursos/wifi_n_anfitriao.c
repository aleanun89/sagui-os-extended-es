// SPDX-License-Identifier: GPL-2.0-or-later
/* wifi_n_anfitriao.c - pruebas en el anfitrión de kernel/lib/wifi_n.c (D238).
 *
 * Lo compila y lo ejecuta tests/h215_wifi.py (parte «anfitrion»), con los
 * sanitizadores: transferencias de recepción armadas a mano (una trama, varias
 * agregadas con su alineación a 128 bytes, el relleno SHIFT, un informe de TX
 * dentro y al principio, CRC malo, descriptores cortados y basura al azar),
 * los elementos HT y WMM de una baliza, nuestras HT Capabilities, la respuesta
 * a un ADDBA y la escalera de tasas. Cada comprobación imprime una línea; al
 * final, «wifi_n ok» o el número de fallos. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "wifi_n.h"

static int falhas;

static void ver(int ok, const char *o)
{
    printf("%s %s\n", ok ? "ok" : "FALLO", o);
    if (!ok) falhas++;
}

/* ------------------------------------------------------------ recepción */

typedef struct {
    int      n;
    uint32_t tam[16];
    uint8_t  prim[16];           /* primer byte de cada trama */
    uint8_t  taxa[16];
    int      phy[16];
    int      decifrada[16];
    int      relatorios;
    uint32_t tam_rel;
    uint8_t  prim_rel;
} visto_t;

static void cb_quadro(void *ctx, const wn_rx_t *q)
{
    visto_t *v = ctx;
    if (v->n < 16) {
        v->tam[v->n] = q->n;
        v->prim[v->n] = q->quadro[0];
        v->taxa[v->n] = q->taxa;
        v->phy[v->n] = q->phy != NULL;
        v->decifrada[v->n] = q->decifrada;
    }
    /* que la trama entera se pueda leer (ASan lo comprueba) */
    volatile uint8_t s = 0;
    for (uint32_t i = 0; i < q->n; i++) s ^= q->quadro[i];
    (void)s;
    v->n++;
}

static void cb_relatorio(void *ctx, const uint8_t *p, uint32_t n)
{
    visto_t *v = ctx;
    v->relatorios++;
    v->tam_rel = n;
    v->prim_rel = n ? p[0] : 0;
}

static void le32w(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}

/* Pone en b un descriptor y su trama; devuelve el tamaño sin alinear. */
static uint32_t por(uint8_t *b, uint32_t pktlen, uint32_t infosz, uint32_t shift, uint32_t pktcnt, uint32_t rpt,
                    uint32_t extra0, uint8_t taxa, uint8_t marca)
{
    memset(b, 0, WN_RXDESC);
    le32w(b, pktlen | (infosz << WN_DW0_INFOSZ_S) | (shift << WN_DW0_SHIFT_S) | (infosz ? WN_DW0_PHYST : 0) | extra0);
    le32w(b + 8, pktcnt << WN_DW2_PKTCNT_S);
    le32w(b + 12, (uint32_t)taxa | (rpt << WN_DW3_RPT_S) | (taxa >= 12 ? WN_DW3_HT : 0));
    memset(b + WN_RXDESC, 0x5A, infosz * 8 + shift);
    memset(b + WN_RXDESC + infosz * 8 + shift, marca, pktlen);
    return WN_RXDESC + infosz * 8 + shift + pktlen;
}

static uint32_t alinhar(uint32_t x) { return (x + 127) & ~127u; }

static void teste_rx(void)
{
    static uint8_t buf[16384];
    wn_rx_conta_t c;
    visto_t v;

    /* una trama sola, con estado del PHY */
    memset(&c, 0, sizeof c); memset(&v, 0, sizeof v);
    uint32_t t = por(buf, 100, 4, 0, 1, WN_RPT_RX, 0, 4, 0x80);
    int r = wn_rx_desglosar(buf, t, cb_quadro, cb_relatorio, &v, &c);
    ver(r == 1 && v.n == 1 && v.tam[0] == 100 && v.prim[0] == 0x80 && v.phy[0] && v.taxa[0] == 4 && v.decifrada[0],
        "rx: una trama con el estado del PHY");

    /* tres agregadas: cada una en un múltiplo de 128, la segunda con SHIFT 2 y MCS 7 */
    memset(&c, 0, sizeof c); memset(&v, 0, sizeof v);
    uint32_t k = 0;
    k += alinhar(por(buf + k, 1500, 4, 0, 3, WN_RPT_RX, 0, 19, 0x88));
    k += alinhar(por(buf + k, 60, 4, 2, 0, WN_RPT_RX, WN_DW0_SWDEC, 19, 0x08));
    k += por(buf + k, 1000, 4, 0, 0, WN_RPT_RX, 0, 13, 0x88);
    r = wn_rx_desglosar(buf, k, cb_quadro, cb_relatorio, &v, &c);
    ver(r == 3 && v.tam[0] == 1500 && v.tam[1] == 60 && v.tam[2] == 1000, "rx: tres tramas agregadas, alineadas a 128");
    ver(v.prim[1] == 0x08 && v.taxa[1] == 19 && !v.decifrada[1] && v.decifrada[0], "rx: el relleno SHIFT y el SWDEC");
    ver(c.max_por_transf == 3, "rx: el máximo de tramas por transferencia");

    /* la última cortada: se entregan las anteriores y se cuenta */
    memset(&c, 0, sizeof c); memset(&v, 0, sizeof v);
    r = wn_rx_desglosar(buf, k - 10, cb_quadro, cb_relatorio, &v, &c);
    ver(r == 2 && c.cortadas == 1, "rx: la última trama cortada no se entrega");

    /* PKTCNT dice más de las que hay: se para al acabarse la transferencia */
    memset(&c, 0, sizeof c); memset(&v, 0, sizeof v);
    k = alinhar(por(buf, 200, 4, 0, 9, WN_RPT_RX, 0, 4, 0x88));
    k += por(buf + k, 200, 4, 0, 0, WN_RPT_RX, 0, 4, 0x88);
    r = wn_rx_desglosar(buf, k, cb_quadro, cb_relatorio, &v, &c);
    ver(r == 2, "rx: PKTCNT mayor que las tramas que hay");

    /* CRC malo en medio: se cuenta y se sigue */
    memset(&c, 0, sizeof c); memset(&v, 0, sizeof v);
    k = alinhar(por(buf, 300, 4, 0, 3, WN_RPT_RX, 0, 4, 0x88));
    k += alinhar(por(buf + k, 300, 4, 0, 0, WN_RPT_RX, WN_DW0_CRCERR, 4, 0x88));
    k += por(buf + k, 300, 4, 0, 0, WN_RPT_RX, 0, 4, 0x88);
    r = wn_rx_desglosar(buf, k, cb_quadro, cb_relatorio, &v, &c);
    ver(r == 2 && c.erros == 1, "rx: la trama con CRC malo se tira y se sigue con la siguiente");

    /* informe de TX al principio: la transferencia entera es el informe */
    memset(&c, 0, sizeof c); memset(&v, 0, sizeof v);
    t = por(buf, 8, 0, 0, 1, WN_RPT_TX1, 0, 0, 0x33);
    r = wn_rx_desglosar(buf, t, cb_quadro, cb_relatorio, &v, &c);
    ver(r == 0 && v.relatorios == 1 && v.tam_rel == 8 && v.prim_rel == 0x33, "rx: informe de TX en el primer descriptor");

    /* informe de TX dentro de una agregación */
    memset(&c, 0, sizeof c); memset(&v, 0, sizeof v);
    k = alinhar(por(buf, 400, 4, 0, 3, WN_RPT_RX, 0, 4, 0x88));
    k += alinhar(por(buf + k, 8, 0, 0, 0, WN_RPT_TX1, 0, 0, 0x44));
    k += por(buf + k, 400, 4, 0, 0, WN_RPT_RX, 0, 4, 0x88);
    r = wn_rx_desglosar(buf, k, cb_quadro, cb_relatorio, &v, &c);
    ver(r == 2 && v.relatorios == 1 && v.prim_rel == 0x44, "rx: informe de TX dentro de la agregación");

    /* tramas de menos de 10 bytes, longitud 0 y transferencias cortas */
    memset(&c, 0, sizeof c); memset(&v, 0, sizeof v);
    t = por(buf, 5, 0, 0, 1, WN_RPT_RX, 0, 4, 0x88);
    ver(wn_rx_desglosar(buf, t, cb_quadro, cb_relatorio, &v, &c) == 0, "rx: trama de 5 bytes descartada");
    ver(wn_rx_desglosar(buf, 23, cb_quadro, cb_relatorio, &v, &c) == 0, "rx: menos de un descriptor");
    memset(buf, 0, 64);
    ver(wn_rx_desglosar(buf, 64, cb_quadro, cb_relatorio, &v, &c) == 0, "rx: longitud 0");

    /* basura al azar: nunca se lee fuera (ASan) */
    srand(238);
    int total = 0;
    for (int it = 0; it < 20000; it++) {
        uint32_t n = (uint32_t)(rand() % 16385);
        for (uint32_t i = 0; i < n; i++) buf[i] = (uint8_t)rand();
        uint8_t *copia = malloc(n ? n : 1);
        memcpy(copia, buf, n);
        memset(&c, 0, sizeof c); memset(&v, 0, sizeof v);
        total += wn_rx_desglosar(copia, n, cb_quadro, cb_relatorio, &v, &c);
        free(copia);
    }
    printf("(%d tramas sacadas de basura)\n", total);
    ver(1, "rx: 20000 transferencias al azar sin leer fuera");
}

/* ------------------------------------------------------------ elementos */

static void teste_ies(void)
{
    uint8_t ies[200];
    uint32_t k = 0;
    ies[k++] = 0; ies[k++] = 3; ies[k++] = 'a'; ies[k++] = 'b'; ies[k++] = 'c';
    ies[k++] = 45; ies[k++] = 26;
    ies[k++] = 0x2C; ies[k++] = 0x01;          /* GI corto a 20 MHz */
    ies[k++] = 0x17;
    ies[k++] = 0xFF; ies[k++] = 0xFF;          /* MCS 0-15 */
    for (int i = 2; i < 16; i++) ies[k++] = 0;
    for (int i = 0; i < 7; i++) ies[k++] = 0;
    ies[k++] = 61; ies[k++] = 22; ies[k++] = 6;
    for (int i = 0; i < 21; i++) ies[k++] = 0;
    ies[k++] = 221; ies[k++] = 24; ies[k++] = 0x00; ies[k++] = 0x50; ies[k++] = 0xF2; ies[k++] = 2; ies[k++] = 1; ies[k++] = 1;
    for (int i = 0; i < 18; i++) ies[k++] = 0;
    wn_ap_t a;
    wn_analisar_ies(ies, k, &a);
    ver(a.ht && a.sgi20 && a.mcs == 0xFF && a.ht_op && a.canal_ht == 6 && a.wmm, "ies: HT, GI corto, MCS, canal y WMM");

    wn_analisar_ies(ies, 5 + 2 + 20, &a);      /* HT Capabilities cortado */
    ver(!a.ht && !a.wmm, "ies: un elemento que se sale del final no cuenta");
    ies[5 + 2 + 3] = 0;                        /* sin MCS 0-7 */
    wn_analisar_ies(ies, k, &a);
    ver(!a.ht && a.wmm, "ies: HT sin ningún MCS de 1 flujo no sirve");

    uint8_t o[64];
    memset(o, 0xEE, sizeof o);
    uint32_t n = wn_ie_ht_cap(o);
    ver(n == WN_IE_HT_CAP_TAM && o[0] == 45 && o[1] == 26 && o[2] == 0x2C && o[3] == 0 && o[5] == 0xFF && o[6] == 0 &&
        o[n] == 0xEE, "ht_cap: 28 bytes, SMPS desactivado, GI corto, MCS 0-7");
    wn_analisar_ies(o, n, &a);
    ver(a.ht && a.sgi20 && a.mcs == 0xFF, "ht_cap: nuestro elemento se lee igual");
    n = wn_ie_wmm(o);
    ver(n == WN_IE_WMM_TAM && o[0] == 221 && o[1] == 7 && o[5] == 2 && o[6] == 0 && o[7] == 1 && o[8] == 0,
        "wmm: elemento de información, versión 1, sin U-APSD");
    wn_analisar_ies(o, n, &a);
    ver(a.wmm && !a.ht, "wmm: se reconoce");
}

/* ------------------------------------------------------------ Block Ack y tasas */

static void teste_addba(void)
{
    const uint8_t p[9] = { 3, 0, 0x42, 0x03, 0x10, 0x00, 0x00, 0x50, 0x01 };
    uint8_t o[16];
    uint32_t n = wn_addba_resposta(p, sizeof p, WN_ESTADO_RECUSADO, o);
    ver(n == 9 && o[0] == 3 && o[1] == 1 && o[2] == 0x42 && o[3] == 37 && o[4] == 0 && o[5] == 0x03 && o[6] == 0x10 &&
        o[7] == 0 && o[8] == 0, "addba: rechazo con el token, los parámetros y el plazo del pedido");
    ver(wn_addba_resposta(p, 8, WN_ESTADO_RECUSADO, o) == 0, "addba: pedido corto, sin respuesta");
    const uint8_t d[9] = { 3, 2, 0, 0, 0, 0, 0, 0, 0 };
    ver(wn_addba_resposta(d, sizeof d, WN_ESTADO_RECUSADO, o) == 0, "addba: un DELBA no se contesta");
}

static void teste_taxas(void)
{
    ver(wn_taxa_dec(0, false) == 10 && wn_taxa_dec(2, false) == 55 && wn_taxa_dec(11, false) == 540, "tasas: b/g");
    ver(wn_taxa_dec(12, false) == 65 && wn_taxa_dec(19, false) == 650 && wn_taxa_dec(19, true) == 722 &&
        wn_taxa_dec(20, false) == 0, "tasas: MCS 0-7 con GI largo y corto");
    /* sin HT, la escalera de la D215: de uno en uno entre 0 y 11 */
    uint8_t t = 8;
    for (int i = 0; i < 20; i++) t = wn_taxa_subir(t, false, 0xFF);
    ver(t == 11, "tasas b/g: sube hasta 54 Mb/s");
    for (int i = 0; i < 20; i++) t = wn_taxa_descer(t, false);
    ver(t == 0 && wn_taxa_inicial(false, 0xFF) == 8, "tasas b/g: baja hasta 1 Mb/s; empieza en 24");
    /* con HT: 0 -> 2 -> 4 -> 12..19 y vuelta */
    t = wn_taxa_inicial(true, 0xFF);
    ver(t == 15, "tasas HT: empieza en MCS 3");
    for (int i = 0; i < 20; i++) t = wn_taxa_subir(t, true, 0xFF);
    ver(t == 19, "tasas HT: sube hasta MCS 7");
    t = wn_taxa_descer(t, true);
    ver(t == 18, "tasas HT: baja de uno en uno");
    for (int i = 0; i < 6; i++) t = wn_taxa_descer(t, true);
    ver(t == 12, "tasas HT: hasta MCS 0");
    t = wn_taxa_descer(t, true);
    ver(t == 4, "tasas HT: de MCS 0 a OFDM 6");
    t = wn_taxa_descer(t, true);
    t = wn_taxa_descer(t, true);
    ver(t == 0 && wn_taxa_descer(0, true) == 0, "tasas HT: luego CCK 5,5 y CCK 1");
    t = wn_taxa_subir(0, true, 0xFF);
    t = wn_taxa_subir(t, true, 0xFF);
    t = wn_taxa_subir(t, true, 0xFF);
    ver(t == 12, "tasas HT: de vuelta a MCS 0");
    t = 15;
    t = wn_taxa_subir(t, true, 0x0F);
    ver(t == 15, "tasas HT: no pasa de los MCS que recibe el punto de acceso");
    ver(wn_taxa_inicial(true, 0x03) == 13 && wn_taxa_inicial(true, 0) == 8, "tasas HT: inicial con pocos MCS o ninguno");
    ver(wn_taxa_subir(8, true, 0xFF) == 12, "tasas HT: de una tasa b/g a MCS 0");
}

int main(void)
{
    teste_rx();
    teste_ies();
    teste_addba();
    teste_taxas();
    if (falhas) { printf("wifi_n: %d fallos\n", falhas); return 1; }
    printf("wifi_n ok\n");
    return 0;
}
