// SPDX-License-Identifier: GPL-2.0-or-later
/* r8169_8168e_anfitriao.c - pruebas en el anfitrión de la elección del
 * arranque por versión del r8169 y de la secuencia de la familia
 * RTL8168E/8168E-VL (D124). El QEMU no emula estas tarjetas: aquí las
 * secuencias de kernel/terceros/linux/r8169/r8169_inicio.h corren contra una
 * tarjeta de mentira (registros, ERI, EPHY) y se comprueba lo que escriben.
 * Compilado y ejecutado por tests/h96_realtek_8168e.py; imprime "ok ..." o
 * "FALHA ..." y al final "TUDO OK". */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../../kernel/rede/r8169_logica.h"
#include "../../kernel/inc/abi.h"
#include "../../kernel/rede/r8169_ajustes.h"

static int rtl_versao;
static uint8_t regs[256];
static uint32_t eri[0x800];
static uint16_t ephy[32];
static int ephy_escritas, clkreq_apagado;

static uint32_t r32(uint16_t r) { return (uint32_t)regs[r] | (uint32_t)regs[r + 1] << 8 | (uint32_t)regs[r + 2] << 16 | (uint32_t)regs[r + 3] << 24; }
static void w32(uint16_t r, uint32_t v) { for (int i = 0; i < 4; i++) regs[r + i] = (uint8_t)(v >> (8 * i)); }
static uint8_t rtl_r8(uint16_t r) { return regs[r]; }
static uint16_t rtl_r16(uint16_t r) { return (uint16_t)(regs[r] | regs[r + 1] << 8); }
static uint32_t rtl_r32(uint16_t r) { return r32(r); }
static void rtl_w8(uint16_t r, uint8_t v) { regs[r] = v; }
static void rtl_w16(uint16_t r, uint16_t v) { regs[r] = (uint8_t)v; regs[r + 1] = (uint8_t)(v >> 8); }
static void rtl_w32(uint16_t r, uint32_t v)
{
    if (r == 0x74) {                         /* ERIAR */
        uint32_t end = v & 0xfff, mascara = (v >> 12) & 0xf, m = 0;
        for (int i = 0; i < 4; i++) if (mascara & (1u << i)) m |= 0xffu << (8 * i);
        if (v & 0x80000000u) { eri[end] = (eri[end] & ~m) | (r32(0x70) & m); w32(0x74, v & ~0x80000000u); }
        else { w32(0x70, eri[end]); w32(0x74, v | 0x80000000u); }
        return;
    }
    if (r == 0x80) {                         /* EPHYAR: PHY de PCIe */
        uint32_t reg = (v >> 16) & 0x1f;
        if (v & 0x80000000u) { ephy[reg] = (uint16_t)v; ephy_escritas++; w32(0x80, v & ~0x80000000u); }
        else w32(0x80, 0x80000000u | (reg << 16) | ephy[reg]);
        return;
    }
    w32(r, v);
}
static void rtl_espera_us(uint32_t us) { (void)us; }
static void sagui_pcie_desliga_clkreq(void) { clkreq_apagado++; }
#pragma GCC diagnostic ignored "-Wunused-function"
#include "../../kernel/terceros/linux/r8169/r8169_inicio.h"

static int falhas;

static void confere(bool c, const char *o_que)
{
    printf("%s %s\n", c ? "ok" : "FALHA", o_que);
    if (!c) falhas++;
}

static void tarjeta_como_la_bios(int v)
{
    memset(regs, 0, sizeof regs);
    memset(eri, 0, sizeof eri);
    memset(ephy, 0, sizeof ephy);
    ephy_escritas = clkreq_apagado = 0;
    rtl_versao = v;
    regs[0xd3] = 0x80;                   /* MCU: NOW_IS_OOB */
    regs[0x53] = 0x80;                   /* Config2: ClkReqEn */
    regs[0x54] = 0x04;                   /* Config3: Jumbo_En0 */
    regs[0x55] = 0x01;                   /* Config4: Jumbo_En1 */
    regs[0x56] = 0x09;                   /* Config5: Spi_en | ASPM_en */
    regs[0x1b] = 0x07;                   /* EEE_LED */
    w32(0xf0, 0);                        /* MISC */
    ephy[0x0c] = 0x3df0;
    ephy[0x19] = 0xffff;
}

int main(void)
{
    char t[160];

    /* 1. Elección del arranque por versión. */
    static const struct { int v; int perfil; } perfis[] = {
        { 25, PERFIL_LEGADO }, { 26, PERFIL_LEGADO },   /* HP dv7, probado: no cambia */
        { 31, PERFIL_LEGADO }, { 32, PERFIL_8168E }, { 33, PERFIL_8168E }, { 34, PERFIL_8168E },
        { 35, PERFIL_LEGADO }, { 36, PERFIL_LEGADO }, { 38, PERFIL_LEGADO }, { 39, PERFIL_LEGADO },
        { 40, PERFIL_MODERNO }, { 46, PERFIL_MODERNO }, { 48, PERFIL_MODERNO }, { 51, PERFIL_LEGADO },
        { 2, PERFIL_LEGADO }, { 0, PERFIL_LEGADO },
    };
    for (unsigned i = 0; i < sizeof perfis / sizeof perfis[0]; i++) {
        snprintf(t, sizeof t, "versión %d -> perfil %d", perfis[i].v, perfis[i].perfil);
        confere(r8169_perfil_da_versao(perfis[i].v) == perfis[i].perfil, t);
    }
    /* La del Toshiba L850: XID 2c9 -> 34, RTL8168evl/8111evl, inicio 8168e. */
    uint32_t xid = 0;
    int v = r8169_versao_do_txconfig(0x2c9u << 20 | 0x00030F80u, &xid);
    confere(v == 34 && xid == 0x2c9 && strcmp(r8169_nome_da_versao(v), "RTL8168evl/8111evl") == 0 &&
            r8169_perfil_da_versao(v) == PERFIL_8168E, "Toshiba L850: XID 2c9 = versión 34 (8168evl), perfil 8168e");
    v = r8169_versao_do_txconfig(0x2c1u << 20, &xid);
    confere(v == 32 && r8169_perfil_da_versao(v) == PERFIL_8168E, "XID 2c1 = versión 32 (8168e), perfil 8168e");
    v = r8169_versao_do_txconfig(0x2c0u << 20, &xid);
    confere(v == 33 && r8169_perfil_da_versao(v) == PERFIL_8168E, "XID 2c0 = versión 33 (8168e), perfil 8168e");
    confere(strcmp(r8169_nome_driver(0x8168), "r8168") == 0 && strcmp(r8169_nome_driver(0x8136), "rtl810x") == 0 &&
            strcmp(r8169_nome_driver(0x8169), "r8169") == 0, "nombre del driver por modelo PCI (la 8168 ya no es rtl810x)");

    /* 2. RxConfig, TxConfig y CPlusCmd por versión (rtl_init_rxcfg, rtl_set_tx_config_registers). */
    rtl_versao = 34; confere(rtl_init_rxcfg() == 0xC700, "RxConfig de la 34 = 0xC700 (RX128, MULTI, DMA sin límite)");
    rtl_versao = 32; confere(rtl_init_rxcfg() == 0x8700, "RxConfig de la 32 = 0x8700 (RX128, DMA)");
    rtl_versao = 33; confere(rtl_init_rxcfg() == 0x8700, "RxConfig de la 33 = 0x8700");
    rtl_versao = 34; confere(rtl_tx_config() == 0x03000780, "TxConfig de la 34 = IFG 3, DMA 7, AUTO_FIFO");
    rtl_versao = 33; confere(rtl_tx_config() == 0x03000700, "TxConfig de la 33 sin AUTO_FIFO");
    rtl_versao = 34; confere(rtl_is_8168evl_up(), "la 34 es 8168evl en adelante");
    rtl_versao = 33; confere(!rtl_is_8168evl_up(), "la 33 no");
    confere(rtl_cp_cmd(0xffff) == 0x2003, "CPlusCmd: conserva Normal_mode e INTT, sin RxChkSum ni RxVlan");
    confere(rtl_cp_cmd(0x0060) == 0, "CPlusCmd: quita RxChkSum y RxVlan de la BIOS");

    /* 3. La secuencia de la 34 (rtl_hw_start_8168e_2 y lo común). */
    tarjeta_como_la_bios(34);
    rtl_hw_aspm_clkreq_disable();
    confere(!(regs[0x53] & 0x80) && !(regs[0x56] & 1), "34: ASPM y ClkReq apagados en el chip");
    rtl_hw_start_8168e();
    confere(regs[0xec] == 0x27, "34: MaxTxPacketSize = EarlySize");
    confere(ephy[0x09] == 0x0080 && ephy[0x19] == 0xffff && ephy[0x00] == 0x0004 && ephy[0x0c] == 0x0200 &&
            ephy_escritas == 4, "34: EPHY 09/19/00/0c como e_info_8168e_2");
    confere(eri[0xc8] == 0x00100002 && eri[0xe8] == 0x00100006, "34: tamaños de FIFO (ERI 0xc8/0xe8)");
    confere((eri[0x1d0] & 2) && (eri[0x1b0] & 0x13) == 0x13 && eri[0xcc] == 0x50 && eri[0xd0] == 0x07ff0060,
            "34: ERI 0x1d0, 0x1b0 (y EEE), 0xcc, 0xd0");
    confere(eri[0xdc] & 1, "34: filtro de paquetes reiniciado");
    confere(clkreq_apagado == 1, "34: CLKREQ de PCIe apagado");
    confere(!(regs[0xd3] & 0x80), "34: sale del modo fuera de banda");
    confere((regs[0x1b] & 7) == 0, "34: LED de EEE");
    confere((regs[0xd0] & 0x40) && (r32(0xf0) & (1u << 22)), "34: PFM_EN y PWM_EN");
    confere(!(regs[0x56] & 0x08), "34: Spi_en apagado");
    confere(rtl_r16(0xe2) == 0, "34: interrupciones sin agrupar");
    rtl_enable_exit_l1();
    confere((eri[0xd4] & 0x1f00) == 0x1f00, "34: salida de L1 (ERI 0xd4)");
    rtl_jumbo_config_8168e();
    confere(regs[0xec] == 0x27 && (regs[0x54] & 4), "34: el jumbo no se toca (como en Linux)");
    static const uint8_t mac[6] = { 0x70, 0x54, 0xd2, 0x22, 0xc8, 0xc9 };
    rtl_rar_set(mac);
    confere(r32(0) == 0x22d25470u && (r32(4) & 0xffff) == 0xc9c8 && eri[0xe0] == 0x22d25470u && eri[0xe4] == 0xc9c8 &&
            eri[0xf0] == 0x54700000u && eri[0xf4] == 0xc9c822d2u && regs[0x50] == 0,
            "34: MAC en el IDR y en el ERI (rtl_rar_exgmac_set), con traba");

    /* 4. La secuencia de la 32/33 (rtl_hw_start_8168e_1). */
    tarjeta_como_la_bios(33);
    w32(0xf0, 0x12345678u & ~(1u << 29));
    rtl_hw_start_8168e();
    confere(regs[0xec] == (8064 >> 7), "33: MaxTxPacketSize = TxPacketMax");
    confere(ephy_escritas == 13 && ephy[0x19] == 0xfe6c && ephy[0x0a] == 0x0040 && ephy[0x1e] == 0x2000,
            "33: EPHY como e_info_8168e_1 (13 escrituras)");
    confere(clkreq_apagado == 1 && !(r32(0xf0) & (1u << 29)) && r32(0xf0) == (0x12345678u & ~(1u << 29)),
            "33: CLKREQ apagado y TXPLA_RST pulsado sin tocar el resto de MISC");
    confere(!(regs[0x56] & 0x08) && (regs[0xd3] & 0x80), "33: Spi_en apagado (sin tocar MCU, como en Linux)");
    rtl_jumbo_config_8168e();
    confere(regs[0xec] == 0x3f && !(regs[0x54] & 4) && !(regs[0x55] & 1), "33: jumbo apagado (r8168e_hw_jumbo_disable)");
    regs[0x50] = 0xc0;
    rtl_rar_set(mac);
    confere(eri[0xe0] == 0, "33: la MAC no va al ERI");

    /* 4b. Ajustes en caliente (ifconfig eth0 r8169 ...). La 34 arranca sin RX_MULTI_EN. */
    rtl_versao = 34;
    r8169_ajustes_t aj = { (rtl_init_rxcfg() | R8169_ACEITAR) & ~R8169_MULTI, 2048, 2048 };
    confere(aj.rxcfg == 0x870E, "34: RxConfig inicial 0x870E (sin RX_MULTI_EN)");
    ajuste_driver_t pe = { AJUSTE_MULTI, 0, 0, 1, 0 };
    confere(r8169_aplicar_ajuste(&aj, &pe, 16384) == 0 && aj.rxcfg == 0xC70E, "multi=1 vuelve a 0xC70E");
    ajuste_driver_t pr = { AJUSTE_RXCFG | AJUSTE_RMS | AJUSTE_BUF, 0x8700, 16384, 0, 16383 };
    confere(r8169_aplicar_ajuste(&aj, &pr, 16384) == 0 && aj.rxcfg == 0x870E && aj.rms == 16384 && aj.buf == 16383,
            "rxcfg=8700 (con aceptar añadido) rms=16384 buf=16383 (como Linux)");
    ajuste_driver_t malo = { AJUSTE_BUF | AJUSTE_MULTI, 0, 0, 1, 4096 };
    confere(r8169_aplicar_ajuste(&aj, &malo, 2048) == -1 && aj.buf == 16383 && aj.rxcfg == 0x870E,
            "búfer mayor que el paso: rechazado y sin cambios");
    ajuste_driver_t rms0 = { AJUSTE_RMS, 0, 10, 0, 0 };
    confere(r8169_aplicar_ajuste(&aj, &rms0, 2048) == -1, "rms=10 rechazado");

    /* 5. El HP dv7 (25/26) sigue en el clásico y sin estas secuencias. */
    confere(r8169_perfil_da_versao(25) == PERFIL_LEGADO && r8169_perfil_da_versao(26) == PERFIL_LEGADO,
            "8168d del HP dv7 sigue en el inicio clásico");

    printf(falhas ? "FALHAS: %d\n" : "TUDO OK\n", falhas);
    return falhas != 0;
}
