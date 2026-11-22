// SPDX-License-Identifier: GPL-2.0-or-later
/* acel_intel_exec.c - el blitter 2D de Intel Gen9, Gen11 y Gen12 por execlists (D434, v8.5.5.3).
 *
 * SIN PROBAR EN HARDWARE: QEMU no emula ninguna GPU de Intel. La capa común solo lo activa si
 * su autoprueba del arranque da los mismos píxeles que la CPU y termina a tiempo; si no, para
 * el motor y se queda la CPU con el motivo en el log.
 *
 * Por qué execlists: desde Gen9 el blitter ya no acepta el anillo heredado de acel_intel.c (D359).
 * Aquí se manda cada tanda de órdenes como un contexto lógico: una imagen de registros en la
 * GGTT (página 0: PPHWSP; página 1: la imagen que monta i915 para un contexto nuevo), un anillo de
 * 4 KB, una página de estado (HWS) donde el hardware escribe el CSB y la marca de fin de tanda, y
 * una PPGTT de 4 niveles que replica la GGTT en la pantalla y la zona de prueba (con execlists las
 * direcciones de las órdenes pasan por la PPGTT del contexto). El envío es ELSP (Gen9) o ELSQ +
 * EXECLIST_CONTROL (Gen11 en adelante).
 *
 * Límites a propósito (mínimo para 2D): cada tanda empieza en la posición 0 del anillo, se
 * espera a que el motor termine y deje el contexto (plazo de 250 ms) antes de la siguiente, y las
 * órdenes son las de acel_intel_logica.h (XY_COLOR_BLT, XY_SRC_COPY_BLT y MI_FLUSH_DW). No hay GuC,
 * ni HuC, ni firmware, ni modos de pantalla (el GOP sigue mandando), ni 3D.
 *
 * Memoria: la zona (contexto, página de estado, anillo y PPGTT) va justo detrás de la pantalla y
 * de la zona de prueba, en la memoria robada, como en acel_intel.c. Si el GTT no la mapea, se
 * escriben sus entradas solo si todas las páginas están dentro de la memoria robada y fuera de la
 * zona reservada; si no, la CPU.
 *
 * El log va por pasos («paso N»): con una foto del dmesg se sabe dónde se paró (docs/gpus.md,
 * «Qué probar en el mini PC i3-1220P»). Más detalle: docs/decisoes.md, D434. */
#include "acel2d.h"
#include "acel_intel_logica.h"
#include "console.h"
#include "paginacao.h"
#include "texto.h"
#include "rastro.h"
#include "intel_rps.h"
#include "wc.h"
#include "memoria.h"

#define EX_BCS_BASE        0x22000u
#define EX_CTX_BYTES       (12u * 1024u)    /* página 0 (PPHWSP), 1 (registros) y 2 (reserva por si el guardado ocupa más) */
#define EX_HWS_BYTES       (4u * 1024u)
#define EX_RING_BYTES      (4u * 1024u)     /* cada tanda cabe de sobra: empieza siempre en 0 */
#define EX_OFF_HWS         EX_CTX_BYTES
#define EX_OFF_RING        (EX_OFF_HWS + EX_HWS_BYTES)
#define EX_OFF_PPGTT       (EX_OFF_RING + EX_RING_BYTES)
#define EX_PT_MAX          32u              /* tablas de 2 MB: hasta 64 MB de pantalla y zona de prueba */
#define EX_AREA_MAX        (EX_OFF_PPGTT + (3u + EX_PT_MAX) * 4096u)
_Static_assert(EX_AREA_MAX <= INTEL_EXEC_ZONA2D_MAX, "la zona del 3D (D438) va detras de INTEL_EXEC_ZONA2D_MAX");
#define EX_CSB_DWORD       (0x40u / 4u)     /* I915_HWS_CSB_BUF0_INDEX: palabra 0x10 de la página de estado */
#define EX_MARCA_DWORD     0x80u            /* la marca de fin de tanda (byte 0x200: alineada a 8, bit 5 a 0) */
#define EX_PRAZO_MS        1000u            /* D438: una tanda junta muchas órdenes (copias de pantalla entera); sin GPU que responda, se para */
#define EX_PRAZO_CSB_MS    2u               /* después, una entrada del CSB que no llega a la memoria se lee del MMIO */
#define EX_PRAZO_FW_MS     50u
#define EX_LIMITE_APERTURA (128u << 20)
#define EX_MOSTRAR_CSB     4u               /* tandas cuyas entradas del CSB se anotan */
#define EX_CSB_VAZIA       0xFFFFFFFFu      /* entrada todavía sin escribir (reset_csb_pointers de i915) */
#define EX_MASCARA_ON(b)   (((uint32_t)(b) << 16) | (uint32_t)(b))
#define EX_MASCARA_OFF(b)  ((uint32_t)(b) << 16)

/* Estado del driver (una sola tarjeta). */
static volatile uint8_t *mmio;
static uint32_t mmio_fis;
static volatile uint32_t *area;     /* CPU: página 0 del contexto; la página 1 es area + 1024 palabras (UC, hasta el anillo) */
/* D434 (2D asíncrono, 2026-11-22): el anillo en una ventana aparte, en WC si la PAT lo permite (la CPU solo lo escribe;
 * nunca la misma página con dos tipos), y la PPGTT en otra sin caché. */
static volatile uint32_t *anel_w, *pp_w;
static bool anel_wc, rps;
static uint32_t n_flush_postos, n_flush_poupados, n_ordens;
static uint32_t area_fis, area_gfx, area_bytes; /* la zona en la apertura y en la GGTT (inicio del contexto) */
static uint32_t fb_gfx, pitch, ring_off, n_pt;
static uint64_t fis_tela;           /* la página física (memoria robada) de la primera de la pantalla */
static int gen, var;
static bool ligado, acordada, elsq, motor_nosso, avisou_marca, avisou_mmio;
static volatile uint32_t *gtt;      /* ventana del GTT (BAR 0 + 8 MB) para las entradas de la zona */
static uint32_t gtt_fis, gtt_tam;
static uint32_t csb_lido;           /* última entrada del CSB que ya se consumió */
static uint32_t envios, marca;
static int plano_tela = -1;

static inline uint32_t rd(uint32_t r)             { return *(volatile uint32_t *)(mmio + r); }
static inline void     wr(uint32_t r, uint32_t v) { *(volatile uint32_t *)(mmio + r) = v; }
static inline volatile uint32_t *pag_estado(void) { return area + 1024; }
static inline volatile uint32_t *anel(void)       { return anel_w; }
static inline volatile uint32_t *hws(void)        { return area + EX_OFF_HWS / 4; }
static inline int n_pipes(void)                   { return gen >= 12 ? 4 : 3; }

/* ------------------------------------------------------------- forcewake del dominio GT */

/* En Gen9, Gen11 y Gen12 el BCS (0x22000-0x23FFF) y el PAT (0x40E0, 0x4800) son del dominio GT
 * (intel_uncore.c de i915: __gen9_fw_ranges, __gen11_fw_ranges y __gen12_fw_ranges): basta con él. */
static bool fw_pegar(char *m, int nm)
{
    acel_prazo_t p;
    wr(EX_FW_GT, EX_MASCARA_OFF(0xFFFDu));          /* lo que dejó el firmware: fuera (WaRsClearFWBitsAtReset); el bit 1 es del 3D (D438) */
    acel_prazo(&p, EX_PRAZO_FW_MS);
    while (rd(EX_FW_GT_ACK) & 1u)
        if (acel_prazo_esgotado(&p)) break;
    wr(EX_FW_GT, EX_MASCARA_ON(1));
    acel_prazo(&p, EX_PRAZO_FW_MS);
    while (!(rd(EX_FW_GT_ACK) & 1u))
        if (acel_prazo_esgotado(&p)) {
            snformatar(m, (size_t)nm, "paso 4: forcewake GT %x sin respuesta (acuse %x)", EX_FW_GT, rd(EX_FW_GT_ACK));
            wr(EX_FW_GT, EX_MASCARA_OFF(1));
            return false;
        }
    acordada = true;
    klog("[ACEL2D] intel exec: paso 4: GPU despierta (forcewake GT %x, acuse %x)\n", EX_FW_GT, rd(EX_FW_GT_ACK));
    return true;
}

static void fw_soltar(void)
{
    if (acordada) { wr(EX_FW_GT, EX_MASCARA_OFF(1)); acordada = false; }
}

/* ------------------------------------------------------------- pantalla y GTT */

/* ¿Qué plano 1 (pipes A-C; también D en Gen12) muestra la pantalla? Planos de Gen9 en adelante.
 * Solo lee. */
static int plano_da_tela(uint32_t fb, uint32_t pt, char *m, int nm)
{
    int pior = INTEL_PLANO_OUTRO;
    plano_tela = -1;
    for (int pp = 0; pp < n_pipes(); pp++) {
        uint32_t cntr = rd(INTEL_DSPCNTR(pp)), surf = rd(INTEL_DSPSURF(pp)), stride = rd(INTEL_DSPSTRIDE(pp));
        uint32_t off = rd(INTEL_DSPTILEOFF(pp));
        int r = intel_plano9_conferir(gen, cntr, surf, stride, off, fb, pt);
        klog("[ACEL2D] intel exec: paso 2: plano %c: CTL %x SURF %x STRIDE %x OFFSET %x: %s\n", 'A' + pp, cntr, surf,
             stride, off, r == INTEL_PLANO_OK ? "lineal, XRGB8888, mismo pitch" : intel_plano_texto(r));
        if (r == INTEL_PLANO_OK) { if (plano_tela < 0) plano_tela = pp; }
        else if (r != INTEL_PLANO_OUTRO) {
            pior = r;
            snformatar(m, (size_t)nm, "paso 2: plano %c: pantalla %s (CTL %x STRIDE %x)", 'A' + pp, intel_plano_texto(r),
                       cntr, stride);
        }
    }
    if (pior != INTEL_PLANO_OUTRO) return pior;
    if (plano_tela < 0) {
        snformatar(m, (size_t)nm, "paso 2: ningun plano muestra la pantalla (grafica %x)", fb);
        return INTEL_PLANO_OUTRO;
    }
    return INTEL_PLANO_OK;
}

/* ¿Otra superficie encendida (planos 1-4 o cursor de cada pipe) usa la memoria donde va la zona?
 * Solo lee. */
static bool superficies_livres(uint32_t ini, uint32_t fim, char *m, int nm)
{
    for (int pp = 0; pp < n_pipes(); pp++) {
        uint32_t o = 0x1000u * (uint32_t)pp;
        uint32_t alto = (rd(0x6001Cu + o) & 0xFFFu) + 1;   /* PIPESRC: alto - 1 */
        for (uint32_t q = 0; q < 4; q++) {
            uint32_t oq = o + 0x100u * q;                   /* PLANE_CTL_1..4 del pipe: +0x100 cada uno */
            uint32_t cntr = rd(0x70180u + oq), surf = rd(0x7019Cu + oq) & ~0xFFFu, stride = rd(0x70188u + oq);
            if ((cntr & EX_PLANO_LIGADO) && surf != fb_gfx &&
                intel_solapa(surf, ((stride & 0xFFFu) * 64u) * alto + 0x10000u, ini, fim)) {
                snformatar(m, (size_t)nm, "paso 2: el plano %u del pipe %c (SURF %x) usa la memoria de debajo de la pantalla",
                           q + 1, 'A' + pp, surf);
                return false;
            }
        }
        uint32_t cc = rd(0x70080u + o), cb = rd(0x70084u + o) & ~0xFFFu;   /* CUR_CTL, CUR_BASE */
        if ((cc & 0x27u) && intel_solapa(cb, 256u << 10, ini, fim)) {
            snformatar(m, (size_t)nm, "paso 2: el cursor del pipe %c esta donde iria la zona (%x)", 'A' + pp, cb);
            return false;
        }
    }
    return true;
}

/* El GTT de la pantalla y de la zona: las entradas de la pantalla tienen que ser válidas y
 * contiguas (como acel_intel.c); las de la zona, válidas y contiguas o, si faltan, se escriben.
 * Todo (pantalla, zona de prueba y zona del driver) tiene que estar en la memoria robada y fuera
 * de la zona reservada. Deja en «fis_tela» la física de la primera página. */
static bool gtt_preparar(const pci_disp_t *d, uint32_t bar0, const acel_tela_t *t, uint32_t fim, char *m, int nm)
{
    uint32_t n_tela = (pitch * (uint32_t)t->alt + 0xFFFu) >> 12;
    uint32_t n = (fim + 0xFFFu) >> 12;
    uint32_t ini = intel_gtt_desloc(INTEL_VAR_BDW) + (fb_gfx >> 12) * intel_pte_bytes(INTEL_VAR_BDW);
    uint32_t fis = bar0 + (ini & ~0xFFFu);
    uint32_t tam = (ini & 0xFFFu) + n * intel_pte_bytes(INTEL_VAR_BDW);
    if (!gtt || gtt_fis != fis || gtt_tam < tam) {
        gtt = (volatile uint32_t *)janela_mapear_tipo(fis, tam, PG_PCD | PG_PWT);
        gtt_fis = fis; gtt_tam = gtt ? tam : 0;
    }
    if (!gtt) { snformatar(m, (size_t)nm, "paso 3: sin ventana para el GTT"); return false; }
    volatile uint32_t *e = (volatile uint32_t *)((volatile uint8_t *)gtt + (ini & 0xFFFu));

    uint64_t sb = 0, st = 0;
    uint32_t gmch = pci_le32(d, 0x50) & 0xFFFFu;
    uint32_t bsm_lo = pci_le32(d, gen >= 11 ? 0xC0 : 0x5C), bsm_hi = gen >= 11 ? pci_le32(d, 0xC4) : 0;
    bool robada = intel_robada9(gen, gmch, bsm_lo, bsm_hi, &sb, &st);
    klog("[ACEL2D] intel exec: paso 3: memoria robada %x%08x, %u MB (GMCH %x, BSM %x %x)\n", (uint32_t)(sb >> 32),
         (uint32_t)sb, (uint32_t)(st >> 20), gmch, bsm_hi, bsm_lo);
    if (!robada) {
        snformatar(m, (size_t)nm, "paso 3: no se sabe donde esta la memoria robada (GMCH %x, BSM %x)", gmch, bsm_lo);
        return false;
    }
    uint64_t rb = 0, f0 = 0;
    uint32_t rt = 0, ruim = 0;
    bool res = intel_reservado9(gen, rd(INTEL_STOLEN_RESERVED), rd(INTEL_STOLEN_RESERVED + 4), sb + st, &rb, &rt);
    int r = intel_gtt_conferir(INTEL_VAR_BDW, e, n_tela, res, rb, rt, &ruim, &f0);
    klog("[ACEL2D] intel exec: paso 3: GTT: pantalla %u paginas desde %x: primera fisica %x%08x: %s; reservada %x%08x + %u KB\n",
         n_tela, fb_gfx, (uint32_t)(f0 >> 32), (uint32_t)f0, intel_gtt_texto(r), (uint32_t)(rb >> 32), (uint32_t)rb,
         res ? rt >> 10 : 0);
    if (r != INTEL_GTT_OK) {
        snformatar(m, (size_t)nm, "paso 3: GTT de la pantalla: %s (pagina %u)", intel_gtt_texto(r), ruim);
        return false;
    }
    /* La pantalla, la zona de prueba y la del driver, enteras en la memoria robada: así, si hay que
     * escribir entradas, nunca apuntan a la RAM del sistema. */
    if (!intel_dentro_robada(f0, n, sb, st)) {
        snformatar(m, (size_t)nm, "paso 3: la pantalla y la zona (%u paginas desde la fisica %x%08x) no caben en la memoria robada",
                   n, (uint32_t)(f0 >> 32), (uint32_t)f0);
        return false;
    }
    /* El resto (zona de prueba y zona del driver): válidas y contiguas, o a escribir. */
    uint32_t molde = e[0] & 0xFFEu;
    uint32_t faltan = 0;
    for (uint32_t k = n_tela; k < n; k++) {
        uint32_t lo = e[2 * k], hi = e[2 * k + 1];
        uint64_t esperada = f0 + (uint64_t)k * 4096u;
        if (lo & 1u) {
            uint64_t f = (((uint64_t)hi & 0x3FFFu) << 32) | (lo & 0xFFFFF000u);
            if (f != esperada) {
                snformatar(m, (size_t)nm, "paso 3: GTT: la pagina %u de la zona apunta a %x%08x, no a la contigua", k,
                           (uint32_t)(f >> 32), (uint32_t)f);
                return false;
            }
        } else faltan++;
        if (res && esperada + 4096u > rb && esperada < rb + rt) {
            snformatar(m, (size_t)nm, "paso 3: GTT: la pagina %u de la zona cae en la memoria reservada", k);
            return false;
        }
    }
    if (faltan) {
        /* Solo dentro de la memoria robada (comprobado arriba) y fuera de la reservada. */
        for (uint32_t k = n_tela; k < n; k++) {
            if (e[2 * k] & 1u) continue;
            uint32_t lo, hi;
            intel_pte_gen9(f0 + (uint64_t)k * 4096u, molde, &lo, &hi);
            e[2 * k + 1] = hi;               /* primero la mitad alta; la válida es la baja */
            e[2 * k] = lo;
        }
        wr(EX_GFX_FLSH_CNTL, 1u);           /* GFX_FLSH_CNTL: vaciar la GGTT */
        (void)rd(EX_GFX_FLSH_CNTL);
        for (uint32_t k = n_tela; k < n; k++) {
            uint64_t f = (((uint64_t)e[2 * k + 1] & 0x3FFFu) << 32) | (e[2 * k] & 0xFFFFF000u);
            if (!(e[2 * k] & 1u) || f != f0 + (uint64_t)k * 4096u) {
                snformatar(m, (size_t)nm, "paso 3: GTT: la pagina %u de la zona no quedo mapeada", k);
                return false;
            }
        }
        klog("[ACEL2D] intel exec: paso 3: GTT: %u paginas de la zona mapeadas por el driver (en la memoria robada)\n",
             faltan);
    }
    fis_tela = f0;
    return true;
}

/* ------------------------------------------------------------- envío de lotes */

/* Consume las entradas nuevas del CSB; true si alguna dice que el motor dejó el contexto. Se leen
 * de la página de estado; una entrada todavía sin escribir (~0: puede llegar tarde, HSDES#22011327657
 * en i915) se espera y, pasado «mmio_ok», se toma la copia de los registros del motor. */
static bool csb_consumir(bool mmio_ok)
{
    uint32_t w, rl, tam = intel_csb_tam(gen);
    bool livre = false;
    intel_csb_ptrs(rd(EX_BCS_BASE + EX_RING_CSB_PTR), gen, &w, &rl);
    for (uint32_t v = 0; v < tam && intel_csb_novas(w, csb_lido, gen); v++) {   /* como mucho, el CSB entero */
        uint32_t j = (csb_lido + 1) % tam;
        volatile uint32_t *c = hws() + EX_CSB_DWORD + 2 * j;
        uint32_t lo = c[0], hi = c[1];
        const char *de = "memoria";
        if (lo == EX_CSB_VAZIA && hi == EX_CSB_VAZIA) {
            if (!mmio_ok) break;
            lo = rd(EX_BCS_BASE + intel_csb_mmio(j));
            hi = rd(EX_BCS_BASE + intel_csb_mmio(j) + 4);
            de = "MMIO";
            if (!avisou_mmio) {
                klog("[ACEL2D] intel exec: el CSB no llega a la pagina de estado: se lee del MMIO\n");
                avisou_mmio = true;
            }
        }
        bool fim = intel_csb_terminou(lo, hi, gen);
        if (fim) livre = true;
        if (envios <= EX_MOSTRAR_CSB)
            klog("[ACEL2D] intel exec: tanda %u: CSB[%u] = %08x %08x (%s, %s)\n", envios, j, lo, hi,
                 fim ? "motor libre" : "cambio", de);
        c[0] = EX_CSB_VAZIA;                 /* consumida: así se ve cuándo se vuelve a escribir */
        c[1] = EX_CSB_VAZIA;
        csb_lido = j;
        wr(EX_BCS_BASE + EX_RING_CSB_PTR, intel_csb_avisar(j, gen));
    }
    return livre;
}

/* D438 (revisión del 2D tras el Dell UHD 620): las órdenes ya no van una por tanda. Se juntan en
 * «pend» (en la RAM) y se mandan de una vez: si el motor está libre, en cuanto llegan; si no, cuando
 * termina la tanda en marcha (en la siguiente orden, al llenarse «pend» o en in_esperar, que la capa
 * llama antes de que la CPU toque la pantalla: el driver es asíncrono). Así una ráfaga de copias
 * paga un solo cambio de contexto y una sola espera en vez de una por orden (en el Dell: 8 copias,
 * 9286 us frente a 191 de la CPU). Cada tanda sigue empezando en la posición 0 del anillo y solo se
 * reescribe la imagen con el motor libre (la marca y el CSB, como antes). */
#define EX_PEND_MAX (EX_RING_BYTES / 4u - 8u)    /* sitio para el NOOP de relleno y la marca */
/* Diagnóstico (Dell UHD 620, 2026-11-21): compilado con -DEX_ASINCRONO=0, cada orden vuelve a esperar a su
 * tanda (como en la primera versión de D434), para separar un fallo del modo asíncrono de otro del kernel.
 * Las marcas del rastro (0x0400 | paso, 0x0480 | tanda) sobreviven a un reinicio en caliente (rastro.h). */
#ifndef EX_ASINCRONO
#define EX_ASINCRONO 1
#endif
static uint32_t pend[EX_PEND_MAX];
static uint32_t n_pend, cola_tanda;
static bool ocupado, t_fim, t_livre;
static acel_prazo_t t_csb;                      /* desde el envío: pasados 2 ms, el CSB se lee del MMIO */

/* Sin esperar: ¿terminó la tanda en marcha (marca en la página de estado, o HEAD en la cola si la
 * marca no llega a la memoria) y dejó el motor libre (CSB)? */
static bool ex_sondear(void)
{
    if (!ocupado) return true;
    bool mmio_ok = acel_prazo_esgotado(&t_csb);
    if (csb_consumir(mmio_ok)) t_livre = true;
    if (!t_fim) {
        if (hws()[EX_MARCA_DWORD] == marca) t_fim = true;
        else if (t_livre && (rd(EX_BCS_BASE + EX_RING_HEAD) & 0x001FFFFCu) == cola_tanda) {
            t_fim = true;
            if (!avisou_marca) {
                klog("[ACEL2D] intel exec: la marca no llega a la pagina de estado (HEAD si llega a la cola)\n");
                avisou_marca = true;
            }
        }
    }
    if (t_fim && t_livre) ocupado = false;
    return !ocupado;
}

/* Espera, con plazo, a que termine la tanda en marcha. false: el motor se para (STOP_RING) y la
 * capa común vuelve a la CPU. */
static bool ex_aguardar(uint32_t ms)
{
    if (!ocupado) return true;
    acel_prazo_t p;
    acel_prazo(&p, ms);
    for (;;) {
        if (ex_sondear()) return true;
        if (acel_prazo_esgotado(&p)) {
            klog("[ACEL2D] intel exec: tiempo agotado (%u ms) en la tanda %u: marca %x (espera %x), libre %d, CSB %x, "
                 "STAT %x, HEAD %x, TAIL %x (cola %x)\n", ms, envios, hws()[EX_MARCA_DWORD], marca, t_livre,
                 rd(EX_BCS_BASE + EX_RING_CSB_PTR), rd(EX_BCS_BASE + EX_RING_EXEC_STAT), rd(EX_BCS_BASE + EX_RING_HEAD),
                 rd(EX_BCS_BASE + EX_RING_TAIL), cola_tanda);
            for (uint32_t j = 0; j < intel_csb_tam(gen); j++)
                klog("[ACEL2D] intel exec: CSB[%u] memoria %08x %08x, MMIO %08x %08x\n", j,
                     hws()[EX_CSB_DWORD + 2 * j], hws()[EX_CSB_DWORD + 2 * j + 1], rd(EX_BCS_BASE + intel_csb_mmio(j)),
                     rd(EX_BCS_BASE + intel_csb_mmio(j) + 4));
            /* Que el motor no siga: STOP_RING (intel_engine_stop_cs de i915). */
            wr(EX_BCS_BASE + EX_RING_MI_MODE, EX_MASCARA_ON(EX_STOP_RING));
            ocupado = false;
            n_pend = 0;
            ligado = false;
            return false;
        }
    }
}

/* Manda lo que hay en «pend» (si «vazia», también sin órdenes: la tanda de prueba): espera a la
 * anterior, lo copia al anillo desde 0 con un NOOP si hace falta (cola alineada a 8 bytes) y la
 * marca detrás, apunta la imagen del contexto a ello (HEAD 0, TAIL) y manda el descriptor. */
static int n_pdst, n_psrc;   /* rectángulos de la tanda en «pend» desde el último vaciado (ex_dependencia) */
static bool tlb_pendente;    /* D434: la PPGTT cambió (volcado): la tanda siguiente empieza invalidando el TLB */
#define EX_RASCUNHO_DWORD 0x90u   /* palabra de la página de estado para la escritura que pide la invalidación del TLB */

static bool ex_despachar(bool vazia)
{
    if (!ligado) return false;
    if (!n_pend && !vazia) return true;
    if (!ex_aguardar(EX_PRAZO_MS)) return false;
    n_pdst = n_psrc = 0;   /* la marca de detrás vacía: la tanda siguiente empieza sin nada pendiente */
    volatile uint32_t *r = anel();
    uint32_t k = 0, mb[4];
    if (tlb_pendente) {
        /* MI_FLUSH_DW con MI_INVALIDATE_TLB y una escritura detrás (gen8_emit_flush_xcs de i915: la invalidación pide
         * una operación posterior). */
        intel_cmd_marca(mb, area_gfx + EX_OFF_HWS + EX_RASCUNHO_DWORD * 4u, 0);
        mb[0] |= 1u << 18;
        for (int i = 0; i < 4; i++) r[k++] = mb[i];
        tlb_pendente = false;
    }
    for (uint32_t i = 0; i < n_pend; i++) r[k++] = pend[i];
    if (k & 1u) r[k++] = INTEL_MI_NOOP;
    marca++;
    int nm = intel_cmd_marca(mb, area_gfx + EX_OFF_HWS + EX_MARCA_DWORD * 4u, marca);
    for (int i = 0; i < nm; i++) r[k++] = mb[i];
    volatile uint32_t *est = pag_estado();
    est[EX_CTX_RING_HEAD] = 0;
    est[EX_CTX_RING_TAIL] = k * 4u;
    cola_tanda = k * 4u;
    n_pend = 0;
    __asm__ volatile("lock; orl $0, (%%esp)" ::: "memory");
    (void)rd(EX_BCS_BASE + EX_RING_HEAD);   /* barrera: lo escrito por la CPU llega antes de que el motor lo lea */

    uint64_t desc = intel_ctx_descritor(area_gfx, gen, 3u);
    uint32_t off[4], val[4];
    int q = intel_envio_seq(desc, elsq, off, val);
    envios++;
    if (envios < 64) rastro_marca(0x0480u | envios);
    ocupado = true; t_fim = t_livre = false;
    acel_prazo(&t_csb, EX_PRAZO_CSB_MS);
    if (rps) intel_rps_trabalho();   /* D434: la GPU en RP0 mientras trabaja (intel_rps.c, común con el 3D) */
    for (int i = 0; i < q; i++) wr(EX_BCS_BASE + off[i], val[i]);
    if (elsq) wr(EX_BCS_BASE + EX_RING_ELSQ_CTRL, EX_EL_CTRL_LOAD);
    return true;
}

/* Añade «n» palabras a lo pendiente; si el motor está libre, la tanda sale ya. false: la capa
 * común para el motor. */
static bool ex_enviar(const uint32_t *b, int n)
{
    if (!ligado || n <= 0 || (uint32_t)n > EX_PEND_MAX) return false;
    if (n_pend + (uint32_t)n > EX_PEND_MAX && !ex_despachar(false)) return false;
    for (int i = 0; i < n; i++) pend[n_pend++] = b[i];
    if (!EX_ASINCRONO) return ex_despachar(false) && ex_aguardar(EX_PRAZO_MS);
    if (ex_sondear()) return ex_despachar(false);
    return true;
}

/* ------------------------------------------------------------- órdenes 2D */

/* D434 (2D asíncrono, 2026-11-22): el MI_FLUSH_DW detrás de cada orden pasa a ir solo delante de una orden que depende
 * de otra de la misma tanda: que lea lo que otra escribió, o escriba donde otra lee o escribe (desde el último vaciado).
 * Las independientes van seguidas, sin esperarse unas a otras. La marca del final de cada tanda ya vacía. */
#define EX_RET_MAX 32
typedef struct { int x0, y0, x1, y1; } ex_ret_t;
static ex_ret_t ret_dst[EX_RET_MAX], ret_src[EX_RET_MAX];

static bool ret_cruza(const ex_ret_t *r, int x, int y, int l, int a)
{
    return x < r->x1 && r->x0 < x + l && y < r->y1 && r->y0 < y + a;
}

/* Si la orden (lee ox.. si «le», escribe dx..) depende de lo pendiente, escribe un vaciado en b; devuelve sus palabras. */
static int ex_dependencia(uint32_t *b, bool le, int ox, int oy, int dx, int dy, int l, int a)
{
    bool dep = n_pdst >= EX_RET_MAX || n_psrc >= EX_RET_MAX;
    for (int i = 0; i < n_pdst && !dep; i++) dep = ret_cruza(&ret_dst[i], dx, dy, l, a) || (le && ret_cruza(&ret_dst[i], ox, oy, l, a));
    for (int i = 0; i < n_psrc && !dep; i++) dep = ret_cruza(&ret_src[i], dx, dy, l, a);
    int n = 0;
    if (dep) { n = intel_cmd_flush(b, gen); n_pdst = n_psrc = 0; n_flush_postos++; }
    else n_flush_poupados++;
    ret_dst[n_pdst++] = (ex_ret_t){ dx, dy, dx + l, dy + a };
    if (le) ret_src[n_psrc++] = (ex_ret_t){ ox, oy, ox + l, oy + a };
    return n;
}

/* Que la orden quepa en «pend» sin que ex_enviar tenga que mandar la tanda a medias (perdería los rectángulos). */
static bool ex_sitio(void)
{
    return n_pend + 2u * INTEL_PAL_MAX + 8u <= EX_PEND_MAX || ex_despachar(false);
}

static bool blit_copia(int ox, int oy, int dx, int dy, int l, int a)
{
    uint32_t b[2 * INTEL_PAL_MAX + 4];
    if (!ex_sitio()) return false;
    int n = ex_dependencia(b, true, ox, oy, dx, dy, l, a);
    n += intel_cmd_copia(b + n, gen, pitch, fb_gfx, ox, oy, dx, dy, l, a);
    if (n & 1) b[n++] = INTEL_MI_NOOP;
    n_ordens++;
    return ex_enviar(b, n);
}

/* Copia con solapes en cualquier dirección: se parte en franjas que no se pisan (igual que
 * acel_intel.c, D195). */
/* Diagnóstico (D434, 2026-11-22): con -DEX_SOLAPE_DIRECTO=1 las copias solapadas van enteras, sin franjas, para ver si el
 * blitter de Gen9 resuelve el solape por sí mismo (la autoprueba compara copias solapadas en todas las direcciones). */
#ifndef EX_SOLAPE_DIRECTO
#define EX_SOLAPE_DIRECTO 0
#endif
static bool in_copiar(int ox, int oy, int dx, int dy, int l, int a)
{
    bool solapa = dx < ox + l && ox < dx + l && dy < oy + a && oy < dy + a;
    bool ok = true;
    /* D434 (Dell UHD 620, 2026-11-22): el blitter de Gen9 resuelve el solape solo (autoprueba completa y tanda de copias
     * solapadas en todas las direcciones iguales a la CPU bit a bit; 966 us frente a 5998 en franjas). Gen11/12, sin
     * comprobar: en franjas como D195. */
    if (!solapa || EX_SOLAPE_DIRECTO || gen == 9) return blit_copia(ox, oy, dx, dy, l, a);
    if (dy != oy) {
        int h = dy > oy ? dy - oy : oy - dy;
        if (dy > oy)
            for (int j = a; j > 0 && ok; j -= h) { int hh = j < h ? j : h; ok = blit_copia(ox, oy + j - hh, dx, dy + j - hh, l, hh); }
        else
            for (int j = 0; j < a && ok; j += h) { int hh = a - j < h ? a - j : h; ok = blit_copia(ox, oy + j, dx, dy + j, l, hh); }
    } else if (dx != ox) {
        int w = dx > ox ? dx - ox : ox - dx;
        if (dx > ox)
            for (int i = l; i > 0 && ok; i -= w) { int ww = i < w ? i : w; ok = blit_copia(ox + i - ww, oy, dx + i - ww, dy, ww, a); }
        else
            for (int i = 0; i < l && ok; i += w) { int ww = l - i < w ? l - i : w; ok = blit_copia(ox + i, oy, dx + i, dy, ww, a); }
    }
    return ok;
}

static bool in_preencher(int x, int y, int l, int a, uint32_t cor)
{
    uint32_t b[2 * INTEL_PAL_MAX + 4];
    if (!ex_sitio()) return false;
    int n = ex_dependencia(b, false, 0, 0, x, y, l, a);
    n += intel_cmd_relleno(b + n, gen, pitch, fb_gfx, x, y, l, a, cor);
    if (n & 1) b[n++] = INTEL_MI_NOOP;
    n_ordens++;
    return ex_enviar(b, n);
}

/* La capa llama aquí antes de que la CPU toque la pantalla: se manda lo pendiente y se espera. */
static bool in_esperar(void) { return ex_despachar(false) && ex_aguardar(EX_PRAZO_MS); }

/* ------------------------------------------------------------- volcado del búfer de atrás (D387, D434)
 *
 * El blitter copia del búfer de atrás (RAM del sistema) a la pantalla: la CPU solo pone la orden y sigue (la capa espera
 * con acel_sincronizar antes de que la CPU escriba en la pantalla). Las páginas del búfer van en la PPGTT del contexto
 * (no en la GGTT: la pantalla y el resto del sistema no lo ven), en dos regiones (0 el búfer de atrás, 1 el de la
 * autoprueba), SOLO DE LECTURA (la GPU no puede escribir en la RAM) y con el índice 5 del PAT con caché en la LLC (WB,
 * el valor que le da i915): la LLC es coherente con la CPU, así que no hace falta vaciar nada antes de cada copia. Las
 * tablas van en RAM y se vacían de la caché de la CPU al escribirlas; al cambiarlas, la tanda siguiente invalida el
 * TLB. Solo Gen9 (el PAT y la PTE de Gen11/12 no se han comprobado). */
#define VG_GFX0      0x30000000u        /* región 0: hasta 16 MB (2048x2048) */
#define VG_PT0       8u
#define VG_GFX1      0x31000000u        /* región 1: 2 MB */
#define VG_PT1       1u
#define VG_PAT_HI    0x40E4u            /* GEN8_PRIVATE_PAT_HI: índices 4-7; el 4 es del 3D (D448), el 5 del volcado */
#define VG_PAT5      0x1Bu              /* GEN8_PPAT_WB | GEN8_PPAT_LLCELLC | GEN8_PPAT_AGE(1): el índice 5 de i915 */
#define VG_PTE_RO5   (1u | (1u << 3) | (1u << 7))   /* presente, sin escritura, PWT + PAT = índice 5 */
static uint32_t vg_pt[VG_PT0 + VG_PT1];
static struct { uint32_t gfx, desl, passo, larg, alt, npag; } vg_r[2];
static bool vg_pronto, vg_pat_tocado;
static uint32_t vg_pat_antes, vg_ops;

static void vg_cache_vaciar(const void *p, uint32_t n)
{
    uint32_t a = (uint32_t)p & ~63u, fim = (uint32_t)p + n;
    __asm__ volatile("mfence" ::: "memory");
    for (; a < fim; a += 64u) __asm__ volatile("clflush (%0)" :: "r"(a) : "memory");
    __asm__ volatile("mfence" ::: "memory");
}

static bool iv_preparar(char *m, int nm)
{
    if (!ligado || gen != 9) { snformatar(m, (size_t)nm, "%s", !ligado ? "el blitter no esta en marcha" : "solo Gen9 (D434)"); return false; }
    if (vg_pronto) return true;
    if (!in_esperar()) { snformatar(m, (size_t)nm, "el blitter no termina lo pendiente"); return false; }
    for (uint32_t j = 0; j < VG_PT0 + VG_PT1; j++) {
        if (!vg_pt[j] && !(vg_pt[j] = memfisica_alocar())) { snformatar(m, (size_t)nm, "sin RAM para las tablas"); return false; }
        memset((void *)vg_pt[j], 0, 4096u);
        vg_cache_vaciar((const void *)vg_pt[j], 4096u);
    }
    volatile uint32_t *pd = pp_w + 2u * 1024u;   /* página 2 de la PPGTT: el directorio (intel_ppgtt_monta) */
    for (uint32_t j = 0; j < VG_PT0 + VG_PT1; j++) {
        uint32_t lo, hi, i = j < VG_PT0 ? (VG_GFX0 >> 21) + j : (VG_GFX1 >> 21) + (j - VG_PT0);
        intel_ppgtt_entrada(vg_pt[j], &lo, &hi);
        pd[2u * i + 1u] = hi;
        pd[2u * i] = lo;
    }
    uint32_t pat = rd(VG_PAT_HI);
    if (!vg_pat_tocado) { vg_pat_antes = pat; vg_pat_tocado = true; }
    wr(VG_PAT_HI, (pat & ~0xFF00u) | (VG_PAT5 << 8));
    klog("[ACEL2D] intel exec: volcado: tablas en RAM (%x...), PAT %x: %x -> %x (indice 5 = WB en la LLC)\n", vg_pt[0], VG_PAT_HI,
         pat, rd(VG_PAT_HI));
    memset(vg_r, 0, sizeof vg_r);
    tlb_pendente = true;
    vg_pronto = true;
    return true;
}

static bool iv_mapear(int reg, const void *buf, uint32_t passo, uint32_t larg, uint32_t alt, char *m, int nm)
{
    if (!vg_pronto || reg < 0 || reg > 1) { snformatar(m, (size_t)nm, "volcado sin preparar"); return false; }
    if (!in_esperar()) { snformatar(m, (size_t)nm, "el blitter no termina lo pendiente"); return false; }
    uint32_t *pt = reg ? &vg_pt[VG_PT0] : vg_pt, n_pt = reg ? VG_PT1 : VG_PT0;
    for (uint32_t j = 0; j < n_pt; j++) { memset((void *)pt[j], 0, 4096u); }
    vg_r[reg].npag = 0;
    tlb_pendente = true;
    if (!buf) { for (uint32_t j = 0; j < n_pt; j++) vg_cache_vaciar((const void *)pt[j], 4096u); return true; }
    uint32_t v = (uint32_t)buf, desl = v & 0xFFFu, np = (desl + passo * alt + 4095u) >> 12;
    if (!passo || (passo & 3u) || passo >= 32768u || larg * 4u > passo || np > n_pt * 512u) {
        snformatar(m, (size_t)nm, "el bufer (%u paginas, paso %u) no cabe en la region %d (%u paginas)", np, passo, reg, n_pt * 512u);
        return false;
    }
    for (uint32_t i = 0; i < np; i++) {
        uint32_t fl;
        fis_t f = espaco_traduzir(espaco_kernel(), (v & ~0xFFFu) + i * 4096u, &fl);
        if (!f) { snformatar(m, (size_t)nm, "la pagina %u del bufer no esta mapeada", i); return false; }
        uint32_t *t = (uint32_t *)pt[i / 512u];
        t[2u * (i % 512u)] = (uint32_t)(f & 0xFFFFF000u) | VG_PTE_RO5;
        t[2u * (i % 512u) + 1u] = (uint32_t)((f >> 32) & 0xFFFFu);
    }
    for (uint32_t j = 0; j < n_pt; j++) vg_cache_vaciar((const void *)pt[j], 4096u);
    vg_r[reg].gfx = reg ? VG_GFX1 : VG_GFX0; vg_r[reg].desl = desl; vg_r[reg].passo = passo;
    vg_r[reg].larg = larg; vg_r[reg].alt = alt; vg_r[reg].npag = np;
    klog("[ACEL2D] intel exec: volcado: region %d: %u paginas (%x... -> grafica %x), paso %u, %ux%u\n", reg, np, v, vg_r[reg].gfx + desl,
         passo, larg, alt);
    return true;
}

/* (x, y, l, a) de la región a (dx, dy) de la pantalla (dy puede caer en la zona de prueba). Sin esperar. */
static bool iv_volcar(int reg, int x, int y, int l, int a, int dx, int dy)
{
    if (!vg_pronto || reg < 0 || reg > 1 || !vg_r[reg].npag || !ligado) return false;
    if (x < 0 || y < 0 || l <= 0 || a <= 0 || (uint32_t)(x + l) > vg_r[reg].larg || (uint32_t)(y + a) > vg_r[reg].alt || dx < 0 || dy < 0)
        return false;
    uint32_t b[2 * INTEL_PAL_MAX + 4];
    if (!ex_sitio()) return false;
    int n = ex_dependencia(b, false, 0, 0, dx, dy, l, a);   /* lee la RAM: solo cuenta lo que escribe en la pantalla */
    uint32_t src = vg_r[reg].gfx + vg_r[reg].desl;
    b[n++] = INTEL_XY_SRC_COPY(gen);
    b[n++] = INTEL_BLT_32BPP | INTEL_ROP_SRCCOPY | pitch;
    b[n++] = ((uint32_t)dy << 16) | (uint32_t)dx;
    b[n++] = ((uint32_t)(dy + a) << 16) | (uint32_t)(dx + l);
    b[n++] = fb_gfx; b[n++] = 0;
    b[n++] = ((uint32_t)y << 16) | (uint32_t)x;
    b[n++] = vg_r[reg].passo;
    b[n++] = src; b[n++] = 0;
    if (n & 1) b[n++] = INTEL_MI_NOOP;
    n_ordens++;
    vg_ops++;
    return ex_enviar(b, n);
}

static bool iv_esperar(uint32_t ms) { return ex_despachar(false) && ex_aguardar(ms ? ms : 1); }

static void iv_soltar(void)
{
    if (!vg_pronto) return;
    if (ligado) (void)in_esperar();
    volatile uint32_t *pd = pp_w + 2u * 1024u;
    for (uint32_t j = 0; j < VG_PT0 + VG_PT1; j++) {
        uint32_t i = j < VG_PT0 ? (VG_GFX0 >> 21) + j : (VG_GFX1 >> 21) + (j - VG_PT0);
        pd[2u * i] = 0; pd[2u * i + 1u] = 0;
    }
    tlb_pendente = true;
    if (vg_pat_tocado && mmio) { wr(VG_PAT_HI, (rd(VG_PAT_HI) & ~0xFF00u) | (vg_pat_antes & 0xFF00u)); vg_pat_tocado = false; }
    memset(vg_r, 0, sizeof vg_r);
    vg_pronto = false;
    klog("[ACEL2D] intel exec: volcado: soltado (%u volcados)\n", vg_ops);
}

static void iv_detalle(const char *fase)
{
    klog("[ACEL2D] intel exec volcado (%s): %s; region 0 %u paginas (paso %u, %ux%u), region 1 %u paginas; PAT_HI %x; %u volcados\n",
         fase, vg_pronto ? "preparado" : "sin preparar", vg_r[0].npag, vg_r[0].passo, vg_r[0].larg, vg_r[0].alt, vg_r[1].npag,
         mmio ? rd(VG_PAT_HI) : 0, vg_ops);
}

static const acel_volcado_t intel_volcado = { iv_preparar, iv_mapear, iv_volcar, iv_esperar, iv_soltar, iv_detalle, NULL, NULL, NULL };

static void in_desligar(void)
{
    if (ligado) (void)in_esperar();   /* lo pendiente sale y termina (con plazo) antes de soltar el motor */
    if (ligado) klog("[ACEL2D] intel exec: parado (el contexto queda sin trabajo)\n");
    ligado = false;
    if (rps) { intel_rps_soltar(INTEL_RPS_2D); rps = false; }
    if (vg_pronto) {   /* el motor ya no lee: las tablas fuera de la PPGTT y el PAT como estaba */
        if (vg_pat_tocado && mmio) { wr(VG_PAT_HI, (rd(VG_PAT_HI) & ~0xFF00u) | (vg_pat_antes & 0xFF00u)); vg_pat_tocado = false; }
        vg_pronto = false;
    }
    fw_soltar();
}

/* ------------------------------------------------------------- arranque */

/* Pone el motor en modo execlists y prepara la zona. Antes, todo lo que se puede leer. */
static bool in_iniciar(const pci_disp_t *d, int geracao, const acel_tela_t *t, char *m, int nm)
{
    const char *nome = NULL;
    vg_pronto = false;   /* la PPGTT se vuelve a montar entera: el volcado, si lo había, se prepara otra vez */
    tlb_pendente = false;
    var = intel_variante_exec(d->modelo, &nome);
    if (!var || intel_var_geracao(var) != geracao) {
        snformatar(m, (size_t)nm, "paso 1: el modelo %04x no esta en la lista de execlists de Gen%d", d->modelo, geracao);
        return false;
    }
    gen = geracao;
    elsq = gen >= 11;
    uint32_t bar0 = pci_le32(d, 0x10), gmadr = pci_le32(d, 0x18);
    if ((((bar0 >> 1) & 3) == 2 && pci_le32(d, 0x14)) || (((gmadr >> 1) & 3) == 2 && pci_le32(d, 0x1C))) {
        snformatar(m, (size_t)nm, "paso 1: BAR por encima de 4 GB (MMIO %x%08x, apertura %x%08x)", pci_le32(d, 0x14), bar0,
                   pci_le32(d, 0x1C), gmadr);
        return false;
    }
    bar0 &= ~0xFu; gmadr &= ~0xFu;
    klog("[ACEL2D] intel exec: paso 1: %s (%04x), Gen%d, MMIO %x, apertura %x, pantalla %x %dx%d pitch %u, %u KB del VBE/GOP\n",
         nome ? nome : "?", d->modelo, gen, bar0, gmadr, t->fb_fisico, t->larg, t->alt, t->pitch, t->memoria >> 10);
    if (!bar0 || !gmadr) { snformatar(m, (size_t)nm, "paso 1: BAR sin asignar (MMIO %x, apertura %x)", bar0, gmadr); return false; }
    if (t->fb_fisico < gmadr || t->fb_fisico - gmadr >= EX_LIMITE_APERTURA) {
        snformatar(m, (size_t)nm, "paso 1: la pantalla (%x) no esta en la apertura (%x)", t->fb_fisico, gmadr);
        return false;
    }
    if (t->bytes_px != 4) { snformatar(m, (size_t)nm, "paso 1: la pantalla no es de 32 bits"); return false; }
    fb_gfx = t->fb_fisico - gmadr;
    pitch = t->pitch;
    if (fb_gfx & 0xFFFu) { snformatar(m, (size_t)nm, "paso 1: la pantalla no empieza en una pagina (grafica %x)", fb_gfx); return false; }
    if (pitch & 63 || pitch >= 32768 || pitch < (uint32_t)t->larg * 4) {
        snformatar(m, (size_t)nm, "paso 1: pitch %u no valido para el blitter (multiplo de 64, menos de 32 KB)", pitch);
        return false;
    }
    /* Lo que toca el blitter (pantalla y zona de prueba) va en la PPGTT; detrás, la zona del driver. */
    ring_off = intel_anel_desloc(pitch, t->alt, ACEL2D_ZONA_A);
    n_pt = intel_ppgtt_n_pt(fb_gfx, fb_gfx + ring_off);
    if (n_pt > EX_PT_MAX) {
        snformatar(m, (size_t)nm, "paso 1: la pantalla necesita %u tablas de la PPGTT (maximo %u)", n_pt, EX_PT_MAX);
        return false;
    }
    area_bytes = EX_OFF_PPGTT + (3u + n_pt) * 4096u;
    uint32_t fim = ring_off + area_bytes;
    if (fb_gfx + fim > EX_LIMITE_APERTURA) {
        snformatar(m, (size_t)nm, "paso 1: la zona no cabe en la apertura (grafica %x + %u KB)", fb_gfx, fim >> 10);
        return false;
    }
    if (!mmio || mmio_fis != bar0) {
        mmio = (volatile uint8_t *)janela_mapear_tipo(bar0, 2 * 1024 * 1024, PG_PCD | PG_PWT);
        mmio_fis = bar0;
    }
    if (!mmio) { snformatar(m, (size_t)nm, "paso 1: sin ventana para el MMIO"); return false; }
    if (plano_da_tela(fb_gfx, pitch, m, nm) != INTEL_PLANO_OK) return false;
    if (!superficies_livres(fb_gfx + pitch * (uint32_t)t->alt, fb_gfx + fim, m, nm)) return false;
    if (!gtt_preparar(d, bar0, t, fim, m, nm)) return false;

    /* A partir de aquí se escribe: bus maestro (la GPU lee la zona) y forcewake. */
    uint32_t cmd = pci_le32(d, 0x04);
    if ((cmd & 0x06) != 0x06) pci_escreve32(d, 0x04, (cmd & 0xFFFFu) | 0x06);
    if (!area || area_fis != t->fb_fisico + ring_off) {
        /* Tres ventanas sin solaparse: contexto y página de estado sin caché (la CPU los lee), el anillo en WC si
         * la PAT lo permite (wc.c; la CPU solo lo escribe) y la PPGTT sin caché. */
        uint32_t f = t->fb_fisico + ring_off;
        anel_wc = txt_compara(wc_via(), "PAT") == 0;
        area = (volatile uint32_t *)janela_mapear_tipo(f, EX_OFF_RING, PG_PCD | PG_PWT);
        anel_w = (volatile uint32_t *)janela_mapear_tipo(f + EX_OFF_RING, EX_RING_BYTES, anel_wc ? PG_PWT : (PG_PCD | PG_PWT));
        pp_w = (volatile uint32_t *)janela_mapear_tipo(f + EX_OFF_PPGTT, EX_AREA_MAX - EX_OFF_PPGTT, PG_PCD | PG_PWT);
        if (!anel_w || !pp_w) area = NULL;
        area_fis = f;
        motor_nosso = false;
    }
    if (!area) { snformatar(m, (size_t)nm, "paso 1: sin ventana para la zona de execlists"); return false; }
    area_gfx = fb_gfx + ring_off;
    if (!fw_pegar(m, nm)) return false;

    /* Paso 5: el motor sin usar (nada de otro contexto ni del modo heredado), salvo que lo dejáramos
     * nosotros en un arranque anterior («acel2d probar» vuelve a iniciar). EXECLIST_STATUS solo se
     * mira en Gen9: para Gen11+ i915 no documenta sus bits. */
    uint32_t stat = rd(EX_BCS_BASE + EX_RING_EXEC_STAT), ctl = rd(EX_BCS_BASE + EX_RING_CTL);
    klog("[ACEL2D] intel exec: paso 5: antes: EXECLIST_STATUS %x, RING_CTL %x, GFX_MODE %x, MI_MODE %x, CSB %x%s\n", stat,
         ctl, rd(EX_BCS_BASE + EX_RING_MODE), rd(EX_BCS_BASE + EX_RING_MI_MODE), rd(EX_BCS_BASE + EX_RING_CSB_PTR),
         motor_nosso ? " (lo dejamos nosotros)" : "");
    if (!motor_nosso && ((gen < 11 && intel_exec_ocupado(stat)) || (ctl & 1u))) {
        snformatar(m, (size_t)nm, "paso 5: el blitter ya esta en uso (STATUS %x, CTL %x): no se toca", stat, ctl);
        fw_soltar();
        return false;
    }

    /* La zona a 0 (contexto, página de estado, anillo y PPGTT), con la CPU. */
    for (uint32_t i = 0; i < EX_OFF_RING / 4; i++) area[i] = 0;
    for (uint32_t i = 0; i < EX_RING_BYTES / 4; i++) anel_w[i] = 0;
    for (uint32_t i = 0; i < (area_bytes - EX_OFF_PPGTT) / 4; i++) pp_w[i] = 0;
    __asm__ volatile("lock; orl $0, (%%esp)" ::: "memory");
    (void)rd(EX_BCS_BASE + EX_RING_HEAD);

    /* El índice 3 del PAT (el de la PPGTT) sin caché, como i915. */
    uint32_t pat_reg = intel_pat3_reg(gen), pat_antes = rd(pat_reg);
    wr(pat_reg, intel_pat3_valor(gen, pat_antes));
    klog("[ACEL2D] intel exec: paso 5: PAT %x: %x -> %x\n", pat_reg, pat_antes, rd(pat_reg));

    /* Modo execlists, página de estado y CSB, como enable_execlists() y reset_csb_pointers() de i915. */
    wr(EX_BCS_BASE + EX_RING_HWSTAM, 0xFFFFFFFFu);
    wr(EX_BCS_BASE + EX_RING_MODE, EX_MASCARA_ON(gen >= 11 ? EX_GFX_MODE_SEM_LEGADO : EX_GFX_MODE_LISTA));
    wr(EX_BCS_BASE + EX_RING_MI_MODE, EX_MASCARA_OFF(EX_STOP_RING));
    wr(EX_BCS_BASE + EX_RING_HWS_PGA, area_gfx + EX_OFF_HWS);
    (void)rd(EX_BCS_BASE + EX_RING_HWS_PGA);
    wr(EX_BCS_BASE + EX_RING_CSB_PTR, intel_csb_reiniciar(gen));
    (void)rd(EX_BCS_BASE + EX_RING_CSB_PTR);
    for (uint32_t i = 0; i < 2u * intel_csb_tam(gen); i++) hws()[EX_CSB_DWORD + i] = EX_CSB_VAZIA;
    wr(EX_BCS_BASE + EX_RING_CSB_PTR, intel_csb_reiniciar(gen));
    csb_lido = intel_csb_tam(gen) - 1u;
    motor_nosso = true;

    /* Paso 6: la PPGTT (pantalla y zona de prueba) y la imagen de registros del contexto. */
    uint64_t f_ppgtt = fis_tela + ring_off + EX_OFF_PPGTT;
    if (!intel_ppgtt_monta((uint32_t *)pp_w, n_pt, f_ppgtt, fb_gfx, fb_gfx + ring_off, fis_tela) ||
        !intel_lrc_monta((uint32_t *)pag_estado(), 1024, gen, EX_BCS_BASE, 0, 0, area_gfx + EX_OFF_RING,
                         ((EX_RING_BYTES - 4096u) & 0x001FF000u) | 1u, f_ppgtt)) {
        snformatar(m, (size_t)nm, "paso 6: la PPGTT o la imagen del contexto no se pueden montar (Gen%d)", gen);
        fw_soltar();
        return false;
    }
    klog("[ACEL2D] intel exec: paso 6: contexto en %x (pagina de estado %x, anillo %x), PPGTT de %u tablas en la fisica "
         "%x%08x, %s, CSB %x\n", area_gfx, area_gfx + EX_OFF_HWS, area_gfx + EX_OFF_RING, n_pt, (uint32_t)(f_ppgtt >> 32),
         (uint32_t)f_ppgtt, elsq ? "ELSQ" : "ELSP", rd(EX_BCS_BASE + EX_RING_CSB_PTR));

    ligado = true;
    envios = 0;
    /* Paso 7: una tanda con solo el vaciado y la marca. Si no termina, se para. */
    uint32_t vacio[4];
    int nv = intel_cmd_flush(vacio, gen);
    ocupado = false; n_pend = 0;
    if (!ex_enviar(vacio, nv) || !in_esperar()) {
        uint32_t stat2 = rd(EX_BCS_BASE + EX_RING_EXEC_STAT);
        in_desligar();
        snformatar(m, (size_t)nm, "paso 7: la tanda de prueba no termina (marca %x de %x, STATUS %x)", hws()[EX_MARCA_DWORD],
                   marca, stat2);
        return false;
    }
    klog("[ACEL2D] intel exec: paso 7: el blitter ejecuta tandas (marca %x, CSB %x, STAT %x); sigue la autoprueba (paso 8)\n",
         hws()[EX_MARCA_DWORD], rd(EX_BCS_BASE + EX_RING_CSB_PTR), rd(EX_BCS_BASE + EX_RING_EXEC_STAT));
    /* D434: sin i915 la GPU se queda en la frecuencia del firmware (100 MHz en la UHD 620): el gobernador común con el 3D
     * pide RP0 mientras hay trabajo (solo Gen9: los registros de Gen11/12 no se han comprobado). */
    rps = gen == 9 && intel_rps_ligar(mmio, INTEL_RPS_2D);
    n_flush_postos = n_flush_poupados = n_ordens = 0;
    klog("[ACEL2D] intel exec: paso 7: anillo %s; frecuencia %s\n", anel_wc ? "en WC" : "sin cache (UC)",
         rps ? "con RP0 al trabajar (intel_rps.c)" : "sin tocar");
    return true;
}

/* «acel2d probar --detalle»: el estado del motor y de los puertos para el log. */
static void in_detalle(const char *fase)
{
    if (!mmio) return;
    klog("[ACEL2D] intel exec detalle (%s): Gen%d CTL %x HEAD %x TAIL %x START %x MI_MODE %x GFX_MODE %x\n", fase, gen,
         rd(EX_BCS_BASE + EX_RING_CTL), rd(EX_BCS_BASE + EX_RING_HEAD), rd(EX_BCS_BASE + EX_RING_TAIL),
         rd(EX_BCS_BASE + EX_RING_START), rd(EX_BCS_BASE + EX_RING_MI_MODE), rd(EX_BCS_BASE + EX_RING_MODE));
    klog("[ACEL2D] intel exec detalle (%s): EXECLIST_STATUS %x, CSB ptr %x, envios %u, marca %x de %x, zona %x (grafica %x)\n",
         fase, rd(EX_BCS_BASE + EX_RING_EXEC_STAT), rd(EX_BCS_BASE + EX_RING_CSB_PTR), envios,
         area ? hws()[EX_MARCA_DWORD] : 0, marca, area_fis, area_gfx);
    klog("[ACEL2D] intel exec detalle (%s): forcewake GT %x = %x, acuse %x = %x, PAT %x = %x\n", fase, EX_FW_GT,
         rd(EX_FW_GT), EX_FW_GT_ACK, rd(EX_FW_GT_ACK), intel_pat3_reg(gen), rd(intel_pat3_reg(gen)));
    {
        char fr[200];
        intel_rps_texto(fr, sizeof fr);
        klog("[ACEL2D] intel exec detalle (%s): %u ordenes, %u vaciados entre ordenes dependientes y %u ahorrados (independientes); "
             "anillo %s; %s%s\n", fase, n_ordens, n_flush_postos, n_flush_poupados, anel_wc ? "en WC" : "UC", fr,
             rps ? "" : " (el 2D no la pide)");
    }
    if (plano_tela >= 0)
        klog("[ACEL2D] intel exec detalle (%s): plano %c CTL %x SURF %x STRIDE %x; pantalla %x pitch %u\n", fase,
             'A' + plano_tela, rd(INTEL_DSPCNTR(plano_tela)), rd(INTEL_DSPSURF(plano_tela)), rd(INTEL_DSPSTRIDE(plano_tela)),
             fb_gfx, pitch);
}

const acel_driver_t acel_intel_exec = {
    "intel", ACEL2D_INTEL, in_iniciar, in_copiar, in_preencher, in_esperar, NULL, in_desligar,
    true,    /* la zona va debajo de la pantalla visible */
    0,       /* sin mínimo: cada tanda cuesta un envío completo */
    EX_ASINCRONO != 0,   /* D438: asíncrono: las órdenes se juntan y la capa espera (acel_sincronizar) antes de que la CPU escriba */
    in_detalle,
    false,   /* solo 32 bits */
    &intel_volcado,   /* D434: volcado del búfer de atrás por el blitter (solo Gen9; la capa lo prueba antes de usarlo) */
};
