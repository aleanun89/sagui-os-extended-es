// SPDX-License-Identifier: GPL-3.0-only
/* ure_logica.h - lógica pura del Ethernet USB Realtek RTL8152, RTL8153, RTL8153B,
 * RTL8156 y RTL8156B (D433), sin llamadas al núcleo: el driver (kernel/rede/ure.c)
 * le pasa las transferencias de control por ure_io_t, y las pruebas del anfitrión
 * (tests/recursos/ure_anfitriao.c) le pasan un chip simulado.
 *
 * Los valores de registros y las secuencias de inicio vienen de OpenBSD
 * sys/dev/usb/if_ure.c (ISC/BSD de dos clausulas), copiados en
 * kernel/terceros/bsd/ure/ureg.h (ver docs/terceros.md). El orden de las
 * operaciones se ha respetado; la forma del código es propia.
 *
 * Fuera de alcance (ver docs/ure.md): RTL8157/8159 y RTL8153D (0x7420), que
 * necesitan firmware y otro inicio; el descarte de VLAN y las descargas de
 * sumas de control.
 *
 * Aviso de la fuente de las secuencias (OpenBSD if_ure.c, BSD de dos clausulas; el texto
 * completo de la licencia está en kernel/terceros/bsd/ure/ureg.h):
 *   Copyright (c) 2015, 2016, 2019 Kevin Lo <kevlo@openbsd.org>
 *   Copyright (c) 2020 Jonathon Fletcher <jonathon.fletcher@gmail.com>
 *   All rights reserved. */
#ifndef SAGUI_URE_LOGICA_H
#define SAGUI_URE_LOGICA_H

#ifdef SAGUI_KERNEL
#include "tipos.h"
#else
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#endif
#include "../terceros/bsd/ure/ureg.h"
#include "ure_ids.h"

/* Constantes de Ethernet que usa el USB (no vienen de la cabecera de registros). */
#ifndef ETHER_HDR_LEN
#define ETHER_HDR_LEN        14
#endif
#ifndef ETHER_CRC_LEN
#define ETHER_CRC_LEN        4
#endif
#ifndef ETHER_VLAN_ENCAP_LEN
#define ETHER_VLAN_ENCAP_LEN 4
#endif
#ifndef ETHER_MAX_LEN
#define ETHER_MAX_LEN        1518
#endif

/* Registros MII y bits de anuncio (IEEE 802.3, clausula 22 y 1000BASE-T): valores estandar,
 * no vienen de la cabecera de registros de OpenBSD (que los toma de dev/mii/mii.h). */
#ifndef MII_BMCR
#define MII_BMCR        0
#define MII_ANAR        4
#define MII_100T2CR     9
#define BMCR_AUTOEN     0x1000
#define BMCR_STARTNEG   0x0200
#define ANAR_10         0x0020
#define ANAR_10_FD      0x0040
#define ANAR_TX         0x0080
#define ANAR_TX_FD      0x0100
#define ANAR_FC         0x0400
#define ANAR_PAUSE_ASYM 0x0800
#define GTCR_ADV_1000THDX 0x0100
#define GTCR_ADV_1000TFDX 0x0200
#endif

#define URE_MTU               1500
#define URE_TRAMA_MAX         1514   /* ETH_MAX de la pila (sin CRC) */
#define URE_TRAMA_MIN         60     /* ETH_MIN de la pila (sin CRC): se rellena con ceros */
#define URE_RX_CABECERA       24     /* struct ure_rxpkt: pktlen, vlan, csum y tres reservados (6 x 32 bits) */
#define URE_TX_CABECERA       8      /* struct ure_txpkt: pktlen, vlan */
#define URE_RX_BUF_ALIGN_V    8      /* alineación de cada trama dentro de la transferencia */
#define URE_BUF_RX            16384  /* usb_rx_abrir() no admite más de 16 KB por transferencia */
#define URE_BUF_TX            1600   /* cabecera + trama + relleno */

/* Chips soportados (y reconocidos sin soporte). */
enum { URE_CH_NINGUNO = 0, URE_CH_8152, URE_CH_8153, URE_CH_8153B, URE_CH_8156, URE_CH_8156B };

/* Pasos del arranque, para el diagnóstico (ure_nome_passo). */
enum { URE_P_NINGUNO = 0, URE_P_CHIP, URE_P_AUTOCARGA, URE_P_FLASH, URE_P_REINICIO, URE_P_OOB,
       URE_P_INICIO_PHY, URE_P_MAC, URE_P_FILTRO, URE_P_ANUNCIO, URE_P_CONTROL, URE_P_FIN };

/* Codigos de error de las transferencias de recepción (negativos). */
#define URE_RX_E_CORTA   (-1)   /* quedan menos de 12 bytes: cabecera incompleta */
#define URE_RX_E_LARGA   (-2)   /* la trama dice más bytes de los que quedan */
#define URE_RX_E_PEQUENA (-3)   /* trama menor que la cabecera Ethernet más el CRC */
#define URE_RX_E_MAYOR   (-4)   /* trama mayor que el búfer de la pila */

/* Acceso a registros: lo pone el driver (usb_controle) o la prueba (chip simulado).
 * ctl(ctx, lectura, valor, indice, buf, n): 0 o <0 si falla la transferencia.
 * espera_ms(ctx, ms): esperas del inicio (siempre acotadas por el llamador). */
typedef struct ure_io {
    void *ctx;
    int  (*ctl)(void *ctx, int lectura, uint16_t valor, uint16_t indice, uint8_t *buf, uint16_t n);
    void (*espera_ms)(void *ctx, uint32_t ms);
    uint32_t errores;         /* transferencias de control fallidas (diagnóstico) */
} ure_io_t;

/* Recepción: la transferencia agregada y la posición del lector. */
typedef struct ure_rx {
    const uint8_t *buf;
    uint32_t n;
    uint32_t pos;
} ure_rx_t;

static inline uint32_t ure_alinea(uint32_t x, uint32_t a) { return (x + a - 1) & ~(a - 1); }

static inline const char *ure_nome_chip(int chip)
{
    switch (chip) {
    case URE_CH_8152:  return "RTL8152";
    case URE_CH_8153:  return "RTL8153";
    case URE_CH_8153B: return "RTL8153B";
    case URE_CH_8156:  return "RTL8156";
    case URE_CH_8156B: return "RTL8156B";
    default:           return "no soportado";
    }
}

static inline const char *ure_nome_passo(int p)
{
    static const char *nomes[] = { "ninguno", "identificar el chip", "autocarga del chip", "carga de la flash",
                                   "reinicio del controlador", "control OOB", "inicio del PHY",
                                   "dirección MAC", "filtro de recepción", "anuncio de velocidades",
                                   "transferencias de control (el USB no contesta)", "fin" };
    return (p >= 0 && p <= URE_P_FIN) ? nomes[p] : "?";
}

/* Chip por la versión de PLA_TCR1 (con URE_VERSION_MASK aplicada). 0 = sin soporte
 * (RTL8157/8159, RTL8153D y versiones desconocidas). */
static inline int ure_chip_de_versao(uint16_t ver)
{
    switch (ver) {
    case 0x4c00: case 0x4c10:                               return URE_CH_8152;
    case 0x5c00: case 0x5c10: case 0x5c20: case 0x5c30:     return URE_CH_8153;
    case 0x6000: case 0x6010:                               return URE_CH_8153B;
    case 0x7020: case 0x7030:                               return URE_CH_8156;
    case 0x7410:                                            return URE_CH_8156B;
    default:                                                return URE_CH_NINGUNO;
    }
}

/* 8153B, 8156 y 8156B comparten las secuencias «B». */
static inline bool ure_es_b(int chip) { return chip == URE_CH_8153B || chip == URE_CH_8156 || chip == URE_CH_8156B; }
static inline bool ure_es_815x(int chip) { return chip == URE_CH_8156 || chip == URE_CH_8156B; }

/* Identificadores USB de los adaptadores de la tabla de OpenBSD (ure_ids.h). */
static inline bool ure_id_conhecido(uint16_t vid, uint16_t pid)
{
    for (int i = 0; i < URE_N_IDS; i++)
        if (ure_ids[i].vid == vid && ure_ids[i].pid == pid) return true;
    return false;
}

/* ---------------------------------------------------------- acceso a registros */

static inline int ure_lee(ure_io_t *io, uint16_t addr, uint16_t idx, uint8_t *buf, uint16_t n)
{
    int r = io->ctl(io->ctx, 1, addr, idx, buf, n);      /* 1 = lectura */
    if (r < 0) io->errores++;
    return r;
}

static inline int ure_escribe(ure_io_t *io, uint16_t addr, uint16_t idx, const uint8_t *buf, uint16_t n)
{
    int r = io->ctl(io->ctx, 0, addr, idx, (uint8_t *)buf, n);   /* 0 = escritura */
    if (r < 0) io->errores++;
    return r;
}

static inline uint32_t ure_r4(ure_io_t *io, uint16_t reg, uint16_t idx)
{
    uint8_t t[4] = { 0, 0, 0, 0 };
    ure_lee(io, reg, idx | 0, t, 4);
    return (uint32_t)t[0] | (uint32_t)t[1] << 8 | (uint32_t)t[2] << 16 | (uint32_t)t[3] << 24;
}

static inline uint16_t ure_r2(ure_io_t *io, uint16_t reg, uint16_t idx)
{
    uint32_t v = ure_r4(io, reg & ~3, idx);
    return (uint16_t)((v >> ((reg & 2) << 3)) & 0xffff);
}

static inline uint8_t ure_r1(ure_io_t *io, uint16_t reg, uint16_t idx)
{
    uint32_t v = ure_r4(io, reg & ~3, idx);
    return (uint8_t)((v >> ((reg & 3) << 3)) & 0xff);
}

/* Escrituras con máscara de bytes (URE_BYTE_EN_*), como ure_write_1/2/4 de OpenBSD. */
static inline int ure_w4(ure_io_t *io, uint16_t reg, uint16_t idx, uint32_t val)
{
    uint8_t t[4] = { (uint8_t)val, (uint8_t)(val >> 8), (uint8_t)(val >> 16), (uint8_t)(val >> 24) };
    return ure_escribe(io, reg, idx | URE_BYTE_EN_DWORD, t, 4);
}

static inline int ure_w2(ure_io_t *io, uint16_t reg, uint16_t idx, uint32_t val)
{
    uint16_t byen = URE_BYTE_EN_WORD;
    uint32_t shift = reg & 2;
    val &= 0xffff;
    if (reg & 2) {
        byen <<= shift;
        val <<= shift << 3;
        reg &= ~3;
    }
    uint32_t v = val;
    uint8_t t[4] = { (uint8_t)v, (uint8_t)(v >> 8), (uint8_t)(v >> 16), (uint8_t)(v >> 24) };
    return ure_escribe(io, reg, idx | byen, t, 4);
}

static inline int ure_w1(ure_io_t *io, uint16_t reg, uint16_t idx, uint32_t val)
{
    uint16_t byen = URE_BYTE_EN_BYTE;
    uint32_t shift = reg & 3;
    val &= 0xff;
    if (reg & 3) {
        byen <<= shift;
        val <<= shift << 3;
        reg &= ~3;
    }
    uint8_t t[4] = { (uint8_t)val, (uint8_t)(val >> 8), (uint8_t)(val >> 16), (uint8_t)(val >> 24) };
    return ure_escribe(io, reg, idx | byen, t, 4);
}

static inline void ure_set1(ure_io_t *io, uint16_t reg, uint16_t idx, uint32_t b) { ure_w1(io, reg, idx, ure_r1(io, reg, idx) | b); }
static inline void ure_clr1(ure_io_t *io, uint16_t reg, uint16_t idx, uint32_t b) { ure_w1(io, reg, idx, ure_r1(io, reg, idx) & ~b); }
static inline void ure_set2(ure_io_t *io, uint16_t reg, uint16_t idx, uint32_t b) { ure_w2(io, reg, idx, ure_r2(io, reg, idx) | b); }
static inline void ure_clr2(ure_io_t *io, uint16_t reg, uint16_t idx, uint32_t b) { ure_w2(io, reg, idx, ure_r2(io, reg, idx) & ~b); }
static inline void ure_clr4(ure_io_t *io, uint16_t reg, uint16_t idx, uint32_t b) { ure_w4(io, reg, idx, ure_r4(io, reg, idx) & ~b); }

/* PHY por OCP (ure_ocp_reg_read/write de OpenBSD): la ventana de 4 KB va en PLA_OCP_GPHY_BASE. */
static inline uint16_t ure_ocp_lee(ure_io_t *io, uint16_t addr)
{
    ure_w2(io, URE_PLA_OCP_GPHY_BASE, URE_MCU_TYPE_PLA, addr & 0xf000);
    return ure_r2(io, (addr & 0x0fff) | 0xb000, URE_MCU_TYPE_PLA);
}

static inline void ure_ocp_escribe(ure_io_t *io, uint16_t addr, uint16_t dato)
{
    ure_w2(io, URE_PLA_OCP_GPHY_BASE, URE_MCU_TYPE_PLA, addr & 0xf000);
    ure_w2(io, (addr & 0x0fff) | 0xb000, URE_MCU_TYPE_PLA, dato);
}

/* Registros MII estándar (802.3) a través de la OCP (URE_OCP_BASE_MII + reg*2). */
static inline void ure_mii_escribe(ure_io_t *io, int reg, uint16_t v) { ure_ocp_escribe(io, (uint16_t)(URE_OCP_BASE_MII + reg * 2), v); }

/* ---------------------------------------------------------- pasos comunes */

/* Espera (acotada) a que la autocarga del chip termine: 500 vueltas de 20 ms, como OpenBSD (el arranque va
 * en la tarea «ure», que duerme, y se corta en cuanto el adaptador se retira: las transferencias fallan). */
#define URE_VUELTAS_AUTOCARGA 500
static inline int ure_espera_autocarga(ure_io_t *io)
{
    for (int i = 0; i < URE_VUELTAS_AUTOCARGA; i++) {
        uint32_t e0 = io->errores;
        if (ure_r2(io, URE_PLA_BOOT_CTRL, URE_MCU_TYPE_PLA) & URE_AUTOLOAD_DONE) return 0;
        if (io->errores != e0) return -1;   /* el USB no contesta: no tiene sentido seguir esperando */
        io->espera_ms(io->ctx, 20);
    }
    return -1;
}

/* Espera (acotada) al estado del PHY: 500 vueltas de 20 ms, como OpenBSD. Devuelve el último estado leído. */
static inline uint16_t ure_fase_phy(ure_io_t *io, uint16_t deseado)
{
    uint16_t r = 0;
    for (int i = 0; i < 500; i++) {
        uint32_t e0 = io->errores;
        r = ure_ocp_lee(io, URE_OCP_PHY_STATUS) & URE_PHY_STAT_MASK;
        if (deseado) {
            if (r == deseado) break;
        } else if (r == URE_PHY_STAT_LAN_ON || r == URE_PHY_STAT_PWRDN || r == URE_PHY_STAT_EXT_INIT) {
            break;
        }
        if (io->errores != e0) break;
        io->espera_ms(io->ctx, 20);
    }
    return r;
}

/* RTL8153B/8156/8156B: la flash del PHY (si la hay) tiene que terminar de cargarse. */
static inline int ure_espera_flash(ure_io_t *io)
{
    if ((ure_r2(io, URE_PLA_GPHY_CTRL, URE_MCU_TYPE_PLA) & URE_GPHY_FLASH) &&
        !(ure_r2(io, URE_USB_GPHY_CTRL, URE_MCU_TYPE_USB) & URE_BYPASS_FLASH)) {
        for (int i = 0; i < 100; i++) {
            if (ure_r2(io, URE_USB_GPHY_CTRL, URE_MCU_TYPE_USB) & URE_GPHY_PATCH_DONE) return 0;
            io->espera_ms(io->ctx, 1);
        }
        return -1;
    }
    return 0;
}

/* Reinicio del controlador (ure_reset): los 8156 de la primera generación usan la
 * secuencia de ECM; los demás, CR_RST con sondeo de hasta 100 ms. */
static inline int ure_reinicio(ure_io_t *io, int chip)
{
    if (chip == URE_CH_8156) {
        ure_clr1(io, URE_PLA_CR, URE_MCU_TYPE_PLA, URE_CR_TE);
        ure_clr2(io, URE_USB_BMU_RESET, URE_MCU_TYPE_USB, BMU_RESET_EP_IN);
        ure_set2(io, URE_USB_USB_CTRL, URE_MCU_TYPE_USB, URE_CDC_ECM_EN);
        ure_clr1(io, URE_PLA_CR, URE_MCU_TYPE_PLA, URE_CR_RE);
        ure_set2(io, URE_USB_BMU_RESET, URE_MCU_TYPE_USB, BMU_RESET_EP_IN);
        ure_clr2(io, URE_USB_USB_CTRL, URE_MCU_TYPE_USB, URE_CDC_ECM_EN);
        return 0;
    }
    ure_w1(io, URE_PLA_CR, URE_MCU_TYPE_PLA, URE_CR_RST);
    for (int i = 0; i < 100; i++) {
        if (!(ure_r1(io, URE_PLA_CR, URE_MCU_TYPE_PLA) & URE_CR_RST)) return 0;
        io->espera_ms(io->ctx, 1);
    }
    return -1;
}

/* BMU: reinicia los dos endpoints bulk (ure_reset_bmu, rama USB). */
static inline void ure_reinicio_bmu(ure_io_t *io)
{
    uint8_t r = ure_r1(io, URE_USB_BMU_RESET, URE_MCU_TYPE_USB);
    r &= ~(BMU_RESET_EP_IN | BMU_RESET_EP_OUT);
    ure_w1(io, URE_USB_BMU_RESET, URE_MCU_TYPE_USB, r);
    r |= BMU_RESET_EP_IN | BMU_RESET_EP_OUT;
    ure_w1(io, URE_USB_BMU_RESET, URE_MCU_TYPE_USB, r);
}

static inline void ure_teredo(ure_io_t *io, int chip)
{
    if (ure_es_b(chip)) ure_w1(io, URE_PLA_TEREDO_CFG, URE_MCU_TYPE_PLA, 0xff);
    else ure_clr2(io, URE_PLA_TEREDO_CFG, URE_MCU_TYPE_PLA, URE_TEREDO_SEL | URE_TEREDO_RS_EVENT_MASK | URE_OOB_TEREDO_EN);
    ure_w2(io, URE_PLA_WDT6_CTRL, URE_MCU_TYPE_PLA, URE_WDT6_SET_MODE);
    ure_w2(io, URE_PLA_REALWOW_TIMER, URE_MCU_TYPE_PLA, 0);
    ure_w4(io, URE_PLA_TEREDO_TIMER, URE_MCU_TYPE_PLA, 0);
}

/* Espera (acotada, 1 s: URE_TIMEOUT de OpenBSD) a que la lista de enlaces esté lista (OOB). */
static inline int ure_oob_listo(ure_io_t *io)
{
    for (int i = 0; i < URE_TIMEOUT; i++) {
        uint32_t e0 = io->errores;
        if (ure_r1(io, URE_PLA_OOB_CTRL, URE_MCU_TYPE_PLA) & URE_LINK_LIST_READY) return 0;
        if (io->errores != e0) return -1;
        io->espera_ms(io->ctx, 1);
    }
    return -1;
}

/* ---------------------------------------------------------- inicio del chip (attach) */

/* RTL8152 (ure_rtl8152_init): ajustes de energía y agregación. ver = 0x4c00 o 0x4c10. */
static inline void ure_init_8152(ure_io_t *io, uint16_t ver)
{
    uint32_t pw;
    ure_ocp_escribe(io, URE_OCP_ALDPS_CONFIG, URE_ENPDNPS | URE_LINKENA | URE_DIS_SDSAVE);
    io->espera_ms(io->ctx, 20);
    if (ver == 0x4c00) ure_clr2(io, URE_PLA_LED_FEATURE, URE_MCU_TYPE_PLA, URE_LED_MODE_MASK);
    ure_clr2(io, URE_USB_UPS_CTRL, URE_MCU_TYPE_USB, URE_POWER_CUT);
    ure_clr2(io, URE_USB_PM_CTRL_STATUS, URE_MCU_TYPE_USB, URE_RESUME_INDICATE);
    ure_set2(io, URE_PLA_PHY_PWR, URE_MCU_TYPE_PLA, URE_TX_10M_IDLE_EN | URE_PFM_PWM_SWITCH);
    pw = ure_r4(io, URE_PLA_MAC_PWR_CTRL, URE_MCU_TYPE_PLA);
    pw &= ~URE_MCU_CLK_RATIO_MASK;
    pw |= URE_MCU_CLK_RATIO | URE_D3_CLK_GATED_EN;
    ure_w4(io, URE_PLA_MAC_PWR_CTRL, URE_MCU_TYPE_PLA, pw);
    ure_w2(io, URE_PLA_GPHY_INTR_IMR, URE_MCU_TYPE_PLA,
           URE_GPHY_STS_MSK | URE_SPEED_DOWN_MSK | URE_SPDWN_RXDV_MSK | URE_SPDWN_LINKCHG_MSK);
    ure_set2(io, URE_PLA_RSTTALLY, URE_MCU_TYPE_PLA, URE_TALLY_RESET);
    ure_clr2(io, URE_USB_USB_CTRL, URE_MCU_TYPE_USB, URE_RX_AGG_DISABLE | URE_RX_ZERO_EN);
}

/* RTL8153 (ure_rtl8153_init). ver = 0x5c00..0x5c30; super = el dispositivo va a 5 Gb/s. */
static inline int ure_init_8153(ure_io_t *io, uint16_t ver, bool super)
{
    uint8_t u1u2[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    uint16_t reg;
    ure_escribe(io, URE_USB_TOLERANCE, URE_BYTE_EN_SIX_BYTES, u1u2, 8);
    if (ure_espera_autocarga(io) < 0) return URE_P_AUTOCARGA;
    ure_fase_phy(io, 0);
    if (ver == 0x5c00 || ver == 0x5c10 || ver == 0x5c20)
        ure_ocp_escribe(io, URE_OCP_ADC_CFG, URE_CKADSEL_L | URE_ADC_EN | URE_EN_EMI_L);
    ure_fase_phy(io, URE_PHY_STAT_LAN_ON);
    ure_clr2(io, URE_USB_U2P3_CTRL, URE_MCU_TYPE_USB, URE_U2P3_ENABLE);
    if (ver == 0x5c10) {
        reg = ure_r2(io, URE_USB_SSPHYLINK2, URE_MCU_TYPE_USB);
        reg &= ~URE_PWD_DN_SCALE_MASK;
        reg |= URE_PWD_DN_SCALE(96);
        ure_w2(io, URE_USB_SSPHYLINK2, URE_MCU_TYPE_USB, reg);
        ure_set1(io, URE_USB_USB2PHY, URE_MCU_TYPE_USB, URE_USB2PHY_L1 | URE_USB2PHY_SUSPEND);
    } else if (ver == 0x5c20) {
        ure_clr1(io, URE_PLA_DMY_REG0, URE_MCU_TYPE_PLA, URE_ECM_ALDPS);
    }
    if (ver == 0x5c20 || ver == 0x5c30) {
        if (ure_r2(io, URE_USB_BURST_SIZE, URE_MCU_TYPE_USB)) ure_set1(io, URE_USB_CSR_DUMMY1, URE_MCU_TYPE_USB, URE_DYNAMIC_BURST);
        else ure_clr1(io, URE_USB_CSR_DUMMY1, URE_MCU_TYPE_USB, URE_DYNAMIC_BURST);
    }
    ure_set1(io, URE_USB_CSR_DUMMY2, URE_MCU_TYPE_USB, URE_EP4_FULL_FC);
    ure_clr2(io, URE_USB_WDT11_CTRL, URE_MCU_TYPE_USB, URE_TIMER11_EN);
    ure_clr2(io, URE_PLA_LED_FEATURE, URE_MCU_TYPE_PLA, URE_LED_MODE_MASK);
    reg = (ver == 0x5c10 && !super) ? URE_LPM_TIMER_500MS : URE_LPM_TIMER_500US;
    ure_w1(io, URE_USB_LPM_CTRL, URE_MCU_TYPE_USB, URE_FIFO_EMPTY_1FB | URE_ROK_EXIT_LPM | reg);
    reg = ure_r2(io, URE_USB_AFE_CTRL2, URE_MCU_TYPE_USB);
    reg &= ~URE_SEN_VAL_MASK;
    reg |= URE_SEN_VAL_NORMAL | URE_SEL_RXIDLE;
    ure_w2(io, URE_USB_AFE_CTRL2, URE_MCU_TYPE_USB, reg);
    ure_w2(io, URE_USB_CONNECT_TIMER, URE_MCU_TYPE_USB, 0x0001);
    ure_clr2(io, URE_USB_POWER_CUT, URE_MCU_TYPE_USB, URE_PWR_EN | URE_PHASE2_EN);
    ure_clr2(io, URE_USB_MISC_0, URE_MCU_TYPE_USB, URE_PCUT_STATUS);
    memset(u1u2, 0xff, sizeof u1u2);
    ure_escribe(io, URE_USB_TOLERANCE, URE_BYTE_EN_SIX_BYTES, u1u2, 8);
    ure_w2(io, URE_PLA_MAC_PWR_CTRL, URE_MCU_TYPE_PLA, 0);
    ure_w2(io, URE_PLA_MAC_PWR_CTRL2, URE_MCU_TYPE_PLA, 0);
    ure_w2(io, URE_PLA_MAC_PWR_CTRL3, URE_MCU_TYPE_PLA, 0);
    ure_w2(io, URE_PLA_MAC_PWR_CTRL4, URE_MCU_TYPE_PLA, 0);
    ure_clr2(io, URE_USB_USB_CTRL, URE_MCU_TYPE_USB, URE_RX_AGG_DISABLE | URE_RX_ZERO_EN);
    ure_set2(io, URE_PLA_RSTTALLY, URE_MCU_TYPE_PLA, URE_TALLY_RESET);
    return 0;
}

/* RTL8153B, RTL8156 y RTL8156B (ure_rtl8153b_init). Devuelve 0 o el paso que falló. */
static inline int ure_init_b(ure_io_t *io, int chip, uint16_t ver, bool super)
{
    uint16_t reg;
    if (ure_es_815x(chip)) {
        ure_clr1(io, URE_USB_ECM_OP, URE_MCU_TYPE_USB, URE_EN_ALL_SPEED);
        ure_w2(io, URE_USB_SPEED_OPTION, URE_MCU_TYPE_USB, 0);
        ure_set2(io, URE_USB_ECM_OPTION, URE_MCU_TYPE_USB, URE_BYPASS_MAC_RESET);
        if (chip == URE_CH_8156B) ure_set2(io, URE_USB_U2P3_CTRL, URE_MCU_TYPE_USB, URE_RX_DETECT8);
    }
    ure_clr2(io, URE_USB_LPM_CONFIG, URE_MCU_TYPE_USB, LPM_U1U2_EN);
    if (chip == URE_CH_8156B && ure_espera_flash(io) < 0) return URE_P_FLASH;
    if (ure_espera_autocarga(io) < 0) return URE_P_AUTOCARGA;
    ure_fase_phy(io, 0);
    ure_fase_phy(io, URE_PHY_STAT_LAN_ON);
    ure_clr2(io, URE_USB_U2P3_CTRL, URE_MCU_TYPE_USB, URE_U2P3_ENABLE);
    ure_w2(io, URE_USB_MSC_TIMER, URE_MCU_TYPE_USB, 4095);
    if (chip != URE_CH_8153B) ure_w2(io, URE_USB_U1U2_TIMER, URE_MCU_TYPE_USB, 500);
    ure_clr2(io, URE_USB_POWER_CUT, URE_MCU_TYPE_USB, URE_PWR_EN);
    ure_clr2(io, URE_USB_MISC_0, URE_MCU_TYPE_USB, URE_PCUT_STATUS);
    ure_clr1(io, URE_USB_POWER_CUT, URE_MCU_TYPE_USB, URE_UPS_EN | URE_USP_PREWAKE);
    ure_clr1(io, URE_USB_MISC_2, URE_MCU_TYPE_USB, URE_UPS_FORCE_PWR_DOWN | URE_UPS_NO_UPS);
    ure_clr1(io, URE_PLA_INDICATE_FALG, URE_MCU_TYPE_PLA, URE_UPCOMING_RUNTIME_D3);
    ure_clr1(io, URE_PLA_SUSPEND_FLAG, URE_MCU_TYPE_PLA, URE_LINK_CHG_EVENT);
    ure_clr2(io, URE_PLA_EXTRA_STATUS, URE_MCU_TYPE_PLA, URE_LINK_CHANGE_FLAG);
    ure_w1(io, URE_PLA_CRWECR, URE_MCU_TYPE_PLA, URE_CRWECR_CONFIG);
    ure_clr2(io, URE_PLA_CONFIG34, URE_MCU_TYPE_PLA, URE_LINK_OFF_WAKE_EN);
    ure_w1(io, URE_PLA_CRWECR, URE_MCU_TYPE_PLA, URE_CRWECR_NORAML);
    if (chip == URE_CH_8153B) {
        reg = ure_r2(io, URE_PLA_EXTRA_STATUS, URE_MCU_TYPE_PLA);
        if (ure_r2(io, URE_PLA_PHYSTATUS, URE_MCU_TYPE_PLA) & URE_PHYSTATUS_LINK) reg |= URE_CUR_LINK_OK;
        else reg &= ~URE_CUR_LINK_OK;
        ure_w2(io, URE_PLA_EXTRA_STATUS, URE_MCU_TYPE_PLA, reg | URE_POLL_LINK_CHG);
    }
    if (super) ure_set2(io, URE_USB_LPM_CONFIG, URE_MCU_TYPE_USB, LPM_U1U2_EN);
    if (chip == URE_CH_8156B) {
        ure_clr2(io, URE_PLA_RCR, URE_MCU_TYPE_PLA, URE_SLOT_EN);
        ure_set2(io, URE_PLA_CPCR, URE_MCU_TYPE_PLA, URE_FLOW_CTRL_EN);
        ure_w2(io, URE_USB_FC_TIMER, URE_MCU_TYPE_USB, URE_CTRL_TIMER_EN | 75);
        reg = ure_r2(io, URE_USB_FW_CTRL, URE_MCU_TYPE_USB);
        if (!(ure_r2(io, URE_PLA_POL_GPIO_CTRL, URE_MCU_TYPE_PLA) & URE_DACK_DET_EN)) reg |= URE_FLOW_CTRL_PATCH_2;
        reg &= ~URE_AUTO_SPEEDUP;
        ure_w2(io, URE_USB_FW_CTRL, URE_MCU_TYPE_USB, reg);
        ure_set2(io, URE_USB_FW_TASK, URE_MCU_TYPE_USB, URE_FC_PATCH_TASK);
    }
    if (ure_es_815x(chip)) {
        ure_clr2(io, URE_PLA_MAC_PWR_CTRL3, URE_MCU_TYPE_PLA, URE_PLA_MCU_SPDWN_EN);
        reg = ure_r2(io, URE_PLA_EXTRA_STATUS, URE_MCU_TYPE_PLA);
        reg &= ~URE_CUR_LINK_OK;
        if (ure_r2(io, URE_PLA_PHYSTATUS, URE_MCU_TYPE_PLA) & URE_PHYSTATUS_LINK) reg |= URE_CUR_LINK_OK;
        ure_w2(io, URE_PLA_EXTRA_STATUS, URE_MCU_TYPE_PLA, reg | URE_POLL_LINK_CHG);
    } else {
        ure_set2(io, URE_PLA_MAC_PWR_CTRL2, URE_MCU_TYPE_PLA, URE_MAC_CLK_SPDWN_EN);
    }
    ure_clr2(io, URE_USB_USB_CTRL, URE_MCU_TYPE_USB, URE_RX_AGG_DISABLE | URE_RX_ZERO_EN);
    if (chip == URE_CH_8156) ure_set1(io, URE_USB_BMU_CONFIG, URE_MCU_TYPE_USB, URE_ACT_ODMA);
    if (chip != URE_CH_8153B) {
        /* 0xa5b4 bit 15 = 0: modo de forzado por MDIO (como OpenBSD) */
        ure_ocp_escribe(io, 0xa5b4, ure_ocp_lee(io, 0xa5b4) & ~0x8000);
    }
    ure_set2(io, URE_PLA_RSTTALLY, URE_MCU_TYPE_PLA, URE_TALLY_RESET);
    (void)ver;
    return 0;
}

/* Arranque del chip (attach): identifica la versión y aplica el inicio de su familia.
 * Devuelve 0, o el paso (URE_P_*) donde no respondió. ver = PLA_TCR1 & URE_VERSION_MASK. */
static inline int ure_iniciar_chip(ure_io_t *io, int chip, uint16_t ver, bool super)
{
    int r;
    switch (chip) {
    case URE_CH_8152:
        ure_init_8152(io, ver);
        r = 0;
        break;
    case URE_CH_8153:
        r = ure_init_8153(io, ver, super);
        break;
    case URE_CH_8153B:
    case URE_CH_8156:
    case URE_CH_8156B:
        r = ure_init_b(io, chip, ver, super);
        break;
    default:
        return URE_P_CHIP;
    }
    if (r == 0 && io->errores) return URE_P_CONTROL;   /* una transferencia falló: no hay chip que dar por bueno */
    return r;
}

/* MAC del chip: el RTL8152 de versión 4c00 la guarda en PLA_IDR; los demás, en PLA_BACKUP (como OpenBSD). */
static inline void ure_ler_mac(ure_io_t *io, int chip, uint16_t ver, uint8_t mac[6])
{
    uint8_t t[8] = { 0 };
    ure_lee(io, (chip == URE_CH_8152 && ver == 0x4c00) ? URE_PLA_IDR : URE_PLA_BACKUP, URE_MCU_TYPE_PLA, t, 8);
    for (int i = 0; i < 6; i++) mac[i] = t[i];
}

/* ---------------------------------------------------------- arranque de la interfaz (init) */

/* ure_rxvlan de OpenBSD con la etiqueta VLAN por hardware apagada: la pila no lee la VLAN de la cabecera
 * de recepción, así que el chip tiene que dejar la etiqueta dentro de la trama (y la pila la descarta). */
static inline void ure_rxvlan_apagar(ure_io_t *io, int chip)
{
    if (ure_es_815x(chip)) ure_clr2(io, URE_PLA_RCR1, URE_MCU_TYPE_PLA, URE_INNER_VLAN | URE_OUTER_VLAN);
    else ure_clr2(io, URE_PLA_CPCR, URE_MCU_TYPE_PLA, URE_CPCR_RX_VLAN);
}

/* Reinicio del controlador para la interfaz (ure_rtl8152_nic_reset / ure_rtl8153_nic_reset). */
static inline int ure_nic_reinicio(ure_io_t *io, int chip, uint16_t ver, bool super, bool alta)
{
    uint32_t reg;
    uint8_t u1u2[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    if (chip == URE_CH_8152) {
        ure_ocp_escribe(io, URE_OCP_ALDPS_CONFIG, URE_ENPDNPS | URE_LINKENA | URE_DIS_SDSAVE);
        io->espera_ms(io->ctx, 20);
        ure_clr4(io, URE_PLA_RCR, URE_MCU_TYPE_PLA, URE_RCR_ACPT_ALL);
        ure_set2(io, URE_PLA_MISC_1, URE_MCU_TYPE_PLA, URE_RXDY_GATED_EN);
        ure_teredo(io, chip);
        ure_w1(io, URE_PLA_CRWECR, URE_MCU_TYPE_PLA, URE_CRWECR_NORAML);
        ure_w1(io, URE_PLA_CR, URE_MCU_TYPE_PLA, 0);
        ure_clr1(io, URE_PLA_OOB_CTRL, URE_MCU_TYPE_PLA, URE_NOW_IS_OOB);
        ure_clr2(io, URE_PLA_SFF_STS_7, URE_MCU_TYPE_PLA, URE_MCU_BORW_EN);
        if (ure_oob_listo(io) < 0) return URE_P_OOB;
        ure_set2(io, URE_PLA_SFF_STS_7, URE_MCU_TYPE_PLA, URE_RE_INIT_LL);
        if (ure_oob_listo(io) < 0) return URE_P_OOB;
        if (ure_reinicio(io, chip) < 0) return URE_P_REINICIO;
        ure_w4(io, URE_PLA_RXFIFO_CTRL0, URE_MCU_TYPE_PLA, URE_RXFIFO_THR1_NORMAL);
        /* umbrales del FIFO de recepción: los «completos» a velocidad completa (USB 1.1), los altos si no */
        ure_w4(io, URE_PLA_RXFIFO_CTRL1, URE_MCU_TYPE_PLA, alta ? URE_RXFIFO_THR2_HIGH : URE_RXFIFO_THR2_FULL);
        ure_w4(io, URE_PLA_RXFIFO_CTRL2, URE_MCU_TYPE_PLA, alta ? URE_RXFIFO_THR3_HIGH : URE_RXFIFO_THR3_FULL);
        ure_w4(io, URE_PLA_TXFIFO_CTRL, URE_MCU_TYPE_PLA, URE_TXFIFO_THR_NORMAL);
        ure_w1(io, URE_USB_TX_AGG, URE_MCU_TYPE_USB, URE_TX_AGG_MAX_THRESHOLD);
        ure_w4(io, URE_USB_RX_BUF_TH, URE_MCU_TYPE_USB, URE_RX_THR_HIGH);
        ure_w4(io, URE_USB_TX_DMA, URE_MCU_TYPE_USB, URE_TEST_MODE_DISABLE | URE_TX_SIZE_ADJUST1);
        ure_rxvlan_apagar(io, chip);
        ure_w2(io, URE_PLA_RMS, URE_MCU_TYPE_PLA, ETHER_MAX_LEN + ETHER_VLAN_ENCAP_LEN);
        ure_set2(io, URE_PLA_TCR0, URE_MCU_TYPE_PLA, URE_TCR0_AUTO_FIFO);
        ure_ocp_escribe(io, URE_OCP_ALDPS_CONFIG, URE_ENPWRSAVE | URE_ENPDNPS | URE_LINKENA | URE_DIS_SDSAVE);
        return 0;
    }

    /* RTL8153, 8153B, 8156 y 8156B */
    if (chip == URE_CH_8153) {
        ure_escribe(io, URE_USB_TOLERANCE, URE_BYTE_EN_SIX_BYTES, u1u2, 8);
    } else {
        ure_clr2(io, URE_USB_LPM_CONFIG, URE_MCU_TYPE_USB, LPM_U1U2_EN);
    }
    ure_clr2(io, URE_USB_U2P3_CTRL, URE_MCU_TYPE_USB, URE_U2P3_ENABLE);
    ure_ocp_escribe(io, URE_OCP_POWER_CFG, ure_ocp_lee(io, URE_OCP_POWER_CFG) & ~URE_EN_ALDPS);
    for (int i = 0; i < 20; i++) {
        io->espera_ms(io->ctx, 1);
        if (ure_r2(io, 0xe000, URE_MCU_TYPE_PLA) & 0x0100) break;
    }
    ure_set2(io, URE_PLA_MISC_1, URE_MCU_TYPE_PLA, URE_RXDY_GATED_EN);
    ure_teredo(io, chip);
    ure_clr4(io, URE_PLA_RCR, URE_MCU_TYPE_PLA, URE_RCR_ACPT_ALL);
    ure_reinicio(io, chip);
    ure_reinicio_bmu(io);
    ure_clr1(io, URE_PLA_OOB_CTRL, URE_MCU_TYPE_PLA, URE_NOW_IS_OOB);
    ure_clr2(io, URE_PLA_SFF_STS_7, URE_MCU_TYPE_PLA, URE_MCU_BORW_EN);
    if (!ure_es_815x(chip)) {
        if (ure_oob_listo(io) < 0) return URE_P_OOB;
        ure_set2(io, URE_PLA_SFF_STS_7, URE_MCU_TYPE_PLA, URE_RE_INIT_LL);
        if (ure_oob_listo(io) < 0) return URE_P_OOB;
    }
    ure_rxvlan_apagar(io, chip);
    ure_w2(io, URE_PLA_RMS, URE_MCU_TYPE_PLA, URE_FRAMELEN(URE_MTU));
    ure_w1(io, URE_PLA_MTPS, URE_MCU_TYPE_PLA, MTPS_JUMBO);
    if (ure_es_815x(chip)) {
        ure_w2(io, URE_PLA_RX_FIFO_FULL, URE_MCU_TYPE_PLA, chip == URE_CH_8156 ? 1024 : 512);
        ure_w2(io, URE_PLA_RX_FIFO_EMPTY, URE_MCU_TYPE_PLA, chip == URE_CH_8156 ? 2048 : 1024);
        ure_w2(io, URE_PLA_TXFIFO_CTRL, URE_MCU_TYPE_PLA, 8);
        ure_w2(io, URE_PLA_TXFIFO_FULL, URE_MCU_TYPE_PLA, 128);
        if (chip == URE_CH_8156) ure_set2(io, URE_USB_BMU_CONFIG, URE_MCU_TYPE_USB, URE_ACT_ODMA);
        reg = ure_r2(io, URE_PLA_RXFIFO_FULL, URE_MCU_TYPE_PLA);
        reg &= ~URE_RXFIFO_FULL_MASK;
        ure_w2(io, URE_PLA_RXFIFO_FULL, URE_MCU_TYPE_PLA, reg | 0x0008);
        ure_clr2(io, URE_PLA_MAC_PWR_CTRL3, URE_MCU_TYPE_PLA, URE_PLA_MCU_SPDWN_EN);
        ure_clr2(io, URE_USB_SPEED_OPTION, URE_MCU_TYPE_USB, URE_RG_PWRDN_EN | URE_ALL_SPEED_OFF);
        ure_w4(io, URE_USB_RX_BUF_TH, URE_MCU_TYPE_USB, 0x00600400);
    } else {
        if (chip == URE_CH_8153B) {
            ure_set2(io, URE_PLA_TCR0, URE_MCU_TYPE_PLA, URE_TCR0_AUTO_FIFO);
            ure_reinicio(io, chip);
            ure_w4(io, URE_PLA_RXFIFO_CTRL0, URE_MCU_TYPE_PLA, URE_RXFIFO_THR1_NORMAL);
            ure_w2(io, URE_PLA_RXFIFO_CTRL1, URE_MCU_TYPE_PLA, URE_RXFIFO_THR2_NORMAL);
            ure_w2(io, URE_PLA_RXFIFO_CTRL2, URE_MCU_TYPE_PLA, URE_RXFIFO_THR3_NORMAL);
            ure_w4(io, URE_PLA_TXFIFO_CTRL, URE_MCU_TYPE_PLA, URE_TXFIFO_THR_NORMAL2);
            ure_w4(io, URE_USB_RX_BUF_TH, URE_MCU_TYPE_USB, URE_RX_THR_B);
            ure_clr2(io, URE_PLA_MAC_PWR_CTRL3, URE_MCU_TYPE_PLA, URE_PLA_MCU_SPDWN_EN);
        } else {
            ure_set2(io, URE_PLA_TCR0, URE_MCU_TYPE_PLA, URE_TCR0_AUTO_FIFO);
            ure_reinicio(io, chip);
            ure_w4(io, URE_PLA_RXFIFO_CTRL0, URE_MCU_TYPE_PLA, URE_RXFIFO_THR1_NORMAL);
            ure_w2(io, URE_PLA_RXFIFO_CTRL1, URE_MCU_TYPE_PLA, URE_RXFIFO_THR2_NORMAL);
            ure_w2(io, URE_PLA_RXFIFO_CTRL2, URE_MCU_TYPE_PLA, URE_RXFIFO_THR3_NORMAL);
            ure_w4(io, URE_PLA_TXFIFO_CTRL, URE_MCU_TYPE_PLA, URE_TXFIFO_THR_NORMAL2);
            ure_set1(io, URE_PLA_CONFIG6, URE_MCU_TYPE_PLA, URE_LANWAKE_CLR_EN);
            ure_clr1(io, URE_PLA_LWAKE_CTRL_REG, URE_MCU_TYPE_PLA, URE_LANWAKE_PIN);
            ure_clr2(io, URE_USB_SSPHYLINK1, URE_MCU_TYPE_USB, URE_DELAY_PHY_PWR_CHG);
        }
    }
    ure_ocp_escribe(io, URE_OCP_POWER_CFG, ure_ocp_lee(io, URE_OCP_POWER_CFG) | URE_EN_ALDPS);
    if (ver == 0x5c20 || ver == 0x5c30 || ure_es_815x(chip)) ure_set2(io, URE_USB_U2P3_CTRL, URE_MCU_TYPE_USB, URE_U2P3_ENABLE);
    if (ure_es_b(chip)) {
        if (super) ure_set2(io, URE_USB_LPM_CONFIG, URE_MCU_TYPE_USB, LPM_U1U2_EN);
    } else {
        uint8_t u[8];
        memset(u, 0xff, sizeof u);
        ure_escribe(io, URE_USB_TOLERANCE, URE_BYTE_EN_SIX_BYTES, u, 8);
    }
    return 0;
}

/* Configuración de la MAC y del modo de recepción/envío (ure_ifmedia_init, ure_iff y FMC/CR). */
static inline void ure_configurar_mac(ure_io_t *io, int chip, uint16_t ver, const uint8_t mac[6], uint32_t rxbuf, bool super, bool alta)
{
    uint8_t m8[8] = { mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], 0, 0 };
    uint32_t reg;
    ure_w1(io, URE_PLA_CRWECR, URE_MCU_TYPE_PLA, URE_CRWECR_CONFIG);
    ure_escribe(io, URE_PLA_IDR, URE_MCU_TYPE_PLA | URE_BYTE_EN_SIX_BYTES, m8, 8);
    ure_w1(io, URE_PLA_CRWECR, URE_MCU_TYPE_PLA, URE_CRWECR_NORAML);
    if (chip != URE_CH_8152) {
        if (chip == URE_CH_8156B) ure_clr2(io, URE_USB_RX_AGGR_NUM, URE_MCU_TYPE_USB, URE_RX_AGGR_NUM_MASK);
        reg = rxbuf - URE_FRAMELEN(URE_MTU) - (URE_RX_CABECERA + URE_RX_BUF_ALIGN_V);
        if (ure_es_b(chip)) {
            ure_w2(io, URE_USB_RX_EARLY_SIZE, URE_MCU_TYPE_USB, reg / URE_RX_BUF_ALIGN_V);
            ure_w2(io, URE_USB_RX_EARLY_AGG, URE_MCU_TYPE_USB, chip == URE_CH_8153B ? 158 : 80);
            ure_w2(io, URE_USB_PM_CTRL_STATUS, URE_MCU_TYPE_USB, 1875);
            if (alta) {
                reg = ure_r2(io, URE_USB_L1_CTRL, URE_MCU_TYPE_USB);
                reg &= ~0x0f;
                ure_w2(io, URE_USB_L1_CTRL, URE_MCU_TYPE_USB, reg | 0x01);
            }
            if (ver == 0x6010 || chip == URE_CH_8156B) {
                ure_clr2(io, URE_USB_FW_TASK, URE_MCU_TYPE_USB, URE_FC_PATCH_TASK);
                io->espera_ms(io->ctx, 1);
                ure_set2(io, URE_USB_FW_TASK, URE_MCU_TYPE_USB, URE_FC_PATCH_TASK);
            }
        } else {
            ure_w2(io, URE_USB_RX_EARLY_SIZE, URE_MCU_TYPE_USB, reg / 4);
            reg = super ? URE_COALESCE_SUPER / 8 : alta ? URE_COALESCE_HIGH / 8 : URE_COALESCE_SLOW / 8;
            ure_w2(io, URE_USB_RX_EARLY_AGG, URE_MCU_TYPE_USB, reg);
        }
    }
    /* reinicio del filtro de paquetes y activación de transmisión y recepción */
    ure_clr2(io, URE_PLA_FMC, URE_MCU_TYPE_PLA, URE_FMC_FCR_MCU_EN);
    ure_set2(io, URE_PLA_FMC, URE_MCU_TYPE_PLA, URE_FMC_FCR_MCU_EN);
    ure_set1(io, URE_PLA_CR, URE_MCU_TYPE_PLA, URE_CR_RE | URE_CR_TE);
    if (ure_es_b(chip)) ure_w1(io, URE_USB_UPT_RXDMA_OWN, URE_MCU_TYPE_USB, URE_OWN_UPDATE | URE_OWN_CLEAR);
    ure_clr2(io, URE_PLA_MISC_1, URE_MCU_TYPE_PLA, URE_RXDY_GATED_EN);
}

/* Filtro: acepta lo nuestro, la difusión y todo el multicast (la pila filtra). */
static inline void ure_filtro(ure_io_t *io)
{
    uint32_t rx = ure_r4(io, URE_PLA_RCR, URE_MCU_TYPE_PLA);
    uint8_t hash[8] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
    rx &= ~URE_RCR_ACPT_ALL;
    rx |= URE_RCR_APM | URE_RCR_AB | URE_RCR_AM;
    ure_escribe(io, URE_PLA_MAR, URE_MCU_TYPE_PLA | URE_BYTE_EN_DWORD, hash, 8);
    ure_w4(io, URE_PLA_RCR, URE_MCU_TYPE_PLA, rx);
}

/* Anuncio de velocidades (ure_ifmedia_upd, automático). 8152: hasta 100 Mb/s (sin 1000).
 * 8156 y 8156B: además 2500 Mb/s por el registro 0xa5d4. */
static inline int ure_anuncio(ure_io_t *io, int chip)
{
    uint16_t anar = ANAR_TX_FD | ANAR_TX | ANAR_10_FD | ANAR_10;
    uint16_t gig = (chip == URE_CH_8152) ? 0 : (GTCR_ADV_1000TFDX | GTCR_ADV_1000THDX);
    if (ure_es_815x(chip)) {
        uint16_t c = ure_ocp_lee(io, URE_OCP_10GBT_CTRL);
        if (c == 0xffff) return URE_P_ANUNCIO;
        c |= URE_ADV_2500TFDX;
        ure_ocp_escribe(io, URE_OCP_BASE_MII + MII_ANAR * 2, anar | ANAR_PAUSE_ASYM | ANAR_FC);
        ure_mii_escribe(io, MII_100T2CR, gig);
        ure_ocp_escribe(io, URE_OCP_10GBT_CTRL, c);
    } else {
        ure_mii_escribe(io, MII_ANAR, anar | ANAR_PAUSE_ASYM | ANAR_FC);
        ure_mii_escribe(io, MII_100T2CR, gig);
    }
    ure_mii_escribe(io, MII_BMCR, BMCR_AUTOEN | BMCR_STARTNEG);
    return 0;
}

/* Arranque de la interfaz (ure_init): reinicio del controlador, MAC, filtro y anuncio. */
static inline int ure_arrancar(ure_io_t *io, int chip, uint16_t ver, bool super, bool alta, const uint8_t mac[6],
                        uint32_t rxbuf, int *paso)
{
    int r = ure_nic_reinicio(io, chip, ver, super, alta);
    if (r) { *paso = r; return -1; }
    ure_configurar_mac(io, chip, ver, mac, rxbuf, super, alta);
    ure_filtro(io);
    r = ure_anuncio(io, chip);
    if (r) { *paso = r; return -1; }
    if (io->errores) { *paso = URE_P_CONTROL; return -1; }
    *paso = URE_P_FIN;
    return 0;
}

/* ---------------------------------------------------------- enlace */

/* Al subir el enlace, OpenBSD (ure_link_state) vuelve a activar la transmisión y la recepción. */
static inline void ure_enlace_arriba(ure_io_t *io)
{
    ure_set1(io, URE_PLA_CR, URE_MCU_TYPE_PLA, URE_CR_RE | URE_CR_TE);
}

/* Velocidad (Mb/s) y dúplex del PLA_PHYSTATUS; 0 = sin enlace. */
static inline int ure_velocidade(uint32_t ps, bool *fdx)
{
    *fdx = (ps & URE_PHYSTATUS_FDX) != 0;
    if (!(ps & URE_PHYSTATUS_LINK)) return 0;
    if (ps & URE_PHYSTATUS_2500MBPS) return 2500;
    if (ps & URE_PHYSTATUS_1000MBPS) return 1000;
    if (ps & URE_PHYSTATUS_100MBPS) return 100;
    if (ps & URE_PHYSTATUS_10MBPS) return 10;
    return 0;
}

/* ---------------------------------------------------------- recepción y envío */

/* Lee la siguiente trama de la transferencia agregada. Cada trama va precedida por una
 * cabecera de 12 bytes (la longitud en los 15 bits bajos, con el CRC incluido) y ocupa
 * un múltiplo de 8 bytes. Devuelve los bytes copiados a dst (sin CRC), 0 si no quedan
 * tramas, o un URE_RX_E_* (la transferencia sigue más allá del error). */
static inline int ure_rx_siguiente(ure_rx_t *r, uint8_t *dst, uint32_t max)
{
    for (;;) {
        if (r->pos >= r->n) return 0;
        if (r->n - r->pos < URE_RX_CABECERA) { r->pos = r->n; return URE_RX_E_CORTA; }
        const uint8_t *c = r->buf + r->pos;
        uint32_t pl = ((uint32_t)c[0] | (uint32_t)c[1] << 8 | (uint32_t)c[2] << 16 | (uint32_t)c[3] << 24) & URE_RXPKT_LEN_MASK;
        uint32_t ini = r->pos + URE_RX_CABECERA;
        uint32_t quedan = r->n - ini;
        /* el siguiente registro empieza tras la trama, alineado a 8; si no cabe, se acaba aquí */
        if (pl > quedan) { r->pos = r->n; return URE_RX_E_LARGA; }
        uint32_t sig = ini + ure_alinea(pl, URE_RX_BUF_ALIGN_V);
        r->pos = sig > r->n ? r->n : sig;
        if (pl < ETHER_HDR_LEN + ETHER_CRC_LEN) return URE_RX_E_PEQUENA;
        uint32_t f = pl - ETHER_CRC_LEN;
        if (f > max) return URE_RX_E_MAYOR;
        for (uint32_t i = 0; i < f; i++) dst[i] = r->buf[ini + i];
        return (int)f;
    }
}

/* Monta una trama de envío: cabecera de 8 bytes (longitud con los bits FS y LS, vlan = 0)
 * más la trama, rellenada a 60 bytes. Devuelve el total, o 0 si no cabe o es demasiado grande. */
static inline uint32_t ure_tx_montar(uint8_t *out, uint32_t max, const uint8_t *q, uint32_t n)
{
    uint32_t f = n < URE_TRAMA_MIN ? URE_TRAMA_MIN : n;
    if (n == 0 || n > URE_TRAMA_MAX || URE_TX_CABECERA + f > max) return 0;
    uint32_t pl = f | URE_TXPKT_TX_FS | URE_TXPKT_TX_LS;
    out[0] = (uint8_t)pl; out[1] = (uint8_t)(pl >> 8); out[2] = (uint8_t)(pl >> 16); out[3] = (uint8_t)(pl >> 24);
    out[4] = out[5] = out[6] = out[7] = 0;
    for (uint32_t i = 0; i < n; i++) out[URE_TX_CABECERA + i] = q[i];
    for (uint32_t i = n; i < f; i++) out[URE_TX_CABECERA + i] = 0;
    return URE_TX_CABECERA + f;
}

/* OpenBSD y FreeBSD mandan cada envío con un paquete corto al final (USBD_FORCE_SHORT_XFER); usb_bulk no
 * sabe mandar el paquete de longitud cero, así que, si el total cae justo en un múltiplo del tamaño de
 * paquete, la trama crece un byte (un cero más de relleno: la longitud de la cabecera cambia con ella y
 * el receptor lo ignora, como el relleno hasta 60). Devuelve el total nuevo. */
static inline uint32_t ure_tx_evitar_mps(uint8_t *out, uint32_t max, uint32_t t, uint32_t mps)
{
    if (!mps || t < URE_TX_CABECERA || (t % mps) != 0 || t + 1 > max) return t;
    uint32_t pl = (uint32_t)out[0] | (uint32_t)out[1] << 8 | (uint32_t)out[2] << 16 | (uint32_t)out[3] << 24;
    uint32_t f = pl & URE_TXPKT_LEN_MASK;
    if (f + 1 > URE_TRAMA_MAX || URE_TX_CABECERA + f != t) return t;
    pl = (pl & ~(uint32_t)URE_TXPKT_LEN_MASK) | (f + 1);
    out[0] = (uint8_t)pl; out[1] = (uint8_t)(pl >> 8); out[2] = (uint8_t)(pl >> 16); out[3] = (uint8_t)(pl >> 24);
    out[t] = 0;
    return t + 1;
}

#endif
