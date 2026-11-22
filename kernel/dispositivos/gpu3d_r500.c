// SPDX-License-Identifier: GPL-2.0-or-later
/* gpu3d_r500.c - 3D por la GPU en las Radeon R5xx (X1300 a X1950: RV515, R520,
 * RV530, RV560, RV570, R580), debajo de la capa común gpu3d (D285, D287).
 *
 * SIN PROBAR EN HARDWARE: QEMU no emula ninguna Radeon. Por eso este driver NO se
 * enciende solo: la capa lo deja «apagado» hasta «gpu3d encender» o «gpu3d probar»
 * (o la opción de arranque --gpu3d=radeon), y solo queda activo si la autoprueba de
 * la capa lee los píxeles esperados y a tiempo. El objetivo es la Mobility Radeon
 * X1400 (RV515/M54, 1002:7145) del Amilo del mantenedor. Las órdenes las construye
 * r500_pacotes.h, que sí se prueba en el PC (tests/r500_3d_unidad.c).
 *
 * Cómo funciona:
 *   - en R5xx los triángulos solo se pueden mandar por el procesador de órdenes
 *     (CP), que necesita su microcódigo (R520_cp.bin de linux-firmware, en
 *     /lib/firmware/radeon; no entra con SEM_FIRMWARE=1 y entonces no hay 3D);
 *   - la cola de órdenes del CP (64 KB) y todo lo demás (destino de color, Z,
 *     texturas) van en VRAM de la tarjeta, DETRÁS de la pantalla y de la zona de
 *     prueba del 2D, en una zona de hasta 16 MB que se comprueba contra el tamaño
 *     real de la VRAM, la apertura de la CPU, lo que el controlador de memoria (MC)
 *     llama memoria de vídeo; va al primer hueco libre detrás de todo lo que se
 *     muestra (las superficies de D1 y D2 con su tamaño de los registros y los
 *     cursores por hardware, D386: el Amilo deja D2 activo dentro de la VRAM
 *     que hay detrás de la pantalla). La CPU la ve por el BAR 0;
 *   - los vértices van dentro de las órdenes (3D_DRAW_IMMD_2): no hay búferes de
 *     vértices. Cada dibujo lleva su destino, su Z/mezcla, su textura y su programa
 *     de fragmentos; lo fijo (r5_inicio) se manda una vez al encender;
 *   - el CP solo trabaja cuando se le avisa (CP_RB_WPTR), y eso se hace al esperar
 *     (leer el cuadro, cambiar una textura o el destino) o si la cola se llena;
 *   - D387 (Amilo): con el CP en marcha, el 2D de acel_radeon.c (que comparte el
 *     motor: volcado, vuelta, copias y rellenos) NO va por MMIO sino por este anillo
 *     (gpu3d_r500_2d), como hacen Linux y xf86-video-ati: mezclar órdenes del 2D por
 *     MMIO con el CP en marcha dejaba el motor ocupado para siempre tras miles de cuadros;
 *   - vallas: cada aviso acaba vaciando las cachés del 3D y escribiendo su número
 *     en un registro de apuntes (SCRATCH_REG2), que el CP solo escribe con el motor
 *     parado (ISYNC_CNTL). Se espera a ese número, con plazo.
 *
 * Qué toca y qué no (seguridad en hardware real):
 *   - sin bus maestro, sin GART, sin interrupciones: la tarjeta no lee ni escribe
 *     memoria del sistema (la cola está en VRAM; la escritura de vuelta del puntero
 *     y de los apuntes, D387, va a la página de reserva de nuestra zona de la VRAM);
 *   - registros: los del CP (0x700-0x7FF), ISYNC_CNTL (se restaura al apagar), los
 *     apuntes 1 y 2, el registro 0x0D del PLL (reparto del reloj entre las tuberías
 *     de píxeles, como hacen Linux y X.org; solo si no está ya así) y, por la cola,
 *     el estado 3D (0x2000-0x4FFF). NO toca los controladores de pantalla, los
 *     relojes de pantalla, la energía, el MC (solo lo lee) ni RBBM_SOFT_RESET;
 *   - toda espera tiene plazo. Si algo no responde se para el CP (CP_CSQ_CNTL = 0)
 *     y se deja: nunca se reinicia el motor con la pantalla del VBE encendida. Si el
 *     motor quedara ocupado, el 2D lo nota con su propio vigilante (D244) y sigue
 *     la CPU.
 *
 * Diagnóstico: «[GPU3D] r500: etapa N ...» en cada paso del encendido, y
 * r5_detalle() («gpu3d probar --detalle», o sola tras un fallo) vuelca RBBM_STATUS,
 * el estado del CP, los punteros de la cola, los apuntes y la zona.
 *
 * Qué hay que comprobar en el hardware: docs/gpu3d.md. */
#include "gpu3d.h"
#include "console.h"
#include "memoria.h"
#include "paginacao.h"
#include "prazo.h"
#include "texto.h"
#include "vfs.h"
#include "video.h"
#include "wc.h"
#include "r500_pacotes.h"
#include "vram_zona_logica.h"
#include "radeon2d_ordens.h"
#include "g3d_v2_provas.h"

#define MMIO_TAM             0x10000
/* D451: 256 KB (eran 64 KB). Un lote de SaguiGL (3072 vértices, unas 24.600 palabras) no cabía entero en la cola:
 * la CPU esperaba a la GPU en cada lote en vez de preparar el siguiente mientras ella dibuja. */
#define ANEL_LOG2_QW         15                /* log2(ANEL_BYTES / 8), para CP_RB_CNTL */
#define ANEL_BYTES           (8u << ANEL_LOG2_QW)
#define ANEL_PAL             (ANEL_BYTES / 4)
#define RESERVA_BYTES        4096              /* página a la que apuntan las escrituras de vuelta (apagadas) */
#define PREP_PAL             6144              /* cola en RAM: un dibujo de 510 vértices y su estado */
/* D387 (cuarta tanda): la zona 3D ocupa ya casi toda la VRAM libre (r5_zona3d_buscar, vram_zona_logica.h), pero el
 * kernel solo tiene 128 MB de ventanas para TODOS los dispositivos (paginacao.h) y no se pueden soltar: se ven
 * siempre sus primeros ZONA_VISTA (cola, página de reserva y los búferes que caben ahí, como hasta ahora) y el
 * resto, cuando la CPU tiene que escribirlo o leerlo (texturas, el cuadro sin la vuelta), por una ventana móvil de
 * JANELA_BYTES que se vuelve a apuntar a trozos, como la apertura que mapea Linux por búfer (ioremap). */
#define ZONA_VISTA           (16u << 20)
#define JANELA_BYTES         (1u << 20)
#define UCODE_BYTES          2048
#define PRAZO_CURTO_MS       250
#ifdef SAGUI_SEM_FIRMWARE
#define SEM_FIRMWARE         1
#else
#define SEM_FIRMWARE         0
#endif

/* Registros que se usan por MMIO (los del estado 3D están en r500_pacotes.h). */
#define CLOCK_CNTL_INDEX     0x0008
#define CLOCK_CNTL_DATA      0x000C
#define   PLL_ESCREVER       0x80u
#define PLL_SCLK_PIPE        0x0D              /* R500_DYN_SCLK_PWMEM_PIPE */
#define MC_IND_INDEX         0x0070
#define MC_IND_DATA          0x0074
#define MC_FB_LOC_RV515      0x01
#define MC_FB_LOC_R520       0x04
#define CONFIG_MEMSIZE       0x00F8
#define CONFIG_APER_SIZE     0x0108
#define CP_RB_BASE           0x0700
#define CP_RB_CNTL           0x0704
#define   RB_SEM_ATUALIZAR   (1u << 27)        /* el CP no escribe su puntero de lectura en memoria */
#define   RB_RPTR_ESCREVER   (1u << 31)
#define CP_RB_RPTR_ADDR      0x070C
#define CP_RB_RPTR           0x0710
#define CP_RB_WPTR           0x0714
#define CP_RB_WPTR_DELAY     0x0718
#define CP_RB_RPTR_WR        0x071C
#define CP_CSQ_CNTL          0x0740
#define   CSQ_ANEL           (4u << 28)        /* cola principal por el anillo (PRIBM_INDBM) */
#define CP_CSQ_MODE          0x0744
#define SCRATCH_UMSK         0x0770
#define SCRATCH_ADDR         0x0774
#define CP_ME_RAM_ADDR       0x07D4
#define CP_ME_RAM_DATAH      0x07DC
#define CP_ME_RAM_DATAL      0x07E0
#define CP_CSQ_STAT          0x07F8
#define RBBM_STATUS          0x0E40
#define   RBBM_ATIVO         0x80000000u
#define APUNTE_TESTE         (R5_SCRATCH_REG0 + 4)
#define APUNTE_VALLA         (R5_SCRATCH_REG0 + 8)
#define D1GRPH_ENABLE        0x6100
#define D1GRPH_SUPERFICIE    0x6110
#define D1GRPH_PITCH         0x6120
#define D1CRTC_CONTROL       0x6080
#define D1GRPH_CONTROL       0x6104
#define D1GRPH_DESL_Y        0x6128            /* D1GRPH_SURFACE_OFFSET_Y */
#define D1GRPH_X_FIM         0x6134            /* D1GRPH_X_END */
#define D1GRPH_Y_FIM         0x6138            /* D1GRPH_Y_END */
#define D1MODE_VP_INI        0x6580            /* D1MODE_VIEWPORT_START (x << 16 | y) */
#define D1MODE_VP_TAM        0x6584            /* D1MODE_VIEWPORT_SIZE (ancho << 16 | alto) */
#define D1CUR_CONTROL        0x6400
#define D1CUR_SUPERFICIE     0x6408
#define CURSOR_BYTES         0x4000u           /* 64x64 a 32 bits */
#define D2_DESLOC            0x0800

enum { F_RV515, F_R520, F_RV530, F_RV560, F_RV570, F_R580 };
static const char *const nome_familia[] = { "RV515", "R520", "RV530", "RV560", "RV570", "R580" };
static const uint8_t fpus_familia[] = { 2, 8, 5, 8, 8, 8 };
/* Solo estas tarjetas (las R5xx discretas de la lista del driver radeon de Linux). Las
 * integradas RS600/RS690 usan otro microcódigo y otra memoria: no están. */
static const struct { uint16_t de, ate; uint8_t familia; } modelos[] = {
    { 0x7100, 0x710F, F_R520 },  { 0x7140, 0x715F, F_RV515 }, { 0x7180, 0x719F, F_RV515 }, { 0x71C0, 0x71DE, F_RV530 },
    { 0x7200, 0x7211, F_RV515 }, { 0x7240, 0x724F, F_R580 },  { 0x7280, 0x7280, F_RV570 }, { 0x7284, 0x7284, F_R580 },
    { 0x7288, 0x728C, F_RV570 }, { 0x7290, 0x7297, F_RV560 },
};

static volatile uint8_t *mmio;
static uint32_t mmio_fis;
static int familia;
static uint16_t modelo;
static uint32_t mc_fb_ini, mc_fb_fim, mc_tela, mc_zona;   /* direcciones en el MC */
static uint32_t zona_off, zona_bytes;                     /* desde el principio de la VRAM (BAR 0) */
/* D432: la pantalla como destino de la presentación: el controlador que la muestra (0 o D2_DESLOC), su
 * paso en píxeles, su tamaño y sus bytes por píxel (del VBE, al encender). */
static uint32_t tela_ctl, tela_passo_px;
static int tela_l, tela_a, tela_bytes_px;
static uint32_t apres_n, apres_vblank_fora, tex_wc_n, tex_uc_n;
static uint8_t *zona_cpu;
static uint32_t zona_vista;                /* bytes de la zona que se ven siempre (desde zona_cpu) */
static uint32_t janela_virt;               /* la ventana móvil (reservada una vez) */
static volatile uint8_t *r5_janela(uint64_t gpu, uint32_t bytes);
static uint32_t zona_fis_map, zona_bytes_map;
static volatile uint32_t *anel;
static uint32_t wptr, enviado;             /* wptr: hasta dónde se ha escrito; enviado: lo que sabe el CP */
static uint32_t seq, seq_vista;            /* última valla emitida y última confirmada */
static uint32_t isync_antes, tuberias;
static bool pronto, cp_ligado, isync_guardado;
static int etapa = -1;   /* D357: hasta dónde llegó el último encendido (-1: nunca se intentó) */
static const char *const nomes_etapa[7] = {
    "-", "sonda de solo lectura", "leer el microcodigo", "tuberias y su reloj", "cargar el microcodigo y arrancar el CP",
    "prueba de la cola", "estado fijo del 3D",
};
static const char *nome_etapa(int e) { return e >= 1 && e <= 6 ? nomes_etapa[e] : "sin intentar"; }
static g3d_arena_t arena;
static g3d_buffer_t b_cor, b_z, b_tex[GPU3D_TEXTURAS];
static r5_alvo_t alvo;
static uint32_t *prep;
/* D426 (gpu3d v2): formato de cada hueco (G3D_FMT_*), el destino actual (el principal o un hueco, con su Z),
 * lo que quedó en el motor (juegos de coordenadas del vértice, programa de fragmentos cargado) y la caché de
 * programas generados (por la combinación: no se genera cada cuadro). */
static uint8_t tex_fmt[GPU3D_TEXTURAS];
static g3d_buffer_t b_zdest, *cur_cor = &b_cor;
static r5_alvo_t alvo_princ;
static int dest_vaga = -1;
static int ntc_posto = 1;          /* r5_inicio deja el formato de v1 (un juego) */
static bool v2_sujo;               /* un dibujo v2 cambió registros que v1 no pone */
static int fp_posto = -1;          /* entrada de la caché cargada en el US (-1: la de v1 u otra) */
#define R5_FP_CACHE 8
#define R5_FP_DESC 72
static struct { bool usado; uint32_t uso; uint8_t desc[R5_FP_DESC]; r5_fp_t fp; } fp_cache[R5_FP_CACHE];
static uint32_t fp_relogio, fp_gerados, fp_acertos, fp_cargas;
static uint32_t caps_v2;           /* lo que pasó las pruebas v2 (para el detalle) */

static inline uint32_t rd(uint32_t r)             { return *(volatile uint32_t *)(mmio + r); }
static inline void     wr(uint32_t r, uint32_t v) { *(volatile uint32_t *)(mmio + r) = v; }

/* D387 (Amilo, 2026-11-19): el CP se queda parado con la cola leída entera (RPTR = WPTR), RBBM_STATUS 90010140
 * (petición del host colgada: HIRQ_ON_RBB, RBBM_HIBUSY) y vallas sin hacer. Medido: con solo el 3D y la valla de
 * siempre, 19.494 cuadros; + tijera en la valla y el 2D por el anillo, 1.986; + HDP en la valla y la valla leída
 * de la VRAM sin parar, 971 (y la prueba de vida: el CP NO ejecuta la cola). Cada cosa es una opción que se
 * cambia sin reiniciar con «acel2d volcado paso N» (el mismo número otra vez la deshace):
 *   27 VRAM     leer la valla de su copia en la VRAM (escritura de vuelta; decide al «gpu3d encender»: si está
 *               puesta entonces se enciende la escritura de vuelta) en vez del registro por MMIO
 *   28 RBBM     tras la valla, esperar también a RBBM_STATUS parado
 *   29 HDP      invalidar el HDP en la valla (R5V_HDP)
 *   30 PAUSA    50 us sin tocar la tarjeta entre lectura y lectura de la valla (POR DEFECTO)
 *   31 TIJERA   la tijera en la valla (R5V_TIJERA)
 *   32 LINUX    cachés y WAIT_UNTIL de la valla con los valores de Linux (R5V_LINUX)
 *   33 SINESPERA  la valla sin WAIT_UNTIL (R5V_SEM_ESPERA)
 *   34 LEERCOLA antes de avisar al CP, leer por la ventana de la CPU la última palabra escrita en la cola (en
 *               la VRAM): una lectura no adelanta a las escrituras anteriores por el mismo camino (el HDP), así
 *               que el CP no puede leer una cola a medio escribir (POR DEFECTO). Linux tiene la cola en la
 *               RAM (GTT), donde esto no hace falta
 *   35 2DSINESPERA  las órdenes del 2D por el anillo sin su WAIT_UNTIL (queda el vaciado de su caché)
 *   36 COLAENTERA   D436: con la cola llena, esperar a que la GPU acabe TODO (una valla), como antes; sin ella
 *               (POR DEFECTO) solo se espera a que el CP lea lo bastante (RPTR), como Linux, y la CPU sigue
 *               preparando el cuadro mientras la GPU dibuja
 * Por defecto: la valla de siempre (la que más aguantó), la valla leída del registro con pausas (Linux sin
 * escritura de vuelta tampoco lee sin parar: radeon_fence_wait duerme entre lecturas) y la cola leída de vuelta. */
#define R5O_VRAM      (1u << 0)
#define R5O_RBBM      (1u << 1)
#define R5O_HDP       (1u << 2)
#define R5O_PAUSA     (1u << 3)
#define R5O_TIJERA    (1u << 4)
#define R5O_LINUX     (1u << 5)
#define R5O_SEM_WAIT  (1u << 6)
#define R5O_LER_ANEL  (1u << 7)
#define R5O_2D_SEM_WAIT (1u << 8)
#define R5O_COLA_ENTERA (1u << 9)
static uint32_t r5_opc = R5O_PAUSA | R5O_LER_ANEL;
static bool wb_ok, wb_ligada;     /* escritura de vuelta encendida al encender el 3D, y comprobada (etapa 5) */
static uint32_t hdp_cntl;         /* HOST_PATH_CNTL al encender */
static uint32_t resgates_mmio, n_vallas, n_cola_llena;   /* n_cola_llena: vueltas esperando sitio en la cola (D436) */
#define WB_SCRATCH 0x800u         /* la copia de los apuntes, dentro de la página de reserva */
static volatile uint32_t *wb_apuntes(void) { return (volatile uint32_t *)(zona_cpu + ANEL_BYTES + WB_SCRATCH); }

static void opc_log(const char *que)
{
    klog("[GPU3D] r500: opciones de la espera (%s) %x:%s%s%s%s%s%s%s%s%s%s; escritura de vuelta %s\n", que, r5_opc,
         r5_opc & R5O_VRAM ? " 27VRAM" : " (valla del registro)", r5_opc & R5O_RBBM ? " 28RBBM" : "", r5_opc & R5O_HDP ? " 29HDP" : "",
         r5_opc & R5O_PAUSA ? " 30PAUSA" : "", r5_opc & R5O_TIJERA ? " 31TIJERA" : "", r5_opc & R5O_LINUX ? " 32LINUX" : "",
         r5_opc & R5O_SEM_WAIT ? " 33SINESPERA" : "", r5_opc & R5O_LER_ANEL ? " 34LEERCOLA" : "",
         r5_opc & R5O_2D_SEM_WAIT ? " 35-2DSINESPERA" : "", r5_opc & R5O_COLA_ENTERA ? " 36COLAENTERA" : "", wb_ligada ? (wb_ok ? "encendida y comprobada" : "encendida SIN llegar") : "apagada");
}

bool gpu3d_r500_opcao(int paso)
{
    if (paso < 27 || paso > 36) return false;
    r5_opc ^= 1u << (paso - 27);
    opc_log("cambiada");
    if (paso == 27 && (r5_opc & R5O_VRAM) && !wb_ok)
        klog("[GPU3D] r500: la escritura de vuelta no esta encendida: la opcion 27 vale desde el proximo «gpu3d encender»\n");
    return true;
}

static uint32_t banderas_valla(void)
{
    return ((r5_opc & R5O_TIJERA) ? R5V_TIJERA : 0) | ((r5_opc & R5O_LINUX) ? R5V_LINUX : 0) |
           ((r5_opc & R5O_SEM_WAIT) ? R5V_SEM_ESPERA : 0) | ((r5_opc & R5O_HDP) ? R5V_HDP : 0);
}

static void pausa_us(uint32_t us)
{
    uint64_t fim = prazo_tsc() + (uint64_t)hw_tsc_khz() * us / 1000u;
    while (prazo_tsc() < fim) __asm__ volatile("pause");
}

static void esperar_ms(uint32_t ms)
{
    prazo_t p = prazo_ms(ms);
    while (!prazo_vencido(&p)) { }
}

/* Registros indirectos: el MC (solo lectura) y el PLL. */
static uint32_t mc_ler(uint32_t reg)
{
    wr(MC_IND_INDEX, 0x7F0000u | (reg & 0xFFFFu));
    uint32_t v = rd(MC_IND_DATA);
    wr(MC_IND_INDEX, 0);
    return v;
}

static uint32_t pll_ler(uint32_t reg)
{
    *(volatile uint8_t *)(mmio + CLOCK_CNTL_INDEX) = (uint8_t)(reg & 0x3F);
    return rd(CLOCK_CNTL_DATA);
}

static void pll_escrever(uint32_t reg, uint32_t v)
{
    *(volatile uint8_t *)(mmio + CLOCK_CNTL_INDEX) = (uint8_t)((reg & 0x3F) | PLL_ESCREVER);
    wr(CLOCK_CNTL_DATA, v);
    *(volatile uint8_t *)(mmio + CLOCK_CNTL_INDEX) = (uint8_t)(reg & 0x3F);
}

static void r5_detalle(const char *fase)
{
    klog("[GPU3D] r500 %s: opciones de la espera %x, escritura de vuelta %s; en la VRAM: apuntes prueba %x valla %x, RPTR %x; "
         "%u vallas, %u rescates por MMIO, %u vueltas con la cola llena; HOST_PATH_CNTL %x\n", fase, r5_opc, wb_ok ? "si" : "no",
         zona_cpu ? wb_apuntes()[1] : 0, zona_cpu ? wb_apuntes()[2] : 0, zona_cpu ? *(volatile uint32_t *)(zona_cpu + ANEL_BYTES) : 0,
         n_vallas, resgates_mmio, n_cola_llena, hdp_cntl);
    klog("[GPU3D] r500 %s: ultima etapa del encendido: %d de 6 (%s); CP %s\n", fase, etapa, nome_etapa(etapa),
         cp_ligado ? "en marcha" : "parado");
    if (!mmio) { klog("[GPU3D] r500 %s: sin iniciar (ningun registro leido)\n", fase); return; }
    klog("[GPU3D] r500 %s: RBBM_STATUS %x CP_CSQ_CNTL %x CP_CSQ_STAT %x CP_RB_CNTL %x ISYNC_CNTL %x\n", fase, rd(RBBM_STATUS),
         rd(CP_CSQ_CNTL), rd(CP_CSQ_STAT), rd(CP_RB_CNTL), rd(R5_ISYNC_CNTL));
    klog("[GPU3D] r500 %s: cola en MC %x: RPTR %x WPTR %x (escrito %x, avisado %x); apuntes: prueba %x valla %x (emitida %x, vista %x)\n",
         fase, rd(CP_RB_BASE), rd(CP_RB_RPTR), rd(CP_RB_WPTR), wptr, enviado, rd(APUNTE_TESTE), rd(APUNTE_VALLA), seq, seq_vista);
    klog("[GPU3D] r500 %s: %s %04x, %u tuberias; VRAM en MC %x-%x, pantalla %x; zona 3D %x+%u KB (libres %u KB)\n", fase,
         nome_familia[familia], modelo, tuberias, mc_fb_ini, mc_fb_fim, mc_tela, mc_zona, zona_bytes >> 10,
         pronto ? (uint32_t)(g3d_arena_livre(&arena) >> 10) : 0);
    klog("[GPU3D] r500 %s: destino %dx%d paso %u: color %x, Z%d %x; GB_PIPE_SELECT %x PLL[0D] %x\n", fase, alvo.larg, alvo.alt,
         alvo.passo_px, alvo.cor_gpu, alvo.zbits, alvo.z_gpu, rd(R5_GB_PIPE_SELECT), pll_ler(PLL_SCLK_PIPE));
    klog("[GPU3D] r500 %s: v2 (D426): capacidades %x; destino %s; programas: %u generados, %u de la cache, %u cargas; "
         "juegos de coordenadas en el motor %d\n", fase, caps_v2, dest_vaga < 0 ? "principal" : "textura", fp_gerados, fp_acertos,
         fp_cargas, ntc_posto);
    klog("[GPU3D] r500 %s: presentar (D432): pantalla MC %x paso %u %dx%d de %d bytes/pixel por el controlador D%d: %s; "
         "%u copias, %u sin retrazado a tiempo; texturas subidas con write-combining %u, sin cache %u\n", fase, mc_tela, tela_passo_px,
         tela_l, tela_a, tela_bytes_px, tela_ctl ? 2 : 1, mc_tela && tela_bytes_px == 4 ? "se puede" : "no se puede", apres_n,
         apres_vblank_fora, tex_wc_n, tex_uc_n);
    /* D390: lo que hay en el destino (sentinelas, colores, Z) y los ajustes del fichero los cuenta
     * la capa común para todos los drivers (gpu3d_diag.h). */
}

/* ------------------------------------------------------------ la cola */

static uint32_t livre(void) { return ((rd(CP_RB_RPTR) & (ANEL_PAL - 1)) - wptr - 1) & (ANEL_PAL - 1); }

/* Dice al CP hasta dónde puede leer (rellenando hasta un múltiplo de 16 palabras, como Linux). */
static void avisar(void)
{
    while (wptr & 15) { anel[wptr] = R5_PACOTE2; wptr = (wptr + 1) & (ANEL_PAL - 1); }
    __asm__ volatile("lock; orl $0, (%%esp)" ::: "memory");   /* lo escrito en la VRAM, antes que el aviso */
    if (r5_opc & R5O_LER_ANEL) (void)anel[(wptr - 1) & (ANEL_PAL - 1)];   /* D387: la cola, escrita de verdad en la VRAM */
    wr(CP_RB_WPTR, wptr);
    (void)rd(CP_RB_WPTR);
    enviado = wptr;
}

static bool motor_parado(uint32_t ms)
{
    prazo_t p = prazo_ms(ms);
    while (rd(RBBM_STATUS) & RBBM_ATIVO)
        if (prazo_vencido(&p)) return false;
    return true;
}

/* D387 (diagnóstico): tras un plazo agotado, ¿sigue vivo el CP? Una escritura del apunte de prueba por el
 * anillo, sin valla, y 50 ms para verla (en la VRAM y en el registro). Solo escribe el log. */
/* D426 (Amilo): banderas de depuración de /sistema/conf/gpu3d-r500.conf («inicio 0x4FF0 N»):
 *   1  sin v2 (no se hacen las pruebas v2: solo v1)
 *   2  v2 sincronizado: cada parte del estado de un dibujo v2 con su valla y su línea en el log
 *   4  v2 con una unidad (sin las pruebas de varias unidades)
 *   8  los semáforos de textura en todas las lecturas del programa v2 (como antes; para comparar)
 *   20 pedir también el esténcil y LIMPAR2 (apagados en R5xx: ver r5_capacidades)
 *   80 sin la espera al cambiar entre v1 y v2 o de programa (D451); 100 esa espera delante de cada dibujo;
 *   200 el 2D por el anillo sin esperar antes a que termine el 3D (D451); 400 la cola sin write-combining (D451)
 * Y, con un tiempo agotado, la cola alrededor del RPTR en el log. */
#define R5D_SEM_V2   1u
#define R5D_SYNC     2u
#define R5D_UMA      4u
#define R5D_SEM_TODAS 8u
#define R5D_ESTENCIL 0x20u
#define R5D_SEM_WC   0x40u   /* D432: las texturas se suben como antes (ventana sin caché), sin write-combining */
#define R5D_SEM_BARREIRA 0x80u    /* D451: sin el vaciado y la espera al cambiar entre v1 y v2 o de programa v2 */
#define R5D_BARREIRA_TODAS 0x100u /* D451 (diagnóstico): el vaciado y la espera delante de cada dibujo */
#define R5D_2D_SEM_BARREIRA 0x200u /* D451: el 2D por el anillo sin esperar antes a que el 3D termine */
#define R5D_ANEL_SEM_WC 0x400u    /* D451: la cola sin write-combining (como antes: sin caché) */
static uint32_t anel_wc_virt, anel_wc_fis;   /* D451: la cola vista en write-combining (una vez; no se suelta) */
static uint32_t r5_dep;
static bool v2_ultimo;               /* D451: el último dibujo fue v2 */
static bool pendente_v1_sync;
static void dep_ler(void)
{
    r5_dep = 0;
    for (int i = 0; i < gpu3d_ajustes.n; i++) if (gpu3d_ajustes.reg[i] == R5_REG_DEPURAR) r5_dep = gpu3d_ajustes.val[i];
    if (r5_dep) klog("[GPU3D] r500: depuracion D426: %x\n", r5_dep);
}

static void volcar_cola(void)
{
    uint32_t r = rd(CP_RB_RPTR) & (ANEL_PAL - 1);
    for (int b = -64; b < 16; b += 8) {
        uint32_t i = (r + (uint32_t)b) & (ANEL_PAL - 1), w[8];
        for (int k = 0; k < 8; k++) w[k] = anel[(i + (uint32_t)k) & (ANEL_PAL - 1)];
        klog("[GPU3D] r500 cola %03x: %08x %08x %08x %08x %08x %08x %08x %08x\n", i, w[0], w[1], w[2], w[3], w[4], w[5], w[6], w[7]);
    }
}

static void prova_de_vida(void)
{
    static uint32_t n;
    volcar_cola();
    uint32_t v = 0xA11FE000u | (++n & 0xFFFu);
    anel[wptr] = R5_PACOTE0(APUNTE_TESTE, 0); wptr = (wptr + 1) & (ANEL_PAL - 1);
    anel[wptr] = v; wptr = (wptr + 1) & (ANEL_PAL - 1);
    avisar();
    prazo_t p = prazo_ms(50);
    while ((wb_ok ? wb_apuntes()[1] : rd(APUNTE_TESTE)) != v && !prazo_vencido(&p)) pausa_us(50);
    uint32_t mem = wb_apuntes()[1], reg = rd(APUNTE_TESTE);
    klog("[GPU3D] r500: prueba de vida del CP: %s (escrito %x: en la VRAM %x, registro %x; RPTR %x WPTR %x RBBM_STATUS %x CP_CSQ_STAT %x)\n",
         mem == v || reg == v ? "el CP SIGUE ejecutando la cola" : "el CP NO ejecuta la cola", v, mem, reg, rd(CP_RB_RPTR), wptr,
         rd(RBBM_STATUS), rd(CP_CSQ_STAT));
}

/* Manda lo pendiente con una valla detrás y espera a que el CP la escriba y el motor pare. */
static int sincronizar(uint32_t ms)
{
    if (!cp_ligado) return G3D_FALHA;
    if (wptr == enviado && seq_vista == seq) return G3D_OK;   /* nada pendiente */
    uint32_t b[24];
    g3d_fila_t f;
    g3d_fila_iniciar(&f, b, 24);
    seq++;
    n_vallas++;
    r5_valla(&f, APUNTE_VALLA, seq, banderas_valla(), r5_tijera1(alvo.larg, alvo.alt), hdp_cntl);
    /* Siempre queda sitio para la valla: emitir() guarda 64 palabras. */
    for (uint32_t i = 0; i < f.n; i++) { anel[wptr] = b[i]; wptr = (wptr + 1) & (ANEL_PAL - 1); }
    avisar();
    prazo_t p = prazo_ms(ms);
    bool vram = (r5_opc & R5O_VRAM) && wb_ok;
    uint64_t khz = hw_tsc_khz(), prox = prazo_tsc() + khz * 2u;
    /* Sin leer el registro mientras el CP trabaja (D387): la copia de la VRAM, por la ventana de la CPU.
     * D436: cada vuelta puede ceder la CPU (gpu3d_respirar); el plazo se mira justo después de leer la valla, así
     * que el tiempo cedido no da un falso «no llega». Mientras cede, otro proceso puede mandar su valla: «seq» es la
     * última y se espera a esa (se escriben en orden). */
    while ((vram ? wb_apuntes()[2] : rd(APUNTE_VALLA)) != seq) {
        if (vram && prazo_tsc() >= prox) {   /* la copia no cambia: una lectura del registro cada 2 ms */
            prox = prazo_tsc() + khz * 2u;
            if (rd(APUNTE_VALLA) == seq) {
                if (resgates_mmio++ < 4)
                    klog("[GPU3D] r500: la valla %x se vio en el registro y no en la VRAM (copia %x)\n", seq, wb_apuntes()[2]);
                break;
            }
        }
        if (prazo_vencido(&p)) {
            klog("[GPU3D] r500: la valla %x no llega en %u ms (apunte %x, en la VRAM %x, RPTR %x, WPTR %x, RBBM_STATUS %x)\n", seq, ms,
                 rd(APUNTE_VALLA), wb_apuntes()[2], rd(CP_RB_RPTR), wptr, rd(RBBM_STATUS));
            prova_de_vida();
            return G3D_FALHA;
        }
        if (r5_opc & R5O_PAUSA) pausa_us(50);
        gpu3d_respirar();
    }
    seq_vista = seq;
    if ((r5_opc & R5O_RBBM) && !motor_parado(ms)) {
        klog("[GPU3D] r500: la valla %x llego pero el motor sigue ocupado (RBBM_STATUS %x)\n", seq, rd(RBBM_STATUS));
        prova_de_vida();
        return G3D_FALHA;
    }
    return G3D_OK;
}

/* Copia una cola preparada al anillo. Si no cabe, manda lo que hay y espera a que termine. */
static int emitir(const g3d_fila_t *f)
{
    if (!cp_ligado) return G3D_FALHA;
    if (f->transbordou || f->n + 64 >= ANEL_PAL) {
        klog("[GPU3D] r500: una cola de ordenes no cupo en su bufer (%u palabras; error interno)\n", f->n);
        return G3D_FALHA;
    }
    /* D436: con la cola llena, como Linux (RPTR): se avisa al CP y se espera solo a que haya sitio, cediendo la CPU;
     * la GPU sigue dibujando lo de antes mientras la CPU prepara lo siguiente. Con la opción 36, la valla de antes
     * (esperar a que acabe todo). Se vuelve a mirar en bucle: mientras se cede, otro proceso (el 2D) puede gastar sitio. */
    prazo_t pz = prazo_ms(GPU3D_PRAZO_MS);
    while (livre() < f->n + 64) {
        if (r5_opc & R5O_COLA_ENTERA) {
            int r = sincronizar(GPU3D_PRAZO_MS);
            if (r != G3D_OK) return r;
            if (livre() >= f->n + 64) break;
        } else {
            if (wptr != enviado) avisar();
            n_cola_llena++;
            if (livre() >= f->n + 64) break;
        }
        if (prazo_vencido(&pz)) {
            klog("[GPU3D] r500: la cola no se vacia (RPTR %x, WPTR %x, avisado %x, RBBM_STATUS %x)\n", rd(CP_RB_RPTR), wptr, enviado,
                 rd(RBBM_STATUS));
            prova_de_vida();
            return G3D_FALHA;
        }
        pausa_us(50);
        gpu3d_respirar();
    }
    for (uint32_t i = 0; i < f->n; i++) { anel[wptr] = f->p[i]; wptr = (wptr + 1) & (ANEL_PAL - 1); }
    return G3D_OK;
}

/* dbg5 (Amilo, volcado por la GART): una copia del motor 2D mandada por el CP, como r100_copy_blit del driver
 * radeon (el código es propio; de allí, el orden y los valores): un paquete 3 BITBLT_MULTI (GMC con los dos
 * pitch/offset y los dos recortes, origen, destino, recorte de 0 a 0x1FFF, x/y de origen y destino, ancho/alto),
 * el vaciado de la caché de destino del 2D (DSTCACHE_CTLSTAT) y WAIT_UNTIL (2D parado y limpio, host parado y
 * limpio, DMA/GUI parado), y detrás la valla de siempre, que se espera con plazo. Solo con el CP ya en marcha
 * (3D encendido). src_po/dst_po: (pitch / 64) << 22 | dirección en el MC >> 10. */
bool gpu3d_r500_cp_em_marcha(void) { return pronto && cp_ligado; }

/* D399 (gpu_energia.c): antes de cambiar los relojes, que el CP termine lo que tiene (una valla y el
 * motor parado). 0: no hay CP en marcha; 1: parado; -1: no paró en "ms". */
int gpu3d_r500_ocioso(uint32_t ms)
{
    if (!pronto || !cp_ligado) return 0;
    gpu3d_respirar_bloquear(true);   /* D436: las entradas del 2D y de la energía no ceden la CPU */
    int r = sincronizar(ms) == G3D_OK ? 1 : -1;
    gpu3d_respirar_bloquear(false);
    return r;
}

bool gpu3d_r500_copia_cp(uint32_t src_po, uint32_t dst_po, int sx, int sy, int dx, int dy, int l, int a, uint32_t ms)
{
    if (!pronto || !cp_ligado) return false;
    uint32_t b[24];
    g3d_fila_t f;
    g3d_fila_iniciar(&f, b, 24);
    g3d_por(&f, 0xC0000000u | (8u << 16) | (0x9Bu << 8));            /* paquete 3, BITBLT_MULTI, 9 palabras */
    g3d_por(&f, (1u << 0) | (1u << 1) | (1u << 2) | (1u << 3) |      /* pitch/offset de origen y destino, recortes */
                (15u << 4) | (6u << 8) | (3u << 12) | 0x00CC0000u |  /* sin patrón, ARGB8888, origen de color, ROP copia */
                (2u << 24) | (1u << 28) | (1u << 30));               /* origen en memoria, sin comparar, sin máscara */
    g3d_por(&f, src_po);
    g3d_por(&f, dst_po);
    g3d_por(&f, 0x1FFFu | (0x1FFFu << 16));
    g3d_por(&f, 0);
    g3d_por(&f, 0x1FFFu | (0x1FFFu << 16));
    g3d_por(&f, ((uint32_t)sx << 16) | (uint32_t)sy);
    g3d_por(&f, ((uint32_t)dx << 16) | (uint32_t)dy);
    g3d_por(&f, ((uint32_t)l << 16) | (uint32_t)a);
    g3d_por(&f, 0x1714u >> 2);                                       /* paquete 0: DSTCACHE_CTLSTAT */
    g3d_por(&f, 0xFu);                                               /* vaciar todo */
    g3d_por(&f, 0x1720u >> 2);                                       /* paquete 0: WAIT_UNTIL */
    g3d_por(&f, (1u << 16) | (1u << 18) | (1u << 9));                /* 2D y host parados y limpios, DMA/GUI parado */
    gpu3d_respirar_bloquear(true);
    bool ok = emitir(&f) == G3D_OK;
    if (ok && sincronizar(ms) != G3D_OK) { r5_detalle("copia por el CP: tiempo agotado"); ok = false; }
    gpu3d_respirar_bloquear(false);
    return ok;
}

/* D387 (Amilo, 2026-11-19): el 2D de acel_radeon.c por el anillo mientras el CP está en marcha. Con el 3D
 * encendido y el volcado del escritorio por MMIO, tras 1.748 a 12.149 cuadros el motor quedaba ocupado para
 * siempre (RBBM_STATUS 90010140: GUI_ACTIVE, RBBM_HIBUSY, CP_CMDSTRM_BUSY y una petición del host en el bus de
 * registros, HIRQ_ON_RBB; ni el 2D ni el 3D ocupados; RPTR = WPTR). Linux y xf86-video-ati no escriben el 2D
 * por MMIO con el CP en marcha: lo mandan por el anillo con PACKET0 (OUT_ACCEL_REG en ACCEL_CP), seguido del
 * vaciado de la caché del 2D y WAIT_UNTIL (radeon2d_ordens.h). Aquí se pone la orden en el anillo sin avisar
 * al CP; gpu3d_r500_2d_esperar avisa y espera la valla (que vacía y espera al 2D y al 3D) y el motor parado. */
bool gpu3d_r500_2d(const ra2d_ordem_t *o)
{
    if (!pronto || !cp_ligado) return false;
    uint32_t b[2 * (RA2D_MAX_ESCRITAS + 2)];
    ra2d_ordem_t c = *o;
    ra2d_fim_cp(&c);
    if (r5_opc & R5O_2D_SEM_WAIT) c.n--;   /* D387, opción 35: sin el WAIT_UNTIL del final (el último par) */
    uint32_t n = ra2d_pacotes(&c, b, (uint32_t)(sizeof b / sizeof b[0]));
    if (!n) { klog("[GPU3D] r500: una orden del 2D no se puede poner en el anillo (%d escrituras; error interno)\n", c.n); return false; }
    g3d_fila_t f;
    g3d_fila_iniciar(&f, b, n);
    f.n = n;
    gpu3d_respirar_bloquear(true);
    bool ok = true;
    /* D451 (Amilo, Brumacanto): una copia del 2D metida entre dos dibujos v2 del 3D, con el 3D todavía dibujando,
     * colgaba la X1400 (GA ocupado; con «acel2d apagar» o con cada dibujo sincronizado no pasaba; con v1, AoEN, nunca
     * se vio). Delante de cada orden del 2D: vaciar las cachés del 3D y esperar al 2D y al 3D parados y limpios. */
    if (!(r5_dep & R5D_2D_SEM_BARREIRA)) {
        uint32_t bb[8];
        g3d_fila_t fb;
        g3d_fila_iniciar(&fb, bb, 8);
        r5_vaciar(&fb);
        ok = emitir(&fb) == G3D_OK;
    }
    if (ok) ok = emitir(&f) == G3D_OK;
    gpu3d_respirar_bloquear(false);
    return ok;
}

bool gpu3d_r500_2d_esperar(uint32_t ms)
{
    if (!pronto || !cp_ligado) return false;
    gpu3d_respirar_bloquear(true);
    bool ok = sincronizar(ms) == G3D_OK;
    gpu3d_respirar_bloquear(false);
    if (ok) return true;
    r5_detalle("2D por el CP: tiempo agotado");
    return false;
}

/* ------------------------------------------------------------ apagar */

static void cp_parar(void)
{
    wr(CP_CSQ_MODE, 0);
    wr(CP_CSQ_CNTL, 0);
    wr(SCRATCH_UMSK, 0);
    wr(CP_RB_CNTL, rd(CP_RB_CNTL) | RB_SEM_ATUALIZAR);   /* D387: sin escritura de vuelta con el CP parado */
    wb_ok = wb_ligada = false;
    cp_ligado = false;
}

static void r5_desligar(void)
{
    bool estava = pronto || cp_ligado;
    pronto = false;
    if (mmio && cp_ligado) {
        if (!motor_parado(50)) klog("[GPU3D] r500: el motor sigue ocupado al parar el CP (RBBM_STATUS %x)\n", rd(RBBM_STATUS));
        cp_parar();
        if (isync_guardado) wr(R5_ISYNC_CNTL, isync_antes);
        klog("[GPU3D] r500: CP parado (CP_CSQ_CNTL %x, RBBM_STATUS %x); la pantalla y el 2D siguen como estaban\n",
             rd(CP_CSQ_CNTL), rd(RBBM_STATUS));
    }
    isync_guardado = false;
    if (prep) { kfree(prep); prep = NULL; }
    memset(&alvo, 0, sizeof alvo);
    if (estava) klog("[GPU3D] r500: 3D apagado\n");
}

/* ------------------------------------------------------------ encender */

static int familia_de(uint16_t m)
{
    for (unsigned i = 0; i < sizeof modelos / sizeof modelos[0]; i++)
        if (m >= modelos[i].de && m <= modelos[i].ate) return modelos[i].familia;
    return -1;
}

static inline uint32_t be32(const uint8_t *p) { return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3]; }

static uint32_t ler_bar(const pci_disp_t *d, uint8_t reg, bool *alto)
{
    uint32_t v = pci_le32(d, reg);
    *alto = (v & 1) == 0 && ((v >> 1) & 3) == 2 && pci_le32(d, (uint8_t)(reg + 4)) != 0;
    return v;
}

/* Etapa 1 (solo lecturas): la tarjeta, su memoria y dónde cabe la zona 3D. */
static int sondar(const pci_disp_t *d, const acel_tela_t *t, char *m, int nm)
{
    bool alto0, alto2;
    uint32_t bar0 = ler_bar(d, 0x10, &alto0), bar2 = ler_bar(d, 0x18, &alto2);
    if ((bar0 & 1) || (bar2 & 1) || alto0 || alto2 || !(bar2 & ~0xFu) || !(bar0 & ~0xFu)) {
        snformatar(m, (size_t)nm, "BAR 0 (%x) o BAR 2 (%x) no son de memoria de 32 bits", bar0, bar2);
        return G3D_NAO;
    }
    bar0 &= ~0xFu;
    if (!(pci_le32(d, 0x04) & 0x02)) { snformatar(m, (size_t)nm, "la tarjeta no tiene la memoria activada en el PCI"); return G3D_NAO; }
    if (!mmio || mmio_fis != (bar2 & ~0xFu)) {
        mmio_fis = bar2 & ~0xFu;
        mmio = (volatile uint8_t *)janela_mapear_tipo(mmio_fis, MMIO_TAM, PG_PCD | PG_PWT);
    }
    if (!mmio) { snformatar(m, (size_t)nm, "sin ventana para el MMIO"); return G3D_NAO; }
    uint32_t est = rd(RBBM_STATUS);
    if (est == 0xFFFFFFFFu) { snformatar(m, (size_t)nm, "el MMIO no responde (RBBM_STATUS %x)", est); return G3D_NAO; }
    /* La memoria de vídeo en el MC y la pantalla dentro de ella. */
    uint32_t loc = mc_ler(familia == F_RV515 ? MC_FB_LOC_RV515 : MC_FB_LOC_R520);
    uint32_t memsize = rd(CONFIG_MEMSIZE), aper = rd(CONFIG_APER_SIZE);
    mc_fb_ini = (loc & 0xFFFFu) << 16;
    mc_fb_fim = ((loc >> 16) << 16) | 0xFFFFu;
    uint32_t pp = t->pitch / 4;
    tela_passo_px = pp; tela_l = t->larg; tela_a = t->alt; tela_bytes_px = t->bytes_px;   /* D432 */
    uint32_t e1 = rd(D1GRPH_ENABLE), s1 = rd(D1GRPH_SUPERFICIE), p1 = rd(D1GRPH_PITCH);
    uint32_t e2 = rd(D1GRPH_ENABLE + D2_DESLOC), s2 = rd(D1GRPH_SUPERFICIE + D2_DESLOC), p2 = rd(D1GRPH_PITCH + D2_DESLOC);
    klog("[GPU3D] r500: etapa 1: %s %04x; BAR0 %x BAR2 %x; MC_FB_LOCATION %x (VRAM en MC %x-%x); CONFIG_MEMSIZE %u MB, apertura %u MB, VBE %u KB\n",
         nome_familia[familia], modelo, bar0, bar2, loc, mc_fb_ini, mc_fb_fim, memsize >> 20, aper >> 20, t->memoria >> 10);
    klog("[GPU3D] r500: etapa 1: D1 %s superficie %x pitch %u; D2 %s superficie %x pitch %u; RBBM_STATUS %x CP_CSQ_CNTL %x\n",
         e1 & 1 ? "activo" : "apagado", s1, p1, e2 & 1 ? "activo" : "apagado", s2, p2, est, rd(CP_CSQ_CNTL));
    if ((e1 & 1) && p1 == pp) { mc_tela = s1; tela_ctl = 0; }
    else if ((e2 & 1) && p2 == pp) { mc_tela = s2; tela_ctl = D2_DESLOC; }
    else { snformatar(m, (size_t)nm, "ningun controlador AVIVO activo con el pitch del VBE (%u pixeles)", pp); return G3D_NAO; }
    if (loc == 0xFFFFFFFFu || mc_fb_fim <= mc_fb_ini || mc_tela < mc_fb_ini || mc_tela > mc_fb_fim) {
        snformatar(m, (size_t)nm, "la pantalla (MC %x) no esta en la VRAM que dice el MC (%x-%x)", mc_tela, mc_fb_ini, mc_fb_fim);
        return G3D_NAO;
    }
    /* La CPU tiene que ver la VRAM desde su principio por el BAR 0: la pantalla lo confirma. */
    uint32_t tela_off = mc_tela - mc_fb_ini;
    if (t->fb_fisico < bar0 || t->fb_fisico - bar0 != tela_off) {
        snformatar(m, (size_t)nm, "la pantalla esta en %x del BAR 0 pero en %x de la VRAM: no se sabe donde cae cada direccion",
                   t->fb_fisico - bar0, tela_off);
        return G3D_NAO;
    }
    /* Límite: lo menor entre la VRAM real, la apertura de la CPU y la VRAM del MC. */
    uint64_t lim = (uint64_t)mc_fb_fim - mc_fb_ini + 1;
    if (memsize < (16u << 20) || memsize == 0xFFFFFFFFu || aper < (16u << 20) || aper == 0xFFFFFFFFu) {
        snformatar(m, (size_t)nm, "tamanos de memoria raros (CONFIG_MEMSIZE %x, CONFIG_APER_SIZE %x)", memsize, aper);
        return G3D_NAO;
    }
    if (lim > memsize) lim = memsize;
    if (lim > aper) lim = aper;
    /* Lo que la tarjeta muestra o lee de la VRAM (D386): la pantalla del VBE con la zona
     * de prueba del 2D debajo, la superficie de cada controlador con la imagen activa
     * (con su tamaño de los registros, o 8 MB si no se sabe) y los cursores por hardware.
     * La zona 3D va al primer hueco libre detrás de la pantalla, no justo detrás. */
    vram_ocupado_t oc[5];
    int noc = 0;
    oc[noc].ini = tela_off;
    oc[noc++].fim = (uint64_t)tela_off + (uint64_t)t->pitch * (uint32_t)(t->alt + 2 * ACEL2D_ZONA_A);
    for (int c = 0; c < 2; c++) {
        uint32_t o = c ? D2_DESLOC : 0;
        uint32_t ge = c ? e2 : e1, gs = c ? s2 : s1, gp = c ? p2 : p1;
        uint32_t crtc = rd(D1CRTC_CONTROL + o), ctl = rd(D1GRPH_CONTROL + o), dy = rd(D1GRPH_DESL_Y + o);
        uint32_t xf = rd(D1GRPH_X_FIM + o), yf = rd(D1GRPH_Y_FIM + o), vpi = rd(D1MODE_VP_INI + o), vpt = rd(D1MODE_VP_TAM + o);
        uint32_t cc = rd(D1CUR_CONTROL + o), cs = rd(D1CUR_SUPERFICIE + o);
        uint64_t bytes = avivo_superficie_bytes(ctl, gp, yf, dy, vpi, vpt);
        klog("[GPU3D] r500: etapa 1: D%d CRTC %s (%x), GRPH_CONTROL %x, fin %ux%u, desplazamiento Y %u, ventana %ux%u en %u,%u: "
             "%u KB desde MC %x%s; cursor %s en MC %x\n", c + 1, crtc & 1 ? "encendido" : "apagado", crtc, ctl, xf & 0x3FFFu,
             yf & 0x3FFFu, dy & 0x3FFFu, vpt >> 16, vpt & 0x3FFFu, vpi >> 16, vpi & 0x3FFFu, (uint32_t)(bytes >> 10), gs,
             bytes == VRAM_MARGEM_DESCONHECIDA ? " (tamano desconocido: margen de 8 MB)" : "", cc & 1 ? "encendido" : "apagado", cs);
        if ((ge & 1) && gs >= mc_fb_ini && gs <= mc_fb_fim) {
            oc[noc].ini = gs - mc_fb_ini;
            oc[noc++].fim = (uint64_t)(gs - mc_fb_ini) + bytes;
        }
        if ((cc & 1) && cs >= mc_fb_ini && cs <= mc_fb_fim) {
            oc[noc].ini = cs - mc_fb_ini;
            oc[noc++].fim = (uint64_t)(cs - mc_fb_ini) + CURSOR_BYTES;
        }
    }
    uint64_t ini64;
    uint32_t tam;
    if (!r5_zona3d_buscar(oc, noc, oc[0].fim, lim, &ini64, &tam)) {
        snformatar(m, (size_t)nm, "no caben 2 MB para el 3D detras de la pantalla y de las superficies activas (VRAM util %u KB)",
                   (uint32_t)(lim >> 10));
        return G3D_NAO;
    }
    uint32_t ini = (uint32_t)ini64;
    if ((uint64_t)mc_fb_ini + ini + tam > 0xFFFFFFFFull || (uint64_t)bar0 + ini + tam > 0xFFFFFFFFull) {
        snformatar(m, (size_t)nm, "la zona 3D pasa de los 4 GB");
        return G3D_NAO;
    }
    mc_zona = mc_fb_ini + ini;
    /* La comprobación de siempre, aparte de la búsqueda: nada de lo que se muestra en la zona. */
    int pisa = vram_zona_pisa(oc, noc, ini, tam);
    if (pisa >= 0) {
        snformatar(m, (size_t)nm, "la zona 3D (MC %x, %u KB) pisa lo que muestra la tarjeta (MC %x-%x)", mc_zona, tam >> 10,
                   mc_fb_ini + (uint32_t)oc[pisa].ini, mc_fb_ini + (uint32_t)oc[pisa].fim);
        return G3D_NAO;
    }
    zona_off = ini; zona_bytes = tam;
    uint32_t fis = bar0 + ini, vista = tam < ZONA_VISTA ? tam : ZONA_VISTA;
    if (!zona_cpu || zona_fis_map != fis || zona_bytes_map < vista) {
        zona_cpu = (uint8_t *)janela_mapear_tipo(fis, vista, PG_PCD | PG_PWT);
        zona_fis_map = fis; zona_bytes_map = vista;
    }
    if (!zona_cpu) { snformatar(m, (size_t)nm, "sin ventana del kernel para %u KB de memoria de video", vista >> 10); return G3D_NAO; }
    zona_vista = vista;
    if (!janela_virt) janela_virt = janela_reservar(JANELA_BYTES);
    if (!janela_virt && tam > vista) { zona_bytes = tam = vista; }   /* sin ventana móvil: solo lo que se ve */
    klog("[GPU3D] r500: etapa 1: zona 3D de %u KB en MC %x (a %u KB del principio de la VRAM; CPU %x); la CPU ve siempre los "
         "primeros %u KB y el resto por una ventana de %u KB; VRAM util %u KB\n", tam >> 10, mc_zona, ini >> 10, fis, vista >> 10,
         janela_virt ? JANELA_BYTES >> 10 : 0, (uint32_t)(lim >> 10));
    return G3D_OK;
}

static int r5_iniciar_etapas(const pci_disp_t *d, int geracao, const acel_tela_t *t, char *m, int nm)
{
    pronto = false;
    etapa = 1;
    if (SEM_FIRMWARE) {
        snformatar(m, (size_t)nm, "esta compilacion no lleva firmware (SEM_FIRMWARE=1): sin el microcodigo del CP no hay 3D en R5xx");
        return G3D_NAO;
    }
    int fam = d->fabricante == 0x1002 && geracao == 2 ? familia_de(d->modelo) : -1;
    if (fam < 0) {
        snformatar(m, (size_t)nm, "%04x:%04x no es una Radeon R5xx de la lista", d->fabricante, d->modelo);
        return G3D_NAO;
    }
    if (cp_ligado) r5_desligar();
    familia = fam; modelo = d->modelo;
    if (!hw_tsc_khz()) { snformatar(m, (size_t)nm, "sin TSC medido para los plazos de la GPU"); return G3D_NAO; }
    int r = sondar(d, t, m, nm);
    if (r != G3D_OK) return r;

    /* Etapa 2: el microcódigo, del sistema de archivos. */
    etapa = 2;
    uint8_t *uc = NULL;
    uint32_t nuc = 0;
    if (vfs_ler_inteiro("/lib/firmware/radeon/R520_cp.bin", &uc, &nuc) < 0 || nuc != UCODE_BYTES) {
        snformatar(m, (size_t)nm, "falta el microcodigo /lib/firmware/radeon/R520_cp.bin (o no mide %u bytes: %u)", UCODE_BYTES, nuc);
        if (uc) kfree(uc);
        return G3D_NAO;
    }
    if (!prep) prep = kmalloc(PREP_PAL * 4);
    if (!prep) { kfree(uc); snformatar(m, (size_t)nm, "sin memoria para preparar las ordenes"); return G3D_NAO; }
    /* ¿La CPU ve de verdad esa VRAM? Escribir y leer en la página de reserva. */
    anel = (volatile uint32_t *)zona_cpu;
    /* D451 (Amilo): la cola en la VRAM sin caché se escribía a unos 30 MB/s y, con Brumacanto por la GPU (unos 50.000
     * vértices por cuadro, 1,6 MB de órdenes), era lo que más tardaba del kernel. Con la PAT en marcha (la misma
     * condición que las texturas, D432) la cola se escribe por una segunda vista en write-combining; avisar() ya vacía
     * los búferes de combinación (LOCK) y relee la última palabra antes de mover el WPTR. Los apuntes y lo demás de la
     * zona siguen sin caché. «inicio 4ff0 400» en gpu3d-r500.conf lo quita. */
    if (wc_ativo() && wc_via()[0] == 'P' && !(r5_dep & R5D_ANEL_SEM_WC)) {
        if (!anel_wc_virt || anel_wc_fis != zona_fis_map) {
            anel_wc_virt = janela_mapear_tipo(zona_fis_map, ANEL_BYTES, PG_PWT);
            anel_wc_fis = zona_fis_map;
        }
        if (anel_wc_virt) anel = (volatile uint32_t *)anel_wc_virt;
    }
    klog("[GPU3D] r500: cola de ordenes %s (D451)\n", (uint32_t)(uintptr_t)anel == anel_wc_virt && anel_wc_virt ? "en write-combining" : "sin cache");
    volatile uint32_t *res = (volatile uint32_t *)(zona_cpu + ANEL_BYTES);
    res[0] = 0x5A6D3D05u; res[1] = ~0x5A6D3D05u;
    __asm__ volatile("lock; orl $0, (%%esp)" ::: "memory");
    if (res[0] != 0x5A6D3D05u || res[1] != ~0x5A6D3D05u) {
        snformatar(m, (size_t)nm, "la VRAM de la zona 3D no guarda lo escrito (%x): no es memoria utilizable", res[0]);
        kfree(uc);
        return G3D_NAO;
    }
    /* D387 (cuarta tanda): ¿la ventana móvil ve de verdad el final de la zona? (la VRAM existe ahí y la ventana
     * apunta bien). Si no, la zona se queda en lo que se ve siempre. */
    if (zona_bytes > zona_vista) {
        volatile uint32_t *fim = (volatile uint32_t *)r5_janela((uint64_t)mc_zona + zona_bytes - 4096u, 4096u);
        if (fim) { fim[0] = 0x3D5A0F1Du; fim[1023] = ~0x3D5A0F1Du; __asm__ volatile("lock; orl $0, (%%esp)" ::: "memory"); }
        bool bem = fim && fim[0] == 0x3D5A0F1Du && fim[1023] == ~0x3D5A0F1Du;
        klog("[GPU3D] r500: etapa 2: final de la zona (MC %x) por la ventana movil: %s\n", mc_zona + zona_bytes - 4096u,
             bem ? "se lee lo escrito" : "NO se lee lo escrito: la zona se queda en la parte que se ve siempre");
        if (!bem) zona_bytes = zona_vista;
    }
    if (!motor_parado(PRAZO_CURTO_MS)) {
        snformatar(m, (size_t)nm, "el motor grafico esta ocupado antes de empezar (RBBM_STATUS %x): no se toca", rd(RBBM_STATUS));
        kfree(uc);
        return G3D_NAO;
    }
    klog("[GPU3D] r500: etapa 2: microcodigo R520_cp.bin leido (%u bytes, primeras palabras %x %x); VRAM de la zona comprobada\n",
         nuc, be32(uc), be32(uc + 4));

    /* Etapa 3: tuberías de píxeles y su reloj (como rv515_gpu_init de Linux). */
    etapa = 3;
    uint32_t gps = rd(R5_GB_PIPE_SELECT), pll = pll_ler(PLL_SCLK_PIPE);
    tuberias = ((gps >> 12) & 3) + 1;
    uint32_t pll_novo = (1u << ((rd(R5_DST_PIPE_CONFIG) >> 2) & 3)) | (((gps >> 8) & 0xFu) << 4);
    if (pll != pll_novo) pll_escrever(PLL_SCLK_PIPE, pll_novo);
    klog("[GPU3D] r500: etapa 3: GB_PIPE_SELECT %x (%u tuberias), PLL[0D] %x -> %x%s\n", gps, tuberias, pll, pll_ler(PLL_SCLK_PIPE),
         pll != pll_novo ? " (cambiado)" : " (ya estaba)");

    /* Etapa 4: CP parado, microcódigo dentro y la cola (en VRAM) a cero. Sin escrituras de vuelta. */
    etapa = 4;
    isync_antes = rd(R5_ISYNC_CNTL); isync_guardado = true;
    cp_parar();
    wr(CP_ME_RAM_ADDR, 0);
    for (uint32_t i = 0; i < UCODE_BYTES; i += 8) {
        wr(CP_ME_RAM_DATAH, be32(uc + i));
        wr(CP_ME_RAM_DATAL, be32(uc + i + 4));
    }
    kfree(uc);
    /* D386, D390: los ajustes de depuración (/sistema/conf/gpu3d-r500.conf) los lee la capa
     * antes de iniciar(): están en gpu3d_ajustes. */
    for (uint32_t i = 0; i < ANEL_PAL; i++) anel[i] = R5_PACOTE2;
    uint32_t cntl = (uint32_t)ANEL_LOG2_QW | (9u << 8) | (1u << 18);   /* log2(ANEL_BYTES / 8), bloques de 4 KB, lectura de 1 en 1 */
    wr(CP_RB_WPTR_DELAY, 64);
    wr(CP_RB_CNTL, cntl | RB_SEM_ATUALIZAR);
    wr(CP_RB_BASE, mc_zona);
    wr(CP_RB_CNTL, cntl | RB_RPTR_ESCREVER | RB_SEM_ATUALIZAR);
    wr(CP_RB_RPTR_WR, 0);
    wptr = enviado = 0;
    wr(CP_RB_WPTR, 0);
    wr(CP_RB_RPTR_ADDR, (mc_zona + ANEL_BYTES) & ~3u);
    wr(SCRATCH_ADDR, mc_zona + ANEL_BYTES + WB_SCRATCH);
    /* D387: escritura de vuelta de los apuntes 1 y 2 (y del puntero de lectura) a la página de reserva de la
     * VRAM, como r100_cp_init con la escritura de vuelta encendida (sin RB_NO_UPDATE): la valla se lee de ahí,
     * no del registro. Sigue sin tocar la RAM: la página es de la zona 3D. */
    for (uint32_t i = 0; i < 1024; i++) res[i] = 0;
    hdp_cntl = rd(R5_HOST_PATH_CNTL);
    wb_ok = false;
    wb_ligada = (r5_opc & R5O_VRAM) != 0;   /* D387: solo con la opción 27 puesta al encender (antes, nunca) */
    wr(SCRATCH_UMSK, wb_ligada ? (1u << 1) | (1u << 2) : 0);
    wr(CP_RB_CNTL, wb_ligada ? cntl : cntl | RB_SEM_ATUALIZAR);
    esperar_ms(1);
    uint32_t rptr0 = rd(CP_RB_RPTR);
    wr(CP_CSQ_MODE, (80u << 0) | (16u << 8));   /* dónde empiezan las colas indirectas (no se usan) */
    wr(CP_RB_WPTR_DELAY, 0);
    wr(CP_CSQ_MODE, 0x00004D4Du);
    wr(CP_CSQ_CNTL, CSQ_ANEL);
    cp_ligado = true;
    klog("[GPU3D] r500: etapa 4: CP en marcha: cola en MC %x, CP_RB_CNTL %x, RPTR %x, CP_CSQ_CNTL %x, RBBM_STATUS %x\n", rd(CP_RB_BASE),
         rd(CP_RB_CNTL), rptr0, rd(CP_CSQ_CNTL), rd(RBBM_STATUS));
    if (rptr0 != 0) {
        snformatar(m, (size_t)nm, "el puntero de lectura de la cola no quedo a cero (%x)", rptr0);
        r5_detalle("al iniciar");
        r5_desligar();
        return G3D_FALHA;
    }

    /* Etapa 5: la prueba de la cola: el CP tiene que escribir un registro de apuntes. */
    etapa = 5;
    wr(APUNTE_TESTE, 0xCAFEDEADu);
    wr(APUNTE_VALLA, 0);
    seq = seq_vista = 0x3D500000u;
    anel[wptr++] = R5_PACOTE0(APUNTE_TESTE, 0);
    anel[wptr++] = 0xDEADBEEFu;
    avisar();
    prazo_t pz = prazo_ms(PRAZO_CURTO_MS);
    while (rd(APUNTE_TESTE) != 0xDEADBEEFu)
        if (prazo_vencido(&pz)) {
            snformatar(m, (size_t)nm, "el CP no ejecuta la prueba de la cola (apunte %x, RPTR %x, WPTR %x, RBBM_STATUS %x)",
                       rd(APUNTE_TESTE), rd(CP_RB_RPTR), wptr, rd(RBBM_STATUS));
            r5_detalle("prueba de la cola");
            r5_desligar();
            return G3D_FALHA;
        }
    /* D387: ¿llega la copia del apunte a la VRAM? Si no, la valla se lee del registro como antes. */
    if (wb_ligada) {
        pz = prazo_ms(PRAZO_CURTO_MS);
        while (wb_apuntes()[1] != 0xDEADBEEFu && !prazo_vencido(&pz)) { }
        wb_ok = wb_apuntes()[1] == 0xDEADBEEFu;
    }
    klog("[GPU3D] r500: etapa 5: prueba de la cola superada (el CP lee la cola de la VRAM; RPTR %x); copia del apunte en la VRAM %x "
         "(RPTR en la VRAM %x); HOST_PATH_CNTL %x\n", rd(CP_RB_RPTR), wb_apuntes()[1], *(volatile uint32_t *)(zona_cpu + ANEL_BYTES), hdp_cntl);
    opc_log("al encender");

    /* Etapa 6: el estado fijo del 3D, con su valla. */
    etapa = 6;
    g3d_fila_t f;
    g3d_fila_iniciar(&f, prep, PREP_PAL);
    r5_inicio(&f, tuberias, fpus_familia[familia]);
    r5_ajustes_por(&f, &gpu3d_ajustes, G3D_AJ_INICIO);
    if (emitir(&f) != G3D_OK || sincronizar(PRAZO_CURTO_MS) != G3D_OK) {
        snformatar(m, (size_t)nm, "el CP no termina el estado fijo del 3D (%u palabras; apunte %x, RBBM_STATUS %x)", f.n,
                   rd(APUNTE_VALLA), rd(RBBM_STATUS));
        r5_detalle("estado fijo");
        r5_desligar();
        return G3D_FALHA;
    }
    klog("[GPU3D] r500: etapa 6: estado fijo del 3D aceptado (%u palabras); RBBM_STATUS %x\n", f.n, rd(RBBM_STATUS));

    /* Sin ventana de la CPU en la capa: r5_criar pone la de cada búfer si cae en la parte que se ve. */
    g3d_arena_iniciar(&arena, (uint64_t)mc_zona + ANEL_BYTES + RESERVA_BYTES, NULL, zona_bytes - ANEL_BYTES - RESERVA_BYTES);
    memset(&b_cor, 0, sizeof b_cor); memset(&b_z, 0, sizeof b_z); memset(b_tex, 0, sizeof b_tex); memset(&alvo, 0, sizeof alvo);
    memset(tex_fmt, 0, sizeof tex_fmt); memset(&b_zdest, 0, sizeof b_zdest); memset(fp_cache, 0, sizeof fp_cache);
    cur_cor = &b_cor; dest_vaga = -1; ntc_posto = 1; v2_sujo = false; fp_posto = -1; v2_ultimo = false;
    pronto = true;
    return G3D_OK;
}

/* D357: «etapa N (nombre): motivo», para que «gpu3d probar» diga dónde se paró sin mirar el log. */
static int r5_iniciar(const pci_disp_t *d, int geracao, const acel_tela_t *t, uint32_t variante, char *m, int nm)
{
    (void)variante;   /* en R5xx las órdenes van siempre en el anillo del CP */
    etapa = 0;
    dep_ler();
    int r = r5_iniciar_etapas(d, geracao, t, m, nm);
    if (r != G3D_OK && etapa >= 1) {
        char tx[160];
        snformatar(tx, sizeof tx, "etapa %d (%s): %s", etapa, nome_etapa(etapa), m);
        snformatar(m, (size_t)nm, "%s", tx);
        klog("[GPU3D] r500: el encendido se paro en la etapa %d (%s)\n", etapa, nome_etapa(etapa));
    }
    return r;
}

/* ------------------------------------------------------------ la memoria de la zona (D387, cuarta tanda) */

/* Un búfer de la zona; con la ventana de la CPU solo si cae entero en la parte que se ve siempre. */
static bool r5_criar(g3d_buffer_t *b, uint8_t tipo, uint64_t bytes)
{
    if (!g3d_buffer_criar(&arena, b, tipo, bytes, 4096)) return false;
    uint64_t off = b->gpu - mc_zona;
    b->cpu = off + b->bytes <= zona_vista ? zona_cpu + (uint32_t)off : NULL;
    return true;
}

/* La ventana móvil sobre [gpu, gpu + bytes) de la zona (bytes + desplazamiento en la página <= JANELA_BYTES):
 * se vuelven a apuntar sus páginas (con su invalidación del TLB) y se devuelve la dirección de «gpu». */
static volatile uint8_t *r5_janela_tipo(uint64_t gpu, uint32_t bytes, uint32_t tipo)
{
    uint64_t off = gpu - mc_zona;
    if (!janela_virt || off + bytes > zona_bytes) return NULL;
    uint32_t fis = zona_fis_map + (uint32_t)off, base = fis & ~0xFFFu, pags = ((fis & 0xFFFu) + bytes + 0xFFFu) >> 12;
    if (pags > JANELA_BYTES / 4096u) return NULL;
    for (uint32_t i = 0; i < pags; i++) janela_pagina(janela_virt + i * 4096u, base + i * 4096u, tipo);
    return (volatile uint8_t *)(janela_virt + (fis & 0xFFFu));
}
static volatile uint8_t *r5_janela(uint64_t gpu, uint32_t bytes) { return r5_janela_tipo(gpu, bytes, PG_PCD | PG_PWT); }

/* D432 (medido en el Amilo): subir una textura por la ventana sin caché va a unos 100 MB/s, y con una textura
 * por cuadro (lo que presentan los juegos) eso es lo que más tarda. Si el framebuffer ya va en write-combining
 * por la PAT (wc.c, entrada 1 = solo PWT), la subida usa la ventana móvil con ese mismo tipo (el framebuffer
 * del Amilo mide ~2 GB/s así). Solo para escribir texturas enteras: lo demás (cola, apuntes, lecturas) sigue
 * sin caché. Tras escribir, una instrucción con LOCK vacía los búferes de combinación antes de que la GPU lea.
 * «inicio 4ff0 40» en gpu3d-r500.conf lo quita. */
static bool r5_wc_pode(void)
{
    return janela_virt && wc_ativo() && wc_via()[0] == 'P' && !(r5_dep & R5D_SEM_WC);
}

/* De un búfer que la CPU no ve siempre: cuántas de las «resto» filas que quedan caben en la ventana. */
static int r5_filas_janela(const g3d_buffer_t *b, int resto)
{
    int n = (int)((JANELA_BYTES - 4096u) / b->passo);
    if (n < 1) n = 1;
    return n < resto ? n : resto;
}

/* ------------------------------------------------------------ lo que pide la capa */

static int r5_esperar(g3d_vala_t vala, uint32_t ms)
{
    (void)vala;   /* una sola cola: se espera a todo lo pendiente */
    if (!pronto) return G3D_FALHA;
    int r = sincronizar(ms);
    if (r != G3D_OK) r5_detalle("tiempo agotado");
    return r;
}

static void soltar_alvo(void)
{
    g3d_buffer_soltar(&arena, &b_cor);
    g3d_buffer_soltar(&arena, &b_z);
    g3d_buffer_soltar(&arena, &b_zdest);   /* D426: si se quedó dibujando en una textura */
    dest_vaga = -1; cur_cor = &b_cor;
    memset(&alvo, 0, sizeof alvo);
}

static int r5_alvo(int larg, int alt, int zbits)
{
    if (!pronto) return G3D_FALHA;
    if (larg < 8 || alt < 8 || larg > GPU3D_MAX_LADO || alt > GPU3D_MAX_LADO) return G3D_NAO;
    if (r5_esperar(0, GPU3D_PRAZO_MS) != G3D_OK) return G3D_FALHA;
    soltar_alvo();
    uint32_t passo_px = ((uint32_t)larg + 15) & ~15u;
    int zb = zbits == 16 ? 16 : 24;
    if (!r5_criar(&b_cor, G3D_BUF_ALVO, (uint64_t)passo_px * 4 * (uint32_t)alt)) return G3D_NAO;
    if (!r5_criar(&b_z, G3D_BUF_Z, (uint64_t)passo_px * (zb == 24 ? 4 : 2) * (uint32_t)alt)) {
        zb = 16;
        if (!r5_criar(&b_z, G3D_BUF_Z, (uint64_t)passo_px * 2 * (uint32_t)alt)) { soltar_alvo(); return G3D_NAO; }
    }
    b_cor.passo = passo_px * 4; b_cor.larg = larg; b_cor.alt = alt; b_cor.bpp = 32;
    b_z.passo = passo_px * (zb == 24 ? 4 : 2); b_z.larg = larg; b_z.alt = alt; b_z.bpp = zb == 24 ? 32 : 16;
    alvo.cor_gpu = (uint32_t)b_cor.gpu; alvo.z_gpu = (uint32_t)b_z.gpu;
    alvo.passo_px = passo_px; alvo.larg = larg; alvo.alt = alt; alvo.zbits = zb;
    klog("[GPU3D] r500: destino %dx%d (paso %u): color en MC %x, Z de %d bits en MC %x; libres %u KB\n", larg, alt, passo_px,
         alvo.cor_gpu, zb, alvo.z_gpu, (uint32_t)(g3d_arena_livre(&arena) >> 10));
    return G3D_OK;
}

/* Una entrega: destino, estado y los vértices, de R5_VERT_POR_PACOTE en R5_VERT_POR_PACOTE. */
static int desenhar_int(const gpu3d_estado_t *e, const r5_tex_t *t, const gpu3d_vert_t *v, int n)
{
    for (int i = 0; i < n; i += R5_VERT_POR_PACOTE) {
        int q = n - i < R5_VERT_POR_PACOTE ? n - i : R5_VERT_POR_PACOTE;
        g3d_fila_t f;
        g3d_fila_iniciar(&f, prep, PREP_PAL);
        if (i == 0) {
            if ((v2_sujo || ntc_posto != 1) && (r5_dep & R5D_SYNC)) {
                r5_reg(&f, R5_WAIT_UNTIL, R5_WAIT_3D_LIMPO);
                int rr = emitir(&f);
                if (rr == G3D_OK) rr = sincronizar(500);
                klog("[GPU3D] r500 v2 sinc: v1 tras v2 (ntc %d): antes: %s\n", ntc_posto, rr == G3D_OK ? "bien" : "CUELGA");
                if (rr != G3D_OK) return G3D_FALHA;
                g3d_fila_iniciar(&f, prep, PREP_PAL);
            }
            /* D451 (Amilo, Brumacanto): cambiar de v2 a v1 con el motor todavía dibujando lo de v2 colgaba la X1400
             * (GA ocupado para siempre; con cada parte sincronizada, R5D_SYNC, no pasaba): se vacían las cachés y se
             * espera al 3D parado y limpio antes de tocar el programa de píxeles y el resto del estado. */
            if (!(r5_dep & R5D_SEM_BARREIRA) && (v2_ultimo || (r5_dep & R5D_BARREIRA_TODAS))) r5_vaciar(&f);
            v2_ultimo = false;
            if (v2_sujo || ntc_posto != 1) {
                /* D426: lo que un dibujo v2 cambió y el estado de v1 no vuelve a poner. */
                if (ntc_posto != 1) r5_estado_vertices(&f, 1);
                r5_reg(&f, R5_US_PIXSIZE, R5_T_TEX);
                r5_reg(&f, R5_RB3D_ABLEND, 0);
                r5_reg(&f, R5_RB3D_ROPCNTL, 0);
                r5_reg(&f, R5_FG_ALPHA_FUNC, 0);
                r5_reg(&f, R5_ZB_STENCILREFMASK, 0);
                ntc_posto = 1; v2_sujo = false; pendente_v1_sync = true;
            }
            fp_posto = -1;   /* v1 carga su propio programa */
            r5_estado_alvo(&f, &alvo);
            r5_estado_desenho(&f, e, t);
            r5_ajustes_por(&f, &gpu3d_ajustes, G3D_AJ_DIBUJO);
        }
        bool sy1 = i == 0 && (r5_dep & R5D_SYNC) && pendente_v1_sync;
        r5_triangulos(&f, v + i, q);
        int r = emitir(&f);
        if (r != G3D_OK) return r;
        if (sy1) {
            pendente_v1_sync = false;
            r = sincronizar(500);
            klog("[GPU3D] r500 v2 sinc: v1 tras v2: dibujo: %s\n", r == G3D_OK ? "bien" : "CUELGA");
            if (r != G3D_OK) return G3D_FALHA;
        }
    }
    return G3D_OK;
}

/* Borrar es dibujar un rectángulo que siempre pasa, con el Z al fondo. */
static int r5_limpar(uint32_t mascara, uint32_t cor)
{
    if (!pronto || !b_cor.usado) return G3D_FALHA;
    if (!(mascara & 3)) return G3D_OK;
    gpu3d_estado_t e = { (mascara & 1 ? 0 : R5_INT_SEM_COR) | (mascara & 2 ? R5_INT_Z_FORCAR : 0), G3D_Z_SEMPRE, G3D_F_UM, G3D_F_ZERO, 0 };
    uint32_t x1 = g3d_float_de_fixo(alvo.larg, 0), y1 = g3d_float_de_fixo(alvo.alt, 0), um = 0x3F800000u;
    uint32_t c = 0xFF000000u | (cor & 0x00FFFFFFu);
    gpu3d_vert_t q[6] = {
        { 0, 0, um, um, c, 0, 0, 0 }, { x1, 0, um, um, c, 0, 0, 0 }, { x1, y1, um, um, c, 0, 0, 0 },
        { 0, 0, um, um, c, 0, 0, 0 }, { x1, y1, um, um, c, 0, 0, 0 }, { 0, y1, um, um, c, 0, 0, 0 },
    };
    int r = desenhar_int(&e, NULL, q, 6);
    if (r != G3D_OK) r5_detalle("al limpiar");
    return r;
}

static int r5_textura(int vaga, int larg, int alt, const uint32_t *argb)
{
    if (!pronto) return G3D_FALHA;
    if (vaga < 0 || vaga >= GPU3D_TEXTURAS) return G3D_NAO;
    g3d_buffer_t *b = &b_tex[vaga];
    if (!b->usado && !argb) return G3D_OK;
    if (vaga == dest_vaga) return G3D_NAO;   /* D426: es el destino actual (la capa ya no lo deja) */
    /* Puede estar en uso por un dibujo pendiente. */
    if (r5_esperar(0, GPU3D_PRAZO_MS) != G3D_OK) return G3D_FALHA;
    g3d_buffer_soltar(&arena, b);
    tex_fmt[vaga] = G3D_FMT_ARGB8888;
    if (!argb) return G3D_OK;
    if (larg < 1 || alt < 1 || larg > GPU3D_MAX_LADO || alt > GPU3D_MAX_LADO) return G3D_NAO;
    uint32_t passo = (((uint32_t)larg + 15) & ~15u) * 4;
    if (!r5_criar(b, G3D_BUF_TEXTURA, (uint64_t)passo * (uint32_t)alt)) return G3D_NAO;
    b->passo = passo; b->larg = larg; b->alt = alt; b->bpp = 32;
    bool wc = r5_wc_pode();   /* D432 */
    if (wc) tex_wc_n++; else tex_uc_n++;
    for (int y = 0; y < alt;) {
        /* D387: fuera de la parte que se ve siempre, por la ventana móvil, a trozos de filas. Con
         * write-combining (D432), siempre por la ventana móvil, que es la que lleva ese tipo. */
        bool fixa = b->cpu && !wc;
        int n = fixa ? alt - y : r5_filas_janela(b, alt - y);
        volatile uint8_t *base = fixa ? (volatile uint8_t *)b->cpu + (uint32_t)y * passo
                                      : r5_janela_tipo(b->gpu + (uint32_t)y * passo, (uint32_t)n * passo, wc ? PG_PWT : PG_PCD | PG_PWT);
        if (!base) { g3d_buffer_soltar(&arena, b); return G3D_NAO; }
        for (int k = 0; k < n; k++, y++) {
            if (wc) { memcpy((void *)(base + (uint32_t)k * passo), argb + y * larg, (uint32_t)larg * 4u); continue; }
            volatile uint32_t *dst = (volatile uint32_t *)(base + (uint32_t)k * passo);
            for (int x = 0; x < larg; x++) dst[x] = argb[y * larg + x];
        }
    }
    __asm__ volatile("lock; orl $0, (%%esp)" ::: "memory");
    /* La ventana móvil no se queda en write-combining: quien la use después la pide con su tipo, pero una
     * lectura por un puntero viejo no debe encontrarla así. */
    if (wc) r5_janela(b->gpu, 4096u < (uint32_t)b->bytes ? 4096u : (uint32_t)b->bytes);
    return G3D_OK;
}

static int r5_desenhar(const gpu3d_estado_t *e, const gpu3d_vert_t *v, int n, g3d_vala_t *vala)
{
    if (!pronto || !b_cor.usado) return G3D_FALHA;
    if (n <= 0 || n % 3) return G3D_NAO;
    gpu3d_estado_t ee = *e;
    ee.flags &= 0xFFu;   /* los flags internos no vienen de fuera */
    r5_tex_t tex, *pt = NULL;
    if (ee.flags & G3D_TEXTURA) {
        const g3d_buffer_t *b = ee.textura < GPU3D_TEXTURAS ? &b_tex[ee.textura] : NULL;
        if (!b || !b->usado || tex_fmt[ee.textura] != G3D_FMT_ARGB8888) return G3D_NAO;
        /* Repetir solo vale con lados potencia de 2. */
        if ((ee.flags & G3D_TEX_REPETE_S) && (b->larg & (b->larg - 1))) return G3D_NAO;
        if ((ee.flags & G3D_TEX_REPETE_T) && (b->alt & (b->alt - 1))) return G3D_NAO;
        tex.gpu = (uint32_t)b->gpu; tex.passo_px = b->passo / 4; tex.larg = b->larg; tex.alt = b->alt;
        pt = &tex;
    }
    int r = desenhar_int(&ee, pt, v, n);
    if (r != G3D_OK) { r5_detalle("al dibujar"); return r; }
    if (vala) *vala = seq + 1;   /* la valla que llevará el próximo aviso */
    return G3D_OK;
}

static int r5_ler(uint32_t *dest, int passo_px)
{
    if (!pronto || !b_cor.usado) return G3D_FALHA;
    if (r5_esperar(0, GPU3D_PRAZO_MS) != G3D_OK) return G3D_FALHA;
    /* D391: con el CP parado, el motor 2D (acel_radeon.c, GART del volcado) escribe el cuadro en la RAM del
     * programa y la CPU no lee la VRAM. Si no se puede (apagado, destino que no sirve, un fallo), la CPU. */
    const g3d_buffer_t *bc = cur_cor;   /* D426: el destino actual (el principal o una textura) */
    acel_origem_t o = { alvo.cor_gpu, (const uint32_t *)bc->cpu, bc->passo, 0 };
    gpu3d_respirar_bloquear(true);   /* D436: la vuelta por la GART cierra las interrupciones a propósito */
    bool por_gart = acel_retornar(&o, alvo.larg, alvo.alt, dest, passo_px);
    gpu3d_respirar_bloquear(false);
    if (por_gart) return G3D_OK;
    for (int y = 0; y < alvo.alt;) {
        int n = bc->cpu ? alvo.alt - y : r5_filas_janela(bc, alvo.alt - y);
        const volatile uint8_t *base = bc->cpu ? (const volatile uint8_t *)bc->cpu + (uint32_t)y * bc->passo
                                               : r5_janela(bc->gpu + (uint32_t)y * bc->passo, (uint32_t)n * bc->passo);
        if (!base) return G3D_FALHA;
        for (int k = 0; k < n; k++, y++) {
            const volatile uint32_t *src = (const volatile uint32_t *)(base + (uint32_t)k * bc->passo);
            uint32_t *d = dest + y * passo_px;
            for (int x = 0; x < alvo.larg; x++) d[x] = src[x] & 0x00FFFFFFu;
            /* D436: leer la VRAM por la CPU es lento (en el Amilo, 1024x768 tarda ~255 ms): sin esto, el mezclador
             * del kernel se quedaba sin turno y la tarjeta cortaba. La ventana (r5_janela) no cambia al ceder: es
             * de este destino, y otro hilo del dueño no entra (gpu3d_op). */
            if (!(k & 15)) gpu3d_respirar();
        }
    }
    return G3D_OK;
}

static void r5_memoria(uint32_t *total_kb, uint32_t *livre_kb)
{
    *total_kb = pronto ? (uint32_t)(arena.bytes >> 10) : 0;
    *livre_kb = pronto ? (uint32_t)(g3d_arena_livre(&arena) >> 10) : 0;
}

/* D390: lo que la CPU ve del destino, para las sentinelas y el análisis de la capa. El color
 * es ARGB8888 lineal; el Z de 24 bits va en los bits altos (la plantilla abajo). */
static void r5_vistas(g3d_vista_t *c, g3d_vista_t *z)
{
    if (!pronto || !b_cor.usado || !b_z.usado || !b_cor.cpu || !b_z.cpu || dest_vaga >= 0) return;
    *c = (g3d_vista_t){ b_cor.cpu, b_cor.passo, alvo.larg, alvo.alt, 32, 0, 0, false };
    *z = (g3d_vista_t){ b_z.cpu, b_z.passo, alvo.larg, alvo.alt, (uint8_t)(alvo.zbits == 16 ? 16 : 32),
                        (uint8_t)(alvo.zbits == 16 ? 16 : 24), (uint8_t)(alvo.zbits == 16 ? 0 : 8), false };
}

/* ------------------------------------------------------------ gpu3d v2 (D426) */

/* Lo que decide el programa de fragmentos, en bytes (la clave de la caché). */
static void fp_desc(const gpu3d_estado2_t *e, uint8_t d[R5_FP_DESC])
{
    memset(d, 0, R5_FP_DESC);
    int nu = e->unidades > 4 ? 4 : e->unidades, k = 0;
    d[k++] = (uint8_t)nu;
    d[k++] = (e->flags & G3D2_NEBLINA) ? 1 : 0;
    for (int u = 0; u < nu; u++) {
        const gpu3d_unidade_t *x = &e->u[u];
        d[k++] = x->ativa;
        if (!x->ativa) { k += 16; continue; }
        d[k++] = x->coord; d[k++] = x->rgb_func; d[k++] = x->alfa_func; d[k++] = x->rgb_escala; d[k++] = x->alfa_escala;
        for (int j = 0; j < 3; j++) { d[k++] = x->rgb_fonte[j]; d[k++] = x->rgb_opd[j]; }
        for (int j = 0; j < 3; j++) { d[k++] = (uint8_t)((x->alfa_fonte[j] << 4) | x->alfa_opd[j]); }
        k += 2;
    }
}

/* El programa de un estado: de la caché o generado (y guardado en la menos usada). */
static int programa(const gpu3d_estado2_t *e)
{
    uint8_t d[R5_FP_DESC];
    fp_desc(e, d);
    fp_relogio++;
    for (int i = 0; i < R5_FP_CACHE; i++)
        if (fp_cache[i].usado && !memcmp(fp_cache[i].desc, d, R5_FP_DESC)) { fp_cache[i].uso = fp_relogio; fp_acertos++; return i; }
    int livre = -1;
    for (int i = 0; i < R5_FP_CACHE && livre < 0; i++) if (!fp_cache[i].usado) livre = i;
    if (livre < 0) {
        livre = 0;
        for (int i = 1; i < R5_FP_CACHE; i++) if (fp_cache[i].uso < fp_cache[livre].uso) livre = i;
    }
    if (fp_posto == livre) fp_posto = -1;
    fp_cache[livre].usado = true; fp_cache[livre].uso = fp_relogio;
    memcpy(fp_cache[livre].desc, d, R5_FP_DESC);
    r5_fp_gerar(&fp_cache[livre].fp, e);
    if (r5_dep & R5D_SEM_TODAS)   /* depuración: los semáforos en todas las lecturas (el fallo de la X1400) */
        for (int k = 0; k < fp_cache[livre].fp.n; k++)
            if ((fp_cache[livre].fp.p[k * R5_FP_PAL_INST] & 3u) == R5_FP_TIPO_TEX) {
                fp_cache[livre].fp.p[k * R5_FP_PAL_INST] |= R5_FP_ESPERA_TEX;
                fp_cache[livre].fp.p[k * R5_FP_PAL_INST + 1] |= R5_FP_TEX_PEDIR;
            }
    fp_gerados++;
    if (fp_cache[livre].fp.erro) klog("[GPU3D] r500 v2: un programa de fragmentos no cabe (%d instrucciones): ese dibujo va por software\n",
                                      fp_cache[livre].fp.n);
    return livre;
}

static bool pot2(int v) { return v > 0 && !(v & (v - 1)); }

/* Un dibujo v2: estado y vértices, de r5_vert_por_pacote2 en r5_vert_por_pacote2. */
static int desenhar2_int(const gpu3d_estado2_t *e, const gpu3d_vert2_t *v, int n)
{
    if (!pronto || !b_cor.usado || !alvo.cor_gpu) return G3D_FALHA;
    if (n <= 0 || n % 3) return G3D_NAO;
    r5_tex_t tx[4];
    uint32_t mask = 0;
    int nu = e->unidades > 4 ? 4 : e->unidades;
    for (int u = 0; u < nu; u++) {
        const gpu3d_unidade_t *un = &e->u[u];
        if (!un->ativa) continue;
        if (un->vaga >= GPU3D_TEXTURAS || (int)un->vaga == dest_vaga || un->tipo != G3D_TEX_2D) return G3D_NAO;
        const g3d_buffer_t *b = &b_tex[un->vaga];
        if (!b->usado || (un->filtro & (G3D_FIL_MIP_VECINO | G3D_FIL_MIP_LINEAR))) return G3D_NAO;
        /* Como en v1: repetir o reflejar solo con lados potencia de 2 (R5xx, con el paso propio). */
        if ((un->env_s != G3D_ENV_BORDE && !pot2(b->larg)) || (un->env_t != G3D_ENV_BORDE && !pot2(b->alt))) return G3D_NAO;
        static const uint32_t f1[4] = { 0, R5_TX_DXT1, R5_TX_DXT3, R5_TX_DXT5 };
        tx[u] = (r5_tex_t){ (uint32_t)b->gpu, b->passo / 4, b->larg, b->alt, f1[tex_fmt[un->vaga] & 3] };
        mask |= 1u << u;
    }
    int pi = programa(e);
    const r5_fp_t *fp = &fp_cache[pi].fp;
    if (fp->erro || fp->n < 1) return G3D_NAO;
    int por = r5_vert_por_pacote2(fp->ntc);
    for (int i = 0; i < n; i += por) {
        int q = n - i < por ? n - i : por;
        g3d_fila_t f;
        g3d_fila_iniciar(&f, prep, PREP_PAL);
        if (i == 0) {
            bool sy = (r5_dep & R5D_SYNC) != 0;
            /* D451: lo mismo al pasar de v1 a v2 y al cambiar de programa o de formato de vértices en v2. */
            if (!(r5_dep & R5D_SEM_BARREIRA) && (!v2_ultimo || fp_posto != pi || fp->ntc != ntc_posto || (r5_dep & R5D_BARREIRA_TODAS)))
                r5_vaciar(&f);
            v2_ultimo = true;
            /* D426 (depuración): cada parte con su valla, para ver cuál cuelga el CP. */
            #define R5_PARTE(nome) do { if (sy) { int rr_ = emitir(&f); if (rr_ == G3D_OK) rr_ = sincronizar(500); \
                klog("[GPU3D] r500 v2 sinc: %s: %s (WPTR %x)\n", nome, rr_ == G3D_OK ? "bien" : "CUELGA", wptr); \
                if (rr_ != G3D_OK) { fp_posto = -1; return G3D_FALHA; } g3d_fila_iniciar(&f, prep, PREP_PAL); } } while (0)
            if (sy) klog("[GPU3D] r500 v2 sinc: dibujo n %d ntc %d (motor %d) unidades %x programa %d inst, temp %d, consts %x, flags %x\n",
                         n, fp->ntc, ntc_posto, mask, fp->n, fp->max_temp, fp->consts, e->flags);
            if (fp->ntc != ntc_posto) {
                if (sy) {
                    /* Por partes: espera y vaciado, PSC, programa de vértices, formato de salida, RS. */
                    static uint32_t tmp[512];
                    g3d_fila_t t;
                    g3d_fila_iniciar(&t, tmp, 512);
                    r5_estado_vertices(&t, fp->ntc);
                    static const char *const nomes[5] = { "vertices: espera y PVS_FLUSH", "vertices: PSC", "vertices: programa",
                                                          "vertices: formato", "vertices: RS" };
                    int parte = 0;
                    for (uint32_t k = 0; k + 1 < t.n; k += 2) {
                        uint32_t reg = (t.p[k] & 0xFFFFu) << 2;
                        int np = reg == R5_VAP_PSC_0 ? 1 : reg == R5_VAP_PVS_INDICE ? 2 : reg == R5_VAP_OUT_VTX_FMT_0 ? 3 :
                                 reg == R5_RS_COUNT ? 4 : parte;
                        if (np != parte) { R5_PARTE(nomes[parte]); parte = np; }
                        g3d_por(&f, t.p[k]); g3d_por(&f, t.p[k + 1]);
                    }
                    R5_PARTE(nomes[parte]);
                } else r5_estado_vertices(&f, fp->ntc);
                ntc_posto = fp->ntc;
            }
            r5_estado_alvo(&f, &alvo);
            R5_PARTE("destino");
            for (int u = 0; u < nu; u++) if (mask & (1u << u)) r5_estado_unidade(&f, (uint32_t)u, &tx[u], &e->u[u]);
            r5_reg(&f, R5_TX_INVALTAGS, 0);
            r5_reg(&f, R5_TX_ENABLE, mask);
            R5_PARTE("unidades");
            r5_fp_constantes(&f, fp, e);
            R5_PARTE("constantes");
            if (fp_posto != pi) { r5_fp_carregar(&f, fp); fp_posto = pi; fp_cargas++; }
            R5_PARTE("programa");
            r5_estado_fragmento(&f, e, alvo.larg, alvo.alt, alvo.zbits == 24);
            r5_ajustes_por(&f, &gpu3d_ajustes, G3D_AJ_DIBUJO);
            R5_PARTE("fragmento");
            v2_sujo = true;
        }
        r5_triangulos2(&f, v + i, q, fp->ntc, fp->mapa);
        int r = emitir(&f);
        if (r != G3D_OK) { fp_posto = -1; return r; }
        if (r5_dep & R5D_SYNC) {
            r = sincronizar(500);
            klog("[GPU3D] r500 v2 sinc: triangulos (%d): %s\n", q, r == G3D_OK ? "bien" : "CUELGA");
            if (r != G3D_OK) { fp_posto = -1; return G3D_FALHA; }
        }
        #undef R5_PARTE
    }
    return G3D_OK;
}

static int r5_desenhar2(const gpu3d_estado2_t *e, const gpu3d_vert2_t *v, int n, g3d_vala_t *vala)
{
    int r = desenhar2_int(e, v, n);
    if (r == G3D_FALHA) { r5_detalle("al dibujar (v2)"); return r; }
    if (r == G3D_OK && vala) *vala = seq + 1;
    return r;
}

/* Borrar con máscaras, tijera, un Z y un esténcil dados: un rectángulo con ese estado. */
static int r5_limpar2(const gpu3d_limpar2_t *l)
{
    if (!pronto || !b_cor.usado) return G3D_FALHA;
    gpu3d_estado2_t e;
    memset(&e, 0, sizeof e);
    e.versao = 2; e.func_z = G3D_CMP_SEMPRE; e.func_alfa = G3D_CMP_SEMPRE; e.op_logica = G3D_LOG_COPIA;
    e.mez_orig_rgb = e.mez_orig_alfa = G3D_F_UM; e.mez_dest_rgb = e.mez_dest_alfa = G3D_F_ZERO;
    e.mascara_cor = (l->mascara & G3D_LIMPA_COR) ? l->mascara_cor : 0;
    if (l->mascara & G3D_LIMPA_Z) e.flags |= G3D2_Z_TESTE | G3D2_Z_ESCRITA;
    if ((l->mascara & G3D_LIMPA_EST) && alvo.zbits == 24) {
        e.flags |= G3D2_ESTENCIL;
        e.est_func = G3D_CMP_SEMPRE; e.est_ref = (uint8_t)l->estencil; e.est_mascara = 0xFF; e.est_escrita = l->mascara_est;
        e.est_falha = e.est_zfalha = e.est_zpassa = G3D_EST_SUBSTITUIR;
    }
    if (!e.mascara_cor && !(e.flags & (G3D2_Z_TESTE | G3D2_ESTENCIL))) return G3D_OK;
    if (l->tijera) {
        if (!l->tl || !l->ta) return G3D_OK;
        e.flags |= G3D2_TIJERA; e.tijera_x = l->tx; e.tijera_y = l->ty; e.tijera_l = l->tl; e.tijera_a = l->ta;
    }
    uint32_t x1 = g3d_float_de_fixo(alvo.larg, 0), y1 = g3d_float_de_fixo(alvo.alt, 0), um = 0x3F800000u;
    uint32_t z = l->z >= 0xFFFFFFu ? um : g3d_float_de_fixo((int32_t)l->z, 24);
    gpu3d_vert2_t q[6];
    memset(q, 0, sizeof q);
    const uint32_t xs[6] = { 0, x1, x1, 0, x1, 0 }, ys[6] = { 0, 0, y1, 0, y1, y1 };
    for (int i = 0; i < 6; i++) { q[i].x = xs[i]; q[i].y = ys[i]; q[i].z = z; q[i].rhw = um; q[i].cor = l->cor; }
    int r = desenhar2_int(&e, q, 6);
    if (r == G3D_FALHA) r5_detalle("al limpiar (v2)");
    return r;
}

/* Texturas v2: ARGB como en v1; DXT1/3/5 tal cual (bloques de 4x4), con el paso de 16 texels (32 o 64 bytes
 * por fila de bloques). Solo 2D y un nivel (sin mipmaps en R5xx de momento). */
static int r5_textura2(const gpu3d_textura2_t *t)
{
    if (!pronto) return G3D_FALHA;
    if (t->vaga >= GPU3D_TEXTURAS) return G3D_NAO;
    if (!t->dados) return r5_textura((int)t->vaga, 0, 0, NULL);
    if (t->tipo != G3D_TEX_2D || t->nivel || t->face || t->larg < 1 || t->alt < 1 || t->larg > GPU3D_MAX_LADO || t->alt > GPU3D_MAX_LADO)
        return G3D_NAO;
    if (t->formato == G3D_FMT_ARGB8888) return r5_textura((int)t->vaga, (int)t->larg, (int)t->alt, (const uint32_t *)(uintptr_t)t->dados);
    if (t->formato > G3D_FMT_DXT5 || (int)t->vaga == dest_vaga) return G3D_NAO;
    g3d_buffer_t *b = &b_tex[t->vaga];
    if (r5_esperar(0, GPU3D_PRAZO_MS) != G3D_OK) return G3D_FALHA;
    g3d_buffer_soltar(&arena, b);
    tex_fmt[t->vaga] = G3D_FMT_ARGB8888;
    uint32_t bs = t->formato == G3D_FMT_DXT1 ? 8u : 16u, passo_px = (t->larg + 15u) & ~15u;
    uint32_t filas = (t->alt + 3u) / 4u, fila_b = passo_px / 4u * bs, orig_b = (t->larg + 3u) / 4u * bs;
    if (!r5_criar(b, G3D_BUF_TEXTURA, (uint64_t)fila_b * filas)) return G3D_NAO;
    b->passo = passo_px * 4; b->larg = (int)t->larg; b->alt = (int)t->alt; b->bpp = t->formato == G3D_FMT_DXT1 ? 4 : 8;
    const uint32_t *o = (const uint32_t *)(uintptr_t)t->dados;
    g3d_buffer_t tmp = *b;
    tmp.passo = fila_b;   /* para la ventana móvil: filas de bloques */
    for (uint32_t y = 0; y < filas;) {
        uint32_t n = b->cpu ? filas - y : (uint32_t)r5_filas_janela(&tmp, (int)(filas - y));
        volatile uint8_t *base = b->cpu ? (volatile uint8_t *)b->cpu + y * fila_b : r5_janela(b->gpu + y * fila_b, n * fila_b);
        if (!base) { g3d_buffer_soltar(&arena, b); return G3D_NAO; }
        for (uint32_t k = 0; k < n; k++, y++) {
            volatile uint32_t *d = (volatile uint32_t *)(base + k * fila_b);
            const uint32_t *s = o + y * (orig_b / 4u);
            for (uint32_t x = 0; x < orig_b / 4u; x++) d[x] = s[x];
        }
    }
    __asm__ volatile("lock; orl $0, (%%esp)" ::: "memory");
    tex_fmt[t->vaga] = (uint8_t)t->formato;
    return G3D_OK;
}

/* Dibujar en una textura ARGB (su nivel 0) con un Z propio (de 16 bits, o de 24 con esténcil), o volver al
 * destino principal. Al cambiar, las cachés de color y Z del 3D se vacían y se espera al motor (la textura se
 * va a leer después). */
static int r5_destino(int vaga, int zbits, int estencil)
{
    if (!pronto || !b_cor.usado) return G3D_FALHA;
    if (dest_vaga >= 0) {
        g3d_fila_t f;
        uint32_t b[16];
        g3d_fila_iniciar(&f, b, 16);
        r5_vaciar(&f);
        if (emitir(&f) != G3D_OK || r5_esperar(0, GPU3D_PRAZO_MS) != G3D_OK) return G3D_FALHA;
        g3d_buffer_soltar(&arena, &b_zdest);
        alvo = alvo_princ; cur_cor = &b_cor; dest_vaga = -1;
    }
    if (vaga < 0) return G3D_OK;
    if (vaga >= GPU3D_TEXTURAS) return G3D_NAO;
    g3d_buffer_t *t = &b_tex[vaga];
    if (!t->usado || tex_fmt[vaga] != G3D_FMT_ARGB8888 || t->larg < 8 || t->alt < 8) return G3D_NAO;
    int zb = (zbits == 24 || estencil) ? 24 : 16;
    uint32_t passo_px = t->passo / 4;
    if (!r5_criar(&b_zdest, G3D_BUF_Z, (uint64_t)passo_px * (zb == 24 ? 4u : 2u) * (uint32_t)t->alt)) return G3D_NAO;
    b_zdest.passo = passo_px * (zb == 24 ? 4u : 2u); b_zdest.larg = t->larg; b_zdest.alt = t->alt; b_zdest.bpp = zb == 24 ? 32 : 16;
    alvo_princ = alvo;
    alvo.cor_gpu = (uint32_t)t->gpu; alvo.z_gpu = (uint32_t)b_zdest.gpu; alvo.passo_px = passo_px;
    alvo.larg = t->larg; alvo.alt = t->alt; alvo.zbits = zb;
    cur_cor = t; dest_vaga = vaga;
    return G3D_OK;
}

/* ------------------------------------------------------------ D432: presentar en la pantalla
 *
 * La GPU copia (escalando) el destino del 3D, o una textura, a la superficie que muestra el CRTC: la
 * pantalla del VBE está en la misma VRAM (mc_tela, en la X1400 del Amilo MC c0000000) y es ARGB8888
 * lineal, así que sirve de destino de color tal cual, con su paso. Es un dibujo v1 normal (un
 * rectángulo con textura, sin Z ni mezcla) con el destino cambiado mientras dura:
 *   1. vaciar las cachés de color y Z y esperar al motor (r5_vaciar: el vaciado SIEMPRE delante de la
 *      espera, D426), para que lo dibujado esté en la VRAM antes de leerlo como textura;
 *   2. destino = la pantalla (el Z no se usa: ni prueba ni escritura); bandas negras si se piden;
 *      el rectángulo con la fuente como textura;
 *   3. la valla de siempre (vacía las cachés sobre la pantalla y espera, con plazo) y el destino vuelve
 *      a ser el del programa (cada dibujo lleva el suyo).
 * No se toca ningún registro de la pantalla: solo se leen (CP parado) la superficie y el paso del
 * controlador, por si cambiaron desde el encendido, y con G3D_APRES_VSYNC el bit de retrazado. La
 * ventana móvil de la CPU no interviene: la CPU no lee ni escribe nada aquí. */
#define D1CRTC_STATUS        0x609C            /* AVIVO: bit 0 = en el retrazado vertical */
#define APRES_VBLANK_MS      20

static bool r5_tela_serve(void)
{
    if (!pronto || !cp_ligado || !mc_tela || tela_bytes_px != 4 || tela_l < 8 || tela_a < 8 || tela_l > 4080 || tela_a > 4080 ||
        tela_passo_px < (uint32_t)tela_l || tela_passo_px > 8191u) return false;
    return (int)video_largura() == tela_l && (int)video_altura() == tela_a && video_pitch() == tela_passo_px * 4u &&
           video_formato_direto() && video_bpp() == 32;
}

static int r5_apresentar(const g3d_apres_t *p)
{
    if (!p) return r5_tela_serve() ? G3D_OK : G3D_NAO;
    if (!pronto) return G3D_FALHA;
    if (!r5_tela_serve() || !b_cor.usado || dest_vaga >= 0) return G3D_NAO;
    const g3d_buffer_t *src = &b_cor;
    if (p->vaga >= 0) {
        if (p->vaga >= GPU3D_TEXTURAS || !b_tex[p->vaga].usado || tex_fmt[p->vaga] != G3D_FMT_ARGB8888) return G3D_NAO;
        src = &b_tex[p->vaga];
    }
    if (src->larg < 1 || src->alt < 1 || src->larg > GPU3D_MAX_LADO || src->alt > GPU3D_MAX_LADO) return G3D_NAO;
    /* 1. Lo pendiente, a la VRAM, y el motor parado (se manda con el resto: el CP aún no está avisado). */
    uint32_t b[16];
    g3d_fila_t f;
    g3d_fila_iniciar(&f, b, 16);
    r5_vaciar(&f);
    int r = emitir(&f);
    if (r != G3D_OK) return r;
    /* Con el CP parado (nada avisado desde la última valla): ¿la pantalla sigue donde estaba? */
    if (seq_vista == seq) {
        uint32_t sup = rd(D1GRPH_SUPERFICIE + tela_ctl), pit = rd(D1GRPH_PITCH + tela_ctl);
        if (sup != mc_tela || pit != tela_passo_px) {
            klog("[GPU3D] r500 presentar: la pantalla cambio (superficie %x, antes %x; pitch %u, antes %u): no se presenta\n",
                 sup, mc_tela, pit, tela_passo_px);
            mc_tela = 0;
            return G3D_NAO;
        }
    }
    /* 2. El destino es la pantalla mientras dura (desenhar_int y la valla usan «alvo»). */
    r5_alvo_t guardado = alvo;
    alvo.cor_gpu = mc_tela; alvo.passo_px = tela_passo_px; alvo.larg = tela_l; alvo.alt = tela_a;
    gpu3d_vert_t q[6];
    if (p->flags & G3D_APRES_BANDAS) {
        int rr[4][4], n = g3d_apres_bandas(p, tela_l, tela_a, rr);
        gpu3d_estado_t e = { 0, G3D_Z_SEMPRE, G3D_F_UM, G3D_F_ZERO, 0 };
        for (int i = 0; i < n && r == G3D_OK; i++) {
            g3d_quad_cor(rr[i][0], rr[i][1], rr[i][2], rr[i][3], 0xFF000000u, q);
            r = desenhar_int(&e, NULL, q, 6);
        }
    }
    if (r == G3D_OK) {
        gpu3d_estado_t e = { G3D_TEXTURA | (p->flags & G3D_APRES_LINEAR ? G3D_TEX_LINEAR : 0), G3D_Z_SEMPRE, G3D_F_UM, G3D_F_ZERO, 0 };
        r5_tex_t t = { (uint32_t)src->gpu, src->passo / 4, src->larg, src->alt, 0 };
        g3d_apres_quad(p, src->larg, src->alt, q);
        r = desenhar_int(&e, &t, q, 6);
    }
    /* Desgarro: si se pide, se avisa al CP al empezar el retrazado (solo se lee el estado del CRTC, con
     * plazo; si no llega, se presenta igual). */
    if (r == G3D_OK && (p->flags & G3D_APRES_VSYNC) && !(rd(D1CRTC_STATUS + tela_ctl) & 1u)) {
        prazo_t pz = prazo_ms(APRES_VBLANK_MS);
        while (!(rd(D1CRTC_STATUS + tela_ctl) & 1u)) {
            if (prazo_vencido(&pz)) { apres_vblank_fora++; break; }
            pausa_us(50);
            gpu3d_respirar();
        }
    }
    /* 3. La valla: cachés a la pantalla y motor parado, con plazo. */
    if (r == G3D_OK) r = sincronizar(GPU3D_PRAZO_MS);
    alvo = guardado;
    if (r != G3D_OK) { r5_detalle("al presentar en la pantalla"); return r; }
    if (!apres_n++)
        klog("[GPU3D] r500 presentar (D432): primera copia a la pantalla: fuente MC %x paso %u (%dx%d) -> pantalla MC %x paso %u "
             "(%dx%d), destino %dx%d+%d+%d%s\n", (uint32_t)src->gpu, src->passo / 4, src->larg, src->alt, mc_tela, tela_passo_px,
             tela_l, tela_a, p->dl, p->da, p->dx, p->dy, p->flags & G3D_APRES_VSYNC ? ", con espera al retrazado" : "");
    return G3D_OK;
}

/* Lo que se anuncia: lo que la R5xx sabe hacer según este driver, y de eso solo lo que pasa las pruebas
 * (g3d_v2_provas.c). Ni mipmaps, ni cubos, ni NPOT con repetición (ver docs/gpu3d.md), ni proyectivas. */
static void r5_capacidades(gpu3d_capas_t *c)
{
    caps_v2 = 0;
    if (!pronto) return;
    /* D426 (Amilo, 2026-11-20): sin ESTENCIL ni LIMPAR2. Pasan la prueba pequeña, pero en la escena «sombra» de
     * es11demo la X1400 dejó rayas negras en el fondo y una sombra con huecos (5063 de 76800 píxeles distintos del
     * software): hasta saber por qué, el esténcil va por software (R5D_ESTENCIL lo vuelve a pedir para depurar). */
    uint32_t cand = G3D_CAP_V2 | G3D_CAP_COMBINAR | G3D_CAP_DOT3 | G3D_CAP_ALFA_TESTE | G3D_CAP_TIJERA |
                    G3D_CAP_MASCARAS | G3D_CAP_Z_FUNCS | G3D_CAP_MEZCLA_EXT | G3D_CAP_MEZCLA_SEP | G3D_CAP_MEZCLA_EQ |
                    G3D_CAP_LOGICA | G3D_CAP_NEBLINA | G3D_CAP_TEX_ESPELHO | G3D_CAP_TEX_ANISO | G3D_CAP_DESTINO_TEX;
    if (r5_dep & R5D_ESTENCIL) cand |= G3D_CAP_ESTENCIL | G3D_CAP_LIMPAR2;
    uint32_t un = (r5_dep & R5D_UMA) ? 1 : 4, fmts = 0xFu;
    if (r5_dep & R5D_SEM_V2) {
        klog("[GPU3D] r500 v2: sin v2 (depuracion)\n"); c->bits = 0; c->unidades = 1; c->formatos = 1;
                                c->max_lado = GPU3D_MAX_LADO; c->max_aniso = 1; c->zbits = 3; c->estencil_bits = 0; return; }
    static const g3d_v2_ops_t ops = { r5_alvo, r5_limpar, r5_textura, r5_textura2, r5_desenhar2, r5_limpar2, r5_ler, r5_destino };
    uint32_t ok = g3d_v2_provar(&ops, "r500", cand, &un, &fmts);
    caps_v2 = ok;
    c->bits = ok;
    c->unidades = ok ? un : 1;
    c->formatos = fmts;
    c->max_lado = GPU3D_MAX_LADO;
    c->max_aniso = (ok & G3D_CAP_TEX_ANISO) ? 16 : 1;
    c->zbits = 3;
    c->estencil_bits = (ok & G3D_CAP_ESTENCIL) ? 8 : 0;
}

const gpu3d_driver_t gpu3d_r500 = {
    "radeon-r500", r5_iniciar, r5_alvo, r5_limpar, r5_textura, r5_desenhar, r5_esperar, r5_ler, r5_desligar, r5_detalle, r5_memoria,
    "r500", r5_faixas_ajustes, R5_N_FAIXAS_AJUSTES, r5_vistas,
    r5_capacidades, r5_desenhar2, r5_textura2, r5_limpar2, r5_destino,
    r5_apresentar,
};
