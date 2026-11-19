// SPDX-License-Identifier: GPL-2.0-or-later
/* energia_sim.c - driver de energía de mentira para las pruebas en QEMU (D399, mkimagem.py --gpu-energia-simulada).
 *
 * No toca ningún hardware: una BIOS ATOM de juguete (atom_sintetica.h, escrita a mano) corre en el
 * intérprete de verdad (kernel/terceros/mit/atom/) contra registros en la RAM. Así h401 prueba en QEMU
 * la capa común (modos, gobernador «auto», comprobación, vuelta a la BIOS y bloqueo tras un fallo),
 * el intérprete dentro del kernel y lo que enseñan Tareas, Configuración, acel2d e infopc.
 * --gpu-energia-prueba=fallo: SetEngineClock escribe 20 MHz de más (la comprobación lo ve);
 * --gpu-energia-prueba=cuelgue: SetEngineClock se queda en un bucle (el vigilante del intérprete lo corta). */
#include "gpu_energia.h"
#include "abi.h"
#include "bootinfo.h"
#include "console.h"
#include "info_hardware.h"
#include "memoria.h"
#include "prazo.h"
#include "texto.h"
#include "atom_sintetica.h"
#include "../terceros/mit/atom/atom.h"

static uint8_t *rom;
static uint32_t rom_tam, cmd_off, dados_off;
static uint32_t regs[0x800], pll[64], mcr[64];
static struct card_info card;
static struct atom_context *ctx;
static uint32_t *ps;
static bool ensaio;
static uint32_t escritas;

static uint32_t s_reg_r(struct card_info *c, uint32_t r) { (void)c; return r < 0x800 ? regs[r] : 0; }
static void s_reg_w(struct card_info *c, uint32_t r, uint32_t v)
{
    (void)c; escritas++;
    if (ensaio) { klog("[GPUENERGIA] simulado: ensayo: REG %x <- %x (no se escribe)\n", r * 4, v); return; }
    if (r < 0x800) regs[r] = v;
}
static uint32_t s_pll_r(struct card_info *c, uint32_t r) { (void)c; return pll[r & 63]; }
static void s_pll_w(struct card_info *c, uint32_t r, uint32_t v)
{
    (void)c; escritas++;
    if (ensaio) { klog("[GPUENERGIA] simulado: ensayo: PLL %x <- %x (no se escribe)\n", r & 63, v); return; }
    pll[r & 63] = v;
}
static uint32_t s_mc_r(struct card_info *c, uint32_t r) { (void)c; return mcr[r & 63]; }
static void s_mc_w(struct card_info *c, uint32_t r, uint32_t v) { (void)c; if (!ensaio) mcr[r & 63] = v; }
static void s_atraso(struct card_info *c, uint32_t us) { (void)c; (void)us; }
static unsigned long s_ms(struct card_info *c)
{
    (void)c;
    static unsigned long n;
    uint32_t khz = hw_tsc_khz();
    return khz ? (unsigned long)(prazo_tsc() / khz) : ++n;
}

static bool s_sondar(gpu_en_placa_t *p)
{
    uint8_t opc = cabecalho_boot[CAB_GPU_OPC];
    if (!(opc & CAB_GPU_EN_SIM)) return false;
    int var = (opc & CAB_GPU_EN_FALLO) ? SINT_FALLO : (opc & CAB_GPU_EN_CUELGUE) ? SINT_CUELGUE : SINT_NORMAL;
    rom = kmalloc(SINT_TAM);
    ps = kmalloc(ATOM_PS_PALAVRAS * 4);
    if (!rom || !ps) { txt_copia_n(p->motivo, "simulado: sin memoria", sizeof p->motivo); return true; }
    rom_tam = atom_sintetica(rom, var);
    uint32_t rc;
    atom_rom_validar(rom, rom_tam, &p->fabricante, &p->modelo, &rc, &cmd_off, &dados_off);
    atom_firmware(rom, rom_tam, dados_off, &p->fw);
    atom_powerplay(rom, rom_tam, dados_off, &p->tab);
    memset(&card, 0, sizeof card);
    card.reg_read = s_reg_r; card.reg_write = s_reg_w;
    card.ioreg_read = s_reg_r; card.ioreg_write = s_reg_w;
    card.pll_read = s_pll_r; card.pll_write = s_pll_w;
    card.mc_read = s_mc_r; card.mc_write = s_mc_w;
    card.atraso_us = s_atraso; card.ms_agora = s_ms;
    card.bios_tam = rom_tam; card.rv515 = true;
    ctx = atom_parse(&card, rom);
    if (ctx) atom_allocate_fb_scratch(ctx);
    /* los relojes «del arranque» de la tarjeta de mentira: los de por defecto */
    pll[SINT_PLL_SCLK] = p->fw.sclk_def;
    pll[SINT_PLL_MCLK] = p->fw.mclk_def;
    txt_copia_n(p->bios, ctx ? ctx->nome : "?", sizeof p->bios);
    p->pode_ler = p->pode_mudar = ctx != NULL;
    p->temp_mili_c = GPU_EN_SIN_SENSOR;
    txt_copia_n(p->temp_fuente, "sin sensor (tarjeta simulada)", sizeof p->temp_fuente);
    p->vram_kb = 16384;
    p->confirmada = true;   /* el automático la usa (las pruebas lo necesitan) */
    snformatar(p->motivo, sizeof p->motivo, "tarjeta simulada (BIOS ATOM de juguete%s): %d estados", var == SINT_FALLO ? ", escribe mal" :
               var == SINT_CUELGUE ? ", se cuelga" : "", p->tab.n);
    klog("[GPUENERGIA] simulado: BIOS «%s», %d estados, %u/%u kHz por defecto\n", p->bios, p->tab.n, p->fw.sclk_def * 10,
         p->fw.mclk_def * 10);
    return true;
}

static bool executar(int idx, uint32_t *v, char *m, int nm)
{
    memset(ps, 0, ATOM_PS_PALAVRAS * 4);
    ps[0] = *v;
    ctx->prazo_laco_ms = 200;
    ctx->prazo_total_ms = 1000;
    if (atom_executar(ctx, idx, ps, ATOM_PS_PALAVRAS)) {
        snformatar(m, (size_t)nm, "la tabla %d de la BIOS se corto (motivo %d)", idx, ctx->motivo_aborto);
        klog("[GPUENERGIA] simulado: tabla %d cortada (motivo %d)\n", idx, ctx->motivo_aborto);
        return false;
    }
    *v = ps[0];
    return true;
}

static bool s_ler(uint32_t *sclk, uint32_t *mclk, char *m, int nm)
{
    uint32_t s = 0, k = 0;
    if (!ctx || !executar(ATOM_CMD_GET_SCLK, &s, m, nm) || !executar(ATOM_CMD_GET_MCLK, &k, m, nm)) return false;
    *sclk = s; *mclk = k;
    return true;
}

static bool s_aplicar(const gpu_en_estado_t *e, uint32_t sclk, uint32_t mclk, uint32_t sclk_atual, uint32_t mclk_atual,
                      bool so_ensaio, char *m, int nm)
{
    if (!ctx) { snformatar(m, (size_t)nm, "sin BIOS"); return false; }
    ensaio = so_ensaio;
    escritas = 0;
    bool ok = true;
    if (e && e->tensao == GPU_EN_V_GPIO && e->gpio_valido && !ensaio)
        regs[e->gpio_reg / 4] = gpu_en_gpio_valor(e, regs[e->gpio_reg / 4]);
    if (!e && !ensaio) regs[SINT_GPIO_REG / 4] &= ~(1u << SINT_GPIO_BIT);
    if (sclk != sclk_atual) { uint32_t v = sclk; ok = executar(ATOM_CMD_SET_SCLK, &v, m, nm); }
    if (ok && mclk != mclk_atual) { uint32_t v = mclk; ok = executar(ATOM_CMD_SET_MCLK, &v, m, nm); }
    if (ensaio) {
        klog("[GPUENERGIA] simulado: ensayo de %u/%u kHz: %u escrituras\n", sclk * 10, mclk * 10, escritas);
        ensaio = false;
        if (ok) snformatar(m, (size_t)nm, "ensayo: %u escrituras (en dmesg)", escritas);
        return ok;
    }
    if (!ok) return false;
    uint32_t s, k;
    if (!s_ler(&s, &k, m, nm)) return false;
    if (!gpu_en_relogio_ok(sclk, s) || !gpu_en_relogio_ok(mclk, k)) {
        snformatar(m, (size_t)nm, "la BIOS dice %u/%u kHz y se pidieron %u/%u", s * 10, k * 10, sclk * 10, mclk * 10);
        return false;
    }
    return true;
}

static bool s_parar(uint32_t ms, char *m, int nm) { (void)ms; (void)m; (void)nm; return true; }
static int  s_ecras(void) { return 1; }
static void s_detalle(void)
{
    klog("[GPUENERGIA] simulado: PLL 30 %x 31 %x 32 %x; REG 400 %x; GPIO %x = %x\n", pll[SINT_PLL_SCLK], pll[SINT_PLL_MCLK],
         pll[SINT_PLL_COPIA], regs[SINT_REG_ECO / 4], SINT_GPIO_REG, regs[SINT_GPIO_REG / 4]);
}

const gpu_en_driver_t gpu_en_sim = {
    "simulado", s_sondar, s_ler, s_aplicar, s_parar, s_ecras, s_detalle,
};
