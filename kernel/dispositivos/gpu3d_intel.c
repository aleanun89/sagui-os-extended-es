// SPDX-License-Identifier: GPL-2.0-or-later
/* gpu3d_intel.c - 3D por la GPU en Intel Gen3: 915, 945, G33 y Pineview (D285).
 *
 * SIN PROBAR EN HARDWARE: QEMU no emula ninguna GPU de Intel. La capa común
 * (kernel/gui/gpu3d.c) solo lo usa si su autoprueba da los píxeles esperados y
 * a tiempo; si no, SaguiGL sigue por software. Las órdenes las construye
 * gen3_pacotes.h, que sí se prueba en el anfitrión (tests/gen3_3d_unidad.c).
 *
 * Qué usa:
 *   - el ring de órdenes que ya pone en marcha el blitter 2D (acel_intel.c): en
 *     Gen3 es el mismo para 2D y 3D. Si el 2D quedó en la CPU por ser más lento,
 *     el 3D enciende el ring él mismo; si el 2D falló, no se toca (salvo
 *     --gpu3d=intel);
 *   - memoria: solo la que el VBE dejó en el GTT (la «robada» que anuncia), entre
 *     la zona de prueba del 2D y el ring. No se escribe el GTT ni ningún registro
 *     de pantalla, de relojes o de energía. Ahí van el destino de color
 *     (ARGB8888), el Z (24 bits, o 16 si no cabe), las texturas y el lote;
 *   - órdenes: por lotes (MI_BATCH_BUFFER_START con dirección del GTT y sin
 *     privilegios, a un búfer de la zona; lo normal) o escritas en el propio ring («gpu3d encender anillo»), para
 *     poder comparar los dos caminos en la máquina;
 *   - cada entrega lleva el estado completo (fijo, destino, Z/mezcla, textura y
 *     programa de píxeles) y los triángulos: no depende de lo que hubiera antes
 *     ni de lo que el 2D haga entre medias;
 *   - limpiar el color y el Z: con el blitter (XY_COLOR_BLT), que es lo que ya
 *     prueba el 2D;
 *   - vallas: el número de entrega; una valla está cumplida cuando HEAD alcanza
 *     a TAIL (cada entrega acaba en MI_FLUSH). Antes de reutilizar el búfer del
 *     lote se espera a la anterior.
 *
 * El cuadro vuelve a la RAM leyendo la memoria de vídeo por la apertura (lento:
 * es memoria sin caché). La medida de «gpu3d probar» lo da aparte.
 *
 * Qué hay que comprobar en el hardware: docs/gpu3d.md. */
#include "gpu3d.h"
#include "acel_intel.h"
#include "console.h"
#include "cpu.h"
#include "memoria.h"
#include "paginacao.h"
#include "texto.h"
#include "gen3_pacotes.h"

#define LOTE_BYTES      (64 * 1024)
#define LOTE_PAL        (LOTE_BYTES / 4 - 8)
#define ANEL_PAL        6000           /* órdenes en el anillo: menos de la mitad del ring (64 KB) */
#define ARENA_MAX       (24u * 1024 * 1024)

/* Solo estos modelos (los de Gen3 de la tabla del 2D). */
static const uint16_t modelos[] = { 0x2582, 0x2592, 0x2772, 0x27A2, 0x27AE, 0x29B2, 0x29C2, 0x29D2, 0xA001, 0xA011 };

static g3d_arena_t arena;
static g3d_buffer_t b_lote, b_cor, b_z, b_tex[GPU3D_TEXTURAS];
static g3_alvo_t alvo;
static uint32_t *prep;                 /* la cola se prepara en RAM */
static uint32_t arena_fis;
static uint8_t *arena_cpu;
static uint32_t arena_mapeada;
static bool pronto, por_anel, anel_meu;
static g3d_vala_t vala_emitida;
static pci_disp_t placa;
static acel_tela_t tela;

static bool modelo_ok(uint16_t m)
{
    for (unsigned i = 0; i < sizeof modelos / sizeof modelos[0]; i++)
        if (modelos[i] == m) return true;
    return false;
}

static void i3_detalle(const char *fase)
{
    intel_anel_t a;
    if (!intel_anel_info(&a)) { klog("[GPU3D] intel %s: el ring no esta en marcha\n", fase); return; }
    /* IPEIR 0x2088, IPEHR 0x208C, INSTDONE 0x2090, ACTHD 0x20C8, EIR 0x20B0, ESR 0x20B8, INSTPM 0x20C0, PGTBL_ER 0x2024 */
    klog("[GPU3D] intel %s: CTL %x HEAD %x TAIL %x; IPEIR %x IPEHR %x INSTDONE %x ACTHD %x\n", fase, a.ctl, a.head, a.tail,
         intel_anel_reg(0x2088), intel_anel_reg(0x208C), intel_anel_reg(0x2090), intel_anel_reg(0x20C8));
    klog("[GPU3D] intel %s: EIR %x ESR %x INSTPM %x PGTBL_ER %x; vallas emitidas %u; %s\n", fase, intel_anel_reg(0x20B0),
         intel_anel_reg(0x20B8), intel_anel_reg(0x20C0), intel_anel_reg(0x2024), vala_emitida,
         por_anel ? "ordenes en el anillo" : "lotes");
    klog("[GPU3D] intel %s: zona %x+%u KB (libres %u KB); lote %x; color %x (%dx%d, paso %u); Z%d %x (paso %u)\n", fase,
         (uint32_t)arena.gpu, (uint32_t)(arena.bytes >> 10), (uint32_t)(g3d_arena_livre(&arena) >> 10), (uint32_t)b_lote.gpu,
         alvo.cor_gpu, alvo.larg, alvo.alt, alvo.cor_passo, alvo.zbits, alvo.z_gpu, alvo.z_passo);
}

static void i3_desligar(void)
{
    if (!pronto && !anel_meu) return;
    pronto = false;
    if (anel_meu) {
        /* El ring lo encendió el 3D: se para (acel_intel avisa a la capa; ya no hay nada que apagar). */
        anel_meu = false;
        if (acel_intel.desligar) acel_intel.desligar();
    }
    if (prep) { kfree(prep); prep = NULL; }
    klog("[GPU3D] intel: 3D apagado\n");
}

static int i3_iniciar(const pci_disp_t *d, int geracao, const acel_tela_t *t, uint32_t variante, char *m, int nm)
{
    pronto = false;
    if (d->fabricante != 0x8086 || geracao != 3 || !modelo_ok(d->modelo)) {
        snformatar(m, (size_t)nm, "%04x:%04x no es una Intel Gen3 de la lista", d->fabricante, d->modelo);
        return G3D_NAO;
    }
    placa = *d; tela = *t;
    por_anel = (variante & GPU3D_CAB_ANEL) != 0;
    intel_anel_t a;
    if (!intel_anel_info(&a)) {
        /* El 2D no lo tiene encendido (más lento que la CPU, o apagado a mano): lo enciende el 3D. */
        char m2[96] = "";
        klog("[GPU3D] intel: el ring del 2D no esta en marcha: lo enciende el 3D\n");
        uint32_t cmd = pci_le32(d, 0x04);
        if (!acel_intel.iniciar(d, geracao, t, m2, sizeof m2)) {
            pci_escreve32(d, 0x04, cmd & 0xFFFFu);
            snformatar(m, (size_t)nm, "el ring no arranca: %s", m2);
            return G3D_FALHA;
        }
        anel_meu = true;
        if (!intel_anel_info(&a)) { snformatar(m, (size_t)nm, "el ring no da sus datos"); i3_desligar(); return G3D_FALHA; }
    }
    if (a.geracao != 3) { snformatar(m, (size_t)nm, "el ring es de Gen%d", a.geracao); return G3D_NAO; }
    /* La zona: página a página, y con sitio al menos para el lote y un destino pequeño. */
    uint32_t off = (a.arena_off + 0xFFFu) & ~0xFFFu;
    uint32_t fim = (a.arena_off + a.arena_bytes) & ~0xFFFu;
    uint32_t bytes = fim > off ? fim - off : 0;
    if (bytes > ARENA_MAX) bytes = ARENA_MAX;
    klog("[GPU3D] intel: %04x:%04x Gen3; zona libre de la memoria del VBE: %u KB en %x (grafica %x); %s\n", d->fabricante,
         d->modelo, bytes >> 10, a.fb_fisico + off, a.fb_gfx + off, por_anel ? "ordenes en el anillo" : "lotes");
    if (bytes < LOTE_BYTES + 512 * 1024) {
        snformatar(m, (size_t)nm, "sin memoria de video libre para el 3D (%u KB tras la pantalla)", bytes >> 10);
        i3_desligar();
        return G3D_NAO;
    }
    if (!arena_cpu || arena_fis != a.fb_fisico + off || arena_mapeada < bytes) {
        arena_cpu = (uint8_t *)janela_mapear_tipo(a.fb_fisico + off, bytes, PG_PCD | PG_PWT);
        arena_fis = a.fb_fisico + off;
        arena_mapeada = bytes;
    }
    if (!arena_cpu) { snformatar(m, (size_t)nm, "sin ventana del kernel para %u KB de memoria de video", bytes >> 10); i3_desligar(); return G3D_NAO; }
    g3d_arena_iniciar(&arena, a.fb_gfx + off, arena_cpu, bytes);
    memset(&b_lote, 0, sizeof b_lote); memset(&b_cor, 0, sizeof b_cor); memset(&b_z, 0, sizeof b_z);
    memset(b_tex, 0, sizeof b_tex); memset(&alvo, 0, sizeof alvo);
    if (!prep) prep = kmalloc(LOTE_BYTES);
    if (!prep || !g3d_buffer_criar(&arena, &b_lote, G3D_BUF_LOTE, LOTE_BYTES, 4096)) {
        snformatar(m, (size_t)nm, "sin memoria para la cola de ordenes");
        i3_desligar();
        return G3D_NAO;
    }
    vala_emitida = 0;
    /* El ring tiene que estar quieto y avanzar con una orden vacía antes de mandar 3D. */
    bool sim = int_salva_desliga();
    bool ok = intel_anel_reservar(INTEL_ANEL_PAL_ENVIO);
    if (ok) intel_anel_enviar(false);
    int_restaura(sim);
    if (!ok || !intel_anel_esperar(250)) {
        snformatar(m, (size_t)nm, "el ring no avanza con una orden vacia");
        i3_detalle("al iniciar");
        i3_desligar();
        return G3D_FALHA;
    }
    pronto = true;
    return G3D_OK;
}

static int i3_esperar(g3d_vala_t vala, uint32_t ms)
{
    (void)vala;   /* una sola cola: cumplida cuando el ring queda vacío */
    if (!pronto) return G3D_FALHA;
    if (intel_anel_esperar(ms)) return G3D_OK;
    i3_detalle("tiempo agotado");
    return G3D_FALHA;
}

/* Entrega lo preparado en «prep» (n palabras): por lote o en el anillo. */
static int entregar(uint32_t n, g3d_vala_t *vala)
{
    if (!por_anel) {
        /* El búfer del lote se reutiliza: la entrega anterior tiene que haber terminado. */
        if (i3_esperar(vala_emitida, GPU3D_PRAZO_MS) != G3D_OK) return G3D_FALHA;
        prep[n++] = G3_MI_LOTE_FIM;
        if (n & 1) prep[n++] = G3_MI_NOOP;
        uint32_t *dst = (uint32_t *)b_lote.cpu;
        for (uint32_t i = 0; i < n; i++) dst[i] = prep[i];
        __asm__ volatile("lock; orl $0, (%%esp)" ::: "memory");
    }
    bool sim = int_salva_desliga();
    bool ok;
    if (!por_anel) {
        ok = intel_anel_reservar(2 + INTEL_ANEL_PAL_ENVIO);
        if (ok) {
            intel_anel_emitir(G3_MI_LOTE_INICIO | G3_LOTE_GTT);
            intel_anel_emitir((uint32_t)b_lote.gpu | G3_LOTE_NAO_SEGURO);
        }
    } else {
        ok = intel_anel_reservar(n + INTEL_ANEL_PAL_ENVIO);
        if (ok) for (uint32_t i = 0; i < n; i++) intel_anel_emitir(prep[i]);
    }
    if (ok) intel_anel_enviar(true);
    int_restaura(sim);
    if (!ok) { i3_detalle("sin sitio en el ring"); return G3D_FALHA; }
    vala_emitida++;
    if (!vala_emitida) vala_emitida = 1;
    if (vala) *vala = vala_emitida;
    return G3D_OK;
}

static void soltar_alvo(void)
{
    g3d_buffer_soltar(&arena, &b_cor);
    g3d_buffer_soltar(&arena, &b_z);
    memset(&alvo, 0, sizeof alvo);
}

static int i3_alvo(int larg, int alt, int zbits)
{
    if (!pronto) return G3D_FALHA;
    if (larg < 8 || alt < 8 || larg > GPU3D_MAX_LADO || alt > GPU3D_MAX_LADO) return G3D_NAO;
    if (i3_esperar(vala_emitida, GPU3D_PRAZO_MS) != G3D_OK) return G3D_FALHA;
    soltar_alvo();
    uint32_t passo = ((uint32_t)larg * 4 + 63) & ~63u;
    if (!g3d_buffer_criar(&arena, &b_cor, G3D_BUF_ALVO, (uint64_t)passo * (uint32_t)alt, 4096)) return G3D_NAO;
    /* Z de 24 bits si cabe; si no (o si se pidió 16), de 16. */
    int zb = zbits == 16 ? 16 : 24;
    uint32_t zp = zb == 24 ? passo : (((uint32_t)larg * 2 + 63) & ~63u);
    if (!g3d_buffer_criar(&arena, &b_z, G3D_BUF_Z, (uint64_t)zp * (uint32_t)alt, 4096)) {
        zb = 16; zp = ((uint32_t)larg * 2 + 63) & ~63u;
        if (!g3d_buffer_criar(&arena, &b_z, G3D_BUF_Z, (uint64_t)zp * (uint32_t)alt, 4096)) { soltar_alvo(); return G3D_NAO; }
    }
    b_cor.passo = passo; b_cor.larg = larg; b_cor.alt = alt; b_cor.bpp = 32;
    b_z.passo = zp; b_z.larg = larg; b_z.alt = alt; b_z.bpp = zb == 24 ? 32 : 16;
    alvo.cor_gpu = (uint32_t)b_cor.gpu; alvo.cor_passo = passo;
    alvo.z_gpu = (uint32_t)b_z.gpu; alvo.z_passo = zp;
    alvo.larg = larg; alvo.alt = alt; alvo.zbits = zb;
    klog("[GPU3D] intel: destino %dx%d: color en %x (paso %u), Z de %d bits en %x (paso %u); libres %u KB\n", larg, alt,
         alvo.cor_gpu, passo, zb, alvo.z_gpu, zp, (uint32_t)(g3d_arena_livre(&arena) >> 10));
    return G3D_OK;
}

static int i3_limpar(uint32_t mascara, uint32_t cor)
{
    if (!pronto || !b_cor.usado) return G3D_FALHA;
    g3d_fila_t f;
    g3d_fila_iniciar(&f, prep, 16);
    if (mascara & 1) g3_blt_preencher(&f, alvo.cor_gpu, alvo.cor_passo, alvo.larg, alvo.alt, 32, cor & 0x00FFFFFFu);
    if (mascara & 2) g3_blt_preencher(&f, alvo.z_gpu, alvo.z_passo, alvo.larg, alvo.alt, alvo.zbits == 24 ? 32 : 16,
                                      alvo.zbits == 24 ? 0x00FFFFFFu : 0xFFFFu);
    if (!f.n) return G3D_OK;
    /* El blitter va siempre por el ring (como el 2D), detrás de lo que haya pendiente. */
    bool sim = int_salva_desliga();
    bool ok = intel_anel_reservar(f.n + INTEL_ANEL_PAL_ENVIO);
    if (ok) { for (uint32_t i = 0; i < f.n; i++) intel_anel_emitir(prep[i]); intel_anel_enviar(true); }
    int_restaura(sim);
    if (!ok) { i3_detalle("sin sitio para limpiar"); return G3D_FALHA; }
    vala_emitida++;
    return G3D_OK;
}

static int i3_textura(int vaga, int larg, int alt, const uint32_t *argb)
{
    if (!pronto) return G3D_FALHA;
    if (vaga < 0 || vaga >= GPU3D_TEXTURAS) return G3D_NAO;
    /* Puede estar en uso por una entrega pendiente. */
    if (i3_esperar(vala_emitida, GPU3D_PRAZO_MS) != G3D_OK) return G3D_FALHA;
    g3d_buffer_t *b = &b_tex[vaga];
    g3d_buffer_soltar(&arena, b);
    if (!argb) return G3D_OK;
    if (larg < 1 || alt < 1 || larg > GPU3D_MAX_LADO || alt > GPU3D_MAX_LADO) return G3D_NAO;
    uint32_t passo = ((uint32_t)larg * 4 + 63) & ~63u;
    if (!g3d_buffer_criar(&arena, b, G3D_BUF_TEXTURA, (uint64_t)passo * (uint32_t)alt, 4096)) return G3D_NAO;
    b->passo = passo; b->larg = larg; b->alt = alt; b->bpp = 32;
    for (int y = 0; y < alt; y++) {
        uint32_t *dst = (uint32_t *)(b->cpu + (uint32_t)y * passo);
        for (int x = 0; x < larg; x++) dst[x] = argb[y * larg + x];
    }
    __asm__ volatile("lock; orl $0, (%%esp)" ::: "memory");
    return G3D_OK;
}

static int i3_desenhar(const gpu3d_estado_t *e, const gpu3d_vert_t *v, int n, g3d_vala_t *vala)
{
    if (!pronto || !b_cor.usado) return G3D_FALHA;
    g3_tex_t tex, *pt = NULL;
    if (e->flags & G3D_TEXTURA) {
        const g3d_buffer_t *b = e->textura < GPU3D_TEXTURAS ? &b_tex[e->textura] : NULL;
        if (!b || !b->usado) return G3D_NAO;
        /* Repetir solo vale con lados potencia de 2 en Gen3. */
        bool pot = !(b->larg & (b->larg - 1)) && !(b->alt & (b->alt - 1));
        if (!pot && (e->flags & (G3D_TEX_REPETE_S | G3D_TEX_REPETE_T))) return G3D_NAO;
        tex.gpu = (uint32_t)b->gpu; tex.passo = b->passo; tex.larg = b->larg; tex.alt = b->alt;
        pt = &tex;
    }
    uint32_t max_pal = por_anel ? ANEL_PAL : LOTE_PAL;
    int por_vez = (int)((max_pal - G3_PAL_ESTADO - 4) / 8);
    por_vez -= por_vez % 3;
    for (int i = 0; i < n; i += por_vez) {
        int q = n - i < por_vez ? n - i : por_vez;
        g3d_fila_t f;
        g3d_fila_iniciar(&f, prep, max_pal);
        g3_estado_fixo(&f);
        g3_estado_alvo(&f, &alvo);
        g3_estado_desenho(&f, e, pt);
        g3_triangulos(&f, v + i, q);
        if (f.n & 1) g3d_por(&f, G3_MI_NOOP);
        if (f.transbordou) return G3D_FALHA;
        int r = entregar(f.n, vala);
        if (r != G3D_OK) return r;
    }
    return G3D_OK;
}

static int i3_ler(uint32_t *dest, int passo_px)
{
    if (!pronto || !b_cor.usado) return G3D_FALHA;
    if (i3_esperar(vala_emitida, GPU3D_PRAZO_MS) != G3D_OK) return G3D_FALHA;
    for (int y = 0; y < alvo.alt; y++) {
        const uint32_t *src = (const uint32_t *)(b_cor.cpu + (uint32_t)y * alvo.cor_passo);
        uint32_t *d = dest + y * passo_px;
        for (int x = 0; x < alvo.larg; x++) d[x] = src[x] & 0x00FFFFFFu;
    }
    return G3D_OK;
}

static void i3_memoria(uint32_t *total_kb, uint32_t *livre_kb)
{
    *total_kb = pronto ? (uint32_t)(arena.bytes >> 10) : 0;
    *livre_kb = pronto ? (uint32_t)(g3d_arena_livre(&arena) >> 10) : 0;
}

const gpu3d_driver_t gpu3d_intel = {
    "intel-gen3", i3_iniciar, i3_alvo, i3_limpar, i3_textura, i3_desenhar, i3_esperar, i3_ler, i3_desligar, i3_detalle, i3_memoria,
};
