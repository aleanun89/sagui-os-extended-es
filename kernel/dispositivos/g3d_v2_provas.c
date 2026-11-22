// SPDX-License-Identifier: GPL-2.0-or-later
/* g3d_v2_provas.c - pruebas de las capacidades gpu3d v2 de un driver (D426). Ver g3d_v2_provas.h.
 *
 * Cada prueba es un dibujo pequeño cuyo resultado se sabe de antemano (cuentas en los comentarios, con los
 * bytes de 0 a 255) y se comprueba en uno o varios píxeles con una tolerancia de 6 por canal (el hardware
 * redondea distinto que la cuenta exacta). Todo con enteros: los flotantes de los vértices se construyen con
 * g3d_float_de_fixo. */
#include "g3d_v2_provas.h"
#include "console.h"
#include "memoria.h"
#include "texto.h"

#define PL 64          /* destino de la prueba */
#define PA 32
#define TOL 6
#define V_A 28         /* huecos de textura de la prueba */
#define V_B 29
#define V_C 30
#define V_D 31

typedef struct {
    const g3d_v2_ops_t *o;
    const char *nome;
    uint32_t *px;          /* el destino leído */
    bool gpu_mal;          /* el driver devolvió G3D_FALHA: no se sigue */
} ctx_t;

static uint32_t fl(int32_t v, int frac) { return g3d_float_de_fixo(v, frac); }

static gpu3d_estado2_t estado_base(void)
{
    gpu3d_estado2_t e;
    memset(&e, 0, sizeof e);
    e.versao = 2;
    e.func_z = G3D_CMP_SEMPRE; e.func_alfa = G3D_CMP_SEMPRE; e.mascara_cor = 15;
    e.mez_orig_rgb = e.mez_orig_alfa = G3D_F_UM; e.mez_dest_rgb = e.mez_dest_alfa = G3D_F_ZERO;
    e.op_logica = G3D_LOG_COPIA; e.est_func = G3D_CMP_SEMPRE; e.est_mascara = 0xFF; e.est_escrita = 0xFF;
    return e;
}

/* Unidad que sustituye por su textura (o modula con lo anterior). */
static void unidade(gpu3d_unidade_t *u, int vaga, uint8_t rgb_func, uint8_t f0, uint8_t f1)
{
    memset(u, 0, sizeof *u);
    u->ativa = 1; u->vaga = (uint8_t)vaga; u->tipo = G3D_TEX_2D; u->env_s = u->env_t = G3D_ENV_BORDE; u->aniso = 1;
    u->rgb_func = rgb_func; u->alfa_func = G3D_CB_SUBSTITUIR;
    u->rgb_escala = u->alfa_escala = 1;
    u->rgb_fonte[0] = f0; u->rgb_fonte[1] = f1; u->rgb_fonte[2] = G3D_FT_CONSTANTE;
    u->rgb_opd[0] = u->rgb_opd[1] = u->rgb_opd[2] = G3D_OPD_COR;
    u->alfa_fonte[0] = G3D_FT_ANTERIOR; u->alfa_fonte[1] = G3D_FT_TEXTURA; u->alfa_fonte[2] = G3D_FT_CONSTANTE;
    u->alfa_opd[0] = u->alfa_opd[1] = u->alfa_opd[2] = G3D_OPD_ALFA;
}

/* Un rectángulo [x0, x1) x [y0, y1) con su color, especular, z (de 0 a 65536) y coordenadas (s0..s1, t0..t1)
 * en coma fija de 16 bits, iguales para las cuatro unidades. */
static int ret(ctx_t *c, const gpu3d_estado2_t *e, int x0, int y0, int x1, int y1, uint32_t cor, uint32_t espec, int32_t z,
               int32_t s0, int32_t s1, int32_t t0, int32_t t1)
{
    if (c->gpu_mal) return G3D_FALHA;
    gpu3d_vert2_t v[6];
    const int xs[6] = { x0, x1, x1, x0, x1, x0 }, ys[6] = { y0, y0, y1, y0, y1, y1 };
    for (int i = 0; i < 6; i++) {
        memset(&v[i], 0, sizeof v[i]);
        v[i].x = fl(xs[i], 0); v[i].y = fl(ys[i], 0); v[i].z = fl(z, 16); v[i].rhw = fl(1, 0);
        v[i].cor = cor; v[i].espec = espec;
        int32_t s = xs[i] == x0 ? s0 : s1, t = ys[i] == y0 ? t0 : t1;
        for (int u = 0; u < 4; u++) { v[i].tc[u][0] = fl(s, 16); v[i].tc[u][1] = fl(t, 16); v[i].tc[u][3] = fl(1, 0); }
    }
    int r = c->o->desenhar2(e, v, 6, NULL);
    if (r == G3D_FALHA) c->gpu_mal = true;
    return r;
}

static bool limpar(ctx_t *c, uint32_t cor)
{
    if (c->gpu_mal) return false;
    int r = c->o->limpar(3, cor);
    if (r == G3D_FALHA) c->gpu_mal = true;
    return r == G3D_OK;
}

static bool ler(ctx_t *c)
{
    if (c->gpu_mal) return false;
    int r = c->o->ler(c->px, PL);
    if (r == G3D_FALHA) c->gpu_mal = true;
    return r == G3D_OK;
}

static int dif(uint32_t a, uint32_t b, int sh) { int d = (int)((a >> sh) & 0xFF) - (int)((b >> sh) & 0xFF); return d < 0 ? -d : d; }

/* ¿El píxel (x, y) es «esp» (0x00RRGGBB) con la tolerancia? Si no, al log. */
static bool pixel(ctx_t *c, int n, const char *que, int x, int y, uint32_t esp)
{
    uint32_t o = c->px[y * PL + x] & 0x00FFFFFFu;
    if (dif(o, esp, 16) <= TOL && dif(o, esp, 8) <= TOL && dif(o, esp, 0) <= TOL) return true;
    klog("[GPU3D] %s v2: prueba %d (%s) MAL: pixel (%d,%d) esperado %06x obtenido %06x\n", c->nome, n, que, x, y, esp, o);
    return false;
}

static int tex_solida(ctx_t *c, int vaga, uint32_t argb)
{
    uint32_t t[4] = { argb, argb, argb, argb };
    int r = c->o->textura(vaga, 2, 2, t);
    if (r == G3D_FALHA) c->gpu_mal = true;
    return r;
}

/* ------------------------------------------------------------ las pruebas */

/* 0: lo mínimo de v2 (una unidad MODULATE como v1, cuatro texels en su sitio). */
static bool p_base(ctx_t *c)
{
    static const uint32_t t[4] = { 0xFFFF0000u, 0xFF00FF00u, 0xFF0000FFu, 0xFFFFFFFFu };
    if (c->o->textura(V_A, 2, 2, t) != G3D_OK || !limpar(c, 0)) return false;
    gpu3d_estado2_t e = estado_base();
    e.unidades = 1;
    unidade(&e.u[0], V_A, G3D_CB_MODULAR, G3D_FT_TEXTURA, G3D_FT_PRIMARIA);
    if (ret(c, &e, 0, 0, 8, 8, 0xFFFFFFFFu, 0, 32768, 0, 65536, 0, 65536) != G3D_OK || !ler(c)) return false;
    return pixel(c, 0, "base", 2, 2, 0xFF0000) && pixel(c, 0, "base", 6, 2, 0x00FF00) && pixel(c, 0, "base", 2, 6, 0x0000FF) &&
           pixel(c, 0, "base", 6, 6, 0xFFFFFF);
}

/* 1: cuatro unidades con COMBINE.
 *   u0 = textura (0x80, 0, 0)                       -> (128, 0, 0)
 *   u1 = anterior + textura (0, 0, 0x80)            -> (128, 0, 128)
 *   u2 = interpolar(textura (255,0,0), anterior, alfa de la constante 0x40):
 *        255 * 64/255 + (128, 0, 128) * 191/255     -> (160, 0, 96)
 *   u3 = anterior - (1 - constante 0xEF, 0xFF, 0xEF) -> (144, 0, 80)
 * y una segunda: u0 = (0x60 + 0x40 - 0x80) * 2 = 0x40 (sumar con signo, escala 2); u1 = 0x40 * 0x80 / 255. */
static bool p_combinar(ctx_t *c)
{
    if (tex_solida(c, V_A, 0xFF800000u) || tex_solida(c, V_B, 0xFF000080u) || tex_solida(c, V_C, 0xFFFF0000u) ||
        tex_solida(c, V_D, 0xFF606060u) || !limpar(c, 0)) return false;
    gpu3d_estado2_t e = estado_base();
    e.unidades = 4;
    unidade(&e.u[0], V_A, G3D_CB_SUBSTITUIR, G3D_FT_TEXTURA, G3D_FT_TEXTURA);
    unidade(&e.u[1], V_B, G3D_CB_SOMAR, G3D_FT_ANTERIOR, G3D_FT_TEXTURA);
    unidade(&e.u[2], V_C, G3D_CB_INTERPOLAR, G3D_FT_TEXTURA, G3D_FT_ANTERIOR);
    e.u[2].rgb_opd[2] = G3D_OPD_ALFA; e.u[2].constante = 0x40000000u;
    unidade(&e.u[3], V_D, G3D_CB_SUBTRAIR, G3D_FT_ANTERIOR, G3D_FT_CONSTANTE);
    e.u[3].rgb_opd[1] = G3D_OPD_UM_MENOS_COR; e.u[3].constante = 0xFFEFFFEFu;
    if (ret(c, &e, 0, 0, 8, 8, 0xFFFFFFFFu, 0, 32768, 0, 65536, 0, 65536) != G3D_OK) return false;
    gpu3d_estado2_t e2 = estado_base();
    e2.unidades = 2;
    unidade(&e2.u[0], V_D, G3D_CB_SOMAR_SINAL, G3D_FT_TEXTURA, G3D_FT_PRIMARIA);
    e2.u[0].rgb_escala = 2;
    unidade(&e2.u[1], V_A, G3D_CB_MODULAR, G3D_FT_ANTERIOR, G3D_FT_TEXTURA);
    e2.u[1].rgb_fonte[1] = G3D_FT_CONSTANTE; e2.u[1].constante = 0xFF808080u;
    if (ret(c, &e2, 8, 0, 16, 8, 0xFF404040u, 0, 32768, 0, 65536, 0, 65536) != G3D_OK || !ler(c)) return false;
    return pixel(c, 1, "combinar 4 unidades", 4, 4, 0x900050) && pixel(c, 1, "combinar con signo y escala", 12, 4, 0x202020);
}

/* 2: DOT3: 4 * ((0.5-0.5)^2 + (1-0.5)^2 + (0.5-0.5)^2) = 1 -> blanco; con la alfa de la unidad = la del vértice. */
static bool p_dot3(ctx_t *c)
{
    if (tex_solida(c, V_A, 0xFF80FF80u) || !limpar(c, 0)) return false;
    gpu3d_estado2_t e = estado_base();
    e.unidades = 1;
    unidade(&e.u[0], V_A, G3D_CB_DOT3_RGB, G3D_FT_TEXTURA, G3D_FT_CONSTANTE);
    e.u[0].constante = 0xFF80FF80u;
    if (ret(c, &e, 0, 0, 8, 8, 0xFFFFFFFFu, 0, 32768, 0, 65536, 0, 65536) != G3D_OK || !ler(c)) return false;
    return pixel(c, 2, "dot3", 4, 4, 0xFFFFFF);
}

/* 3: prueba alfa (mayor que 0x80): el rojo de alfa 0x40 no se dibuja, el de 0xC0 sí. */
static bool p_alfa(ctx_t *c)
{
    if (!limpar(c, 0)) return false;
    gpu3d_estado2_t e = estado_base();
    e.flags = G3D2_ALFA_TESTE; e.func_alfa = G3D_CMP_MAIOR; e.ref_alfa = 0x80;
    if (ret(c, &e, 0, 0, 8, 8, 0x40FF0000u, 0, 32768, 0, 0, 0, 0) != G3D_OK ||
        ret(c, &e, 8, 0, 16, 8, 0xC0FF0000u, 0, 32768, 0, 0, 0, 0) != G3D_OK || !ler(c)) return false;
    return pixel(c, 3, "alfa", 4, 4, 0x000000) && pixel(c, 3, "alfa", 12, 4, 0xFF0000);
}

/* 4: esténcil con LIMPAR2: esténcil a 0; un rectángulo sin color pone 5 en x < 16; el verde con «igual a 5»
 * sobre x < 32 solo sale en x < 16. Y un LIMPAR2 con tijera y solo el canal rojo. */
static bool p_estencil(ctx_t *c)
{
    gpu3d_limpar2_t l;
    memset(&l, 0, sizeof l);
    l.mascara = G3D_LIMPA_COR | G3D_LIMPA_Z | G3D_LIMPA_EST; l.cor = 0xFF000000u; l.z = 0xFFFFFF; l.estencil = 0;
    l.mascara_cor = 15; l.mascara_est = 0xFF;
    int r = c->o->limpar2(&l);
    if (r != G3D_OK) { if (r == G3D_FALHA) c->gpu_mal = true; return false; }
    gpu3d_estado2_t e = estado_base();
    e.flags = G3D2_ESTENCIL; e.mascara_cor = 0;
    e.est_func = G3D_CMP_SEMPRE; e.est_ref = 5; e.est_zpassa = G3D_EST_SUBSTITUIR;
    if (ret(c, &e, 0, 0, 16, 8, 0xFFFF0000u, 0, 32768, 0, 0, 0, 0) != G3D_OK) return false;
    e = estado_base();
    e.flags = G3D2_ESTENCIL; e.est_func = G3D_CMP_IGUAL; e.est_ref = 5;
    if (ret(c, &e, 0, 0, 32, 8, 0xFF00FF00u, 0, 32768, 0, 0, 0, 0) != G3D_OK) return false;
    memset(&l, 0, sizeof l);
    l.mascara = G3D_LIMPA_COR; l.cor = 0xFFFFFFFFu; l.mascara_cor = G3D_MC_R; l.tijera = 1;
    l.tx = 40; l.ty = 0; l.tl = 8; l.ta = 8;
    r = c->o->limpar2(&l);
    if (r != G3D_OK) { if (r == G3D_FALHA) c->gpu_mal = true; return false; }
    if (!ler(c)) return false;
    return pixel(c, 4, "estencil", 8, 4, 0x00FF00) && pixel(c, 4, "estencil", 24, 4, 0x000000) &&
           pixel(c, 4, "limpar2 con tijera y mascara", 44, 4, 0xFF0000) && pixel(c, 4, "limpar2 con tijera", 52, 4, 0x000000);
}

/* 5: tijera y máscara de color en un dibujo. */
static bool p_tijera(ctx_t *c)
{
    if (!limpar(c, 0)) return false;
    gpu3d_estado2_t e = estado_base();
    e.flags = G3D2_TIJERA; e.tijera_x = 4; e.tijera_y = 2; e.tijera_l = 8; e.tijera_a = 4;
    if (ret(c, &e, 0, 0, 16, 8, 0xFF0000FFu, 0, 32768, 0, 0, 0, 0) != G3D_OK) return false;
    e = estado_base();
    e.mascara_cor = G3D_MC_G;
    if (ret(c, &e, 16, 0, 24, 8, 0xFFFFFFFFu, 0, 32768, 0, 0, 0, 0) != G3D_OK || !ler(c)) return false;
    return pixel(c, 5, "tijera", 6, 3, 0x0000FF) && pixel(c, 5, "tijera", 2, 3, 0x000000) && pixel(c, 5, "tijera", 6, 7, 0x000000) &&
           pixel(c, 5, "tijera", 13, 3, 0x000000) && pixel(c, 5, "mascara de color", 20, 4, 0x00FF00);
}

/* 6: funciones del Z: rojo en z = 0,5 en x < 32; encima, verde con «igual» en z = 0,5 (pasa) y en z = 0,25 (no
 * pasa), y azul con «mayor» en z = 0,25 (no pasa) y en z = 0,75 (pasa). */
static bool p_zfuncs(ctx_t *c)
{
    if (!limpar(c, 0)) return false;
    gpu3d_estado2_t e = estado_base();
    e.flags = G3D2_Z_TESTE | G3D2_Z_ESCRITA; e.func_z = G3D_CMP_SEMPRE;
    if (ret(c, &e, 0, 0, 32, 8, 0xFFFF0000u, 0, 32768, 0, 0, 0, 0) != G3D_OK) return false;
    e.func_z = G3D_CMP_IGUAL;
    if (ret(c, &e, 0, 0, 8, 8, 0xFF00FF00u, 0, 32768, 0, 0, 0, 0) != G3D_OK ||
        ret(c, &e, 8, 0, 16, 8, 0xFF00FF00u, 0, 16384, 0, 0, 0, 0) != G3D_OK) return false;
    e.func_z = G3D_CMP_MAIOR;
    if (ret(c, &e, 16, 0, 24, 8, 0xFF0000FFu, 0, 16384, 0, 0, 0, 0) != G3D_OK ||
        ret(c, &e, 24, 0, 32, 8, 0xFF0000FFu, 0, 49152, 0, 0, 0, 0) != G3D_OK || !ler(c)) return false;
    return pixel(c, 6, "Z igual", 4, 4, 0x00FF00) && pixel(c, 6, "Z igual", 12, 4, 0xFF0000) && pixel(c, 6, "Z mayor", 20, 4, 0xFF0000) &&
           pixel(c, 6, "Z mayor", 28, 4, 0x0000FF);
}

/* 7: mezcla. Fondo 0x40: 0x80 - 0x40 (resta, factores 1 y 1) = 0x40; fondo 0x40 con origen * destino = 0x20
 * (factor de origen COR_DEST, ampliada); color con 1/0 y alfa con 0/1 (separada) = 0x80; destino - origen sobre
 * el rojo (0xC0) = 0x40. */
static bool p_mezcla(ctx_t *c, uint32_t cand, uint32_t *ok)
{
    if (!limpar(c, 0x404040)) return false;
    gpu3d_estado2_t e = estado_base();
    e.flags = G3D2_MEZCLA;
    if (cand & G3D_CAP_MEZCLA_EQ) {
        e.mez_eq_rgb = e.mez_eq_alfa = G3D_EQ_SUBTRAI; e.mez_dest_rgb = e.mez_dest_alfa = G3D_F_UM;
        if (ret(c, &e, 0, 0, 8, 8, 0x80808080u, 0, 32768, 0, 0, 0, 0) != G3D_OK) return false;
        e.mez_eq_rgb = e.mez_eq_alfa = G3D_EQ_SUBTRAI_INV;
        if (ret(c, &e, 24, 0, 32, 8, 0x80808080u, 0, 32768, 0, 0, 0, 0) != G3D_OK) return false;
    }
    e = estado_base(); e.flags = G3D2_MEZCLA;
    e.mez_orig_rgb = e.mez_orig_alfa = G3D_F_COR_DEST; e.mez_dest_rgb = e.mez_dest_alfa = G3D_F_ZERO;
    if (ret(c, &e, 8, 0, 16, 8, 0x80808080u, 0, 32768, 0, 0, 0, 0) != G3D_OK) return false;
    e = estado_base(); e.flags = G3D2_MEZCLA;
    e.mez_orig_rgb = G3D_F_UM; e.mez_dest_rgb = G3D_F_ZERO; e.mez_orig_alfa = G3D_F_ZERO; e.mez_dest_alfa = G3D_F_UM;
    if (ret(c, &e, 16, 0, 24, 8, 0x80808080u, 0, 32768, 0, 0, 0, 0) != G3D_OK || !ler(c)) return false;
    *ok = 0;
    if (pixel(c, 7, "mezcla ampliada", 12, 4, 0x202020)) *ok |= G3D_CAP_MEZCLA_EXT;
    if (pixel(c, 7, "mezcla separada", 20, 4, 0x808080)) *ok |= G3D_CAP_MEZCLA_SEP;
    if ((cand & G3D_CAP_MEZCLA_EQ) && pixel(c, 7, "resta", 4, 4, 0x404040) && pixel(c, 7, "resta invertida", 28, 4, 0x000000))
        *ok |= G3D_CAP_MEZCLA_EQ;
    return true;
}

/* 8: operación lógica XOR: 0x0F0F0F ^ 0xFF00FF = 0xF00FF0. */
static bool p_logica(ctx_t *c)
{
    if (!limpar(c, 0x0F0F0F)) return false;
    gpu3d_estado2_t e = estado_base();
    e.flags = G3D2_LOGICA; e.op_logica = 6;   /* GL_XOR - GL_CLEAR */
    if (ret(c, &e, 0, 0, 8, 8, 0xFFFF00FFu, 0, 32768, 0, 0, 0, 0) != G3D_OK || !ler(c)) return false;
    return pixel(c, 8, "operacion logica", 4, 4, 0xF00FF0);
}

/* 9: niebla: rojo con f = 0x40/255 y niebla azul: (64, 0, 191). */
static bool p_neblina(ctx_t *c)
{
    if (!limpar(c, 0)) return false;
    gpu3d_estado2_t e = estado_base();
    e.flags = G3D2_NEBLINA; e.cor_neblina = 0x0000FF;
    if (ret(c, &e, 0, 0, 8, 8, 0xFFFF0000u, 0x40000000u, 32768, 0, 0, 0, 0) != G3D_OK || !ler(c)) return false;
    return pixel(c, 9, "niebla", 4, 4, 0x4000BF);
}

/* 10: espejo: textura de 2x1 (rojo, verde) de s = 0 a 2 en 16 píxeles. En x = 10 (s = 1,31 -> 0,69) verde y
 * en x = 13 (s = 1,69 -> 0,31) rojo; repitiendo sería al revés. */
static bool p_espelho(ctx_t *c)
{
    static const uint32_t t[2] = { 0xFFFF0000u, 0xFF00FF00u };
    if (c->o->textura(V_A, 2, 1, t) != G3D_OK || !limpar(c, 0)) return false;
    gpu3d_estado2_t e = estado_base();
    e.unidades = 1;
    unidade(&e.u[0], V_A, G3D_CB_SUBSTITUIR, G3D_FT_TEXTURA, G3D_FT_TEXTURA);
    e.u[0].env_s = G3D_ENV_ESPELHO;
    if (ret(c, &e, 0, 0, 16, 4, 0xFFFFFFFFu, 0, 32768, 0, 131072, 0, 65536) != G3D_OK || !ler(c)) return false;
    return pixel(c, 10, "espejo", 2, 2, 0xFF0000) && pixel(c, 10, "espejo", 10, 2, 0x00FF00) && pixel(c, 10, "espejo", 13, 2, 0xFF0000);
}

/* 11: un bloque S3TC de 4x4: color0 rojo (0xF800), color1 azul (0x001F); la fila 0 con el índice 0 y las demás
 * con el 1. En un cuadrado de 8x8 (vecino más próximo): arriba rojo, abajo azul. */
static bool p_dxt(ctx_t *c, uint32_t fmt)
{
    static uint8_t b[16];   /* estático: «dados» es una dirección de 32 bits (la prueba del PC, sin PIE, también) */
    int k = 0;
    if (fmt == G3D_FMT_DXT3) for (int i = 0; i < 8; i++) b[k++] = 0xFF;                   /* alfa explícito: todo 15 */
    if (fmt == G3D_FMT_DXT5) { b[k++] = 0xFF; b[k++] = 0x00; for (int i = 0; i < 6; i++) b[k++] = 0; }   /* índice 0 = alfa0 */
    b[k++] = 0x00; b[k++] = 0xF8; b[k++] = 0x1F; b[k++] = 0x00;
    b[k++] = 0x00; b[k++] = 0x55; b[k++] = 0x55; b[k++] = 0x55;
    gpu3d_textura2_t t;
    memset(&t, 0, sizeof t);
    t.vaga = V_B; t.formato = fmt; t.tipo = G3D_TEX_2D; t.niveis = 1; t.larg = 4; t.alt = 4;
    t.dados = (uint32_t)(uintptr_t)b; t.bytes = (uint32_t)k;
    int r = c->o->textura2(&t);
    if (r != G3D_OK) { if (r == G3D_FALHA) c->gpu_mal = true; return false; }
    if (!limpar(c, 0)) return false;
    gpu3d_estado2_t e = estado_base();
    e.unidades = 1;
    unidade(&e.u[0], V_B, G3D_CB_SUBSTITUIR, G3D_FT_TEXTURA, G3D_FT_TEXTURA);
    if (ret(c, &e, 0, 0, 8, 8, 0xFFFFFFFFu, 0, 32768, 0, 65536, 0, 65536) != G3D_OK || !ler(c)) return false;
    const char *n = fmt == G3D_FMT_DXT1 ? "DXT1" : fmt == G3D_FMT_DXT3 ? "DXT3" : "DXT5";
    return pixel(c, 11, n, 1, 0, 0xFF0000) && pixel(c, 11, n, 6, 1, 0xFF0000) && pixel(c, 11, n, 1, 7, 0x0000FF) &&
           pixel(c, 11, n, 6, 4, 0x0000FF);
}

/* 12: lado que no es potencia de 2 repitiendo: 3x1 (rojo, verde, azul) de s = 0 a 2 en 12 píxeles; x = 2
 * (s = 0,42) es el texel 1 (verde) y x = 7 (s = 1,25 -> 0,25) el texel 0 (rojo); sujetando sería el 2. */
static bool p_npot(ctx_t *c)
{
    static const uint32_t t[3] = { 0xFFFF0000u, 0xFF00FF00u, 0xFF0000FFu };
    if (c->o->textura(V_C, 3, 1, t) != G3D_OK || !limpar(c, 0)) return false;
    gpu3d_estado2_t e = estado_base();
    e.unidades = 1;
    unidade(&e.u[0], V_C, G3D_CB_SUBSTITUIR, G3D_FT_TEXTURA, G3D_FT_TEXTURA);
    e.u[0].env_s = G3D_ENV_REPETIR; e.u[0].env_t = G3D_ENV_REPETIR;
    if (ret(c, &e, 0, 0, 12, 4, 0xFFFFFFFFu, 0, 32768, 0, 131072, 0, 65536) != G3D_OK || !ler(c)) return false;
    return pixel(c, 12, "npot", 1, 2, 0xFF0000) && pixel(c, 12, "npot", 2, 2, 0x00FF00) && pixel(c, 12, "npot", 7, 2, 0xFF0000);
}

/* 13: anisótropo 4: una textura lisa da su color (que no cuelgue y que lea). */
static bool p_aniso(ctx_t *c)
{
    if (tex_solida(c, V_A, 0xFF2080C0u) || !limpar(c, 0)) return false;
    gpu3d_estado2_t e = estado_base();
    e.unidades = 1;
    unidade(&e.u[0], V_A, G3D_CB_SUBSTITUIR, G3D_FT_TEXTURA, G3D_FT_TEXTURA);
    e.u[0].aniso = 4; e.u[0].filtro = G3D_FIL_LINEAR_MAG | G3D_FIL_LINEAR_MIN;
    if (ret(c, &e, 0, 0, 16, 8, 0xFFFFFFFFu, 0, 32768, 0, 65536 * 4, 0, 65536) != G3D_OK || !ler(c)) return false;
    return pixel(c, 13, "anisotropo", 8, 4, 0x2080C0);
}

/* 14: dibujar en una textura de 16x16 (borrarla a verde, poner un cuadro rojo) y muestrearla en el principal. */
static bool p_destino(ctx_t *c)
{
    static uint32_t t[16 * 16];
    for (int i = 0; i < 256; i++) t[i] = 0xFF000000u;
    if (c->o->textura(V_D, 16, 16, t) != G3D_OK) return false;
    int r = c->o->destino(V_D, 16, 0);
    if (r != G3D_OK) { if (r == G3D_FALHA) c->gpu_mal = true; return false; }
    bool ok = limpar(c, 0x00FF00);
    gpu3d_estado2_t e = estado_base();
    if (ok) ok = ret(c, &e, 8, 8, 16, 16, 0xFFFF0000u, 0, 32768, 0, 0, 0, 0) == G3D_OK;
    r = c->o->destino(-1, 24, 8);
    if (r == G3D_FALHA) c->gpu_mal = true;
    if (!ok || r != G3D_OK || !limpar(c, 0)) return false;
    e.unidades = 1;
    unidade(&e.u[0], V_D, G3D_CB_SUBSTITUIR, G3D_FT_TEXTURA, G3D_FT_TEXTURA);
    if (ret(c, &e, 0, 0, 16, 16, 0xFFFFFFFFu, 0, 32768, 0, 65536, 0, 65536) != G3D_OK || !ler(c)) return false;
    return pixel(c, 14, "destino textura", 3, 3, 0x00FF00) && pixel(c, 14, "destino textura", 12, 12, 0xFF0000);
}

/* 15: mipmaps: nivel 0 de 4x4 rojo, nivel 1 de 2x2 verde y nivel 2 de 1x1 azul (vecino entre niveles). Un cuadrado
 * de 8x8 con la textura entera amplía (rojo); uno de 2x2 la reduce a la mitad (nivel 1, verde); uno de 1x1, al
 * nivel 2 (azul). */
static bool p_mip(ctx_t *c)
{
    static uint32_t n0[16], n1[4], n2[1];
    for (int i = 0; i < 16; i++) n0[i] = 0xFFFF0000u;
    for (int i = 0; i < 4; i++) n1[i] = 0xFF00FF00u;
    n2[0] = 0xFF0000FFu;
    static const uint32_t *nv[3] = { n0, n1, n2 };
    for (uint32_t k = 0; k < 3; k++) {
        gpu3d_textura2_t t;
        memset(&t, 0, sizeof t);
        t.vaga = V_C; t.formato = G3D_FMT_ARGB8888; t.tipo = G3D_TEX_2D; t.nivel = k; t.niveis = 3;
        t.larg = 4u >> k; t.alt = 4u >> k; t.dados = (uint32_t)(uintptr_t)nv[k]; t.bytes = t.larg * t.alt * 4;
        int r = c->o->textura2(&t);
        if (r != G3D_OK) { if (r == G3D_FALHA) c->gpu_mal = true; return false; }
    }
    if (!limpar(c, 0)) return false;
    gpu3d_estado2_t e = estado_base();
    e.unidades = 1;
    unidade(&e.u[0], V_C, G3D_CB_SUBSTITUIR, G3D_FT_TEXTURA, G3D_FT_TEXTURA);
    e.u[0].filtro = G3D_FIL_MIP_VECINO;
    if (ret(c, &e, 0, 0, 8, 8, 0xFFFFFFFFu, 0, 32768, 0, 65536, 0, 65536) != G3D_OK ||
        ret(c, &e, 10, 0, 12, 2, 0xFFFFFFFFu, 0, 32768, 0, 65536, 0, 65536) != G3D_OK ||
        ret(c, &e, 14, 0, 15, 1, 0xFFFFFFFFu, 0, 32768, 0, 65536, 0, 65536) != G3D_OK || !ler(c)) return false;
    bool ok = pixel(c, 15, "mipmap 0", 4, 4, 0xFF0000) && pixel(c, 15, "mipmap 1", 10, 0, 0x00FF00) &&
              pixel(c, 15, "mipmap 1", 11, 1, 0x00FF00) && pixel(c, 15, "mipmap 2", 14, 0, 0x0000FF);
    if (!ok)   /* para el diagnóstico: las dos primeras filas (el nivel 0 en 0-7, el 1 en 10-11 y el 2 en 14) */
        for (int y = 0; y < 2; y++)
            klog("[GPU3D] %s v2: prueba 15, fila %d: %06x %06x %06x %06x | %06x %06x | %06x\n", c->nome, y, c->px[y * PL] & 0xFFFFFFu,
                 c->px[y * PL + 1] & 0xFFFFFFu, c->px[y * PL + 4] & 0xFFFFFFu, c->px[y * PL + 7] & 0xFFFFFFu, c->px[y * PL + 10] & 0xFFFFFFu,
                 c->px[y * PL + 11] & 0xFFFFFFu, c->px[y * PL + 14] & 0xFFFFFFu);
    return ok;
}

uint32_t g3d_v2_provar(const g3d_v2_ops_t *o, const char *nome, uint32_t cand, uint32_t *unidades, uint32_t *formatos)
{
    ctx_t c = { o, nome, NULL, false };
    uint32_t ok = 0, fmt_ok = 1u << G3D_FMT_ARGB8888;
    if (!o->alvo || !o->limpar || !o->textura || !o->desenhar2 || !o->ler) return 0;
    c.px = (uint32_t *)kmalloc(PL * PA * 4);
    if (!c.px) return 0;
    if (o->alvo(PL, PA, 24) != G3D_OK) { kfree(c.px); return 0; }
    if (!p_base(&c)) {
        klog("[GPU3D] %s v2: el dibujo v2 mas sencillo no sale: sin v2 (todo lo nuevo por software)\n", nome);
        *unidades = 1; *formatos = fmt_ok;
        goto fim;
    }
    ok = G3D_CAP_V2;
    uint32_t un = 1;
    if ((cand & G3D_CAP_COMBINAR) && *unidades >= 4 && p_combinar(&c)) { ok |= G3D_CAP_COMBINAR; un = 4; }
    if ((cand & G3D_CAP_DOT3) && (ok & G3D_CAP_COMBINAR) && p_dot3(&c)) ok |= G3D_CAP_DOT3;
    if ((cand & G3D_CAP_ALFA_TESTE) && p_alfa(&c)) ok |= G3D_CAP_ALFA_TESTE;
    /* LIMPAR2 se prueba con el esténcil (prueba 4): sin esténcil que funcione, tampoco se anuncia. */
    if ((cand & G3D_CAP_ESTENCIL) && o->limpar2 && p_estencil(&c)) ok |= (G3D_CAP_ESTENCIL | G3D_CAP_LIMPAR2) & cand;
    if ((cand & (G3D_CAP_TIJERA | G3D_CAP_MASCARAS)) && p_tijera(&c)) ok |= cand & (G3D_CAP_TIJERA | G3D_CAP_MASCARAS);
    if ((cand & G3D_CAP_Z_FUNCS) && p_zfuncs(&c)) ok |= G3D_CAP_Z_FUNCS;
    uint32_t mz = 0;
    if ((cand & (G3D_CAP_MEZCLA_EXT | G3D_CAP_MEZCLA_SEP | G3D_CAP_MEZCLA_EQ)) && p_mezcla(&c, cand, &mz))
        ok |= mz & cand;
    if ((cand & G3D_CAP_LOGICA) && p_logica(&c)) ok |= G3D_CAP_LOGICA;
    if ((cand & G3D_CAP_NEBLINA) && p_neblina(&c)) ok |= G3D_CAP_NEBLINA;
    if ((cand & G3D_CAP_TEX_ESPELHO) && p_espelho(&c)) ok |= G3D_CAP_TEX_ESPELHO;
    if (o->textura2)
        for (uint32_t f = G3D_FMT_DXT1; f <= G3D_FMT_DXT5; f++)
            if ((*formatos & (1u << f)) && p_dxt(&c, f)) fmt_ok |= 1u << f;
    if ((cand & G3D_CAP_TEX_NPOT) && p_npot(&c)) ok |= G3D_CAP_TEX_NPOT;
    if ((cand & G3D_CAP_TEX_MIPMAP) && o->textura2 && p_mip(&c)) ok |= G3D_CAP_TEX_MIPMAP;
    if ((cand & G3D_CAP_TEX_ANISO) && p_aniso(&c)) ok |= G3D_CAP_TEX_ANISO;
    if ((cand & G3D_CAP_DESTINO_TEX) && o->destino && p_destino(&c)) ok |= G3D_CAP_DESTINO_TEX;
    *unidades = un;
    *formatos = fmt_ok;
fim:
    for (int v = V_A; v <= V_D; v++) if (!c.gpu_mal) o->textura(v, 0, 0, NULL);
    kfree(c.px);
    if (c.gpu_mal) {
        klog("[GPU3D] %s v2: la GPU fallo durante las pruebas: sin v2\n", nome);
        *unidades = 1; *formatos = 1u << G3D_FMT_ARGB8888;
        return 0;
    }
    klog("[GPU3D] %s v2: pruebas: pedidas 0x%x, superadas 0x%x (faltan 0x%x); %u unidades; formatos 0x%x\n", nome, cand, ok,
         cand & ~ok, *unidades, *formatos);
    return ok;
}
