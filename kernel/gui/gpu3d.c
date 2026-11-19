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
#include "console.h"
#include "cpu.h"
#include "info_hardware.h"
#include "memoria.h"
#include "processo.h"
#include "sensores_conv.h"
#include "sistema.h"
#include "texto.h"
#include "vfs.h"

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
static uint32_t n_tri, n_lotes, n_quadros, n_falhas, n_recusas, us_autoprova, n_desenhos_prova;
static uint32_t pontos, tri_s, kpix_s;
static bool desligando;

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
    if (drv && drv->desligar) drv->desligar();
    desligando = false;
    dono = 0; alvo_l = alvo_a = 0;
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
    } else {
        const acel_driver_t *d2 = NULL;
        bool suspeito = false;
        if (acel2d_placa(&disp, &geracao, &d2, &tela, &suspeito) && d2 == &acel_intel && geracao == 3) {
            drv = &gpu3d_intel;
            snformatar(placa_nome, sizeof placa_nome, "Intel Gen3 %04x:%04x", disp.fabricante, disp.modelo);
            estado = GPU3D_SIN_PROBAR;
            snformatar(motivo, sizeof motivo, "%s: 3D sin probar todavia (se prueba con el primer uso o con «gpu3d probar»)", placa_nome);
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
    if (drv == &gpu3d_r600) {
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
static bool vertices_ok(const gpu3d_vert_t *v, int n)
{
    for (int i = 0; i < n; i++) {
        const uint32_t c[6] = { v[i].x, v[i].y, v[i].z, v[i].rhw, v[i].u, v[i].v };
        for (int k = 0; k < 6; k++) if (((c[k] >> 23) & 0xFF) == 0xFF) return false;
        int32_t x = g3d_fixo_de_float(v[i].x, 4), y = g3d_fixo_de_float(v[i].y, 4), z = g3d_fixo_de_float(v[i].z, 16);
        if (x < -32 || x > (alvo_l + 2) * 16 || y < -32 || y > (alvo_a + 2) * 16 || z < 0 || z > 65536) return false;
        if ((v[i].rhw & 0x80000000u) || ((v[i].rhw >> 23) & 0xFF) > 127 + 24) return false;
        if (((v[i].u >> 23) & 0xFF) > 127 + 7 || ((v[i].v >> 23) & 0xFF) > 127 + 7) return false;
    }
    return true;
}

int32_t gpu3d_op(uint32_t ptr, uint32_t n)
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
        return 0;
    }
    if (estado != GPU3D_ATIVO || dono != pid || !alvo_l) return ERRO_ACESSO;
    int r = G3D_OK;
    const char *que = "";
    switch (p.op) {
    case GPU3D_OP_FECHAR:
        for (int i = 0; i < GPU3D_TEXTURAS; i++) drv->textura(i, 0, 0, NULL);
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
        r = drv->textura((int)p.a, (int)p.b, (int)p.c, (const uint32_t *)p.ptr); que = "cargar una textura";
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
        if (p.a < (uint32_t)alvo_l || p.a > 8192) return ERRO_INVALIDO;
        if (!usuario_ok((void *)p.ptr, ((uint32_t)(alvo_a - 1) * p.a + (uint32_t)alvo_l) * 4, true)) return ERRO_ENDERECO;
        r = drv->ler((uint32_t *)p.ptr, (int)p.a); que = "leer el cuadro";
        if (r == G3D_OK) n_quadros++;
        break;
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
    if (dono_vivo()) {
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
