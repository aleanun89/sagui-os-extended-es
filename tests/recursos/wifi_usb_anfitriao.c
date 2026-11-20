// SPDX-License-Identifier: GPL-2.0-or-later
/* wifi_usb_anfitriao.c - pruebas en el anfitrión de la lógica pura de los pinchos
 * Wi-Fi USB experimentales (D409): kernel/rede/wifi_usb_logica.h (VID:PID),
 * rtl8192cu_logica.h (Realtek RTL8188CU/8192CU) y rt2870_logica.h (Ralink
 * RT2870/RT3070/RT5370), más la bandera sem_informes de kernel/lib/wifi_n.c.
 *
 * Lo compila y lo ejecuta tests/h409_wifi_usb.py (con ASan y UBSan). Recibe como
 * argumentos las rutas de los firmware de firmware/. Cada comprobación imprime
 * una línea; al final «wifi_usb ok» o el número de fallos. */
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include "wifi_n.h"
#include "../../kernel/rede/wifi_usb_logica.h"
#include "../../kernel/rede/rtl8192cu_logica.h"
#include "../../kernel/rede/rt2870_logica.h"

static int falhas;

static void ver(int ok, const char *o)
{
    printf("%s %s\n", ok ? "ok" : "FALLO", o);
    if (!ok) falhas++;
}

static uint8_t *ler(const char *ruta, long *n)
{
    FILE *f = fopen(ruta, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    *n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *b = malloc((size_t)*n);
    if (fread(b, 1, (size_t)*n, f) != (size_t)*n) { free(b); b = NULL; }
    fclose(f);
    return b;
}

/* ------------------------------------------------------------ VID:PID */

static void ids(void)
{
    ver(wusb_familia(0x0BDA, 0x8179) == WUSB_RTL8188EU, "0bda:8179 es el RTL8188EU (el de la D215)");
    ver(wusb_familia(0x0BDA, 0x8176) == WUSB_RTL8192CU, "0bda:8176 es un RTL8188CUS");
    ver(wusb_familia(0x0BDA, 0x8178) == WUSB_RTL8192CU, "0bda:8178 es un RTL8192CU");
    ver(wusb_familia(0x0BDA, 0x817A) == WUSB_RTL8192CU, "0bda:817a es un RTL8188CU");
    ver(wusb_familia(0x148F, 0x2870) == WUSB_RT2870, "148f:2870 es un RT2870");
    ver(wusb_familia(0x148F, 0x3070) == WUSB_RT2870, "148f:3070 es un RT3070");
    ver(wusb_familia(0x148F, 0x3072) == WUSB_RT2870, "148f:3072 es un RT3072");
    ver(wusb_familia(0x148F, 0x5370) == WUSB_RT2870, "148f:5370 es un RT5370");
    ver(wusb_familia(0x0CF3, 0x9271) == WUSB_AR9271, "0cf3:9271 es un AR9271");
    ver(wusb_familia(0x1234, 0x5678) == WUSB_NINGUNO, "un VID:PID cualquiera no es de ninguna familia");
    ver(wusb_familia(0x0BDA, 0x0000) == WUSB_NINGUNO, "0bda:0000 no es de ninguna familia");
    ver(!wusb_experimental(WUSB_RTL8188EU) && wusb_experimental(WUSB_RTL8192CU) && wusb_experimental(WUSB_RT2870) &&
            wusb_experimental(WUSB_AR9271), "solo el 8188EU está probado en hardware");
    ver(WUSB_N(wusb_ids_rtl8192cu) >= 60 && WUSB_N(wusb_ids_rt2870) >= 200 && WUSB_N(wusb_ids_ar9271) >= 10,
        "las tablas traen los dispositivos de urtwn, run y athn");
    /* ninguna pareja en dos familias */
    int dup = 0;
    for (int i = 0; i < WUSB_N(wusb_ids_rtl8192cu); i++) {
        if (wusb_en(wusb_ids_rt2870, WUSB_N(wusb_ids_rt2870), wusb_ids_rtl8192cu[i].vid, wusb_ids_rtl8192cu[i].pid)) dup++;
        if (wusb_en(wusb_ids_ar9271, WUSB_N(wusb_ids_ar9271), wusb_ids_rtl8192cu[i].vid, wusb_ids_rtl8192cu[i].pid)) dup++;
        if (wusb_ids_rtl8192cu[i].vid == 0x0BDA && wusb_ids_rtl8192cu[i].pid == 0x8179) dup++;
    }
    for (int i = 0; i < WUSB_N(wusb_ids_rt2870); i++)
        if (wusb_en(wusb_ids_ar9271, WUSB_N(wusb_ids_ar9271), wusb_ids_rt2870[i].vid, wusb_ids_rt2870[i].pid)) dup++;
    ver(dup == 0, "ningún VID:PID está en dos familias");
}

/* ------------------------------------------------------------ Realtek CU */

static void realtek(const char *fwA, const char *fwT)
{
    /* SYS_CFG: bit 27 = TYPE_92C (RTL8192C), bit 19 = UMC, bits 15..12 = versión, bit 23 = chip de prueba */
    r92c_chip_t c = r92c_tipo_chip(0x00080000u, 0);
    ver(c.ok && !c.c2t2r && c.umc && c.a_cut && c.ntx == 1 && c.nrx == 1, "RTL8188CU UMC corte A: 1T1R");
    ver(strcmp(r92c_nome_firmware(&c), "rtl8192cufw_A.bin") == 0, "el 8188CU UMC A usa rtl8192cufw_A.bin");
    c = r92c_tipo_chip(0x08000000u, 0);
    ver(c.ok && c.c2t2r && !c.c1t2r && !c.umc && c.ntx == 2 && c.nrx == 2, "RTL8192CU TSMC: 2T2R");
    ver(strcmp(r92c_nome_firmware(&c), "rtl8192cufw_TMSC.bin") == 0, "el 8192CU usa rtl8192cufw_TMSC.bin");
    c = r92c_tipo_chip(0x08000000u, 1u << 22);   /* HPON_FSM bonding 1 = 8192C capado a 1T2R */
    ver(c.c2t2r && c.c1t2r && c.ntx == 1 && c.nrx == 2, "RTL8192C capado: 1T2R");
    c = r92c_tipo_chip(0x00800000u, 0);
    ver(!c.ok, "el chip de prueba (TRP_VAUX_EN) no se soporta");
    c = r92c_tipo_chip(0x00081000u, 0);
    ver(c.umc && !c.a_cut, "UMC con versión 1 no es corte A");

    /* firmware reales */
    const char *rutas[2] = { fwA, fwT };
    for (int i = 0; i < 2; i++) {
        long n = 0;
        uint8_t *fw = ler(rutas[i], &n);
        ver(fw != NULL, "se lee el firmware");
        if (!fw) continue;
        uint32_t saltar; unsigned v, sv;
        const char *m = r92c_firmware_ok(fw, (uint32_t)n, &saltar, &v, &sv);
        ver(m == NULL && saltar == 32 && v > 0, "cabecera del firmware CU válida (firma 0x88c/0x92c)");
        printf("  firmware %s: %ld bytes, versión %u.%u\n", rutas[i], n, v, sv);
        fw[0] ^= 0x55; fw[1] ^= 0x55;
        ver(r92c_firmware_ok(fw, (uint32_t)n, &saltar, &v, &sv) != NULL, "una firma estropeada se rechaza");
        ver(r92c_firmware_ok(fw, 40, &saltar, &v, &sv) != NULL, "un firmware corto se rechaza");
        free(fw);
    }

    /* eFuse: MAC, tipo de placa, regulación */
    uint8_t rom[512];
    memset(rom, 0xFF, sizeof rom);
    struct r92c_rom *r = (struct r92c_rom *)rom;
    r->id = 0x8129;
    const uint8_t mac[6] = { 0x00, 0x13, 0x37, 0xab, 0xcd, 0xef };
    memcpy(r->macaddr, mac, 6);
    r->rf_opt1 = (uint8_t)(R92C_BOARD_TYPE_HIGHPA << 5 | 3);
    r92c_efuse_t e = r92c_leer_rom(rom);
    ver(e.valida && !memcmp(e.mac, mac, 6) && e.placa == R92C_BOARD_TYPE_HIGHPA && e.regulacion == 3,
        "la ROM da la MAC, la placa de alta potencia y la regulación 3");
    ver(offsetof(struct r92c_rom, macaddr) == 0x16 && sizeof(struct r92c_rom) <= 512, "struct r92c_rom: la MAC está en el desplazamiento 0x16 de la eFuse");
    memset(r->macaddr, 0xFF, 6);
    ver(!r92c_leer_rom(rom).valida, "una ROM sin MAC no vale");
    memcpy(r->macaddr, mac, 6);
    r->macaddr[0] = 0x01;
    ver(!r92c_leer_rom(rom).valida, "una MAC multicast no vale");

    /* potencia: regulación 0 (todas), cadena 0, canal 6 (grupo 1). Base de rtl8192cu_txagc[0][0]
     * = CCK 0, OFDM 0c 0c 0c 0a 08 06 04 02, MCS 0e 0d 0c 0a 08 06 04 02 (copia de la tabla de OpenBSD). */
    memset(rom, 0, sizeof rom);
    r = (struct r92c_rom *)rom;
    r->rf_opt1 = 0;                    /* regulación 0, placa dongle */
    for (int g = 0; g < 3; g++) { r->cck_tx_pwr[0][g] = 0x2d; r->ht40_1s_tx_pwr[0][g] = 0x2a; r->ofdm_tx_pwr_diff[g] = 0x02;
                                  r->ht20_tx_pwr_diff[g] = 0x01; r->ht40_2s_tx_pwr_diff[g] = 0x03; }
    uint16_t p[R92C_NPOTENCIAS];
    r92c_potencia(rom, true, R92C_BOARD_TYPE_DONGLE, 1, 0, 6, p);
    ver(p[RTWN_RIDX_CCK1] == 0x2d && p[RTWN_RIDX_CCK11] == 0x2d, "potencia CCK = base 0 + eFuse (0x2d)");
    ver(p[RTWN_RIDX_OFDM6] == 0x0c + 0x2a + 2 && p[RTWN_RIDX_OFDM54] == 0x02 + 0x2a + 2,
        "potencia OFDM = base de la tabla + HT40 1S + corrección OFDM");
    ver(p[RTWN_RIDX_MCS0] == 0x0e + 0x2a + 1 && p[RTWN_RIDX_MCS0 + 7] == 0x02 + 0x2a + 1,
        "potencia MCS = base + HT40 1S + corrección HT20");
    r92c_potencia(rom, true, R92C_BOARD_TYPE_DONGLE, 2, 0, 6, p);   /* 2T: restan 3 en la HT40 de 1 flujo */
    ver(p[RTWN_RIDX_OFDM6] == 0x0c + (0x2a - 3) + 2, "con dos cadenas de transmisión se resta la diferencia 2S");
    for (int i = 0; i < 3; i++) r->cck_tx_pwr[0][i] = 0x3f;
    r92c_potencia(rom, true, R92C_BOARD_TYPE_DONGLE, 1, 0, 6, p);
    ver(p[RTWN_RIDX_CCK1] == R92C_MAX_TX_PWR, "la potencia se recorta a 0x3f");
    r->rf_opt1 = 2;                    /* regulación 2: solo lo de la eFuse */
    r92c_potencia(rom, true, R92C_BOARD_TYPE_DONGLE, 1, 0, 6, p);
    ver(p[RTWN_RIDX_OFDM6] == 0x2a + 2 && p[RTWN_RIDX_MCS0] == 0x2a + 1, "regulación 2: sin la tabla base");
    ver(r92c_grupo_canal(1) == 0 && r92c_grupo_canal(3) == 0 && r92c_grupo_canal(4) == 1 && r92c_grupo_canal(9) == 1 &&
            r92c_grupo_canal(10) == 2 && r92c_grupo_canal(14) == 2, "grupos de canales 1-3, 4-9 y 10-14");

    /* descriptor de TX */
    uint8_t d[64];
    r92c_montar_txdesc(d, 100, 32, false, false, true, false, 8);
    uint32_t w[8];
    for (int i = 0; i < 8; i++) w[i] = (uint32_t)d[4 * i] | (uint32_t)d[4 * i + 1] << 8 | (uint32_t)d[4 * i + 2] << 16 | (uint32_t)d[4 * i + 3] << 24;
    ver((w[0] & 0xffff) == 100 && ((w[0] >> 16) & 0xff) == 32 && (w[0] & 0x80000000u) && (w[0] & 0x08000000u) && (w[0] & 0x04000000u),
        "TXDW0: longitud 100, desplazamiento 32, OWN, FSG y LSG");
    ver(((w[1] >> 22) & 3) == R92C_TXDW1_CIPHER_AES, "TXDW1: cifrado AES cuando se pide");
    ver(((w[1] >> 8) & 0x1f) == R92C_TXDW1_QSEL_BE && ((w[1] >> 16) & 0xf) == R92C_RAID_11BG, "datos: cola BE y RAID b/g");
    ver((w[4] & R92C_TXDW4_DRVRATE) && (w[4] & R92C_TXDW4_HWSEQ) && (w[5] & 0x3f) == 8, "TXDW4/5: tasa fijada por el driver (índice 8) y secuencia por hardware");
    uint16_t x = 0;
    for (int i = 0; i < 16; i++) x ^= (uint16_t)(d[2 * i] | d[2 * i + 1] << 8);
    ver(x == 0, "el XOR de las 16 palabras del descriptor es 0 (suma de control)");
    r92c_montar_txdesc(d, 40, 40, true, false, false, true, 8);
    for (int i = 0; i < 8; i++) w[i] = (uint32_t)d[4 * i] | (uint32_t)d[4 * i + 1] << 8 | (uint32_t)d[4 * i + 2] << 16 | (uint32_t)d[4 * i + 3] << 24;
    ver(((w[1] >> 8) & 0x1f) == R92C_TXDW1_QSEL_MGNT && ((w[1] >> 16) & 0xf) == R92C_RAID_11B && (w[5] & 0x3f) == 0,
        "gestión: cola de gestión, CCK 1 Mb/s");
    ver(((w[1] >> 26) & 0x1f) == 1 && ((w[0] >> 16) & 0xff) == 40, "con hueco: PKTOFF 1 y desplazamiento 40");
    r92c_montar_txdesc(d, 60, 32, false, true, false, false, 8);
    for (int i = 0; i < 8; i++) w[i] = (uint32_t)d[4 * i] | (uint32_t)d[4 * i + 1] << 8 | (uint32_t)d[4 * i + 2] << 16 | (uint32_t)d[4 * i + 3] << 24;
    ver((w[0] & 0x01000000u) && (w[5] & 0x3f) == 0, "difusión: BMCAST y CCK 1 Mb/s");

    /* RSSI */
    struct r92c_rx_phystat ph;
    memset(&ph, 0, sizeof ph);
    ph.phydw1 = 100u << 1;
    ver(r92c_rssi(11, (const uint8_t *)&ph, false) == 100 - 110, "RSSI OFDM = phydw1[7:1] - 110");
    struct r92c_rx_cck cck;
    memset(&cck, 0, sizeof cck);
    cck.agc_rpt = 0x40 | 0x10;          /* sin CCK_HIPWR: rpt = 1, rssi = 0x10 */
    ver(r92c_rssi(0, (const uint8_t *)&cck, false) == -12 - 0x10, "RSSI CCK sin alta potencia");
    cck.agc_rpt = 0x20 | 0x04;          /* con CCK_HIPWR: rpt = 1, rssi = 4 << 1 */
    ver(r92c_rssi(1, (const uint8_t *)&cck, true) == -12 - 8, "RSSI CCK con alta potencia");
}

/* ------------------------------------------------------------ wifi_n: sin informes */

static void cb_wn(void *ctx, const wn_rx_t *q) { (void)q; (*(int *)ctx)++; }
static void rel_wn(void *ctx, const uint8_t *p, uint32_t n) { (void)p; (void)n; (*(int *)ctx) += 100; }

static void sin_informes(void)
{
    /* Una trama de 30 bytes con los bits 15..14 de DW3 = 1 (informe de TX en el 8188EU, otra cosa en los CU). */
    uint8_t b[24 + 32 + 30];
    memset(b, 0, sizeof b);
    uint32_t dw0 = 30 | (4u << 16);          /* PKTLEN 30, INFOSZ 4 (32 bytes) */
    uint32_t dw3 = 1u << 14;
    for (int i = 0; i < 4; i++) { b[i] = (uint8_t)(dw0 >> (8 * i)); b[12 + i] = (uint8_t)(dw3 >> (8 * i)); }
    wn_rx_conta_t c;
    int cuenta = 0;
    memset(&c, 0, sizeof c);
    wn_rx_desglosar(b, sizeof b, cb_wn, rel_wn, &cuenta, &c);
    ver(cuenta == 0 || cuenta >= 100, "con informes (8188EU) ese descriptor es un informe de TX");
    ver(c.relatorios == 1, "se cuenta como informe");
    memset(&c, 0, sizeof c);
    c.sem_informes = 1;
    cuenta = 0;
    wn_rx_desglosar(b, sizeof b, cb_wn, rel_wn, &cuenta, &c);
    ver(cuenta == 1 && c.relatorios == 0 && c.quadros == 1, "con sem_informes (CU) es una trama normal");
}

/* ------------------------------------------------------------ Ralink */

static void ralink(const char *fwr)
{
    ver(rt_mac_soportado(0x3070) && rt_mac_soportado(0x3071) && rt_mac_soportado(0x5390) && rt_mac_soportado(0x2872),
        "soportados: RT2870, RT3070, RT3071/3072 y RT5370");
    ver(!rt_mac_soportado(0x3572) && !rt_mac_soportado(0x5592) && !rt_mac_soportado(0x3593) && !rt_mac_soportado(0x5392),
        "no soportados todavía: RT3572, RT3593, RT5392 y RT5592");
    ver(rt_fw_desplazamiento(0x3070) == 0 && rt_fw_desplazamiento(0x2872) == 0 && rt_fw_desplazamiento(0x3071) == 4096 &&
            rt_fw_desplazamiento(0x5390) == 4096, "mitad de rt2870.bin: RT2870/RT3070 la primera, RT3071/RT5370 la segunda");
    long n = 0;
    uint8_t *fw = ler(fwr, &n);
    ver(fw && n == 2 * RT_FW_TAM, "rt2870.bin ocupa 8192 bytes (dos firmwares de 4 KB)");
    if (fw) {
        int distinto = memcmp(fw, fw + RT_FW_TAM, RT_FW_TAM) != 0;
        ver(distinto, "las dos mitades son distintas");
        free(fw);
    }
    ver(!strcmp(rt_nome_rf(RT3070_RF_3020), "RT3020") && !strcmp(rt_nome_rf(RT5390_RF_5370), "RT5370"), "nombres de RF");

    /* ROM sintética de un RT3070 1T1R con RF3020 */
    uint16_t w[RT_ROM_PALABRAS];
    for (int i = 0; i < RT_ROM_PALABRAS; i++) w[i] = 0xffff;
    w[RT2860_EEPROM_MAC01] = 0x1300; w[RT2860_EEPROM_MAC23] = 0xab37; w[RT2860_EEPROM_MAC45] = 0xefcd;
    w[RT2860_EEPROM_ANTENNA] = (RT3070_RF_3020 << 8) | (1 << 4) | 1;
    w[RT2860_EEPROM_FREQ_LEDS] = 0x0f00 | 0x21;
    w[RT2860_EEPROM_LNA] = 0x0004;
    w[RT2860_EEPROM_RSSI1_2GHZ] = 0x0302;
    w[RT2860_EEPROM_RSSI2_2GHZ] = 0xff02;
    w[RT2860_EEPROM_PWR2GHZ_BASE1] = 0x0807;     /* canales 1 y 2: 7 y 8 */
    w[RT2860_EEPROM_PWR2GHZ_BASE1 + 6] = 0x7f7f;   /* canales 13 y 14: fuera de rango (127) -> 5 */
    w[RT2860_EEPROM_PWR2GHZ_BASE2] = 0x0a09;
    w[RT2860_EEPROM_CONFIG] = 0x0004;
    w[RT2860_EEPROM_RPWR] = 0x1111; w[RT2860_EEPROM_RPWR + 1] = 0x2222;
    w[RT2860_EEPROM_BBP_BASE] = 0x1234;
    rt_rom_t o;
    rt_rom_analisar(w, 0x3070, &o);
    const uint8_t mac[6] = { 0x00, 0x13, 0x37, 0xab, 0xcd, 0xef };
    ver(o.mac_valida && !memcmp(o.mac, mac, 6), "ROM RT: MAC en el orden de las palabras");
    ver(o.rf == RT3070_RF_3020 && o.ntx == 1 && o.nrx == 1, "ROM RT: RF3020 1T1R");
    ver(o.freq == 0x21 && o.lna0 == 4 && o.rssi_2ghz[0] == 2 && o.rssi_2ghz[1] == 3, "ROM RT: frecuencia, LNA y desvíos de RSSI");
    ver(o.txpow1[0] == 7 && o.txpow1[1] == 8 && o.txpow2[0] == 9 && o.txpow2[1] == 10, "ROM RT: potencia por canal (dos cadenas)");
    ver(o.txpow1[12] == 5 && o.txpow1[13] == 5, "ROM RT: las potencias fuera de rango pasan a 5");
    ver(o.ext_2ghz_lna && !o.rfswitch, "ROM RT: LNA externo");
    ver(o.txpow20mhz[0] == 0x22221111u, "ROM RT: potencia por tasa (palabras bajas y altas)");
    ver(o.bbp[0].reg == 0x12 && o.bbp[0].val == 0x34, "ROM RT: ajustes BBP del fabricante");
    w[RT2860_EEPROM_ANTENNA] = 0xffff;
    rt_rom_analisar(w, 0x3070, &o);
    ver(o.rf == RT3070_RF_3020 && o.ntx == 1 && o.nrx == 1, "ROM RT: antena sin definir -> RF3020 1T1R");
    w[0] = 0x5370; w[RT2860_EEPROM_ANTENNA] = 0x0011;
    rt_rom_analisar(w, 0x5390, &o);
    ver(o.rf == RT5390_RF_5370 && o.ntx == 1 && o.nrx == 1, "ROM RT5370: el RF sale de la palabra 0");
    w[RT2860_EEPROM_MAC01] = 0xffff; w[RT2860_EEPROM_MAC23] = 0xffff; w[RT2860_EEPROM_MAC45] = 0xffff;
    rt_rom_analisar(w, 0x3070, &o);
    ver(!o.mac_valida, "ROM RT: sin MAC no vale");

    /* TX */
    uint8_t buf[256];
    memset(buf, 0xAA, sizeof buf);
    uint32_t tot = rt_montar_tx(buf, 61, false, false, 8, false, RT_WCID_AP);
    ver(tot == 4 + ((16 + 61 + 3) & ~3u) + 4, "TX: TXD + TXWI + trama alineada a 32 bits + 4 de cola");
    ver(buf[0] + (buf[1] << 8) == ((16 + 61 + 3) & ~3u) && buf[3] == RT2860_TX_QSEL_EDCA, "TXD: longitud y cola EDCA");
    uint16_t phy = (uint16_t)(buf[6] | buf[7] << 8);
    ver((phy & RT2860_PHY_MODE) == RT2860_PHY_OFDM && (phy & RT2860_PHY_MCS) == 4, "TXWI: OFDM, MCS 4 (24 Mb/s) para el índice 8");
    ver((buf[8] & RT2860_TX_ACK) && buf[9] == RT_WCID_AP && (buf[10] | buf[11] << 8) == 61, "TXWI: pide ACK, WCID 1, longitud 61");
    ver(buf[12] == 0xAA || buf[12] == 0, "TXWI de 16 bytes: el resto a cero");
    tot = rt_montar_tx(buf, 40, true, false, 8, true, 1);
    phy = (uint16_t)(buf[6] | buf[7] << 8);
    ver((phy & RT2860_PHY_MODE) == RT2860_PHY_CCK && (phy & RT2860_PHY_MCS) == 0 && buf[9] == 0xff, "gestión: CCK 1 Mb/s y WCID 0xff");
    tot = rt_montar_tx(buf, 40, false, true, 8, true, 1);
    ver(!(buf[8] & RT2860_TX_ACK), "difusión: sin ACK");
    tot = rt_montar_tx(buf, 40, false, false, 1, true, 1);
    phy = (uint16_t)(buf[6] | buf[7] << 8);
    ver((phy & RT2860_PHY_MODE) == RT2860_PHY_CCK && (phy & RT2860_PHY_MCS) == (1 | RT2860_PHY_SHPRE), "CCK 2 Mb/s con preámbulo corto");
    ver(rt_duracao_ack(0, false) == 314 && rt_duracao_ack(8, false) == 44 && rt_duracao_ack(1, true) == 258 && rt_duracao_ack(1, false) == 162,
        "duraciones del ACK de la tabla de run(4)");
    (void)tot;
}

typedef struct { int n; uint32_t tam[8]; int rssi[8]; uint8_t pri[8]; } rtv_t;
static void cb_rt(void *ctx, uint8_t *q, uint32_t n, int rssi)
{
    rtv_t *v = ctx;
    if (v->n < 8) { v->tam[v->n] = n; v->rssi[v->n] = rssi; v->pri[v->n] = q[0]; }
    v->n++;
}

static uint32_t poner_rt(uint8_t *b, uint32_t len_trama, uint8_t rssi0, uint8_t rssi1, uint32_t flags, bool l2pad, uint8_t pri)
{
    uint32_t pad = l2pad ? 2 : 0;
    uint32_t dmalen = (16 + len_trama + pad + 3) & ~3u;
    memset(b, 0, dmalen + 8);
    b[0] = (uint8_t)dmalen; b[1] = (uint8_t)(dmalen >> 8);
    b[4 + 2] = (uint8_t)len_trama; b[4 + 3] = (uint8_t)(len_trama >> 8);
    b[4 + 8] = rssi0; b[4 + 9] = rssi1;
    b[4 + 16] = pri;
    b[4 + 17] = 0x01;                       /* to DS */
    for (uint32_t i = 2; i < len_trama; i++) b[4 + 16 + pad + i] = (uint8_t)i;
    if (l2pad) { b[4 + 16] = pri; b[4 + 17] = 0x01; b[4 + 16 + 24] = 0; }   /* cabecera de 24, 2 de relleno */
    for (int i = 0; i < 4; i++) b[4 + dmalen + i] = (uint8_t)(flags >> (8 * i));
    return dmalen + 8;
}

static void rx_ralink(void)
{
    uint8_t buf[4096];
    memset(buf, 0, sizeof buf);
    uint32_t n = 0;
    n += poner_rt(buf + n, 60, 40, 10, 0, false, 0x80);                        /* baliza */
    n += poner_rt(buf + n, 70, 30, 50, RT2860_RX_L2PAD, true, 0x08);            /* datos con relleno */
    n += poner_rt(buf + n, 50, 30, 50, RT2860_RX_CRCERR, false, 0x08);          /* con CRC malo */
    n += poner_rt(buf + n, 33, 20, 20, 0, false, 0x08);
    rtv_t v;
    rt_rx_conta_t c = { 0, 0, 0 };
    memset(&v, 0, sizeof v);
    int q = rt_rx_desglosar(buf, n, 2, 0, 0, cb_rt, &v, &c);
    ver(q == 3 && v.n == 3 && c.erros == 1 && c.cortadas == 0, "RX: 4 tramas agregadas, 3 buenas y 1 con CRC malo");
    ver(v.tam[0] == 60 && v.tam[1] == 70 && v.tam[2] == 33 && v.pri[0] == 0x80 && v.pri[1] == 0x08, "RX: longitudes y orden");
    ver(v.rssi[0] == -12 - 40 && v.rssi[1] == -12 - 50, "RX: RSSI del mejor de los dos receptores (rssi2dbm del run(4))");
    memset(&v, 0, sizeof v);
    c = (rt_rx_conta_t){ 0, 0, 0 };
    q = rt_rx_desglosar(buf, n, 1, 0, 0, cb_rt, &v, &c);
    ver(v.rssi[0] == -12 - 40 && v.rssi[1] == -12 - 30, "RX: con un solo receptor solo cuenta el primero");
    memset(&v, 0, sizeof v);
    c = (rt_rx_conta_t){ 0, 0, 0 };
    q = rt_rx_desglosar(buf, n - 5, 2, 0, 0, cb_rt, &v, &c);
    ver(c.cortadas == 1 && q == 2, "RX: una transferencia cortada no se sale de su buffer");
    memset(&v, 0, sizeof v);
    c = (rt_rx_conta_t){ 0, 0, 0 };
    uint8_t mal[32];
    memset(mal, 0, sizeof mal);
    mal[0] = 3;                          /* dmalen no múltiplo de 4 */
    q = rt_rx_desglosar(mal, sizeof mal, 2, 0, 0, cb_rt, &v, &c);
    ver(q == 0 && c.cortadas == 1, "RX: longitud DMA no alineada se descarta");
    /* basura al azar: nunca lee fuera */
    unsigned s = 12345;
    for (int k = 0; k < 2000; k++) {
        for (int i = 0; i < 256; i++) { s = s * 1103515245u + 12345u; buf[i] = (uint8_t)(s >> 16); }
        c = (rt_rx_conta_t){ 0, 0, 0 };
        memset(&v, 0, sizeof v);
        rt_rx_desglosar(buf, (uint32_t)(k % 257), 2, 0, 0, cb_rt, &v, &c);
    }
    ver(1, "RX: 2000 transferencias de basura sin salirse del buffer (ASan)");
}

int main(int argc, char **argv)
{
    if (argc < 4) { fprintf(stderr, "uso: %s fwA fwTMSC rt2870.bin\n", argv[0]); return 2; }
    ids();
    realtek(argv[1], argv[2]);
    sin_informes();
    ralink(argv[3]);
    rx_ralink();
    if (falhas) { printf("%d fallos\n", falhas); return 1; }
    printf("wifi_usb ok\n");
    return 0;
}
