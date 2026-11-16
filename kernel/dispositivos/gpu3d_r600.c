// SPDX-License-Identifier: GPL-2.0-or-later
/* gpu3d_r600.c - 3D por la GPU en las Radeon R7xx (RV710, RV730, RV740, RV770) y,
 * desde la D357, R6xx (R600, RV610, RV620, RV630, RV635, RV670): el procesador de
 * órdenes (CP) con su microcódigo, el estado del motor 3D y los dibujos, debajo de la
 * capa común gpu3d (D284; la capa es la de la D285).
 *
 * SIN PROBAR EN HARDWARE: QEMU no emula ninguna Radeon. Por eso no se enciende solo
 * («gpu3d probar» o «gpu3d encender») y la capa común solo lo activa si la autoprueba lee los
 * píxeles esperados. El objetivo es la Mobility Radeon HD 4530/4570 (RV710/M92,
 * 1002:9553) del HP dv7 del mantenedor. Lo que cambia en las R6xx (D357): microcódigo
 * (PFP de 576 palabras y ME de 5376, cargado como r600_cp_load_microcode), los valores
 * por defecto del motor (r6_inicio_motor), ME_INITIALIZE, START_3D_CMDBUF en el estado
 * base, la codificación de las operaciones de los sombreadores, SURFACE_BASE_UPDATE
 * tras cambiar las bases (RV6xx) y la mezcla sin «por superficie» de la R600.
 *
 * Qué toca y qué no:
 *   - memoria: solo VRAM de la tarjeta, DETRÁS de todo lo que puede usar el 2D por
 *     DMA (r600_fim_2d): la cola de órdenes (64 KB), los sombreadores, y lo que pida
 *     la capa común (vértices, superficie, Z, texturas). Direcciones del controlador
 *     de memoria comprobadas contra MC_VM_FB_LOCATION, por debajo de 4 GB, con la
 *     memoria virtual de la GPU apagada. Sin GART, sin bus maestro, sin interrupciones;
 *   - registros: los del CP (0x86D8, 0x8700, 0xC1xx), el reinicio SOLO del CP
 *     (GRBM_SOFT_RESET bit 0), los valores por defecto del motor 3D (0x8xxx-0xAxxx,
 *     r7_inicio_motor) y, por la cola, el contexto 3D. NO toca los controladores de
 *     pantalla (0x6xxx), el HDP, los relojes, la energía, el DMA ni SRBM_SOFT_RESET;
 *   - si algo no responde: se para el CP (CP_ME_CNTL) y se deja. Nunca se reinicia
 *     el motor entero (un GRBM_SOFT_RESET completo con la pantalla del VBE encendida
 *     no se ha probado). La pantalla la sigue pintando la CPU como siempre.
 *
 * Diagnóstico: cada etapa del inicio escribe una línea «[GPU3D] r600: etapa N ...»;
 * detalle() vuelca GRBM_STATUS, CP_STAT, punteros de la cola y registros de apuntes.
 *
 * Referencias de números y secuencias: ver r600_3d.h y docs/terceros.md. */
#include "gpu3d.h"
#include "console.h"
#include "paginacao.h"
#include "prazo.h"
#include "r600_3d.h"
#include "r600_base.h"
#include "texto.h"
#include "vfs.h"
#include "memoria.h"

#define RING_BYTES   (64 * 1024)
#define RING_DW      (RING_BYTES / 4)
#define SH_BYTES     (64 * 1024)       /* página de sombreadores (y hueco hasta la arena) */
#define FIXO         (RING_BYTES + SH_BYTES)
#define VB_BYTES     (512 * 1024)
#define ZONA_MIN     (4u << 20)
#define ZONA_MAX     (16u << 20)
#define MAX_ALVO     1024
#define MAX_TEX      1024
#define PRAZO_MS     250               /* esperas internas (sitio en la cola, inicio) */
#define SRBM_STATUS  0x0E50
#define D1CUR_CONTROL 0x6400
#define D1CUR_SURFACE 0x6408
#define SCRATCH_TESTE (R6_SCRATCH_REG0 + 4)
#define SCRATCH_VALLA (R6_SCRATCH_REG0 + 8)
#define NOP_TIPO2    0x80000000u

static r600_base_t base;
static acel_tela_t tela;
static int familia;
static bool r7;                                /* familia R7xx (si no, R6xx) */
static int etapa = -1;                         /* D357: hasta dónde llegó el último encendido (-1: nunca se intentó) */
static const char *const nomes_etapa[7] = {
    "sonda de solo lectura", "leer el microcodigo", "parar el CP y cargar el microcodigo", "valores por defecto del motor 3D",
    "cola y arranque del CP", "prueba de la cola", "sombreadores y estado base",
};
static bool sondado, cp_ligado;
static uint32_t zona_off, zona_bytes;          /* desde el principio de la pantalla */
static volatile uint8_t *zona;                 /* ventana sin caché de toda la zona */
static uint32_t zona_fis_map, zona_bytes_map;
static volatile uint32_t *ring;
static uint32_t mc_zona;
static uint32_t wptr, seq, qd_pipes;
static g3d_arena_t arena;                      /* lo que queda de la zona tras la cola y los sombreadores */
static g3d_buffer_t b_vb, b_cor, b_z, b_tex[GPU3D_TEXTURAS];
static uint32_t vb_usado;
static bool pendente;                          /* hay órdenes en la cola sin valla esperada */
static int alvo_l, alvo_a, alvo_passo, alvo_zbits;
static int sh_atual;                           /* -1 ninguno, 0 color, 1 textura */
static gpu3d_estado_t est_posto;
static bool est_valido;

static inline uint32_t rd(uint32_t r)             { return *(volatile uint32_t *)(base.mmio + r); }
static inline void     wr(uint32_t r, uint32_t v) { *(volatile uint32_t *)(base.mmio + r) = v; }

static void esperar_ms(uint32_t ms)
{
    prazo_t p = prazo_ms(ms);
    while (!prazo_vencido(&p)) { }
}

static const char *nome_etapa(int e) { return e >= 0 && e <= 6 ? nomes_etapa[e] : "sin intentar"; }

static void r3_detalle(const char *fase)
{
    klog("[GPU3D] r600 detalle (%s): ultima etapa del encendido: %d de 6 (%s); CP %s\n", fase, etapa, nome_etapa(etapa),
         cp_ligado ? "en marcha" : "parado");
    if (!base.mmio || !sondado) return;
    klog("[GPU3D] r600 detalle (%s): GRBM_STATUS %x STATUS2 %x SRBM_STATUS %x CP_STAT %x CP_ME_CNTL %x\n", fase,
         rd(R6_GRBM_STATUS), rd(R6_GRBM_STATUS2), rd(SRBM_STATUS), rd(R6_CP_STAT), rd(R6_CP_ME_CNTL));
    klog("[GPU3D] r600 detalle (%s): CP_RB_CNTL %x BASE %x (cola en MC %x) RPTR %x WPTR %x (escrito %x) apuntes: prueba %x valla %x (esperada %x)\n",
         fase, rd(R6_CP_RB_CNTL), rd(R6_CP_RB_BASE), mc_zona, rd(R6_CP_RB_RPTR), rd(R6_CP_RB_WPTR), wptr, rd(SCRATCH_TESTE),
         rd(SCRATCH_VALLA), seq);
    klog("[GPU3D] r600 detalle (%s): zona 3D en MC %x, %u KB, libres %u KB; destino %dx%d paso %d, color MC %x, Z MC %x (%d bits)\n",
         fase, mc_zona, zona_bytes >> 10, cp_ligado ? (uint32_t)(g3d_arena_livre(&arena) >> 10) : 0, alvo_l, alvo_a, alvo_passo,
         (uint32_t)b_cor.gpu, (uint32_t)b_z.gpu, alvo_zbits);
}

/* ------------------------------------------------------------------ sonda (solo lectura) */

static bool r3_sondar(const pci_disp_t *d, int geracao, const acel_tela_t *t, char *m, int nm)
{
    sondado = false;
    if (!r600_sondar(d, geracao, t, &base, m, nm)) return false;
    tela = *t;
    if (!txt_compara_n(base.nome, "RV710", 5)) familia = R63D_RV710;
    else if (!txt_compara_n(base.nome, "RV730", 5)) familia = R63D_RV730;
    else if (!txt_compara_n(base.nome, "RV740", 5)) familia = R63D_RV740;
    else if (!txt_compara_n(base.nome, "RV770", 5)) familia = R63D_RV770;
    /* D357: R6xx. La RV620 lleva los valores de la RV610 (Linux y Mesa las tratan igual). */
    else if (!txt_compara_n(base.nome, "RV610", 5) || !txt_compara_n(base.nome, "RV620", 5)) familia = R63D_RV610;
    else if (!txt_compara_n(base.nome, "RV630", 5)) familia = R63D_RV630;
    else if (!txt_compara_n(base.nome, "RV635", 5)) familia = R63D_RV635;
    else if (!txt_compara_n(base.nome, "RV670", 5)) familia = R63D_RV670;
    else if (!txt_compara_n(base.nome, "R600", 4)) familia = R63D_R600;
    else { snformatar(m, (size_t)nm, "%s: familia sin 3D todavia", base.nome); return false; }
    r7 = R63D_E_R7XX(familia);
    if ((base.gen == 7) != r7) {
        snformatar(m, (size_t)nm, "%s: la familia no cuadra con la generacion %d de la tabla", base.nome, base.gen);
        return false;
    }
    if (base.vm0 & 1) {
        snformatar(m, (size_t)nm, "la memoria virtual de la GPU esta encendida (VM_CONTEXT0_CNTL %x): no se arriesga", base.vm0);
        return false;
    }
    /* Dónde cabe la zona 3D: detrás de lo del 2D, dentro de la VRAM real, de la
     * apertura de la CPU y de lo que el MC llama memoria de vídeo. */
    uint32_t ini = (r600_fim_2d(t) + 0xFFFFFu) & ~0xFFFFFu;
    uint64_t lim = t->memoria;
    if (base.memsize >= (16u << 20) && base.memsize != 0xFFFFFFFFu) lim = base.memsize;
    if (lim > base.desloc) lim -= base.desloc; else lim = 0;
    if (base.aper >= (16u << 20) && base.aper != 0xFFFFFFFFu && lim > (uint64_t)base.aper - base.desloc)
        lim = (uint64_t)base.aper - base.desloc;
    if ((uint64_t)base.mc_tela + lim - 1 > base.vram_fim) lim = base.vram_fim + 1 - base.mc_tela;
    if ((uint64_t)base.mc_tela + lim > 0xFFFFFFFFull) lim = 0xFFFFFFFFull - base.mc_tela;
    if ((uint64_t)base.mc_tela < base.vram_ini || lim < (uint64_t)ini + ZONA_MIN) {
        snformatar(m, (size_t)nm, "no caben 4 MB para el 3D detras del 2D (VRAM util %u KB, hace falta desde %u KB)",
                   (uint32_t)(lim >> 10), ini >> 10);
        return false;
    }
    uint32_t tam = (uint32_t)lim - ini;
    if (tam > ZONA_MAX) tam = ZONA_MAX;
    tam &= ~0xFFFFFu;
    if ((base.mc_tela + ini) & 0xFFu) { snformatar(m, (size_t)nm, "la zona 3D (MC %x) no esta alineada a 256 bytes", base.mc_tela + ini); return false; }
    for (int c = 0; c < 2; c++) {
        uint32_t cc = rd(D1CUR_CONTROL + (c ? 0x800u : 0)), cs = rd(D1CUR_SURFACE + (c ? 0x800u : 0));
        if ((cc & 1) && cs + 0x4000u > base.mc_tela + ini && cs < base.mc_tela + ini + tam) {
            snformatar(m, (size_t)nm, "el cursor por hardware de D%d (MC %x) esta en la zona 3D", c + 1, cs);
            return false;
        }
    }
    zona_off = ini; zona_bytes = tam;
    mc_zona = base.mc_tela + ini;
    klog("[GPU3D] r600: %s (%04x), %s, familia %d; zona 3D: %u KB en MC %x (a %u KB de la pantalla); VRAM %u MB, apertura %u MB, VBE %u KB\n",
         base.nome, base.modelo, r7 ? "R7xx" : "R6xx", familia, tam >> 10, mc_zona, ini >> 10, base.memsize >> 20, base.aper >> 20, t->memoria >> 10);
    klog("[GPU3D] r600: antes de tocar nada: CP_ME_CNTL %x CP_RB_CNTL %x GRBM_STATUS %x CP_STAT %x CP_INT_CNTL %x\n",
         rd(R6_CP_ME_CNTL), rd(R6_CP_RB_CNTL), rd(R6_GRBM_STATUS), rd(R6_CP_STAT), rd(R6_CP_INT_CNTL));
    sondado = true;
    return true;
}

/* ------------------------------------------------------------------ la cola */

static void hdp(void) { r600_hdp_vaciar(&base, (volatile uint32_t *)(zona + RING_BYTES)); }

static uint32_t livre(void) { return ((rd(R6_CP_RB_RPTR) & (RING_DW - 1)) - wptr - 1) & (RING_DW - 1); }

static void avisar(void)
{
    while (wptr & 15) { ring[wptr] = NOP_TIPO2; wptr = (wptr + 1) & (RING_DW - 1); }
    hdp();
    wr(R6_CP_RB_WPTR, wptr);
    (void)rd(R6_CP_RB_WPTR);
}

/* Copia un lote a la cola (esperando sitio con plazo). false: la cola no avanza o el lote no cabía. */
static bool emitir(const r6_lote_t *l)
{
    if (!cp_ligado || l->cheio) {
        if (l->cheio) klog("[GPU3D] r600: un lote de ordenes no cupo en su bufer (error interno)\n");
        return false;
    }
    if (livre() < (uint32_t)l->n + 32) {
        avisar();
        prazo_t p = prazo_ms(PRAZO_MS);
        while (livre() < (uint32_t)l->n + 32)
            if (prazo_vencido(&p)) { klog("[GPU3D] r600: la cola no avanza (RPTR %x, WPTR %x)\n", rd(R6_CP_RB_RPTR), wptr); return false; }
    }
    for (int i = 0; i < l->n; i++) { ring[wptr] = l->p[i]; wptr = (wptr + 1) & (RING_DW - 1); }
    return true;
}

/* Manda lo encolado con una valla: vacía las cachés de color y Z, espera al motor y apunta el número. */
static bool submeter(g3d_vala_t *valla)
{
    uint32_t b[32];
    r6_lote_t l;
    r63d_lote(&l, b, 32);
    seq++;
    if (!seq) seq = 1;
    r63d_valla(&l, (uint32_t)b_cor.gpu, b_cor.usado ? (uint32_t)b_cor.bytes : 0, (uint32_t)b_z.gpu,
               b_z.usado ? (uint32_t)b_z.bytes : 0, SCRATCH_VALLA, seq);
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
            klog("[GPU3D] r600: la valla %x no llega (apunte %x, RPTR %x, WPTR %x, GRBM_STATUS %x)\n", v, rd(SCRATCH_VALLA),
                 rd(R6_CP_RB_RPTR), wptr, rd(R6_GRBM_STATUS));
            return false;
        }
    return true;
}

/* ------------------------------------------------------------------ encender y apagar */

static void cp_parar(void)
{
    wr(R6_CP_ME_CNTL, R6_CP_ME_HALT | R6_CP_PFP_HALT);
    wr(R6_SCRATCH_UMSK, 0);
    cp_ligado = false;
}

static void cp_reiniciar(void)
{
    wr(R6_GRBM_SOFT_RESET, R6_SOFT_RESET_CP);   /* solo el bit del CP */
    (void)rd(R6_GRBM_SOFT_RESET);
    esperar_ms(15);
    wr(R6_GRBM_SOFT_RESET, 0);
    (void)rd(R6_GRBM_SOFT_RESET);
}

static inline uint32_t be32(const uint8_t *p) { return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3]; }

static bool tocou;   /* ligar_cp ya escribió algún registro de la GPU */

static bool ligar_cp(char *m, int nm)
{
    /* La RV740 usa el de la RV730 (como Linux). D357: la RV620 y la RV635 cargan los de la RV610 y la RV630,
     * que en linux-firmware son los mismos bytes que RV620_* y RV635_* (mismas sumas SHA-256). */
    static const char *const fw_nome[R63D_N_FAMILIAS] = { "RV770", "RV730", "RV730", "RV710", "R600", "RV610", "RV630", "RV670", "RV630" };
    const uint32_t pal_pfp = r7 ? R7_PFP_UCODE_PALAVRAS : R6_PFP_UCODE_PALAVRAS;
    const uint32_t pal_me = r7 ? R7_ME_UCODE_PALAVRAS : R6_ME_UCODE_PALAVRAS;
    if (!sondado) { snformatar(m, (size_t)nm, "sin sondar"); return false; }
    etapa = 1;
    cp_ligado = false;
    tocou = false;
    sh_atual = -1;
    est_valido = false;
    pendente = false;
    alvo_l = alvo_a = 0;
    memset(&b_vb, 0, sizeof b_vb); memset(&b_cor, 0, sizeof b_cor); memset(&b_z, 0, sizeof b_z); memset(b_tex, 0, sizeof b_tex);

    /* Etapa 1: el microcódigo, del sistema de archivos. */
    char cam[64];
    uint8_t *pfp = NULL, *me = NULL;
    uint32_t npfp = 0, nme = 0;
    snformatar(cam, sizeof cam, "/lib/firmware/radeon/%s_pfp.bin", fw_nome[familia]);
    if (vfs_ler_inteiro(cam, &pfp, &npfp) < 0 || npfp != pal_pfp * 4) {
        snformatar(m, (size_t)nm, "falta el microcodigo %s (o no mide %u bytes: %u)", cam, pal_pfp * 4, npfp);
        if (pfp) kfree(pfp);
        return false;
    }
    snformatar(cam, sizeof cam, "/lib/firmware/radeon/%s_me.bin", fw_nome[familia]);
    if (vfs_ler_inteiro(cam, &me, &nme) < 0 || nme != pal_me * 4) {
        snformatar(m, (size_t)nm, "falta el microcodigo %s (o no mide %u bytes: %u)", cam, pal_me * 4, nme);
        kfree(pfp);
        if (me) kfree(me);
        return false;
    }
    klog("[GPU3D] r600: etapa 1: microcodigo %s leido (PFP %u bytes, ME %u bytes)\n", fw_nome[familia], npfp, nme);

    /* La ventana de la CPU sobre la zona (una vez; las ventanas no se devuelven). */
    uint32_t fis = tela.fb_fisico + zona_off;
    if (!zona || zona_fis_map != fis || zona_bytes_map != zona_bytes) {
        zona = (volatile uint8_t *)janela_mapear_tipo(fis, zona_bytes, PG_PCD | PG_PWT);
        zona_fis_map = fis; zona_bytes_map = zona_bytes;
    }
    if (!zona) { snformatar(m, (size_t)nm, "sin ventana del kernel para la zona 3D"); kfree(pfp); kfree(me); return false; }
    ring = (volatile uint32_t *)zona;
    /* ¿La CPU ve de verdad esa memoria? Escribir y leer una palabra de la página de sombreadores. */
    volatile uint32_t *sh = (volatile uint32_t *)(zona + RING_BYTES);
    sh[1023] = 0x5A6D3D00u; sh[1022] = ~0x5A6D3D00u;
    if (sh[1023] != 0x5A6D3D00u || sh[1022] != ~0x5A6D3D00u) {
        snformatar(m, (size_t)nm, "la VRAM de la zona 3D no guarda lo escrito (%x): no es memoria utilizable", sh[1023]);
        kfree(pfp); kfree(me);
        return false;
    }
    uint32_t gs = rd(R6_GRBM_STATUS);
    if (gs & R6_GUI_ACTIVE) {
        snformatar(m, (size_t)nm, "el motor grafico esta ocupado antes de empezar (GRBM_STATUS %x): no se toca", gs);
        kfree(pfp); kfree(me);
        return false;
    }

    /* Etapa 2: parar el CP, reiniciarlo y cargarle el microcódigo (R7xx: PFP y luego ME, como
     * rv770_cp_load_microcode; R6xx: ME y luego PFP, como r600_cp_load_microcode). */
    etapa = 2;
    tocou = true;
    cp_parar();
    wr(R6_CP_RB_CNTL, R6_RB_NO_UPDATE | (15u << 8) | 3u);
    cp_reiniciar();
    if (r7) {
        wr(R6_CP_PFP_UCODE_ADDR, 0);
        for (uint32_t i = 0; i < pal_pfp; i++) wr(R6_CP_PFP_UCODE_DATA, be32(pfp + i * 4));
        wr(R6_CP_PFP_UCODE_ADDR, 0);
        wr(R6_CP_ME_RAM_WADDR, 0);
        for (uint32_t i = 0; i < pal_me; i++) wr(R6_CP_ME_RAM_DATA, be32(me + i * 4));
    } else {
        wr(R6_CP_ME_RAM_WADDR, 0);
        for (uint32_t i = 0; i < pal_me; i++) wr(R6_CP_ME_RAM_DATA, be32(me + i * 4));
        wr(R6_CP_PFP_UCODE_ADDR, 0);
        for (uint32_t i = 0; i < pal_pfp; i++) wr(R6_CP_PFP_UCODE_DATA, be32(pfp + i * 4));
    }
    wr(R6_CP_PFP_UCODE_ADDR, 0);
    wr(R6_CP_ME_RAM_WADDR, 0);
    wr(R6_CP_ME_RAM_RADDR, 0);
    uint32_t me0 = be32(me);
    kfree(pfp); kfree(me);
    klog("[GPU3D] r600: etapa 2: CP parado, reiniciado y microcodigo cargado (primera palabra del ME %x; CP_STAT %x)\n", me0,
         rd(R6_CP_STAT));

    /* Etapa 3: valores por defecto del motor 3D (por MMIO). En las R6xx RAMCFG y SX_DEBUG_1 están en
     * otro sitio, y SMX_DC_CTL0, DB_DEBUG3 y DB_DEBUG4 no se leen (son de las R7xx). */
    etapa = 3;
    r7_leituras_t le;
    memset(&le, 0, sizeof le);
    le.mc_arb_ramcfg = rd(r7 ? R7_MC_ARB_RAMCFG : R6_RAMCFG);
    le.cc_gc_shader_pipe_config = rd(R7_CC_GC_SHADER_PIPE_CONFIG);
    le.cc_rb_backend_disable = rd(R7_CC_RB_BACKEND_DISABLE);
    le.ta_cntl_aux = rd(R7_TA_CNTL_AUX);
    le.sx_debug_1 = rd(r7 ? R7_SX_DEBUG_1 : R6_SX_DEBUG_1);
    if (r7) { le.smx_dc_ctl0 = rd(R7_SMX_DC_CTL0); le.db_debug3 = rd(R7_DB_DEBUG3); le.db_debug4 = rd(R7_DB_DEBUG4); }
    le.sq_config = rd(R7_SQ_CONFIG);
    le.sq_ms_fifo_sizes = rd(R6_SQ_MS_FIFO_SIZES);
    static r6_par_t pares[R7_INICIO_MAX];
    uint32_t max_ctx = 4;
    int np = r63d_inicio_motor(familia, &le, pares, R7_INICIO_MAX, &max_ctx);
    if (np < 0) { snformatar(m, (size_t)nm, "sin tabla de inicio para la familia %d", familia); return false; }
    for (int i = 0; i < np; i++) wr(pares[i].reg, pares[i].valor);
    uint32_t inativas = (le.cc_gc_shader_pipe_config >> 8) & 0xFFu;
    qd_pipes = 0;
    for (int i = 0; i < 8; i++) if (!(inativas & (1u << i))) qd_pipes++;
    if (!qd_pipes) qd_pipes = 1;
    klog("[GPU3D] r600: etapa 3: motor 3D con %d registros por defecto (RAMCFG %x, tuberias %x, backends %x, GB_TILING_CONFIG %x, %u tuberias activas)\n",
         np, le.mc_arb_ramcfg, le.cc_gc_shader_pipe_config, le.cc_rb_backend_disable, rd(0x98F0), qd_pipes);

    /* Etapa 4: la cola (64 KB en VRAM), sin escritura de vuelta del puntero. */
    etapa = 4;
    cp_reiniciar();
    uint32_t cntl = (9u << 8) | 13u;   /* log2(4096 / 8) y log2(65536 / 8) */
    wr(R6_CP_RB_CNTL, cntl);
    wr(R6_CP_SEM_WAIT_TIMER, 0);
    wr(R6_CP_RB_WPTR_DELAY, 0);
    wr(R6_CP_RB_CNTL, cntl | R6_RB_RPTR_WR_ENA);
    wr(R6_CP_RB_RPTR_WR, 0);
    wptr = 0;
    wr(R6_CP_RB_WPTR, 0);
    wr(R6_CP_RB_RPTR_ADDR, (mc_zona + RING_BYTES + 0x1000u) & 0xFFFFFFFCu);   /* no se usa (NO_UPDATE), pero apunta a memoria nuestra */
    wr(R6_CP_RB_RPTR_ADDR_HI, 0);
    wr(R6_SCRATCH_UMSK, 0);
    cntl |= R6_RB_NO_UPDATE;
    esperar_ms(1);
    wr(R6_CP_RB_CNTL, cntl);
    wr(R6_CP_RB_BASE, mc_zona >> 8);
    wr(R6_CP_DEBUG, (1u << 27) | (1u << 28));
    for (uint32_t i = 0; i < RING_DW; i++) ring[i] = NOP_TIPO2;

    /* Etapa 5: arrancar el CP (ME_INITIALIZE) y probar la cola con un registro de apuntes. */
    static uint32_t b[1024];
    r6_lote_t l;
    r63d_lote(&l, b, 16);
    r63d_me_initialize(&l, r7, max_ctx);
    cp_ligado = true;
    for (int i = 0; i < l.n; i++) { ring[wptr] = l.p[i]; wptr++; }
    avisar();
    wr(R6_CP_ME_CNTL, 0xFF);
    klog("[GPU3D] r600: etapa 4: cola en MC %x, CP en marcha (CP_ME_CNTL %x, RPTR %x, WPTR %x)\n", mc_zona, rd(R6_CP_ME_CNTL),
         rd(R6_CP_RB_RPTR), wptr);
    etapa = 5;
    wr(SCRATCH_TESTE, 0xCAFEDEADu);
    r63d_lote(&l, b, 16);
    r63d_teste_cola(&l, SCRATCH_TESTE, 0xDEADBEEFu);
    if (!emitir(&l)) { snformatar(m, (size_t)nm, "la cola del CP no avanza tras arrancarlo (RPTR %x)", rd(R6_CP_RB_RPTR)); return false; }
    avisar();
    prazo_t pz = prazo_ms(PRAZO_MS);
    while (rd(SCRATCH_TESTE) != 0xDEADBEEFu)
        if (prazo_vencido(&pz)) {
            snformatar(m, (size_t)nm, "el CP no ejecuta la prueba de la cola (apunte %x, RPTR %x, WPTR %x, CP_STAT %x)", rd(SCRATCH_TESTE),
                       rd(R6_CP_RB_RPTR), wptr, rd(R6_CP_STAT));
            return false;
        }
    klog("[GPU3D] r600: etapa 5: prueba de la cola superada (el CP escribio el registro de apuntes; RPTR %x)\n", rd(R6_CP_RB_RPTR));

    /* Etapa 6: sombreadores a la VRAM y estado base del contexto, con una valla. */
    etapa = 6;
    for (int i = 0; i < 3 * 64; i++) sh[i] = 0;
    uint32_t s[R63D_SH_MAX];
    int n = r63d_vs(s, r7);
    for (int i = 0; i < n; i++) sh[i] = s[i];
    n = r63d_ps_cor(s, r7);
    for (int i = 0; i < n; i++) sh[64 + i] = s[i];
    n = r63d_ps_tex(s, r7);
    for (int i = 0; i < n; i++) sh[128 + i] = s[i];
    g3d_arena_iniciar(&arena, (uint64_t)mc_zona + FIXO, (uint8_t *)zona + FIXO, zona_bytes - FIXO);
    if (!g3d_buffer_criar(&arena, &b_vb, G3D_BUF_LOTE, VB_BYTES, 4096)) { snformatar(m, (size_t)nm, "sin sitio para los vertices"); return false; }
    vb_usado = 0;
    r63d_lote(&l, b, 1024);
    r63d_estado_base(&l, familia, r7, qd_pipes, mc_zona + RING_BYTES);
    seq = 0x3D000000u;
    wr(SCRATCH_VALLA, seq);
    g3d_vala_t v;
    if (!emitir(&l) || !submeter(&v) || !valla_esperar(v, PRAZO_MS)) {
        snformatar(m, (size_t)nm, "el CP no termina el estado base (%d palabras; apunte %x, GRBM_STATUS %x)", l.n, rd(SCRATCH_VALLA),
                   rd(R6_GRBM_STATUS));
        return false;
    }
    klog("[GPU3D] r600: etapa 6: sombreadores en MC %x y estado base (%d palabras) aceptados; GRBM_STATUS %x\n",
         mc_zona + RING_BYTES, l.n, rd(R6_GRBM_STATUS));
    return true;
}

static void r3_desligar(void)
{
    if (!base.mmio || !sondado) return;
    if (cp_ligado) {
        prazo_t p = prazo_ms(50);
        while (rd(R6_GRBM_STATUS) & R6_GUI_ACTIVE)
            if (prazo_vencido(&p)) { klog("[GPU3D] r600: el motor sigue ocupado al parar el CP (GRBM_STATUS %x)\n", rd(R6_GRBM_STATUS)); break; }
    }
    cp_parar();
    alvo_l = alvo_a = 0;
    pendente = false;
    klog("[GPU3D] r600: CP parado (CP_ME_CNTL %x); la pantalla sigue como estaba\n", rd(R6_CP_ME_CNTL));
}

/* D357: «etapa N (nombre): motivo», para que «gpu3d probar» diga dónde se paró sin mirar el log. */
static void prefixar_etapa(char *m, int nm)
{
    char t[160];
    snformatar(t, sizeof t, "etapa %d (%s): %s", etapa, nome_etapa(etapa), m);
    snformatar(m, (size_t)nm, "%s", t);
}

static int r3_iniciar(const pci_disp_t *d, int geracao, const acel_tela_t *t, uint32_t variante, char *m, int nm)
{
    (void)variante;
#ifdef SAGUI_SEM_FIRMWARE
    (void)d; (void)geracao; (void)t; (void)r3_sondar; (void)ligar_cp; (void)prefixar_etapa;
    snformatar(m, (size_t)nm, "esta compilacion no lleva firmware (SEM_FIRMWARE=1): sin microcodigo no hay 3D");
    return G3D_NAO;
#else
    etapa = 0;
    if (!r3_sondar(d, geracao, t, m, nm)) { prefixar_etapa(m, nm); return G3D_NAO; }   /* solo leyó: no es un fallo de la GPU */
    if (ligar_cp(m, nm)) return G3D_OK;
    prefixar_etapa(m, nm);
    klog("[GPU3D] r600: el encendido se paro en la etapa %d (%s)\n", etapa, nome_etapa(etapa));
    return tocou ? G3D_FALHA : G3D_NAO;
#endif
}

/* ------------------------------------------------------------------ destino, texturas y dibujos */

static int r3_esperar(g3d_vala_t vala, uint32_t ms)
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

static int r3_alvo(int larg, int alt, int zbits)
{
    if (!cp_ligado) return G3D_FALHA;
    if (larg > MAX_ALVO || alt > MAX_ALVO) return G3D_NAO;
    if (r3_esperar(0, GPU3D_PRAZO_MS) != G3D_OK) return G3D_FALHA;
    g3d_buffer_soltar(&arena, &b_cor);
    g3d_buffer_soltar(&arena, &b_z);
    alvo_l = alvo_a = 0;
    int passo = (larg + 63) & ~63, alt_buf = (alt + 63) & ~63;   /* holgura para el mosaico del Z */
    uint32_t bytes = (uint32_t)passo * (uint32_t)alt_buf * 4u;
    if (!g3d_buffer_criar(&arena, &b_cor, G3D_BUF_ALVO, bytes, 4096)) return G3D_NAO;
    if (!g3d_buffer_criar(&arena, &b_z, G3D_BUF_Z, bytes, 4096)) { g3d_buffer_soltar(&arena, &b_cor); return G3D_NAO; }
    b_z.cpu = NULL;                  /* el Z va en mosaico: la CPU no lo toca */
    zbits = zbits == 16 ? 16 : 24;
    uint32_t b[64];
    r6_lote_t l;
    r63d_lote(&l, b, 64);
    r63d_alvo(&l, (uint32_t)b_cor.gpu, (uint32_t)b_z.gpu, passo, alt_buf, larg, alt, zbits, R63D_PRECISA_SBU(familia));
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
    uint32_t b[96];
    r6_lote_t l;
    r63d_lote(&l, b, 96);
    int quer = t ? 1 : 0;
    if (quer != sh_atual) {
        r63d_programas(&l, mc_zona + RING_BYTES, mc_zona + RING_BYTES + (quer ? 0x200u : 0x100u), quer);
        sh_atual = quer;
    }
    if (t) r63d_textura(&l, (uint32_t)t->gpu, t->larg, t->alt, (int)(t->passo / 4), !(e->flags & G3D_TEX_REPETE_S),
                        !(e->flags & G3D_TEX_REPETE_T), (e->flags & G3D_TEX_LINEAR) != 0);
    if (limpar)   /* el Z siempre pasa y se escribe con el valor del fondo; el color solo si se pide */
        r63d_estado(&l, (limpar & 2) != 0, true, 7, false, 1, 0, (limpar & 1) != 0, 0, 0, alvo_l, alvo_a, familia == R63D_R600);
    else
        r63d_estado(&l, (e->flags & G3D_Z_TESTE) != 0, (e->flags & G3D_Z_ESCRITA) != 0, zf[e->func_z <= 2 ? e->func_z : 0],
                    (e->flags & G3D_MEZCLA) != 0, fat[e->mez_orig & 3], fat[e->mez_dest & 3], true, 0, 0, alvo_l, alvo_a,
                    familia == R63D_R600);
    if (!emitir(&l)) return false;
    if (limpar) est_valido = false;
    else { est_posto = *e; est_valido = true; }
    return true;
}

/* Copia n vértices al búfer de vértices (vaciándolo antes si no caben) y encola el dibujo. */
static int vertices(const gpu3d_vert_t *v, int n)
{
    uint32_t bytes = (uint32_t)n * R63D_VERT_BYTES;
    if (bytes > b_vb.bytes) return G3D_NAO;
    if (vb_usado + bytes > b_vb.bytes) {
        pendente = true;             /* una valla nueva: hasta que llegue, la GPU puede estar leyendo estos vértices */
        if (r3_esperar(0, GPU3D_PRAZO_MS) != G3D_OK) return G3D_FALHA;
    }
    volatile uint32_t *d = (volatile uint32_t *)(b_vb.cpu + vb_usado);
    const uint32_t *o = (const uint32_t *)v;
    for (uint32_t i = 0; i < bytes / 4; i++) d[i] = o[i];
    uint32_t b[32];
    r6_lote_t l;
    r63d_lote(&l, b, 32);
    r63d_desenhar(&l, (uint32_t)b_vb.gpu + vb_usado, n, 4, R63D_SEM_CACHE_VTX(familia));
    if (!emitir(&l)) return G3D_FALHA;
    vb_usado += bytes;
    pendente = true;
    return G3D_OK;
}

static int r3_limpar(uint32_t mascara, uint32_t cor)
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

static int r3_textura(int vaga, int larg, int alt, const uint32_t *argb)
{
    if (!cp_ligado) return G3D_FALHA;
    if (vaga < 0 || vaga >= GPU3D_TEXTURAS) return G3D_NAO;
    g3d_buffer_t *b = &b_tex[vaga];
    if (!argb && !b->usado) return G3D_OK;
    if (argb && (larg < 1 || alt < 1 || larg > MAX_TEX || alt > MAX_TEX)) return G3D_NAO;
    /* Que la GPU no esté leyendo la textura mientras se cambia o se suelta. */
    if (r3_esperar(0, GPU3D_PRAZO_MS) != G3D_OK) return G3D_FALHA;
    g3d_buffer_soltar(&arena, b);
    est_valido = false;
    if (!argb) return G3D_OK;
    int passo = (larg + 63) & ~63;     /* lineal alineada: filas de múltiplos de 64 texels */
    if (!g3d_buffer_criar(&arena, b, G3D_BUF_TEXTURA, (uint64_t)passo * (uint32_t)((alt + 7) & ~7) * 4u, 4096)) return G3D_NAO;
    b->passo = (uint32_t)passo * 4; b->larg = larg; b->alt = alt; b->bpp = 32;
    volatile uint32_t *d = (volatile uint32_t *)b->cpu;
    for (int y = 0; y < alt; y++)
        for (int x = 0; x < larg; x++) d[y * passo + x] = argb[y * larg + x];
    return G3D_OK;
}

static int r3_desenhar(const gpu3d_estado_t *e, const gpu3d_vert_t *v, int n, g3d_vala_t *vala)
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

static int r3_ler(uint32_t *dest, int passo_px)
{
    if (!cp_ligado || !alvo_l) return G3D_FALHA;
    /* Siempre una valla nueva: lo que se lee tiene que estar ya en la VRAM (cachés de color vaciadas). */
    pendente = true;
    if (r3_esperar(0, GPU3D_PRAZO_MS) != G3D_OK) return G3D_FALHA;
    volatile uint32_t *s = (volatile uint32_t *)b_cor.cpu;
    for (int y = 0; y < alvo_a; y++)
        for (int x = 0; x < alvo_l; x++) dest[y * passo_px + x] = s[y * alvo_passo + x] & 0x00FFFFFFu;
    return G3D_OK;
}

static void r3_memoria(uint32_t *total_kb, uint32_t *livre_kb)
{
    *total_kb = zona_bytes >> 10;
    *livre_kb = cp_ligado ? (uint32_t)(g3d_arena_livre(&arena) >> 10) : 0;
}

const gpu3d_driver_t gpu3d_r600 = {
    "r600", r3_iniciar, r3_alvo, r3_limpar, r3_textura, r3_desenhar, r3_esperar, r3_ler, r3_desligar, r3_detalle, r3_memoria,
};
