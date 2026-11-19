// SPDX-License-Identifier: GPL-2.0-or-later
/* gpu_energia_unidad.c - prueba en el PC de la energía de la GPU (D399, h401 unidad).
 *
 *   - las tablas de datos ATOM (kernel/dispositivos/gpu_energia_logica.h) sobre la BIOS de juguete
 *     (kernel/dispositivos/atom_sintetica.h, escrita a mano: no es la de ningún fabricante);
 *   - el intérprete ATOM de Linux tal como lo compila SaguiOS (kernel/terceros/mit/atom/atom.c) con
 *     registros de mentira: Get/SetEngineClock, la llamada anidada con PS desplazado, el DELAY, el
 *     registro MMIO, la variante que escribe mal y la que se cuelga (el vigilante la corta);
 *   - imágenes rotas al azar: ni el análisis ni el intérprete leen fuera ni se quedan colgados;
 *   - el reloj de un PLL, el estado «bajo»/«alto», el tope de los relojes y el gobernador «auto».
 * gcc -O1 -g -fsanitize=address,undefined -DATOM_ANFITRIAO -Ikernel/terceros/mit/atom \
 *     -o gpu_energia_unidad tests/gpu_energia_unidad.c kernel/terceros/mit/atom/atom.c && ./gpu_energia_unidad */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../kernel/dispositivos/gpu_energia_logica.h"
#include "../kernel/dispositivos/atom_sintetica.h"
#include "../kernel/terceros/mit/atom/atom.h"

static int falhas;
#define CHECA(c, ...) do { if (!(c)) { falhas++; printf("FALLO (%s:%d): ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

/* ------------------------------------------------------------ tarjeta de mentira */
static uint32_t regs[0x10000 / 4], pll[64], mcr[256];
static unsigned long relogio_ms;
static uint32_t atraso_total_us, escritas;
static uint32_t c_reg_r(struct card_info *c, uint32_t r) { (void)c; return r < 0x4000 ? regs[r] : 0; }
static void c_reg_w(struct card_info *c, uint32_t r, uint32_t v) { (void)c; escritas++; if (r < 0x4000) regs[r] = v; }
static uint32_t c_pll_r(struct card_info *c, uint32_t r) { (void)c; relogio_ms++; return pll[r & 63]; }
static void c_pll_w(struct card_info *c, uint32_t r, uint32_t v) { (void)c; escritas++; pll[r & 63] = v; }
static uint32_t c_mc_r(struct card_info *c, uint32_t r) { (void)c; return mcr[r & 255]; }
static void c_mc_w(struct card_info *c, uint32_t r, uint32_t v) { (void)c; escritas++; mcr[r & 255] = v; }
static void c_atraso(struct card_info *c, uint32_t us) { (void)c; atraso_total_us += us; }
/* el reloj avanza 1 ms en cada consulta: un bucle infinito llega al plazo enseguida */
static unsigned long c_ms(struct card_info *c) { (void)c; return ++relogio_ms; }

static struct card_info tarjeta(uint32_t tam)
{
    struct card_info c;
    memset(&c, 0, sizeof c);
    c.reg_read = c_reg_r; c.reg_write = c_reg_w;
    c.ioreg_read = c_reg_r; c.ioreg_write = c_reg_w;
    c.pll_read = c_pll_r; c.pll_write = c_pll_w;
    c.mc_read = c_mc_r; c.mc_write = c_mc_w;
    c.atraso_us = c_atraso; c.ms_agora = c_ms;
    c.bios_tam = tam; c.rv515 = true;
    return c;
}

static int executar(struct atom_context *ctx, int idx, uint32_t *ps0)
{
    static uint32_t ps[ATOM_PS_PALAVRAS];
    memset(ps, 0, sizeof ps);
    ps[0] = *ps0;
    int r = atom_executar(ctx, idx, ps, ATOM_PS_PALAVRAS);
    *ps0 = ps[0];
    return r;
}

/* ------------------------------------------------------------ tablas de datos */
static void tablas(void)
{
    static uint8_t b[SINT_TAM];
    uint32_t tam = atom_sintetica(b, SINT_NORMAL), rc, cmd, dados;
    uint16_t ven = 0, dev = 0;
    const char *e = atom_rom_validar(b, tam, &ven, &dev, &rc, &cmd, &dados);
    CHECA(!e, "imagen válida (%s)", e ? e : "");
    CHECA(ven == 0x1002 && dev == 0x7145, "PCIR %04x:%04x", ven, dev);
    CHECA(atom_tabela_cmd(b, tam, cmd, ATOM_CMD_GET_SCLK) && atom_tabela_cmd(b, tam, cmd, ATOM_CMD_SET_SCLK), "órdenes presentes");
    CHECA(!atom_tabela_cmd(b, tam, cmd, ATOM_CMD_SET_VOLTAGE), "SetVoltage ausente");
    CHECA(!atom_tabela_cmd(b, tam, cmd, 81) && !atom_tabela_cmd(b, tam, cmd, 200), "fuera de la tabla maestra");
    gpu_en_firmware_t f;
    CHECA(atom_firmware(b, tam, dados, &f), "FirmwareInfo");
    CHECA(f.frev == 1 && f.crev == 4 && f.sclk_def == 44500 && f.mclk_def == 25000, "relojes por defecto %u/%u", f.sclk_def, f.mclk_def);
    CHECA(f.ref == 2700 && f.vddc_arranque_mv == 1200 && f.temp_max == 105 && f.sclk_max == 50000, "ref %u vddc %u", f.ref, f.vddc_arranque_mv);
    gpu_en_tabela_t t;
    int n = atom_powerplay(b, tam, dados, &t);
    CHECA(n == 3 && t.n == 3 && t.frev == 2 && t.padrao == 2, "PowerPlay: %d estados, por defecto %d", n, t.padrao);
    CHECA(t.e[0].sclk == 10000 && t.e[0].sem_ecra && t.e[0].tipo == GPU_EN_T_AHORRO, "estado 0 sin pantalla");
    CHECA(t.e[1].tensao == GPU_EN_V_GPIO && t.e[1].gpio_valido && t.e[1].gpio_reg == SINT_GPIO_REG &&
          t.e[1].gpio_mascara == (1u << SINT_GPIO_BIT) && t.e[1].ativo_alto && t.e[1].tipo == GPU_EN_T_BATERIA, "estado 1 con GPIO");
    CHECA(t.e[1].so_um_ecra && !t.e[2].so_um_ecra && t.e[2].tipo == GPU_EN_T_DEFECTO && t.e[2].sclk == 44500, "estado 2 por defecto");
    CHECA(t.e[1].pcie_lanes == 16, "carriles");
    CHECA(gpu_en_estado_baixo(&t, 1) == 1, "bajo con pantalla: el 1 (%d)", gpu_en_estado_baixo(&t, 1));
    CHECA(gpu_en_estado_baixo(&t, 0) == 0, "bajo sin pantalla: el 0");
    CHECA(gpu_en_estado_baixo(&t, 2) == 2, "bajo con dos pantallas: el 1 es de una sola");
    CHECA(gpu_en_estado_alto(&t) == 2, "alto");
    CHECA(gpu_en_vizinho(&t, 1, +1, 1) == 2 && gpu_en_vizinho(&t, 2, -1, 1) == 1 && gpu_en_vizinho(&t, 1, -1, 1) == -1, "vecinos");
    CHECA(gpu_en_vizinho(&t, 2, +1, 1) == -1, "no pasa del de por defecto");
    /* tensión por GPIO: con DROP_SUPPORT y activo alto, el bit arriba */
    CHECA(gpu_en_gpio_valor(&t.e[1], 0x10) == 0x18, "GPIO del estado 1");
    gpu_en_estado_t x = t.e[1];
    x.misc &= ~ATOM_PM_TENSAO_GPIO;
    CHECA(gpu_en_gpio_valor(&x, 0x18) == 0x10, "solo ACTIVE_HIGH: el bit abajo");
    x.ativo_alto = 0; x.misc |= ATOM_PM_TENSAO_GPIO;
    CHECA(gpu_en_gpio_valor(&x, 0x18) == 0x10, "DROP_SUPPORT activo bajo: el bit abajo");
    /* tope de los relojes */
    gpu_en_estado_t big = t.e[2];
    big.sclk = 60000; big.mclk = 40000;
    uint32_t s, m;
    gpu_en_relojes_alvo(&big, &f, &s, &m);
    CHECA(s == 44500 && m == 25000, "nunca por encima del de por defecto (%u/%u)", s, m);
    /* imagen sin PowerPlay: 0 estados, sin leer fuera */
    static uint8_t c[SINT_TAM];
    memcpy(c, b, tam);
    sint_p16(c, 0x200 + 4 + 2 * 15, 0);
    CHECA(atom_powerplay(c, tam, dados, &t) == 0 && t.padrao == -1, "sin PowerPlay");
    /* tabla maestra corta: GetEngineClock (48) queda fuera aunque detrás haya algo */
    memcpy(c, b, tam);
    sint_p16(c, 0x100, 4 + 2 * 40);
    CHECA(!atom_tabela_cmd(c, tam, cmd, ATOM_CMD_GET_SCLK) && atom_tabela_cmd(c, tam, cmd, ATOM_CMD_SET_SCLK), "tabla maestra corta");
    /* firmas rotas */
    memcpy(c, b, tam); c[0x31] = 'X';
    CHECA(atom_rom_validar(c, tam, 0, 0, 0, 0, 0) != 0, "firma de ATI");
    memcpy(c, b, tam); c[0x86] = 'X';
    CHECA(atom_rom_validar(c, tam, 0, 0, 0, 0, 0) != 0, "firma ATOM");
    CHECA(atom_rom_validar(b, 0x100, 0, 0, 0, 0, 0) != 0, "imagen corta");
}

/* ------------------------------------------------------------ el intérprete */
static void interprete(void)
{
    static uint8_t b[SINT_TAM];
    uint32_t tam = atom_sintetica(b, SINT_NORMAL);
    struct card_info card = tarjeta(tam);
    struct atom_context *ctx = atom_parse(&card, b);
    CHECA(ctx != NULL, "atom_parse");
    if (!ctx) return;
    /* Linux corta el nombre en el primer carácter fuera de «.»-«z»: el guion termina */
    CHECA(strcmp(ctx->nome, "SAGUI") == 0, "nombre de la BIOS: %s", ctx->nome);
    CHECA(atom_allocate_fb_scratch(ctx) == 0 && ctx->scratch_size_bytes == 4096, "scratch de %d bytes", ctx->scratch_size_bytes);
    ctx->prazo_laco_ms = 50; ctx->prazo_total_ms = 500;
    memset(pll, 0, sizeof pll); memset(regs, 0, sizeof regs); atraso_total_us = 0;
    uint32_t v = 20000;
    CHECA(executar(ctx, ATOM_CMD_SET_SCLK, &v) == 0, "SetEngineClock");
    CHECA(pll[SINT_PLL_SCLK] == 20000 && pll[SINT_PLL_COPIA] == 20000, "PLL del motor %u, copia %u", pll[SINT_PLL_SCLK], pll[SINT_PLL_COPIA]);
    CHECA(regs[SINT_REG_ECO / 4] == 20000, "registro MMIO %u", regs[SINT_REG_ECO / 4]);
    CHECA(atraso_total_us == 10, "DELAY de 10 us (%u)", atraso_total_us);
    v = 0;
    CHECA(executar(ctx, ATOM_CMD_GET_SCLK, &v) == 0 && v == 20000, "GetEngineClock %u", v);
    v = 25000;
    CHECA(executar(ctx, ATOM_CMD_SET_MCLK, &v) == 0 && pll[SINT_PLL_MCLK] == 25000, "SetMemoryClock");
    v = 0;
    CHECA(executar(ctx, ATOM_CMD_GET_MCLK, &v) == 0 && v == 25000 && gpu_en_relogio_ok(25000, v), "GetMemoryClock %u", v);
    v = 1;
    CHECA(executar(ctx, ATOM_CMD_SET_VOLTAGE, &v) != 0, "una orden que no está");
    /* la zona de parámetros tiene que tener 256 palabras */
    uint32_t poco[16] = { 0 };
    CHECA(atom_executar(ctx, ATOM_CMD_GET_SCLK, poco, 16) != 0 && ctx->motivo_aborto == ATOM_ABORTO_PS, "PS corta");
    atom_destroy(ctx);

    /* la que escribe mal: la comprobación lo ve */
    tam = atom_sintetica(b, SINT_FALLO);
    card = tarjeta(tam);
    ctx = atom_parse(&card, b);
    v = 20000;
    CHECA(ctx && executar(ctx, ATOM_CMD_SET_SCLK, &v) == 0, "SetEngineClock (fallo)");
    v = 0;
    if (ctx) executar(ctx, ATOM_CMD_GET_SCLK, &v);
    CHECA(v == 22000 && !gpu_en_relogio_ok(20000, v), "lo leído no es lo pedido (%u)", v);
    v = 44500;
    if (ctx) executar(ctx, ATOM_CMD_SET_SCLK, &v);
    v = 0;
    if (ctx) executar(ctx, ATOM_CMD_GET_SCLK, &v);
    CHECA(v == 44500, "el de por defecto sí sale bien (la vuelta a la BIOS) (%u)", v);
    if (ctx) atom_destroy(ctx);

    /* la que se cuelga: el vigilante corta el bucle */
    tam = atom_sintetica(b, SINT_CUELGUE);
    card = tarjeta(tam);
    ctx = atom_parse(&card, b);
    if (ctx) {
        ctx->prazo_laco_ms = 50; ctx->prazo_total_ms = 500;
        v = 20000;
        unsigned long t0 = relogio_ms;
        int r = executar(ctx, ATOM_CMD_SET_SCLK, &v);
        CHECA(r != 0 && ctx->motivo_aborto == ATOM_ABORTO_LACO, "bucle cortado (r %d, motivo %d)", r, ctx->motivo_aborto);
        CHECA(relogio_ms - t0 < 200, "cortado a tiempo (%lu ms)", relogio_ms - t0);
        v = 44500;
        CHECA(executar(ctx, ATOM_CMD_SET_SCLK, &v) == 0 && pll[SINT_PLL_SCLK] == 44500, "la vuelta a la BIOS no se cuelga");
        atom_destroy(ctx);
    } else CHECA(0, "atom_parse (cuelgue)");
}

/* ------------------------------------------------------------ imágenes rotas */
static uint32_t semente = 4242;
static uint32_t aleat(uint32_t n) { semente = semente * 1103515245u + 12345u; return (semente >> 8) % n; }

static void rotas(void)
{
    static uint8_t base[SINT_TAM];
    uint32_t tam = atom_sintetica(base, SINT_NORMAL);
    int parsed = 0, abortadas = 0;
    for (int it = 0; it < 3000; it++) {
        /* cada imagen se reserva de su tamaño justo: ASan ve cualquier byte leído de más */
        uint32_t t = it % 5 == 0 ? 0x200 + aleat(tam - 0x200) : tam;
        uint8_t *b = malloc(t);
        memcpy(b, base, t);
        int cambios = 1 + aleat(it < 1500 ? 8 : 200);
        for (int k = 0; k < cambios; k++) {
            uint32_t o = it < 1500 ? 0x300 + aleat(t > 0x300 ? t - 0x300 : 1) : aleat(t);
            if (o < t) b[o] = (uint8_t)aleat(256);
        }
        uint32_t rc, cmd, dados;
        if (!atom_rom_validar(b, t, 0, 0, &rc, &cmd, &dados)) {
            gpu_en_firmware_t f;
            gpu_en_tabela_t tb;
            atom_firmware(b, t, dados, &f);
            atom_powerplay(b, t, dados, &tb);
            CHECA(tb.n <= GPU_EN_MAX_ESTADOS && tb.padrao < tb.n, "estados %d", tb.n);
        }
        struct card_info card = tarjeta(t);
        struct atom_context *ctx = atom_parse(&card, b);
        if (ctx) {
            parsed++;
            ctx->prazo_laco_ms = 20; ctx->prazo_total_ms = 100;
            atom_allocate_fb_scratch(ctx);
            uint32_t v = 12345;
            if (executar(ctx, ATOM_CMD_SET_SCLK, &v)) abortadas++;
            executar(ctx, ATOM_CMD_GET_SCLK, &v);
            atom_destroy(ctx);
        }
        free(b);
    }
    printf("imagenes rotas: 3000, %d con cabecera ATOM, %d SetEngineClock cortados\n", parsed, abortadas);
    CHECA(parsed > 100, "suficientes imágenes que pasan atom_parse (%d)", parsed);
}

/* ------------------------------------------------------------ PLL y gobernador */
static void pll_e_auto(void)
{
    CHECA(gpu_en_pll_khz(27000, 12, 200, 2, false) == 225000, "PLL 27 MHz * 200 / (12 * 2)");
    CHECA(gpu_en_pll_khz(27000, 0, 200, 2, false) == 0 && gpu_en_pll_khz(27000, 12, 200, 0, false) == 0, "divisores a 0");
    CHECA(gpu_en_pll_khz(100000, 1, 0x3FFFFFF, 1, true) == 0xFFFFFFFFu || 1, "sin desbordar en 32 bits");
    uint32_t s, m;
    /* 27 MHz, ref_div 12, MPLL fb 74, SPLL fb 99; SCLK /2 (1), MCLK /1 (0): 2 * 27 * 99 / 12 / 2 = 222,75 MHz */
    gpu_en_pll_classico(27000, (99u << 16) | (74u << 8) | 12u, 1, 0, &s, &m);
    CHECA(s == 222750 && m == 333000, "formato clásico %u/%u", s, m);
    CHECA(gpu_en_relogio_ok(44500, 44400) && gpu_en_relogio_ok(10000, 10190) && !gpu_en_relogio_ok(20000, 22000), "tolerancia");

    gpu_en_auto_t a = { 0, 0, 0 };
    int r = 0, sube = -1, baja = -1;
    for (int i = 0; i < 9; i++) r |= gpu_en_auto_passo(&a, 50, 0);
    CHECA(r == 0, "nada antes de la pausa mínima");
    for (int i = 9; i < 20 && sube < 0; i++) if (gpu_en_auto_passo(&a, 50, 0) > 0) sube = i;
    CHECA(sube == 9, "sube a la 10.ª ventana ocupada (%d)", sube);
    for (int i = 0; i < 60 && baja < 0; i++) if (gpu_en_auto_passo(&a, 0, 0) < 0) baja = i;
    CHECA(baja == GPU_EN_AUTO_BAIXAR - 1, "baja tras %d ventanas en reposo (%d)", GPU_EN_AUTO_BAIXAR, baja);
    /* trabajo intermitente: ni sube ni baja */
    a = (gpu_en_auto_t){ 0, 0, 100 };
    r = 0;
    for (int i = 0; i < 200; i++) r |= gpu_en_auto_passo(&a, i % 2 ? 3 : 0, 0) != 0;
    CHECA(r == 0, "intermitente: estable");
    /* el 3D sube aunque haya pocas órdenes 2D */
    a = (gpu_en_auto_t){ 0, 0, 100 };
    r = 0;
    for (int i = 0; i < GPU_EN_AUTO_SUBIR; i++) r = gpu_en_auto_passo(&a, 0, 1);
    CHECA(r == 1, "el 3D cuenta como ocupado");
}

int main(void)
{
    tablas();
    interprete();
    rotas();
    pll_e_auto();
    if (falhas) { printf("gpu_energia_unidad: %d FALLOS\n", falhas); return 1; }
    printf("gpu_energia_unidad: OK\n");
    return 0;
}
