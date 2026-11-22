// SPDX-License-Identifier: GPL-2.0-or-later
/* gpu3d_intel9.c - 3D por la GPU en Intel Gen9 (Skylake, Kaby Lake, Coffee/Whiskey/Comet Lake;
 * la UHD 620 8086:3ea0) por el motor de render (RCS) con execlists (D438, v8.5.5.3).
 *
 * Probado en el Dell con UHD 620 (D438: autoprueba 8/8). Arranca APAGADO (como las R7xx): solo
 * «gpu3d probar» / «gpu3d encender» (o --gpu3d=intel al grabar) lo ponen en marcha, y la capa
 * común solo lo deja activo si las ocho pruebas de su autoprueba dan bien.
 *
 * El envío es el de D434 (acel_intel_exec.c), con el RCS (0x2000) en vez del BCS: un contexto
 * lógico (la imagen de i915 para el RCS, gen9_rcs_offsets, con el inhibidor de la carga en cada
 * tanda), una página de estado con el CSB y la marca, un anillo y una PPGTT de 4 niveles que
 * replica la GGTT solo en la zona del 3D. Lotes (D448): cada tanda lleva el estado fijo una vez y
 * tantos dibujos como quepan (de cada uno, solo el estado que cambia); se manda al leer el cuadro,
 * al esperar o cuando se llena, y mientras la GPU la ejecuta la CPU monta la siguiente en la otra
 * mitad del anillo. Solo se espera (con plazo) a la marca y a que el CSB diga que el motor quedó
 * libre antes de mandar la siguiente, y al leer.
 *
 * Memoria: una zona en la memoria robada detrás de la pantalla, de la zona de prueba y de la
 * zona del 2D (D434), de 4 a 16 MB: estado, sombreadores, página de estado, anillo, contexto,
 * PPGTT, vértices y una arena con el Z (mosaico Y), las texturas y, si no cabe en RAM, el destino.
 * Si el GTT no mapea la zona, se escriben sus entradas solo dentro de la memoria robada y fuera de
 * la reservada (como D434). La CPU la ve por la apertura: en WC lo que solo escribe, sin caché lo
 * que lee. D448: el destino de color va en RAM del sistema con caché (WB en la LLC, coherente con
 * la CPU), mapeada solo en la PPGTT, para que leer el cuadro no pase por la apertura.
 *
 * Seguridad: antes de escribir nada se lee todo (modelo exacto, BAR, pantalla, planos, memoria
 * robada, GTT, RCS sin uso). Todas las esperas tienen plazo; si una tanda no termina, STOP_RING,
 * los registros del motor al log y la capa común apaga el 3D (todo sigue por software); tras un
 * cuelgue la RAM del destino no se devuelve. Nunca se reinicia la GPU ni se toca la pantalla. Log
 * por etapas: «[GPU3D] intel9: etapa N»; medidas por cuadro cada 600 cuadros.
 * Más detalle y qué probar: docs/gpu3d.md («Intel Gen9-12 (3D)») y docs/decisoes.md (D438). */
#include "gpu3d.h"
#include "intel9_3d.h"
#include "acel_intel_logica.h"
#include "console.h"
#include "paginacao.h"
#include "prazo.h"
#include "pci.h"
#include "texto.h"
#include "memoria.h"
#include "fpu.h"
#include "sse2.h"
#include "info_hardware.h"
#include "wc.h"
#include "g3d_v2_provas.h"
#include "processo.h"
#include "relogio.h"
#include "cpu.h"
#include "intel_rps.h"

#define RCS            0x2000u
/* Disposición de la zona (desde su principio; dirección gráfica = zona_gfx + desplazamiento). D448:
 * lo que la CPU solo escribe (estado, anillo, sombreadores, vértices, texturas) va en ventanas WC si la
 * PAT lo permite; lo que la CPU lee (página de estado, contexto) y la PPGTT, sin caché (UC). Nunca
 * la misma página con los dos tipos (el manual de Intel no lo admite). */
#define Z_ESTADO       0x00000u          /* dos huecos de estado de 32 KB, uno por lote (WC) */
#define EST_HUECO      0x8000u
#define Z_HWS          0x10000u          /* página de estado: CSB en la palabra 0x10, marca en la 0x80 (UC) */
#define Z_SCRATCH      0x11000u          /* 8 bytes para los PIPE_CONTROL de los workarounds (UC) */
#define Z_CTX          0x12000u          /* (UC) */
#define CTX_PAGINAS    24u               /* 22 de i915 para Gen9 y dos de reserva */
#define Z_PPGTT        0x2A000u          /* (UC) */
#define PT_MAX         10u               /* tablas de 2 MB: 16 MB de zona sin alinear a 2 MB tocan 9 (el Dell UHD 620) */
#define Z_UC_FIM       0x3A000u          /* hasta aquí, sin caché */
#define Z_ANEL         0x3A000u          /* dos mitades de 16 KB, una por lote (WC) */
#define ANEL_BYTES     0x8000u
#define ANEL_METADE    (ANEL_BYTES / 2u)
#define Z_K_COR        0x42000u          /* sombreador de color (WC) */
#define Z_K_TEX        0x42800u          /* sombreador de textura */
#define Z_K2           0x43000u          /* D453: caché de sombreadores de v2, K2_N de 4 KB */
#define K2_N           32u
#define Z_VB           0x63000u          /* dos huecos de vértices, uno por lote (WC) */
#define VB_BYTES       0x42000u          /* 3072 vértices de v2 (88 bytes) */
#define Z_KV           0xE8000u          /* D457: caché de sombreadores de vértices de v3, KV_N de 4 KB (WC) */
#define KV_N           8u
#define Z_ARENA        0xF0000u          /* destino, Z, esténcil y texturas (WC) */
_Static_assert(Z_VB + 2u * VB_BYTES <= Z_KV && Z_KV + KV_N * I9_KERNELV_BYTES <= Z_ARENA, "la disposicion de la zona 3D");
#define ZONA_MIN       (4u << 20)
#define ZONA_MAX       (16u << 20)
#define APERTURA_MAX   (128u << 20)
#define MAX_LADO       2048
#define CSB_DWORD      (0x40u / 4u)
#define MARCA_DWORD    0x80u
#define CSB_VAZIA      0xFFFFFFFFu
#define PRAZO_INICIO   250u
#define PRAZO_CSB_MS   2u
/* D448: el destino de color en RAM del sistema con caché (coherente con la LLC), mapeado en la PPGTT
 * en esta dirección gráfica (fuera de la zona; la PPGTT es del contexto: no toca la GGTT ni la pantalla).
 * Así la CPU lee el cuadro de su caché en vez de por la apertura sin caché. */
#define RAM_GFX        0x38000000u
#define RAM_PT_MAX     8u                /* 8 tablas de 2 MB: destinos de hasta 16 MB (D457: 1920x1080 a pantalla completa cabe) */
/* D457: el montón de los búferes de vértices e índices de v3, también en RAM con caché (la CPU sube con memcpy y la GPU
 * lee por la LLC, coherente), en otra dirección gráfica de la PPGTT. */
#define VB_GFX         0x3C000000u
#define VB_PT_MAX      8u                /* hasta 16 MB */
#define PAT_HI         0x40E4u           /* GEN8_PRIVATE_PAT_HI: índices 4-7 */
#define PAT_WB_LLC     0x0Bu             /* GEN8_PPAT_WB | GEN8_PPAT_LLCELLC: el índice 4 de i915 (PPAT_CACHED = _PAGE_PAT) */
#define PTE_PAT4       (1u | 2u | (1u << 7))   /* presente, escritura, PAT = 1 (índice 4) */
#define MOCS_RAM       1u                /* entrada 1 de MOCS del RCS: la caché la dice la PTE */
#define FW_RENDER      0xA278u           /* FORCEWAKE_RENDER_GEN9 */
#define FW_RENDER_ACK  0x0D84u
#define FW_GT          0xA188u           /* FORCEWAKE_GT_GEN9 (el 2D usa el bit 0; el 3D el 1) */
#define FW_GT_ACK      0x130044u
#define FW_BIT_GT      2u
#define MASC_ON(b)     (((uint32_t)(b) << 16) | (uint32_t)(b))
#define MASC_OFF(b)    ((uint32_t)(b) << 16)
/* Bits de /sistema/conf/gpu3d-intel9.conf, línea «inicio fff0 N» (no es un registro: no se escribe). */
#define AJ_FLAGS       0xFFF0u
#define FL_SEM_WA_MOTOR 0x01u            /* no escribir los workarounds del RCS por MMIO */
#define FL_SEM_WA_GT    0x02u            /* ni los del GT */
#define FL_SEM_WA_CTX   0x04u            /* ni los de contexto por LRI */
#define FL_RESTAURAR    0x08u            /* no poner el inhibidor de la carga en cada tanda */
#define FL_LOG_TANDAS   0x10u            /* las primeras 16 tandas, cada una al log */
#define FL_SEM_MOCS     0x20u            /* no programar las entradas 0 y 1 de MOCS del RCS */
#define FL_SEM_LOTES    0x40u            /* D448: un lote por dibujo (como D438, para comparar o diagnosticar) */
#define FL_SEM_WC       0x80u            /* D448: toda la zona sin caché (UC), como D438 */
#define FL_COR_ZONA     0x100u           /* D448: el destino en la zona (por la apertura), como D438 */
#define FL_CLFLUSH      0x200u           /* D448: vaciar de la caché de la CPU el destino en RAM antes de leerlo */
#define FL_SEM_GUARDAR  0x400u           /* D448 (prueba): tampoco guardar el contexto al acabar cada tanda */
#define FL_PERFIL       0x800u           /* D448: marcas de tiempo de la GPU en cada tanda (cuánto tarda cada parte) */
#define FL_SEM_FREQ     0x1000u          /* D448: no pedir al PCU la frecuencia máxima (RP0) al trabajar */
#define FL_RPCS         0x2000u          /* D448 (prueba): R_PWR_CLK_STATE en el contexto (8 EU por subcorte, como i915) */
#define FL_L3_WB        0x4000u          /* D448 (prueba): el destino en RAM también con caché en la L3 (MOCS 1) */
#define FL_SEM_L3       0x8000u          /* D448 (prueba): L3CNTLREG solo en el primer lote */
#define FL_SEM_V2       0x10000u         /* D453: no anunciar gpu3d v2 (SaguiGL hace por software lo que no cabe en v1) */
#define FL_MIP_LINEAR   0x20000u         /* D453 (prueba): texturas con mipmaps lineales (si no, en mosaico Y) */
#define FL_MIP_SESGO    0x40000u         /* D453 (prueba): LOD de las texturas con mipmaps un nivel más bajo */
#define FL_SEM_V3       0x100000u        /* D457: no anunciar gpu3d v3 (SaguiGL transforma con la CPU, como en v2) */
#define FL_V3_DESC_INV  0x200000u        /* D457 (diagnóstico): el descarte de caras de v3 con el otro sentido */
#define FL_V3_SINC      0x400000u        /* D457 (diagnóstico): cada dibujo v3 en su tanda, esperada (las 8 primeras, al log) */
#define FL_MIP_PADRAO   0x80000u         /* D453 (diagnóstico): las texturas con mipmaps llevan en cada texel su (x, y) de la
                                          * disposición (0xXXYY5A) en vez de sus datos: el color leído dice dónde leyó la GPU */
#define PERFIL_DW      0x100u            /* las 5 marcas de tiempo, en la página de estado (palabra 0x100) */
#define RCS_TIMESTAMP  (RCS + 0x358u)
#define RPNSWREQ       0xA008u           /* GEN6_RPNSWREQ: Gen9, la frecuencia pedida en los bits 31:23 (x 50/3 MHz) */
#define RPSTAT1        0xA01Cu           /* GEN6_RPSTAT1: CAGF en los bits 31:23 (Gen9) */
#define RP_CONTROL     0xA024u
#define RP_STATE_CAP   0x145998u         /* espejo del MCHBAR: RP0 7:0, RP1 15:8, RPn 23:16 (x 50 MHz) */

static volatile uint8_t *mmio;
static uint32_t mmio_fis;
static volatile uint32_t *gtt;
static uint32_t gtt_fis, gtt_tam;
/* Ventanas de la CPU sobre la zona, por la apertura (D448): [0, Z_HWS) y [Z_UC_FIM, fin) en WC
 * (o UC sin PAT o con la bandera 80), [Z_HWS, Z_UC_FIM) siempre UC. zw() elige. */
static volatile uint8_t *zona_wc0, *zona_uc, *zona_wc1;
static uint32_t zona_fis_cpu, zona_map, zona_tipo;
static uint32_t zona_gfx, zona_bytes, n_pt;
static uint64_t zona_fis;                /* física (memoria robada) del principio de la zona */
static int gen, plat, etapa = -1;
static bool ligado, acordado, motor_nosso, avisou_marca, avisou_mmio, pendente, com_wc;
static uint32_t csb_lido, envios, marca, flags;
static const i9_wa_t *was;
static int n_was;
static uint32_t aj_reg[G3D_AJUSTES_MAX], aj_val[G3D_AJUSTES_MAX];
static int n_aj;
static g3d_arena_t arena;
static g3d_buffer_t b_cor, b_z, b_est, b_tex[GPU3D_TEXTURAS];
/* D453: la caché de sombreadores de v2: la combinación, el último lote que lo usa y su antigüedad. */
typedef struct { bool valido; i9_ps2_t chave; uint32_t uso, lru; } k2_t;
static k2_t k2[K2_N];
static uint32_t k2_relogio, n_k2_gerados;
/* D457: la caché de sombreadores de vértices de v3 (como la de v2) y el montón de los búferes residentes. */
typedef struct { bool valido; i9_vs_t chave; uint32_t uso, lru; } kv_t;
static kv_t kv[KV_N];
static uint32_t kv_relogio, n_kv_gerados;
static g3d_vbmonte_t vbm;
static uint32_t vb_ram, vb_ram_pag, vb_pt[VB_PT_MAX];
static uint32_t v3_desenhos, v3_subidas, v3_montes, v3_sem;
static uint8_t tex_niveis[GPU3D_TEXTURAS];   /* D453: niveles de mipmap de cada hueco */
static bool tex_y[GPU3D_TEXTURAS];           /* D453: hueco en mosaico Y */
static int alvo_l, alvo_a, alvo_zbits;
static uint32_t alvo_passo;
/* El lote que se está montando (D448): en el hueco «k_lote» (anillo, estado y vértices); el otro
 * puede estar en la GPU (pendente, con la marca «marca»). */
static i9_lote_t lote;
static bool lote_aberto;
static uint32_t k_lote, vb_usado;
/* Destino en RAM con caché (D448): páginas seguidas de la RAM baja (puntero = física), sus tablas
 * de la PPGTT (también en RAM) y si hay que invalidar el TLB de la GPU en el próximo lote. */
static uint32_t ram_cor, ram_cor_pag, ram_pt[RAM_PT_MAX];
static bool cor_ram, tlb_pendente, pat_tocado;
static uint32_t pat_hi_antes;
/* Medidas (D448): ciclos del TSC y cuentas desde el encendido. */
static uint64_t t_valla, t_copia, t_espera_lote, t_montar;
static uint32_t n_ler, n_lotes, n_desenhos, n_mudancas, n_cheios, eir_antes, falta_antes;
static uint32_t ult_n_ler, ult_n_lotes, ult_n_desenhos;
static uint64_t ult_t_valla, ult_t_copia, ult_t_montar, ult_t_espera;
static bool travado;                     /* una tanda no terminó: no se vuelve a encender hasta reiniciar */
/* Perfil (bandera 800): marcas de tiempo de la GPU por tanda; tics del reloj del CS por ms. */
static uint32_t ts_por_ms, n_perfil;
static uint64_t perfil_soma[4];
static bool l3_posto;
/* Frecuencia (D448): sin i915 nadie pide reloj y la GPU se queda en 100 MHz (11 veces menos que RP0 en
 * la UHD 620). Al mandar trabajo se pide RP0 (GEN6_RPNSWREQ, como gen6_rps_set de i915); la tarea
 * «gpu3d-intel9» vuelve a lo que había pedido el firmware tras 1 s sin trabajo, y al apagar. El PCU
 * sigue mandando: no pasa de sus límites de potencia y temperatura. */
static bool freq_pode;                   /* D457: el gobernador común con el 2D (intel_rps.c) */
static pci_disp_t disp9;
static char mot_ultimo[96];

static const char *const nomes_etapa[7] = {
    "sonda de solo lectura", "zona y GTT", "forcewake", "RCS sin uso", "workarounds y execlists",
    "contexto y tanda vacia", "estado 3D sin dibujo",
};

static inline uint32_t rd(uint32_t r)             { return *(volatile uint32_t *)(mmio + r); }
static inline void     wr(uint32_t r, uint32_t v) { *(volatile uint32_t *)(mmio + r) = v; }
static inline volatile uint32_t *zw(uint32_t off)
{
    if (off < Z_HWS) return (volatile uint32_t *)(zona_wc0 + off);
    if (off < Z_UC_FIM) return (volatile uint32_t *)(zona_uc + (off - Z_HWS));
    return (volatile uint32_t *)(zona_wc1 + (off - Z_UC_FIM));
}
/* Lo escrito por la CPU en las ventanas WC sale de sus búferes antes de seguir (una instrucción
 * con LOCK los vacía, SDM vol. 3, 11.3.1; vale en cualquier CPU con PAT). */
static inline void wc_vaciar(void) { __asm__ volatile("lock; orl $0, (%%esp)" ::: "memory"); }
static uint32_t us(uint64_t ciclos) { return hw_tsc_khz() ? (uint32_t)(ciclos * 1000u / hw_tsc_khz()) : 0; }
/* Saca de la caché de la CPU las líneas de [p, p + n) (la GPU las lee o escribe por la memoria). */
static void cache_vaciar(const void *p, uint32_t n)
{
    uint32_t a = (uint32_t)p & ~63u, fim = (uint32_t)p + n;
    __asm__ volatile("mfence" ::: "memory");
    for (; a < fim; a += 64u) __asm__ volatile("clflush (%0)" :: "r"(a) : "memory");
    __asm__ volatile("mfence" ::: "memory");
}
static inline volatile uint32_t *hws(void)        { return zw(Z_HWS); }
static inline volatile uint32_t *lrc(void)        { return zw(Z_CTX + 4096u); }   /* página 1: los registros */

/* D390: lo que /sistema/conf/gpu3d-intel9.conf puede tocar, por LRI en cada tanda (nunca por MMIO
 * directo): CACHE_MODE/COMMON_SLICE/HDC_CHICKEN (0x7000-0x73FC), L3 (0xB000-0xB1FC) y los
 * «chicken» de las partes (0xE000-0xE8FC); y la palabra de banderas 0xFFF0, que no se escribe.
 * Nunca la pantalla, el GTT, los relojes ni la energía. */
static const g3d_faixa_t i9_faixas[] = {
    { 0x7000u, 0x73FCu }, { 0xB000u, 0xB1FCu }, { 0xE000u, 0xE8FCu }, { AJ_FLAGS, AJ_FLAGS },
};

static const char *nome_etapa(int e) { return e >= 0 && e <= 6 ? nomes_etapa[e] : "sin intentar"; }

static void prefixar(char *m, int nm)
{
    char t[160];
    snformatar(t, sizeof t, "etapa %d (%s): %s", etapa, nome_etapa(etapa), m);
    snformatar(m, (size_t)nm, "%s", t);
}

/* ------------------------------------------------------------------ forcewake */

static bool fw_esperar(uint32_t ack, uint32_t bit, bool ligado_, uint32_t ms)
{
    prazo_t p = prazo_ms(ms);
    while (((rd(ack) & bit) != 0) != ligado_)
        if (prazo_vencido(&p)) return false;
    return true;
}

/* El dominio de render solo lo usa el 3D: se limpia lo que dejó el firmware y se pide el bit 0.
 * El del GT lo comparte con el 2D (D434, bit 0): aquí el bit 1, sin tocar los demás. */
static bool fw_pegar(char *m, int nm)
{
    wr(FW_RENDER, MASC_OFF(0xFFFFu));
    (void)fw_esperar(FW_RENDER_ACK, 1u, false, 50);
    wr(FW_RENDER, MASC_ON(1u));
    if (!fw_esperar(FW_RENDER_ACK, 1u, true, 50)) {
        snformatar(m, (size_t)nm, "forcewake de render %x sin respuesta (acuse %x)", FW_RENDER, rd(FW_RENDER_ACK));
        wr(FW_RENDER, MASC_OFF(1u));
        return false;
    }
    wr(FW_GT, MASC_ON(FW_BIT_GT));
    if (!fw_esperar(FW_GT_ACK, FW_BIT_GT, true, 50)) {
        snformatar(m, (size_t)nm, "forcewake del GT %x (bit 1) sin respuesta (acuse %x)", FW_GT, rd(FW_GT_ACK));
        wr(FW_RENDER, MASC_OFF(1u));
        wr(FW_GT, MASC_OFF(FW_BIT_GT));
        return false;
    }
    acordado = true;
    klog("[GPU3D] intel9: etapa 2: GPU despierta (render %x acuse %x, GT %x acuse %x)\n", FW_RENDER, rd(FW_RENDER_ACK),
         FW_GT, rd(FW_GT_ACK));
    return true;
}

/* Antes de cada tanda: si alguien quitó el forcewake (p. ej. «acel2d probar» lo pone a cero), se pide otra vez. */
static bool fw_conferir(void)
{
    if ((rd(FW_RENDER_ACK) & 1u) && (rd(FW_GT_ACK) & FW_BIT_GT)) return true;
    klog("[GPU3D] intel9: el forcewake se perdio (render %x, GT %x): se pide de nuevo\n", rd(FW_RENDER_ACK), rd(FW_GT_ACK));
    wr(FW_RENDER, MASC_ON(1u));
    wr(FW_GT, MASC_ON(FW_BIT_GT));
    return fw_esperar(FW_RENDER_ACK, 1u, true, 50) && fw_esperar(FW_GT_ACK, FW_BIT_GT, true, 50);
}

static void fw_soltar(void)
{
    if (!acordado) return;
    wr(FW_RENDER, MASC_OFF(1u));
    wr(FW_GT, MASC_OFF(FW_BIT_GT));
    acordado = false;
}

/* ------------------------------------------------------------------ sonda (solo lee) */

/* ¿Algún plano (1-4 de los pipes A-C) o cursor encendido usa la memoria de la zona? Solo lee. */
static bool superficies_livres(uint32_t ini, uint32_t fim, uint32_t fb_gfx, char *m, int nm)
{
    for (uint32_t pp = 0; pp < 3; pp++) {
        uint32_t o = 0x1000u * pp, alto = (rd(0x6001Cu + o) & 0xFFFu) + 1;
        for (uint32_t q = 0; q < 4; q++) {
            uint32_t oq = o + 0x100u * q;
            uint32_t cntr = rd(0x70180u + oq), surf = rd(0x7019Cu + oq) & ~0xFFFu, stride = rd(0x70188u + oq);
            if ((cntr & EX_PLANO_LIGADO) && surf != fb_gfx &&
                intel_solapa(surf, ((stride & 0xFFFu) * 64u) * alto + 0x10000u, ini, fim)) {
                snformatar(m, (size_t)nm, "el plano %u del pipe %c (SURF %x) usa la memoria de la zona 3D", q + 1, 'A' + pp, surf);
                return false;
            }
        }
        uint32_t cc = rd(0x70080u + o), cb = rd(0x70084u + o) & ~0xFFFu;
        if ((cc & 0x27u) && intel_solapa(cb, 256u << 10, ini, fim)) {
            snformatar(m, (size_t)nm, "el cursor del pipe %c (%x) esta en la zona 3D", 'A' + pp, cb);
            return false;
        }
    }
    return true;
}

/* El GTT de la pantalla tiene que ser válido y contiguo (de ahí sale la física de la zona); el de
 * la zona, válido y contiguo o vacío. Calcula el tamaño de la zona con la memoria robada y la
 * reservada. Solo lee: escribe «faltan» (las entradas que habrá que poner). */
static bool sondar_gtt(const pci_disp_t *d, uint32_t bar0, uint32_t fb_gfx, uint32_t n_tela, uint32_t z_off, uint64_t *f0_out,
                       uint32_t *faltan, char *m, int nm)
{
    uint32_t n_max = (z_off + ZONA_MAX) >> 12;
    uint32_t ini = intel_gtt_desloc(INTEL_VAR_BDW) + (fb_gfx >> 12) * 8u;
    uint32_t fis = bar0 + (ini & ~0xFFFu), tam = (ini & 0xFFFu) + n_max * 8u;
    if (!gtt || gtt_fis != fis || gtt_tam < tam) {
        gtt = (volatile uint32_t *)janela_mapear_tipo(fis, tam, PG_PCD | PG_PWT);
        gtt_fis = fis; gtt_tam = gtt ? tam : 0;
    }
    if (!gtt) { snformatar(m, (size_t)nm, "sin ventana para el GTT"); return false; }
    volatile uint32_t *e = (volatile uint32_t *)((volatile uint8_t *)gtt + (ini & 0xFFFu));
    uint64_t sb = 0, st = 0, rb = 0, f0 = 0;
    uint32_t gmch = pci_le32(d, 0x50) & 0xFFFFu, bsm = pci_le32(d, 0x5C), rt = 0, ruim = 0;
    if (!intel_robada9(gen, gmch, bsm, 0, &sb, &st)) {
        snformatar(m, (size_t)nm, "no se sabe donde esta la memoria robada (GMCH %x, BSM %x)", gmch, bsm);
        return false;
    }
    bool res = intel_reservado9(gen, rd(INTEL_STOLEN_RESERVED), rd(INTEL_STOLEN_RESERVED + 4), sb + st, &rb, &rt);
    int r = intel_gtt_conferir(INTEL_VAR_BDW, e, n_tela, res, rb, rt, &ruim, &f0);
    klog("[GPU3D] intel9: etapa 0: memoria robada %x%08x, %u MB (GMCH %x, BSM %x); reservada %x%08x + %u KB; "
         "GTT de la pantalla (%u paginas): %s, primera fisica %x%08x\n", (uint32_t)(sb >> 32), (uint32_t)sb, (uint32_t)(st >> 20),
         gmch, bsm, (uint32_t)(rb >> 32), (uint32_t)rb, res ? rt >> 10 : 0, n_tela, intel_gtt_texto(r), (uint32_t)(f0 >> 32),
         (uint32_t)f0);
    if (r != INTEL_GTT_OK) { snformatar(m, (size_t)nm, "GTT de la pantalla: %s (pagina %u)", intel_gtt_texto(r), ruim); return false; }
    /* Lo que cabe de la zona: hasta el final de la robada o el principio de la reservada. */
    uint64_t z0 = f0 + z_off, lim = sb + st;
    if (res && rb > z0 && rb < lim) lim = rb;
    if (z0 < sb || z0 >= lim || lim - z0 < ZONA_MIN) {
        snformatar(m, (size_t)nm, "no caben %u MB de zona 3D en la memoria robada (desde %x%08x hasta %x%08x)", ZONA_MIN >> 20,
                   (uint32_t)(z0 >> 32), (uint32_t)z0, (uint32_t)(lim >> 32), (uint32_t)lim);
        return false;
    }
    uint64_t cabe = lim - z0;
    zona_bytes = cabe > ZONA_MAX ? ZONA_MAX : (uint32_t)cabe & ~0xFFFFu;
    /* Que la PPGTT quepa en su sitio: si hiciera falta más tablas de las que hay, una zona menor. */
    while (zona_bytes > ZONA_MIN && intel_ppgtt_n_pt(fb_gfx + z_off, fb_gfx + z_off + zona_bytes) > PT_MAX) zona_bytes -= 1u << 20;
    if (fb_gfx + z_off + zona_bytes > APERTURA_MAX) {
        snformatar(m, (size_t)nm, "la zona 3D no cabe en los primeros %u MB de la apertura", APERTURA_MAX >> 20);
        return false;
    }
    /* Las entradas de la zona: válidas y contiguas con la pantalla, o sin usar. */
    uint32_t k0 = z_off >> 12, nz = zona_bytes >> 12;
    *faltan = 0;
    for (uint32_t k = k0; k < k0 + nz; k++) {
        uint32_t lo = e[2 * k], hi = e[2 * k + 1];
        uint64_t esperada = f0 + (uint64_t)k * 4096u;
        if (lo & 1u) {
            uint64_t f = (((uint64_t)hi & 0x3FFFu) << 32) | (lo & 0xFFFFF000u);
            if (f != esperada) {
                snformatar(m, (size_t)nm, "GTT: la pagina %u de la zona apunta a %x%08x, no a la contigua", k - k0,
                           (uint32_t)(f >> 32), (uint32_t)f);
                return false;
            }
        } else (*faltan)++;
    }
    *f0_out = f0;
    return true;
}

/* ------------------------------------------------------------------ envío por execlists */

static bool csb_consumir(bool mmio_ok)
{
    uint32_t w, rl, tam = intel_csb_tam(gen);
    bool livre = false;
    intel_csb_ptrs(rd(RCS + EX_RING_CSB_PTR), gen, &w, &rl);
    for (uint32_t v = 0; v < tam && intel_csb_novas(w, csb_lido, gen); v++) {
        uint32_t j = (csb_lido + 1) % tam;
        volatile uint32_t *c = hws() + CSB_DWORD + 2 * j;
        uint32_t lo = c[0], hi = c[1];
        if (lo == CSB_VAZIA && hi == CSB_VAZIA) {
            if (!mmio_ok) break;
            lo = rd(RCS + intel_csb_mmio(j));
            hi = rd(RCS + intel_csb_mmio(j) + 4);
            if (!avisou_mmio) { klog("[GPU3D] intel9: el CSB no llega a la pagina de estado: se lee del MMIO\n"); avisou_mmio = true; }
        }
        bool fim = intel_csb_terminou(lo, hi, gen);
        if (fim) livre = true;
        if (envios <= 4) klog("[GPU3D] intel9: tanda %u: CSB[%u] = %08x %08x (%s)\n", envios, j, lo, hi, fim ? "motor libre" : "cambio");
        c[0] = CSB_VAZIA; c[1] = CSB_VAZIA;
        csb_lido = j;
        wr(RCS + EX_RING_CSB_PTR, intel_csb_avisar(j, gen));
    }
    return livre;
}

static void log_motor(const char *fase)
{
    klog("[GPU3D] intel9 %s: HEAD %x TAIL %x START %x CTL %x ACTHD %x MI_MODE %x GFX_MODE %x EXECLIST %x CSB %x\n", fase,
         rd(RCS + EX_RING_HEAD), rd(RCS + EX_RING_TAIL), rd(RCS + EX_RING_START), rd(RCS + EX_RING_CTL), rd(RCS + 0x74),
         rd(RCS + EX_RING_MI_MODE), rd(RCS + EX_RING_MODE), rd(RCS + EX_RING_EXEC_STAT), rd(RCS + EX_RING_CSB_PTR));
    klog("[GPU3D] intel9 %s: IPEIR %x IPEHR %x INSTDONE %x SC %x SAMPLER %x ROW %x EIR %x ESR %x FALLO %x TLB %x %x\n", fase,
         rd(RCS + 0x64), rd(RCS + 0x68), rd(RCS + 0x6C), rd(0x7100), rd(0xE160), rd(0xE164), rd(RCS + 0xB0), rd(RCS + 0xB8),
         rd(0x4094), rd(0x4B10), rd(0x4B14));
}

/* Manda lo que ya está en la mitad «k» del anillo (n palabras): apunta el contexto a ella y manda
 * el descriptor. El motor tiene que estar libre (la tanda anterior, esperada). */
static void enviar_metade(uint32_t k, int n)
{
    volatile uint32_t *l = lrc();
    l[EX_CTX_RING_HEAD] = k * ANEL_METADE;
    l[EX_CTX_RING_TAIL] = k * ANEL_METADE + (uint32_t)n * 4u;
    if (!(flags & FL_RESTAURAR))   /* sin cargar el estado guardado: la tanda lo pone todo */
        l[EX_CTX_CONTROL] = MASC_ON(EX_CTX_CONTROL_INHIBIT_SYN) | MASC_ON(EX_CTX_CONTROL_RESTORE_INHIBIT) |
                            ((flags & FL_SEM_GUARDAR) ? MASC_ON(EX_CTX_CONTROL_SAVE_INHIBIT) : MASC_OFF(EX_CTX_CONTROL_SAVE_INHIBIT)) |
                            MASC_OFF(EX_CTX_CONTROL_RS_ENABLE);
    wc_vaciar();                   /* el anillo, el estado y los vértices (WC) ya en la memoria */
    (void)rd(RCS + EX_RING_HEAD);
    uint64_t desc = intel_ctx_descritor(zona_gfx + Z_CTX, gen, 0u);
    uint32_t off[4], val[4];
    int q = intel_envio_seq(desc, false, off, val);
    envios++;
    for (int i = 0; i < q; i++) wr(RCS + off[i], val[i]);
    pendente = true;
}

/* Copia una tanda corta al anillo (mitad 0) y la manda. */
static void enviar(const uint32_t *b, int n)
{
    volatile uint32_t *r = zw(Z_ANEL);
    for (int i = 0; i < n; i++) r[i] = b[i];
    enviar_metade(0, n);
}

/* Espera, con plazo, a la marca «m» y a que el CSB diga que el motor dejó el contexto. Tras la
 * espera, un error nuevo del motor (EIR) o un fallo de página (RING_FAULT) también es un fallo. */
static bool esperar_tanda(uint32_t m_esp, uint32_t ms, const char *quien)
{
    if (!pendente) return true;
    prazo_t p = prazo_ms(ms), pc = prazo_ms(PRAZO_CSB_MS);
    bool fim = false, livre = false, mmio_ok = false;
    for (;;) {
        if (!mmio_ok && prazo_vencido(&pc)) mmio_ok = true;
        if (csb_consumir(mmio_ok)) livre = true;
        if (!fim) {
            if (hws()[MARCA_DWORD] == m_esp) fim = true;
            else if (livre && (rd(RCS + EX_RING_HEAD) & 0x001FFFFCu) == lrc()[EX_CTX_RING_TAIL]) {
                fim = true;
                if (!avisou_marca) { klog("[GPU3D] intel9: la marca no llega a la pagina de estado (HEAD si)\n"); avisou_marca = true; }
            }
        }
        if (fim && livre) break;
        if (prazo_vencido(&p)) {
            snformatar(mot_ultimo, sizeof mot_ultimo, "%s: la tanda %u no termina en %u ms (marca %x de %x)", quien, envios, ms,
                       hws()[MARCA_DWORD], m_esp);
            klog("[GPU3D] intel9: %s; libre %d\n", mot_ultimo, livre);
            log_motor("tras el plazo");
            for (uint32_t j = 0; j < intel_csb_tam(gen); j++)
                klog("[GPU3D] intel9: CSB[%u] memoria %08x %08x, MMIO %08x %08x\n", j, hws()[CSB_DWORD + 2 * j],
                     hws()[CSB_DWORD + 2 * j + 1], rd(RCS + intel_csb_mmio(j)), rd(RCS + intel_csb_mmio(j) + 4));
            wr(RCS + EX_RING_MI_MODE, MASC_ON(EX_STOP_RING));   /* que el motor no siga (intel_engine_stop_cs) */
            pendente = false;
            ligado = false;
            travado = true;
            return false;
        }
    }
    pendente = false;
    uint32_t eir = rd(RCS + 0xB0), falta = rd(0x4094);
    if ((eir & ~eir_antes) || ((falta & 1u) && falta != falta_antes)) {
        snformatar(mot_ultimo, sizeof mot_ultimo, "%s: el motor marco un error (EIR %x, RING_FAULT %x)", quien, eir, falta);
        klog("[GPU3D] intel9: %s\n", mot_ultimo);
        log_motor("tras el error");
        wr(RCS + EX_RING_MI_MODE, MASC_ON(EX_STOP_RING));
        ligado = false;
        return false;
    }
    return true;
}

/* ------------------------------------------------------------------ lotes (D448) */

static uint32_t hueco_est(uint32_t k) { return Z_ESTADO + k * EST_HUECO; }
static uint32_t hueco_vb(uint32_t k)  { return Z_VB + k * VB_BYTES; }

/* Abre el lote en el hueco k_lote. El hueco ya está libre: la tanda que lo usó por última vez se
 * esperó al mandar la del otro hueco. */
static bool lote_abrir(void)
{
    if (lote_aberto) return true;
    if (!ligado || !alvo_l) return false;
    i9_lote_cfg_t c;
    memset(&c, 0, sizeof c);
    c.gen = gen; c.base = zona_gfx; c.marca_ggtt = zona_gfx + Z_HWS + MARCA_DWORD * 4u; c.marca = marca + 1u;
    c.scratch = zona_gfx + Z_SCRATCH; c.k_cor = Z_K_COR; c.k_tex = Z_K_TEX;
    c.cor = (uint32_t)b_cor.gpu; c.cor_passo = alvo_passo; c.larg = alvo_l; c.alt = alvo_a;
    c.cor_mocs = cor_ram ? (uint8_t)MOCS_RAM : 0u;
    c.z = (uint32_t)b_z.gpu; c.z_passo = i9_passo_z(alvo_l, alvo_zbits); c.zbits = alvo_zbits;
    if (b_est.usado) { c.est = (uint32_t)b_est.gpu; c.est_passo = b_est.passo; }
    c.vb = zona_gfx + hueco_vb(k_lote); c.vb_bytes = VB_BYTES;
    c.tlb = tlb_pendente;
    c.perfil_ggtt = (flags & FL_PERFIL) ? zona_gfx + Z_HWS + PERFIL_DW * 4u : 0;
    c.sem_l3 = (flags & FL_SEM_L3) && l3_posto;
    l3_posto = true;
    c.aj_reg = aj_reg; c.aj_val = aj_val; c.n_aj = n_aj;
    if (!(flags & FL_SEM_WA_CTX)) { c.wa = was; c.n_wa = n_was; }
    uint32_t *cmdw = (uint32_t *)zw(Z_ANEL + k_lote * ANEL_METADE), *estw = (uint32_t *)zw(hueco_est(k_lote));
    if (i9_lote_abrir(&lote, &c, cmdw, (int)(ANEL_METADE / 4u), estw, hueco_est(k_lote), EST_HUECO) != 0) {
        klog("[GPU3D] intel9: el lote no se puede abrir (error interno: %d palabras)\n", lote.n);
        return false;
    }
    vb_usado = 0;
    lote_aberto = true;
    return true;
}

static void perfil_ler(void);
static int desenho_d(const i9_desenho_t *dd, const void *v, int n, uint32_t passo, g3d_vala_t *vala, uint64_t t0);

/* Cierra el lote abierto y lo manda: antes espera a la tanda que esté en la GPU (la del otro hueco).
 * No espera a esta: la CPU sigue con el otro hueco. */
static bool lote_enviar(const char *quien)
{
    if (!lote_aberto) return true;
    lote_aberto = false;
    int n = i9_lote_fechar(&lote);
    if (n < 0) {
        snformatar(mot_ultimo, sizeof mot_ultimo, "%s: el lote no cuadra (error interno, %d palabras)", quien, lote.n);
        klog("[GPU3D] intel9: %s\n", mot_ultimo);
        return false;
    }
    uint64_t t0 = prazo_tsc();
    bool era = pendente, ok = esperar_tanda(marca, GPU3D_PRAZO_MS, quien);
    t_espera_lote += prazo_tsc() - t0;
    if (!ok) return false;
    if (era) perfil_ler();
    if (!fw_conferir()) { snformatar(mot_ultimo, sizeof mot_ultimo, "forcewake perdido"); ligado = false; return false; }
    if ((flags & FL_LOG_TANDAS) && envios < 16)
        klog("[GPU3D] intel9: tanda %u (hueco %u): %d palabras, %d dibujos, %d cambios de estado, %u vertices, %u bytes de estado%s\n",
             envios + 1, k_lote, n, lote.desenhos, lote.mudancas, vb_usado / I9_VERT_BYTES, lote.est_n,
             lote.c.tlb ? ", TLB" : "");
    marca = lote.c.marca;
    if (lote.c.tlb) tlb_pendente = false;
    n_lotes++; n_desenhos += (uint32_t)lote.desenhos; n_mudancas += (uint32_t)lote.mudancas;
    if (freq_pode) intel_rps_trabalho();
    enviar_metade(k_lote, n);
    k_lote ^= 1u;
    return true;
}

/* Bandera 800: lo que tardó cada parte de la tanda que acaba de terminar, por las marcas de la GPU. */
static void perfil_ler(void)
{
    if (!(flags & FL_PERFIL) || !ts_por_ms) return;
    volatile uint32_t *t = hws() + PERFIL_DW;
    uint32_t d[4];
    for (int i = 0; i < 4; i++) d[i] = t[2 * (i + 1)] - t[2 * i];
    for (int i = 0; i < 4; i++) {
        perfil_soma[i] += d[i];
        d[i] = (uint32_t)((uint64_t)d[i] * 1000u / ts_por_ms);
    }
    n_perfil++;
    if (n_perfil <= 6 || n_perfil % 600 == 0)
        klog("[GPU3D] intel9 perfil: tanda %u: registros %u us, estado %u us, dibujos %u us, vaciado final %u us (media de %u: %u %u %u %u us)\n",
             envios, d[0], d[1], d[2], d[3], n_perfil, (uint32_t)(perfil_soma[0] * 1000u / ts_por_ms / n_perfil),
             (uint32_t)(perfil_soma[1] * 1000u / ts_por_ms / n_perfil), (uint32_t)(perfil_soma[2] * 1000u / ts_por_ms / n_perfil),
             (uint32_t)(perfil_soma[3] * 1000u / ts_por_ms / n_perfil));
}

/* Manda lo que haya y espera a que la GPU acabe todo. */
static bool lote_terminar(uint32_t ms, const char *quien)
{
    if (!lote_enviar(quien)) { ligado = false; return false; }
    uint64_t t0 = prazo_tsc();
    bool era = pendente, ok = esperar_tanda(marca, ms, quien);
    t_espera_lote += prazo_tsc() - t0;
    if (ok && era) perfil_ler();
    return ok;
}

/* Un dibujo en el lote abierto (lo abre, y si no cabe, manda el lleno y abre otro). «e» NULL:
 * borrado con la máscara «limpar». */
static int desenho(const gpu3d_estado_t *e, const gpu3d_vert_t *v, int n, uint32_t limpar, g3d_vala_t *vala)
{
    if (!ligado || !alvo_l) return G3D_FALHA;
    if (n <= 0) return G3D_OK;
    if ((uint32_t)n * I9_VERT_BYTES > VB_BYTES) return G3D_NAO;
    uint64_t t0 = prazo_tsc();
    i9_desenho_t d;
    memset(&d, 0, sizeof d);
    d.formato = I9_FMT_V1;
    d.k_ps = Z_K_COR;
    d.mascara = 15;
    d.f_orig = 1; d.f_dest = 0x11;              /* ONE, ZERO */
    if (limpar) {
        d.mascara = (limpar & 1u) ? 15 : 0;
        d.z_teste = d.z_escrita = (limpar & 2u) != 0;
        d.func_z = 0;                           /* siempre: borrar es dibujar un rectángulo al fondo */
    } else {
        d.z_teste = (e->flags & G3D_Z_TESTE) != 0;
        d.z_escrita = (e->flags & G3D_Z_ESCRITA) != 0;
        if (d.z_escrita && !d.z_teste) { d.z_teste = true; d.func_z = 0; }   /* escribir sin probar: siempre */
        else d.func_z = (uint8_t)i9_func_z(e->func_z);
        d.mezcla = (e->flags & G3D_MEZCLA) != 0;
        if (d.mezcla) { d.f_orig = (uint8_t)i9_fator(e->mez_orig); d.f_dest = (uint8_t)i9_fator(e->mez_dest); }
        if ((e->flags & G3D_TEXTURA) && e->textura < GPU3D_TEXTURAS && b_tex[e->textura].usado) {
            const g3d_buffer_t *x = &b_tex[e->textura];
            d.k_ps = Z_K_TEX;
            d.n_tex = 1;
            d.t[0].tex = (uint32_t)x->gpu; d.t[0].passo = x->passo; d.t[0].larg = x->larg; d.t[0].alt = x->alt;
            d.t[0].mag_linear = d.t[0].min_linear = (e->flags & G3D_TEX_LINEAR) != 0;
            d.t[0].env_s = (e->flags & G3D_TEX_REPETE_S) ? 0 : 1;
            d.t[0].env_t = (e->flags & G3D_TEX_REPETE_T) ? 0 : 1;
        }
    }
    return desenho_d(&d, v, n, I9_VERT_BYTES, vala, t0);
}

/* Un dibujo ya traducido (v1 o v2) con sus «n» vértices de «passo» bytes. */
static int desenho_d(const i9_desenho_t *dd, const void *v, int n, uint32_t passo, g3d_vala_t *vala, uint64_t t0)
{
    uint64_t e0 = t_espera_lote;
    const i9_desenho_t *pd = dd;
    uint32_t bytes = (uint32_t)n * passo;
    if (bytes > VB_BYTES) return G3D_NAO;
    if (!lote_abrir()) return G3D_FALHA;
    if (vb_usado + bytes > VB_BYTES || !i9_lote_cabe(&lote)) {
        n_cheios++;
        if (!lote_enviar("lote lleno") || !lote_abrir()) { ligado = false; return G3D_FALHA; }
    }
    /* Los vértices, tal cual, en el hueco del lote (WC). */
    volatile uint32_t *dv = zw(hueco_vb(k_lote) + vb_usado);
    const uint32_t *o = (const uint32_t *)v;
    for (uint32_t i = 0; i < bytes / 4u; i++) dv[i] = o[i];
    if (i9_lote_desenho(&lote, pd, vb_usado, n) != 0) {
        snformatar(mot_ultimo, sizeof mot_ultimo, "el dibujo no cabe en el lote (error interno)");
        klog("[GPU3D] intel9: %s\n", mot_ultimo);
        lote_aberto = false;
        return G3D_FALHA;
    }
    vb_usado += bytes;
    if (pd->formato == I9_FMT_V2 && pd->k_ps >= Z_K2) k2[(pd->k_ps - Z_K2) / I9_KERNEL2_BYTES].uso = lote.c.marca;   /* en este lote */
    if (vala) *vala = lote.c.marca;
    t_montar += prazo_tsc() - t0 - (t_espera_lote - e0);   /* lo que esperó a la GPU va aparte */
    /* Bandera 40: un lote por dibujo (sin esperarlo aquí). */
    if ((flags & FL_SEM_LOTES) && !lote_enviar("un lote por dibujo")) { ligado = false; return G3D_FALHA; }
    return G3D_OK;
}

/* La tanda más simple: solo la marca (un PIPE_CONTROL con CS stall y escritura en la GGTT). */
static bool tanda_vazia(void)
{
    uint32_t b[8];
    int n = 0;
    b[n++] = I9_PIPE_CONTROL;
    b[n++] = I9_PC_CS_STALL | I9_PC_WRITE_IMM | I9_PC_GGTT;
    b[n++] = zona_gfx + Z_HWS + MARCA_DWORD * 4u;
    b[n++] = 0;
    b[n++] = ++marca;
    b[n++] = 0;
    enviar(b, n);
    return esperar_tanda(marca, PRAZO_INICIO, "tanda vacia");
}

/* ------------------------------------------------------------------ workarounds por MMIO */

static void wa_mmio(void)
{
    for (int i = 0; i < n_was; i++) {
        const i9_wa_t *w = &was[i];
        if (w->tipo == I9_WA_CTX) continue;
        if ((w->tipo == I9_WA_MOTOR && (flags & FL_SEM_WA_MOTOR)) || (w->tipo == I9_WA_GT && (flags & FL_SEM_WA_GT))) continue;
        uint32_t antes = rd(w->reg);
        if (w->mascarado == 2 && (antes == 0 || antes == 0xFFFFFFFFu)) {
            klog("[GPU3D] intel9: wa %x (%s): se lee %x (registro de varias copias sin direccion): no se toca\n", w->reg, w->nome, antes);
            continue;
        }
        wr(w->reg, w->mascarado == 1 ? ((w->set | w->clr) << 16 | w->set) : ((antes & ~w->clr) | w->set));
        klog("[GPU3D] intel9: wa %x: %x -> %x (%s)\n", w->reg, antes, rd(w->reg), w->nome);
    }
}

/* ------------------------------------------------------------------ encender */

static void cor_soltar(void);
static void vb_soltar(void);
static void soltar_tudo(void)
{
    for (int i = 0; i < GPU3D_TEXTURAS; i++) if (b_tex[i].usado) g3d_buffer_soltar(&arena, &b_tex[i]);
    cor_soltar();
    if (b_z.usado) g3d_buffer_soltar(&arena, &b_z);
    if (b_est.usado) g3d_buffer_soltar(&arena, &b_est);
    vb_soltar();                       /* D457: tras un cuelgue, ram_desmapear tampoco devuelve el montón */
    alvo_l = alvo_a = 0;
}

static int i9_iniciar(const pci_disp_t *d, int geracao, const acel_tela_t *t, uint32_t variante, char *m, int nm)
{
    (void)variante;
    etapa = 0;
    if (travado) {
        snformatar(m, (size_t)nm, "el RCS no termino una tanda en un encendido anterior (%s): no se vuelve a probar hasta reiniciar",
                   mot_ultimo);
        prefixar(m, nm);
        return G3D_NAO;
    }
    ligado = false;
    pendente = false;
    lote_aberto = false;
    if (ram_cor) { memfisica_liberar_seguidas(ram_cor, ram_cor_pag); ram_cor = 0; ram_cor_pag = 0; }   /* de un encendido sin apagar */
    if (vb_ram) { memfisica_liberar_seguidas(vb_ram, vb_ram_pag); vb_ram = 0; vb_ram_pag = 0; }
    memset(&vbm, 0, sizeof vbm);
    cor_ram = false;
    disp9 = *d;
    const char *nome = NULL;
    int var = intel_variante_exec(d->modelo, &nome);
    if (!var || intel_var_geracao(var) != geracao) {
        snformatar(m, (size_t)nm, "el modelo %04x no esta en la lista de Gen9-12 (D434)", d->modelo);
        prefixar(m, nm);
        return G3D_NAO;
    }
    if (geracao != 9) {
        snformatar(m, (size_t)nm, "%s (Gen%d): el 3D de Gen11/12 todavia no esta (D438: primero Gen9)", nome ? nome : "?", geracao);
        prefixar(m, nm);
        return G3D_NAO;
    }
    gen = geracao;
    plat = i9_plataforma(d->modelo);
    n_was = i9_workarounds(gen, plat, &was);
    flags = 0; n_aj = 0;
    for (int i = 0; i < gpu3d_ajustes.n; i++) {
        if (gpu3d_ajustes.reg[i] == AJ_FLAGS) { flags |= gpu3d_ajustes.val[i]; continue; }
        if (n_aj < G3D_AJUSTES_MAX) { aj_reg[n_aj] = gpu3d_ajustes.reg[i]; aj_val[n_aj] = gpu3d_ajustes.val[i]; n_aj++; }
    }
    uint32_t bar0 = pci_le32(d, 0x10), gmadr = pci_le32(d, 0x18);
    if ((((bar0 >> 1) & 3) == 2 && pci_le32(d, 0x14)) || (((gmadr >> 1) & 3) == 2 && pci_le32(d, 0x1C))) {
        snformatar(m, (size_t)nm, "BAR por encima de 4 GB (MMIO %x%08x, apertura %x%08x)", pci_le32(d, 0x14), bar0,
                   pci_le32(d, 0x1C), gmadr);
        prefixar(m, nm);
        return G3D_NAO;
    }
    bar0 &= ~0xFu; gmadr &= ~0xFu;
    klog("[GPU3D] intel9: etapa 0: %s (%04x, Gen%d, plataforma %d), MMIO %x, apertura %x, pantalla %x %dx%d pitch %u; "
         "%d workarounds, banderas %x, %d ajustes\n", nome ? nome : "?", d->modelo, gen, plat, bar0, gmadr, t->fb_fisico, t->larg,
         t->alt, t->pitch, n_was, flags, n_aj);
    if (!bar0 || !gmadr || t->fb_fisico < gmadr || t->fb_fisico - gmadr >= APERTURA_MAX || t->bytes_px != 4 ||
        ((t->fb_fisico - gmadr) & 0xFFFu)) {
        snformatar(m, (size_t)nm, "pantalla fuera de la apertura o no es de 32 bits (pantalla %x, apertura %x)", t->fb_fisico, gmadr);
        prefixar(m, nm);
        return G3D_NAO;
    }
    if (!hw_tsc_khz()) { snformatar(m, (size_t)nm, "sin TSC medido para los plazos"); prefixar(m, nm); return G3D_NAO; }
    uint32_t fb_gfx = t->fb_fisico - gmadr;
    uint32_t r2d = intel_anel_desloc(t->pitch, t->alt, ACEL2D_ZONA_A);
    uint32_t z_off = (r2d + INTEL_EXEC_ZONA2D_MAX + 0xFFFFu) & ~0xFFFFu;   /* detrás de la zona del 2D (D434) */
    if (!mmio || mmio_fis != bar0) {
        mmio = (volatile uint8_t *)janela_mapear_tipo(bar0, 2u << 20, PG_PCD | PG_PWT);
        mmio_fis = bar0;
    }
    if (!mmio) { snformatar(m, (size_t)nm, "sin ventana para el MMIO"); prefixar(m, nm); return G3D_NAO; }
    uint32_t n_tela = (t->pitch * (uint32_t)t->alt + 0xFFFu) >> 12, faltan = 0;
    uint64_t f0 = 0;
    if (!sondar_gtt(d, bar0, fb_gfx, n_tela, z_off, &f0, &faltan, m, nm)) { prefixar(m, nm); return G3D_NAO; }
    zona_gfx = fb_gfx + z_off;
    zona_fis = f0 + z_off;
    if (!superficies_livres(zona_gfx, zona_gfx + zona_bytes, fb_gfx, m, nm)) { prefixar(m, nm); return G3D_NAO; }
    n_pt = intel_ppgtt_n_pt(zona_gfx, zona_gfx + zona_bytes);
    if (n_pt > PT_MAX || Z_PPGTT + (3u + n_pt) * 4096u > Z_UC_FIM) {
        snformatar(m, (size_t)nm, "la zona necesita %u tablas de la PPGTT (maximo %u)", n_pt, PT_MAX);
        prefixar(m, nm);
        return G3D_NAO;
    }
    klog("[GPU3D] intel9: etapa 0: zona 3D de %u KB en la grafica %x (fisica %x%08x), %u entradas del GTT sin poner\n",
         zona_bytes >> 10, zona_gfx, (uint32_t)(zona_fis >> 32), (uint32_t)zona_fis, faltan);

    /* Etapa 1: las entradas del GTT que falten (dentro de la robada: comprobado), la ventana y una
     * prueba de que la memoria guarda lo escrito. */
    etapa = 1;
    if (faltan) {
        uint32_t ini = intel_gtt_desloc(INTEL_VAR_BDW) + (fb_gfx >> 12) * 8u;
        volatile uint32_t *e = (volatile uint32_t *)((volatile uint8_t *)gtt + (ini & 0xFFFu));
        uint32_t molde = e[0] & 0xFFEu, k0 = z_off >> 12, nz = zona_bytes >> 12;
        for (uint32_t k = k0; k < k0 + nz; k++) {
            if (e[2 * k] & 1u) continue;
            uint32_t lo, hi;
            intel_pte_gen9(f0 + (uint64_t)k * 4096u, molde, &lo, &hi);
            e[2 * k + 1] = hi;
            e[2 * k] = lo;
        }
        wr(EX_GFX_FLSH_CNTL, 1u);
        (void)rd(EX_GFX_FLSH_CNTL);
        for (uint32_t k = k0; k < k0 + nz; k++)
            if (!(e[2 * k] & 1u)) { snformatar(m, (size_t)nm, "GTT: la pagina %u de la zona no quedo mapeada", k - k0); prefixar(m, nm); return G3D_FALHA; }
        klog("[GPU3D] intel9: etapa 1: %u entradas del GTT puestas (en la memoria robada)\n", faltan);
    }
    /* D448: WC (PAT entrada 1, solo PWT; wc.c) para lo que la CPU solo escribe, si la PAT está
     * programada; si no, o con la bandera 80, todo UC como en D438. */
    uint32_t cpu_fis = t->fb_fisico + z_off;
    com_wc = !(flags & FL_SEM_WC) && txt_compara(wc_via(), "PAT") == 0;
    uint32_t tipo = com_wc ? PG_PWT : (PG_PCD | PG_PWT);
    if (!zona_uc || zona_fis_cpu != cpu_fis || zona_map < zona_bytes || zona_tipo != tipo) {
        zona_wc0 = (volatile uint8_t *)janela_mapear_tipo(cpu_fis, Z_HWS, tipo);
        zona_uc = (volatile uint8_t *)janela_mapear_tipo(cpu_fis + Z_HWS, Z_UC_FIM - Z_HWS, PG_PCD | PG_PWT);
        zona_wc1 = (volatile uint8_t *)janela_mapear_tipo(cpu_fis + Z_UC_FIM, zona_bytes - Z_UC_FIM, tipo);
        bool ok = zona_wc0 && zona_uc && zona_wc1;
        zona_fis_cpu = cpu_fis; zona_map = ok ? zona_bytes : 0; zona_tipo = ok ? tipo : 0;
        if (!ok) zona_uc = NULL;
        motor_nosso = false;
    }
    if (!zona_uc) { snformatar(m, (size_t)nm, "sin ventana para la zona 3D"); prefixar(m, nm); return G3D_NAO; }
    static const uint32_t pruebas[4] = { 0, Z_HWS, Z_ARENA, 0 };
    for (int i = 0; i < 4; i++) {
        uint32_t off = i == 3 ? zona_bytes - 4096u : pruebas[i];
        volatile uint32_t *p = zw(off);
        p[0] = 0x5A5A0000u + (uint32_t)i; p[1] = 0xA5A5FFFFu;
        wc_vaciar();
        if (p[0] != 0x5A5A0000u + (uint32_t)i || p[1] != 0xA5A5FFFFu) {
            snformatar(m, (size_t)nm, "la zona no guarda lo escrito (desplazamiento %x: %x %x)", off, p[0], p[1]);
            prefixar(m, nm);
            return G3D_FALHA;
        }
    }
    klog("[GPU3D] intel9: etapa 1: la CPU ve la zona (%u KB por la apertura %x; %s)\n", zona_bytes >> 10, cpu_fis,
         com_wc ? "estado, anillo, vertices y texturas en WC" : "todo sin cache (UC)");

    /* Etapa 2: bus maestro y forcewake. */
    etapa = 2;
    uint32_t c = pci_le32(d, 0x04);
    if ((c & 0x06) != 0x06) pci_escreve32(d, 0x04, (c & 0xFFFFu) | 0x06);
    if (!fw_pegar(m, nm)) { prefixar(m, nm); return G3D_FALHA; }

    /* Etapa 3: el RCS sin usar (nadie más: ni i915, ni el modo heredado), salvo que lo dejáramos nosotros. */
    etapa = 3;
    uint32_t stat = rd(RCS + EX_RING_EXEC_STAT), ctl = rd(RCS + EX_RING_CTL);
    klog("[GPU3D] intel9: etapa 3: antes: EXECLIST_STATUS %x, RING_CTL %x, GFX_MODE %x, MI_MODE %x, CSB %x%s\n", stat, ctl,
         rd(RCS + EX_RING_MODE), rd(RCS + EX_RING_MI_MODE), rd(RCS + EX_RING_CSB_PTR), motor_nosso ? " (lo dejamos nosotros)" : "");
    if (!motor_nosso && (intel_exec_ocupado(stat) || (ctl & 1u))) {
        snformatar(m, (size_t)nm, "el RCS ya esta en uso (STATUS %x, CTL %x): no se toca", stat, ctl);
        fw_soltar();
        prefixar(m, nm);
        return G3D_NAO;
    }
    eir_antes = rd(RCS + 0xB0);
    falta_antes = rd(0x4094);
    if (falta_antes & 1u) klog("[GPU3D] intel9: etapa 3: RING_FAULT ya tenia un fallo de antes (%x): solo cuenta uno nuevo\n", falta_antes);

    /* Etapa 4: workarounds (motor y GT), PAT 3 y MOCS 0 sin caché, modo execlists, página de estado y CSB. */
    etapa = 4;
    for (uint32_t i = 0; i < Z_ARENA; i += 4u) *zw(i) = 0;   /* la parte fija; la arena la escribe quien la usa */
    wc_vaciar();
    (void)rd(RCS + EX_RING_HEAD);
    wa_mmio();
    uint32_t pat = intel_pat3_reg(gen), pat_antes = rd(pat);
    wr(pat, intel_pat3_valor(gen, pat_antes));
    klog("[GPU3D] intel9: etapa 4: PAT %x: %x -> %x\n", pat, pat_antes, rd(pat));
    /* D448: el índice 4 del PAT de la GPU (GEN8_PRIVATE_PAT_HI, byte 0) con caché en la LLC (WB, el valor
     * que le da i915, que lo usa para todo lo «cached») para el destino en RAM; los demás índices no se
     * tocan. Se deja como estaba al apagar. */
    uint32_t pat_hi = rd(PAT_HI);
    if (!pat_tocado) { pat_hi_antes = pat_hi; pat_tocado = true; }
    if (!(flags & FL_COR_ZONA)) wr(PAT_HI, (pat_hi & ~0xFFu) | PAT_WB_LLC);
    klog("[GPU3D] intel9: etapa 4: PAT %x: %x -> %x; %x: %x -> %x (indice 4 = WB en la LLC)\n", pat, pat_antes, rd(pat), PAT_HI,
         pat_hi, rd(PAT_HI));
    uint32_t mocs = rd(0xC800u), mocs1 = rd(0xC804u), l3cc = rd(0xB020u);
    if (!(flags & FL_SEM_MOCS)) {
        wr(0xC800u, 0x1u | (2u << 2));                    /* LE_1_UC | LE_TC_2_LLC_ELLC (I915_MOCS_UNCACHED) */
        wr(0xC804u, 3u << 4);                             /* entrada 1: LE_0_PAGETABLE | LE_TC_0_PAGETABLE | LRUM(3) (I915_MOCS_PTE) */
        wr(0xB020u, ((flags & FL_L3_WB) ? 3u << 4 : 1u << 4) << 16 | (1u << 4));   /* L3_1_UC en la 0; la 1 UC (o L3_3_WB) */
    }
    klog("[GPU3D] intel9: etapa 4: MOCS 0/1 del RCS %x %x -> %x %x, L3CC 0/1 %x -> %x\n", mocs, mocs1, rd(0xC800u), rd(0xC804u),
         l3cc, rd(0xB020u));
    /* D448: la frecuencia de la GPU (solo se lee; con la bandera 1000 se pide RP0). */
    uint32_t cap = rd(RP_STATE_CAP);
    klog("[GPU3D] intel9: etapa 4: frecuencia: actual %u MHz, pedida %x, RP_CONTROL %x; RP0 %u MHz, RP1 %u MHz, RPn %u MHz\n",
         (rd(RPSTAT1) >> 23) * 50u / 3u, rd(RPNSWREQ), rd(RP_CONTROL), (cap & 0xFFu) * 50u, ((cap >> 8) & 0xFFu) * 50u,
         ((cap >> 16) & 0xFFu) * 50u);
    /* D457: la frecuencia la lleva el gobernador común con el 2D (intel_rps.c: RP0 con trabajo, la del firmware tras 1 s sin). */
    freq_pode = !(flags & FL_SEM_FREQ) && intel_rps_ligar(mmio, INTEL_RPS_3D);
    wr(RCS + EX_RING_HWSTAM, 0xFFFFFFFFu);
    wr(RCS + EX_RING_MODE, MASC_ON(EX_GFX_MODE_LISTA));
    wr(RCS + EX_RING_MI_MODE, MASC_OFF(EX_STOP_RING));
    wr(RCS + EX_RING_HWS_PGA, zona_gfx + Z_HWS);
    (void)rd(RCS + EX_RING_HWS_PGA);
    wr(RCS + EX_RING_CSB_PTR, intel_csb_reiniciar(gen));
    for (uint32_t i = 0; i < 2u * intel_csb_tam(gen); i++) hws()[CSB_DWORD + i] = CSB_VAZIA;
    wr(RCS + EX_RING_CSB_PTR, intel_csb_reiniciar(gen));
    csb_lido = intel_csb_tam(gen) - 1u;
    motor_nosso = true;

    /* Etapa 5: PPGTT de la zona, la imagen del contexto del RCS y una tanda con solo la marca. */
    etapa = 5;
    uint64_t f_pp = zona_fis + Z_PPGTT;
    static uint32_t pag[1024];
    if (!intel_ppgtt_monta((uint32_t *)zw(Z_PPGTT), n_pt, f_pp, zona_gfx, zona_gfx + zona_bytes, zona_fis) ||
        !intel_lrc_monta_tab(pag, 1024, intel_lrc_tabela_rcs(gen), gen, RCS, 0, 0, zona_gfx + Z_ANEL,
                             ((ANEL_BYTES - 4096u) & 0x001FF000u) | 1u, f_pp)) {
        snformatar(m, (size_t)nm, "la PPGTT o la imagen del contexto no se pueden montar");
        fw_soltar();
        prefixar(m, nm);
        return G3D_FALHA;
    }
    for (int i = 0; i < 1024; i++) lrc()[i] = pag[i];
    if (flags & FL_RPCS) { lrc()[0x43] = 0x80000088u; klog("[GPU3D] intel9: etapa 5: R_PWR_CLK_STATE = %x en el contexto\n", lrc()[0x43]); }
    /* El reloj del CS (RING_TIMESTAMP) contra el TSC, para las marcas de tiempo (bandera 800). */
    {
        uint32_t a0 = rd(RCS_TIMESTAMP);
        uint64_t c0 = prazo_tsc();
        prazo_t pt = prazo_ms(3);
        while (!prazo_vencido(&pt)) { }
        uint32_t a1 = rd(RCS_TIMESTAMP);
        uint32_t ms = us(prazo_tsc() - c0) / 1000u;
        ts_por_ms = ms ? (a1 - a0) / ms : 0;
        n_perfil = 0; perfil_soma[0] = perfil_soma[1] = perfil_soma[2] = perfil_soma[3] = 0;
        klog("[GPU3D] intel9: etapa 5: reloj del CS: %u tics por ms\n", ts_por_ms);
    }
    klog("[GPU3D] intel9: etapa 5: contexto en %x (%u paginas), pagina de estado %x, anillo %x, PPGTT de %u tablas en la fisica "
         "%x%08x\n", zona_gfx + Z_CTX, CTX_PAGINAS, zona_gfx + Z_HWS, zona_gfx + Z_ANEL, n_pt, (uint32_t)(f_pp >> 32), (uint32_t)f_pp);
    ligado = true;
    envios = 0; marca = 0; k_lote = 0; lote_aberto = false; tlb_pendente = true; l3_posto = false;
    t_valla = t_copia = t_espera_lote = t_montar = 0;
    n_ler = n_lotes = n_desenhos = n_mudancas = n_cheios = 0;
    ult_n_ler = ult_n_lotes = ult_n_desenhos = 0; ult_t_valla = ult_t_copia = ult_t_montar = ult_t_espera = 0;
    if (!tanda_vazia()) {
        snformatar(m, (size_t)nm, "%s", mot_ultimo);
        fw_soltar();
        prefixar(m, nm);
        return G3D_FALHA;
    }
    klog("[GPU3D] intel9: etapa 5: el RCS ejecuta tandas (marca %x, CSB %x)\n", hws()[MARCA_DWORD], rd(RCS + EX_RING_CSB_PTR));

    /* Etapa 6: los sombreadores y una tanda con todo el estado 3D y ningún vértice. */
    etapa = 6;
    static uint32_t ps[I9_PS_MAX_INST * 4];
    for (int k = 0; k < 2; k++) {
        int n = i9_ps_gerar(ps, gen, k == 1);
        if (n <= 0 || (uint32_t)n * 16u > I9_KERNEL_BYTES - 256u) {
            snformatar(m, (size_t)nm, "el sombreador %d no se puede generar", k);
            fw_soltar(); ligado = false; prefixar(m, nm);
            return G3D_FALHA;
        }
        volatile uint32_t *kd = zw(k ? Z_K_TEX : Z_K_COR);
        for (int i = 0; i < n * 4; i++) kd[i] = ps[i];
        for (int i = n * 4; i < (int)(I9_KERNEL_BYTES / 4u); i += 4) { kd[i] = i9eu_opcode(I9_OP_NOP, gen); kd[i + 1] = kd[i + 2] = kd[i + 3] = 0; }
        wc_vaciar();
        klog("[GPU3D] intel9: etapa 6: sombreador de %s: %d instrucciones en %x\n", k ? "textura" : "color", n, zona_gfx + (k ? Z_K_TEX : Z_K_COR));
    }
    g3d_arena_iniciar(&arena, zona_gfx + Z_ARENA, (uint8_t *)zw(Z_ARENA), zona_bytes - Z_ARENA);
    memset(&b_cor, 0, sizeof b_cor); memset(&b_z, 0, sizeof b_z); memset(&b_est, 0, sizeof b_est); memset(b_tex, 0, sizeof b_tex);
    memset(k2, 0, sizeof k2);
    memset(kv, 0, sizeof kv);
    v3_desenhos = v3_subidas = 0;
    alvo_l = 64; alvo_a = 64; alvo_zbits = 24; alvo_passo = i9_passo_cor(64);
    b_cor.gpu = zona_gfx + Z_ARENA; b_z.gpu = zona_gfx + Z_ARENA + 0x10000u;   /* provisional: no se dibuja nada */
    cor_ram = false;
    bool r = lote_abrir() && lote_terminar(PRAZO_INICIO, "estado 3D");   /* el prólogo entero, sin dibujos */
    alvo_l = alvo_a = 0;
    if (!r) {
        snformatar(m, (size_t)nm, "el RCS no termina la tanda del estado 3D: %s", mot_ultimo);
        fw_soltar(); ligado = false; prefixar(m, nm);
        return G3D_FALHA;
    }
    klog("[GPU3D] intel9: etapa 6: el RCS acepta el estado 3D (%u tandas); sigue la autoprueba comun\n", envios);
    return G3D_OK;
}

/* ------------------------------------------------------------------ destino en RAM (D448) */

/* Quita de la PPGTT la región en RAM de «gfx» (pt_max tablas de 2 MB) y suelta sus páginas. Solo con el motor sin
 * trabajo y sin lote abierto; tras un cuelgue las páginas no se devuelven (la GPU podría escribirlas todavía). */
static void ram_desmapear(uint32_t gfx, uint32_t pt_max, uint32_t *f, uint32_t *pag)
{
    if (!*f) return;
    volatile uint32_t *pd = zw(Z_PPGTT + 2u * 4096u);
    for (uint32_t j = 0; j < pt_max; j++) { pd[2u * ((gfx >> 21) + j)] = 0; pd[2u * ((gfx >> 21) + j) + 1u] = 0; }
    tlb_pendente = true;
    if (!travado) memfisica_liberar_seguidas(*f, *pag);
    *f = 0; *pag = 0;
}

/* Páginas seguidas de la RAM baja (la CPU las ve con caché, puntero = física) mapeadas en la PPGTT
 * desde «gfx» con el índice 4 del PAT (WB en la LLC: la GPU y la CPU ven lo mismo sin vaciar
 * nada, como los objetos I915_CACHE_LLC de i915). Las tablas también van en RAM y se vacían de la
 * caché de la CPU después de escribirlas (la GPU las lee sin caché, como las de la zona). D448 para el
 * destino; D457 también para el montón de los búferes de vértices. */
static bool ram_mapear(uint32_t gfx, uint32_t *pts, uint32_t pt_max, uint32_t bytes, uint32_t *f_out, uint32_t *pag_out, bool zerar)
{
    uint32_t pag = (bytes + 4095u) >> 12, n_t = (pag + 511u) / 512u;
    if (!pag || n_t > pt_max) return false;
    for (uint32_t j = 0; j < n_t; j++)
        if (!pts[j] && !(pts[j] = memfisica_alocar())) return false;
    uint32_t f = memfisica_alocar_seguidas(pag);
    if (!f) return false;
    if (zerar) { memset((void *)f, 0, pag * 4096u); cache_vaciar((const void *)f, pag * 4096u); }
    for (uint32_t j = 0; j < n_t; j++) {
        uint32_t *t = (uint32_t *)pts[j];
        for (uint32_t e = 0; e < 512u; e++) {
            uint32_t k = j * 512u + e;
            t[2u * e] = k < pag ? (f + k * 4096u) | PTE_PAT4 : 0;
            t[2u * e + 1u] = 0;
        }
        cache_vaciar(t, 4096u);
    }
    volatile uint32_t *pd = zw(Z_PPGTT + 2u * 4096u);
    for (uint32_t j = 0; j < pt_max; j++) {
        uint32_t lo = 0, hi = 0;
        if (j < n_t) intel_ppgtt_entrada(pts[j], &lo, &hi);
        pd[2u * ((gfx >> 21) + j) + 1u] = hi;
        pd[2u * ((gfx >> 21) + j)] = lo;
    }
    tlb_pendente = true;
    *f_out = f; *pag_out = pag;
    return true;
}
static void ram_soltar(void) { ram_desmapear(RAM_GFX, RAM_PT_MAX, &ram_cor, &ram_cor_pag); }
static bool ram_criar(uint32_t bytes) { return ram_mapear(RAM_GFX, ram_pt, RAM_PT_MAX, bytes, &ram_cor, &ram_cor_pag, true); }
/* D457: el montón de los búferes de v3. */
static void vb_soltar(void) { ram_desmapear(VB_GFX, VB_PT_MAX, &vb_ram, &vb_ram_pag); memset(&vbm, 0, sizeof vbm); }

static void cor_soltar(void)
{
    if (cor_ram) { ram_soltar(); memset(&b_cor, 0, sizeof b_cor); cor_ram = false; }
    else if (b_cor.usado) g3d_buffer_soltar(&arena, &b_cor);
}


/* ------------------------------------------------------------------ medidas (D448) */


/* Lo de los últimos cuadros, por cuadro: tandas, dibujos y microsegundos de cada parte. */
static void resumo(const char *fase)
{
    uint32_t q = n_ler - ult_n_ler;
    if (!q) return;
    klog("[GPU3D] intel9 %s: %u cuadros: %u.%02u tandas y %u dibujos por cuadro; por cuadro %u us esperando a la GPU al leer, "
         "%u us copiando el cuadro, %u us montando, %u us esperando en total; destino %s, %s\n", fase, q,
         (n_lotes - ult_n_lotes) / q, (n_lotes - ult_n_lotes) * 100u / q % 100u, (n_desenhos - ult_n_desenhos) / q,
         us(t_valla - ult_t_valla) / q, us(t_copia - ult_t_copia) / q, us(t_montar - ult_t_montar) / q,
         us(t_espera_lote - ult_t_espera) / q, cor_ram ? "en RAM con cache" : "en la zona (apertura)",
         com_wc ? "WC" : "UC");
    ult_n_ler = n_ler; ult_n_lotes = n_lotes; ult_n_desenhos = n_desenhos;
    ult_t_valla = t_valla; ult_t_copia = t_copia; ult_t_montar = t_montar; ult_t_espera = t_espera_lote;
}

/* ------------------------------------------------------------------ la interfaz de la capa */

static int i9_esperar(g3d_vala_t vala, uint32_t ms)
{
    (void)vala;
    if (!ligado) return G3D_FALHA;
    return lote_terminar(ms ? ms : GPU3D_PRAZO_MS, "esperar") ? G3D_OK : G3D_FALHA;
}

static int i9_alvo(int larg, int alt, int zbits)
{
    if (!ligado) return G3D_FALHA;
    if (larg < 1 || alt < 1 || larg > MAX_LADO || alt > MAX_LADO) return G3D_NAO;
    if (i9_esperar(0, GPU3D_PRAZO_MS) != G3D_OK) return G3D_FALHA;
    cor_soltar();
    if (b_z.usado) g3d_buffer_soltar(&arena, &b_z);
    if (b_est.usado) g3d_buffer_soltar(&arena, &b_est);
    alvo_l = alvo_a = 0;
    zbits = zbits == 16 ? 16 : 24;
    uint32_t passo = i9_passo_cor(larg), bytes = passo * (uint32_t)alt;
    /* D448: el destino en RAM con caché si cabe (la CPU lo lee deprisa); si no, en la zona. */
    if (!(flags & FL_COR_ZONA) && ram_criar(bytes)) {
        memset(&b_cor, 0, sizeof b_cor);
        b_cor.usado = true; b_cor.tipo = G3D_BUF_ALVO; b_cor.gpu = RAM_GFX; b_cor.bytes = bytes; b_cor.cpu = (uint8_t *)ram_cor;
        cor_ram = true;
    } else if (!g3d_buffer_criar(&arena, &b_cor, G3D_BUF_ALVO, bytes, 4096)) return G3D_NAO;
    if (!g3d_buffer_criar(&arena, &b_z, G3D_BUF_Z, (uint64_t)i9_passo_z(larg, zbits) * i9_alt_z(alt), 4096)) {
        cor_soltar();
        return G3D_NAO;
    }
    b_z.cpu = NULL;                        /* el Z va en mosaico Y: la CPU no lo toca */
    /* D453: el esténcil (8 bits, mosaico W de 64 bytes x 64 filas), si cabe; sin él, lo que lo pide va por software. */
    uint32_t pe = i9_passo_est(larg);   /* el doble del ancho (mosaico W): con el ancho, las filas de bloques se pisaban */
    if (!(flags & FL_SEM_V2) && g3d_buffer_criar(&arena, &b_est, G3D_BUF_Z, (uint64_t)pe * i9_alt_est(alt) / 2u, 4096)) {
        b_est.cpu = NULL; b_est.passo = pe;
    }
    b_cor.passo = passo; b_cor.larg = larg; b_cor.alt = alt; b_cor.bpp = 32;
    alvo_l = larg; alvo_a = alt; alvo_passo = passo; alvo_zbits = zbits;
    klog("[GPU3D] intel9: destino %dx%d (Z de %d bits) %s\n", larg, alt, zbits,
         cor_ram ? "en RAM con cache (indice 4 del PAT, MOCS 1)" : "en la zona (por la apertura)");
    return G3D_OK;
}

static int i9_limpar(uint32_t mascara, uint32_t cor)
{
    if (!ligado || !alvo_l) return G3D_FALHA;
    if (!(mascara & 3u)) return G3D_OK;
    /* Borrar es dibujar un rectángulo del destino entero al fondo (z = 1) que siempre pasa. */
    uint32_t um = 0x3F800000u, c = cor | 0xFF000000u;
    uint32_t x1 = g3d_float_de_fixo(alvo_l, 0), y1 = g3d_float_de_fixo(alvo_a, 0);
    gpu3d_vert_t q[6];
    q[0] = (gpu3d_vert_t){ 0, 0, um, um, c, 0, 0, 0 };
    q[1] = (gpu3d_vert_t){ x1, 0, um, um, c, 0, 0, 0 };
    q[2] = (gpu3d_vert_t){ 0, y1, um, um, c, 0, 0, 0 };
    q[3] = q[2]; q[4] = q[1];
    q[5] = (gpu3d_vert_t){ x1, y1, um, um, c, 0, 0, 0 };
    return desenho(NULL, q, 6, mascara & 3u, NULL);
}

static int i9_textura(int vaga, int larg, int alt, const uint32_t *argb)
{
    if (!ligado) return G3D_FALHA;
    if (vaga < 0 || vaga >= GPU3D_TEXTURAS) return G3D_NAO;
    g3d_buffer_t *b = &b_tex[vaga];
    if (!argb && !b->usado) return G3D_OK;
    if (argb && (larg < 1 || alt < 1 || larg > MAX_LADO || alt > MAX_LADO)) return G3D_NAO;
    if (i9_esperar(0, GPU3D_PRAZO_MS) != G3D_OK) return G3D_FALHA;   /* que la GPU no la esté leyendo */
    if (b->usado) g3d_buffer_soltar(&arena, b);
    if (!argb) return G3D_OK;
    uint32_t passo = ((uint32_t)larg * 4u + 63u) & ~63u;
    if (!g3d_buffer_criar(&arena, b, G3D_BUF_TEXTURA, (uint64_t)passo * (uint32_t)((alt + 3) & ~3), 4096)) return G3D_NAO;
    b->passo = passo; b->larg = larg; b->alt = alt; b->bpp = 32;
    tex_niveis[vaga] = 1; tex_y[vaga] = false;
    for (int y = 0; y < alt; y++) {
        volatile uint32_t *d = (volatile uint32_t *)(b->cpu + (uint32_t)y * passo);
        for (int x = 0; x < larg; x++) d[x] = argb[y * larg + x];
    }
    wc_vaciar();
    return G3D_OK;
}

/* D453: un nivel de mipmap en su sitio (i9_mip_pos) del hueco ya creado. */
static void escrever_nivel(const g3d_buffer_t *b, bool ytile, int x0, int y0, int w, int h, const uint32_t *argb)
{
    if (ytile) {                        /* texel a texel en su sitio del mosaico Y */
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++)
                *(volatile uint32_t *)(b->cpu + i9_ytile_off((uint32_t)(x0 + x) * 4u, (uint32_t)(y0 + y), b->passo)) = argb[y * w + x];
        wc_vaciar();
        return;
    }
    for (int y = 0; y < h; y++) {
        volatile uint32_t *d = (volatile uint32_t *)(b->cpu + (uint32_t)(y0 + y) * b->passo) + x0;
        for (int x = 0; x < w; x++) d[x] = argb[y * w + x];
    }
    wc_vaciar();
}

static int i9_desenhar(const gpu3d_estado_t *e, const gpu3d_vert_t *v, int n, g3d_vala_t *vala)
{
    if (!ligado || !alvo_l) return G3D_FALHA;
    if (n <= 0) return G3D_OK;
    for (int i = 0; i < n; i += GPU3D_MAX_VERT) {
        int k = n - i < GPU3D_MAX_VERT ? n - i : GPU3D_MAX_VERT;
        int r = desenho(e, v + i, k, 0, vala);
        if (r != G3D_OK) return r;
    }
    return G3D_OK;
}

static int i9_ler(uint32_t *dest, int passo_px)
{
    if (!ligado || !alvo_l) return G3D_FALHA;
    uint64_t t0 = prazo_tsc();
    if (i9_esperar(0, GPU3D_PRAZO_MS) != G3D_OK) return G3D_FALHA;
    uint64_t t1 = prazo_tsc();
    t_valla += t1 - t0;
    const uint8_t *s = (const uint8_t *)b_cor.cpu;
    uint32_t fila = (uint32_t)alvo_l * 4u;
    if (cor_ram && (flags & FL_CLFLUSH)) cache_vaciar(s, alvo_passo * (uint32_t)alvo_a);
    /* En RAM con caché, SSE2 copia a la velocidad de la memoria; por la apertura (UC/WC) de 16 en 16
     * bytes (D402). Una sola entrada en la FPU para todo el cuadro. */
    if ((fpu_recursos() & CPU_SSE2) && fpu_kernel_inicio()) {
        if ((uint32_t)passo_px * 4u == alvo_passo && fila == alvo_passo) {
            sse2_copiar(dest, s, alvo_passo * (uint32_t)alvo_a);
            sse2_escurecer(dest, (unsigned)(alvo_l * alvo_a), 0, 0x00FFFFFFu);
        } else
            for (int y = 0; y < alvo_a; y++) {
                uint32_t *d = dest + y * passo_px;
                sse2_copiar(d, s + (uint32_t)y * alvo_passo, fila);
                sse2_escurecer(d, (unsigned)alvo_l, 0, 0x00FFFFFFu);
            }
        fpu_kernel_fim();
    } else
        for (int y = 0; y < alvo_a; y++) {
            uint32_t *d = dest + y * passo_px;
            const volatile uint32_t *f = (const volatile uint32_t *)(s + (uint32_t)y * alvo_passo);
            for (int x = 0; x < alvo_l; x++) d[x] = f[x] & 0x00FFFFFFu;
        }
    t_copia += prazo_tsc() - t1;
    n_ler++;
    if (n_ler % 600 == 0) resumo("cada 600 cuadros");
    return G3D_OK;
}

static void i9_desligar(void)
{
    if (!mmio) return;
    if (ligado && (pendente || lote_aberto)) (void)lote_terminar(250, "al apagar");
    lote_aberto = false;
    ligado = false;
    pendente = false;
    if (n_ler) resumo("al apagar");
    soltar_tudo();                     /* tras un cuelgue, ram_soltar no devuelve las páginas del destino */
    /* Solo el índice 4 (el byte 0): el 5 es del volcado del 2D (D434), que puede estar en marcha. */
    if (pat_tocado && !travado) { wr(PAT_HI, (rd(PAT_HI) & ~0xFFu) | (pat_hi_antes & 0xFFu)); pat_tocado = false; }
    if (freq_pode) intel_rps_soltar(INTEL_RPS_3D);
    freq_pode = false;
    fw_soltar();
    klog("[GPU3D] intel9: apagado (el RCS queda sin trabajo; la pantalla sigue como estaba)\n");
}

static void i9_detalle(const char *fase)
{
    klog("[GPU3D] intel9 detalle (%s): ultima etapa del encendido: %d de 6 (%s); %s; tandas %u, marca %x de %x; zona %x (%u KB), "
         "libre %u KB; banderas %x; %s; destino %s\n", fase, etapa, nome_etapa(etapa), ligado ? "en marcha" : "parado", envios,
         zona_uc ? hws()[MARCA_DWORD] : 0, marca, zona_gfx, zona_bytes >> 10, (uint32_t)(g3d_arena_livre(&arena) >> 10), flags,
         com_wc ? "zona en WC" : "zona sin cache (UC)", cor_ram ? "en RAM con cache" : "en la zona");
    if (!mmio || etapa < 2) return;
    log_motor(fase);
    {
        char fr[200];
        intel_rps_texto(fr, sizeof fr);
        klog("[GPU3D] intel9 detalle (%s): %s; RP_CONTROL %x%s\n", fase, fr, rd(RP_CONTROL), freq_pode ? "" : " (el 3D no la pide)");
    }
    klog("[GPU3D] intel9 detalle (%s): forcewake render %x GT %x; PAT %x = %x, %x = %x; MOCS 0/1 %x %x, L3CC %x; L3CNTLREG %x\n", fase,
         rd(FW_RENDER_ACK), rd(FW_GT_ACK), intel_pat3_reg(gen), rd(intel_pat3_reg(gen)), PAT_HI, rd(PAT_HI), rd(0xC800u),
         rd(0xC804u), rd(0xB020u), rd(0x7034u));
    if (n_ler && hw_tsc_khz())
        klog("[GPU3D] intel9 detalle (%s): %u lecturas del cuadro, %u lotes (%u llenos), %u dibujos, %u cambios de estado; "
             "espera al leer %u ms, copia %u ms, montar %u ms, espera total %u ms\n", fase, n_ler, n_lotes, n_cheios, n_desenhos,
             n_mudancas, us(t_valla) / 1000u, us(t_copia) / 1000u, us(t_montar) / 1000u, us(t_espera_lote) / 1000u);
    klog("[GPU3D] intel9 detalle (%s): v3 (D457): %u dibujos desde buferes, %u subidas, %u sombreadores de vertices; monton %u KB "
         "(%u usados en %u buferes)%s\n", fase, v3_desenhos, v3_subidas, n_kv_gerados, vb_ram ? vbm.total >> 10 : 0, vbm.usado >> 10,
         vbm.n, (flags & FL_SEM_V3) ? "; apagado con la bandera 100000" : "");
    if (mot_ultimo[0]) klog("[GPU3D] intel9 detalle (%s): ultimo fallo: %s\n", fase, mot_ultimo);
}

static void i9_memoria(uint32_t *total_kb, uint32_t *livre_kb)
{
    *total_kb = zona_bytes > Z_ARENA ? (zona_bytes - Z_ARENA) >> 10 : 0;
    *livre_kb = ligado ? (uint32_t)(g3d_arena_livre(&arena) >> 10) : 0;
}

static void i9_vistas(g3d_vista_t *c, g3d_vista_t *z)
{
    (void)z;
    if (!ligado || !alvo_l || !b_cor.usado || !b_cor.cpu) return;
    *c = (g3d_vista_t){ b_cor.cpu, alvo_passo, alvo_l, alvo_a, 32, 0, 0, false };
}

/* ------------------------------------------------------------------ gpu3d v2 (D453) */

/* El sombreador de la combinación «c»: de la caché o generado en un hueco libre (ni del lote abierto
 * ni, si hace falta, de la tanda que está en la GPU, que se espera). -1: no cabe o no se puede. */
static int k2_obter(const i9_ps2_t *c)
{
    int livre = -1;
    for (uint32_t i = 0; i < K2_N; i++) {
        if (k2[i].valido && !memcmp(&k2[i].chave, c, sizeof *c)) { k2[i].lru = ++k2_relogio; return (int)i; }
        if (k2[i].uso == lote.c.marca && k2[i].valido) continue;          /* lo usa el lote abierto */
        if (livre < 0 || !k2[i].valido || (k2[livre].valido && k2[i].lru < k2[livre].lru)) livre = (int)i;
    }
    if (livre < 0) return -1;
    k2_t *k = &k2[livre];
    if (k->valido && (int32_t)(k->uso - hws()[MARCA_DWORD]) > 0 && !esperar_tanda(marca, GPU3D_PRAZO_MS, "sombreador de v2"))
        return -1;                                                         /* lo usaba la tanda en la GPU: que acabe */
    static uint32_t ps[I9_PS2_MAX_INST * 4];
    int n = i9_ps2_gerar(ps, I9_PS2_MAX_INST, gen, c);
    if (n <= 0 || (uint32_t)n * 16u > I9_KERNEL2_BYTES - 256u) { k->valido = false; return -1; }
    volatile uint32_t *kd = zw(Z_K2 + (uint32_t)livre * I9_KERNEL2_BYTES);
    for (int i = 0; i < n * 4; i++) kd[i] = ps[i];
    for (int i = n * 4; i < (int)(I9_KERNEL2_BYTES / 4u); i += 4) { kd[i] = i9eu_opcode(I9_OP_NOP, gen); kd[i + 1] = kd[i + 2] = kd[i + 3] = 0; }
    wc_vaciar();
    k->valido = true; k->chave = *c; k->lru = ++k2_relogio; k->uso = 0;
    if (n_k2_gerados++ < 8) klog("[GPU3D] intel9 v2: sombreador %u (%d unidades): %d instrucciones en el hueco %d\n", n_k2_gerados, c->n, n, livre);
    return livre;
}

static bool unidade_ok(const gpu3d_unidade_t *u)
{
    if (u->tipo != G3D_TEX_2D || u->rgb_func > G3D_CB_DOT3_RGBA || u->alfa_func > G3D_CB_SUBTRAIR) return false;
    if ((u->rgb_escala != 1 && u->rgb_escala != 2 && u->rgb_escala != 4) || (u->alfa_escala != 1 && u->alfa_escala != 2 && u->alfa_escala != 4))
        return false;
    for (int k = 0; k < 3; k++)
        if (u->rgb_fonte[k] > G3D_FT_ANTERIOR || u->alfa_fonte[k] > G3D_FT_ANTERIOR || u->rgb_opd[k] > G3D_OPD_UM_MENOS_ALFA ||
            (u->alfa_opd[k] != G3D_OPD_ALFA && u->alfa_opd[k] != G3D_OPD_UM_MENOS_ALFA)) return false;
    return u->vaga < GPU3D_TEXTURAS && b_tex[u->vaga].usado;
}

/* El estado v2 en el de la GPU (estado de píxeles del dibujo y la combinación del sombreador); también para v3 (D457). */
static int traduzir2(const gpu3d_estado2_t *e, i9_desenho_t *dp, i9_ps2_t *pp)
{
    i9_desenho_t d;
    i9_ps2_t ps;
    memset(&d, 0, sizeof d);
    memset(&ps, 0, sizeof ps);
    d.formato = I9_FMT_V2;
    d.mascara = e->mascara_cor & 15u;
    d.f_orig = 1; d.f_dest = 0x11;
    d.z_teste = (e->flags & G3D2_Z_TESTE) != 0;
    d.z_escrita = (e->flags & G3D2_Z_ESCRITA) != 0;
    if (d.z_escrita && !d.z_teste) { d.z_teste = true; d.func_z = 0; }      /* escribir sin probar: siempre */
    else if (d.z_teste) { int f = i9_cmp(e->func_z); if (f < 0) return G3D_NAO; d.func_z = (uint8_t)f; }
    if (e->flags & G3D2_ESTENCIL) {
        if (!b_est.usado) return G3D_NAO;
        int f = i9_cmp(e->est_func), a = i9_op_est(e->est_falha), b = i9_op_est(e->est_zfalha), c = i9_op_est(e->est_zpassa);
        if (f < 0 || a < 0 || b < 0 || c < 0) return G3D_NAO;
        d.est = true; d.est_func = (uint8_t)f; d.est_falha = (uint8_t)a; d.est_zfalha = (uint8_t)b; d.est_zpassa = (uint8_t)c;
        d.est_ref = e->est_ref; d.est_mascara = e->est_mascara; d.est_escrita = e->est_escrita;
    }
    if (e->flags & G3D2_ALFA_TESTE) {
        int f = i9_cmp(e->func_alfa);
        if (f < 0) return G3D_NAO;
        d.alfa = true; d.alfa_func = (uint8_t)f; d.alfa_ref = e->ref_alfa;
    }
    if (e->flags & G3D2_TIJERA) { d.tijera = true; d.tx = e->tijera_x; d.ty = e->tijera_y; d.tl = e->tijera_l; d.ta = e->tijera_a; }
    if (e->flags & G3D2_LOGICA) {
        int o = i9_logica(e->op_logica);
        if (o < 0) return G3D_NAO;
        d.logica = true; d.op_logica = (uint8_t)o;
    } else if (e->flags & G3D2_MEZCLA) {
        int fo = i9_fator2(e->mez_orig_rgb), fd = i9_fator2(e->mez_dest_rgb), fao = i9_fator2(e->mez_orig_alfa),
            fad = i9_fator2(e->mez_dest_alfa), q = i9_eq(e->mez_eq_rgb), qa = i9_eq(e->mez_eq_alfa);
        if (fo < 0 || fd < 0 || fao < 0 || fad < 0 || q < 0 || qa < 0 || e->mez_dest_rgb == G3D_F_ALFA_SATURADO ||
            e->mez_dest_alfa == G3D_F_ALFA_SATURADO) return G3D_NAO;
        d.mezcla = true;
        d.f_orig = (uint8_t)fo; d.f_dest = (uint8_t)fd; d.eq = (uint8_t)q;
        d.alfa_sep = fao != fo || fad != fd || qa != q;
        if (d.alfa_sep) { d.f_orig_a = (uint8_t)fao; d.f_dest_a = (uint8_t)fad; d.eq_a = (uint8_t)qa; }
    }
    /* Las unidades: cada activa con su textura y su combinación; el sombreador sale de la combinación. */
    int ult = -1, nu = e->unidades > 4 ? 4 : e->unidades;
    for (int u = 0; u < nu; u++) {
        const gpu3d_unidade_t *un = &e->u[u];
        if (!un->ativa) continue;
        if (!unidade_ok(un)) return G3D_NAO;
        const g3d_buffer_t *x = &b_tex[un->vaga];
        i9_tex_t *t = &d.t[u];
        t->tex = (uint32_t)x->gpu; t->passo = x->passo; t->larg = x->larg; t->alt = x->alt;
        t->mag_linear = (un->filtro & G3D_FIL_LINEAR_MAG) != 0; t->min_linear = (un->filtro & G3D_FIL_LINEAR_MIN) != 0;
        t->env_s = un->env_s <= G3D_ENV_ESPELHO ? un->env_s : G3D_ENV_BORDE;
        t->env_t = un->env_t <= G3D_ENV_ESPELHO ? un->env_t : G3D_ENV_BORDE;
        t->niveis = tex_niveis[un->vaga];
        t->mosaico_y = tex_y[un->vaga];
        if (t->niveis > 1 && (flags & FL_MIP_SESGO)) t->lod_bias = -1;
        t->mip = (un->filtro & G3D_FIL_MIP_LINEAR) ? 2 : (un->filtro & G3D_FIL_MIP_VECINO) ? 1 : 0;
        i9_unid_t *pu = &ps.u[u];
        pu->ativa = 1; pu->rgb_func = un->rgb_func; pu->alfa_func = un->alfa_func;
        pu->rgb_escala = un->rgb_escala; pu->alfa_escala = un->alfa_escala; pu->constante = un->constante;
        for (int k = 0; k < 3; k++) {
            pu->rgb_fonte[k] = un->rgb_fonte[k]; pu->rgb_opd[k] = un->rgb_opd[k];
            pu->alfa_fonte[k] = un->alfa_fonte[k]; pu->alfa_opd[k] = un->alfa_opd[k];
        }
        ult = u;
    }
    d.n_tex = (uint8_t)(ult + 1);
    for (int u = 0; u < ult; u++) if (!e->u[u].ativa) d.t[u] = d.t[ult];   /* algo válido en los huecos que no se muestrean */
    ps.n = (uint8_t)(ult + 1);
    if (e->flags & G3D2_NEBLINA) { ps.neblina = true; ps.cor_neblina = e->cor_neblina & 0xFFFFFFu; }
    *dp = d; *pp = ps;
    return G3D_OK;
}

static int i9_desenhar2(const gpu3d_estado2_t *e, const gpu3d_vert2_t *v, int n, g3d_vala_t *vala)
{
    if (!ligado || !alvo_l) return G3D_FALHA;
    if (n <= 0) return G3D_OK;
    uint64_t t0 = prazo_tsc();
    i9_desenho_t d;
    i9_ps2_t ps;
    int r = traduzir2(e, &d, &ps);
    if (r != G3D_OK) return r;
    if (!lote_abrir()) return G3D_FALHA;
    int k = k2_obter(&ps);
    if (k < 0) return ligado ? G3D_NAO : G3D_FALHA;
    d.k_ps = Z_K2 + (uint32_t)k * I9_KERNEL2_BYTES;
    return desenho_d(&d, v, n, I9_VERT2_BYTES, vala, t0);
}

static int i9_textura2(const gpu3d_textura2_t *t)
{
    if (!ligado) return G3D_FALHA;
    if (!t->dados) return i9_textura((int)t->vaga, 0, 0, NULL);
    if (t->formato != G3D_FMT_ARGB8888 || t->tipo != G3D_TEX_2D || t->face || t->vaga >= GPU3D_TEXTURAS) return G3D_NAO;
    int nv = t->niveis ? (int)t->niveis : 1, w = (int)t->larg, h = (int)t->alt;
    g3d_buffer_t *b = &b_tex[t->vaga];
    if (t->nivel == 0) {
        if (nv == 1) return i9_textura((int)t->vaga, w, h, (const uint32_t *)t->dados);
        if (nv > 12 || w < 1 || h < 1 || w > MAX_LADO || h > MAX_LADO) return G3D_NAO;
        int tw, th;
        i9_mip_tamanho(w, h, nv, &tw, &th);
        if (i9_esperar(0, GPU3D_PRAZO_MS) != G3D_OK) return G3D_FALHA;   /* que la GPU no lo esté leyendo */
        if (b->usado) g3d_buffer_soltar(&arena, b);
        bool yt = !(flags & FL_MIP_LINEAR);
        uint32_t passo = yt ? ((uint32_t)tw * 4u + 127u) & ~127u : ((uint32_t)tw * 4u + 63u) & ~63u;
        uint32_t filas = yt ? ((uint32_t)th + 31u) & ~31u : (uint32_t)th;
        if (!g3d_buffer_criar(&arena, b, G3D_BUF_TEXTURA, (uint64_t)passo * filas, 4096)) return G3D_NAO;
        b->passo = passo; b->larg = w; b->alt = h; b->bpp = 32;
        tex_niveis[t->vaga] = (uint8_t)nv; tex_y[t->vaga] = yt;
        if (flags & FL_MIP_PADRAO) {
            uint32_t pw = passo / 4u;
            for (uint32_t y = 0; y < filas; y++)
                for (uint32_t x = 0; x < pw; x++)
                    *(volatile uint32_t *)(b->cpu + (yt ? i9_ytile_off(x * 4u, y, passo) : y * passo + x * 4u)) =
                        0xFF000000u | (x & 0xFFu) << 16 | (y & 0xFFu) << 8 | 0x5Au;
            wc_vaciar();
            klog("[GPU3D] intel9 v2: textura de %dx%d con %d niveles en %s: patron de coordenadas (%ux%u, paso %u)\n", w, h, nv,
                 yt ? "mosaico Y" : "lineal", pw, filas, passo);
            return G3D_OK;
        }
        escrever_nivel(b, yt, 0, 0, w, h, (const uint32_t *)t->dados);
        return G3D_OK;
    }
    int l = (int)t->nivel, ew = b->larg >> l, eh = b->alt >> l;
    if (!b->usado || l >= tex_niveis[t->vaga] || w != (ew < 1 ? 1 : ew) || h != (eh < 1 ? 1 : eh)) return G3D_NAO;
    if (i9_esperar(0, GPU3D_PRAZO_MS) != G3D_OK) return G3D_FALHA;
    if (flags & FL_MIP_PADRAO) return G3D_OK;   /* diagnóstico: se deja el patrón */
    int x0, y0;
    i9_mip_pos(b->larg, b->alt, l, &x0, &y0);
    escrever_nivel(b, tex_y[t->vaga], x0, y0, w, h, (const uint32_t *)t->dados);
    return G3D_OK;
}

static int i9_limpar2(const gpu3d_limpar2_t *l)
{
    if (!ligado || !alvo_l) return G3D_FALHA;
    uint32_t m = l->mascara & (G3D_LIMPA_COR | G3D_LIMPA_Z | G3D_LIMPA_EST);
    if (!m) return G3D_OK;
    if ((m & G3D_LIMPA_EST) && !b_est.usado) return G3D_NAO;
    i9_desenho_t d;
    memset(&d, 0, sizeof d);
    d.formato = I9_FMT_V1; d.k_ps = Z_K_COR;
    d.f_orig = 1; d.f_dest = 0x11;
    d.mascara = (m & G3D_LIMPA_COR) ? (l->mascara_cor & 15u) : 0;
    d.z_teste = d.z_escrita = (m & G3D_LIMPA_Z) != 0;                     /* siempre, con el valor dado */
    if (m & G3D_LIMPA_EST) {
        d.est = true; d.est_func = 0; d.est_falha = d.est_zfalha = d.est_zpassa = 2;   /* ALWAYS, REPLACE */
        d.est_ref = (uint8_t)l->estencil; d.est_mascara = 0xFF; d.est_escrita = l->mascara_est;
    }
    if (l->tijera) { d.tijera = true; d.tx = l->tx; d.ty = l->ty; d.tl = l->tl; d.ta = l->ta; }
    uint32_t zf = g3d_float_de_fixo(l->z >= 0xFFFFFFu ? 1 << 24 : (int32_t)l->z, 24), um = 0x3F800000u;
    uint32_t x1 = g3d_float_de_fixo(alvo_l, 0), y1 = g3d_float_de_fixo(alvo_a, 0), c = l->cor;
    gpu3d_vert_t q[6];
    q[0] = (gpu3d_vert_t){ 0, 0, zf, um, c, 0, 0, 0 };
    q[1] = (gpu3d_vert_t){ x1, 0, zf, um, c, 0, 0, 0 };
    q[2] = (gpu3d_vert_t){ 0, y1, zf, um, c, 0, 0, 0 };
    q[3] = q[2]; q[4] = q[1];
    q[5] = (gpu3d_vert_t){ x1, y1, zf, um, c, 0, 0, 0 };
    return desenho_d(&d, q, 6, I9_VERT_BYTES, NULL, prazo_tsc());
}

/* ------------------------------------------------------------------ gpu3d v3 (D457)
 *
 * Los búferes residentes viven en un montón de RAM con caché (VB_GFX en la PPGTT, índice 4 del PAT: WB en la LLC,
 * coherente con la CPU, como el destino de D448): la CPU sube con memcpy y el VF los lee por la LLC (MOCS 1, la PTE).
 * El sombreador de vértices sale de la transformación (i9_vs_gerar, caché de KV_N huecos como la de v2) y las matrices
 * van por constantes empujadas en cada lote (solo cuando cambian). El recorte, la división por w, la vista y el descarte
 * de caras, por la GPU (3DSTATE_CLIP, 3DSTATE_SF, 3DSTATE_RASTER). Los píxeles, como v2 (traduzir2, el mismo
 * sombreador de la combinación). Seguridad: el montón está dentro de la PPGTT del contexto (no toca la GGTT ni la
 * pantalla); la capa ya comprobó que cada atributo cabe en lo subido, y el VF no lee fuera del tamaño de cada búfer. */

/* El sombreador de vértices de la clave «c»: de la caché o generado en un hueco libre (como k2_obter). */
static int kv_obter(const i9_vs_t *c)
{
    int livre = -1;
    for (uint32_t i = 0; i < KV_N; i++) {
        if (kv[i].valido && !memcmp(&kv[i].chave, c, sizeof *c)) { kv[i].lru = ++kv_relogio; return (int)i; }
        if (kv[i].uso == lote.c.marca && kv[i].valido) continue;          /* lo usa el lote abierto */
        if (livre < 0 || !kv[i].valido || (kv[livre].valido && kv[i].lru < kv[livre].lru)) livre = (int)i;
    }
    if (livre < 0) return -1;
    kv_t *k = &kv[livre];
    if (k->valido && (int32_t)(k->uso - hws()[MARCA_DWORD]) > 0 && !esperar_tanda(marca, GPU3D_PRAZO_MS, "sombreador de vertices"))
        return -1;
    static uint32_t vs[I9_VS_MAX_INST * 4];
    int n = i9_vs_gerar(vs, I9_VS_MAX_INST, gen, c);
    if (n <= 0 || (uint32_t)n * 16u > I9_KERNELV_BYTES - 256u) { k->valido = false; return -1; }
    volatile uint32_t *kd = zw(Z_KV + (uint32_t)livre * I9_KERNELV_BYTES);
    for (int i = 0; i < n * 4; i++) kd[i] = vs[i];
    for (int i = n * 4; i < (int)(I9_KERNELV_BYTES / 4u); i += 4) { kd[i] = i9eu_opcode(I9_OP_NOP, gen); kd[i + 1] = kd[i + 2] = kd[i + 3] = 0; }
    wc_vaciar();
    k->valido = true; k->chave = *c; k->lru = ++kv_relogio; k->uso = 0;
    if (n_kv_gerados++ < 8)
        klog("[GPU3D] intel9 v3: sombreador de vertices %u (%u unidades, color %u, niebla %u, matrices %x): %d instrucciones, "
             "%d constantes, %d atributos, en el hueco %d\n", n_kv_gerados, c->n_unid, c->cor, c->neblina, c->tex_mat, n, i9_vs_nc(c),
             i9_vs_n_atr(c), livre);
    return livre;
}

/* El montón: lo más grande que haya seguido de 16 a 1 MB (la RAM baja; si no hay, sin v3 para este búfer). */
static bool vb_criar(void)
{
    for (uint32_t t = 16u << 20; t >= 1u << 20; t >>= 1) {
        if (t > VB_PT_MAX * (2u << 20)) continue;
        /* sin borrar: la capa solo deja dibujar lo ya subido a cada búfer (y la GPU solo lee de aquí) */
        if (ram_mapear(VB_GFX, vb_pt, VB_PT_MAX, t, &vb_ram, &vb_ram_pag, false)) {
            g3d_vbmonte_iniciar(&vbm, t);
            if (!v3_montes++) klog("[GPU3D] intel9 v3: monton de vertices de %u KB en RAM con cache (fisica %x, grafica %x)\n",
                                   t >> 10, vb_ram, VB_GFX);
            return true;
        }
    }
    if (!v3_sem++) klog("[GPU3D] intel9 v3: sin RAM seguida para el monton de vertices (1 MB)\n");
    return false;
}

static int i9_vbuf(int vaga, uint32_t bytes, uint32_t desloc, const void *dados, uint32_t n)
{
    if (!ligado) return G3D_FALHA;
    if (vaga < 0 || vaga >= GPU3D_VBUFS) return G3D_NAO;
    if (!dados && !bytes && !vbm.tam[vaga]) return G3D_OK;
    /* Un lote abierto o en la GPU puede estar leyendo ese búfer (o el sitio que va a ocupar). */
    if (i9_esperar(0, GPU3D_PRAZO_MS) != G3D_OK) return G3D_FALHA;
    if (dados) {
        if (!vb_ram || !vbm.tam[vaga] || desloc > vbm.tam[vaga] || n > vbm.tam[vaga] - desloc) return G3D_NAO;
        uint8_t *d = (uint8_t *)vb_ram + vbm.off[vaga] + desloc;
        memcpy(d, dados, n);
        if (flags & FL_CLFLUSH) cache_vaciar(d, n);   /* diagnóstico: por si la LLC no fuera coherente */
        v3_subidas++;
        return G3D_OK;
    }
    g3d_vbmonte_soltar(&vbm, vaga);
    if (!bytes) return G3D_OK;   /* el montón se queda hasta apagar el 3D (crearlo cuesta: 16 MB en la PPGTT) */
    if (!vb_ram && !vb_criar()) return G3D_NAO;
    return g3d_vbmonte_criar(&vbm, vaga, bytes) ? G3D_OK : G3D_NAO;
}

/* Un atributo de un búfer residente para el VF: dirección del vértice 0, lo que se puede leer, paso y formato. */
static bool atrib3(const gpu3d_atrib_t *a, i9_vb3_t *o)
{
    if (a->vaga >= GPU3D_VBUFS || !vbm.tam[a->vaga] || (a->passo & 3u) || (a->desloc & 3u) || !a->passo ||
        a->desloc >= vbm.tam[a->vaga]) return false;
    o->dir = VB_GFX + vbm.off[a->vaga] + a->desloc;
    o->bytes = vbm.tam[a->vaga] - a->desloc;
    o->passo = a->passo;
    switch (a->formato) {
    case G3D_AT_FLOAT2:  o->fmt = I9_VF_RG32F;   o->comps = 2; break;
    case G3D_AT_FLOAT3:  o->fmt = I9_VF_RGB32F;  o->comps = 3; break;
    case G3D_AT_FLOAT4:  o->fmt = I9_VF_RGBA32F; o->comps = 4; break;
    case G3D_AT_UBYTE4N: o->fmt = I9_VF_RGBA8;   o->comps = 4; break;
    default: return false;
    }
    return true;
}

static int i9_desenhar3(const g3d_desenho3_t *g, g3d_vala_t *vala)
{
    if (!ligado || !alvo_l) return G3D_FALHA;
    const gpu3d_estado2_t *e = g->e;
    const gpu3d_transf_t *t = g->t;
    const gpu3d_desenho3_t *dd = &g->d;
    if ((t->flags & G3D3_LUZ) || ((t->flags & G3D3_NEBLINA) && t->neb_modo != G3D_NEB_LINEAR) || !vb_ram || dd->modo > G3D_PRIM_ABANICO)
        return G3D_NAO;
    if (dd->n < 3) return G3D_OK;
    uint64_t t0 = prazo_tsc();
    i9_desenho_t d;
    i9_ps2_t ps;
    int r = traduzir2(e, &d, &ps);
    if (r != G3D_OK) return r;
    /* La clave del sombreador de vértices: de dónde sale cada atributo y qué se hace con él. */
    i9_vs_t k;
    memset(&k, 0, sizeof k);
    k.n_unid = ps.n;
    k.cor = dd->cor_a.formato == G3D_AT_NADA ? 0 : dd->cor_a.formato == G3D_AT_FLOAT4 ? 2 : 1;
    if (k.cor == 1 && dd->cor_a.formato != G3D_AT_UBYTE4N) return G3D_NAO;
    for (int u = 0; u < k.n_unid; u++) {
        k.tc[u] = dd->tc[u].formato != G3D_AT_NADA;
        if (t->flags & G3D3_TEX_MAT(u)) k.tex_mat |= (uint8_t)(1u << u);
    }
    if (t->flags & G3D3_NEBLINA) k.neblina = (t->flags & G3D3_NEB_MEZCLADA) ? 2 : 1;
    i9_geo3_t geo;
    memset(&geo, 0, sizeof geo);
    geo.nc = (uint8_t)i9_vs_nc(&k); geo.n_atr = (uint8_t)i9_vs_n_atr(&k);
    int na = 0;
    if (!atrib3(&dd->pos, &geo.vb[na++]) || dd->pos.formato == G3D_AT_UBYTE4N) return G3D_NAO;
    if (k.cor && !atrib3(&dd->cor_a, &geo.vb[na++])) return G3D_NAO;
    for (int u = 0; u < k.n_unid; u++)
        if (k.tc[u] && (!atrib3(&dd->tc[u], &geo.vb[na++]) || dd->tc[u].formato == G3D_AT_UBYTE4N)) return G3D_NAO;
    if (na != geo.n_atr) return G3D_NAO;
    if (dd->indices != GPU3D_SEM_INDICES) {
        if (dd->indices >= GPU3D_VBUFS || !vbm.tam[dd->indices]) return G3D_NAO;
        geo.ib = VB_GFX + vbm.off[dd->indices];
        geo.ib_bytes = g->bytes_indices < vbm.tam[dd->indices] ? g->bytes_indices : vbm.tam[dd->indices];
        if (!geo.ib_bytes) return G3D_NAO;
    }
    static uint32_t cst[16 * 8];
    i9_vs_dados_t vd = { t->mvp, t->mv, (const uint32_t (*)[16])t->tex, t->cor, (const uint32_t (*)[4])t->tc_const, t->neb_a, t->neb_b,
                         t->neb_cor };
    i9_vs_consts(cst, &k, &vd);
    geo.cst = cst;
    geo.topologia = dd->modo == G3D_PRIM_TIRA ? 5 : dd->modo == G3D_PRIM_ABANICO ? 6 : 4;
    /* El descarte por el signo del área en la pantalla con la y hacia abajo: > 0 es el sentido de las agujas del
     * reloj (CW) en la pantalla, que es como la GPU llama a las caras (FrontWinding, como anv con Vulkan). */
    bool inv = (flags & FL_V3_DESC_INV) != 0;
    geo.frente = 0;
    geo.cull = t->descarte == G3D_DESC_POS ? (inv ? 3 : 2) : t->descarte == G3D_DESC_NEG ? (inv ? 2 : 3) : 1;
    geo.mocs = (uint8_t)MOCS_RAM;
    geo.vp[0] = t->vp_escala[0]; geo.vp[1] = t->vp_escala[1]; geo.vp[2] = t->vp_escala[2];
    geo.vp[3] = t->vp_desloc[0]; geo.vp[4] = t->vp_desloc[1]; geo.vp[5] = t->vp_desloc[2];
    geo.primeiro = dd->primeiro; geo.n = dd->n;
    uint64_t e0 = t_espera_lote;
    if (!lote_abrir()) return G3D_FALHA;
    if (!i9_lote_cabe(&lote)) {
        n_cheios++;
        if (!lote_enviar("lote lleno") || !lote_abrir()) { ligado = false; return G3D_FALHA; }
    }
    int k2i = k2_obter(&ps);
    if (k2i < 0) return ligado ? G3D_NAO : G3D_FALHA;
    int kvi = kv_obter(&k);
    if (kvi < 0) return ligado ? G3D_NAO : G3D_FALHA;
    d.k_ps = Z_K2 + (uint32_t)k2i * I9_KERNEL2_BYTES;
    geo.k_vs = Z_KV + (uint32_t)kvi * I9_KERNELV_BYTES;
    if (i9_lote_desenho3(&lote, &d, &geo) != 0) {
        snformatar(mot_ultimo, sizeof mot_ultimo, "el dibujo v3 no cabe en el lote (error interno)");
        klog("[GPU3D] intel9: %s\n", mot_ultimo);
        lote_aberto = false;
        return G3D_FALHA;
    }
    k2[k2i].uso = lote.c.marca;
    kv[kvi].uso = lote.c.marca;
    if (vala) *vala = lote.c.marca;
    v3_desenhos++;
    t_montar += prazo_tsc() - t0 - (t_espera_lote - e0);
    if (flags & FL_V3_SINC) {
        bool ok = lote_terminar(500, "dibujo v3 sincronizado");
        if (v3_desenhos <= 8)
            klog("[GPU3D] intel9 v3 sinc: dibujo %u de %u vertices (%u atributos, %s, topologia %u, descarte %u): %s\n", v3_desenhos,
                 dd->n, geo.n_atr, geo.ib ? "con indices" : "sin indices", geo.topologia, geo.cull, ok ? "bien" : "CUELGA");
        if (!ok) { i9_detalle("al dibujar (v3)"); return G3D_FALHA; }
    } else if ((flags & FL_SEM_LOTES) && !lote_enviar("un lote por dibujo")) { ligado = false; return G3D_FALHA; }
    return G3D_OK;
}

/* Lo que se anuncia: lo que este driver sabe hacer en Gen9 y, de eso, solo lo que pasa las pruebas en la
 * máquina (g3d_v2_provas.c). Sin cubos, anisótropo, DXT, coordenadas proyectivas ni destino en textura
 * todavía (D453). */
static void i9_capacidades(gpu3d_capas_t *c)
{
    if (!ligado || (flags & FL_SEM_V2)) return;
    uint32_t cand = G3D_CAP_V2 | G3D_CAP_COMBINAR | G3D_CAP_DOT3 | G3D_CAP_ALFA_TESTE | G3D_CAP_ESTENCIL | G3D_CAP_LIMPAR2 |
                    G3D_CAP_TIJERA | G3D_CAP_MASCARAS | G3D_CAP_Z_FUNCS | G3D_CAP_MEZCLA_EXT | G3D_CAP_MEZCLA_SEP |
                    G3D_CAP_MEZCLA_EQ | G3D_CAP_LOGICA | G3D_CAP_NEBLINA | G3D_CAP_TEX_ESPELHO | G3D_CAP_TEX_NPOT |
                    G3D_CAP_TEX_MIPMAP | G3D_CAP_TEX_NPOT_MIP;
    uint32_t un = 4, fmts = 1u << G3D_FMT_ARGB8888;
    static const g3d_v2_ops_t ops = { i9_alvo, i9_limpar, i9_textura, i9_textura2, i9_desenhar2, i9_limpar2, i9_ler, NULL };
    uint32_t ok = g3d_v2_provar(&ops, "intel9", cand, &un, &fmts);
    klog("[GPU3D] intel9 v2: se anuncian %x de %x (%u unidades); %u sombreadores generados\n", ok, cand, ok ? un : 1, n_k2_gerados);
    c->bits = ok;
    c->unidades = ok ? un : 1;
    c->formatos = fmts & (1u << G3D_FMT_ARGB8888);
    c->max_lado = MAX_LADO;
    c->max_aniso = 1;
    c->zbits = 3;
    c->estencil_bits = (ok & G3D_CAP_ESTENCIL) ? 8 : 0;
    /* D457: la capa lo pone a prueba contra la CPU (autoprueba v3) antes de dejar que se use; si falla, sigue v2. */
    if ((ok & G3D_CAP_V2) && !(flags & FL_SEM_V3))
        c->bits3 = G3D_C3_V3 | G3D_C3_NEBLINA | G3D_C3_TEX_MAT | G3D_C3_INDICES | G3D_C3_TIRAS | G3D_C3_COR_FLOAT;
    else klog("[GPU3D] intel9 v3 (D457): sin anunciar (%s)\n", (flags & FL_SEM_V3) ? "bandera 100000" : "sin v2");
}

const gpu3d_driver_t gpu3d_intel9 = {
    "intel9", i9_iniciar, i9_alvo, i9_limpar, i9_textura, i9_desenhar, i9_esperar, i9_ler, i9_desligar, i9_detalle, i9_memoria,
    "intel9", i9_faixas, (int)(sizeof i9_faixas / sizeof i9_faixas[0]), i9_vistas,
    i9_capacidades, i9_desenhar2, i9_textura2, i9_limpar2, NULL, NULL,   /* sin destino en textura ni presentar (D432) */
    i9_vbuf, i9_desenhar3,                                                /* D457 */
};
