// SPDX-License-Identifier: GPL-3.0-only
/* sgl2_gpu.c - SaguiGL 2.0 por la GPU: el backend de gpu3d v4 (D480).
 *
 * Con un driver de gpu3d que anuncia v4 (hoy VirGL: QEMU y Proxmox), un dibujo de ES 2.0 no pasa por el
 * rasterizador de la CPU: el programa enlazado se traduce una vez de RI a TGSI en texto (sgl_tgsi.c), se sube a la
 * GPU, y cada glDrawArrays/glDrawElements manda atributos, índices, uniformes, texturas y el estado de ES 2.0 ya
 * resuelto (mezcla, Z, esténcil, tijera, máscaras, viewport). Los destinos (el framebuffer por defecto, una textura
 * o un renderbuffer de un FBO) viven en la GPU mientras se dibuja: «sujo» marca el que tiene un contenido más nuevo
 * que la CPU, y cualquier cosa que lea o escriba ese destino con la CPU (un dibujo por la CPU, glReadPixels,
 * glCopyTex*, el cambio de cuadro de EGL, glFinish) lo trae antes con sgl2_gpu_sincronizar(). Al revés, cada objeto
 * tiene un número de versión de lo que escribió la CPU y se sube de nuevo cuando cambia.
 *
 * Lo que no hace la GPU lo hace la CPU, dibujo a dibujo y sin colgar: puntos y líneas, un programa que no se
 * traduce, una textura o destino que no cabe, una textura de profundidad (D486: ni se muestrea ni es el Z de un FBO en la GPU), el esténcil en un anfitrión sin formato Z+esténcil, un error de la GPU
 * (entonces se apaga la GPU en este proceso y todo sigue por la CPU). La primera vez que se abre la GPU hay una
 * autoprueba que dibuja una escena por la CPU y por la GPU con la misma tubería de ES 2.0 y compara los píxeles.
 *
 * Limitaciones documentadas (docs/saguigl2.md): el Z y el esténcil viven solo en la GPU (si un dibujo cae a la CPU
 * en mitad de un cuadro, la CPU usa el suyo); los destinos son el nivel 0 y la cara 0 de una textura; no hay VBO
 * residentes (los atributos se envían con cada dibujo). */
#include "sgl2.h"
#include "sgl_tgsi.h"

#ifdef SGL_ANFITRIAO
#include "../../kernel/inc/abi.h"
#define G_TEM_KERNEL 0
#else
#define G_TEM_KERNEL 1
#endif

typedef gpu3d_v4_atrib_t v4_atrib_t;

/* de sgl.c (la ES 1.1 de siempre): la autoprueba usa un contexto en memoria, y la GPU de ES 1.1 y la de ES 2.0 no se mezclan */
struct sgl_ctx *sgl_criar_contexto_mem(uint32_t *px, int larg, int alt, int zbits);
void sgl_destruir_contexto(struct sgl_ctx *c);
int  sgl_gpu_ativa(void);

#define ID_NEGRO2D   1
#define ID_NEGROCUBO 2
#define ID_PRIMEIRO  3
#define SLOT_LIMPAR  (G3D4_PROGRAMAS - 1)

static struct {
    int      estado;          /* 0 sin intentar, 1 abierta, -1 no hay o falló */
    int      modo;            /* 0 CPU, 1 auto, 2 auto sin autoprueba */
    int      ctxs, epoca, provando, provado;
    int      inv_frente;      /* la GPU del anfitrión da la vuelta a las caras de frente (lo detecta la autoprueba) */
    uint8_t  id_usado[G3D4_RECURSOS];
    uint8_t  slot_usado[G3D4_PROGRAMAS];
    gpu3d_v4_caps_t caps;
    unsigned n_des, n_lim, n_cpu;
} G = { .modo = 1, .epoca = 1 };

/* ------------------------------------------------------------ las llamadas al kernel */
#if G_TEM_KERNEL
static int op(unsigned o, unsigned a, unsigned b, unsigned c, const void *ptr)
{
    gpu3d_pedido_t q;
    q.op = o; q.a = a; q.b = b; q.c = c; q.ptr = (unsigned)ptr; q.bytes = 0;
    return info_sistema(INFO_GPU3D_OP, &q, sizeof q);
}
#else
static int op(unsigned o, unsigned a, unsigned b, unsigned c, const void *ptr) { (void)o; (void)a; (void)b; (void)c; (void)ptr; return GPU3D_SEM_SUPORTE; }
#endif

#define P32(x) ((unsigned)(unsigned long)(x))   /* los punteros del ABI de gpu3d son de 32 bits */
static uint32_t fb(float f) { uint32_t u; memcpy(&u, &f, 4); return u; }

static int env_zero(const char *nome)
{
#if G_TEM_KERNEL
    const char *e = obter_env(nome);
    return e && e[0] == '0';
#else
    (void)nome; return 0;
#endif
}

void sgl2_gpu_modo(int modo) { G.modo = modo; }
int  sgl2_gpu_estado(void) { return G.estado; }
void sgl2_gpu_contadores(unsigned *des, unsigned *lim, unsigned *cpu) { if (des) *des = G.n_des; if (lim) *lim = G.n_lim; if (cpu) *cpu = G.n_cpu; }

/* ------------------------------------------------------------ recursos y programas */
static int res_ok(const s2_gres_t *g) { return g->id && g->epoca == (uint32_t)G.epoca; }

static uint32_t id_novo(void)
{
    for (int i = ID_PRIMEIRO; i < G3D4_RECURSOS; i++) if (!G.id_usado[i]) { G.id_usado[i] = 1; return (uint32_t)i; }
    return 0;
}

void sgl2_gpu_soltar(s2_gres_t *g)
{
    if (!g->id) return;
    if (g->epoca == (uint32_t)G.epoca && G.estado > 0) {
        gpu3d_v4_rec_t r = { g->id, g->tipo, 0, 0, 0, 0, 0, 0 };
        op(GPU3D_OP_V4_RECURSO, 0, 0, 0, &r);
        if (g->id < G3D4_RECURSOS) G.id_usado[g->id] = 0;
    }
    memset(g, 0, sizeof *g);
}

/* apaga la GPU en este proceso: todo sigue por la CPU (lo pendiente en la GPU se pierde) */
static void falhar(const char *por_que)
{
    (void)por_que;
    if (G.estado <= 0) return;
    G.estado = -1;
    G.epoca++;
    memset(G.id_usado, 0, sizeof G.id_usado);
    memset(G.slot_usado, 0, sizeof G.slot_usado);
    op(GPU3D_OP_FECHAR, 0, 0, 0, 0);
}

static int op_v4(unsigned o, const void *ptr)
{
    int r = op(o, 0, 0, 0, ptr);
    if (r < 0) { falhar("la GPU no responde"); return -1; }
    return r;      /* 0 bien, 1 no lo hace */
}

static int crear(s2_gres_t *g, int tipo, int l, int a, int niv, int zbits, int est)
{
    uint32_t id = id_novo();
    if (!id) return 0;
    gpu3d_v4_rec_t r = { id, (unsigned)tipo, (unsigned)l, (unsigned)a, (unsigned)niv, (unsigned)zbits, (unsigned)est, 0 };
    if (op_v4(GPU3D_OP_V4_RECURSO, &r) != 0) { if (G.estado > 0) G.id_usado[id] = 0; return 0; }
    memset(g, 0, sizeof *g);
    g->id = id; g->epoca = (uint32_t)G.epoca; g->l = l; g->a = a; g->niv = niv; g->tipo = (uint8_t)tipo; g->zest = (uint8_t)est; g->zbits = (uint8_t)zbits;
    return 1;
}

static int subir(const s2_gres_t *g, int nivel, int face, int l, int a, const uint32_t *px, int passo_bytes, int invertir)
{
    gpu3d_v4_subir_t s = { g->id, (unsigned)nivel, (unsigned)face, 0, 0, (unsigned)l, (unsigned)a, P32(px), (unsigned)passo_bytes, (unsigned)invertir };
    return op_v4(GPU3D_OP_V4_SUBIR, &s) == 0;
}

/* una textura de ES 2.0 en la GPU: crearla (o rehacerla si cambió de forma) y subir todos sus niveles si cambió */
static uint32_t tex_gpu(s2_tex_t *t)
{
    if (!sgl2_tex_completa(t)) return 0;
    int cubo = t->alvo == GL_TEXTURE_CUBE_MAP, nf = cubo ? 6 : 1;
    s2_nivel_t *n0 = &t->niv[0][0];
    if (n0->z) return 0;     /* D486: una textura de profundidad solo existe en la CPU (el dibujo que la muestrea cae a la CPU) */
    int tipo = cubo ? G3D4_R_CUBO : G3D4_R_ALVO;
    int nuevo = 0;
    if (res_ok(&t->g) && (t->g.l != n0->larg || t->g.a != n0->alt || t->g.tipo != tipo || (cubo && t->g.niv != t->nniv))) sgl2_gpu_soltar(&t->g);
    if (t->g.id && !res_ok(&t->g)) memset(&t->g, 0, sizeof t->g);
    if (!t->g.id) {
        if (!crear(&t->g, tipo, n0->larg, n0->alt, t->nniv, 0, 0)) return 0;
        nuevo = 1;
    }
    /* los mipmaps de una textura renderizable pueden faltar en la GPU (se creó con menos niveles): se rehace */
    if (!nuevo && t->g.niv < t->nniv) {
        sgl2_gpu_soltar(&t->g);
        if (!crear(&t->g, tipo, n0->larg, n0->alt, t->nniv, 0, 0)) return 0;
        nuevo = 1;
    }
    if (nuevo || t->g.ver != t->ver) {
        for (int f = 0; f < nf; f++)
            for (int k = 0; k < t->nniv; k++) {
                s2_nivel_t *n = &t->niv[f][k];
                if (!subir(&t->g, k, f, n->larg, n->alt, n->px, n->larg * 4, 0)) return 0;
            }
        t->g.ver = t->ver;
    }
    return t->g.id;
}

/* el destino de un dibujo: su color y su Z/esténcil en la GPU, al día con la CPU */
static int alvo_gpu(sgl2_ctx_t *c, s2_alvo_t *a)
{
    (void)c;
    s2_gres_t *g = a->g;
    int nuevo = 0;
    if (g->id && !res_ok(g)) memset(g, 0, sizeof *g);
    if (g->id && (g->l != a->larg || g->a != a->alt)) sgl2_gpu_soltar(g);
    if (!g->id) {
        if (!crear(g, G3D4_R_ALVO, a->larg, a->alt, 1, 0, 0)) return 0;
        nuevo = 1;
    }
    if (nuevo || g->ver != *a->ver) {
        /* el contenido de la CPU: las filas del framebuffer por defecto van al revés (paso negativo) */
        int invertir = a->passo < 0;
        const uint32_t *base = invertir ? a->cor + (long)(a->alt - 1) * a->passo : a->cor;
        int p = invertir ? -a->passo : a->passo;
        if (!a->alfa) {
            uint32_t *t = aloca((size_t)a->larg * a->alt * 4);
            if (!t) return 0;
            for (int y = 0; y < a->alt; y++) for (int x = 0; x < a->larg; x++) t[(size_t)y * a->larg + x] = base[(size_t)y * p + x] | 0xFF000000u;
            int ok = subir(g, 0, 0, a->larg, a->alt, t, a->larg * 4, invertir);
            libera(t);
            if (!ok) return 0;
        } else if (!subir(g, 0, 0, a->larg, a->alt, base, p * 4, invertir)) return 0;
        g->ver = *a->ver;
    }
    if (a->zbits > 0 || a->estbits > 0) {
        s2_gres_t *z = a->gz;
        int est = a->estbits > 0 ? 1 : 0, zb = a->zbits == 16 ? 16 : 24;
        if (z->id && !res_ok(z)) memset(z, 0, sizeof *z);
        if (z->id && (z->l != a->larg || z->a != a->alt || z->zest != est)) sgl2_gpu_soltar(z);
        if (!z->id) {
            if (!crear(z, G3D4_R_ZE, a->larg, a->alt, 1, zb, est)) return 0;
            gpu3d_v4_limpar_t l = { 0, z->id, 6, { 0, 0, 0, 0 }, 0, 0x3FF00000u, 0 };
            l.alvo_cor = 0;
            /* el Z nuevo empieza limpio (un destino de color hace falta para limpiar: se usa el del dibujo) */
            l.alvo_cor = g->id;
            l.mascara = 2 | (est ? 4 : 0);
            if (op_v4(GPU3D_OP_V4_LIMPAR, &l) != 0) return 0;
        }
    }
    return 1;
}

/* ------------------------------------------------------------ programas */
static int prog_gpu(s2_prog_t *p)
{
    if (p->gslot && p->gepoca == G.epoca) return 1;
    if (p->gnao) return 0;
    static char vs[G3D4_TGSI_MAX], fs[G3D4_TGSI_MAX];
    int gen_vs[RI_MAX_ENT], gen_fs[RI_MAX_ENT];
    for (int i = 0; i < RI_MAX_ENT; i++) gen_vs[i] = gen_fs[i] = -1;
    for (int k = 0; k < p->nvary; k++) { int r = p->vary_ent[k]; gen_fs[r] = k; if (p->mapa_vary[r] >= 0) gen_vs[p->mapa_vary[r]] = k; }
    char erro[96];
    int nv = sgl_tgsi_gerar(p->pv, gen_vs, vs, (int)sizeof vs, erro, sizeof erro);
    int nf = nv >= 0 ? sgl_tgsi_gerar(p->pf, gen_fs, fs, (int)sizeof fs, erro, sizeof erro) : -1;
    if (nv < 0 || nf < 0) { p->gnao = 1; return 0; }
    int slot = -1;
    for (int i = 0; i < SLOT_LIMPAR; i++) if (!G.slot_usado[i]) { slot = i; break; }
    if (slot < 0) return 0;
    gpu3d_v4_prog_t pr = { P32(vs), (unsigned)nv + 1, P32(fs), (unsigned)nf + 1 };
    int r = op(GPU3D_OP_V4_PROGRAMA, (unsigned)slot, 0, 0, &pr);
    if (r < 0) { falhar("programa"); return 0; }
    if (r) { p->gnao = 1; return 0; }
    G.slot_usado[slot] = 1;
    p->gslot = slot + 1; p->gepoca = G.epoca;
    return 1;
}

void sgl2_gpu_prog_solto(s2_prog_t *p)
{
    if (p->gslot && p->gepoca == G.epoca && G.estado > 0) {
        op(GPU3D_OP_V4_PROGRAMA, (unsigned)(p->gslot - 1), 0, 0, 0);
        G.slot_usado[p->gslot - 1] = 0;
    }
    p->gslot = 0; p->gnao = 0;
}

void sgl2_gpu_prog_ligado(s2_prog_t *p)
{
#ifdef SGL_ANFITRIAO
    /* pruebas del PC: con SGL2_TGSI=fichero se traduce cada programa al enlazar y se añade su TGSI al fichero;
     * si no se traduce, una línea «; ERROR» (la prueba h480 lo cuenta) */
    const char *fn = getenv("SGL2_TGSI");
    if (!fn) return;
    static char vs[G3D4_TGSI_MAX], fs[G3D4_TGSI_MAX];
    int gen_vs[RI_MAX_ENT], gen_fs[RI_MAX_ENT];
    for (int i = 0; i < RI_MAX_ENT; i++) gen_vs[i] = gen_fs[i] = -1;
    for (int k = 0; k < p->nvary; k++) { int r = p->vary_ent[k]; gen_fs[r] = k; if (p->mapa_vary[r] >= 0) gen_vs[p->mapa_vary[r]] = k; }
    char erro[96] = "";
    int nv = sgl_tgsi_gerar(p->pv, gen_vs, vs, (int)sizeof vs, erro, sizeof erro);
    int nf = nv >= 0 ? sgl_tgsi_gerar(p->pf, gen_fs, fs, (int)sizeof fs, erro, sizeof erro) : -1;
    FILE *f = fopen(fn, "a");
    if (!f) return;
    if (nv < 0 || nf < 0) fprintf(f, "; ERROR %s\n", erro);
    else fprintf(f, "; PROGRAMA\n%s; FRAGMENTOS\n%s; FIN\n", vs, fs);
    fclose(f);
#else
    (void)p;
#endif
}

/* ------------------------------------------------------------ la sesión con el kernel */
static const char *VS_LIMPAR = "VERT\nDCL IN[0]\nDCL OUT[0], POSITION\n  0: MOV OUT[0], IN[0]\n  1: END\n";
static const char *FS_LIMPAR = "FRAG\nPROPERTY FS_COLOR0_WRITES_ALL_CBUFS 1\nDCL OUT[0], COLOR\nDCL CONST[0]\n  0: MOV OUT[0], CONST[0]\n  1: END\n";

static int sesion_abrir(void);
static int autoprova(void);

static int gpu_pronta(sgl2_ctx_t *c)
{
    (void)c;
    if (G.estado < 0 || G.modo == 0) return 0;
    if (G.estado == 0 && (sgl_gpu_ativa() || !sesion_abrir())) return 0;
    return G.estado > 0;
}

static int sesion_abrir(void)
{
    if (G.estado != 0) return G.estado > 0;
    if (!G_TEM_KERNEL || env_zero("SAGUI_GL_V4")) { G.estado = -1; return 0; }
    if (op(GPU3D_OP_ABRIR, 8, 8, 16, 0) != 0) { G.estado = -1; return 0; }
    gpu3d_v4_caps_t k;
    memset(&k, 0, sizeof k);
    if (op(GPU3D_OP_V4_CAPS, 0, 0, 0, &k) != 0 || !(k.bits & G3D4_C_V4)) { op(GPU3D_OP_FECHAR, 0, 0, 0, 0); G.estado = -1; return 0; }
    G.caps = k;
    G.estado = 1; G.epoca++;
    memset(G.id_usado, 0, sizeof G.id_usado); memset(G.slot_usado, 0, sizeof G.slot_usado);
    G.id_usado[0] = G.id_usado[ID_NEGRO2D] = G.id_usado[ID_NEGROCUBO] = 1;
    /* una textura negra opaca de 1x1 para las incompletas (2D y cubo) y el programa de las limpiezas parciales */
    static const uint32_t negro = 0xFF000000u;
    gpu3d_v4_rec_t r2 = { ID_NEGRO2D, G3D4_R_TEX2D, 1, 1, 1, 0, 0, 0 }, rc = { ID_NEGROCUBO, G3D4_R_CUBO, 1, 1, 1, 0, 0, 0 };
    int ok = op(GPU3D_OP_V4_RECURSO, 0, 0, 0, &r2) == 0 && op(GPU3D_OP_V4_RECURSO, 0, 0, 0, &rc) == 0;
    for (int f = 0; ok && f < 7; f++) {
        gpu3d_v4_subir_t s = { f == 0 ? ID_NEGRO2D : ID_NEGROCUBO, 0, f ? (unsigned)(f - 1) : 0, 0, 0, 1, 1, P32(&negro), 4, 0 };
        ok = op(GPU3D_OP_V4_SUBIR, 0, 0, 0, &s) == 0;
    }
    gpu3d_v4_prog_t pr = { P32(VS_LIMPAR), (unsigned)tam_texto(VS_LIMPAR) + 1, P32(FS_LIMPAR), (unsigned)tam_texto(FS_LIMPAR) + 1 };
    if (ok) ok = op(GPU3D_OP_V4_PROGRAMA, SLOT_LIMPAR, 0, 0, &pr) == 0;
    if (!ok) { G.estado = 1; falhar("sesion"); return 0; }
    G.slot_usado[SLOT_LIMPAR] = 1;
    if (G.modo == 1 && !G.provado) {
        G.provado = 1;
        G.provando = 1;
        int bien = autoprova();
        G.provando = 0;
        if (!bien) { falhar("autoprueba"); return 0; }
    }
    return 1;
}

void sgl2_gpu_ctx_novo(sgl2_ctx_t *c) { (void)c; G.ctxs++; }
void sgl2_gpu_ctx_livre(sgl2_ctx_t *c)
{
    (void)c;
    if (--G.ctxs <= 0) {
        G.ctxs = 0;
        if (G.estado > 0) {
            op(GPU3D_OP_FECHAR, 0, 0, 0, 0);
            G.estado = 0; G.epoca++;
            memset(G.id_usado, 0, sizeof G.id_usado); memset(G.slot_usado, 0, sizeof G.slot_usado);
        }
    }
}

/* ------------------------------------------------------------ lo pendiente de la GPU */
static int ler_destino(s2_gres_t *g, uint32_t *cor, int passo_px, int larg, int alt, int alfa)
{
    gpu3d_v4_ler_t r;
    memset(&r, 0, sizeof r);
    r.rec = g->id; r.larg = (unsigned)larg; r.alt = (unsigned)alt;
    if (alfa) {
        r.dest = P32(cor); r.passo = passo_px * 4;
        return op_v4(GPU3D_OP_V4_LER, &r) == 0;
    }
    /* sin alfa en el destino: se conserva el byte alto que tenía */
    uint32_t *t = aloca((size_t)larg * alt * 4);
    if (!t) return 0;
    r.dest = P32(t); r.passo = larg * 4;
    int ok = op_v4(GPU3D_OP_V4_LER, &r) == 0;
    if (ok) for (int y = 0; y < alt; y++) {
        uint32_t *d = cor + (long)y * passo_px; const uint32_t *o = t + (size_t)y * larg;
        for (int x = 0; x < larg; x++) d[x] = (d[x] & 0xFF000000u) | (o[x] & 0x00FFFFFFu);
    }
    libera(t);
    return ok;
}

void sgl2_gpu_sincronizar(sgl2_ctx_t *c)
{
    if (!c || !c->gpu_pend) return;
    c->gpu_pend = 0;
    if (G.estado <= 0) return;
    if (c->gdef.sujo && res_ok(&c->gdef)) {
        uint32_t *px; int l, h;
        c->gdef.sujo = 0;
        if (c->base && sgl_destino(c->base, &px, &l, &h) && l == c->gdef.l && h == c->gdef.a)
            if (!ler_destino(&c->gdef, px + (size_t)(h - 1) * l, -l, l, h, 0)) return;
    }
    for (int i = 0; i < c->ntex; i++) {
        s2_tex_t *t = c->tex[i];
        if (!t || !t->g.sujo || !res_ok(&t->g)) continue;
        t->g.sujo = 0;
        s2_nivel_t *n = &t->niv[0][0];
        if (n->px && n->larg == t->g.l && n->alt == t->g.a && !ler_destino(&t->g, n->px, n->larg, n->larg, n->alt, n->fmt == GL_RGBA)) return;
    }
    for (int i = 0; i < c->nrb; i++) {
        s2_rb_t *r = c->rb[i];
        if (!r || !r->g.sujo || !res_ok(&r->g)) continue;
        r->g.sujo = 0;
        if (r->cor && r->larg == r->g.l && r->alt == r->g.a &&
            !ler_destino(&r->g, r->cor, r->larg, r->larg, r->alt, r->fmt != GL_RGB565 && r->fmt != GL_RGB8_OES)) return;
    }
}
void sgl2_gpu_sincronizar_atual(void) { if (sgl2_C) sgl2_gpu_sincronizar(sgl2_C); }

/* ------------------------------------------------------------ el estado de ES 2.0 -> el de v4 */
static unsigned fator(GLenum f)
{
    switch (f) {
    case GL_ZERO: return 0; case GL_ONE: return 1; case GL_SRC_COLOR: return 2; case GL_ONE_MINUS_SRC_COLOR: return 3;
    case GL_DST_COLOR: return 4; case GL_ONE_MINUS_DST_COLOR: return 5; case GL_SRC_ALPHA: return 6; case GL_ONE_MINUS_SRC_ALPHA: return 7;
    case GL_DST_ALPHA: return 8; case GL_ONE_MINUS_DST_ALPHA: return 9; case GL_CONSTANT_COLOR: return 10; case GL_ONE_MINUS_CONSTANT_COLOR: return 11;
    case GL_CONSTANT_ALPHA: return 12; case GL_ONE_MINUS_CONSTANT_ALPHA: return 13; case GL_SRC_ALPHA_SATURATE: return 14;
    }
    return 1;
}
static unsigned ecuacion(GLenum e) { return e == GL_FUNC_SUBTRACT ? 1u : e == GL_FUNC_REVERSE_SUBTRACT ? 2u : 0u; }
static unsigned func(GLenum f) { return f >= 0x200 && f <= 0x207 ? (unsigned)(f - 0x200) : 7u; }
static unsigned opest(GLenum o)
{
    switch (o) {
    case GL_KEEP: return 0; case GL_ZERO: return 1; case GL_REPLACE: return 2; case GL_INCR: return 3; case GL_DECR: return 4;
    case GL_INCR_WRAP: return 5; case GL_DECR_WRAP: return 6; case GL_INVERT: return 7;
    }
    return 0;
}

static void estado(sgl2_ctx_t *c, const s2_alvo_t *a, gpu3d_v4_estado_t *e)
{
    memset(e, 0, sizeof *e);
    e->mez_on = (unsigned)c->blend_on;
    e->eq_rgb = ecuacion(c->b_eq[0]); e->eq_a = ecuacion(c->b_eq[1]);
    e->src_rgb = fator(c->b_src[0]); e->dst_rgb = fator(c->b_dst[0]); e->src_a = fator(c->b_src[1]); e->dst_a = fator(c->b_dst[1]);
    for (int i = 0; i < 4; i++) e->cor_mez[i] = fb(c->b_cor[i]);
    e->masc_cor = (c->masc_cor[0] ? 1u : 0u) | (c->masc_cor[1] ? 2u : 0u) | (c->masc_cor[2] ? 4u : 0u) | (c->masc_cor[3] && a->alfa ? 8u : 0u);
    int tem_z = a->z != NULL && (a->zbits > 0 || a->estbits > 0), tem_est = a->est != NULL && a->estbits > 0;
    e->z_on = (unsigned)(c->z_on && tem_z); e->z_func = func(c->z_func); e->z_escrita = (unsigned)(c->masc_z != 0);
    e->est_on = (unsigned)(c->est_on && tem_est);
    for (int i = 0; i < 2; i++) {
        e->est[i].func = func(c->est[i].func); e->est[i].ref = (unsigned)c->est[i].ref & 0xFFu;
        e->est[i].masc_v = c->est[i].masc_v & 0xFFu; e->est[i].masc_e = c->est[i].masc_e & 0xFFu;
        e->est[i].falha = opest(c->est[i].falha); e->est[i].zfalha = opest(c->est[i].zfalha); e->est[i].zpassa = opest(c->est[i].zpassa);
    }
    e->cull = c->cull_on ? (c->cull_face == GL_FRONT ? 1u : c->cull_face == GL_BACK ? 2u : 3u) : 0u;
    e->frente_ccw = (c->frente == GL_CCW) ^ (G.inv_frente != 0);
    e->off_on = (unsigned)c->off_on; e->off_fator = fb(c->off_fator); e->off_unid = fb(c->off_unid);
    e->tij_on = (unsigned)c->tij_on;
    for (int i = 0; i < 4; i++) { e->tij[i] = c->tij[i]; e->vp[i] = c->vp[i]; }
    e->z_escala = fb((c->z_longe - c->z_perto) * 0.5f); e->z_trasl = fb((c->z_longe + c->z_perto) * 0.5f);
}

static void amostra(const s2_tex_t *t, gpu3d_v4_amostra_t *s)
{
    GLenum m = t->min_f;
    s->min_img = (m == GL_LINEAR || m == GL_LINEAR_MIPMAP_NEAREST || m == GL_LINEAR_MIPMAP_LINEAR) ? 1 : 0;
    s->min_mip = (m == GL_NEAREST || m == GL_LINEAR) ? 0 : (m == GL_NEAREST_MIPMAP_NEAREST || m == GL_LINEAR_MIPMAP_NEAREST) ? 1 : 2;
    s->mag_img = t->mag_f == GL_LINEAR;
    s->wrap_s = t->wrap_s == GL_CLAMP_TO_EDGE ? 1u : t->wrap_s == GL_MIRRORED_REPEAT ? 2u : 0u;
    s->wrap_t = t->wrap_t == GL_CLAMP_TO_EDGE ? 1u : t->wrap_t == GL_MIRRORED_REPEAT ? 2u : 0u;
}

static int ler_indice(const void *idx, GLenum tipo, int i)
{
    if (tipo == GL_UNSIGNED_BYTE) return ((const uint8_t *)idx)[i];
    if (tipo == GL_UNSIGNED_SHORT) { uint16_t v; memcpy(&v, (const uint8_t *)idx + 2 * i, 2); return v; }
    uint32_t v; memcpy(&v, (const uint8_t *)idx + 4 * i, 4); return v > 0x7FFFFFFFu ? 0x7FFFFFFF : (int)v;
}

/* ------------------------------------------------------------ dibujar */
int sgl2_gpu_desenhar(sgl2_ctx_t *c, GLenum modo, GLint primeiro, GLsizei n, GLenum tipo_idx, const void *idx)
{
    if (modo != GL_TRIANGLES && modo != GL_TRIANGLE_STRIP && modo != GL_TRIANGLE_FAN) return 0;
    if (!gpu_pronta(c)) return 0;
    s2_prog_t *p = NULL;
    if (c->prog_uso && (int)c->prog_uso < c->nobj && c->obj_tipo[c->prog_uso] == 2) p = c->obj[c->prog_uso];
    if (!p || !p->ligado || p->pv->nent > G3D4_ATRIBS || p->pv->namost > G3D4_AMOSTRAS || p->pf->namost > G3D4_AMOSTRAS) return 0;
    if (n < 3 || !prog_gpu(p)) return 0;
    s2_alvo_t a;
    if (!sgl2_alvo_x(c, &a, 1) || !a.gpu_ok || a.larg < 1 || a.alt < 1 || a.larg > (int)G.caps.max_lado || a.alt > (int)G.caps.max_lado) return 0;
    if ((c->est_on && a.estbits > 0 && !(G.caps.bits & G3D4_C_ESTENCIL))) return 0;
    if (a.estbits > 0 && !(G.caps.bits & G3D4_C_ESTENCIL)) a.estbits = 0, a.est = NULL;
    sgl2_uniformes_subir(p);

    /* índices y el rango de vértices que usan */
    const void *ip = idx;
    int tam_i = tipo_idx == GL_UNSIGNED_BYTE ? 1 : tipo_idx == GL_UNSIGNED_SHORT ? 2 : 4;
    if (tipo_idx && c->buf_elem) {
        s2_buf_t *b = sgl2_buf(c->buf_elem);
        if (!b || !b->dados || (unsigned long)idx + (size_t)n * tam_i > (size_t)b->tam) return 0;
        ip = b->dados + (unsigned long)idx;
    }
    if (tipo_idx && !ip) return 0;
    int base = primeiro, nverts = n, tipo_gpu = 0;
    void *ind = NULL;
    if (tipo_idx) {
        int mn = 0x7FFFFFFF, mx = -1;
        for (int i = 0; i < n; i++) { int k = ler_indice(ip, tipo_idx, i); if (k < mn) mn = k; if (k > mx) mx = k; }
        if ((long)mx - mn + 1 > (long)n * 4 + 4096 || mx - mn + 1 > 1 << 20) return 0;     /* índices dispersos: la CPU */
        base = mn; nverts = mx - mn + 1;
        tipo_gpu = nverts <= 65536 ? 2 : 4;
        ind = aloca((size_t)n * tipo_gpu);
        if (!ind) return 0;
        for (int i = 0; i < n; i++) {
            int k = ler_indice(ip, tipo_idx, i) - mn;
            if (tipo_gpu == 2) ((uint16_t *)ind)[i] = (uint16_t)k; else ((uint32_t *)ind)[i] = (uint32_t)k;
        }
    }

    /* atributos: los de la CPU si ya son floats con paso de 4 bytes, si no convertidos */
    gpu3d_v4_des_t *d = aloca_zerado(sizeof *d);
    float *conv[G3D4_ATRIBS];
    memset(conv, 0, sizeof conv);
    int fallo = !d;
    if (!fallo) {
        at_fonte_t at[S2_MAX_ATRIB];
        sgl2_at_preparar(c, at);
        ri_prog_t *pv = p->pv;
        unsigned total = (unsigned)n * (unsigned)tipo_gpu;
        d->natrib = (unsigned)pv->nent;
        for (int r = 0; r < pv->nent && !fallo; r++) {
            v4_atrib_t *x = &d->at[r];
            int loc = p->atr_loc[r];
            const at_fonte_t *f = loc >= 0 ? &at[loc] : NULL;
            if (!f || !f->ptr) {
                const float *v = f ? f->atual : NULL;
                float k[4] = { 0, 0, 0, 1 };
                if (v) memcpy(k, v, 16);
                x->ativo = 0;
                for (int q = 0; q < 4; q++) x->cte[q] = fb(k[q]);
                continue;
            }
            x->ativo = 1;
            if (f->tipo == GL_FLOAT && (f->passo & 3) == 0 && (((unsigned long)f->ptr) & 3) == 0 &&
                (!f->lim || f->ptr + (size_t)(base + nverts - 1) * f->passo + f->tam * 4 <= f->lim)) {
                x->comps = (unsigned)f->tam; x->passo = (unsigned)f->passo; x->dados = P32(f->ptr + (size_t)base * f->passo);
            } else {
                conv[r] = aloca((size_t)nverts * 4 * sizeof(float));
                if (!conv[r]) { fallo = 1; break; }
                for (int v = 0; v < nverts; v++) sgl2_at_ler(f, base + v, conv[r] + (size_t)v * 4);
                x->comps = 4; x->passo = 16; x->dados = P32(conv[r]);
            }
            total += (unsigned)nverts * x->comps * 4;
        }
        if (total > G.caps.max_bytes) fallo = 1;
    }
    int hecho = 0;
    if (!fallo) {
        /* texturas de cada etapa: las de ES 2.0 o un negro si están incompletas */
        for (int et = 0; et < 2 && !fallo; et++) {
            ri_prog_t *q = et ? p->pf : p->pv;
            d->namostras[et] = (unsigned)q->namost;
            for (int i = 0; i < q->namost; i++) {
                int k = q->amost_tipo[i] == RI_TEX_CUBO;
                int un = p->amost[et][i];
                GLuint id = c->un_tex[k][un];
                s2_tex_t *t = id ? sgl2_tex(id) : &c->tex0[k];
                gpu3d_v4_amostra_t *s = &d->am[et][i];
                uint32_t rec = t ? tex_gpu(t) : 0;
                if (!rec && (G.estado <= 0 || (t && sgl2_tex_completa(t)))) { fallo = 1; break; }   /* completa y no cabe: la CPU */
                if (!rec) { s->rec = k ? ID_NEGROCUBO : ID_NEGRO2D; s->wrap_s = s->wrap_t = 1; }
                else { s->rec = rec; amostra(t, s); }
            }
        }
        if (!fallo && alvo_gpu(c, &a)) {
            d->programa = (unsigned)(p->gslot - 1);
            d->modo = modo == GL_TRIANGLES ? G3D4_PRIM_TRIANGULOS : modo == GL_TRIANGLE_STRIP ? G3D4_PRIM_TIRA : G3D4_PRIM_LEQUE;
            d->nverts = (unsigned)nverts; d->n = (unsigned)n; d->tipo_ind = (unsigned)tipo_gpu; d->indices = P32(ind);
            d->alvo_cor = a.g->id; d->alvo_ze = (a.zbits > 0 || a.estbits > 0) ? a.gz->id : 0;
            d->unif[0] = P32(p->uni[0]); d->nunif[0] = (unsigned)p->pv->nuni;
            d->unif[1] = P32(p->uni[1]); d->nunif[1] = (unsigned)p->pf->nuni;
            estado(c, &a, &d->e);
            int r = op_v4(GPU3D_OP_V4_DESENHAR, d);
            if (r == 0) {
                hecho = 1;
                a.g->sujo = 1; c->gpu_pend = 1;
                G.n_des++;
            }
        }
    }
    if (!hecho) G.n_cpu++;
    for (int i = 0; i < G3D4_ATRIBS; i++) if (conv[i]) libera(conv[i]);
    if (d) libera(d);
    if (ind) libera(ind);
    return hecho;
}

/* ------------------------------------------------------------ limpiar */
int sgl2_gpu_limpar(sgl2_ctx_t *c, GLbitfield m)
{
    if (!gpu_pronta(c)) return 0;
    s2_alvo_t a;
    if (!sgl2_alvo_x(c, &a, 1) || !a.gpu_ok || a.larg < 1 || a.alt < 1 || a.larg > (int)G.caps.max_lado || a.alt > (int)G.caps.max_lado) return 0;
    if (a.estbits > 0 && !(G.caps.bits & G3D4_C_ESTENCIL)) { a.estbits = 0; a.est = NULL; }
    if (!alvo_gpu(c, &a)) return 0;
    int tem_z = a.zbits > 0 || a.estbits > 0;
    unsigned mc = (m & GL_COLOR_BUFFER_BIT) ? 1u : 0u, mz = (m & GL_DEPTH_BUFFER_BIT) && a.z && c->masc_z ? 2u : 0u;
    unsigned me = (m & GL_STENCIL_BUFFER_BIT) && a.estbits > 0 && a.est ? 4u : 0u;
    if (!(mc | mz | me)) return 1;
    /* ¿entera? (sin tijera que recorte, todas las máscaras activas) */
    int entera = !c->tij_on || (c->tij[0] <= 0 && c->tij[1] <= 0 && c->tij[0] + c->tij[2] >= a.larg && c->tij[1] + c->tij[3] >= a.alt);
    int cmask = c->masc_cor[0] && c->masc_cor[1] && c->masc_cor[2] && (c->masc_cor[3] || !a.alfa);
    if (mc && !cmask) entera = 0;
    if (me && (c->est[0].masc_e & 0xFF) != 0xFF) entera = 0;
    int r;
    if (entera) {
        gpu3d_v4_limpar_t l;
        memset(&l, 0, sizeof l);
        l.alvo_cor = a.g->id; l.alvo_ze = tem_z ? a.gz->id : 0; l.mascara = mc | mz | me;
        for (int i = 0; i < 4; i++) l.cor[i] = fb(c->cor_limpa[i]);
        double z = (double)c->z_limpo;
        uint32_t w[2]; memcpy(w, &z, 8);
        l.z_lo = w[0]; l.z_hi = w[1]; l.est = (unsigned)c->est_limpo & 0xFFu;
        r = op_v4(GPU3D_OP_V4_LIMPAR, &l);
    } else {
        /* limpieza parcial: un rectángulo con el programa de color constante, con las máscaras y la tijera del estado */
        static float quad[4][4] = { { -1, -1, 0, 1 }, { 1, -1, 0, 1 }, { -1, 1, 0, 1 }, { 1, 1, 0, 1 } };
        float q[4][4];
        float zc = c->z_limpo * 2.0f - 1.0f;
        for (int i = 0; i < 4; i++) { q[i][0] = quad[i][0]; q[i][1] = quad[i][1]; q[i][2] = zc; q[i][3] = 1; }
        gpu3d_v4_des_t *d = aloca_zerado(sizeof *d);
        if (!d) return 0;
        d->programa = SLOT_LIMPAR; d->modo = G3D4_PRIM_TIRA; d->nverts = 4; d->n = 4; d->natrib = 1;
        d->at[0].ativo = 1; d->at[0].comps = 4; d->at[0].passo = 16; d->at[0].dados = P32(q);
        float cor[4]; memcpy(cor, c->cor_limpa, 16);
        d->unif[1] = P32(cor); d->nunif[1] = 1;
        d->alvo_cor = a.g->id; d->alvo_ze = tem_z ? a.gz->id : 0;
        gpu3d_v4_estado_t *e = &d->e;
        e->masc_cor = mc ? ((c->masc_cor[0] ? 1u : 0u) | (c->masc_cor[1] ? 2u : 0u) | (c->masc_cor[2] ? 4u : 0u) | (c->masc_cor[3] && a.alfa ? 8u : 0u)) : 0u;
        e->z_on = mz ? 1u : 0u; e->z_func = 7; e->z_escrita = mz ? 1u : 0u;
        if (me) {
            e->est_on = 1;
            for (int i = 0; i < 2; i++) {
                e->est[i].func = 7; e->est[i].ref = (unsigned)c->est_limpo & 0xFFu; e->est[i].masc_v = 0xFF; e->est[i].masc_e = c->est[0].masc_e & 0xFFu;
                e->est[i].falha = e->est[i].zfalha = e->est[i].zpassa = 2;
            }
        }
        e->tij_on = (unsigned)c->tij_on;
        for (int i = 0; i < 4; i++) { e->tij[i] = c->tij[i]; }
        e->vp[0] = 0; e->vp[1] = 0; e->vp[2] = a.larg; e->vp[3] = a.alt;
        e->z_escala = fb(0.5f); e->z_trasl = fb(0.5f);
        r = op_v4(GPU3D_OP_V4_DESENHAR, d);
        libera(d);
    }
    if (r != 0) return 0;
    a.g->sujo = 1; c->gpu_pend = 1;
    G.n_lim++;
    return 1;
}

/* ------------------------------------------------------------ la autoprueba (contra la CPU) */
#define AP_L 64
#define AP_A 48
static const char *AP_VS =
    "attribute vec2 a_pos; attribute vec4 a_cor; attribute vec2 a_tc; uniform vec2 u_d; varying vec4 v_cor; varying vec2 v_tc;\n"
    "void main() { v_cor = a_cor; v_tc = a_tc; gl_Position = vec4(a_pos + u_d, 0.0, 1.0); }\n";
static const char *AP_FS =
    "precision mediump float; uniform sampler2D u_tex; varying vec4 v_cor; varying vec2 v_tc;\n"
    "void main() { vec4 t = texture2D(u_tex, v_tc); gl_FragColor = vec4(t.rgb * v_cor.rgb, v_cor.a); }\n";

/* dibuja la escena en el contexto actual (el de la autoprueba) y deja los píxeles en «px» */
static int ap_escena(unsigned char *px)
{
    GLuint vs = glCreateShader(GL_VERTEX_SHADER), fs = glCreateShader(GL_FRAGMENT_SHADER), pr = glCreateProgram();
    glShaderSource(vs, 1, &AP_VS, 0); glCompileShader(vs);
    glShaderSource(fs, 1, &AP_FS, 0); glCompileShader(fs);
    glAttachShader(pr, vs); glAttachShader(pr, fs);
    glBindAttribLocation(pr, 0, "a_pos"); glBindAttribLocation(pr, 1, "a_cor"); glBindAttribLocation(pr, 2, "a_tc");
    glLinkProgram(pr);
    GLint ok = 0;
    glGetProgramiv(pr, GL_LINK_STATUS, &ok);
    if (!ok) return 0;
    uint32_t tx[4] = { 0xFFFFFFFFu, 0xFF2060C0u, 0xFFC06020u, 0xFFFFFFFFu };     /* damero de 2x2 */
    unsigned char b[16];
    for (int i = 0; i < 4; i++) { b[i * 4] = (unsigned char)(tx[i] >> 16); b[i * 4 + 1] = (unsigned char)(tx[i] >> 8); b[i * 4 + 2] = (unsigned char)tx[i]; b[i * 4 + 3] = 255; }
    GLuint t;
    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, b);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glUseProgram(pr);
    glUniform1i(glGetUniformLocation(pr, "u_tex"), 0);
    GLint ud = glGetUniformLocation(pr, "u_d");
    static const float pos[2][3][2] = { { { -0.8f, -0.7f }, { 0.7f, -0.6f }, { -0.1f, 0.8f } }, { { -0.5f, -0.5f }, { 0.9f, 0.1f }, { 0.2f, 0.9f } } };
    static const float cor[3][4] = { { 1, 0.5f, 0.5f, 1 }, { 0.5f, 1, 0.5f, 0.6f }, { 0.5f, 0.5f, 1, 0.6f } };
    static const float tc[3][2] = { { 0, 0 }, { 1, 0 }, { 0.5f, 1 } };
    glViewport(0, 0, AP_L, AP_A);
    glClearColor(0.1f, 0.2f, 0.3f, 1.0f);
    glClearDepthf(1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnableVertexAttribArray(0); glEnableVertexAttribArray(1); glEnableVertexAttribArray(2);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 0, cor);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 0, tc);
    for (int k = 0; k < 2; k++) {
        glUniform2f(ud, k ? 0.05f : 0.0f, 0.0f);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, pos[k]);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glDisable(GL_BLEND);
    }
    glDisableVertexAttribArray(0); glDisableVertexAttribArray(1); glDisableVertexAttribArray(2);
    glReadPixels(0, 0, AP_L, AP_A, GL_RGBA, GL_UNSIGNED_BYTE, px);
    glDeleteTextures(1, &t);
    glUseProgram(0);
    glDeleteProgram(pr); glDeleteShader(vs); glDeleteShader(fs);
    return 1;
}

/* ¿qué cara es la de frente para la GPU? Un triángulo antihorario con GL_CULL_FACE (se elimina la trasera): si no se
 * ve, esta GPU da la vuelta a las caras y se compensa en cada dibujo. 1 si se pudo decidir. */
static const char *OR_VS = "attribute vec2 p; void main() { gl_Position = vec4(p, 0.0, 1.0); }\n";
static const char *OR_FS = "precision mediump float; void main() { gl_FragColor = vec4(1.0); }\n";
static int orientacao(void)
{
    GLuint vs = glCreateShader(GL_VERTEX_SHADER), fs = glCreateShader(GL_FRAGMENT_SHADER), pr = glCreateProgram();
    glShaderSource(vs, 1, &OR_VS, 0); glCompileShader(vs);
    glShaderSource(fs, 1, &OR_FS, 0); glCompileShader(fs);
    glAttachShader(pr, vs); glAttachShader(pr, fs);
    glBindAttribLocation(pr, 0, "p");
    glLinkProgram(pr);
    static const float tri[3][2] = { { -0.8f, -0.8f }, { 0.8f, -0.8f }, { 0.0f, 0.8f } };    /* antihorario */
    static unsigned char px[AP_L * AP_A * 4];
    int visto[2] = { 0, 0 };
    glUseProgram(pr);
    glViewport(0, 0, AP_L, AP_A);
    glDisable(GL_DEPTH_TEST); glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE); glCullFace(GL_BACK); glFrontFace(GL_CCW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, tri);
    for (int k = 0; k < 2; k++) {
        G.inv_frente = k;
        glClearColor(0, 0, 0, 1); glClear(GL_COLOR_BUFFER_BIT);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glReadPixels(0, 0, AP_L, AP_A, GL_RGBA, GL_UNSIGNED_BYTE, px);
        visto[k] = px[((AP_A / 3) * AP_L + AP_L / 2) * 4] > 128;
    }
    glDisableVertexAttribArray(0);
    glDisable(GL_CULL_FACE);
    glUseProgram(0);
    glDeleteProgram(pr); glDeleteShader(vs); glDeleteShader(fs);
    G.inv_frente = visto[0] ? 0 : visto[1] ? 1 : 0;
    return visto[0] != visto[1];
}

static int autoprova(void)
{
    static uint32_t destino[AP_L * AP_A];
    struct sgl_ctx *base = sgl_criar_contexto_mem(destino, AP_L, AP_A, 16);
    if (!base) return 1;      /* sin memoria: no se puede probar, se confía */
    sgl2_ctx_t *anterior = sgl2_C, *c = sgl2_criar(base, 16, 0);
    if (!c) { sgl_destruir_contexto(base); return 1; }
    static unsigned char cpu[AP_L * AP_A * 4], gpu[AP_L * AP_A * 4];
    int modo = G.modo;
    sgl2_usar(c);
    G.modo = 0;
    int a = ap_escena(cpu);
    G.modo = 1;
    unsigned d0 = G.n_des;
    int b = a && ap_escena(gpu);
    int usada = G.n_des > d0;
    int orient = b && orientacao();
    sgl2_usar(anterior);
    G.modo = modo;
    sgl2_destruir(c);
    sgl_destruir_contexto(base);
    unsigned dif = 0;
    if (b) {
        for (int i = 0; i < AP_L * AP_A; i++) {
            int mal = 0;
            for (int k = 0; k < 3; k++) { int x = cpu[i * 4 + k] - gpu[i * 4 + k]; if (x < 0) x = -x; if (x > 10) mal = 1; }
            dif += (unsigned)mal;
        }
    }
    int bien = a && b && usada && orient && dif <= (unsigned)(AP_L * AP_A) / 25;    /* hasta un 4 % de píxeles distintos (bordes) */
    op(GPU3D_OP_V4_INFORME, bien ? 1 : 2, dif, (unsigned)G.inv_frente, 0);
    return bien;
}
