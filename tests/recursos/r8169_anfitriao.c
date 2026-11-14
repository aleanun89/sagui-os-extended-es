// SPDX-License-Identifier: GPL-2.0-or-later
/* r8169_anfitriao.c - testes no anfitrião da lógica do driver r8169 (D091):
 * a tabela de versões pelo XID, o perfil de inicialização, o RxConfig de
 * cada versão e a decodificação do descritor de recepção. Compilado e
 * rodado por tests/h70_realtek_moderno.py; imprime "ok ..." ou "FALHA ...". */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "../../kernel/rede/r8169_logica.h"

/* O r8169_inicio.h espera os acessos aos registradores: aqui vão para uma
 * placa de mentira, com o ERI (0x70/0x74), o MAC OCP (0xb0) e o PHY OCP
 * (0xb8) respondendo na hora, para conferir o que as sequências escrevem. */
static int rtl_versao;
static uint8_t regs[256];
static uint32_t eri[0x800];
static uint16_t mac_ocp[0x10000];

static uint32_t r32(uint16_t r) { return (uint32_t)regs[r] | (uint32_t)regs[r + 1] << 8 | (uint32_t)regs[r + 2] << 16 | (uint32_t)regs[r + 3] << 24; }
static void w32(uint16_t r, uint32_t v) { for (int i = 0; i < 4; i++) regs[r + i] = (uint8_t)(v >> (8 * i)); }
static uint8_t rtl_r8(uint16_t r)
{
    if (r == 0xd3) regs[r] |= 0x32;      /* MCU: LINK_LIST_RDY e RXTX_EMPTY */
    return regs[r];
}
static __attribute__((unused)) uint16_t rtl_r16(uint16_t r) { return (uint16_t)(regs[r] | regs[r + 1] << 8); }
static uint32_t rtl_r32(uint16_t r)
{
    if (r == 0x40) return r32(r) | (1u << 11);   /* TXCFG_EMPTY */
    return r32(r);
}
static void rtl_w8(uint16_t r, uint8_t v) { regs[r] = v; }
static __attribute__((unused)) void rtl_w16(uint16_t r, uint16_t v) { regs[r] = (uint8_t)v; regs[r + 1] = (uint8_t)(v >> 8); }
static void rtl_w32(uint16_t r, uint32_t v)
{
    if (r == 0x74) {                         /* ERIAR */
        uint32_t end = v & 0xfff, mascara = (v >> 12) & 0xf, m = 0;
        for (int i = 0; i < 4; i++) if (mascara & (1u << i)) m |= 0xffu << (8 * i);
        if (v & 0x80000000u) { eri[end] = (eri[end] & ~m) | (r32(0x70) & m); w32(0x74, v & ~0x80000000u); }
        else { w32(0x70, eri[end]); w32(0x74, v | 0x80000000u); }
        return;
    }
    if (r == 0xb0) {                         /* OCPDR: MAC OCP */
        uint32_t reg = (v >> 15) & 0xfffe;   /* bits 16..30 = reg / 2 */
        if (v & 0x80000000u) mac_ocp[reg] = (uint16_t)v;
        else w32(0xb0, (v & ~0xffffu) | mac_ocp[reg]);
        return;
    }
    if (r == 0xb8) { w32(0xb8, (v & 0x80000000u) ? v & ~0x80000000u : (v | 0x80000000u) & 0xffff0000u); return; }
    w32(r, v);
}
static void rtl_espera_us(uint32_t us) { (void)us; }
static void sagui_pcie_desliga_clkreq(void) {}
/* D124: las secuencias de las 8168e se prueban en h96 (r8169_8168e_anfitriao.c) */
#pragma GCC diagnostic ignored "-Wunused-function"
#include "../../kernel/terceros/linux/r8169/r8169_inicio.h"

static int falhas;

static void confere(bool c, const char *o_que)
{
    printf("%s %s\n", c ? "ok" : "FALHA", o_que);
    if (!c) falhas++;
}

int main(void)
{
    /* Tabela de versões: XID nos bits 20..31 de TxConfig (máscara 0xfcf). */
    static const struct { uint32_t xid; int ver; const char *nome; } casos[] = {
        { 0x541, 46, "RTL8168h/8111h" },   /* a do ASUS B450 (provável) */
        { 0x4c0, 40, "RTL8168g/8111g" },
        { 0x509, 42, "RTL8168gu/8111gu" },
        { 0x5c8, 44, "RTL8411b" },
        { 0x502, 51, "RTL8168ep/8111ep" },
        { 0x480, 35, "RTL8168f/8111f" },
        { 0x2c8, 34, "RTL8168evl/8111evl" },
        { 0x2c1, 32, "RTL8168e/8111e" },
        { 0x281, 25, "RTL8168d/8111d" },
        { 0x28b, 31, "RTL8168dp/8111dp" },
        { 0x3c0, 19, "RTL8168c/8111c" },
        { 0x3c4, 22, "RTL8168c/8111c" },   /* máscara 0x7c8 */
        { 0x3c8, 18, "RTL8168cp/8111cp" },
        { 0x380, 17, "RTL8168b/8111b" },
        { 0x384, 17, "RTL8168b/8111b" },
        { 0x300, 11, "RTL8168b/8111b" },
        { 0x409, 29, "RTL8105e" },
        { 0x340, 10, "RTL8101e/RTL8100e" },
        { 0x008, 2, "RTL8169s" },
        { 0x000, 0, "desconocido" },
        { 0x7cf, 0, "desconocido" },
    };
    char t[160];
    for (unsigned i = 0; i < sizeof casos / sizeof casos[0]; i++) {
        uint32_t xid = 0;
        /* bits fora do XID ligados (0x00f00000 é o 0x030 do meio, que a máscara ignora) */
        uint32_t txcfg = casos[i].xid << 20 | 0x00030F80u;
        int v = r8169_versao_do_txconfig(txcfg, &xid);
        snprintf(t, sizeof t, "XID %03x -> versao %d (%s), esperado %d (%s)", casos[i].xid, v, r8169_nome_da_versao(v),
                 casos[i].ver, casos[i].nome);
        const char *nome = r8169_nome_da_versao(v);
        bool igual = v == casos[i].ver && xid == (casos[i].xid & 0xfcf);
        for (int k = 0; igual && (nome[k] || casos[i].nome[k]); k++) igual = nome[k] == casos[i].nome[k];
        confere(igual, t);
    }
    /* Perfil: só 40..48 no moderno; 32..34 no 8168e (D124, ver h96); o resto (inclusive o dv7) no legado. */
    confere(r8169_perfil_da_versao(46) == PERFIL_MODERNO, "8111h no perfil moderno");
    confere(r8169_perfil_da_versao(40) == PERFIL_MODERNO, "8111g no perfil moderno");
    confere(r8169_perfil_da_versao(48) == PERFIL_MODERNO, "8107e no perfil moderno");
    confere(r8169_perfil_da_versao(51) == PERFIL_LEGADO, "8168ep no legado (DASH)");
    for (int v = 0; v < 40; v++)
        if ((v < 32 || v > 34) && r8169_perfil_da_versao(v) != PERFIL_LEGADO) { confere(false, "versões < 40 no legado"); break; }
    confere(r8169_perfil_da_versao(0) == PERFIL_LEGADO, "versão desconhecida no legado");

    /* RxConfig por versão (rtl_init_rxcfg) */
    rtl_versao = 46; confere(rtl_init_rxcfg() == 0xCF00, "RxConfig da 8111h = 0xCF00 (RX128, MULTI, EARLY_OFF, DMA)");
    rtl_versao = 40; confere(rtl_init_rxcfg() == 0xCF00, "RxConfig da 8111g = 0xCF00");
    rtl_versao = 19; confere(rtl_init_rxcfg() == 0xC700, "RxConfig da 8111c = 0xC700");
    rtl_versao = 11; confere((rtl_init_rxcfg() | 0x0E) == 0xE70E, "RxConfig da 8168b = o legado 0xE70E");
    rtl_versao = 25; confere(rtl_init_rxcfg() == 0x8700, "RxConfig da 8111d = 0x8700");

    /* Sequências do perfil moderno numa 8111h que a BIOS deixou fora de
     * banda, com a recepção bloqueada e o ASPM ligado no chip. */
    rtl_versao = 46;
    w32(0xf0, 1u << 19);                 /* MISC: RXDV_GATED_EN */
    regs[0xd3] = 0x80;                   /* MCU: NOW_IS_OOB */
    regs[0x53] = 0x80;                   /* Config2: ClkReqEn */
    regs[0x54] = 0x02;                   /* Config3: Rdy_to_L23 */
    regs[0x56] = 0x01;                   /* Config5: ASPM_en */
    regs[0x37] = 0x0c;
    mac_ocp[0xe8de] = 1u << 14;
    eri[0xe0] = 0xa376b040u; eri[0xe4] = 0x51d7;   /* 40:b0:76:a3:d7:51 */
    rtl_hw_init_8168g();
    confere(!(regs[0xd3] & 0x80), "hw_init_8168g tira o modo fora de banda");
    confere((regs[0x37] & 0x0c) == 0, "hw_init_8168g desliga RX/TX antes do reset");
    confere(mac_ocp[0xe8de] == (1u << 15), "MAC OCP 0xe8de: bit 14 limpo, bit 15 ligado");
    uint8_t mac[6];
    confere(rtl_read_mac_address(mac) && mac[0] == 0x40 && mac[1] == 0xb0 && mac[2] == 0x76 && mac[3] == 0xa3 &&
            mac[4] == 0xd7 && mac[5] == 0x51, "MAC lida do eFuse (ERI 0xe0/0xe4)");
    rtl_rar_set(mac);
    confere(r32(0) == 0xa376b040u && (r32(4) & 0xffff) == 0x51d7 && regs[0x50] == 0, "MAC escrita no IDR, com trava");
    rtl_hw_aspm_clkreq_disable();
    confere(!(regs[0x53] & 0x80) && !(regs[0x56] & 1), "ASPM e ClkReq desligados no chip");
    rtl_hw_start_8168h_1();
    confere(!(r32(0xf0) & (1u << 19)), "hw_start_8168h_1 desbloqueia a recepção (RXDV_GATED_EN)");
    confere(eri[0xc8] == 0x00080002 && eri[0xe8] == 0x00100006, "tamanhos de FIFO (ERI 0xc8/0xe8)");
    confere((eri[0xcc] & 0xff) == 0x38 && (eri[0xd0] & 0xff) == 0x48, "limiares de pausa");
    confere((eri[0xdc] & 0x1d) == 0x1d, "filtro de pacotes reiniciado e ERI 0xdc com 0x1c");
    confere((eri[0x5f0] & 0xffff) == 0x4f87, "ERI 0x5f0 = 0x4f87");
    confere(!(regs[0x54] & 2), "Rdy_to_L23 desligado");
    confere((mac_ocp[0xe056] & 0xf0) == 0x70 && (mac_ocp[0xe052] & 0x6000) == 0 && (mac_ocp[0xe052] & 0x8008) == 0x8008 &&
            (mac_ocp[0xd420] & 0xfff) == 0x47f && mac_ocp[0xe63e] == 0, "MAC OCP da 8111h");
    rtl_versao = 40;
    w32(0xf0, 1u << 19);
    rtl_hw_start_8168g();
    confere(!(r32(0xf0) & (1u << 19)) && (eri[0x2f8] & 0xffff) == 0x1d8f, "hw_start_8168g desbloqueia a recepção");
    rtl_versao = 25;
    confere(!rtl_read_mac_address(mac), "8111d não lê a MAC do ERI");

    /* Descritor de recepção: o tamanho inclui o CRC. */
    int n;
    confere(r8169_classificar(R8169_FS | R8169_LS | 64, &n) == RX_OK && n == 60, "quadro mínimo: 64 - CRC = 60");
    confere(r8169_classificar(R8169_FS | R8169_LS | 1518, &n) == RX_OK && n == 1514, "quadro máximo: 1518 - CRC = 1514");
    confere(r8169_classificar(R8169_FS | R8169_LS | 1522, &n) == RX_LONGO, "quadro com VLAN de 1522 é longo demais");
    confere(r8169_classificar(R8169_FS | R8169_LS | 342 | (3u << 17), &n) == RX_OK && n == 338,
            "bits de protocolo (PID) não mudam o tamanho");
    confere(r8169_classificar(R8169_FS | 2048, &n) == RX_FRAGMENTADO, "sem LS: fragmentado");
    confere(r8169_classificar(R8169_LS | 100, &n) == RX_FRAGMENTADO, "sem FS: fragmentado");
    confere(r8169_classificar(R8169_FS | R8169_LS | R8169_RxRES | R8169_RxCRC | 100, &n) == RX_ERRO, "RxRES: erro");
    confere(r8169_classificar(R8169_FS | R8169_LS | 17, &n) == RX_CURTO, "13 bytes: curto");
    confere(r8169_classificar(R8169_FS | R8169_LS | R8169_EOR | 64, &n) == RX_OK && n == 60, "EOR não muda nada");

    printf(falhas ? "FALHAS: %d\n" : "TUDO OK\n", falhas);
    return falhas != 0;
}
