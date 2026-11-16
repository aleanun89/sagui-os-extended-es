// SPDX-License-Identifier: GPL-2.0-or-later
/* acel_intel.c - el blitter de las GPU de Intel, de Gen3 a Gen8 (D195, D359).
 *
 * SIN PROBAR EN HARDWARE: QEMU no emula ninguna GPU de Intel. Por eso la capa
 * común solo lo activa si su autoprueba del arranque da los mismos píxeles que
 * la CPU y termina a tiempo; si no, apaga el ring y se queda la CPU.
 *
 * Las órdenes son las del motor 2D («BLT») de Intel, iguales de Gen3 a Gen6:
 *   XY_COLOR_BLT     (0x50): rellena un rectángulo con un color (ROP 0xF0, PATCOPY);
 *   XY_SRC_COPY_BLT  (0x53): copia un rectángulo (ROP 0xCC, SRCCOPY);
 * con 32 bits por píxel (BR13 bits 25:24 = 11b) y escribiendo los cuatro bytes.
 * Se mandan por un ring buffer: una zona de la memoria de vídeo donde la CPU
 * escribe las órdenes y avanza TAIL; la GPU las ejecuta y avanza HEAD.
 *   - Gen3 a Gen5 (945GM del Amilo, G33, GM45, Ironlake): el ring principal
 *     (RCS, registros 0x2030-0x203C), que también ejecuta las órdenes 2D;
 *   - Gen6 (HD 2000/3000 del G470): el ring propio del blitter (BCS,
 *     0x22030-0x2203C), y antes hay que despertar la GPU (FORCEWAKE), porque
 *     sus registros no responden mientras duerme.
 *   - D359: Gen7 (Ivy Bridge, HD 4000 del L850), Gen7.5 (Haswell) y Gen8
 *     (Broadwell): el mismo anillo BCS en modo heredado (sin execlists), con
 *     el forcewake «multihilo» (FORCEWAKE_MT) y, en Gen8, órdenes con
 *     direcciones de 64 bits. Antes de escribir nada se comprueba el modelo
 *     exacto, que un plano de la pantalla muestre el framebuffer en lineal,
 *     32 bits y el mismo pitch, y que el GTT mapee la pantalla, la zona de
 *     prueba y el anillo en páginas seguidas fuera de la zona reservada del
 *     hardware. Gen9 y siguientes necesitan execlists: no entran (D359).
 *
 * Memoria: el VBE dejó la pantalla en la memoria robada a la RAM, vista por la
 * CPU en la apertura (BAR 2, GMADR) y por la GPU en el GTT con la misma
 * posición: la dirección gráfica de la pantalla es vesa_fb - GMADR. El ring va
 * al final de la memoria que anunció el VBE (ya está en el GTT), lejos de la
 * pantalla y de la zona de prueba.
 *
 * Copias solapadas: no se confía en que el blitter elija la dirección: una
 * copia que se pisa se parte en franjas que no se solapan (de alto o de ancho
 * igual al desplazamiento), en el orden que no pisa lo que falta por copiar,
 * con un vaciado (MI_FLUSH) entre franjas.
 *
 * Qué hay que comprobar en el hardware: docs/decisoes.md, D195. */
#include "acel2d.h"
#include "acel_intel.h"
#include "acel_intel_logica.h"
#include "console.h"
#include "paginacao.h"
#include "texto.h"

#define RING_BYTES     (64 * 1024)
#define MI_NOOP        INTEL_MI_NOOP
/* Las órdenes (XY_COLOR_BLT, XY_SRC_COPY_BLT, MI_FLUSH/MI_FLUSH_DW) las arma
 * acel_intel_logica.h según la generación (probadas en el anfitrión, h361). */

/* Registros del ring (desplazamientos desde la base de cada ring). */
#define RCS_BASE       0x02000
#define BCS_BASE       0x22000
#define R_TAIL         0x30
#define R_HEAD         0x34
#define R_START        0x38
#define R_CTL          0x3C
#define R_MI_MODE      0x9C          /* bit 8: STOP_RING (Gen4 en adelante); solo para el log */
#define CTL_VALIDO     1u
#define HEAD_MASCARA   0x001FFFFCu
#define FORCEWAKE      0xA18C
#define FORCEWAKE_ACK  0x130090
/* D359: Gen7+ */
#define FORCEWAKE_MT        0xA188      /* registro con máscara: bits 31:16 dicen qué bits 15:0 cambian */
#define FORCEWAKE_MT_ACK    0x130040    /* Ivy Bridge */
#define FORCEWAKE_ACK_HSW   0x130044    /* Haswell y Broadwell */
#define ECOBUS              0xA180      /* bit 5: el firmware activó el forcewake multihilo (Ivy Bridge) */
#define ECOBUS_MT           (1u << 5)
#define GT_THREAD_STATUS    0x13805C    /* bits 2:0: hilos del GT aún despertando */
#define MASCARA_ON(b)       (((uint32_t)(b) << 16) | (uint32_t)(b))
#define MASCARA_OFF(b)      ((uint32_t)(b) << 16)
#define R_GFX_MODE          0x29C       /* RING_MODE_GEN7 */
#define GFX_RUN_LIST        (1u << 15)  /* execlists */
#define GFX_PPGTT           (1u << 9)
#define STOP_RING           (1u << 8)   /* en MI_MODE (con máscara) */
#define R_IPEIR             0x64
#define R_IPEHR             0x68
#define R_INSTDONE          0x6C
#define R_ACTHD             0x74
#define LIMITE_APERTURA     (128u << 20) /* lo mínimo que mide la apertura (BAR 2) en Gen7-8 */
/* La página de estado (HWS) del BCS: i915 la programa siempre antes de arrancar el anillo
 * (intel_ring_setup_status_page): Gen7 en BLT_HWS_PGA_GEN7, Gen8 en RING_HWS_PGA del anillo.
 * HWSTAM a ~0: el hardware no copia ahí el estado de las interrupciones. En Gen6-7, además,
 * se invalida la TLB de la GPU (INSTPM). */
#define BLT_HWS_PGA_GEN7    0x4280
#define R_HWS_PGA           0x80
#define R_HWSTAM            0x98
#define R_INSTPM            0xC0
#define INSTPM_TLB_INV      (1u << 9)
#define INSTPM_SYNC_FLUSH   (1u << 5)
#define HWS_BYTES           4096

static volatile uint8_t *mmio;
static uint32_t mmio_fis;
static volatile uint32_t *ring;
static uint32_t ring_fis;
static uint32_t ring_gfx, fb_gfx, pitch, tail, base;
static int gen;
static bool ligado, acordada;
/* D359: Gen7+: la variante, el forcewake que se usa (registro, acuse y valores) y el GTT. */
static int var;
static uint32_t fw_reg, fw_ack, fw_on, fw_off;
static volatile uint32_t *gtt;       /* las entradas del GTT de la pantalla, la zona de prueba y el anillo */
static uint32_t gtt_fis, gtt_tam;
static int plano_tela = -1;          /* el plano (pipe) que muestra la pantalla */
/* D285: el 3D de Gen3 comparte este ring. Zona libre de la memoria del VBE (entre
 * la zona de prueba del 2D y el ring) y si hay una carga 3D pendiente: mientras
 * la haya, las esperas del 2D usan el plazo largo del 3D (un lote de triángulos
 * puede tardar más que los 30 ms de una copia). */
static uint32_t arena_off, arena_bytes, tela_fis;
static bool carga_3d;
#define PRAZO_3D_MS 2000

static inline uint32_t rd(uint32_t r)              { return *(volatile uint32_t *)(mmio + r); }
static inline void     wr(uint32_t r, uint32_t v)  { *(volatile uint32_t *)(mmio + r) = v; }

/* Espacio libre en el ring (bytes), con HEAD leído del registro. */
static uint32_t ring_livre(void)
{
    uint32_t head = rd(base + R_HEAD) & HEAD_MASCARA;
    return (head - tail - 8) & (RING_BYTES - 1);
}

static void avancar(void);

static bool reservar(uint32_t palavras)
{
    if (ring_livre() >= palavras * 4 + 16) return true;
    avancar();   /* lo que ya está escrito tiene que salir para que HEAD avance */
    acel_prazo_t p;
    if (carga_3d) acel_prazo_longo(&p, PRAZO_3D_MS); else acel_prazo(&p, 250);
    while (ring_livre() < palavras * 4 + 16)
        if (acel_prazo_esgotado(&p)) return false;
    return true;
}

static void emitir(uint32_t v)
{
    ring[tail / 4] = v;
    tail = (tail + 4) & (RING_BYTES - 1);
}

/* Alinea a 8 bytes y avisa a la GPU. */
static void avancar(void)
{
    if (tail & 7) emitir(MI_NOOP);
    __asm__ volatile("lock; orl $0, (%%esp)" ::: "memory");
    /* D359: lo escrito por la apertura tiene que llegar a la memoria antes de que la GPU lo
     * lea: una lectura de MMIO hace de barrera (lo que hace i915, intel_gt_flush_ggtt_writes). */
    if (gen >= 7) (void)rd(base + R_HEAD);
    wr(base + R_TAIL, tail);
}

static void emitir_n(const uint32_t *b, int n)
{
    for (int i = 0; i < n; i++) emitir(b[i]);
}

static void emitir_flush(void)
{
    uint32_t b[4];
    emitir_n(b, intel_cmd_flush(b, gen));
}

static bool in_esperar(void)
{
    acel_prazo_t p;
    if (carga_3d) acel_prazo_longo(&p, PRAZO_3D_MS); else acel_prazo(&p, 250);
    while ((rd(base + R_HEAD) & HEAD_MASCARA) != tail)
        if (acel_prazo_esgotado(&p)) {
            klog("[ACEL2D] intel: tiempo agotado%s: HEAD %x TAIL %x CTL %x\n", carga_3d ? " (con carga 3D)" : "",
                 rd(base + R_HEAD), tail, rd(base + R_CTL));
            return false;
        }
    carga_3d = false;
    return true;
}

static bool blit_copia(int ox, int oy, int dx, int dy, int l, int a)
{
    uint32_t b[INTEL_PAL_MAX];
    if (!reservar(INTEL_PAL_MAX + 6)) return false;
    emitir_n(b, intel_cmd_copia(b, gen, pitch, fb_gfx, ox, oy, dx, dy, l, a));
    emitir_flush();
    return true;
}

static bool in_copiar(int ox, int oy, int dx, int dy, int l, int a)
{
    bool solapa = dx < ox + l && ox < dx + l && dy < oy + a && oy < dy + a;
    bool ok = true;
    if (!solapa) ok = blit_copia(ox, oy, dx, dy, l, a);
    else if (dy != oy) {
        int h = dy > oy ? dy - oy : oy - dy;
        if (dy > oy)   /* hacia abajo: desde la franja de abajo */
            for (int j = a; j > 0 && ok; j -= h) { int hh = j < h ? j : h; ok = blit_copia(ox, oy + j - hh, dx, dy + j - hh, l, hh); }
        else
            for (int j = 0; j < a && ok; j += h) { int hh = a - j < h ? a - j : h; ok = blit_copia(ox, oy + j, dx, dy + j, l, hh); }
    } else if (dx != ox) {
        int w = dx > ox ? dx - ox : ox - dx;
        if (dx > ox)   /* a la derecha: desde la franja de la derecha */
            for (int i = l; i > 0 && ok; i -= w) { int ww = i < w ? i : w; ok = blit_copia(ox + i - ww, oy, dx + i - ww, dy, ww, a); }
        else
            for (int i = 0; i < l && ok; i += w) { int ww = l - i < w ? l - i : w; ok = blit_copia(ox + i, oy, dx + i, dy, ww, a); }
    }
    avancar();
    return ok;
}

static bool in_preencher(int x, int y, int l, int a, uint32_t cor)
{
    uint32_t b[INTEL_PAL_MAX];
    if (!reservar(INTEL_PAL_MAX + 6)) return false;
    emitir_n(b, intel_cmd_relleno(b, gen, pitch, fb_gfx, x, y, l, a, cor));
    emitir_flush();
    avancar();
    return true;
}

/* Suelta el forcewake que se cogió (Gen6: FORCEWAKE = 0; Gen7+: el bit con máscara). */
static void soltar_forcewake(void)
{
    if (fw_reg) wr(fw_reg, fw_off);
}

static void in_desligar(void)
{
    if (ligado) {
        wr(base + R_CTL, 0);   /* el ring se para; la pantalla no depende de él */
        wr(base + R_HEAD, 0);
        wr(base + R_TAIL, 0);
        ligado = false;
    }
    if (acordada) { soltar_forcewake(); acordada = false; }
    carga_3d = false;
    klog("[ACEL2D] intel: ring parado\n");
    gpu3d_anel_parado();   /* D285: el 3D de Gen3 vivía en este ring */
}

/* Coge un forcewake: espera a que el acuse anterior se borre (sin fallar), escribe «on»
 * y espera el acuse (bit 0) con plazo de 50 ms. */
static bool pegar_forcewake(uint32_t reg, uint32_t ack, uint32_t on, uint32_t off, char *m, int nm)
{
    acel_prazo_t p;
    acel_prazo(&p, 50);
    while (rd(ack) & 1)
        if (acel_prazo_esgotado(&p)) break;
    wr(reg, on);
    acel_prazo(&p, 50);
    while (!(rd(ack) & 1))
        if (acel_prazo_esgotado(&p)) {
            snformatar(m, (size_t)nm, "forcewake %x sin respuesta (acuse %x = %x)", reg, ack, rd(ack));
            wr(reg, off);
            return false;
        }
    fw_reg = reg; fw_ack = ack; fw_on = on; fw_off = off;
    acordada = true;
    return true;
}

/* Gen6: la GPU tiene que estar despierta para que sus registros respondan. */
static bool acordar(char *m, int nm)
{
    if (!pegar_forcewake(FORCEWAKE, FORCEWAKE_ACK, 1, 0, m, nm)) {
        snformatar(m, (size_t)nm, "FORCEWAKE sin respuesta (ack %x)", rd(FORCEWAKE_ACK));
        return false;
    }
    return true;
}

/* D359: Gen7+. Ivy Bridge: el forcewake multihilo si el firmware lo activó (ECOBUS bit 5,
 * que solo se lee con la GPU despierta), y si no el de Gen6 (lo que hace i915). Haswell y
 * Broadwell: siempre el multihilo, con el acuse en 0x130044. Después, que los hilos del GT
 * hayan despertado (GT_THREAD_STATUS, 5 ms; si no, solo se anota). */
static bool acordar_moderna(char *m, int nm)
{
    bool ok;
    wr(FORCEWAKE_MT, MASCARA_OFF(0xFFFFu));   /* WaRsClearFWBitsAtReset: lo que dejó el firmware */
    if (var == INTEL_VAR_IVB) {
        wr(FORCEWAKE, 0);
        (void)rd(ECOBUS);
        ok = pegar_forcewake(FORCEWAKE_MT, FORCEWAKE_MT_ACK, MASCARA_ON(1), MASCARA_OFF(1), m, nm);
        uint32_t eco = ok ? rd(ECOBUS) : 0;
        klog("[ACEL2D] intel: Ivy Bridge, ECOBUS %x: forcewake %s\n", eco, eco & ECOBUS_MT ? "multihilo" : "de Gen6");
        if (!ok || !(eco & ECOBUS_MT)) {
            if (ok) { soltar_forcewake(); acordada = false; }
            ok = pegar_forcewake(FORCEWAKE, FORCEWAKE_ACK, 1, 0, m, nm);
        }
    } else {
        ok = pegar_forcewake(FORCEWAKE_MT, FORCEWAKE_ACK_HSW, MASCARA_ON(1), MASCARA_OFF(1), m, nm);
    }
    if (!ok) return false;
    acel_prazo_t p;
    acel_prazo(&p, 5);
    while (rd(GT_THREAD_STATUS) & 7u)
        if (acel_prazo_esgotado(&p)) {
            klog("[ACEL2D] intel: los hilos del GT no despiertan del todo (GT_THREAD_STATUS %x): se sigue\n",
                 rd(GT_THREAD_STATUS));
            break;
        }
    klog("[ACEL2D] intel: GPU despierta (forcewake %x, acuse %x)\n", fw_reg, rd(fw_ack));
    return true;
}

/* D359: ¿qué plano muestra la pantalla? Pipes A, B y C (Ivy Bridge en adelante). Solo
 * lee registros de la pantalla (no hace falta forcewake). 0: alguno la muestra y vale. */
static int plano_da_tela(uint32_t fb, uint32_t pt, char *m, int nm)
{
    int pior = INTEL_PLANO_OUTRO;
    plano_tela = -1;
    for (int pp = 0; pp < 3; pp++) {
        intel_plano_t pl = { rd(INTEL_DSPCNTR(pp)), rd(INTEL_DSPLINOFF(pp)), rd(INTEL_DSPSTRIDE(pp)),
                             rd(INTEL_DSPSURF(pp)), rd(INTEL_DSPTILEOFF(pp)) };
        int r = intel_plano_conferir(&pl, var, fb, pt);
        klog("[ACEL2D] intel: plano %c: CNTR %x SURF %x LINOFF %x OFFSET %x STRIDE %x (inicio %x): %s\n", 'A' + pp,
             pl.cntr, pl.surf, pl.linoff, pl.tileoff, pl.stride, intel_plano_inicio(&pl, var), intel_plano_texto(r));
        if (r == INTEL_PLANO_OK) { if (plano_tela < 0) plano_tela = pp; }
        else if (r != INTEL_PLANO_OUTRO) {
            pior = r;
            snformatar(m, (size_t)nm, "plano %c: pantalla %s (CNTR %x STRIDE %x)", 'A' + pp, intel_plano_texto(r),
                       pl.cntr, pl.stride);
        }
    }
    /* Uno que no vale (en tiles, otro formato) manda aunque otro sí valga: la GPU escribiría
     * en un formato que esa salida no muestra bien. */
    if (pior != INTEL_PLANO_OUTRO) return pior;
    if (plano_tela < 0) {
        snformatar(m, (size_t)nm, "ningun plano encendido muestra la pantalla (grafica %x)", fb);
        return INTEL_PLANO_OUTRO;
    }
    return INTEL_PLANO_OK;
}

/* D359: el GTT de la pantalla, la zona de prueba y el anillo: válido, contiguo y fuera de la
 * zona reservada del hardware. Solo lee. */
static bool conferir_gtt(const pci_disp_t *d, uint32_t bar0, uint32_t fb, uint32_t bytes, char *m, int nm)
{
    uint32_t n = bytes >> 12, pb = intel_pte_bytes(var);
    uint32_t ini = intel_gtt_desloc(var) + (fb >> 12) * pb;
    uint32_t fis = bar0 + (ini & ~0xFFFu);
    uint32_t tam = (ini & 0xFFFu) + n * pb;
    if (!gtt || gtt_fis != fis || gtt_tam < tam) {   /* la ventana no se suelta: solo se rehace si no basta */
        gtt = (volatile uint32_t *)janela_mapear_tipo(fis, tam, PG_PCD | PG_PWT);
        gtt_fis = fis; gtt_tam = gtt ? tam : 0;
    }
    if (!gtt) { snformatar(m, (size_t)nm, "sin ventana para el GTT"); return false; }
    volatile uint32_t *e = (volatile uint32_t *)((volatile uint8_t *)gtt + (ini & 0xFFFu));
    /* La memoria robada: todo lo que se va a escribir (zona de prueba, anillo, página de
     * estado) tiene que estar dentro; si el GTT llevara ahí RAM del sistema, la escritura
     * la estropearía. */
    uint64_t sb = 0, st = 0;
    uint32_t gmch = pci_le32(d, 0x50) & 0xFFFFu, bdsm = pci_le32(d, 0x5C);
    if (!intel_robada(var, gmch, bdsm, &sb, &st)) {
        snformatar(m, (size_t)nm, "no se sabe donde esta la memoria robada (GMCH %x, BDSM %x)", gmch, bdsm);
        return false;
    }
    uint64_t rb = 0, f0 = 0;
    uint32_t rt = 0, ruim = 0, rreg = rd(INTEL_STOLEN_RESERVED);
    bool res = intel_reservado(var, rreg, sb + st, &rb, &rt);
    int r = intel_gtt_conferir(var, e, n, res, rb, rt, &ruim, &f0);
    klog("[ACEL2D] intel: GTT en BAR0+%x, %u paginas desde la grafica %x: primera fisica %x%08x; reservada %s "
         "(reg %x, %x%08x, %u KB): %s\n", intel_gtt_desloc(var), n, fb, (uint32_t)(f0 >> 32), (uint32_t)f0,
         res ? "si" : "no", rreg, (uint32_t)(rb >> 32), (uint32_t)rb, rt >> 10, intel_gtt_texto(r));
    klog("[ACEL2D] intel: memoria robada %x%08x, %u MB (GMCH %x, BDSM %x)\n", (uint32_t)(sb >> 32), (uint32_t)sb,
         (uint32_t)(st >> 20), gmch, bdsm);
    if (r != INTEL_GTT_OK) {
        uint32_t lo = e[ruim * (pb / 4)];
        snformatar(m, (size_t)nm, "GTT: %s (pagina %u de %u, entrada %x)", intel_gtt_texto(r), ruim, n, lo);
        return false;
    }
    if (!intel_dentro_robada(f0, n, sb, st)) {
        snformatar(m, (size_t)nm, "GTT: la pantalla y el anillo (fisica %x%08x, %u paginas) no estan en la memoria robada",
                   (uint32_t)(f0 >> 32), (uint32_t)f0, n);
        return false;
    }
    return true;
}

/* D359: ¿algún otro plano primario, sprite o cursor encendido tiene su memoria en [ini, fim)?
 * Solo lee registros de la pantalla. Los planos que muestran la propia pantalla (el elegido y
 * los clones con el mismo inicio) no cuentan: ya se comprobaron. */
static bool superficies_livres(uint32_t ini, uint32_t fim, char *m, int nm)
{
    for (int pp = 0; pp < 3; pp++) {
        uint32_t o = 0x1000u * (uint32_t)pp;
        uint32_t alto = (rd(0x6001Cu + o) & 0xFFFu) + 1;   /* PIPESRC: alto - 1 en los bits 11:0 */
        intel_plano_t pl = { rd(INTEL_DSPCNTR(pp)), rd(INTEL_DSPLINOFF(pp)), rd(INTEL_DSPSTRIDE(pp)),
                             rd(INTEL_DSPSURF(pp)), rd(INTEL_DSPTILEOFF(pp)) };
        if ((pl.cntr & INTEL_PLANO_LIGADO) && intel_plano_inicio(&pl, var) != fb_gfx &&
            intel_solapa(pl.surf & ~0xFFFu, (pl.stride & 0x3FFC0u) * alto + 0x10000u, ini, fim)) {
            snformatar(m, (size_t)nm, "el plano %c (SURF %x) usa la memoria de debajo de la pantalla", 'A' + pp, pl.surf);
            return false;
        }
        uint32_t sc = rd(0x70280u + o), ss = rd(0x7029Cu + o) & ~0xFFFu;   /* SPRCTL, SPRSURF */
        uint32_t sa = ((rd(0x70290u + o) >> 16) & 0xFFFu) + 1;              /* SPRSIZE: alto - 1 */
        if ((sc & (1u << 31)) && intel_solapa(ss, (rd(0x70288u + o) & 0x3FFC0u) * sa + 0x10000u, ini, fim)) {
            snformatar(m, (size_t)nm, "el sprite del pipe %c (SURF %x) usa la memoria de debajo de la pantalla", 'A' + pp, ss);
            return false;
        }
        /* CURCNTR/CURBASE; hasta 256x256 a 32 bits = 256 KB */
        uint32_t cc = rd(0x70080u + o), cb = rd(0x70084u + o) & ~0xFFFu;
        if ((cc & 0x27u) && intel_solapa(cb, 256u << 10, ini, fim)) {
            snformatar(m, (size_t)nm, "el cursor del pipe %c esta donde iria el anillo (%x)", 'A' + pp, cb);
            return false;
        }
    }
    return true;
}

/* D359: Gen7 (Ivy Bridge, Haswell) y Gen8 (Broadwell) por el anillo heredado del blitter.
 * Todo lo que se puede comprobar leyendo va antes de escribir nada en la tarjeta. */
static bool iniciar_moderna(const pci_disp_t *d, const acel_tela_t *t, char *m, int nm)
{
    const char *nome = NULL;
    var = intel_variante(d->modelo, &nome);
    if (!var || intel_var_geracao(var) != gen) {
        snformatar(m, (size_t)nm, "el modelo %04x no esta en la lista exacta de Gen%d", d->modelo, gen);
        return false;
    }
    uint32_t bar0 = pci_le32(d, 0x10), gmadr = pci_le32(d, 0x18);
    if ((((bar0 >> 1) & 3) == 2 && pci_le32(d, 0x14)) || (((gmadr >> 1) & 3) == 2 && pci_le32(d, 0x1C))) {
        snformatar(m, (size_t)nm, "BAR por encima de 4 GB (MMIO %x, apertura %x)", bar0, gmadr);
        return false;
    }
    bar0 &= ~0xFu; gmadr &= ~0xFu;
    klog("[ACEL2D] intel: %s (%04x), Gen%s, GTTMMADR %x, apertura %x, pantalla %x %dx%d pitch %u, %u KB del VBE\n",
         nome, d->modelo, var == INTEL_VAR_HSW ? "7.5" : var == INTEL_VAR_BDW ? "8" : "7", bar0, gmadr, t->fb_fisico,
         t->larg, t->alt, t->pitch, t->memoria >> 10);
    if (!bar0 || !gmadr) { snformatar(m, (size_t)nm, "BAR sin asignar (MMIO %x, apertura %x)", bar0, gmadr); return false; }
    if (t->fb_fisico < gmadr || t->fb_fisico - gmadr >= LIMITE_APERTURA) {
        snformatar(m, (size_t)nm, "la pantalla (%x) no esta en la apertura (%x)", t->fb_fisico, gmadr);
        return false;
    }
    if (t->bytes_px != 4) { snformatar(m, (size_t)nm, "la pantalla no es de 32 bits"); return false; }
    fb_gfx = t->fb_fisico - gmadr;
    pitch = t->pitch;
    if (fb_gfx & 0xFFFu) { snformatar(m, (size_t)nm, "la pantalla no empieza en una pagina (grafica %x)", fb_gfx); return false; }
    if (pitch & 63 || pitch >= 32768 || pitch < (uint32_t)t->larg * 4) {
        snformatar(m, (size_t)nm, "pitch %u no valido para el blitter (multiplo de 64, menos de 32 KB)", pitch);
        return false;
    }
    /* El anillo, justo detrás de la pantalla y de la zona de prueba (con UEFI/GOP no hay
     * «memoria del VBE»: vale si el GTT ya mapea esas páginas seguidas a la pantalla), y
     * detrás del anillo, su página de estado. */
    uint32_t ring_off = intel_anel_desloc(pitch, t->alt, ACEL2D_ZONA_A);
    uint32_t fim = ring_off + RING_BYTES + HWS_BYTES;
    if (fb_gfx + fim > LIMITE_APERTURA) {
        snformatar(m, (size_t)nm, "el anillo no cabe en la apertura (grafica %x + %u KB)", fb_gfx, fim >> 10);
        return false;
    }
    if (!mmio || mmio_fis != bar0) {
        mmio = (volatile uint8_t *)janela_mapear_tipo(bar0, 2 * 1024 * 1024, PG_PCD | PG_PWT);
        mmio_fis = bar0;
    }
    if (!mmio) { snformatar(m, (size_t)nm, "sin ventana para el MMIO"); return false; }
    if (plano_da_tela(fb_gfx, pitch, m, nm) != INTEL_PLANO_OK) return false;
    /* Lo que hay debajo de la pantalla (zona de prueba, anillo, página de estado) no puede
     * ser de otro plano, sprite o cursor que el firmware dejara encendido. */
    if (!superficies_livres(fb_gfx + pitch * (uint32_t)t->alt, fb_gfx + fim, m, nm)) return false;
    if (!conferir_gtt(d, bar0, fb_gfx, fim, m, nm)) return false;
    if (!ring || ring_fis != t->fb_fisico + ring_off) {
        ring = (volatile uint32_t *)janela_mapear_tipo(t->fb_fisico + ring_off, RING_BYTES + HWS_BYTES, PG_PCD | PG_PWT);
        ring_fis = t->fb_fisico + ring_off;
    }
    if (!ring) { snformatar(m, (size_t)nm, "sin ventana para el anillo"); return false; }
    ring_gfx = fb_gfx + ring_off;
    arena_off = 0; arena_bytes = 0; tela_fis = t->fb_fisico;   /* sin 3D en Gen7+ (todavía) */
    /* A partir de aquí se escribe: bus maestro (la GPU lee el anillo) y forcewake. */
    uint32_t cmd = pci_le32(d, 0x04);
    if ((cmd & 0x06) != 0x06) pci_escreve32(d, 0x04, (cmd & 0xFFFFu) | 0x06);
    if (!acordar_moderna(m, nm)) return false;
    uint32_t ctl = rd(base + R_CTL), modo = rd(base + R_GFX_MODE), mi = rd(base + R_MI_MODE);
    klog("[ACEL2D] intel: BCS antes: CTL %x HEAD %x TAIL %x START %x MI_MODE %x GFX_MODE %x\n", ctl, rd(base + R_HEAD),
         rd(base + R_TAIL), rd(base + R_START), mi, modo);
    if (ctl & CTL_VALIDO) {
        snformatar(m, (size_t)nm, "el anillo BCS ya estaba en uso (CTL %x): no se toca", ctl);
        soltar_forcewake(); acordada = false;
        return false;
    }
    if (modo & (GFX_RUN_LIST | GFX_PPGTT)) {
        snformatar(m, (size_t)nm, "el firmware dejo el BCS con %s (GFX_MODE %x): no se toca",
                   modo & GFX_RUN_LIST ? "execlists" : "PPGTT", modo);
        soltar_forcewake(); acordada = false;
        return false;
    }
    for (uint32_t i = 0; i < (RING_BYTES + HWS_BYTES) / 4; i++) ring[i] = MI_NOOP;   /* anillo y página de estado a 0 */
    (void)rd(base + R_HEAD);   /* barrera: los NOOP en la memoria antes de arrancar */
    /* La página de estado, como i915 (intel_ring_setup_status_page) antes de arrancar. */
    uint32_t hws_reg = gen == 7 ? BLT_HWS_PGA_GEN7 : base + R_HWS_PGA;
    wr(base + R_HWSTAM, 0xFFFFFFFFu);
    wr(hws_reg, ring_gfx + RING_BYTES);
    (void)rd(hws_reg);
    if (gen == 7) {
        wr(base + R_INSTPM, MASCARA_ON(INSTPM_TLB_INV | INSTPM_SYNC_FLUSH));
        acel_prazo_t p;
        acel_prazo(&p, 2);
        while (rd(base + R_INSTPM) & INSTPM_SYNC_FLUSH)
            if (acel_prazo_esgotado(&p)) { klog("[ACEL2D] intel: INSTPM: el vaciado de la TLB no termina: se sigue\n"); break; }
    }
    klog("[ACEL2D] intel: pagina de estado del BCS en %x (registro %x = %x)\n", ring_gfx + RING_BYTES, hws_reg, rd(hws_reg));
    wr(base + R_CTL, 0);
    wr(base + R_HEAD, 0);
    wr(base + R_TAIL, 0);
    wr(base + R_START, ring_gfx);
    wr(base + R_CTL, ((RING_BYTES - 4096) & 0x001FF000u) | CTL_VALIDO);
    wr(base + R_MI_MODE, MASCARA_OFF(STOP_RING));   /* que no quede parado (i915, xcs_resume) */
    tail = 0;
    ligado = true;
    uint32_t head = rd(base + R_HEAD), start = rd(base + R_START);
    ctl = rd(base + R_CTL);
    klog("[ACEL2D] intel: anillo BCS en %x (grafica %x): CTL %x HEAD %x START %x MI_MODE %x\n", ring_fis, ring_gfx, ctl,
         head, start, rd(base + R_MI_MODE));
    if ((head & HEAD_MASCARA) != 0 || start != ring_gfx || !(ctl & CTL_VALIDO)) {
        in_desligar();
        snformatar(m, (size_t)nm, "el anillo no arranco (CTL %x HEAD %x START %x)", ctl, head, start);
        return false;
    }
    emitir_flush();
    avancar();
    if (!in_esperar()) {
        uint32_t h = rd(base + R_HEAD), ipehr = rd(base + R_IPEHR);
        in_desligar();
        snformatar(m, (size_t)nm, "el anillo no avanza (HEAD %x IPEHR %x)", h, ipehr);
        return false;
    }
    klog("[ACEL2D] intel: el anillo BCS ejecuta ordenes (HEAD %x)\n", rd(base + R_HEAD));
    return true;
}

/* D246/D359: «acel2d probar --detalle»: el estado del anillo y del forcewake para el log. */
static void in_detalle(const char *fase)
{
    if (!mmio) return;
    if (gen < 4) {
        klog("[ACEL2D] intel detalle (%s): Gen%d CTL %x HEAD %x TAIL %x (escrito %x) START %x\n", fase, gen,
             rd(base + R_CTL), rd(base + R_HEAD), rd(base + R_TAIL), tail, rd(base + R_START));
        return;
    }
    klog("[ACEL2D] intel detalle (%s): Gen%d anillo %s: CTL %x HEAD %x TAIL %x (escrito %x) START %x (esperado %x) "
         "ACTHD %x MI_MODE %x IPEIR %x IPEHR %x INSTDONE %x\n", fase, gen, base == BCS_BASE ? "BCS" : "RCS",
         rd(base + R_CTL), rd(base + R_HEAD), rd(base + R_TAIL), tail, rd(base + R_START), ring_gfx, rd(base + R_ACTHD),
         rd(base + R_MI_MODE), rd(base + R_IPEIR), rd(base + R_IPEHR), rd(base + R_INSTDONE));
    if (gen >= 6)
        klog("[ACEL2D] intel detalle (%s): forcewake %x = %x, acuse %x = %x, GFX_MODE %x, GT_THREAD_STATUS %x\n", fase,
             fw_reg, fw_reg ? rd(fw_reg) : 0, fw_ack, fw_ack ? rd(fw_ack) : 0, gen >= 7 ? rd(base + R_GFX_MODE) : 0,
             rd(GT_THREAD_STATUS));
    if (gen >= 7 && plano_tela >= 0)
        klog("[ACEL2D] intel detalle (%s): plano %c CNTR %x SURF %x STRIDE %x; pantalla grafica %x pitch %u\n", fase,
             'A' + plano_tela, rd(INTEL_DSPCNTR(plano_tela)), rd(INTEL_DSPSURF(plano_tela)),
             rd(INTEL_DSPSTRIDE(plano_tela)), fb_gfx, pitch);
}

static bool in_iniciar(const pci_disp_t *d, int geracao, const acel_tela_t *t, char *m, int nm)
{
    /* D285: el 3D ya lo había puesto en marcha (el 2D estaba apagado): se usa tal cual. */
    if (ligado && mmio && ring && gen == geracao) {
        klog("[ACEL2D] intel: el ring ya estaba en marcha (lo encendio el 3D): se comparte\n");
        return true;
    }
    gen = geracao;
    base = gen >= 6 ? BCS_BASE : RCS_BASE;
    fw_reg = fw_ack = 0;
    if (gen >= 7) return iniciar_moderna(d, t, m, nm);   /* D359 */
    var = INTEL_VAR_NADA;
    uint32_t bar0 = pci_le32(d, 0x10), gmadr = pci_le32(d, 0x18);
    /* Gen4 en adelante: los dos BAR son de 64 bits; por encima de 4 GB no se llega. */
    if ((((bar0 >> 1) & 3) == 2 && pci_le32(d, 0x14)) || (((gmadr >> 1) & 3) == 2 && pci_le32(d, 0x1C))) {
        snformatar(m, (size_t)nm, "BAR por encima de 4 GB (MMIO %x, apertura %x)", bar0, gmadr);
        return false;
    }
    bar0 &= ~0xFu; gmadr &= ~0xFu;
    uint32_t mmio_tam = gen >= 4 ? 2 * 1024 * 1024 : 512 * 1024;
    klog("[ACEL2D] intel: Gen%d, MMIO %x (%u KB), apertura %x, pantalla %x, %u KB de memoria del VBE, ring %s\n",
         gen, bar0, mmio_tam >> 10, gmadr, t->fb_fisico, t->memoria >> 10, gen >= 6 ? "BCS" : "RCS");
    if (!bar0 || !gmadr) { snformatar(m, (size_t)nm, "BAR sin asignar (MMIO %x, apertura %x)", bar0, gmadr); return false; }
    if (t->fb_fisico < gmadr || t->fb_fisico - gmadr >= 0x20000000u) {
        snformatar(m, (size_t)nm, "la pantalla (%x) no esta en la apertura (%x)", t->fb_fisico, gmadr);
        return false;
    }
    fb_gfx = t->fb_fisico - gmadr;
    pitch = t->pitch;
    if (pitch & 3 || pitch >= 32768) { snformatar(m, (size_t)nm, "pitch %u no valido para el blitter", pitch); return false; }
    /* El ring al final de la memoria del VBE, por encima de la pantalla y de la zona de prueba. */
    uint32_t usado = (t->pitch * (uint32_t)(t->alt + 64) + 0xFFFu) & ~0xFFFu;
    uint32_t ring_off = (t->memoria - RING_BYTES) & ~0xFFFu;
    if (t->memoria < RING_BYTES || ring_off < usado) {
        snformatar(m, (size_t)nm, "sin memoria de video para el ring (%u KB, la pantalla usa %u KB)", t->memoria >> 10, usado >> 10);
        return false;
    }
    /* Memoria y bus maestro (la GPU lee el ring y la pantalla por el GTT); solo
     * ahora, con la pantalla comprobada en su apertura. */
    uint32_t cmd = pci_le32(d, 0x04);
    if ((cmd & 0x06) != 0x06) pci_escreve32(d, 0x04, (cmd & 0xFFFFu) | 0x06);
    if (!mmio || mmio_fis != bar0) {
        mmio = (volatile uint8_t *)janela_mapear_tipo(bar0, mmio_tam, PG_PCD | PG_PWT);
        mmio_fis = bar0;
    }
    if (!ring || ring_fis != t->fb_fisico + ring_off) {
        ring = (volatile uint32_t *)janela_mapear_tipo(t->fb_fisico + ring_off, RING_BYTES, PG_PCD | PG_PWT);
        ring_fis = t->fb_fisico + ring_off;
    }
    if (!mmio || !ring) { snformatar(m, (size_t)nm, "sin ventana para el MMIO o el ring"); return false; }
    ring_gfx = fb_gfx + ring_off;
    arena_off = usado; arena_bytes = ring_off - usado; tela_fis = t->fb_fisico;
    if (gen >= 6 && !acordar(m, nm)) return false;
    uint32_t ctl = rd(base + R_CTL);
    klog("[ACEL2D] intel: antes: CTL %x HEAD %x TAIL %x START %x MI_MODE %x\n", ctl, rd(base + R_HEAD), rd(base + R_TAIL),
         rd(base + R_START), rd(base + R_MI_MODE));
    if (ctl & CTL_VALIDO) {
        snformatar(m, (size_t)nm, "el ring ya estaba en uso (CTL %x): no se toca", ctl);
        if (acordada) { soltar_forcewake(); acordada = false; }
        return false;
    }
    for (uint32_t i = 0; i < RING_BYTES / 4; i++) ring[i] = MI_NOOP;
    wr(base + R_CTL, 0);
    wr(base + R_HEAD, 0);
    wr(base + R_TAIL, 0);
    wr(base + R_START, ring_gfx);
    wr(base + R_CTL, ((RING_BYTES - 4096) & 0x001FF000u) | CTL_VALIDO);
    tail = 0;
    ligado = true;
    uint32_t head = rd(base + R_HEAD), start = rd(base + R_START);
    ctl = rd(base + R_CTL);
    klog("[ACEL2D] intel: ring en %x (grafica %x): CTL %x HEAD %x START %x\n", ring_fis, ring_gfx, ctl, head, start);
    if ((head & HEAD_MASCARA) != 0 || start != ring_gfx || !(ctl & CTL_VALIDO)) {
        in_desligar();
        snformatar(m, (size_t)nm, "el ring no arranco (CTL %x HEAD %x START %x)", ctl, head, start);
        return false;
    }
    /* Una orden vacía: el ring tiene que avanzar antes de mandar nada de verdad. */
    emitir_flush();
    avancar();
    if (!in_esperar()) { in_desligar(); snformatar(m, (size_t)nm, "el ring no avanza (HEAD %x)", rd(base + R_HEAD)); return false; }
    return true;
}

const acel_driver_t acel_intel = {
    "intel", ACEL2D_INTEL, in_iniciar, in_copiar, in_preencher, in_esperar, NULL, in_desligar,
    true,    /* la zona de prueba va debajo de la pantalla visible */
    0,       /* D246: sin mínimo (sin medida en hardware; «acel2d probar --detalle» da los tiempos) */
    false,   /* cada orden se espera en el propio driver cuando hace falta */
    in_detalle,   /* D359 */
    false,   /* D305: solo 32 bits */
};

/* ------------------------------------------------------------ D285: el ring para el 3D de Gen3 */

bool intel_anel_info(intel_anel_t *a)
{
    if (!ligado || !mmio || !ring) return false;
    a->geracao = gen;
    a->fb_fisico = tela_fis;
    a->fb_gfx = fb_gfx;
    a->arena_off = arena_off;
    a->arena_bytes = arena_bytes;
    a->ctl = rd(base + R_CTL);
    a->head = rd(base + R_HEAD);
    a->tail = tail;
    return true;
}

/* Reserva «palavras» seguidas (sin partir una orden en el final del ring: lo que
 * falte hasta el final se rellena con MI_NOOP) y espera a que haya sitio. */
bool intel_anel_reservar(uint32_t palavras)
{
    if (!ligado || palavras * 4 + 64 > RING_BYTES / 2) return false;
    uint32_t ate_fim = (RING_BYTES - tail) / 4;
    if (ate_fim < palavras) {
        if (!reservar(ate_fim)) return false;
        while (tail) emitir(MI_NOOP);
    }
    return reservar(palavras);
}

void intel_anel_emitir(uint32_t v) { emitir(v); }

/* Termina con un vaciado (MI_FLUSH: la caché de dibujo llega a la memoria) y avisa a la GPU. */
void intel_anel_enviar(bool carga)
{
    emitir_flush();
    if (carga) {
        /* Tras una carga 3D, un segundo vaciado con unas órdenes vacías en medio: cuando
         * HEAD alcance a TAIL, el primero ya ha tenido tiempo de llegar a la memoria
         * (lo que dibujó la GPU se lee después con la CPU). */
        for (int i = 0; i < 6; i++) emitir(MI_NOOP);
        emitir_flush();
        carga_3d = true;
    }
    avancar();
}

bool intel_anel_ocioso(void)
{
    if (!ligado) return false;
    if ((rd(base + R_HEAD) & HEAD_MASCARA) != tail) return false;
    carga_3d = false;
    return true;
}

bool intel_anel_esperar(uint32_t ms)
{
    if (!ligado) return false;
    acel_prazo_t p;
    acel_prazo_longo(&p, ms);
    while ((rd(base + R_HEAD) & HEAD_MASCARA) != tail)
        if (acel_prazo_esgotado(&p)) return false;
    carga_3d = false;
    return true;
}

uint32_t intel_anel_reg(uint32_t r) { return mmio ? rd(r) : 0; }
