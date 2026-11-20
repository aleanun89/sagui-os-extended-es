// SPDX-License-Identifier: GPL-2.0-or-later
/* bt_fw.c - carga de firmware de los controladores Bluetooth Broadcom, Realtek y Atheros (D411).
 * EXPERIMENTAL, sin probar en hardware real. Ver bt_fw.h. */
#include "bt_fw.h"

/* ------------------------------------------------------------------ Broadcom (.hcd) */

/* Registro: opcode (2, LE), longitud (1), datos. El archivo acaba justo en el último registro. */
int bt_hcd_siguiente(const uint8_t *d, int n, int *pos, bt_hcd_reg_t *r)
{
    int k = *pos;
    if (k == n) return 0;
    if (k < 0 || n - k < 3) return -1;
    int len = d[k + 2];
    if (n - k - 3 < len) return -1;
    r->op = bt_le16(d + k);
    r->par = d + k + 3;
    r->n = len;
    *pos = k + 3 + len;
    return 1;
}

int bt_hcd_validar(const uint8_t *d, int n)
{
    int pos = 0, cuenta = 0, r;
    bt_hcd_reg_t g;
    if (!d || n < 3) return -1;
    while ((r = bt_hcd_siguiente(d, n, &pos, &g)) > 0) {
        /* Solo órdenes de fabricante (OGF 0x3F): un .hcd con otra cosa no es un parche. */
        if ((g.op >> 10) != 0x3F) return -2;
        cuenta++;
    }
    return r < 0 ? -1 : cuenta;
}

int bt_bcm_nombre(const uint8_t *nome, int n, uint16_t vid, uint16_t pid, int variante, char *out, int max)
{
    static const char h[] = "0123456789abcdef";
    char chip[24];
    int k = 0;
    for (int i = 0; i < n && k < (int)sizeof chip - 1; i++) {
        uint8_t c = nome[i];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) chip[k++] = (char)c;
        else break;   /* NUL, espacio, «#», «_», «-»: el resto es versión o reloj */
    }
    chip[k] = 0;
    if (k < 4 || max < 40) return -1;
    int o = 0;
    const char *pre = "brcm/";
    for (; *pre; pre++) out[o++] = *pre;
    for (int i = 0; i < k; i++) out[o++] = chip[i];
    if (variante == 0) {
        out[o++] = '-';
        for (int s = 12; s >= 0; s -= 4) out[o++] = h[(vid >> s) & 15];
        out[o++] = '-';
        for (int s = 12; s >= 0; s -= 4) out[o++] = h[(pid >> s) & 15];
    }
    const char *ext = ".hcd";
    for (; *ext; ext++) out[o++] = *ext;
    out[o] = 0;
    return 0;
}

/* ------------------------------------------------------------------ Realtek */

/* Chips en estado de ROM (todavía sin parche), por lmp_subversion, revisión HCI y versión HCI. */
static const bt_rtl_chip_t rtl_chips[] = {
    { "RTL8723B",  "rtl_bt/rtl8723b_fw.bin",  "rtl_bt/rtl8723b_config.bin",  0x8723, 0x000B, 0x06 },
    { "RTL8723D",  "rtl_bt/rtl8723d_fw.bin",  "rtl_bt/rtl8723d_config.bin",  0x8723, 0x000D, 0x08 },
    { "RTL8821A",  "rtl_bt/rtl8821a_fw.bin",  "rtl_bt/rtl8821a_config.bin",  0x8821, 0x000A, 0x06 },
    { "RTL8821C",  "rtl_bt/rtl8821c_fw.bin",  "rtl_bt/rtl8821c_config.bin",  0x8821, 0x000C, 0x08 },
    { "RTL8761B",  "rtl_bt/rtl8761bu_fw.bin", "rtl_bt/rtl8761bu_config.bin", 0x8761, 0x000B, 0x0A },
};

const bt_rtl_chip_t *bt_rtl_buscar(uint16_t lmp_sub, uint16_t hci_rev, uint8_t hci_ver)
{
    for (unsigned i = 0; i < sizeof rtl_chips / sizeof rtl_chips[0]; i++)
        if (rtl_chips[i].lmp_sub == lmp_sub && rtl_chips[i].hci_rev == hci_rev && rtl_chips[i].hci_ver == hci_ver)
            return &rtl_chips[i];
    return NULL;
}

static const uint8_t rtl_firma[8] = { 'R', 'e', 'a', 'l', 't', 'e', 'c', 'h' };
static const uint8_t rtl_firma_v2[8] = { 'R', 'T', 'B', 'T', 'C', 'o', 'r', 'e' };
static const uint8_t rtl_epilogo[4] = { 0x51, 0x04, 0xFD, 0x77 };
#define RTL_CAB 14   /* firma(8) fw_version(4) num_parches(2) */
#define RTL_FRAG 252

/* Cabecera: «Realtech», fw_version (LE32), número de parches (LE16); después, num identificadores de chip
 * (LE16), num longitudes (LE16) y num desplazamientos (LE32). Se elige el parche cuyo identificador es
 * rom_version + 1. El archivo acaba en una sección de extensión (firma 51 04 fd 77 al final; registros
 * opcode, longitud, datos, leídos desde atrás: opcode 0 = identificador de proyecto). */
int bt_rtl_parche(const uint8_t *fw, int n, uint8_t rom_version, bt_rtl_parche_t *p)
{
    if (!fw || n < RTL_CAB) return BT_RTL_E_FIRMA;
    if (!memcmp(fw, rtl_firma_v2, 8)) return BT_RTL_E_V2;
    if (memcmp(fw, rtl_firma, 8)) return BT_RTL_E_FIRMA;
    if (n < RTL_CAB + 4 || memcmp(fw + n - 4, rtl_epilogo, 4)) return BT_RTL_E_EPILOGO;
    uint32_t ver = (uint32_t)fw[8] | (uint32_t)fw[9] << 8 | (uint32_t)fw[10] << 16 | (uint32_t)fw[11] << 24;
    int np = bt_le16(fw + 12);
    if (np < 1 || RTL_CAB + 8 * np > n) return BT_RTL_E_CORRUPTO;
    p->project_id = -1;
    /* sección de extensión, de atrás adelante */
    int fin = n - 4;
    while (fin > RTL_CAB + 8 * np + 2) {
        uint8_t op = fw[--fin];
        uint8_t len = fw[--fin];
        if (op == 0xFF) break;
        if (len == 0) break;
        if (fin < len) break;
        fin -= len;
        if (op == 0 && len == 1) { p->project_id = fw[fin]; break; }
    }
    for (int i = 0; i < np; i++) {
        if (bt_le16(fw + RTL_CAB + 2 * i) != (uint16_t)(rom_version + 1)) continue;
        int len = bt_le16(fw + RTL_CAB + 2 * np + 2 * i);
        const uint8_t *o = fw + RTL_CAB + 4 * np + 4 * i;
        uint32_t off = (uint32_t)o[0] | (uint32_t)o[1] << 8 | (uint32_t)o[2] << 16 | (uint32_t)o[3] << 24;
        if (len < 4 || off > (uint32_t)n || (uint32_t)len > (uint32_t)n - off) return BT_RTL_E_CORRUPTO;
        p->ptr = fw + off;
        p->n = len;
        p->fw_version = ver;
        return 0;
    }
    return BT_RTL_E_SIN_PARCHE;
}

int bt_rtl_n_frag(int total) { return total / RTL_FRAG + 1; }

/* El fragmento i: índice (0..0x7F; pasado el 0x7F vuelve a empezar en 1) con el bit 7 en el último,
 * y hasta 252 bytes del flujo. Si el total es múltiplo de 252 el último fragmento va vacío. */
int bt_rtl_fragmento(const bt_rtl_parche_t *p, const uint8_t *cfg, int ncfg, int i, uint8_t *out)
{
    int total = p->n + ncfg;
    int nf = bt_rtl_n_frag(total);
    if (i < 0 || i >= nf) return -1;
    uint8_t idx = (uint8_t)i;
    if (i > 0x7F) idx = (uint8_t)((i & 0x7F) + 1);
    int len = RTL_FRAG;
    if (i == nf - 1) { idx |= 0x80; len = total % RTL_FRAG; }
    out[0] = idx;
    int base = i * RTL_FRAG;
    for (int k = 0; k < len; k++) {
        int o = base + k;
        uint8_t v;
        if (o >= p->n) v = cfg[o - p->n];
        else if (o >= p->n - 4) v = (uint8_t)(p->fw_version >> (8 * (o - (p->n - 4))));
        else v = p->ptr[o];
        out[1 + k] = v;
    }
    return 1 + len;
}

/* ------------------------------------------------------------------ Atheros (ath3k) */

static const struct { uint16_t vid, pid; uint8_t tipo; } ath_ids[] = {
    { 0x0CF3, 0x3000, BT_ATH_3011 }, { 0x0CF3, 0x3002, BT_ATH_3011 }, { 0x0CF3, 0x7015, BT_ATH_3011 },
    { 0x13D3, 0x3304, BT_ATH_3011 },
    { 0x0CF3, 0x3004, BT_ATH_3012 }, { 0x0CF3, 0x311D, BT_ATH_3012 }, { 0x0CF3, 0x311E, BT_ATH_3012 },
    { 0x0CF3, 0x311F, BT_ATH_3012 }, { 0x0CF3, 0x3121, BT_ATH_3012 }, { 0x0CF3, 0x817A, BT_ATH_3012 },
    { 0x0CF3, 0xE003, BT_ATH_3012 }, { 0x0CF3, 0xE004, BT_ATH_3012 }, { 0x0CF3, 0xE005, BT_ATH_3012 },
    { 0x0CF3, 0xE006, BT_ATH_3012 },
    { 0x04CA, 0x3005, BT_ATH_3012 }, { 0x04CA, 0x3006, BT_ATH_3012 }, { 0x04CA, 0x3007, BT_ATH_3012 },
    { 0x04CA, 0x3008, BT_ATH_3012 }, { 0x04CA, 0x300B, BT_ATH_3012 }, { 0x04CA, 0x300D, BT_ATH_3012 },
    { 0x04CA, 0x300F, BT_ATH_3012 }, { 0x04CA, 0x3010, BT_ATH_3012 },
    { 0x13D3, 0x3362, BT_ATH_3012 }, { 0x13D3, 0x3375, BT_ATH_3012 }, { 0x13D3, 0x3393, BT_ATH_3012 },
    { 0x13D3, 0x3402, BT_ATH_3012 }, { 0x13D3, 0x3408, BT_ATH_3012 }, { 0x13D3, 0x3423, BT_ATH_3012 },
    { 0x13D3, 0x3432, BT_ATH_3012 }, { 0x13D3, 0x3472, BT_ATH_3012 }, { 0x13D3, 0x3474, BT_ATH_3012 },
    { 0x0489, 0xE04D, BT_ATH_3012 }, { 0x0489, 0xE04E, BT_ATH_3012 }, { 0x0489, 0xE056, BT_ATH_3012 },
    { 0x0489, 0xE057, BT_ATH_3012 }, { 0x0930, 0x0219, BT_ATH_3012 }, { 0x0930, 0x021C, BT_ATH_3012 },
    { 0x0930, 0x0220, BT_ATH_3012 },
};

int bt_ath_tipo(uint16_t vid, uint16_t pid)
{
    for (unsigned i = 0; i < sizeof ath_ids / sizeof ath_ids[0]; i++)
        if (ath_ids[i].vid == vid && ath_ids[i].pid == pid) return ath_ids[i].tipo;
    return BT_ATH_NINGUNO;
}

static const char hexa[] = "0123456789abcdef";

static int poner_txt(char *out, int max, int *o, const char *t)
{
    for (; *t; t++) { if (*o + 1 >= max) return -1; out[(*o)++] = *t; }
    out[*o] = 0;
    return 0;
}

static int poner_hex32(char *out, int max, int *o, uint32_t v)
{
    if (*o + 8 + 1 >= max) return -1;
    for (int s = 28; s >= 0; s -= 4) out[(*o)++] = hexa[(v >> s) & 15];
    out[*o] = 0;
    return 0;
}

int bt_ath_nombre_parche(uint32_t rom, char *out, int max)
{
    int o = 0;
    out[0] = 0;
    if (poner_txt(out, max, &o, "ar3k/AthrBT_0x") || poner_hex32(out, max, &o, rom) || poner_txt(out, max, &o, ".dfu")) return -1;
    return 0;
}

/* ref_clock: 0 = 26 MHz, 1 = 40 MHz, 2 = 19,2 MHz (como en la cabecera de versión del ath3k). */
int bt_ath_nombre_cfg(uint32_t rom, uint8_t ref_clock, char *out, int max)
{
    int o = 0;
    out[0] = 0;
    const char *mhz = ref_clock == 1 ? "_40" : ref_clock == 2 ? "_19" : ref_clock == 0 ? "_26" : "_0";
    if (poner_txt(out, max, &o, "ar3k/ramps_0x") || poner_hex32(out, max, &o, rom) || poner_txt(out, max, &o, mhz) ||
        poner_txt(out, max, &o, ".dfu")) return -1;
    return 0;
}

#define ATH_DNLOAD        0x01
#define ATH_GETSTATE      0x05
#define ATH_SET_NORMAL    0x07
#define ATH_GETVERSION    0x09
#define ATH_SWITCH_VID_PID 0x0A
#define ATH_VENDOR_OUT    0x40
#define ATH_VENDOR_IN     0xC0
#define ATH_MODO_MASCARA  0x3F
#define ATH_MODO_NORMAL   0x01
#define ATH_PARCHE_HECHO  0x80
#define ATH_CFG_HECHO     0x40
#define ATH_CAB           20
#define ATH_BLOQUE        1024

/* Un archivo .dfu: 20 bytes por control (DNLOAD) y el resto por bulk. */
static int ath_enviar(const bt_plat_t *p, const uint8_t *d, int n)
{
    if (n <= ATH_CAB) return -1;
    uint8_t cab[ATH_CAB];
    memcpy(cab, d, ATH_CAB);
    if (p->usb_vendor(ATH_VENDOR_OUT, ATH_DNLOAD, 0, 0, cab, ATH_CAB) < 0) return -2;
    for (int o = ATH_CAB; o < n;) {
        int k = n - o < ATH_BLOQUE ? n - o : ATH_BLOQUE;
        if (p->usb_bulk_salida(d + o, k) < 0) return -3;
        o += k;
    }
    return 0;
}

static int ath_archivo(const bt_plat_t *p, const char *nome)
{
    const uint8_t *d;
    int n;
    if (p->firmware(nome, &d, &n) < 0) { bt_log("[BT] Atheros: falta /lib/firmware/%s", nome); return -4; }
    int r = ath_enviar(p, d, n);
    p->firmware_liberar(d);
    if (r < 0) bt_log("[BT] Atheros: fallo al descargar %s (%d)", nome, r);
    return r;
}

int bt_ath_cargar(const bt_plat_t *p)
{
    int tipo = bt_ath_tipo(p->usb_vid, p->usb_pid);
    if (!tipo || !p->usb_vendor || !p->usb_bulk_salida || !p->firmware || !p->firmware_liberar) return 0;
    uint8_t estado = 0;
    if (p->usb_vendor(ATH_VENDOR_IN, ATH_GETSTATE, 0, 0, &estado, 1) < 0) { bt_log("[BT] Atheros: no contesta a GETSTATE"); return -1; }
    if (tipo == BT_ATH_3011) {
        if ((estado & ATH_MODO_MASCARA) == ATH_MODO_NORMAL) return 0;   /* ya corre el firmware */
        if (ath_archivo(p, "ath3k-1.fw") < 0) return -1;
        p->usb_vendor(ATH_VENDOR_OUT, ATH_SWITCH_VID_PID, 0, 0, NULL, 0);
        bt_log("[BT] Atheros AR3011: firmware cargado, el dispositivo se reenumera");
        return 1;
    }
    /* AR3012: parche y configuración según la versión de ROM; lo ya cargado se salta. */
    uint8_t v[16];
    memset(v, 0, sizeof v);
    if (p->usb_vendor(ATH_VENDOR_IN, ATH_GETVERSION, 0, 0, v, 16) < 0) { bt_log("[BT] Atheros: no contesta a GETVERSION"); return -1; }
    uint32_t rom = (uint32_t)v[0] | (uint32_t)v[1] << 8 | (uint32_t)v[2] << 16 | (uint32_t)v[3] << 24;
    uint8_t reloj = v[12];
    char nome[48];
    int hecho = 0;
    if (!(estado & ATH_PARCHE_HECHO)) {
        if (bt_ath_nombre_parche(rom, nome, sizeof nome) < 0 || ath_archivo(p, nome) < 0) return -1;
        hecho = 1;
    }
    if (p->usb_vendor(ATH_VENDOR_IN, ATH_GETSTATE, 0, 0, &estado, 1) < 0) return -1;
    if (!(estado & ATH_CFG_HECHO)) {
        if (bt_ath_nombre_cfg(rom, reloj, nome, sizeof nome) < 0 || ath_archivo(p, nome) < 0) return -1;
        hecho = 1;
    }
    if (!hecho) return 0;
    p->usb_vendor(ATH_VENDOR_OUT, ATH_SET_NORMAL, 0, 0, NULL, 0);
    p->usb_vendor(ATH_VENDOR_OUT, ATH_SWITCH_VID_PID, 0, 0, NULL, 0);
    bt_log("[BT] Atheros AR3012 (ROM %08x): parche y configuracion cargados, el dispositivo se reenumera", rom);
    return 1;
}

/* ------------------------------------------------------------------ máquina de estados HCI */

enum { F_NADA, F_BCM_NOME, F_BCM_MINI, F_BCM_ESPERA1, F_BCM_REG, F_BCM_ESPERA2, F_BCM_RESET,
       F_RTL_ROM, F_RTL_DESC, F_RTL_RESET };

static int fase = F_NADA;
static bool intentado, cargado, bcm_ok;
static const uint8_t *fw_dados, *cfg_dados;     /* archivos leídos (se liberan al acabar) */
static int fw_n, cfg_n;
static bt_fw_ident_t ident;
static uint32_t plazo;
static int pos_hcd, frag, n_frag;
static bt_rtl_parche_t rtl_p;
static char estado_txt[64];
static uint16_t esperando;

void bt_fw_reiniciar(void)
{
    fase = F_NADA;
    intentado = cargado = false;
    fw_dados = cfg_dados = NULL;
    estado_txt[0] = 0;
    esperando = 0;
}

bool bt_fw_cargado(void) { return cargado; }
const char *bt_fw_estado(void) { return estado_txt; }

static void liberar(void)
{
    const bt_plat_t *p = bt_plat();
    if (fw_dados && p && p->firmware_liberar) p->firmware_liberar(fw_dados);
    if (cfg_dados && p && p->firmware_liberar) p->firmware_liberar(cfg_dados);
    fw_dados = cfg_dados = NULL;
}

static int acabar(void)
{
    liberar();
    fase = F_NADA;
    esperando = 0;
    return BTF_FIN;
}

static int mandar(uint16_t op, const void *par, int n)
{
    esperando = op;
    if (bt_hci_cmd(op, par, n) < 0) { bt_log("[BT] firmware: cola de ordenes llena"); return acabar(); }
    return BTF_SIGUE;
}

bool bt_fw_espera(uint16_t op) { return fase != F_NADA && esperando && op == esperando; }

static int rtl_fragmento_siguiente(void)
{
    uint8_t b[BT_CMD_PAR];
    int n = bt_rtl_fragmento(&rtl_p, cfg_dados, cfg_n, frag, b);
    if (n < 0) return acabar();
    return mandar(OP_FW_RTL_DESCARGA, b, n);
}

static int bcm_registro_siguiente(void)
{
    bt_hcd_reg_t g;
    int r = bt_hcd_siguiente(fw_dados, fw_n, &pos_hcd, &g);
    if (r < 0) { bt_log("[BT] Broadcom: parche corrupto"); bcm_ok = false; fase = F_BCM_RESET; return mandar(OP_RESET, NULL, 0); }
    if (r == 0) { fase = F_BCM_ESPERA2; plazo = bt_agora() + 250; esperando = 0; return BTF_SIGUE; }   /* tras Launch_RAM */
    return mandar(g.op, g.par, g.n);
}

static int bcm_empezar(void)
{
    const bt_plat_t *p = bt_plat();
    if (!p->firmware) return acabar();
    fase = F_BCM_NOME;
    return mandar(OP_RD_NOME, NULL, 0);
}

static int rtl_empezar(const bt_rtl_chip_t *c)
{
    const bt_plat_t *p = bt_plat();
    if (!p->firmware) return acabar();
    bt_log("[BT] Realtek %s en ROM (lmp %04x, rev %04x): hace falta %s", c->chip, ident.lmp_sub, ident.hci_rev, c->fw);
    fase = F_RTL_ROM;
    return mandar(OP_FW_RTL_ROM, NULL, 0);
}

/* Se llama una vez por arranque, tras Read_Local_Version. */
int bt_fw_empezar(const bt_fw_ident_t *id)
{
    if (intentado) { cargado = false; return BTF_FIN; }   /* segunda pasada (tras releer la versión) */
    intentado = true;
    ident = *id;
    estado_txt[0] = 0;
    if (id->fabricante == BT_FAB_BROADCOM) return bcm_empezar();
    if (id->fabricante == BT_FAB_REALTEK) {
        const bt_rtl_chip_t *c = bt_rtl_buscar(id->lmp_sub, id->hci_rev, id->hci_ver);
        if (c) return rtl_empezar(c);
        bt_log("[BT] Realtek (lmp %04x, rev %04x, hci %u): sin tabla de firmware, se sigue con la ROM", id->lmp_sub, id->hci_rev, id->hci_ver);
    }
    return BTF_FIN;
}

static int bcm_respuesta(uint16_t op, uint8_t st, const uint8_t *r, int n)
{
    const bt_plat_t *p = bt_plat();
    (void)op;
    switch (fase) {
    case F_BCM_NOME: {
        char nom[48];
        if (st || n < 1) { bt_log("[BT] Broadcom: no se pudo leer el nombre local (%02x)", st); return acabar(); }
        char primero[48];
        primero[0] = 0;
        for (int v = 0; v < 2; v++) {
            if (bt_bcm_nombre(r, n > 248 ? 248 : n, ident.vid, ident.pid, v, nom, sizeof nom) < 0) {
                bt_log("[BT] Broadcom: nombre local irreconocible, se sigue con la ROM");
                return acabar();
            }
            if (!v) bt_copia(primero, nom, sizeof primero);
            if (p->firmware(nom, &fw_dados, &fw_n) >= 0) break;
            fw_dados = NULL;
        }
        if (!fw_dados) {
            bt_log("[BT] Broadcom: sin parche (/lib/firmware/%s no existe): se sigue con lo que tenga la ROM", primero);
            return acabar();
        }
        if (bt_hcd_validar(fw_dados, fw_n) < 1) {
            bt_log("[BT] Broadcom: %s no es un parche .hcd valido", nom);
            return acabar();
        }
        bt_copia(estado_txt, nom, sizeof estado_txt);
        bt_log("[BT] Broadcom: cargando %s", nom);
        bcm_ok = true;
        fase = F_BCM_MINI;
        return mandar(OP_FW_BCM_MINI, NULL, 0);
    }
    case F_BCM_MINI:
        if (st) bt_log("[BT] Broadcom: Download_Minidriver rechazada (%02x): se prueba igual", st);
        fase = F_BCM_ESPERA1;
        plazo = bt_agora() + 50;
        esperando = 0;
        return BTF_SIGUE;
    case F_BCM_REG:
        if (st) {
            bt_log("[BT] Broadcom: registro %04x rechazado (%02x): se aborta el parche", op, st);
            bcm_ok = false;
            fase = F_BCM_RESET;
            return mandar(OP_RESET, NULL, 0);
        }
        return bcm_registro_siguiente();
    case F_BCM_RESET:
        cargado = bcm_ok && st == 0;
        return acabar();
    }
    return acabar();
}

static int rtl_respuesta(uint16_t op, uint8_t st, const uint8_t *r, int n)
{
    const bt_plat_t *p = bt_plat();
    const bt_rtl_chip_t *c = bt_rtl_buscar(ident.lmp_sub, ident.hci_rev, ident.hci_ver);
    switch (fase) {
    case F_RTL_ROM: {
        if (st || n < 1 || !c) { bt_log("[BT] Realtek: no se pudo leer la version de la ROM (%02x)", st); return acabar(); }
        uint8_t rom = r[0];
        if (p->firmware(c->fw, &fw_dados, &fw_n) < 0) {
            fw_dados = NULL;
            bt_log("[BT] Realtek: falta /lib/firmware/%s: se sigue con la ROM", c->fw);
            return acabar();
        }
        int e = bt_rtl_parche(fw_dados, fw_n, rom, &rtl_p);
        if (e < 0) {
            bt_log("[BT] Realtek: %s no sirve para la ROM %u (error %d)", c->fw, rom, e);
            return acabar();
        }
        if (c->cfg && p->firmware(c->cfg, &cfg_dados, &cfg_n) < 0) { cfg_dados = NULL; cfg_n = 0; bt_log("[BT] Realtek: sin %s: se carga sin configuracion", c->cfg); }
        if (!cfg_dados) cfg_n = 0;
        n_frag = bt_rtl_n_frag(rtl_p.n + cfg_n);
        frag = 0;
        bt_copia(estado_txt, c->fw, sizeof estado_txt);
        bt_log("[BT] Realtek: ROM %u, parche %d bytes + config %d, %d fragmentos (fw %08x)", rom, rtl_p.n, cfg_n, n_frag, rtl_p.fw_version);
        fase = F_RTL_DESC;
        return rtl_fragmento_siguiente();
    }
    case F_RTL_DESC:
        if (st) {
            bt_log("[BT] Realtek: fragmento %d rechazado (%02x): se aborta", frag, st);
            return acabar();
        }
        if (++frag >= n_frag) { fase = F_RTL_RESET; return mandar(OP_RESET, NULL, 0); }
        return rtl_fragmento_siguiente();
    case F_RTL_RESET:
        cargado = st == 0;
        return acabar();
    }
    (void)op;
    return acabar();
}

int bt_fw_respuesta(uint16_t op, uint8_t st, const uint8_t *r, int n)
{
    if (fase == F_NADA) return BTF_FIN;
    if (fase >= F_RTL_ROM) return rtl_respuesta(op, st, r, n);
    return bcm_respuesta(op, st, r, n);
}

int bt_fw_tic(void)
{
    if (fase == F_NADA) return BTF_FIN;
    if (fase == F_BCM_ESPERA1 && (int32_t)(bt_agora() - plazo) >= 0) {
        fase = F_BCM_REG;
        pos_hcd = 0;
        return bcm_registro_siguiente();
    }
    if (fase == F_BCM_ESPERA2 && (int32_t)(bt_agora() - plazo) >= 0) {
        fase = F_BCM_RESET;
        return mandar(OP_RESET, NULL, 0);
    }
    return BTF_SIGUE;
}

int bt_fw_abortar(void)
{
    bt_log("[BT] firmware: el controlador dejo de contestar durante la carga: se sigue sin parche");
    cargado = false;
    return acabar();
}
