/* rt2870_logica.h - la parte pura del soporte de los pinchos Wi-Fi Ralink
 * RT2870 / RT3070 / RT3071 / RT3072 / RT5370 (D409, experimental, sin probar en
 * hardware).
 *
 * Sin acceso al hardware, para probarla en el anfitrión con el mismo código que
 * compila el kernel (tests/recursos/wifi_usb_anfitriao.c, tests/h409_wifi_usb.py):
 *   - qué chips soporta el driver y qué mitad de rt2870.bin lleva cada uno;
 *   - el análisis de la ROM (EEPROM o eFuse): MAC, RF, cadenas, potencia, LNA;
 *   - el descriptor de transmisión (TXD de USB + TXWI);
 *   - el desglose de una transferencia de recepción (varias tramas, cada una con
 *     RXWI, el descriptor al final y el relleno L2PAD);
 *   - el RSSI en dBm.
 * Los registros, tablas y formatos vienen del run(4) de OpenBSD (ISC, Damien
 * Bergamini) en kernel/terceros/bsd/run/rt2860reg.h; el código es propio,
 * escrito leyéndolo. */
#ifndef SAGUI_RT2870_LOGICA_H
#define SAGUI_RT2870_LOGICA_H
#ifdef SAGUI_KERNEL
#include "tipos.h"
#else
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#endif
#ifndef __packed
#define __packed __attribute__((packed))
#endif
#include "../terceros/bsd/run/rt2860reg.h"

/* ---------------------------------------------------------- chips */

#define RT_MAC_RT2872 0x2872
#define RT_MAC_RT3070 0x3070
#define RT_MAC_RT3071 0x3071
#define RT_MAC_RT5390 0x5390

/* Lo que implementa rt2870.c (2,4 GHz, una antena de transmisión como mucho dos). Los demás
 * (RT2860, RT3572, RT3593, RT5392, RT5592...) se reconocen por VID:PID pero el driver dice que no. */
static inline bool rt_mac_soportado(unsigned ver)
{
    return ver == RT_MAC_RT2872 || ver == RT_MAC_RT3070 || ver == RT_MAC_RT3071 || ver == RT_MAC_RT5390;
}

/* rt2870.bin de linux-firmware = dos firmwares de 4 KB: el primero para los RT2860, RT2872 y RT3070; el
 * segundo para los demás (run(4): «run-rt2870» y «run-rt3071»). */
#define RT_FW_TAM 4096
static inline uint32_t rt_fw_desplazamiento(unsigned ver)
{
    return (ver == 0x2860 || ver == RT_MAC_RT2872 || ver == RT_MAC_RT3070) ? 0 : RT_FW_TAM;
}

static inline const char *rt_nome_rf(unsigned rf)
{
    switch (rf) {
    case RT2860_RF_2820: return "RT2820";
    case RT2860_RF_2850: return "RT2850";
    case RT2860_RF_2720: return "RT2720";
    case RT2860_RF_2750: return "RT2750";
    case RT3070_RF_3020: return "RT3020";
    case RT3070_RF_2020: return "RT2020";
    case RT3070_RF_3021: return "RT3021";
    case RT3070_RF_3022: return "RT3022";
    case RT3070_RF_3052: return "RT3052";
    case RT5390_RF_5370: return "RT5370";
    case RT5390_RF_5372: return "RT5372";
    }
    return "desconocido";
}

/* ---------------------------------------------------------- la ROM */

#define RT_ROM_PALABRAS 0x90

typedef struct {
    uint8_t  mac[6];
    bool     mac_valida;
    uint8_t  nrx, ntx;
    uint16_t rf;                 /* RT2860_RF_* / RT3070_RF_* / RT5390_RF_* */
    uint8_t  freq;               /* desplazamiento de frecuencia del RF */
    int8_t   txpow1[14], txpow2[14];   /* potencia por canal 1..14 (2,4 GHz) */
    uint32_t txpow20mhz[5];      /* potencia por tasa (TX_PWR_CFG0..4) */
    int8_t   rssi_2ghz[3];
    uint8_t  lna0;               /* ganancia del LNA, grupo 0 (2,4 GHz) */
    uint8_t  txmixgain_2ghz;
    bool     ext_2ghz_lna, patch_dac, rfswitch;
    uint8_t  leds;
    uint16_t led[3];
    struct { uint8_t reg, val; } bbp[10], rfv[10];   /* ajustes del fabricante */
} rt_rom_t;

static inline int8_t rt_txpow_limpio(int8_t v, unsigned ver)
{
    int max = ver >= RT_MAC_RT5390 ? 27 : 31;
    return (v < 0 || v > max) ? 5 : v;
}

/* w = las palabras de 16 bits de la ROM (RT_ROM_PALABRAS), ver = ASIC_VER_ID >> 16. */
static inline void rt_rom_analisar(const uint16_t *w, unsigned ver, rt_rom_t *o)
{
    memset(o, 0, sizeof *o);
    uint16_t v;
    o->mac[0] = (uint8_t)w[RT2860_EEPROM_MAC01];      o->mac[1] = (uint8_t)(w[RT2860_EEPROM_MAC01] >> 8);
    o->mac[2] = (uint8_t)w[RT2860_EEPROM_MAC23];      o->mac[3] = (uint8_t)(w[RT2860_EEPROM_MAC23] >> 8);
    o->mac[4] = (uint8_t)w[RT2860_EEPROM_MAC45];      o->mac[5] = (uint8_t)(w[RT2860_EEPROM_MAC45] >> 8);
    bool vacia = true;
    for (int i = 0; i < 6; i++) if (o->mac[i] != 0xFF && o->mac[i] != 0) vacia = false;
    o->mac_valida = !vacia && !(o->mac[0] & 1);
    for (int i = 0; i < 10; i++) {
        v = w[RT2860_EEPROM_BBP_BASE + i];
        o->bbp[i].val = (uint8_t)v; o->bbp[i].reg = (uint8_t)(v >> 8);
        if (ver >= RT_MAC_RT3071) {
            v = w[RT3071_EEPROM_RF_BASE + i];
            o->rfv[i].val = (uint8_t)v; o->rfv[i].reg = (uint8_t)(v >> 8);
        }
    }
    v = w[RT2860_EEPROM_FREQ_LEDS];
    o->freq = (v & 0xff) != 0xff ? (uint8_t)(v & 0xff) : 0;
    if ((v >> 8) != 0xff) {
        o->leds = (uint8_t)(v >> 8);
        o->led[0] = w[RT2860_EEPROM_LED1]; o->led[1] = w[RT2860_EEPROM_LED2]; o->led[2] = w[RT2860_EEPROM_LED3];
    } else { o->leds = 0x01; o->led[0] = 0x5555; o->led[1] = 0x2221; o->led[2] = 0x5627; }
    /* RF y cadenas */
    uint16_t a = (ver == RT_MAC_RT5390) ? w[0x00] : w[RT2860_EEPROM_ANTENNA];
    if (a == 0xffff) {
        if (ver >= RT_MAC_RT3070) { o->rf = RT3070_RF_3020; o->ntx = 1; o->nrx = 1; }
        else { o->rf = RT2860_RF_2820; o->ntx = 1; o->nrx = 2; }
    } else {
        if (ver == RT_MAC_RT5390) { o->rf = a; a = w[RT2860_EEPROM_ANTENNA]; }
        else o->rf = (a >> 8) & 0xf;
        o->ntx = (a >> 4) & 0xf;
        o->nrx = a & 0xf;
    }
    if (o->ntx < 1 || o->ntx > 2) o->ntx = 1;
    if (o->nrx < 1 || o->nrx > 2) o->nrx = 1;
    v = w[RT2860_EEPROM_CONFIG];
    if ((v >> 8) != 0xff) o->patch_dac = (v >> 15) & 1;
    if ((v & 0xff) != 0xff) { o->ext_2ghz_lna = (v >> 2) & 1; o->rfswitch = v & 1; }
    /* potencia por canal (2,4 GHz) */
    for (int i = 0; i < 14; i += 2) {
        v = w[RT2860_EEPROM_PWR2GHZ_BASE1 + i / 2];
        o->txpow1[i] = (int8_t)(v & 0xff); o->txpow1[i + 1] = (int8_t)(v >> 8);
        if (ver != RT_MAC_RT5390) {
            v = w[RT2860_EEPROM_PWR2GHZ_BASE2 + i / 2];
            o->txpow2[i] = (int8_t)(v & 0xff); o->txpow2[i + 1] = (int8_t)(v >> 8);
        }
    }
    for (int i = 0; i < 14; i++) {
        o->txpow1[i] = rt_txpow_limpio(o->txpow1[i], ver);
        if (ver < RT_MAC_RT5390) o->txpow2[i] = rt_txpow_limpio(o->txpow2[i], ver);
    }
    for (int r = 0; r < 5; r++)
        o->txpow20mhz[r] = (uint32_t)w[RT2860_EEPROM_RPWR + r * 2] | (uint32_t)w[RT2860_EEPROM_RPWR + r * 2 + 1] << 16;
    v = w[RT2860_EEPROM_RSSI1_2GHZ];
    o->rssi_2ghz[0] = (int8_t)(v & 0xff); o->rssi_2ghz[1] = (int8_t)(v >> 8);
    v = w[RT2860_EEPROM_RSSI2_2GHZ];
    if (ver >= RT_MAC_RT3070) { if ((v & 0xff) != 0xff) o->txmixgain_2ghz = (uint8_t)(v & 0x7); }
    else o->rssi_2ghz[2] = (int8_t)(v & 0xff);
    o->lna0 = (uint8_t)(w[RT2860_EEPROM_LNA] & 0xff);
    for (int i = 0; i < 3; i++) if (o->rssi_2ghz[i] < -10 || o->rssi_2ghz[i] > 10) o->rssi_2ghz[i] = 0;
}

/* ---------------------------------------------------------- transmisión */

/* Las tasas 0..11 son las del resto de la pila (CCK 1, 2, 5,5, 11; OFDM 6..54). */
static inline void rt_tasa(uint8_t ridx, bool preambulo_corto, uint16_t *phy)
{
    if (ridx > 11) ridx = 11;
    if (ridx < 4) {
        uint16_t mcs = ridx;
        if (ridx != 0 && preambulo_corto) mcs |= RT2860_PHY_SHPRE;
        *phy = (uint16_t)(RT2860_PHY_CCK | mcs);
    } else *phy = (uint16_t)(RT2860_PHY_OFDM | (ridx - 4));
}

/* rt2860_rates del run(4): tasa de la respuesta de control (ACK) y duraciones del ACK en microsegundos. */
static const struct { uint8_t ctl_ridx; uint16_t sp_ack, lp_ack; } rt_tasas[12] = {
    { 0, 314, 314 }, { 1, 258, 162 }, { 2, 223, 127 }, { 3, 213, 117 }, { 4, 60, 60 }, { 4, 52, 52 },
    { 6, 48, 48 }, { 6, 44, 44 }, { 8, 44, 44 }, { 8, 40, 40 }, { 8, 40, 40 }, { 8, 40, 40 },
};

/* El campo «duración» de una trama que espera ACK a la tasa ridx. */
static inline uint16_t rt_duracao_ack(uint8_t ridx, bool preambulo_corto)
{
    if (ridx > 11) ridx = 11;
    uint8_t c = rt_tasas[ridx].ctl_ridx;
    return preambulo_corto ? rt_tasas[c].sp_ack : rt_tasas[c].lp_ack;
}

#define RT_WCID_AP 1     /* la entrada de la tabla WCID del punto de acceso */
#define RT_TX_CAB 4      /* el TXD de USB */
#define RT_TXWI   16

/* Arma TXD + TXWI delante de la trama (el llamador copia la 802.11 en buf + 20)
 * y devuelve los bytes totales a mandar por el endpoint. n = bytes de la trama. */
static inline uint32_t rt_montar_tx(uint8_t *buf, uint32_t n, bool gestion, bool difusion, uint8_t ridx,
                                    bool preambulo_corto, uint8_t wcid)
{
    uint32_t xfer = (RT_TXWI + n + 3) & ~3u;
    buf[0] = (uint8_t)xfer; buf[1] = (uint8_t)(xfer >> 8); buf[2] = 0;
    buf[3] = RT2860_TX_QSEL_EDCA;
    uint8_t *w = buf + RT_TX_CAB;
    for (int i = 0; i < RT_TXWI; i++) w[i] = 0;
    uint16_t phy;
    rt_tasa((gestion || difusion) ? 0 : ridx, preambulo_corto, &phy);
    w[0] = 0;                                  /* flags: ni A-MPDU ni fragmento */
    w[1] = RT2860_TX_TXOP_BACKOFF;
    w[2] = (uint8_t)phy; w[3] = (uint8_t)(phy >> 8);
    w[4] = difusion ? 0 : RT2860_TX_ACK;       /* xflags: pide ACK; el número de secuencia lo pone el chip */
    w[5] = gestion ? 0xff : wcid;
    w[6] = (uint8_t)n; w[7] = (uint8_t)(n >> 8);
    for (uint32_t i = n; i < xfer - RT_TXWI; i++) w[RT_TXWI + i] = 0;   /* relleno hasta 32 bits */
    return RT_TX_CAB + xfer + 4;               /* y 4 bytes de cola, como el run(4) */
}

/* ---------------------------------------------------------- recepción */

typedef void (*rt_rx_f)(void *ctx, uint8_t *quadro, uint32_t n, int rssi_dbm);

typedef struct { uint32_t quadros, erros, cortadas; } rt_rx_conta_t;

static inline int rt_rssi_dbm(int rssi_raw, int8_t desvio, uint8_t lna0)
{
    return -12 - ((int)desvio - (int)lna0) - rssi_raw;
}

/* Una transferencia: 4 bytes de DMA (longitud), RXWI (16), trama, relleno, el descriptor (4) al final;
 * la siguiente empieza en dmalen + 8. Escribe en buf (mueve la cabecera si hay L2PAD). Devuelve las tramas. */
static inline int rt_rx_desglosar(uint8_t *buf, uint32_t len, uint8_t nrx, int8_t desvio, uint8_t lna0, rt_rx_f cb,
                                  void *ctx, rt_rx_conta_t *c)
{
    int entregues = 0;
    while (len > 8) {
        uint32_t dmalen = ((uint32_t)buf[0] | (uint32_t)buf[1] << 8) & 0xffff;
        if (dmalen == 0 || (dmalen & 3) || dmalen + 8 > len) { c->cortadas++; break; }
        uint8_t *rxwi = buf + 4;
        uint32_t flags = (uint32_t)rxwi[dmalen] | (uint32_t)rxwi[dmalen + 1] << 8 | (uint32_t)rxwi[dmalen + 2] << 16 |
                         (uint32_t)rxwi[dmalen + 3] << 24;
        uint32_t n = ((uint32_t)rxwi[2] | (uint32_t)rxwi[3] << 8) & 0xfff;
        if (dmalen < 16 + 4 || n > dmalen - 16) { c->cortadas++; }
        else if (flags & (RT2860_RX_CRCERR | RT2860_RX_ICVERR | RT2860_RX_MICERR)) c->erros++;
        else if (n >= 10) {
            uint8_t *q = rxwi + 16;
            if (flags & RT2860_RX_L2PAD) {
                uint32_t cab = 24;
                if ((q[0] & 0x0c) == 0x08) { if (q[0] & 0x80) cab += 2; if ((q[1] & 0x03) == 0x03) cab += 6; }
                else if ((q[0] & 0x0c) == 0x04) cab = 16;
                if (cab + 2 <= dmalen - 16) { memmove(q + 2, q, cab); q += 2; }
            }
            int r0 = rxwi[8];   /* rxwi->rssi[0] */
            int r1 = nrx > 1 ? rxwi[9] : 0;
            int raw = (nrx > 1 && r1 > r0) ? r1 : r0;
            cb(ctx, q, n, rt_rssi_dbm(raw, desvio, lna0));
            entregues++;
            c->quadros++;
        }
        buf += dmalen + 8;
        len -= dmalen + 8;
    }
    return entregues;
}
#endif
