/* rtl8192cu_logica.h - la parte pura del soporte de los pinchos Wi-Fi Realtek
 * RTL8188CU / RTL8192CU / RTL8188RU (D409, experimental, sin probar en hardware).
 *
 * Sin acceso al hardware, para probarla en el anfitrión con el mismo código que
 * compila el kernel (tests/recursos/wifi_usb_anfitriao.c, tests/h409_wifi_usb.py):
 *   - el tipo de chip desde SYS_CFG y HPON_FSM (urtwn: rtwn_read_chipid);
 *   - el nombre del firmware según la revisión;
 *   - la comprobación de la cabecera del firmware;
 *   - la imagen de la eFuse (MAC, tipo de placa, regulación);
 *   - la potencia por tasa y por canal (urtwn: rtwn_r92c_get_txpower);
 *   - el descriptor de transmisión de 32 bytes con su suma de control;
 *   - el RSSI del estado del PHY.
 * Los registros y las tablas vienen del urtwn/rtwn de OpenBSD (ISC, Damien
 * Bergamini) en kernel/terceros/bsd/rtwn/; el código es propio, escrito leyéndolo. */
#ifndef SAGUI_RTL8192CU_LOGICA_H
#define SAGUI_RTL8192CU_LOGICA_H
#ifdef SAGUI_KERNEL
#include "tipos.h"
#else
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#endif

#ifndef IEEE80211_ADDR_LEN
#define IEEE80211_ADDR_LEN 6
#endif
#ifndef nitems
#define nitems(a) ((int)(sizeof(a) / sizeof((a)[0])))
#endif
#ifndef __packed
#define __packed __attribute__((packed))
#endif
#ifndef R92C_MACID_BSS
#define R92C_MACID_BSS        0      /* la entrada de la estación del AP (r92creg.h, fw_cmd_macid_cfg) */
#endif
#ifndef R92C_TXDESC_SUMSIZE
#define R92C_TXDESC_SUMSIZE   32
#define R92C_TXDESC_SUMOFFSET 14
#endif
#include "../terceros/bsd/rtwn/r92creg_88e.h"
#include "../terceros/bsd/rtwn/r92creg_92c.h"

/* ---------------------------------------------------------- tipo de chip */

typedef struct {
    bool    ok;           /* false = chip de prueba (TRP_VAUX_EN), no soportado */
    bool    c2t2r;        /* RTL8192CU: 2 cadenas de transmisión y recepción */
    bool    c1t2r;        /* RTL8192C capado a 1T2R (por el número de bonding) */
    bool    umc;          /* fabricado por UMC (si no, TSMC) */
    bool    a_cut;        /* revisión A de UMC */
    uint8_t ntx, nrx;     /* cadenas de transmisión y de recepción */
} r92c_chip_t;

/* sys_cfg = REG_SYS_CFG (0x0f0), hpon = REG_HPON_FSM (0x0ec). */
static inline r92c_chip_t r92c_tipo_chip(uint32_t sys_cfg, uint32_t hpon)
{
    r92c_chip_t c;
    c.ok = !(sys_cfg & R92C_SYS_CFG_TRP_VAUX_EN);
    c.c2t2r = (sys_cfg & R92C_SYS_CFG_TYPE_92C) != 0;
    c.c1t2r = c.c2t2r && MS(hpon, R92C_HPON_FSM_CHIP_BONDING_ID) == R92C_HPON_FSM_CHIP_BONDING_ID_92C_1T2R;
    c.umc = (sys_cfg & R92C_SYS_CFG_VENDOR_UMC) != 0;
    c.a_cut = c.umc && MS(sys_cfg, R92C_SYS_CFG_CHIP_VER_RTL) == 0;
    c.ntx = (c.c2t2r && !c.c1t2r) ? 2 : 1;
    c.nrx = c.c2t2r ? 2 : 1;
    return c;
}

/* El firmware de linux-firmware (rtlwifi/): como el urtwn, la UMC de corte A de
 * 1T1R usa el «U» y todo lo demás el «T» (TSMC). */
static inline const char *r92c_nome_firmware(const r92c_chip_t *c)
{
    return (c->umc && c->a_cut && !c->c2t2r) ? "rtl8192cufw_A.bin" : "rtl8192cufw_TMSC.bin";
}

/* 0 = bien (*saltar = bytes de la cabecera que se saltan); si no, un motivo. */
static inline const char *r92c_firmware_ok(const uint8_t *fw, uint32_t n, uint32_t *saltar, unsigned *ver, unsigned *sub)
{
    *saltar = 0;
    *ver = *sub = 0;
    if (n < sizeof(struct r92c_fw_hdr) + 1024) return "firmware demasiado corto";
    const struct r92c_fw_hdr *h = (const struct r92c_fw_hdr *)fw;
    unsigned sig = (unsigned)(h->signature >> 4);
    if (sig != 0x88c && sig != 0x92c) return "el archivo no es un firmware del RTL8188C/8192C";
    *saltar = sizeof *h;
    *ver = h->version;
    *sub = h->subversion;
    return (const char *)0;
}

/* ---------------------------------------------------------- eFuse */

typedef struct {
    bool    valida;
    uint8_t mac[6];
    uint8_t placa;        /* R92C_BOARD_TYPE_*: 0 dongle, 1 alta potencia, 2 minicard... */
    uint8_t regulacion;
} r92c_efuse_t;

static inline r92c_efuse_t r92c_leer_rom(const uint8_t *rom)
{
    r92c_efuse_t e;
    const struct r92c_rom *r = (const struct r92c_rom *)rom;
    memcpy(e.mac, r->macaddr, 6);
    e.placa = (uint8_t)MS(r->rf_opt1, R92C_ROM_RF1_BOARD_TYPE);
    e.regulacion = (uint8_t)MS(r->rf_opt1, R92C_ROM_RF1_REGULATORY);
    bool vacia = true;
    for (int i = 0; i < 6; i++) if (e.mac[i] != 0xFF && e.mac[i] != 0) vacia = false;
    e.valida = r->id == 0x8129 && !vacia && !(e.mac[0] & 1);
    return e;
}

/* ---------------------------------------------------------- potencia */

#define R92C_NPOTENCIAS 28   /* RTWN_RIDX_CCK1 .. RTWN_RIDX_MCS15 */

/* El grupo de canales de la potencia: 1-3, 4-9, 10-14. */
static inline int r92c_grupo_canal(int canal)
{
    return canal <= 3 ? 0 : canal <= 9 ? 1 : 2;
}

/* Potencia por tasa de la cadena «cadena» (0 o 1) en el canal, a 20 MHz: la base
 * de la tabla del fabricante según la regulación, más lo de la eFuse
 * (rtwn_r92c_get_txpower). placa_alta_potencia: tabla de los RTL8188RU. */
static inline void r92c_potencia(const uint8_t *rom, bool c2t2r, uint8_t placa, uint8_t ntx, int cadena, int canal,
                                 uint16_t p[R92C_NPOTENCIAS])
{
    const struct r92c_rom *r = (const struct r92c_rom *)rom;
    int grupo = r92c_grupo_canal(canal);
    uint8_t reg = (uint8_t)MS(r->rf_opt1, R92C_ROM_RF1_REGULATORY);
    const struct r92c_txpwr *base = (!c2t2r && placa == R92C_BOARD_TYPE_HIGHPA) ? &rtl8188ru_txagc[cadena]
                                                                                 : &rtl8192cu_txagc[cadena];
    for (int i = 0; i < R92C_NPOTENCIAS; i++) p[i] = 0;
    if (reg == 0)
        for (int i = RTWN_RIDX_CCK1; i <= RTWN_RIDX_CCK11; i++) p[i] = base->pwr[0][i];
    for (int i = RTWN_RIDX_OFDM6; i <= RTWN_RIDX_MAX; i++) {
        if (reg == 3) {
            p[i] = base->pwr[0][i];
            uint16_t max = (uint16_t)((r->ht20_max_pwr[grupo] >> (cadena * 4)) & 0xF);
            if (p[i] > max) p[i] = max;
        } else if (reg == 1) p[i] = base->pwr[grupo][i];
        else if (reg != 2) p[i] = base->pwr[0][i];
    }
    uint16_t cck = r->cck_tx_pwr[cadena][grupo];
    for (int i = RTWN_RIDX_CCK1; i <= RTWN_RIDX_CCK11; i++) {
        p[i] = (uint16_t)(p[i] + cck);
        if (p[i] > R92C_MAX_TX_PWR) p[i] = R92C_MAX_TX_PWR;
    }
    uint16_t ht = r->ht40_1s_tx_pwr[cadena][grupo], dif;
    if (ntx > 1) {
        dif = (uint16_t)((r->ht40_2s_tx_pwr_diff[grupo] >> (cadena * 4)) & 0xF);
        ht = ht > dif ? (uint16_t)(ht - dif) : 0;
    }
    dif = (uint16_t)((r->ofdm_tx_pwr_diff[grupo] >> (cadena * 4)) & 0xF);
    uint16_t ofdm = (uint16_t)(ht + dif);
    for (int i = RTWN_RIDX_OFDM6; i <= RTWN_RIDX_OFDM54; i++) {
        p[i] = (uint16_t)(p[i] + ofdm);
        if (p[i] > R92C_MAX_TX_PWR) p[i] = R92C_MAX_TX_PWR;
    }
    dif = (uint16_t)((r->ht20_tx_pwr_diff[grupo] >> (cadena * 4)) & 0xF);
    ht = (uint16_t)(ht + dif);   /* corrección HT40 -> HT20 */
    for (int i = RTWN_RIDX_MCS0; i <= RTWN_RIDX_MCS15; i++) {
        p[i] = (uint16_t)(p[i] + ht);
        if (p[i] > R92C_MAX_TX_PWR) p[i] = R92C_MAX_TX_PWR;
    }
}

/* ---------------------------------------------------------- transmisión */

#define R92C_TXDESC_LEN 32

static inline void r92c_pon32(uint8_t *b, int i, uint32_t v)
{
    b[4 * i] = (uint8_t)v;
    b[4 * i + 1] = (uint8_t)(v >> 8);
    b[4 * i + 2] = (uint8_t)(v >> 16);
    b[4 * i + 3] = (uint8_t)(v >> 24);
}

/* Descriptor de 32 bytes de una trama de «n» bytes (la 802.11 va detrás, a
 * «desl» bytes: 32, o 40 con el hueco PKTOFF). gestion: CCK 1 Mb/s por la cola de
 * gestión. Los datos (o la difusión) van a «taxa» (índice RTWN_RIDX_*) con la tasa
 * fijada por el driver (DRVRATE): sin informe de transmisión no hay control de
 * tasa adaptativo (urtwn manda OFDM54 y deja el control al chip). */
static inline void r92c_montar_txdesc(uint8_t *buf, uint32_t n, uint32_t desl, bool gestion, bool difusion, bool cifrar,
                                      bool hueco, uint8_t taxa)
{
    for (uint32_t i = 0; i < desl; i++) buf[i] = 0;
    uint32_t w0 = SM(R92C_TXDW0_PKTLEN, n) | SM(R92C_TXDW0_OFFSET, desl) | R92C_TXDW0_OWN | R92C_TXDW0_FSG |
                  R92C_TXDW0_LSG;
    if (difusion) w0 |= R92C_TXDW0_BMCAST;
    uint32_t w1 = 0, w4 = 0, w5 = 0;
    if (cifrar) w1 |= SM(R92C_TXDW1_CIPHER, R92C_TXDW1_CIPHER_AES);
    if (hueco) w1 |= SM(R92C_TXDW1_PKTOFF, 1);
    if (gestion || difusion) {
        w1 |= SM(R92C_TXDW1_MACID, 0) | SM(R92C_TXDW1_QSEL, gestion ? R92C_TXDW1_QSEL_MGNT : R92C_TXDW1_QSEL_BE) |
              SM(R92C_TXDW1_RAID, R92C_RAID_11B);
        w4 |= R92C_TXDW4_DRVRATE;
        w5 |= SM(R92C_TXDW5_DATARATE, 0);
    } else {
        w1 |= SM(R92C_TXDW1_MACID, R92C_MACID_BSS) | SM(R92C_TXDW1_QSEL, R92C_TXDW1_QSEL_BE) |
              SM(R92C_TXDW1_RAID, R92C_RAID_11BG) | R92C_TXDW1_AGGBK;
        w4 |= R92C_TXDW4_DRVRATE | SM(R92C_TXDW4_RTSRATE, 8);
        w5 |= 0x0001ff00u | SM(R92C_TXDW5_DATARATE, taxa);
    }
    w4 |= R92C_TXDW4_HWSEQ;
    r92c_pon32(buf, 0, w0);
    r92c_pon32(buf, 1, w1);
    r92c_pon32(buf, 2, 0);
    r92c_pon32(buf, 3, (uint32_t)R92C_TXDW3_HWSEQEN << 16);
    r92c_pon32(buf, 4, w4);
    r92c_pon32(buf, 5, w5);
    r92c_pon32(buf, 6, 0);
    r92c_pon32(buf, 7, 0);
    uint16_t soma = 0;
    for (int i = 0; i < R92C_TXDESC_SUMSIZE / 2; i++) soma ^= (uint16_t)(buf[2 * i] | buf[2 * i + 1] << 8);
    buf[2 * R92C_TXDESC_SUMOFFSET] = (uint8_t)soma;
    buf[2 * R92C_TXDESC_SUMOFFSET + 1] = (uint8_t)(soma >> 8);
}

/* ---------------------------------------------------------- recepción */

/* RSSI en dBm del estado del PHY de una trama recibida a la tasa «taxa»
 * (rtwn_get_rssi del urtwn). cck_alta: HSSI_PARAM2 tiene CCK_HIPWR. */
static inline int r92c_rssi(int taxa, const uint8_t *phy, bool cck_alta)
{
    static const int8_t cckoff[] = { 16, -12, -26, -46 };
    if (taxa <= 3) {
        const struct r92c_rx_cck *c = (const struct r92c_rx_cck *)phy;
        int rpt, rssi;
        if (cck_alta) { rpt = (c->agc_rpt >> 5) & 3; rssi = (c->agc_rpt & 0x1F) << 1; }
        else { rpt = (c->agc_rpt >> 6) & 3; rssi = c->agc_rpt & 0x3E; }
        return cckoff[rpt] - rssi;
    }
    const struct r92c_rx_phystat *f = (const struct r92c_rx_phystat *)phy;
    return (int)((f->phydw1 >> 1) & 0x7F) - 110;
}
#endif
