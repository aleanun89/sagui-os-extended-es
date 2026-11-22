// SPDX-License-Identifier: GPL-2.0-or-later
/* gpu3d_evergreen.c - 3D por la GPU en las Radeon Evergreen (HD 5000: Cedar, Redwood,
 * Juniper, Cypress, Hemlock) y Northern Islands sin Cayman (HD 6000 y HD 7500M/7600M:
 * Barts, Turks, Caicos), debajo de la capa común gpu3d (D427; la capa es la de la D285).
 *
 * Es el mismo esquema que el de las R6xx/R7xx (gpu3d_r600.c, D284/D357/D402), que funciona
 * en el HP dv7: el procesador de órdenes (CP) con su microcódigo lee una cola en la VRAM,
 * los sombreadores (uno de vértices que copia y dos de píxeles: color y textura) van
 * escritos a mano y el cuadro vuelve a la RAM leído por la CPU. Lo que cambia en
 * Evergreen está en evergreen_3d.h (registros, recursos, ISA) y aquí: el microcódigo
 * (PFP de 1120 palabras y ME de 1376), el inicio del motor (evergreen_gpu_init), el
 * arranque del CP (evergreen_cp_resume/start, con el preámbulo del estado limpio) y las
 * direcciones del MC de 40 bits (D408: la HD 7600M del Toshiba tiene la VRAM en
 * 0xF_0000_0000).
 *
 * SIN PROBAR EN HARDWARE cuando se escribió: QEMU no emula ninguna Radeon. Por eso no se
 * enciende solo («gpu3d probar» o «gpu3d encender», o una imagen con --gpu3d=radeon) y la
 * capa común solo lo activa si la autoprueba lee los píxeles esperados.
 *
 * Qué toca y qué no:
 *   - memoria: solo VRAM de la tarjeta, DETRÁS de todo lo que puede usar el 2D por DMA
 *     (r600_fim_2d), de las superficies de los otros controladores y de los cursores, y
 *     dentro de los primeros 64 MB del BAR 0 (en Evergreen no hay registro con el tamaño
 *     de la apertura, D286). Sin GART, sin bus maestro, sin interrupciones, con la memoria
 *     virtual de la GPU apagada;
 *   - registros: los del CP (0x86D8, 0x8700, 0xC1xx), el reinicio del CP y de los bloques
 *     del 3D que dependen de él (GRBM_SOFT_RESET: CP, PA, SPI, SH, SX, VGT, como Linux), los
 *     valores por defecto del motor 3D (eg3d_inicio_motor) y, por la cola, el contexto 3D.
 *     NO toca los controladores de pantalla, el DMIF, el HDP (salvo su registro de vaciado),
 *     los relojes, la energía, el DMA, el MC ni SRBM_SOFT_RESET;
 *   - si algo no responde: se para el CP y se deja. Nunca se reinicia el motor entero.
 *
 * Firmware: el 2D de estas tarjetas (D286, motor DMA) no carga ninguno; el 3D carga aquí el
 * PFP y el ME del CP. El RLC no se carga (solo hace falta para interrupciones y ahorro de
 * energía, que no se usan: en las R7xx tampoco se carga y el 3D funciona) y el del MC lo deja
 * cargado la BIOS (Linux solo lo carga si el secuenciador del MC no está en marcha: la etapa
 * 3 lo lee y lo dice).
 *
 * Diagnóstico: cada etapa del inicio escribe una línea «[GPU3D] evergreen: etapa N ...»;
 * detalle() vuelca GRBM_STATUS, CP_STAT, punteros de la cola y registros de apuntes.
 *
 * Referencias de números y secuencias: ver evergreen_3d.h y docs/terceros.md. */
#include "gpu3d.h"
#include "console.h"
#include "paginacao.h"
#include "prazo.h"
#include "evergreen_3d.h"
#include "evergreen_dma.h"
#include "r600_base.h"
#include "vram_zona_logica.h"
#include "pci.h"
#include "texto.h"
#include "vfs.h"
#include "memoria.h"
#include "fpu.h"
#include "sse2.h"
#include "video.h"

#define RING_BYTES   (64 * 1024)
#define RING_DW      (RING_BYTES / 4)
#define SH_BYTES     (64 * 1024)       /* página de sombreadores (y hueco hasta la arena) */
#define FIXO         (RING_BYTES + SH_BYTES)
#define VB_BYTES     (512 * 1024)
#define ZONA_MIN     (4u << 20)
#define ZONA_MAX     (128u << 20)
#define BAR_SEGURO   (64u << 20)       /* D286: lo que se usa del BAR 0 sin saber su tamaño */
#define VISIVEL_MAX  (16u << 20)       /* lo que la CPU ve siempre (cola, sombreadores, vértices, destino) */
#define JMOVEL_BYTES ((4u << 20) + 4096u)   /* ventana móvil para escribir texturas de la parte grande */
#define MAX_ALVO     1024
#define MAX_TEX      1024
#define PRAZO_MS     250               /* esperas internas (sitio en la cola, inicio) */
#define SRBM_STATUS  0x0E50
#define HDP_FLUSH    0x5480            /* HDP_MEM_COHERENCY_FLUSH_CNTL */
/* Controladores de pantalla de Evergreen (evergreen_reg.h; + evg_crtc_desloc(n)). Solo se leen. */
#define EVG_GRPH_ENABLE        0x6800
#define EVG_GRPH_CONTROL       0x6804
#define EVG_GRPH_SUPERFICIE    0x6810
#define EVG_GRPH_PITCH         0x6818
#define EVG_GRPH_SUPERFICIE_HI 0x681C
#define EVG_GRPH_DESL_Y        0x6828
#define EVG_GRPH_Y_FIM         0x6838
#define EVG_CUR_CONTROL        0x6998
#define EVG_CUR_SUPERFICIE     0x699C
#define EVG_CUR_SUPERFICIE_HI  0x69A4
#define EVG_VIEWPORT_INI       0x6D70
#define EVG_VIEWPORT_TAM       0x6D74
#define EVG_CRTC_CONTROL       0x6E70
#define SCRATCH_TESTE (R6_SCRATCH_REG0 + 4)
#define SCRATCH_VALLA (R6_SCRATCH_REG0 + 8)
#define NOP_TIPO2    0x80000000u

static r600_base_t base;
static acel_tela_t tela;
static int familia = -1;
static const eg3d_chip_t *chip;
static int etapa = -1;                         /* hasta dónde llegó el último encendido (-1: nunca se intentó) */
static const char *const nomes_etapa[7] = {
    "sonda de solo lectura", "leer el microcodigo", "parar el CP y cargar el microcodigo", "valores por defecto del motor 3D",
    "cola y arranque del CP", "prueba de la cola", "sombreadores y estado base",
};
static bool sondado, cp_ligado, tocou;
static uint64_t mc_tela;                       /* la pantalla en el MC, 40 bits (D408) */
static uint32_t zona_off, zona_bytes;          /* desde el principio de la pantalla */
static volatile uint8_t *zona;                 /* ventana sin caché de la parte visible de la zona */
static uint32_t zona_vis;                      /* bytes de la zona con ventana fija (los primeros) */
static uint32_t jmovel;                        /* ventana móvil reservada (0: no hay) */
static uint32_t zona_fis_map, zona_bytes_map;
static volatile uint32_t *ring;
static uint64_t mc_zona;
static uint32_t wptr, seq, bancos = 2;
static int alin_passo = 64;
static uint32_t sq_config_lido, gb_addr_lido, gb_map_lido, gb_addr_usado, gb_map_usado, mc_seq_sup;
static uint64_t t_valla, t_copia;              /* ciclos del TSC: esperar la valla y copiar a la RAM */
static uint32_t n_ler_px, n_ler;
static g3d_arena_t arena;                      /* lo que queda de la parte visible tras la cola y los sombreadores */
static g3d_arena_t arena_g;                    /* el resto de la zona, sin ventana fija (Z y texturas) */
static bool arena_g_ok;
static g3d_buffer_t b_vb, b_cor, b_z, b_tex[GPU3D_TEXTURAS];
static uint32_t vb_usado;
static bool pendente;                          /* hay órdenes en la cola sin valla esperada */
static int alvo_l, alvo_a, alvo_passo, alvo_zbits;
static int sh_atual;                           /* -1 ninguno, 0 color, 1 textura */
static gpu3d_estado_t est_posto;
static bool est_valido;

static inline uint32_t rd(uint32_t r)             { return *(volatile uint32_t *)(base.mmio + r); }
static inline void     wr(uint32_t r, uint32_t v) { *(volatile uint32_t *)(base.mmio + r) = v; }

/* D390: los registros que /sistema/conf/gpu3d-evergreen.conf puede tocar (la capa lee el fichero): el
 * contexto del 3D (0x28000-0x28FFC) y la configuración de VGT, PA y SQ (0x8800-0x8EFC), del SPI
 * (0x9100-0x91FC), DB_DEBUG..DB_WATERMARKS (0x9830-0x985C) y GB_ADDR_CONFIG/GB_BACKEND_MAP (0x98F8-0x98FC),
 * todos por la cola (SET_CONFIG_REG / SET_CONTEXT_REG). Nunca GRBM ni CP (0x8000-0x87FF), el MC, los
 * relojes, la energía ni la pantalla. */
static const g3d_faixa_t eg_faixas_ajustes[] = {
    { 0x28000u, 0x28FFCu }, { 0x8800u, 0x8EFCu }, { 0x9100u, 0x91FCu }, { 0x9830u, 0x985Cu }, { 0x98F8u, 0x98FCu },
};

static void ajustes_por(r6_lote_t *l, int quando)
{
    for (int i = 0; i < gpu3d_ajustes.n; i++)
        if (gpu3d_ajustes.quando[i] == quando) eg3d_reg(l, gpu3d_ajustes.reg[i], gpu3d_ajustes.val[i]);
}

static void esperar_ms(uint32_t ms)
{
    prazo_t p = prazo_ms(ms);
    while (!prazo_vencido(&p)) { }
}

static const char *nome_etapa(int e) { return e >= 0 && e <= 6 ? nomes_etapa[e] : "sin intentar"; }
/* D432: presentar en la pantalla (abajo, «presentar en la pantalla»). */
static uint32_t apres_n, apres_vblank_fora;
static const char *apres_porque = "sin comprobar";

/* Una dirección del MC de 40 bits como «alto:bajo» (como el 2D, D408). */
#define MC_HI(x) ((uint32_t)((x) >> 32))
#define MC_LO(x) ((uint32_t)(x))

static void eg_detalle(const char *fase)
{
    klog("[GPU3D] evergreen detalle (%s): ultima etapa del encendido: %d de 6 (%s); CP %s\n", fase, etapa, nome_etapa(etapa),
         cp_ligado ? "en marcha" : "parado");
    if (!base.mmio || !sondado) return;
    klog("[GPU3D] evergreen detalle (%s): GRBM_STATUS %x STATUS_SE0 %x SRBM_STATUS %x CP_STAT %x CP_ME_CNTL %x\n", fase,
         rd(R6_GRBM_STATUS), rd(R6_GRBM_STATUS2), rd(SRBM_STATUS), rd(R6_CP_STAT), rd(R6_CP_ME_CNTL));
    klog("[GPU3D] evergreen detalle (%s): CP_RB_CNTL %x BASE %x (cola en MC %x:%x) RPTR %x WPTR %x (escrito %x) apuntes: prueba %x "
         "valla %x (esperada %x)\n", fase, rd(R6_CP_RB_CNTL), rd(R6_CP_RB_BASE), MC_HI(mc_zona), MC_LO(mc_zona), rd(R6_CP_RB_RPTR),
         rd(R6_CP_RB_WPTR), wptr, rd(SCRATCH_TESTE), rd(SCRATCH_VALLA), seq);
    klog("[GPU3D] evergreen detalle (%s): GB_ADDR_CONFIG %x (BIOS %x) GB_BACKEND_MAP %x (BIOS %x) SQ_CONFIG %x MC_SEQ_SUP_CNTL %x "
         "VM_CONTEXT0_CNTL %x\n", fase, rd(EG3D_GB_ADDR_CONFIG), gb_addr_lido, rd(EG3D_GB_BACKEND_MAP), gb_map_lido, rd(EG3D_SQ_CONFIG),
         rd(EG3D_MC_SEQ_SUP_CNTL), rd(0x1410));
    klog("[GPU3D] evergreen detalle (%s): zona 3D en MC %x:%x, %u KB, libres %u KB; destino %dx%d paso %d, color MC %x:%x, "
         "Z MC %x:%x (%d bits)\n", fase, MC_HI(mc_zona), MC_LO(mc_zona), zona_bytes >> 10,
         cp_ligado ? (uint32_t)((g3d_arena_livre(&arena) + (arena_g_ok ? g3d_arena_livre(&arena_g) : 0)) >> 10) : 0, alvo_l, alvo_a,
         alvo_passo, MC_HI(b_cor.gpu), MC_LO(b_cor.gpu), MC_HI(b_z.gpu), MC_LO(b_z.gpu), alvo_zbits);
    uint32_t khz = hw_tsc_khz();
    if (khz && n_ler)
        klog("[GPU3D] evergreen detalle (%s): %u cuadros leidos (%u kpx): esperando la valla %u us, copiando a la RAM %u us "
             "(%u us por cuadro)\n", fase, n_ler, n_ler_px / 1000u, (uint32_t)(t_valla * 1000u / khz), (uint32_t)(t_copia * 1000u / khz),
             (uint32_t)((t_valla + t_copia) * 1000u / khz / n_ler));
    klog("[GPU3D] evergreen detalle (%s): presentar (D432): %s; %u copias a la pantalla, %u sin retrazado a tiempo\n", fase,
         apres_porque, apres_n, apres_vblank_fora);
}

/* ------------------------------------------------------------------ sonda (solo lectura) */

static bool eg_sondar(const pci_disp_t *d, int geracao, const acel_tela_t *t, char *m, int nm)
{
    sondado = false;
    if (geracao != EVG_GEN_EVERGREEN) {
        snformatar(m, (size_t)nm, "generacion %d: el 3D de Evergreen/NI no lleva Cayman (VLIW4, otro CF) ni otras", geracao);
        return false;
    }
    if (!r600_sondar(d, geracao, t, &base, m, nm)) return false;
    tela = *t;
    familia = eg3d_familia_por_nome(base.nome);
    chip = eg3d_chip(familia);
    if (!chip || base.gen != EVG_GEN_EVERGREEN) {
        snformatar(m, (size_t)nm, "%s (%04x): familia sin 3D todavia", base.nome, base.modelo);
        return false;
    }
    if (base.vm0 & 1) {
        snformatar(m, (size_t)nm, "la memoria virtual de la GPU esta encendida (VM_CONTEXT0_CNTL %x): no se arriesga", base.vm0);
        return false;
    }
    mc_tela = ((uint64_t)base.mc_alto << 32) | base.mc_tela;
    /* Dónde cabe la zona 3D (bytes desde la pantalla): detrás de lo del 2D, dentro de la VRAM real
     * (CONFIG_MEMSIZE), de los primeros 64 MB del BAR 0 y de lo que el MC llama memoria de vídeo. */
    uint64_t lim = t->memoria;
    if (base.memsize >= (16u << 20) && base.memsize != 0xFFFFFFFFu) lim = base.memsize;
    lim = lim > base.desloc ? lim - base.desloc : 0;
    if (lim > (uint64_t)BAR_SEGURO - base.desloc) lim = base.desloc < BAR_SEGURO ? (uint64_t)BAR_SEGURO - base.desloc : 0;
    if (mc_tela + lim - 1 > base.vram_fim) lim = base.vram_fim + 1 > mc_tela ? base.vram_fim + 1 - mc_tela : 0;
    if (mc_tela < base.vram_ini) lim = 0;
    /* D386: lo que muestra la tarjeta (la pantalla con lo del 2D, las superficies de los otros
     * controladores con la imagen activa y los cursores), en bytes desde la pantalla. */
    vram_ocupado_t oc[1 + 2 * EVG_MAX_CRTC];
    int noc = 0;
    oc[noc].ini = 0; oc[noc++].fim = r600_fim_2d(t);
    const evg_modelo_t *em = evg_modelo(base.modelo);
    int ncrtc = em ? em->crtcs : 2;
    for (int c = 0; c < ncrtc && c < EVG_MAX_CRTC; c++) {
        uint32_t o = evg_crtc_desloc(c);
        uint32_t cr = rd(EVG_CRTC_CONTROL + o), en = rd(EVG_GRPH_ENABLE + o), cc = rd(EVG_CUR_CONTROL + o);
        uint64_t sup = ((uint64_t)(rd(EVG_GRPH_SUPERFICIE_HI + o) & 0xFFu) << 32) | rd(EVG_GRPH_SUPERFICIE + o);
        uint64_t cs = ((uint64_t)(rd(EVG_CUR_SUPERFICIE_HI + o) & 0xFFu) << 32) | rd(EVG_CUR_SUPERFICIE + o);
        if (!(cr & 1) && !(en & 1) && !(cc & 1)) continue;   /* apagado: sin línea en el log */
        uint64_t bytes = avivo_superficie_bytes(rd(EVG_GRPH_CONTROL + o), rd(EVG_GRPH_PITCH + o), rd(EVG_GRPH_Y_FIM + o),
                                                rd(EVG_GRPH_DESL_Y + o), rd(EVG_VIEWPORT_INI + o), rd(EVG_VIEWPORT_TAM + o));
        klog("[GPU3D] evergreen: controlador %d CRTC %s, GRPH %s, superficie %x:%x: %u KB%s; cursor %s en MC %x:%x\n", c + 1,
             cr & 1 ? "encendido" : "apagado", en & 1 ? "activo" : "apagado", MC_HI(sup), MC_LO(sup), (uint32_t)(bytes >> 10),
             bytes == VRAM_MARGEM_DESCONHECIDA ? " (tamano desconocido: margen de 8 MB)" : "", cc & 1 ? "encendido" : "apagado",
             MC_HI(cs), MC_LO(cs));
        /* Superficies por debajo de la pantalla: solo cuenta lo que llegue a pasarla. */
        if ((en & 1) && sup + bytes > mc_tela) {
            oc[noc].ini = sup > mc_tela ? sup - mc_tela : 0;
            oc[noc++].fim = sup + bytes - mc_tela;
        }
        if ((cc & 1) && cs + 0x4000u > mc_tela) {
            oc[noc].ini = cs > mc_tela ? cs - mc_tela : 0;
            oc[noc++].fim = cs + 0x4000u - mc_tela;
        }
    }
    uint64_t ini64;
    uint32_t tam;
    if (!vram_zona_buscar(oc, noc, oc[0].fim, lim, ZONA_MIN, ZONA_MAX, 1u << 20, &ini64, &tam)) {
        snformatar(m, (size_t)nm, "no caben 4 MB para el 3D detras del 2D y de las superficies activas (VRAM util %u KB, 2D hasta %u KB)",
                   (uint32_t)(lim >> 10), (uint32_t)(oc[0].fim >> 10));
        return false;
    }
    if ((mc_tela + ini64) & 0xFFu) {
        snformatar(m, (size_t)nm, "la zona 3D (MC %x:%x) no esta alineada a 256 bytes", MC_HI(mc_tela + ini64), MC_LO(mc_tela + ini64));
        return false;
    }
    int pisa = vram_zona_pisa(oc, noc, ini64, tam);
    if (pisa >= 0) {
        snformatar(m, (size_t)nm, "la zona 3D (%u KB desde la pantalla) pisa lo que muestra la tarjeta", (uint32_t)(ini64 >> 10));
        return false;
    }
    zona_off = (uint32_t)ini64; zona_bytes = tam;
    mc_zona = mc_tela + ini64;
    klog("[GPU3D] evergreen: %s (%04x), familia %d (%s, microcodigo %s); zona 3D: %u KB en MC %x:%x (a %u KB de la pantalla); "
         "VRAM %u MB, VBE %u KB\n", base.nome, base.modelo, familia, chip->nome, chip->firmware, tam >> 10, MC_HI(mc_zona),
         MC_LO(mc_zona), zona_off >> 10, base.memsize >> 20, t->memoria >> 10);
    klog("[GPU3D] evergreen: antes de tocar nada: CP_ME_CNTL %x CP_RB_CNTL %x GRBM_STATUS %x CP_STAT %x CP_INT_CNTL %x\n",
         rd(R6_CP_ME_CNTL), rd(R6_CP_RB_CNTL), rd(R6_GRBM_STATUS), rd(R6_CP_STAT), rd(R6_CP_INT_CNTL));
    sondado = true;
    return true;
}

/* ------------------------------------------------------------------ la cola */

static void hdp(void) { r600_hdp_vaciar(&base, (volatile uint32_t *)(zona + RING_BYTES)); }

/* La zona tiene dos partes (como en las R7xx, D402): la visible (los primeros VISIVEL_MAX bytes) con
 * ventana fija de la CPU (cola, sombreadores, vértices y destino de color) y la grande (el resto) sin
 * ventana: el Z (la CPU no lo toca) y las texturas, que la CPU escribe una vez por una ventana móvil. */
static g3d_arena_t *arena_de(const g3d_buffer_t *b)
{
    return arena_g_ok && b->gpu >= arena_g.gpu && b->gpu < arena_g.gpu + arena_g.bytes ? &arena_g : &arena;
}
static void soltar(g3d_buffer_t *b) { if (b->usado) g3d_buffer_soltar(arena_de(b), b); }
static bool criar_grande(g3d_buffer_t *b, uint8_t tipo, uint64_t bytes)
{
    if (arena_g_ok && g3d_buffer_criar(&arena_g, b, tipo, bytes, 4096)) return true;
    return g3d_buffer_criar(&arena, b, tipo, bytes, 4096);
}
static volatile uint8_t *ver_grande(const g3d_buffer_t *b)
{
    if (b->cpu) return b->cpu;
    if (!jmovel) jmovel = janela_reservar(JMOVEL_BYTES);
    if (!jmovel || b->bytes > JMOVEL_BYTES) return NULL;
    uint32_t fis = tela.fb_fisico + zona_off + (uint32_t)(b->gpu - mc_zona);
    for (uint32_t o = 0; o < (uint32_t)b->bytes; o += 4096) {
        janela_soltar(jmovel + o);                    /* quita lo de antes e invalida el TLB */
        janela_pagina(jmovel + o, fis + o, PG_PCD | PG_PWT);
    }
    return (volatile uint8_t *)jmovel;
}

static uint32_t livre(void) { return ((rd(R6_CP_RB_RPTR) & (RING_DW - 1)) - wptr - 1) & (RING_DW - 1); }

static void avisar(void)
{
    while (wptr & 15) { ring[wptr] = NOP_TIPO2; wptr = (wptr + 1) & (RING_DW - 1); }
    hdp();
    wr(R6_CP_RB_WPTR, wptr);
    (void)rd(R6_CP_RB_WPTR);
}

static bool emitir(const r6_lote_t *l)
{
    if (!cp_ligado || l->cheio) {
        if (l->cheio) klog("[GPU3D] evergreen: un lote de ordenes no cupo en su bufer (error interno)\n");
        return false;
    }
    if (livre() < (uint32_t)l->n + 32) {
        avisar();
        prazo_t p = prazo_ms(PRAZO_MS);
        while (livre() < (uint32_t)l->n + 32)
            if (prazo_vencido(&p)) { klog("[GPU3D] evergreen: la cola no avanza (RPTR %x, WPTR %x)\n", rd(R6_CP_RB_RPTR), wptr); return false; }
    }
    for (int i = 0; i < l->n; i++) { ring[wptr] = l->p[i]; wptr = (wptr + 1) & (RING_DW - 1); }
    return true;
}

static bool submeter(g3d_vala_t *valla)
{
    uint32_t b[40];
    r6_lote_t l;
    r63d_lote(&l, b, 40);
    seq++;
    if (!seq) seq = 1;
    eg3d_valla(&l, SCRATCH_VALLA, seq, chip->cache_vertices);
    if (!emitir(&l)) return false;
    avisar();
    *valla = seq;
    return true;
}

static bool valla_esperar(g3d_vala_t v, uint32_t ms)
{
    prazo_t p = prazo_ms(ms);
    while (rd(SCRATCH_VALLA) != v)
        if (prazo_vencido(&p)) {
            klog("[GPU3D] evergreen: la valla %x no llega (apunte %x, RPTR %x, WPTR %x, GRBM_STATUS %x, CP_STAT %x)\n", v,
                 rd(SCRATCH_VALLA), rd(R6_CP_RB_RPTR), wptr, rd(R6_GRBM_STATUS), rd(R6_CP_STAT));
            return false;
        }
    return true;
}

/* ------------------------------------------------------------------ encender y apagar */

static void cp_parar(void)
{
    wr(R6_CP_ME_CNTL, R6_CP_ME_HALT | R6_CP_PFP_HALT);   /* r700_cp_stop */
    wr(R6_SCRATCH_UMSK, 0);
    cp_ligado = false;
}

/* evergreen_cp_resume: con el CP se reinician los bloques del 3D que dependen de él (nunca el motor
 * entero, ni SRBM). */
static void cp_reiniciar(void)
{
    wr(R6_GRBM_SOFT_RESET, EG3D_SOFT_RESET_CP_Y_3D);
    (void)rd(R6_GRBM_SOFT_RESET);
    esperar_ms(15);
    wr(R6_GRBM_SOFT_RESET, 0);
    (void)rd(R6_GRBM_SOFT_RESET);
}

static inline uint32_t be32(const uint8_t *p) { return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3]; }

static bool ligar_cp(char *m, int nm)
{
    if (!sondado) { snformatar(m, (size_t)nm, "sin sondar"); return false; }
    etapa = 1;
    cp_ligado = false;
    tocou = false;
    sh_atual = -1;
    est_valido = false;
    pendente = false;
    alvo_l = alvo_a = 0;
    memset(&b_vb, 0, sizeof b_vb); memset(&b_cor, 0, sizeof b_cor); memset(&b_z, 0, sizeof b_z); memset(b_tex, 0, sizeof b_tex);
    arena_g_ok = false;

    /* Etapa 1: el microcódigo, del sistema de archivos. */
    char cam[64];
    uint8_t *pfp = NULL, *me = NULL;
    uint32_t npfp = 0, nme = 0;
    snformatar(cam, sizeof cam, "/lib/firmware/radeon/%s_pfp.bin", chip->firmware);
    if (vfs_ler_inteiro(cam, &pfp, &npfp) < 0 || npfp != EG3D_PFP_PALAVRAS * 4) {
        snformatar(m, (size_t)nm, "falta el microcodigo %s (o no mide %u bytes: %u)", cam, EG3D_PFP_PALAVRAS * 4, npfp);
        if (pfp) kfree(pfp);
        return false;
    }
    snformatar(cam, sizeof cam, "/lib/firmware/radeon/%s_me.bin", chip->firmware);
    if (vfs_ler_inteiro(cam, &me, &nme) < 0 || nme != EG3D_ME_PALAVRAS * 4) {
        snformatar(m, (size_t)nm, "falta el microcodigo %s (o no mide %u bytes: %u)", cam, EG3D_ME_PALAVRAS * 4, nme);
        kfree(pfp);
        if (me) kfree(me);
        return false;
    }
    klog("[GPU3D] evergreen: etapa 1: microcodigo %s leido (PFP %u bytes, ME %u bytes)\n", chip->firmware, npfp, nme);

    /* La ventana de la CPU sobre la zona (una vez; las ventanas no se devuelven). */
    uint32_t fis = tela.fb_fisico + zona_off;
    zona_vis = zona_bytes < VISIVEL_MAX ? zona_bytes : VISIVEL_MAX;
    if (!zona || zona_fis_map != fis || zona_bytes_map != zona_vis) {
        zona = (volatile uint8_t *)janela_mapear_tipo(fis, zona_vis, PG_PCD | PG_PWT);
        zona_fis_map = fis; zona_bytes_map = zona_vis;
    }
    if (!zona) { snformatar(m, (size_t)nm, "sin ventana del kernel para la zona 3D"); kfree(pfp); kfree(me); return false; }
    ring = (volatile uint32_t *)zona;
    /* ¿La CPU ve de verdad esa memoria? Escribir y leer una palabra de la página de sombreadores. */
    volatile uint32_t *sh = (volatile uint32_t *)(zona + RING_BYTES);
    sh[4095] = 0x5A6D3D00u; sh[4094] = ~0x5A6D3D00u;
    if (sh[4095] != 0x5A6D3D00u || sh[4094] != ~0x5A6D3D00u) {
        snformatar(m, (size_t)nm, "la VRAM de la zona 3D no guarda lo escrito (%x): no es memoria utilizable", sh[4095]);
        kfree(pfp); kfree(me);
        return false;
    }
    /* La última página de la parte grande también tiene que guardar lo escrito (por la ventana móvil). */
    if (zona_bytes > zona_vis) {
        g3d_buffer_t ult = { true, G3D_BUF_TEXTURA, mc_zona + zona_bytes - 4096u, 4096, NULL, 0, 0, 0, 0 };
        volatile uint32_t *u = (volatile uint32_t *)ver_grande(&ult);
        if (!u) { snformatar(m, (size_t)nm, "sin ventana movil del kernel para la zona 3D grande"); kfree(pfp); kfree(me); return false; }
        u[1023] = 0x5A6D3D01u; u[1022] = ~0x5A6D3D01u;
        if (u[1023] != 0x5A6D3D01u || u[1022] != ~0x5A6D3D01u) {
            snformatar(m, (size_t)nm, "la VRAM al final de la zona 3D (MC %x:%x) no guarda lo escrito (%x)",
                       MC_HI(mc_zona + zona_bytes - 4096u), MC_LO(mc_zona + zona_bytes - 4096u), u[1023]);
            kfree(pfp); kfree(me);
            return false;
        }
    }
    uint32_t gs = rd(R6_GRBM_STATUS);
    if (gs & R6_GUI_ACTIVE) {
        snformatar(m, (size_t)nm, "el motor grafico esta ocupado antes de empezar (GRBM_STATUS %x): no se toca", gs);
        kfree(pfp); kfree(me);
        return false;
    }

    /* Etapa 2: parar el CP y cargarle el microcódigo (evergreen_cp_load_microcode: PFP y luego ME). */
    etapa = 2;
    tocou = true;
    cp_parar();
    wr(R6_CP_RB_CNTL, R6_RB_NO_UPDATE | (15u << 8) | 3u);
    wr(R6_CP_PFP_UCODE_ADDR, 0);
    for (uint32_t i = 0; i < EG3D_PFP_PALAVRAS; i++) wr(R6_CP_PFP_UCODE_DATA, be32(pfp + i * 4));
    wr(R6_CP_PFP_UCODE_ADDR, 0);
    wr(R6_CP_ME_RAM_WADDR, 0);
    for (uint32_t i = 0; i < EG3D_ME_PALAVRAS; i++) wr(R6_CP_ME_RAM_DATA, be32(me + i * 4));
    wr(R6_CP_PFP_UCODE_ADDR, 0);
    wr(R6_CP_ME_RAM_WADDR, 0);
    wr(R6_CP_ME_RAM_RADDR, 0);
    uint32_t me0 = be32(me);
    kfree(pfp); kfree(me);
    klog("[GPU3D] evergreen: etapa 2: CP parado y microcodigo cargado (primera palabra del ME %x; CP_STAT %x)\n", me0, rd(R6_CP_STAT));

    /* Etapa 3: valores por defecto del motor 3D (por MMIO, lo de evergreen_gpu_init que es del 3D). */
    etapa = 3;
    eg3d_leituras_t le;
    memset(&le, 0, sizeof le);
    le.mc_arb_ramcfg = rd(EG3D_MC_ARB_RAMCFG);
    le.gb_addr_config = gb_addr_lido = rd(EG3D_GB_ADDR_CONFIG);
    le.gb_backend_map = gb_map_lido = rd(EG3D_GB_BACKEND_MAP);
    /* Los backends apagados de cada motor de sombreado (4 bits cada uno, el último motor primero, como Linux). */
    for (int se = chip->num_ses - 1; se >= 0; se--) {
        wr(EG3D_GRBM_GFX_INDEX, EG3D_GFX_SE(se));
        wr(EG3D_RLC_GFX_INDEX, EG3D_GFX_SE(se));
        le.rb_desativados = (le.rb_desativados << 4) | ((rd(EG3D_CC_RB_BACKEND_DISABLE) >> 16) & 0xFu);
    }
    wr(EG3D_GRBM_GFX_INDEX, EG3D_GFX_BROADCAST);
    wr(EG3D_RLC_GFX_INDEX, EG3D_GFX_BROADCAST);
    le.sx_debug_1 = rd(EG3D_SX_DEBUG_1);
    le.smx_dc_ctl0 = rd(EG3D_SMX_DC_CTL0);
    le.sq_config = sq_config_lido = rd(EG3D_SQ_CONFIG);
    le.sq_lds_resource_mgmt = rd(EG3D_SQ_LDS_RESOURCE_MGMT);
    mc_seq_sup = rd(EG3D_MC_SEQ_SUP_CNTL);
    static r6_par_t pares[EG3D_INICIO_MAX];
    uint32_t max_ctx = 4;
    int np = eg3d_inicio_motor(familia, &le, pares, EG3D_INICIO_MAX, &max_ctx);
    if (np < 0) { snformatar(m, (size_t)nm, "sin tabla de inicio para la familia %d", familia); return false; }
    for (int i = 0; i < np; i++) wr(pares[i].reg, pares[i].valor);
    gb_addr_usado = eg3d_gb_addr_config(familia, &le);
    gb_map_usado = eg3d_gb_backend_map(familia, &le);
    alin_passo = eg3d_alinhamento_passo(gb_addr_usado);
    bancos = eg3d_bancos(le.mc_arb_ramcfg);
    klog("[GPU3D] evergreen: etapa 3: motor 3D con %d registros por defecto (MC_ARB_RAMCFG %x, GB_ADDR_CONFIG %x -> %x, "
         "GB_BACKEND_MAP %x -> %x, backends apagados %x, SQ_CONFIG %x, secuenciador del MC %s (MC_SEQ_SUP_CNTL %x); paso alineado "
         "a %d pixeles, NUM_BANKS %u)\n", np, le.mc_arb_ramcfg, gb_addr_lido, gb_addr_usado, gb_map_lido, gb_map_usado,
         le.rb_desativados, sq_config_lido, mc_seq_sup & 1 ? "en marcha" : "PARADO", mc_seq_sup, alin_passo, bancos);

    /* Etapa 4: la cola (64 KB en VRAM), sin escritura de vuelta del puntero (evergreen_cp_resume). */
    etapa = 4;
    cp_reiniciar();
    uint32_t cntl = (9u << 8) | 13u;   /* log2(4096 / 8) y log2(65536 / 8) */
    wr(R6_CP_RB_CNTL, cntl);
    wr(R6_CP_SEM_WAIT_TIMER, 0);
    wr(EG3D_CP_SEM_INCOMPLETE_TIMER_CNTL, 0);
    wr(R6_CP_RB_WPTR_DELAY, 0);
    wr(R6_CP_RB_CNTL, cntl | R6_RB_RPTR_WR_ENA);
    wr(R6_CP_RB_RPTR_WR, 0);
    wptr = 0;
    wr(R6_CP_RB_WPTR, 0);
    uint64_t pag = mc_zona + RING_BYTES + 0x1000u;   /* no se usan (NO_UPDATE, UMSK 0), pero apuntan a memoria nuestra */
    wr(R6_CP_RB_RPTR_ADDR, (uint32_t)pag & 0xFFFFFFFCu);
    wr(R6_CP_RB_RPTR_ADDR_HI, MC_HI(pag) & 0xFFu);
    wr(EG3D_SCRATCH_ADDR, (uint32_t)(pag >> 8));
    wr(R6_SCRATCH_UMSK, 0);
    cntl |= R6_RB_NO_UPDATE;
    esperar_ms(1);
    wr(R6_CP_RB_CNTL, cntl);
    wr(R6_CP_RB_BASE, (uint32_t)(mc_zona >> 8));      /* bits 39:8 */
    wr(R6_CP_DEBUG, (1u << 27) | (1u << 28));
    for (uint32_t i = 0; i < RING_DW; i++) ring[i] = NOP_TIPO2;

    /* Etapa 5: arrancar el CP (ME_INITIALIZE, como en las R7xx) y probar la cola con un registro de apuntes. */
    static uint32_t b[2048];
    r6_lote_t l;
    r63d_lote(&l, b, 16);
    r63d_me_initialize(&l, true, max_ctx);
    cp_ligado = true;
    for (int i = 0; i < l.n; i++) { ring[wptr] = l.p[i]; wptr++; }
    avisar();
    wr(R6_CP_ME_CNTL, 0xFF);
    klog("[GPU3D] evergreen: etapa 4: cola en MC %x:%x (CP_RB_BASE %x), CP en marcha (CP_ME_CNTL %x, RPTR %x, WPTR %x)\n",
         MC_HI(mc_zona), MC_LO(mc_zona), rd(R6_CP_RB_BASE), rd(R6_CP_ME_CNTL), rd(R6_CP_RB_RPTR), wptr);
    etapa = 5;
    wr(SCRATCH_TESTE, 0xCAFEDEADu);
    r63d_lote(&l, b, 16);
    eg3d_reg(&l, SCRATCH_TESTE, 0xDEADBEEFu);
    if (!emitir(&l)) { snformatar(m, (size_t)nm, "la cola del CP no avanza tras arrancarlo (RPTR %x)", rd(R6_CP_RB_RPTR)); return false; }
    avisar();
    prazo_t pz = prazo_ms(PRAZO_MS);
    while (rd(SCRATCH_TESTE) != 0xDEADBEEFu)
        if (prazo_vencido(&pz)) {
            snformatar(m, (size_t)nm, "el CP no ejecuta la prueba de la cola (apunte %x, RPTR %x, WPTR %x, CP_STAT %x)", rd(SCRATCH_TESTE),
                       rd(R6_CP_RB_RPTR), wptr, rd(R6_CP_STAT));
            return false;
        }
    klog("[GPU3D] evergreen: etapa 5: prueba de la cola superada (el CP escribio el registro de apuntes; RPTR %x)\n", rd(R6_CP_RB_RPTR));

    /* Etapa 6: sombreadores a la VRAM y estado base del contexto, con una valla. */
    etapa = 6;
    for (int i = 0; i < 3 * (EG3D_SH_PASSO / 4); i++) sh[i] = 0;
    uint32_t s[EG3D_SH_MAX];
    int n = eg3d_vs(s);
    for (int i = 0; i < n; i++) sh[i] = s[i];
    n = eg3d_ps_cor(s);
    for (int i = 0; i < n; i++) sh[EG3D_SH_PASSO / 4 + i] = s[i];
    n = eg3d_ps_tex(s);
    for (int i = 0; i < n; i++) sh[2 * EG3D_SH_PASSO / 4 + i] = s[i];
    g3d_arena_iniciar(&arena, mc_zona + FIXO, (uint8_t *)zona + FIXO, zona_vis - FIXO);
    arena_g_ok = zona_bytes > zona_vis;
    if (arena_g_ok) g3d_arena_iniciar(&arena_g, mc_zona + zona_vis, NULL, zona_bytes - zona_vis);
    klog("[GPU3D] evergreen: etapa 6: zona de %u KB: %u KB con ventana fija de la CPU y %u KB sin ella (Z y texturas)\n",
         zona_bytes >> 10, zona_vis >> 10, arena_g_ok ? (zona_bytes - zona_vis) >> 10 : 0);
    if (!g3d_buffer_criar(&arena, &b_vb, G3D_BUF_LOTE, VB_BYTES, 4096)) { snformatar(m, (size_t)nm, "sin sitio para los vertices"); return false; }
    vb_usado = 0;
    r63d_lote(&l, b, 2048);
    eg3d_estado_base(&l, familia, sq_config_lido, mc_zona + RING_BYTES);
    ajustes_por(&l, G3D_AJ_INICIO);    /* D390: /sistema/conf/gpu3d-evergreen.conf, tras el estado base */
    seq = 0x3D000000u;
    wr(SCRATCH_VALLA, seq);
    g3d_vala_t v;
    if (!emitir(&l) || !submeter(&v) || !valla_esperar(v, PRAZO_MS)) {
        snformatar(m, (size_t)nm, "el CP no termina el estado base (%d palabras; apunte %x, RPTR %x, GRBM_STATUS %x)", l.n,
                   rd(SCRATCH_VALLA), rd(R6_CP_RB_RPTR), rd(R6_GRBM_STATUS));
        return false;
    }
    klog("[GPU3D] evergreen: etapa 6: sombreadores en MC %x:%x y estado base (%d palabras) aceptados; GRBM_STATUS %x\n",
         MC_HI(mc_zona + RING_BYTES), MC_LO(mc_zona + RING_BYTES), l.n, rd(R6_GRBM_STATUS));
    return true;
}

static void eg_desligar(void)
{
    if (!base.mmio || !sondado) return;
    if (cp_ligado) {
        prazo_t p = prazo_ms(50);
        while (rd(R6_GRBM_STATUS) & R6_GUI_ACTIVE)
            if (prazo_vencido(&p)) { klog("[GPU3D] evergreen: el motor sigue ocupado al parar el CP (GRBM_STATUS %x)\n", rd(R6_GRBM_STATUS)); break; }
    }
    cp_parar();
    alvo_l = alvo_a = 0;
    pendente = false;
    klog("[GPU3D] evergreen: CP parado (CP_ME_CNTL %x); la pantalla sigue como estaba\n", rd(R6_CP_ME_CNTL));
}

/* «etapa N (nombre): motivo», para que «gpu3d probar» diga dónde se paró sin mirar el log (D357). */
static void prefixar_etapa(char *m, int nm)
{
    char t[160];
    snformatar(t, sizeof t, "etapa %d (%s): %s", etapa, nome_etapa(etapa), m);
    snformatar(m, (size_t)nm, "%s", t);
}

static int eg_iniciar(const pci_disp_t *d, int geracao, const acel_tela_t *t, uint32_t variante, char *m, int nm)
{
#ifdef SAGUI_SEM_FIRMWARE
    (void)d; (void)geracao; (void)t; (void)variante; (void)eg_sondar; (void)ligar_cp; (void)prefixar_etapa;
    snformatar(m, (size_t)nm, "esta compilacion no lleva firmware (SEM_FIRMWARE=1): sin microcodigo no hay 3D");
    return G3D_NAO;
#else
    etapa = 0;
    if (variante & GPU3D_CAB_GTT) klog("[GPU3D] evergreen: «gtt» no hace nada aqui (sin GART en Evergreen/NI todavia): el cuadro vuelve por la CPU\n");
    if (!eg_sondar(d, geracao, t, m, nm)) { prefixar_etapa(m, nm); return G3D_NAO; }   /* solo leyó: no es un fallo de la GPU */
    if (ligar_cp(m, nm)) return G3D_OK;
    prefixar_etapa(m, nm);
    klog("[GPU3D] evergreen: el encendido se paro en la etapa %d (%s)\n", etapa, nome_etapa(etapa));
    return tocou ? G3D_FALHA : G3D_NAO;
#endif
}

/* ------------------------------------------------------------------ destino, texturas y dibujos */

static int eg_esperar(g3d_vala_t vala, uint32_t ms)
{
    if (!cp_ligado) return G3D_FALHA;
    if (pendente) {
        g3d_vala_t v;
        if (!submeter(&v)) return G3D_FALHA;
        pendente = false;
        vb_usado = 0;
        vala = v;                    /* la última valla cubre todas las anteriores */
    } else if (!vala) return G3D_OK;
    else vala = seq;
    return valla_esperar(vala, ms) ? G3D_OK : G3D_FALHA;
}

static int eg_alvo(int larg, int alt, int zbits)
{
    if (!cp_ligado) return G3D_FALHA;
    if (larg > MAX_ALVO || alt > MAX_ALVO) return G3D_NAO;
    if (eg_esperar(0, GPU3D_PRAZO_MS) != G3D_OK) return G3D_FALHA;
    soltar(&b_cor);
    soltar(&b_z);
    alvo_l = alvo_a = 0;
    int passo = r63d_passo(larg, alin_passo), alt_buf = (alt + 63) & ~63;   /* holgura para el mosaico del Z */
    uint32_t bytes = (uint32_t)passo * (uint32_t)alt_buf * 4u;
    if (!g3d_buffer_criar(&arena, &b_cor, G3D_BUF_ALVO, bytes, 4096)) return G3D_NAO;
    if (!criar_grande(&b_z, G3D_BUF_Z, bytes)) { soltar(&b_cor); return G3D_NAO; }
    b_z.cpu = NULL;                  /* el Z va en mosaico: la CPU no lo toca */
    zbits = zbits == 16 ? 16 : 24;
    uint32_t b[80];
    r6_lote_t l;
    r63d_lote(&l, b, 80);
    eg3d_alvo(&l, b_cor.gpu, b_z.gpu, passo, alt_buf, larg, alt, zbits, bancos);
    if (!emitir(&l)) return G3D_FALHA;
    alvo_l = larg; alvo_a = alt; alvo_passo = passo; alvo_zbits = zbits;
    b_cor.passo = (uint32_t)passo * 4; b_cor.larg = larg; b_cor.alt = alt; b_cor.bpp = 32;
    est_valido = false;
    pendente = true;
    return G3D_OK;
}

/* Pone sombreadores, textura y estado si cambiaron. «limpar»: 0 no; si no, la máscara del borrado. */
static bool por_estado(const gpu3d_estado_t *e, uint32_t limpar)
{
    static const uint8_t zf[3] = { 1, 3, 7 }, fat[4] = { 0, 1, 4, 5 };
    const g3d_buffer_t *t = NULL;
    if (!limpar && (e->flags & G3D_TEXTURA) && e->textura < GPU3D_TEXTURAS && b_tex[e->textura].usado) t = &b_tex[e->textura];
    if (!limpar && est_valido && est_posto.flags == e->flags && est_posto.func_z == e->func_z && est_posto.mez_orig == e->mez_orig &&
        est_posto.mez_dest == e->mez_dest && est_posto.textura == e->textura) return true;
    uint32_t b[112 + 3 * G3D_AJUSTES_MAX];
    r6_lote_t l;
    r63d_lote(&l, b, 112 + 3 * G3D_AJUSTES_MAX);
    int quer = t ? 1 : 0;
    if (quer != sh_atual) {
        eg3d_programas(&l, mc_zona + RING_BYTES, mc_zona + RING_BYTES + (quer ? 2u * EG3D_SH_PASSO : EG3D_SH_PASSO));
        sh_atual = quer;
    }
    if (t) eg3d_textura(&l, t->gpu, t->larg, t->alt, (int)(t->passo / 4), !(e->flags & G3D_TEX_REPETE_S), !(e->flags & G3D_TEX_REPETE_T),
                        (e->flags & G3D_TEX_LINEAR) != 0, bancos);
    if (limpar)   /* el Z siempre pasa y se escribe con el valor del fondo; el color solo si se pide */
        eg3d_estado(&l, (limpar & 2) != 0, true, 7, false, 1, 0, (limpar & 1) != 0, 0, 0, alvo_l, alvo_a);
    else
        eg3d_estado(&l, (e->flags & G3D_Z_TESTE) != 0, (e->flags & G3D_Z_ESCRITA) != 0, zf[e->func_z <= 2 ? e->func_z : 0],
                    (e->flags & G3D_MEZCLA) != 0, fat[e->mez_orig & 3], fat[e->mez_dest & 3], true, 0, 0, alvo_l, alvo_a);
    ajustes_por(&l, G3D_AJ_DIBUJO);    /* D390: después del estado, para que no lo pise */
    if (!emitir(&l)) return false;
    if (limpar) est_valido = false;
    else { est_posto = *e; est_valido = true; }
    return true;
}

/* Copia n vértices al búfer de vértices (vaciándolo antes si no caben) y encola el dibujo. */
static int vertices(const gpu3d_vert_t *v, int n)
{
    uint32_t bytes = (uint32_t)n * EG3D_VERT_BYTES;
    if (bytes > b_vb.bytes) return G3D_NAO;
    if (vb_usado + bytes > b_vb.bytes) {
        pendente = true;             /* una valla nueva: hasta que llegue, la GPU puede estar leyendo estos vértices */
        if (eg_esperar(0, GPU3D_PRAZO_MS) != G3D_OK) return G3D_FALHA;
    }
    volatile uint32_t *d = (volatile uint32_t *)(b_vb.cpu + vb_usado);
    const uint32_t *o = (const uint32_t *)v;
    for (uint32_t i = 0; i < bytes / 4; i++) d[i] = o[i];
    uint32_t b[40];
    r6_lote_t l;
    r63d_lote(&l, b, 40);
    eg3d_desenhar(&l, b_vb.gpu + vb_usado, n, 4, chip->cache_vertices);
    if (!emitir(&l)) return G3D_FALHA;
    vb_usado += bytes;
    pendente = true;
    return G3D_OK;
}

static int eg_limpar(uint32_t mascara, uint32_t cor)
{
    if (!cp_ligado || !alvo_l) return G3D_FALHA;
    if (!(mascara & 3)) return G3D_OK;
    gpu3d_estado_t e = { 0, 0, 0, 0, 0 };
    if (!por_estado(&e, mascara & 3)) return G3D_FALHA;
    /* Borrar es dibujar un rectángulo al fondo (z = 1) que siempre pasa. */
    gpu3d_vert_t q[6];
    uint32_t x1 = r63d_float(alvo_l), y1 = r63d_float(alvo_a), c = cor | 0xFF000000u;
    q[0] = (gpu3d_vert_t){ 0, 0, R63D_F_1, R63D_F_1, c, 0, 0, 0 };
    q[1] = (gpu3d_vert_t){ x1, 0, R63D_F_1, R63D_F_1, c, 0, 0, 0 };
    q[2] = (gpu3d_vert_t){ 0, y1, R63D_F_1, R63D_F_1, c, 0, 0, 0 };
    q[3] = q[2]; q[4] = q[1];
    q[5] = (gpu3d_vert_t){ x1, y1, R63D_F_1, R63D_F_1, c, 0, 0, 0 };
    return vertices(q, 6);
}

static int eg_textura(int vaga, int larg, int alt, const uint32_t *argb)
{
    if (!cp_ligado) return G3D_FALHA;
    if (vaga < 0 || vaga >= GPU3D_TEXTURAS) return G3D_NAO;
    g3d_buffer_t *b = &b_tex[vaga];
    if (!argb && !b->usado) return G3D_OK;
    if (argb && (larg < 1 || alt < 1 || larg > MAX_TEX || alt > MAX_TEX)) return G3D_NAO;
    /* Que la GPU no esté leyendo la textura mientras se cambia o se suelta. */
    if (eg_esperar(0, GPU3D_PRAZO_MS) != G3D_OK) return G3D_FALHA;
    soltar(b);
    est_valido = false;
    if (!argb) return G3D_OK;
    int passo = r63d_passo(larg, alin_passo);   /* lineal alineada: múltiplo de 64 texels (o del grupo) */
    if (!criar_grande(b, G3D_BUF_TEXTURA, (uint64_t)passo * (uint32_t)((alt + 7) & ~7) * 4u)) return G3D_NAO;
    b->passo = (uint32_t)passo * 4; b->larg = larg; b->alt = alt; b->bpp = 32;
    volatile uint32_t *d = (volatile uint32_t *)ver_grande(b);
    if (!d) { soltar(b); return G3D_NAO; }
    for (int y = 0; y < alt; y++)
        for (int x = 0; x < larg; x++) d[y * passo + x] = argb[y * larg + x];
    return G3D_OK;
}

static int eg_desenhar(const gpu3d_estado_t *e, const gpu3d_vert_t *v, int n, g3d_vala_t *vala)
{
    if (!cp_ligado || !alvo_l) return G3D_FALHA;
    if (n <= 0) return G3D_OK;
    if (!por_estado(e, 0)) return G3D_FALHA;
    int r = vertices(v, n);
    if (r != G3D_OK) return r;
    avisar();                          /* la GPU empieza ya; la valla, cuando alguien espere */
    if (vala) {
        if (!submeter(vala)) return G3D_FALHA;
        pendente = false;
    }
    return G3D_OK;
}

static int eg_ler(uint32_t *dest, int passo_px)
{
    if (!cp_ligado || !alvo_l) return G3D_FALHA;
    /* Siempre una valla nueva: lo que se lee tiene que estar ya en la VRAM (cachés de color vaciadas). */
    pendente = true;
    uint64_t t0 = prazo_tsc();
    if (eg_esperar(0, GPU3D_PRAZO_MS) != G3D_OK) return G3D_FALHA;
    uint64_t t1 = prazo_tsc();
    t_valla += t1 - t0;
    volatile uint32_t *s = (volatile uint32_t *)b_cor.cpu;
    /* La ventana de la CPU es sin caché (UC): con SSE2 se lee de 16 en 16 bytes (D402). */
    bool sse2 = (fpu_recursos() & CPU_SSE2) != 0;
    for (int y = 0; y < alvo_a; y++) {
        uint32_t *d = dest + y * passo_px;
        if (sse2 && fpu_kernel_inicio()) {
            sse2_copiar(d, (const void *)(s + y * alvo_passo), (unsigned)alvo_l * 4u);
            fpu_kernel_fim();
            for (int x = 0; x < alvo_l; x++) d[x] &= 0x00FFFFFFu;
        } else
            for (int x = 0; x < alvo_l; x++) d[x] = s[y * alvo_passo + x] & 0x00FFFFFFu;
    }
    t_copia += prazo_tsc() - t1;
    n_ler++;
    n_ler_px += (uint32_t)(alvo_l * alvo_a);
    if (n_ler % 600 == 0) eg_detalle("cada 600 cuadros");
    return G3D_OK;
}

static void eg_memoria(uint32_t *total_kb, uint32_t *livre_kb)
{
    *total_kb = zona_bytes >> 10;
    *livre_kb = cp_ligado ? (uint32_t)((g3d_arena_livre(&arena) + (arena_g_ok ? g3d_arena_livre(&arena_g) : 0)) >> 10) : 0;
}

/* D390: lo que la CPU ve del destino: el color (ARGB8888 lineal). El Z va en mosaico: no se ve. */
static void eg_vistas(g3d_vista_t *c, g3d_vista_t *z)
{
    (void)z;
    if (!cp_ligado || !alvo_l || !b_cor.usado || !b_cor.cpu) return;
    *c = (g3d_vista_t){ b_cor.cpu, (uint32_t)alvo_passo * 4u, alvo_l, alvo_a, 32, 0, 0, false };
}

/* ------------------------------------------------------------------ D432: presentar en la pantalla
 *
 * Como en las R7xx (gpu3d_r600.c): la GPU copia (escalando) el destino del 3D, o una textura, a la
 * superficie que muestra el CRTC. La pantalla está en la misma VRAM, con su dirección de 40 bits (D408:
 * f:xxxxxxxx en el Toshiba; CB_COLOR0_BASE lleva los bits 39:8), y es ARGB8888 lineal: sirve de destino
 * de color con su paso y el modo de superficie de la BIOS (GRPH_CONTROL, bits 20-22: 0 lineal general,
 * 1 lineal alineada, lo que deja el VBE del Toshiba; en mosaico no se presenta). Pasos: una valla (lo
 * dibujado, a la VRAM); destino = la pantalla sin Z, bandas negras si se piden y el rectángulo con la
 * fuente como textura; otra valla esperada con plazo; y el destino del programa otra vez.
 * De la pantalla solo se LEEN registros (superficie, paso, modo, prioridad y, con G3D_APRES_VSYNC, el
 * bit de retrazado). La prioridad de la pantalla en el controlador de memoria (D424) la pone y la
 * mantiene el 2D (acel_r600.c): aquí no se toca; el log dice cómo está, porque sin ella una escritura
 * grande en la VRAM puede dejar a la pantalla sin datos (parpadeo). SIN PROBAR EN HARDWARE. */
#define EVG_CRTC_STATUS      0x6E8C        /* bit 0 = en el retrazado vertical */
#define EVG_PRIORITY_A_CNT   0x6B18        /* bit 20 = PRIORITY_ALWAYS_ON (D424) */
#define APRES_VBLANK_MS      20

static bool eg_tela_serve(uint32_t *modo)
{
    uint32_t o = evg_crtc_desloc(base.controlador), passo = tela.pitch / 4u;
    *modo = 0;
    if (!cp_ligado || !sondado) { apres_porque = "el CP no esta en marcha"; return false; }
    if (tela.bytes_px != 4 || (tela.pitch & 31u) || tela.larg < 8 || tela.alt < 8 || tela.larg > 8192 || tela.alt > 8192) {
        apres_porque = "la pantalla no es de 32 bits con el paso multiplo de 8 pixeles"; return false;
    }
    if ((int)video_largura() != tela.larg || (int)video_altura() != tela.alt || video_pitch() != tela.pitch ||
        !video_formato_direto() || video_bpp() != 32) { apres_porque = "el modo de video cambio desde el encendido"; return false; }
    uint32_t ctl = rd(EVG_GRPH_CONTROL + o), pit = rd(EVG_GRPH_PITCH + o);
    uint64_t sup = ((uint64_t)(rd(EVG_GRPH_SUPERFICIE_HI + o) & 0xFFu) << 32) | rd(EVG_GRPH_SUPERFICIE + o);
    if (sup != mc_tela || pit != passo) { apres_porque = "la superficie o el paso del CRTC cambiaron desde el encendido"; return false; }
    uint32_t m = (ctl >> 20) & 7u;
    if (m > 1) { apres_porque = "la pantalla esta en mosaico (no lineal)"; return false; }
    if (passo % (uint32_t)alin_passo) { apres_porque = "el paso de la pantalla no esta alineado al grupo de la tarjeta"; return false; }
    *modo = m;
    apres_porque = "se puede";
    return true;
}

/* Sombreadores, la textura (o ninguna) y el estado de la copia: sin Z ni mezcla, tijera a toda la pantalla. */
static bool apres_estado(const g3d_buffer_t *t, bool linear, int tl, int ta)
{
    uint32_t b[112];
    r6_lote_t l;
    r63d_lote(&l, b, 112);
    int quer = t ? 1 : 0;
    if (quer != sh_atual) {
        eg3d_programas(&l, mc_zona + RING_BYTES, mc_zona + RING_BYTES + (quer ? 2u * EG3D_SH_PASSO : EG3D_SH_PASSO));
        sh_atual = quer;
    }
    if (t) eg3d_textura(&l, t->gpu, t->larg, t->alt, (int)(t->passo / 4), true, true, linear, bancos);
    eg3d_estado(&l, false, false, 7, false, 1, 0, true, 0, 0, tl, ta);
    est_valido = false;
    return emitir(&l);
}

static int eg_apresentar(const g3d_apres_t *p)
{
    uint32_t modo;
    bool serve = eg_tela_serve(&modo);
    uint32_t o = evg_crtc_desloc(base.controlador);
    if (!p) {
        klog("[GPU3D] evergreen presentar (D432): %s (pantalla MC %x:%x, paso %u, %dx%d, modo de superficie %u, alineacion %d; "
             "prioridad de la pantalla PRIORITY_A_CNT %x: %s)\n", apres_porque, MC_HI(mc_tela), MC_LO(mc_tela), tela.pitch / 4u, tela.larg,
             tela.alt, modo, alin_passo, sondado ? rd(EVG_PRIORITY_A_CNT + o) : 0,
             sondado && (rd(EVG_PRIORITY_A_CNT + o) & (1u << 20)) ? "siempre alta (D424)" : "la de la BIOS (sin el 2D activo puede parpadear)");
        return serve ? G3D_OK : G3D_NAO;
    }
    if (!cp_ligado) return G3D_FALHA;
    if (!serve) { klog("[GPU3D] evergreen presentar (D432): ya no se puede: %s\n", apres_porque); return G3D_NAO; }
    if (!alvo_l) return G3D_NAO;
    const g3d_buffer_t *src = &b_cor;
    if (p->vaga >= 0) {
        if (p->vaga >= GPU3D_TEXTURAS || !b_tex[p->vaga].usado) return G3D_NAO;
        src = &b_tex[p->vaga];
    }
    if (src->larg < 1 || src->alt < 1 || src->larg > MAX_TEX || src->alt > MAX_TEX) return G3D_NAO;
    /* 1. Lo dibujado hasta ahora, a la VRAM. */
    pendente = true;
    if (eg_esperar(0, GPU3D_PRAZO_MS) != G3D_OK) return G3D_FALHA;
    /* 2. La pantalla como destino y la copia. */
    int tl = tela.larg, ta = tela.alt, passo = (int)(tela.pitch / 4u), alt_buf = (ta + 7) & ~7;
    uint32_t b[80];
    r6_lote_t l;
    r63d_lote(&l, b, 80);
    eg3d_alvo_modo(&l, mc_tela, 0, passo, alt_buf, tl, ta, 0, bancos, modo);
    bool ok = emitir(&l);
    gpu3d_vert_t q[6];
    if (ok && (p->flags & G3D_APRES_BANDAS)) {
        int rr[4][4], n = g3d_apres_bandas(p, tl, ta, rr);
        if (n) ok = apres_estado(NULL, false, tl, ta);
        for (int i = 0; i < n && ok; i++) {
            g3d_quad_cor(rr[i][0], rr[i][1], rr[i][2], rr[i][3], 0xFF000000u, q);
            ok = vertices(q, 6) == G3D_OK;
        }
    }
    if (ok) ok = apres_estado(src, (p->flags & G3D_APRES_LINEAR) != 0, tl, ta);
    if (ok) {
        g3d_apres_quad(p, src->larg, src->alt, q);
        ok = vertices(q, 6) == G3D_OK;
    }
    /* Desgarro: si se pide, el CP empieza al llegar el retrazado (solo se lee el estado del CRTC, con plazo). */
    if (ok && (p->flags & G3D_APRES_VSYNC) && !(rd(EVG_CRTC_STATUS + o) & 1u)) {
        prazo_t pz = prazo_ms(APRES_VBLANK_MS);
        while (!(rd(EVG_CRTC_STATUS + o) & 1u))
            if (prazo_vencido(&pz)) { apres_vblank_fora++; break; }
    }
    /* 3. La valla (vacía el color hasta la VRAM: ahora es la pantalla), esperada con plazo. */
    g3d_vala_t v = 0;
    if (ok) ok = submeter(&v) && valla_esperar(v, GPU3D_PRAZO_MS);
    vb_usado = 0;
    if (!ok) { eg_detalle("al presentar en la pantalla"); return G3D_FALHA; }
    /* 4. El destino del programa, para lo que venga. */
    r63d_lote(&l, b, 80);
    eg3d_alvo(&l, b_cor.gpu, b_z.gpu, alvo_passo, (alvo_a + 63) & ~63, alvo_l, alvo_a, alvo_zbits, bancos);
    if (!emitir(&l)) return G3D_FALHA;
    est_valido = false;
    pendente = true;
    if (!apres_n++)
        klog("[GPU3D] evergreen presentar (D432): primera copia a la pantalla: fuente MC %x:%x paso %u (%dx%d) -> pantalla MC %x:%x "
             "paso %d (%dx%d, modo %u), destino %dx%d+%d+%d%s\n", MC_HI(src->gpu), MC_LO(src->gpu), src->passo / 4, src->larg, src->alt,
             MC_HI(mc_tela), MC_LO(mc_tela), passo, tl, ta, modo, p->dl, p->da, p->dx, p->dy,
             p->flags & G3D_APRES_VSYNC ? ", con espera al retrazado" : "");
    return G3D_OK;
}

const gpu3d_driver_t gpu3d_evergreen = {
    "evergreen", eg_iniciar, eg_alvo, eg_limpar, eg_textura, eg_desenhar, eg_esperar, eg_ler, eg_desligar, eg_detalle, eg_memoria,
    "evergreen", eg_faixas_ajustes, (int)(sizeof eg_faixas_ajustes / sizeof eg_faixas_ajustes[0]), eg_vistas,
    NULL, NULL, NULL, NULL, NULL,   /* gpu3d v2: todavía no (fase B de la D427) */
    eg_apresentar,                  /* D432 */
};
