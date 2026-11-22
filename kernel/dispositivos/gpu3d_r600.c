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
#include "vram_zona_logica.h"
#include "gart_r700_logica.h"
#include "pci.h"
#include "texto.h"
#include "vfs.h"
#include "memoria.h"
#include "fpu.h"
#include "sse2.h"
#include "video.h"
#include "g3d_v2_provas.h"
#include "wc.h"

#define RING_BYTES   (64 * 1024)
#define RING_DW      (RING_BYTES / 4)
#define SH_BYTES     (64 * 1024)       /* página de sombreadores (y hueco hasta la arena) */
#define FIXO         (RING_BYTES + SH_BYTES)
#define VB_BYTES     (512 * 1024)
#define ZONA_MIN     (4u << 20)
#define ZONA_MAX     (128u << 20)      /* D402: hasta 128 MB de la VRAM real (antes 16) */
#define VISIVEL_MAX  (16u << 20)       /* D402: lo que la CPU ve siempre (cola, sombreadores, vértices, destino) */
#define JMOVEL_BYTES ((4u << 20) + 4096u)   /* D402: ventana móvil para escribir texturas de la parte grande */
#define MAX_ALVO     1024
#define MAX_TEX      1024
#define PRAZO_MS     250               /* esperas internas (sitio en la cola, inicio) */
#define SRBM_STATUS  0x0E50
#define D1CUR_CONTROL 0x6400
#define D1CUR_SURFACE 0x6408
/* Las superficies de D1/D2 (registros AVIVO, los mismos que en R5xx; D386). */
#define D1CRTC_CONTROL     0x6080
#define D1GRPH_ENABLE      0x6100
#define D1GRPH_CONTROL     0x6104
#define D1GRPH_SUPERFICIE  0x6110
#define D1GRPH_PITCH       0x6120
#define D1GRPH_DESL_Y      0x6128
#define D1GRPH_Y_FIM       0x6138
#define D1MODE_VP_INI      0x6580
#define D1MODE_VP_TAM      0x6584
#define D1GRPH_SUP_ALTA    0x6914      /* R7xx: bits 39:32 de la superficie de D1 */
#define D2GRPH_SUP_ALTA    0x6114      /* y de D2 */
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
static volatile uint8_t *zona;                 /* ventana sin caché de la parte visible de la zona (D402) */
static uint32_t zona_vis;                      /* D402: bytes de la zona con ventana fija (los primeros) */
static uint32_t jmovel;                        /* D402: ventana móvil reservada (0: no hay) */
static uint32_t zona_fis_map, zona_bytes_map;
static volatile uint32_t *ring;
static uint32_t mc_zona;
static uint32_t wptr, seq, qd_pipes;
static int alin_passo = 64;
/* D402: dónde se va el tiempo de cada cuadro (ciclos del TSC): esperar la valla y copiar a la RAM. */
static uint64_t t_valla, t_copia;
static uint32_t n_ler_px, n_ler;                    /* D402: alineación del paso de color y texturas (píxeles) */
static g3d_arena_t arena;                      /* lo que queda de la parte visible tras la cola y los sombreadores */
static g3d_arena_t arena_g;                    /* D402: el resto de la zona, sin ventana fija (Z y texturas) */
static bool arena_g_ok;
static g3d_buffer_t b_vb, b_cor, b_z, b_tex[GPU3D_TEXTURAS];
static uint32_t vb_usado;
static bool pendente;                          /* hay órdenes en la cola sin valla esperada */
/* D432: presentar en la pantalla (abajo, «presentar en la pantalla»). */
static uint32_t apres_n, apres_vblank_fora;
static const char *apres_porque = "sin comprobar";
static int alvo_l, alvo_a, alvo_passo, alvo_zbits;
static int sh_atual;                           /* -1 ninguno, 0 color, 1 textura */
static gpu3d_estado_t est_posto;
static bool est_valido;
/* D426 (gpu3d v2): formato de cada hueco, el destino actual (el principal o un hueco con su Z), qué
 * sombreadores y qué SPI quedaron (1: los de v1, 2: los de v2) y la caché de sombreadores de píxeles
 * generados: 8 en la VRAM (zona de sombreadores, desde 0x1000, 4 KB cada uno), por la combinación. */
#define SH_VS2      0x400u
#define SH_PS2      0x1000u
#define PS2_CACHE   8
#define PS2_DESC    104
static uint8_t tex_fmt[GPU3D_TEXTURAS];
static g3d_buffer_t b_zdest, *cur_cor = &b_cor, *cur_z = &b_z;
static int dest_vaga = -1, princ_l, princ_a, princ_passo, princ_zbits;
static int modo_sh = 1, ps_posto = -1;
static struct { bool usado; uint32_t uso; uint8_t desc[PS2_DESC]; int pal; } ps_cache[PS2_CACHE];
static uint32_t ps_relogio, ps_gerados, ps_acertos, caps_v2;
/* D457 (gpu3d v3, «v3 en R6xx/R7xx»): los búferes de vértices viven en un bloque de la zona (b_vbm, en la parte grande
 * si la hay), repartido por huecos (vbm); los sombreadores de vértices v3 generados, en una caché de 8 en la página de
 * sombreadores (0x9000-0xCFFF, 2 KB cada uno); lo último que se puso (modo_sh 3: el rasterizador de v3) para no
 * repetirlo en cada dibujo de un mundo hecho de cientos de búferes. */
#define SH_VS3      0x9000u
#define VS3_CACHE   8
#define VS3_BYTES   2048u
static g3d_buffer_t b_vbm;
static g3d_vbmonte_t vbm;
static bool vbm_sujo;                          /* se subió algo desde el último vaciado de la caché de vértices */
static struct { bool usado; uint32_t uso; r63d_vs3_desc_t desc; int pal; } vs3_cache[VS3_CACHE];
static int vs3_posto = -1;
static gpu3d_estado2_t e3_posto;
static gpu3d_transf_t t3_posto;
static bool e3_valido, t3_valido;
static uint32_t v3_dibujos, v3_subidas, v3_kb, v3_vs_gerados, v3_transf, v3_montes, v3_wc;
/* D457: banderas de /sistema/conf/gpu3d-r600.conf («inicio 28ffc N»: un registro que no existe; no se escribe nunca):
 *   1  sin v3 aunque la tarjeta esté confirmada;      2  v3 en una tarjeta sin confirmar (la autoprueba decide igual);
 *   4  cada dibujo v3 se espera y se dice en el registro («r600 v3 sinc»);
 *   8  el descarte de caras de v3 con la otra convención;   10 v3 sin índices;   20 los búferes sin write-combining */
#define R3_REG_DEPURAR    0x28FFCu
#define R3D_SEM_V3        0x01u
#define R3D_COM_V3        0x02u
#define R3D_V3_SYNC       0x04u
#define R3D_V3_DESC_INV   0x08u
#define R3D_V3_SEM_IND    0x10u
#define R3D_V3_SEM_WC     0x20u
static uint32_t r3_dep;
static void dep_ler(void);

/* D406: la GART de las R7xx (ver el bloque «D406» más abajo). */
#define GART_BYTES   (16u << 20)
#define GART_PAG     (GART_BYTES / 4096u)
#define GART_TAB     (GART_PAG * 8u)
#define GART_RAM     (4u << 20)                /* el cuadro en la RAM: hasta 1024x1024x4 */
#define GART_RAM_PAG (GART_RAM / 4096u)
#define GART_P1      GART_RAM_PAG              /* página de prueba: la GPU escribe aquí */
#define GART_P2      (GART_RAM_PAG + 1)        /* página de prueba: la GPU lee de aquí */

static pci_disp_t disp3;
static bool quer_gart;
static struct {
    bool ligada, aparcada, aper_mudada, bm_nosso, regs_escritos;
    uint64_t ini;                              /* la ventana en el MC */
    g3d_buffer_t tab, prova;                   /* tabla y dos páginas de prueba en la VRAM visible */
    uint32_t ram;                              /* física = puntero (RAM baja): GART_RAM_PAG + 2 páginas */
    uint32_t relleno;
    uint32_t regs[R7G_MAX_PARES], orig[R7G_MAX_PARES];
    int nregs;
    uint32_t aper_orig[2];
    uint32_t cuadros, kb;
    char motivo[112];
} gart;


static inline uint32_t rd(uint32_t r)             { return *(volatile uint32_t *)(base.mmio + r); }
static inline void     wr(uint32_t r, uint32_t v) { *(volatile uint32_t *)(base.mmio + r) = v; }

/* D390: los registros que /sistema/conf/gpu3d-r600.conf puede tocar (la capa lee el fichero):
 * el contexto del 3D (SET_CONTEXT_REG: CB, DB, PA, SPI, SQ, VGT, 0x28000-0x28FFC) y la
 * configuración de VGT, PA y SQ (0x8800-0x8DFC), del SPI (0x9100-0x91FC) y DB_DEBUG/DB_WATERMARKS
 * (0x9830-0x983C), todos por la cola (SET_CONFIG_REG). Nunca GRBM ni CP (0x8000-0x87FF), el MC,
 * los relojes, la energía ni la pantalla, que van por MMIO y no por la cola. */
static const g3d_faixa_t r3_faixas_ajustes[] = {
    { 0x28000u, 0x28FFCu }, { 0x8800u, 0x8DFCu }, { 0x9100u, 0x91FCu }, { 0x9830u, 0x983Cu },
};

static void ajustes_por(r6_lote_t *l, int quando)
{
    for (int i = 0; i < gpu3d_ajustes.n; i++)
        if (gpu3d_ajustes.quando[i] == quando && gpu3d_ajustes.reg[i] != R3_REG_DEPURAR)   /* D457: banderas, no un registro */
            r63d_reg(l, gpu3d_ajustes.reg[i], gpu3d_ajustes.val[i]);
}

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
         fase, mc_zona, zona_bytes >> 10,
         cp_ligado ? (uint32_t)((g3d_arena_livre(&arena) + (arena_g_ok ? g3d_arena_livre(&arena_g) : 0)) >> 10) : 0, alvo_l, alvo_a, alvo_passo,
         (uint32_t)b_cor.gpu, (uint32_t)b_z.gpu, alvo_zbits);
    if (quer_gart || gart.ligada || gart.aparcada)
        klog("[GPU3D] r600 detalle (%s): GART %s%s%s; %u cuadros vueltos por ella\n", fase,
             gart.ligada ? "encendida" : gart.aparcada ? "APARCADA" : "apagada", gart.ligada ? "" : ": ", gart.ligada ? "" : gart.motivo,
             gart.cuadros);
    klog("[GPU3D] r600 detalle (%s): presentar (D432): %s; %u copias a la pantalla, %u sin retrazado a tiempo\n", fase, apres_porque,
         apres_n, apres_vblank_fora);
    klog("[GPU3D] r600 detalle (%s): v2 (D426): capacidades %x; destino %s; sombreadores de pixeles: %u generados, %u de la cache; "
         "SPI de v%d\n", fase, caps_v2, dest_vaga < 0 ? "principal" : "textura", ps_gerados, ps_acertos, modo_sh);
    klog("[GPU3D] r600 detalle (%s): v3 (D457): banderas %x; %u dibujos desde buferes, %u transformaciones, %u subidas (%u KB, %u en "
         "write-combining); monton %u KB (%u usados en %u buferes, %u creados); sombreadores de vertices v3: %u generados%s\n", fase, r3_dep,
         v3_dibujos, v3_transf, v3_subidas, v3_kb, v3_wc, b_vbm.usado ? vbm.total >> 10 : 0, vbm.usado >> 10, vbm.n, v3_montes, v3_vs_gerados,
         modo_sh == 3 ? "; el motor tiene puesto el rasterizador de v3" : "");
    uint32_t khz = hw_tsc_khz();
    if (khz && n_ler)
        klog("[GPU3D] r600 detalle (%s): %u cuadros leidos (%u kpx): esperando la valla %u us, copiando a la RAM %u us (%u us por cuadro)\n",
             fase, n_ler, n_ler_px / 1000u, (uint32_t)(t_valla * 1000u / khz), (uint32_t)(t_copia * 1000u / khz),
             (uint32_t)((t_valla + t_copia) * 1000u / khz / n_ler));
}

/* ------------------------------------------------------------------ sonda (solo lectura) */

static bool r3_sondar(const pci_disp_t *d, int geracao, const acel_tela_t *t, char *m, int nm)
{
    sondado = false;
    if (!r600_sondar(d, geracao, t, &base, m, nm)) return false;
    /* D408: la sonda ya acepta la VRAM por encima de 4 GB en el MC (para el 2D); el 3D trabaja con
     * direcciones de 32 bits y no se ha adaptado: se queda en software. */
    if (base.mc_alto) {
        snformatar(m, (size_t)nm, "la VRAM esta en el MC por encima de 4 GB (%x:%x): el 3D no lo admite todavia (D408)",
                   base.mc_alto, base.mc_tela);
        return false;
    }
    tela = *t;
    disp3 = *d;
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
    if ((base.vm0 & 1) && !gpu3d_r600_gart_nossa()) {   /* D406: la nuestra (la GART) sí */
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
    /* D386: lo que muestra la tarjeta (la pantalla con lo del 2D, la superficie del otro
     * controlador si tiene la imagen activa y los cursores), en bytes desde la pantalla;
     * la zona va al primer hueco libre detrás de todo eso. */
    vram_ocupado_t oc[5];
    int noc = 0;
    oc[noc].ini = 0; oc[noc++].fim = r600_fim_2d(t);
    for (int c = 0; c < 2; c++) {
        uint32_t o = c ? 0x800u : 0;
        uint32_t en = rd(D1GRPH_ENABLE + o), sup = rd(D1GRPH_SUPERFICIE + o), pit = rd(D1GRPH_PITCH + o);
        uint32_t hi = r7 ? rd(c ? D2GRPH_SUP_ALTA : D1GRPH_SUP_ALTA) : 0;
        uint32_t cc = rd(D1CUR_CONTROL + o), cs = rd(D1CUR_SURFACE + o);
        uint64_t bytes = avivo_superficie_bytes(rd(D1GRPH_CONTROL + o), pit, rd(D1GRPH_Y_FIM + o), rd(D1GRPH_DESL_Y + o),
                                                rd(D1MODE_VP_INI + o), rd(D1MODE_VP_TAM + o));
        klog("[GPU3D] r600: D%d CRTC %s, GRPH %s, superficie %x (alta %x) pitch %u: %u KB%s; cursor %s en MC %x\n", c + 1,
             rd(D1CRTC_CONTROL + o) & 1 ? "encendido" : "apagado", en & 1 ? "activo" : "apagado", sup, hi, pit,
             (uint32_t)(bytes >> 10), bytes == VRAM_MARGEM_DESCONHECIDA ? " (tamano desconocido: margen de 8 MB)" : "",
             cc & 1 ? "encendido" : "apagado", cs);
        /* Superficies por debajo de la pantalla: solo cuenta lo que llegue a pasarla. */
        if ((en & 1) && !hi && (uint64_t)sup + bytes > base.mc_tela) {
            oc[noc].ini = sup > base.mc_tela ? sup - base.mc_tela : 0;
            oc[noc++].fim = (uint64_t)sup + bytes - base.mc_tela;
        }
        if ((cc & 1) && (uint64_t)cs + 0x4000u > base.mc_tela) {
            oc[noc].ini = cs > base.mc_tela ? cs - base.mc_tela : 0;
            oc[noc++].fim = (uint64_t)cs + 0x4000u - base.mc_tela;
        }
    }
    uint64_t ini64;
    uint32_t tam;
    if (!vram_zona_buscar(oc, noc, oc[0].fim, lim, ZONA_MIN, ZONA_MAX, 1u << 20, &ini64, &tam)) {
        snformatar(m, (size_t)nm, "no caben 4 MB para el 3D detras del 2D y de las superficies activas (VRAM util %u KB)",
                   (uint32_t)(lim >> 10));
        return false;
    }
    ini = (uint32_t)ini64;
    if ((base.mc_tela + ini) & 0xFFu) { snformatar(m, (size_t)nm, "la zona 3D (MC %x) no esta alineada a 256 bytes", base.mc_tela + ini); return false; }
    int pisa = vram_zona_pisa(oc, noc, ini, tam);
    if (pisa >= 0) {
        snformatar(m, (size_t)nm, "la zona 3D (MC %x, %u KB) pisa lo que muestra la tarjeta (MC %x-%x)", base.mc_tela + ini,
                   tam >> 10, base.mc_tela + (uint32_t)oc[pisa].ini, base.mc_tela + (uint32_t)oc[pisa].fim);
        return false;
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

/* D402: la zona tiene dos partes. La visible (los primeros VISIVEL_MAX bytes) tiene ventana fija de la
 * CPU: cola, sombreadores, vértices y destino de color (que se lee cada cuadro). La grande (el resto,
 * hasta ZONA_MAX) no se mapea: van el Z (la CPU no lo toca) y las texturas, que la CPU escribe una vez
 * por una ventana móvil de 4 MB (janela_reservar/janela_pagina), así el kernel no gasta 128 MB de su
 * franja de ventanas (que es de 128 MB para todo). */
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
/* Ventana de la CPU sobre un búfer de la parte grande (NULL si no cabe en la ventana móvil). */
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
    r63d_valla(&l, (uint32_t)cur_cor->gpu, cur_cor->usado ? (uint32_t)cur_cor->bytes : 0, (uint32_t)cur_z->gpu,
               cur_z->usado ? (uint32_t)cur_z->bytes : 0, SCRATCH_VALLA, seq);   /* D426: el destino actual */
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
    est_valido = false; e3_valido = false;
    pendente = false;
    alvo_l = alvo_a = 0;
    memset(&b_vb, 0, sizeof b_vb); memset(&b_cor, 0, sizeof b_cor); memset(&b_z, 0, sizeof b_z); memset(b_tex, 0, sizeof b_tex);
    arena_g_ok = false;
    memset(&b_vbm, 0, sizeof b_vbm); memset(&vbm, 0, sizeof vbm); memset(vs3_cache, 0, sizeof vs3_cache);   /* D457 */
    vbm_sujo = e3_valido = t3_valido = false; vs3_posto = -1;
    v3_dibujos = v3_subidas = v3_kb = v3_vs_gerados = v3_transf = v3_montes = v3_wc = 0;

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
    zona_vis = zona_bytes < VISIVEL_MAX ? zona_bytes : VISIVEL_MAX;
    if (!zona || zona_fis_map != fis || zona_bytes_map != zona_vis) {
        zona = (volatile uint8_t *)janela_mapear_tipo(fis, zona_vis, PG_PCD | PG_PWT);
        zona_fis_map = fis; zona_bytes_map = zona_vis;
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
    /* D402: la última página de la parte grande también tiene que guardar lo escrito (por la ventana móvil). */
    if (zona_bytes > zona_vis) {
        g3d_buffer_t ult = { true, G3D_BUF_TEXTURA, mc_zona + zona_bytes - 4096u, 4096, NULL, 0, 0, 0, 0 };
        volatile uint32_t *u = (volatile uint32_t *)ver_grande(&ult);
        if (!u) { snformatar(m, (size_t)nm, "sin ventana movil del kernel para la zona 3D grande"); kfree(pfp); kfree(me); return false; }
        u[1023] = 0x5A6D3D01u; u[1022] = ~0x5A6D3D01u;
        if (u[1023] != 0x5A6D3D01u || u[1022] != ~0x5A6D3D01u) {
            snformatar(m, (size_t)nm, "la VRAM al final de la zona 3D (MC %x) no guarda lo escrito (%x)", mc_zona + zona_bytes - 4096u, u[1023]);
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
    uint32_t gbt = rd(0x98F0);
    alin_passo = r63d_alinhamento_passo(gbt);
    klog("[GPU3D] r600: etapa 3: motor 3D con %d registros por defecto (RAMCFG %x, tuberias %x, backends %x, GB_TILING_CONFIG %x, %u tuberias activas; paso alineado a %d pixeles)\n",
         np, le.mc_arb_ramcfg, le.cc_gc_shader_pipe_config, le.cc_rb_backend_disable, gbt, qd_pipes, alin_passo);

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
    {   /* D426: el de vértices de v2 (88 bytes por vértice) */
        uint32_t s2[R63D_VS2_MAX];
        int n2 = r63d_vs2(s2);
        for (int i = 0; i < n2; i++) sh[SH_VS2 / 4 + i] = s2[i];
    }
    memset(ps_cache, 0, sizeof ps_cache);
    memset(tex_fmt, 0, sizeof tex_fmt);
    modo_sh = 1; ps_posto = -1; dest_vaga = -1; cur_cor = &b_cor; cur_z = &b_z;
    g3d_arena_iniciar(&arena, (uint64_t)mc_zona + FIXO, (uint8_t *)zona + FIXO, zona_vis - FIXO);
    arena_g_ok = zona_bytes > zona_vis;
    if (arena_g_ok) g3d_arena_iniciar(&arena_g, (uint64_t)mc_zona + zona_vis, NULL, zona_bytes - zona_vis);
    klog("[GPU3D] r600: etapa 6: zona de %u KB: %u KB con ventana fija de la CPU y %u KB sin ella (Z y texturas)\n",
         zona_bytes >> 10, zona_vis >> 10, arena_g_ok ? (zona_bytes - zona_vis) >> 10 : 0);
    if (!g3d_buffer_criar(&arena, &b_vb, G3D_BUF_LOTE, VB_BYTES, 4096)) { snformatar(m, (size_t)nm, "sin sitio para los vertices"); return false; }
    vb_usado = 0;
    r63d_lote(&l, b, 1024);
    r63d_estado_base(&l, familia, r7, qd_pipes, mc_zona + RING_BYTES);
    ajustes_por(&l, G3D_AJ_INICIO);    /* D390: /sistema/conf/gpu3d-r600.conf, tras el estado base */
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

/* ================================================================ D406: GART de las R7xx y la vuelta del cuadro
 *
 * Solo con «gpu3d encender gtt» (variante GPU3D_CAB_GTT) y solo en R7xx (las R6xx encienden la GART con
 * otros registros, r600_pcie_gart_enable: sin máquina para probarlo, no se hace). Secuencia de Linux 6.6
 * (gart_r700_logica.h). La ventana es de 16 MB detrás de la VRAM; la tabla (32 KB) va en la parte visible de
 * la zona 3D; las entradas apuntan a 4 MB seguidos de RAM baja del kernel (el cuadro de hasta 1024x1024) y a
 * dos páginas para la autoprueba; el resto, a la página de relleno. Para dar los pasos de uno en uno en una
 * máquina nueva, /sistema/conf/gart-r700.paso con un número (0 solo lee, 1 + la tabla, 2 + los registros y la
 * TLB, 3 + bus maestro y la GPU escribe en la RAM, 4 + la GPU lee de la RAM): se para ahí y lo deshace.
 * Sin fichero, todo y queda encendida. Si el motor no para al apagar, la GART queda «aparcada» (todas las
 * entradas a la página de relleno) en vez de apagarla con la GPU a medias (lo aprendido en la X1400, D387). */
/* D406 (v8.5.5): R7xx donde la GART y la vuelta del cuadro por CP_DMA ya se han probado en hardware real: en
 * ellas se encienden solas con el 3D (sin «gtt»); «gpu3d encender sin-gtt» (GPU3D_CAB_SEM_GTT) lo evita. Si no se
 * enciende, el cuadro vuelve por la CPU como antes. El 3D sigue apagado por defecto. */
static const struct { uint16_t fab, modelo; } gart_confirmadas[] = {
    { 0x1002, 0x9553 },   /* Mobility HD 4530/4570 (RV710) del HP dv7: pasos 0-4, 945 fps frente a 50, 15.000 cuadros, 2026-11-19 */
};
static bool gart_confirmada(const pci_disp_t *d)
{
    for (unsigned k = 0; k < sizeof gart_confirmadas / sizeof gart_confirmadas[0]; k++)
        if (gart_confirmadas[k].fab == d->fabricante && gart_confirmadas[k].modelo == d->modelo) return true;
    return false;
}

/* D406: para la sonda del 2D (acel_r600.c): la memoria virtual encendida es la nuestra. */
bool gpu3d_r600_gart_nossa(void) { return gart.ligada; }

static int gart_paso_max(void)
{
    uint8_t *b = NULL;
    uint32_t n = 0;
    if (vfs_ler_inteiro("/sistema/conf/gart-r700.paso", &b, &n) < 0 || !b) return 99;
    int v = 0;
    bool algum = false;
    for (uint32_t i = 0; i < n && b[i] >= '0' && b[i] <= '9'; i++) { v = v * 10 + (b[i] - '0'); algum = true; }
    kfree(b);
    return algum ? v : 99;
}

static void gart_tab_pte(uint32_t i, uint64_t e)
{
    volatile uint32_t *t = (volatile uint32_t *)gart.tab.cpu;
    t[2 * i] = (uint32_t)e;
    t[2 * i + 1] = (uint32_t)(e >> 32);
}

/* r600_pcie_gart_tlb_flush: el HDP primero (la tabla la escribió la CPU por la apertura), luego la
 * invalidación del contexto 0 en la ventana y esperar la respuesta (2 = fallo). */
static bool gart_tlb(void)
{
    r600_hdp_vaciar(&base, (volatile uint32_t *)gart.tab.cpu);
    wr(R7G_VM_CONTEXT0_INVAL_LO, (uint32_t)(gart.ini >> 12));
    wr(R7G_VM_CONTEXT0_INVAL_HI, (uint32_t)((gart.ini + GART_BYTES - 1) >> 12));
    wr(R7G_VM_CONTEXT0_REQUEST_RESPONSE, R7G_REQUEST_TYPE(1));
    prazo_t p = prazo_ms(10);
    do {
        uint32_t r = (rd(R7G_VM_CONTEXT0_REQUEST_RESPONSE) & R7G_RESPONSE_MASK) >> R7G_RESPONSE_SHIFT;
        if (r == 2) { klog("[GPU3D] r600 GART: la invalidacion de la TLB fallo (respuesta 2)\n"); return false; }
        if (r) return true;
    } while (!prazo_vencido(&p));
    klog("[GPU3D] r600 GART: la invalidacion de la TLB no responde\n");
    return false;
}

static void gart_log(const char *fase)
{
    klog("[GPU3D] r600 GART (%s): FB_LOCATION %x SYS_APER %x-%x (por defecto %x) AGP %x-%x base %x; VM_CONTEXT0_CNTL %x "
         "L2_CNTL %x L2_STATUS %x MD_L1_TLB0 %x MB_L1_TLB0 %x PT %x-%x base %x fallo %x\n", fase, rd(R7G_MC_VM_FB_LOCATION),
         rd(R7G_MC_VM_SYS_APER_LOW), rd(R7G_MC_VM_SYS_APER_HIGH), rd(R7G_MC_VM_SYS_APER_DEFAULT), rd(R7G_MC_VM_AGP_BOT),
         rd(R7G_MC_VM_AGP_TOP), rd(R7G_MC_VM_AGP_BASE), rd(R7G_VM_CONTEXT0_CNTL), rd(R7G_VM_L2_CNTL), rd(R7G_VM_L2_STATUS),
         rd(R7G_MC_VM_MD_L1_TLB0_CNTL), rd(R7G_MC_VM_MB_L1_TLB0_CNTL), rd(R7G_VM_CONTEXT0_PT_START), rd(R7G_VM_CONTEXT0_PT_END),
         rd(R7G_VM_CONTEXT0_PT_BASE), rd(R7G_VM_CONTEXT0_FAULT_DEFAULT));
}

/* Deja los registros como estaban (rv770_pcie_gart_disable y después los valores de antes). */
static void gart_registos_fora(void)
{
    if (gart.regs_escritos) {
        r7g_par_t p[R7G_MAX_PARES];
        int n = r7g_desligar(p);
        for (int i = 0; i < n; i++) wr(p[i].reg, p[i].valor);
        for (int i = gart.nregs - 1; i >= 0; i--) wr(gart.regs[i], gart.orig[i]);
        (void)gart_tlb();
        gart.regs_escritos = false;
    }
    if (gart.aper_mudada) {
        wr(R7G_MC_VM_SYS_APER_LOW, gart.aper_orig[0]);
        wr(R7G_MC_VM_SYS_APER_HIGH, gart.aper_orig[1]);
        gart.aper_mudada = false;
    }
}

/* Apagar: con el motor parado, todo como estaba; si no para, aparcada (encendida, todo al relleno). */
static void gart_desligar(void)
{
    if (!gart.ligada && !gart.regs_escritos) return;
    bool parado = true;
    if (cp_ligado) {
        prazo_t p = prazo_ms(50);
        while (rd(R6_GRBM_STATUS) & R6_GUI_ACTIVE)
            if (prazo_vencido(&p)) { parado = false; break; }
        if (rd(R6_CP_RB_RPTR) != wptr) parado = false;
    }
    if (!parado) {
        if (gart.tab.cpu) {
            for (uint32_t i = 0; i < GART_PAG; i++) gart_tab_pte(i, r7g_pte_relleno(gart.relleno));
            (void)gart_tlb();
        }
        gart.aparcada = true;
        gart.ligada = false;
        klog("[GPU3D] r600 GART aparcada (el motor no para: GRBM_STATUS %x, RPTR %x, WPTR %x): encendida con todo a la pagina "
             "de relleno; no se reutiliza hasta reiniciar\n", rd(R6_GRBM_STATUS), rd(R6_CP_RB_RPTR), wptr);
        return;
    }
    gart_registos_fora();
    if (gart.bm_nosso) {
        pci_escreve32(&disp3, 0x04, (pci_le32(&disp3, 0x04) & 0xFFFFu) & ~0x04u);
        gart.bm_nosso = false;
    }
    gart.ligada = false;
    gart_log("apagada");
}

/* Copia de prueba por CP_DMA con su valla. */
static bool gart_copiar(uint32_t orig, uint32_t dest, uint32_t bytes)
{
    uint32_t b[48];
    r6_lote_t l;
    r63d_lote(&l, b, 48);
    r63d_cp_dma(&l, orig, dest, bytes);
    g3d_vala_t v;
    return emitir(&l) && submeter(&v) && valla_esperar(v, PRAZO_MS);
}

/* 1 encendida; 0 no se encendió y nada quedó tocado (el 3D sigue sin ella); -1 la GPU no respondió. */
static int gart_ligar(char *m, int nm)
{
    int ate = gart_paso_max();
    if (ate != 99) klog("[GPU3D] r600 GART: /sistema/conf/gart-r700.paso dice %d: se para ahi y se deshace\n", ate);
    if (!r7) { snformatar(m, (size_t)nm, "solo en R7xx (las R6xx tienen otra GART)"); return 0; }
    if (gart.aparcada) { snformatar(m, (size_t)nm, "quedo aparcada tras un motor que no paraba: hasta reiniciar"); return 0; }
    bool rv740 = familia == R63D_RV740;
    /* Paso 0: solo leer. */
    gart_log("paso 0, antes de tocar nada");
    uint32_t cmd = pci_le32(&disp3, 0x04);
    klog("[GPU3D] r600 GART paso 0: PCI orden %x estado %x\n", cmd & 0xFFFF, cmd >> 16);
    static pci_disp_t v[64];
    int nv = pci_listar(v, 64);
    for (int i = 0; i < nv; i++)
        if (v[i].classe == 0x06 && v[i].subclasse == 0x04 && ((pci_le32(&v[i], 0x18) >> 8) & 0xFF) == disp3.barramento)
            klog("[GPU3D] r600 GART paso 0: puente %x:%x.%x (%04x:%04x) orden %x\n", v[i].barramento, v[i].dispositivo,
                 v[i].funcao, v[i].fabricante, v[i].modelo, pci_le32(&v[i], 0x04) & 0xFFFF);
    if (rd(R7G_VM_CONTEXT0_CNTL) & 1) { snformatar(m, (size_t)nm, "el contexto 0 ya estaba encendido: no se toca"); return 0; }
    bool mudar_aper = false;
    int c = r7g_janela(rd(R7G_MC_VM_FB_LOCATION), rd(R7G_MC_VM_SYS_APER_LOW), rd(R7G_MC_VM_SYS_APER_HIGH), rd(R7G_MC_VM_AGP_BOT),
                       rd(R7G_MC_VM_AGP_TOP), GART_BYTES, &gart.ini, &mudar_aper);
    if (c) { snformatar(m, (size_t)nm, "%s", r7g_janela_motivo(c)); return 0; }
    klog("[GPU3D] r600 GART paso 0: ventana de 16 MB en MC %x-%x%s\n", (uint32_t)gart.ini, (uint32_t)(gart.ini + GART_BYTES - 1),
         mudar_aper ? "; la apertura del sistema no cubre la VRAM: se pondra como Linux" : "");
    if (ate == 0) { snformatar(m, (size_t)nm, "parada en el paso 0 (solo lectura)"); return 0; }

    /* Paso 1: memoria y tabla. */
    if (!gart.ram) gart.ram = memfisica_alocar_seguidas(GART_RAM_PAG + 2);
    if (!gart.relleno) { gart.relleno = memfisica_alocar(); if (gart.relleno) memset((void *)gart.relleno, 0, 4096); }
    if (!gart.ram || !gart.relleno) { snformatar(m, (size_t)nm, "sin %u KB seguidos de RAM baja", (GART_RAM_PAG + 2) * 4); return 0; }
    if (!gart.tab.usado && !g3d_buffer_criar(&arena, &gart.tab, G3D_BUF_LOTE, GART_TAB, 4096)) {
        snformatar(m, (size_t)nm, "sin sitio para la tabla en la VRAM");
        return 0;
    }
    if (!gart.prova.usado && !g3d_buffer_criar(&arena, &gart.prova, G3D_BUF_LOTE, 8192, 4096)) {
        snformatar(m, (size_t)nm, "sin sitio para la prueba en la VRAM");
        return 0;
    }
    for (uint32_t i = 0; i < GART_PAG; i++)
        gart_tab_pte(i, i < GART_RAM_PAG + 2 ? r7g_pte(gart.ram + i * 4096u) : r7g_pte_relleno(gart.relleno));
    volatile uint32_t *t = (volatile uint32_t *)gart.tab.cpu;
    if (t[0] != (uint32_t)r7g_pte(gart.ram) || t[2 * (GART_PAG - 1)] != (uint32_t)r7g_pte_relleno(gart.relleno)) {
        snformatar(m, (size_t)nm, "la tabla no se lee como se escribio (%x)", t[0]);
        return 0;
    }
    klog("[GPU3D] r600 GART paso 1: tabla de %u KB en MC %x; RAM del cuadro en %x (%u KB), relleno en %x; entrada 0 %x:%x\n",
         GART_TAB >> 10, (uint32_t)gart.tab.gpu, gart.ram, GART_RAM >> 10, gart.relleno, t[1], t[0]);
    if (ate == 1) { snformatar(m, (size_t)nm, "parada en el paso 1 (tabla escrita)"); return 0; }

    /* Paso 2: registros y TLB (rv770_pcie_gart_enable), con el motor parado. */
    if (rd(R6_GRBM_STATUS) & R6_GUI_ACTIVE) { snformatar(m, (size_t)nm, "el motor no esta parado (GRBM_STATUS %x)", rd(R6_GRBM_STATUS)); return 0; }
    gart.nregs = r7g_regs_tocados(rv740, gart.regs);
    for (int i = 0; i < gart.nregs; i++) gart.orig[i] = rd(gart.regs[i]);
    if (mudar_aper) {
        uint32_t lo, hi;
        r7g_aper(rd(R7G_MC_VM_FB_LOCATION), &lo, &hi);
        gart.aper_orig[0] = rd(R7G_MC_VM_SYS_APER_LOW);
        gart.aper_orig[1] = rd(R7G_MC_VM_SYS_APER_HIGH);
        gart.aper_mudada = true;
        wr(R7G_MC_VM_SYS_APER_LOW, lo);
        wr(R7G_MC_VM_SYS_APER_HIGH, hi);
    }
    r7g_par_t p[R7G_MAX_PARES];
    int np = r7g_ligar(rv740, gart.ini, gart.ini + GART_BYTES - 1, gart.tab.gpu, gart.relleno, p);
    gart.regs_escritos = true;
    for (int i = 0; i < np; i++) wr(p[i].reg, p[i].valor);
    bool tlb = gart_tlb();
    gart_log("paso 2, encendida");
    if (!tlb) { gart_registos_fora(); snformatar(m, (size_t)nm, "la TLB no se invalida"); return 0; }
    if (ate == 2) { gart_registos_fora(); gart_log("paso 2, deshecho"); snformatar(m, (size_t)nm, "parada en el paso 2 (registros)"); return 0; }

    /* Paso 3: bus maestro y la GPU escribe en la RAM (VRAM -> RAM por la ventana). */
    if (!(cmd & 0x04)) { pci_escreve32(&disp3, 0x04, (cmd & 0xFFFFu) | 0x04u); gart.bm_nosso = true; }
    gart.ligada = true;   /* desde aquí, gart_desligar sabe deshacerlo (o aparcarla) */
    volatile uint32_t *pv = (volatile uint32_t *)gart.prova.cpu;
    uint32_t *r1 = (uint32_t *)(gart.ram + GART_P1 * 4096u), *r2 = (uint32_t *)(gart.ram + GART_P2 * 4096u);
    for (uint32_t i = 0; i < 1024; i++) { pv[i] = 0x6A700000u ^ (i * 0x9E3779B1u); r1[i] = 0; }
    if (!gart_copiar((uint32_t)gart.prova.gpu, (uint32_t)gart.ini + GART_P1 * 4096u, 4096)) {
        gart_log("paso 3, sin valla");
        gart_desligar();
        snformatar(m, (size_t)nm, "la copia VRAM -> RAM por la GART no termina");
        return -1;
    }
    uint32_t mal = 0, pm = 0;
    for (uint32_t i = 0; i < 1024; i++) if (r1[i] != (0x6A700000u ^ (i * 0x9E3779B1u))) { if (!mal) pm = i; mal++; }
    klog("[GPU3D] r600 GART paso 3: la GPU escribio la RAM: %u de 1024 palabras distintas (primera %u: %x)\n", mal, pm, r1[pm]);
    if (mal) { gart_desligar(); snformatar(m, (size_t)nm, "la GPU no deja en la RAM lo que copia (%u palabras distintas)", mal); return 0; }
    if (ate == 3) { gart_desligar(); snformatar(m, (size_t)nm, "parada en el paso 3 (VRAM -> RAM bien)"); return 0; }

    /* Paso 4: la GPU lee de la RAM (RAM -> VRAM). */
    for (uint32_t i = 0; i < 1024; i++) { r2[i] = 0x51A90000u ^ (i * 0x85EBCA6Bu); pv[1024 + i] = 0; }
    if (!gart_copiar((uint32_t)gart.ini + GART_P2 * 4096u, (uint32_t)gart.prova.gpu + 4096u, 4096)) {
        gart_log("paso 4, sin valla");
        gart_desligar();
        snformatar(m, (size_t)nm, "la copia RAM -> VRAM por la GART no termina");
        return -1;
    }
    mal = 0;
    for (uint32_t i = 0; i < 1024; i++) if (pv[1024 + i] != (0x51A90000u ^ (i * 0x85EBCA6Bu))) { if (!mal) pm = i; mal++; }
    klog("[GPU3D] r600 GART paso 4: la GPU leyo la RAM: %u de 1024 palabras distintas\n", mal);
    if (mal) { gart_desligar(); snformatar(m, (size_t)nm, "la GPU no lee bien la RAM (%u palabras distintas)", mal); return 0; }
    if (ate == 4) { gart_desligar(); snformatar(m, (size_t)nm, "parada en el paso 4 (RAM -> VRAM bien)"); return 0; }
    gart.kb = GART_RAM >> 10;
    klog("[GPU3D] r600 GART encendida: ventana de 16 MB en MC %x, %u KB de RAM para el cuadro; la vuelta va por CP_DMA\n",
         (uint32_t)gart.ini, gart.kb);
    return 1;
}

/* D406: lo que quedó (para «gpu3d» e infopc): GPU3D_VAR_GTT si la vuelta va por la GART. */
void gpu3d_r600_estado(uint32_t *variante, uint32_t *ram_kb)
{
    *variante = gart.ligada ? GPU3D_VAR_GTT : 0;
    *ram_kb = gart.ligada ? gart.kb : 0;
}

static void r3_desligar(void)
{
    if (!base.mmio || !sondado) return;
    if (cp_ligado) {
        prazo_t p = prazo_ms(50);
        while (rd(R6_GRBM_STATUS) & R6_GUI_ACTIVE)
            if (prazo_vencido(&p)) { klog("[GPU3D] r600: el motor sigue ocupado al parar el CP (GRBM_STATUS %x)\n", rd(R6_GRBM_STATUS)); break; }
    }
    gart_desligar();   /* D406: antes de parar el CP (si el motor no para, queda aparcada) */
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
    quer_gart = (variante & GPU3D_CAB_GTT) != 0;
#ifdef SAGUI_SEM_FIRMWARE
    (void)d; (void)geracao; (void)t; (void)r3_sondar; (void)ligar_cp; (void)prefixar_etapa; (void)gart_ligar; (void)gart_confirmada; (void)dep_ler;
    snformatar(m, (size_t)nm, "esta compilacion no lleva firmware (SEM_FIRMWARE=1): sin microcodigo no hay 3D");
    return G3D_NAO;
#else
    etapa = 0;
    gart.tab.usado = gart.prova.usado = false;   /* D406: la arena se vuelve a crear al encender */
    if (!r3_sondar(d, geracao, t, m, nm)) { prefixar_etapa(m, nm); return G3D_NAO; }   /* solo leyó: no es un fallo de la GPU */
    dep_ler();   /* D457 */
    if (!quer_gart && r7 && !(variante & GPU3D_CAB_SEM_GTT) && gart_confirmada(d)) {
        quer_gart = true;
        klog("[GPU3D] r600 GART: tarjeta confirmada (%04x:%04x): se enciende sola con el 3D («gpu3d encender sin-gtt» la evita)\n",
             d->fabricante, d->modelo);
    }
    if (ligar_cp(m, nm)) {
        if (!quer_gart) return G3D_OK;
        char mg[112];
        int g = gart_ligar(mg, sizeof mg);
        if (g > 0) return G3D_OK;
        snformatar(gart.motivo, sizeof gart.motivo, "%s", mg);
        klog("[GPU3D] r600 GART: no se enciende: %s%s\n", mg, g < 0 ? " (la GPU no respondio: el 3D se apaga)" : "; el cuadro sigue volviendo por la CPU");
        if (g == 0) return G3D_OK;
        snformatar(m, (size_t)nm, "GART: %s", mg);
        return G3D_FALHA;
    }
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
    } else {
        /* Revisión de D457: sin «pendente» puede quedar una valla mandada y no esperada (un dibujo con «vala»):
         * esperar a la última siempre, para que quien va a escribir una textura o un búfer no pise lo que la GPU
         * aún lee. Si ya llegó, es una lectura del registro. */
        if (rd(SCRATCH_VALLA) == seq) return G3D_OK;
        vala = seq;
    }
    return valla_esperar(vala, ms) ? G3D_OK : G3D_FALHA;
}

static int r3_alvo(int larg, int alt, int zbits)
{
    if (!cp_ligado) return G3D_FALHA;
    if (larg > MAX_ALVO || alt > MAX_ALVO) return G3D_NAO;
    if (r3_esperar(0, GPU3D_PRAZO_MS) != G3D_OK) return G3D_FALHA;
    soltar(&b_cor);
    soltar(&b_z);
    soltar(&b_zdest);                /* D426: si se quedó dibujando en una textura */
    dest_vaga = -1; cur_cor = &b_cor; cur_z = &b_z;
    alvo_l = alvo_a = 0;
    int passo = r63d_passo(larg, alin_passo), alt_buf = (alt + 63) & ~63;   /* D402; holgura para el mosaico del Z */
    uint32_t bytes = (uint32_t)passo * (uint32_t)alt_buf * 4u;
    if (!g3d_buffer_criar(&arena, &b_cor, G3D_BUF_ALVO, bytes, 4096)) return G3D_NAO;
    if (!criar_grande(&b_z, G3D_BUF_Z, bytes)) { soltar(&b_cor); return G3D_NAO; }
    b_z.cpu = NULL;                  /* el Z va en mosaico: la CPU no lo toca */
    zbits = zbits == 16 ? 16 : 24;
    uint32_t b[64];
    r6_lote_t l;
    r63d_lote(&l, b, 64);
    r63d_alvo(&l, (uint32_t)b_cor.gpu, (uint32_t)b_z.gpu, passo, alt_buf, larg, alt, zbits, R63D_PRECISA_SBU(familia));
    if (!emitir(&l)) return G3D_FALHA;
    alvo_l = larg; alvo_a = alt; alvo_passo = passo; alvo_zbits = zbits;
    b_cor.passo = (uint32_t)passo * 4; b_cor.larg = larg; b_cor.alt = alt; b_cor.bpp = 32;
    est_valido = false; e3_valido = false;
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
    uint32_t b[128 + 3 * G3D_AJUSTES_MAX];
    r6_lote_t l;
    r63d_lote(&l, b, 128 + 3 * G3D_AJUSTES_MAX);
    if (modo_sh != 1) {   /* D426: tras un dibujo v2, el SPI y lo que el estado de v1 no pone */
        if (modo_sh == 3) r63d_v3_sair(&l);   /* D457: sin recorte ni vista, vértices ya en la pantalla */
        r63d_spi(&l, 3);
        r63d_estado_v1(&l);
        modo_sh = 1; sh_atual = -1; ps_posto = -1;
    }
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
    ajustes_por(&l, G3D_AJ_DIBUJO);    /* D390: después del estado, para que no lo pise */
    if (!emitir(&l)) return false;
    e3_valido = false;                 /* D457: el estado de los píxeles de v3 ya no es el que quedó */
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
    if (vaga == dest_vaga) return G3D_NAO;   /* D426: es el destino actual (la capa ya no lo deja) */
    /* Que la GPU no esté leyendo la textura mientras se cambia o se suelta. */
    if (r3_esperar(0, GPU3D_PRAZO_MS) != G3D_OK) return G3D_FALHA;
    soltar(b);
    tex_fmt[vaga] = G3D_FMT_ARGB8888;
    est_valido = false; e3_valido = false;
    if (!argb) return G3D_OK;
    int passo = r63d_passo(larg, alin_passo);   /* lineal alineada: múltiplo de 64 o 128 texels según el grupo (D402) */
    if (!criar_grande(b, G3D_BUF_TEXTURA, (uint64_t)passo * (uint32_t)((alt + 7) & ~7) * 4u)) return G3D_NAO;
    b->passo = (uint32_t)passo * 4; b->larg = larg; b->alt = alt; b->bpp = 32;
    volatile uint32_t *d = (volatile uint32_t *)ver_grande(b);
    if (!d) { soltar(b); return G3D_NAO; }
    for (int y = 0; y < alt; y++)
        for (int x = 0; x < larg; x++) d[y * passo + x] = argb[y * larg + x];
    return G3D_OK;
}

static int r3_desenhar(const gpu3d_estado_t *e, const gpu3d_vert_t *v, int n, g3d_vala_t *vala)
{
    if (!cp_ligado || !alvo_l) return G3D_FALHA;
    if (n <= 0) return G3D_OK;
    if ((e->flags & G3D_TEXTURA) && e->textura < GPU3D_TEXTURAS && tex_fmt[e->textura] != G3D_FMT_ARGB8888) return G3D_NAO;   /* D426 */
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
    uint64_t t0 = prazo_tsc();
    if (r3_esperar(0, GPU3D_PRAZO_MS) != G3D_OK) return G3D_FALHA;
    uint64_t t1 = prazo_tsc();
    t_valla += t1 - t0;
    /* D406: con la GART, la GPU deja el destino en la RAM (CP_DMA, el destino entero de una vez) y la CPU
     * lo copia de la RAM con caché; sin ella, la CPU lo lee de la VRAM sin caché (lo de la D402). */
    uint32_t bytes_cor = (uint32_t)alvo_passo * (uint32_t)alvo_a * 4u;
    if (gart.ligada && bytes_cor <= GART_RAM && dest_vaga < 0) {
        uint32_t b[64];
        r6_lote_t l;
        r63d_lote(&l, b, 64);
        r63d_cp_dma(&l, (uint32_t)b_cor.gpu, (uint32_t)gart.ini, bytes_cor);
        if (!emitir(&l)) return G3D_FALHA;
        pendente = true;
        if (r3_esperar(0, GPU3D_PRAZO_MS) != G3D_OK) return G3D_FALHA;
        uint64_t t2 = prazo_tsc();
        t_valla += t2 - t1;
        const uint32_t *o = (const uint32_t *)gart.ram;
        for (int y = 0; y < alvo_a; y++) {
            uint32_t *d = dest + y * passo_px;
            const uint32_t *f = o + y * alvo_passo;
            for (int x = 0; x < alvo_l; x++) d[x] = f[x] & 0x00FFFFFFu;
        }
        t_copia += prazo_tsc() - t2;
        gart.cuadros++;
        n_ler++;
        n_ler_px += (uint32_t)(alvo_l * alvo_a);
        if (n_ler % 600 == 0) r3_detalle("cada 600 cuadros");
        return G3D_OK;
    }
    /* D426: dibujando en una textura, se lee esa textura (por la ventana móvil si está en la parte grande). */
    volatile uint32_t *s = (volatile uint32_t *)(dest_vaga >= 0 ? ver_grande(cur_cor) : b_cor.cpu);
    if (!s) return G3D_FALHA;
    /* D402: la ventana de la CPU es sin caché (UC) y cada lectura es una petición por el PCIe;
     * de 4 en 4 bytes el dv7 traía 5,5 MB/s. Con SSE2 se lee de 16 en 16 (una fila cada vez:
     * fpu_kernel_inicio cierra las interrupciones) y el alfa se quita ya en la RAM. */
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
    if (n_ler % 600 == 0) r3_detalle("cada 600 cuadros");
    return G3D_OK;
}

static void r3_memoria(uint32_t *total_kb, uint32_t *livre_kb)
{
    *total_kb = zona_bytes >> 10;
    *livre_kb = cp_ligado ? (uint32_t)((g3d_arena_livre(&arena) + (arena_g_ok ? g3d_arena_livre(&arena_g) : 0)) >> 10) : 0;
}

/* D390: lo que la CPU ve del destino: el color (ARGB8888 lineal). El Z va en mosaico: no se ve. */
static void r3_vistas(g3d_vista_t *c, g3d_vista_t *z)
{
    (void)z;
    if (!cp_ligado || !alvo_l || !b_cor.usado || !b_cor.cpu || dest_vaga >= 0) return;
    *c = (g3d_vista_t){ b_cor.cpu, (uint32_t)alvo_passo * 4u, alvo_l, alvo_a, 32, 0, 0, false };
}

/* ------------------------------------------------------------------ gpu3d v2 (D426) */

/* Lo que decide el sombreador de píxeles (la combinación, las constantes y la niebla), en bytes. */
static void ps_desc(const gpu3d_estado2_t *e, uint8_t d[PS2_DESC])
{
    memset(d, 0, PS2_DESC);
    int nu = e->unidades > 4 ? 4 : e->unidades, k = 0;
    d[k++] = (uint8_t)nu;
    d[k++] = (e->flags & G3D2_NEBLINA) ? 1 : 0;
    if (e->flags & G3D2_NEBLINA) { d[k++] = (uint8_t)(e->cor_neblina >> 16); d[k++] = (uint8_t)(e->cor_neblina >> 8); d[k++] = (uint8_t)e->cor_neblina; }
    else k += 3;
    for (int u = 0; u < nu; u++) {
        const gpu3d_unidade_t *x = &e->u[u];
        d[k++] = x->ativa;
        if (!x->ativa) { k += 23; continue; }
        d[k++] = x->coord; d[k++] = x->rgb_func; d[k++] = x->alfa_func; d[k++] = x->rgb_escala; d[k++] = x->alfa_escala;
        for (int j = 0; j < 3; j++) { d[k++] = x->rgb_fonte[j]; d[k++] = x->rgb_opd[j]; d[k++] = (uint8_t)((x->alfa_fonte[j] << 4) | x->alfa_opd[j]); }
        for (int j = 0; j < 4; j++) d[k++] = (uint8_t)(x->constante >> (8 * j));
        k += 1;
    }
}

/* El sombreador de píxeles de un estado: de la caché o generado y escrito en su sitio de la VRAM. -1: no cabe;
 * -2: la GPU no terminó (al esperar para reutilizar un sitio). */
static int ps_programa(const gpu3d_estado2_t *e)
{
    static r63d_ps2_t ps;
    uint8_t d[PS2_DESC];
    ps_desc(e, d);
    ps_relogio++;
    for (int i = 0; i < PS2_CACHE; i++)
        if (ps_cache[i].usado && !memcmp(ps_cache[i].desc, d, PS2_DESC)) { ps_cache[i].uso = ps_relogio; ps_acertos++; return i; }
    int l = -1;
    for (int i = 0; i < PS2_CACHE && l < 0; i++) if (!ps_cache[i].usado) l = i;
    if (l < 0) {
        l = 0;
        for (int i = 1; i < PS2_CACHE; i++) if (ps_cache[i].uso < ps_cache[l].uso) l = i;
        /* Un dibujo pendiente puede estar usando ese sitio: se espera antes de escribirlo. */
        pendente = true;
        if (r3_esperar(0, GPU3D_PRAZO_MS) != G3D_OK) return -2;
    }
    r63d_ps2_gerar(&ps, e, r7);
    ps_gerados++;
    if (ps.erro) {
        klog("[GPU3D] r600 v2: un sombreador de pixeles no cabe (%d grupos): ese dibujo va por software\n", ps.grupos);
        return -1;
    }
    volatile uint32_t *sh = (volatile uint32_t *)(zona + RING_BYTES + SH_PS2 + (uint32_t)l * 4096u);
    for (int i = 0; i < ps.n; i++) sh[i] = ps.p[i];
    ps_cache[l].usado = true; ps_cache[l].uso = ps_relogio; ps_cache[l].pal = ps.n;
    memcpy(ps_cache[l].desc, d, PS2_DESC);
    if (ps_posto == l) ps_posto = -1;
    return l;
}

/* Vértices v2 (88 bytes, tal cual) al búfer de vértices y el dibujo. */
static int vertices2(const gpu3d_vert2_t *v, int n)
{
    uint32_t bytes = (uint32_t)n * R63D_VERT2_BYTES;
    if (bytes > b_vb.bytes) return G3D_NAO;
    if (vb_usado + bytes > b_vb.bytes) {
        pendente = true;
        if (r3_esperar(0, GPU3D_PRAZO_MS) != G3D_OK) return G3D_FALHA;
    }
    volatile uint32_t *d = (volatile uint32_t *)(b_vb.cpu + vb_usado);
    const uint32_t *o = (const uint32_t *)v;
    for (uint32_t i = 0; i < bytes / 4; i++) d[i] = o[i];
    uint32_t b[32];
    r6_lote_t l;
    r63d_lote(&l, b, 32);
    r63d_desenhar2(&l, (uint32_t)b_vb.gpu + vb_usado, n, R63D_VERT2_BYTES, R63D_SEM_CACHE_VTX(familia));
    if (!emitir(&l)) return G3D_FALHA;
    vb_usado += bytes;
    pendente = true;
    return G3D_OK;
}

static int desenhar2_int(const gpu3d_estado2_t *e, const gpu3d_vert2_t *v, int n)
{
    if (!cp_ligado || !alvo_l) return G3D_FALHA;
    if (n <= 0) return G3D_OK;
    int nu = e->unidades > 4 ? 4 : e->unidades;
    for (int u = 0; u < nu; u++) {
        const gpu3d_unidade_t *x = &e->u[u];
        if (!x->ativa) continue;
        if (x->vaga >= GPU3D_TEXTURAS || (int)x->vaga == dest_vaga || x->tipo != G3D_TEX_2D || !b_tex[x->vaga].usado ||
            (x->filtro & (G3D_FIL_MIP_VECINO | G3D_FIL_MIP_LINEAR))) return G3D_NAO;
    }
    int pi = ps_programa(e);
    if (pi == -2) return G3D_FALHA;
    if (pi < 0) return G3D_NAO;
    uint32_t b[256 + 3 * G3D_AJUSTES_MAX];
    r6_lote_t l;
    r63d_lote(&l, b, 256 + 3 * G3D_AJUSTES_MAX);
    if (modo_sh != 2) {
        if (modo_sh == 3) r63d_v3_sair(&l);   /* D457: el SPI de v3 es el de v2 (6 parámetros); el rasterizador no */
        else r63d_spi(&l, 6);
        modo_sh = 2; ps_posto = -1;
    }
    if (ps_posto != pi) {
        r63d_programas2(&l, mc_zona + RING_BYTES + SH_VS2, mc_zona + RING_BYTES + SH_PS2 + (uint32_t)pi * 4096u, R63D_PS2_GPRS,
                        (uint32_t)ps_cache[pi].pal * 4u);
        ps_posto = pi;
    }
    for (int u = 0; u < nu; u++) {
        const gpu3d_unidade_t *x = &e->u[u];
        if (!x->ativa) continue;
        const g3d_buffer_t *t = &b_tex[x->vaga];
        r63d_textura2(&l, u, (uint32_t)t->gpu, t->larg, t->alt, (int)(t->passo / 4), tex_fmt[x->vaga], x, (uint32_t)t->bytes);
    }
    r63d_estado2(&l, e, alvo_l, alvo_a, alvo_zbits == 24, familia == R63D_R600);
    ajustes_por(&l, G3D_AJ_DIBUJO);
    if (!emitir(&l)) return G3D_FALHA;
    sh_atual = -1;                 /* v1 tiene que volver a poner sus sombreadores */
    est_valido = false; e3_valido = false;
    return vertices2(v, n);
}

static int r3_desenhar2(const gpu3d_estado2_t *e, const gpu3d_vert2_t *v, int n, g3d_vala_t *vala)
{
    int r = desenhar2_int(e, v, n);
    if (r != G3D_OK) { if (r == G3D_FALHA) r3_detalle("al dibujar (v2)"); return r; }
    avisar();
    if (vala) {
        if (!submeter(vala)) return G3D_FALHA;
        pendente = false;
    }
    return G3D_OK;
}

/* Borrar con máscaras, tijera, un Z y un esténcil dados: un rectángulo con ese estado (el Z de 24 bits
 * lleva el esténcil de 8 desde la D426: DEPTH_8_24). */
static int r3_limpar2(const gpu3d_limpar2_t *lp)
{
    if (!cp_ligado || !alvo_l) return G3D_FALHA;
    gpu3d_estado2_t e;
    memset(&e, 0, sizeof e);
    e.versao = 2; e.func_z = G3D_CMP_SEMPRE; e.func_alfa = G3D_CMP_SEMPRE; e.op_logica = G3D_LOG_COPIA;
    e.mez_orig_rgb = e.mez_orig_alfa = G3D_F_UM; e.mez_dest_rgb = e.mez_dest_alfa = G3D_F_ZERO;
    e.mascara_cor = (lp->mascara & G3D_LIMPA_COR) ? lp->mascara_cor : 0;
    if (lp->mascara & G3D_LIMPA_Z) e.flags |= G3D2_Z_TESTE | G3D2_Z_ESCRITA;
    if ((lp->mascara & G3D_LIMPA_EST) && alvo_zbits == 24) {
        e.flags |= G3D2_ESTENCIL;
        e.est_func = G3D_CMP_SEMPRE; e.est_ref = (uint8_t)lp->estencil; e.est_mascara = 0xFF; e.est_escrita = lp->mascara_est;
        e.est_falha = e.est_zfalha = e.est_zpassa = G3D_EST_SUBSTITUIR;
    }
    if (!e.mascara_cor && !(e.flags & (G3D2_Z_TESTE | G3D2_ESTENCIL))) return G3D_OK;
    if (lp->tijera) {
        if (!lp->tl || !lp->ta) return G3D_OK;
        e.flags |= G3D2_TIJERA; e.tijera_x = lp->tx; e.tijera_y = lp->ty; e.tijera_l = lp->tl; e.tijera_a = lp->ta;
    }
    uint32_t x1 = r63d_float(alvo_l), y1 = r63d_float(alvo_a);
    uint32_t z = lp->z >= 0xFFFFFFu ? R63D_F_1 : g3d_float_de_fixo((int32_t)lp->z, 24);
    gpu3d_vert2_t q[6];
    memset(q, 0, sizeof q);
    const uint32_t xs[6] = { 0, x1, 0, 0, x1, x1 }, ys[6] = { 0, 0, y1, y1, 0, y1 };
    for (int i = 0; i < 6; i++) { q[i].x = xs[i]; q[i].y = ys[i]; q[i].z = z; q[i].rhw = R63D_F_1; q[i].cor = lp->cor; }
    return r3_desenhar2(&e, q, 6, NULL);
}

/* Texturas v2: ARGB como en v1; DXT1/3/5 con sus bloques tal cual, en 1D en mosaico (D426, DXT; paso de 8 bloques). */
static int r3_textura2(const gpu3d_textura2_t *t)
{
    if (!cp_ligado) return G3D_FALHA;
    if (t->vaga >= GPU3D_TEXTURAS) return G3D_NAO;
    if (!t->dados) return r3_textura((int)t->vaga, 0, 0, NULL);
    if (t->tipo != G3D_TEX_2D || t->nivel || t->face || t->larg < 1 || t->alt < 1 || t->larg > MAX_TEX || t->alt > MAX_TEX)
        return G3D_NAO;
    if (t->formato == G3D_FMT_ARGB8888) return r3_textura((int)t->vaga, (int)t->larg, (int)t->alt, (const uint32_t *)(uintptr_t)t->dados);
    if (t->formato > G3D_FMT_DXT5 || (int)t->vaga == dest_vaga) return G3D_NAO;
    g3d_buffer_t *b = &b_tex[t->vaga];
    if (r3_esperar(0, GPU3D_PRAZO_MS) != G3D_OK) return G3D_FALHA;
    soltar(b);
    tex_fmt[t->vaga] = G3D_FMT_ARGB8888;
    est_valido = false; e3_valido = false;
    uint32_t bs = t->formato == G3D_FMT_DXT1 ? 8u : 16u, pal = bs / 4u;
    int passo = r63d_passo_tex((int)t->larg, t->formato, alin_passo);
    uint32_t pb = (uint32_t)passo / 4u, nbx = (t->larg + 3u) / 4u, nby = (t->alt + 3u) / 4u;
    if (!criar_grande(b, G3D_BUF_TEXTURA, (uint64_t)pb * ((nby + 7u) & ~7u) * bs)) return G3D_NAO;
    b->passo = (uint32_t)passo * 4; b->larg = (int)t->larg; b->alt = (int)t->alt; b->bpp = t->formato == G3D_FMT_DXT1 ? 4 : 8;
    volatile uint8_t *d = ver_grande(b);
    if (!d) { soltar(b); return G3D_NAO; }
    /* D426, DXT: cada bloque a su sitio en el 1D en mosaico (los datos vienen por filas de bloques). */
    const uint32_t *o = (const uint32_t *)(uintptr_t)t->dados;
    for (uint32_t by = 0; by < nby; by++)
        for (uint32_t bx = 0; bx < nbx; bx++) {
            volatile uint32_t *dl = (volatile uint32_t *)(d + r63d_bc_desloc(bx, by, pb, bs));
            const uint32_t *ol = o + (by * nbx + bx) * pal;
            for (uint32_t k = 0; k < pal; k++) dl[k] = ol[k];
        }
    tex_fmt[t->vaga] = (uint8_t)t->formato;
    return G3D_OK;
}

/* Dibujar en una textura ARGB (su nivel 0) con un Z propio, o volver al principal. */
static int r3_destino(int vaga, int zbits, int estencil)
{
    if (!cp_ligado || !alvo_l) return G3D_FALHA;
    uint32_t b[64];
    r6_lote_t l;
    if (dest_vaga >= 0) {
        pendente = true;                       /* la valla vacía la caché de color sobre la textura */
        if (r3_esperar(0, GPU3D_PRAZO_MS) != G3D_OK) return G3D_FALHA;
        soltar(&b_zdest);
        dest_vaga = -1; cur_cor = &b_cor; cur_z = &b_z;
        alvo_l = princ_l; alvo_a = princ_a; alvo_passo = princ_passo; alvo_zbits = princ_zbits;
        r63d_lote(&l, b, 64);
        r63d_alvo(&l, (uint32_t)b_cor.gpu, (uint32_t)b_z.gpu, alvo_passo, (alvo_a + 63) & ~63, alvo_l, alvo_a, alvo_zbits,
                  R63D_PRECISA_SBU(familia));
        if (!emitir(&l)) return G3D_FALHA;
        est_valido = false; e3_valido = false;
        pendente = true;
    }
    if (vaga < 0) return G3D_OK;
    if (vaga >= GPU3D_TEXTURAS) return G3D_NAO;
    g3d_buffer_t *t = &b_tex[vaga];
    if (!t->usado || tex_fmt[vaga] != G3D_FMT_ARGB8888 || t->larg < 8 || t->alt < 8) return G3D_NAO;
    int passo = (int)(t->passo / 4), alt_buf = (t->alt + 63) & ~63;
    int zb = (zbits == 24 || estencil) ? 24 : 16;
    if (!criar_grande(&b_zdest, G3D_BUF_Z, (uint64_t)passo * (uint32_t)alt_buf * 4u)) return G3D_NAO;
    b_zdest.cpu = NULL;
    princ_l = alvo_l; princ_a = alvo_a; princ_passo = alvo_passo; princ_zbits = alvo_zbits;
    r63d_lote(&l, b, 64);
    r63d_alvo(&l, (uint32_t)t->gpu, (uint32_t)b_zdest.gpu, passo, alt_buf, t->larg, t->alt, zb, R63D_PRECISA_SBU(familia));
    if (!emitir(&l)) { soltar(&b_zdest); return G3D_FALHA; }
    alvo_l = t->larg; alvo_a = t->alt; alvo_passo = passo; alvo_zbits = zb;
    cur_cor = t; cur_z = &b_zdest; dest_vaga = vaga;
    est_valido = false; e3_valido = false;
    pendente = true;
    return G3D_OK;
}

/* ------------------------------------------------------------------ gpu3d v3 (D457, «v3 en R6xx/R7xx»)
 *
 * Búferes de vértices en la VRAM y sombreador de vértices generado (r63d_vs3_gerar, r600_3d_pacotes.c). Seguridad: lo
 * mismo que v2 (todo por la cola del CP, el contexto 3D, nada de pantalla, relojes ni MC); los búferes están dentro de
 * nuestra zona de la VRAM y la capa ya comprobó que cada dibujo cabe en lo subido; una lectura fuera del recurso
 * devuelve 0 en el hardware. Solo se anuncia en las tarjetas confirmadas (v3_confirmadas) o con «inicio 28ffc 2», y
 * aun así la autoprueba v3 de la capa tiene que dar los mismos píxeles que la CPU. */
static const struct { uint16_t fab, modelo; } v3_confirmadas[] = {
    { 0x1002, 0x9553 },   /* Mobility HD 4530/4570 (RV710) del HP dv7: autoprueba v3 BIEN (2026-11-22) */
};
static bool v3_confirmada(void)
{
    for (unsigned k = 0; k < sizeof v3_confirmadas / sizeof v3_confirmadas[0]; k++)
        if (v3_confirmadas[k].fab == disp3.fabricante && v3_confirmadas[k].modelo == disp3.modelo) return true;
    return false;
}

static void dep_ler(void)
{
    r3_dep = 0;
    for (int i = 0; i < gpu3d_ajustes.n; i++) if (gpu3d_ajustes.reg[i] == R3_REG_DEPURAR) r3_dep = gpu3d_ajustes.val[i];
    if (r3_dep) klog("[GPU3D] r600: banderas de gpu3d-r600.conf (D457): %x\n", r3_dep);
}

/* Que la GPU haya terminado todo lo mandado (sin esperar si ya está). */
static int r3_ocioso(void)
{
    if (!pendente && rd(SCRATCH_VALLA) == seq) return G3D_OK;
    pendente = true;
    return r3_esperar(0, GPU3D_PRAZO_MS);
}

/* Copia n bytes de la RAM a la dirección «gpu» de la zona: la parte visible por su ventana fija (sin caché); el resto,
 * o todo si se puede usar write-combining (PAT, como las texturas de la R5xx en D432), por la ventana móvil a trozos. */
static bool r3_escrever(uint32_t gpu, const uint8_t *o, uint32_t n)
{
    uint32_t off = gpu - mc_zona;
    if (gpu < mc_zona || off >= zona_bytes || n > zona_bytes - off) return false;
    bool wc = wc_ativo() && wc_via()[0] == 'P' && !(r3_dep & R3D_V3_SEM_WC);
    if (!wc && off + n <= zona_vis) {
        volatile uint8_t *d = zona + off;
        uint32_t i = 0;
        for (; i + 4 <= n; i += 4)
            *(volatile uint32_t *)(d + i) = (uint32_t)o[i] | ((uint32_t)o[i + 1] << 8) | ((uint32_t)o[i + 2] << 16) | ((uint32_t)o[i + 3] << 24);
        for (; i < n; i++) d[i] = o[i];
    } else {
        if (!jmovel) jmovel = janela_reservar(JMOVEL_BYTES);
        if (!jmovel) return false;
        for (uint32_t feito = 0; feito < n;) {
            uint32_t fis = tela.fb_fisico + zona_off + off + feito, dentro = fis & 0xFFFu;
            uint32_t m = n - feito > (4u << 20) ? (4u << 20) : n - feito;
            uint32_t pags = (dentro + m + 4095u) >> 12;
            for (uint32_t i = 0; i < pags; i++) {
                janela_soltar(jmovel + i * 4096u);
                janela_pagina(jmovel + i * 4096u, (fis & ~0xFFFu) + i * 4096u, wc ? PG_PWT : PG_PCD | PG_PWT);
            }
            volatile uint8_t *d = (volatile uint8_t *)(jmovel + dentro);
            if (wc) memcpy((void *)d, o + feito, m);
            else for (uint32_t i = 0; i < m; i++) d[i] = o[feito + i];
            feito += m;
        }
        if (wc) v3_wc++;
    }
    __asm__ volatile("lock; orl $0, (%%esp)" ::: "memory");   /* lo de write-combining, a la memoria antes de avisar al CP */
    return true;
}

static int r3_vbuf(int vaga, uint32_t bytes, uint32_t desloc, const void *dados, uint32_t n)
{
    if (!cp_ligado) return G3D_FALHA;
    if (vaga < 0 || vaga >= GPU3D_VBUFS) return G3D_NAO;
    if (!dados && !bytes && !vbm.tam[vaga]) return G3D_OK;
    /* un dibujo pendiente puede estar leyendo ese búfer (o el sitio que va a ocupar) */
    if (r3_ocioso() != G3D_OK) return G3D_FALHA;
    if (dados) {
        if (!b_vbm.usado || !vbm.tam[vaga] || desloc > vbm.tam[vaga] || n > vbm.tam[vaga] - desloc) return G3D_NAO;
        if (!r3_escrever((uint32_t)b_vbm.gpu + vbm.off[vaga] + desloc, (const uint8_t *)dados, n)) return G3D_NAO;
        v3_subidas++;
        v3_kb += n >> 10;
        vbm_sujo = true;
        return G3D_OK;
    }
    g3d_vbmonte_soltar(&vbm, vaga);
    if (!bytes) {
        if (b_vbm.usado && !vbm.n) soltar(&b_vbm);   /* nadie usa búferes: la memoria vuelve a las texturas */
        return G3D_OK;
    }
    if (!b_vbm.usado) {
        /* el montón: la mitad de lo libre de la parte grande de la zona (o de la visible si no hay), hasta 64 MB (lo otro
         * queda para las texturas; con poco, un mundo grande expulsa un búfer en cada dibujo: lo aprendido en la R5xx) */
        #define R3_VB_MAX (64u << 20)
        #define R3_VB_MIN (256u << 10)
        uint64_t livre = g3d_arena_livre(arena_g_ok ? &arena_g : &arena) / 2;
        uint32_t t = livre > R3_VB_MAX ? R3_VB_MAX : (uint32_t)livre & ~(R3_VB_MIN - 1u);
        for (; t >= R3_VB_MIN; t = (t / 2) & ~(R3_VB_MIN - 1u))
            if (criar_grande(&b_vbm, G3D_BUF_VERTICES, t)) break;
        if (t < R3_VB_MIN) return G3D_NAO;
        g3d_vbmonte_iniciar(&vbm, t);
        vbm_sujo = true;
        if (!v3_montes++) klog("[GPU3D] r600 v3: monton de vertices de %u KB en la VRAM (MC %x)\n", t >> 10, (uint32_t)b_vbm.gpu);
    }
    if (!g3d_vbmonte_criar(&vbm, vaga, bytes)) {
        if (!vbm.n) soltar(&b_vbm);
        return G3D_NAO;
    }
    return G3D_OK;
}

/* El sombreador de vértices v3 de un dibujo: de la caché o generado y escrito en su sitio. -1 no cabe; -2 la GPU no terminó. */
static int vs3_programa(const r63d_vs3_desc_t *ds)
{
    static r63d_vs3_t vs;
    static uint32_t relogio;
    relogio++;
    for (int i = 0; i < VS3_CACHE; i++)
        if (vs3_cache[i].usado && !memcmp(&vs3_cache[i].desc, ds, sizeof *ds)) { vs3_cache[i].uso = relogio; return i; }
    r63d_vs3_gerar(&vs, ds, r7);
    v3_vs_gerados++;
    if (vs.erro || (uint32_t)vs.n * 4u > VS3_BYTES) {
        if (v3_vs_gerados < 4) klog("[GPU3D] r600 v3: un sombreador de vertices no se pudo generar: ese dibujo va por la CPU\n");
        return -1;
    }
    int l = -1;
    for (int i = 0; i < VS3_CACHE && l < 0; i++) if (!vs3_cache[i].usado) l = i;
    if (l < 0) {
        l = 0;
        for (int i = 1; i < VS3_CACHE; i++) if (vs3_cache[i].uso < vs3_cache[l].uso) l = i;
        if (r3_ocioso() != G3D_OK) return -2;   /* un dibujo pendiente puede estar usándolo */
    }
    volatile uint32_t *sh = (volatile uint32_t *)(zona + RING_BYTES + SH_VS3 + (uint32_t)l * VS3_BYTES);
    for (int i = 0; i < vs.n; i++) sh[i] = vs.p[i];
    vs3_cache[l].usado = true; vs3_cache[l].uso = relogio; vs3_cache[l].desc = *ds; vs3_cache[l].pal = vs.n;
    if (vs3_posto == l) vs3_posto = -1;
    return l;
}

static int r3_desenhar3(const g3d_desenho3_t *d, g3d_vala_t *vala)
{
    if (!cp_ligado || !alvo_l) return G3D_FALHA;
    const gpu3d_estado2_t *e = d->e;
    const gpu3d_transf_t *t = d->t;
    bool idx = d->d.indices != GPU3D_SEM_INDICES;
    if ((t->flags & G3D3_LUZ) || ((t->flags & G3D3_NEBLINA) && t->neb_modo != G3D_NEB_LINEAR) || !b_vbm.usado || d->d.n < 1 ||
        d->d.n > GPU3D_MAX_VERT3 || d->d.modo > G3D_PRIM_ABANICO || (idx && (r3_dep & R3D_V3_SEM_IND))) return G3D_NAO;
    /* la parte de los píxeles: lo mismo que pide v2 */
    int nu = e->unidades > 4 ? 4 : e->unidades;
    for (int u = 0; u < nu; u++) {
        const gpu3d_unidade_t *x = &e->u[u];
        if (!x->ativa) continue;
        if (x->vaga >= GPU3D_TEXTURAS || (int)x->vaga == dest_vaga || x->tipo != G3D_TEX_2D || !b_tex[x->vaga].usado ||
            (x->filtro & (G3D_FIL_MIP_VECINO | G3D_FIL_MIP_LINEAR))) return G3D_NAO;
    }
    /* el programa de vértices */
    r63d_vs3_desc_t ds;
    memset(&ds, 0, sizeof ds);
    ds.pos_fmt = d->d.pos.formato;
    ds.cor_fmt = d->d.cor_a.formato;
    ds.unidades = (uint8_t)nu;
    for (int u = 0; u < nu; u++) {
        ds.tc_fmt[u] = d->d.tc[u].formato;
        if (t->flags & G3D3_TEX_MAT(u)) ds.tex_mat |= (uint8_t)(1u << u);
    }
    ds.neblina = !(t->flags & G3D3_NEBLINA) ? R63D_V3_NEB_NADA : (t->flags & G3D3_NEB_MEZCLADA) ? R63D_V3_NEB_MEZCLADA : R63D_V3_NEB_ESPEC;
    int vi = vs3_programa(&ds);
    if (vi == -2) return G3D_FALHA;
    if (vi < 0) return G3D_NAO;
    const r63d_vs3_desc_t *dc = &vs3_cache[vi].desc;
    /* los arrays: el primer vértice de cada uno (sin índices: el «primeiro»), con lo que queda de su hueco */
    int nb = 0;                 /* qué lee cada recurso: el orden de r63d_vs3_gerar (posición, color, juegos) */
    uint8_t atr[R63D_VS3_BUFS];
    atr[nb++] = 0;
    if (dc->cor_fmt != G3D_AT_NADA) atr[nb++] = 1;
    for (int u = 0; u < nu; u++) if (dc->tc_fmt[u] != G3D_AT_NADA) atr[nb++] = (uint8_t)(2 + u);
    r63d_vbuf_t vb[R63D_VS3_BUFS];
    for (int k = 0; k < nb; k++) {
        const gpu3d_atrib_t *a = atr[k] == 0 ? &d->d.pos : atr[k] == 1 ? &d->d.cor_a : &d->d.tc[atr[k] - 2];
        if (a->vaga >= GPU3D_VBUFS || !vbm.tam[a->vaga] || (a->passo & 3) || (a->desloc & 3)) return G3D_NAO;
        uint64_t des = (uint64_t)a->desloc + (idx ? 0 : (uint64_t)d->d.primeiro * a->passo);
        if (des >= vbm.tam[a->vaga]) return G3D_NAO;
        vb[k].mc = (uint32_t)b_vbm.gpu + vbm.off[a->vaga] + (uint32_t)des;
        vb[k].bytes = vbm.tam[a->vaga] - (uint32_t)des;
        vb[k].passo = a->passo;
    }
    uint32_t mc_idx = 0;
    if (idx) {
        uint32_t iv = d->d.indices;
        if (iv >= GPU3D_VBUFS || !vbm.tam[iv] || (uint64_t)d->d.primeiro * 2u + (uint64_t)d->d.n * 2u > vbm.tam[iv]) return G3D_NAO;
        mc_idx = (uint32_t)b_vbm.gpu + vbm.off[iv] + d->d.primeiro * 2u;
    }
    int pi = ps_programa(e);
    if (pi == -2) return G3D_FALHA;
    if (pi < 0) return G3D_NAO;
    static uint32_t b[640 + 3 * G3D_AJUSTES_MAX];
    r6_lote_t l;
    r63d_lote(&l, b, (int)(sizeof b / sizeof b[0]));
    int modo_antes = modo_sh;
    if (modo_sh != 3) {
        if (modo_sh != 2) r63d_spi(&l, 6);     /* las salidas de v3 son las de v2 */
        r63d_v3_entrar(&l);
        modo_sh = 3; ps_posto = -1; vs3_posto = -1; e3_valido = false; t3_valido = false;
        sh_atual = -1; est_valido = false;     /* v1 tiene que volver a poner los suyos */
    }
    if (ps_posto != pi || vs3_posto != vi) {
        r63d_programas3(&l, mc_zona + RING_BYTES + SH_VS3 + (uint32_t)vi * VS3_BYTES, (uint32_t)vs3_cache[vi].pal * 4u,
                        mc_zona + RING_BYTES + SH_PS2 + (uint32_t)pi * 4096u, R63D_PS2_GPRS, (uint32_t)ps_cache[pi].pal * 4u);
        ps_posto = pi; vs3_posto = vi;
    }
    if (!e3_valido || memcmp(e, &e3_posto, sizeof *e)) {
        for (int u = 0; u < nu; u++) {
            const gpu3d_unidade_t *x = &e->u[u];
            if (!x->ativa) continue;
            const g3d_buffer_t *tx = &b_tex[x->vaga];
            r63d_textura2(&l, u, (uint32_t)tx->gpu, tx->larg, tx->alt, (int)(tx->passo / 4), tex_fmt[x->vaga], x, (uint32_t)tx->bytes);
        }
        r63d_estado2(&l, e, alvo_l, alvo_a, alvo_zbits == 24, familia == R63D_R600);
        ajustes_por(&l, G3D_AJ_DIBUJO);
        e3_posto = *e; e3_valido = true;
    }
    if (!t3_valido || memcmp(t, &t3_posto, sizeof *t)) {
        static uint32_t c[R63D_VS3_NCONST * 4];
        r63d_vs3_constantes(c, t);
        r63d_vs3_consts_emitir(&l, c);
        r63d_v3_vista(&l, t);
        r63d_reg(&l, 0x28814, r63d_v3_descarte(t->descarte, (r3_dep & R3D_V3_DESC_INV) != 0));   /* PA_SU_SC_MODE_CNTL */
        t3_posto = *t; t3_valido = true;
        v3_transf++;
    }
    if (vbm_sujo) {   /* lo subido desde el último dibujo: fuera de la caché de vértices (o de texturas) */
        r63d_sync_vertices(&l, (uint32_t)b_vbm.gpu, (uint32_t)b_vbm.bytes, R63D_SEM_CACHE_VTX(familia));
        vbm_sujo = false;
    }
    r63d_desenhar3(&l, vb, nb, (int)d->d.modo, d->d.n, idx, mc_idx);
    if (l.cheio) {   /* nada llegó a la cola: el motor sigue en el modo de antes */
        e3_valido = t3_valido = false; ps_posto = -1; vs3_posto = -1;
        if (modo_antes != 3) modo_sh = modo_antes;
        return G3D_NAO;
    }
    if (!emitir(&l)) { e3_valido = t3_valido = false; ps_posto = -1; r3_detalle("al dibujar (v3)"); return G3D_FALHA; }
    pendente = true;
    if (!v3_dibujos++)
        klog("[GPU3D] r600 v3: primer dibujo desde buferes: %u %s, %d arrays, sombreador de vertices de %d palabras; %d palabras en la cola\n",
             d->d.n, idx ? "indices" : "vertices", nb, vs3_cache[vi].pal, l.n);
    avisar();
    if (r3_dep & R3D_V3_SYNC) {
        int r = r3_esperar(0, 500);
        klog("[GPU3D] r600 v3 sinc: dibujo de %u %s (modo %u, %d arrays): %s\n", d->d.n, idx ? "indices" : "vertices", d->d.modo, nb,
             r == G3D_OK ? "bien" : "CUELGA");
        if (r != G3D_OK) { r3_detalle("al dibujar (v3)"); return G3D_FALHA; }
    }
    if (vala) {
        if (!submeter(vala)) return G3D_FALHA;
        pendente = false;
    }
    return G3D_OK;
}

/* ------------------------------------------------------------------ D432: presentar en la pantalla
 *
 * La GPU copia (escalando) el destino del 3D, o una textura, a la superficie que muestra el CRTC, que
 * está en la misma VRAM (base.mc_tela) y es ARGB8888 lineal: sirve de destino de color con su paso y
 * el modo de superficie que le dejó la BIOS (D1GRPH_CONTROL, bits 20-23: 0 lineal general, 1 lineal
 * alineada; en mosaico no se presenta). Pasos:
 *   1. una valla: lo dibujado, a la VRAM (se va a leer como textura);
 *   2. destino = la pantalla, sin Z (DB_DEPTH_INFO sin formato); bandas negras si se piden y el
 *      rectángulo con la fuente como textura, con los sombreadores de v1;
 *   3. otra valla, que vacía la caché de color sobre la pantalla, esperada con plazo;
 *   4. el destino del programa vuelve a ponerse (queda en la cola para el próximo dibujo).
 * De la pantalla solo se LEEN registros (superficie, paso, modo y, con G3D_APRES_VSYNC, el bit de
 * retrazado): no se escribe ninguno. SIN PROBAR EN HARDWARE (el dv7 estaba apagado al escribirlo). */
#define D1CRTC_STATUS    0x609C            /* AVIVO: bit 0 = en el retrazado vertical */
#define APRES_VBLANK_MS  20

static bool r3_tela_serve(uint32_t *modo)
{
    uint32_t o = base.controlador ? 0x800u : 0, passo = tela.pitch / 4u;
    *modo = 0;
    if (!cp_ligado || !sondado) { apres_porque = "el CP no esta en marcha"; return false; }
    if (base.mc_alto) { apres_porque = "la pantalla esta por encima de 4 GB en el MC"; return false; }
    if (tela.bytes_px != 4 || (tela.pitch & 31u) || tela.larg < 8 || tela.alt < 8 || tela.larg > 8192 || tela.alt > 8192) {
        apres_porque = "la pantalla no es de 32 bits con el paso multiplo de 8 pixeles"; return false;
    }
    if ((int)video_largura() != tela.larg || (int)video_altura() != tela.alt || video_pitch() != tela.pitch ||
        !video_formato_direto() || video_bpp() != 32) { apres_porque = "el modo de video cambio desde el encendido"; return false; }
    uint32_t ctl = rd(D1GRPH_CONTROL + o), sup = rd(D1GRPH_SUPERFICIE + o), pit = rd(D1GRPH_PITCH + o);
    if (sup != base.mc_tela || pit != passo) { apres_porque = "la superficie o el paso del CRTC cambiaron desde el encendido"; return false; }
    uint32_t m = (ctl >> 20) & 0xFu;
    if (m > 1) { apres_porque = "la pantalla esta en mosaico (no lineal)"; return false; }
    /* D402: en el dv7 las superficies con el paso sin alinear al grupo de GB_TILING_CONFIG salían mal. */
    if (passo % (uint32_t)alin_passo) { apres_porque = "el paso de la pantalla no esta alineado al grupo de la tarjeta"; return false; }
    *modo = m;
    apres_porque = "se puede";
    return true;
}

/* Sombreadores de v1, la textura (o ninguna) y el estado de la copia: sin Z ni mezcla, tijera a toda la pantalla. */
static bool apres_estado(const g3d_buffer_t *t, bool linear, int tl, int ta)
{
    uint32_t b[128];
    r6_lote_t l;
    r63d_lote(&l, b, 128);
    if (modo_sh != 1) {
        if (modo_sh == 3) r63d_v3_sair(&l);   /* D457 */
        r63d_spi(&l, 3);
        r63d_estado_v1(&l);
        modo_sh = 1; sh_atual = -1; ps_posto = -1;
    }
    int quer = t ? 1 : 0;
    if (quer != sh_atual) {
        r63d_programas(&l, mc_zona + RING_BYTES, mc_zona + RING_BYTES + (quer ? 0x200u : 0x100u), quer);
        sh_atual = quer;
    }
    if (t) r63d_textura(&l, (uint32_t)t->gpu, t->larg, t->alt, (int)(t->passo / 4), true, true, linear);
    r63d_estado(&l, false, false, 7, false, 1, 0, true, 0, 0, tl, ta, familia == R63D_R600);
    est_valido = false; e3_valido = false;
    return emitir(&l);
}

static int r3_apresentar(const g3d_apres_t *p)
{
    uint32_t modo;
    bool serve = r3_tela_serve(&modo);
    if (!p) {
        klog("[GPU3D] r600 presentar (D432): %s (pantalla MC %x, paso %u, %dx%d, modo de superficie %u, alineacion %d)\n", apres_porque,
             base.mc_tela, tela.pitch / 4u, tela.larg, tela.alt, modo, alin_passo);
        return serve ? G3D_OK : G3D_NAO;
    }
    if (!cp_ligado) return G3D_FALHA;
    if (!serve) { klog("[GPU3D] r600 presentar (D432): ya no se puede: %s\n", apres_porque); return G3D_NAO; }
    if (!alvo_l || dest_vaga >= 0) return G3D_NAO;
    const g3d_buffer_t *src = &b_cor;
    if (p->vaga >= 0) {
        if (p->vaga >= GPU3D_TEXTURAS || !b_tex[p->vaga].usado || tex_fmt[p->vaga] != G3D_FMT_ARGB8888) return G3D_NAO;
        src = &b_tex[p->vaga];
    }
    if (src->larg < 1 || src->alt < 1 || src->larg > MAX_TEX || src->alt > MAX_TEX) return G3D_NAO;
    /* 1. Lo dibujado hasta ahora, a la VRAM. */
    pendente = true;
    if (r3_esperar(0, GPU3D_PRAZO_MS) != G3D_OK) return G3D_FALHA;
    /* 2. La pantalla como destino y la copia. */
    int tl = tela.larg, ta = tela.alt, passo = (int)(tela.pitch / 4u), alt_buf = (ta + 7) & ~7;
    uint32_t b[64];
    r6_lote_t l;
    r63d_lote(&l, b, 64);
    r63d_alvo_modo(&l, base.mc_tela, 0, passo, alt_buf, tl, ta, 0, R63D_PRECISA_SBU(familia), modo);
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
    uint32_t o = base.controlador ? 0x800u : 0;
    if (ok && (p->flags & G3D_APRES_VSYNC) && !(rd(D1CRTC_STATUS + o) & 1u)) {
        prazo_t pz = prazo_ms(APRES_VBLANK_MS);
        while (!(rd(D1CRTC_STATUS + o) & 1u))
            if (prazo_vencido(&pz)) { apres_vblank_fora++; break; }
    }
    /* 3. La valla sobre la pantalla. */
    if (ok) {
        r63d_lote(&l, b, 64);
        seq++;
        if (!seq) seq = 1;
        r63d_valla(&l, base.mc_tela, (uint32_t)passo * (uint32_t)ta * 4u, 0, 0, SCRATCH_VALLA, seq);
        ok = emitir(&l);
        if (ok) { avisar(); ok = valla_esperar(seq, GPU3D_PRAZO_MS); }
    }
    vb_usado = 0;
    if (!ok) { r3_detalle("al presentar en la pantalla"); return G3D_FALHA; }
    /* 4. El destino del programa, para lo que venga. */
    r63d_lote(&l, b, 64);
    r63d_alvo(&l, (uint32_t)b_cor.gpu, (uint32_t)b_z.gpu, alvo_passo, (alvo_a + 63) & ~63, alvo_l, alvo_a, alvo_zbits,
              R63D_PRECISA_SBU(familia));
    if (!emitir(&l)) return G3D_FALHA;
    est_valido = false; e3_valido = false;
    pendente = true;
    if (!apres_n++)
        klog("[GPU3D] r600 presentar (D432): primera copia a la pantalla: fuente MC %x paso %u (%dx%d) -> pantalla MC %x paso %d "
             "(%dx%d, modo %u), destino %dx%d+%d+%d%s\n", (uint32_t)src->gpu, src->passo / 4, src->larg, src->alt, base.mc_tela, passo,
             tl, ta, modo, p->dl, p->da, p->dx, p->dy, p->flags & G3D_APRES_VSYNC ? ", con espera al retrazado" : "");
    return G3D_OK;
}

/* Lo que se anuncia: lo que este driver sabe hacer en las R6xx/R7xx, y de eso solo lo que pasa las pruebas
 * (g3d_v2_provas.c). Sin mipmaps, cubos ni coordenadas proyectivas todavía. */
static void r3_capacidades(gpu3d_capas_t *c)
{
    caps_v2 = 0;
    if (!cp_ligado) return;
    uint32_t cand = G3D_CAP_V2 | G3D_CAP_COMBINAR | G3D_CAP_DOT3 | G3D_CAP_ALFA_TESTE | G3D_CAP_ESTENCIL | G3D_CAP_TIJERA |
                    G3D_CAP_MASCARAS | G3D_CAP_Z_FUNCS | G3D_CAP_MEZCLA_EXT | G3D_CAP_MEZCLA_SEP | G3D_CAP_MEZCLA_EQ |
                    G3D_CAP_LOGICA | G3D_CAP_NEBLINA | G3D_CAP_TEX_ESPELHO | G3D_CAP_TEX_NPOT | G3D_CAP_TEX_ANISO |
                    G3D_CAP_LIMPAR2 | G3D_CAP_DESTINO_TEX;
    uint32_t un = 4, fmts = 0xFu;
    static const g3d_v2_ops_t ops = { r3_alvo, r3_limpar, r3_textura, r3_textura2, r3_desenhar2, r3_limpar2, r3_ler, r3_destino };
    uint32_t ok = g3d_v2_provar(&ops, "r600", cand, &un, &fmts);
    caps_v2 = ok;
    c->bits = ok;
    c->unidades = ok ? un : 1;
    c->formatos = fmts;
    c->max_lado = MAX_TEX;
    c->max_aniso = (ok & G3D_CAP_TEX_ANISO) ? 16 : 1;
    c->zbits = 3;
    c->estencil_bits = (ok & G3D_CAP_ESTENCIL) ? 8 : 0;
    /* D457: v3 en las confirmadas (o si se pide); la capa lo pone a prueba contra la CPU (autoprueba v3) antes de usarlo */
    if ((ok & G3D_CAP_V2) && !(r3_dep & R3D_SEM_V3) && (v3_confirmada() || (r3_dep & R3D_COM_V3))) {
        c->bits3 = G3D_C3_V3 | G3D_C3_NEBLINA | G3D_C3_TEX_MAT | G3D_C3_TIRAS | G3D_C3_COR_FLOAT;
        if (!(r3_dep & R3D_V3_SEM_IND)) c->bits3 |= G3D_C3_INDICES;
    } else if (ok & G3D_CAP_V2)
        klog("[GPU3D] r600 v3 (D457): sin anunciar (%s)\n", (r3_dep & R3D_SEM_V3) ? "lo quita gpu3d-r600.conf" :
             "tarjeta sin confirmar: \"inicio 28ffc 2\" en /sistema/conf/gpu3d-r600.conf lo pide");
}

const gpu3d_driver_t gpu3d_r600 = {
    "r600", r3_iniciar, r3_alvo, r3_limpar, r3_textura, r3_desenhar, r3_esperar, r3_ler, r3_desligar, r3_detalle, r3_memoria,
    "r600", r3_faixas_ajustes, (int)(sizeof r3_faixas_ajustes / sizeof r3_faixas_ajustes[0]), r3_vistas,
    r3_capacidades, r3_desenhar2, r3_textura2, r3_limpar2, r3_destino,
    r3_apresentar,
    r3_vbuf, r3_desenhar3,
};
