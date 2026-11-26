// SPDX-License-Identifier: GPL-3.0-only
/* sgl2.c - SaguiGL 2.0 (D475): la API de OpenGL ES 2.0 (estado, objetos, sombreadores y programas).
 * La tubería (dibujar, limpiar, muestrear) está en sgl2_desenho.c; el compilador en sgl_glsl.c. */
#include "sgl2.h"
#include "sgl2_comuns.h"

sgl2_ctx_t *sgl2_C;
void *sgl2_atual;
#define C sgl2_C

void sgl2_erro(GLenum e) { if (C && C->erro == GL_NO_ERROR) C->erro = e; }
#define ERRO(e) sgl2_erro(e)

/* ------------------------------------------------------------ tablas de objetos */
static void **tabela(void ***tab, int *n, GLuint nome, int criar)
{
    if (!nome) return NULL;
    if ((int)nome >= *n) {
        if (!criar) return NULL;
        int nn = *n ? *n : 16;
        while (nn <= (int)nome) nn *= 2;
        if (nn > 1 << 20) return NULL;
        void **t = aloca_zerado((size_t)nn * sizeof(void *));
        if (!t) return NULL;
        if (*n) { memcpy(t, *tab, (size_t)*n * sizeof(void *)); libera(*tab); }
        *tab = t; *n = nn;
    }
    return &(*tab)[nome];
}
static GLuint nome_livre(void **tab, int n)
{
    for (int i = 1; i < n; i++) if (!tab[i]) return (GLuint)i;
    return (GLuint)(n ? n : 1);
}
s2_tex_t *sgl2_tex(GLuint nome) { if (!C) return NULL; void **p = tabela((void ***)&C->tex, &C->ntex, nome, 0); return p ? *p : NULL; }
s2_buf_t *sgl2_buf(GLuint nome) { if (!C) return NULL; void **p = tabela((void ***)&C->buf, &C->nbuf, nome, 0); return p ? *p : NULL; }
s2_rb_t  *sgl2_rb(GLuint nome)  { if (!C) return NULL; void **p = tabela((void ***)&C->rb, &C->nrb, nome, 0); return p ? *p : NULL; }
s2_fbo_t *sgl2_fbo(GLuint nome) { if (!C) return NULL; void **p = tabela((void ***)&C->fbo, &C->nfbo, nome, 0); return p ? *p : NULL; }
static s2_tex_t *tex_nova(GLuint nome)
{
    void **p = tabela((void ***)&C->tex, &C->ntex, nome, 1);
    if (!p) return NULL;
    if (!*p) {
        s2_tex_t *t = aloca_zerado(sizeof *t);
        if (!t) return NULL;
        t->nome = nome; t->min_f = GL_NEAREST_MIPMAP_LINEAR; t->mag_f = GL_LINEAR; t->wrap_s = t->wrap_t = GL_REPEAT; t->completa = -1;
        *p = t;
    }
    return *p;
}
static void *obj(GLuint nome, int tipo)
{
    if (!C || !nome || (int)nome >= C->nobj || !C->obj[nome]) return NULL;
    return C->obj_tipo[nome] == tipo ? C->obj[nome] : NULL;
}
static GLuint obj_novo(void *o, int tipo)
{
    GLuint n = nome_livre(C->obj, C->nobj);
    int antes = C->nobj;
    void **p = tabela(&C->obj, &C->nobj, n, 1);
    if (!p) return 0;
    if (C->nobj != antes) {
        uint8_t *t = aloca_zerado((size_t)C->nobj);
        if (!t) return 0;
        if (C->obj_tipo) { memcpy(t, C->obj_tipo, (size_t)antes); libera(C->obj_tipo); }
        C->obj_tipo = t;
    }
    *p = o; C->obj_tipo[n] = (uint8_t)tipo;
    return n;
}
#define SHADER(n) ((s2_shader_t *)obj((n), 1))
#define PROG(n)   ((s2_prog_t *)obj((n), 2))

/* ------------------------------------------------------------ contexto */
static void estado_inicial(sgl2_ctx_t *c)
{
    c->z_perto = 0; c->z_longe = 1;
    c->masc_cor[0] = c->masc_cor[1] = c->masc_cor[2] = c->masc_cor[3] = 1; c->masc_z = 1;
    c->cull_face = GL_BACK; c->frente = GL_CCW;
    c->z_func = GL_LESS; c->z_limpo = 1;
    c->b_src[0] = c->b_src[1] = GL_ONE; c->b_dst[0] = c->b_dst[1] = GL_ZERO; c->b_eq[0] = c->b_eq[1] = GL_FUNC_ADD;
    for (int f = 0; f < 2; f++) {
        c->est[f].func = GL_ALWAYS; c->est[f].ref = 0; c->est[f].masc_v = 0xFFFFFFFFu; c->est[f].masc_e = 0xFFFFFFFFu;
        c->est[f].falha = c->est[f].zfalha = c->est[f].zpassa = GL_KEEP;
    }
    c->dither = 1; c->cob_valor = 1; c->linha_larg = 1;
    c->pack_alin = c->unpack_alin = 4;
    c->dica_mip = c->dica_deriv = GL_DONT_CARE;
    for (int i = 0; i < S2_MAX_ATRIB; i++) { c->atual[i][0] = c->atual[i][1] = c->atual[i][2] = 0; c->atual[i][3] = 1; c->at[i].tam = 4; c->at[i].tipo = GL_FLOAT; }
    for (int k = 0; k < 2; k++) {
        s2_tex_t *t = &c->tex0[k];
        t->alvo = k ? GL_TEXTURE_CUBE_MAP : GL_TEXTURE_2D;
        t->min_f = GL_NEAREST_MIPMAP_LINEAR; t->mag_f = GL_LINEAR; t->wrap_s = t->wrap_t = GL_REPEAT; t->completa = -1;
    }
}

sgl2_ctx_t *sgl2_criar(struct sgl_ctx *base, int zbits, int estbits)
{
    sgl2_ctx_t *c = aloca_zerado(sizeof *c);
    if (!c) return NULL;
    c->base = base; c->zbits_cfg = zbits; c->est_cfg = estbits;
    estado_inicial(c);
    sgl2_gpu_ctx_novo(c);
    uint32_t *px; int l, a;
    if (base && sgl_destino(base, &px, &l, &a)) { c->vp[2] = c->tij[2] = l; c->vp[3] = c->tij[3] = a; }
    return c;
}
static void nivel_soltar(s2_nivel_t *n)
{
    if (n->px) libera(n->px);
    if (n->z) libera(n->z);
    if (n->est) libera(n->est);
    n->px = NULL; n->z = NULL; n->est = NULL;
}
/* D486: ¿tiene datos el nivel? (colores, o profundidad de una textura de profundidad) */
static int nivel_con_datos(const s2_nivel_t *n) { return n->px || n->z; }
static int fmt_profundidad(GLenum f) { return f == GL_DEPTH_COMPONENT || f == GL_DEPTH_STENCIL_OES; }
static void tex_soltar_niveis(s2_tex_t *t)
{
    for (int f = 0; f < 6; f++) for (int k = 0; k < S2_NIVEIS; k++) nivel_soltar(&t->niv[f][k]);
}
static void prog_soltar_exec(s2_prog_t *p)
{
    sgl2_gpu_prog_solto(p);
    if (p->xv) ri_exec_liberar(p->xv);
    if (p->xf) ri_exec_liberar(p->xf);
    if (p->pv) ri_liberar(p->pv);
    if (p->pf) ri_liberar(p->pf);
    for (int k = 0; k < 2; k++) if (p->uni[k]) libera(p->uni[k]);
    if (p->unif) libera(p->unif);
    p->xv = p->xf = NULL; p->pv = p->pf = NULL; p->uni[0] = p->uni[1] = NULL; p->unif = NULL; p->nunif = p->nativos = 0;
}
void sgl2_destruir(sgl2_ctx_t *c)
{
    if (!c) return;
    sgl2_gpu_sincronizar(c);
    for (int i = 0; i < c->ntex; i++) if (c->tex[i]) sgl2_gpu_soltar(&c->tex[i]->g);
    for (int i = 0; i < c->nrb; i++) if (c->rb[i]) sgl2_gpu_soltar(&c->rb[i]->g);
    for (int i = 0; i < c->nfbo; i++) if (c->fbo[i]) sgl2_gpu_soltar(&c->fbo[i]->gz);
    sgl2_gpu_soltar(&c->gdef); sgl2_gpu_soltar(&c->gdef_z);
    for (int i = 0; i < c->ntex; i++) if (c->tex[i]) { tex_soltar_niveis(c->tex[i]); libera(c->tex[i]); }
    for (int i = 0; i < c->nbuf; i++) if (c->buf[i]) { if (c->buf[i]->dados) libera(c->buf[i]->dados); libera(c->buf[i]); }
    for (int i = 0; i < c->nrb; i++) if (c->rb[i]) { s2_rb_t *r = c->rb[i]; if (r->cor) libera(r->cor); if (r->z) libera(r->z); if (r->est) libera(r->est); libera(r); }
    for (int i = 0; i < c->nfbo; i++) if (c->fbo[i]) libera(c->fbo[i]);
    for (int i = 0; i < c->nobj; i++) if (c->obj[i]) {
        if (c->obj_tipo[i] == 1) { s2_shader_t *s = c->obj[i]; if (s->fonte) libera(s->fonte); }
        else prog_soltar_exec(c->obj[i]);
        libera(c->obj[i]);
    }
    tex_soltar_niveis(&c->tex0[0]); tex_soltar_niveis(&c->tex0[1]);
    if (c->tex) libera(c->tex);
    if (c->buf) libera(c->buf);
    if (c->rb) libera(c->rb);
    if (c->fbo) libera(c->fbo);
    if (c->obj) libera(c->obj);
    if (c->obj_tipo) libera(c->obj_tipo);
    if (c->z_def) libera(c->z_def);
    if (c->est_def) libera(c->est_def);
    if (c->vsai) libera(c->vsai);
    if (C == c) sgl2_usar(NULL);
    sgl2_gpu_ctx_livre(c);
    libera(c);
}
void sgl2_usar(sgl2_ctx_t *c) { C = c; sgl2_atual = c; }

/* ------------------------------------------------------------ estado */
GLenum sgl2_glGetError(void) { if (!C) return GL_NO_ERROR; GLenum e = C->erro; C->erro = GL_NO_ERROR; return e; }

static int *cap_ptr(GLenum cap)
{
    switch (cap) {
    case GL_CULL_FACE: return &C->cull_on;
    case GL_BLEND: return &C->blend_on;
    case GL_DITHER: return &C->dither;
    case GL_STENCIL_TEST: return &C->est_on;
    case GL_DEPTH_TEST: return &C->z_on;
    case GL_SCISSOR_TEST: return &C->tij_on;
    case GL_POLYGON_OFFSET_FILL: return &C->off_on;
    case GL_SAMPLE_ALPHA_TO_COVERAGE: return &C->alfa_cob;
    case GL_SAMPLE_COVERAGE: return &C->cob_on;
    }
    return NULL;
}
void sgl2_glEnable(GLenum cap) { if (!C) return; int *p = cap_ptr(cap); if (p) *p = 1; else ERRO(GL_INVALID_ENUM); }
void sgl2_glDisable(GLenum cap) { if (!C) return; int *p = cap_ptr(cap); if (p) *p = 0; else ERRO(GL_INVALID_ENUM); }
GLboolean sgl2_glIsEnabled(GLenum cap) { if (!C) return GL_FALSE; int *p = cap_ptr(cap); if (!p) { ERRO(GL_INVALID_ENUM); return GL_FALSE; } return *p ? GL_TRUE : GL_FALSE; }

static int eh_fator(GLenum f, int dst)
{
    switch (f) {
    case GL_ZERO: case GL_ONE: case GL_SRC_COLOR: case GL_ONE_MINUS_SRC_COLOR: case GL_DST_COLOR: case GL_ONE_MINUS_DST_COLOR:
    case GL_SRC_ALPHA: case GL_ONE_MINUS_SRC_ALPHA: case GL_DST_ALPHA: case GL_ONE_MINUS_DST_ALPHA: case GL_CONSTANT_COLOR:
    case GL_ONE_MINUS_CONSTANT_COLOR: case GL_CONSTANT_ALPHA: case GL_ONE_MINUS_CONSTANT_ALPHA: return 1;
    case GL_SRC_ALPHA_SATURATE: return !dst;
    }
    return 0;
}
static int eh_eq(GLenum e) { return e == GL_FUNC_ADD || e == GL_FUNC_SUBTRACT || e == GL_FUNC_REVERSE_SUBTRACT; }
void glBlendFuncSeparate(GLenum sr, GLenum dr, GLenum sa, GLenum da)
{
    if (!C) return;
    if (!eh_fator(sr, 0) || !eh_fator(dr, 1) || !eh_fator(sa, 0) || !eh_fator(da, 1)) { ERRO(GL_INVALID_ENUM); return; }
    C->b_src[0] = sr; C->b_dst[0] = dr; C->b_src[1] = sa; C->b_dst[1] = da;
}
void sgl2_glBlendFunc(GLenum s, GLenum d) { glBlendFuncSeparate(s, d, s, d); }
void glBlendEquationSeparate(GLenum r, GLenum a) { if (!C) return; if (!eh_eq(r) || !eh_eq(a)) { ERRO(GL_INVALID_ENUM); return; } C->b_eq[0] = r; C->b_eq[1] = a; }
void glBlendEquation(GLenum m) { glBlendEquationSeparate(m, m); }
static float sat(float v) { return v < 0 ? 0 : v > 1 ? 1 : v; }
void glBlendColor(GLfloat r, GLfloat g, GLfloat b, GLfloat a) { if (!C) return; C->b_cor[0] = sat(r); C->b_cor[1] = sat(g); C->b_cor[2] = sat(b); C->b_cor[3] = sat(a); }
void sgl2_glClearColor(GLfloat r, GLfloat g, GLfloat b, GLfloat a) { if (!C) return; C->cor_limpa[0] = sat(r); C->cor_limpa[1] = sat(g); C->cor_limpa[2] = sat(b); C->cor_limpa[3] = sat(a); }
void sgl2_glClearDepthf(GLfloat d) { if (C) C->z_limpo = sat(d); }
void sgl2_glClearStencil(GLint s) { if (C) C->est_limpo = s; }
void sgl2_glColorMask(GLboolean r, GLboolean g, GLboolean b, GLboolean a) { if (!C) return; C->masc_cor[0] = !!r; C->masc_cor[1] = !!g; C->masc_cor[2] = !!b; C->masc_cor[3] = !!a; }
void sgl2_glDepthMask(GLboolean f) { if (C) C->masc_z = !!f; }
static int eh_func(GLenum f) { return f >= GL_NEVER && f <= GL_ALWAYS; }
void sgl2_glDepthFunc(GLenum f) { if (!C) return; if (!eh_func(f)) { ERRO(GL_INVALID_ENUM); return; } C->z_func = f; }
void sgl2_glDepthRangef(GLfloat n, GLfloat f) { if (!C) return; C->z_perto = sat(n); C->z_longe = sat(f); }
void sgl2_glCullFace(GLenum m) { if (!C) return; if (m != GL_FRONT && m != GL_BACK && m != GL_FRONT_AND_BACK) { ERRO(GL_INVALID_ENUM); return; } C->cull_face = m; }
void sgl2_glFrontFace(GLenum m) { if (!C) return; if (m != GL_CW && m != GL_CCW) { ERRO(GL_INVALID_ENUM); return; } C->frente = m; }
void sgl2_glViewport(GLint x, GLint y, GLsizei l, GLsizei a)
{
    if (!C) return;
    if (l < 0 || a < 0) { ERRO(GL_INVALID_VALUE); return; }
    if (l > S2_MAX_LADO) l = S2_MAX_LADO;
    if (a > S2_MAX_LADO) a = S2_MAX_LADO;
    C->vp[0] = x; C->vp[1] = y; C->vp[2] = l; C->vp[3] = a;
}
void sgl2_glScissor(GLint x, GLint y, GLsizei l, GLsizei a) { if (!C) return; if (l < 0 || a < 0) { ERRO(GL_INVALID_VALUE); return; } C->tij[0] = x; C->tij[1] = y; C->tij[2] = l; C->tij[3] = a; }
void sgl2_glLineWidth(GLfloat w) { if (!C) return; if (!(w > 0)) { ERRO(GL_INVALID_VALUE); return; } C->linha_larg = w; }
void sgl2_glPolygonOffset(GLfloat f, GLfloat u) { if (!C) return; C->off_fator = f; C->off_unid = u; }
void sgl2_glSampleCoverage(GLfloat v, GLboolean inv) { if (!C) return; C->cob_valor = sat(v); C->cob_inv = !!inv; }
void sgl2_glHint(GLenum alvo, GLenum modo)
{
    if (!C) return;
    if (modo != GL_DONT_CARE && modo != GL_FASTEST && modo != GL_NICEST) { ERRO(GL_INVALID_ENUM); return; }
    if (alvo == GL_GENERATE_MIPMAP_HINT) C->dica_mip = modo;
    else if (alvo == GL_FRAGMENT_SHADER_DERIVATIVE_HINT_OES) C->dica_deriv = modo;
    else ERRO(GL_INVALID_ENUM);
}
void sgl2_glPixelStorei(GLenum p, GLint v)
{
    if (!C) return;
    if (v != 1 && v != 2 && v != 4 && v != 8) { ERRO(GL_INVALID_VALUE); return; }
    if (p == GL_PACK_ALIGNMENT) C->pack_alin = v;
    else if (p == GL_UNPACK_ALIGNMENT) C->unpack_alin = v;
    else ERRO(GL_INVALID_ENUM);
}
static int face_ok(GLenum f) { return f == GL_FRONT || f == GL_BACK || f == GL_FRONT_AND_BACK; }
void glStencilFuncSeparate(GLenum face, GLenum func, GLint ref, GLuint masc)
{
    if (!C) return;
    if (!face_ok(face) || !eh_func(func)) { ERRO(GL_INVALID_ENUM); return; }
    for (int f = 0; f < 2; f++) if (face == GL_FRONT_AND_BACK || (face == GL_FRONT) == (f == 0)) { C->est[f].func = func; C->est[f].ref = ref; C->est[f].masc_v = masc; }
}
void sgl2_glStencilFunc(GLenum func, GLint ref, GLuint masc) { glStencilFuncSeparate(GL_FRONT_AND_BACK, func, ref, masc); }
void glStencilMaskSeparate(GLenum face, GLuint masc)
{
    if (!C) return;
    if (!face_ok(face)) { ERRO(GL_INVALID_ENUM); return; }
    for (int f = 0; f < 2; f++) if (face == GL_FRONT_AND_BACK || (face == GL_FRONT) == (f == 0)) C->est[f].masc_e = masc;
}
void sgl2_glStencilMask(GLuint masc) { glStencilMaskSeparate(GL_FRONT_AND_BACK, masc); }
static int eh_est_op(GLenum o) { return o == GL_KEEP || o == GL_ZERO || o == GL_REPLACE || o == GL_INCR || o == GL_DECR || o == GL_INVERT || o == GL_INCR_WRAP || o == GL_DECR_WRAP; }
void glStencilOpSeparate(GLenum face, GLenum sf, GLenum zf, GLenum zp)
{
    if (!C) return;
    if (!face_ok(face) || !eh_est_op(sf) || !eh_est_op(zf) || !eh_est_op(zp)) { ERRO(GL_INVALID_ENUM); return; }
    for (int f = 0; f < 2; f++) if (face == GL_FRONT_AND_BACK || (face == GL_FRONT) == (f == 0)) { C->est[f].falha = sf; C->est[f].zfalha = zf; C->est[f].zpassa = zp; }
}
void sgl2_glStencilOp(GLenum sf, GLenum zf, GLenum zp) { glStencilOpSeparate(GL_FRONT_AND_BACK, sf, zf, zp); }

/* ------------------------------------------------------------ consultas */
static const char *const EXTS = "GL_OES_standard_derivatives GL_OES_rgb8_rgba8 GL_OES_element_index_uint GL_OES_texture_npot "
    "GL_OES_depth_texture GL_OES_mapbuffer GL_OES_depth24 GL_OES_packed_depth_stencil";   /* D486 */
const GLubyte *sgl2_glGetString(GLenum n)
{
    switch (n) {
    case GL_VENDOR: return (const GLubyte *)"SaguiOS";
    case GL_RENDERER: return (const GLubyte *)"SaguiGL 2.0 (CPU)";
    case GL_VERSION: return (const GLubyte *)"OpenGL ES 2.0 SaguiGL 2.0";
    case GL_SHADING_LANGUAGE_VERSION: return (const GLubyte *)"OpenGL ES GLSL ES 1.00 SaguiGL 2.0";
    case GL_EXTENSIONS: return (const GLubyte *)EXTS;
    }
    ERRO(GL_INVALID_ENUM);
    return NULL;
}

/* el valor de una consulta: n valores; *cor = 1 si son colores/profundidades (normalizados al pasar a entero) */
static int consultar(GLenum p, double *v, int *cor)
{
    *cor = 0;
    s2_alvo_t a; memset(&a, 0, sizeof a);
    switch (p) {
    case GL_ACTIVE_TEXTURE: v[0] = GL_TEXTURE0 + C->unid; return 1;
    case GL_ALIASED_LINE_WIDTH_RANGE: v[0] = 1; v[1] = 16; return 2;
    case GL_ALIASED_POINT_SIZE_RANGE: v[0] = 1; v[1] = 64; return 2;
    case GL_ARRAY_BUFFER_BINDING: v[0] = C->buf_array; return 1;
    case GL_ELEMENT_ARRAY_BUFFER_BINDING: v[0] = C->buf_elem; return 1;
    case GL_BLEND: case GL_CULL_FACE: case GL_DEPTH_TEST: case GL_DITHER: case GL_POLYGON_OFFSET_FILL: case GL_SAMPLE_ALPHA_TO_COVERAGE:
    case GL_SAMPLE_COVERAGE: case GL_SCISSOR_TEST: case GL_STENCIL_TEST: v[0] = *cap_ptr(p); return 1;
    case GL_BLEND_COLOR: for (int i = 0; i < 4; i++) v[i] = C->b_cor[i]; *cor = 1; return 4;
    case GL_BLEND_SRC_RGB: v[0] = C->b_src[0]; return 1;
    case GL_BLEND_SRC_ALPHA: v[0] = C->b_src[1]; return 1;
    case GL_BLEND_DST_RGB: v[0] = C->b_dst[0]; return 1;
    case GL_BLEND_DST_ALPHA: v[0] = C->b_dst[1]; return 1;
    case GL_BLEND_EQUATION_RGB: v[0] = C->b_eq[0]; return 1;
    case GL_BLEND_EQUATION_ALPHA: v[0] = C->b_eq[1]; return 1;
    case GL_COLOR_CLEAR_VALUE: for (int i = 0; i < 4; i++) v[i] = C->cor_limpa[i]; *cor = 1; return 4;
    case GL_COLOR_WRITEMASK: for (int i = 0; i < 4; i++) v[i] = C->masc_cor[i]; return 4;
    case GL_COMPRESSED_TEXTURE_FORMATS: return 0;
    case GL_NUM_COMPRESSED_TEXTURE_FORMATS: case GL_NUM_SHADER_BINARY_FORMATS: v[0] = 0; return 1;
    case GL_SHADER_BINARY_FORMATS: return 0;
    case GL_SHADER_COMPILER: v[0] = 1; return 1;
    case GL_CULL_FACE_MODE: v[0] = C->cull_face; return 1;
    case GL_CURRENT_PROGRAM: v[0] = C->prog_uso; return 1;
    case GL_DEPTH_CLEAR_VALUE: v[0] = C->z_limpo; *cor = 1; return 1;
    case GL_DEPTH_FUNC: v[0] = C->z_func; return 1;
    case GL_DEPTH_RANGE: v[0] = C->z_perto; v[1] = C->z_longe; *cor = 1; return 2;
    case GL_DEPTH_WRITEMASK: v[0] = C->masc_z; return 1;
    case GL_FRAMEBUFFER_BINDING: v[0] = C->fbo_ligado; return 1;
    case GL_FRONT_FACE: v[0] = C->frente; return 1;
    case GL_GENERATE_MIPMAP_HINT: v[0] = C->dica_mip; return 1;
    case GL_FRAGMENT_SHADER_DERIVATIVE_HINT_OES: v[0] = C->dica_deriv; return 1;
    case GL_IMPLEMENTATION_COLOR_READ_FORMAT: v[0] = GL_RGBA; return 1;
    case GL_IMPLEMENTATION_COLOR_READ_TYPE: v[0] = GL_UNSIGNED_BYTE; return 1;
    case GL_LINE_WIDTH: v[0] = C->linha_larg; return 1;
    case GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS: v[0] = S2_MAX_UNID; return 1;
    case GL_MAX_CUBE_MAP_TEXTURE_SIZE: case GL_MAX_TEXTURE_SIZE: case GL_MAX_RENDERBUFFER_SIZE: v[0] = S2_MAX_LADO; return 1;
    case GL_MAX_FRAGMENT_UNIFORM_VECTORS: case GL_MAX_VERTEX_UNIFORM_VECTORS: v[0] = S2_MAX_UNIF_VEC; return 1;
    case GL_MAX_TEXTURE_IMAGE_UNITS: case GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS: v[0] = 8; return 1;
    case GL_MAX_VARYING_VECTORS: v[0] = S2_MAX_VARY; return 1;
    case GL_MAX_VERTEX_ATTRIBS: v[0] = S2_MAX_ATRIB; return 1;
    case GL_MAX_VIEWPORT_DIMS: v[0] = v[1] = S2_MAX_LADO; return 2;
    case GL_PACK_ALIGNMENT: v[0] = C->pack_alin; return 1;
    case GL_UNPACK_ALIGNMENT: v[0] = C->unpack_alin; return 1;
    case GL_POLYGON_OFFSET_FACTOR: v[0] = C->off_fator; return 1;
    case GL_POLYGON_OFFSET_UNITS: v[0] = C->off_unid; return 1;
    case GL_RENDERBUFFER_BINDING: v[0] = C->rb_ligado; return 1;
    case GL_SAMPLE_BUFFERS: case GL_SAMPLES: v[0] = 0; return 1;
    case GL_SAMPLE_COVERAGE_INVERT: v[0] = C->cob_inv; return 1;
    case GL_SAMPLE_COVERAGE_VALUE: v[0] = C->cob_valor; return 1;
    case GL_SCISSOR_BOX: for (int i = 0; i < 4; i++) v[i] = C->tij[i]; return 4;
    case GL_VIEWPORT: for (int i = 0; i < 4; i++) v[i] = C->vp[i]; return 4;
    case GL_STENCIL_FUNC: v[0] = C->est[0].func; return 1;
    case GL_STENCIL_REF: v[0] = C->est[0].ref; return 1;
    case GL_STENCIL_VALUE_MASK: v[0] = C->est[0].masc_v; return 1;
    case GL_STENCIL_WRITEMASK: v[0] = C->est[0].masc_e; return 1;
    case GL_STENCIL_FAIL: v[0] = C->est[0].falha; return 1;
    case GL_STENCIL_PASS_DEPTH_FAIL: v[0] = C->est[0].zfalha; return 1;
    case GL_STENCIL_PASS_DEPTH_PASS: v[0] = C->est[0].zpassa; return 1;
    case GL_STENCIL_BACK_FUNC: v[0] = C->est[1].func; return 1;
    case GL_STENCIL_BACK_REF: v[0] = C->est[1].ref; return 1;
    case GL_STENCIL_BACK_VALUE_MASK: v[0] = C->est[1].masc_v; return 1;
    case GL_STENCIL_BACK_WRITEMASK: v[0] = C->est[1].masc_e; return 1;
    case GL_STENCIL_BACK_FAIL: v[0] = C->est[1].falha; return 1;
    case GL_STENCIL_BACK_PASS_DEPTH_FAIL: v[0] = C->est[1].zfalha; return 1;
    case GL_STENCIL_BACK_PASS_DEPTH_PASS: v[0] = C->est[1].zpassa; return 1;
    case GL_STENCIL_CLEAR_VALUE: v[0] = C->est_limpo; return 1;
    case GL_SUBPIXEL_BITS: v[0] = 4; return 1;
    case GL_TEXTURE_BINDING_2D: v[0] = C->un_tex[0][C->unid]; return 1;
    case GL_TEXTURE_BINDING_CUBE_MAP: v[0] = C->un_tex[1][C->unid]; return 1;
    case GL_RED_BITS: case GL_GREEN_BITS: case GL_BLUE_BITS: case GL_ALPHA_BITS: case GL_DEPTH_BITS: case GL_STENCIL_BITS:
        sgl2_alvo(C, &a);
        v[0] = p == GL_ALPHA_BITS ? (a.alfa ? 8 : 0) : p == GL_DEPTH_BITS ? (a.z ? a.zbits : 0) : p == GL_STENCIL_BITS ? (a.est ? 8 : 0) : 8;
        if (!a.cor && p != GL_DEPTH_BITS && p != GL_STENCIL_BITS) v[0] = 0;
        return 1;
    }
    return -1;
}
void sgl2_glGetIntegerv(GLenum p, GLint *r)
{
    if (!C || !r) return;
    double v[8]; int cor, n = consultar(p, v, &cor);
    if (n < 0) { ERRO(GL_INVALID_ENUM); return; }
    for (int i = 0; i < n; i++) {
        if (cor) { double x = v[i] * 2147483647.0; r[i] = (GLint)(x < 0 ? x - 0.5 : x + 0.5); }
        else { double x = v[i]; r[i] = x >= 4294967295.0 ? -1 : x >= 2147483648.0 ? (GLint)(uint32_t)x : (GLint)(x < 0 ? x - 0.5 : x + 0.5); }
    }
}
void sgl2_glGetFloatv(GLenum p, GLfloat *r)
{
    if (!C || !r) return;
    double v[8]; int cor, n = consultar(p, v, &cor);
    if (n < 0) { ERRO(GL_INVALID_ENUM); return; }
    for (int i = 0; i < n; i++) r[i] = (GLfloat)v[i];
}
void sgl2_glGetBooleanv(GLenum p, GLboolean *r)
{
    if (!C || !r) return;
    double v[8]; int cor, n = consultar(p, v, &cor);
    if (n < 0) { ERRO(GL_INVALID_ENUM); return; }
    for (int i = 0; i < n; i++) r[i] = v[i] != 0 ? GL_TRUE : GL_FALSE;
}

/* ------------------------------------------------------------ búferes */
static GLuint *buf_alvo(GLenum alvo) { return alvo == GL_ARRAY_BUFFER ? &C->buf_array : alvo == GL_ELEMENT_ARRAY_BUFFER ? &C->buf_elem : NULL; }
void sgl2_glGenBuffers(GLsizei n, GLuint *ids)
{
    if (!C) return;
    if (n < 0) { ERRO(GL_INVALID_VALUE); return; }
    for (int i = 0; i < n; i++) {
        GLuint k = nome_livre((void **)C->buf, C->nbuf);
        void **p = tabela((void ***)&C->buf, &C->nbuf, k, 1);
        s2_buf_t *b = p ? aloca_zerado(sizeof *b) : NULL;
        if (!b) { ERRO(GL_OUT_OF_MEMORY); return; }
        b->nome = k; b->uso = GL_STATIC_DRAW; *p = b; ids[i] = k;
    }
}
void sgl2_glBindBuffer(GLenum alvo, GLuint id)
{
    if (!C) return;
    GLuint *a = buf_alvo(alvo);
    if (!a) { ERRO(GL_INVALID_ENUM); return; }
    if (id && !sgl2_buf(id)) {
        void **p = tabela((void ***)&C->buf, &C->nbuf, id, 1);
        s2_buf_t *b = p ? aloca_zerado(sizeof *b) : NULL;
        if (!b) { ERRO(GL_OUT_OF_MEMORY); return; }
        b->nome = id; b->uso = GL_STATIC_DRAW; *p = b;
    }
    *a = id;
}
void sgl2_glDeleteBuffers(GLsizei n, const GLuint *ids)
{
    if (!C) return;
    if (n < 0) { ERRO(GL_INVALID_VALUE); return; }
    for (int i = 0; i < n; i++) {
        s2_buf_t *b = sgl2_buf(ids[i]);
        if (!b) continue;
        if (C->buf_array == ids[i]) C->buf_array = 0;
        if (C->buf_elem == ids[i]) C->buf_elem = 0;
        for (int k = 0; k < S2_MAX_ATRIB; k++) if (C->at[k].buf == ids[i]) C->at[k].buf = 0;
        if (b->dados) libera(b->dados);
        libera(b); C->buf[ids[i]] = NULL;
    }
}
GLboolean sgl2_glIsBuffer(GLuint id) { return C && sgl2_buf(id) ? GL_TRUE : GL_FALSE; }
void sgl2_glBufferData(GLenum alvo, GLsizeiptr tam, const void *dados, GLenum uso)
{
    if (!C) return;
    GLuint *a = buf_alvo(alvo);
    if (!a) { ERRO(GL_INVALID_ENUM); return; }
    if (uso != GL_STREAM_DRAW && uso != GL_STATIC_DRAW && uso != GL_DYNAMIC_DRAW) { ERRO(GL_INVALID_ENUM); return; }
    if (tam < 0) { ERRO(GL_INVALID_VALUE); return; }
    s2_buf_t *b = sgl2_buf(*a);
    if (!b) { ERRO(GL_INVALID_OPERATION); return; }
    uint8_t *d = tam ? aloca((size_t)tam) : NULL;
    if (tam && !d) { ERRO(GL_OUT_OF_MEMORY); return; }
    if (d) { if (dados) memcpy(d, dados, (size_t)tam); else memset(d, 0, (size_t)tam); }
    if (b->dados) libera(b->dados);
    b->dados = d; b->tam = (int)tam; b->uso = uso;
    b->mapeado = 0;      /* D486: glBufferData rehace el almacén y lo desmapea */
}
void sgl2_glBufferSubData(GLenum alvo, GLintptr desl, GLsizeiptr tam, const void *dados)
{
    if (!C) return;
    GLuint *a = buf_alvo(alvo);
    if (!a) { ERRO(GL_INVALID_ENUM); return; }
    s2_buf_t *b = sgl2_buf(*a);
    if (!b) { ERRO(GL_INVALID_OPERATION); return; }
    if (b->mapeado) { ERRO(GL_INVALID_OPERATION); return; }
    if (desl < 0 || tam < 0 || desl + tam > b->tam) { ERRO(GL_INVALID_VALUE); return; }
    if (tam && dados) memcpy(b->dados + desl, dados, (size_t)tam);
}
void sgl2_glGetBufferParameteriv(GLenum alvo, GLenum p, GLint *r)
{
    if (!C) return;
    GLuint *a = buf_alvo(alvo);
    if (!a) { ERRO(GL_INVALID_ENUM); return; }
    s2_buf_t *b = sgl2_buf(*a);
    if (!b) { ERRO(GL_INVALID_OPERATION); return; }
    if (p == GL_BUFFER_SIZE) *r = b->tam;
    else if (p == GL_BUFFER_USAGE) *r = (GLint)b->uso;
    else if (p == GL_BUFFER_ACCESS_OES) *r = GL_WRITE_ONLY_OES;       /* D486 */
    else if (p == GL_BUFFER_MAPPED_OES) *r = b->mapeado;
    else ERRO(GL_INVALID_ENUM);
}

/* D486: GL_OES_mapbuffer. El almacén ya está en la memoria del proceso: mapear es dar su puntero (la CPU lo lee en cada dibujo y la GPU
 * recibe los atributos de ahí, así que no hay nada que subir al desmapear). Solo escritura, como pide la extensión. */
void *sgl2_glMapBufferOES(GLenum alvo, GLenum acesso)
{
    if (!C) return NULL;
    GLuint *a = buf_alvo(alvo);
    if (!a) { ERRO(GL_INVALID_ENUM); return NULL; }
    if (acesso != GL_WRITE_ONLY_OES) { ERRO(GL_INVALID_ENUM); return NULL; }
    s2_buf_t *b = sgl2_buf(*a);
    if (!b) { ERRO(GL_INVALID_OPERATION); return NULL; }
    if (b->mapeado) { ERRO(GL_INVALID_OPERATION); return NULL; }
    b->mapeado = 1;
    return b->dados;
}
GLboolean sgl2_glUnmapBufferOES(GLenum alvo)
{
    if (!C) return GL_FALSE;
    GLuint *a = buf_alvo(alvo);
    if (!a) { ERRO(GL_INVALID_ENUM); return GL_FALSE; }
    s2_buf_t *b = sgl2_buf(*a);
    if (!b) { ERRO(GL_INVALID_OPERATION); return GL_FALSE; }
    if (!b->mapeado) { ERRO(GL_INVALID_OPERATION); return GL_FALSE; }
    b->mapeado = 0;
    return GL_TRUE;
}
void sgl2_glGetBufferPointervOES(GLenum alvo, GLenum nome, GLvoid **v)
{
    if (!C) return;
    GLuint *a = buf_alvo(alvo);
    if (!a || nome != GL_BUFFER_MAP_POINTER_OES) { ERRO(GL_INVALID_ENUM); return; }
    s2_buf_t *b = sgl2_buf(*a);
    if (!b) { ERRO(GL_INVALID_OPERATION); return; }
    *v = b->mapeado ? b->dados : NULL;
}

/* ------------------------------------------------------------ texturas */
static int alvo_cara(GLenum alvo, int *face, GLenum *tipo)
{
    if (alvo == GL_TEXTURE_2D) { *face = 0; *tipo = GL_TEXTURE_2D; return 1; }
    if (alvo >= GL_TEXTURE_CUBE_MAP_POSITIVE_X && alvo <= GL_TEXTURE_CUBE_MAP_NEGATIVE_Z) { *face = (int)(alvo - GL_TEXTURE_CUBE_MAP_POSITIVE_X); *tipo = GL_TEXTURE_CUBE_MAP; return 1; }
    return 0;
}
static s2_tex_t *tex_ligada(GLenum tipo)
{
    int k = tipo == GL_TEXTURE_CUBE_MAP;
    GLuint id = C->un_tex[k][C->unid];
    if (!id) return &C->tex0[k];
    return sgl2_tex(id);
}
void sgl2_glActiveTexture(GLenum t)
{
    if (!C) return;
    if (t < GL_TEXTURE0 || t >= GL_TEXTURE0 + S2_MAX_UNID) { ERRO(GL_INVALID_ENUM); return; }
    C->unid = (int)(t - GL_TEXTURE0);
}
void sgl2_glGenTextures(GLsizei n, GLuint *ids)
{
    if (!C) return;
    if (n < 0) { ERRO(GL_INVALID_VALUE); return; }
    for (int i = 0; i < n; i++) {
        GLuint k = nome_livre((void **)C->tex, C->ntex);
        if (!tex_nova(k)) { ERRO(GL_OUT_OF_MEMORY); return; }
        ids[i] = k;
    }
}
void sgl2_glBindTexture(GLenum alvo, GLuint id)
{
    if (!C) return;
    if (alvo != GL_TEXTURE_2D && alvo != GL_TEXTURE_CUBE_MAP) { ERRO(GL_INVALID_ENUM); return; }
    if (id) {
        s2_tex_t *t = sgl2_tex(id);
        if (!t) t = tex_nova(id);
        if (!t) { ERRO(GL_OUT_OF_MEMORY); return; }
        if (t->alvo && t->alvo != alvo) { ERRO(GL_INVALID_OPERATION); return; }
        t->alvo = alvo;
    }
    C->un_tex[alvo == GL_TEXTURE_CUBE_MAP][C->unid] = id;
}
static void fbo_desanexar(GLenum tipo, GLuint nome)
{
    s2_fbo_t *f = sgl2_fbo(C->fbo_ligado);
    if (!f) return;
    s2_anexo_t *a[3] = { &f->cor, &f->z, &f->est };
    for (int i = 0; i < 3; i++) if (a[i]->tipo == tipo && a[i]->nome == nome) memset(a[i], 0, sizeof *a[i]);
}
void sgl2_glDeleteTextures(GLsizei n, const GLuint *ids)
{
    if (!C) return;
    if (n < 0) { ERRO(GL_INVALID_VALUE); return; }
    for (int i = 0; i < n; i++) {
        s2_tex_t *t = sgl2_tex(ids[i]);
        if (!t) continue;
        for (int k = 0; k < 2; k++) for (int u = 0; u < S2_MAX_UNID; u++) if (C->un_tex[k][u] == ids[i]) C->un_tex[k][u] = 0;
        fbo_desanexar(GL_TEXTURE, ids[i]);
        sgl2_gpu_soltar(&t->g);
        tex_soltar_niveis(t);
        libera(t); C->tex[ids[i]] = NULL;
    }
}
GLboolean sgl2_glIsTexture(GLuint id) { s2_tex_t *t = sgl2_tex(id); return t && t->alvo ? GL_TRUE : GL_FALSE; }

/* formato y tipo de ES 2.0: bytes por píxel o 0 (con el error) */
static int formato(GLenum ifmt, GLenum fmt, GLenum tipo)
{
    if (fmt_profundidad(fmt)) {          /* D486: GL_OES_depth_texture y GL_OES_packed_depth_stencil */
        if (ifmt != fmt) { ERRO(GL_INVALID_OPERATION); return 0; }
        if (fmt == GL_DEPTH_COMPONENT) { if (tipo == GL_UNSIGNED_SHORT) return 2; if (tipo == GL_UNSIGNED_INT) return 4; }
        else if (tipo == GL_UNSIGNED_INT_24_8_OES) return 4;
        ERRO(GL_INVALID_OPERATION);
        return 0;
    }
    if (fmt != GL_RGBA && fmt != GL_RGB && fmt != GL_LUMINANCE && fmt != GL_LUMINANCE_ALPHA && fmt != GL_ALPHA) { ERRO(GL_INVALID_ENUM); return 0; }
    if (tipo != GL_UNSIGNED_BYTE && tipo != GL_UNSIGNED_SHORT_5_6_5 && tipo != GL_UNSIGNED_SHORT_4_4_4_4 && tipo != GL_UNSIGNED_SHORT_5_5_5_1) { ERRO(GL_INVALID_ENUM); return 0; }
    if ((GLenum)ifmt != fmt) { ERRO(ifmt == GL_RGBA || ifmt == GL_RGB || ifmt == GL_LUMINANCE || ifmt == GL_LUMINANCE_ALPHA || ifmt == GL_ALPHA ? GL_INVALID_OPERATION : GL_INVALID_VALUE); return 0; }
    if (tipo == GL_UNSIGNED_BYTE) return fmt == GL_RGBA ? 4 : fmt == GL_RGB ? 3 : fmt == GL_LUMINANCE_ALPHA ? 2 : 1;
    if (tipo == GL_UNSIGNED_SHORT_5_6_5 && fmt == GL_RGB) return 2;
    if ((tipo == GL_UNSIGNED_SHORT_4_4_4_4 || tipo == GL_UNSIGNED_SHORT_5_5_5_1) && fmt == GL_RGBA) return 2;
    ERRO(GL_INVALID_OPERATION);
    return 0;
}
static uint32_t texel(const uint8_t *p, GLenum fmt, GLenum tipo)
{
    unsigned r, g, b, a = 255;
    if (tipo == GL_UNSIGNED_BYTE) {
        switch (fmt) {
        case GL_RGBA: r = p[0]; g = p[1]; b = p[2]; a = p[3]; break;
        case GL_RGB: r = p[0]; g = p[1]; b = p[2]; break;
        case GL_LUMINANCE_ALPHA: r = g = b = p[0]; a = p[1]; break;
        case GL_LUMINANCE: r = g = b = p[0]; break;
        default: r = g = b = 0; a = p[0]; break;
        }
    } else {
        unsigned v = (unsigned)p[0] | ((unsigned)p[1] << 8);
        if (tipo == GL_UNSIGNED_SHORT_5_6_5) { r = (v >> 11) & 31; g = (v >> 5) & 63; b = v & 31; r = (r << 3) | (r >> 2); g = (g << 2) | (g >> 4); b = (b << 3) | (b >> 2); }
        else if (tipo == GL_UNSIGNED_SHORT_4_4_4_4) { r = (v >> 12) & 15; g = (v >> 8) & 15; b = (v >> 4) & 15; a = v & 15; r *= 17; g *= 17; b *= 17; a *= 17; }
        else { r = (v >> 11) & 31; g = (v >> 6) & 31; b = (v >> 1) & 31; a = (v & 1) ? 255 : 0; r = (r << 3) | (r >> 2); g = (g << 3) | (g >> 2); b = (b << 3) | (b >> 2); }
    }
    return (a << 24) | (r << 16) | (g << 8) | b;
}
static void copiar_texels(uint32_t *d, int dpasso, int l, int a, const void *dados, GLenum fmt, GLenum tipo, int bpp)
{
    const uint8_t *s = dados;
    int linha = l * bpp, al = C->unpack_alin;
    linha = (linha + al - 1) / al * al;
    for (int y = 0; y < a; y++) for (int x = 0; x < l; x++) d[y * dpasso + x] = texel(s + y * linha + x * bpp, fmt, tipo);
}
/* D486: los datos de una textura de profundidad (el esténcil de un DEPTH24_STENCIL8 va en «est») a floats de 0 a 1 */
static void copiar_profundidade(s2_nivel_t *n, int x, int y, int l, int a, const void *dados, GLenum fmt, GLenum tipo, int bpp)
{
    const uint8_t *s = dados;
    int linha = l * bpp, al = C->unpack_alin;
    linha = (linha + al - 1) / al * al;
    for (int j = 0; j < a; j++) for (int i = 0; i < l; i++) {
        const uint8_t *p = s + (size_t)j * linha + (size_t)i * bpp;
        size_t k = (size_t)(y + j) * n->larg + x + i;
        if (tipo == GL_UNSIGNED_SHORT) n->z[k] = (float)((unsigned)p[0] | ((unsigned)p[1] << 8)) * (1.0f / 65535.0f);
        else {
            uint32_t v = (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
            if (fmt == GL_DEPTH_STENCIL_OES) { n->z[k] = (float)(v >> 8) * (1.0f / 16777215.0f); n->est[k] = (uint8_t)v; }
            else n->z[k] = (float)((double)v * (1.0 / 4294967295.0));
        }
    }
}
static int log2i(int v) { int k = 0; while ((1 << (k + 1)) <= v) k++; return k; }
void sgl2_glTexImage2D(GLenum alvo, GLint nivel, GLint ifmt, GLsizei l, GLsizei a, GLint borda, GLenum fmt, GLenum tipo, const void *dados)
{
    if (!C) return;
    int face; GLenum tt;
    if (!alvo_cara(alvo, &face, &tt)) { ERRO(GL_INVALID_ENUM); return; }
    if (nivel < 0 || nivel > log2i(S2_MAX_LADO) || l < 0 || a < 0 || l > (S2_MAX_LADO >> nivel) || a > (S2_MAX_LADO >> nivel) || borda != 0) { ERRO(GL_INVALID_VALUE); return; }
    if (tt == GL_TEXTURE_CUBE_MAP && l != a) { ERRO(GL_INVALID_VALUE); return; }
    int bpp = formato((GLenum)ifmt, fmt, tipo);
    if (!bpp) return;
    s2_tex_t *t = tex_ligada(tt);
    if (!t) { ERRO(GL_INVALID_OPERATION); return; }
    s2_nivel_t *n = &t->niv[face][nivel];
    uint32_t *px = NULL;
    if (fmt_profundidad(fmt)) {          /* D486: una textura de profundidad no es de cubo ni tiene px */
        if (tt != GL_TEXTURE_2D) { ERRO(GL_INVALID_OPERATION); return; }
        float *z = NULL; uint8_t *est = NULL;
        if (l && a) {
            z = aloca((size_t)l * a * sizeof(float));
            est = fmt == GL_DEPTH_STENCIL_OES ? aloca_zerado((size_t)l * a) : NULL;
            if (!z || (fmt == GL_DEPTH_STENCIL_OES && !est)) { if (z) libera(z); if (est) libera(est); ERRO(GL_OUT_OF_MEMORY); return; }
        }
        nivel_soltar(n);
        n->z = z; n->est = est; n->larg = l; n->alt = a; n->fmt = fmt;
        n->zb = (uint8_t)(fmt == GL_DEPTH_STENCIL_OES ? 24 : tipo == GL_UNSIGNED_SHORT ? 16 : 32);
        if (z) { if (dados) copiar_profundidade(n, 0, 0, l, a, dados, fmt, tipo, bpp); else for (size_t i = 0; i < (size_t)l * a; i++) z[i] = 1.0f; }
        t->completa = -1; t->ver++;
        return;
    }
    if (l && a) {
        px = aloca((size_t)l * a * 4);
        if (!px) { ERRO(GL_OUT_OF_MEMORY); return; }
        if (dados) copiar_texels(px, l, l, a, dados, fmt, tipo, bpp);
        else memset(px, 0, (size_t)l * a * 4);
    }
    nivel_soltar(n);
    n->px = px; n->larg = l; n->alt = a; n->fmt = fmt;
    t->completa = -1; t->ver++;
}
void sgl2_glTexSubImage2D(GLenum alvo, GLint nivel, GLint x, GLint y, GLsizei l, GLsizei a, GLenum fmt, GLenum tipo, const void *dados)
{
    if (!C) return;
    int face; GLenum tt;
    if (!alvo_cara(alvo, &face, &tt)) { ERRO(GL_INVALID_ENUM); return; }
    if (nivel < 0 || nivel >= S2_NIVEIS) { ERRO(GL_INVALID_VALUE); return; }
    s2_tex_t *t = tex_ligada(tt);
    if (!t) { ERRO(GL_INVALID_OPERATION); return; }
    s2_nivel_t *n = &t->niv[face][nivel];
    if (!nivel_con_datos(n) && (n->larg || n->alt)) { ERRO(GL_INVALID_OPERATION); return; }
    if (x < 0 || y < 0 || l < 0 || a < 0 || x + l > n->larg || y + a > n->alt) { ERRO(GL_INVALID_VALUE); return; }
    int bpp = formato(fmt, fmt, tipo);
    if (!bpp) return;
    if (fmt != n->fmt) { ERRO(GL_INVALID_OPERATION); return; }
    if (n->z) {                          /* D486 */
        if (l && a && dados) copiar_profundidade(n, x, y, l, a, dados, fmt, tipo, bpp);
        t->ver++;
        return;
    }
    if (l && a && dados) copiar_texels(n->px + (size_t)y * n->larg + x, n->larg, l, a, dados, fmt, tipo, bpp);
    t->ver++;
}
void sgl2_glCompressedTexImage2D(GLenum alvo, GLint nivel, GLenum ifmt, GLsizei l, GLsizei a, GLint b, GLsizei tam, const void *d)
{ (void)alvo; (void)nivel; (void)ifmt; (void)l; (void)a; (void)b; (void)tam; (void)d; if (C) ERRO(GL_INVALID_ENUM); }
void sgl2_glCompressedTexSubImage2D(GLenum alvo, GLint nivel, GLint x, GLint y, GLsizei l, GLsizei a, GLenum f, GLsizei tam, const void *d)
{ (void)alvo; (void)nivel; (void)x; (void)y; (void)l; (void)a; (void)f; (void)tam; (void)d; if (C) ERRO(GL_INVALID_ENUM); }

static GLint tex_param_ok(GLenum p, GLint v)
{
    switch (p) {
    case GL_TEXTURE_MIN_FILTER: return v == GL_NEAREST || v == GL_LINEAR || (v >= GL_NEAREST_MIPMAP_NEAREST && v <= GL_LINEAR_MIPMAP_LINEAR);
    case GL_TEXTURE_MAG_FILTER: return v == GL_NEAREST || v == GL_LINEAR;
    case GL_TEXTURE_WRAP_S: case GL_TEXTURE_WRAP_T: return v == GL_REPEAT || v == GL_CLAMP_TO_EDGE || v == GL_MIRRORED_REPEAT;
    }
    return -1;
}
void sgl2_glTexParameteri(GLenum alvo, GLenum p, GLint v)
{
    if (!C) return;
    if (alvo != GL_TEXTURE_2D && alvo != GL_TEXTURE_CUBE_MAP) { ERRO(GL_INVALID_ENUM); return; }
    int ok = tex_param_ok(p, v);
    if (ok <= 0) { ERRO(GL_INVALID_ENUM); return; }
    s2_tex_t *t = tex_ligada(alvo);
    if (!t) return;
    if (p == GL_TEXTURE_MIN_FILTER) t->min_f = (GLenum)v;
    else if (p == GL_TEXTURE_MAG_FILTER) t->mag_f = (GLenum)v;
    else if (p == GL_TEXTURE_WRAP_S) t->wrap_s = (GLenum)v;
    else t->wrap_t = (GLenum)v;
    t->completa = -1;
}
void sgl2_glTexParameterf(GLenum alvo, GLenum p, GLfloat v) { sgl2_glTexParameteri(alvo, p, (GLint)v); }
void sgl2_glTexParameteriv(GLenum alvo, GLenum p, const GLint *v) { if (v) sgl2_glTexParameteri(alvo, p, v[0]); }
void sgl2_glTexParameterfv(GLenum alvo, GLenum p, const GLfloat *v) { if (v) sgl2_glTexParameteri(alvo, p, (GLint)v[0]); }
void sgl2_glGetTexParameteriv(GLenum alvo, GLenum p, GLint *r)
{
    if (!C) return;
    if (alvo != GL_TEXTURE_2D && alvo != GL_TEXTURE_CUBE_MAP) { ERRO(GL_INVALID_ENUM); return; }
    s2_tex_t *t = tex_ligada(alvo);
    if (!t) return;
    switch (p) {
    case GL_TEXTURE_MIN_FILTER: *r = (GLint)t->min_f; break;
    case GL_TEXTURE_MAG_FILTER: *r = (GLint)t->mag_f; break;
    case GL_TEXTURE_WRAP_S: *r = (GLint)t->wrap_s; break;
    case GL_TEXTURE_WRAP_T: *r = (GLint)t->wrap_t; break;
    default: ERRO(GL_INVALID_ENUM);
    }
}
void sgl2_glGetTexParameterfv(GLenum alvo, GLenum p, GLfloat *r) { GLint v = 0; GLenum e0 = C ? C->erro : 0; sgl2_glGetTexParameteriv(alvo, p, &v); if (C && C->erro == e0) *r = (GLfloat)v; }

int sgl2_tex_completa(s2_tex_t *t)
{
    if (t->completa >= 0) return t->completa;
    int nf = t->alvo == GL_TEXTURE_CUBE_MAP ? 6 : 1;
    s2_nivel_t *n0 = &t->niv[0][0];
    t->completa = 0; t->nniv = 1;
    if (!nivel_con_datos(n0) || n0->larg <= 0 || n0->alt <= 0) return 0;
    for (int f = 1; f < nf; f++) {
        s2_nivel_t *n = &t->niv[f][0];
        if (!nivel_con_datos(n) || n->larg != n0->larg || n->alt != n0->alt || n->fmt != n0->fmt) return 0;
    }
    int mip = t->min_f != GL_NEAREST && t->min_f != GL_LINEAR;
    if (mip) {
        int l = n0->larg, a = n0->alt, k = 0;
        while (l > 1 || a > 1) {
            l = l > 1 ? l / 2 : 1; a = a > 1 ? a / 2 : 1; k++;
            if (k >= S2_NIVEIS) return 0;
            for (int f = 0; f < nf; f++) {
                s2_nivel_t *n = &t->niv[f][k];
                if (!nivel_con_datos(n) || n->larg != l || n->alt != a || n->fmt != n0->fmt) return 0;
            }
        }
        t->nniv = k + 1;
    }
    t->completa = 1;
    return 1;
}
void sgl2_glGenerateMipmap(GLenum alvo)
{
    if (!C) return;
    if (alvo != GL_TEXTURE_2D && alvo != GL_TEXTURE_CUBE_MAP) { ERRO(GL_INVALID_ENUM); return; }
    s2_tex_t *t = tex_ligada(alvo);
    if (!t) return;
    int nf = alvo == GL_TEXTURE_CUBE_MAP ? 6 : 1;
    for (int f = 0; f < nf; f++) {
        s2_nivel_t *n0 = &t->niv[f][0];
        if (!n0->px || fmt_profundidad(n0->fmt) || (nf == 6 && (n0->larg != t->niv[0][0].larg || n0->larg != n0->alt))) { ERRO(GL_INVALID_OPERATION); return; }
    }
    for (int f = 0; f < nf; f++) {
        for (int k = 1; k < S2_NIVEIS; k++) {
            s2_nivel_t *o = &t->niv[f][k - 1], *d = &t->niv[f][k];
            if (o->larg <= 1 && o->alt <= 1) break;
            int l = o->larg > 1 ? o->larg / 2 : 1, a = o->alt > 1 ? o->alt / 2 : 1;
            uint32_t *px = aloca((size_t)l * a * 4);
            if (!px) { ERRO(GL_OUT_OF_MEMORY); return; }
            for (int y = 0; y < a; y++)
                for (int x = 0; x < l; x++) {
                    int x0 = x * 2, y0 = y * 2, x1 = x0 + 1 < o->larg ? x0 + 1 : x0, y1 = y0 + 1 < o->alt ? y0 + 1 : y0;
                    uint32_t p[4] = { o->px[y0 * o->larg + x0], o->px[y0 * o->larg + x1], o->px[y1 * o->larg + x0], o->px[y1 * o->larg + x1] };
                    uint32_t r = 0;
                    for (int s = 0; s < 32; s += 8) { unsigned v = 0; for (int i = 0; i < 4; i++) v += (p[i] >> s) & 255; r |= ((v + 2) / 4) << s; }
                    px[y * l + x] = r;
                }
            nivel_soltar(d);
            d->px = px; d->larg = l; d->alt = a; d->fmt = o->fmt;
            t->ver++;
        }
    }
    t->completa = -1;
}

/* ------------------------------------------------------------ renderbuffers y framebuffers */
static void gerar_nomes(GLsizei n, GLuint *ids, void ***tab, int *nt, size_t tam)
{
    if (n < 0) { ERRO(GL_INVALID_VALUE); return; }
    for (int i = 0; i < n; i++) {
        GLuint k = nome_livre(*tab, *nt);
        void **p = tabela(tab, nt, k, 1);
        void *o = p ? aloca_zerado(tam) : NULL;
        if (!o) { ERRO(GL_OUT_OF_MEMORY); return; }
        *(GLuint *)o = k; *p = o; ids[i] = k;
    }
}
void glGenRenderbuffers(GLsizei n, GLuint *ids) { if (C) gerar_nomes(n, ids, (void ***)&C->rb, &C->nrb, sizeof(s2_rb_t)); }
void glGenFramebuffers(GLsizei n, GLuint *ids) { if (C) gerar_nomes(n, ids, (void ***)&C->fbo, &C->nfbo, sizeof(s2_fbo_t)); }
void glBindRenderbuffer(GLenum alvo, GLuint id)
{
    if (!C) return;
    if (alvo != GL_RENDERBUFFER) { ERRO(GL_INVALID_ENUM); return; }
    if (id && !sgl2_rb(id)) {
        void **p = tabela((void ***)&C->rb, &C->nrb, id, 1);
        s2_rb_t *r = p ? aloca_zerado(sizeof *r) : NULL;
        if (!r) { ERRO(GL_OUT_OF_MEMORY); return; }
        r->nome = id; *p = r;
    }
    C->rb_ligado = id;
}
void glBindFramebuffer(GLenum alvo, GLuint id)
{
    if (!C) return;
    if (alvo != GL_FRAMEBUFFER) { ERRO(GL_INVALID_ENUM); return; }
    if (id && !sgl2_fbo(id)) {
        void **p = tabela((void ***)&C->fbo, &C->nfbo, id, 1);
        s2_fbo_t *f = p ? aloca_zerado(sizeof *f) : NULL;
        if (!f) { ERRO(GL_OUT_OF_MEMORY); return; }
        f->nome = id; *p = f;
    }
    C->fbo_ligado = id;
}
static void rb_soltar(s2_rb_t *r)
{
    if (r->cor) libera(r->cor);
    if (r->z) libera(r->z);
    if (r->est) libera(r->est);
    r->cor = NULL; r->z = NULL; r->est = NULL;
}
void glDeleteRenderbuffers(GLsizei n, const GLuint *ids)
{
    if (!C) return;
    if (n < 0) { ERRO(GL_INVALID_VALUE); return; }
    for (int i = 0; i < n; i++) {
        s2_rb_t *r = sgl2_rb(ids[i]);
        if (!r) continue;
        if (C->rb_ligado == ids[i]) C->rb_ligado = 0;
        fbo_desanexar(GL_RENDERBUFFER, ids[i]);
        sgl2_gpu_soltar(&r->g);
        rb_soltar(r); libera(r); C->rb[ids[i]] = NULL;
    }
}
void glDeleteFramebuffers(GLsizei n, const GLuint *ids)
{
    if (!C) return;
    if (n < 0) { ERRO(GL_INVALID_VALUE); return; }
    for (int i = 0; i < n; i++) {
        s2_fbo_t *f = sgl2_fbo(ids[i]);
        if (!f) continue;
        if (C->fbo_ligado == ids[i]) C->fbo_ligado = 0;
        sgl2_gpu_soltar(&f->gz);
        libera(f); C->fbo[ids[i]] = NULL;
    }
}
GLboolean glIsRenderbuffer(GLuint id) { return C && sgl2_rb(id) ? GL_TRUE : GL_FALSE; }
GLboolean glIsFramebuffer(GLuint id) { return C && sgl2_fbo(id) ? GL_TRUE : GL_FALSE; }
void glRenderbufferStorage(GLenum alvo, GLenum fmt, GLsizei l, GLsizei a)
{
    if (!C) return;
    if (alvo != GL_RENDERBUFFER) { ERRO(GL_INVALID_ENUM); return; }
    int tipo = fmt == GL_RGBA4 || fmt == GL_RGB5_A1 || fmt == GL_RGB565 || fmt == GL_RGBA8_OES || fmt == GL_RGB8_OES ? 1 :
               fmt == GL_DEPTH_COMPONENT16 || fmt == GL_DEPTH_COMPONENT24_OES ? 2 : fmt == GL_STENCIL_INDEX8 ? 3 :
               fmt == GL_DEPTH24_STENCIL8_OES ? 4 : 0;       /* D486: GL_OES_depth24 y GL_OES_packed_depth_stencil */
    if (!tipo) { ERRO(GL_INVALID_ENUM); return; }
    if (l < 0 || a < 0 || l > S2_MAX_LADO || a > S2_MAX_LADO) { ERRO(GL_INVALID_VALUE); return; }
    s2_rb_t *r = sgl2_rb(C->rb_ligado);
    if (!r) { ERRO(GL_INVALID_OPERATION); return; }
    rb_soltar(r);
    r->ver++;
    size_t n = (size_t)l * a;
    if (n) {
        if (tipo == 1) { r->cor = aloca_zerado(n * 4); if (!r->cor) { ERRO(GL_OUT_OF_MEMORY); return; } }
        if (tipo == 2 || tipo == 4) { r->z = aloca(n * sizeof(float)); if (!r->z) { ERRO(GL_OUT_OF_MEMORY); return; } for (size_t i = 0; i < n; i++) r->z[i] = 1.0f; }
        if (tipo == 3 || tipo == 4) { r->est = aloca_zerado(n); if (!r->est) { ERRO(GL_OUT_OF_MEMORY); return; } }
    }
    r->fmt = fmt; r->larg = l; r->alt = a;
}
void glGetRenderbufferParameteriv(GLenum alvo, GLenum p, GLint *v)
{
    if (!C) return;
    if (alvo != GL_RENDERBUFFER) { ERRO(GL_INVALID_ENUM); return; }
    s2_rb_t *r = sgl2_rb(C->rb_ligado);
    if (!r) { ERRO(GL_INVALID_OPERATION); return; }
    int R = 0, G = 0, B = 0, A = 0, Z = 0, S = 0;
    switch (r->fmt) {
    case GL_RGBA4: R = G = B = A = 4; break;
    case GL_RGB5_A1: R = G = B = 5; A = 1; break;
    case GL_RGB565: R = B = 5; G = 6; break;
    case GL_RGBA8_OES: R = G = B = A = 8; break;
    case GL_RGB8_OES: R = G = B = 8; break;
    case GL_DEPTH_COMPONENT16: Z = 16; break;
    case GL_DEPTH_COMPONENT24_OES: Z = 24; break;
    case GL_STENCIL_INDEX8: S = 8; break;
    case GL_DEPTH24_STENCIL8_OES: Z = 24; S = 8; break;
    }
    switch (p) {
    case GL_RENDERBUFFER_WIDTH: *v = r->larg; break;
    case GL_RENDERBUFFER_HEIGHT: *v = r->alt; break;
    case GL_RENDERBUFFER_INTERNAL_FORMAT: *v = r->fmt ? (GLint)r->fmt : GL_RGBA4; break;
    case GL_RENDERBUFFER_RED_SIZE: *v = R; break;
    case GL_RENDERBUFFER_GREEN_SIZE: *v = G; break;
    case GL_RENDERBUFFER_BLUE_SIZE: *v = B; break;
    case GL_RENDERBUFFER_ALPHA_SIZE: *v = A; break;
    case GL_RENDERBUFFER_DEPTH_SIZE: *v = Z; break;
    case GL_RENDERBUFFER_STENCIL_SIZE: *v = S; break;
    default: ERRO(GL_INVALID_ENUM);
    }
}
static s2_anexo_t *anexo(s2_fbo_t *f, GLenum a)
{
    return a == GL_COLOR_ATTACHMENT0 ? &f->cor : a == GL_DEPTH_ATTACHMENT || a == GL_DEPTH_STENCIL_ATTACHMENT ? &f->z : a == GL_STENCIL_ATTACHMENT ? &f->est : NULL;
}
void glFramebufferRenderbuffer(GLenum alvo, GLenum at, GLenum rbalvo, GLuint rb)
{
    if (!C) return;
    if (alvo != GL_FRAMEBUFFER || (rb && rbalvo != GL_RENDERBUFFER)) { ERRO(GL_INVALID_ENUM); return; }
    s2_fbo_t *f = sgl2_fbo(C->fbo_ligado);
    if (!f) { ERRO(GL_INVALID_OPERATION); return; }
    s2_anexo_t *a = anexo(f, at);
    if (!a) { ERRO(GL_INVALID_ENUM); return; }
    if (rb && !sgl2_rb(rb)) { ERRO(GL_INVALID_OPERATION); return; }
    memset(a, 0, sizeof *a);
    if (rb) { a->tipo = GL_RENDERBUFFER; a->nome = rb; }
    if (at == GL_DEPTH_STENCIL_ATTACHMENT) { f->est = *a; }       /* D486: a los dos puntos */
}
void glFramebufferTexture2D(GLenum alvo, GLenum at, GLenum talvo, GLuint tex, GLint nivel)
{
    if (!C) return;
    if (alvo != GL_FRAMEBUFFER) { ERRO(GL_INVALID_ENUM); return; }
    s2_fbo_t *f = sgl2_fbo(C->fbo_ligado);
    if (!f) { ERRO(GL_INVALID_OPERATION); return; }
    s2_anexo_t *a = anexo(f, at);
    if (!a) { ERRO(GL_INVALID_ENUM); return; }
    if (!tex) { memset(a, 0, sizeof *a); if (at == GL_DEPTH_STENCIL_ATTACHMENT) f->est = *a; return; }
    int face; GLenum tt;
    if (!alvo_cara(talvo, &face, &tt)) { ERRO(GL_INVALID_ENUM); return; }
    s2_tex_t *t = sgl2_tex(tex);
    if (!t) { ERRO(GL_INVALID_OPERATION); return; }
    if (nivel != 0) { ERRO(GL_INVALID_VALUE); return; }
    if (t->alvo != tt) { ERRO(GL_INVALID_OPERATION); return; }
    a->tipo = GL_TEXTURE; a->nome = tex; a->nivel = nivel; a->face = face;
    if (at == GL_DEPTH_STENCIL_ATTACHMENT) f->est = *a;
}
void glGetFramebufferAttachmentParameteriv(GLenum alvo, GLenum at, GLenum p, GLint *v)
{
    if (!C) return;
    if (alvo != GL_FRAMEBUFFER) { ERRO(GL_INVALID_ENUM); return; }
    s2_fbo_t *f = sgl2_fbo(C->fbo_ligado);
    if (!f) { ERRO(GL_INVALID_OPERATION); return; }
    s2_anexo_t *a = anexo(f, at);
    if (!a) { ERRO(GL_INVALID_ENUM); return; }
    switch (p) {
    case GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE: *v = (GLint)a->tipo; break;
    case GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME: if (!a->tipo) { ERRO(GL_INVALID_ENUM); break; } *v = (GLint)a->nome; break;
    case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL: if (a->tipo != GL_TEXTURE) { ERRO(GL_INVALID_ENUM); break; } *v = a->nivel; break;
    case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE: {
        if (a->tipo != GL_TEXTURE) { ERRO(GL_INVALID_ENUM); break; }
        s2_tex_t *t = sgl2_tex(a->nome);
        *v = t && t->alvo == GL_TEXTURE_CUBE_MAP ? (GLint)(GL_TEXTURE_CUBE_MAP_POSITIVE_X + a->face) : 0;
        break;
    }
    default: ERRO(GL_INVALID_ENUM);
    }
}
/* tamaño y forma de un anexo: 0 si no vale para ese sitio */
static int anexo_medir(const s2_anexo_t *a, int sitio, int *l, int *h)
{
    if (a->tipo == GL_TEXTURE) {
        s2_tex_t *t = sgl2_tex(a->nome);
        if (!t) return 0;
        s2_nivel_t *n = &t->niv[a->face][a->nivel];
        if (n->larg <= 0 || n->alt <= 0) return 0;
        if (sitio == 0 && (!n->px || (n->fmt != GL_RGBA && n->fmt != GL_RGB))) return 0;
        if (sitio == 1 && !n->z) return 0;                               /* D486: profundidad en una textura de profundidad */
        if (sitio == 2 && (!n->z || n->fmt != GL_DEPTH_STENCIL_OES)) return 0;
        *l = n->larg; *h = n->alt;
        return 1;
    }
    s2_rb_t *r = sgl2_rb(a->nome);
    if (!r || !r->larg || !r->alt) return 0;
    if (sitio == 0 && !r->cor) return 0;
    if (sitio == 1 && !r->z) return 0;
    if (sitio == 2 && !r->est) return 0;
    *l = r->larg; *h = r->alt;
    return 1;
}
GLenum sgl2_fbo_estado(sgl2_ctx_t *c)
{
    (void)c;
    s2_fbo_t *f = sgl2_fbo(C->fbo_ligado);
    if (!f) return GL_FRAMEBUFFER_COMPLETE;
    s2_anexo_t *a[3] = { &f->cor, &f->z, &f->est };
    int n = 0, L = -1, H = -1;
    for (int i = 0; i < 3; i++) {
        if (!a[i]->tipo) continue;
        int l, h;
        if (!anexo_medir(a[i], i, &l, &h)) return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
        if (n && (l != L || h != H)) return GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS;
        L = l; H = h; n++;
    }
    return n ? GL_FRAMEBUFFER_COMPLETE : GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT;
}
GLenum glCheckFramebufferStatus(GLenum alvo)
{
    if (!C) return 0;
    if (alvo != GL_FRAMEBUFFER) { ERRO(GL_INVALID_ENUM); return 0; }
    return sgl2_fbo_estado(C);
}
int sgl2_alvo(sgl2_ctx_t *c, s2_alvo_t *a) { return sgl2_alvo_x(c, a, 0); }
int sgl2_alvo_x(sgl2_ctx_t *c, s2_alvo_t *a, int para_gpu)
{
    if (!para_gpu) sgl2_gpu_sincronizar(c);     /* D480: la CPU lee o escribe el destino: antes, lo que la GPU dibujó */
    memset(a, 0, sizeof *a);
    s2_fbo_t *f = sgl2_fbo(c->fbo_ligado);
    if (!f) {
        uint32_t *px; int l, h;
        if (!c->base || !sgl_destino(c->base, &px, &l, &h) || !px || l <= 0 || h <= 0) return 0;
        if (l != c->def_larg || h != c->def_alt || !c->z_def) {
            size_t n = (size_t)l * h;
            if (c->z_def) libera(c->z_def);
            if (c->est_def) libera(c->est_def);
            c->z_def = aloca(n * sizeof(float)); c->est_def = c->est_cfg ? aloca_zerado(n) : NULL;
            if (!c->z_def) { c->def_larg = 0; return 0; }
            for (size_t i = 0; i < n; i++) c->z_def[i] = 1.0f;
            c->def_larg = l; c->def_alt = h;
        }
        a->cor = px + (size_t)(h - 1) * l; a->passo = -l;
        a->z = c->z_def; a->passo_z = l;
        a->est = c->est_def; a->passo_e = l;
        a->larg = l; a->alt = h; a->alfa = 0; a->zbits = c->zbits_cfg; a->estbits = c->est_cfg;
        a->g = &c->gdef; a->gz = &c->gdef_z; a->ver = &c->def_ver; a->gpu_ok = 1;
        return 1;
    }
    if (sgl2_fbo_estado(c) != GL_FRAMEBUFFER_COMPLETE) return 0;
    if (f->cor.tipo == GL_TEXTURE) {
        s2_tex_t *t = sgl2_tex(f->cor.nome);
        s2_nivel_t *n = &t->niv[f->cor.face][f->cor.nivel];
        a->cor = n->px; a->passo = n->larg; a->larg = n->larg; a->alt = n->alt; a->alfa = n->fmt == GL_RGBA;
        a->g = &t->g; a->ver = &t->ver; a->gpu_ok = f->cor.nivel == 0 && f->cor.face == 0;
        t->completa = -1;
    } else if (f->cor.tipo == GL_RENDERBUFFER) {
        s2_rb_t *r = sgl2_rb(f->cor.nome);
        a->cor = r->cor; a->passo = r->larg; a->larg = r->larg; a->alt = r->alt;
        a->alfa = r->fmt != GL_RGB565 && r->fmt != GL_RGB8_OES;
        a->g = &r->g; a->ver = &r->ver; a->gpu_ok = 1;
    }
    a->gz = &f->gz;
    /* D486: el Z y el esténcil pueden ser renderbuffers (de 16 o 24 bits; el esténcil, de 8) o texturas de profundidad; con una textura el
     * dibujo va por la CPU (la GPU guarda el Z en su propio recurso, que no se puede muestrear: ver docs/saguigl2.md) */
    if (f->z.tipo == GL_TEXTURE) {
        s2_tex_t *t = sgl2_tex(f->z.nome); s2_nivel_t *n = &t->niv[f->z.face][f->z.nivel];
        a->z = n->z; a->passo_z = n->larg; a->larg = n->larg; a->alt = n->alt; a->zbits = n->zb == 16 ? 16 : 24; a->gpu_ok = 0;
    } else if (f->z.tipo) {
        s2_rb_t *r = sgl2_rb(f->z.nome); a->z = r->z; a->passo_z = r->larg; a->larg = r->larg; a->alt = r->alt; a->zbits = r->fmt == GL_DEPTH_COMPONENT16 ? 16 : 24;
    }
    if (f->est.tipo == GL_TEXTURE) {
        s2_tex_t *t = sgl2_tex(f->est.nome); s2_nivel_t *n = &t->niv[f->est.face][f->est.nivel];
        a->est = n->est; a->passo_e = n->larg; a->larg = n->larg; a->alt = n->alt; a->estbits = 8; a->gpu_ok = 0;
    } else if (f->est.tipo) {
        s2_rb_t *r = sgl2_rb(f->est.nome); a->est = r->est; a->passo_e = r->larg; a->larg = r->larg; a->alt = r->alt; a->estbits = 8;
    }
    return 1;
}

/* copiar del framebuffer a una textura */
static int ler_px(s2_alvo_t *a, int x, int y, uint32_t *p)
{
    if (!a->cor || x < 0 || y < 0 || x >= a->larg || y >= a->alt) return 0;
    *p = a->cor[(long)y * a->passo + x];
    if (!a->alfa) *p |= 0xFF000000u;
    return 1;
}
static uint32_t conv_copia(uint32_t p, GLenum fmt)
{
    unsigned r = (p >> 16) & 255, a = p >> 24;
    switch (fmt) {
    case GL_RGB: return p | 0xFF000000u;
    case GL_LUMINANCE: return 0xFF000000u | (r << 16) | (r << 8) | r;
    case GL_LUMINANCE_ALPHA: return (a << 24) | (r << 16) | (r << 8) | r;
    case GL_ALPHA: return a << 24;
    }
    return p;
}
static int copia_ok(s2_alvo_t *a, GLenum fmt)
{
    if (fmt != GL_RGBA && fmt != GL_RGB && fmt != GL_LUMINANCE && fmt != GL_LUMINANCE_ALPHA && fmt != GL_ALPHA) { ERRO(GL_INVALID_ENUM); return 0; }
    if (!a->cor) { ERRO(GL_INVALID_OPERATION); return 0; }
    if (!a->alfa && (fmt == GL_RGBA || fmt == GL_LUMINANCE_ALPHA || fmt == GL_ALPHA)) { ERRO(GL_INVALID_OPERATION); return 0; }
    return 1;
}
void sgl2_glCopyTexImage2D(GLenum alvo, GLint nivel, GLenum fmt, GLint x, GLint y, GLsizei l, GLsizei h, GLint borda)
{
    if (!C) return;
    int face; GLenum tt;
    if (!alvo_cara(alvo, &face, &tt)) { ERRO(GL_INVALID_ENUM); return; }
    if (nivel < 0 || nivel >= S2_NIVEIS || l < 0 || h < 0 || l > S2_MAX_LADO || h > S2_MAX_LADO || borda) { ERRO(GL_INVALID_VALUE); return; }
    if (tt == GL_TEXTURE_CUBE_MAP && l != h) { ERRO(GL_INVALID_VALUE); return; }
    s2_alvo_t a;
    if (!sgl2_alvo(C, &a)) { ERRO(GL_INVALID_FRAMEBUFFER_OPERATION); return; }
    if (!copia_ok(&a, fmt)) return;
    s2_tex_t *t = tex_ligada(tt);
    if (!t) return;
    uint32_t *px = l && h ? aloca((size_t)l * h * 4) : NULL;
    if (l && h && !px) { ERRO(GL_OUT_OF_MEMORY); return; }
    for (int j = 0; j < h; j++) for (int i = 0; i < l; i++) { uint32_t p = 0; ler_px(&a, x + i, y + j, &p); px[j * l + i] = conv_copia(p, fmt); }
    s2_nivel_t *n = &t->niv[face][nivel];
    nivel_soltar(n);
    n->px = px; n->larg = l; n->alt = h; n->fmt = fmt;
    t->completa = -1; t->ver++;
}
void sgl2_glCopyTexSubImage2D(GLenum alvo, GLint nivel, GLint xo, GLint yo, GLint x, GLint y, GLsizei l, GLsizei h)
{
    if (!C) return;
    int face; GLenum tt;
    if (!alvo_cara(alvo, &face, &tt)) { ERRO(GL_INVALID_ENUM); return; }
    if (nivel < 0 || nivel >= S2_NIVEIS) { ERRO(GL_INVALID_VALUE); return; }
    s2_tex_t *t = tex_ligada(tt);
    if (!t) return;
    s2_nivel_t *n = &t->niv[face][nivel];
    if (!n->px) { ERRO(GL_INVALID_OPERATION); return; }
    if (xo < 0 || yo < 0 || l < 0 || h < 0 || xo + l > n->larg || yo + h > n->alt) { ERRO(GL_INVALID_VALUE); return; }
    s2_alvo_t a;
    if (!sgl2_alvo(C, &a)) { ERRO(GL_INVALID_FRAMEBUFFER_OPERATION); return; }
    if (!copia_ok(&a, n->fmt)) return;
    for (int j = 0; j < h; j++) for (int i = 0; i < l; i++) {
        uint32_t p;
        if (ler_px(&a, x + i, y + j, &p)) n->px[(yo + j) * n->larg + xo + i] = conv_copia(p, n->fmt);
    }
    t->ver++;
}

void sgl2_glReadPixels(GLint x, GLint y, GLsizei l, GLsizei h, GLenum fmt, GLenum tipo, void *dados)
{
    if (!C) return;
    if (l < 0 || h < 0) { ERRO(GL_INVALID_VALUE); return; }
    if (fmt != GL_RGBA && fmt != GL_RGB && fmt != GL_ALPHA && fmt != GL_LUMINANCE && fmt != GL_LUMINANCE_ALPHA) { ERRO(GL_INVALID_ENUM); return; }
    if (tipo != GL_UNSIGNED_BYTE && tipo != GL_UNSIGNED_SHORT_5_6_5 && tipo != GL_UNSIGNED_SHORT_4_4_4_4 && tipo != GL_UNSIGNED_SHORT_5_5_5_1) { ERRO(GL_INVALID_ENUM); return; }
    if (fmt != GL_RGBA || tipo != GL_UNSIGNED_BYTE) { ERRO(GL_INVALID_OPERATION); return; }
    s2_alvo_t a;
    if (!sgl2_alvo(C, &a)) { ERRO(GL_INVALID_FRAMEBUFFER_OPERATION); return; }
    uint8_t *d = dados;
    int linha = l * 4;
    for (int j = 0; j < h; j++)
        for (int i = 0; i < l; i++) {
            uint32_t p;
            if (!ler_px(&a, x + i, y + j, &p)) continue;
            uint8_t *q = d + (size_t)j * linha + i * 4;
            q[0] = (uint8_t)(p >> 16); q[1] = (uint8_t)(p >> 8); q[2] = (uint8_t)p; q[3] = (uint8_t)(p >> 24);
        }
}

/* ------------------------------------------------------------ dibujar y limpiar (sgl2_desenho.c) */
void sgl2_glClear(GLbitfield m)
{
    if (!C) return;
    if (m & ~(GLbitfield)(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT)) { ERRO(GL_INVALID_VALUE); return; }
    if (sgl2_fbo_estado(C) != GL_FRAMEBUFFER_COMPLETE) { ERRO(GL_INVALID_FRAMEBUFFER_OPERATION); return; }
    sgl2_limpar(C, m);
}
static int modo_ok(GLenum m) { return m <= GL_TRIANGLE_FAN; }
void sgl2_glDrawArrays(GLenum m, GLint p, GLsizei n)
{
    if (!C) return;
    if (!modo_ok(m)) { ERRO(GL_INVALID_ENUM); return; }
    if (p < 0 || n < 0) { ERRO(GL_INVALID_VALUE); return; }
    if (sgl2_fbo_estado(C) != GL_FRAMEBUFFER_COMPLETE) { ERRO(GL_INVALID_FRAMEBUFFER_OPERATION); return; }
    sgl2_desenhar(C, m, p, n, 0, NULL);
}
void sgl2_glDrawElements(GLenum m, GLsizei n, GLenum tipo, const void *idx)
{
    if (!C) return;
    if (!modo_ok(m)) { ERRO(GL_INVALID_ENUM); return; }
    if (tipo != GL_UNSIGNED_BYTE && tipo != GL_UNSIGNED_SHORT && tipo != GL_UNSIGNED_INT) { ERRO(GL_INVALID_ENUM); return; }
    if (n < 0) { ERRO(GL_INVALID_VALUE); return; }
    if (sgl2_fbo_estado(C) != GL_FRAMEBUFFER_COMPLETE) { ERRO(GL_INVALID_FRAMEBUFFER_OPERATION); return; }
    sgl2_desenhar(C, m, 0, n, tipo, idx);
}

/* ------------------------------------------------------------ atributos */
void glVertexAttribPointer(GLuint i, GLint tam, GLenum tipo, GLboolean norm, GLsizei passo, const void *p)
{
    if (!C) return;
    if (i >= S2_MAX_ATRIB || tam < 1 || tam > 4 || passo < 0) { ERRO(GL_INVALID_VALUE); return; }
    if (tipo != GL_BYTE && tipo != GL_UNSIGNED_BYTE && tipo != GL_SHORT && tipo != GL_UNSIGNED_SHORT && tipo != GL_FLOAT && tipo != GL_FIXED) { ERRO(GL_INVALID_ENUM); return; }
    s2_atrib_t *a = &C->at[i];
    a->tam = tam; a->tipo = tipo; a->norm = !!norm; a->passo = passo; a->ptr = p; a->buf = C->buf_array;
}
void glEnableVertexAttribArray(GLuint i) { if (!C) return; if (i >= S2_MAX_ATRIB) { ERRO(GL_INVALID_VALUE); return; } C->at[i].ativo = 1; }
void glDisableVertexAttribArray(GLuint i) { if (!C) return; if (i >= S2_MAX_ATRIB) { ERRO(GL_INVALID_VALUE); return; } C->at[i].ativo = 0; }
static void atrib4(GLuint i, float x, float y, float z, float w) { if (!C) return; if (i >= S2_MAX_ATRIB) { ERRO(GL_INVALID_VALUE); return; } C->atual[i][0] = x; C->atual[i][1] = y; C->atual[i][2] = z; C->atual[i][3] = w; }
void glVertexAttrib1f(GLuint i, GLfloat x) { atrib4(i, x, 0, 0, 1); }
void glVertexAttrib2f(GLuint i, GLfloat x, GLfloat y) { atrib4(i, x, y, 0, 1); }
void glVertexAttrib3f(GLuint i, GLfloat x, GLfloat y, GLfloat z) { atrib4(i, x, y, z, 1); }
void glVertexAttrib4f(GLuint i, GLfloat x, GLfloat y, GLfloat z, GLfloat w) { atrib4(i, x, y, z, w); }
void glVertexAttrib1fv(GLuint i, const GLfloat *v) { atrib4(i, v[0], 0, 0, 1); }
void glVertexAttrib2fv(GLuint i, const GLfloat *v) { atrib4(i, v[0], v[1], 0, 1); }
void glVertexAttrib3fv(GLuint i, const GLfloat *v) { atrib4(i, v[0], v[1], v[2], 1); }
void glVertexAttrib4fv(GLuint i, const GLfloat *v) { atrib4(i, v[0], v[1], v[2], v[3]); }
void glGetVertexAttribfv(GLuint i, GLenum p, GLfloat *r)
{
    if (!C) return;
    if (i >= S2_MAX_ATRIB) { ERRO(GL_INVALID_VALUE); return; }
    s2_atrib_t *a = &C->at[i];
    switch (p) {
    case GL_CURRENT_VERTEX_ATTRIB: for (int k = 0; k < 4; k++) r[k] = C->atual[i][k]; break;
    case GL_VERTEX_ATTRIB_ARRAY_ENABLED: r[0] = (float)a->ativo; break;
    case GL_VERTEX_ATTRIB_ARRAY_SIZE: r[0] = (float)a->tam; break;
    case GL_VERTEX_ATTRIB_ARRAY_STRIDE: r[0] = (float)a->passo; break;
    case GL_VERTEX_ATTRIB_ARRAY_TYPE: r[0] = (float)a->tipo; break;
    case GL_VERTEX_ATTRIB_ARRAY_NORMALIZED: r[0] = (float)a->norm; break;
    case GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING: r[0] = (float)a->buf; break;
    default: ERRO(GL_INVALID_ENUM);
    }
}
void glGetVertexAttribiv(GLuint i, GLenum p, GLint *r)
{
    GLfloat f[4] = { 0, 0, 0, 0 };
    GLenum e0 = C ? C->erro : 0;
    glGetVertexAttribfv(i, p, f);
    if (!C || C->erro != e0) return;
    int n = p == GL_CURRENT_VERTEX_ATTRIB ? 4 : 1;
    for (int k = 0; k < n; k++) r[k] = (GLint)f[k];
}
void glGetVertexAttribPointerv(GLuint i, GLenum p, void **r)
{
    if (!C) return;
    if (i >= S2_MAX_ATRIB) { ERRO(GL_INVALID_VALUE); return; }
    if (p != GL_VERTEX_ATTRIB_ARRAY_POINTER) { ERRO(GL_INVALID_ENUM); return; }
    *r = (void *)C->at[i].ptr;
}

/* ------------------------------------------------------------ sombreadores */
GLuint glCreateShader(GLenum tipo)
{
    if (!C) return 0;
    if (tipo != GL_VERTEX_SHADER && tipo != GL_FRAGMENT_SHADER) { ERRO(GL_INVALID_ENUM); return 0; }
    s2_shader_t *s = aloca_zerado(sizeof *s);
    if (!s) { ERRO(GL_OUT_OF_MEMORY); return 0; }
    s->tipo = tipo;
    GLuint n = obj_novo(s, 1);
    if (!n) { libera(s); ERRO(GL_OUT_OF_MEMORY); return 0; }
    s->nome = n;
    return n;
}
static int obj_existe(GLuint n) { return C && n && (int)n < C->nobj && C->obj[n]; }
static void shader_soltar(GLuint n)
{
    s2_shader_t *s = SHADER(n);
    if (!s) return;
    if (s->fonte) libera(s->fonte);
    libera(s);
    C->obj[n] = NULL;
}
void glDeleteShader(GLuint n)
{
    if (!C || !n) return;
    s2_shader_t *s = SHADER(n);
    if (!s) { ERRO(GL_INVALID_VALUE); return; }
    if (s->ligados) s->apagar = 1; else shader_soltar(n);
}
GLboolean glIsShader(GLuint n) { return SHADER(n) ? GL_TRUE : GL_FALSE; }
void glShaderSource(GLuint n, GLsizei cont, const GLchar *const *txt, const GLint *tam)
{
    if (!C) return;
    s2_shader_t *s = SHADER(n);
    if (!s) { ERRO(obj_existe(n) ? GL_INVALID_OPERATION : GL_INVALID_VALUE); return; }
    if (cont < 0) { ERRO(GL_INVALID_VALUE); return; }
    int total = 0;
    for (int i = 0; i < cont; i++) total += txt[i] ? (tam && tam[i] >= 0 ? tam[i] : (int)tam_texto(txt[i])) : 0;
    char *f = aloca((size_t)total + 1);
    if (!f) { ERRO(GL_OUT_OF_MEMORY); return; }
    int k = 0;
    for (int i = 0; i < cont; i++) {
        if (!txt[i]) continue;
        int t = tam && tam[i] >= 0 ? tam[i] : (int)tam_texto(txt[i]);
        memcpy(f + k, txt[i], (size_t)t); k += t;
    }
    f[k] = 0;
    if (s->fonte) libera(s->fonte);
    s->fonte = f; s->nfonte = k;
}
void glCompileShader(GLuint n)
{
    if (!C) return;
    s2_shader_t *s = SHADER(n);
    if (!s) { ERRO(obj_existe(n) ? GL_INVALID_OPERATION : GL_INVALID_VALUE); return; }
    ri_prog_t *p = sgl_glsl_compilar(s->fonte ? s->fonte : "", s->nfonte, s->tipo == GL_FRAGMENT_SHADER, s->log, sizeof s->log - 2);
    s->compilado = p != NULL;
    if (p) { s->log[0] = 0; ri_liberar(p); }
    else { int k = (int)tam_texto(s->log); s->log[k] = '\n'; s->log[k + 1] = 0; }
}
void glGetShaderiv(GLuint n, GLenum p, GLint *r)
{
    if (!C) return;
    s2_shader_t *s = SHADER(n);
    if (!s) { ERRO(obj_existe(n) ? GL_INVALID_OPERATION : GL_INVALID_VALUE); return; }
    switch (p) {
    case GL_SHADER_TYPE: *r = (GLint)s->tipo; break;
    case GL_DELETE_STATUS: *r = s->apagar; break;
    case GL_COMPILE_STATUS: *r = s->compilado; break;
    case GL_INFO_LOG_LENGTH: *r = s->log[0] ? (GLint)tam_texto(s->log) + 1 : 0; break;
    case GL_SHADER_SOURCE_LENGTH: *r = s->fonte ? s->nfonte + 1 : 0; break;
    default: ERRO(GL_INVALID_ENUM);
    }
}
static void copiar_log(const char *log, GLsizei max, GLsizei *tam, GLchar *d)
{
    int n = (int)tam_texto(log);
    if (max <= 0 || !d) { if (tam) *tam = 0; return; }
    if (n > max - 1) n = max - 1;
    memcpy(d, log, (size_t)n); d[n] = 0;
    if (tam) *tam = n;
}
void glGetShaderInfoLog(GLuint n, GLsizei max, GLsizei *tam, GLchar *log)
{
    if (!C) return;
    s2_shader_t *s = SHADER(n);
    if (!s) { ERRO(obj_existe(n) ? GL_INVALID_OPERATION : GL_INVALID_VALUE); return; }
    if (max < 0) { ERRO(GL_INVALID_VALUE); return; }
    copiar_log(s->log, max, tam, log);
}
void glGetShaderSource(GLuint n, GLsizei max, GLsizei *tam, GLchar *d)
{
    if (!C) return;
    s2_shader_t *s = SHADER(n);
    if (!s) { ERRO(obj_existe(n) ? GL_INVALID_OPERATION : GL_INVALID_VALUE); return; }
    if (max < 0) { ERRO(GL_INVALID_VALUE); return; }
    copiar_log(s->fonte ? s->fonte : "", max, tam, d);
}
void glGetShaderPrecisionFormat(GLenum st, GLenum pt, GLint *faixa, GLint *prec)
{
    if (!C) return;
    if ((st != GL_VERTEX_SHADER && st != GL_FRAGMENT_SHADER) || pt < GL_LOW_FLOAT || pt > GL_HIGH_INT) { ERRO(GL_INVALID_ENUM); return; }
    if (pt <= GL_HIGH_FLOAT) { faixa[0] = 127; faixa[1] = 127; *prec = 23; }
    else { faixa[0] = 24; faixa[1] = 24; *prec = 0; }
}
void glShaderBinary(GLsizei n, const GLuint *s, GLenum f, const void *b, GLsizei l) { (void)n; (void)s; (void)f; (void)b; (void)l; if (C) ERRO(GL_INVALID_ENUM); }
void glReleaseShaderCompiler(void) {}

/* ------------------------------------------------------------ programas */
GLuint glCreateProgram(void)
{
    if (!C) return 0;
    s2_prog_t *p = aloca_zerado(sizeof *p);
    if (!p) { ERRO(GL_OUT_OF_MEMORY); return 0; }
    GLuint n = obj_novo(p, 2);
    if (!n) { libera(p); ERRO(GL_OUT_OF_MEMORY); return 0; }
    p->nome = n;
    return n;
}
GLboolean glIsProgram(GLuint n) { return PROG(n) ? GL_TRUE : GL_FALSE; }
static void desligar_shader(GLuint s)
{
    s2_shader_t *sh = SHADER(s);
    if (!sh) return;
    if (sh->ligados > 0) sh->ligados--;
    if (!sh->ligados && sh->apagar) shader_soltar(s);
}
static void prog_soltar(GLuint n)
{
    s2_prog_t *p = PROG(n);
    if (!p) return;
    desligar_shader(p->vs); desligar_shader(p->fs);
    prog_soltar_exec(p);
    libera(p);
    C->obj[n] = NULL;
}
void glDeleteProgram(GLuint n)
{
    if (!C || !n) return;
    s2_prog_t *p = PROG(n);
    if (!p) { ERRO(GL_INVALID_VALUE); return; }
    if (C->prog_uso == n) p->apagar = 1; else prog_soltar(n);
}
void glAttachShader(GLuint pn, GLuint sn)
{
    if (!C) return;
    s2_prog_t *p = PROG(pn); s2_shader_t *s = SHADER(sn);
    if (!p || !s) { ERRO((!p && !obj_existe(pn)) || (!s && !obj_existe(sn)) ? GL_INVALID_VALUE : GL_INVALID_OPERATION); return; }
    GLuint *sitio = s->tipo == GL_VERTEX_SHADER ? &p->vs : &p->fs;
    if (*sitio) { ERRO(GL_INVALID_OPERATION); return; }
    *sitio = sn; s->ligados++;
}
void glDetachShader(GLuint pn, GLuint sn)
{
    if (!C) return;
    s2_prog_t *p = PROG(pn); s2_shader_t *s = SHADER(sn);
    if (!p || !s) { ERRO((!p && !obj_existe(pn)) || (!s && !obj_existe(sn)) ? GL_INVALID_VALUE : GL_INVALID_OPERATION); return; }
    if (p->vs == sn) p->vs = 0; else if (p->fs == sn) p->fs = 0; else { ERRO(GL_INVALID_OPERATION); return; }
    desligar_shader(sn);
}
void glGetAttachedShaders(GLuint pn, GLsizei max, GLsizei *n, GLuint *s)
{
    if (!C) return;
    s2_prog_t *p = PROG(pn);
    if (!p) { ERRO(obj_existe(pn) ? GL_INVALID_OPERATION : GL_INVALID_VALUE); return; }
    if (max < 0) { ERRO(GL_INVALID_VALUE); return; }
    int k = 0;
    if (p->vs && k < max) s[k++] = p->vs;
    if (p->fs && k < max) s[k++] = p->fs;
    if (n) *n = k;
}
void glBindAttribLocation(GLuint pn, GLuint i, const GLchar *nome)
{
    if (!C) return;
    s2_prog_t *p = PROG(pn);
    if (!p) { ERRO(obj_existe(pn) ? GL_INVALID_OPERATION : GL_INVALID_VALUE); return; }
    if (i >= S2_MAX_ATRIB) { ERRO(GL_INVALID_VALUE); return; }
    if (!compara_texto_n(nome, "gl_", 3)) { ERRO(GL_INVALID_OPERATION); return; }
    for (int k = 0; k < p->nbind; k++) if (!compara_texto(p->bind_nome[k], nome)) { p->bind_loc[k] = (int)i; return; }
    if (p->nbind >= S2_MAX_ATRIB) return;
    copia_texto(p->bind_nome[p->nbind], nome, 64); p->bind_loc[p->nbind++] = (int)i;
}
static void log_por(s2_prog_t *p, const char *m, const char *x)
{
    int k = (int)tam_texto(p->log);
    formatar_texto(p->log + k, sizeof p->log - (size_t)k, "%s%s%s\n", m, x ? " " : "", x ? x : "");
}
/* el enlace compila otra vez los dos sombreadores (el ejecutable no cambia si después se recompilan) */
static void ligar(s2_prog_t *p)
{
    prog_soltar_exec(p);
    p->ligado = 0; p->validado = 0; p->log[0] = 0;
    s2_shader_t *vs = SHADER(p->vs), *fs = SHADER(p->fs);
    if (!vs || !fs) { log_por(p, "falta un sombreador de vértices o de fragmentos", NULL); return; }
    if (!vs->compilado || !fs->compilado) { log_por(p, "un sombreador no está compilado", NULL); return; }
    char e[256];
    p->pv = sgl_glsl_compilar(vs->fonte, vs->nfonte, 0, e, sizeof e);
    if (!p->pv) { log_por(p, e, NULL); return; }
    p->pf = sgl_glsl_compilar(fs->fonte, fs->nfonte, 1, e, sizeof e);
    if (!p->pf) { log_por(p, e, NULL); prog_soltar_exec(p); return; }
    ri_prog_t *pv = p->pv, *pf = p->pf;
    /* varyings: por nombre y del mismo tipo */
    for (int i = 0; i < RI_MAX_ENT; i++) { p->mapa_vary[i] = -1; p->atr_loc[i] = -1; }
    p->nvary = 0; p->ent_coord = p->ent_face = p->ent_pcoord = -1;
    for (int i = 0; i < pf->nvary; i++) {
        ri_var_t *f = &pf->vary[i], *v = NULL;
        for (int k = 0; k < pv->nvary; k++) if (!compara_texto(pv->vary[k].nome, f->nome)) v = &pv->vary[k];
        if (!v) { log_por(p, "varying sin declarar en el sombreador de vértices:", f->nome); prog_soltar_exec(p); return; }
        if (v->tipo_gl != f->tipo_gl || v->arr != f->arr) { log_por(p, "varying con tipos distintos:", f->nome); prog_soltar_exec(p); return; }
        int k = (f->arr ? f->arr : 1) * f->nreg;
        for (int r = 0; r < k; r++) p->mapa_vary[f->reg + r] = v->reg + r;
    }
    for (int r = 0; r < pf->nent; r++) {
        switch (pf->ent[r].sem) {
        case RI_SEM_GENERICO: if (p->mapa_vary[r] >= 0) p->vary_ent[p->nvary++] = r; break;
        case RI_SEM_POSICAO: p->ent_coord = r; break;
        case RI_SEM_FACE: p->ent_face = r; break;
        case RI_SEM_PONTO_COORD: p->ent_pcoord = r; break;
        }
    }
    if (p->nvary > S2_MAX_VARY) { log_por(p, "demasiados varyings (8 vectores como mucho)", NULL); prog_soltar_exec(p); return; }
    p->sai_pos = p->sai_ptam = p->sai_cor = -1;
    for (int r = 0; r < pv->nsai; r++) { if (pv->sai[r].sem == RI_SEM_POSICAO) p->sai_pos = r; if (pv->sai[r].sem == RI_SEM_PONTO_TAM) p->sai_ptam = r; }
    for (int r = 0; r < pf->nsai; r++) if (pf->sai[r].sem == RI_SEM_COR) p->sai_cor = r;
    /* atributos: los de glBindAttribLocation y después los libres más bajos */
    int usado[S2_MAX_ATRIB]; memset(usado, 0, sizeof usado);
    for (int pass = 0; pass < 2; pass++)
        for (int i = 0; i < pv->nattr; i++) {
            ri_var_t *a = &pv->attr[i];
            int loc = -1;
            for (int k = 0; k < p->nbind; k++) if (!compara_texto(p->bind_nome[k], a->nome)) loc = p->bind_loc[k];
            if (pass == 0 && loc < 0) continue;
            if (pass == 1) {
                if (loc >= 0) continue;
                for (int l = 0; l + a->nreg <= S2_MAX_ATRIB && loc < 0; l++) { int livre = 1; for (int q = 0; q < a->nreg; q++) if (usado[l + q]) livre = 0; if (livre) loc = l; }
                if (loc < 0) { log_por(p, "demasiados atributos", NULL); prog_soltar_exec(p); return; }
            }
            if (loc + a->nreg > S2_MAX_ATRIB) { log_por(p, "atributo fuera de las posiciones:", a->nome); prog_soltar_exec(p); return; }
            for (int q = 0; q < a->nreg; q++) { usado[loc + q] = 1; p->atr_loc[a->reg + q] = loc + q; }
        }
    p->nativos_at = pv->nattr;
    /* uniformes: los de las dos etapas por nombre, un elemento por posición */
    int total = 0;
    for (int e2 = 0; e2 < 2; e2++) { ri_prog_t *q = e2 ? pf : pv; for (int i = 0; i < q->nunif; i++) total += q->unif[i].arr ? q->unif[i].arr : 1; }
    p->unif = aloca_zerado((size_t)(total ? total : 1) * sizeof(s2_unif_t));
    if (!p->unif) { log_por(p, "sin memoria", NULL); prog_soltar_exec(p); return; }
    for (int e2 = 0; e2 < 2; e2++) {
        ri_prog_t *q = e2 ? pf : pv;
        for (int i = 0; i < q->nunif; i++) {
            ri_var_t *u = &q->unif[i];
            int n = u->arr ? u->arr : 1;
            int ja = -1;
            for (int k = 0; k < p->nunif; k++) if (p->unif[k].ativo >= 0 && !compara_texto(p->unif[k].nome, u->nome)) ja = k;
            if (ja >= 0) {
                s2_unif_t *b = &p->unif[ja];
                if (b->tipo != u->tipo_gl || b->tam != n) { log_por(p, "uniforme con tipos distintos en las dos etapas:", u->nome); prog_soltar_exec(p); return; }
                for (int k = 0; k < n; k++) p->unif[ja + k].reg[e2] = u->reg + k * u->nreg;
                continue;
            }
            for (int k = 0; k < n; k++) {
                s2_unif_t *b = &p->unif[p->nunif++];
                copia_texto(b->nome, u->nome, sizeof b->nome);
                if (k) { int l = (int)tam_texto(b->nome); formatar_texto(b->nome + l, sizeof b->nome - (size_t)l, "[%d]", k); }
                b->tipo = u->tipo_gl; b->tam = k ? 1 : n; b->nreg = u->nreg; b->comps = u->comps;
                b->reg[e2] = u->reg + k * u->nreg; b->reg[!e2] = -1;
                b->ativo = k ? -1 : p->nativos++;
                if (k) b->tam = -(k);   /* elemento k de un array (tam negativo = índice) */
            }
        }
    }
    if (pv->nuni > S2_MAX_UNIF_VEC || pf->nuni > S2_MAX_UNIF_VEC) { log_por(p, "demasiados uniformes (256 vectores por etapa)", NULL); prog_soltar_exec(p); return; }
    p->uni[0] = aloca_zerado((size_t)(pv->nuni ? pv->nuni : 1) * sizeof(float[4]));
    p->uni[1] = aloca_zerado((size_t)(pf->nuni ? pf->nuni : 1) * sizeof(float[4]));
    p->xv = ri_exec_criar(pv); p->xf = ri_exec_criar(pf);
    if (!p->uni[0] || !p->uni[1] || !p->xv || !p->xf) { log_por(p, "sin memoria", NULL); prog_soltar_exec(p); return; }
    memset(p->amost, 0, sizeof p->amost);
    p->uni_sujo = 1;
    p->ligado = 1;
    sgl2_gpu_prog_ligado(p);
}
void glLinkProgram(GLuint n)
{
    if (!C) return;
    s2_prog_t *p = PROG(n);
    if (!p) { ERRO(obj_existe(n) ? GL_INVALID_OPERATION : GL_INVALID_VALUE); return; }
    ligar(p);
}
void glUseProgram(GLuint n)
{
    if (!C) return;
    s2_prog_t *p = n ? PROG(n) : NULL;
    if (n && !p) { ERRO(obj_existe(n) ? GL_INVALID_OPERATION : GL_INVALID_VALUE); return; }
    if (p && !p->ligado) { ERRO(GL_INVALID_OPERATION); return; }
    GLuint antes = C->prog_uso;
    C->prog_uso = n;
    s2_prog_t *a = PROG(antes);
    if (a && antes != n && a->apagar) prog_soltar(antes);
}
void glValidateProgram(GLuint n)
{
    if (!C) return;
    s2_prog_t *p = PROG(n);
    if (!p) { ERRO(obj_existe(n) ? GL_INVALID_OPERATION : GL_INVALID_VALUE); return; }
    p->validado = p->ligado;
}
void glGetProgramiv(GLuint n, GLenum q, GLint *r)
{
    if (!C) return;
    s2_prog_t *p = PROG(n);
    if (!p) { ERRO(obj_existe(n) ? GL_INVALID_OPERATION : GL_INVALID_VALUE); return; }
    switch (q) {
    case GL_DELETE_STATUS: *r = p->apagar; break;
    case GL_LINK_STATUS: *r = p->ligado; break;
    case GL_VALIDATE_STATUS: *r = p->validado; break;
    case GL_INFO_LOG_LENGTH: *r = p->log[0] ? (GLint)tam_texto(p->log) + 1 : 0; break;
    case GL_ATTACHED_SHADERS: *r = (p->vs != 0) + (p->fs != 0); break;
    case GL_ACTIVE_ATTRIBUTES: *r = p->ligado ? p->pv->nattr : 0; break;
    case GL_ACTIVE_ATTRIBUTE_MAX_LENGTH: { int m = 0; if (p->ligado) for (int i = 0; i < p->pv->nattr; i++) { int l = (int)tam_texto(p->pv->attr[i].nome) + 1; if (l > m) m = l; } *r = m; break; }
    case GL_ACTIVE_UNIFORMS: *r = p->ligado ? p->nativos : 0; break;
    case GL_ACTIVE_UNIFORM_MAX_LENGTH: { int m = 0; if (p->ligado) for (int i = 0; i < p->nunif; i++) if (p->unif[i].ativo >= 0) { int l = (int)tam_texto(p->unif[i].nome) + 1 + (p->unif[i].tam > 1 ? 3 : 0); if (l > m) m = l; } *r = m; break; }
    default: ERRO(GL_INVALID_ENUM);
    }
}
void glGetProgramInfoLog(GLuint n, GLsizei max, GLsizei *tam, GLchar *log)
{
    if (!C) return;
    s2_prog_t *p = PROG(n);
    if (!p) { ERRO(obj_existe(n) ? GL_INVALID_OPERATION : GL_INVALID_VALUE); return; }
    if (max < 0) { ERRO(GL_INVALID_VALUE); return; }
    copiar_log(p->log, max, tam, log);
}
static void nome_saida(const char *nome, int arr, GLsizei max, GLsizei *tam, GLchar *d)
{
    char b[110];
    formatar_texto(b, sizeof b, arr ? "%s[0]" : "%s", nome);
    copiar_log(b, max, tam, d);
}
void glGetActiveAttrib(GLuint n, GLuint i, GLsizei max, GLsizei *tam, GLint *sz, GLenum *tipo, GLchar *nome)
{
    if (!C) return;
    s2_prog_t *p = PROG(n);
    if (!p) { ERRO(obj_existe(n) ? GL_INVALID_OPERATION : GL_INVALID_VALUE); return; }
    if (!p->ligado || (int)i >= p->pv->nattr || max < 0) { ERRO(GL_INVALID_VALUE); return; }
    ri_var_t *a = &p->pv->attr[i];
    if (sz) *sz = 1;
    if (tipo) *tipo = a->tipo_gl;
    nome_saida(a->nome, 0, max, tam, nome);
}
void glGetActiveUniform(GLuint n, GLuint i, GLsizei max, GLsizei *tam, GLint *sz, GLenum *tipo, GLchar *nome)
{
    if (!C) return;
    s2_prog_t *p = PROG(n);
    if (!p) { ERRO(obj_existe(n) ? GL_INVALID_OPERATION : GL_INVALID_VALUE); return; }
    if (!p->ligado || (int)i >= p->nativos || max < 0) { ERRO(GL_INVALID_VALUE); return; }
    for (int k = 0; k < p->nunif; k++) if (p->unif[k].ativo == (int)i) {
        s2_unif_t *u = &p->unif[k];
        if (sz) *sz = u->tam;
        if (tipo) *tipo = u->tipo;
        nome_saida(u->nome, u->tam > 1, max, tam, nome);
        return;
    }
}
GLint glGetAttribLocation(GLuint n, const GLchar *nome)
{
    if (!C) return -1;
    s2_prog_t *p = PROG(n);
    if (!p) { ERRO(obj_existe(n) ? GL_INVALID_OPERATION : GL_INVALID_VALUE); return -1; }
    if (!p->ligado) { ERRO(GL_INVALID_OPERATION); return -1; }
    for (int i = 0; i < p->pv->nattr; i++) if (!compara_texto(p->pv->attr[i].nome, nome)) return p->atr_loc[p->pv->attr[i].reg];
    return -1;
}
GLint glGetUniformLocation(GLuint n, const GLchar *nome)
{
    if (!C) return -1;
    s2_prog_t *p = PROG(n);
    if (!p) { ERRO(obj_existe(n) ? GL_INVALID_OPERATION : GL_INVALID_VALUE); return -1; }
    if (!p->ligado) { ERRO(GL_INVALID_OPERATION); return -1; }
    for (int i = 0; i < p->nunif; i++) if (!compara_texto(p->unif[i].nome, nome)) return i;
    /* «nome[0]» es el elemento 0 */
    int l = (int)tam_texto(nome);
    if (l > 3 && !compara_texto(nome + l - 3, "[0]")) {
        char b[96]; if (l - 3 >= (int)sizeof b) return -1;
        memcpy(b, nome, (size_t)(l - 3)); b[l - 3] = 0;
        for (int i = 0; i < p->nunif; i++) if (p->unif[i].ativo >= 0 && p->unif[i].tam >= 1 && !compara_texto(p->unif[i].nome, b)) return i;
    }
    return -1;
}

/* glUniform*: n componentes por elemento, «inteiro» = de la familia i */
static s2_prog_t *prog_uso(void) { return PROG(C->prog_uso); }
static int tipo_base(GLenum t, int *comps, int *eh_int, int *eh_bool, int *eh_samp, int *mat)
{
    *eh_int = *eh_bool = *eh_samp = *mat = 0; *comps = 1;
    switch (t) {
    case GL_FLOAT: return 1;
    case GL_FLOAT_VEC2: *comps = 2; return 1; case GL_FLOAT_VEC3: *comps = 3; return 1; case GL_FLOAT_VEC4: *comps = 4; return 1;
    case GL_INT: *eh_int = 1; return 1; case GL_INT_VEC2: *eh_int = 1; *comps = 2; return 1; case GL_INT_VEC3: *eh_int = 1; *comps = 3; return 1; case GL_INT_VEC4: *eh_int = 1; *comps = 4; return 1;
    case GL_BOOL: *eh_bool = 1; return 1; case GL_BOOL_VEC2: *eh_bool = 1; *comps = 2; return 1; case GL_BOOL_VEC3: *eh_bool = 1; *comps = 3; return 1; case GL_BOOL_VEC4: *eh_bool = 1; *comps = 4; return 1;
    case GL_FLOAT_MAT2: *mat = 2; *comps = 4; return 1; case GL_FLOAT_MAT3: *mat = 3; *comps = 9; return 1; case GL_FLOAT_MAT4: *mat = 4; *comps = 16; return 1;
    case GL_SAMPLER_2D: case GL_SAMPLER_CUBE: *eh_samp = 1; *eh_int = 1; return 1;
    }
    return 0;
}
/* cuántos elementos quedan desde esta posición hasta el final de su array */
static int elementos_restantes(s2_prog_t *p, int loc)
{
    s2_unif_t *u = &p->unif[loc];
    int k = u->tam < 0 ? -u->tam : 0;
    int base = loc - k;
    int tam = p->unif[base].tam > 0 ? p->unif[base].tam : 1;
    return tam - k;
}
static void uniforme(GLint loc, GLsizei cont, const float *fv, const GLint *iv, int n, int fam_int, int mat)
{
    if (!C) return;
    s2_prog_t *p = prog_uso();
    if (!p) { ERRO(GL_INVALID_OPERATION); return; }
    if (loc == -1) return;
    if (loc < 0 || loc >= p->nunif) { ERRO(GL_INVALID_OPERATION); return; }
    if (cont < 0) { ERRO(GL_INVALID_VALUE); return; }
    s2_unif_t *u = &p->unif[loc];
    int comps, eh_int, eh_bool, eh_samp, umat;
    tipo_base(u->tipo, &comps, &eh_int, &eh_bool, &eh_samp, &umat);
    if (mat ? umat != mat : (umat || comps != n || (!eh_bool && eh_int != fam_int))) { ERRO(GL_INVALID_OPERATION); return; }
    int resta = elementos_restantes(p, loc);
    int arr = u->tam != 1 || resta > 1;
    if (cont > 1 && !arr) { ERRO(GL_INVALID_OPERATION); return; }
    if (cont > resta) cont = resta;
    if (eh_samp) {
        for (int e = 0; e < cont; e++) if (iv[e] < 0 || iv[e] >= S2_MAX_UNID) { ERRO(GL_INVALID_VALUE); return; }
        for (int e = 0; e < cont; e++)
            for (int st = 0; st < 2; st++) if (p->unif[loc + e].reg[st] >= 0 && p->unif[loc + e].reg[st] < RI_MAX_AMOST) p->amost[st][p->unif[loc + e].reg[st]] = iv[e];
        return;
    }
    for (int e = 0; e < cont; e++) {
        s2_unif_t *ue = &p->unif[loc + e];
        for (int st = 0; st < 2; st++) {
            if (ue->reg[st] < 0) continue;
            float (*d)[4] = p->uni[st] + ue->reg[st];
            if (mat) {
                for (int col = 0; col < mat; col++) for (int l = 0; l < mat; l++) d[col][l] = fv[e * mat * mat + col * mat + l];
            } else for (int k = 0; k < n; k++) {
                float v = fam_int ? (float)iv[e * n + k] : fv[e * n + k];
                d[0][k] = eh_bool ? (v != 0 ? 1.0f : 0.0f) : v;
            }
        }
    }
    p->uni_sujo = 1;
}
void glUniform1f(GLint l, GLfloat a) { float v[1] = { a }; uniforme(l, 1, v, NULL, 1, 0, 0); }
void glUniform2f(GLint l, GLfloat a, GLfloat b) { float v[2] = { a, b }; uniforme(l, 1, v, NULL, 2, 0, 0); }
void glUniform3f(GLint l, GLfloat a, GLfloat b, GLfloat c) { float v[3] = { a, b, c }; uniforme(l, 1, v, NULL, 3, 0, 0); }
void glUniform4f(GLint l, GLfloat a, GLfloat b, GLfloat c, GLfloat d) { float v[4] = { a, b, c, d }; uniforme(l, 1, v, NULL, 4, 0, 0); }
void glUniform1i(GLint l, GLint a) { GLint v[1] = { a }; uniforme(l, 1, NULL, v, 1, 1, 0); }
void glUniform2i(GLint l, GLint a, GLint b) { GLint v[2] = { a, b }; uniforme(l, 1, NULL, v, 2, 1, 0); }
void glUniform3i(GLint l, GLint a, GLint b, GLint c) { GLint v[3] = { a, b, c }; uniforme(l, 1, NULL, v, 3, 1, 0); }
void glUniform4i(GLint l, GLint a, GLint b, GLint c, GLint d) { GLint v[4] = { a, b, c, d }; uniforme(l, 1, NULL, v, 4, 1, 0); }
void glUniform1fv(GLint l, GLsizei n, const GLfloat *v) { uniforme(l, n, v, NULL, 1, 0, 0); }
void glUniform2fv(GLint l, GLsizei n, const GLfloat *v) { uniforme(l, n, v, NULL, 2, 0, 0); }
void glUniform3fv(GLint l, GLsizei n, const GLfloat *v) { uniforme(l, n, v, NULL, 3, 0, 0); }
void glUniform4fv(GLint l, GLsizei n, const GLfloat *v) { uniforme(l, n, v, NULL, 4, 0, 0); }
void glUniform1iv(GLint l, GLsizei n, const GLint *v) { uniforme(l, n, NULL, v, 1, 1, 0); }
void glUniform2iv(GLint l, GLsizei n, const GLint *v) { uniforme(l, n, NULL, v, 2, 1, 0); }
void glUniform3iv(GLint l, GLsizei n, const GLint *v) { uniforme(l, n, NULL, v, 3, 1, 0); }
void glUniform4iv(GLint l, GLsizei n, const GLint *v) { uniforme(l, n, NULL, v, 4, 1, 0); }
static void umat(GLint l, GLsizei n, GLboolean tr, const GLfloat *v, int m) { if (!C) return; if (tr) { ERRO(GL_INVALID_VALUE); return; } uniforme(l, n, v, NULL, m * m, 0, m); }
void glUniformMatrix2fv(GLint l, GLsizei n, GLboolean tr, const GLfloat *v) { umat(l, n, tr, v, 2); }
void glUniformMatrix3fv(GLint l, GLsizei n, GLboolean tr, const GLfloat *v) { umat(l, n, tr, v, 3); }
void glUniformMatrix4fv(GLint l, GLsizei n, GLboolean tr, const GLfloat *v) { umat(l, n, tr, v, 4); }
static int ler_uniforme(GLuint n, GLint loc, float *out)
{
    if (!C) return 0;
    s2_prog_t *p = PROG(n);
    if (!p) { ERRO(obj_existe(n) ? GL_INVALID_OPERATION : GL_INVALID_VALUE); return 0; }
    if (!p->ligado || loc < 0 || loc >= p->nunif) { ERRO(GL_INVALID_OPERATION); return 0; }
    s2_unif_t *u = &p->unif[loc];
    int comps, eh_int, eh_bool, eh_samp, mat;
    tipo_base(u->tipo, &comps, &eh_int, &eh_bool, &eh_samp, &mat);
    int st = u->reg[0] >= 0 ? 0 : 1;
    if (eh_samp) { out[0] = (float)p->amost[st][u->reg[st]]; return 1; }
    float (*d)[4] = p->uni[st] + u->reg[st];
    if (mat) { for (int c = 0; c < mat; c++) for (int l = 0; l < mat; l++) out[c * mat + l] = d[c][l]; return mat * mat; }
    for (int k = 0; k < comps; k++) out[k] = d[0][k];
    return comps;
}
void glGetUniformfv(GLuint n, GLint loc, GLfloat *r) { float v[16]; int k = ler_uniforme(n, loc, v); for (int i = 0; i < k; i++) r[i] = v[i]; }
void glGetUniformiv(GLuint n, GLint loc, GLint *r) { float v[16]; int k = ler_uniforme(n, loc, v); for (int i = 0; i < k; i++) r[i] = (GLint)v[i]; }
void sgl2_uniformes_subir(s2_prog_t *p)
{
    if (!p->uni_sujo) return;
    ri_exec_uniformes(p->xv, (const float (*)[4])p->uni[0]);
    ri_exec_uniformes(p->xf, (const float (*)[4])p->uni[1]);
    p->uni_sujo = 0;
}
