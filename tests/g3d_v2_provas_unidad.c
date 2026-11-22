// SPDX-License-Identifier: GPL-2.0-or-later
/* g3d_v2_provas_unidad.c - prueba en el PC de las pruebas v2 de los drivers (D426: kernel/dispositivos/
 * g3d_v2_provas.c).
 *
 *   gcc -O1 -fno-builtin -Wall -Wno-builtin-declaration-mismatch -I kernel/inc -o g3d_v2_provas_unidad \
 *       tests/g3d_v2_provas_unidad.c kernel/dispositivos/g3d_v2_provas.c kernel/gui/gpu3d_mem.c
 *
 * Las pruebas que hace el driver en la máquina tienen sus píxeles esperados calculados a mano. Para no
 * quitarle a una tarjeta una capacidad que sí tiene por una cuenta mal hecha, aquí un driver «perfecto» por la
 * CPU (rectángulos, que es lo único que dibujan esas pruebas, con la tubería de docs/gpu3d.md «gpu3d v2») tiene
 * que pasarlas todas; y uno con cada capacidad rota a propósito tiene que perder justo esa. */
#include "g3d_v2_provas.h"

int printf(const char *, ...);
void *memset(void *d, int c, size_t n) { unsigned char *p = d; while (n--) *p++ = (unsigned char)c; return d; }
void *memcpy(void *d, const void *s, size_t n) { unsigned char *a = d; const unsigned char *b = s; while (n--) *a++ = *b++; return d; }
#include "../tests/g3d_combinar_ref.h"

static char log_buf[4096];
void klog(const char *f, ...) { (void)f; }
static unsigned char heap[64 * 1024];
void *kmalloc(size_t n) { return n <= sizeof heap ? heap : 0; }
void kfree(void *p) { (void)p; }

static int falhas, feitas;
#define VER(c, ...) do { feitas++; if (!(c)) { falhas++; printf("FALLO: " __VA_ARGS__); printf("\n"); } } while (0)

/* ------------------------------------------------------------ el driver por la CPU */

#define MAXW 64
#define MAXH 32
static uint32_t roto;            /* G3D_CAP_* que se rompen a propósito; 0x80000000: DXT */
static uint32_t pr_cor[MAXW * MAXH], *cor = pr_cor;
static float pr_z[MAXW * MAXH], dz[16 * 16];
static unsigned char pr_est[MAXW * MAXH], dest_est[16 * 16];
static float *zb = pr_z;
static unsigned char *eb = pr_est;
static int W, H, dest_v = -1;
static struct { int l, a; uint32_t px[16 * 16]; } tex[32];

static float fb(uint32_t b) { union { uint32_t u; float f; } x; x.u = b; return x.f; }
static float canal(uint32_t c, int sh) { return (float)((c >> sh) & 0xFF) / 255.0f; }
static ref_v4 v4(uint32_t c) { ref_v4 r = { { canal(c, 16), canal(c, 8), canal(c, 0), canal(c, 24) } }; return r; }
static uint32_t de_v4(ref_v4 v) { uint32_t r = 0; int sh[4] = { 16, 8, 0, 24 }; for (int c = 0; c < 4; c++) r |= (uint32_t)(ref_sat(v.c[c]) * 255.0f + 0.5f) << sh[c]; return r; }

static int f_alvo(int l, int a, int z) { (void)z; W = l; H = a; return 0; }
static int f_limpar(uint32_t m, uint32_t c)
{
    for (int i = 0; i < W * H; i++) { if (m & 1) cor[i] = 0xFF000000u | c; if (m & 2) zb[i] = 1; }
    return 0;
}
static int f_textura(int v, int l, int a, const uint32_t *p)
{
    if (!p) { tex[v].l = 0; return 0; }
    if (l * a > 256) return 1;
    tex[v].l = l; tex[v].a = a;
    for (int i = 0; i < l * a; i++) tex[v].px[i] = p[i];
    return 0;
}
static uint32_t rgb565(uint32_t c) { return 0xFF000000u | (((c >> 11) & 31) * 255 / 31) << 16 | (((c >> 5) & 63) * 255 / 63) << 8 | ((c & 31) * 255 / 31); }
static int f_textura2(const gpu3d_textura2_t *t)
{
    if (!t->dados) return f_textura((int)t->vaga, 0, 0, NULL);
    if (t->formato == G3D_FMT_ARGB8888) return f_textura((int)t->vaga, (int)t->larg, (int)t->alt, (const uint32_t *)(uintptr_t)t->dados);
    if (roto & 0x80000000u) return 1;
    const unsigned char *b = (const unsigned char *)(uintptr_t)t->dados;
    if (t->formato != G3D_FMT_DXT1) b += 8;   /* el alfa de DXT3/5 no se mira aquí */
    uint32_t c0 = b[0] | (b[1] << 8), c1 = b[2] | (b[3] << 8), idx = b[4] | (b[5] << 8) | (b[6] << 16) | ((uint32_t)b[7] << 24);
    tex[t->vaga].l = 4; tex[t->vaga].a = 4;
    for (int i = 0; i < 16; i++) { uint32_t k = (idx >> (2 * i)) & 3; tex[t->vaga].px[i] = k == 0 ? rgb565(c0) : k == 1 ? rgb565(c1) : 0xFF808080u; }
    return 0;
}

static float envolver(float s, int env)
{
    if (env == G3D_ENV_REPETIR) { s -= (float)(int)s; if (s < 0) s += 1; return s; }
    if (env == G3D_ENV_ESPELHO) { float f = s - 2 * (float)(int)(s / 2); if (f < 0) f += 2; return f > 1 ? 2 - f : f; }
    return s < 0 ? 0 : s > 0.9999f ? 0.9999f : s;
}

static bool cmp(int f, float a, float b) { switch (f) { case 0: return false; case 1: return a < b; case 2: return a == b; case 3: return a <= b;
    case 4: return a > b; case 5: return a != b; case 6: return a >= b; default: return true; } }
static unsigned char est_op(int op, unsigned char v, unsigned char ref)
{
    switch (op) { case 1: return 0; case 2: return ref; case 3: return v == 255 ? 255 : v + 1; case 4: return v ? v - 1 : 0;
    case 5: return (unsigned char)~v; case 6: return (unsigned char)(v + 1); case 7: return (unsigned char)(v - 1); default: return v; }
}
static float fator(int f, ref_v4 s, ref_v4 d, int c)
{
    switch (f) { case 0: return 0; case 1: return 1; case 2: return s.c[3]; case 3: return 1 - s.c[3]; case 4: return s.c[c]; case 5: return 1 - s.c[c];
    case 6: return d.c[c]; case 7: return 1 - d.c[c]; case 8: return d.c[3]; case 9: return 1 - d.c[3];
    default: return c == 3 ? 1 : (s.c[3] < 1 - d.c[3] ? s.c[3] : 1 - d.c[3]); }
}
static uint32_t logica(int op, uint32_t s, uint32_t d)
{
    switch (op) { case 0: return 0; case 1: return s & d; case 2: return s & ~d; case 3: return s; case 4: return ~s & d; case 5: return d;
    case 6: return s ^ d; case 7: return s | d; case 8: return ~(s | d); case 9: return ~(s ^ d); case 10: return ~d; case 11: return s | ~d;
    case 12: return ~s; case 13: return ~s | d; case 14: return ~(s & d); default: return 0xFFFFFFFFu; }
}

static int f_desenhar2(const gpu3d_estado2_t *e, const gpu3d_vert2_t *v, int n, g3d_vala_t *vala)
{
    (void)vala;
    for (int q = 0; q + 6 <= n; q += 6) {
        float x0 = 1e9f, x1 = -1e9f, y0 = 1e9f, y1 = -1e9f, s0 = 0, s1 = 0, t0 = 0, t1 = 0;
        for (int i = q; i < q + 6; i++) {
            float x = fb(v[i].x), y = fb(v[i].y);
            if (x < x0) { x0 = x; s0 = fb(v[i].tc[0][0]); } if (x > x1) { x1 = x; s1 = fb(v[i].tc[0][0]); }
            if (y < y0) { y0 = y; t0 = fb(v[i].tc[0][1]); } if (y > y1) { y1 = y; t1 = fb(v[i].tc[0][1]); }
        }
        float z = fb(v[q].z);
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++) {
                float px = (float)x + 0.5f, py = (float)y + 0.5f;
                if (px < x0 || px >= x1 || py < y0 || py >= y1) continue;
                if ((e->flags & G3D2_TIJERA) && !(roto & G3D_CAP_TIJERA) &&
                    (x < e->tijera_x || y < e->tijera_y || x >= e->tijera_x + e->tijera_l || y >= e->tijera_y + e->tijera_a)) continue;
                float s = s0 + (px - x0) / (x1 - x0) * (s1 - s0), t = t0 + (py - y0) / (y1 - y0) * (t1 - t0);
                ref_entrada_t in;
                in.prim = v4(v[q].cor); in.espec = v4(v[q].espec); in.neblina = v4(e->cor_neblina);
                for (int u = 0; u < 4; u++) {
                    in.cons[u] = v4(e->u[u].constante);
                    if (u < e->unidades && e->u[u].ativa) {
                        int vg = e->u[u].vaga;
                        int env_s = e->u[u].env_s == G3D_ENV_ESPELHO && (roto & G3D_CAP_TEX_ESPELHO) ? G3D_ENV_REPETIR : e->u[u].env_s;
                        if (env_s == G3D_ENV_REPETIR && (roto & G3D_CAP_TEX_NPOT) && (tex[vg].l & (tex[vg].l - 1))) env_s = G3D_ENV_BORDE;
                        int tx = (int)(envolver(s, env_s) * (float)tex[vg].l), ty = (int)(envolver(t, e->u[u].env_t) * (float)tex[vg].a);
                        in.tex[u] = v4(tex[vg].px[ty * tex[vg].l + tx]);
                    }
                }
                gpu3d_estado2_t ee = *e;
                if (roto & G3D_CAP_COMBINAR) for (int u = 0; u < 4; u++) ee.u[u].rgb_escala = 1;
                if (roto & G3D_CAP_DOT3) for (int u = 0; u < 4; u++) if (ee.u[u].rgb_func >= G3D_CB_DOT3_RGB) ee.u[u].rgb_func = G3D_CB_MODULAR;
                if (roto & G3D_CAP_NEBLINA) ee.flags &= ~G3D2_NEBLINA;
                ref_v4 o = ref_combinar(&ee, &in);
                int i = y * W + x;
                if ((e->flags & G3D2_ALFA_TESTE) && !(roto & G3D_CAP_ALFA_TESTE) &&
                    !cmp(e->func_alfa, (float)(int)(o.c[3] * 255 + 0.5f), (float)e->ref_alfa)) continue;
                bool est = (e->flags & G3D2_ESTENCIL) && !(roto & G3D_CAP_ESTENCIL);
                if (est && !cmp(e->est_func, (float)(e->est_ref & e->est_mascara), (float)(eb[i] & e->est_mascara))) {
                    eb[i] = (unsigned char)((eb[i] & ~e->est_escrita) | (est_op(e->est_falha, eb[i], e->est_ref) & e->est_escrita));
                    continue;
                }
                int fz = (roto & G3D_CAP_Z_FUNCS) && e->func_z == G3D_CMP_IGUAL ? G3D_CMP_SEMPRE : e->func_z;
                if ((e->flags & G3D2_Z_TESTE) && !cmp(fz, z, zb[i])) {
                    if (est) eb[i] = (unsigned char)((eb[i] & ~e->est_escrita) | (est_op(e->est_zfalha, eb[i], e->est_ref) & e->est_escrita));
                    continue;
                }
                if (est) eb[i] = (unsigned char)((eb[i] & ~e->est_escrita) | (est_op(e->est_zpassa, eb[i], e->est_ref) & e->est_escrita));
                if ((e->flags & G3D2_Z_TESTE) && (e->flags & G3D2_Z_ESCRITA)) zb[i] = z;
                uint32_t sv = de_v4(o), dv = cor[i], r;
                if ((e->flags & G3D2_LOGICA) && !(roto & G3D_CAP_LOGICA)) r = logica(e->op_logica, sv, dv);
                else if (e->flags & G3D2_MEZCLA) {
                    ref_v4 sc = v4(sv), dc = v4(dv), res;
                    for (int c = 0; c < 4; c++) {
                        bool alfa = c == 3 && !(roto & G3D_CAP_MEZCLA_SEP);
                        int fo = c == 3 && alfa ? e->mez_orig_alfa : c == 3 ? e->mez_orig_rgb : e->mez_orig_rgb;
                        int fd = c == 3 && alfa ? e->mez_dest_alfa : c == 3 ? e->mez_dest_rgb : e->mez_dest_rgb;
                        int eq = c == 3 ? e->mez_eq_alfa : e->mez_eq_rgb;
                        if ((roto & G3D_CAP_MEZCLA_SEP) && c < 3) { fo = e->mez_orig_alfa; fd = e->mez_dest_alfa; }
                        if ((roto & G3D_CAP_MEZCLA_EXT) && fo > 3) fo = 1;
                        if (roto & G3D_CAP_MEZCLA_EQ) eq = 0;
                        float a = sc.c[c] * fator(fo, sc, dc, c), b = dc.c[c] * fator(fd, sc, dc, c);
                        res.c[c] = eq == G3D_EQ_SUBTRAI ? a - b : eq == G3D_EQ_SUBTRAI_INV ? b - a : a + b;
                    }
                    r = de_v4(res);
                } else r = sv;
                uint32_t m = 0, mc = (roto & G3D_CAP_MASCARAS) ? 15 : e->mascara_cor;
                if (mc & G3D_MC_R) m |= 0xFF0000u;
                if (mc & G3D_MC_G) m |= 0xFF00u;
                if (mc & G3D_MC_B) m |= 0xFFu;
                if (mc & G3D_MC_A) m |= 0xFF000000u;
                cor[i] = (r & m) | (dv & ~m);
            }
    }
    return 0;
}

static int f_limpar2(const gpu3d_limpar2_t *l)
{
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            if (l->tijera && !(roto & G3D_CAP_TIJERA) && (x < l->tx || y < l->ty || x >= l->tx + l->tl || y >= l->ty + l->ta)) continue;
            int i = y * W + x;
            if (l->mascara & G3D_LIMPA_COR) {
                uint32_t m = 0;
                if (l->mascara_cor & G3D_MC_R) m |= 0xFF0000u;
                if (l->mascara_cor & G3D_MC_G) m |= 0xFF00u;
                if (l->mascara_cor & G3D_MC_B) m |= 0xFFu;
                if (l->mascara_cor & G3D_MC_A) m |= 0xFF000000u;
                cor[i] = (l->cor & m) | (cor[i] & ~m);
            }
            if (l->mascara & G3D_LIMPA_Z) zb[i] = (float)l->z / 16777215.0f;
            if (l->mascara & G3D_LIMPA_EST) eb[i] = (unsigned char)((eb[i] & ~l->mascara_est) | (l->estencil & l->mascara_est));
        }
    return 0;
}

static int f_ler(uint32_t *d, int passo)
{
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) d[y * passo + x] = cor[y * W + x] & 0xFFFFFFu;
    return 0;
}

static int sw, sh_;
static int f_destino(int v, int zbits, int est)
{
    (void)zbits; (void)est;
    if (roto & G3D_CAP_DESTINO_TEX) return 1;
    if (v < 0) { if (dest_v >= 0) { cor = pr_cor; zb = pr_z; eb = pr_est; W = sw; H = sh_; dest_v = -1; } return 0; }
    sw = W; sh_ = H;
    cor = tex[v].px; zb = dz; eb = dest_est; W = tex[v].l; H = tex[v].a; dest_v = v;
    return 0;
}

static const g3d_v2_ops_t ops = { f_alvo, f_limpar, f_textura, f_textura2, f_desenhar2, f_limpar2, f_ler, f_destino };

int main(void)
{
    (void)log_buf;
    const uint32_t todas = G3D_CAP_V2 | G3D_CAP_COMBINAR | G3D_CAP_DOT3 | G3D_CAP_ALFA_TESTE | G3D_CAP_ESTENCIL | G3D_CAP_TIJERA |
                           G3D_CAP_MASCARAS | G3D_CAP_Z_FUNCS | G3D_CAP_MEZCLA_EXT | G3D_CAP_MEZCLA_SEP | G3D_CAP_MEZCLA_EQ |
                           G3D_CAP_LOGICA | G3D_CAP_NEBLINA | G3D_CAP_TEX_ESPELHO | G3D_CAP_TEX_NPOT | G3D_CAP_TEX_ANISO |
                           G3D_CAP_LIMPAR2 | G3D_CAP_DESTINO_TEX;
    uint32_t un = 4, fm = 0xF;
    uint32_t ok = g3d_v2_provar(&ops, "cpu", todas, &un, &fm);
    VER(ok == todas, "el driver por la CPU pasa todas (0x%x de 0x%x; faltan 0x%x)", ok, todas, todas & ~ok);
    VER(un == 4 && fm == 0xF, "cuatro unidades y los cuatro formatos (%u, %x)", un, fm);
    /* Cada capacidad rota se pierde (y la mayoría de las demás siguen). */
    static const uint32_t cada[] = { G3D_CAP_COMBINAR, G3D_CAP_DOT3, G3D_CAP_ALFA_TESTE, G3D_CAP_ESTENCIL, G3D_CAP_TIJERA, G3D_CAP_MASCARAS,
                                     G3D_CAP_Z_FUNCS, G3D_CAP_MEZCLA_EXT, G3D_CAP_MEZCLA_SEP, G3D_CAP_MEZCLA_EQ, G3D_CAP_LOGICA,
                                     G3D_CAP_NEBLINA, G3D_CAP_TEX_ESPELHO, G3D_CAP_TEX_NPOT, G3D_CAP_DESTINO_TEX };
    for (unsigned i = 0; i < sizeof cada / sizeof cada[0]; i++) {
        roto = cada[i]; un = 4; fm = 0xF;
        ok = g3d_v2_provar(&ops, "cpu", todas, &un, &fm);
        VER(!(ok & cada[i]) && (ok & G3D_CAP_V2), "rota 0x%x: no se anuncia (0x%x)", cada[i], ok);
    }
    roto = 0x80000000u; un = 4; fm = 0xF;
    ok = g3d_v2_provar(&ops, "cpu", todas, &un, &fm);
    VER(fm == 1, "sin DXT: solo ARGB (%x)", fm);
    printf("g3d_v2_provas_unidad: %d comprobaciones, %d fallos\n", feitas, falhas);
    return falhas ? 1 : 0;
}
