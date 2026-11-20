// SPDX-License-Identifier: GPL-2.0-or-later
/* wifi_pcie_logica.h - la parte pura del soporte de más Wi-Fi integradas PCIe (D410):
 * catálogo de tarjetas por identificador PCI, familias de las Atheros (ath9k/athn),
 * decodificación del SREV, geometría y lectura de la ROM de la familia AR5008
 * (AR9285 «4K», AR9287, AR9280...), cálculo de canales y del sintetizador de 2,4 GHz.
 * Sin acceso al hardware: la prueba de unidad del anfitrión
 * (tests/recursos/wifi_pcie_anfitriao.c, tests/h410_wifi_pcie.py) compila exactamente
 * este archivo, igual que ar9485_logica.h con el AR9485.
 *
 * Las constantes (SREV, geometría de la ROM, formato de la cabecera) salen del athn(4)
 * de OpenBSD (ISC: athnreg.h, ar5008reg.h, ar5416reg.h, ar9285reg.h, ar9287reg.h,
 * ar9280.c) y los identificadores PCI de if_athn_pci.c y del ath9k de Linux (solo los
 * números). No se copia código. EXPERIMENTAL: ninguna de estas tarjetas (salvo el
 * AR9485, que sigue en ar9485.c) se ha probado en hardware. */
#ifndef SAGUI_WIFI_PCIE_LOGICA_H
#define SAGUI_WIFI_PCIE_LOGICA_H
#ifdef SAGUI_KERNEL
#include "tipos.h"
#else
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#endif

/* ---------------------------------------------------------- catálogo por PCI */

typedef enum {
    WPCI_FAM_DESCONOCIDA = 0,
    WPCI_FAM_ATH_5008,      /* Atheros AR5008/AR9002: AR9285, AR9287, AR9280... (DMA de 20 palabras, ROM por palabras) */
    WPCI_FAM_ATH_9003,      /* Atheros AR9003: AR9380, AR9485, AR9462, AR9565 (el de ar9485.c) */
    WPCI_FAM_BRCM_SMAC,     /* Broadcom 802.11n «softmac» (brcmsmac): BCM4313, BCM43224... con microcódigo */
    WPCI_FAM_RALINK_PCI     /* Ralink/MediaTek RT2860/RT3090 PCI(e): rt2800 con firmware rt2860.bin */
} wpci_fam_t;

typedef enum {
    WPCI_ESTADO_ACTIVO = 0,      /* hay driver (solo el AR9485) */
    WPCI_ESTADO_IDENTIFICADA     /* se reconoce y se registra; el driver aún no existe (experimental) */
} wpci_estado_t;

typedef struct {
    uint16_t    fabricante, modelo;
    const char *nome;
    wpci_fam_t  fam;
    uint8_t     cadeias;         /* cadenas de transmisión máximas */
    bool        ghz5;            /* también 5 GHz */
    wpci_estado_t estado;
    const char *rom;             /* de dónde sale la calibración: «EEPROM 4K», «EEPROM AR9300»... */
    const char *firmware;        /* ruta en firmware/ o «ninguno» */
    const char *portatil;        /* máquina del mantenedor o portátiles típicos */
} wpci_chip_t;

#define WPCI_V_ATHEROS   0x168c
#define WPCI_V_BROADCOM  0x14e4
#define WPCI_V_RALINK    0x1814

static const wpci_chip_t wpci_catalogo[] = {
    { 0x168c, 0x002a, "Atheros AR9280/AR9281", WPCI_FAM_ATH_5008, 2, true,  WPCI_ESTADO_IDENTIFICADA, "EEPROM AR5416", "ninguno", "portatiles 2008-2010 de doble banda" },
    { 0x168c, 0x002b, "Atheros AR9285",        WPCI_FAM_ATH_5008, 1, false, WPCI_ESTADO_IDENTIFICADA, "EEPROM 4K",     "ninguno", "HP dv7 (probable), netbooks y portatiles 2009-2011" },
    { 0x168c, 0x002c, "Atheros AR2427",        WPCI_FAM_ATH_5008, 1, false, WPCI_ESTADO_IDENTIFICADA, "EEPROM 4K",     "ninguno", "netbooks de gama baja" },
    { 0x168c, 0x002d, "Atheros AR9227",        WPCI_FAM_ATH_5008, 2, false, WPCI_ESTADO_IDENTIFICADA, "EEPROM AR9287", "ninguno", "portatiles 2009-2010" },
    { 0x168c, 0x002e, "Atheros AR9287",        WPCI_FAM_ATH_5008, 2, false, WPCI_ESTADO_IDENTIFICADA, "EEPROM AR9287", "ninguno", "portatiles 2009-2010" },
    { 0x168c, 0x0030, "Atheros AR9380/AR9382", WPCI_FAM_ATH_9003, 3, true,  WPCI_ESTADO_IDENTIFICADA, "EEPROM AR9300", "ninguno", "portatiles 2010-2012 de doble banda" },
    { 0x168c, 0x0032, "Atheros AR9485",        WPCI_FAM_ATH_9003, 1, false, WPCI_ESTADO_ACTIVO,       "EEPROM AR9300", "ninguno", "Toshiba Satellite L850" },
    { 0x168c, 0x0033, "Atheros AR9580",        WPCI_FAM_ATH_9003, 3, true,  WPCI_ESTADO_IDENTIFICADA, "EEPROM AR9300", "ninguno", "portatiles 2011-2012" },
    { 0x168c, 0x0034, "Atheros AR9462",        WPCI_FAM_ATH_9003, 2, true,  WPCI_ESTADO_IDENTIFICADA, "EEPROM AR9300", "ninguno", "Wi-Fi + Bluetooth combinada, 2012" },
    { 0x168c, 0x0036, "Atheros AR9565",        WPCI_FAM_ATH_9003, 1, false, WPCI_ESTADO_IDENTIFICADA, "EEPROM AR9300", "ninguno", "Wi-Fi + Bluetooth combinada, 2013" },
    { 0x168c, 0x0037, "Atheros AR9485 (variante)", WPCI_FAM_ATH_9003, 1, false, WPCI_ESTADO_IDENTIFICADA, "EEPROM AR9300", "ninguno", "portatiles 2012" },
    { 0x14e4, 0x4727, "Broadcom BCM4313",      WPCI_FAM_BRCM_SMAC, 1, false, WPCI_ESTADO_IDENTIFICADA, "SPROM/OTP",    "firmware/broadcom/bcm43xx-0.fw y bcm43xx_hdr-0.fw (por poner)", "Toshiba Satellite L635" },
    { 0x14e4, 0x4357, "Broadcom BCM43225",     WPCI_FAM_BRCM_SMAC, 2, false, WPCI_ESTADO_IDENTIFICADA, "SPROM/OTP",    "firmware/broadcom/ (por poner)", "portatiles 2010-2012" },
    { 0x14e4, 0x4353, "Broadcom BCM43224",     WPCI_FAM_BRCM_SMAC, 2, true,  WPCI_ESTADO_IDENTIFICADA, "SPROM/OTP",    "firmware/broadcom/ (por poner)", "portatiles 2010-2012" },
    { 0x1814, 0x3090, "Ralink RT3090",         WPCI_FAM_RALINK_PCI, 1, false, WPCI_ESTADO_IDENTIFICADA, "EEPROM 93C66/eFuse", "firmware/ralink/rt2860.bin (por poner)", "portatiles 2009-2011" },
    { 0x1814, 0x3060, "Ralink RT3060",         WPCI_FAM_RALINK_PCI, 1, false, WPCI_ESTADO_IDENTIFICADA, "EEPROM 93C66/eFuse", "firmware/ralink/rt2860.bin (por poner)", "portatiles 2009-2011" },
    { 0x1814, 0x3062, "Ralink RT3062",         WPCI_FAM_RALINK_PCI, 2, false, WPCI_ESTADO_IDENTIFICADA, "EEPROM 93C66/eFuse", "firmware/ralink/rt2860.bin (por poner)", "portatiles 2009-2011" },
};
#define WPCI_N_CATALOGO ((int)(sizeof wpci_catalogo / sizeof wpci_catalogo[0]))

/* Entrada del catálogo para un identificador PCI; NULL si no es de la familia. */
static inline const wpci_chip_t *wpci_buscar(uint16_t fabricante, uint16_t modelo)
{
    for (int i = 0; i < WPCI_N_CATALOGO; i++)
        if (wpci_catalogo[i].fabricante == fabricante && wpci_catalogo[i].modelo == modelo) return &wpci_catalogo[i];
    return (const wpci_chip_t *)0;
}

/* ---------------------------------------------------------- SREV de las Atheros */

/* Versiones de MAC (AR_SREV_VERSION_*): las de athnreg.h de OpenBSD y, para las posteriores, las del ath9k. */
#define WPCI_SREV_5416_PCIE  0x00c
#define WPCI_SREV_5416_PCI   0x00d
#define WPCI_SREV_9100       0x014
#define WPCI_SREV_9160       0x040
#define WPCI_SREV_9280       0x080
#define WPCI_SREV_9285       0x0c0
#define WPCI_SREV_9271       0x140     /* USB */
#define WPCI_SREV_9287       0x180
#define WPCI_SREV_9380       0x1c0     /* AR9380/AR9382 y AR9580 (la revisión las distingue) */
#define WPCI_SREV_9330       0x200     /* SoC, no PCIe de portátil */
#define WPCI_SREV_9485       0x240
#define WPCI_SREV_9462       0x280
#define WPCI_SREV_9565       0x2c0
#define WPCI_SREV_9340       0x300     /* SoC */

/* Del registro AR_SREV (0x4020) a versión y revisión de la MAC, como en athn_attach (el formato corto solo
 * lo usan las AR5416 antiguas; de AR9160 en adelante el byte bajo vale 0xFF y vale el formato largo). */
static inline void wpci_ath_srev(uint32_t reg, uint32_t *ver, uint32_t *rev)
{
    if ((reg & 0xff) == 0xff) {                 /* formato largo (AR9160 en adelante) */
        *ver = (reg & 0xfffc0000u) >> 12;
        *rev = (reg & 0x00000f00u) >> 8;
    } else {                                    /* formato corto (AR5008) */
        *ver = (reg & 0xf0) >> 4;
        *rev = reg & 0x07;
    }
}

typedef enum { WPCI_ROM_NINGUNA = 0, WPCI_ROM_4K, WPCI_ROM_DEF, WPCI_ROM_9287, WPCI_ROM_9300 } wpci_rom_t;

/* Chip y familia por versión de MAC. */
typedef struct { const char *nome; wpci_fam_t fam; wpci_rom_t rom; uint8_t cadeias; bool soportado_pcie; } wpci_ath_ver_t;

static inline wpci_ath_ver_t wpci_ath_version(uint32_t ver)
{
    wpci_ath_ver_t r = { "desconocido", WPCI_FAM_DESCONOCIDA, WPCI_ROM_NINGUNA, 0, false };
    switch (ver) {
    case WPCI_SREV_5416_PCIE: case WPCI_SREV_5416_PCI:
        r = (wpci_ath_ver_t){ "AR5416/AR5418", WPCI_FAM_ATH_5008, WPCI_ROM_DEF, 3, true }; break;
    case WPCI_SREV_9160: r = (wpci_ath_ver_t){ "AR9160", WPCI_FAM_ATH_5008, WPCI_ROM_DEF, 3, true }; break;
    case WPCI_SREV_9280: r = (wpci_ath_ver_t){ "AR9280/AR9281", WPCI_FAM_ATH_5008, WPCI_ROM_DEF, 2, true }; break;
    case WPCI_SREV_9285: r = (wpci_ath_ver_t){ "AR9285", WPCI_FAM_ATH_5008, WPCI_ROM_4K, 1, true }; break;
    case WPCI_SREV_9287: r = (wpci_ath_ver_t){ "AR9287", WPCI_FAM_ATH_5008, WPCI_ROM_9287, 2, true }; break;
    case WPCI_SREV_9380: r = (wpci_ath_ver_t){ "AR9380/AR9382/AR9580", WPCI_FAM_ATH_9003, WPCI_ROM_9300, 3, true }; break;
    case WPCI_SREV_9485: r = (wpci_ath_ver_t){ "AR9485", WPCI_FAM_ATH_9003, WPCI_ROM_9300, 1, true }; break;
    case WPCI_SREV_9462: r = (wpci_ath_ver_t){ "AR9462", WPCI_FAM_ATH_9003, WPCI_ROM_9300, 2, true }; break;
    case WPCI_SREV_9565: r = (wpci_ath_ver_t){ "AR9565", WPCI_FAM_ATH_9003, WPCI_ROM_9300, 1, true }; break;
    case WPCI_SREV_9271: r = (wpci_ath_ver_t){ "AR9271 (USB)", WPCI_FAM_ATH_5008, WPCI_ROM_9287, 1, false }; break;
    case WPCI_SREV_9330: case WPCI_SREV_9340: r = (wpci_ath_ver_t){ "AR93x0 (SoC)", WPCI_FAM_ATH_9003, WPCI_ROM_9300, 1, false }; break;
    default: break;
    }
    return r;
}

/* ---------------------------------------------------------- ROM de la familia AR5008 */

#define WPCI_ROM_MAGIA        0xa55a    /* AR_EEPROM_MAGIC en una máquina little endian */
#define WPCI_ROM_VER_ATHEROS  0xe       /* AR_EEP_VER: versión mayor de la ROM */

/* Dónde empieza la imagen (en palabras de 16 bits) y cuánto mide (en bytes); 0 si no es de esta familia. */
static inline int wpci_rom5008_geometria(wpci_rom_t t, uint16_t *base_palavras, uint16_t *tam_bytes)
{
    switch (t) {
    case WPCI_ROM_4K:   *base_palavras = 64;  *tam_bytes = 376;  return 0;   /* AR9285_EEP_START_LOC, sizeof(ar9285_eeprom) */
    case WPCI_ROM_DEF:  *base_palavras = 256; *tam_bytes = 3256; return 0;   /* AR5416_EEP_START_LOC, sizeof(ar5416_eeprom) */
    case WPCI_ROM_9287: *base_palavras = 128; *tam_bytes = 727;  return 0;   /* AR9287_EEP_START_LOC, sizeof(ar9287_eeprom) */
    default: return -1;
    }
}

/* Palabra 0 de la ROM: 0 = nativa, 1 = hay que intercambiar los bytes de cada palabra, -1 = no es ROM. */
static inline int wpci_rom5008_magia(uint16_t m)
{
    if (m == WPCI_ROM_MAGIA) return 0;
    if (m == (uint16_t)((WPCI_ROM_MAGIA << 8) | (WPCI_ROM_MAGIA >> 8))) return 1;
    return -1;
}

/* Suma de control: el OR exclusivo de todas las palabras (ya con el orden de bytes del anfitrión) vale 0xFFFF. */
static inline bool wpci_rom5008_soma_ok(const uint16_t *w, int n, bool trocar)
{
    uint16_t s = 0;
    for (int i = 0; i < n; i++) {
        uint16_t x = w[i];
        if (trocar) x = (uint16_t)((x << 8) | (x >> 8));
        s ^= x;
    }
    return s == 0xffff;
}

/* Cabecera común de la ROM (struct ar5416_base_eep_header, los primeros 31 bytes; igual en AR9285 y AR9287). */
typedef struct {
    uint16_t longitude, soma, versao;
    uint8_t  eep_ver, eep_rev;
    uint8_t  opcap;
    bool     g, a, n_2g;            /* 802.11g, 802.11a, 802.11n en 2,4 GHz (los bits de «sin HT» al revés) */
    uint8_t  mac[6];
    uint8_t  rxmask, txmask;
    uint8_t  cadeias_tx, cadeias_rx;
    bool     rfsilent_ativo, rfsilent_invertido;
    uint8_t  rfsilent_pino;
    uint16_t devcap;
    uint8_t  kc_entradas;           /* entradas de la caché de claves */
    uint8_t  tipo_dispositivo;
} wpci_rom5008_t;

#define WPCI_ROM5008_CAB 31

static inline int wpci_popcount8(uint8_t v) { int n = 0; while (v) { n += v & 1; v >>= 1; } return n; }

/* Lee la cabecera de una imagen (bytes, orden del anfitrión, «tam» ≥ 31). 0 = bien; negativo = error:
 * -1 imagen corta, -2 versión no soportada (athn pide la mayor 14 y la menor distinta de 0). */
static inline int wpci_rom5008_cabecera(const uint8_t *b, int tam, wpci_rom5008_t *o)
{
    if (tam < WPCI_ROM5008_CAB) return -1;
    o->longitude = (uint16_t)(b[0] | b[1] << 8);
    o->soma = (uint16_t)(b[2] | b[3] << 8);
    o->versao = (uint16_t)(b[4] | b[5] << 8);
    o->eep_ver = (uint8_t)((o->versao >> 12) & 0xf);
    o->eep_rev = (uint8_t)(o->versao & 0xfff);        /* athn lo guarda en 8 bits en la práctica: las menores son < 32 */
    if (o->eep_ver != WPCI_ROM_VER_ATHEROS || (o->versao & 0xfff) == 0) return -2;
    o->opcap = b[6];
    o->a = (o->opcap & 0x01) != 0;
    o->g = (o->opcap & 0x02) != 0;
    o->n_2g = o->g && (o->opcap & 0x20) == 0;         /* AR_OPFLAGS_11N_2G20 = «sin HT20» */
    for (int i = 0; i < 6; i++) o->mac[i] = b[10 + i];
    o->rxmask = b[16];
    o->txmask = b[17];
    o->cadeias_tx = (uint8_t)wpci_popcount8(o->txmask);
    o->cadeias_rx = (uint8_t)wpci_popcount8(o->rxmask);
    uint16_t rfs = (uint16_t)(b[18] | b[19] << 8);
    o->rfsilent_ativo = (rfs & 0x0001) != 0;
    o->rfsilent_pino = (uint8_t)((rfs & 0x001c) >> 2);
    o->rfsilent_invertido = (rfs & 0x0002) != 0;
    o->devcap = (uint16_t)(b[22] | b[23] << 8);
    uint8_t kc = (uint8_t)((o->devcap & 0xf000) >> 12);
    uint32_t nkc = kc ? (1u << kc) : 128;     /* en 32 bits: con kc >= 8 no cabe en un byte */
    o->kc_entradas = (uint8_t)(nkc > 128 ? 128 : nkc);
    o->tipo_dispositivo = b[30];
    return 0;
}

/* ---------------------------------------------------------- canales y sintetizador de 2,4 GHz */

/* Frecuencia de un canal de 2,4 GHz (1 a 14); 0 si no vale. */
static inline uint16_t wpci_canal_freq(uint8_t canal)
{
    if (canal == 14) return 2484;
    if (canal >= 1 && canal <= 13) return (uint16_t)(2407 + 5 * canal);
    return 0;
}

/* Palabra de AR9280_PHY_SYNTH_CONTROL (0x9874) para 2,4 GHz en AR9280/AR9285/AR9287
 * (ar9280_set_synth de athn): ((MHz << 16) / 15) | BMODE | FRACMODE sobre los 30 bits bajos. */
#define WPCI_AR9280_BMODE     0x20000000u
#define WPCI_AR9280_FRACMODE  0x10000000u
static inline uint32_t wpci_ar9280_synth_2g(uint16_t freq)
{
    return (((uint32_t)freq << 16) / 15) | WPCI_AR9280_BMODE | WPCI_AR9280_FRACMODE;
}

#endif
