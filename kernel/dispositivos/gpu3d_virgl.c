// SPDX-License-Identifier: GPL-3.0-only
/* gpu3d_virgl.c - el 3D de gpu3d por VirGL (D474): la GPU del anfitrión en QEMU y Proxmox.
 *
 * Con una virtio-gpu que negoció VIRGL (D472, D473) la capa gpu3d (kernel/gui/gpu3d.c) elige este driver y lo
 * somete a su autoprueba de siempre (las ocho pruebas contra lo que debe salir, píxel a píxel). Todo va por el
 * contexto 3D número 2 de virglrenderer, con el codificador de virgl.c:
 *   - destino: un recurso 2D B8G8R8A8 con respaldo en RAM (para leerlo con TRANSFER_FROM_HOST_3D) y un Z sin
 *     respaldo (Z24_UNORM_S8_UINT o Z16_UNORM), cada uno con su objeto «surface», en el framebuffer;
 *   - vértices: los gpu3d_vert_t tal cual (32 bytes) se escriben dentro del mismo SUBMIT_3D que los dibuja
 *     (RESOURCE_INLINE_WRITE) en un búfer de vértices de 1 MB que se recorre en anillo: una sola ida y vuelta
 *     por la cola en cada entrega;
 *   - sombreadores TGSI en texto: el de vértices pasa de píxeles a coordenadas de recorte (con w = 1/rhw, así la
 *     GPU interpola con perspectiva) con dos constantes que cambian con el destino; dos de fragmentos (color y
 *     color por textura), los dos con el especular sumado al final;
 *   - estado: mezcla, Z y muestreo se crean la primera vez que hacen falta y se guardan (cada combinación de v1
 *     es un objeto; nunca se destruyen hasta soltar el contexto);
 *   - texturas: un recurso 2D B8G8R8A8 por hueco (0xAARRGGBB en memoria, sin conversión) con su «sampler view»;
 *   - vallas: los dibujos van sin valla (QEMU responde al encolarlos); esperar manda un NOP con valla, cuya
 *     respuesta llega cuando la GPU terminó todo lo anterior; leer va con valla;
 *   - presentar (D432): el cuadro vuelve a la RAM (TRANSFER_FROM_HOST_3D del rectángulo) y la CPU lo escala al
 *     respaldo del escritorio, que va al scanout por vg_atualizar/vg_descargar. Es lo sencillo y robusto: la
 *     pantalla y su respaldo nunca se separan (el cursor y la vuelta al compositor leen ese respaldo).
 * Solo gpu3d v1 (y presentar): lo que no cabe lo dibuja SaguiGL por software. El kernel no usa coma flotante:
 * los reales que hacen falta se construyen con enteros. */
#include "gpu3d.h"
#include "virgl.h"
#include "virtio_gpu.h"
#include "console.h"
#include "memoria.h"
#include "sensores_conv.h"
#include "texto.h"
#include "video.h"

#define CTX          2u
#define SUB_PAG      40u                    /* el búfer de SUBMIT_3D: 160 KB (3072 vértices son 96 KB) */
#define VB_BYTES     (1u << 20)
#define MAX_LADO     2048
#define PRAZO_ORDEN  500u
#define F_1          0x3F800000u
#define F_M1         0xBF800000u
#define F_2          0x40000000u
#define F_05         0x3F000000u
#define PIPE_CLEAR_DEPTH   (1u << 0)
#define PIPE_CLEAR_STENCIL (1u << 1)
#define PIPE_CLEAR_COLOR0  (1u << 2)
#define PIPE_PRIM_TRIANGLES 4
#define BF_ONE        1u
#define BF_SRC_ALPHA  3u
#define BF_ZERO       0x11u
#define BF_INV_SRC_ALPHA 0x13u

typedef struct { uint32_t id; uint8_t *resp; uint32_t pg; int l, a; uint32_t vista; } tex_t;

static bool pronto;
static uint8_t *sub;                         /* SUB_PAG páginas: cabecera + flujo */
static virgl_flujo_t fl;
static uint32_t prox_id = 0x100, prox_h;     /* id de recurso (del dispositivo) y asa de objeto (del contexto) */
static uint32_t vb_id, vb_desl;
static uint32_t rt_id, z_id, rt_surf, z_surf, rt_pg;
static uint8_t *rt_resp;
static int L, A, zb;
static tex_t tex[GPU3D_TEXTURAS];
static uint32_t h_rast, h_ve, h_vs, h_fs_cor, h_fs_tex;
static uint32_t h_blend[17], h_dsa[16], h_samp[8];
static g3d_vala_t enviada, feita;
static uint32_t fmt_z24, fmt_z16;           /* los formatos de Z que el anfitrión sabe usar (capset) */
static uint32_t n_envios, n_bytes_vert, usado_kb;

/* ------------------------------------------------------------ números reales con enteros */

/* num/den (enteros, den > 0) en IEEE 754 de 32 bits, redondeado. */
static uint32_t fl_div(uint32_t num, uint32_t den)
{
    if (!num || !den) return 0;
    uint64_t n = num, d = den;
    int k = 0;
    while (n < (d << 23)) { n <<= 1; k++; }
    while (n >= (d << 24)) { d <<= 1; k--; }
    uint32_t q = (uint32_t)sconv_div64(n + d / 2, (uint32_t)d);
    if (q >= (1u << 24)) { q >>= 1; k--; }
    return ((uint32_t)(127 + 23 - k) << 23) | (q & 0x7FFFFFu);
}

/* ------------------------------------------------------------ el flujo */

static void iniciar_flujo(void)
{
    fl.p = (uint32_t *)(sub + VIRGL_CAB_ENVIAR);
    fl.n = 0;
    fl.max = (SUB_PAG * PAGINA - VIRGL_CAB_ENVIAR) / 4;
    fl.lleno = false;
}

static int enviar(bool valla, uint32_t ms)
{
    if (fl.lleno) { klog("[GPU3D] virgl: el flujo no cabe en el bufer de SUBMIT_3D\n"); return G3D_FALHA; }
    if (!fl.n) return G3D_OK;
    uint32_t t = virgl_enviar(CTX, sub, fl.n, valla, ms);
    n_envios++;
    iniciar_flujo();
    if (t != 0x1100u) { klog("[GPU3D] virgl: SUBMIT_3D respondio %x\n", t); return G3D_FALHA; }
    enviada++;
    if (valla) feita = enviada;
    return G3D_OK;
}

static uint32_t asa(void) { return ++prox_h; }
static void cmd(uint32_t c, uint32_t obj, uint32_t len) { virgl_cmd(&fl, c, obj, len); }
static void w(uint32_t v) { virgl_w(&fl, v); }
static void asociar(uint32_t obj, uint32_t h) { cmd(VC_ASOCIAR_OBJETO, obj, 1); w(h); }
static void destruir(uint32_t obj, uint32_t h) { if (h) { cmd(VC_DESTRUIR_OBJETO, obj, 1); w(h); } }

static uint8_t *pedir(uint32_t bytes, uint32_t *pg)
{
    uint32_t n = (bytes + PAGINA - 1) / PAGINA, f = memfisica_alocar_seguidas(n);
    if (!f) return NULL;
    memset((void *)f, 0, n * PAGINA);
    *pg = n;
    usado_kb += n * (PAGINA / 1024);
    return (uint8_t *)f;
}
static void devolver(uint8_t **p, uint32_t *pg)
{
    if (*p) { memfisica_liberar_seguidas((uint32_t)*p, *pg); usado_kb -= *pg * (PAGINA / 1024); }
    *p = NULL; *pg = 0;
}

/* ------------------------------------------------------------ sombreadores */

static const char vs_txt[] =
    "VERT\n"
    "DCL IN[0]\n"
    "DCL IN[1]\n"
    "DCL IN[2]\n"
    "DCL IN[3]\n"
    "DCL OUT[0], POSITION\n"
    "DCL OUT[1], GENERIC[0]\n"
    "DCL OUT[2], GENERIC[1]\n"
    "DCL OUT[3], GENERIC[2]\n"
    "DCL CONST[0..1]\n"
    "DCL TEMP[0..1]\n"
    "  0: RCP TEMP[0].x, IN[0].wwww\n"
    "  1: MAD TEMP[1], IN[0], CONST[0], CONST[1]\n"
    "  2: MUL OUT[0].xyz, TEMP[1], TEMP[0].xxxx\n"
    "  3: MOV OUT[0].w, TEMP[0].xxxx\n"
    "  4: MOV OUT[1], IN[1].zyxw\n"
    "  5: MOV OUT[2], IN[2].zyxw\n"
    "  6: MOV OUT[3], IN[3]\n"
    "  7: END\n";
static const char fs_cor_txt[] =
    "FRAG\n"
    "PROPERTY FS_COLOR0_WRITES_ALL_CBUFS 1\n"
    "DCL IN[0], GENERIC[0], PERSPECTIVE\n"
    "DCL IN[1], GENERIC[1], PERSPECTIVE\n"
    "DCL OUT[0], COLOR\n"
    "  0: ADD OUT[0].xyz, IN[0], IN[1]\n"
    "  1: MOV OUT[0].w, IN[0]\n"
    "  2: END\n";
static const char fs_tex_txt[] =
    "FRAG\n"
    "PROPERTY FS_COLOR0_WRITES_ALL_CBUFS 1\n"
    "DCL IN[0], GENERIC[0], PERSPECTIVE\n"
    "DCL IN[1], GENERIC[1], PERSPECTIVE\n"
    "DCL IN[2], GENERIC[2], PERSPECTIVE\n"
    "DCL OUT[0], COLOR\n"
    "DCL SAMP[0]\n"
    "DCL SVIEW[0], 2D, FLOAT\n"
    "DCL TEMP[0]\n"
    "  0: TEX TEMP[0], IN[2], SAMP[0], 2D\n"
    "  1: MUL TEMP[0], TEMP[0], IN[0]\n"
    "  2: ADD OUT[0].xyz, TEMP[0], IN[1]\n"
    "  3: MOV OUT[0].w, TEMP[0]\n"
    "  4: END\n";

/* ------------------------------------------------------------ objetos de estado (en caché) */

static uint32_t fator(uint8_t f)
{
    return f == G3D_F_ZERO ? BF_ZERO : f == G3D_F_ALFA ? BF_SRC_ALPHA : f == G3D_F_UM_MENOS_ALFA ? BF_INV_SRC_ALPHA : BF_ONE;
}

static uint32_t blend(const gpu3d_estado_t *e)
{
    bool mez = (e->flags & G3D_MEZCLA) != 0;
    int k = mez ? 1 + (e->mez_orig & 3) * 4 + (e->mez_dest & 3) : 0;
    if (h_blend[k]) return h_blend[k];
    uint32_t h = asa(), rt = 0xFu << 27;
    if (mez) {
        uint32_t o = fator(e->mez_orig), d = fator(e->mez_dest);
        rt |= 1u | (o << 4) | (d << 9) | (o << 17) | (d << 22);   /* suma en el color y en el alfa */
    }
    cmd(VC_CREAR_OBJETO, VO_BLEND, 11);
    w(h); w(0); w(0); w(rt);
    for (int i = 1; i < 8; i++) w(0);
    return h_blend[k] = h;
}

static uint32_t dsa(const gpu3d_estado_t *e)
{
    bool t = (e->flags & G3D_Z_TESTE) != 0, es = t && (e->flags & G3D_Z_ESCRITA);
    uint32_t f = e->func_z == G3D_Z_MENOR ? 1u : e->func_z == G3D_Z_MENOR_IGUAL ? 3u : 7u;   /* LESS, LEQUAL, ALWAYS */
    int k = (t ? 1 : 0) | (es ? 2 : 0) | (int)(t ? (e->func_z > 2 ? 2 : e->func_z) : 0) << 2;
    if (h_dsa[k]) return h_dsa[k];
    uint32_t h = asa();
    cmd(VC_CREAR_OBJETO, VO_DSA, 5);
    w(h); w(t ? (1u | (es ? 2u : 0u) | (f << 2)) : 0u); w(0); w(0); w(0);
    return h_dsa[k] = h;
}

static uint32_t amostrador(uint32_t flags)
{
    int k = (flags & G3D_TEX_REPETE_S ? 1 : 0) | (flags & G3D_TEX_REPETE_T ? 2 : 0) | (flags & G3D_TEX_LINEAR ? 4 : 0);
    if (h_samp[k]) return h_samp[k];
    uint32_t h = asa(), ws = k & 1 ? 0u : 2u, wt = k & 2 ? 0u : 2u, fi = k & 4 ? 1u : 0u;   /* REPEAT o CLAMP_TO_EDGE */
    cmd(VC_CREAR_OBJETO, VO_SAMPLER_STATE, 9);
    w(h);
    w(ws | (wt << 3) | (2u << 6) | (fi << 9) | (2u << 11) | (fi << 13));   /* sin mipmaps */
    for (int i = 0; i < 7; i++) w(0);                                      /* lod_bias, min_lod, max_lod, borde */
    return h_samp[k] = h;
}

/* ------------------------------------------------------------ el driver */

static void soltar_tex(int i)
{
    if (!tex[i].id) return;
    destruir(VO_SAMPLER_VIEW, tex[i].vista);
    enviar(false, PRAZO_ORDEN);
    virgl_recurso_soltar(tex[i].id);
    devolver(&tex[i].resp, &tex[i].pg);
    memset(&tex[i], 0, sizeof tex[i]);
}

static void soltar_alvo(void)
{
    if (rt_id) virgl_recurso_soltar(rt_id);
    if (z_id) virgl_recurso_soltar(z_id);
    devolver(&rt_resp, &rt_pg);
    rt_id = z_id = 0; L = A = 0;
}

static void v_desligar(void)
{
    if (!pronto) return;
    pronto = false;
    if (!virgl_ultima() || !vg_ativo()) {
        /* La GPU virtual dejó de responder: no se le manda nada más (cada orden esperaría su plazo) y los
         * respaldos se abandonan sin devolverlos, porque el anfitrión aún podría escribir en ellos. */
        klog("[GPU3D] virgl: la GPU virtual no responde; se abandonan el contexto y %u KB de respaldos\n", usado_kb);
        memset(tex, 0, sizeof tex);
        rt_id = z_id = vb_id = 0; rt_resp = NULL; rt_pg = 0; rt_surf = z_surf = 0; L = A = 0;
        sub = NULL; usado_kb = 0;
        return;
    }
    virgl_ctx_destruir(CTX);                         /* se lleva todos los objetos del contexto */
    for (int i = 0; i < GPU3D_TEXTURAS; i++) {
        if (tex[i].id) virgl_recurso_soltar(tex[i].id);
        devolver(&tex[i].resp, &tex[i].pg);
        memset(&tex[i], 0, sizeof tex[i]);
    }
    soltar_alvo();
    rt_surf = z_surf = 0;
    if (vb_id) virgl_recurso_soltar(vb_id);
    vb_id = 0;
    if (sub) memfisica_liberar_seguidas((uint32_t)sub, SUB_PAG);
    sub = NULL;
    usado_kb = 0;
}

static int v_iniciar(const pci_disp_t *d, int geracao, const acel_tela_t *t, uint32_t variante, char *m, int nm)
{
    (void)d; (void)geracao; (void)t; (void)variante;
    v_desligar();
    if (!virgl_disponible()) { snformatar(m, (size_t)nm, "la virtio-gpu no tiene VIRGL negociado"); return G3D_NAO; }
    uint32_t f = memfisica_alocar_seguidas(SUB_PAG);
    if (!f) { snformatar(m, (size_t)nm, "sin memoria para el bufer de ordenes"); return G3D_NAO; }
    sub = (uint8_t *)f;
    usado_kb = SUB_PAG * (PAGINA / 1024);
    iniciar_flujo();
    /* Los formatos que hacen falta, según el capset v1 del anfitrión. virglrenderer solo rellena las máscaras de
     * muestreo y de destino (las de profundidad y vértices llegan a cero), así que el Z se elige por la de muestreo:
     * con WSLg (Mesa d3d12) no hay Z24_UNORM_S8_UINT pero sí Z24X8. */
    uint32_t caps[65];
    if (!virgl_caps(caps, 65)) { memfisica_liberar_seguidas(f, SUB_PAG); sub = NULL; snformatar(m, (size_t)nm, "GET_CAPSET no respondio"); return G3D_FALHA; }
    #define TEM(k, fmt) ((caps[(k) + (fmt) / 32] >> ((fmt) % 32)) & 1u)
    static const uint32_t z24[] = { VFMT_Z24S8, VFMT_Z24X8, VFMT_S8Z24, VFMT_Z32F, VFMT_Z16 };
    fmt_z24 = fmt_z16 = 0;
    for (int i = 0; i < 5 && !fmt_z24; i++) if (TEM(VCAPS_MUESTREO, z24[i])) fmt_z24 = z24[i];
    fmt_z16 = TEM(VCAPS_MUESTREO, VFMT_Z16) ? VFMT_Z16 : fmt_z24;
    klog("[GPU3D] virgl: formatos del anfitrion: muestreo %x %x %x, destino %x %x %x, Z %x, vertices %x %x %x; Z de 24 bits con el formato %u, de 16 con el %u\n",
         caps[1], caps[2], caps[3], caps[17], caps[18], caps[19], caps[33], caps[49], caps[50], caps[51], fmt_z24, fmt_z16);
    if (!fmt_z24 || !TEM(VCAPS_DESTINO, VFMT_B8G8R8A8) || !TEM(VCAPS_MUESTREO, VFMT_B8G8R8A8)) {
        memfisica_liberar_seguidas(f, SUB_PAG); sub = NULL;
        snformatar(m, (size_t)nm, "el anfitrion no tiene los formatos que hacen falta (B8G8R8A8 y un Z)");
        return G3D_NAO;
    }
    #undef TEM
    prox_h = 0; enviada = feita = 0; vb_desl = 0;
    memset(h_blend, 0, sizeof h_blend); memset(h_dsa, 0, sizeof h_dsa); memset(h_samp, 0, sizeof h_samp);
    memset(tex, 0, sizeof tex);
    if (!virgl_ctx_crear(CTX, "sagui-gpu3d")) {
        memfisica_liberar_seguidas(f, SUB_PAG); sub = NULL;
        snformatar(m, (size_t)nm, "CTX_CREATE no respondio");
        return G3D_FALHA;
    }
    pronto = true;
    vb_id = prox_id++;
    if (!virgl_recurso_crear(CTX, vb_id, TGT_BUFFER, FMT_BUFFER, BIND_VERTEX_BUFFER, VB_BYTES, 1, NULL, 0)) {
        vb_id = 0;
        v_desligar();
        snformatar(m, (size_t)nm, "no se pudo crear el bufer de vertices");
        return G3D_FALHA;
    }
    /* Estado fijo: rasterizador (sin recorte por Z: SaguiGL ya recortó; centros de píxel en .5), elementos de
     * vértice (posición, color, especular, u y v) y los tres sombreadores. */
    h_rast = asa();
    cmd(VC_CREAR_OBJETO, VO_RASTERIZER, 9);
    w(h_rast); w((1u << 29) | (1u << 30)); w(F_1); w(0); w(0); w(F_1); w(0); w(0); w(0);
    asociar(VO_RASTERIZER, h_rast);
    h_ve = asa();
    cmd(VC_CREAR_OBJETO, VO_VERTEX_ELEMENTS, 1 + 4 * 4);
    w(h_ve);
    w(0);  w(0); w(0); w(VFMT_R32G32B32A32_F);   /* x, y, z, rhw */
    w(16); w(0); w(0); w(VFMT_R8G8B8A8);         /* cor: bytes B, G, R, A (el sombreador los ordena) */
    w(20); w(0); w(0); w(VFMT_R8G8B8A8);         /* espec */
    w(24); w(0); w(0); w(VFMT_R32G32_F);         /* u, v */
    asociar(VO_VERTEX_ELEMENTS, h_ve);
    h_vs = asa(); virgl_sombreador(&fl, h_vs, VSOMBRA_VERTICES, vs_txt);
    h_fs_cor = asa(); virgl_sombreador(&fl, h_fs_cor, VSOMBRA_FRAGMENTOS, fs_cor_txt);
    h_fs_tex = asa(); virgl_sombreador(&fl, h_fs_tex, VSOMBRA_FRAGMENTOS, fs_tex_txt);
    cmd(VC_ASOCIAR_SOMBR, 0, 2); w(h_vs); w(VSOMBRA_VERTICES);
    cmd(VC_ASOCIAR_SOMBR, 0, 2); w(h_fs_cor); w(VSOMBRA_FRAGMENTOS);
    if (enviar(true, GPU3D_PRAZO_MS) != G3D_OK) {
        v_desligar();
        snformatar(m, (size_t)nm, "el estado fijo no se pudo enviar");
        return G3D_FALHA;
    }
    klog("[GPU3D] virgl: contexto %u con el estado fijo y tres sombreadores TGSI; bufer de vertices de %u KB\n", CTX, VB_BYTES >> 10);
    return G3D_OK;
}

static int v_alvo(int larg, int alt, int zbits)
{
    if (!pronto) return G3D_FALHA;
    if (larg < 8 || alt < 8 || larg > MAX_LADO || alt > MAX_LADO) return G3D_NAO;
    uint32_t pg = 0;
    uint8_t *r = pedir((uint32_t)(larg * alt) * 4, &pg);
    if (!r) return G3D_NAO;
    uint32_t nrt = prox_id++, nz = prox_id++;
    int nzb = zbits == 16 ? 16 : 24;
    uint32_t zf = nzb == 16 ? fmt_z16 : fmt_z24;
    if (!virgl_recurso_crear(CTX, nrt, TGT_2D, VFMT_B8G8R8A8, BIND_RENDER_TARGET | BIND_SAMPLER_VIEW, (uint32_t)larg, (uint32_t)alt,
                             r, (uint32_t)(larg * alt) * 4)) { devolver(&r, &pg); return G3D_FALHA; }
    if (!virgl_recurso_crear(CTX, nz, TGT_2D, zf, BIND_DEPTH_STENCIL, (uint32_t)larg, (uint32_t)alt, NULL, 0)) {
        virgl_recurso_soltar(nrt); devolver(&r, &pg); return G3D_FALHA;
    }
    uint32_t sc = asa(), sz = asa();
    cmd(VC_CREAR_OBJETO, VO_SURFACE, 5); w(sc); w(nrt); w(VFMT_B8G8R8A8); w(0); w(0);
    cmd(VC_CREAR_OBJETO, VO_SURFACE, 5); w(sz); w(nz); w(zf); w(0); w(0);
    cmd(VC_SET_FRAMEBUFFER, 0, 3); w(1); w(sz); w(sc);
    destruir(VO_SURFACE, rt_surf);
    destruir(VO_SURFACE, z_surf);
    /* Ventana: de [-1, 1] a píxeles, sin invertir (la fila 0 del recurso es la de arriba, y = 0). */
    cmd(VC_SET_VIEWPORT, 0, 7); w(0);
    w(g3d_float_de_fixo(larg, 1)); w(g3d_float_de_fixo(alt, 1)); w(F_05);
    w(g3d_float_de_fixo(larg, 1)); w(g3d_float_de_fixo(alt, 1)); w(F_05);
    /* Constantes del sombreador de vértices: CONST[0] = (2/L, 2/A, 2, 0), CONST[1] = (-1, -1, -1, 0). */
    cmd(VC_CONSTANTES, 0, 2 + 8); w(VSOMBRA_VERTICES); w(0);
    w(fl_div(2, (uint32_t)larg)); w(fl_div(2, (uint32_t)alt)); w(F_2); w(0);
    w(F_M1); w(F_M1); w(F_M1); w(0);
    if (enviar(false, PRAZO_ORDEN) != G3D_OK) { virgl_recurso_soltar(nrt); virgl_recurso_soltar(nz); devolver(&r, &pg); return G3D_FALHA; }
    soltar_alvo();
    rt_id = nrt; z_id = nz; rt_surf = sc; z_surf = sz; rt_resp = r; rt_pg = pg;
    L = larg; A = alt; zb = nzb;
    return G3D_OK;
}

static int v_limpar(uint32_t mascara, uint32_t c)
{
    if (!pronto || !rt_id) return G3D_FALHA;
    uint32_t b = (mascara & 1 ? PIPE_CLEAR_COLOR0 : 0) | (mascara & 2 ? PIPE_CLEAR_DEPTH | PIPE_CLEAR_STENCIL : 0);
    if (!b) return G3D_OK;
    cmd(VC_LIMPIAR, 0, 8);
    w(b);
    w(fl_div((c >> 16) & 0xFF, 255)); w(fl_div((c >> 8) & 0xFF, 255)); w(fl_div(c & 0xFF, 255)); w(0);
    w(0); w(0x3FF00000u);                  /* Z = 1.0 (double): el fondo */
    w(0);
    return enviar(false, PRAZO_ORDEN);
}

static int v_textura(int vaga, int larg, int alt, const uint32_t *argb)
{
    if (!pronto) return G3D_FALHA;
    if (vaga < 0 || vaga >= GPU3D_TEXTURAS) return G3D_NAO;
    soltar_tex(vaga);
    if (!argb) return G3D_OK;
    if (larg < 1 || alt < 1 || larg > MAX_LADO || alt > MAX_LADO) return G3D_NAO;
    tex_t *t = &tex[vaga];
    uint32_t bytes = (uint32_t)(larg * alt) * 4;
    t->resp = pedir(bytes, &t->pg);
    if (!t->resp) return G3D_NAO;
    memcpy(t->resp, argb, bytes);
    t->id = prox_id++; t->l = larg; t->a = alt;
    if (!virgl_recurso_crear(CTX, t->id, TGT_2D, VFMT_B8G8R8A8, BIND_SAMPLER_VIEW, (uint32_t)larg, (uint32_t)alt, t->resp, bytes)) {
        devolver(&t->resp, &t->pg); memset(t, 0, sizeof *t); return G3D_FALHA;
    }
    if (!virgl_transferir(CTX, true, t->id, 0, 0, (uint32_t)larg, (uint32_t)alt, (uint32_t)larg * 4, 0, false, PRAZO_ORDEN)) {
        virgl_recurso_soltar(t->id); devolver(&t->resp, &t->pg); memset(t, 0, sizeof *t); return G3D_FALHA;
    }
    t->vista = asa();
    cmd(VC_CREAR_OBJETO, VO_SAMPLER_VIEW, 6);
    w(t->vista); w(t->id); w(VFMT_B8G8R8A8); w(0); w(0); w(0u | (1u << 3) | (2u << 6) | (3u << 9));
    return enviar(false, PRAZO_ORDEN);
}

static int v_desenhar(const gpu3d_estado_t *e, const gpu3d_vert_t *v, int n, g3d_vala_t *vala)
{
    if (!pronto || !rt_id) return G3D_FALHA;
    if (n < 3) { if (vala) *vala = enviada; return G3D_OK; }
    uint32_t bytes = (uint32_t)n * (uint32_t)sizeof(gpu3d_vert_t);
    if (bytes > VB_BYTES) return G3D_NAO;
    if (vb_desl + bytes > VB_BYTES) vb_desl = 0;
    bool tx = (e->flags & G3D_TEXTURA) && e->textura < GPU3D_TEXTURAS && tex[e->textura].id;
    asociar(VO_BLEND, blend(e));
    asociar(VO_DSA, dsa(e));
    cmd(VC_ASOCIAR_SOMBR, 0, 2); w(tx ? h_fs_tex : h_fs_cor); w(VSOMBRA_FRAGMENTOS);
    if (tx) {
        uint32_t s = amostrador(e->flags);
        cmd(VC_SAMPLER_VIEWS, 0, 3); w(VSOMBRA_FRAGMENTOS); w(0); w(tex[e->textura].vista);
        cmd(VC_ASOCIAR_SAMPLERS, 0, 3); w(VSOMBRA_FRAGMENTOS); w(0); w(s);
    }
    uint32_t dw = bytes / 4;
    cmd(VC_ESCRIBIR, 0, 11 + dw);
    w(vb_id); w(0); w(0); w(0); w(0); w(vb_desl); w(0); w(0); w(bytes); w(1); w(1);
    if (fl.n + dw <= fl.max) { memcpy(fl.p + fl.n, v, bytes); fl.n += dw; } else fl.lleno = true;
    cmd(VC_SET_VERTEX_BUF, 0, 3); w(sizeof(gpu3d_vert_t)); w(vb_desl); w(vb_id);
    cmd(VC_DIBUJAR, 0, 12);
    w(0); w((uint32_t)n); w(PIPE_PRIM_TRIANGLES); w(0); w(1); w(0); w(0); w(0); w(0); w(0); w((uint32_t)n - 1); w(0);
    vb_desl = (vb_desl + bytes + 63) & ~63u;
    n_bytes_vert += bytes;
    int r = enviar(false, PRAZO_ORDEN);
    if (vala) *vala = enviada;
    return r;
}

static int v_esperar(g3d_vala_t vala, uint32_t ms)
{
    if (!pronto) return G3D_FALHA;
    if (feita >= enviada || (vala && vala <= feita)) return G3D_OK;
    w(0);                                      /* NOP: la valla de esta entrega cubre todo lo anterior */
    return enviar(true, ms);
}

/* Trae el rectángulo (x, y, l, a) del recurso a su respaldo, esperando a la GPU. */
static int traer(uint32_t id, int larg, int x, int y, int l, int a)
{
    if (!virgl_transferir(CTX, false, id, (uint32_t)x, (uint32_t)y, (uint32_t)l, (uint32_t)a, (uint32_t)larg * 4,
                          ((uint32_t)y * (uint32_t)larg + (uint32_t)x) * 4, true, GPU3D_PRAZO_MS)) return G3D_FALHA;
    feita = enviada;
    return G3D_OK;
}

static int v_ler(uint32_t *dest, int passo_px)
{
    if (!pronto || !rt_id) return G3D_FALHA;
    if (traer(rt_id, L, 0, 0, L, A) != G3D_OK) return G3D_FALHA;
    const uint32_t *o = (const uint32_t *)rt_resp;
    for (int y = 0; y < A; y++)
        for (int x = 0; x < L; x++) dest[(uint32_t)y * (uint32_t)passo_px + (uint32_t)x] = o[(uint32_t)y * (uint32_t)L + (uint32_t)x] & 0x00FFFFFFu;
    return G3D_OK;
}

static int v_apresentar(const g3d_apres_t *p)
{
    if (!pronto) return G3D_FALHA;
    bool tela_ok = video_ativo() && video_formato_direto() && video_bpp() == 32 && vg_ativo();
    if (!p) return tela_ok ? G3D_OK : G3D_NAO;
    if (!tela_ok) return G3D_NAO;
    uint32_t id; int fl_l; const uint32_t *f;
    if (p->vaga < 0) { if (!rt_id) return G3D_FALHA; id = rt_id; fl_l = L; f = (const uint32_t *)rt_resp; }
    else { if (p->vaga >= GPU3D_TEXTURAS || !tex[p->vaga].id) return G3D_NAO; id = tex[p->vaga].id; fl_l = tex[p->vaga].l; f = (const uint32_t *)tex[p->vaga].resp; }
    if (traer(id, fl_l, p->ox, p->oy, p->ol, p->oa) != G3D_OK) return G3D_FALHA;
    uint8_t *fb = video_placa();
    uint32_t pitch = video_pitch();
    int tl = (int)video_largura(), ta = (int)video_altura();
    if (p->flags & G3D_APRES_BANDAS) {
        int r[4][4], n = g3d_apres_bandas(p, tl, ta, r);
        for (int i = 0; i < n; i++) {
            for (int y = r[i][1]; y < r[i][1] + r[i][3]; y++)
                memset(fb + (uint32_t)y * pitch + (uint32_t)r[i][0] * 4, 0, (uint32_t)r[i][2] * 4);
            vg_atualizar(r[i][0], r[i][1], r[i][2], r[i][3]);
        }
    }
    for (int y = 0; y < p->da; y++) {
        const uint32_t *o = f + (uint32_t)(p->oy + (int)(((uint32_t)y * 2 + 1) * (uint32_t)p->oa / (2u * (uint32_t)p->da))) * (uint32_t)fl_l + (uint32_t)p->ox;
        uint32_t *d = (uint32_t *)(fb + (uint32_t)(p->dy + y) * pitch) + p->dx;
        if (p->dl == p->ol) { for (int x = 0; x < p->dl; x++) d[x] = o[x] & 0x00FFFFFFu; continue; }
        for (int x = 0; x < p->dl; x++) d[x] = o[(uint32_t)(((uint32_t)x * 2 + 1) * (uint32_t)p->ol / (2u * (uint32_t)p->dl))] & 0x00FFFFFFu;
    }
    vg_atualizar(p->dx, p->dy, p->dl, p->da);
    vg_descargar();
    return G3D_OK;
}

static void v_detalle(const char *fase)
{
    klog("[GPU3D] virgl %s: destino %dx%d con Z de %d bits, %u entregas, valla enviada %u y cumplida %u, %u KB de vertices, %u KB de RAM\n",
         fase, L, A, zb, n_envios, enviada, feita, n_bytes_vert >> 10, usado_kb);
}

/* La memoria es la del anfitrión (sin límite que se pueda preguntar): se da una cifra nominal de 256 MB y lo
 * que el invitado tiene en respaldos. */
static void v_memoria(uint32_t *total_kb, uint32_t *livre_kb)
{
    *total_kb = 256u * 1024u;
    *livre_kb = usado_kb < *total_kb ? *total_kb - usado_kb : 0;
}

/* La CPU no ve el destino (está en el anfitrión): el diagnóstico de sentinelas no aplica. */
static void v_vistas(g3d_vista_t *c, g3d_vista_t *z) { (void)c; (void)z; }

const gpu3d_driver_t gpu3d_virgl = {
    "virgl", v_iniciar, v_alvo, v_limpar, v_textura, v_desenhar, v_esperar, v_ler, v_desligar, v_detalle, v_memoria,
    "virgl", NULL, 0, v_vistas,
    NULL, NULL, NULL, NULL, NULL,
    v_apresentar,
    NULL, NULL,
};
