// SPDX-License-Identifier: GPL-2.0-or-later
/* gpu3d.c - 3D por la GPU: la capa común (D285).
 *
 * SaguiGL (userland/libsgl) hace con la CPU lo que siempre hizo (matrices,
 * luces, recorte) y, si hay un driver 3D, manda aquí los triángulos ya en
 * coordenadas del destino. Esta capa:
 *   1. al arrancar solo mira qué tarjeta tiene la pantalla (la que ya eligió la
 *      capa 2D por el PCI) y si hay driver para ella: no toca la GPU;
 *   2. la primera vez que un programa abre el motor (o con «gpu3d probar» /
 *      «gpu3d encender») lo pone en marcha y lo somete a la autoprueba:
 *      limpiar, un rectángulo de color, Z, mezcla, textura, color interpolado y
 *      especular, leyendo píxeles concretos del cuadro, con tiempo máximo;
 *   3. solo si todo cuadra queda activo. Si no, o si después una orden no
 *      termina a tiempo (vigilante), lo apaga con el motivo en el log
 *      ([GPU3D] apagado: ...) y SaguiGL sigue por software: un fallo de la GPU
 *      no cuelga el sistema ni deja la pantalla sin dibujar;
 *   4. el motor es de un proceso cada vez (el destino y las texturas son
 *      suyos); los demás siguen por software hasta que lo suelte o termine.
 * La opción de arranque SGL_MOTOR=software (D158) o --gpu3d=no lo quitan.
 *
 * Las piezas que comparten los drivers (memoria de GPU por bloques, búferes y
 * coma flotante con enteros) están en gpu3d_mem.c, que también se prueba en el PC.
 *
 * D390: el diagnóstico es común a todos los drivers (gpu3d_diag.h): con «gpu3d probar
 * --detalle» o «--ver», antes de cada etapa de la autoprueba la CPU pone sentinelas en el
 * color y el Z del destino y, tras leerlo, el log dice cuánto sigue intacto y qué colores
 * dejó la GPU; con «--ver» se guarda además la imagen de cada etapa para que la orden la
 * enseñe en una ventana. Los ajustes de registros de /sistema/conf/gpu3d-<driver>.conf se
 * leen aquí, antes de encender, y se filtran con los rangos que declara el driver. */
#include "gpu3d.h"
#include "bootinfo.h"
#include "gui.h"
#include "video.h"
#include "console.h"
#include "cpu.h"
#include "info_hardware.h"
#include "memoria.h"
#include "processo.h"
#include "prazo.h"
#include "fpu.h"
#include "sensores_conv.h"
#include "sistema.h"
#include "texto.h"
#include "vfs.h"
#include "virgl.h"

static const gpu3d_driver_t *drv;
static pci_disp_t disp;
static int geracao;
static acel_tela_t tela;
static uint32_t estado = GPU3D_SEM_GPU;
static uint8_t cab;                       /* CAB_GPU3D */
static uint32_t variante;                 /* GPU3D_CAB_ANEL y GPU3D_CAB_GTT (D358) */
static char motivo[160] = "sin iniciar";
static char placa_nome[40];
static int dono;                          /* pid; 0 = libre */
static int alvo_l, alvo_a, alvo_z;
static gpu3d_estado_t est_atual;
/* gpu3d v2 (D425): capacidades del driver encendido, el estado v2, el destino actual (el principal
 * o un hueco de textura) y el tamaño del nivel 0 de cada hueco (para dibujar en él). */
static gpu3d_capas_t capas;
static gpu3d_estado2_t est2_atual;
static bool est2_valido;
static int dest_vaga = -1, dest_l, dest_a;
static uint16_t vaga_l[GPU3D_TEXTURAS], vaga_a[GPU3D_TEXTURAS];
/* La ABI v2 no cambia de tamaño (los drivers y SaguiGL la comparten; docs/gpu3d.md). */
_Static_assert(sizeof(gpu3d_capas_t) == 64 && sizeof(gpu3d_vert2_t) == 88 && sizeof(gpu3d_unidade_t) == 28 &&
               sizeof(gpu3d_estado2_t) == 168 && sizeof(gpu3d_textura2_t) == 48 && sizeof(gpu3d_limpar2_t) == 36,
               "gpu3d v2: tamaño de la ABI");
/* gpu3d v3 (D457): los búferes residentes de la GPU (contabilidad: la capa sabe qué hay, cuánto ocupa, cuánto
 * se subió y cuándo se usó por última vez, y expulsa el menos usado si uno nuevo no cabe), la transformación y
 * las luces del próximo DESENHAR3, y el resultado de la autoprueba v3. */
typedef struct { uint32_t bytes, cheio, seq; uint16_t max_idx; uint8_t uso; bool presente; } vb_t;
static vb_t vb[GPU3D_VBUFS];
static uint32_t vb_seq, vb_n, vb_bytes, n_vb_expulsos, n_vb_subidas, vb_kb_subidos, n_des3, n_vert3;
static gpu3d_transf_t transf_atual;
static gpu3d_luzes_t luzes_atuais;
static bool transf_valida, luzes_validas;
static uint32_t bits3_driver;             /* lo que anunció el driver (capas.bits3 queda a 0 si su autoprueba falla) */
static uint32_t prova3;                   /* 0 sin hacer, 1 bien, 2 | escena << 8: falló esa escena */
#define GPU3D_VBUF_MAX_BYTES (16u << 20)  /* un búfer */
#define GPU3D_VBUF_TROZO     (64u << 10)  /* lo que se copia de una vez (entre trozos, gpu3d_respirar) */
_Static_assert(sizeof(gpu3d_vbuf_t) == 32 && sizeof(gpu3d_atrib_t) == 8 && sizeof(gpu3d_transf_t) == 580 &&
               sizeof(gpu3d_luz_t) == 96 && sizeof(gpu3d_luzes_t) == 880 && sizeof(gpu3d_desenho3_t) == 80,
               "gpu3d v3: tamaño de la ABI");
static uint32_t n_tri, n_lotes, n_quadros, n_falhas, n_recusas, us_autoprova, n_desenhos_prova;
static uint32_t pontos, tri_s, kpix_s;
static uint32_t n_v4_des, n_v4_limpar, n_v4_subir, n_v4_ler, n_v4_prog, n_v4_rec;   /* D480 */
static bool desligando;
/* D432: presentaciones directas a la pantalla, las que el compositor no dejó hacer, veces que retiró la
 * pantalla, si hay que repintar las bandas negras y el último rectángulo de destino. */
static uint32_t n_apres, n_apres_nao, n_excl_perdidas;
static bool apres_bandas;
static int apres_dx, apres_dy, apres_dl, apres_da;

/* D390: diagnóstico común (gpu3d_diag.h). */
g3d_ajustes_t gpu3d_ajustes;
static uint32_t diag;                     /* GPU3D_PROVA_* de la «gpu3d probar» en curso (0: la autoprueba de siempre) */
static int ap_etapa = -1;                 /* la prueba de la autoprueba en curso */
typedef struct {
    int           estado;                 /* GPU3D_ETAPA_* */
    bool          com_imagem;
    g3d_analise_t an;
    int           x, y;
    uint32_t      esperado, obtido;
    uint32_t      px[GPU3D_VER_L * GPU3D_VER_A];
    uint16_t      z[GPU3D_VER_L * GPU3D_VER_A];
} etapa_vista_t;
static etapa_vista_t *etapas;             /* con GPU3D_PROVA_VER: una por prueba (~74 KB cada una) */

/* ------------------------------------------------------------ utilidades */

static inline uint64_t tsc(void)
{
    uint32_t lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

static uint32_t us_desde(uint64_t t0)
{
    uint32_t khz = hw_tsc_khz();
    return khz ? (uint32_t)sconv_div64((tsc() - t0) * 1000, khz) : 0;
}

/* ------------------------------------------------------------ apagar */

static void soltar_driver(void)
{
    if (desligando) return;
    desligando = true;
    gui_exclusiva_soltar(0, "el 3D se apaga");   /* D432: antes de que el driver deje de poder escribir */
    if (drv && drv->desligar) drv->desligar();
    desligando = false;
    dono = 0; alvo_l = alvo_a = 0;
    memset(vb, 0, sizeof vb); vb_n = vb_bytes = 0; transf_valida = luzes_validas = false;   /* D457: el driver ya soltó sus búferes */
}

static void diag_destino(const char *fase);
static void diag_etapa(uint32_t *buf);

/* El vigilante: la GPU falló. Software hasta «gpu3d encender». */
static void falha(const char *m)
{
    n_falhas++;
    klog("[GPU3D] apagado: %s\n", m);
    if (drv && drv->detalle) drv->detalle("tras el fallo");
    diag_destino("tras el fallo");
    soltar_driver();
    estado = GPU3D_FALHOU;
    snformatar(motivo, sizeof motivo, "%s: apagado, SaguiGL sigue por software (%s)", placa_nome, m);
}

void gpu3d_exclusiva_perdida(void) { n_excl_perdidas++; apres_bandas = true; }

/* acel_intel.c paró el ring (el 2D se apagó, a mano o por su vigilante). */
void gpu3d_anel_parado(void)
{
    if (desligando || drv != &gpu3d_intel || estado != GPU3D_ATIVO) return;
    klog("[GPU3D] el ring de la Intel se paro (lo apago el 2D): el 3D se apaga y se volvera a probar con el proximo uso\n");
    soltar_driver();
    estado = GPU3D_SIN_PROBAR;
    snformatar(motivo, sizeof motivo, "%s: el 2D paro el ring; se volvera a probar con el proximo uso", placa_nome);
}

/* ------------------------------------------------------------ la autoprueba */

#define AP_L 128
#define AP_A 96
#define AP_FUNDO 0x00102030u
_Static_assert(AP_L == GPU3D_VER_L && AP_A == GPU3D_VER_A, "las imagenes de --ver son el destino de la autoprueba");

static void vert(gpu3d_vert_t *v, int x, int y, int z_q16, uint32_t cor, uint32_t espec, int u_q16, int v_q16)
{
    v->x = g3d_float_de_fixo(x, 0);
    v->y = g3d_float_de_fixo(y, 0);
    v->z = g3d_float_de_fixo(z_q16, 16);
    v->rhw = 0x3F800000u;   /* 1.0 */
    v->cor = cor; v->espec = espec;
    v->u = g3d_float_de_fixo(u_q16, 16);
    v->v = g3d_float_de_fixo(v_q16, 16);
}

/* Un rectángulo (dos triángulos) de un color, con u, v de 0 a 1. */
static int quad(const gpu3d_estado_t *e, int x0, int y0, int x1, int y1, int z_q16, uint32_t cor, uint32_t espec)
{
    gpu3d_vert_t v[6];
    vert(&v[0], x0, y0, z_q16, cor, espec, 0, 0);
    vert(&v[1], x1, y0, z_q16, cor, espec, 65536, 0);
    vert(&v[2], x1, y1, z_q16, cor, espec, 65536, 65536);
    v[3] = v[0];
    v[4] = v[2];
    vert(&v[5], x0, y1, z_q16, cor, espec, 0, 65536);
    return drv->desenhar(e, v, 6, NULL);
}

typedef struct { int x, y; uint32_t esperado; int tol; } amostra_t;

static bool perto(uint32_t a, uint32_t b, int tol)
{
    for (int s = 0; s < 24; s += 8) {
        int d = (int)((a >> s) & 0xFF) - (int)((b >> s) & 0xFF);
        if (d < -tol || d > tol) return false;
    }
    return true;
}

/* Lee el cuadro y mira las muestras. false: alguna no cuadra (queda en r). */
static bool conferir(uint32_t *buf, const amostra_t *am, int n, info_gpu3d_prova_t *r, bool estragar)
{
    if (drv->ler(buf, AP_L) != G3D_OK) { r->x = r->y = -1; return false; }
    if (estragar) buf[am[0].y * AP_L + am[0].x] ^= 0x00F0F0u;
    diag_etapa(buf);
    for (int i = 0; i < n; i++) {
        uint32_t p = buf[am[i].y * AP_L + am[i].x] & 0x00FFFFFFu;
        if (!perto(p, am[i].esperado, am[i].tol)) {
            r->x = am[i].x; r->y = am[i].y; r->esperado = am[i].esperado; r->obtido = p;
            return false;
        }
    }
    return true;
}

static const char *const nomes_prova[GPU3D_N_PROVAS] = {
    "limpiar", "rectangulo de color", "Z", "mezcla", "textura", "color interpolado", "especular", "Z de 16 bits",
};

/* ------------------------------------------------------------ diagnóstico común (D390) */

/* Con caché (RAM del GTT de Intel, que exige CLFLUSH y SSE2): escribe y descarta las líneas,
 * tras escribir con la CPU y antes de leer lo que escribió la GPU. */
static void vaciar_cache(const g3d_vista_t *v)
{
    if (!v->cpu || !v->cache) return;
    uint32_t a = (uint32_t)v->cpu & ~31u, fim = (uint32_t)v->cpu + v->passo * (uint32_t)v->alt;
    __asm__ volatile("mfence" ::: "memory");
    for (; a < fim; a += 32) __asm__ volatile("clflush (%0)" :: "r"(a) : "memory");
    __asm__ volatile("mfence" ::: "memory");
}

static void vistas_atuais(g3d_vista_t *c, g3d_vista_t *z)
{
    memset(c, 0, sizeof *c);
    memset(z, 0, sizeof *z);
    if (drv && drv->vistas) drv->vistas(c, z);
}

/* Antes de una etapa (solo con --detalle o --ver): la sentinela en el color y en el Z. Si la
 * GPU no termina lo anterior, no se toca nada (la etapa lo dirá). */
static void sentinelas(void)
{
    if (!diag || drv->esperar(0, GPU3D_PRAZO_MS) != G3D_OK) return;
    g3d_vista_t c, z;
    vistas_atuais(&c, &z);
    if (c.cpu) { g3d_vista_encher(&c, G3D_SENTINELA_COR); vaciar_cache(&c); }
    if (z.cpu) { g3d_vista_encher(&z, G3D_SENTINELA_Z); vaciar_cache(&z); }
    __asm__ volatile("lock; orl $0, (%%esp)" ::: "memory");   /* lo escrito sale antes de la siguiente orden */
}

static void log_analise(const char *fase, const g3d_analise_t *an)
{
    klog("[GPU3D] %s: color (%u pixeles): %u sin tocar (%x); otros: %x x%u, %x x%u, %x x%u, %x x%u, resto %u\n", fase, an->total,
         an->cor_intacta, G3D_SENTINELA_COR, an->cores[0], an->n_cores[0], an->cores[1], an->n_cores[1], an->cores[2],
         an->n_cores[2], an->cores[3], an->n_cores[3], an->resto);
    if (an->tem_z) klog("[GPU3D] %s: Z: %u sin tocar (%x), %u al maximo\n", fase, an->z_intacto, G3D_SENTINELA_Z, an->z_maximo);
    else klog("[GPU3D] %s: Z: la CPU no lo ve en esta tarjeta\n", fase);
}

/* Tras leer el cuadro de una etapa por la interfaz común (drv->ler, en buf): el análisis al log
 * y, con --ver, la imagen de la etapa (el color leído y el Z tal como está). */
static void diag_etapa(uint32_t *buf)
{
    if (!diag || ap_etapa < 0 || ap_etapa >= GPU3D_N_PROVAS) return;
    g3d_vista_t c = { (volatile uint8_t *)buf, AP_L * 4, AP_L, AP_A, 32, 0, 0, false }, cv, z;
    vistas_atuais(&cv, &z);
    if (z.cpu) {
        vaciar_cache(&z);
        if (z.larg > AP_L) z.larg = AP_L;
        if (z.alt > AP_A) z.alt = AP_A;
    }
    g3d_analise_t an;
    g3d_analisar(&c, &z, &an);
    char fase[48];
    snformatar(fase, sizeof fase, "etapa %d (%s)", ap_etapa, nomes_prova[ap_etapa]);
    log_analise(fase, &an);
    if (!etapas) return;
    etapa_vista_t *e = &etapas[ap_etapa];
    e->com_imagem = true;
    e->an = an;
    memcpy(e->px, buf, sizeof e->px);
    for (int y = 0; y < AP_A; y++)
        for (int x = 0; x < AP_L; x++)
            e->z[y * AP_L + x] = z.cpu && x < z.larg && y < z.alt ? g3d_z_codigo(&z, g3d_vista_ler(&z, x, y)) : 0;
}

/* El destino tal como está, sin pasar por la GPU (tras un fallo puede no responder). Solo
 * destinos pequeños (la autoprueba y la medida): leer sin caché un destino grande tardaría. */
static void diag_destino(const char *fase)
{
    if (!drv || !drv->vistas) return;
    g3d_vista_t c, z;
    vistas_atuais(&c, &z);
    if (!c.cpu || c.larg * c.alt > 256 * 192) return;
    vaciar_cache(&c);
    vaciar_cache(&z);
    g3d_analise_t an;
    g3d_analisar(&c, &z, &an);
    char f[48];
    snformatar(f, sizeof f, "destino %dx%d %s", c.larg, c.alt, fase);
    log_analise(f, &an);
}

/* Antes de encender: los ajustes de /sistema/conf/gpu3d-<driver>.conf (D386 para la R5xx;
 * D390 para todos), solo dentro de los rangos que declara el driver. */
static void ajustes_carregar(void)
{
    memset(&gpu3d_ajustes, 0, sizeof gpu3d_ajustes);
    if (!drv || !drv->conf) return;
    char cam[64];
    snformatar(cam, sizeof cam, "/sistema/conf/gpu3d-%s.conf", drv->conf);
    uint8_t *t = NULL;
    uint32_t n = 0;
    if (vfs_ler_inteiro(cam, &t, &n) < 0 || !t) {
        if (t) kfree(t);
        return;
    }
    g3d_ajustes_ler((const char *)t, n, drv->faixas, drv->n_faixas, &gpu3d_ajustes);
    kfree(t);
    klog("[GPU3D] %s: %d ajustes de depuracion de %s (%d lineas malas, %d fuera de los registros permitidos)%s\n", drv->nome,
         gpu3d_ajustes.n, cam, gpu3d_ajustes.malas, gpu3d_ajustes.fora, drv->n_faixas ? "" : "; este driver no admite ajustes");
    for (int i = 0; i < gpu3d_ajustes.n; i++)
        klog("[GPU3D] %s:   %s %x = %x\n", drv->nome, gpu3d_ajustes.quando[i] == G3D_AJ_DIBUJO ? "dibujo" : "inicio",
             gpu3d_ajustes.reg[i], gpu3d_ajustes.val[i]);
}

/* Las tres capas de la prueba del Z: rojo en medio, verde detrás (no lo tapa) y azul delante (sí). */
static bool prova_z(uint32_t *buf, info_gpu3d_prova_t *r)
{
    gpu3d_estado_t e = { G3D_Z_TESTE | G3D_Z_ESCRITA, G3D_Z_MENOR, G3D_F_UM, G3D_F_ZERO, 0 };
    sentinelas();
    if (drv->limpar(3, AP_FUNDO) != G3D_OK) return false;
    if (quad(&e, 16, 16, 80, 64, 32768, 0xFFFF0000u, 0) != G3D_OK) return false;
    if (quad(&e, 40, 8, 110, 90, 49152, 0xFF00FF00u, 0) != G3D_OK) return false;
    if (quad(&e, 50, 30, 70, 50, 16384, 0xFF0000FFu, 0) != G3D_OK) return false;
    static const amostra_t am[] = {
        { 30, 40, 0xFF0000, 1 }, { 45, 60, 0xFF0000, 1 }, { 100, 40, 0x00FF00, 1 }, { 60, 80, 0x00FF00, 1 },
        { 60, 40, 0x0000FF, 1 }, { 5, 5, AP_FUNDO, 0 },
    };
    return conferir(buf, am, 6, r, false);
}

static bool autoprova(info_gpu3d_prova_t *r, char *m, int nm)
{
    uint32_t *buf = kmalloc(AP_L * AP_A * 4);
    if (!buf) { snformatar(m, (size_t)nm, "sin memoria para la autoprueba"); return false; }
    uint64_t t0 = tsc();
    r->provas = r->feitas = 0; r->falhou = -1;
    if (etapas) memset(etapas, 0, sizeof *etapas * GPU3D_N_PROVAS);
    int p = 0;
    bool ok = false;
    gpu3d_estado_t e;
    #define PASSO(n) do { p = (n); ap_etapa = p; r->feitas |= 1u << p; } while (0)
    #define FEITO()  (r->provas |= 1u << p)
    do {
        if (drv->alvo(AP_L, AP_A, 24) != G3D_OK) { snformatar(m, (size_t)nm, "no se pudo crear el destino de %dx%d", AP_L, AP_A); p = -1; break; }
        /* 0: limpiar (todos los píxeles). */
        PASSO(0);
        sentinelas();
        if (drv->limpar(3, AP_FUNDO) != G3D_OK || drv->ler(buf, AP_L) != G3D_OK) break;
        diag_etapa(buf);
        bool igual = true;
        for (int i = 0; i < AP_L * AP_A && igual; i++)
            if ((buf[i] & 0x00FFFFFFu) != AP_FUNDO) { igual = false; r->x = i % AP_L; r->y = i / AP_L; r->esperado = AP_FUNDO; r->obtido = buf[i] & 0x00FFFFFFu; }
        if (!igual) break;
        FEITO();
        /* 1: un rectángulo de color sin Z ni mezcla: dentro el color, fuera el fondo. */
        PASSO(1);
        e = (gpu3d_estado_t){ 0, G3D_Z_SEMPRE, G3D_F_UM, G3D_F_ZERO, 0 };
        if (quad(&e, 16, 16, 80, 64, 32768, 0xFFC08040u, 0) != G3D_OK) break;
        static const amostra_t a1[] = {
            { 48, 40, 0xC08040, 1 }, { 18, 18, 0xC08040, 1 }, { 77, 61, 0xC08040, 1 }, { 18, 61, 0xC08040, 1 }, { 77, 18, 0xC08040, 1 },
            { 8, 8, AP_FUNDO, 0 }, { 90, 40, AP_FUNDO, 0 }, { 48, 70, AP_FUNDO, 0 }, { 48, 12, AP_FUNDO, 0 }, { 12, 40, AP_FUNDO, 0 },
        };
        if (!conferir(buf, a1, 10, r, (cab & GPU3D_CAB_P_PIXEL) != 0)) break;
        FEITO();
        /* 2: Z. */
        PASSO(2);
        if (!prova_z(buf, r)) break;
        FEITO();
        /* 3: mezcla: blanco con alfa 128 sobre el fondo. */
        PASSO(3);
        sentinelas();
        if (drv->limpar(3, AP_FUNDO) != G3D_OK) break;
        e = (gpu3d_estado_t){ G3D_MEZCLA, G3D_Z_SEMPRE, G3D_F_ALFA, G3D_F_UM_MENOS_ALFA, 0 };
        if (quad(&e, 16, 16, 80, 64, 32768, 0x80FFFFFFu, 0) != G3D_OK) break;
        static const amostra_t a3[] = { { 48, 40, 0x889098, 3 }, { 20, 20, 0x889098, 3 }, { 100, 80, AP_FUNDO, 0 } };
        if (!conferir(buf, a3, 3, r, false)) break;
        FEITO();
        /* 4: textura de 4x4 (cada texel, 16x16 píxeles): orden de canales, orientación y vecino más próximo. */
        PASSO(4);
        uint32_t tx[16];
        amostra_t a4[16];
        for (int j = 0; j < 4; j++)
            for (int i = 0; i < 4; i++) {
                tx[j * 4 + i] = 0xFF000000u | ((uint32_t)(40 + i * 60) << 16) | ((uint32_t)(30 + j * 70) << 8) | (uint32_t)((i ^ j) * 80 + 15);
                a4[j * 4 + i] = (amostra_t){ 16 + i * 16 + 8, 16 + j * 16 + 8, tx[j * 4 + i] & 0x00FFFFFFu, 2 };
            }
        int rt = drv->textura(0, 4, 4, tx);
        if (rt != G3D_OK) break;
        sentinelas();
        if (drv->limpar(3, AP_FUNDO) != G3D_OK) break;
        e = (gpu3d_estado_t){ G3D_TEXTURA, G3D_Z_SEMPRE, G3D_F_UM, G3D_F_ZERO, 0 };
        if (quad(&e, 16, 16, 80, 80, 32768, 0xFFFFFFFFu, 0) != G3D_OK) break;
        if (!conferir(buf, a4, 16, r, false)) break;
        drv->textura(0, 0, 0, NULL);
        FEITO();
        /* 5: un triángulo con un color en cada vértice: en el centro, la media. */
        PASSO(5);
        sentinelas();
        if (drv->limpar(3, AP_FUNDO) != G3D_OK) break;
        e = (gpu3d_estado_t){ 0, G3D_Z_SEMPRE, G3D_F_UM, G3D_F_ZERO, 0 };
        gpu3d_vert_t v[3];
        vert(&v[0], 16, 16, 32768, 0xFFFF0000u, 0, 0, 0);
        vert(&v[1], 112, 16, 32768, 0xFF00FF00u, 0, 0, 0);
        vert(&v[2], 64, 88, 32768, 0xFF0000FFu, 0, 0, 0);
        if (drv->desenhar(&e, v, 3, NULL) != G3D_OK) break;
        static const amostra_t a5[] = { { 64, 40, 0x555555, 8 }, { 22, 18, 0xF00500, 24 }, { 106, 18, 0x05F000, 24 }, { 64, 84, 0x0808F0, 24 } };
        if (!conferir(buf, a5, 4, r, false)) break;
        FEITO();
        /* 6: el especular se suma al final (así llega la niebla). */
        PASSO(6);
        if (quad(&e, 16, 16, 80, 64, 32768, 0xFF404040u, 0x00203010u) != G3D_OK) break;
        static const amostra_t a6[] = { { 48, 40, 0x607050, 2 } };
        if (!conferir(buf, a6, 1, r, false)) break;
        FEITO();
        /* 7: el Z de 16 bits (para destinos grandes que no caben con 24). */
        PASSO(7);
        if (drv->alvo(AP_L, AP_A, 16) != G3D_OK) break;
        if (!prova_z(buf, r)) break;
        FEITO();
        ok = true;
    } while (0);
    #undef PASSO
    #undef FEITO
    r->us_autoprova = us_desde(t0);
    ap_etapa = -1;
    if (!ok) {
        r->falhou = p;
        if (p >= 0 && r->x >= 0 && (r->esperado || r->obtido))
            snformatar(m, (size_t)nm, "prueba %d (%s): el pixel %d,%d es %06x y se esperaba %06x%s", p, nomes_prova[p], r->x, r->y,
                       r->obtido, r->esperado, (cab & GPU3D_CAB_P_PIXEL) && p == 1 ? " (simulado)" : "");
        else if (p >= 0)
            snformatar(m, (size_t)nm, "prueba %d (%s): la GPU no acepto la orden o no respondio a tiempo", p, nomes_prova[p]);
    }
    klog("[GPU3D] autoprueba: %s en %u us (pruebas hechas %x, superadas %x)%s%s\n", ok ? "BIEN" : "FALLO", r->us_autoprova,
         r->feitas, r->provas, ok ? "" : ": ", ok ? "" : m);
    /* D390: el resultado de cada imagen guardada para «gpu3d probar --ver». */
    for (int i = 0; etapas && i < GPU3D_N_PROVAS; i++) {
        etapa_vista_t *e = &etapas[i];
        e->x = e->y = -1;
        if (r->provas & (1u << i)) e->estado = GPU3D_ETAPA_BIEN;
        else if (r->feitas & (1u << i)) e->estado = e->com_imagem ? GPU3D_ETAPA_MAL : GPU3D_ETAPA_SIN_IMAGEN;
        else e->estado = GPU3D_ETAPA_NADA;
        if (i == r->falhou && r->x >= 0 && (r->esperado || r->obtido)) { e->x = r->x; e->y = r->y; e->esperado = r->esperado; e->obtido = r->obtido; }
    }
    kfree(buf);
    return ok;
}

/* ------------------------------------------------------------ la medida */

#define MD_L 256
#define MD_A 192
#define MD_PEQUENOS 1200
#define MD_GRANDES  40

static bool medir(info_gpu3d_prova_t *r)
{
    gpu3d_vert_t *v = kmalloc(300 * sizeof(gpu3d_vert_t));
    uint32_t *buf = kmalloc(MD_L * MD_A * 4);
    bool ok = false;
    if (!v || !buf || !hw_tsc_khz()) goto fim;
    if (drv->alvo(MD_L, MD_A, 24) != G3D_OK || drv->limpar(3, 0) != G3D_OK || drv->ler(buf, MD_L) != G3D_OK) goto fim;
    /* Triángulos pequeños (8x8) con Z: lo que cuesta mandar órdenes. */
    gpu3d_estado_t e = { G3D_Z_TESTE | G3D_Z_ESCRITA, G3D_Z_MENOR_IGUAL, G3D_F_UM, G3D_F_ZERO, 0 };
    uint32_t s = 0x3D5EEDu;
    uint64_t t0 = tsc();
    for (int k = 0; k < MD_PEQUENOS; k += 100) {
        for (int i = 0; i < 100; i++) {
            s = s * 1103515245u + 12345u;
            int x = (int)((s >> 8) % (MD_L - 8)), y = (int)((s >> 18) % (MD_A - 8));
            uint32_t c = 0xFF000000u | (s & 0x00FFFFFFu);
            vert(&v[i * 3], x, y, 20000 + i * 64, c, 0, 0, 0);
            vert(&v[i * 3 + 1], x + 8, y, 20000 + i * 64, c, 0, 0, 0);
            vert(&v[i * 3 + 2], x, y + 8, 20000 + i * 64, c, 0, 0, 0);
        }
        if (drv->desenhar(&e, v, 300, NULL) != G3D_OK) goto fim;
    }
    if (drv->esperar(0, GPU3D_PRAZO_MS) != G3D_OK) goto fim;
    r->tri_pequenos = MD_PEQUENOS; r->us_pequenos = us_desde(t0);
    /* Rectángulos a todo el destino con mezcla: el relleno. */
    e = (gpu3d_estado_t){ G3D_MEZCLA, G3D_Z_SEMPRE, G3D_F_ALFA, G3D_F_UM_MENOS_ALFA, 0 };
    t0 = tsc();
    for (int k = 0; k < MD_GRANDES; k++)
        if (quad(&e, 0, 0, MD_L, MD_A, 32768, 0x40000000u | (uint32_t)(k * 0x050309), 0) != G3D_OK) goto fim;
    if (drv->esperar(0, GPU3D_PRAZO_MS) != G3D_OK) goto fim;
    r->tri_grandes = MD_GRANDES * 2; r->us_grandes = us_desde(t0); r->pix_grandes = MD_GRANDES * MD_L * MD_A;
    t0 = tsc();
    if (drv->ler(buf, MD_L) != G3D_OK) goto fim;
    r->us_leitura = us_desde(t0); r->pix_leitura = MD_L * MD_A;
    /* Puntuación provisional: 100 000 triángulos/s y 100 Mpíxeles/s dan 1000 puntos. */
    r->tri_s = r->us_pequenos ? (uint32_t)sconv_div64((uint64_t)r->tri_pequenos * 1000000u, r->us_pequenos) : 0;
    r->kpix_s = r->us_grandes ? (uint32_t)sconv_div64((uint64_t)r->pix_grandes * 1000u, r->us_grandes) : 0;
    r->pontos = r->tri_s / 200 + r->kpix_s / 200;
    pontos = r->pontos; tri_s = r->tri_s; kpix_s = r->kpix_s;
    klog("[GPU3D] medida: %u triangulos de 8x8 en %u us (%u/s); %u rectangulos de %dx%d con mezcla en %u us (%u kpix/s); "
         "leer %dx%d: %u us; %u puntos\n", r->tri_pequenos, r->us_pequenos, r->tri_s, MD_GRANDES, MD_L, MD_A, r->us_grandes,
         r->kpix_s, MD_L, MD_A, r->us_leitura, r->pontos);
    ok = true;
fim:
    if (v) kfree(v);
    if (buf) kfree(buf);
    return ok;
}

/* ------------------------------------------------------------ encender */

void gpu3d_iniciar(void)
{
    cab = cabecalho_boot[CAB_GPU3D];
    int pedido = cab & GPU3D_CAB_MODO;
    bool seguro = (info_boot.opcoes & OPC_BOOT_ACEL_SEGURO) || (cabecalho_boot[CAB_ACEL2D] & CAB_ACEL_SEGURO);   /* D323 */
    if (seguro) pedido = GPU3D_CAB_NAO;
    variante = cab & (GPU3D_CAB_ANEL | GPU3D_CAB_GTT | GPU3D_CAB_SEM_GTT);
    drv = NULL; estado = GPU3D_SEM_GPU;
    snformatar(placa_nome, sizeof placa_nome, "ninguna");
    if ((info_boot.opcoes & OPC_BOOT_SGL_SOFTWARE) || pedido == GPU3D_CAB_NAO) {
        estado = GPU3D_DESATIVADO;
        snformatar(motivo, sizeof motivo, "la opcion de arranque pide SaguiGL por software (%s)",
                   seguro ? "modo seguro de la aceleracion" : pedido == GPU3D_CAB_NAO ? "--gpu3d=no" : "SGL_MOTOR=software");
    } else if (pedido == GPU3D_CAB_SIMULADO) {
        drv = &gpu3d_sim;
        memset(&disp, 0, sizeof disp); memset(&tela, 0, sizeof tela);
        snformatar(placa_nome, sizeof placa_nome, "simulada (pruebas)");
        estado = GPU3D_SIN_PROBAR;
        snformatar(motivo, sizeof motivo, "driver simulado por la CPU (--gpu3d=simulado): se prueba con el primer uso");
    } else if (virgl_disponible()) {
        /* D474: la virtio-gpu con VIRGL negociado (D472, D473). Es una GPU virtual: encenderla no toca hardware
         * (un fallo solo deja el contexto del anfitrión), así que se prueba sola con el primer uso. */
        drv = &gpu3d_virgl;
        memset(&disp, 0, sizeof disp); memset(&tela, 0, sizeof tela);
        snformatar(placa_nome, sizeof placa_nome, "virtio-gpu VirGL");
        estado = GPU3D_SIN_PROBAR;
        snformatar(motivo, sizeof motivo, "%s: 3D por la GPU del anfitrion sin probar todavia (se prueba con el primer uso)", placa_nome);
    } else {
        const acel_driver_t *d2 = NULL;
        bool suspeito = false;
        if (acel2d_placa(&disp, &geracao, &d2, &tela, &suspeito) && d2 == &acel_intel && geracao == 3) {
            drv = &gpu3d_intel;
            snformatar(placa_nome, sizeof placa_nome, "Intel Gen3 %04x:%04x", disp.fabricante, disp.modelo);
            estado = GPU3D_SIN_PROBAR;
            snformatar(motivo, sizeof motivo, "%s: 3D sin probar todavia (se prueba con el primer uso o con «gpu3d probar»)", placa_nome);
        } else if (d2 == &acel_intel_exec && geracao == 9) {
            /* D438: Intel Gen9 (Skylake ... Comet Lake; la UHD 620 8086:3ea0) por el RCS con execlists.
             * Sin probar en hardware: como las Radeon, NO se enciende con el primer uso salvo con
             * --gpu3d=intel; «gpu3d probar» / «gpu3d encender» lo prueban. Gen11 y Gen12 aún no. */
            drv = &gpu3d_intel9;
            snformatar(placa_nome, sizeof placa_nome, "Intel Gen9 %04x:%04x", disp.fabricante, disp.modelo);
            if (pedido == GPU3D_CAB_INTEL) {
                estado = GPU3D_SIN_PROBAR;
                snformatar(motivo, sizeof motivo, "%s: 3D sin probar todavia (--gpu3d=intel: se prueba con el primer uso)", placa_nome);
            } else {
                estado = GPU3D_APAGADO;
                snformatar(motivo, sizeof motivo, "%s: 3D apagado; no se enciende solo («gpu3d probar» lo comprueba y lo activa; --gpu3d=intel al grabar)", placa_nome);
            }
        } else if (d2 == &acel_r600 && (geracao == 7 || geracao == 6)) {
            /* D284: Radeon R7xx; D357: también R6xx. Sin probar en hardware y el inicio carga microcódigo y
             * reinicia el procesador de órdenes: NO se enciende con el primer uso, solo a mano (o con
             * --gpu3d=radeon). D357: estas líneas del estado se habían perdido al fusionar la rama de las
             * R5xx (02bae96d): las R7xx quedaban «sin GPU con driver 3D» y «gpu3d probar» no las encendía. */
            drv = &gpu3d_r600;
            snformatar(placa_nome, sizeof placa_nome, "Radeon %s %04x:%04x", geracao == 7 ? "R7xx" : "R6xx", disp.fabricante, disp.modelo);
            if (pedido == GPU3D_CAB_RADEON) {
                estado = GPU3D_SIN_PROBAR;
                snformatar(motivo, sizeof motivo, "%s: 3D sin probar todavia (--gpu3d=radeon: se prueba con el primer uso)", placa_nome);
            } else {
                estado = GPU3D_APAGADO;
                snformatar(motivo, sizeof motivo, "%s: 3D apagado; no se enciende solo («gpu3d probar» lo comprueba y lo activa; --gpu3d=radeon al grabar)", placa_nome);
            }
        } else if (d2 == &acel_r600 && geracao == 8) {
            /* D427: Radeon Evergreen y Northern Islands sin Cayman (generación 8 de la tabla del 2D, D286).
             * Como las R6xx/R7xx: sin probar en hardware y el inicio carga microcódigo y reinicia el CP, así
             * que NO se enciende con el primer uso salvo con --gpu3d=radeon. Cayman (9) no tiene 3D. */
            drv = &gpu3d_evergreen;
            snformatar(placa_nome, sizeof placa_nome, "Radeon Evergreen/NI %04x:%04x", disp.fabricante, disp.modelo);
            if (pedido == GPU3D_CAB_RADEON) {
                estado = GPU3D_SIN_PROBAR;
                snformatar(motivo, sizeof motivo, "%s: 3D sin probar todavia (--gpu3d=radeon: se prueba con el primer uso)", placa_nome);
            } else {
                estado = GPU3D_APAGADO;
                snformatar(motivo, sizeof motivo, "%s: 3D apagado; no se enciende solo («gpu3d probar» lo comprueba y lo activa; --gpu3d=radeon al grabar)", placa_nome);
            }
        } else if (d2 == &acel_radeon && geracao == 2 &&
                   disp.modelo >= 0x7100 && disp.modelo <= 0x72FF) {
            /* D287: Radeon R5xx. Sin probar en hardware: no se enciende con el primer uso
             * (queda «apagado a mano») salvo con --gpu3d=radeon; «gpu3d encender» o
             * «gpu3d probar» lo ponen en marcha con su autoprueba. El driver comprueba el
             * modelo exacto al encender. */
            drv = &gpu3d_r500;
            snformatar(placa_nome, sizeof placa_nome, "Radeon R5xx %04x:%04x", disp.fabricante, disp.modelo);
            if (pedido == GPU3D_CAB_RADEON) {
                estado = GPU3D_SIN_PROBAR;
                snformatar(motivo, sizeof motivo, "%s: 3D sin probar todavia (--gpu3d=radeon: se prueba con el primer uso)", placa_nome);
            } else {
                estado = GPU3D_APAGADO;
                snformatar(motivo, sizeof motivo, "%s: 3D apagado; no se enciende solo («gpu3d probar» lo comprueba y lo activa; --gpu3d=radeon al grabar)", placa_nome);
            }
        } else if (d2)
            snformatar(motivo, sizeof motivo, "la tarjeta %04x:%04x no tiene driver 3D: SaguiGL por software", disp.fabricante, disp.modelo);
        else
            snformatar(motivo, sizeof motivo, "ninguna tarjeta con driver 3D: SaguiGL por software");
    }
    klog("[GPU3D] %s: %s\n", drv ? drv->nome : "software", motivo);
}

const char *gpu3d_resumo(void) { return motivo; }

/* D358: la variante que de verdad quedó (GPU3D_VAR_*): el driver de Intel puede pasar
 * al anillo si el lote no se ejecuta, o seguir sin la RAM del GTT si no la puede usar. */
static uint32_t variante_real(uint32_t *ram_kb)
{
    uint32_t v = (variante & GPU3D_CAB_ANEL ? GPU3D_VAR_ANEL : 0) | (variante & GPU3D_CAB_GTT ? GPU3D_VAR_GTT : 0), kb = 0;
    if (drv == &gpu3d_intel && estado == GPU3D_ATIVO) gpu3d_intel_estado(&v, &kb);
    if (drv == &gpu3d_r600 && estado == GPU3D_ATIVO) gpu3d_r600_estado(&v, &kb);   /* D406: la GART de las R7xx */
    if (ram_kb) *ram_kb = kb;
    return v;
}

/* Pone el driver en marcha y lo prueba. true: activo. */
/* D425: lo que el driver dice que sabe hacer, sin lo que depende de funciones que no tiene. */
static void capas_calcular(void)
{
    memset(&capas, 0, sizeof capas);
    capas.versao = 2; capas.unidades = 1; capas.max_lado = GPU3D_MAX_LADO; capas.max_aniso = 1;
    capas.formatos = 1u << G3D_FMT_ARGB8888; capas.zbits = 3;
    bits3_driver = 0; prova3 = 0;
    if (!drv) return;
    /* D432: presentar no depende de v2; el driver dice si puede escribir en la pantalla de ahora. */
    bool apres = drv->apresentar && drv->apresentar(NULL) == G3D_OK;
    if (!drv->capacidades) {
        if (apres) capas.bits = G3D_CAP_APRESENTAR;
        klog("[GPU3D] v1: presentar sin volver a la RAM (D432): %s\n", apres ? "si" : "no");
        return;
    }
    drv->capacidades(&capas);
    capas.versao = 2;
    if (!drv->desenhar2) { capas.bits = 0; capas.unidades = 1; }
    capas.bits = (capas.bits & ~G3D_CAP_APRESENTAR) | (apres ? G3D_CAP_APRESENTAR : 0);
    if (capas.unidades < 1) capas.unidades = 1;
    if (capas.unidades > 4) capas.unidades = 4;
    if (capas.max_lado > GPU3D_MAX_LADO || capas.max_lado < 64) capas.max_lado = GPU3D_MAX_LADO;
    if (capas.max_aniso < 1) capas.max_aniso = 1;
    if (capas.max_aniso > 16) capas.max_aniso = 16;
    if (capas.estencil_bits != 8) { capas.estencil_bits = 0; capas.bits &= ~G3D_CAP_ESTENCIL; }
    capas.formatos |= 1u << G3D_FMT_ARGB8888;
    capas.formatos &= 0xFu;
    if (!drv->textura2) {
        capas.formatos = 1u << G3D_FMT_ARGB8888;
        capas.bits &= ~(G3D_CAP_TEX_CUBO | G3D_CAP_TEX_MIPMAP);
    }
    if (!(capas.bits & G3D_CAP_TEX_MIPMAP) || !(capas.bits & G3D_CAP_TEX_NPOT)) capas.bits &= ~G3D_CAP_TEX_NPOT_MIP;
    if (!drv->destino) capas.bits &= ~G3D_CAP_DESTINO_TEX;
    if (!drv->limpar2) capas.bits &= ~(G3D_CAP_LIMPAR2 | G3D_CAP_ESTENCIL);
    if (!(capas.bits & G3D_CAP_TEX_ANISO)) capas.max_aniso = 1;
    /* D457: v3 pide v2 (la parte de los píxeles es la de v2) y las dos funciones. */
    if (!drv->vbuf || !drv->desenhar3 || !(capas.bits & G3D_CAP_V2)) capas.bits3 = 0;
    if (!(capas.bits3 & G3D_C3_V3)) capas.bits3 = 0;
    if (!(capas.bits3 & G3D_C3_NEBLINA)) capas.bits3 &= ~G3D_C3_NEB_EXP;
    if (!(capas.bits3 & G3D_C3_LUZ)) { capas.bits3 &= ~(G3D_C3_LUZ_ESPEC | G3D_C3_LUZ_FOCO); capas.max_luzes = 0; }
    else if (capas.max_luzes < 1 || capas.max_luzes > 8) capas.max_luzes = capas.max_luzes < 1 ? 1 : 8;
    capas.bits3 &= 0x3FFu;
    capas.vbufs = capas.bits3 ? GPU3D_VBUFS : 0;
    memset(capas.reservado, 0, sizeof capas.reservado);
    bits3_driver = capas.bits3; prova3 = 0;
    klog("[GPU3D] v2: capacidades 0x%x, %u unidades, formatos 0x%x; presentar sin volver a la RAM (D432): %s\n", capas.bits,
         capas.unidades, capas.formatos, capas.bits & G3D_CAP_APRESENTAR ? "si" : "no");
}

/* ------------------------------------------------------------ gpu3d v3: la autoprueba (D457)
 *
 * Tres escenas pequeñas en el destino de 128x96. Cada una se dibuja dos veces: por el desenhar3 del driver (la
 * GPU lee los búferes, transforma, recorta y proyecta) y por g3d_v3_cpu + desenhar2 (la CPU hace eso mismo con
 * enteros y la GPU solo pinta, que es lo que ya pasó la autoprueba de v2 del driver). Las dos imágenes se
 * comparan píxel a píxel: se toleran diferencias pequeñas de color y unos pocos píxeles de borde.
 * Si no cuadran, v3 se queda apagado (capas.bits3 = 0: SaguiGL sigue transformando con la CPU, como antes) y el
 * log dice qué escena y qué píxel; el 3D sigue encendido. Solo si la GPU deja de responder se apaga todo. */
#define P3_ESCENAS   3
#define P3_TOL       28      /* diferencia por canal que no cuenta */
#define P3_MAX_MAL   370     /* píxeles distintos tolerados (3 % de 128x96: los bordes) */
#define P3_MIN_CHEIO 600     /* píxeles que la referencia debe pintar (si no, la escena no prueba nada) */
#define Q(x) ((int32_t)((x) * 65536))
static uint32_t fx(int32_t q16) { return g3d_float_de_fixo(q16, 16); }

static const char *const nomes_prova3[P3_ESCENAS] = {
    "perspectiva, recorte, color por vertice y Z", "indices, tira y descarte de caras", "textura con matriz y niebla",
};

/* c = a x b (matrices por columnas) */
static void mat_mul(uint32_t c[16], const uint32_t a[16], const uint32_t b[16])
{
    for (int col = 0; col < 4; col++)
        for (int l = 0; l < 4; l++) {
            uint32_t v = 0;
            for (int k = 0; k < 4; k++) v = g3d_fl_soma(v, g3d_fl_mul(a[k * 4 + l], b[col * 4 + k]));
            c[col * 4 + l] = v;
        }
}
static void mat_q16(uint32_t m[16], const int32_t q[16]) { for (int i = 0; i < 16; i++) m[i] = fx(q[i]); }

typedef struct { uint32_t x, y, z; uint8_t cor[4]; uint32_t s, t; } p3_vert_t;   /* 24 bytes */
static void p3_v(p3_vert_t *v, int32_t x, int32_t y, int32_t z, uint32_t rgba, int32_t s, int32_t t)
{
    v->x = fx(x); v->y = fx(y); v->z = fx(z);
    v->cor[0] = (uint8_t)(rgba >> 24); v->cor[1] = (uint8_t)(rgba >> 16); v->cor[2] = (uint8_t)(rgba >> 8); v->cor[3] = (uint8_t)rgba;
    v->s = fx(s); v->t = fx(t);
}

static struct { const uint8_t *v; uint32_t nv; const uint8_t *i; uint32_t ni; } p3_mem;
static g3d_v3_mem_t p3_ler(void *ctx, int vaga)
{
    (void)ctx;
    if (vaga == 0) return (g3d_v3_mem_t){ p3_mem.v, p3_mem.nv };
    if (vaga == 1) return (g3d_v3_mem_t){ p3_mem.i, p3_mem.ni };
    return (g3d_v3_mem_t){ NULL, 0 };
}
static int p3_sair(void *ctx, const gpu3d_estado2_t *e, const gpu3d_vert2_t *v, int n)
{
    (void)ctx;
    return drv->desenhar2(e, v, n, NULL);
}

/* Las dos imágenes de una escena: cuántos píxeles difieren (y el peor). */
static int p3_comparar(const uint32_t *gpu, const uint32_t *cpu, int *px, int *py, int *cheio)
{
    int mal = 0, pior = 0;
    *px = *py = -1; *cheio = 0;
    for (int i = 0; i < AP_L * AP_A; i++) {
        uint32_t a = gpu[i] & 0x00FFFFFFu, b = cpu[i] & 0x00FFFFFFu;
        if (b != AP_FUNDO) (*cheio)++;
        if (perto(a, b, P3_TOL)) continue;
        mal++;
        int d = 0;
        for (int k = 0; k < 24; k += 8) { int x = (int)((a >> k) & 0xFF) - (int)((b >> k) & 0xFF); d += x < 0 ? -x : x; }
        /* el peor que no sea de borde: sus cuatro vecinos de la referencia son como él */
        bool dentro = i >= AP_L && i < AP_L * (AP_A - 1) && i % AP_L && i % AP_L != AP_L - 1 &&
                      perto(cpu[i - 1], b, 8) && perto(cpu[i + 1], b, 8) && perto(cpu[i - AP_L], b, 8) && perto(cpu[i + AP_L], b, 8);
        if (dentro) d += 1000;
        if (d > pior) { pior = d; *px = i % AP_L; *py = i / AP_L; }
    }
    return mal;
}

/* Diagnóstico (revisión de D457): las dos imágenes de una escena en el registro, a la mitad (64x48), una al lado
 * de la otra: la CPU a la izquierda y la GPU a la derecha. Un carácter por color: '.' fondo, 'k' negro, 'W'
 * blanco, 'g' gris, 'R' 'G' 'B' 'Y' 'C' 'M' el canal o los dos canales que dominan, '?' lo demás. */
static char p3_letra(uint32_t c)
{
    c &= 0xFFFFFFu;
    if (perto(c, AP_FUNDO, 8)) return '.';
    int r = (int)(c >> 16) & 0xFF, g = (int)(c >> 8) & 0xFF, b = (int)c & 0xFF;
    int mx = r > g ? (r > b ? r : b) : (g > b ? g : b), mn = r < g ? (r < b ? r : b) : (g < b ? g : b);
    if (mx < 40) return 'k';
    if (mn > 200) return 'W';
    if (mx - mn < 40) return 'g';
    bool R = r > mx - 50, G = g > mx - 50, B = b > mx - 50;
    if (R && G && !B) return 'Y';
    if (G && B && !R) return 'C';
    if (R && B && !G) return 'M';
    if (R && !G && !B) return 'R';
    if (G && !R && !B) return 'G';
    if (B && !R && !G) return 'B';
    return '?';
}
static void p3_mapa(int escena, const uint32_t *gpu, const uint32_t *cpu)
{
    char l[2 * (AP_L / 2) + 4];
    for (int y = 0; y < AP_A; y += 2) {
        int k = 0;
        for (int x = 0; x < AP_L; x += 2) l[k++] = p3_letra(cpu[y * AP_L + x]);
        l[k++] = ' '; l[k++] = '|'; l[k++] = ' ';
        for (int x = 0; x < AP_L; x += 2) l[k++] = p3_letra(gpu[y * AP_L + x]);
        l[k] = 0;
        klog("[GPU3D] v3 mapa %d y%02d %s\n", escena, y, l);
    }
}

/* true salvo que la GPU deje de responder. El resultado queda en prova3 y en capas.bits3. */
static bool autoprova3(void)
{
    uint64_t t0 = tsc();
    uint32_t *img = kmalloc(AP_L * AP_A * 4 * 2);
    gpu3d_vert2_t *lote = kmalloc(sizeof(gpu3d_vert2_t) * 96);
    p3_vert_t *vs = kmalloc(sizeof(p3_vert_t) * 16);
    gpu3d_transf_t *t = kmalloc(sizeof *t);
    capas.bits3 = 0;
    prova3 = 2;
    if (!img || !lote || !vs || !t) {
        klog("[GPU3D] v3: sin memoria para la autoprueba: la transformacion sigue por la CPU\n");
        if (img) kfree(img);
        if (lote) kfree(lote);
        if (vs) kfree(vs);
        if (t) kfree(t);
        return true;
    }
    uint32_t b3 = bits3_driver;
    bool viva = true, bien = true;
    static const int32_t proj_q[16] = {   /* perspectiva de 60 grados, 4:3, cerca 1, lejos 50 */
        Q(1.299038), 0, 0, 0,   0, Q(1.732051), 0, 0,   0, 0, Q(-1.040816), Q(-1),   0, 0, Q(-2.040816), 0 };
    uint32_t proj[16];
    mat_q16(proj, proj_q);
    static uint16_t idx[16];
    int escena = 0;
    #define P3_FALHA_GPU() do { viva = false; goto fim; } while (0)
    if (drv->alvo(AP_L, AP_A, 24) != G3D_OK) { klog("[GPU3D] v3: no se pudo crear el destino de la autoprueba\n"); bien = false; goto fim; }
    for (escena = 0; escena < P3_ESCENAS; escena++) {
        gpu3d_estado2_t e;
        gpu3d_desenho3_t d[2];
        int nd = 1, nv = 0, ni = 0;
        memset(&e, 0, sizeof e); memset(t, 0, sizeof *t); memset(d, 0, sizeof d);
        e.versao = 2; e.flags = G3D2_Z_TESTE | G3D2_Z_ESCRITA; e.func_z = G3D_CMP_MENOR_IGUAL; e.mascara_cor = 15;
        t->versao = 3;
        t->vp_escala[0] = fx(Q(AP_L / 2)); t->vp_desloc[0] = fx(Q(AP_L / 2));
        t->vp_escala[1] = fx(-Q(AP_A / 2)); t->vp_desloc[1] = fx(Q(AP_A / 2));
        t->vp_escala[2] = fx(Q(0.5)); t->vp_desloc[2] = fx(Q(0.5));
        t->cor = 0xFFFFFFFFu;
        for (int u = 0; u < 4; u++) t->tc_const[u][3] = 0x3F800000u;
        d[0].pos = d[1].pos = (gpu3d_atrib_t){ 0, G3D_AT_FLOAT3, sizeof(p3_vert_t), 0 };
        d[0].indices = d[1].indices = GPU3D_SEM_INDICES;
        uint32_t mv[16];
        if (escena == 0) {
            /* un rectángulo grande girado 36,87 grados (coseno 0,8 y seno 0,6) que se sale por la izquierda, por la
             * derecha y por el plano cercano, con un color en cada esquina, y otro pequeño que lo atraviesa */
            static const int32_t mv_q[16] = { Q(0.8), 0, Q(-0.6), 0,   0, Q(1), 0, 0,   Q(0.6), 0, Q(0.8), 0,   0, 0, Q(-2.5), Q(1) };
            mat_q16(mv, mv_q);
            p3_v(&vs[0], Q(-4), Q(-1.5), 0, 0xFF2020FFu, 0, 0); p3_v(&vs[1], Q(4), Q(-1.5), 0, 0x20FF20FFu, 0, 0);
            p3_v(&vs[2], Q(4), Q(1.5), 0, 0x2020FFFFu, 0, 0);   p3_v(&vs[3], Q(-4), Q(-1.5), 0, 0xFF2020FFu, 0, 0);
            p3_v(&vs[4], Q(4), Q(1.5), 0, 0x2020FFFFu, 0, 0);   p3_v(&vs[5], Q(-4), Q(1.5), 0, 0xF0F020FFu, 0, 0);
            p3_v(&vs[6], Q(-0.5), Q(-0.9), Q(-1.2), 0xFFFFFFFFu, 0, 0); p3_v(&vs[7], Q(-0.5), Q(-0.9), Q(1.2), 0xFFFFFFFFu, 0, 0);
            p3_v(&vs[8], Q(-0.5), Q(0.9), Q(1.2), 0x808080FFu, 0, 0);   p3_v(&vs[9], Q(-0.5), Q(-0.9), Q(-1.2), 0xFFFFFFFFu, 0, 0);
            p3_v(&vs[10], Q(-0.5), Q(0.9), Q(1.2), 0x808080FFu, 0, 0);  p3_v(&vs[11], Q(-0.5), Q(0.9), Q(-1.2), 0x808080FFu, 0, 0);
            nv = 12;
            mat_mul(t->mvp, proj, mv);
            d[0].cor_a = (gpu3d_atrib_t){ 0, G3D_AT_UBYTE4N, sizeof(p3_vert_t), 12 };
            d[0].modo = G3D_PRIM_TRIANGULOS; d[0].primeiro = 0; d[0].n = 12;
        } else if (escena == 1) {
            /* sin perspectiva: una tira de cuatro triángulos a la izquierda y la misma, con el orden al revés, a la
             * derecha: con el descarte de caras solo debe verse una */
            static const int32_t mv_q[16] = { Q(0.9), 0, 0, 0,   0, Q(0.9), 0, 0,   0, 0, Q(1), 0,   Q(-0.1), Q(0.05), 0, Q(1) };
            mat_q16(t->mvp, mv_q);
            for (int k = 0; k < 6; k++) {
                p3_v(&vs[k], Q(-0.9) + k * Q(0.15), k & 1 ? Q(0.7) : Q(-0.7), Q(0.2), 0, 0, 0);
                p3_v(&vs[6 + k], Q(0.2) + k * Q(0.15), k & 1 ? Q(0.7) : Q(-0.7), Q(-0.3), 0, 0, 0);
            }
            nv = 12;
            t->cor = 0xFF30C0E0u;
            t->descarte = G3D_DESC_POS;
            if (b3 & G3D_C3_TIRAS) {
                d[0].modo = d[1].modo = G3D_PRIM_TIRA; d[0].n = d[1].n = 6;
                for (int k = 0; k < 6; k++) { idx[k] = (uint16_t)k; idx[6 + k] = (uint16_t)(6 + (k ^ 1)); }
                ni = 12;
            } else {
                d[0].modo = d[1].modo = G3D_PRIM_TRIANGULOS; d[0].n = d[1].n = 6;
                static const uint16_t tri[12] = { 0, 1, 2, 2, 1, 3,   7, 6, 8, 8, 6, 9 };
                for (int k = 0; k < 12; k++) idx[k] = tri[k];
                ni = 12;
            }
            nd = 2; d[1].primeiro = 6;
            if (b3 & G3D_C3_INDICES) d[0].indices = d[1].indices = 1;
            else {
                /* sin índices: los mismos vértices, ya en orden */
                p3_vert_t tmp[12];
                for (int k = 0; k < 12; k++) tmp[k] = vs[idx[k]];
                for (int k = 0; k < 12; k++) vs[k] = tmp[k];
                ni = 0;
            }
        } else {
            /* un suelo con textura que se aleja: la coordenada pasa por una matriz y la niebla crece con la distancia */
            static const int32_t mv_q[16] = { Q(1), 0, 0, 0,   0, Q(0.47), Q(-0.8829), 0,   0, Q(0.8829), Q(0.47), 0,   0, Q(-0.4), Q(-3.2), Q(1) };
            static const int32_t tx_q[16] = { Q(2), 0, 0, 0,   0, Q(1.5), 0, 0,   0, 0, Q(1), 0,   Q(0.25), 0, 0, Q(1) };
            mat_q16(mv, mv_q);
            mat_mul(t->mvp, proj, mv);
            for (int k = 0; k < 16; k++) t->mv[k] = mv[k];
            p3_v(&vs[0], Q(-2), Q(-2.5), 0, 0, 0, 0);     p3_v(&vs[1], Q(2), Q(-2.5), 0, 0, Q(1), 0);
            p3_v(&vs[2], Q(2), Q(2.5), 0, 0, Q(1), Q(1)); p3_v(&vs[3], Q(-2), Q(-2.5), 0, 0, 0, 0);
            p3_v(&vs[4], Q(2), Q(2.5), 0, 0, Q(1), Q(1)); p3_v(&vs[5], Q(-2), Q(2.5), 0, 0, 0, Q(1));
            nv = 6;
            d[0].modo = G3D_PRIM_TRIANGULOS; d[0].n = 6;
            t->cor = 0xFFFFFFFFu;
            d[0].tc[0] = (gpu3d_atrib_t){ 0, G3D_AT_FLOAT2, sizeof(p3_vert_t), 16 };
            if (b3 & G3D_C3_TEX_MAT) { t->flags |= G3D3_TEX_MAT(0); mat_q16(t->tex[0], tx_q); }
            if (b3 & G3D_C3_NEBLINA) {
                /* lineal de 1,5 a 6 */
                t->flags |= G3D3_NEBLINA; t->neb_modo = G3D_NEB_LINEAR;
                t->neb_a = fx(Q(6.0 / 4.5)); t->neb_b = fx(Q(1.0 / 4.5));
                if (capas.bits & G3D_CAP_NEBLINA) { e.flags |= G3D2_NEBLINA; e.cor_neblina = 0x00C0A040u; }
                else { t->flags |= G3D3_NEB_MEZCLADA; t->neb_cor = 0x00C0A040u; }
            }
            uint32_t tx[16];
            for (int j = 0; j < 4; j++)
                for (int i = 0; i < 4; i++)
                    tx[j * 4 + i] = 0xFF000000u | ((uint32_t)(40 + i * 60) << 16) | ((uint32_t)(30 + j * 70) << 8) | (uint32_t)((i ^ j) * 80 + 15);
            if (drv->textura(0, 4, 4, tx) != G3D_OK) { klog("[GPU3D] v3: no se pudo cargar la textura de la autoprueba\n"); bien = false; break; }
            e.unidades = 1;
            gpu3d_unidade_t *u = &e.u[0];
            u->ativa = 1; u->vaga = 0; u->tipo = G3D_TEX_2D; u->filtro = G3D_FIL_LINEAR_MAG | G3D_FIL_LINEAR_MIN;
            u->env_s = u->env_t = G3D_ENV_REPETIR; u->aniso = 1; u->coord = 0;
            u->rgb_func = u->alfa_func = G3D_CB_MODULAR; u->rgb_escala = u->alfa_escala = 1;
            u->rgb_fonte[0] = u->alfa_fonte[0] = G3D_FT_TEXTURA; u->rgb_fonte[1] = u->alfa_fonte[1] = G3D_FT_ANTERIOR;
            u->rgb_fonte[2] = u->alfa_fonte[2] = G3D_FT_CONSTANTE;
            for (int k = 0; k < 3; k++) { u->rgb_opd[k] = G3D_OPD_COR; u->alfa_opd[k] = G3D_OPD_ALFA; }
        }
        p3_mem.v = (const uint8_t *)vs; p3_mem.nv = (uint32_t)nv * sizeof(p3_vert_t);
        p3_mem.i = (const uint8_t *)idx; p3_mem.ni = (uint32_t)ni * 2;
        /* la referencia: la CPU transforma y la GPU pinta (v2) */
        int r = drv->limpar(3, AP_FUNDO);
        g3d_v3_cpu_t c = { p3_ler, p3_sair, NULL, lote, 96 };
        for (int k = 0; k < nd && r == G3D_OK; k++) {
            g3d_desenho3_t g = { &e, t, NULL, d[k], 0, p3_mem.ni };
            r = g3d_v3_cpu(&g, &c);
        }
        if (r == G3D_OK) r = drv->ler(img + AP_L * AP_A, AP_L);
        if (r == G3D_OK && (diag & GPU3D_PROVA_VER)) {
            /* Diagnóstico: ¿la imagen es estable? La misma lectura otra vez y el mismo dibujo otra vez. */
            uint32_t *otra = kmalloc(AP_L * AP_A * 4);
            if (otra) {
                int dl = 0, dd = 0;
                if (drv->ler(otra, AP_L) == G3D_OK)
                    for (int i = 0; i < AP_L * AP_A; i++) dl += (otra[i] ^ img[AP_L * AP_A + i]) & 0xFFFFFFu ? 1 : 0;
                int r2 = drv->limpar(3, AP_FUNDO);
                for (int k = 0; k < nd && r2 == G3D_OK; k++) {
                    g3d_desenho3_t g = { &e, t, NULL, d[k], 0, p3_mem.ni };
                    r2 = g3d_v3_cpu(&g, &c);
                }
                if (r2 == G3D_OK && drv->ler(otra, AP_L) == G3D_OK)
                    for (int i = 0; i < AP_L * AP_A; i++) dd += (otra[i] ^ img[AP_L * AP_A + i]) & 0xFFFFFFu ? 1 : 0;
                int negros = 0, nz = -1;
                for (int i = 0; i < AP_L * AP_A; i++) negros += (img[AP_L * AP_A + i] & 0xFFFFFFu) == 0;
                /* y sin la prueba de Z */
                gpu3d_estado2_t ez = e;
                ez.flags &= ~(uint32_t)(G3D2_Z_TESTE | G3D2_Z_ESCRITA);
                r2 = drv->limpar(3, AP_FUNDO);
                for (int k = 0; k < nd && r2 == G3D_OK; k++) {
                    g3d_desenho3_t g = { &ez, t, NULL, d[k], 0, p3_mem.ni };
                    r2 = g3d_v3_cpu(&g, &c);
                }
                if (r2 == G3D_OK && drv->ler(otra, AP_L) == G3D_OK) {
                    nz = 0;
                    for (int i = 0; i < AP_L * AP_A; i++) nz += (otra[i] & 0xFFFFFFu) == 0;
                }
                klog("[GPU3D] v3: escena %d: referencia con %d pixeles negros; leida otra vez: %d distintos; dibujada otra vez: %d distintos; "
                     "sin Z: %d negros\n", escena, negros, dl, dd, nz);
                kfree(otra);
            }
        }
        if (r == G3D_FALHA) P3_FALHA_GPU();
        if (r != G3D_OK) { klog("[GPU3D] v3: escena %d (%s): el driver no dibujo la referencia por v2\n", escena, nomes_prova3[escena]); bien = false; break; }
        /* la GPU entera: búferes residentes y desenhar3 */
        r = drv->limpar(3, AP_FUNDO);
        if (r == G3D_OK) r = drv->vbuf(0, p3_mem.nv, 0, NULL, 0);
        if (r == G3D_OK) r = drv->vbuf(0, p3_mem.nv, 0, vs, p3_mem.nv);
        if (r == G3D_OK && ni) r = drv->vbuf(1, p3_mem.ni, 0, NULL, 0);
        if (r == G3D_OK && ni) r = drv->vbuf(1, p3_mem.ni, 0, idx, p3_mem.ni);
        for (int k = 0; k < nd && r == G3D_OK; k++) {
            g3d_desenho3_t g = { &e, t, NULL, d[k], 0, p3_mem.ni };
            g.max_vert = (uint32_t)nv - 1;
            r = drv->desenhar3(&g, NULL);
        }
        if (r == G3D_OK) r = drv->ler(img, AP_L);
        drv->vbuf(0, 0, 0, NULL, 0);
        if (ni) drv->vbuf(1, 0, 0, NULL, 0);
        if (escena == 2) drv->textura(0, 0, 0, NULL);
        if (r == G3D_FALHA) P3_FALHA_GPU();
        if (r != G3D_OK) { klog("[GPU3D] v3: escena %d (%s): el driver no acepto el dibujo desde los buferes\n", escena, nomes_prova3[escena]); bien = false; break; }
        int px, py, cheio;
        int mal = p3_comparar(img, img + AP_L * AP_A, &px, &py, &cheio);
        bool ok = mal <= P3_MAX_MAL && cheio >= P3_MIN_CHEIO;
        if (!ok || (diag & GPU3D_PROVA_DETALLE))
            klog("[GPU3D] v3: escena %d (%s): %s; %d pixeles distintos de %d (se toleran %d), la referencia pinta %d%s\n", escena,
                 nomes_prova3[escena], ok ? "bien" : "FALLO", mal, AP_L * AP_A, P3_MAX_MAL, cheio, cheio < P3_MIN_CHEIO ? " (demasiado pocos)" : "");
        if (px >= 0 && (!ok || (diag & GPU3D_PROVA_DETALLE)))
            klog("[GPU3D] v3:   el peor, %d,%d: la GPU da %06x y la CPU %06x\n", px, py, img[py * AP_L + px] & 0xFFFFFFu,
                 img[AP_L * AP_A + py * AP_L + px] & 0xFFFFFFu);
        if (!ok || (diag & GPU3D_PROVA_VER)) p3_mapa(escena, img, img + AP_L * AP_A);
        if (!ok) { bien = false; if (!(diag & GPU3D_PROVA_VER)) break; }   /* con --ver, las tres escenas */
    }
fim:
    #undef P3_FALHA_GPU
    kfree(img); kfree(lote); kfree(vs); kfree(t);
    if (!viva) { prova3 = 2u | ((uint32_t)escena << 8); return false; }
    if (bien) { capas.bits3 = b3; prova3 = 1; }
    else prova3 = 2u | ((uint32_t)escena << 8);
    klog("[GPU3D] v3 (la GPU transforma y guarda los vertices, D457): autoprueba %s en %u us; bits 0x%x%s\n", bien ? "BIEN" : "FALLO",
         us_desde(t0), capas.bits3, bien ? "" : ": la transformacion sigue por la CPU (v2), el 3D sigue encendido");
    if (!bien) capas.vbufs = capas.max_luzes = 0;
    else capas.vbufs = GPU3D_VBUFS;
    return true;
}
#undef Q

static bool ligar(info_gpu3d_prova_t *r)
{
    info_gpu3d_prova_t local;
    if (!r) { memset(&local, 0, sizeof local); r = &local; }
    if (!drv) return false;
    char m[120] = "";
    if (drv == &gpu3d_r500) {
        /* D287: comparte el motor con el 2D por MMIO de acel_radeon.c: si ese 2D falló, no se arriesga. */
        const acel_driver_t *d2 = NULL;
        bool suspeito = false;
        if (!acel2d_placa(&disp, &geracao, &d2, &tela, &suspeito) || d2 != &acel_radeon) {
            estado = GPU3D_FALHOU;
            snformatar(motivo, sizeof motivo, "%s: la capa 2D ya no conoce la tarjeta", placa_nome);
            return false;
        }
        if (suspeito && (cab & GPU3D_CAB_MODO) != GPU3D_CAB_RADEON) {
            estado = GPU3D_FALHOU;
            snformatar(motivo, sizeof motivo, "%s: el 2D de esta tarjeta fallo (%s): no se arriesga el 3D (--gpu3d=radeon lo fuerza)",
                       placa_nome, acel2d_motivo());
            motivo[sizeof motivo - 1] = 0;
            klog("[GPU3D] %s\n", motivo);
            return false;
        }
    }
    if (drv == &gpu3d_intel) {
        const acel_driver_t *d2 = NULL;
        bool suspeito = false;
        if (!acel2d_placa(&disp, &geracao, &d2, &tela, &suspeito) || d2 != &acel_intel) {
            estado = GPU3D_FALHOU;
            snformatar(motivo, sizeof motivo, "%s: la capa 2D ya no conoce la tarjeta", placa_nome);
            return false;
        }
        if (suspeito && (cab & GPU3D_CAB_MODO) != GPU3D_CAB_INTEL) {
            estado = GPU3D_FALHOU;
            snformatar(motivo, sizeof motivo, "%s: el 2D de esta tarjeta fallo (%s): no se arriesga el 3D (--gpu3d=intel lo fuerza)",
                       placa_nome, acel2d_motivo());
            motivo[sizeof motivo - 1] = 0;
            klog("[GPU3D] %s\n", motivo);
            return false;
        }
        if (!hw_tsc_khz()) {
            estado = GPU3D_FALHOU;
            snformatar(motivo, sizeof motivo, "%s: sin TSC medido para los plazos de la GPU", placa_nome);
            return false;
        }
    }
    if (drv == &gpu3d_intel9) {
        /* D438: el 3D va por el RCS y el 2D (D434) por el BCS: motores distintos, así que un 2D
         * fallido no lo impide (el driver vuelve a mirar pantalla, GTT y memoria robada). */
        const acel_driver_t *d2 = NULL;
        bool suspeito = false;
        if (!acel2d_placa(&disp, &geracao, &d2, &tela, &suspeito) || d2 != &acel_intel_exec || !hw_tsc_khz()) {
            estado = GPU3D_FALHOU;
            snformatar(motivo, sizeof motivo, "%s: la capa 2D ya no conoce la tarjeta o no hay TSC medido para los plazos", placa_nome);
            return false;
        }
        if (suspeito) klog("[GPU3D] aviso: el 2D por execlists de esta tarjeta no esta activo (%s); el 3D usa otro motor y se prueba igual\n", acel2d_motivo());
    }
    if (drv == &gpu3d_r600 || drv == &gpu3d_evergreen) {   /* D427: las Evergreen/NI, con las mismas reglas */
        const acel_driver_t *d2 = NULL;
        bool suspeito = false;
        if (!acel2d_placa(&disp, &geracao, &d2, &tela, &suspeito) || d2 != &acel_r600 || !hw_tsc_khz()) {
            estado = GPU3D_FALHOU;
            snformatar(motivo, sizeof motivo, "%s: la capa 2D ya no conoce la tarjeta o no hay TSC medido para los plazos", placa_nome);
            return false;
        }
        /* El 3D va por el procesador de órdenes y el 2D por el motor DMA: son independientes, así
         * que un 2D fallido no lo impide (en la Intel sí, porque comparten el ring). */
        if (suspeito) klog("[GPU3D] aviso: el 2D por DMA de esta tarjeta no esta activo (%s); el 3D no lo usa y se prueba igual\n", acel2d_motivo());
    }
    klog("[GPU3D] encender %s (%s)%s%s\n", drv->nome, placa_nome, variante & GPU3D_CAB_ANEL ? ", ordenes en el anillo" : "",
         variante & GPU3D_CAB_GTT ? ", RAM en el GTT" : "");
    ajustes_carregar();
    int ri = drv->iniciar(&disp, geracao, &tela, variante, m, sizeof m);
    if (ri != G3D_OK) {
        if (ri == G3D_FALHA) n_falhas++;
        soltar_driver();
        estado = GPU3D_FALHOU;
        snformatar(motivo, sizeof motivo, "%s: no se pudo iniciar el 3D: %s", placa_nome, m);
        klog("[GPU3D] software: %s\n", motivo);
        return false;
    }
    estado = GPU3D_ATIVO;   /* para la autoprueba; si falla, falha() lo cambia */
    if (!autoprova(r, m, sizeof m)) {
        char mm[140];
        snformatar(mm, sizeof mm, "autoprueba fallida: %s", m);
        falha(mm);
        return false;
    }
    us_autoprova = r->us_autoprova;
    capas_calcular();
    if (bits3_driver && !autoprova3()) {
        /* autoprova3 solo devuelve false si la GPU dejó de responder: lo demás (píxeles distintos) apaga v3, no el 3D */
        falha("la GPU no respondio a tiempo en la autoprueba de gpu3d v3");
        return false;
    }
    alvo_l = alvo_a = 0; dono = 0;
    uint32_t ram_kb, vr = variante_real(&ram_kb);
    char gtt[48] = "";
    if (vr & GPU3D_VAR_GTT) snformatar(gtt, sizeof gtt, "; RAM en el GTT%s", vr & GPU3D_VAR_GTT_TEX ? " con texturas" : "");
    else if (variante & GPU3D_CAB_GTT) snformatar(gtt, sizeof gtt, "; sin la RAM en el GTT pedida");
    snformatar(motivo, sizeof motivo, "%s: autoprueba superada en %u ms (por autoprueba; %s%s)", placa_nome,
               (us_autoprova + 999) / 1000, vr & GPU3D_VAR_ANEL ? "ordenes en el anillo" : "lotes", gtt);
    klog("[GPU3D] activo: %s\n", motivo);
    return true;
}

/* ------------------------------------------------------------ las llamadas */

/* D436 (audio): el hilo que está dentro de gpu3d_op (0: ninguno). Como las esperas del driver ceden la CPU
 * (gpu3d_respirar), otro hilo del mismo programa podría entrar mientras tanto y pisar lo que el driver tiene a medias
 * (la cola preparada, el estado): espera en la puerta. «gpu3d apagar/encender/probar» tampoco entran. Si el hilo murió
 * dentro (le mataron mientras cedía), la puerta se abre: lo que dejó en la cola son órdenes completas. */
static int op_hilo;
static uint32_t n_respiros, n_puerta, n_respiros_fpu;
static bool respirar_sempre;   /* pruebas (simulado, 0x1010): ceder en cada llamada, sin esperar 2 ms */
void gpu3d_respirar_forcar(bool sim) { respirar_sempre = sim; }
/* Las entradas del driver que usa el 2D (acel_radeon.c: órdenes por la cola del CP, la vuelta por la GART) o la energía
 * no ceden nunca: quien las llama puede tener las interrupciones cerradas a propósito o la FPU tomada (y no lo sabemos). */
static int resp_bloqueo;
void gpu3d_respirar_bloquear(bool sim) { if (sim) resp_bloqueo++; else if (resp_bloqueo > 0) resp_bloqueo--; }

static bool op_hilo_vivo(void)
{
    if (!op_hilo) return false;
    processo_t *o = processo_por_pid(op_hilo);
    if (o && o->estado != P_ZUMBI && o->estado != P_LIVRE) return true;
    klog("[GPU3D] el hilo %d termino dentro de una orden del 3D: la puerta queda libre\n", op_hilo);
    op_hilo = 0;
    return false;
}

/* sti; nop; cli: las interrupciones pendientes entran en el nop (el reloj puede cambiar de proceso ahí mismo) */
static void deixar_irq(void)
{
    bool i0 = int_salva_desliga();
    int_liga();
    __asm__ volatile("nop");
    int_desliga();
    int_restaura(i0);
}

void gpu3d_respirar(void)
{
    static uint64_t prox;
    processo_t *eu = processo_atual();
    if (!eu || !eu->usuario || eu->pid == 0 || eu->pid != op_hilo || contexto_irq() || resp_bloqueo) return;
    /* Amilo, 2026-11-21 (#NM en el kernel, en sse3_copiar): dentro de un fpu_kernel_inicio/fim (las copias con SSE,
     * rapido_inicio) NUNCA: ese tramo cuenta con las interrupciones cerradas y con la FPU suya; al volver de otro
     * proceso CR0.TS está a 1 y la siguiente instrucción SSE es una excepción del kernel. Ahí se sigue girando. */
    if (fpu_kernel_ativo()) {
        if (!n_respiros_fpu++) klog("[GPU3D] una espera dentro de una copia con SSE: no se cede la CPU ahi (D436)\n");
        return;
    }
    uint32_t khz = hw_tsc_khz();
    uint64_t t = prazo_tsc();
    if (!khz || (t < prox && !respirar_sempre)) return;
    prox = t + (uint64_t)khz * GPU3D_RESPIRAR_MS;
    n_respiros++;
    deixar_irq();
    ceder();   /* vuelve enseguida si no hay nadie más listo */
}

static bool dono_vivo(void)
{
    if (!dono) return false;
    if (processo_por_pid(dono)) return true;
    klog("[GPU3D] el proceso %d termino sin soltar el motor: queda libre\n", dono);
    dono = 0; alvo_l = alvo_a = 0;
    return false;
}

/* x, y dentro del destino (con un margen), z de 0 a 1 y lo demás finito y razonable:
 * la GPU no recorta y no debe recibir números que no sean de un triángulo de verdad. */
static bool coord_ok(uint32_t x, uint32_t y, uint32_t z, uint32_t rhw)
{
    int32_t px = g3d_fixo_de_float(x, 4), py = g3d_fixo_de_float(y, 4), pz = g3d_fixo_de_float(z, 16);
    if (px < -32 || px > (dest_l + 2) * 16 || py < -32 || py > (dest_a + 2) * 16 || pz < 0 || pz > 65536) return false;
    if ((rhw & 0x80000000u) || ((rhw >> 23) & 0xFF) > 127 + 24) return false;
    return true;
}

/* D425: lo mismo para los vértices v2; las coordenadas de textura, finitas y como mucho 2^15. */
static bool vertices2_ok(const gpu3d_vert2_t *v, int n)
{
    for (int i = 0; i < n; i++) {
        const uint32_t c[4] = { v[i].x, v[i].y, v[i].z, v[i].rhw };
        for (int k = 0; k < 4; k++) if (((c[k] >> 23) & 0xFF) == 0xFF) return false;
        if (!coord_ok(v[i].x, v[i].y, v[i].z, v[i].rhw)) return false;
        for (int u = 0; u < 4; u++)
            for (int k = 0; k < 4; k++) if (((v[i].tc[u][k] >> 23) & 0xFF) > 127 + 15) return false;
    }
    return true;
}

/* D425: un estado v2 con valores conocidos y que no pide más de lo que el driver anunció. */
static bool estado2_ok(const gpu3d_estado2_t *e, bool *sem_suporte)
{
    *sem_suporte = false;
    uint32_t b = capas.bits;
    if (e->versao != 2 || e->func_z > 7 || e->func_alfa > 7 || e->mascara_cor > 15 || e->op_logica > 15 ||
        e->mez_orig_rgb > G3D_F_ALFA_SATURADO || e->mez_orig_alfa > G3D_F_ALFA_SATURADO ||
        e->mez_dest_rgb > G3D_F_UM_MENOS_ALFA_DEST || e->mez_dest_alfa > G3D_F_UM_MENOS_ALFA_DEST ||
        e->mez_eq_rgb > G3D_EQ_SUBTRAI_INV || e->mez_eq_alfa > G3D_EQ_SUBTRAI_INV || e->unidades > 4 ||
        e->est_func > 7 || e->est_falha > 7 || e->est_zfalha > 7 || e->est_zpassa > 7 ||
        (e->flags & ~0xFFu)) return false;
    if ((e->flags & G3D2_TIJERA) && ((int)e->tijera_x + e->tijera_l > dest_l || (int)e->tijera_y + e->tijera_a > dest_a))
        return false;
    bool falta = !(b & G3D_CAP_V2) || e->unidades > capas.unidades;
    if ((e->flags & G3D2_ALFA_TESTE) && !(b & G3D_CAP_ALFA_TESTE)) falta = true;
    if ((e->flags & G3D2_ESTENCIL) && !(b & G3D_CAP_ESTENCIL)) falta = true;
    if ((e->flags & G3D2_TIJERA) && !(b & G3D_CAP_TIJERA)) falta = true;
    if ((e->flags & G3D2_NEBLINA) && !(b & G3D_CAP_NEBLINA)) falta = true;
    if ((e->flags & G3D2_LOGICA) && !(b & G3D_CAP_LOGICA)) falta = true;
    if (e->mascara_cor != 15 && !(b & G3D_CAP_MASCARAS)) falta = true;
    if (e->func_z != G3D_CMP_MENOR && e->func_z != G3D_CMP_MENOR_IGUAL && e->func_z != G3D_CMP_SEMPRE && !(b & G3D_CAP_Z_FUNCS))
        falta = true;
    if (e->flags & G3D2_MEZCLA) {
        if ((e->mez_orig_rgb > G3D_F_UM_MENOS_ALFA || e->mez_dest_rgb > G3D_F_UM_MENOS_ALFA ||
             e->mez_orig_alfa > G3D_F_UM_MENOS_ALFA || e->mez_dest_alfa > G3D_F_UM_MENOS_ALFA) && !(b & G3D_CAP_MEZCLA_EXT))
            falta = true;
        if ((e->mez_orig_rgb != e->mez_orig_alfa || e->mez_dest_rgb != e->mez_dest_alfa || e->mez_eq_rgb != e->mez_eq_alfa) &&
            !(b & G3D_CAP_MEZCLA_SEP)) falta = true;
        if ((e->mez_eq_rgb || e->mez_eq_alfa) && !(b & G3D_CAP_MEZCLA_EQ)) falta = true;
    }
    for (int i = 0; i < e->unidades; i++) {
        const gpu3d_unidade_t *u = &e->u[i];
        if (!u->ativa) continue;
        if (u->vaga >= GPU3D_TEXTURAS || u->tipo > G3D_TEX_CUBO || u->filtro > 15 || u->env_s > 2 || u->env_t > 2 ||
            u->coord > 3 || u->rgb_func > G3D_CB_DOT3_RGBA || u->alfa_func > G3D_CB_SUBTRAIR) return false;
        if ((u->rgb_escala != 1 && u->rgb_escala != 2 && u->rgb_escala != 4) ||
            (u->alfa_escala != 1 && u->alfa_escala != 2 && u->alfa_escala != 4)) return false;
        for (int k = 0; k < 3; k++)
            if (u->rgb_fonte[k] > 3 || u->rgb_opd[k] > 3 || u->alfa_fonte[k] > 3 || u->alfa_opd[k] < 2 || u->alfa_opd[k] > 3)
                return false;
        if ((int)u->vaga == dest_vaga) return false;   /* no se lee la textura en la que se dibuja */
        bool classico = u->rgb_func <= G3D_CB_MODULAR && u->alfa_func <= G3D_CB_MODULAR && u->rgb_escala == 1 && u->alfa_escala == 1;
        if (!classico && !(b & G3D_CAP_COMBINAR)) falta = true;
        if (u->rgb_func >= G3D_CB_DOT3_RGB && !(b & G3D_CAP_DOT3)) falta = true;
        if (u->tipo == G3D_TEX_CUBO && !(b & G3D_CAP_TEX_CUBO)) falta = true;
        if ((u->env_s == G3D_ENV_ESPELHO || u->env_t == G3D_ENV_ESPELHO) && !(b & G3D_CAP_TEX_ESPELHO)) falta = true;
        if ((u->filtro & (G3D_FIL_MIP_VECINO | G3D_FIL_MIP_LINEAR)) && !(b & G3D_CAP_TEX_MIPMAP)) falta = true;
        if (u->aniso > capas.max_aniso) falta = true;
        /* lados que no son potencia de 2: sin mipmaps (NPOT) o con ellos (NPOT_MIP) */
        int l = vaga_l[u->vaga], a = vaga_a[u->vaga];
        if ((l & (l - 1)) || (a & (a - 1))) {
            if (!(b & G3D_CAP_TEX_NPOT)) falta = true;
            if ((u->filtro & (G3D_FIL_MIP_VECINO | G3D_FIL_MIP_LINEAR)) && !(b & G3D_CAP_TEX_NPOT_MIP)) falta = true;
        }
    }
    *sem_suporte = falta;
    return true;
}

static bool vertices_ok(const gpu3d_vert_t *v, int n)
{
    for (int i = 0; i < n; i++) {
        const uint32_t c[6] = { v[i].x, v[i].y, v[i].z, v[i].rhw, v[i].u, v[i].v };
        for (int k = 0; k < 6; k++) if (((c[k] >> 23) & 0xFF) == 0xFF) return false;
        if (!coord_ok(v[i].x, v[i].y, v[i].z, v[i].rhw)) return false;
        if (((v[i].u >> 23) & 0xFF) > 127 + 7 || ((v[i].v >> 23) & 0xFF) > 127 + 7) return false;
    }
    return true;
}

/* ---- gpu3d v3 (D457): búferes residentes ---- */

static void vb_soltar(int i)
{
    if (!vb[i].presente) return;
    if (drv && drv->vbuf) drv->vbuf(i, 0, 0, NULL, 0);
    vb_bytes -= vb[i].bytes; vb_n--;
    memset(&vb[i], 0, sizeof vb[i]);
}

static void vb_soltar_todos(void)
{
    for (int i = 0; i < GPU3D_VBUFS; i++) vb_soltar(i);
    transf_valida = luzes_validas = false;
}

/* Expulsa el búfer que lleva más tiempo sin usarse (salvo «exceto»). false: no queda ninguno. */
static bool vb_expulsar(int exceto)
{
    int m = -1;
    for (int i = 0; i < GPU3D_VBUFS; i++)
        if (vb[i].presente && i != exceto && (m < 0 || vb[i].seq < vb[m].seq)) m = i;
    if (m < 0) return false;
    if (!n_vb_expulsos) klog("[GPU3D] v3: la memoria de la GPU para vertices esta llena (%u buferes, %u KB): se expulsa el menos usado (el programa lo vuelve a subir si lo dibuja)\n", vb_n, vb_bytes >> 10);
    vb_soltar(m);
    n_vb_expulsos++;
    return true;
}

static bool floats_ok(const uint32_t *f, int n)
{
    for (int i = 0; i < n; i++) if (((f[i] >> 23) & 0xFF) == 0xFF) return false;
    return true;
}

static uint32_t atrib_bytes(const gpu3d_atrib_t *a) { return a->formato == G3D_AT_UBYTE4N ? 4u : (uint32_t)a->formato * 4u; }
static bool atrib_ok(const gpu3d_atrib_t *a, uint32_t formatos)
{
    if (a->formato > G3D_AT_UBYTE4N || !(formatos & (1u << a->formato))) return false;
    if (a->formato == G3D_AT_NADA) return true;
    return a->vaga < GPU3D_VBUFS && a->passo >= 4 && !(a->passo & 3) && !(a->desloc & 3) && a->desloc < GPU3D_VBUF_MAX_BYTES;
}

/* Una transformación con valores conocidos, la vista dentro del destino y nada que el driver no anunciara. */
static bool transf_ok(const gpu3d_transf_t *t, bool *sem_suporte)
{
    *sem_suporte = false;
    uint32_t b = capas.bits3;
    if (t->versao != 3 || (t->flags & ~0x1F7u) || t->neb_modo > G3D_NEB_EXP2 || t->descarte > G3D_DESC_NEG) return false;
    if (!floats_ok(t->mvp, 16) || !floats_ok(t->mv, 16) || !floats_ok(&t->tex[0][0], 64) || !floats_ok(t->normal, 9) ||
        !floats_ok(t->vp_escala, 3) || !floats_ok(t->vp_desloc, 3) || !floats_ok(t->normal_const, 3) ||
        !floats_ok(&t->tc_const[0][0], 16) || !floats_ok(&t->neb_a, 2)) return false;
    /* la vista, en 1/16 de píxel: desloc +- |escala| dentro del destino (la GPU recorta a la vista) */
    int32_t ex = g3d_fixo_de_float(t->vp_escala[0] & 0x7FFFFFFFu, 4), ox = g3d_fixo_de_float(t->vp_desloc[0], 4);
    int32_t ey = g3d_fixo_de_float(t->vp_escala[1] & 0x7FFFFFFFu, 4), oy = g3d_fixo_de_float(t->vp_desloc[1], 4);
    if (ex < 8 || ey < 8 || ox - ex < -1 || oy - ey < -1 || ox + ex > dest_l * 16 + 1 || oy + ey > dest_a * 16 + 1) return false;
    int32_t ez = g3d_fixo_de_float(t->vp_escala[2] & 0x7FFFFFFFu, 16), oz = g3d_fixo_de_float(t->vp_desloc[2], 16);
    if (oz - ez < -1 || oz + ez > 65537) return false;
    bool falta = !(b & G3D_C3_V3);
    if ((t->flags & G3D3_NEBLINA) && !(b & G3D_C3_NEBLINA)) falta = true;
    if ((t->flags & G3D3_NEBLINA) && t->neb_modo != G3D_NEB_LINEAR && !(b & G3D_C3_NEB_EXP)) falta = true;
    if ((t->flags & G3D3_LUZ) && !(b & G3D_C3_LUZ)) falta = true;
    if ((t->flags & 0xF0u) && !(b & G3D_C3_TEX_MAT)) falta = true;
    *sem_suporte = falta;
    return true;
}

/* El atributo cabe en lo subido a su búfer para los vértices 0..max_vert. 0 bien, GPU3D_EXPULSO o un error. */
static int32_t atrib_cabe(const gpu3d_atrib_t *a, uint32_t max_vert)
{
    if (a->formato == G3D_AT_NADA) return 0;
    vb_t *k = &vb[a->vaga];
    if (!k->presente) return GPU3D_EXPULSO;
    if (k->uso != G3D_VB_VERTICES) return ERRO_INVALIDO;
    uint64_t fim = (uint64_t)a->desloc + (uint64_t)max_vert * a->passo + atrib_bytes(a);
    if (fim > k->cheio) return ERRO_INVALIDO;
    k->seq = vb_seq;
    return 0;
}

static int32_t op_dentro(uint32_t ptr, uint32_t n);

int32_t gpu3d_op(uint32_t ptr, uint32_t n)
{
    processo_t *eu = processo_atual();
    /* D436: un hilo cada vez (ver op_hilo); el que espera deja correr al de dentro */
    while (op_hilo && op_hilo != eu->pid && op_hilo_vivo()) {
        n_puerta++;
        deixar_irq();
        ceder();
    }
    int antes = op_hilo;
    op_hilo = eu->pid;
    int32_t r = op_dentro(ptr, n);
    op_hilo = antes == eu->pid ? antes : 0;
    /* diagnóstico: las primeras negativas, con la operación (el programa sigue por software y no dice cuál fue) */
    static uint32_t negativas;
    if ((r == GPU3D_SEM_SUPORTE || r == ERRO_INVALIDO) && negativas < 8 && usuario_ok((void *)ptr, sizeof(gpu3d_pedido_t), false)) {
        const gpu3d_pedido_t *q = (const gpu3d_pedido_t *)ptr;
        if (q->op != GPU3D_OP_ABRIR && q->op != GPU3D_OP_CAPACIDADES) {
            negativas++;
            klog("[GPU3D] el proceso %d: la orden %u (a %u b %u c %u) no se hace: %s\n", eu->pid, q->op, q->a, q->b, q->c,
                 r == GPU3D_SEM_SUPORTE ? "no la admite" : "invalida");
        }
    }
    return r;
}

static int32_t op_dentro(uint32_t ptr, uint32_t n)
{
    gpu3d_pedido_t *q = (gpu3d_pedido_t *)ptr;
    if (n < sizeof *q || !usuario_ok(q, sizeof *q, false)) return ERRO_ENDERECO;
    gpu3d_pedido_t p = *q;
    int pid = processo_recursos()->pid;
    if (p.op == GPU3D_OP_ABRIR) {
        if (estado != GPU3D_ATIVO && estado != GPU3D_SIN_PROBAR) return GPU3D_SEM_SUPORTE;
        if (dono != pid && dono_vivo()) { n_recusas++; return GPU3D_SEM_SUPORTE; }
        if (p.a < 8 || p.b < 8 || p.a > GPU3D_MAX_LADO || p.b > GPU3D_MAX_LADO) return GPU3D_SEM_SUPORTE;
        if (estado == GPU3D_SIN_PROBAR && !ligar(NULL)) return GPU3D_SEM_SUPORTE;
        for (int i = 0; i < GPU3D_TEXTURAS; i++) drv->textura(i, 0, 0, NULL);
        if (drv->v4 && drv->v4->soltar_tudo) drv->v4->soltar_tudo();   /* D480 */
        vb_soltar_todos();   /* D457 */
        n_vb_expulsos = n_vb_subidas = vb_kb_subidos = n_des3 = n_vert3 = 0;
        /* D425: c = bits de Z | bits de esténcil << 8. El esténcil va con un Z de 24 (Z24S8): un driver
         * v1 recibe solo los bits de Z, como siempre. */
        int zb_ped = (int)(p.c & 0xFF), est_ped = (int)((p.c >> 8) & 0xFF);
        if (est_ped && zb_ped == 16) zb_ped = 24;
        p.c = (uint32_t)zb_ped;
        int r = drv->alvo((int)p.a, (int)p.b, (int)p.c);
        if (r == G3D_FALHA) { falha("la GPU no creo el destino"); return ERRO_TEMPO; }
        if (r != G3D_OK) {
            klog("[GPU3D] el destino de %ux%u no cabe en la memoria de la GPU: ese programa sigue por software\n", p.a, p.b);
            dono = 0; alvo_l = alvo_a = 0;
            n_recusas++;
            return GPU3D_SEM_SUPORTE;
        }
        dono = pid; alvo_l = (int)p.a; alvo_a = (int)p.b; alvo_z = (int)p.c;
        est_atual = (gpu3d_estado_t){ 0, G3D_Z_MENOR, G3D_F_UM, G3D_F_ZERO, 0 };
        est2_valido = false; dest_vaga = -1; dest_l = alvo_l; dest_a = alvo_a;
        transf_valida = luzes_validas = false;
        memset(vaga_l, 0, sizeof vaga_l); memset(vaga_a, 0, sizeof vaga_a);
        return 0;
    }
    if (p.op == GPU3D_OP_CAPACIDADES) {
        /* D425: lo puede preguntar cualquiera; antes del primer uso, un driver sin probar dice lo de v1. */
        if (!usuario_ok((void *)p.ptr, sizeof(gpu3d_capas_t), true)) return ERRO_ENDERECO;
        gpu3d_capas_t c;
        if (estado == GPU3D_ATIVO) {
            c = capas;
            /* D457: para «gpu3d» e infopc: la autoprueba v3 y la contabilidad de los búferes */
            c.reservado[0] = prova3 | (bits3_driver << 16); c.reservado[1] = vb_n; c.reservado[2] = vb_bytes >> 10;
            c.reservado[3] = n_vb_expulsos; c.reservado[4] = n_des3;
        }
        else { memset(&c, 0, sizeof c); c.versao = 2; c.unidades = 1; c.max_lado = GPU3D_MAX_LADO; c.max_aniso = 1;
               c.formatos = 1u << G3D_FMT_ARGB8888; c.zbits = 3; }
        *(gpu3d_capas_t *)p.ptr = c;
        return estado == GPU3D_ATIVO || estado == GPU3D_SIN_PROBAR ? 0 : GPU3D_SEM_SUPORTE;
    }
    if (estado != GPU3D_ATIVO || dono != pid || !alvo_l) return ERRO_ACESSO;
    int r = G3D_OK;
    const char *que = "";
    switch (p.op) {
    case GPU3D_OP_FECHAR:
        gui_exclusiva_soltar(pid, "el programa suelta el motor 3D");   /* D432 */
        klog("[GPU3D] el proceso %d suelta el motor; las esperas de la GPU cedieron la CPU %u veces, %u esperas en la puerta, "
             "%u dentro de una copia con SSE sin ceder (D436)\n", pid, n_respiros, n_puerta, n_respiros_fpu);
        if (dest_vaga >= 0 && drv->destino) drv->destino(-1, alvo_z, 0);
        dest_vaga = -1;
        for (int i = 0; i < GPU3D_TEXTURAS; i++) drv->textura(i, 0, 0, NULL);
        if (drv->v4 && drv->v4->soltar_tudo) {
            if (n_v4_des) klog("[GPU3D] v4 (D480): %u dibujos, %u limpiezas, %u subidas, %u lecturas, %u programas y %u recursos al soltar\n",
                               n_v4_des, n_v4_limpar, n_v4_subir, n_v4_ler, n_v4_prog, n_v4_rec);
            drv->v4->soltar_tudo();
            n_v4_des = n_v4_limpar = n_v4_subir = n_v4_ler = n_v4_prog = n_v4_rec = 0;
        }
        if (n_des3 || n_vb_subidas)
            klog("[GPU3D] v3 (D457): %u dibujos desde buferes (%u vertices), %u subidas (%u KB), %u buferes (%u KB) al soltar, %u expulsiones\n",
                 n_des3, n_vert3, n_vb_subidas, vb_kb_subidos, vb_n, vb_bytes >> 10, n_vb_expulsos);
        vb_soltar_todos();
        dono = 0; alvo_l = alvo_a = 0;
        return 0;
    case GPU3D_OP_LIMPAR:
        r = drv->limpar(p.a & 3, p.b); que = "limpiar";
        break;
    case GPU3D_OP_TEXTURA:
        if (p.a >= GPU3D_TEXTURAS) return ERRO_INVALIDO;
        if (!p.ptr) { r = drv->textura((int)p.a, 0, 0, NULL); que = "soltar una textura"; break; }
        if (p.b < 1 || p.c < 1 || p.b > GPU3D_MAX_LADO || p.c > GPU3D_MAX_LADO) return GPU3D_SEM_SUPORTE;
        if (!usuario_ok((void *)p.ptr, p.b * p.c * 4, false)) return ERRO_ENDERECO;
        if ((int)p.a == dest_vaga) return ERRO_INVALIDO;
        r = drv->textura((int)p.a, (int)p.b, (int)p.c, (const uint32_t *)p.ptr); que = "cargar una textura";
        if (r == G3D_OK) { vaga_l[p.a] = (uint16_t)p.b; vaga_a[p.a] = (uint16_t)p.c; }
        break;
    case GPU3D_OP_ESTADO: {
        if (!usuario_ok((void *)p.ptr, sizeof est_atual, false)) return ERRO_ENDERECO;
        gpu3d_estado_t e = *(const gpu3d_estado_t *)p.ptr;
        if (e.func_z > G3D_Z_SEMPRE || e.mez_orig > G3D_F_UM_MENOS_ALFA || e.mez_dest > G3D_F_UM_MENOS_ALFA ||
            e.textura >= GPU3D_TEXTURAS) return ERRO_INVALIDO;
        est_atual = e;
        return 0;
    }
    case GPU3D_OP_TRIANGULOS: {
        if (!p.a) return 0;
        if (p.a > GPU3D_MAX_VERT || p.a % 3) return ERRO_INVALIDO;
        if (!usuario_ok((void *)p.ptr, p.a * sizeof(gpu3d_vert_t), false)) return ERRO_ENDERECO;
        if (!vertices_ok((const gpu3d_vert_t *)p.ptr, (int)p.a)) return ERRO_INVALIDO;
        /* Pruebas: la GPU «deja de responder» tras unas cuantas entregas. */
        if ((cab & GPU3D_CAB_P_TEMPO) && ++n_desenhos_prova > 2) { falha("tiempo agotado esperando a la GPU (simulado)"); return ERRO_TEMPO; }
        r = drv->desenhar(&est_atual, (const gpu3d_vert_t *)p.ptr, (int)p.a, NULL); que = "dibujar";
        if (r == G3D_OK) { n_tri += p.a / 3; n_lotes++; }
        break;
    }
    case GPU3D_OP_LER:
        /* D425: lee el destino actual (el principal o la textura de GPU3D_OP_DESTINO). */
        if (p.a < (uint32_t)dest_l || p.a > 8192) return ERRO_INVALIDO;
        if (!usuario_ok((void *)p.ptr, ((uint32_t)(dest_a - 1) * p.a + (uint32_t)dest_l) * 4, true)) return ERRO_ENDERECO;
        r = drv->ler((uint32_t *)p.ptr, (int)p.a); que = "leer el cuadro";
        if (r == G3D_OK) n_quadros++;
        break;
    /* ---- gpu3d v2 (D425) ---- */
    case GPU3D_OP_ESTADO2: {
        if (!usuario_ok((void *)p.ptr, sizeof est2_atual, false)) return ERRO_ENDERECO;
        gpu3d_estado2_t e = *(const gpu3d_estado2_t *)p.ptr;
        bool falta;
        if (!estado2_ok(&e, &falta)) return ERRO_INVALIDO;
        if (falta) { est2_valido = false; n_recusas++; return GPU3D_SEM_SUPORTE; }
        est2_atual = e; est2_valido = true;
        return 0;
    }
    case GPU3D_OP_TRIANGULOS2: {
        if (!p.a) return 0;
        if (!drv->desenhar2 || !est2_valido) return GPU3D_SEM_SUPORTE;
        if (p.a > GPU3D_MAX_VERT || p.a % 3) return ERRO_INVALIDO;
        if (!usuario_ok((void *)p.ptr, p.a * sizeof(gpu3d_vert2_t), false)) return ERRO_ENDERECO;
        if (!vertices2_ok((const gpu3d_vert2_t *)p.ptr, (int)p.a)) return ERRO_INVALIDO;
        if ((cab & GPU3D_CAB_P_TEMPO) && ++n_desenhos_prova > 2) { falha("tiempo agotado esperando a la GPU (simulado)"); return ERRO_TEMPO; }
        r = drv->desenhar2(&est2_atual, (const gpu3d_vert2_t *)p.ptr, (int)p.a, NULL); que = "dibujar (v2)";
        if (r == G3D_OK) { n_tri += p.a / 3; n_lotes++; }
        break;
    }
    case GPU3D_OP_TEXTURA2: {
        if (!usuario_ok((void *)p.ptr, sizeof(gpu3d_textura2_t), false)) return ERRO_ENDERECO;
        gpu3d_textura2_t t = *(const gpu3d_textura2_t *)p.ptr;
        if (t.vaga >= GPU3D_TEXTURAS || (int)t.vaga == dest_vaga) return ERRO_INVALIDO;
        if (!drv->textura2) return GPU3D_SEM_SUPORTE;
        if (!t.dados) {
            r = drv->textura2(&t); que = "soltar una textura";
            if (r == G3D_OK) vaga_l[t.vaga] = vaga_a[t.vaga] = 0;
            break;
        }
        if (t.formato > G3D_FMT_DXT5 || t.tipo > G3D_TEX_CUBO || t.face > 5 || (t.tipo == G3D_TEX_2D && t.face) ||
            t.nivel > 11 || t.niveis > 12 || t.larg < 1 || t.alt < 1) return ERRO_INVALIDO;
        if (t.larg > capas.max_lado || t.alt > capas.max_lado || !(capas.formatos & (1u << t.formato))) return GPU3D_SEM_SUPORTE;
        if (t.tipo == G3D_TEX_CUBO && (!(capas.bits & G3D_CAP_TEX_CUBO) || t.larg != t.alt)) return GPU3D_SEM_SUPORTE;
        if ((t.nivel || t.niveis > 1) && !(capas.bits & G3D_CAP_TEX_MIPMAP)) return GPU3D_SEM_SUPORTE;
        uint32_t precisa = t.formato == G3D_FMT_ARGB8888 ? t.larg * t.alt * 4 :
                           ((t.larg + 3) / 4) * ((t.alt + 3) / 4) * (t.formato == G3D_FMT_DXT1 ? 8u : 16u);
        if (t.bytes != precisa) return ERRO_INVALIDO;
        if (!usuario_ok((void *)t.dados, t.bytes, false)) return ERRO_ENDERECO;
        r = drv->textura2(&t); que = "cargar una textura (v2)";
        if (r == G3D_OK && t.nivel == 0 && t.face == 0) { vaga_l[t.vaga] = (uint16_t)t.larg; vaga_a[t.vaga] = (uint16_t)t.alt; }
        break;
    }
    case GPU3D_OP_LIMPAR2: {
        if (!usuario_ok((void *)p.ptr, sizeof(gpu3d_limpar2_t), false)) return ERRO_ENDERECO;
        gpu3d_limpar2_t l = *(const gpu3d_limpar2_t *)p.ptr;
        if (!drv->limpar2 || !(capas.bits & G3D_CAP_LIMPAR2)) return GPU3D_SEM_SUPORTE;
        if ((l.mascara & ~7u) || l.z > 0xFFFFFF || l.estencil > 255 || l.mascara_cor > 15) return ERRO_INVALIDO;
        if (l.tijera && ((int)l.tx + l.tl > dest_l || (int)l.ty + l.ta > dest_a)) return ERRO_INVALIDO;
        if ((l.mascara & G3D_LIMPA_EST) && !(capas.bits & G3D_CAP_ESTENCIL)) return GPU3D_SEM_SUPORTE;
        if ((l.tijera && !(capas.bits & G3D_CAP_TIJERA)) || (l.mascara_cor != 15 && !(capas.bits & G3D_CAP_MASCARAS)))
            return GPU3D_SEM_SUPORTE;
        r = drv->limpar2(&l); que = "limpiar (v2)";
        break;
    }
    case GPU3D_OP_DESTINO: {
        if (!drv->destino || !(capas.bits & G3D_CAP_DESTINO_TEX)) return GPU3D_SEM_SUPORTE;
        if ((p.b != 0 && p.b != 16 && p.b != 24) || (p.c != 0 && p.c != 8)) return ERRO_INVALIDO;
        if (p.a == GPU3D_DESTINO_PRINCIPAL) {
            r = drv->destino(-1, alvo_z, 0); que = "volver al destino principal";
            if (r == G3D_OK) { dest_vaga = -1; dest_l = alvo_l; dest_a = alvo_a; est2_valido = false; transf_valida = false; }
            break;
        }
        if (p.a >= GPU3D_TEXTURAS || !vaga_l[p.a] || vaga_l[p.a] < 8 || vaga_a[p.a] < 8) return ERRO_INVALIDO;
        if (p.c && !(capas.bits & G3D_CAP_ESTENCIL)) return GPU3D_SEM_SUPORTE;
        r = drv->destino((int)p.a, (int)p.b, (int)p.c); que = "dibujar en una textura";
        if (r == G3D_OK) { dest_vaga = (int)p.a; dest_l = vaga_l[p.a]; dest_a = vaga_a[p.a]; est2_valido = false; transf_valida = false; }
        break;
    }
    /* ---- D432: presentar sin volver a la RAM ---- */
    case GPU3D_OP_APRESENTAR: {
        if (!usuario_ok((void *)p.ptr, sizeof(gpu3d_apresentar_t), false)) return ERRO_ENDERECO;
        gpu3d_apresentar_t a = *(const gpu3d_apresentar_t *)p.ptr;
        if (!drv->apresentar || !(capas.bits & G3D_CAP_APRESENTAR)) return GPU3D_SEM_SUPORTE;
        if (a.flags & ~(G3D_APRES_LINEAR | G3D_APRES_VSYNC)) return ERRO_INVALIDO;
        g3d_apres_t g;
        int fl, fa;
        if (a.fonte == GPU3D_DESTINO_PRINCIPAL) {
            if (dest_vaga >= 0) return ERRO_INVALIDO;
            g.vaga = -1; fl = alvo_l; fa = alvo_a;
        } else {
            if (a.fonte >= GPU3D_TEXTURAS || !vaga_l[a.fonte] || (int)a.fonte == dest_vaga) return ERRO_INVALIDO;
            g.vaga = (int)a.fonte; fl = vaga_l[a.fonte]; fa = vaga_a[a.fonte];
        }
        if (!a.ol || !a.oa || a.ox + a.ol > fl || a.oy + a.oa > fa || !a.dl || !a.da) return ERRO_INVALIDO;
        /* ¿El compositor cede (o sigue cediendo) la pantalla a esa ventana? */
        int ox, oy, dl, da, jl, ja;
        bool nova = false;
        if (!gui_exclusiva_apresentar(pid, (int)a.janela, &ox, &oy, &dl, &da, &jl, &ja, &nova)) { n_apres_nao++; return GPU3D_SEM_SUPORTE; }
        int tl = (int)video_largura(), ta = (int)video_altura();
        if (a.dx + a.dl > jl || a.dy + a.da > ja) {
            gui_exclusiva_soltar(pid, "rectangulo de destino fuera de la ventana");
            return ERRO_INVALIDO;
        }
        /* Del contenido de la ventana (jl x ja) a donde lo pone la pantalla completa (dl x da en ox, oy). */
        g.ox = a.ox; g.oy = a.oy; g.ol = a.ol; g.oa = a.oa;
        g.dx = ox + a.dx * dl / jl; g.dy = oy + a.dy * da / ja;
        g.dl = ox + (a.dx + a.dl) * dl / jl - g.dx; g.da = oy + (a.dy + a.da) * da / ja - g.dy;
        if (g.dx < 0 || g.dy < 0 || g.dl < 1 || g.da < 1 || g.dx + g.dl > tl || g.dy + g.da > ta) {
            gui_exclusiva_soltar(pid, "rectangulo de destino fuera de la pantalla");
            n_apres_nao++;
            return GPU3D_SEM_SUPORTE;
        }
        g.flags = a.flags;
        if (nova || apres_bandas || g.dx != apres_dx || g.dy != apres_dy || g.dl != apres_dl || g.da != apres_da) g.flags |= G3D_APRES_BANDAS;
        r = drv->apresentar(&g); que = "presentar en la pantalla";
        if (r == G3D_OK) {
            if (nova) klog("[GPU3D] presentar (D432): fuente %s %dx%d+%d+%d de %dx%d -> pantalla %dx%d+%d+%d de %dx%d%s%s\n",
                           g.vaga < 0 ? "destino" : "textura", g.ol, g.oa, g.ox, g.oy, fl, fa, g.dl, g.da, g.dx, g.dy, tl, ta,
                           g.flags & G3D_APRES_LINEAR ? ", bilineal" : "", g.flags & G3D_APRES_VSYNC ? ", con espera al retrazado" : "");
            n_apres++;
            apres_dx = g.dx; apres_dy = g.dy; apres_dl = g.dl; apres_da = g.da;
            /* La flecha, encima del cuadro (el driver ya esperó a la GPU). Si cae sobre las bandas, el
             * próximo cuadro las repinta para borrarla de donde estaba. */
            int cx, cy;
            apres_bandas = video_cursor_exclusiva(&cx, &cy) &&
                           (cx < g.dx || cy < g.dy || cx + video_cursor_largura() > g.dx + g.dl || cy + video_cursor_altura() > g.dy + g.da);
        } else if (r == G3D_NAO) {
            gui_exclusiva_soltar(pid, "el driver no puede presentar eso");
            n_apres_nao++;
        }
        break;
    }
    /* ---- gpu3d v3 (D457): la GPU guarda y transforma los vértices ---- */
    case GPU3D_OP_VBUF: {
        if (!usuario_ok((void *)p.ptr, sizeof(gpu3d_vbuf_t), false)) return ERRO_ENDERECO;
        gpu3d_vbuf_t b = *(const gpu3d_vbuf_t *)p.ptr;
        if (!(capas.bits3 & G3D_C3_V3)) return GPU3D_SEM_SUPORTE;
        if (b.vaga >= GPU3D_VBUFS || b.uso > G3D_VB_INDICES16 || (b.flags & ~G3D_VB_NOVO)) return ERRO_INVALIDO;
        vb_t *k = &vb[b.vaga];
        if (!b.bytes) { vb_soltar((int)b.vaga); return 0; }
        if (b.uso == G3D_VB_INDICES16 && !(capas.bits3 & G3D_C3_INDICES)) return GPU3D_SEM_SUPORTE;
        if (b.flags & G3D_VB_NOVO) {
            if (b.bytes > GPU3D_VBUF_MAX_BYTES) return GPU3D_SEM_SUPORTE;
            vb_soltar((int)b.vaga);
            r = drv->vbuf((int)b.vaga, b.bytes, 0, NULL, 0); que = "crear un bufer de vertices";
            while (r == G3D_NAO && vb_expulsar((int)b.vaga)) r = drv->vbuf((int)b.vaga, b.bytes, 0, NULL, 0);
            if (r != G3D_OK) break;
            k->presente = true; k->bytes = b.bytes; k->cheio = 0; k->uso = (uint8_t)b.uso; k->max_idx = 0; k->seq = ++vb_seq;
            vb_bytes += b.bytes; vb_n++;
        } else if (!k->presente) return GPU3D_EXPULSO;
        else if (k->uso != b.uso || k->bytes != b.bytes) return ERRO_INVALIDO;
        if (!b.n) return 0;
        if (b.desloc > k->cheio || b.n > k->bytes || b.desloc > k->bytes - b.n) return ERRO_INVALIDO;
        if (!usuario_ok((void *)b.dados, b.n, false)) return ERRO_ENDERECO;
        if (b.uso == G3D_VB_INDICES16) {
            if ((b.desloc | b.n) & 1) return ERRO_INVALIDO;
            const uint16_t *ix = (const uint16_t *)b.dados;
            for (uint32_t i = 0; i < b.n / 2; i++) if (ix[i] > k->max_idx) k->max_idx = ix[i];
        }
        que = "subir vertices";
        for (uint32_t o = 0; o < b.n && r == G3D_OK; o += GPU3D_VBUF_TROZO) {
            uint32_t m = b.n - o < GPU3D_VBUF_TROZO ? b.n - o : GPU3D_VBUF_TROZO;
            if (o) gpu3d_respirar();   /* D436: una subida grande no deja sin sonido */
            r = drv->vbuf((int)b.vaga, k->bytes, b.desloc + o, (const uint8_t *)b.dados + o, m);
        }
        if (r == G3D_OK) {
            if (b.desloc + b.n > k->cheio) k->cheio = b.desloc + b.n;
            n_vb_subidas++; vb_kb_subidos += (b.n + 1023) >> 10;
        }
        break;
    }
    case GPU3D_OP_TRANSF: {
        if (!usuario_ok((void *)p.ptr, sizeof(gpu3d_transf_t), false)) return ERRO_ENDERECO;
        if (!(capas.bits3 & G3D_C3_V3)) return GPU3D_SEM_SUPORTE;
        transf_valida = false;
        transf_atual = *(const gpu3d_transf_t *)p.ptr;
        bool falta;
        if (!transf_ok(&transf_atual, &falta)) return ERRO_INVALIDO;
        if (falta) { n_recusas++; return GPU3D_SEM_SUPORTE; }
        transf_valida = true;
        return 0;
    }
    case GPU3D_OP_LUZES: {
        if (!usuario_ok((void *)p.ptr, sizeof(gpu3d_luzes_t), false)) return ERRO_ENDERECO;
        if (!(capas.bits3 & G3D_C3_LUZ)) return GPU3D_SEM_SUPORTE;
        luzes_validas = false;
        luzes_atuais = *(const gpu3d_luzes_t *)p.ptr;
        const gpu3d_luzes_t *l = &luzes_atuais;
        if (l->versao != 3 || (l->flags & ~G3D_LUZ_COR_MATERIAL) || (l->luzes & ~0xFFu) ||
            !floats_ok(l->ambiente_global, 21) || !floats_ok((const uint32_t *)l->luz, 8 * 24)) return ERRO_INVALIDO;
        int nl = 0;
        for (int i = 0; i < 8; i++) if (l->luzes & (1u << i)) nl++;
        if ((uint32_t)nl > capas.max_luzes) { n_recusas++; return GPU3D_SEM_SUPORTE; }
        luzes_validas = true;
        return 0;
    }
    case GPU3D_OP_DESENHAR3: {
        if (!usuario_ok((void *)p.ptr, sizeof(gpu3d_desenho3_t), false)) return ERRO_ENDERECO;
        g3d_desenho3_t g;
        g.d = *(const gpu3d_desenho3_t *)p.ptr;
        if (!(capas.bits3 & G3D_C3_V3) || !est2_valido || !transf_valida) return GPU3D_SEM_SUPORTE;
        if (g.d.modo > G3D_PRIM_ABANICO || g.d.n > GPU3D_MAX_VERT3 || g.d.primeiro > 0x00FFFFFFu ||
            (g.d.modo == G3D_PRIM_TRIANGULOS && g.d.n % 3)) return ERRO_INVALIDO;
        if (g.d.modo != G3D_PRIM_TRIANGULOS && !(capas.bits3 & G3D_C3_TIRAS)) return GPU3D_SEM_SUPORTE;
        if (g.d.n < 3) return 0;
        if ((transf_atual.flags & G3D3_LUZ) && !luzes_validas) return ERRO_INVALIDO;
        if (!atrib_ok(&g.d.pos, 1u << G3D_AT_FLOAT2 | 1u << G3D_AT_FLOAT3 | 1u << G3D_AT_FLOAT4) ||
            !atrib_ok(&g.d.cor_a, 1u << G3D_AT_NADA | 1u << G3D_AT_UBYTE4N | 1u << G3D_AT_FLOAT4) ||
            !atrib_ok(&g.d.normal_a, 1u << G3D_AT_NADA | 1u << G3D_AT_FLOAT3)) return ERRO_INVALIDO;
        for (int u = 0; u < 4; u++)
            if (!atrib_ok(&g.d.tc[u], 1u << G3D_AT_NADA | 1u << G3D_AT_FLOAT2 | 1u << G3D_AT_FLOAT3 | 1u << G3D_AT_FLOAT4)) return ERRO_INVALIDO;
        if (g.d.cor_a.formato == G3D_AT_FLOAT4 && !(capas.bits3 & G3D_C3_COR_FLOAT)) return GPU3D_SEM_SUPORTE;
        vb_seq++;
        g.bytes_indices = 0;
        if (g.d.indices != GPU3D_SEM_INDICES) {
            if (g.d.indices >= GPU3D_VBUFS) return ERRO_INVALIDO;
            vb_t *k = &vb[g.d.indices];
            if (!k->presente) return GPU3D_EXPULSO;
            if (k->uso != G3D_VB_INDICES16 || (uint64_t)(g.d.primeiro + g.d.n) * 2 > k->cheio) return ERRO_INVALIDO;
            k->seq = vb_seq;
            g.max_vert = k->max_idx; g.bytes_indices = k->cheio;
        } else g.max_vert = g.d.primeiro + g.d.n - 1;
        int32_t c = atrib_cabe(&g.d.pos, g.max_vert);
        if (!c) c = atrib_cabe(&g.d.cor_a, g.max_vert);
        if (!c && (transf_atual.flags & G3D3_LUZ)) c = atrib_cabe(&g.d.normal_a, g.max_vert);
        for (int u = 0; u < est2_atual.unidades && !c; u++) c = atrib_cabe(&g.d.tc[u], g.max_vert);
        if (c) return c;
        if ((cab & GPU3D_CAB_P_TEMPO) && ++n_desenhos_prova > 2) { falha("tiempo agotado esperando a la GPU (simulado)"); return ERRO_TEMPO; }
        g.e = &est2_atual; g.t = &transf_atual; g.luz = transf_atual.flags & G3D3_LUZ ? &luzes_atuais : NULL;
        r = drv->desenhar3(&g, NULL); que = "dibujar desde los buferes (v3)";
        if (r == G3D_OK) { n_tri += g.d.modo == G3D_PRIM_TRIANGULOS ? g.d.n / 3 : g.d.n - 2; n_lotes++; n_des3++; n_vert3 += g.d.n; }
        break;
    }
    /* ---- D480: gpu3d v4 (sombreadores programables) ---- */
    case GPU3D_OP_V4_CAPS: {
        if (!drv->v4 || !drv->v4->caps) return GPU3D_SEM_SUPORTE;
        if (!usuario_ok((void *)p.ptr, sizeof(gpu3d_v4_caps_t), true)) return ERRO_ENDERECO;
        gpu3d_v4_caps_t c;
        memset(&c, 0, sizeof c);
        c.versao = 4; c.max_lado = GPU3D_MAX_LADO; c.max_recursos = G3D4_RECURSOS; c.max_programas = G3D4_PROGRAMAS; c.max_bytes = G3D4_BYTES_MAX;
        r = drv->v4->caps(&c); que = "preguntar las capacidades v4";
        if (r == G3D_OK) { c.bits |= G3D4_C_V4; *(gpu3d_v4_caps_t *)p.ptr = c; }
        break;
    }
    case GPU3D_OP_V4_INFORME:
        klog("[GPU3D] autoprueba v4 (libsgl, contra la CPU): %s, %u pixeles distintos, caras de frente %s\n", p.a == 1 ? "BIEN" : "MAL", p.b, p.c ? "invertidas por la GPU (se compensa)" : "como en ES 2.0");
        return 0;
    case GPU3D_OP_V4_PROGRAMA: {
        if (!drv->v4 || !drv->v4->programa) return GPU3D_SEM_SUPORTE;
        if (p.a >= G3D4_PROGRAMAS) return ERRO_INVALIDO;
        if (!p.ptr) { r = drv->v4->programa(p.a, NULL, 0, NULL, 0); que = "soltar un programa v4"; break; }
        if (!usuario_ok((void *)p.ptr, sizeof(gpu3d_v4_prog_t), false)) return ERRO_ENDERECO;
        gpu3d_v4_prog_t g = *(const gpu3d_v4_prog_t *)p.ptr;
        if (g.nvs < 8 || g.nfs < 8 || g.nvs > G3D4_TGSI_MAX || g.nfs > G3D4_TGSI_MAX) return GPU3D_SEM_SUPORTE;
        if (!usuario_ok((void *)g.vs, g.nvs, false) || !usuario_ok((void *)g.fs, g.nfs, false)) return ERRO_ENDERECO;
        if (((const char *)g.vs)[g.nvs - 1] || ((const char *)g.fs)[g.nfs - 1]) return ERRO_INVALIDO;
        r = drv->v4->programa(p.a, (const char *)g.vs, g.nvs, (const char *)g.fs, g.nfs); que = "crear un programa v4";
        if (r == G3D_OK) n_v4_prog++;
        break;
    }
    case GPU3D_OP_V4_RECURSO: {
        if (!drv->v4 || !drv->v4->recurso) return GPU3D_SEM_SUPORTE;
        if (!usuario_ok((void *)p.ptr, sizeof(gpu3d_v4_rec_t), false)) return ERRO_ENDERECO;
        gpu3d_v4_rec_t g = *(const gpu3d_v4_rec_t *)p.ptr;
        if (g.id == 0 || g.id >= G3D4_RECURSOS || g.tipo < G3D4_R_TEX2D || g.tipo > G3D4_R_ZE) return ERRO_INVALIDO;
        if (g.larg) {
            if (g.larg > GPU3D_MAX_LADO || g.alt < 1 || g.alt > GPU3D_MAX_LADO || g.niveis < 1 || g.niveis > G3D4_NIVEIS) return GPU3D_SEM_SUPORTE;
            if (g.tipo == G3D4_R_ZE && ((g.zbits != 16 && g.zbits != 24) || g.estencil > 1)) return ERRO_INVALIDO;
            if (g.tipo == G3D4_R_CUBO && g.larg != g.alt) return GPU3D_SEM_SUPORTE;
        }
        r = drv->v4->recurso(&g); que = "crear un recurso v4";
        if (r == G3D_OK) n_v4_rec++;
        break;
    }
    case GPU3D_OP_V4_SUBIR: {
        if (!drv->v4 || !drv->v4->subir) return GPU3D_SEM_SUPORTE;
        if (!usuario_ok((void *)p.ptr, sizeof(gpu3d_v4_subir_t), false)) return ERRO_ENDERECO;
        gpu3d_v4_subir_t g = *(const gpu3d_v4_subir_t *)p.ptr;
        if (g.id == 0 || g.id >= G3D4_RECURSOS || g.nivel >= G3D4_NIVEIS || g.face > 5 || !g.larg || !g.alt || g.larg > GPU3D_MAX_LADO ||
            g.alt > GPU3D_MAX_LADO || g.x > GPU3D_MAX_LADO || g.y > GPU3D_MAX_LADO || g.passo < g.larg * 4) return ERRO_INVALIDO;
        if (!usuario_ok((void *)g.dados, (g.alt - 1) * g.passo + g.larg * 4, false)) return ERRO_ENDERECO;
        r = drv->v4->subir(&g); que = "subir pixeles v4";
        if (r == G3D_OK) n_v4_subir++;
        break;
    }
    case GPU3D_OP_V4_LIMPAR: {
        if (!drv->v4 || !drv->v4->limpar) return GPU3D_SEM_SUPORTE;
        if (!usuario_ok((void *)p.ptr, sizeof(gpu3d_v4_limpar_t), false)) return ERRO_ENDERECO;
        gpu3d_v4_limpar_t g = *(const gpu3d_v4_limpar_t *)p.ptr;
        if (g.alvo_cor >= G3D4_RECURSOS || g.alvo_ze >= G3D4_RECURSOS || (g.mascara & ~7u)) return ERRO_INVALIDO;
        r = drv->v4->limpar(&g); que = "limpiar v4";
        if (r == G3D_OK) n_v4_limpar++;
        break;
    }
    case GPU3D_OP_V4_LER: {
        if (!drv->v4 || !drv->v4->ler) return GPU3D_SEM_SUPORTE;
        if (!usuario_ok((void *)p.ptr, sizeof(gpu3d_v4_ler_t), false)) return ERRO_ENDERECO;
        gpu3d_v4_ler_t g = *(const gpu3d_v4_ler_t *)p.ptr;
        if (g.rec == 0 || g.rec >= G3D4_RECURSOS || !g.larg || !g.alt || g.larg > GPU3D_MAX_LADO || g.alt > GPU3D_MAX_LADO ||
            g.x > GPU3D_MAX_LADO || g.y > GPU3D_MAX_LADO) return ERRO_INVALIDO;
        /* cada fila dentro de la memoria del programa (el paso puede ser negativo) */
        for (uint32_t j = 0; j < g.alt; j += (j + 1 < g.alt ? g.alt - 1 : 1))
            if (!usuario_ok((void *)(g.dest + (uint32_t)((int32_t)j * g.passo)), g.larg * 4, true)) return ERRO_ENDERECO;
        r = drv->v4->ler(&g); que = "leer un recurso v4";
        if (r == G3D_OK) n_v4_ler++;
        break;
    }
    case GPU3D_OP_V4_DESENHAR: {
        if (!drv->v4 || !drv->v4->desenhar) return GPU3D_SEM_SUPORTE;
        if (!usuario_ok((void *)p.ptr, sizeof(gpu3d_v4_des_t), false)) return ERRO_ENDERECO;
        const gpu3d_v4_des_t *g = (const gpu3d_v4_des_t *)p.ptr;
        if (g->programa >= G3D4_PROGRAMAS || g->modo > G3D4_PRIM_LEQUE || g->natrib > G3D4_ATRIBS || g->alvo_cor == 0 ||
            g->alvo_cor >= G3D4_RECURSOS || g->alvo_ze >= G3D4_RECURSOS || g->namostras[0] > G3D4_AMOSTRAS || g->namostras[1] > G3D4_AMOSTRAS ||
            g->nunif[0] > G3D4_UNIF_MAX || g->nunif[1] > G3D4_UNIF_MAX || (g->tipo_ind != 0 && g->tipo_ind != 2 && g->tipo_ind != 4))
            return ERRO_INVALIDO;
        if (!g->n) return 0;
        if (g->n > GPU3D_MAX_VERT3 * 4 || g->nverts > GPU3D_MAX_VERT3 * 4 || !g->nverts) return GPU3D_SEM_SUPORTE;
        uint64_t bytes = (uint64_t)g->n * g->tipo_ind;
        for (unsigned i = 0; i < g->natrib; i++) {
            const gpu3d_v4_atrib_t *a = &g->at[i];
            if (!a->ativo) continue;
            if (a->comps < 1 || a->comps > 4 || a->passo < a->comps * 4 || (a->passo & 3) || (a->dados & 3)) return ERRO_INVALIDO;
            uint64_t t = (uint64_t)(g->nverts - 1) * a->passo + a->comps * 4;
            if (t > G3D4_BYTES_MAX) return GPU3D_SEM_SUPORTE;
            if (!usuario_ok((void *)a->dados, (uint32_t)t, false)) return ERRO_ENDERECO;
            bytes += g->nverts * a->comps * 4u;
        }
        if (bytes > G3D4_BYTES_MAX) return GPU3D_SEM_SUPORTE;
        if (g->tipo_ind && (!usuario_ok((void *)g->indices, g->n * g->tipo_ind, false) || (g->indices & 3))) return ERRO_ENDERECO;
        for (int e = 0; e < 2; e++)
            if (g->nunif[e] && !usuario_ok((void *)g->unif[e], g->nunif[e] * 16, false)) return ERRO_ENDERECO;
        r = drv->v4->desenhar(g); que = "dibujar v4";
        if (r == G3D_OK) { n_v4_des++; n_tri += g->n / 3; n_lotes++; }
        break;
    }
    default:
        return ERRO_INVALIDO;
    }
    if (r == G3D_FALHA) {
        char m[80];
        snformatar(m, sizeof m, "la GPU no respondio a tiempo al %s", que);
        falha(m);
        return ERRO_TEMPO;
    }
    if (r != G3D_OK) { n_recusas++; return GPU3D_SEM_SUPORTE; }
    return 0;
}

int32_t gpu3d_info(uint32_t ptr, uint32_t n)
{
    info_gpu3d_t *v = (info_gpu3d_t *)ptr;
    if (n < sizeof *v || !usuario_ok(v, sizeof *v, true)) return ERRO_ENDERECO;
    memset(v, 0, sizeof *v);
    v->versao = 1;
    v->estado = estado;
    v->fabricante = disp.fabricante; v->modelo = disp.modelo;
    txt_copia_n(v->nome, drv ? drv->nome : "software", sizeof v->nome);
    txt_copia_n(v->placa, placa_nome, sizeof v->placa);
    txt_copia_n(v->motivo, motivo, sizeof v->motivo);
    v->dono = (uint32_t)(dono_vivo() ? dono : 0);
    v->larg = (uint32_t)alvo_l; v->alt = (uint32_t)alvo_a; v->zbits = (uint32_t)alvo_z;
    v->triangulos = n_tri; v->lotes = n_lotes; v->quadros = n_quadros; v->falhas = n_falhas; v->recusas = n_recusas;
    v->us_autoprova = us_autoprova;
    if (drv && drv->memoria && estado == GPU3D_ATIVO) drv->memoria(&v->memoria_kb, &v->livre_kb);
    v->variante = variante_real(&v->reservado[0]);   /* D358: bits GPU3D_VAR_*; reservado[0] = KB de RAM en el GTT */
    v->prazo_ms = GPU3D_PRAZO_MS;
    v->pontos = pontos; v->tri_s = tri_s; v->kpix_s = kpix_s;
    if (estado == GPU3D_ATIVO) { v->reservado[1] = capas.bits; v->reservado[2] = capas.unidades; }   /* D425 */
    /* D432: presentaciones directas, las que no se pudieron y la ventana que tiene la pantalla (+1; 0 = nadie). */
    v->reservado[3] = n_apres; v->reservado[4] = n_apres_nao;
    v->reservado[5] = (uint32_t)(gui_exclusiva_estado(NULL) + 1);
    return 0;
}

/* D399 (gpu_energia.c): lotes del 3D desde el arranque (gobernador «auto»), la memoria de la GPU que usa
 * y la comprobación después de cambiar los relojes. 0: no hay 3D encendido; 1: responde; -1: no (el 3D
 * no se apaga aquí: quien llama vuelve antes a los relojes de la BIOS y repite). Con un programa dueño
 * del motor no se toca su destino: solo se mira que el motor conteste (una valla en las R5xx). */
uint32_t gpu3d_lotes_total(void) { return n_lotes + n_quadros; }
void gpu3d_memoria_uso(uint32_t *total_kb, uint32_t *livre_kb)
{
    *total_kb = *livre_kb = 0;
    if (drv && drv->memoria && estado == GPU3D_ATIVO) drv->memoria(total_kb, livre_kb);
}
int gpu3d_r500_ocioso(uint32_t ms);
int gpu3d_energia_comprobar(char *m, int nm)
{
    if (estado != GPU3D_ATIVO || !drv) return 0;
    if (dono_vivo()) {
        if (drv == &gpu3d_r500) {
            if (gpu3d_r500_ocioso(GPU3D_PRAZO_MS) > 0) return 1;
            snformatar(m, (size_t)nm, "el CP del 3D no contesta tras el cambio");
            return -1;
        }
        return 1;   /* otros drivers: sin comprobar con el motor en uso */
    }
    info_gpu3d_prova_t r;
    memset(&r, 0, sizeof r);
    if (autoprova(&r, m, nm)) { alvo_l = alvo_a = 0; return 1; }
    return -1;
}

/* D390: la imagen de una etapa de la última «gpu3d probar --ver». */
static int32_t etapa_info(info_gpu3d_etapa_t *v)
{
    int i = v->etapa;
    memset(v, 0, sizeof *v);
    v->versao = 1; v->etapa = i; v->larg = GPU3D_VER_L; v->alt = GPU3D_VER_A; v->x = v->y = -1;
    if (i < 0 || i >= GPU3D_N_PROVAS) return ERRO_INVALIDO;
    if (!etapas) return 0;   /* GPU3D_ETAPA_NADA */
    const etapa_vista_t *e = &etapas[i];
    v->estado = e->estado;
    v->com_z = e->an.tem_z;
    v->cor_intacta = e->an.cor_intacta; v->z_intacto = e->an.z_intacto; v->z_maximo = e->an.z_maximo;
    for (int k = 0; k < 4; k++) { v->cores[k] = e->an.cores[k]; v->n_cores[k] = e->an.n_cores[k]; }
    v->resto = e->an.resto;
    v->x = e->x; v->y = e->y; v->esperado = e->esperado; v->obtido = e->obtido;
    if (e->com_imagem) {
        memcpy(v->px, e->px, sizeof v->px);
        memcpy(v->z, e->z, sizeof v->z);
    }
    return 0;
}

/* «gpu3d probar»: la autoprueba y la medida, ahora. Si el motor es de un
 * programa, no se le quita. D390: con GPU3D_PROVA_DETALLE o GPU3D_PROVA_VER, sentinelas y
 * análisis del destino en cada etapa; con GPU3D_PROVA_VER, además, sus imágenes. */
int32_t gpu3d_prova(uint32_t ptr, uint32_t n)
{
    if (n >= sizeof(info_gpu3d_etapa_t) && usuario_ok((void *)ptr, sizeof(info_gpu3d_etapa_t), true) &&
        ((info_gpu3d_etapa_t *)ptr)->versao == GPU3D_ETAPA_VERSAO)
        return etapa_info((info_gpu3d_etapa_t *)ptr);
    info_gpu3d_prova_t *r = (info_gpu3d_prova_t *)ptr;
    if (n < sizeof *r || !usuario_ok(r, sizeof *r, true)) return ERRO_ENDERECO;
    uint32_t modo = r->versao & (GPU3D_PROVA_DETALLE | GPU3D_PROVA_VER);
    bool com_detalhe = (modo & GPU3D_PROVA_DETALLE) != 0;
    memset(r, 0, sizeof *r);
    r->versao = 1; r->falhou = -1;
    if (!drv || estado == GPU3D_DESATIVADO || estado == GPU3D_SEM_GPU) {
        r->resultado = -1;
        snformatar(r->texto, sizeof r->texto, "sin 3D por GPU: %s", motivo);
        return 0;
    }
    if (dono_vivo() || op_hilo_vivo()) {
        r->resultado = -1;
        snformatar(r->texto, sizeof r->texto, "el motor 3D lo esta usando el proceso %d: cierre ese programa y repita", dono);
        return 0;
    }
    /* Las imágenes de la prueba anterior se tiran: las nuevas, si se piden. */
    if (etapas) { kfree(etapas); etapas = NULL; }
    if (modo & GPU3D_PROVA_VER) {
        etapas = kmalloc(sizeof *etapas * GPU3D_N_PROVAS);
        if (etapas) memset(etapas, 0, sizeof *etapas * GPU3D_N_PROVAS);
        else klog("[GPU3D] prueba: sin memoria para las imagenes de --ver; sigue sin ellas\n");
    }
    diag = modo;
    bool ok;
    char m[120] = "";
    if (estado != GPU3D_ATIVO) {
        if (com_detalhe && drv->detalle) drv->detalle("antes de encender");
        ok = ligar(r);
        if (!ok) txt_copia_n(m, motivo, sizeof m);
    } else {
        ok = autoprova(r, m, sizeof m);
        if (!ok) { char mm[140]; snformatar(mm, sizeof mm, "autoprueba fallida: %s", m); falha(mm); }
        if (ok && bits3_driver && !autoprova3()) { ok = false; snformatar(m, sizeof m, "la GPU no respondio en la autoprueba de gpu3d v3"); falha(m); }
    }
    diag = 0;
    if (ok && !medir(r)) { ok = false; snformatar(m, sizeof m, "la medida no termino"); falha("la GPU no termino la medida"); }
    if (ok && com_detalhe && drv->detalle) drv->detalle("tras la prueba");
    alvo_l = alvo_a = 0;
    r->resultado = ok ? 1 : 0;
    r->reservado[0] = (uint32_t)gpu3d_ajustes.n;       /* D390: ajustes del fichero del driver en este encendido */
    r->reservado[1] = (uint32_t)gpu3d_ajustes.malas;
    r->reservado[2] = etapas ? 1u : 0u;                /* hay imágenes de las etapas (--ver) */
    if (ok)
        snformatar(r->texto, sizeof r->texto, "%s: las %d pruebas dan los pixeles esperados; %u triangulos/s, %u kpix/s, %u puntos",
                   drv->nome, GPU3D_N_PROVAS, r->tri_s, r->kpix_s, r->pontos);
    else
        snformatar(r->texto, sizeof r->texto, "%s: FALLO: %s", drv->nome, m);
    klog("[GPU3D] prueba: %s\n", r->texto);
    return 0;
}

int32_t gpu3d_ctl(uint32_t op)
{
    if (!drv || estado == GPU3D_DESATIVADO || estado == GPU3D_SEM_GPU) return ERRO_INVALIDO;
    if (op_hilo_vivo()) return ERRO_OCUPADO;   /* D436: un programa está dentro de una orden (cediendo la CPU) */
    if (op == 0) {
        if (estado == GPU3D_ATIVO) soltar_driver();
        dono = 0;
        estado = GPU3D_APAGADO;
        snformatar(motivo, sizeof motivo, "%s: apagado a mano (gpu3d apagar), SaguiGL por software", placa_nome);
        klog("[GPU3D] apagado: a mano (gpu3d apagar)\n");
        return 0;
    }
    /* 1 lotes, 2 anillo; D358: 3 lotes y 4 anillo, con la RAM del sistema en el GTT; D406: 5 lotes y 6 anillo
     * sin la GART que las R7xx confirmadas encienden solas. */
    if (op < 1 || op > 6) return ERRO_INVALIDO;
    if (dono_vivo()) return ERRO_OCUPADO;
    if (estado == GPU3D_ATIVO) soltar_driver();
    variante = (op == 2 || op == 4 || op == 6 ? GPU3D_CAB_ANEL : 0) | (op == 3 || op == 4 ? GPU3D_CAB_GTT : 0) |
              (op >= 5 ? GPU3D_CAB_SEM_GTT : 0);
    estado = GPU3D_SIN_PROBAR;
    return ligar(NULL) ? 0 : ERRO_INVALIDO;
}
