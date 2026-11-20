// SPDX-License-Identifier: GPL-2.0-or-later
/* bt_fw_unidad.c - pruebas en el PC de la carga de firmware de los controladores Bluetooth (D411):
 * análisis de los .hcd de Broadcom, de los archivos «Realtech» de Realtek (los reales de linux-firmware,
 * de firmware/realtek/rtl_bt, y otros hechos a mano), fragmentos, nombres, y la máquina de estados contra
 * el controlador HCI simulado (tests/bt_sim.c) con Broadcom, Realtek y Atheros simulados.
 * Uso: bt_fw_unidad <raíz del repositorio>. Sale con 0 si todo bien. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bt_sim.h"
#include "../kernel/bluetooth/bt_fw.h"

static int fallos, pruebas;
#define CHECA(c, ...) do { pruebas++; if (!(c)) { fallos++; printf("FALLA (l.%d): ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static const char *raiz = ".";

static uint8_t *leer(const char *rel, int *n)
{
    char ruta[512];
    snprintf(ruta, sizeof ruta, "%s/%s", raiz, rel);
    FILE *f = fopen(ruta, "rb");
    if (!f) { printf("no puedo abrir %s\n", ruta); exit(2); }
    fseek(f, 0, SEEK_END);
    *n = (int)ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *d = malloc((size_t)*n + 1);
    if (fread(d, 1, (size_t)*n, f) != (size_t)*n) exit(2);
    fclose(f);
    return d;
}

/* ------------------------------------------------------------------ Broadcom: .hcd */

static int hcd_hecho(uint8_t *o)
{
    int k = 0;
    static const uint8_t r1[] = { 0x4C, 0xFC, 0x08, 0x00, 0x00, 0x21, 0x00, 1, 2, 3, 4 };    /* Write_RAM */
    static const uint8_t r2[] = { 0x4C, 0xFC, 0x06, 0x04, 0x00, 0x21, 0x00, 5, 6 };
    static const uint8_t r3[] = { 0x4E, 0xFC, 0x04, 0x00, 0x00, 0x21, 0x00 };                /* Launch_RAM */
    memcpy(o + k, r1, sizeof r1); k += (int)sizeof r1;
    memcpy(o + k, r2, sizeof r2); k += (int)sizeof r2;
    memcpy(o + k, r3, sizeof r3); k += (int)sizeof r3;
    return k;
}

static void t_hcd(void)
{
    uint8_t h[64];
    int n = hcd_hecho(h);
    CHECA(bt_hcd_validar(h, n) == 3, "hcd valido: 3 registros (%d)", bt_hcd_validar(h, n));
    int pos = 0;
    bt_hcd_reg_t g;
    CHECA(bt_hcd_siguiente(h, n, &pos, &g) == 1 && g.op == 0xFC4C && g.n == 8 && g.par[4] == 1, "primer registro");
    CHECA(bt_hcd_siguiente(h, n, &pos, &g) == 1 && g.op == 0xFC4C && g.n == 6, "segundo");
    CHECA(bt_hcd_siguiente(h, n, &pos, &g) == 1 && g.op == 0xFC4E && g.n == 4, "tercero (Launch_RAM)");
    CHECA(bt_hcd_siguiente(h, n, &pos, &g) == 0, "fin");
    CHECA(bt_hcd_validar(h, n - 1) < 0, "recortado en un byte: corrupto");
    CHECA(bt_hcd_validar(h, 2) < 0 && bt_hcd_validar(NULL, 0) < 0, "demasiado corto");
    uint8_t m[8] = { 0x03, 0x0C, 0x00 };   /* Reset (OGF 3): no es de fabricante */
    CHECA(bt_hcd_validar(m, 3) == -2, "una orden que no es de fabricante no es un parche");
    uint8_t g2[4] = { 0x4C, 0xFC, 0x00, 0xFF };   /* basura al final */
    CHECA(bt_hcd_validar(g2, 4) < 0, "cola suelta");
    char nom[64];
    uint8_t loc[248] = "BCM20702A1 #AB 37.4MHz";
    CHECA(bt_bcm_nombre(loc, 248, 0x0A5C, 0x21E8, 0, nom, sizeof nom) == 0 && !strcmp(nom, "brcm/BCM20702A1-0a5c-21e8.hcd"), "nombre con id: %s", nom);
    CHECA(bt_bcm_nombre(loc, 248, 0x0A5C, 0x21E8, 1, nom, sizeof nom) == 0 && !strcmp(nom, "brcm/BCM20702A1.hcd"), "nombre sin id: %s", nom);
    uint8_t loc2[248] = "BCM4335C0_001.002";
    CHECA(bt_bcm_nombre(loc2, 248, 1, 2, 1, nom, sizeof nom) == 0 && !strcmp(nom, "brcm/BCM4335C0.hcd"), "corta en el guion bajo: %s", nom);
    uint8_t loc3[248] = "  ";
    CHECA(bt_bcm_nombre(loc3, 248, 1, 2, 0, nom, sizeof nom) < 0, "nombre local vacio");
}

/* ------------------------------------------------------------------ Realtek */

static void t_rtl_tabla(void)
{
    const bt_rtl_chip_t *c = bt_rtl_buscar(0x8723, 0x000B, 6);
    CHECA(c && !strcmp(c->fw, "rtl_bt/rtl8723b_fw.bin"), "RTL8723B");
    c = bt_rtl_buscar(0x8723, 0x000D, 8);
    CHECA(c && !strcmp(c->fw, "rtl_bt/rtl8723d_fw.bin"), "RTL8723D");
    c = bt_rtl_buscar(0x8821, 0x000A, 6);
    CHECA(c && !strcmp(c->fw, "rtl_bt/rtl8821a_fw.bin"), "RTL8821A");
    c = bt_rtl_buscar(0x8821, 0x000C, 8);
    CHECA(c && !strcmp(c->fw, "rtl_bt/rtl8821c_fw.bin"), "RTL8821C");
    c = bt_rtl_buscar(0x8761, 0x000B, 0x0A);
    CHECA(c && !strcmp(c->fw, "rtl_bt/rtl8761bu_fw.bin") && !strcmp(c->cfg, "rtl_bt/rtl8761bu_config.bin"), "RTL8761B(U)");
    CHECA(!bt_rtl_buscar(0x1234, 0x000B, 6) && !bt_rtl_buscar(0x8723, 0x0001, 6), "chip desconocido o ya parcheado");
}

/* Reconstruye el flujo con todos los fragmentos y lo compara con el esperado; comprueba los índices. */
static void comprobar_flujo(const char *que, const bt_rtl_parche_t *p, const uint8_t *cfg, int ncfg)
{
    int total = p->n + ncfg;
    int nf = bt_rtl_n_frag(total);
    uint8_t *flujo = malloc((size_t)total + 300);
    int k = 0, mal = 0;
    for (int i = 0; i < nf; i++) {
        uint8_t b[256];
        int n = bt_rtl_fragmento(p, cfg, ncfg, i, b);
        uint8_t esp = (uint8_t)(i > 0x7F ? (i & 0x7F) + 1 : i);
        if (i == nf - 1) esp |= 0x80;
        if (n < 1 || n > 253 || b[0] != esp) mal++;
        if (i < nf - 1 && n != 253) mal++;
        if (n > 1) { memcpy(flujo + k, b + 1, (size_t)(n - 1)); k += n - 1; }
    }
    CHECA(!mal, "%s: indices y tamanos de fragmento (%d malos)", que, mal);
    CHECA(k == total, "%s: el flujo mide %d (esperado %d)", que, k, total);
    CHECA(!memcmp(flujo, p->ptr, (size_t)p->n - 4), "%s: parche sin cambios salvo los 4 ultimos bytes", que);
    uint32_t v = (uint32_t)flujo[p->n - 4] | (uint32_t)flujo[p->n - 3] << 8 | (uint32_t)flujo[p->n - 2] << 16 | (uint32_t)flujo[p->n - 1] << 24;
    CHECA(v == p->fw_version, "%s: los 4 ultimos bytes son fw_version", que);
    CHECA(!ncfg || !memcmp(flujo + p->n, cfg, (size_t)ncfg), "%s: la configuracion va detras", que);
    free(flujo);
}

static void t_rtl_reales(void)
{
    static const char *const chips[] = { "rtl8723b", "rtl8723d", "rtl8761bu", "rtl8821a", "rtl8821c" };
    for (int c = 0; c < 5; c++) {
        char ruta[96];
        snprintf(ruta, sizeof ruta, "firmware/realtek/rtl_bt/%s_fw.bin", chips[c]);
        int n;
        uint8_t *fw = leer(ruta, &n);
        int ncfg = 0;
        uint8_t *cfg = NULL;
        snprintf(ruta, sizeof ruta, "firmware/realtek/rtl_bt/%s_config.bin", chips[c]);
        char full[512];
        snprintf(full, sizeof full, "%s/%s", raiz, ruta);
        FILE *f = fopen(full, "rb");
        if (f) { fclose(f); cfg = leer(ruta, &ncfg); }
        bt_rtl_parche_t p;
        int ok = 0;
        for (int rom = 0; rom < 3; rom++) {
            int r = bt_rtl_parche(fw, n, (uint8_t)rom, &p);
            if (r == 0) {
                ok++;
                CHECA(p.n > 1000 && p.ptr >= fw && p.ptr + p.n <= fw + n, "%s ROM %d: el parche cae dentro del archivo", chips[c], rom);
                CHECA(p.project_id >= 0, "%s ROM %d: identificador de proyecto leido (%d)", chips[c], rom, p.project_id);
                CHECA(p.fw_version != 0, "%s: fw_version", chips[c]);
                char q[64];
                snprintf(q, sizeof q, "%s ROM %d", chips[c], rom);
                comprobar_flujo(q, &p, cfg, ncfg);
            }
        }
        CHECA(ok >= 2, "%s: hay parche para las ROM 0 y 1 (%d)", chips[c], ok);
        CHECA(bt_rtl_parche(fw, n, 200, &p) == BT_RTL_E_SIN_PARCHE, "%s: ROM 200 sin parche", chips[c]);
        CHECA(bt_rtl_parche(fw, n - 1, 0, &p) == BT_RTL_E_EPILOGO, "%s: sin la firma final", chips[c]);
        CHECA(bt_rtl_parche(fw, 10, 0, &p) == BT_RTL_E_FIRMA, "%s: demasiado corto", chips[c]);
        uint8_t mal[64];
        memcpy(mal, fw, 64);
        mal[0] = 'X';
        CHECA(bt_rtl_parche(mal, 64, 0, &p) == BT_RTL_E_FIRMA, "%s: firma mala", chips[c]);
        memcpy(mal, "RTBTCore", 8);
        CHECA(bt_rtl_parche(mal, 64, 0, &p) == BT_RTL_E_V2, "%s: formato V2 detectado", chips[c]);
        free(fw);
        free(cfg);
    }
}

/* Archivo «Realtech» hecho a mano con un parche de longitud elegida (dos parches: ROM 0 y 1). */
static int rtl_hecho(uint8_t *o, int largo, int offset_malo)
{
    int k = 0;
    memcpy(o, "Realtech", 8); k = 8;
    o[k++] = 0x78; o[k++] = 0x56; o[k++] = 0x34; o[k++] = 0x12;   /* fw_version 0x12345678 */
    o[k++] = 2; o[k++] = 0;
    o[k++] = 1; o[k++] = 0; o[k++] = 2; o[k++] = 0;               /* chip ids */
    o[k++] = (uint8_t)largo; o[k++] = (uint8_t)(largo >> 8); o[k++] = (uint8_t)largo; o[k++] = (uint8_t)(largo >> 8);
    int off0 = k + 8;
    int offs[2] = { offset_malo ? 0x7FFFFFF : off0, off0 };
    for (int i = 0; i < 2; i++) { o[k++] = (uint8_t)offs[i]; o[k++] = (uint8_t)(offs[i] >> 8); o[k++] = (uint8_t)(offs[i] >> 16); o[k++] = (uint8_t)(offs[i] >> 24); }
    for (int i = 0; i < largo; i++) o[k++] = (uint8_t)(i * 7 + 1);
    o[k++] = 0x0E; o[k++] = 0x01; o[k++] = 0x00;   /* extensión (se lee de atrás adelante): dato 0x0E, longitud 1, opcode 0 */
    o[k++] = 0x51; o[k++] = 0x04; o[k++] = 0xFD; o[k++] = 0x77;
    return k;
}

static void t_rtl_a_mano(void)
{
    uint8_t f[4096];
    bt_rtl_parche_t p;
    int n = rtl_hecho(f, 500, 0);
    CHECA(bt_rtl_parche(f, n, 0, &p) == 0 && p.n == 500 && p.fw_version == 0x12345678 && p.project_id == 0x0E,
          "archivo a mano: ROM 0 (proyecto %d)", p.project_id);
    CHECA(bt_rtl_parche(f, n, 1, &p) == 0 && p.n == 500, "ROM 1");
    CHECA(bt_rtl_parche(f, n, 2, &p) == BT_RTL_E_SIN_PARCHE, "ROM 2 sin parche");
    n = rtl_hecho(f, 500, 1);
    CHECA(bt_rtl_parche(f, n, 0, &p) == BT_RTL_E_CORRUPTO, "desplazamiento fuera del archivo");
    CHECA(bt_rtl_parche(f, n, 1, &p) == 0, "el otro parche sigue valiendo");
    /* total múltiplo de 252: el último fragmento va vacío con el bit de fin */
    uint8_t cfg[100];
    memset(cfg, 0xCC, sizeof cfg);
    n = rtl_hecho(f, 404, 0);   /* 404 + 100 = 504 = 2 x 252 */
    CHECA(bt_rtl_parche(f, n, 0, &p) == 0, "parche de 404");
    CHECA(bt_rtl_n_frag(504) == 3, "504 bytes: tres fragmentos (el ultimo vacio)");
    uint8_t b[256];
    CHECA(bt_rtl_fragmento(&p, cfg, 100, 2, b) == 1 && b[0] == 0x82, "fragmento final vacio con indice 0x82");
    CHECA(bt_rtl_fragmento(&p, cfg, 100, 3, b) < 0, "fragmento inexistente");
    comprobar_flujo("a mano 404+100", &p, cfg, 100);
    n = rtl_hecho(f, 600, 0);
    CHECA(bt_rtl_parche(f, n, 0, &p) == 0, "parche de 600");
    comprobar_flujo("a mano 600 sin config", &p, NULL, 0);
    /* más de 127 fragmentos: el índice vuelve a empezar en 1 */
    static uint8_t grande[40000];
    n = rtl_hecho(grande, 36000, 0);
    CHECA(bt_rtl_parche(grande, n, 0, &p) == 0, "parche de 36000");
    comprobar_flujo("a mano 36000 (143 fragmentos)", &p, NULL, 0);
    uint8_t q[256];
    CHECA(bt_rtl_fragmento(&p, NULL, 0, 127, q) > 0 && q[0] == 0x7F, "fragmento 127 -> 0x7f");
    CHECA(bt_rtl_fragmento(&p, NULL, 0, 128, q) > 0 && q[0] == 0x01, "fragmento 128 -> 0x01");
    CHECA(bt_rtl_fragmento(&p, NULL, 0, 129, q) > 0 && q[0] == 0x02, "fragmento 129 -> 0x02");
}

/* ------------------------------------------------------------------ Atheros: nombres y ids */

static void t_ath_nombres(void)
{
    char n[64];
    CHECA(bt_ath_nombre_parche(0x11020000, n, sizeof n) == 0 && !strcmp(n, "ar3k/AthrBT_0x11020000.dfu"), "parche: %s", n);
    CHECA(bt_ath_nombre_cfg(0x11020000, 1, n, sizeof n) == 0 && !strcmp(n, "ar3k/ramps_0x11020000_40.dfu"), "cfg 40 MHz: %s", n);
    CHECA(bt_ath_nombre_cfg(0x01020001, 0, n, sizeof n) == 0 && !strcmp(n, "ar3k/ramps_0x01020001_26.dfu"), "cfg 26 MHz: %s", n);
    CHECA(bt_ath_nombre_parche(0x11020000, n, 10) < 0, "nombre en un buffer corto");
    CHECA(bt_ath_tipo(0x0CF3, 0x3000) == BT_ATH_3011 && bt_ath_tipo(0x0CF3, 0x3004) == BT_ATH_3012 && bt_ath_tipo(0x0A12, 1) == BT_ATH_NINGUNO,
          "tipos por id USB");
    /* los archivos de linux-firmware que existen cubren las ROM que pedimos */
    static const uint32_t roms[] = { 0x01020001, 0x01020200, 0x01020201, 0x11020000, 0x11020100, 0x31010000, 0x31010100, 0x41020000 };
    for (unsigned i = 0; i < sizeof roms / sizeof roms[0]; i++) {
        char rel[96];
        bt_ath_nombre_parche(roms[i], n, sizeof n);
        snprintf(rel, sizeof rel, "firmware/atheros/%s", n);
        int tam;
        uint8_t *d = leer(rel, &tam);
        CHECA(tam > 1000, "%s existe", n);
        free(d);
    }
}

/* ------------------------------------------------------------------ máquina de estados contra el simulador */

static bt_info_t info(void) { bt_info_t i; bt_g_info(&i); return i; }

static void arrancar_fw(int fab, int lmp, int rev, int ver)
{
    sim_reiniciar();
    sim_cfg.fab = fab; sim_cfg.lmp_sub = lmp; sim_cfg.hci_rev = rev; sim_cfg.hci_ver_fw = ver;
    sim_plat.usb_vid = 0; sim_plat.usb_pid = 0;
}

static void iniciar(void)
{
    bt_g_iniciar();
    bt_hci_iniciar(&sim_plat);
    sim_bombear();
}

static void t_realtek_sim(void)
{
    int n, ncfg;
    uint8_t *fw = leer("firmware/realtek/rtl_bt/rtl8761bu_fw.bin", &n);
    uint8_t *cfg = leer("firmware/realtek/rtl_bt/rtl8761bu_config.bin", &ncfg);
    arrancar_fw(93, 0x8761, 0x000B, 0x0A);
    sim_cfg.rtl_rom = 1;
    sim_arqs[0] = (sim_arq_t){ "rtl_bt/rtl8761bu_fw.bin", fw, n };
    sim_arqs[1] = (sim_arq_t){ "rtl_bt/rtl8761bu_config.bin", cfg, ncfg };
    sim_n_arqs = 2;
    iniciar();
    bt_rtl_parche_t p;
    CHECA(bt_rtl_parche(fw, n, 1, &p) == 0, "parche para la ROM 1");
    CHECA(info().estado == BT_PRONTO, "RTL8761BU: el inicio acaba (%u) %s", info().estado, sim_log);
    CHECA(sim_fw_ops[0] == OP_FW_RTL_ROM, "primero se lee la ROM");
    CHECA(sim_fw_nops == 1 + bt_rtl_n_frag(p.n + ncfg), "una orden por fragmento (%d)", sim_fw_nops);
    CHECA(sim_rtl_n == p.n + ncfg && sim_rtl_indices_mal == 0, "el controlador recibio todo el flujo (%d) con los indices bien", sim_rtl_n);
    CHECA(!memcmp(sim_rtl_flujo, p.ptr, (size_t)p.n - 4) && !memcmp(sim_rtl_flujo + p.n, cfg, (size_t)ncfg), "y es el parche mas la configuracion");
    CHECA(sim_resets == 2, "Reset inicial y Reset tras el parche (%d)", sim_resets);
    CHECA(info().lmp_sub == 0x1234 && info().fabricante == 93, "se releyo la version (lmp %04x)", info().lmp_sub);
    CHECA(sim_arq_leidos == 2 && sim_arq_liberados == 2, "los archivos se leen y se liberan (%d/%d)", sim_arq_leidos, sim_arq_liberados);
    CHECA(strstr(sim_log, "Realtek") != NULL, "el registro lo cuenta");
    CHECA(info().addr[0] == 0x01 && info().acl_mtu == 310, "el resto del inicio sigue igual");

    /* ROM 2 en un archivo que no la trae: se sigue sin parche */
    arrancar_fw(93, 0x8761, 0x000B, 0x0A);
    sim_cfg.rtl_rom = 9;
    sim_arqs[0] = (sim_arq_t){ "rtl_bt/rtl8761bu_fw.bin", fw, n }; sim_n_arqs = 1;
    iniciar();
    CHECA(info().estado == BT_PRONTO && sim_rtl_n == 0 && sim_resets == 1, "ROM sin parche: listo sin cargar nada");
    CHECA(sim_arq_leidos == sim_arq_liberados, "y no se pierde ninguna lectura");

    /* sin archivo */
    arrancar_fw(93, 0x8761, 0x000B, 0x0A);
    iniciar();
    CHECA(info().estado == BT_PRONTO && sim_fw_nops == 1 && strstr(sim_log, "falta /lib/firmware/rtl_bt/rtl8761bu_fw.bin"), "sin firmware: listo y el registro lo dice");

    /* un fragmento rechazado */
    arrancar_fw(93, 0x8761, 0x000B, 0x0A);
    sim_cfg.rtl_rom = 1; sim_cfg.rtl_falla_frag = 5;
    sim_arqs[0] = (sim_arq_t){ "rtl_bt/rtl8761bu_fw.bin", fw, n }; sim_n_arqs = 1;
    iniciar();
    CHECA(info().estado == BT_PRONTO && sim_fw_nops == 1 + 6 && strstr(sim_log, "fragmento 5 rechazado"), "fragmento rechazado: se aborta y se sigue (%d ordenes)", sim_fw_nops);
    CHECA(sim_arq_leidos == sim_arq_liberados, "liberados tras abortar");

    /* el controlador deja de contestar a mitad del parche: 5 s y se sigue */
    arrancar_fw(93, 0x8761, 0x000B, 0x0A);
    sim_cfg.rtl_rom = 1; sim_cfg.mudo_fw = true;
    sim_arqs[0] = (sim_arq_t){ "rtl_bt/rtl8761bu_fw.bin", fw, n }; sim_n_arqs = 1;
    iniciar();
    CHECA(info().estado == BT_INICIANDO, "esperando al fragmento que no contesta");
    sim_cfg.mudo_fw = false;
    sim_avanzar(5100);
    CHECA(info().estado == BT_PRONTO && strstr(sim_log, "dejo de contestar"), "tras 5 s se sigue sin parche (%u) %s", info().estado, sim_log);
    CHECA(sim_arq_leidos == sim_arq_liberados, "liberados tras el tiempo agotado");

    /* ROM que rechaza la orden de versión: se sigue con la ROM */
    arrancar_fw(93, 0x8761, 0x000B, 0x0A);
    sim_cfg.rechaza_op = OP_FW_RTL_ROM;
    sim_arqs[0] = (sim_arq_t){ "rtl_bt/rtl8761bu_fw.bin", fw, n }; sim_n_arqs = 1;
    iniciar();
    CHECA(info().estado == BT_PRONTO && sim_rtl_n == 0, "0xFC6D rechazada: listo sin parche");

    /* Realtek ya parcheado (otra lmp_subversion) o desconocido: ni se intenta */
    arrancar_fw(93, 0x9999, 0x000B, 0x0A);
    iniciar();
    CHECA(info().estado == BT_PRONTO && sim_fw_nops == 0 && strstr(sim_log, "sin tabla"), "Realtek desconocido: sin ordenes de fabricante");
    free(fw);
    free(cfg);
}

static void t_broadcom_sim(void)
{
    uint8_t h[64];
    int n = hcd_hecho(h);
    arrancar_fw(15, 0x220E, 0x0001, 6);
    strcpy(sim_cfg.nome_local, "BCM20702A1 #AB");
    sim_plat.usb_vid = 0x0A5C; sim_plat.usb_pid = 0x21E8;
    sim_arqs[0] = (sim_arq_t){ "brcm/BCM20702A1-0a5c-21e8.hcd", h, n }; sim_n_arqs = 1;
    iniciar();
    sim_avanzar(700);
    CHECA(info().estado == BT_PRONTO, "Broadcom: listo (%u) %s", info().estado, sim_log);
    CHECA(sim_fw_nops == 4 && sim_fw_ops[0] == 0xFC2E && sim_fw_ops[1] == 0xFC4C && sim_fw_ops[2] == 0xFC4C && sim_fw_ops[3] == 0xFC4E,
          "Download_Minidriver, dos Write_RAM y Launch_RAM (%d ordenes)", sim_fw_nops);
    CHECA(sim_fw_ms[1] - sim_fw_ms[0] >= 50, "50 ms entre el minicontrolador y el primer registro (%u)", sim_fw_ms[1] - sim_fw_ms[0]);
    CHECA(sim_ms_reset_tras_launch - sim_ms_launch >= 250, "250 ms entre Launch_RAM y el Reset (%u)", sim_ms_reset_tras_launch - sim_ms_launch);
    CHECA(sim_resets == 2 && info().lmp_sub == 0x1234, "Reset tras el parche y version releida");
    CHECA(sim_arq_leidos == 1 && sim_arq_liberados == 1, "archivo leido y liberado");

    /* nombre alternativo sin identificador USB */
    arrancar_fw(15, 0x220E, 0x0001, 6);
    strcpy(sim_cfg.nome_local, "BCM20702A1");
    sim_plat.usb_vid = 0x0A5C; sim_plat.usb_pid = 0x21E8;
    sim_arqs[0] = (sim_arq_t){ "brcm/BCM20702A1.hcd", h, n }; sim_n_arqs = 1;
    iniciar();
    sim_avanzar(700);
    CHECA(info().estado == BT_PRONTO && sim_fw_nops == 4, "con brcm/BCM20702A1.hcd tambien carga (%d)", sim_fw_nops);

    /* sin parche: funciona lo que funcione en ROM */
    arrancar_fw(15, 0x220E, 0x0001, 6);
    strcpy(sim_cfg.nome_local, "BCM20702A1");
    iniciar();
    sim_avanzar(700);
    CHECA(info().estado == BT_PRONTO && sim_fw_nops == 0,
          "sin .hcd: listo sin ordenes de fabricante (%d)", sim_fw_nops);
    CHECA(strstr(sim_log, "sin parche") != NULL, "el registro dice que no hay parche");

    /* registro rechazado: se aborta, se resetea y se sigue */
    arrancar_fw(15, 0x220E, 0x0001, 6);
    strcpy(sim_cfg.nome_local, "BCM20702A1");
    sim_plat.usb_vid = 0x0A5C; sim_plat.usb_pid = 0x21E8;
    sim_cfg.rechaza_op = 0xFC4C;
    sim_arqs[0] = (sim_arq_t){ "brcm/BCM20702A1-0a5c-21e8.hcd", h, n }; sim_n_arqs = 1;
    iniciar();
    sim_avanzar(700);
    CHECA(info().estado == BT_PRONTO && sim_fw_nops == 2 && strstr(sim_log, "se aborta el parche") && sim_resets == 2,
          "Write_RAM rechazada: aborta, Reset y listo (%d ordenes, %d resets)", sim_fw_nops, sim_resets);
    CHECA(info().lmp_sub == 0x220E, "sin parche no se relee la version");

    /* .hcd corrupto o que no es un parche */
    uint8_t mal[8] = { 0x4C, 0xFC, 0x20, 1, 2 };
    arrancar_fw(15, 0x220E, 0x0001, 6);
    strcpy(sim_cfg.nome_local, "BCM20702A1");
    sim_plat.usb_vid = 0x0A5C; sim_plat.usb_pid = 0x21E8;
    sim_arqs[0] = (sim_arq_t){ "brcm/BCM20702A1-0a5c-21e8.hcd", mal, 5 }; sim_n_arqs = 1;
    iniciar();
    sim_avanzar(700);
    CHECA(info().estado == BT_PRONTO && sim_fw_nops == 0 && strstr(sim_log, "no es un parche .hcd valido"), "hcd corrupto: se descarta antes de mandar nada");
    CHECA(sim_arq_leidos == sim_arq_liberados, "liberado");

    /* Download_Minidriver rechazada (ya estaba en RAM): se prueba igual */
    arrancar_fw(15, 0x220E, 0x0001, 6);
    strcpy(sim_cfg.nome_local, "BCM20702A1");
    sim_plat.usb_vid = 0x0A5C; sim_plat.usb_pid = 0x21E8;
    sim_cfg.rechaza_op = 0xFC2E;
    sim_arqs[0] = (sim_arq_t){ "brcm/BCM20702A1-0a5c-21e8.hcd", h, n }; sim_n_arqs = 1;
    iniciar();
    sim_avanzar(700);
    CHECA(info().estado == BT_PRONTO && sim_fw_nops == 4, "minicontrolador rechazado: se manda el parche igual (%d)", sim_fw_nops);
    sim_plat.usb_vid = 0; sim_plat.usb_pid = 0;
}

static void t_csr_igual(void)
{
    sim_reiniciar();
    sim_plat.usb_vid = 0x0A12; sim_plat.usb_pid = 0x0001;
    iniciar();
    bt_info_t i = info();
    CHECA(i.estado == BT_PRONTO && sim_fw_nops == 0 && sim_resets == 1 && sim_arq_leidos == 0, "CSR (fabricante 10): ni una orden de firmware");
    CHECA(i.fabricante == 10 && i.lmp_sub == 0x1234, "version como siempre");
    sim_reiniciar();
    sim_cfg.clon_csr = true;
    iniciar();
    CHECA(info().estado == BT_PRONTO && sim_fw_nops == 0, "clon CSR BT 2.0 igual");
    sim_plat.usb_vid = 0; sim_plat.usb_pid = 0;
}

static void t_atheros_sim(void)
{
    int n1, n2, n3;
    uint8_t *p = leer("firmware/atheros/ar3k/AthrBT_0x11020000.dfu", &n1);
    uint8_t *c = leer("firmware/atheros/ar3k/ramps_0x11020000_40.dfu", &n2);
    uint8_t *f = leer("firmware/atheros/ath3k-1.fw", &n3);
    /* AR3012 en estado de fábrica */
    sim_reiniciar();
    sim_plat.usb_vid = 0x0CF3; sim_plat.usb_pid = 0x3004;
    sim_ath_estado = 0;
    sim_ath_version[0] = 0x00; sim_ath_version[1] = 0x00; sim_ath_version[2] = 0x02; sim_ath_version[3] = 0x11;   /* ROM 0x11020000 */
    sim_ath_version[12] = 1;   /* 40 MHz */
    sim_arqs[0] = (sim_arq_t){ "ar3k/AthrBT_0x11020000.dfu", p, n1 };
    sim_arqs[1] = (sim_arq_t){ "ar3k/ramps_0x11020000_40.dfu", c, n2 };
    sim_n_arqs = 2;
    CHECA(bt_ath_cargar(&sim_plat) == 1, "AR3012: cargado, el dispositivo se reenumera");
    int dn = 0, normal = 0, cambia = 0;
    for (int i = 0; i < sim_n_usbv; i++) {
        if (sim_usbv[i].pedido == 0x01 && sim_usbv[i].n == 20 && !(sim_usbv[i].tipo & 0x80)) dn++;
        if (sim_usbv[i].pedido == 0x07) normal++;
        if (sim_usbv[i].pedido == 0x0A) cambia++;
    }
    CHECA(dn == 2 && normal == 1 && cambia == 1, "dos cabeceras de 20 bytes, modo normal y cambio de PID (%d %d %d)", dn, normal, cambia);
    CHECA(sim_bulk_bytes == (n1 - 20) + (n2 - 20), "el resto de cada archivo por bulk (%d)", sim_bulk_bytes);
    CHECA(sim_arq_leidos == 2 && sim_arq_liberados == 2, "archivos liberados");

    /* ya parcheado: no se toca nada */
    sim_reiniciar();
    sim_plat.usb_vid = 0x0CF3; sim_plat.usb_pid = 0x3004;
    sim_ath_estado = 0xC0;
    CHECA(bt_ath_cargar(&sim_plat) == 0 && sim_bulk_bytes == 0, "AR3012 ya con parche y configuracion: nada que hacer");

    /* falta el parche */
    sim_reiniciar();
    sim_plat.usb_vid = 0x0CF3; sim_plat.usb_pid = 0x3004;
    CHECA(bt_ath_cargar(&sim_plat) < 0 && strstr(sim_log, "falta /lib/firmware/ar3k/AthrBT_0x00000000.dfu"), "AR3012 sin archivo: error y se sigue (%s)", sim_log);

    /* AR3011 */
    sim_reiniciar();
    sim_plat.usb_vid = 0x0CF3; sim_plat.usb_pid = 0x3000;
    sim_ath_estado = 0;
    sim_arqs[0] = (sim_arq_t){ "ath3k-1.fw", f, n3 }; sim_n_arqs = 1;
    CHECA(bt_ath_cargar(&sim_plat) == 1 && sim_bulk_bytes == n3 - 20, "AR3011: firmware completo (%d)", sim_bulk_bytes);
    sim_reiniciar();
    sim_plat.usb_vid = 0x0CF3; sim_plat.usb_pid = 0x3000;
    sim_ath_estado = 0x01;
    CHECA(bt_ath_cargar(&sim_plat) == 0 && sim_bulk_bytes == 0, "AR3011 ya en modo normal");

    /* no es Atheros */
    sim_reiniciar();
    sim_plat.usb_vid = 0x0A12; sim_plat.usb_pid = 0x0001;
    CHECA(bt_ath_cargar(&sim_plat) == 0 && sim_n_usbv == 0, "un CSR no se toca");
    sim_plat.usb_vid = 0; sim_plat.usb_pid = 0;
    free(p); free(c); free(f);
}

/* ------------------------------------------------------------------ BLE */

static void le_poner(int i, uint8_t a0, uint8_t tipo, int8_t rssi, const uint8_t *ad, int n)
{
    memset(&sim_le[i], 0, sizeof sim_le[i]);
    for (int k = 0; k < 6; k++) sim_le[i].addr[k] = (uint8_t)(a0 + k);
    sim_le[i].tipo = tipo;
    sim_le[i].rssi = rssi;
    memcpy(sim_le[i].ad, ad, (size_t)n);
    sim_le[i].ad_n = n;
}

static const bt_disp_t *buscar_disp(const uint8_t *addr)
{
    for (int i = 0; i < bt_n_disp(); i++) if (!memcmp(bt_disp(i)->addr, addr, 6)) return bt_disp(i);
    return NULL;
}

static void t_ble(void)
{
    /* un controlador sin LE (como el CSR): la búsqueda es solo la clásica, igual que siempre */
    sim_reiniciar();
    iniciar();
    CHECA(info().estado == BT_PRONTO && !bt_hci_le() && !(sim_evmask >> 61 & 1), "sin LE: ni mascara de eventos LE");
    CHECA(bt_buscar() == 0, "buscar");
    sim_avanzar(300);
    CHECA(!info().buscando && sim_le_param == 0 && sim_le_activaciones == 0, "sin LE: la busqueda acaba con la clasica");

    /* con LE */
    sim_reiniciar();
    sim_cfg.le = true;
    uint8_t ad_teclado[] = { 2, 0x01, 0x06, 9, 0x09, 'K', 'e', 'y', 'b', 'o', 'a', 'r', 'd', 3, 0x19, 0xC1, 0x03 };
    uint8_t ad_baliza[] = { 3, 0x03, 0x0F, 0x18 };                                 /* sin nombre ni HID: se ignora */
    uint8_t ad_reloj[] = { 6, 0x08, 'R', 'e', 'l', 'o', 'j', 3, 0x02, 0x12, 0x18 };   /* nombre corto + HID por servicio */
    uint8_t ad_sensor[] = { 7, 0x09, 'S', 'e', 'n', 's', 'o', 'r' };
    le_poner(0, 0x10, 0, -50, ad_teclado, sizeof ad_teclado);
    le_poner(1, 0x20, 1, -70, ad_baliza, sizeof ad_baliza);
    le_poner(2, 0x30, 1, -61, ad_reloj, sizeof ad_reloj);
    le_poner(3, 0x40, 0, -80, ad_sensor, sizeof ad_sensor);
    sim_n_le = 4;
    iniciar();
    CHECA(info().estado == BT_PRONTO && bt_hci_le(), "con LE: el adaptador lo anuncia");
    CHECA((sim_evmask >> 61 & 1) && (sim_evmask & 0x7FFFFFFFFFFFull) == 0x7FFFFFFFFFFFull, "la mascara de eventos incluye LE Meta (%016llx)", (unsigned long long)sim_evmask);
    CHECA(bt_buscar() == 0, "buscar");
    sim_avanzar(300);
    CHECA(info().buscando && sim_le_param == 1 && sim_le_activo == 1, "tras la clasica empieza la LE (parametros y escucha)");
    CHECA(sim_le_param_datos[0] == 1 && sim_le_param_datos[1] == 0x60 && sim_le_param_datos[3] == 0x30, "busqueda activa, 60 ms / 30 ms");
    sim_avanzar(5000);
    CHECA(!info().buscando && sim_le_activo == 0, "a los 4 s se detiene la escucha y acaba la busqueda");
    uint8_t a1[6], a2[6], a3[6], a4[6];
    for (int k = 0; k < 6; k++) { a1[k] = (uint8_t)(0x10 + k); a2[k] = (uint8_t)(0x20 + k); a3[k] = (uint8_t)(0x30 + k); a4[k] = (uint8_t)(0x40 + k); }
    const bt_disp_t *d1 = buscar_disp(a1), *d2 = buscar_disp(a2), *d3 = buscar_disp(a3), *d4 = buscar_disp(a4);
    CHECA(d1 && d1->le && d1->tipo == BT_T_BLE_HID && !strcmp(d1->nome, "Keyboard") && d1->rssi == -50, "teclado BLE por apariencia, con nombre y senal");
    CHECA(!d2, "la baliza anonima no se guarda");
    CHECA(d3 && d3->tipo == BT_T_BLE_HID && !strcmp(d3->nome, "Reloj"), "HID por servicio 0x1812; nombre corto (tipo 0x08)");
    CHECA(d4 && d4->tipo == BT_T_BLE && d4->addr_tipo_le == 0, "sensor BLE normal");
    /* conectar a uno BLE: error claro, sin tocar el controlador */
    int idx = bt_g_indice(d4);
    int cmds = sim_cmds;
    CHECA(bt_conectar(idx, false) == -5 && d4->estado == BT_D_ERRO && strstr(d4->erro, "BLE"), "conectar BLE: aun no se puede (%s)", d4->erro);
    CHECA(sim_cmds == cmds, "y no se mando ninguna orden");

    /* LE rechazada: se queda la clasica y no se atasca */
    sim_reiniciar();
    sim_cfg.le = true;
    sim_cfg.falla_op = OP_LE_PARAM_SCAN; sim_cfg.falla_estado = 0x0C;
    sim_le[0] = (sim_le_t){ .tipo = 0, .ad_n = 0 }; sim_n_le = 0;
    iniciar();
    bt_buscar();
    sim_avanzar(600);
    CHECA(!info().buscando && strstr(sim_log, "busqueda LE rechazada"), "LE rechazada: acaba la busqueda (%s)", sim_log);

    /* cancelar con un conectar mientras escucha LE */
    sim_reiniciar();
    sim_cfg.le = true;
    iniciar();
    bt_buscar();
    sim_avanzar(300);
    CHECA(sim_le_activo == 1, "escuchando");
    bt_disp_t *x = bt_g_por_addr(a1, true);
    x->tem_chave = true;   /* uno conocido, para que conectar siga el camino normal */
    bt_conectar(0, false);
    sim_avanzar(100);
    CHECA(sim_le_activo == 0 && !info().buscando, "conectar mientras se busca detiene la LE");
}

int main(int argc, char **argv)
{
    if (argc > 1) raiz = argv[1];
    t_hcd();
    t_rtl_tabla();
    t_rtl_reales();
    t_rtl_a_mano();
    t_ath_nombres();
    t_csr_igual();
    t_realtek_sim();
    t_broadcom_sim();
    t_atheros_sim();
    t_ble();
    printf("%d comprobaciones, %d fallos\n", pruebas, fallos);
    if (!fallos) printf("todo bien\n");
    return fallos ? 1 : 0;
}
