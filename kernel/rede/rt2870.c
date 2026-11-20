/* rt2870.c - los pinchos Wi-Fi Ralink RT2870 / RT3070 / RT3071 / RT3072 / RT5370 (D409).
 *
 * EXPERIMENTAL, SIN PROBAR EN HARDWARE: el mantenedor no tiene estos pinchos. El
 * orden del encendido, de la carga del microcódigo, de la tabla de registros, del
 * BBP y del RF es el del run(4) de OpenBSD (licencia ISC, Copyright (c) 2007-2010
 * Damien Bergamini y otros); los registros y las tablas, en
 * kernel/terceros/bsd/run/rt2860reg.h (ver docs/terceros.md). El código es propio,
 * escrito leyéndolo. No se copió nada del rt2800usb de Linux (GPL).
 *
 * Qué hace y qué no, frente al RTL8188EU (rtl8188eu.c):
 *   - 2,4 GHz, 20 MHz, tasas b/g fijas (OFDM 24 Mb/s; sin 802.11n, sin A-MPDU);
 *   - todo el cifrado (CCMP, TKIP y WEP) por software en wifi.c: el chip no recibe
 *     claves, pasa las tramas cifradas tal cual (se evita así depender de cómo el RT
 *     quita el IV y el MIC) y las de gestión protegidas se tratan como siempre;
 *   - un solo endpoint de salida (el primero: la cola BE) y la recepción continua
 *     por usb_rx_*;
 *   - sin calibración IQ (solo la del filtro de los RT3070/3071), sin control
 *     automático de tasa, sin ahorro de energía.
 * Todo lo llama la tarea «wifi»: cada acceso es una transferencia de control USB
 * que puede dormir. */
#include "wifi.h"
#include "console.h"
#include "memoria.h"
#include "processo.h"
#include "relogio.h"
#include "texto.h"
#include "cpu.h"
#include "rt2870_logica.h"

#define RX_TAM 16384
#define TX_MAX 2400

static rt_rom_t rom;
static uint8_t rf24_20mhz;

static bool vivo(r88e_t *r)
{
    return r->presente && r->d && r->d->usado && r->d->wifi;
}

static int ctl(r88e_t *r, uint8_t tipo, uint8_t pedido, uint16_t valor, uint16_t indice, void *dados, uint16_t n)
{
    if (!vivo(r)) return USB_ERRO;
    int e = usb_controle(r->d, tipo, pedido, valor, indice, dados, n);
    if (e < 0) {
        r->erros_usb++;
        if (r->erros_usb < 8) klog("[WIFI] RT: petición %u (%x): error USB %d\n", pedido, indice, e);
    }
    return e;
}

static int rd(r88e_t *r, uint16_t reg, uint32_t *v)
{
    uint8_t b[4] = { 0xFF, 0xFF, 0xFF, 0xFF };
    int e = ctl(r, 0xC0, RT2870_READ_REGION_1, 0, reg, b, 4);
    *v = (e >= 0) ? ((uint32_t)b[0] | (uint32_t)b[1] << 8 | (uint32_t)b[2] << 16 | (uint32_t)b[3] << 24) : 0xFFFFFFFFu;
    return e < 0 ? -1 : 0;
}
static uint32_t rd4(r88e_t *r, uint16_t reg) { uint32_t v; rd(r, reg, &v); return v; }
static int wr2(r88e_t *r, uint16_t reg, uint16_t v) { return ctl(r, 0x40, RT2870_WRITE_2, v, reg, (void *)0, 0) < 0 ? -1 : 0; }
static int wr(r88e_t *r, uint16_t reg, uint32_t v)
{
    int e = wr2(r, reg, (uint16_t)(v & 0xFFFF));
    return e < 0 ? e : wr2(r, (uint16_t)(reg + 2), (uint16_t)(v >> 16));
}
static void wrr(r88e_t *r, uint16_t reg, const uint8_t *b, int n)
{
    for (int i = 0; i + 1 < n + 1 && i < n; i += 2) wr2(r, (uint16_t)(reg + i), (uint16_t)(b[i] | (i + 1 < n ? b[i + 1] << 8 : 0)));
}
static void set4(r88e_t *r, uint16_t reg, uint32_t v, int n) { for (; n > 0; n--, reg += 4) wr(r, reg, v); }

static void esperar_ms(uint32_t ms)
{
    uint32_t tiques = (ms * TIQUES_POR_SEGUNDO + 999) / 1000;
    if (!tiques) tiques = 1;
    static const char canal = 0;
    processo_atual()->acordar_em = relogio_tiques() + tiques;
    bool i0 = int_salva_desliga();
    bloquear(&canal);
    ceder();
    int_restaura(i0);
}

/* ---------------------------------------------------------- BBP, RF, MCU */

static int bbp_r(r88e_t *r, uint8_t reg, uint8_t *v)
{
    uint32_t t = 0;
    int i;
    for (i = 0; i < 10; i++) { if (rd(r, RT2860_BBP_CSR_CFG, &t) < 0) return -1; if (!(t & RT2860_BBP_CSR_KICK)) break; }
    if (i == 10) return -1;
    if (wr(r, RT2860_BBP_CSR_CFG, RT2860_BBP_CSR_READ | RT2860_BBP_CSR_KICK | (uint32_t)reg << 8) < 0) return -1;
    for (i = 0; i < 10; i++) { if (rd(r, RT2860_BBP_CSR_CFG, &t) < 0) return -1; if (!(t & RT2860_BBP_CSR_KICK)) break; }
    if (i == 10) return -1;
    *v = (uint8_t)t;
    return 0;
}
static int bbp_w(r88e_t *r, uint8_t reg, uint8_t v)
{
    uint32_t t = 0;
    int i;
    for (i = 0; i < 10; i++) { if (rd(r, RT2860_BBP_CSR_CFG, &t) < 0) return -1; if (!(t & RT2860_BBP_CSR_KICK)) break; }
    if (i == 10) return -1;
    return wr(r, RT2860_BBP_CSR_CFG, RT2860_BBP_CSR_KICK | (uint32_t)reg << 8 | v);
}
static uint8_t bbp_g(r88e_t *r, uint8_t reg) { uint8_t v = 0; bbp_r(r, reg, &v); return v; }

/* RF de los RT3070 y posteriores: 8 bits, por RF_CSR_CFG. */
static int rf3_r(r88e_t *r, uint8_t reg, uint8_t *v)
{
    uint32_t t = 0;
    int i;
    for (i = 0; i < 100; i++) { if (rd(r, RT3070_RF_CSR_CFG, &t) < 0) return -1; if (!(t & RT3070_RF_KICK)) break; }
    if (i == 100) return -1;
    if (wr(r, RT3070_RF_CSR_CFG, RT3070_RF_KICK | (uint32_t)reg << 8) < 0) return -1;
    for (i = 0; i < 100; i++) { if (rd(r, RT3070_RF_CSR_CFG, &t) < 0) return -1; if (!(t & RT3070_RF_KICK)) break; }
    if (i == 100) return -1;
    *v = (uint8_t)t;
    return 0;
}
static int rf3_w(r88e_t *r, uint8_t reg, uint8_t v)
{
    uint32_t t = 0;
    int i;
    for (i = 0; i < 10; i++) { if (rd(r, RT3070_RF_CSR_CFG, &t) < 0) return -1; if (!(t & RT3070_RF_KICK)) break; }
    if (i == 10) return -1;
    return wr(r, RT3070_RF_CSR_CFG, RT3070_RF_WRITE | RT3070_RF_KICK | (uint32_t)reg << 8 | v);
}
static uint8_t rf3_g(r88e_t *r, uint8_t reg) { uint8_t v = 0; rf3_r(r, reg, &v); return v; }

/* RF del RT2870 (RF2720/2750): 24 bits. */
static int rf2_w(r88e_t *r, uint8_t reg, uint32_t v)
{
    uint32_t t = 0;
    int i;
    for (i = 0; i < 10; i++) { if (rd(r, RT2860_RF_CSR_CFG0, &t) < 0) return -1; if (!(t & RT2860_RF_REG_CTRL)) break; }
    if (i == 10) return -1;
    return wr(r, RT2860_RF_CSR_CFG0, RT2860_RF_REG_CTRL | 24 << RT2860_RF_REG_WIDTH_SHIFT | (v & 0x3fffff) << 2 | (reg & 3));
}

static int mcu_cmd(r88e_t *r, uint8_t cmd, uint16_t arg)
{
    uint32_t t = 0;
    int i;
    for (i = 0; i < 100; i++) { if (rd(r, RT2860_H2M_MAILBOX, &t) < 0) return -1; if (!(t & RT2860_H2M_BUSY)) break; }
    if (i == 100) return -1;
    if (wr(r, RT2860_H2M_MAILBOX, RT2860_H2M_BUSY | RT2860_TOKEN_NO_INTR << 16 | arg) < 0) return -1;
    return wr(r, RT2860_HOST_CMD, cmd);
}

/* ---------------------------------------------------------- ROM (eFuse o EEPROM) */

static bool rom_efuse;

static int rom_leer(r88e_t *r, uint16_t addr, uint16_t *val)
{
    if (!rom_efuse) {
        uint8_t b[2] = { 0xFF, 0xFF };
        int e = ctl(r, 0xC0, RT2870_EEPROM_READ, 0, (uint16_t)(addr * 2), b, 2);
        *val = e >= 0 ? (uint16_t)(b[0] | b[1] << 8) : 0xFFFF;
        return e < 0 ? -1 : 0;
    }
    uint32_t t;
    if (rd(r, RT3070_EFUSE_CTRL, &t) < 0) return -1;
    uint16_t a = (uint16_t)(addr * 2);
    t &= ~(RT3070_EFSROM_MODE_MASK | RT3070_EFSROM_AIN_MASK);
    t |= (uint32_t)(a & ~0xf) << RT3070_EFSROM_AIN_SHIFT | RT3070_EFSROM_KICK;
    wr(r, RT3070_EFUSE_CTRL, t);
    int i;
    for (i = 0; i < 100; i++) {
        if (rd(r, RT3070_EFUSE_CTRL, &t) < 0) return -1;
        if (!(t & RT3070_EFSROM_KICK)) break;
    }
    if (i == 100) return -1;
    if ((t & RT3070_EFUSE_AOUT_MASK) == RT3070_EFUSE_AOUT_MASK) { *val = 0xffff; return 0; }
    if (rd(r, (uint16_t)(RT3070_EFUSE_DATA3 - (a & 0xc)), &t) < 0) return -1;
    *val = (a & 2) ? (uint16_t)(t >> 16) : (uint16_t)(t & 0xffff);
    return 0;
}

int rt28_ler_efuse(r88e_t *r, const char **motivo)
{
    *motivo = "";
    uint32_t ver = 0;
    int i;
    for (i = 0; i < 100 && vivo(r); i++) {
        if (rd(r, RT2860_ASIC_VER_ID, &ver) < 0) break;
        if (ver != 0 && ver != 0xffffffffu) break;
    }
    if (!vivo(r) || ver == 0 || ver == 0xffffffffu) { *motivo = "el pincho no responde a los registros"; return -1; }
    r->rt_mac_ver = (uint16_t)(ver >> 16);
    r->rt_rev = (uint16_t)(ver & 0xffff);
    r->sys_cfg = ver;
    if (!rt_mac_soportado(r->rt_mac_ver)) {
        static char m[80];
        snformatar(m, sizeof m, "chip Ralink RT%04X: todavía sin soporte (solo RT2870, RT3070, RT3071/3072 y RT5370)", r->rt_mac_ver);
        *motivo = m;
        return -1;
    }
    rom_efuse = false;
    if (r->rt_mac_ver >= RT_MAC_RT3070) {
        uint32_t t = rd4(r, RT3070_EFUSE_CTRL);
        if (t & RT3070_SEL_EFUSE) rom_efuse = true;
    }
    uint16_t w[RT_ROM_PALABRAS];
    for (i = 0; i < RT_ROM_PALABRAS; i++) {
        if (rom_leer(r, (uint16_t)i, &w[i]) < 0) { *motivo = "no se pudo leer la ROM del pincho"; return -1; }
    }
    rt_rom_analisar(w, r->rt_mac_ver, &rom);
    if (!rom.mac_valida) { *motivo = "la ROM no tiene una MAC válida"; return -1; }
    memcpy(r->mac, rom.mac, 6);
    r->rt_rf = (uint8_t)rom.rf;
    r->rt_ntx = rom.ntx;
    r->rt_nrx = rom.nrx;
    klog("[WIFI] RT%04X (rev %04X), RF %s, %dT%dR, ROM %s\n", r->rt_mac_ver, r->rt_rev, rt_nome_rf(rom.rf), rom.ntx, rom.nrx,
         rom_efuse ? "eFuse" : "EEPROM");
    return 0;
}

const char *rt28_nome_firmware(const r88e_t *r) { (void)r; return "rt2870.bin"; }

/* ---------------------------------------------------------- inicio */

static int cargar_microcodigo(r88e_t *r, const uint8_t *fw, uint32_t nfw, const char **motivo)
{
    uint32_t off = rt_fw_desplazamiento(r->rt_mac_ver);
    if (nfw < off + RT_FW_TAM) { *motivo = "rt2870.bin demasiado corto (se esperan 8 KB)"; return -1; }
    snformatar(r->fw_versao, sizeof r->fw_versao, "rt2870.bin +%u", off);
    wrr(r, RT2870_FW_BASE, fw + off, RT_FW_TAM);
    wr(r, RT2860_H2M_MAILBOX_CID, 0xffffffffu);
    wr(r, RT2860_H2M_MAILBOX_STATUS, 0xffffffffu);
    if (ctl(r, 0x40, RT2870_RESET, 8, 0, (void *)0, 0) < 0) { *motivo = "el pincho no aceptó el reinicio del microcódigo"; return -1; }
    esperar_ms(10);
    wr(r, RT2860_H2M_BBPAGENT, 0);
    wr(r, RT2860_H2M_MAILBOX, 0);
    wr(r, RT2860_H2M_INTSRC, 0);
    mcu_cmd(r, RT2860_MCU_CMD_RFRESET, 0);
    int i;
    for (i = 0; i < 1000 && vivo(r); i++) {
        if (rd4(r, RT2860_SYS_CTRL) & RT2860_MCU_READY) break;
        if (i > 20) esperar_ms(1);
    }
    if (i == 1000 || !vivo(r)) { *motivo = "el microcódigo no arrancó (MCU_READY)"; return -1; }
    return 0;
}

static int bbp_iniciar(r88e_t *r)
{
    uint8_t b0 = 0;
    int i;
    for (i = 0; i < 20; i++) { if (bbp_r(r, 0, &b0) < 0) return -1; if (b0 != 0 && b0 != 0xff) break; }
    if (i == 20) return -1;
    if (r->rt_mac_ver >= RT_MAC_RT5390) {
        static const struct { uint8_t reg, val; } d5[] = { RT5390_DEF_BBP };
        if (r->rt_nrx > 1) bbp_w(r, 105, bbp_g(r, 105) | RT5390_MLD);
        bbp_w(r, 4, bbp_g(r, 4) | RT5390_MAC_IF_CTRL);
        for (i = 0; i < ((int)(sizeof(d5) / sizeof((d5)[0]))); i++) bbp_w(r, d5[i].reg, d5[i].val);
        bbp_w(r, 152, bbp_g(r, 152) | 0x80);
        bbp_w(r, 154, 0);                 /* sin diversidad de antena por hardware */
        bbp_w(r, 142, 1);
        bbp_w(r, 143, 57);
    } else {
        static const struct { uint8_t reg, val; } d2[] = { RT2860_DEF_BBP };
        for (i = 0; i < ((int)(sizeof(d2) / sizeof((d2)[0]))); i++) bbp_w(r, d2[i].reg, d2[i].val);
    }
    if (r->rt_mac_ver >= RT_MAC_RT3070) {
        bbp_w(r, 79, 0x13);
        bbp_w(r, 80, 0x05);
        bbp_w(r, 81, 0x33);
    }
    return 0;
}

static int filtro_calibrar(r88e_t *r, uint8_t init, uint8_t target, uint8_t *val)
{
    uint8_t rf24 = (uint8_t)((rf3_g(r, 24) & 0xc0) | init);
    rf3_w(r, 24, rf24);
    uint8_t rf22 = rf3_g(r, 22);
    rf3_w(r, 22, rf22 | 0x01);          /* bucle de banda base */
    bbp_w(r, 24, 0x00);
    uint8_t pb = 0, sb = 0;
    int i;
    for (i = 0; i < 100; i++) {
        bbp_w(r, 25, 0x90);
        esperar_ms(1);
        pb = bbp_g(r, 55);
        if (pb != 0) break;
    }
    if (i == 100) return -1;
    bbp_w(r, 24, 0x06);
    for (i = 0; i < 100; i++) {
        bbp_w(r, 25, 0x90);
        esperar_ms(1);
        sb = bbp_g(r, 55);
        if ((uint8_t)(pb - sb) > target) break;
        rf24++;
        rf3_w(r, 24, rf24);
    }
    if (i < 100) {
        if (rf24 != init) rf24--;
        *val = rf24;
        rf3_w(r, 24, rf24);
    }
    bbp_w(r, 24, 0x00);
    rf3_w(r, 22, rf3_g(r, 22) & ~0x01);
    return 0;
}

static void rf_iniciar_3070(r88e_t *r)
{
    static const struct { uint8_t reg, val; } d[] = { RT3070_DEF_RF };
    uint32_t t;
    uint8_t rf = rf3_g(r, 30);
    rf3_w(r, 30, rf | 0x80);
    esperar_ms(1);
    rf3_w(r, 30, rf & ~0x80);
    for (int i = 0; i < ((int)(sizeof(d) / sizeof((d)[0]))); i++) rf3_w(r, d[i].reg, d[i].val);
    unsigned ver = r->rt_mac_ver, rev = r->rt_rev;
    if (ver == RT_MAC_RT3070 && rev < 0x0201) {
        t = rd4(r, RT3070_LDO_CFG0);
        wr(r, RT3070_LDO_CFG0, (t & ~0x0f000000u) | 0x0d000000u);   /* de 1,2 V a 1,35 V */
    } else if (ver == RT_MAC_RT3071) {
        rf3_w(r, 6, rf3_g(r, 6) | 0x40);
        rf3_w(r, 31, 0x14);
        t = rd4(r, RT3070_LDO_CFG0) & ~0x1f000000u;
        t |= rev < 0x0211 ? 0x0d000000u : 0x01000000u;
        wr(r, RT3070_LDO_CFG0, t);
        wr(r, RT3070_GPIO_SWITCH, rd4(r, RT3070_GPIO_SWITCH) & ~0x20u);   /* LNA_PE_G1 */
    }
    rf3_w(r, 31, rf3_g(r, 31) & ~0x20);          /* 20 MHz */
    rf24_20mhz = 0x1f;
    filtro_calibrar(r, 0x07, ver < RT_MAC_RT3071 ? 0x16 : 0x13, &rf24_20mhz);
    uint8_t b4 = bbp_g(r, 4);                    /* 40 MHz: calibra su filtro y vuelve a 20 */
    bbp_w(r, 4, (b4 & ~0x18) | 0x10);
    rf3_w(r, 31, rf3_g(r, 31) | 0x20);
    uint8_t rf24_40 = 0x2f;
    filtro_calibrar(r, 0x27, ver < RT_MAC_RT3071 ? 0x19 : 0x15, &rf24_40);
    bbp_w(r, 4, bbp_g(r, 4) & ~0x18);
    if (rev < 0x0211) rf3_w(r, 27, 0x03);
    wr(r, RT3070_OPT_14, rd4(r, RT3070_OPT_14) | 1);
    if (ver == RT_MAC_RT3070 || ver == RT_MAC_RT3071) {
        rf = rf3_g(r, 17);
        rf &= ~RT3070_TX_LO1;
        if ((ver == RT_MAC_RT3070 || (ver == RT_MAC_RT3071 && rev >= 0x0211)) && !rom.ext_2ghz_lna) rf |= 0x20;
        uint8_t mingain = ver == RT_MAC_RT3070 ? 1 : 2;
        if (rom.txmixgain_2ghz >= mingain) rf = (uint8_t)((rf & ~0x7) | rom.txmixgain_2ghz);
        rf3_w(r, 17, rf);
    }
    if (ver == RT_MAC_RT3071) {
        rf = rf3_g(r, 1);
        rf &= ~(RT3070_RX0_PD | RT3070_TX0_PD);
        rf |= RT3070_RF_BLOCK | RT3070_RX1_PD | RT3070_TX1_PD;
        rf3_w(r, 1, rf);
        rf3_w(r, 15, rf3_g(r, 15) & ~RT3070_TX_LO2);
        rf3_w(r, 20, rf3_g(r, 20) & ~RT3070_RX_LO1);
        rf3_w(r, 21, rf3_g(r, 21) & ~RT3070_RX_LO2);
    }
    if (ver == RT_MAC_RT3070 || ver == RT_MAC_RT3071) {
        rf = rf3_g(r, 27);
        rf &= ~0x77;
        if (rev < 0x0211) rf |= 0x03;
        rf3_w(r, 27, rf);
    }
}

static void rf_iniciar_5390(r88e_t *r)
{
    static const struct { uint8_t reg, val; } d[] = { RT5390_DEF_RF };
    uint8_t rf = rf3_g(r, 2);
    rf3_w(r, 2, rf | RT3593_RESCAL);
    esperar_ms(1);
    rf3_w(r, 2, rf & ~RT3593_RESCAL);
    for (int i = 0; i < ((int)(sizeof(d) / sizeof((d)[0]))); i++) rf3_w(r, d[i].reg, d[i].val);
    if (r->rt_rev >= 0x0502) {
        rf3_w(r, 6, 0xe0); rf3_w(r, 25, 0x80); rf3_w(r, 46, 0x73);
        rf3_w(r, 53, 0x00); rf3_w(r, 56, 0x42); rf3_w(r, 61, 0xd1);
    }
    rf24_20mhz = 0x1f;
    if (r->rt_rev < 0x0211) rf3_w(r, 27, 0x3);
    wr(r, RT3070_OPT_14, rd4(r, RT3070_OPT_14) | 1);
}

static void rf_ajustes_3070(r88e_t *r)
{
    unsigned ver = r->rt_mac_ver, rev = r->rt_rev;
    uint8_t bbp;
    if (ver == RT_MAC_RT3071) {
        if (rev >= 0x0211) { bbp_w(r, 103, 0xc0); bbp_w(r, 31, bbp_g(r, 31) & ~0x03); }
        bbp = bbp_g(r, 138);
        if (r->rt_ntx == 1) bbp |= 0x20;
        if (r->rt_nrx == 1) bbp &= ~0x02;
        bbp_w(r, 138, bbp);
        wr(r, RT2860_TX_SW_CFG1, 0);
        if (rev < 0x0211) wr(r, RT2860_TX_SW_CFG2, rom.patch_dac ? 0x2c : 0x0f);
        else wr(r, RT2860_TX_SW_CFG2, 0);
    } else if (ver == RT_MAC_RT3070) {
        if (rev >= 0x0201) { bbp_w(r, 103, 0xc0); bbp_w(r, 31, bbp_g(r, 31) & ~0x03); }
        if (rev < 0x0201) { wr(r, RT2860_TX_SW_CFG1, 0); wr(r, RT2860_TX_SW_CFG2, 0x2c); }
        else wr(r, RT2860_TX_SW_CFG2, 0);
    }
    if (ver >= RT_MAC_RT3071)
        for (int i = 0; i < 10; i++)
            if (rom.rfv[i].reg != 0 && rom.rfv[i].reg != 0xff) rf3_w(r, rom.rfv[i].reg, rom.rfv[i].val);
}

static void rf_ajustes_5390(r88e_t *r)
{
    uint8_t bbp;
    if (r->rt_rev >= 0x0211) { bbp_w(r, 103, 0xc0); bbp_w(r, 31, bbp_g(r, 31) & ~0x03); }
    bbp = bbp_g(r, 138);
    if (r->rt_ntx == 1) bbp |= 0x20;
    if (r->rt_nrx == 1) bbp &= ~0x02;
    bbp_w(r, 138, bbp);
    rf3_w(r, 38, rf3_g(r, 38) & ~RT5390_RX_LO1);
    rf3_w(r, 39, rf3_g(r, 39) & ~RT5390_RX_LO2);
    bbp_w(r, 4, bbp_g(r, 4) | RT5390_MAC_IF_CTRL);
    rf3_w(r, 30, (uint8_t)((rf3_g(r, 30) & ~0x18) | 0x10));
    wr(r, RT2860_TX_SW_CFG1, 0);
    if (r->rt_rev < 0x0211) wr(r, RT2860_TX_SW_CFG2, rom.patch_dac ? 0x2c : 0x0f);
    else wr(r, RT2860_TX_SW_CFG2, 0);
}

static void ajustar_frecuencia(r88e_t *r)
{
    uint8_t rf = rf3_g(r, 17), t = rf;
    rf = (uint8_t)((rf & ~0x7f) | (rom.freq & 0x7f));
    if (rf > 0x5f) rf = 0x5f;
    if (t != rf) mcu_cmd(r, 0x74, (uint16_t)(t << 8 | rf));
}

/* ---------------------------------------------------------- canal */

void rt28_canal(r88e_t *r, uint8_t canal)
{
    if (canal < 1 || canal > 14) return;
    static const struct rfprog { uint8_t chan; uint32_t r1, r2, r3, r4; } rf2850[] = { RT2860_RF2850 };
    static const struct { uint8_t n, r, k; } fr[] = { RT3070_RF3052 };
    int i = canal - 1;                      /* los 14 primeros de la tabla son los canales 1..14 */
    int8_t p1 = rom.txpow1[i], p2 = rom.txpow2[i];
    unsigned ver = r->rt_mac_ver;
    uint8_t rf;
    if (ver >= RT_MAC_RT5390) {
        rf3_w(r, 8, fr[i].n);
        rf3_w(r, 9, fr[i].k & 0x0f);
        rf3_w(r, 11, (uint8_t)((rf3_g(r, 11) & ~0x03) | (fr[i].r & 0x03)));
        rf = (uint8_t)((rf3_g(r, 49) & ~0x3f) | (p1 & 0x3f));
        if ((rf & 0x3f) > 0x27) rf = (uint8_t)((rf & ~0x3f) | 0x27);
        rf3_w(r, 49, rf);
        rf3_w(r, 1, rf3_g(r, 1) | RT3070_RF_BLOCK | RT3070_PLL_PD | RT3070_RX0_PD | RT3070_TX0_PD);
        rf = rf3_g(r, 2);
        rf3_w(r, 2, rf | 0x80);
        esperar_ms(1);
        rf3_w(r, 2, rf & 0x7f);
        ajustar_frecuencia(r);
        if (r->rt_rev >= 0x0502) {
            rf3_w(r, 55, canal <= 11 ? 0x43 : 0x23);
            rf3_w(r, 59, canal <= 11 ? 0x0f : canal == 12 ? 0x0d : 0x0b);
        } else {
            rf3_w(r, 55, 0x44);
            rf3_w(r, 59, 0x8f);
        }
        rf3_w(r, 3, rf3_g(r, 3) | RT3593_VCOCAL);
    } else if (ver >= RT_MAC_RT3070) {
        rf3_w(r, 2, fr[i].n);
        rf3_w(r, 3, (uint8_t)((rf3_g(r, 3) & ~0x0f) | fr[i].k));
        rf3_w(r, 6, (uint8_t)((rf3_g(r, 6) & ~0x03) | fr[i].r));
        rf3_w(r, 12, (uint8_t)((rf3_g(r, 12) & ~0x1f) | p1));
        rf3_w(r, 13, (uint8_t)((rf3_g(r, 13) & ~0x1f) | p2));
        rf = rf3_g(r, 1);
        rf &= ~0xfc;
        if (r->rt_ntx == 1) rf |= 1 << 7 | 1 << 5;
        else if (r->rt_ntx == 2) rf |= 1 << 7;
        if (r->rt_nrx == 1) rf |= 1 << 6 | 1 << 4;
        else if (r->rt_nrx == 2) rf |= 1 << 6;
        rf3_w(r, 1, rf);
        rf3_w(r, 23, (uint8_t)((rf3_g(r, 23) & ~0x7f) | rom.freq));
        rf3_w(r, 24, (uint8_t)((rf3_g(r, 24) & ~0x3f) | rf24_20mhz));
        rf3_w(r, 31, (uint8_t)((rf3_g(r, 31) & ~0x3f) | rf24_20mhz));
        rf3_w(r, 7, rf3_g(r, 7) | 0x01);
    } else {   /* RT2870 (RF2720/2750) */
        uint32_t r2 = rf2850[i].r2;
        if (r->rt_ntx == 1) r2 |= 1 << 12;
        if (r->rt_nrx == 1) r2 |= 1 << 15 | 1 << 4;
        else if (r->rt_nrx == 2) r2 |= 1 << 4;
        uint32_t r3 = rf2850[i].r3 | (uint32_t)p1 << 7, r4 = rf2850[i].r4 | (uint32_t)rom.freq << 13 | (uint32_t)p2 << 4;
        rf2_w(r, RT2860_RF1, rf2850[i].r1); rf2_w(r, RT2860_RF2, r2); rf2_w(r, RT2860_RF3, r3); rf2_w(r, RT2860_RF4, r4);
        esperar_ms(1);
        rf2_w(r, RT2860_RF1, rf2850[i].r1); rf2_w(r, RT2860_RF2, r2); rf2_w(r, RT2860_RF3, r3 | 1); rf2_w(r, RT2860_RF4, r4);
        esperar_ms(1);
        rf2_w(r, RT2860_RF1, rf2850[i].r1); rf2_w(r, RT2860_RF2, r2); rf2_w(r, RT2860_RF3, r3); rf2_w(r, RT2860_RF4, r4);
    }
    /* grupo de canales 0 (2,4 GHz): run_select_chan_group */
    uint8_t lna = rom.lna0;
    bbp_w(r, 62, (uint8_t)(0x37 - lna));
    bbp_w(r, 63, (uint8_t)(0x37 - lna));
    bbp_w(r, 64, (uint8_t)(0x37 - lna));
    bbp_w(r, 86, 0x00);
    if (rom.ext_2ghz_lna) {
        if (ver >= RT_MAC_RT5390) bbp_w(r, 75, 0x52);
        else { bbp_w(r, 82, 0x62); bbp_w(r, 75, 0x46); }
    } else {
        if (ver >= RT_MAC_RT5390) bbp_w(r, 75, 0x50);
        else { bbp_w(r, 82, 0x84); bbp_w(r, 75, 0x50); }
    }
    uint32_t t = rd4(r, RT2860_TX_BAND_CFG);
    t &= ~(RT2860_5G_BAND_SEL_N | RT2860_5G_BAND_SEL_P);
    t |= RT2860_5G_BAND_SEL_N;
    wr(r, RT2860_TX_BAND_CFG, t);
    t = RT2860_RFTR_EN | RT2860_TRSW_EN | RT2860_LNA_PE0_EN;
    if (r->rt_nrx > 1) t |= RT2860_LNA_PE1_EN;
    t |= RT2860_PA_PE_G0_EN;
    if (r->rt_ntx > 1) t |= RT2860_PA_PE_G1_EN;
    wr(r, RT2860_TX_PIN_CFG, t);
    bbp_w(r, 66, (uint8_t)(ver >= RT_MAC_RT3070 ? 0x1c + lna * 2 : 0x2e + lna));
    esperar_ms(1);
    r->canal = canal;
}

/* ---------------------------------------------------------- recepción y envío */

static int rx_abrir(r88e_t *r)
{
    r->rx_fila = 0;
    if (!r->rx_buf) {
        uint32_t b = memfisica_alocar_seguidas(8);
        if (!b) return -1;
        r->rx_buf = (b + 0x3FFFu) & ~0x3FFFu;
    }
    r->rx_h = usb_rx_abrir(r->d, r->rx_buf, RX_TAM);
    return r->rx_h >= 0 ? 0 : -1;
}

static int leds(r88e_t *r, uint16_t cual) { return mcu_cmd(r, RT2860_MCU_CMD_LEDS, (uint16_t)(cual | (rom.leds & 0x7f))); }

int rt28_iniciar(r88e_t *r, const uint8_t *fw, uint32_t nfw, const char **motivo)
{
    *motivo = "";
    uint32_t t = 0;
    int i;
    if (cargar_microcodigo(r, fw, nfw, motivo) < 0) return -1;
    uint8_t *m = r->mac;
    wr(r, RT2860_MAC_ADDR_DW0, (uint32_t)m[0] | (uint32_t)m[1] << 8 | (uint32_t)m[2] << 16 | (uint32_t)m[3] << 24);
    wr(r, RT2860_MAC_ADDR_DW1, (uint32_t)m[4] | (uint32_t)m[5] << 8 | 0xffu << 16);
    for (i = 0; i < 100 && vivo(r); i++) {
        t = rd4(r, RT2860_WPDMA_GLO_CFG);
        if ((t & (RT2860_TX_DMA_BUSY | RT2860_RX_DMA_BUSY)) == 0) break;
        esperar_ms(1);
    }
    if (i == 100 || !vivo(r)) { *motivo = "el DMA del pincho no se detiene"; return -1; }
    t &= 0xff0;
    t |= RT2860_TX_WB_DDONE;
    wr(r, RT2860_WPDMA_GLO_CFG, t);
    wr(r, RT2860_SYS_CTRL, rd4(r, RT2860_SYS_CTRL) & ~RT2860_PME_OEN);
    wr(r, RT2860_MAC_SYS_CTRL, RT2860_BBP_HRST | RT2860_MAC_SRST);
    wr(r, RT2860_USB_DMA_CFG, 0);
    if (ctl(r, 0x40, RT2870_RESET, 1, 0, (void *)0, 0) < 0) { *motivo = "no se pudo reiniciar el chip"; return -1; }
    wr(r, RT2860_MAC_SYS_CTRL, 0);
    for (i = 0; i < 5; i++)
        if (rom.txpow20mhz[i] != 0xffffffffu) wr(r, (uint16_t)RT2860_TX_PWR_CFG(i), rom.txpow20mhz[i]);
    static const struct { uint32_t reg, val; } defmac[] = { RT2870_DEF_MAC };
    for (i = 0; i < ((int)(sizeof(defmac) / sizeof((defmac)[0]))); i++) wr(r, (uint16_t)defmac[i].reg, defmac[i].val);
    wr(r, RT2860_WMM_AIFSN_CFG, 0x00002273);
    wr(r, RT2860_WMM_CWMIN_CFG, 0x00002344);
    wr(r, RT2860_WMM_CWMAX_CFG, 0x000034aa);
    if (r->rt_mac_ver >= RT_MAC_RT5390) wr(r, RT2860_TX_SW_CFG0, 4 << RT2860_DLY_PAPE_EN_SHIFT | 4);
    else if (r->rt_mac_ver >= RT_MAC_RT3070) wr(r, RT2860_TX_SW_CFG0, 4 << RT2860_DLY_PAPE_EN_SHIFT);
    for (i = 0; i < 100 && vivo(r); i++) {
        t = rd4(r, RT2860_MAC_STATUS_REG);
        if (!(t & (RT2860_RX_STATUS_BUSY | RT2860_TX_STATUS_BUSY))) break;
        esperar_ms(1);
    }
    if (i == 100 || !vivo(r)) { *motivo = "el MAC del pincho no se detiene"; return -1; }
    wr(r, RT2860_H2M_BBPAGENT, 0);
    wr(r, RT2860_H2M_MAILBOX, 0);
    esperar_ms(1);
    if (bbp_iniciar(r) < 0) { *motivo = "no se pudo iniciar el BBP"; return -1; }
    t = rd4(r, RT2860_BCN_TIME_CFG);
    t &= ~(RT2860_BCN_TX_EN | RT2860_TSF_TIMER_EN | RT2860_TBTT_TIMER_EN);
    wr(r, RT2860_BCN_TIME_CFG, t);
    set4(r, RT2860_WCID_ENTRY(0), 0, 512);
    set4(r, RT2860_WCID_ATTR(0), 0, 8 * 32);
    set4(r, RT2860_SKEY(0, 0), 0, 8 * 32);
    set4(r, RT2860_SKEY_MODE_0_7, 0, 4);
    t = rd4(r, RT2860_US_CYC_CNT);
    wr(r, RT2860_US_CYC_CNT, (t & ~0xffu) | 0x1e);
    if (r->rt_rev != 0x0101) wr(r, RT2860_TXOP_CTRL_CFG, 0x0000583f);
    wr(r, RT2860_WMM_TXOP0_CFG, 0);
    wr(r, RT2860_WMM_TXOP1_CFG, 48 << 16 | 96);
    for (i = 0; i < 8; i++)
        if (rom.bbp[i].reg != 0 && rom.bbp[i].reg != 0xff) bbp_w(r, rom.bbp[i].reg, rom.bbp[i].val);
    if (r->rt_rf == RT3070_RF_3020 || r->rt_rf == RT5390_RF_5370) {   /* antena principal en los 1T1R */
        if (r->rt_rf == RT5390_RF_5370) bbp_w(r, 152, bbp_g(r, 152) | 0x80);
        else { mcu_cmd(r, RT2860_MCU_CMD_ANTSEL, 1); wr(r, RT2860_GPIO_CTRL, rd4(r, RT2860_GPIO_CTRL) & ~0x0808u); }
    }
    mcu_cmd(r, RT2860_MCU_CMD_LED1, rom.led[0]);
    mcu_cmd(r, RT2860_MCU_CMD_LED2, rom.led[1]);
    mcu_cmd(r, RT2860_MCU_CMD_LED3, rom.led[2]);
    if (r->rt_mac_ver >= RT_MAC_RT5390) rf_iniciar_5390(r);
    else if (r->rt_mac_ver >= RT_MAC_RT3070) rf_iniciar_3070(r);
    uint8_t b3 = bbp_g(r, 3);
    b3 &= ~(1 << 3 | 1 << 4);
    if (r->rt_nrx == 2) b3 |= 1 << 3;
    bbp_w(r, 3, b3);
    uint8_t b1 = bbp_g(r, 1);
    if (r->rt_ntx == 1) b1 &= ~(1 << 3 | 1 << 4);
    bbp_w(r, 1, b1);
    if (r->rt_mac_ver >= RT_MAC_RT5390) rf_ajustes_5390(r);
    else if (r->rt_mac_ver >= RT_MAC_RT3070) rf_ajustes_3070(r);
    rt28_canal(r, 1);
    leds(r, RT2860_LED_RADIO);
    if (!vivo(r)) { *motivo = "el pincho se retiró durante el inicio"; return -1; }
    if (rx_abrir(r) < 0) { *motivo = "no se pudo armar la recepción USB"; return -1; }
    /* activar transmisión y recepción */
    wr(r, RT2860_MAC_SYS_CTRL, RT2860_MAC_TX_EN);
    for (i = 0; i < 200 && vivo(r); i++) {
        t = rd4(r, RT2860_WPDMA_GLO_CFG);
        if ((t & (RT2860_TX_DMA_BUSY | RT2860_RX_DMA_BUSY)) == 0) break;
        esperar_ms(1);
    }
    if (i == 200 || !vivo(r)) { *motivo = "el DMA del pincho no se detiene (activación)"; return -1; }
    t |= RT2860_RX_DMA_EN | RT2860_TX_DMA_EN | RT2860_TX_WB_DDONE;
    wr(r, RT2860_WPDMA_GLO_CFG, t);
    wr(r, RT2860_USB_DMA_CFG, RT2860_USB_TX_EN | RT2860_USB_RX_EN | RT2860_USB_RX_AGG_EN | RT2860_USB_RX_AGG_TO(128) |
                              RT2860_USB_RX_AGG_LMT(2));
    /* filtro de recepción: como estación (no monitor) */
    wr(r, RT2860_RX_FILTR_CFG, RT2860_DROP_CRC_ERR | RT2860_DROP_PHY_ERR | RT2860_DROP_UC_NOME | RT2860_DROP_DUPL |
                               RT2860_DROP_CTS | RT2860_DROP_BA | RT2860_DROP_ACK | RT2860_DROP_VER_ERR |
                               RT2860_DROP_CTRL_RSV | RT2860_DROP_CFACK | RT2860_DROP_CFEND | RT2860_DROP_RTS |
                               RT2860_DROP_PSPOLL);
    wr(r, RT2860_MAC_SYS_CTRL, RT2860_MAC_RX_EN | RT2860_MAC_TX_EN);
    rt28_ht(r, false, false, 0);
    return vivo(r) ? 0 : -1;
}

void rt28_modo_busca(r88e_t *r, bool busca) { (void)r; (void)busca; }

void rt28_ht(r88e_t *r, bool ht, bool sgi, uint8_t mcs_ap)
{
    (void)ht; (void)sgi; (void)mcs_ap;
    r->ht = r->sgi = false;
    r->mcs_ap = 0;
    r->taxa = 8;           /* OFDM 24 Mb/s fijos (índice 4 + 4) */
}

void rt28_associado(r88e_t *r, const uint8_t b[6], uint16_t intervalo, bool slot_curto, bool preambulo_curto)
{
    wr(r, RT2860_MAC_BSSID_DW0, (uint32_t)b[0] | (uint32_t)b[1] << 8 | (uint32_t)b[2] << 16 | (uint32_t)b[3] << 24);
    wr(r, RT2860_MAC_BSSID_DW1, (uint32_t)b[4] | (uint32_t)b[5] << 8);
    wrr(r, RT2860_WCID_ENTRY(RT_WCID_AP), b, 6);
    wr(r, RT2860_LEGACY_BASIC_RATE, 0x15f);
    uint32_t t = rd4(r, RT2860_BKOFF_SLOT_CFG);
    wr(r, RT2860_BKOFF_SLOT_CFG, (t & ~0xffu) | (slot_curto ? 9 : 20));
    t = rd4(r, RT2860_AUTO_RSP_CFG);
    if (preambulo_curto) t |= RT2860_CCK_SHORT_EN; else t &= ~RT2860_CCK_SHORT_EN;
    wr(r, RT2860_AUTO_RSP_CFG, t);
#define OFDM(m) (1 << 3 | (m))
    wr(r, RT2860_LG_FBK_CFG0, OFDM(6) << 28 | OFDM(5) << 24 | OFDM(4) << 20 | OFDM(3) << 16 | OFDM(2) << 12 | OFDM(1) << 8 | OFDM(0) << 4 | OFDM(0));
#undef OFDM
    wr(r, RT2860_LG_FBK_CFG1, 2 << 12 | 1 << 8 | 0 << 4 | 0);
    t = rd4(r, RT2860_BCN_TIME_CFG);
    t &= ~0x1fffffu;
    t |= (uint32_t)(intervalo ? intervalo : 100) * 16;
    t |= RT2860_TSF_TIMER_EN | RT2860_TBTT_TIMER_EN | 1u << RT2860_TSF_SYNC_MODE_SHIFT;
    wr(r, RT2860_BCN_TIME_CFG, t);
    r->rt_wcid_seq = 1;
    leds(r, RT2860_LED_RADIO | RT2860_LED_LINK_2GHZ);
}

void rt28_desassociado(r88e_t *r)
{
    leds(r, RT2860_LED_RADIO);
    uint32_t t = rd4(r, RT2860_BCN_TIME_CFG);
    wr(r, RT2860_BCN_TIME_CFG, t & ~(RT2860_BCN_TX_EN | RT2860_TSF_TIMER_EN | RT2860_TBTT_TIMER_EN));
    static const uint8_t cero[6];
    wrr(r, RT2860_WCID_ENTRY(RT_WCID_AP), cero, 6);
    r->rt_wcid_seq = 0;
}

/* El cifrado va por software (wifi.c): no se instalan claves en el chip. */
void rt28_chave(r88e_t *r, int entrada, const uint8_t chave[16], const uint8_t *mac, uint8_t keyid)
{
    (void)r; (void)entrada; (void)chave; (void)mac; (void)keyid;
}
void rt28_limpar_chaves(r88e_t *r) { (void)r; }

int rt28_enviar(r88e_t *r, const uint8_t *q, uint32_t n, int tipo, bool cifrar)
{
    static uint8_t buf[TX_MAX + 64];
    (void)cifrar;
    if (n < 24 || n > TX_MAX || !vivo(r)) return -1;
    bool gestao = tipo == R88E_TX_GESTAO, dif = (q[4] & 1) != 0;
    uint8_t ridx = (gestao || dif) ? 0 : (tipo == R88E_TX_EAPOL ? 4 : (r->taxa > 11 ? 8 : r->taxa));
    uint32_t total = rt_montar_tx(buf, n, gestao, dif, ridx, false, r->rt_wcid_seq ? RT_WCID_AP : 0xff);
    uint8_t *tr = buf + RT_TX_CAB + RT_TXWI;
    memcpy(tr, q, n);
    if (!dif) {   /* la duración de la trama la pone el driver (run(4)): lo que dura el ACK */
        uint16_t d = rt_duracao_ack(ridx, false);
        tr[2] = (uint8_t)d; tr[3] = (uint8_t)(d >> 8);
    }
    uint32_t mps = r->d->mps_saida ? r->d->mps_saida : 512;
    while ((total % mps) == 0 && total < sizeof buf) buf[total++] = 0;   /* el USB necesita un paquete corto */
    int e = usb_bulk(r->d, false, buf, total);
    if (e < 0) { r->erros_usb++; r->tx_perdidas++; return e; }
    r->tx_transf++;
    r->tx_bytes_usb += total;
    return 0;
}

typedef struct { r88e_t *r; r88e_rx_t chegou; } rx_ctx_t;

static void rx_quadro(void *ctx, uint8_t *q, uint32_t n, int rssi)
{
    rx_ctx_t *x = ctx;
    if ((q[0] & 0x0C) == 0x08) x->r->taxa_rx = 8;
    x->chegou(q, n, rssi, false);   /* el cifrado lo trata wifi.c */
}

int rt28_receber(r88e_t *r, r88e_rx_t chegou)
{
    static uint8_t buf[RX_TAM];
    if (!vivo(r)) return -1;
    int len = usb_rx_ler(r->d, r->rx_h, buf, sizeof buf);
    if (len <= 0) return len;
    r->rx_transf++;
    r->rx_bytes_usb += (uint32_t)len;
    rx_ctx_t x = { r, chegou };
    rt_rx_conta_t c = { 0, 0, 0 };
    int q = rt_rx_desglosar(buf, (uint32_t)len, r->rt_nrx, rom.rssi_2ghz[0], rom.lna0, rx_quadro, &x, &c);
    r->rx.quadros += c.quadros;
    r->rx.erros += c.erros;
    r->rx.cortadas += c.cortadas;
    return q ? q : 1;
}
