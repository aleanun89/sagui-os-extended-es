// SPDX-License-Identifier: GPL-2.0-or-later
/* energia_radeon.c - relojes y estados de energía de las Radeon R5xx por la BIOS ATOM (D399).
 *
 * SIN PROBAR EN HARDWARE: QEMU no emula ninguna Radeon. El objetivo es la Mobility Radeon X1400
 * (RV515/M54, 1002:7145) del Amilo. La capa común (kernel/gui/gpu_energia.c) decide cuándo; aquí:
 *
 *   sondar   (sin tocar la tarjeta salvo leer CONFIG_MEMSIZE) busca una R5xx discreta en el PCI,
 *            copia la ROM de vídeo de C0000 (la sombra que deja la BIOS del PC: solo si su PCIR es
 *            de esta tarjeta) y lee sus tablas de datos: FirmwareInfo (relojes por defecto,
 *            referencia), PowerPlayInfo (los estados, con su tensión) y GPIO_Pin_LUT. Nada se ejecuta.
 *   ler      corre GetEngineClock y GetMemoryClock de la BIOS (lo que hace el radeon_pm_info de
 *            Linux): leen los PLL de verdad (SPLL/MPLL) y devuelven los relojes en 10 kHz.
 *   aplicar  como radeon_set_power_state de Linux (método «profile»): espera al borrado vertical,
 *            corta las lecturas de los controladores de pantalla encendidos
 *            (DISP_READ_REQUEST_DISABLE, rs600_pm_prepare), sube la tensión antes de subir el reloj
 *            o la baja después de bajarlo (rs600_pm_misc: el pin GPIO del estado, o SetVoltage con
 *            el identificador de VDDC), SetEngineClock y SetMemoryClock de la BIOS (nunca por encima
 *            de los de por defecto), devuelve las lecturas a los controladores (rs600_pm_finish) y
 *            comprueba con GetEngineClock/GetMemoryClock que los relojes son los pedidos.
 *            «ensaio» (acel2d energia simular N): corre las mismas tablas sin escribir nada en la
 *            tarjeta salvo los registros índice que hacen falta para LEER (CLOCK_CNTL_INDEX,
 *            MC_IND_INDEX); cada escritura que la BIOS haría va al registro [GPUENERGIA].
 *   La capa común lo llama con las interrupciones cerradas y el motor parado (parar_motor: el 2D
 *   libre en RBBM_STATUS y el CP del 3D con su valla). Lo que Linux hace además en rs600_pm_misc
 *   (DYN_PWRMGT_SCLK_LENGTH, DYN_SCLK_VOL_CNTL, HDP_DYN_CNTL, DYN_BACKBIAS_CNTL y los carriles de
 *   PCI Express) NO se hace: solo relojes y tensión, por los caminos de la BIOS (D399).
 *
 * El intérprete es el de Linux (kernel/terceros/mit/atom/, MIT) con plazos: 500 ms por bucle y 1 s
 * por tabla (Linux: 5 s por bucle); un aborto es un fallo y la capa vuelve a la BIOS.
 * Referencias (MIT): Linux 6.6 drivers/gpu/drm/radeon/ radeon_atombios.c, radeon_pm.c, rs600.c,
 * rv515.c (rv515_mc_rreg), r100.c (r100_pll_rreg/wreg, r100_mm_rreg), radeon_device.c (cail_*),
 * r500_reg.h (AVIVO_D1CRTC_*). Qué mirar en el Amilo: docs/decisoes.md, D399. */
#include "gpu_energia.h"
#include "abi.h"
#include "console.h"
#include "cpu.h"
#include "info_hardware.h"
#include "memoria.h"
#include "paginacao.h"
#include "pci.h"
#include "portas.h"
#include "prazo.h"
#include "texto.h"
#include "../terceros/mit/atom/atom.h"

#define MMIO_TAM            0x10000
#define MM_INDEX            0x0000
#define MM_DATA             0x0004
#define CLOCK_CNTL_INDEX    0x0008
#define CLOCK_CNTL_DATA     0x000C
#define   PLL_WR_EN         0x80u
#define MC_IND_INDEX        0x0070
#define MC_IND_DATA         0x0074
#define CONFIG_MEMSIZE      0x00F8
#define RBBM_STATUS         0x0E40
#define   RBBM_ATIVO        0x80000000u
#define D1CRTC_CONTROL      0x6080
#define   CRTC_EN           0x1u
#define   CRTC_SEM_LEITURAS (1u << 24)     /* AVIVO_CRTC_DISP_READ_REQUEST_DISABLE */
#define D1CRTC_STATUS       0x609C
#define   CRTC_V_BLANK      0x1u
#define D2_DESL             0x0800
#define IO_TAM              0x100
#define ROM_SOMBRA          0xC0000u
#define ROM_MAX             0x20000u
#define PRAZO_LACO_MS       500
#define PRAZO_TABELA_MS     1000

/* Las R5xx discretas (la misma lista que el volcado de acel_radeon.c y gpu3d_r500.c). rv515: CHIP_RV515
 * de Linux (RV505/RV515/RV516: el intérprete lee antes de escribir por IIO). */
static const struct { uint16_t de, ate; uint8_t rv515; } modelos[] = {
    { 0x7100, 0x710F, 0 }, { 0x7140, 0x715F, 1 }, { 0x7180, 0x719F, 1 }, { 0x71C0, 0x71DE, 0 },
    { 0x7200, 0x7211, 1 }, { 0x7240, 0x724F, 0 }, { 0x7280, 0x7280, 0 }, { 0x7284, 0x7284, 0 },
    { 0x7288, 0x728C, 0 }, { 0x7290, 0x7297, 0 },
};
/* Modelos con los cambios de reloj probados en una máquina real: el modo automático de kconfig los usa.
 * Vacía hasta que el Amilo (0x7145) lo confirme (ORDENES de /home/aanun/SO/private/amilo-energia). */
static const uint16_t confirmadas[] = { 0 };

static pci_disp_t disp;
static volatile uint8_t *mmio;
static uint16_t io_base;
static bool e_rv515;
static uint8_t *rom;
static uint32_t rom_tam;
static struct card_info card;
static struct atom_context *ctx;
static uint32_t *ps;
static uint32_t cmd_off, dados_off;
static bool ensaio;
static uint32_t ensaio_escritas;
static bool gpio_guardado;
static uint32_t gpio_reg_arr, gpio_mask_arr, gpio_val_arr;   /* el pin de la tensión como lo dejó la BIOS */
static int ultimo_vddc = -1;

static inline uint32_t rd(uint32_t r)             { return *(volatile uint32_t *)(mmio + r); }
static inline void     wr(uint32_t r, uint32_t v) { *(volatile uint32_t *)(mmio + r) = v; }

/* ------------------------------------------------------------ lo que el intérprete usa (cail_* de Linux) */

static void ensaio_nota(const char *que, uint32_t reg, uint32_t v)
{
    ensaio_escritas++;
    if (ensaio_escritas <= 200) klog("[GPUENERGIA] ensayo: %s %x <- %x (no se escribe)\n", que, reg, v);
}

static uint32_t c_reg_r(struct card_info *c, uint32_t r)
{
    (void)c;
    uint32_t b = r * 4;
    if (b < MMIO_TAM) return rd(b);
    if (ensaio) { ensaio_nota("lectura indirecta (MM_INDEX) omitida de", b, 0); return 0; }
    wr(MM_INDEX, b);
    return rd(MM_DATA);
}

static void c_reg_w(struct card_info *c, uint32_t r, uint32_t v)
{
    (void)c;
    uint32_t b = r * 4;
    if (ensaio) { ensaio_nota("REG", b, v); return; }
    if (b < MMIO_TAM) wr(b, v);
    else { wr(MM_INDEX, b); wr(MM_DATA, v); }
}

/* IIO: por el BAR de E/S, como Linux (r100_io_rreg); sin él, por el MMIO (el «using MMIO for ATOM IIO»). */
static uint32_t c_io_r(struct card_info *c, uint32_t r)
{
    uint32_t b = r * 4;
    if (!io_base) return c_reg_r(c, r);
    if (b < IO_TAM) return porta_le32((uint16_t)(io_base + b));
    if (ensaio) { ensaio_nota("lectura IO indirecta omitida de", b, 0); return 0; }
    porta_escreve32(io_base + MM_INDEX, b);
    return porta_le32((uint16_t)(io_base + MM_DATA));
}

static void c_io_w(struct card_info *c, uint32_t r, uint32_t v)
{
    uint32_t b = r * 4;
    if (!io_base) { c_reg_w(c, r, v); return; }
    if (ensaio) { ensaio_nota("IO", b, v); return; }
    if (b < IO_TAM) porta_escreve32((uint16_t)(io_base + b), v);
    else { porta_escreve32(io_base + MM_INDEX, b); porta_escreve32(io_base + MM_DATA, v); }
}

static uint32_t c_pll_r(struct card_info *c, uint32_t r)
{
    (void)c;
    *(volatile uint8_t *)(mmio + CLOCK_CNTL_INDEX) = (uint8_t)(r & 0x3F);
    return rd(CLOCK_CNTL_DATA);
}

static void c_pll_w(struct card_info *c, uint32_t r, uint32_t v)
{
    (void)c;
    if (ensaio) { ensaio_nota("PLL", r & 0x3F, v); return; }
    *(volatile uint8_t *)(mmio + CLOCK_CNTL_INDEX) = (uint8_t)((r & 0x3F) | PLL_WR_EN);
    wr(CLOCK_CNTL_DATA, v);
}

static uint32_t c_mc_r(struct card_info *c, uint32_t r)
{
    (void)c;
    wr(MC_IND_INDEX, 0x7F0000u | (r & 0xFFFFu));
    uint32_t v = rd(MC_IND_DATA);
    wr(MC_IND_INDEX, 0);
    return v;
}

static void c_mc_w(struct card_info *c, uint32_t r, uint32_t v)
{
    (void)c;
    if (ensaio) { ensaio_nota("MC", r & 0xFFFF, v); return; }
    wr(MC_IND_INDEX, 0xFF0000u | (r & 0xFFFFu));
    wr(MC_IND_DATA, v);
    wr(MC_IND_INDEX, 0);
}

static void c_atraso(struct card_info *c, uint32_t us)
{
    (void)c;
    if (ensaio) return;
    uint32_t khz = hw_tsc_khz();
    if (!khz) { for (uint32_t i = 0; i < us; i++) porta_pausa(); return; }
    uint64_t fim = prazo_tsc() + (uint64_t)khz * us / 1000u;
    while (prazo_tsc() < fim) __asm__ volatile("pause");
}

static unsigned long c_ms(struct card_info *c)
{
    (void)c;
    static unsigned long sem_tsc;
    uint32_t khz = hw_tsc_khz();
    if (!khz) return ++sem_tsc;   /* sin TSC medido: cada consulta cuenta como 1 ms (corta antes, nunca después) */
    return (unsigned long)(prazo_tsc() / khz);
}

static const char *nome_aborto(int a)
{
    static const char *const n[] = { "", "bucle de mas de 500 ms", "tabla de mas de 1 s", "llamadas anidadas de mas",
                                     "zona de parametros corta" };
    return a >= 0 && a <= 4 ? n[a] : "?";
}

/* Una tabla de órdenes con PS[0] = v; devuelve PS[0] en *v. false si no está o se abortó. */
static bool executar(int idx, uint32_t *v, uint32_t ps1, char *m, int nm)
{
    if (!ctx || !atom_tabela_cmd(rom, rom_tam, cmd_off, idx)) {
        snformatar(m, (size_t)nm, "la BIOS no tiene la tabla %d", idx);
        return false;
    }
    memset(ps, 0, ATOM_PS_PALAVRAS * 4);
    ps[0] = *v; ps[1] = ps1;
    ctx->prazo_laco_ms = PRAZO_LACO_MS;
    ctx->prazo_total_ms = PRAZO_TABELA_MS;
    uint64_t t0 = prazo_tsc();
    int r = atom_executar(ctx, idx, ps, ATOM_PS_PALAVRAS);
    uint32_t khz = hw_tsc_khz();
    uint32_t us = khz ? (uint32_t)((prazo_tsc() - t0) * 1000u / khz) : 0;
    if (r) {
        snformatar(m, (size_t)nm, "la tabla %d de la BIOS se corto (%s)", idx, nome_aborto(ctx->motivo_aborto));
        klog("[GPUENERGIA] radeon: tabla %d (PS0 %x) cortada en %u us: %s\n", idx, *v, us, nome_aborto(ctx->motivo_aborto));
        return false;
    }
    klog("[GPUENERGIA] radeon: tabla %d (PS0 %x) en %u us -> PS0 %x%s\n", idx, *v, us, ps[0], ensaio ? " (ensayo)" : "");
    *v = ps[0];
    return true;
}

/* ------------------------------------------------------------ sondar */

static bool copiar_rom(char *m, int nm)
{
    uint32_t v = janela_mapear(ROM_SOMBRA, ROM_MAX);
    if (!v) { snformatar(m, (size_t)nm, "sin ventana para leer la ROM de video (C0000)"); return false; }
    const volatile uint8_t *s = (const volatile uint8_t *)v;
    bool ok = false;
    if (s[0] != 0x55 || s[1] != 0xAA) snformatar(m, (size_t)nm, "no hay ROM de video en C0000 (arranque UEFI?)");
    else {
        uint32_t tam = (uint32_t)s[2] * 512u;
        uint32_t pcir = s[0x18] | (uint32_t)s[0x19] << 8;
        uint16_t ven = 0, dev = 0;
        if (pcir + 8 < ROM_MAX && s[pcir] == 'P' && s[pcir + 1] == 'C' && s[pcir + 2] == 'I' && s[pcir + 3] == 'R') {
            ven = (uint16_t)(s[pcir + 4] | s[pcir + 5] << 8);
            dev = (uint16_t)(s[pcir + 6] | s[pcir + 7] << 8);
        }
        if (tam < 0x200 || tam > ROM_MAX) snformatar(m, (size_t)nm, "ROM de C0000 con tamano raro (%u)", tam);
        else if (ven != disp.fabricante || dev != disp.modelo)
            snformatar(m, (size_t)nm, "la ROM de C0000 es de otra tarjeta (%04x:%04x)", ven, dev);
        else if (!(rom = kmalloc(tam))) snformatar(m, (size_t)nm, "sin memoria para la ROM");
        else {
            for (uint32_t i = 0; i < tam; i++) rom[i] = s[i];
            rom_tam = tam;
            ok = true;
        }
    }
    janela_soltar(v);
    return ok;
}

static bool r_sondar(gpu_en_placa_t *p)
{
    pci_disp_t v[8];
    int n = pci_procurar_classe(0x03, 0x00, v, 8);
    int achado = -1;
    for (int i = 0; i < n && achado < 0; i++) {
        if (v[i].fabricante != 0x1002) continue;
        for (unsigned k = 0; k < sizeof modelos / sizeof modelos[0]; k++)
            if (v[i].modelo >= modelos[k].de && v[i].modelo <= modelos[k].ate) { achado = i; e_rv515 = modelos[k].rv515; }
    }
    if (achado < 0) return false;
    disp = v[achado];
    p->fabricante = disp.fabricante; p->modelo = disp.modelo;
    for (unsigned k = 0; k < sizeof confirmadas / sizeof confirmadas[0]; k++)
        if (confirmadas[k] && confirmadas[k] == disp.modelo) p->confirmada = true;
    /* R5xx: sin sensor de temperatura legible (Linux: THERMAL_TYPE_NONE en las R5xx). */
    p->temp_mili_c = GPU_EN_SIN_SENSOR;
    txt_copia_n(p->temp_fuente, "sin sensor en la tarjeta (R5xx)", sizeof p->temp_fuente);
    /* MMIO (BAR 2) y E/S (el primer BAR de E/S) */
    uint32_t bar2 = pci_le32(&disp, 0x18);
    if ((bar2 & 1) || ((bar2 >> 1) & 3) == 2 || !(bar2 & ~0xFu)) {
        snformatar(p->motivo, sizeof p->motivo, "Radeon R5xx %04x: el BAR 2 no es MMIO de 32 bits (%x)", disp.modelo, bar2);
        return true;
    }
    /* Solo si la tarjeta decodifica E/S (bit 0 de la orden del PCI); si no, el IIO va por el MMIO, como Linux sin BAR de E/S. */
    for (uint8_t r = 0x10; r <= 0x24 && !io_base && (pci_le32(&disp, 0x04) & 0x01); r += 4) {
        uint32_t b = pci_le32(&disp, r);
        if ((b & 1) && (b & ~3u) && (b & ~3u) < 0x10000u) io_base = (uint16_t)(b & ~3u);
    }
    if (!(pci_le32(&disp, 0x04) & 0x02)) {
        snformatar(p->motivo, sizeof p->motivo, "Radeon R5xx %04x: la memoria del PCI esta apagada", disp.modelo);
        return true;
    }
    mmio = (volatile uint8_t *)janela_mapear_tipo(bar2 & ~0xFu, MMIO_TAM, PG_PCD | PG_PWT);
    if (!mmio || rd(RBBM_STATUS) == 0xFFFFFFFFu) {
        snformatar(p->motivo, sizeof p->motivo, "Radeon R5xx %04x: el MMIO no responde", disp.modelo);
        mmio = NULL;
        return true;
    }
    p->vram_kb = rd(CONFIG_MEMSIZE) >> 10;
    if (!copiar_rom(p->motivo, sizeof p->motivo)) return true;
    uint16_t ven, dev;
    uint32_t rc;
    const char *e = atom_rom_validar(rom, rom_tam, &ven, &dev, &rc, &cmd_off, &dados_off);
    if (e) { snformatar(p->motivo, sizeof p->motivo, "ROM de video sin BIOS ATOM valida: %s", e); return true; }
    atom_firmware(rom, rom_tam, dados_off, &p->fw);
    atom_powerplay(rom, rom_tam, dados_off, &p->tab);
    /* El intérprete (sin ejecutar nada todavía: atom_parse solo lee la cabecera y el índice del IIO). */
    memset(&card, 0, sizeof card);
    card.reg_read = c_reg_r; card.reg_write = c_reg_w;
    card.ioreg_read = c_io_r; card.ioreg_write = c_io_w;
    card.pll_read = c_pll_r; card.pll_write = c_pll_w;
    card.mc_read = c_mc_r; card.mc_write = c_mc_w;
    card.atraso_us = c_atraso; card.ms_agora = c_ms;
    card.bios_tam = rom_tam; card.rv515 = e_rv515;
    ps = kmalloc(ATOM_PS_PALAVRAS * 4);
    ctx = ps ? atom_parse(&card, rom) : NULL;
    if (ctx && atom_allocate_fb_scratch(ctx)) { atom_destroy(ctx); ctx = NULL; }
    if (!ctx) { snformatar(p->motivo, sizeof p->motivo, "la BIOS ATOM no se pudo preparar (atom_parse)"); return true; }
    txt_copia_n(p->bios, ctx->nome, sizeof p->bios);
    p->pode_ler = atom_tabela_cmd(rom, rom_tam, cmd_off, ATOM_CMD_GET_SCLK) && atom_tabela_cmd(rom, rom_tam, cmd_off, ATOM_CMD_GET_MCLK);
    p->pode_mudar = p->pode_ler && p->tab.n >= 2 && p->fw.sclk_def &&
                    atom_tabela_cmd(rom, rom_tam, cmd_off, ATOM_CMD_SET_SCLK) && atom_tabela_cmd(rom, rom_tam, cmd_off, ATOM_CMD_SET_MCLK);
    if (p->tab.termico) {
        static const char *const nt[] = { "?", "LM63", "ADM1032", "ADM1030", "MUA6649", "LM64", "F75375", "ASC7512" };
        snformatar(p->temp_fuente, sizeof p->temp_fuente, "sensor externo %s en I2C %x (no se lee)",
                   p->tab.termico < 8 ? nt[p->tab.termico] : "?", (uint32_t)p->tab.termico_dir >> 1);
    }
    snformatar(p->motivo, sizeof p->motivo, "Radeon R5xx %04x: BIOS %s, %d estados (PowerPlay %u.%u), %u/%u MHz por defecto%s",
               disp.modelo, p->bios, p->tab.n, p->tab.frev, p->tab.crev, p->fw.sclk_def / 100, p->fw.mclk_def / 100,
               p->pode_mudar ? "" : p->pode_ler ? "; sin estados para cambiar" : "; sin GetEngineClock");
    klog("[GPUENERGIA] radeon: %04x:%04x en %x:%x.%x (%s), MMIO %x, E/S %x, ROM de C0000 %u KB, BIOS «%s»\n", disp.fabricante,
         disp.modelo, disp.barramento, disp.dispositivo, disp.funcao, e_rv515 ? "RV515" : "R5xx", bar2 & ~0xFu, io_base,
         rom_tam >> 10, p->bios);
    klog("[GPUENERGIA] radeon: FirmwareInfo %u.%u: motor %u kHz, memoria %u kHz por defecto (maximos %u/%u), referencia %u kHz, "
         "VDDC de arranque %u mV, temperatura maxima %u\n", p->fw.frev, p->fw.crev, p->fw.sclk_def * 10, p->fw.mclk_def * 10,
         p->fw.sclk_max * 10, p->fw.mclk_max * 10, p->fw.ref * 10, p->fw.vddc_arranque_mv, p->fw.temp_max);
    klog("[GPUENERGIA] radeon: PowerPlayInfo %u.%u: %u entradas de %u bytes, %d validas, por defecto %d; termico %u (I2C %u, dir %x)\n",
         p->tab.frev, p->tab.crev, p->tab.n_bios, p->tab.tam_entrada, p->tab.n, p->tab.padrao, p->tab.termico,
         p->tab.termico_i2c, p->tab.termico_dir);
    for (int i = 0; i < p->tab.n; i++) {
        const gpu_en_estado_t *s = &p->tab.e[i];
        klog("[GPUENERGIA] radeon: estado %d (BIOS %u): motor %u kHz, memoria %u kHz, misc %x/%x, tipo %u, tension %u "
             "(GPIO %u: reg %x mascara %x %s, VDDC %u), PCIe x%u%s%s\n", i, s->indice_bios, s->sclk * 10, s->mclk * 10, s->misc,
             s->misc2, s->tipo, s->tensao, s->gpio_id, s->gpio_reg, s->gpio_mascara, s->ativo_alto ? "activo alto" : "activo bajo",
             s->vddc_id, s->pcie_lanes, s->sem_ecra ? ", sin pantalla" : "", s->so_um_ecra ? ", una pantalla" : "");
    }
    return true;
}

/* ------------------------------------------------------------ leer y cambiar */

static bool r_ler(uint32_t *sclk, uint32_t *mclk, char *m, int nm)
{
    uint32_t s = 0, k = 0;
    if (!executar(ATOM_CMD_GET_SCLK, &s, 0, m, nm) || !executar(ATOM_CMD_GET_MCLK, &k, 0, m, nm)) return false;
    *sclk = s & 0xFFFFFFu; *mclk = k & 0xFFFFFFu;   /* SET_CLOCK_FREQ_MASK: los bits altos son marcas */
    return true;
}

static int r_ecras(void)
{
    if (!mmio) return 1;
    return ((rd(D1CRTC_CONTROL) & CRTC_EN) ? 1 : 0) + ((rd(D1CRTC_CONTROL + D2_DESL) & CRTC_EN) ? 1 : 0);
}

static bool r_parar(uint32_t ms, char *m, int nm)
{
    if (gpu3d_r500_ocioso(ms) < 0) { snformatar(m, (size_t)nm, "el CP del 3D no termina lo que tiene"); return false; }
    prazo_t p = prazo_ms(ms);
    while (rd(RBBM_STATUS) & RBBM_ATIVO)
        if (prazo_vencido(&p)) { snformatar(m, (size_t)nm, "el motor no para (RBBM_STATUS %x)", rd(RBBM_STATUS)); return false; }
    return true;
}

/* Espera a que los controladores encendidos estén en el borrado vertical (radeon_sync_with_vblank:
 * Linux espera a la interrupción hasta 200 ms y sigue igual si no llega). */
static void esperar_vblank(void)
{
    for (int c = 0; c < 2; c++) {
        uint32_t d = c ? D2_DESL : 0;
        if (!(rd(D1CRTC_CONTROL + d) & CRTC_EN)) continue;
        prazo_t p = prazo_ms(40);
        while (rd(D1CRTC_STATUS + d) & CRTC_V_BLANK) if (prazo_vencido(&p)) break;    /* que empiece uno nuevo */
        p = prazo_ms(40);
        while (!(rd(D1CRTC_STATUS + d) & CRTC_V_BLANK)) if (prazo_vencido(&p)) break;
        return;
    }
}

/* La tensión de "e" (NULL: la del arranque). */
static bool tensao(const gpu_en_estado_t *e, char *m, int nm)
{
    if (!e) {
        if (!gpio_guardado) return true;
        uint32_t v = (rd(gpio_reg_arr) & ~gpio_mask_arr) | gpio_val_arr;
        if (ensaio) ensaio_nota("GPIO de la tension (vuelta a la BIOS)", gpio_reg_arr, v);
        else wr(gpio_reg_arr, v);
        return true;
    }
    if (e->tensao == GPU_EN_V_GPIO) {
        if (!e->gpio_valido) return true;               /* Linux: gpio.valid falso, no se toca */
        if (e->gpio_reg < 0x100 || e->gpio_reg >= MMIO_TAM) {
            snformatar(m, (size_t)nm, "el pin de la tension (%x) cae fuera del MMIO", e->gpio_reg);
            return false;
        }
        if (!gpio_guardado) {
            gpio_guardado = true;
            gpio_reg_arr = e->gpio_reg; gpio_mask_arr = e->gpio_mascara;
            gpio_val_arr = rd(e->gpio_reg) & e->gpio_mascara;
            klog("[GPUENERGIA] radeon: tension: el pin %x/%x estaba a %x al arrancar\n", gpio_reg_arr, gpio_mask_arr, gpio_val_arr);
        }
        uint32_t ant = rd(e->gpio_reg), v = gpu_en_gpio_valor(e, ant);
        if (ensaio) ensaio_nota("GPIO de la tension", e->gpio_reg, v);
        else if (v != ant) wr(e->gpio_reg, v);
        return true;
    }
    if (e->tensao == GPU_EN_V_VDDC) {
        /* radeon_atom_set_voltage, SetVoltage 1.1: tipo VDDC (1), modo ALL_SOURCE (1), el identificador. */
        uint8_t fr = 0, cr = 0;
        if (!ctx || !atom_parse_cmd_header(ctx, ATOM_CMD_SET_VOLTAGE, &fr, &cr) || cr != 1) {
            snformatar(m, (size_t)nm, "el estado pide tension por identificador y SetVoltage %u.%u no se conoce", fr, cr);
            return false;
        }
        if (e->vddc_id == ultimo_vddc) return true;
        uint32_t a = 1u | (1u << 8) | ((uint32_t)e->vddc_id << 16);
        if (!executar(ATOM_CMD_SET_VOLTAGE, &a, 0, m, nm)) return false;
        if (!ensaio) ultimo_vddc = e->vddc_id;
    }
    return true;
}

static bool r_aplicar(const gpu_en_estado_t *e, uint32_t sclk, uint32_t mclk, uint32_t sclk_atual, uint32_t mclk_atual,
                      bool so_ensaio, char *m, int nm)
{
    if (!ctx || !mmio) { snformatar(m, (size_t)nm, "sin BIOS ATOM preparada"); return false; }
    ensaio = so_ensaio;
    ensaio_escritas = 0;
    bool ok = true;
    uint32_t ctl[2] = { 0, 0 };
    if (!ensaio) {
        esperar_vblank();
        for (int c = 0; c < 2; c++) {                            /* rs600_pm_prepare */
            ctl[c] = rd(D1CRTC_CONTROL + (c ? D2_DESL : 0));
            if (ctl[c] & CRTC_EN) wr(D1CRTC_CONTROL + (c ? D2_DESL : 0), ctl[c] | CRTC_SEM_LEITURAS);
        }
    }
    bool depois = sclk < sclk_atual;                             /* upvolt antes de subir, downvolt después de bajar */
    if (!depois) ok = tensao(e, m, nm);
    if (ok && sclk != sclk_atual) { uint32_t v = sclk; ok = executar(ATOM_CMD_SET_SCLK, &v, 0, m, nm); }
    if (ok && mclk != mclk_atual) { uint32_t v = mclk; ok = executar(ATOM_CMD_SET_MCLK, &v, 0, m, nm); }
    if (ok && depois) ok = tensao(e, m, nm);
    if (!ensaio)
        for (int c = 0; c < 2; c++)                              /* rs600_pm_finish: como estaban */
            if (ctl[c] & CRTC_EN) wr(D1CRTC_CONTROL + (c ? D2_DESL : 0), ctl[c] & ~CRTC_SEM_LEITURAS);
    if (ensaio) {
        klog("[GPUENERGIA] radeon: ensayo de %u/%u kHz: %u escrituras que la BIOS habria hecho%s\n", sclk * 10, mclk * 10,
             ensaio_escritas, ensaio_escritas > 200 ? " (solo las 200 primeras al registro)" : "");
        ensaio = false;
        if (ok) snformatar(m, (size_t)nm, "ensayo: %u escrituras (en dmesg)", ensaio_escritas);
        return ok;
    }
    if (!ok) return false;
    uint32_t s = 0, k = 0;
    char mm[64];
    if (!r_ler(&s, &k, mm, sizeof mm)) { snformatar(m, (size_t)nm, "tras el cambio, %s", mm); return false; }
    if (!gpu_en_relogio_ok(sclk, s) || !gpu_en_relogio_ok(mclk, k)) {
        snformatar(m, (size_t)nm, "la BIOS dice %u/%u kHz y se pidieron %u/%u", s * 10, k * 10, sclk * 10, mclk * 10);
        return false;
    }
    return true;
}

static void r_detalle(void)
{
    if (!mmio) { klog("[GPUENERGIA] radeon: sin MMIO\n"); return; }
    /* Los registros del PLL (solo lectura: índice y dato). En las R5xx el formato del SPLL/MPLL no está
     * confirmado: también se da la cuenta con el formato clásico (R100-R4xx) para compararla con la BIOS. */
    for (uint32_t r = 0; r < 0x40; r += 8)
        klog("[GPUENERGIA] radeon: PLL %x: %x %x %x %x %x %x %x %x\n", r, c_pll_r(&card, r), c_pll_r(&card, r + 1),
             c_pll_r(&card, r + 2), c_pll_r(&card, r + 3), c_pll_r(&card, r + 4), c_pll_r(&card, r + 5), c_pll_r(&card, r + 6),
             c_pll_r(&card, r + 7));
    uint32_t s, k;
    gpu_en_firmware_t f;
    if (rom && atom_firmware(rom, rom_tam, dados_off, &f) && f.ref) {
        gpu_en_pll_classico(f.ref * 10, c_pll_r(&card, 0x0A), c_pll_r(&card, 0x0D), c_pll_r(&card, 0x12), &s, &k);
        klog("[GPUENERGIA] radeon: con el formato clasico (R100-R4xx, sin confirmar en R5xx): motor %u kHz, memoria %u kHz\n", s, k);
    }
    klog("[GPUENERGIA] radeon: D1CRTC_CONTROL %x D2CRTC_CONTROL %x RBBM_STATUS %x CONFIG_MEMSIZE %x; E/S %x\n",
         rd(D1CRTC_CONTROL), rd(D1CRTC_CONTROL + D2_DESL), rd(RBBM_STATUS), rd(CONFIG_MEMSIZE), io_base);
    if (gpio_guardado) klog("[GPUENERGIA] radeon: pin de la tension %x = %x (mascara %x; al arrancar %x)\n", gpio_reg_arr,
                            rd(gpio_reg_arr), gpio_mask_arr, gpio_val_arr);
}

const gpu_en_driver_t gpu_en_radeon = {
    "radeon-r5xx", r_sondar, r_ler, r_aplicar, r_parar, r_ecras, r_detalle,
};
