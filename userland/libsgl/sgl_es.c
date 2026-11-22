// SPDX-License-Identifier: GPL-2.0-or-later
/* sgl_es.c - SaguiGL: lo que le falta al motor de siempre para ser OpenGL ES
 * 1.1 completo, más sus extensiones (D425).
 *
 * No es una unidad de compilación aparte: sgl.c lo incluye al final (usa sus
 * funciones y su contexto, que son estáticos). Solo se compila con SGL_ES
 * (fuera del cc de dentro, que sigue con el motor de siempre).
 *
 * La idea: el motor de siempre (triangulo_fixo, triangulo_v6 y triangulo_v7)
 * no cambia ni de píxel ni de velocidad. Cada dibujo mira una vez si pide algo
 * que esas rutas no saben hacer (precisa_es): multitextura, TexEnv distinto de
 * MODULATE, esténcil, prueba alfa, tijera, máscaras, puntos, planos de recorte,
 * un FBO... y entonces va entero por la tubería completa de este fichero, que
 * sigue el orden de la especificación de ES 1.1:
 *
 *   vértice (matrices, paleta, luz, generación de coordenadas, matriz de
 *   textura) -> recorte (frustum y planos del programa) -> rasterizado con
 *   corrección de perspectiva -> unidades de textura (GL_COMBINE) -> suma del
 *   especular -> niebla -> prueba alfa -> tijera -> esténcil -> Z -> mezcla u
 *   operación lógica -> máscaras.
 *
 * Determinista y sin libm, como el resto: los mismos píxeles en el anfitrión
 * (pruebas de conformidad, tests/sgl/es11_conformidad.c) y en el SaguiOS. */

/* ------------------------------------------------------------ utilidades */

#define UNID_ATIVA (C->un[C->unid])
#define UNID_CLI   (C->un[C->unid_cli])

static int es_cmp_valida(GLenum f) { return f >= GL_NEVER && f <= GL_ALWAYS; }

static inline int es_comparar(int func, double a, double b)
{
    switch (func) {
    case GL_NEVER: return 0;
    case GL_LESS: return a < b;
    case GL_EQUAL: return a == b;
    case GL_LEQUAL: return a <= b;
    case GL_GREATER: return a > b;
    case GL_NOTEQUAL: return a != b;
    case GL_GEQUAL: return a >= b;
    default: return 1;
    }
}

static void mat_identidade(float *m) { for (int i = 0; i < 16; i++) m[i] = (i % 5 == 0) ? 1.0f : 0.0f; }

static int mat_e_identidade(const float *m)
{
    for (int i = 0; i < 16; i++) if (m[i] != ((i % 5 == 0) ? 1.0f : 0.0f)) return 0;
    return 1;
}

/* Inversa de una 4x4 (por columnas) con cofactores; 0 si es singular. */
static int mat_inversa(float *r, const float *m)
{
    float inv[16];
    inv[0] = m[5] * m[10] * m[15] - m[5] * m[11] * m[14] - m[9] * m[6] * m[15] + m[9] * m[7] * m[14] + m[13] * m[6] * m[11] - m[13] * m[7] * m[10];
    inv[4] = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] + m[8] * m[6] * m[15] - m[8] * m[7] * m[14] - m[12] * m[6] * m[11] + m[12] * m[7] * m[10];
    inv[8] = m[4] * m[9] * m[15] - m[4] * m[11] * m[13] - m[8] * m[5] * m[15] + m[8] * m[7] * m[13] + m[12] * m[5] * m[11] - m[12] * m[7] * m[9];
    inv[12] = -m[4] * m[9] * m[14] + m[4] * m[10] * m[13] + m[8] * m[5] * m[14] - m[8] * m[6] * m[13] - m[12] * m[5] * m[10] + m[12] * m[6] * m[9];
    inv[1] = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] + m[9] * m[2] * m[15] - m[9] * m[3] * m[14] - m[13] * m[2] * m[11] + m[13] * m[3] * m[10];
    inv[5] = m[0] * m[10] * m[15] - m[0] * m[11] * m[14] - m[8] * m[2] * m[15] + m[8] * m[3] * m[14] + m[12] * m[2] * m[11] - m[12] * m[3] * m[10];
    inv[9] = -m[0] * m[9] * m[15] + m[0] * m[11] * m[13] + m[8] * m[1] * m[15] - m[8] * m[3] * m[13] - m[12] * m[1] * m[11] + m[12] * m[3] * m[9];
    inv[13] = m[0] * m[9] * m[14] - m[0] * m[10] * m[13] - m[8] * m[1] * m[14] + m[8] * m[2] * m[13] + m[12] * m[1] * m[10] - m[12] * m[2] * m[9];
    inv[2] = m[1] * m[6] * m[15] - m[1] * m[7] * m[14] - m[5] * m[2] * m[15] + m[5] * m[3] * m[14] + m[13] * m[2] * m[7] - m[13] * m[3] * m[6];
    inv[6] = -m[0] * m[6] * m[15] + m[0] * m[7] * m[14] + m[4] * m[2] * m[15] - m[4] * m[3] * m[14] - m[12] * m[2] * m[7] + m[12] * m[3] * m[6];
    inv[10] = m[0] * m[5] * m[15] - m[0] * m[7] * m[13] - m[4] * m[1] * m[15] + m[4] * m[3] * m[13] + m[12] * m[1] * m[7] - m[12] * m[3] * m[5];
    inv[14] = -m[0] * m[5] * m[14] + m[0] * m[6] * m[13] + m[4] * m[1] * m[14] - m[4] * m[2] * m[13] - m[12] * m[1] * m[6] + m[12] * m[2] * m[5];
    inv[3] = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] + m[5] * m[2] * m[11] - m[5] * m[3] * m[10] - m[9] * m[2] * m[7] + m[9] * m[3] * m[6];
    inv[7] = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] - m[4] * m[2] * m[11] + m[4] * m[3] * m[10] + m[8] * m[2] * m[7] - m[8] * m[3] * m[6];
    inv[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] + m[4] * m[1] * m[11] - m[4] * m[3] * m[9] - m[8] * m[1] * m[7] + m[8] * m[3] * m[5];
    inv[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] - m[4] * m[1] * m[10] + m[4] * m[2] * m[9] + m[8] * m[1] * m[6] - m[8] * m[2] * m[5];
    float det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];
    if (det == 0.0f) return 0;
    det = 1.0f / det;
    for (int i = 0; i < 16; i++) r[i] = inv[i] * det;
    return 1;
}

/* ------------------------------------------------------------ estado inicial */

static void es_unidade_inicial(unid_t *u)
{
    u->on = u->cubo_on = 0;
    u->sel = u->sel_cubo = -1;
    u->env_modo = GL_MODULATE;
    u->cb_rgb = u->cb_alfa = GL_MODULATE;
    u->src_rgb[0] = u->src_alfa[0] = GL_TEXTURE;
    u->src_rgb[1] = u->src_alfa[1] = GL_PREVIOUS;
    u->src_rgb[2] = u->src_alfa[2] = GL_CONSTANT;
    u->opd_rgb[0] = u->opd_rgb[1] = GL_SRC_COLOR; u->opd_rgb[2] = GL_SRC_ALPHA;
    u->opd_alfa[0] = u->opd_alfa[1] = u->opd_alfa[2] = GL_SRC_ALPHA;
    u->esc_rgb = u->esc_alfa = 1.0f;
    for (int k = 0; k < 4; k++) u->env_cor[k] = 0.0f;
    u->coord_replace = 0;
    u->gen_on = 0; u->gen_modo = GL_REFLECTION_MAP_OES;
    mat_identidade(u->mat[0]); u->mat_n = 1;
    u->t_ativo = 0; u->t_tam = 4; u->t_tipo = GL_FLOAT; u->t_passo = 0; u->t_ptr = 0; u->t_vbo = 0;
    u->tc[0] = u->tc[1] = u->tc[2] = 0.0f; u->tc[3] = 1.0f;
}

static void es_estado_inicial(sgl_ctx_t *c)
{
    for (int i = 0; i < SGL_UNIDADES; i++) es_unidade_inicial(&c->un[i]);
    c->unid = c->unid_cli = 0;
    c->v_tipo = GL_FLOAT; c->n_tipo = GL_FLOAT;
    c->v_vbo = c->c_vbo = c->n_vbo = 0;
    c->vbo_array = c->vbo_elem = 0;
    c->alfa_on = 0; c->alfa_func = GL_ALWAYS; c->alfa_ref = 0.0f;
    c->est_on = 0; c->est_func = GL_ALWAYS; c->est_ref = 0; c->est_masc = 0xFFFFFFFFu; c->est_escr = 0xFFFFFFFFu;
    c->est_falha = c->est_zfalha = c->est_zpassa = GL_KEEP; c->est_limpo = 0;
    c->tij_on = 0; c->tij_x = c->tij_y = 0; c->tij_l = c->larg; c->tij_a = c->alt;
    c->masc_cor = 15; c->masc_z = 1;
    c->z_perto = 0.0f; c->z_longe = 1.0f;
    c->off_on = 0; c->off_fator = c->off_unid = 0.0f;
    c->log_on = 0; c->log_op = GL_COPY;
    c->blend_sa = GL_SRC_ALPHA; c->blend_da = GL_ONE_MINUS_SRC_ALPHA;
    c->blend_eq = c->blend_eqa = GL_FUNC_ADD_OES;
    c->planos = 0;
    for (int i = 0; i < 6; i++) for (int k = 0; k < 4; k++) c->plano_cl[i][k] = 0.0f;
    c->linha_larg = 1.0f;
    c->ponto_tam = 1.0f; c->ponto_min = 0.0f; c->ponto_max = 128.0f; c->ponto_fade = 1.0f;
    c->ponto_att[0] = 1.0f; c->ponto_att[1] = c->ponto_att[2] = 0.0f;
    c->sprite_on = 0; c->ps_ativo = 0; c->ps_tipo = GL_FLOAT; c->ps_passo = 0; c->ps_ptr = 0; c->ps_vbo = 0;
    c->cob_valor = 1.0f; c->cob_inv = 0;
    c->dither = 1; c->ms_on = 1; c->alfa_cob = c->alfa_um = c->suave_p = c->suave_l = 0;
    c->dois_lados = 0;
    for (int i = 0; i < SGL_MAX_LUZES; i++) {
        c->l_dir[i][0] = 0; c->l_dir[i][1] = 0; c->l_dir[i][2] = -1;
        c->l_exp[i] = 0; c->l_corte[i] = 180.0f;
    }
    c->pack_alin = c->unpack_alin = 4;
    for (int i = 0; i < 4; i++) c->dicas[i] = GL_DONT_CARE;
    c->paleta_on = 0; c->paleta_atual = 0;
    for (int i = 0; i < 32; i++) mat_identidade(c->paleta[i]);
    c->mi_ativo = c->pe_ativo = 0;
    c->mi_tam = c->pe_tam = 0; c->mi_tipo = GL_UNSIGNED_BYTE; c->pe_tipo = GL_FLOAT;
    c->mi_passo = c->pe_passo = 0; c->mi_ptr = c->pe_ptr = 0; c->mi_vbo = c->pe_vbo = 0;
    c->alvo_alfa = 0;
    c->fbo = c->rb = 0;
    c->gpu_sw = c->gpu_sw_prox = 0; c->gpu_coube = 1;
}

/* ------------------------------------------------------------ búferes (VBO, OES_mapbuffer) */

typedef struct {
    GLuint   id;
    int      usado;
    uint8_t *dados;
    int      tam, uso;
    int      mapeado;
    uint32_t ger;          /* D457: cambia con el contenido (la copia de la GPU caduca) */
    short    vaga3[2];     /* D457: hueco + 1 del búfer en la GPU como vértices [0] y como índices [1]; 0 = ninguno */
} vbo_t;

#ifdef SGL_TEM_GPU
static void es_vb_mudou(vbo_t *b, int desl, int tam);   /* D457: el contenido cambió (tam < 0: todo) */
static void es_vb_soltar(vbo_t *b);
#else
#define es_vb_mudou(b, desl, tam) ((void)0)
#define es_vb_soltar(b) ((void)0)
#endif

static vbo_t *vbo_por_id(GLuint id)
{
    vbo_t *b = (vbo_t *)C->bufs;
    for (int i = 0; i < C->n_bufs; i++) if (b[i].usado && b[i].id == id) return &b[i];
    return 0;
}

static vbo_t *vbo_novo(GLuint id)
{
    vbo_t *b = (vbo_t *)C->bufs;
    int i;
    for (i = 0; i < C->n_bufs; i++) if (!b[i].usado) break;
    if (i == C->n_bufs) {
        int n = C->n_bufs ? C->n_bufs * 2 : 16;
        vbo_t *nb = (vbo_t *)SGL_ALOCAR((size_t)n * sizeof(vbo_t));
        if (!nb) return 0;
        for (int k = 0; k < C->n_bufs; k++) nb[k] = b[k];
        if (b) SGL_LIBERAR(b);
        C->bufs = nb; C->n_bufs = n; b = nb;
    }
    b[i].id = id; b[i].usado = 1; b[i].dados = 0; b[i].tam = 0; b[i].uso = GL_STATIC_DRAW; b[i].mapeado = 0;
    b[i].ger = 1; b[i].vaga3[0] = b[i].vaga3[1] = 0;
    return &b[i];
}

/* Un id libre (los ids de búfer, como los de textura, los elige la biblioteca). */
static GLuint vbo_id_livre(void)
{
    GLuint id = 1;
    for (;;) { if (!vbo_por_id(id)) return id; id++; }
}

void glGenBuffers(GLsizei n, GLuint *ids)
{
    if (!C) return;
    if (n < 0) { erro(GL_INVALID_VALUE); return; }
    for (int i = 0; i < n; i++) {
        GLuint id = vbo_id_livre();
        if (!vbo_novo(id)) { erro(GL_OUT_OF_MEMORY); ids[i] = 0; continue; }
        ids[i] = id;
    }
}

static GLuint *vbo_alvo(GLenum alvo)
{
    if (alvo == GL_ARRAY_BUFFER) return &C->vbo_array;
    if (alvo == GL_ELEMENT_ARRAY_BUFFER) return &C->vbo_elem;
    return 0;
}

void glBindBuffer(GLenum alvo, GLuint id)
{
    if (!C) return;
    GLuint *a = vbo_alvo(alvo);
    if (!a) { erro(GL_INVALID_ENUM); return; }
    if (id && !vbo_por_id(id) && !vbo_novo(id)) { erro(GL_OUT_OF_MEMORY); return; }
    *a = id;
}

static vbo_t *vbo_ligado(GLenum alvo)
{
    GLuint *a = vbo_alvo(alvo);
    if (!a) { erro(GL_INVALID_ENUM); return 0; }
    if (!*a) { erro(GL_INVALID_OPERATION); return 0; }
    return vbo_por_id(*a);
}

void glBufferData(GLenum alvo, GLsizeiptr tam, const GLvoid *dados, GLenum uso)
{
    if (!C) return;
    if (tam < 0) { erro(GL_INVALID_VALUE); return; }
    if (uso != GL_STATIC_DRAW && uso != GL_DYNAMIC_DRAW) { erro(GL_INVALID_ENUM); return; }
    vbo_t *b = vbo_ligado(alvo);
    if (!b) return;
    GPU_DESPEJAR();
    uint8_t *d = 0;
    if (tam > 0) {
        d = (uint8_t *)SGL_ALOCAR((size_t)tam);
        if (!d) { erro(GL_OUT_OF_MEMORY); return; }
        if (dados) { const uint8_t *s = (const uint8_t *)dados; for (GLsizeiptr i = 0; i < tam; i++) d[i] = s[i]; }
    }
    if (b->dados) SGL_LIBERAR(b->dados);
    b->dados = d; b->tam = (int)tam; b->uso = (int)uso; b->mapeado = 0;
    es_vb_mudou(b, 0, -1);
}

void glBufferSubData(GLenum alvo, GLintptr desl, GLsizeiptr tam, const GLvoid *dados)
{
    if (!C) return;
    vbo_t *b = vbo_ligado(alvo);
    if (!b) return;
    if (desl < 0 || tam < 0 || desl + tam > b->tam) { erro(GL_INVALID_VALUE); return; }
    GPU_DESPEJAR();
    const uint8_t *s = (const uint8_t *)dados;
    for (GLsizeiptr i = 0; i < tam; i++) b->dados[desl + i] = s[i];
    es_vb_mudou(b, (int)desl, (int)tam);
}

static void vbo_soltar_ligacoes(GLuint id)
{
    if (C->vbo_array == id) C->vbo_array = 0;
    if (C->vbo_elem == id) C->vbo_elem = 0;
    if (C->v_vbo == id) C->v_vbo = 0;
    if (C->c_vbo == id) C->c_vbo = 0;
    if (C->n_vbo == id) C->n_vbo = 0;
    if (C->ps_vbo == id) C->ps_vbo = 0;
    if (C->mi_vbo == id) C->mi_vbo = 0;
    if (C->pe_vbo == id) C->pe_vbo = 0;
    for (int u = 0; u < SGL_UNIDADES; u++) if (C->un[u].t_vbo == id) C->un[u].t_vbo = 0;
}

void glDeleteBuffers(GLsizei n, const GLuint *ids)
{
    if (!C) return;
    if (n < 0) { erro(GL_INVALID_VALUE); return; }
    GPU_DESPEJAR();
    for (int i = 0; i < n; i++) {
        if (!ids[i]) continue;
        vbo_t *b = vbo_por_id(ids[i]);
        if (!b) continue;
        es_vb_soltar(b);
        if (b->dados) SGL_LIBERAR(b->dados);
        b->dados = 0; b->usado = 0;
        vbo_soltar_ligacoes(ids[i]);
    }
}

GLboolean glIsBuffer(GLuint id) { return C && id && vbo_por_id(id) ? GL_TRUE : GL_FALSE; }

void glGetBufferParameteriv(GLenum alvo, GLenum nome, GLint *v)
{
    if (!C) return;
    vbo_t *b = vbo_ligado(alvo);
    if (!b) return;
    if (nome == GL_BUFFER_SIZE) *v = b->tam;
    else if (nome == GL_BUFFER_USAGE) *v = b->uso;
    else if (nome == GL_BUFFER_ACCESS_OES) *v = GL_WRITE_ONLY_OES;
    else if (nome == GL_BUFFER_MAPPED_OES) *v = b->mapeado;
    else erro(GL_INVALID_ENUM);
}

void *glMapBufferOES(GLenum alvo, GLenum acesso)
{
    if (!C) return 0;
    if (acesso != GL_WRITE_ONLY_OES) { erro(GL_INVALID_ENUM); return 0; }
    vbo_t *b = vbo_ligado(alvo);
    if (!b) return 0;
    if (b->mapeado) { erro(GL_INVALID_OPERATION); return 0; }
    GPU_DESPEJAR();
    b->mapeado = 1;
    es_vb_mudou(b, 0, -1);
    return b->dados;
}

GLboolean glUnmapBufferOES(GLenum alvo)
{
    if (!C) return GL_FALSE;
    vbo_t *b = vbo_ligado(alvo);
    if (!b) return GL_FALSE;
    if (!b->mapeado) { erro(GL_INVALID_OPERATION); return GL_FALSE; }
    b->mapeado = 0;
    es_vb_mudou(b, 0, -1);
    return GL_TRUE;
}

void glGetBufferPointervOES(GLenum alvo, GLenum nome, GLvoid **v)
{
    if (!C) return;
    if (nome != GL_BUFFER_MAP_POINTER_OES) { erro(GL_INVALID_ENUM); return; }
    vbo_t *b = vbo_ligado(alvo);
    if (!b) return;
    *v = b->mapeado ? b->dados : 0;
}

/* El puntero de un array: si se dio con un búfer ligado, es un desplazamiento en él. */
static const void *vbo_resolver(const void *p, GLuint vbo)
{
    if (!vbo) return p;
    vbo_t *b = vbo_por_id(vbo);
    if (!b || !b->dados) return 0;
    return b->dados + (size_t)p;
}

/* ------------------------------------------------------------ habilitar */

/* glEnable/glDisable/glIsEnabled de lo nuevo. 1 = era de aquí. */
static int es_capacidade(GLenum cap, int v, int consultar, int *res)
{
    int *p = 0;
    switch (cap) {
    case GL_ALPHA_TEST: p = &C->alfa_on; break;
    case GL_STENCIL_TEST: p = &C->est_on; break;
    case GL_SCISSOR_TEST: p = &C->tij_on; break;
    case GL_COLOR_LOGIC_OP: p = &C->log_on; break;
    case GL_POLYGON_OFFSET_FILL: p = &C->off_on; break;
    case GL_POINT_SPRITE_OES: p = &C->sprite_on; break;
    case GL_MATRIX_PALETTE_OES: p = &C->paleta_on; break;
    case GL_DITHER: p = &C->dither; break;
    case GL_MULTISAMPLE: p = &C->ms_on; break;
    case GL_SAMPLE_ALPHA_TO_COVERAGE: p = &C->alfa_cob; break;
    case GL_SAMPLE_ALPHA_TO_ONE: p = &C->alfa_um; break;
    case GL_SAMPLE_COVERAGE: p = &C->cob_on; break;   /* se acepta y no hace nada (no hay multimuestreo) */
    case GL_POINT_SMOOTH: p = &C->suave_p; break;
    case GL_LINE_SMOOTH: p = &C->suave_l; break;
    case GL_TEXTURE_CUBE_MAP_OES: p = &UNID_ATIVA.cubo_on; break;
    case GL_TEXTURE_GEN_STR_OES: p = &UNID_ATIVA.gen_on; break;
    default:
        if (cap >= GL_CLIP_PLANE0 && cap <= GL_CLIP_PLANE5) {
            int b = 1 << (cap - GL_CLIP_PLANE0);
            if (consultar) { *res = (C->planos & b) != 0; return 1; }
            if (v) C->planos |= b; else C->planos &= ~b;
            return 1;
        }
        return 0;
    }
    if (consultar) { *res = *p != 0; return 1; }
    *p = v;
    return 1;
}

GLboolean glIsEnabled(GLenum cap)
{
    if (!C) return GL_FALSE;
    int r = 0;
    switch (cap) {
    case GL_DEPTH_TEST: return C->teste_z ? GL_TRUE : GL_FALSE;
    case GL_CULL_FACE: return C->cull ? GL_TRUE : GL_FALSE;
    case GL_TEXTURE_2D: return UNID_ATIVA.on ? GL_TRUE : GL_FALSE;
    case GL_BLEND: return C->blend ? GL_TRUE : GL_FALSE;
    case GL_FOG: return C->fog ? GL_TRUE : GL_FALSE;
    case GL_LIGHTING: return C->luz_on ? GL_TRUE : GL_FALSE;
    case GL_COLOR_MATERIAL: return C->cor_material ? GL_TRUE : GL_FALSE;
    case GL_NORMALIZE: case GL_RESCALE_NORMAL: return GL_FALSE;
    case GL_VERTEX_ARRAY: return C->v_ativo ? GL_TRUE : GL_FALSE;
    case GL_COLOR_ARRAY: return C->c_ativo ? GL_TRUE : GL_FALSE;
    case GL_NORMAL_ARRAY: return C->n_ativo ? GL_TRUE : GL_FALSE;
    case GL_TEXTURE_COORD_ARRAY: return UNID_CLI.t_ativo ? GL_TRUE : GL_FALSE;
    case GL_POINT_SIZE_ARRAY_OES: return C->ps_ativo ? GL_TRUE : GL_FALSE;
    case GL_MATRIX_INDEX_ARRAY_OES: return C->mi_ativo ? GL_TRUE : GL_FALSE;
    case GL_WEIGHT_ARRAY_OES: return C->pe_ativo ? GL_TRUE : GL_FALSE;
    default: break;
    }
    if (cap >= GL_LIGHT0 && cap <= GL_LIGHT7) return C->luz_ligada[cap - GL_LIGHT0] ? GL_TRUE : GL_FALSE;
    if (es_capacidade(cap, 0, 1, &r)) return r ? GL_TRUE : GL_FALSE;
    erro(GL_INVALID_ENUM);
    return GL_FALSE;
}

/* ------------------------------------------------------------ arrays */

static int es_tipo_valido(GLenum t, int byte, int ubyte, int shrt, int fixo, int flt)
{
    return (t == GL_BYTE && byte) || (t == GL_UNSIGNED_BYTE && ubyte) || (t == GL_SHORT && shrt) ||
           (t == GL_FIXED && fixo) || (t == GL_FLOAT && flt);
}

static int es_tam_tipo(GLenum t)
{
    return t == GL_BYTE || t == GL_UNSIGNED_BYTE ? 1 : t == GL_SHORT || t == GL_UNSIGNED_SHORT ? 2 : 4;
}

/* Lee la componente k de un array. norm: los enteros se normalizan (colores y normales). */
static inline float es_ler(const void *p, GLenum tipo, int k, int norm)
{
    switch (tipo) {
    case GL_FLOAT: return ((const float *)p)[k];
    case GL_FIXED: return (float)((const int32_t *)p)[k] * (1.0f / 65536.0f);
    case GL_UNSIGNED_BYTE: { float v = (float)((const uint8_t *)p)[k]; return norm ? v * (1.0f / 255.0f) : v; }
    case GL_BYTE: { float v = (float)((const int8_t *)p)[k]; return norm ? (2.0f * v + 1.0f) * (1.0f / 255.0f) : v; }
    case GL_SHORT: { float v = (float)((const int16_t *)p)[k]; return norm ? (2.0f * v + 1.0f) * (1.0f / 65535.0f) : v; }
    default: return 0.0f;
    }
}

void glPointSizePointerOES(GLenum tipo, GLsizei passo, const GLvoid *p)
{
    if (!C) return;
    if (tipo != GL_FIXED && tipo != GL_FLOAT) { erro(GL_INVALID_ENUM); return; }
    if (passo < 0) { erro(GL_INVALID_VALUE); return; }
    C->ps_tipo = (int)tipo; C->ps_passo = passo ? passo : 4; C->ps_ptr = p; C->ps_vbo = C->vbo_array;
}

void glMatrixIndexPointerOES(GLint tam, GLenum tipo, GLsizei passo, const GLvoid *p)
{
    if (!C) return;
    if (tipo != GL_UNSIGNED_BYTE) { erro(GL_INVALID_ENUM); return; }
    if (tam < 1 || tam > 4 || passo < 0) { erro(GL_INVALID_VALUE); return; }
    C->mi_tam = tam; C->mi_tipo = (int)tipo; C->mi_passo = passo ? passo : tam; C->mi_ptr = p; C->mi_vbo = C->vbo_array;
}

void glWeightPointerOES(GLint tam, GLenum tipo, GLsizei passo, const GLvoid *p)
{
    if (!C) return;
    if (tipo != GL_FIXED && tipo != GL_FLOAT) { erro(GL_INVALID_ENUM); return; }
    if (tam < 1 || tam > 4 || passo < 0) { erro(GL_INVALID_VALUE); return; }
    C->pe_tam = tam; C->pe_tipo = (int)tipo; C->pe_passo = passo ? passo : tam * 4; C->pe_ptr = p; C->pe_vbo = C->vbo_array;
}

void glCurrentPaletteMatrixOES(GLuint i)
{
    if (!C) return;
    if (i >= 32) { erro(GL_INVALID_VALUE); return; }
    C->paleta_atual = (int)i;
}

void glLoadPaletteFromModelViewMatrixOES(void)
{
    if (!C) return;
    const float *m = C->mv[C->mv_n - 1];
    for (int i = 0; i < 16; i++) C->paleta[C->paleta_atual][i] = m[i];
}

void glActiveTexture(GLenum t)
{
    if (!C) return;
    if (t < GL_TEXTURE0 || t >= GL_TEXTURE0 + SGL_UNIDADES) { erro(GL_INVALID_ENUM); return; }
    C->unid = (int)(t - GL_TEXTURE0);
}

void glClientActiveTexture(GLenum t)
{
    if (!C) return;
    if (t < GL_TEXTURE0 || t >= GL_TEXTURE0 + SGL_UNIDADES) { erro(GL_INVALID_ENUM); return; }
    C->unid_cli = (int)(t - GL_TEXTURE0);
}

void glMultiTexCoord4f(GLenum t, GLfloat s, GLfloat tt, GLfloat r, GLfloat q)
{
    if (!C) return;
    if (t < GL_TEXTURE0 || t >= GL_TEXTURE0 + SGL_UNIDADES) { erro(GL_INVALID_ENUM); return; }
    float *c = C->un[t - GL_TEXTURE0].tc;
    c[0] = s; c[1] = tt; c[2] = r; c[3] = q;
}

void glColor4ub(GLubyte r, GLubyte g, GLubyte b, GLubyte a)
{
    if (!C) return;
    C->cor[0] = (float)r; C->cor[1] = (float)g; C->cor[2] = (float)b; C->cor[3] = (float)a;
}

/* ------------------------------------------------------------ texturas */

/* La textura ligada a «alvo» en la unidad activa (cara de cubo: el cubo). */
static int *es_sel_alvo(GLenum alvo)
{
    if (alvo == GL_TEXTURE_2D) return &UNID_ATIVA.sel;
    if (alvo == GL_TEXTURE_CUBE_MAP_OES) return &UNID_ATIVA.sel_cubo;
    return 0;
}

static int es_face(GLenum alvo) { return alvo >= GL_TEXTURE_CUBE_MAP_POSITIVE_X_OES && alvo <= GL_TEXTURE_CUBE_MAP_NEGATIVE_Z_OES ? (int)(alvo - GL_TEXTURE_CUBE_MAP_POSITIVE_X_OES) : -1; }

static void tex_es_iniciar(textura_t *t)
{
    t->formato_base = GL_RGBA; t->formato_int = GL_RGBA;
    t->cubo = 0; t->faces = 0;
    t->recorte[0] = t->recorte[1] = t->recorte[2] = t->recorte[3] = 0;
    t->aniso = 1.0f; t->comprimida = 0;
}

/* Los parámetros del cubo valen para sus seis caras. */
static void tex_cubo_parametros(textura_t *t)
{
    if (!t->cubo || !t->faces) return;
    for (int f = 0; f < 6; f++) {
        textura_t *x = &t->faces[f];
        x->wrap_s = x->wrap_t = GL_CLAMP_TO_EDGE;   /* D425: las caras siempre al borde (sin costuras de otra cara) */
        x->filtro_min = t->filtro_min; x->filtro_mag = t->filtro_mag;
        x->gerar_mip = t->gerar_mip; x->aniso = t->aniso;
    }
}

static int tex_cubo_preparar(textura_t *t)
{
    if (t->faces) return 1;
    t->faces = (textura_t *)SGL_ALOCAR(6 * sizeof(textura_t));
    if (!t->faces) return 0;
    for (int f = 0; f < 6; f++) {
        textura_t *x = &t->faces[f];
        x->usada = 1; x->id = t->id; x->larg = x->alt = 0; x->px = 0;
        x->n_niveis = 1; x->mip_pronto = 0;
        for (int k = 0; k < MAX_NIVEIS; k++) { x->nivel_px[k] = 0; x->nivel_l[k] = x->nivel_a[k] = 0; }
        tex_es_iniciar(x);
        x->geracao = ++C->ger_cont;
    }
    t->cubo = 1;
    tex_cubo_parametros(t);
    return 1;
}

static void tex_soltar_tudo(textura_t *t)
{
    soltar_niveis(t);
    if (t->px) SGL_LIBERAR(t->px);
    t->px = 0; t->larg = t->alt = 0; t->nivel_px[0] = 0;
    if (t->faces) {
        for (int f = 0; f < 6; f++) { soltar_niveis(&t->faces[f]); if (t->faces[f].px) SGL_LIBERAR(t->faces[f].px); }
        SGL_LIBERAR(t->faces);
        t->faces = 0;
    }
}

void glBindTexture(GLenum alvo, GLuint id)
{
    if (!C) return;
    int *sel = es_sel_alvo(alvo);
    if (!sel) { erro(GL_INVALID_ENUM); return; }
    GPU_DESPEJAR();
    if (id == 0) { *sel = -1; return; }
    textura_t *t = textura_por_id(id);
    if (!t) {
        int k = textura_livre();
        if (k < 0) { erro(GL_OUT_OF_MEMORY); return; }
        t = &C->tex[k];
        textura_nova(t, id);
        tex_es_iniciar(t);
        if (alvo == GL_TEXTURE_CUBE_MAP_OES && !tex_cubo_preparar(t)) { erro(GL_OUT_OF_MEMORY); return; }
    } else if ((alvo == GL_TEXTURE_CUBE_MAP_OES) != (t->cubo != 0)) {
        /* una textura nueva (sin imagen ni cubo) puede pasar a cubo la primera vez */
        if (alvo == GL_TEXTURE_CUBE_MAP_OES && !t->px && !t->cubo) { if (!tex_cubo_preparar(t)) { erro(GL_OUT_OF_MEMORY); return; } }
        else { erro(GL_INVALID_OPERATION); return; }
    }
    *sel = (int)(t - C->tex);
}

GLboolean glIsTexture(GLuint id) { return C && id && textura_por_id(id) ? GL_TRUE : GL_FALSE; }

/* La textura (o la cara) de destino de glTex*Image2D. */
static textura_t *es_tex_destino(GLenum alvo)
{
    int f = es_face(alvo);
    if (alvo != GL_TEXTURE_2D && f < 0) { erro(GL_INVALID_ENUM); return 0; }
    int s = f < 0 ? UNID_ATIVA.sel : UNID_ATIVA.sel_cubo;
    if (s < 0) {
        /* la textura 0 (la de partida): se crea con el id 0 como en GL */
        int k = textura_livre();
        if (k < 0) { erro(GL_OUT_OF_MEMORY); return 0; }
        textura_nova(&C->tex[k], 0x7FFFFFFFu - (GLuint)(f < 0 ? 0 : 1) - (GLuint)C->unid * 2);
        tex_es_iniciar(&C->tex[k]);
        if (f >= 0 && !tex_cubo_preparar(&C->tex[k])) { erro(GL_OUT_OF_MEMORY); return 0; }
        if (f < 0) UNID_ATIVA.sel = k; else UNID_ATIVA.sel_cubo = k;
        s = k;
    }
    textura_t *t = &C->tex[s];
    if (f >= 0) {
        if (!t->cubo && !tex_cubo_preparar(t)) { erro(GL_OUT_OF_MEMORY); return 0; }
        return &t->faces[f];
    }
    return t;
}


static int es_formato_base(GLint f)
{
    switch (f) {
    case GL_RGB: case GL_RGB8_OES: case GL_RGB565_OES: return GL_RGB;
    case GL_RGBA: case GL_RGBA8_OES: case GL_RGBA4_OES: case GL_RGB5_A1_OES: return GL_RGBA;
    case GL_LUMINANCE: return GL_LUMINANCE;
    case GL_LUMINANCE_ALPHA: return GL_LUMINANCE_ALPHA;
    case GL_ALPHA: return GL_ALPHA;
    default: return 0;
    }
}

/* Deja «px» (RGBA de 8 bits, propiedad de quien llama) como nivel «nivel» de t. */
static void tex_por_nivel(textura_t *t, int nivel, unsigned char *px, int larg, int alt, int base)
{
    t->geracao = ++C->ger_cont;
    if (nivel == 0) {
        if (t->px) SGL_LIBERAR(t->px);
        soltar_niveis(t);
        t->px = px; t->larg = larg; t->alt = alt;
        t->nivel_px[0] = px; t->nivel_l[0] = larg; t->nivel_a[0] = alt;
        t->formato_base = base;
        if (t->gerar_mip) tex_preparar_mip(t);
    } else {
        if (t->nivel_px[nivel]) SGL_LIBERAR(t->nivel_px[nivel]);
        t->nivel_px[nivel] = px; t->nivel_l[nivel] = larg; t->nivel_a[nivel] = alt;
        t->mip_pronto = 0; t->n_niveis = 1;
    }
}

/* Bytes de una fila de la imagen de entrada, con GL_UNPACK_ALIGNMENT. */
static size_t es_fila(int larg, int comp, int alin) { size_t f = (size_t)larg * (size_t)comp; return (f + (size_t)alin - 1) / (size_t)alin * (size_t)alin; }

static int es_nivel_ok(const textura_t *t, int nivel, int larg, int alt)
{
    if (nivel == 0) return 1;
    if (!t->px) return 0;
    int l = t->larg >> nivel, a = t->alt >> nivel;
    return larg == (l > 0 ? l : 1) && alt == (a > 0 ? a : 1);
}

void glTexImage2D(GLenum alvo, GLint nivel, GLint formato_interno, GLsizei larg,
                   GLsizei alt, GLint bord, GLenum formato, GLenum tipo, const void *dados)
{
    if (!C) return;
    if (alvo != GL_TEXTURE_2D && es_face(alvo) < 0) { erro(GL_INVALID_ENUM); return; }
    if (nivel < 0 || nivel >= MAX_NIVEIS || bord != 0) { erro(GL_INVALID_VALUE); return; }
    if (larg < 0 || alt < 0 || larg > SGL_MAX_LADO || alt > SGL_MAX_LADO) { erro(GL_INVALID_VALUE); return; }
    if (es_face(alvo) >= 0 && larg != alt) { erro(GL_INVALID_VALUE); return; }
    int comp = bytes_texel(formato, tipo);
    if (comp < 0) { erro(GL_INVALID_ENUM); return; }
    if (comp == 0) { erro(tipo == GL_UNSIGNED_BYTE ? GL_INVALID_ENUM : GL_INVALID_OPERATION); return; }
    /* ES 1.1 pide formato interno = formato; SaguiGL siempre aceptó cualquiera y
     * guarda lo que dice «formato» (los programas de antes no cambian). */
    (void)formato_interno;
    textura_t *t = es_tex_destino(alvo);
    if (!t) return;
    GPU_DESPEJAR();
    if (larg == 0 || alt == 0) { t->geracao = ++C->ger_cont; return; }
    if (!es_nivel_ok(t, nivel, larg, alt)) { erro(GL_INVALID_VALUE); return; }
    unsigned char *px = (unsigned char *)SGL_ALOCAR((size_t)larg * (size_t)alt * 4);
    if (!px) { erro(GL_OUT_OF_MEMORY); return; }
    const unsigned char *q = (const unsigned char *)dados;
    size_t fila = es_fila(larg, comp, C->unpack_alin);
    if (q)
        for (int y = 0; y < alt; y++)
            for (int x = 0; x < larg; x++)
                desempacotar(q + (size_t)y * fila + (size_t)x * comp, formato, tipo, px + ((size_t)y * larg + x) * 4);
    tex_por_nivel(t, nivel, px, larg, alt, (int)formato);
    t->formato_int = (int)formato; t->comprimida = 0;
    refaz_extras();
}

static textura_t *es_tex_sub(GLenum alvo, int nivel, int x, int y, int larg, int alt)
{
    if (alvo != GL_TEXTURE_2D && es_face(alvo) < 0) { erro(GL_INVALID_ENUM); return 0; }
    if (nivel < 0 || nivel >= MAX_NIVEIS) { erro(GL_INVALID_VALUE); return 0; }
    textura_t *t = es_tex_destino(alvo);
    if (!t) return 0;
    if (!t->nivel_px[nivel] && !(nivel == 0 && t->px)) { erro(GL_INVALID_OPERATION); return 0; }
    int l = nivel ? t->nivel_l[nivel] : t->larg, a = nivel ? t->nivel_a[nivel] : t->alt;
    if (x < 0 || y < 0 || larg < 0 || alt < 0 || x + larg > l || y + alt > a) { erro(GL_INVALID_VALUE); return 0; }
    return t;
}

/* Tras cambiar un nivel: la copia de la GPU caduca y, con GL_GENERATE_MIPMAP, se regeneran los demás. */
static void tex_mudou(textura_t *t, int nivel)
{
    t->geracao = ++C->ger_cont;
    if (nivel == 0 && t->gerar_mip) { soltar_niveis(t); tex_preparar_mip(t); }
    else if (nivel > 0) { t->mip_pronto = 0; t->n_niveis = 1; }
    else if (t->mip_pronto) {
        /* el nivel 0 cambió: los generados por nosotros siguen siendo del programa (GL no los toca) */
    }
}

void glTexSubImage2D(GLenum alvo, GLint nivel, GLint x0, GLint y0, GLsizei larg, GLsizei alt,
                     GLenum formato, GLenum tipo, const GLvoid *dados)
{
    if (!C) return;
    int comp = bytes_texel(formato, tipo);
    if (comp < 0) { erro(GL_INVALID_ENUM); return; }
    if (comp == 0) { erro(tipo == GL_UNSIGNED_BYTE ? GL_INVALID_ENUM : GL_INVALID_OPERATION); return; }
    textura_t *t = es_tex_sub(alvo, nivel, x0, y0, larg, alt);
    if (!t || !larg || !alt || !dados) return;
    GPU_DESPEJAR();
    unsigned char *d = nivel ? t->nivel_px[nivel] : t->px;
    int l = nivel ? t->nivel_l[nivel] : t->larg;
    const unsigned char *q = (const unsigned char *)dados;
    size_t fila = es_fila(larg, comp, C->unpack_alin);
    for (int y = 0; y < alt; y++)
        for (int x = 0; x < larg; x++)
            desempacotar(q + (size_t)y * fila + (size_t)x * comp, formato, tipo, d + ((size_t)(y0 + y) * l + (x0 + x)) * 4);
    tex_mudou(t, nivel);
}

/* ---- texturas comprimidas: paletas (OES_compressed_paletted_texture) y S3TC ---- */

static void cor565(unsigned v, unsigned char *o)
{
    unsigned r = (v >> 11) & 31, g = (v >> 5) & 63, b = v & 31;
    o[0] = (unsigned char)((r << 3) | (r >> 2)); o[1] = (unsigned char)((g << 2) | (g >> 4)); o[2] = (unsigned char)((b << 3) | (b >> 2));
}

/* Un bloque S3TC de 4x4 a RGBA (o[64]). fmt: 0 DXT1 RGB, 1 DXT1 RGBA, 2 DXT3, 3 DXT5. */
static void dxt_bloco(const unsigned char *b, int fmt, unsigned char *o)
{
    const unsigned char *cb = fmt >= 2 ? b + 8 : b;
    unsigned c0 = cb[0] | (cb[1] << 8), c1 = cb[2] | (cb[3] << 8);
    unsigned char p[4][4];
    cor565(c0, p[0]); cor565(c1, p[1]);
    p[0][3] = p[1][3] = 255;
    if (c0 > c1 || fmt >= 2) {
        for (int k = 0; k < 3; k++) {
            p[2][k] = (unsigned char)((2 * p[0][k] + p[1][k]) / 3);
            p[3][k] = (unsigned char)((p[0][k] + 2 * p[1][k]) / 3);
        }
        p[2][3] = p[3][3] = 255;
    } else {
        for (int k = 0; k < 3; k++) { p[2][k] = (unsigned char)((p[0][k] + p[1][k]) / 2); p[3][k] = 0; }
        p[2][3] = 255; p[3][3] = fmt == 1 ? 0 : 255;
    }
    unsigned idx = cb[4] | (cb[5] << 8) | ((unsigned)cb[6] << 16) | ((unsigned)cb[7] << 24);
    for (int i = 0; i < 16; i++) {
        const unsigned char *c = p[(idx >> (2 * i)) & 3];
        o[i * 4] = c[0]; o[i * 4 + 1] = c[1]; o[i * 4 + 2] = c[2]; o[i * 4 + 3] = c[3];
    }
    if (fmt == 2) {
        for (int i = 0; i < 16; i++) { unsigned a = (b[i / 2] >> (4 * (i & 1))) & 15; o[i * 4 + 3] = (unsigned char)(a * 17); }
    } else if (fmt == 3) {
        unsigned a0 = b[0], a1 = b[1], al[8];
        al[0] = a0; al[1] = a1;
        if (a0 > a1) for (int k = 1; k < 7; k++) al[k + 1] = ((7 - k) * a0 + k * a1) / 7;
        else { for (int k = 1; k < 5; k++) al[k + 1] = ((5 - k) * a0 + k * a1) / 5; al[6] = 0; al[7] = 255; }
        /* 48 bits de índices de 3 bits, en dos mitades de 24 */
        unsigned lo = b[2] | (b[3] << 8) | ((unsigned)b[4] << 16), hi = b[5] | (b[6] << 8) | ((unsigned)b[7] << 16);
        for (int i = 0; i < 16; i++) {
            unsigned ix = i < 8 ? (lo >> (3 * i)) & 7 : (hi >> (3 * (i - 8))) & 7;
            o[i * 4 + 3] = (unsigned char)al[ix];
        }
    }
}

static int es_dxt_fmt(GLenum f)
{
    switch (f) {
    case GL_COMPRESSED_RGB_S3TC_DXT1_EXT: return 0;
    case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT: return 1;
    case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT: return 2;
    case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT: return 3;
    default: return -1;
    }
}

/* Descomprime una región S3TC (bloques de larg x alt) dentro de d (lado l). */
static void dxt_regiao(const unsigned char *src, int fmt, unsigned char *d, int l, int a, int x0, int y0, int larg, int alt)
{
    int bl = (larg + 3) / 4, ba = (alt + 3) / 4, tb = fmt >= 2 ? 16 : 8;
    unsigned char o[64];
    for (int by = 0; by < ba; by++)
        for (int bx = 0; bx < bl; bx++) {
            dxt_bloco(src + (size_t)(by * bl + bx) * tb, fmt, o);
            for (int y = 0; y < 4; y++)
                for (int x = 0; x < 4; x++) {
                    int px = x0 + bx * 4 + x, py = y0 + by * 4 + y;
                    if (bx * 4 + x >= larg || by * 4 + y >= alt || px >= l || py >= a) continue;
                    unsigned char *q = d + ((size_t)py * l + px) * 4;
                    for (int k = 0; k < 4; k++) q[k] = o[(y * 4 + x) * 4 + k];
                }
        }
}

/* Paletas: bits por índice, bytes por entrada y formato de la entrada. */
static int paleta_info(GLenum f, int *bits, int *tam, GLenum *fmt, GLenum *tipo)
{
    if (f < GL_PALETTE4_RGB8_OES || f > GL_PALETTE8_RGB5_A1_OES) return 0;
    int k = (int)(f - GL_PALETTE4_RGB8_OES);
    *bits = k < 5 ? 4 : 8;
    switch (k % 5) {
    case 0: *tam = 3; *fmt = GL_RGB; *tipo = GL_UNSIGNED_BYTE; break;
    case 1: *tam = 4; *fmt = GL_RGBA; *tipo = GL_UNSIGNED_BYTE; break;
    case 2: *tam = 2; *fmt = GL_RGB; *tipo = GL_UNSIGNED_SHORT_5_6_5; break;
    case 3: *tam = 2; *fmt = GL_RGBA; *tipo = GL_UNSIGNED_SHORT_4_4_4_4; break;
    default: *tam = 2; *fmt = GL_RGBA; *tipo = GL_UNSIGNED_SHORT_5_5_5_1; break;
    }
    return 1;
}

void glCompressedTexImage2D(GLenum alvo, GLint nivel, GLenum fi, GLsizei larg, GLsizei alt,
                            GLint bord, GLsizei tam, const GLvoid *dados)
{
    if (!C) return;
    if (alvo != GL_TEXTURE_2D && es_face(alvo) < 0) { erro(GL_INVALID_ENUM); return; }
    if (bord != 0 || larg < 0 || alt < 0 || larg > SGL_MAX_LADO || alt > SGL_MAX_LADO || tam < 0) { erro(GL_INVALID_VALUE); return; }
    int bits, et; GLenum pf, pt;
    int dxt = es_dxt_fmt(fi);
    const unsigned char *q = (const unsigned char *)dados;
    if (paleta_info(fi, &bits, &et, &pf, &pt)) {
        /* nivel <= 0: -nivel = niveles de mipmap que vienen detrás del primero */
        if (nivel > 0 || -nivel >= MAX_NIVEIS) { erro(GL_INVALID_VALUE); return; }
        int niveis = 1 - nivel, npal = 1 << bits;
        size_t precisa = (size_t)npal * et;
        int l = larg, a = alt;
        for (int k = 0; k < niveis; k++) { precisa += ((size_t)l * a * bits + 7) / 8; l = l > 1 ? l / 2 : 1; a = a > 1 ? a / 2 : 1; }
        if ((size_t)tam < precisa || !q) { erro(GL_INVALID_VALUE); return; }
        textura_t *t = es_tex_destino(alvo);
        if (!t) return;
        GPU_DESPEJAR();
        const unsigned char *idx = q + (size_t)npal * et;
        l = larg; a = alt;
        for (int k = 0; k < niveis; k++) {
            unsigned char *px = (unsigned char *)SGL_ALOCAR((size_t)l * a * 4 + 4);
            if (!px) { erro(GL_OUT_OF_MEMORY); return; }
            for (int i = 0; i < l * a; i++) {
                int ix = bits == 8 ? idx[i] : (i & 1) ? idx[i / 2] & 15 : idx[i / 2] >> 4;
                desempacotar(q + (size_t)ix * et, pf, pt, px + (size_t)i * 4);
            }
            idx += ((size_t)l * a * bits + 7) / 8;
            tex_por_nivel(t, k, px, l, a, (int)pf);
            l = l > 1 ? l / 2 : 1; a = a > 1 ? a / 2 : 1;
        }
        t->formato_int = (int)fi; t->comprimida = (int)fi;
        refaz_extras();
        return;
    }
    if (dxt < 0) { erro(GL_INVALID_ENUM); return; }
    if (nivel < 0 || nivel >= MAX_NIVEIS) { erro(GL_INVALID_VALUE); return; }
    size_t precisa = (size_t)((larg + 3) / 4) * ((alt + 3) / 4) * (dxt >= 2 ? 16 : 8);
    if ((size_t)tam != precisa || (!q && precisa)) { erro(GL_INVALID_VALUE); return; }
    textura_t *t = es_tex_destino(alvo);
    if (!t) return;
    GPU_DESPEJAR();
    if (!larg || !alt) return;
    if (!es_nivel_ok(t, nivel, larg, alt)) { erro(GL_INVALID_VALUE); return; }
    unsigned char *px = (unsigned char *)SGL_ALOCAR((size_t)larg * alt * 4);
    if (!px) { erro(GL_OUT_OF_MEMORY); return; }
    dxt_regiao(q, dxt, px, larg, alt, 0, 0, larg, alt);
    tex_por_nivel(t, nivel, px, larg, alt, dxt == 0 ? GL_RGB : GL_RGBA);
    t->formato_int = (int)fi; t->comprimida = (int)fi;
    refaz_extras();
}

void glCompressedTexSubImage2D(GLenum alvo, GLint nivel, GLint x0, GLint y0, GLsizei larg, GLsizei alt,
                               GLenum formato, GLsizei tam, const GLvoid *dados)
{
    if (!C) return;
    int dxt = es_dxt_fmt(formato), bits, et; GLenum pf, pt;
    if (paleta_info(formato, &bits, &et, &pf, &pt)) { erro(GL_INVALID_OPERATION); return; }   /* como dice la extensión */
    if (dxt < 0) { erro(GL_INVALID_ENUM); return; }
    textura_t *t = es_tex_sub(alvo, nivel, x0, y0, larg, alt);
    if (!t) return;
    if (t->comprimida != (int)formato || (x0 & 3) || (y0 & 3)) { erro(GL_INVALID_OPERATION); return; }
    size_t precisa = (size_t)((larg + 3) / 4) * ((alt + 3) / 4) * (dxt >= 2 ? 16 : 8);
    if ((size_t)tam != precisa || !dados) { erro(GL_INVALID_VALUE); return; }
    GPU_DESPEJAR();
    unsigned char *d = nivel ? t->nivel_px[nivel] : t->px;
    int l = nivel ? t->nivel_l[nivel] : t->larg, a = nivel ? t->nivel_a[nivel] : t->alt;
    dxt_regiao((const unsigned char *)dados, dxt, d, l, a, x0, y0, larg, alt);
    tex_mudou(t, nivel);
}

/* ---- copiar del destino a una textura ---- */

static void es_fbo_resolver(void);

/* El píxel (x, y) del destino (y de GL: hacia arriba) a RGBA. */
static void es_ler_px(int x, int y, unsigned char *o)
{
    uint32_t v = C->px[(size_t)(C->alt - 1 - y) * C->larg + x];
    o[0] = (unsigned char)(v >> 16); o[1] = (unsigned char)(v >> 8); o[2] = (unsigned char)v;
    o[3] = C->alvo_alfa ? (unsigned char)(v >> 24) : 255;
}

static void es_converter_base(unsigned char *o, int base)
{
    if (base == GL_LUMINANCE) { o[1] = o[2] = o[0]; o[3] = 255; }
    else if (base == GL_LUMINANCE_ALPHA) { o[1] = o[2] = o[0]; }
    else if (base == GL_ALPHA) { o[0] = o[1] = o[2] = 255; }
    else if (base == GL_RGB) o[3] = 255;
}

void glCopyTexImage2D(GLenum alvo, GLint nivel, GLenum fi, GLint x, GLint y, GLsizei larg, GLsizei alt, GLint bord)
{
    if (!C) return;
    if (alvo != GL_TEXTURE_2D && es_face(alvo) < 0) { erro(GL_INVALID_ENUM); return; }
    int base = es_formato_base((GLint)fi);
    if (!base) { erro(GL_INVALID_ENUM); return; }
    if (nivel < 0 || nivel >= MAX_NIVEIS || bord != 0 || larg < 0 || alt < 0 || larg > SGL_MAX_LADO || alt > SGL_MAX_LADO)
        { erro(GL_INVALID_VALUE); return; }
    if (!C->alvo_alfa && (base == GL_RGBA || base == GL_ALPHA || base == GL_LUMINANCE_ALPHA)) { erro(GL_INVALID_OPERATION); return; }
    textura_t *t = es_tex_destino(alvo);
    if (!t) return;
    if (!es_nivel_ok(t, nivel, larg, alt)) { erro(GL_INVALID_VALUE); return; }
#ifdef SGL_TEM_GPU
    gpu_sincronizar();
#endif
    unsigned char *px = (unsigned char *)SGL_ALOCAR((size_t)larg * alt * 4 + 4);
    if (!px) { erro(GL_OUT_OF_MEMORY); return; }
    for (int j = 0; j < alt; j++)
        for (int i = 0; i < larg; i++) {
            unsigned char *o = px + ((size_t)j * larg + i) * 4;
            int sx = x + i, sy = y + j;
            if (sx < 0 || sy < 0 || sx >= C->larg || sy >= C->alt) { o[0] = o[1] = o[2] = 0; o[3] = 255; }
            else es_ler_px(sx, sy, o);
            es_converter_base(o, base);
        }
    tex_por_nivel(t, nivel, px, larg, alt, base);
    t->formato_int = base; t->comprimida = 0;
    refaz_extras();
}

void glCopyTexSubImage2D(GLenum alvo, GLint nivel, GLint x0, GLint y0, GLint x, GLint y, GLsizei larg, GLsizei alt)
{
    if (!C) return;
    textura_t *t = es_tex_sub(alvo, nivel, x0, y0, larg, alt);
    if (!t) return;
#ifdef SGL_TEM_GPU
    gpu_sincronizar();
#endif
    unsigned char *d = nivel ? t->nivel_px[nivel] : t->px;
    int l = nivel ? t->nivel_l[nivel] : t->larg;
    for (int j = 0; j < alt; j++)
        for (int i = 0; i < larg; i++) {
            unsigned char *o = d + ((size_t)(y0 + j) * l + (x0 + i)) * 4;
            int sx = x + i, sy = y + j;
            if (sx < 0 || sy < 0 || sx >= C->larg || sy >= C->alt) continue;
            es_ler_px(sx, sy, o);
            es_converter_base(o, t->formato_base);
        }
    tex_mudou(t, nivel);
}

/* ---- parámetros ---- */

static textura_t *es_tex_param(GLenum alvo)
{
    int *sel = es_sel_alvo(alvo);
    if (!sel) { erro(GL_INVALID_ENUM); return 0; }
    if (*sel < 0) {
        /* la textura 0 de esa unidad */
        es_tex_destino(alvo == GL_TEXTURE_2D ? GL_TEXTURE_2D : GL_TEXTURE_CUBE_MAP_POSITIVE_X_OES);
        if (*sel < 0) return 0;
    }
    return &C->tex[*sel];
}

static void es_tex_parametro(GLenum alvo, GLenum nome, const float *v)
{
    if (!C) return;
    textura_t *t = es_tex_param(alvo);
    if (!t) return;
    GPU_DESPEJAR();
    int i = (int)v[0];
    switch (nome) {
    case GL_TEXTURE_WRAP_S: case GL_TEXTURE_WRAP_T:
        if (i != GL_REPEAT && i != GL_CLAMP_TO_EDGE && i != GL_MIRRORED_REPEAT_OES) { erro(GL_INVALID_ENUM); return; }
        if (nome == GL_TEXTURE_WRAP_S) t->wrap_s = i; else t->wrap_t = i;
        break;
    case GL_TEXTURE_MIN_FILTER: case GL_TEXTURE_MAG_FILTER:
        if (!filtro_valido(nome, i)) { erro(GL_INVALID_ENUM); return; }
        if (nome == GL_TEXTURE_MIN_FILTER) t->filtro_min = i; else t->filtro_mag = i;
        break;
    case GL_GENERATE_MIPMAP:
        t->gerar_mip = i ? 1 : 0;
        if (t->gerar_mip && t->px) tex_preparar_mip(t);
        if (t->gerar_mip && t->faces) for (int f = 0; f < 6; f++) if (t->faces[f].px) tex_preparar_mip(&t->faces[f]);
        break;
    case GL_TEXTURE_CROP_RECT_OES:
        for (int k = 0; k < 4; k++) t->recorte[k] = (int)v[k];
        break;
    case GL_TEXTURE_MAX_ANISOTROPY_EXT:
        if (v[0] < 1.0f) { erro(GL_INVALID_VALUE); return; }
        t->aniso = v[0] > 16.0f ? 16.0f : v[0];
        break;
    default: erro(GL_INVALID_ENUM); return;
    }
    tex_cubo_parametros(t);
}

void glTexParameterf(GLenum a, GLenum n, GLfloat v) { float x[4] = { v, 0, 0, 0 }; if (n == GL_TEXTURE_CROP_RECT_OES) { erro(GL_INVALID_ENUM); return; } es_tex_parametro(a, n, x); }
void glTexParameteri(GLenum a, GLenum n, GLint v) { float x[4] = { (float)v, 0, 0, 0 }; if (n == GL_TEXTURE_CROP_RECT_OES) { erro(GL_INVALID_ENUM); return; } es_tex_parametro(a, n, x); }
void glTexParameterfv(GLenum a, GLenum n, const GLfloat *v)
{
    float x[4] = { v[0], 0, 0, 0 };
    if (n == GL_TEXTURE_CROP_RECT_OES) for (int k = 1; k < 4; k++) x[k] = v[k];
    es_tex_parametro(a, n, x);
}
void glTexParameteriv(GLenum a, GLenum n, const GLint *v)
{
    float x[4] = { (float)v[0], 0, 0, 0 };
    if (n == GL_TEXTURE_CROP_RECT_OES) for (int k = 1; k < 4; k++) x[k] = (float)v[k];
    es_tex_parametro(a, n, x);
}

void glGetTexParameterfv(GLenum alvo, GLenum nome, GLfloat *v)
{
    if (!C) return;
    textura_t *t = es_tex_param(alvo);
    if (!t) return;
    switch (nome) {
    case GL_TEXTURE_WRAP_S: v[0] = (float)t->wrap_s; break;
    case GL_TEXTURE_WRAP_T: v[0] = (float)t->wrap_t; break;
    case GL_TEXTURE_MIN_FILTER: v[0] = (float)t->filtro_min; break;
    case GL_TEXTURE_MAG_FILTER: v[0] = (float)t->filtro_mag; break;
    case GL_GENERATE_MIPMAP: v[0] = (float)t->gerar_mip; break;
    case GL_TEXTURE_CROP_RECT_OES: for (int k = 0; k < 4; k++) v[k] = (float)t->recorte[k]; break;
    case GL_TEXTURE_MAX_ANISOTROPY_EXT: v[0] = t->aniso; break;
    default: erro(GL_INVALID_ENUM);
    }
}

void glGetTexParameteriv(GLenum alvo, GLenum nome, GLint *v)
{
    float f[4] = { 0, 0, 0, 0 };
    glGetTexParameterfv(alvo, nome, f);
    int n = nome == GL_TEXTURE_CROP_RECT_OES ? 4 : 1;
    for (int k = 0; k < n; k++) v[k] = (GLint)f[k];
}

void glGenerateMipmapOES(GLenum alvo)
{
    if (!C) return;
    int *sel = es_sel_alvo(alvo);
    if (!sel) { erro(GL_INVALID_ENUM); return; }
    if (*sel < 0) { erro(GL_INVALID_OPERATION); return; }
    textura_t *t = &C->tex[*sel];
    GPU_DESPEJAR();
    if (t->cubo) {
        for (int f = 0; f < 6; f++) {
            if (!t->faces[f].px) { erro(GL_INVALID_OPERATION); return; }
            soltar_niveis(&t->faces[f]);
            if (!tex_preparar_mip(&t->faces[f])) { erro(GL_OUT_OF_MEMORY); return; }
            t->faces[f].geracao = ++C->ger_cont;
        }
        return;
    }
    if (!t->px) { erro(GL_INVALID_OPERATION); return; }
    soltar_niveis(t);
    if (!tex_preparar_mip(t)) erro(GL_OUT_OF_MEMORY);
    t->geracao = ++C->ger_cont;
}

void glGenerateMipmap(GLenum alvo) { glGenerateMipmapOES(alvo); }

static void es_fbo_textura_apagada(int ti);

void glDeleteTextures(GLsizei n, const GLuint *ids)
{
    if (!C || !ids) return;
    if (n < 0) { erro(GL_INVALID_VALUE); return; }
    GPU_DESPEJAR();
    for (int i = 0; i < n; i++) {
        if (!ids[i]) continue;
        textura_t *t = textura_por_id(ids[i]);
        if (!t) continue;
        int ti = (int)(t - C->tex);
#ifdef SGL_TEM_GPU
        if (C->gpu)
            for (int s = 0; s < C->gpu_vagas; s++) if (C->vaga_tex[s] == ti) gpu_soltar_vaga(s);
#endif
        es_fbo_textura_apagada(ti);
        tex_soltar_tudo(t);
        t->usada = 0; t->cubo = 0;
        t->geracao = ++C->ger_cont;
        for (int u = 0; u < SGL_UNIDADES; u++) {
            if (C->un[u].sel == ti) C->un[u].sel = -1;
            if (C->un[u].sel_cubo == ti) C->un[u].sel_cubo = -1;
        }
    }
}

/* ---- TexEnv ---- */

static int es_src_valida(int v) { return v == GL_TEXTURE || v == GL_CONSTANT || v == GL_PRIMARY_COLOR || v == GL_PREVIOUS; }

static void es_tex_env(GLenum alvo, GLenum nome, const float *v)
{
    if (!C) return;
    unid_t *u = &UNID_ATIVA;
    if (alvo == GL_POINT_SPRITE_OES) {
        if (nome != GL_COORD_REPLACE_OES) { erro(GL_INVALID_ENUM); return; }
        GPU_DESPEJAR();
        u->coord_replace = v[0] != 0.0f;
        return;
    }
    if (alvo != GL_TEXTURE_ENV) { erro(GL_INVALID_ENUM); return; }
    int i = (int)v[0];
    GPU_DESPEJAR();
    switch (nome) {
    case GL_TEXTURE_ENV_MODE:
        if (i != GL_MODULATE && i != GL_REPLACE && i != GL_DECAL && i != GL_BLEND && i != GL_ADD && i != GL_COMBINE) { erro(GL_INVALID_ENUM); return; }
        u->env_modo = i; return;
    case GL_TEXTURE_ENV_COLOR:
        for (int k = 0; k < 4; k++) u->env_cor[k] = sat01(v[k]);
        return;
    case GL_COMBINE_RGB:
        if (i != GL_REPLACE && i != GL_MODULATE && i != GL_ADD && i != GL_ADD_SIGNED && i != GL_INTERPOLATE &&
            i != GL_SUBTRACT && i != GL_DOT3_RGB && i != GL_DOT3_RGBA) { erro(GL_INVALID_ENUM); return; }
        u->cb_rgb = i; return;
    case GL_COMBINE_ALPHA:
        if (i != GL_REPLACE && i != GL_MODULATE && i != GL_ADD && i != GL_ADD_SIGNED && i != GL_INTERPOLATE && i != GL_SUBTRACT)
            { erro(GL_INVALID_ENUM); return; }
        u->cb_alfa = i; return;
    case GL_RGB_SCALE: case GL_ALPHA_SCALE:
        if (v[0] != 1.0f && v[0] != 2.0f && v[0] != 4.0f) { erro(GL_INVALID_VALUE); return; }
        if (nome == GL_RGB_SCALE) u->esc_rgb = v[0]; else u->esc_alfa = v[0];
        return;
    case GL_SRC0_RGB: case GL_SRC1_RGB: case GL_SRC2_RGB:
        if (!es_src_valida(i)) { erro(GL_INVALID_ENUM); return; }
        u->src_rgb[nome - GL_SRC0_RGB] = i; return;
    case GL_SRC0_ALPHA: case GL_SRC1_ALPHA: case GL_SRC2_ALPHA:
        if (!es_src_valida(i)) { erro(GL_INVALID_ENUM); return; }
        u->src_alfa[nome - GL_SRC0_ALPHA] = i; return;
    case GL_OPERAND0_RGB: case GL_OPERAND1_RGB: case GL_OPERAND2_RGB:
        if (i != GL_SRC_COLOR && i != GL_ONE_MINUS_SRC_COLOR && i != GL_SRC_ALPHA && i != GL_ONE_MINUS_SRC_ALPHA) { erro(GL_INVALID_ENUM); return; }
        u->opd_rgb[nome - GL_OPERAND0_RGB] = i; return;
    case GL_OPERAND0_ALPHA: case GL_OPERAND1_ALPHA: case GL_OPERAND2_ALPHA:
        if (i != GL_SRC_ALPHA && i != GL_ONE_MINUS_SRC_ALPHA) { erro(GL_INVALID_ENUM); return; }
        u->opd_alfa[nome - GL_OPERAND0_ALPHA] = i; return;
    default: erro(GL_INVALID_ENUM);
    }
}

void glTexEnvf(GLenum a, GLenum n, GLfloat v) { float x[4] = { v, 0, 0, 0 }; if (n == GL_TEXTURE_ENV_COLOR) { erro(GL_INVALID_ENUM); return; } es_tex_env(a, n, x); }
void glTexEnvi(GLenum a, GLenum n, GLint v) { float x[4] = { (float)v, 0, 0, 0 }; if (n == GL_TEXTURE_ENV_COLOR) { erro(GL_INVALID_ENUM); return; } es_tex_env(a, n, x); }
void glTexEnvfv(GLenum a, GLenum n, const GLfloat *v)
{
    float x[4] = { v[0], 0, 0, 0 };
    if (n == GL_TEXTURE_ENV_COLOR) for (int k = 1; k < 4; k++) x[k] = v[k];
    es_tex_env(a, n, x);
}
void glTexEnviv(GLenum a, GLenum n, const GLint *v)
{
    float x[4] = { (float)v[0], 0, 0, 0 };
    /* los colores enteros van de 0 a 2^31-1 (como en GL) */
    if (n == GL_TEXTURE_ENV_COLOR) for (int k = 0; k < 4; k++) x[k] = (float)((double)v[k] / 2147483647.0);
    es_tex_env(a, n, x);
}

void glGetTexEnvfv(GLenum alvo, GLenum nome, GLfloat *v)
{
    if (!C) return;
    unid_t *u = &UNID_ATIVA;
    if (alvo == GL_POINT_SPRITE_OES && nome == GL_COORD_REPLACE_OES) { v[0] = (float)u->coord_replace; return; }
    if (alvo != GL_TEXTURE_ENV) { erro(GL_INVALID_ENUM); return; }
    switch (nome) {
    case GL_TEXTURE_ENV_MODE: v[0] = (float)u->env_modo; break;
    case GL_TEXTURE_ENV_COLOR: for (int k = 0; k < 4; k++) v[k] = u->env_cor[k]; break;
    case GL_COMBINE_RGB: v[0] = (float)u->cb_rgb; break;
    case GL_COMBINE_ALPHA: v[0] = (float)u->cb_alfa; break;
    case GL_RGB_SCALE: v[0] = u->esc_rgb; break;
    case GL_ALPHA_SCALE: v[0] = u->esc_alfa; break;
    case GL_SRC0_RGB: case GL_SRC1_RGB: case GL_SRC2_RGB: v[0] = (float)u->src_rgb[nome - GL_SRC0_RGB]; break;
    case GL_SRC0_ALPHA: case GL_SRC1_ALPHA: case GL_SRC2_ALPHA: v[0] = (float)u->src_alfa[nome - GL_SRC0_ALPHA]; break;
    case GL_OPERAND0_RGB: case GL_OPERAND1_RGB: case GL_OPERAND2_RGB: v[0] = (float)u->opd_rgb[nome - GL_OPERAND0_RGB]; break;
    case GL_OPERAND0_ALPHA: case GL_OPERAND1_ALPHA: case GL_OPERAND2_ALPHA: v[0] = (float)u->opd_alfa[nome - GL_OPERAND0_ALPHA]; break;
    default: erro(GL_INVALID_ENUM);
    }
}

void glGetTexEnviv(GLenum alvo, GLenum nome, GLint *v)
{
    float f[4] = { 0, 0, 0, 0 };
    glGetTexEnvfv(alvo, nome, f);
    if (nome == GL_TEXTURE_ENV_COLOR) for (int k = 0; k < 4; k++) v[k] = (GLint)((double)f[k] * 2147483647.0);
    else v[0] = (GLint)f[0];
}

/* ---- generación de coordenadas (OES_texture_cube_map) ---- */

void glTexGenfvOES(GLenum coord, GLenum nome, const GLfloat *v)
{
    if (!C) return;
    if (coord != GL_TEXTURE_GEN_STR_OES || nome != GL_TEXTURE_GEN_MODE_OES) { erro(GL_INVALID_ENUM); return; }
    int m = (int)v[0];
    if (m != GL_NORMAL_MAP_OES && m != GL_REFLECTION_MAP_OES) { erro(GL_INVALID_ENUM); return; }
    GPU_DESPEJAR();
    UNID_ATIVA.gen_modo = m;
}
void glTexGenfOES(GLenum c, GLenum n, GLfloat v) { glTexGenfvOES(c, n, &v); }
void glTexGeniOES(GLenum c, GLenum n, GLint v) { float f = (float)v; glTexGenfvOES(c, n, &f); }
void glTexGenivOES(GLenum c, GLenum n, const GLint *v) { float f = (float)v[0]; glTexGenfvOES(c, n, &f); }
void glGetTexGenfvOES(GLenum coord, GLenum nome, GLfloat *v)
{
    if (!C) return;
    if (coord != GL_TEXTURE_GEN_STR_OES || nome != GL_TEXTURE_GEN_MODE_OES) { erro(GL_INVALID_ENUM); return; }
    v[0] = (float)UNID_ATIVA.gen_modo;
}
void glGetTexGenivOES(GLenum c, GLenum n, GLint *v) { float f = 0; glGetTexGenfvOES(c, n, &f); v[0] = (GLint)f; }

void glPixelStorei(GLenum nome, GLint v)
{
    if (!C) return;
    if (v != 1 && v != 2 && v != 4 && v != 8) { erro(GL_INVALID_VALUE); return; }
    if (nome == GL_PACK_ALIGNMENT) C->pack_alin = v;
    else if (nome == GL_UNPACK_ALIGNMENT) C->unpack_alin = v;
    else erro(GL_INVALID_ENUM);
}

/* ------------------------------------------------------------ estado de los fragmentos */

void glAlphaFunc(GLenum f, GLclampf ref)
{
    if (!C) return;
    if (!es_cmp_valida(f)) { erro(GL_INVALID_ENUM); return; }
    GPU_DESPEJAR();
    C->alfa_func = (int)f; C->alfa_ref = sat01(ref);
}

void glDepthFunc(GLenum f)
{
    if (!C) return;
    if (!es_cmp_valida(f)) { erro(GL_INVALID_ENUM); return; }
    GPU_DESPEJAR();
    C->func_z = (int)f;
}

void glDepthMask(GLboolean m) { if (C) { GPU_DESPEJAR(); C->masc_z = m ? 1 : 0; } }

void glColorMask(GLboolean r, GLboolean g, GLboolean b, GLboolean a)
{
    if (!C) return;
    GPU_DESPEJAR();
    C->masc_cor = (r ? 1 : 0) | (g ? 2 : 0) | (b ? 4 : 0) | (a ? 8 : 0);
}

void glClearDepthf(GLclampf d) { if (C) C->z_limpo = sat01(d); }

void glDepthRangef(GLclampf n, GLclampf f)
{
    if (!C) return;
    GPU_DESPEJAR();
    C->z_perto = sat01(n); C->z_longe = sat01(f);
}

void glScissor(GLint x, GLint y, GLsizei l, GLsizei a)
{
    if (!C) return;
    if (l < 0 || a < 0) { erro(GL_INVALID_VALUE); return; }
    GPU_DESPEJAR();
    C->tij_x = x; C->tij_y = y; C->tij_l = l; C->tij_a = a;
}

void glPolygonOffset(GLfloat fator, GLfloat unid)
{
    if (!C) return;
    GPU_DESPEJAR();
    C->off_fator = fator; C->off_unid = unid;
}

void glStencilFunc(GLenum f, GLint ref, GLuint masc)
{
    if (!C) return;
    if (!es_cmp_valida(f)) { erro(GL_INVALID_ENUM); return; }
    GPU_DESPEJAR();
    C->est_func = (int)f; C->est_ref = ref < 0 ? 0 : ref > 255 ? 255 : ref; C->est_masc = masc;
}

static int es_est_op_valida(GLenum o) { return o == GL_KEEP || o == GL_ZERO || o == GL_REPLACE || o == GL_INCR || o == GL_DECR || o == GL_INVERT; }

void glStencilOp(GLenum falha, GLenum zfalha, GLenum zpassa)
{
    if (!C) return;
    if (!es_est_op_valida(falha) || !es_est_op_valida(zfalha) || !es_est_op_valida(zpassa)) { erro(GL_INVALID_ENUM); return; }
    GPU_DESPEJAR();
    C->est_falha = (int)falha; C->est_zfalha = (int)zfalha; C->est_zpassa = (int)zpassa;
}

void glStencilMask(GLuint m) { if (C) { GPU_DESPEJAR(); C->est_escr = m; } }
void glClearStencil(GLint s) { if (C) C->est_limpo = s & 0xFF; }

void glClipPlanef(GLenum p, const GLfloat *eq)
{
    if (!C) return;
    if (p < GL_CLIP_PLANE0 || p > GL_CLIP_PLANE5) { erro(GL_INVALID_ENUM); return; }
    /* Al espacio de la vista: el plano por la inversa de la modelo/vista de ahora. */
    float inv[16];
    const float *M = C->mv[C->mv_n - 1];
    if (!mat_inversa(inv, M)) mat_identidade(inv);
    float *o = C->plano_cl[p - GL_CLIP_PLANE0];
    for (int i = 0; i < 4; i++) o[i] = eq[0] * inv[i * 4] + eq[1] * inv[i * 4 + 1] + eq[2] * inv[i * 4 + 2] + eq[3] * inv[i * 4 + 3];
}

void glGetClipPlanef(GLenum p, GLfloat *eq)
{
    if (!C) return;
    if (p < GL_CLIP_PLANE0 || p > GL_CLIP_PLANE5) { erro(GL_INVALID_ENUM); return; }
    for (int i = 0; i < 4; i++) eq[i] = C->plano_cl[p - GL_CLIP_PLANE0][i];
}

void glLogicOp(GLenum op)
{
    if (!C) return;
    if (op < GL_CLEAR || op > GL_SET) { erro(GL_INVALID_ENUM); return; }
    GPU_DESPEJAR();
    C->log_op = (int)op;
}

void glSampleCoverage(GLclampf v, GLboolean inv) { if (C) { C->cob_valor = sat01(v); C->cob_inv = inv ? 1 : 0; } }

void glLineWidth(GLfloat l)
{
    if (!C) return;
    if (!(l > 0.0f)) { erro(GL_INVALID_VALUE); return; }
    GPU_DESPEJAR();
    C->linha_larg = l;
}

void glPointSize(GLfloat t)
{
    if (!C) return;
    if (!(t > 0.0f)) { erro(GL_INVALID_VALUE); return; }
    GPU_DESPEJAR();
    C->ponto_tam = t;
}

void glPointParameterfv(GLenum nome, const GLfloat *v)
{
    if (!C) return;
    GPU_DESPEJAR();
    switch (nome) {
    case GL_POINT_SIZE_MIN: if (v[0] < 0.0f) { erro(GL_INVALID_VALUE); return; } C->ponto_min = v[0]; break;
    case GL_POINT_SIZE_MAX: if (v[0] < 0.0f) { erro(GL_INVALID_VALUE); return; } C->ponto_max = v[0]; break;
    case GL_POINT_FADE_THRESHOLD_SIZE: if (v[0] < 0.0f) { erro(GL_INVALID_VALUE); return; } C->ponto_fade = v[0]; break;
    case GL_POINT_DISTANCE_ATTENUATION: for (int k = 0; k < 3; k++) C->ponto_att[k] = v[k]; break;
    default: erro(GL_INVALID_ENUM);
    }
}

void glPointParameterf(GLenum nome, GLfloat v)
{
    if (nome == GL_POINT_DISTANCE_ATTENUATION) { erro(GL_INVALID_ENUM); return; }
    glPointParameterfv(nome, &v);
}

static int es_fator_valido(GLenum f, int origem)
{
    switch (f) {
    case GL_ZERO: case GL_ONE: case GL_SRC_ALPHA: case GL_ONE_MINUS_SRC_ALPHA: case GL_DST_ALPHA: case GL_ONE_MINUS_DST_ALPHA:
        return 1;
    case GL_SRC_COLOR: case GL_ONE_MINUS_SRC_COLOR: return !origem;
    case GL_DST_COLOR: case GL_ONE_MINUS_DST_COLOR: case GL_SRC_ALPHA_SATURATE: return origem;
    default: return 0;
    }
}

/* OES_blend_func_separate deja usar cualquier factor en los dos lados (como GL 1.4). */
static int es_fator_sep_valido(GLenum f) { return es_fator_valido(f, 1) || es_fator_valido(f, 0); }

void glBlendFunc(GLenum s, GLenum d)
{
    if (!C) return;
    if (!es_fator_valido(s, 1) || !es_fator_valido(d, 0)) { erro(GL_INVALID_ENUM); return; }
    GPU_DESPEJAR();
    C->blend_s = C->blend_sa = (int)s; C->blend_d = C->blend_da = (int)d;
}

void glBlendFuncSeparateOES(GLenum s, GLenum d, GLenum sa, GLenum da)
{
    if (!C) return;
    if (!es_fator_sep_valido(s) || !es_fator_sep_valido(d) || !es_fator_sep_valido(sa) || !es_fator_sep_valido(da) ||
        d == GL_SRC_ALPHA_SATURATE || da == GL_SRC_ALPHA_SATURATE) { erro(GL_INVALID_ENUM); return; }
    GPU_DESPEJAR();
    C->blend_s = (int)s; C->blend_d = (int)d; C->blend_sa = (int)sa; C->blend_da = (int)da;
}

static int es_eq_valida(GLenum e) { return e == GL_FUNC_ADD_OES || e == GL_FUNC_SUBTRACT_OES || e == GL_FUNC_REVERSE_SUBTRACT_OES; }

void glBlendEquationSeparateOES(GLenum e, GLenum ea)
{
    if (!C) return;
    if (!es_eq_valida(e) || !es_eq_valida(ea)) { erro(GL_INVALID_ENUM); return; }
    GPU_DESPEJAR();
    C->blend_eq = (int)e; C->blend_eqa = (int)ea;
}

void glBlendEquationOES(GLenum e) { glBlendEquationSeparateOES(e, e); }

void glLightModelf(GLenum nome, GLfloat v)
{
    if (!C) return;
    if (nome != GL_LIGHT_MODEL_TWO_SIDE) { erro(GL_INVALID_ENUM); return; }
    GPU_DESPEJAR();
    C->dois_lados = v != 0.0f;
}

/* ------------------------------------------------------------ matrices de textura y paleta */

/* La matriz que tocan glLoadIdentity, glMultMatrixf...: también GL_TEXTURE y la paleta. */
static float *es_atual(void)
{
    if (C->modo_mat == GL_TEXTURE) { unid_t *u = &UNID_ATIVA; return u->mat[u->mat_n - 1]; }
    if (C->modo_mat == GL_MATRIX_PALETTE_OES) return C->paleta[C->paleta_atual];
    return 0;
}

void glMatrixMode(GLenum m)
{
    if (!C) return;
    if (m != GL_MODELVIEW && m != GL_PROJECTION && m != GL_TEXTURE && m != GL_MATRIX_PALETTE_OES) { erro(GL_INVALID_ENUM); return; }
    C->modo_mat = (int)m;
}

void glPushMatrix(void)
{
    if (!C) return;
    if (C->modo_mat == GL_TEXTURE) {
        unid_t *u = &UNID_ATIVA;
        if (u->mat_n >= PILHA_TEX) { erro(GL_STACK_OVERFLOW); return; }
        for (int i = 0; i < 16; i++) u->mat[u->mat_n][i] = u->mat[u->mat_n - 1][i];
        u->mat_n++;
        return;
    }
    if (C->modo_mat == GL_MATRIX_PALETTE_OES) { erro(GL_INVALID_OPERATION); return; }
    if (C->modo_mat == GL_PROJECTION) {
        if (C->proj_n >= PILHA_PROJ) { erro(GL_STACK_OVERFLOW); return; }
        for (int i = 0; i < 16; i++) C->proj[C->proj_n][i] = C->proj[C->proj_n - 1][i];
        C->proj_n++;
    } else {
        if (C->mv_n >= PILHA_MV) { erro(GL_STACK_OVERFLOW); return; }
        for (int i = 0; i < 16; i++) C->mv[C->mv_n][i] = C->mv[C->mv_n - 1][i];
        C->mv_n++;
    }
}

void glPopMatrix(void)
{
    if (!C) return;
    if (C->modo_mat == GL_TEXTURE) {
        unid_t *u = &UNID_ATIVA;
        if (u->mat_n <= 1) { erro(GL_STACK_UNDERFLOW); return; }
        u->mat_n--;
        return;
    }
    if (C->modo_mat == GL_MATRIX_PALETTE_OES) { erro(GL_INVALID_OPERATION); return; }
    int *n = C->modo_mat == GL_PROJECTION ? &C->proj_n : &C->mv_n;
    if (*n <= 1) { erro(GL_STACK_UNDERFLOW); return; }
    (*n)--;
}

/* OES_query_matrix: la matriz actual en mantisa (coma fija 16.16) y exponente. */
GLbitfield glQueryMatrixxOES(GLfixed *mant, GLint *expo)
{
    if (!C) return 0xFFFF;
    const float *m = atual();
    GLbitfield malos = 0;
    for (int i = 0; i < 16; i++) {
        float v = m[i];
        if (v != v || v > 3.0e38f || v < -3.0e38f) { malos |= 1u << i; mant[i] = 0; expo[i] = 0; continue; }
        int e = 0;
        float a = v < 0 ? -v : v;
        while (a >= 32767.0f) { a *= 0.5f; v *= 0.5f; e++; }
        while (a != 0.0f && a < 16384.0f && e > -100) { a *= 2.0f; v *= 2.0f; e--; }
        mant[i] = (GLfixed)(v * 65536.0f);
        expo[i] = e;
    }
    return malos;
}

/* ------------------------------------------------------------ consultas (glGet*) */

#define OB_INT  0   /* entero o enum */
#define OB_COR  1   /* color o normal: a entero se escala de [-1, 1] a [-2^31, 2^31-1] */
#define OB_REAL 2   /* real: a entero se redondea */
#define OB_BITS 3   /* OES_matrix_get: los bits del float como entero */

static const GLenum es_comprimidos[] = {
    GL_PALETTE4_RGB8_OES, GL_PALETTE4_RGBA8_OES, GL_PALETTE4_R5_G6_B5_OES, GL_PALETTE4_RGBA4_OES, GL_PALETTE4_RGB5_A1_OES,
    GL_PALETTE8_RGB8_OES, GL_PALETTE8_RGBA8_OES, GL_PALETTE8_R5_G6_B5_OES, GL_PALETTE8_RGBA4_OES, GL_PALETTE8_RGB5_A1_OES,
    GL_COMPRESSED_RGB_S3TC_DXT1_EXT, GL_COMPRESSED_RGBA_S3TC_DXT1_EXT, GL_COMPRESSED_RGBA_S3TC_DXT3_EXT, GL_COMPRESSED_RGBA_S3TC_DXT5_EXT,
};
#define N_COMPRIMIDOS ((int)(sizeof es_comprimidos / sizeof es_comprimidos[0]))

static GLuint es_id_tex(int sel) { return sel >= 0 && C->tex[sel].usada && C->tex[sel].id < 0x7FFFFF00u ? C->tex[sel].id : 0; }

/* Valores de «p» en v[] (hasta 16). Devuelve cuántos (-1: no existe) y el tipo. */
static int es_obter(GLenum p, float *v, int *tipo)
{
    *tipo = OB_INT;
    const unid_t *u = &UNID_ATIVA;
    int r;
    switch (p) {
    case GL_CURRENT_COLOR: for (int k = 0; k < 4; k++) v[k] = C->cor[k] * (1.0f / 255.0f); *tipo = OB_COR; return 4;
    case GL_CURRENT_NORMAL: for (int k = 0; k < 3; k++) v[k] = C->normal[k]; *tipo = OB_COR; return 3;
    case GL_CURRENT_TEXTURE_COORDS: for (int k = 0; k < 4; k++) v[k] = u->tc[k]; *tipo = OB_REAL; return 4;
    case GL_POINT_SIZE: v[0] = C->ponto_tam; *tipo = OB_REAL; return 1;
    case GL_POINT_SIZE_MIN: v[0] = C->ponto_min; *tipo = OB_REAL; return 1;
    case GL_POINT_SIZE_MAX: v[0] = C->ponto_max; *tipo = OB_REAL; return 1;
    case GL_POINT_FADE_THRESHOLD_SIZE: v[0] = C->ponto_fade; *tipo = OB_REAL; return 1;
    case GL_POINT_DISTANCE_ATTENUATION: for (int k = 0; k < 3; k++) v[k] = C->ponto_att[k]; *tipo = OB_REAL; return 3;
    case GL_SMOOTH_POINT_SIZE_RANGE: case GL_ALIASED_POINT_SIZE_RANGE: v[0] = 1; v[1] = 128; return 2;
    case GL_LINE_WIDTH: v[0] = C->linha_larg; *tipo = OB_REAL; return 1;
    case GL_SMOOTH_LINE_WIDTH_RANGE: case GL_ALIASED_LINE_WIDTH_RANGE: v[0] = 1; v[1] = 64; return 2;
    case GL_CULL_FACE_MODE: v[0] = (float)C->cull_face; return 1;
    case GL_FRONT_FACE: v[0] = (float)C->frente; return 1;
    case GL_SHADE_MODEL: v[0] = C->plano ? GL_FLAT : GL_SMOOTH; return 1;
    case GL_DEPTH_RANGE: v[0] = C->z_perto; v[1] = C->z_longe; *tipo = OB_COR; return 2;
    case GL_DEPTH_WRITEMASK: v[0] = (float)C->masc_z; return 1;
    case GL_DEPTH_CLEAR_VALUE: v[0] = C->z_limpo; *tipo = OB_COR; return 1;
    case GL_DEPTH_FUNC: v[0] = (float)C->func_z; return 1;
    case GL_STENCIL_CLEAR_VALUE: v[0] = (float)C->est_limpo; return 1;
    case GL_STENCIL_FUNC: v[0] = (float)C->est_func; return 1;
    case GL_STENCIL_VALUE_MASK: v[0] = (float)(C->est_masc & 0xFF); return 1;
    case GL_STENCIL_FAIL: v[0] = (float)C->est_falha; return 1;
    case GL_STENCIL_PASS_DEPTH_FAIL: v[0] = (float)C->est_zfalha; return 1;
    case GL_STENCIL_PASS_DEPTH_PASS: v[0] = (float)C->est_zpassa; return 1;
    case GL_STENCIL_REF: v[0] = (float)C->est_ref; return 1;
    case GL_STENCIL_WRITEMASK: v[0] = (float)(C->est_escr & 0xFF); return 1;
    case GL_MATRIX_MODE: v[0] = (float)C->modo_mat; return 1;
    case GL_VIEWPORT: v[0] = (float)C->vx; v[1] = (float)C->vy; v[2] = (float)C->vw; v[3] = (float)C->vh; return 4;
    case GL_MODELVIEW_STACK_DEPTH: v[0] = (float)C->mv_n; return 1;
    case GL_PROJECTION_STACK_DEPTH: v[0] = (float)C->proj_n; return 1;
    case GL_TEXTURE_STACK_DEPTH: v[0] = (float)u->mat_n; return 1;
    case GL_MODELVIEW_MATRIX: case GL_MODELVIEW_MATRIX_FLOAT_AS_INT_BITS_OES:
        for (int k = 0; k < 16; k++) v[k] = C->mv[C->mv_n - 1][k];
        *tipo = p == GL_MODELVIEW_MATRIX ? OB_REAL : OB_BITS; return 16;
    case GL_PROJECTION_MATRIX: case GL_PROJECTION_MATRIX_FLOAT_AS_INT_BITS_OES:
        for (int k = 0; k < 16; k++) v[k] = C->proj[C->proj_n - 1][k];
        *tipo = p == GL_PROJECTION_MATRIX ? OB_REAL : OB_BITS; return 16;
    case GL_TEXTURE_MATRIX: case GL_TEXTURE_MATRIX_FLOAT_AS_INT_BITS_OES:
        for (int k = 0; k < 16; k++) v[k] = u->mat[u->mat_n - 1][k];
        *tipo = p == GL_TEXTURE_MATRIX ? OB_REAL : OB_BITS; return 16;
    case GL_ALPHA_TEST_FUNC: v[0] = (float)C->alfa_func; return 1;
    case GL_ALPHA_TEST_REF: v[0] = C->alfa_ref; *tipo = OB_COR; return 1;
    case GL_BLEND_DST: v[0] = (float)C->blend_d; return 1;
    case GL_BLEND_SRC: v[0] = (float)C->blend_s; return 1;
    case GL_BLEND_DST_ALPHA_OES: v[0] = (float)C->blend_da; return 1;
    case GL_BLEND_SRC_ALPHA_OES: v[0] = (float)C->blend_sa; return 1;
    case GL_BLEND_EQUATION_RGB_OES: v[0] = (float)C->blend_eq; return 1;
    case GL_BLEND_EQUATION_ALPHA_OES: v[0] = (float)C->blend_eqa; return 1;
    case GL_LOGIC_OP_MODE: v[0] = (float)C->log_op; return 1;
    case GL_SCISSOR_BOX: v[0] = (float)C->tij_x; v[1] = (float)C->tij_y; v[2] = (float)C->tij_l; v[3] = (float)C->tij_a; return 4;
    case GL_COLOR_CLEAR_VALUE:
        v[0] = (float)((C->cor_limpa >> 16) & 0xFF) / 255.0f; v[1] = (float)((C->cor_limpa >> 8) & 0xFF) / 255.0f;
        v[2] = (float)(C->cor_limpa & 0xFF) / 255.0f; v[3] = (float)C->alfa_limpo / 255.0f;
        *tipo = OB_COR; return 4;
    case GL_COLOR_WRITEMASK: for (int k = 0; k < 4; k++) v[k] = (float)((C->masc_cor >> k) & 1); return 4;
    case GL_MAX_LIGHTS: v[0] = SGL_MAX_LUZES; return 1;
    case GL_MAX_CLIP_PLANES: v[0] = 6; return 1;
    case GL_MAX_TEXTURE_SIZE: case GL_MAX_CUBE_MAP_TEXTURE_SIZE_OES: case GL_MAX_RENDERBUFFER_SIZE_OES: v[0] = SGL_MAX_LADO; return 1;
    case GL_MAX_MODELVIEW_STACK_DEPTH: v[0] = PILHA_MV; return 1;
    case GL_MAX_PROJECTION_STACK_DEPTH: v[0] = PILHA_PROJ; return 1;
    case GL_MAX_TEXTURE_STACK_DEPTH: v[0] = PILHA_TEX; return 1;
    case GL_MAX_VIEWPORT_DIMS: v[0] = LIM_VIEW; v[1] = LIM_VIEW; return 2;
    case GL_MAX_TEXTURE_UNITS: v[0] = SGL_UNIDADES; return 1;
    case GL_MAX_PALETTE_MATRICES_OES: v[0] = 32; return 1;
    case GL_MAX_VERTEX_UNITS_OES: v[0] = 4; return 1;
    case GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT: v[0] = 16; *tipo = OB_REAL; return 1;
    case GL_CURRENT_PALETTE_MATRIX_OES: v[0] = (float)C->paleta_atual; return 1;
    case GL_SUBPIXEL_BITS: v[0] = 4; return 1;
    case GL_RED_BITS: case GL_GREEN_BITS: case GL_BLUE_BITS: v[0] = 8; return 1;
    case GL_ALPHA_BITS: v[0] = C->alvo_alfa ? 8 : 0; return 1;
    case GL_DEPTH_BITS: v[0] = !C->zbuf ? 0 : C->zbits == 16 ? 16 : 24; return 1;
    case GL_STENCIL_BITS: v[0] = C->sem_estencil && !C->fbo ? 0 : 8; return 1;
    case GL_POLYGON_OFFSET_UNITS: v[0] = C->off_unid; *tipo = OB_REAL; return 1;
    case GL_POLYGON_OFFSET_FACTOR: v[0] = C->off_fator; *tipo = OB_REAL; return 1;
    case GL_TEXTURE_BINDING_2D: v[0] = (float)es_id_tex(u->sel); return 1;
    case GL_TEXTURE_BINDING_CUBE_MAP_OES: v[0] = (float)es_id_tex(u->sel_cubo); return 1;
    case GL_ACTIVE_TEXTURE: v[0] = (float)(GL_TEXTURE0 + C->unid); return 1;
    case GL_CLIENT_ACTIVE_TEXTURE: v[0] = (float)(GL_TEXTURE0 + C->unid_cli); return 1;
    case GL_VERTEX_ARRAY_SIZE: v[0] = (float)C->v_tam; return 1;
    case GL_VERTEX_ARRAY_TYPE: v[0] = (float)C->v_tipo; return 1;
    case GL_VERTEX_ARRAY_STRIDE: v[0] = (float)C->v_passo; return 1;
    case GL_NORMAL_ARRAY_TYPE: v[0] = (float)C->n_tipo; return 1;
    case GL_NORMAL_ARRAY_STRIDE: v[0] = (float)C->n_passo; return 1;
    case GL_COLOR_ARRAY_SIZE: v[0] = (float)C->c_tam; return 1;
    case GL_COLOR_ARRAY_TYPE: v[0] = (float)C->c_tipo; return 1;
    case GL_COLOR_ARRAY_STRIDE: v[0] = (float)C->c_passo; return 1;
    case GL_TEXTURE_COORD_ARRAY_SIZE: v[0] = (float)UNID_CLI.t_tam; return 1;
    case GL_TEXTURE_COORD_ARRAY_TYPE: v[0] = (float)UNID_CLI.t_tipo; return 1;
    case GL_TEXTURE_COORD_ARRAY_STRIDE: v[0] = (float)UNID_CLI.t_passo; return 1;
    case GL_POINT_SIZE_ARRAY_TYPE_OES: v[0] = (float)C->ps_tipo; return 1;
    case GL_POINT_SIZE_ARRAY_STRIDE_OES: v[0] = (float)C->ps_passo; return 1;
    case GL_MATRIX_INDEX_ARRAY_SIZE_OES: v[0] = (float)C->mi_tam; return 1;
    case GL_MATRIX_INDEX_ARRAY_TYPE_OES: v[0] = (float)C->mi_tipo; return 1;
    case GL_MATRIX_INDEX_ARRAY_STRIDE_OES: v[0] = (float)C->mi_passo; return 1;
    case GL_WEIGHT_ARRAY_SIZE_OES: v[0] = (float)C->pe_tam; return 1;
    case GL_WEIGHT_ARRAY_TYPE_OES: v[0] = (float)C->pe_tipo; return 1;
    case GL_WEIGHT_ARRAY_STRIDE_OES: v[0] = (float)C->pe_passo; return 1;
    case GL_ARRAY_BUFFER_BINDING: v[0] = (float)C->vbo_array; return 1;
    case GL_ELEMENT_ARRAY_BUFFER_BINDING: v[0] = (float)C->vbo_elem; return 1;
    case GL_VERTEX_ARRAY_BUFFER_BINDING: v[0] = (float)C->v_vbo; return 1;
    case GL_NORMAL_ARRAY_BUFFER_BINDING: v[0] = (float)C->n_vbo; return 1;
    case GL_COLOR_ARRAY_BUFFER_BINDING: v[0] = (float)C->c_vbo; return 1;
    case GL_TEXTURE_COORD_ARRAY_BUFFER_BINDING: v[0] = (float)UNID_CLI.t_vbo; return 1;
    case GL_POINT_SIZE_ARRAY_BUFFER_BINDING_OES: v[0] = (float)C->ps_vbo; return 1;
    case GL_MATRIX_INDEX_ARRAY_BUFFER_BINDING_OES: v[0] = (float)C->mi_vbo; return 1;
    case GL_WEIGHT_ARRAY_BUFFER_BINDING_OES: v[0] = (float)C->pe_vbo; return 1;
    case GL_SAMPLE_BUFFERS: case GL_SAMPLES: v[0] = 0; return 1;
    case GL_SAMPLE_COVERAGE_VALUE: v[0] = C->cob_valor; *tipo = OB_REAL; return 1;
    case GL_SAMPLE_COVERAGE_INVERT: v[0] = (float)C->cob_inv; return 1;
    case GL_NUM_COMPRESSED_TEXTURE_FORMATS: v[0] = N_COMPRIMIDOS; return 1;
    case GL_COMPRESSED_TEXTURE_FORMATS: for (int k = 0; k < N_COMPRIMIDOS; k++) v[k] = (float)es_comprimidos[k]; return N_COMPRIMIDOS;
    case GL_PERSPECTIVE_CORRECTION_HINT: v[0] = C->persp ? GL_NICEST : GL_DONT_CARE; return 1;
    case GL_FOG_HINT: v[0] = (float)C->dicas[0]; return 1;
    case GL_POINT_SMOOTH_HINT: v[0] = (float)C->dicas[1]; return 1;
    case GL_LINE_SMOOTH_HINT: v[0] = (float)C->dicas[2]; return 1;
    case GL_GENERATE_MIPMAP_HINT: v[0] = (float)C->dicas[3]; return 1;
    case GL_LIGHT_MODEL_AMBIENT: for (int k = 0; k < 3; k++) v[k] = C->modelo_amb[k]; v[3] = 1.0f; *tipo = OB_COR; return 4;
    case GL_LIGHT_MODEL_TWO_SIDE: v[0] = (float)C->dois_lados; return 1;
    case GL_FOG_DENSITY: v[0] = C->fog_dens; *tipo = OB_REAL; return 1;
    case GL_FOG_START: v[0] = C->fog_ini; *tipo = OB_REAL; return 1;
    case GL_FOG_END: v[0] = C->fog_fim; *tipo = OB_REAL; return 1;
    case GL_FOG_MODE: v[0] = C->fog_modo ? (float)C->fog_modo : (float)GL_LINEAR; return 1;
    case GL_FOG_COLOR: for (int k = 0; k < 3; k++) v[k] = C->fog_cor[k] / 255.0f; v[3] = 1.0f; *tipo = OB_COR; return 4;
    case GL_UNPACK_ALIGNMENT: v[0] = (float)C->unpack_alin; return 1;
    case GL_PACK_ALIGNMENT: v[0] = (float)C->pack_alin; return 1;
    case GL_IMPLEMENTATION_COLOR_READ_TYPE_OES: v[0] = GL_UNSIGNED_BYTE; return 1;
    case GL_IMPLEMENTATION_COLOR_READ_FORMAT_OES: v[0] = GL_RGB; return 1;
    case GL_FRAMEBUFFER_BINDING_OES: v[0] = (float)C->fbo; return 1;
    case GL_RENDERBUFFER_BINDING_OES: v[0] = (float)C->rb; return 1;
    default: break;
    }
    if (es_capacidade(p, 0, 1, &r) || (p >= GL_LIGHT0 && p <= GL_LIGHT7) ||
        p == GL_DEPTH_TEST || p == GL_CULL_FACE || p == GL_TEXTURE_2D || p == GL_BLEND || p == GL_FOG || p == GL_LIGHTING ||
        p == GL_COLOR_MATERIAL || p == GL_NORMALIZE || p == GL_RESCALE_NORMAL || p == GL_VERTEX_ARRAY || p == GL_COLOR_ARRAY ||
        p == GL_NORMAL_ARRAY || p == GL_TEXTURE_COORD_ARRAY || p == GL_POINT_SIZE_ARRAY_OES || p == GL_MATRIX_INDEX_ARRAY_OES ||
        p == GL_WEIGHT_ARRAY_OES) {
        v[0] = glIsEnabled(p) ? 1.0f : 0.0f;
        return 1;
    }
    return -1;
}

static GLint es_a_inteiro(float f, int tipo)
{
    if (tipo == OB_COR) {
        double d = (double)f * 2147483647.0;
        if (d > 2147483647.0) d = 2147483647.0;
        if (d < -2147483648.0) d = -2147483648.0;
        return (GLint)d;
    }
    if (tipo == OB_BITS) { union { float f; GLint i; } u; u.f = f; return u.i; }
    if (f >= 2147483647.0f) return 0x7FFFFFFF;
    if (f <= -2147483648.0f) return (GLint)0x80000000;
    return (GLint)(f + (f >= 0 ? 0.5f : -0.5f));
}

void glGetFloatv(GLenum p, GLfloat *o)
{
    float v[16]; int t;
    if (!C) return;
    int n = es_obter(p, v, &t);
    if (n < 0) { erro(GL_INVALID_ENUM); return; }
    for (int i = 0; i < n; i++) {
        if (t == OB_BITS) { union { float f; GLint i; } u; u.f = v[i]; o[i] = (float)u.i; }
        else o[i] = v[i];
    }
}

void glGetIntegerv(GLenum p, GLint *o)
{
    float v[16]; int t;
    if (!C) return;
    int n = es_obter(p, v, &t);
    if (n < 0) { erro(GL_INVALID_ENUM); return; }
    for (int i = 0; i < n; i++) o[i] = es_a_inteiro(v[i], t);
}

void glGetBooleanv(GLenum p, GLboolean *o)
{
    float v[16]; int t;
    if (!C) return;
    int n = es_obter(p, v, &t);
    if (n < 0) { erro(GL_INVALID_ENUM); return; }
    for (int i = 0; i < n; i++) o[i] = v[i] != 0.0f ? GL_TRUE : GL_FALSE;
}

void glGetFixedv(GLenum p, GLfixed *o)
{
    float v[16]; int t;
    if (!C) return;
    int n = es_obter(p, v, &t);
    if (n < 0) { erro(GL_INVALID_ENUM); return; }
    for (int i = 0; i < n; i++) {
        if (t == OB_BITS) { o[i] = es_a_inteiro(v[i], t); continue; }
        if (t == OB_INT) { o[i] = (GLfixed)v[i]; continue; }   /* enteros y enums tal cual (como en GL ES) */
        double d = (double)v[i] * 65536.0;
        if (d > 2147483647.0) d = 2147483647.0;
        if (d < -2147483648.0) d = -2147483648.0;
        o[i] = (GLfixed)d;
    }
}

void glGetPointerv(GLenum p, GLvoid **o)
{
    if (!C) return;
    switch (p) {
    case GL_VERTEX_ARRAY_POINTER: *o = (GLvoid *)C->v_ptr; break;
    case GL_NORMAL_ARRAY_POINTER: *o = (GLvoid *)C->n_ptr; break;
    case GL_COLOR_ARRAY_POINTER: *o = (GLvoid *)C->c_ptr; break;
    case GL_TEXTURE_COORD_ARRAY_POINTER: *o = (GLvoid *)UNID_CLI.t_ptr; break;
    case GL_POINT_SIZE_ARRAY_POINTER_OES: *o = (GLvoid *)C->ps_ptr; break;
    case GL_MATRIX_INDEX_ARRAY_POINTER_OES: *o = (GLvoid *)C->mi_ptr; break;
    case GL_WEIGHT_ARRAY_POINTER_OES: *o = (GLvoid *)C->pe_ptr; break;
    default: erro(GL_INVALID_ENUM);
    }
}

void glGetLightfv(GLenum luz, GLenum nome, GLfloat *v)
{
    if (!C) return;
    if (luz < GL_LIGHT0 || luz > GL_LIGHT7) { erro(GL_INVALID_ENUM); return; }
    int i = (int)(luz - GL_LIGHT0);
    switch (nome) {
    case GL_AMBIENT: for (int k = 0; k < 3; k++) v[k] = C->l_amb[i][k]; v[3] = 1.0f; break;
    case GL_DIFFUSE: for (int k = 0; k < 3; k++) v[k] = C->l_dif[i][k]; v[3] = 1.0f; break;
    case GL_SPECULAR: for (int k = 0; k < 3; k++) v[k] = C->l_esp[i][k]; v[3] = 1.0f; break;
    case GL_POSITION: for (int k = 0; k < 4; k++) v[k] = C->l_pos[i][k]; break;
    case GL_SPOT_DIRECTION: for (int k = 0; k < 3; k++) v[k] = C->l_dir[i][k]; break;
    case GL_SPOT_EXPONENT: v[0] = C->l_exp[i]; break;
    case GL_SPOT_CUTOFF: v[0] = C->l_corte[i]; break;
    case GL_CONSTANT_ATTENUATION: case GL_LINEAR_ATTENUATION: case GL_QUADRATIC_ATTENUATION:
        v[0] = C->l_att[i][nome - GL_CONSTANT_ATTENUATION]; break;
    default: erro(GL_INVALID_ENUM);
    }
}

void glGetMaterialfv(GLenum face, GLenum nome, GLfloat *v)
{
    if (!C) return;
    if (face != GL_FRONT && face != GL_BACK) { erro(GL_INVALID_ENUM); return; }
    switch (nome) {
    case GL_AMBIENT: for (int k = 0; k < 4; k++) v[k] = C->m_amb[k]; break;
    case GL_DIFFUSE: for (int k = 0; k < 4; k++) v[k] = C->m_dif[k]; break;
    case GL_SPECULAR: for (int k = 0; k < 3; k++) v[k] = C->m_esp[k]; v[3] = 1.0f; break;
    case GL_EMISSION: for (int k = 0; k < 3; k++) v[k] = C->m_emi[k]; v[3] = 1.0f; break;
    case GL_SHININESS: v[0] = C->m_brilho; break;
    default: erro(GL_INVALID_ENUM);
    }
}

/* ------------------------------------------------------------ glGetString */

static const char es_extensoes[] =
    "GL_OES_blend_equation_separate GL_OES_blend_func_separate GL_OES_blend_subtract "
    "GL_OES_byte_coordinates GL_OES_compressed_paletted_texture GL_OES_depth24 GL_OES_draw_texture "
    "GL_OES_element_index_uint GL_OES_fixed_point GL_OES_framebuffer_object GL_OES_mapbuffer "
    "GL_OES_matrix_get GL_OES_matrix_palette GL_OES_point_size_array GL_OES_point_sprite "
    "GL_OES_query_matrix GL_OES_read_format GL_OES_rgb8_rgba8 GL_OES_single_precision GL_OES_stencil8 "
    "GL_OES_texture_cube_map GL_OES_texture_mirrored_repeat GL_OES_texture_npot "
    "GL_EXT_texture_compression_dxt1 GL_EXT_texture_compression_s3tc GL_EXT_texture_filter_anisotropic";

#ifdef SGL_TEM_GPU
static void es_gpu_capas(void);   /* de mas abajo: glGetString(GL_RENDERER) dice si el driver tiene v3 */
#endif

const GLubyte *glGetString(GLenum nome)
{
    switch (nome) {
    case GL_VENDOR: return (const GLubyte *)"SaguiOS";
#ifdef SGL_TEM_GPU
    /* los programas (Brumacanto, D444) eligen su carga segun esto: sin GPU, con GPU (v1/v2) o con GPU y gpu3d v3 (D457) */
    case GL_RENDERER: if (C && C->gpu) es_gpu_capas();
        return (const GLubyte *)(C && C->gpu ? (C->gpu_bits3 ? "SaguiGL (GPU v3)" : "SaguiGL (GPU)") : "SaguiGL (software)");
#else
    case GL_RENDERER: return (const GLubyte *)"SaguiGL (software)";
#endif
    case GL_VERSION: return (const GLubyte *)"OpenGL ES-CM 1.1";
    case GL_EXTENSIONS: return (const GLubyte *)es_extensoes;
    default:
        erro(GL_INVALID_ENUM);
        return 0;
    }
}

/* ------------------------------------------------------------ la tubería completa: preparación */

/* Una unidad de textura tal como la usa el dibujo: la textura y su combinación ya
 * traducida a GL_COMBINE (los modos clásicos se convierten aquí: la misma forma
 * que llega a los drivers en gpu3d v2). */
typedef struct {
    const textura_t *t;      /* NULL: la unidad deja pasar lo anterior */
    int   cubo;
    int   rgb_f, alfa_f;
    int   rgb_src[3], rgb_opd[3], alfa_src[3], alfa_opd[3];
    float esc_rgb, esc_alfa;
    float cc[4];
    int   tex_cor_zero;      /* textura GL_ALPHA en GL_COMBINE: su color es (0, 0, 0) */
    int   lod;               /* hace falta el nivel de detalle (mipmaps o filtros distintos) */
} es_unid_t;

static es_unid_t EU[SGL_UNIDADES];
static int EN;               /* unidades que cuentan: de la 0 a EN-1 */

static int tex_completa(const textura_t *t)
{
    if (!t || !t->usada) return 0;
    if (t->cubo) {
        if (!t->faces) return 0;
        for (int f = 0; f < 6; f++) if (!t->faces[f].px || t->faces[f].larg != t->faces[0].larg) return 0;
        return 1;
    }
    return t->px != 0;
}

static const textura_t *es_tex_unidade(const unid_t *u, int *cubo)
{
    *cubo = 0;
    if (u->cubo_on && u->sel_cubo >= 0 && tex_completa(&C->tex[u->sel_cubo])) { *cubo = 1; return &C->tex[u->sel_cubo]; }
    if (u->cubo_on && u->sel_cubo >= 0) return 0;   /* cubo encendido pero incompleto: la unidad no hace nada */
    if (u->on && u->sel >= 0 && tex_completa(&C->tex[u->sel]) && !C->tex[u->sel].cubo) return &C->tex[u->sel];
    return 0;
}

static void es_cb(es_unid_t *e, int rf, int s0, int o0, int s1, int o1, int s2, int o2)
{
    e->rgb_f = rf; e->rgb_src[0] = s0; e->rgb_opd[0] = o0; e->rgb_src[1] = s1; e->rgb_opd[1] = o1; e->rgb_src[2] = s2; e->rgb_opd[2] = o2;
}
static void es_cba(es_unid_t *e, int af, int s0, int s1)
{
    e->alfa_f = af; e->alfa_src[0] = s0; e->alfa_opd[0] = GL_SRC_ALPHA; e->alfa_src[1] = s1; e->alfa_opd[1] = GL_SRC_ALPHA;
    e->alfa_src[2] = GL_CONSTANT; e->alfa_opd[2] = GL_SRC_ALPHA;
}

/* La tabla 3.15 de la especificación de ES 1.1 en forma de GL_COMBINE. */
static void es_traduzir(const unid_t *u, es_unid_t *e)
{
    const int TX = GL_TEXTURE, PR = GL_PREVIOUS, CT = GL_CONSTANT, CO = GL_SRC_COLOR, AL = GL_SRC_ALPHA;
    int base = e->t->cubo ? e->t->faces[0].formato_base : e->t->formato_base;
    for (int k = 0; k < 4; k++) e->cc[k] = u->env_cor[k];
    e->esc_rgb = e->esc_alfa = 1.0f;
    e->tex_cor_zero = 0;
    int m = u->env_modo;
    if (m == GL_COMBINE) {
        e->rgb_f = u->cb_rgb; e->alfa_f = u->cb_alfa;
        for (int k = 0; k < 3; k++) {
            e->rgb_src[k] = u->src_rgb[k]; e->rgb_opd[k] = u->opd_rgb[k];
            e->alfa_src[k] = u->src_alfa[k]; e->alfa_opd[k] = u->opd_alfa[k];
        }
        e->esc_rgb = u->esc_rgb; e->esc_alfa = u->esc_alfa;
        e->tex_cor_zero = base == GL_ALPHA;
        return;
    }
    int tem_alfa = base == GL_RGBA || base == GL_LUMINANCE_ALPHA || base == GL_ALPHA;
    int tem_cor = base != GL_ALPHA;
    switch (m) {
    case GL_REPLACE:
        if (tem_cor) es_cb(e, GL_REPLACE, TX, CO, PR, CO, CT, CO); else es_cb(e, GL_REPLACE, PR, CO, PR, CO, CT, CO);
        es_cba(e, GL_REPLACE, tem_alfa ? TX : PR, PR);
        break;
    case GL_DECAL:
        if (base == GL_RGBA) es_cb(e, GL_INTERPOLATE, TX, CO, PR, CO, TX, AL);
        else if (base == GL_RGB) es_cb(e, GL_REPLACE, TX, CO, PR, CO, CT, CO);
        else es_cb(e, GL_REPLACE, PR, CO, PR, CO, CT, CO);   /* indefinido en GL: se deja pasar */
        es_cba(e, GL_REPLACE, PR, PR);
        break;
    case GL_BLEND:
        if (tem_cor) es_cb(e, GL_INTERPOLATE, CT, CO, PR, CO, TX, CO); else es_cb(e, GL_REPLACE, PR, CO, PR, CO, CT, CO);
        if (tem_alfa) es_cba(e, GL_MODULATE, PR, TX); else es_cba(e, GL_REPLACE, PR, PR);
        break;
    case GL_ADD:
        if (tem_cor) es_cb(e, GL_ADD, PR, CO, TX, CO, CT, CO); else es_cb(e, GL_REPLACE, PR, CO, PR, CO, CT, CO);
        if (tem_alfa) es_cba(e, GL_MODULATE, PR, TX); else es_cba(e, GL_REPLACE, PR, PR);
        break;
    default:   /* GL_MODULATE */
        if (tem_cor) es_cb(e, GL_MODULATE, PR, CO, TX, CO, CT, CO); else es_cb(e, GL_REPLACE, PR, CO, PR, CO, CT, CO);
        if (tem_alfa) es_cba(e, GL_MODULATE, PR, TX); else es_cba(e, GL_REPLACE, PR, PR);
        break;
    }
}

static void es_preparar_unidades(void)
{
    EN = 0;
    for (int i = 0; i < SGL_UNIDADES; i++) {
        es_unid_t *e = &EU[i];
        int cubo;
        e->t = es_tex_unidade(&C->un[i], &cubo);
        e->cubo = cubo;
        if (!e->t) continue;
        es_traduzir(&C->un[i], e);
        const textura_t *f = cubo ? &e->t->faces[0] : e->t;
        e->lod = f->filtro_min != f->filtro_mag || (f->filtro_min != GL_NEAREST && f->filtro_min != GL_LINEAR) || f->aniso > 1.0f;
        if (f->filtro_min != GL_NEAREST && f->filtro_min != GL_LINEAR) {
            if (cubo) for (int k = 0; k < 6; k++) tex_preparar_mip(&e->t->faces[k]);
            else tex_preparar_mip((textura_t *)e->t);
        }
        EN = i + 1;
    }
}

/* ¿Hace falta la tubería completa para este dibujo? (las rutas de siempre no saben) */
static int precisa_es(GLenum modo, const fonte_t *f)
{
    if (modo == GL_POINTS || C->fbo) return 1;
    if (C->alfa_on || C->est_on || C->tij_on || C->log_on || C->off_on || C->planos || C->paleta_on) return 1;
    if (!C->masc_z || (C->masc_cor & 7) != 7) return 1;
    if (C->func_z != GL_LESS && C->func_z != GL_LEQUAL && C->func_z != GL_ALWAYS) return 1;
    if (C->z_perto != 0.0f || C->z_longe != 1.0f) return 1;
    if (C->blend) {
        if (C->blend_s != C->blend_sa || C->blend_d != C->blend_da || C->blend_eq != GL_FUNC_ADD_OES || C->blend_eqa != GL_FUNC_ADD_OES)
            return 1;
        int s = C->blend_s, d = C->blend_d;
        if ((s != GL_SRC_ALPHA && s != GL_ONE_MINUS_SRC_ALPHA && s != GL_ONE && s != GL_ZERO) ||
            (d != GL_SRC_ALPHA && d != GL_ONE_MINUS_SRC_ALPHA && d != GL_ONE && d != GL_ZERO)) return 1;
    }
    if (C->linha_larg != 1.0f && modo >= GL_LINES && modo <= GL_LINE_STRIP) return 1;
    if (C->cull && C->cull_face == GL_FRONT_AND_BACK) return 1;
    if (C->dois_lados && C->luz_v7) return 1;
    const unid_t *u = &C->un[0];
    if (u->cubo_on || u->gen_on || !mat_e_identidade(u->mat[u->mat_n - 1])) return 1;
    if (u->on && u->sel >= 0 && C->tex[u->sel].usada) {
        const textura_t *t = &C->tex[u->sel];
        if (u->env_modo != GL_MODULATE || t->cubo || t->wrap_s == GL_MIRRORED_REPEAT_OES || t->wrap_t == GL_MIRRORED_REPEAT_OES ||
            t->aniso > 1.0f) return 1;
    }
    for (int i = 1; i < SGL_UNIDADES; i++) if (C->un[i].on || C->un[i].cubo_on) return 1;
    /* tipos de los arrays que las rutas de siempre no leen */
    if (f && !f->tcv && f->v && (f->v_tipo != GL_FLOAT)) return 1;
    if (f && f->tem_cor && f->c_tipo != GL_FLOAT && f->c_tipo != GL_UNSIGNED_BYTE) return 1;
    if (f && f->tem_tc && ((f->tc_tipo != GL_FLOAT && f->tc_tipo != GL_UNSIGNED_BYTE) || f->tc_tam != 2)) return 1;
    if (f && f->n && f->n_tipo != GL_FLOAT && C->luz_v7) return 1;
    return 0;
}

/* ------------------------------------------------------------ la tubería completa: vértices */

typedef struct {
    vert_t b;                     /* recorte, color (0..255), especular, niebla y ojo */
    float  ew;                    /* w en el espacio de la vista */
    float  tc[SGL_UNIDADES][4];
    float  tam;                   /* tamaño del punto (ya atenuado) */
    float  bc[4], bs[3];          /* luz de dos caras: el color de atrás */
} ves_t;

typedef struct {
    const fonte_t *f;
    const void *ps; int ps_tipo, ps_passo;
    const void *mi, *pe;
    const void *tc[SGL_UNIDADES]; int tc_tam[SGL_UNIDADES], tc_tipo[SGL_UNIDADES], tc_passo[SGL_UNIDADES];
} fonte_es_t;

static void es_luz_olho(vert_t *o, float nx, float ny, float nz);   /* sgl.c */

/* D451: lo que es igual para todos los vértices de un dibujo (lo calcula desenhar_es): si hace falta la normal
 * (luces o generación de coordenadas) y si la matriz de textura de cada unidad es la identidad. */
static int es_quer_normal = 1;
static int es_tmat_id[SGL_UNIDADES];

static void es_buscar(const fonte_es_t *F, int i, ves_t *o)
{
    const fonte_t *f = F->f;
    vert_t *b = &o->b;
    float m[4] = { 0, 0, 0, 1 };
    const char *pv = (const char *)f->v + (size_t)i * f->v_passo;
    if (f->v_tipo == GL_FLOAT) { const float *q = (const float *)pv; m[0] = q[0]; m[1] = q[1]; if (f->v_tam > 2) m[2] = q[2]; if (f->v_tam > 3) m[3] = q[3]; }
    else for (int k = 0; k < f->v_tam; k++) m[k] = es_ler(pv, (GLenum)f->v_tipo, k, 0);
    /* normal del objeto (D451: solo si alguien la usa) */
    float n[3] = { C->normal[0], C->normal[1], C->normal[2] };
    if (es_quer_normal) {
        if (f->nv) { const float *q = f->nv + (size_t)i * 3; n[0] = q[0]; n[1] = q[1]; n[2] = q[2]; }
        else if (f->n) { const char *q = f->n + (size_t)i * f->n_passo; for (int k = 0; k < 3; k++) n[k] = es_ler(q, (GLenum)f->n_tipo, k, 1); }
    }
    float e[4], ne[3] = { 0, 0, 0 };
    if (C->paleta_on && F->mi && F->pe) {
        /* OES_matrix_palette: suma de las matrices de la paleta por sus pesos */
        const uint8_t *ix = (const uint8_t *)F->mi + (size_t)i * C->mi_passo;
        const char *pw = (const char *)F->pe + (size_t)i * C->pe_passo;
        e[0] = e[1] = e[2] = e[3] = 0; ne[0] = ne[1] = ne[2] = 0;
        int nu = C->pe_tam < C->mi_tam ? C->pe_tam : C->mi_tam;
        for (int k = 0; k < nu; k++) {
            float w = es_ler(pw, (GLenum)C->pe_tipo, k, 0);
            const float *M = C->paleta[ix[k] & 31];
            for (int l = 0; l < 4; l++) e[l] += w * (M[l] * m[0] + M[4 + l] * m[1] + M[8 + l] * m[2] + M[12 + l] * m[3]);
            /* normal: cofactores del 3x3 (la inversa traspuesta salvo escala) */
            float a = M[0], bb = M[4], c = M[8], d = M[1], ee = M[5], ff = M[9], g = M[2], h = M[6], kk = M[10];
            float nm[9] = { ee * kk - ff * h, -(d * kk - ff * g), d * h - ee * g,
                            -(bb * kk - c * h), a * kk - c * g, -(a * h - bb * g),
                            bb * ff - c * ee, -(a * ff - c * d), a * ee - bb * d };
            float det = a * nm[0] + bb * nm[1] + c * nm[2];
            if (det < 0.0f) for (int q = 0; q < 9; q++) nm[q] = -nm[q];
            ne[0] += w * (nm[0] * n[0] + nm[3] * n[1] + nm[6] * n[2]);
            ne[1] += w * (nm[1] * n[0] + nm[4] * n[1] + nm[7] * n[2]);
            ne[2] += w * (nm[2] * n[0] + nm[5] * n[1] + nm[8] * n[2]);
        }
    } else {
        const float *M = C->mv[C->mv_n - 1];
        for (int l = 0; l < 4; l++) e[l] = M[l] * m[0] + M[4 + l] * m[1] + M[8 + l] * m[2] + M[12 + l] * m[3];
        if (es_quer_normal) {
            const float *q = C->nm;
            ne[0] = q[0] * n[0] + q[3] * n[1] + q[6] * n[2];
            ne[1] = q[1] * n[0] + q[4] * n[1] + q[7] * n[2];
            ne[2] = q[2] * n[0] + q[5] * n[1] + q[8] * n[2];
        }
    }
    if (es_quer_normal) {
        float ln = raiz(ne[0] * ne[0] + ne[1] * ne[1] + ne[2] * ne[2]);
        if (ln > 0.0f) { ln = 1.0f / ln; ne[0] *= ln; ne[1] *= ln; ne[2] *= ln; }
    }
    const float *P = C->proj[C->proj_n - 1];
    b->x = P[0] * e[0] + P[4] * e[1] + P[8] * e[2] + P[12] * e[3];
    b->y = P[1] * e[0] + P[5] * e[1] + P[9] * e[2] + P[13] * e[3];
    b->z = P[2] * e[0] + P[6] * e[1] + P[10] * e[2] + P[14] * e[3];
    b->w = P[3] * e[0] + P[7] * e[1] + P[11] * e[2] + P[15] * e[3];
    b->ex = e[0]; b->ey = e[1]; b->ez = e[2]; o->ew = e[3];
    /* color */
    b->a = 255.0f;
    if (f->tem_cor) {
        const char *q = (const char *)f->c + (size_t)i * f->c_passo;
        for (int k = 0; k < 4 && k < f->c_tam; k++) {
            float c = f->c_tipo == GL_UNSIGNED_BYTE ? (float)((const uint8_t *)q)[k] : es_ler(q, (GLenum)f->c_tipo, k, 1) * 255.0f;
            if (c < 0) c = 0;
            if (c > 255) c = 255;
            if (k < 3) (&b->r)[k] = c; else b->a = c;
        }
    } else { b->r = f->cor[0]; b->g = f->cor[1]; b->b = f->cor[2]; b->a = f->cor[3]; }
    b->luz = 255.0f;
    b->sr = b->sg = b->sb = 0.0f; b->ff = 1.0f;
    b->u = b->v = 0.0f;
    if (C->luz_v7) {
        vert_t frente = *b;
        if (C->dois_lados) {
            vert_t atras = *b;
            es_luz_olho(&atras, -ne[0], -ne[1], -ne[2]);
            o->bc[0] = atras.r; o->bc[1] = atras.g; o->bc[2] = atras.b; o->bc[3] = atras.a;
            o->bs[0] = atras.sr; o->bs[1] = atras.sg; o->bs[2] = atras.sb;
        }
        es_luz_olho(&frente, ne[0], ne[1], ne[2]);
        *b = frente;
    }
    if (C->fog && C->fog_modo) b->ff = fator_neblina(raiz(e[0] * e[0] + e[1] * e[1] + e[2] * e[2]));
    /* coordenadas de textura de cada unidad que cuenta */
    for (int u = 0; u < EN; u++) {
        float *t = o->tc[u];
        const unid_t *un = &C->un[u];
        t[0] = un->tc[0]; t[1] = un->tc[1]; t[2] = un->tc[2]; t[3] = un->tc[3];
        if (!EU[u].t) continue;
        if (u == 0 && f->tcv) { t[0] = f->tcv[i * 2]; t[1] = f->tcv[i * 2 + 1]; t[2] = 0.0f; t[3] = 1.0f; }
        else if (F->tc[u]) {
            const char *q = (const char *)F->tc[u] + (size_t)i * F->tc_passo[u];
            t[2] = 0.0f; t[3] = 1.0f;
            if (F->tc_tipo[u] == GL_FLOAT) { const float *qf = (const float *)q; for (int k = 0; k < F->tc_tam[u]; k++) t[k] = qf[k]; }   /* D451 */
            else if (F->tc_tipo[u] == GL_UNSIGNED_BYTE) for (int k = 0; k < F->tc_tam[u]; k++) t[k] = (float)((const uint8_t *)q)[k] * (1.0f / 255.0f);
            else for (int k = 0; k < F->tc_tam[u]; k++) t[k] = es_ler(q, (GLenum)F->tc_tipo[u], k, 0);
        }
        if (un->gen_on) {
            if (un->gen_modo == GL_NORMAL_MAP_OES) { t[0] = ne[0]; t[1] = ne[1]; t[2] = ne[2]; }
            else {
                float ux = e[0], uy = e[1], uz = e[2], lu = raiz(ux * ux + uy * uy + uz * uz);
                if (lu > 0.0f) { lu = 1.0f / lu; ux *= lu; uy *= lu; uz *= lu; }
                float d = 2.0f * (ne[0] * ux + ne[1] * uy + ne[2] * uz);
                t[0] = ux - ne[0] * d; t[1] = uy - ne[1] * d; t[2] = uz - ne[2] * d;
            }
            t[3] = 1.0f;
        }
        const float *T = un->mat[un->mat_n - 1];
        if (!es_tmat_id[u]) {
            float r[4];
            for (int l = 0; l < 4; l++) r[l] = T[l] * t[0] + T[4 + l] * t[1] + T[8 + l] * t[2] + T[12 + l] * t[3];
            for (int l = 0; l < 4; l++) t[l] = r[l];
        }
    }
    /* tamaño del punto */
    float tam = C->ponto_tam;
    if (F->ps) tam = es_ler((const char *)F->ps + (size_t)i * F->ps_passo, (GLenum)F->ps_tipo, 0, 0);
    o->tam = tam;
}

/* ------------------------------------------------------------ la tubería completa: fragmentos */

static uint8_t *es_ebuf(void)
{
    if (!C->ebuf && !C->fbo && !C->sem_estencil) {
        C->ebuf = (uint8_t *)SGL_ALOCAR((size_t)C->larg * C->alt);
        if (C->ebuf) { uint8_t *e = C->ebuf; for (int i = 0; i < C->larg * C->alt; i++) e[i] = (uint8_t)C->est_limpo; }
    }
    return C->ebuf;
}

static inline unsigned es_est_op(int op, unsigned v)
{
    switch (op) {
    case GL_ZERO: return 0;
    case GL_REPLACE: return (unsigned)C->est_ref;
    case GL_INCR: return v < 255 ? v + 1 : 255;
    case GL_DECR: return v > 0 ? v - 1 : 0;
    case GL_INVERT: return ~v & 0xFF;
    default: return v;
    }
}

static inline void es_est_escrever(uint8_t *e, int op)
{
    unsigned n = es_est_op(op, *e), m = C->est_escr & 0xFF;
    *e = (uint8_t)((*e & ~m) | (n & m));
}

static inline float es_fator(int f, const float *s, const float *d, int k)
{
    switch (f) {
    case GL_ZERO: return 0.0f;
    case GL_ONE: return 1.0f;
    case GL_SRC_COLOR: return s[k];
    case GL_ONE_MINUS_SRC_COLOR: return 1.0f - s[k];
    case GL_DST_COLOR: return d[k];
    case GL_ONE_MINUS_DST_COLOR: return 1.0f - d[k];
    case GL_SRC_ALPHA: return s[3];
    case GL_ONE_MINUS_SRC_ALPHA: return 1.0f - s[3];
    case GL_DST_ALPHA: return d[3];
    case GL_ONE_MINUS_DST_ALPHA: return 1.0f - d[3];
    case GL_SRC_ALPHA_SATURATE: { if (k == 3) return 1.0f; float a = 1.0f - d[3]; return s[3] < a ? s[3] : a; }
    default: return 1.0f;
    }
}

static inline float es_equacao(int eq, float s, float d)
{
    float r = eq == GL_FUNC_SUBTRACT_OES ? s - d : eq == GL_FUNC_REVERSE_SUBTRACT_OES ? d - s : s + d;
    return r < 0.0f ? 0.0f : r > 1.0f ? 1.0f : r;
}

static inline uint32_t es_logica(int op, uint32_t s, uint32_t d)
{
    switch (op) {
    case GL_CLEAR: return 0;
    case GL_AND: return s & d;
    case GL_AND_REVERSE: return s & ~d;
    case GL_COPY: return s;
    case GL_AND_INVERTED: return ~s & d;
    case GL_NOOP: return d;
    case GL_XOR: return s ^ d;
    case GL_OR: return s | d;
    case GL_NOR: return ~(s | d);
    case GL_EQUIV: return ~(s ^ d);
    case GL_INVERT: return ~d;
    case GL_OR_REVERSE: return s | ~d;
    case GL_COPY_INVERTED: return ~s;
    case GL_OR_INVERTED: return ~s | d;
    case GL_NAND: return ~(s & d);
    default: return 0xFFFFFFFFu;
    }
}

static inline int es_byte(float v) { int r = (int)(v * 255.0f + 0.5f); return r < 0 ? 0 : r > 255 ? 255 : r; }

/* Un fragmento ya sombreado (c[] de 0 a 1): prueba alfa, esténcil, Z, mezcla o
 * lógica, máscaras y escritura. z como en las otras rutas (31 bits). La tijera
 * ya la recortó quien llama. */
static void es_fragmento(int x, int y, double z, const float *c)
{
    if (C->alfa_on && !es_comparar(C->alfa_func, c[3], C->alfa_ref)) return;
    int i = y * C->larg + x;
    uint8_t *eb = C->est_on ? es_ebuf() : 0;
    uint8_t *e = eb ? eb + i : 0;
    if (e) {
        unsigned m = C->est_masc & 0xFF;
        if (!es_comparar(C->est_func, (double)((unsigned)C->est_ref & m), (double)(*e & m))) { es_est_escrever(e, C->est_falha); return; }
    }
    if (C->teste_z && C->zbuf) {
        if (z < 0) z = 0;
        if (z > 0x7FFFFFFE) z = 0x7FFFFFFE;
        if (C->zbits == 32) {
            uint32_t *zb = C->zbuf, zi = (uint32_t)z;
            if (!es_comparar(C->func_z, (double)zi, (double)zb[i])) { if (e) es_est_escrever(e, C->est_zfalha); return; }
            if (C->masc_z) zb[i] = zi;
        } else {
            uint16_t *zb = C->zbuf;
            uint32_t zi = (uint32_t)deszc(z, 15);
            if (zi > 0xFFFE) zi = 0xFFFE;
            if (!es_comparar(C->func_z, (double)zi, (double)zb[i])) { if (e) es_est_escrever(e, C->est_zfalha); return; }
            if (C->masc_z) zb[i] = (uint16_t)zi;
        }
    }
    if (e) es_est_escrever(e, C->est_zpassa);
    uint32_t dv = C->px[i];
    int r, g, b, a;
    if (C->log_on) {
        uint32_t sv = ((uint32_t)es_byte(c[3]) << 24) | ((uint32_t)es_byte(c[0]) << 16) | ((uint32_t)es_byte(c[1]) << 8) | (uint32_t)es_byte(c[2]);
        uint32_t da = C->alvo_alfa ? dv : (dv | 0xFF000000u);
        uint32_t rv = es_logica(C->log_op, sv, da);
        r = (int)((rv >> 16) & 0xFF); g = (int)((rv >> 8) & 0xFF); b = (int)(rv & 0xFF); a = (int)(rv >> 24);
    } else if (C->blend) {
        float d[4] = { (float)((dv >> 16) & 0xFF) / 255.0f, (float)((dv >> 8) & 0xFF) / 255.0f, (float)(dv & 0xFF) / 255.0f,
                       C->alvo_alfa ? (float)(dv >> 24) / 255.0f : 1.0f };
        float o[4];
        for (int k = 0; k < 3; k++) o[k] = es_equacao(C->blend_eq, c[k] * es_fator(C->blend_s, c, d, k), d[k] * es_fator(C->blend_d, c, d, k));
        o[3] = es_equacao(C->blend_eqa, c[3] * es_fator(C->blend_sa, c, d, 3), d[3] * es_fator(C->blend_da, c, d, 3));
        r = es_byte(o[0]); g = es_byte(o[1]); b = es_byte(o[2]); a = es_byte(o[3]);
    } else { r = es_byte(c[0]); g = es_byte(c[1]); b = es_byte(c[2]); a = es_byte(c[3]); }
    if (C->masc_cor != 15) {
        if (!(C->masc_cor & 1)) r = (int)((dv >> 16) & 0xFF);
        if (!(C->masc_cor & 2)) g = (int)((dv >> 8) & 0xFF);
        if (!(C->masc_cor & 4)) b = (int)(dv & 0xFF);
        if (!(C->masc_cor & 8)) a = (int)(dv >> 24);
    }
    C->px[i] = (C->alvo_alfa ? (uint32_t)a << 24 : 0u) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

/* ------------------------------------------------------------ la tubería completa: texturas y combinación */

/* Una textura 2D (o una cara) con su filtro: u, v normalizadas y sus derivadas por píxel. */
static void es_amostra_2d(const textura_t *t, float u, float v, float dux, float dvx, float duy, float dvy, int lod, int *c)
{
    if (!lod) { amostra7(t, u, v, 0.0f, c); return; }
    float tl = (float)t->larg, ta = (float)t->alt;
    dux *= tl; duy *= tl; dvx *= ta; dvy *= ta;
    float rx = dux * dux + dvx * dvx, ry = duy * duy + dvy * dvy;
    float mx = rx > ry ? rx : ry, mn = rx > ry ? ry : rx;
    int n = 1;
    if (t->aniso > 1.0f && mn > 0.0f && mx > 1.0f) {
        /* EXT_texture_filter_anisotropic, barato: hasta 4 muestras por el eje largo */
        float r2 = mx / mn, lim = t->aniso * t->aniso;
        if (r2 > lim) r2 = lim;
        n = r2 >= 9.0f ? 4 : r2 >= 4.0f ? 3 : r2 >= 2.0f ? 2 : 1;
        if (n > (int)t->aniso) n = (int)t->aniso;
        if (n > 1) { mx = mx / (float)(n * n); if (mx < mn) mx = mn; }
    }
    float l = mx > 1.0f ? 0.5f * log2_aprox(mx) : -1.0f;
    if (n == 1) { amostra7(t, u, v, l, c); return; }
    float au = rx > ry ? dux / tl : duy / tl, av = rx > ry ? dvx / ta : dvy / ta;
    int s[4] = { 0, 0, 0, 0 }, q[4];
    for (int k = 0; k < n; k++) {
        float f = ((float)k + 0.5f) / (float)n - 0.5f;
        amostra7(t, u + au * f, v + av * f, l, q);
        for (int j = 0; j < 4; j++) s[j] += q[j];
    }
    for (int j = 0; j < 4; j++) c[j] = (s[j] + n / 2) / n;
}

/* La cara y (s, t) de una dirección; también la derivada de (s, t) si se pide. */
static int es_face_cubo(float x, float y, float z, float *sc, float *tc, float *ma)
{
    float ax = x < 0 ? -x : x, ay = y < 0 ? -y : y, az = z < 0 ? -z : z;
    int f;
    if (ax >= ay && ax >= az) { f = x >= 0 ? 0 : 1; *ma = ax; *sc = x >= 0 ? -z : z; *tc = -y; }
    else if (ay >= az) { f = y >= 0 ? 2 : 3; *ma = ay; *sc = x; *tc = y >= 0 ? z : -z; }
    else { f = z >= 0 ? 4 : 5; *ma = az; *sc = z >= 0 ? x : -x; *tc = -y; }
    return f;
}

/* La muestra de la unidad e en c[] (0..1). a = (s, t, r, q) divididos por w; gx, gy sus gradientes por píxel. */
static void es_texel(const es_unid_t *e, const float *a, const float *gx, const float *gy, float *c)
{
    int q[4];
    if (e->cubo) {
        float sc, tc, ma;
        int f = es_face_cubo(a[0], a[1], a[2], &sc, &tc, &ma);
        if (!(ma > 0.0f)) { c[0] = c[1] = c[2] = c[3] = 0.0f; return; }
        float im = 1.0f / ma, s = (sc * im + 1.0f) * 0.5f, t = (tc * im + 1.0f) * 0.5f;
        const textura_t *ft = &e->t->faces[f];
        float dsx = 0, dtx = 0, dsy = 0, dty = 0;
        if (e->lod && gx) {
            /* diferencias finitas con la dirección del píxel vecino (el w se cancela en sc/ma) */
            const float *G[2] = { gx, gy };
            float d[2][2];
            for (int k = 0; k < 2; k++) {
                float s2, t2, m2;
                es_face_cubo(a[0] + G[k][0], a[1] + G[k][1], a[2] + G[k][2], &s2, &t2, &m2);
                float i2 = m2 > 0.0f ? 1.0f / m2 : 0.0f;
                d[k][0] = (s2 * i2 - sc * im) * 0.5f; d[k][1] = (t2 * i2 - tc * im) * 0.5f;
            }
            dsx = d[0][0]; dtx = d[0][1]; dsy = d[1][0]; dty = d[1][1];
        }
        es_amostra_2d(ft, s, t, dsx, dtx, dsy, dty, e->lod && gx, q);
    } else {
        float qq = a[3];
        if (!(qq > 1.0e-20f || qq < -1.0e-20f)) qq = 1.0e-20f;
        float iq = 1.0f / qq, u = a[0] * iq, v = a[1] * iq;
        float dux = 0, dvx = 0, duy = 0, dvy = 0;
        if (e->lod && gx) {
            dux = (gx[0] - u * gx[3]) * iq; dvx = (gx[1] - v * gx[3]) * iq;
            duy = (gy[0] - u * gy[3]) * iq; dvy = (gy[1] - v * gy[3]) * iq;
        }
        es_amostra_2d(e->t, u, v, dux, dvx, duy, dvy, e->lod && gx, q);
    }
    const float i255 = 1.0f / 255.0f;
    c[0] = (float)q[0] * i255; c[1] = (float)q[1] * i255; c[2] = (float)q[2] * i255; c[3] = (float)q[3] * i255;
    if (e->tex_cor_zero) c[0] = c[1] = c[2] = 0.0f;
}

static inline const float *es_fonte(int s, const float *tex, const float *ant, const float *prim, const float *cc)
{
    return s == GL_TEXTURE ? tex : s == GL_CONSTANT ? cc : s == GL_PRIMARY_COLOR ? prim : ant;
}

/* GL_COMBINE de una unidad: o[] = la salida (0..1). */
static void es_combinar(const es_unid_t *e, const float *tex, const float *ant, const float *prim, float *o)
{
    float A[3][3], Aa[3];
    for (int k = 0; k < 3; k++) {
        const float *s = es_fonte(e->rgb_src[k], tex, ant, prim, e->cc);
        switch (e->rgb_opd[k]) {
        case GL_SRC_COLOR: A[k][0] = s[0]; A[k][1] = s[1]; A[k][2] = s[2]; break;
        case GL_ONE_MINUS_SRC_COLOR: A[k][0] = 1.0f - s[0]; A[k][1] = 1.0f - s[1]; A[k][2] = 1.0f - s[2]; break;
        case GL_SRC_ALPHA: A[k][0] = A[k][1] = A[k][2] = s[3]; break;
        default: A[k][0] = A[k][1] = A[k][2] = 1.0f - s[3]; break;
        }
        const float *sa = es_fonte(e->alfa_src[k], tex, ant, prim, e->cc);
        Aa[k] = e->alfa_opd[k] == GL_SRC_ALPHA ? sa[3] : 1.0f - sa[3];
    }
    int dot = 0;
    for (int k = 0; k < 3; k++) {
        float r;
        switch (e->rgb_f) {
        case GL_REPLACE: r = A[0][k]; break;
        case GL_MODULATE: r = A[0][k] * A[1][k]; break;
        case GL_ADD: r = A[0][k] + A[1][k]; break;
        case GL_ADD_SIGNED: r = A[0][k] + A[1][k] - 0.5f; break;
        case GL_INTERPOLATE: r = A[0][k] * A[2][k] + A[1][k] * (1.0f - A[2][k]); break;
        case GL_SUBTRACT: r = A[0][k] - A[1][k]; break;
        default:
            r = 4.0f * ((A[0][0] - 0.5f) * (A[1][0] - 0.5f) + (A[0][1] - 0.5f) * (A[1][1] - 0.5f) + (A[0][2] - 0.5f) * (A[1][2] - 0.5f));
            dot = 1;
            break;
        }
        r *= e->esc_rgb;
        o[k] = r < 0.0f ? 0.0f : r > 1.0f ? 1.0f : r;
    }
    if (dot && e->rgb_f == GL_DOT3_RGBA) { o[3] = o[0]; return; }
    float r;
    switch (e->alfa_f) {
    case GL_REPLACE: r = Aa[0]; break;
    case GL_MODULATE: r = Aa[0] * Aa[1]; break;
    case GL_ADD: r = Aa[0] + Aa[1]; break;
    case GL_ADD_SIGNED: r = Aa[0] + Aa[1] - 0.5f; break;
    case GL_INTERPOLATE: r = Aa[0] * Aa[2] + Aa[1] * (1.0f - Aa[2]); break;
    default: r = Aa[0] - Aa[1]; break;
    }
    r *= e->esc_alfa;
    o[3] = r < 0.0f ? 0.0f : r > 1.0f ? 1.0f : r;
}

/* Atributos de un fragmento para sombrearlo. */
typedef struct {
    float cor[4];            /* primario, 0..1 */
    float esp[3];            /* especular, 0..1 */
    float ff;                /* factor de niebla (1 = nada) */
    float zn;                /* z de la ventana 0..1 (niebla de siempre) */
    float tc[SGL_UNIDADES][4];
    float gx[SGL_UNIDADES][4], gy[SGL_UNIDADES][4];
    int   com_grad;
} frag_t;

static void es_sombrear(const frag_t *p, float *o)
{
    float ant[4] = { p->cor[0], p->cor[1], p->cor[2], p->cor[3] };
    for (int u = 0; u < EN; u++) {
        const es_unid_t *e = &EU[u];
        if (!e->t) continue;
        float tex[4], r[4];
        es_texel(e, p->tc[u], p->com_grad ? p->gx[u] : 0, p->com_grad ? p->gy[u] : 0, tex);
        es_combinar(e, tex, ant, p->cor, r);
        ant[0] = r[0]; ant[1] = r[1]; ant[2] = r[2]; ant[3] = r[3];
    }
    for (int k = 0; k < 3; k++) { float v = ant[k] + p->esp[k]; o[k] = v > 1.0f ? 1.0f : v; }
    o[3] = ant[3];
    if (C->fog) {
        float f = C->fog_modo ? p->ff : (C->fog_fim - p->zn) / (C->fog_fim - C->fog_ini);
        f = sat01(f);
        for (int k = 0; k < 3; k++) o[k] = o[k] * f + C->fog_cor[k] * (1.0f / 255.0f) * (1.0f - f);
    }
}

/* ------------------------------------------------------------ la tubería completa: rasterizado */

#define NAT_ES (8 + 4 * SGL_UNIDADES)   /* r g b a, sr sg sb, ff y (s, t, r, q) por unidad; divididos por w */
typedef struct { int X, Y; double z; float q; float at[NAT_ES]; } sves_t;

/* La caja de píxeles válida con la tijera. */
static void caixa_es(int *x0, int *y0, int *x1, int *y1, int viewport)
{
    if (viewport) caixa(x0, y0, x1, y1);
    else { *x0 = 0; *y0 = 0; *x1 = C->larg; *y1 = C->alt; }
    if (C->tij_on) {
        int tx0 = C->tij_x, tx1 = C->tij_x + C->tij_l;
        int ty0 = C->alt - (C->tij_y + C->tij_a), ty1 = C->alt - C->tij_y;
        if (*x0 < tx0) *x0 = tx0;
        if (*x1 > tx1) *x1 = tx1;
        if (*y0 < ty0) *y0 = ty0;
        if (*y1 > ty1) *y1 = ty1;
    }
}

static float es_zjanela(float nz)
{
    float zw = (nz + 1.0f) * 0.5f;
    if (C->z_perto != 0.0f || C->z_longe != 1.0f) zw = C->z_perto + (C->z_longe - C->z_perto) * zw;
    if (zw < 0) zw = 0;
    if (zw > 1) zw = 1;
    return zw;
}

static int NATU;   /* atributos que se interpolan en este dibujo */

static void a_pantalha_es(const ves_t *o, sves_t *s)
{
    const vert_t *v = &o->b;
    float iw = 1.0f / v->w;
    float nx = v->x * iw, ny = v->y * iw, nz = v->z * iw;
    float sx = (float)C->vx + (nx + 1.0f) * 0.5f * (float)C->vw;
    float sy = (float)(C->alt - C->vy) - (ny + 1.0f) * 0.5f * (float)C->vh;
    s->X = arred16(sx);
    s->Y = arred16(sy);
    s->z = (double)(int)(es_zjanela(nz) * 2147483520.0f);
    float q = iw;
    if (!(q > 1.0e-12f)) q = 1.0e-12f;
    if (q > 1.0e12f) q = 1.0e12f;
    float r = v->r, g = v->g, b = v->b;
    if (C->luz_on && !C->luz_v7) {
        float m = v->luz * (1.0f / 255.0f);
        r *= m * C->luz_dif[0] + C->luz_amb[0];
        g *= m * C->luz_dif[1] + C->luz_amb[1];
        b *= m * C->luz_dif[2] + C->luz_amb[2];
    }
    s->q = q;
    s->at[0] = r * q; s->at[1] = g * q; s->at[2] = b * q; s->at[3] = v->a * q;
    s->at[4] = v->sr * q; s->at[5] = v->sg * q; s->at[6] = v->sb * q; s->at[7] = v->ff * q;
    for (int u = 0; u < EN; u++) for (int k = 0; k < 4; k++) s->at[8 + u * 4 + k] = o->tc[u][k] * q;
}

static void triangulo_es(sves_t v[3])
{
    double area = (double)(v[1].X - v[0].X) * (double)(v[2].Y - v[0].Y)
                - (double)(v[2].X - v[0].X) * (double)(v[1].Y - v[0].Y);
    if (area == 0) return;
    int frente = C->frente == GL_CCW ? area < 0 : area > 0;
    if (C->cull) {
        if (C->cull_face == GL_BACK && !frente) return;
        if (C->cull_face == GL_FRONT && frente) return;
        if (C->cull_face == GL_FRONT_AND_BACK) return;
    }
    if (area < 0) { sves_t t = v[1]; v[1] = v[2]; v[2] = t; area = -area; }
    int cx0, cy0, cx1, cy1;
    caixa_es(&cx0, &cy0, &cx1, &cy1, 1);
    int minX = v[0].X, maxX = v[0].X, minY = v[0].Y, maxY = v[0].Y;
    for (int i = 1; i < 3; i++) {
        if (v[i].X < minX) minX = v[i].X;
        if (v[i].X > maxX) maxX = v[i].X;
        if (v[i].Y < minY) minY = v[i].Y;
        if (v[i].Y > maxY) maxY = v[i].Y;
    }
    int bx0 = minX >> 4, bx1 = maxX >> 4, by0 = minY >> 4, by1 = maxY >> 4;
    if (bx0 < cx0) bx0 = cx0;
    if (by0 < cy0) by0 = cy0;
    if (bx1 > cx1 - 1) bx1 = cx1 - 1;
    if (by1 > cy1 - 1) by1 = cy1 - 1;
    if (bx0 > bx1 || by0 > by1) return;
    double e0[3], ex[3], ey[3];
    double Px = (double)bx0 * 16.0 + 8.0, Py = (double)by0 * 16.0 + 8.0;
    for (int i = 0; i < 3; i++) {
        const sves_t *a = &v[(i + 1) % 3], *b = &v[(i + 2) % 3];
        int dx = b->X - a->X, dy = b->Y - a->Y;
        e0[i] = (double)dx * (Py - (double)a->Y) - (double)dy * (Px - (double)a->X)
              - (canto_superior_esquerdo(dx, dy) ? 0.0 : 1.0);
        ex[i] = -(double)dy * 16.0;
        ey[i] = (double)dx * 16.0;
    }
    double gzx, gzy, z0;
    {
        double d1 = v[1].z - v[0].z, d2 = v[2].z - v[0].z;
        double nx = d1 * (double)(v[2].Y - v[0].Y) - d2 * (double)(v[1].Y - v[0].Y);
        double ny = d2 * (double)(v[1].X - v[0].X) - d1 * (double)(v[2].X - v[0].X);
        gzx = trun(nx * 65536.0 / area);
        gzy = trun(ny * 65536.0 / area);
        z0 = v[0].z * 65536.0 + gzx * (Px - (double)v[0].X) + gzy * (Py - (double)v[0].Y);
    }
    if (C->off_on) {
        /* glPolygonOffset: fator × pendiente máxima + unidades × r (r = el paso del Z guardado) */
        double m = (gzx < 0 ? -gzx : gzx) > (gzy < 0 ? -gzy : gzy) ? (gzx < 0 ? -gzx : gzx) : (gzy < 0 ? -gzy : gzy);
        m = m * 16.0 / 65536.0;
        double r = C->zbits == 16 ? 32768.0 : 128.0;
        z0 += ((double)C->off_fator * m + (double)C->off_unid * r) * 65536.0;
    }
    float x1 = (float)(v[1].X - v[0].X) * 0.0625f, y1 = (float)(v[1].Y - v[0].Y) * 0.0625f;
    float x2 = (float)(v[2].X - v[0].X) * 0.0625f, y2 = (float)(v[2].Y - v[0].Y) * 0.0625f;
    float af = x1 * y2 - x2 * y1;
    if (!(af > 0.0f)) return;
    float ia = 1.0f / af;
    float ox = (float)bx0 + 0.5f - (float)v[0].X * 0.0625f, oy = (float)by0 + 0.5f - (float)v[0].Y * 0.0625f;
    float gqx, gqy, q0, gx[NAT_ES], gy[NAT_ES], a0[NAT_ES];
    {
        float d1 = v[1].q - v[0].q, d2 = v[2].q - v[0].q;
        gqx = (d1 * y2 - d2 * y1) * ia; gqy = (d2 * x1 - d1 * x2) * ia;
        q0 = v[0].q + gqx * ox + gqy * oy;
    }
    int na = NATU;
    for (int k = 0; k < na; k++) {
        float d1 = v[1].at[k] - v[0].at[k], d2 = v[2].at[k] - v[0].at[k];
        gx[k] = (d1 * y2 - d2 * y1) * ia; gy[k] = (d2 * x1 - d1 * x2) * ia;
        a0[k] = v[0].at[k] + gx[k] * ox + gy[k] * oy;
    }
    frag_t fr;
    fr.com_grad = 0;
    for (int u = 0; u < EN; u++) if (EU[u].lod) fr.com_grad = 1;
    if (fr.com_grad)
        for (int u = 0; u < EN; u++) for (int k = 0; k < 4; k++) { fr.gx[u][k] = gx[8 + u * 4 + k]; fr.gy[u][k] = gy[8 + u * 4 + k]; }
    const float i255 = 1.0f / 255.0f;
    for (int y = by0; y <= by1; y++) {
        float fyo = (float)(y - by0);
        double E0 = e0[0], E1 = e0[1], E2 = e0[2];
        double Z = z0 + gzy * 16.0 * (double)(y - by0);
        float Q = q0 + gqy * fyo, A[NAT_ES];
        for (int k = 0; k < na; k++) A[k] = a0[k] + gy[k] * fyo;
        for (int x = bx0; x <= bx1; x++) {
            if (E0 >= 0 && E1 >= 0 && E2 >= 0) {
                float q = Q > 1.0e-12f ? Q : 1.0e-12f;
                float w = 1.0f / q;
                for (int k = 0; k < 4; k++) fr.cor[k] = A[k] * w * i255;
                for (int k = 0; k < 3; k++) fr.esp[k] = A[4 + k] * w * i255;
                fr.ff = A[7] * w;
                double z = deszc(Z, 16);
                fr.zn = (float)(z * (1.0 / 2147483520.0));
                for (int u = 0; u < EN; u++) for (int k = 0; k < 4; k++) fr.tc[u][k] = A[8 + u * 4 + k];
                float c[4];
                es_sombrear(&fr, c);
                es_fragmento(x, y, z, c);
            }
            E0 += ex[0]; E1 += ex[1]; E2 += ex[2];
            Z += gzx * 16.0;
            Q += gqx;
            for (int k = 0; k < na; k++) A[k] += gx[k];
        }
        e0[0] += ey[0]; e0[1] += ey[1]; e0[2] += ey[2];
    }
}

/* ------------------------------------------------------------ la tubería completa: primitivas y recorte */

#define ES_POL_MAX 16   /* 3 vértices + 6 planos del frustum + 6 del programa (y margen) */

static void interpolar_es(ves_t *r, const ves_t *a, const ves_t *b, float t)
{
    interpolar(&r->b, &a->b, &b->b, t);
    r->ew = a->ew + (b->ew - a->ew) * t;
    for (int u = 0; u < EN; u++) for (int k = 0; k < 4; k++) r->tc[u][k] = a->tc[u][k] + (b->tc[u][k] - a->tc[u][k]) * t;
    r->tam = a->tam + (b->tam - a->tam) * t;
    for (int k = 0; k < 4; k++) r->bc[k] = a->bc[k] + (b->bc[k] - a->bc[k]) * t;
    for (int k = 0; k < 3; k++) r->bs[k] = a->bs[k] + (b->bs[k] - a->bs[k]) * t;
}

/* Distancia firmada a un plano: 0-5 los del frustum, 6-11 los del programa (en la vista). */
static inline float dist_plano_es(const ves_t *v, int p)
{
    if (p < 6) return dist_plano(&v->b, p);
    const float *q = C->plano_cl[p - 6];
    return q[0] * v->b.ex + q[1] * v->b.ey + q[2] * v->b.ez + q[3] * v->ew;
}

static int es_n_planos(void) { return C->planos ? 12 : 6; }
static int es_plano_ativo(int p) { return p < 6 || (C->planos & (1 << (p - 6))); }

#ifdef SGL_TEM_GPU
static int  es_gpu_escolher(GLenum modo);
static void es_gpu_a_software(void);
static int  es_gpu_modo;     /* este dibujo: 0 software, 1 GPU v1, 2 GPU v2 */
static void es_gpu_poligono(const ves_t *p, int n);
static void es_gpu_ponto(const ves_t *o, float sx, float sy, float zw, float tam, float alfa);
static void es_gpu_linha(const ves_t *a, const ves_t *b);
static void es_gpu_poligono2(const ves_t *p, int n);
static void es_gpu_ponto2(const ves_t *o, float sx, float sy, float zw, float tam, float alfa);
static void es_gpu_linha2(const ves_t *a, const ves_t *b);
#endif

static void es_poligono(ves_t *pol0, int n0)
{
    static ves_t pol[2][ES_POL_MAX];
    int n = n0, cur = 0, np = es_n_planos();
    /* D451: con todos los vértices dentro de todos los planos no hay que recortar (y nada se copia); con todos fuera
     * de un mismo plano no queda nada. Da lo mismo que el recorte de abajo (dentro es d >= 0). */
    unsigned todos = ~0u, alguno = 0;
    for (int i = 0; i < n; i++) {
        unsigned c = 0;
        for (int p = 0; p < np; p++) if (es_plano_ativo(p) && !(dist_plano_es(&pol0[i], p) >= 0)) c |= 1u << p;
        todos &= c; alguno |= c;
    }
    if (n < 3 || todos) return;
    ves_t *fin = pol0;
    if (alguno) {
        for (int i = 0; i < n; i++) pol[0][i] = pol0[i];
        for (int p = 0; p < np && n >= 3; p++) {
            if (!es_plano_ativo(p)) continue;
            ves_t *in = pol[cur], *out = pol[cur ^ 1];
            int m = 0;
            for (int i = 0; i < n && m < ES_POL_MAX - 1; i++) {
                const ves_t *a = &in[i], *b = &in[(i + 1) % n];
                float da = dist_plano_es(a, p), db = dist_plano_es(b, p);
                if (da >= 0) out[m++] = *a;
                if ((da >= 0) != (db >= 0) && m < ES_POL_MAX) interpolar_es(&out[m++], a, b, da / (da - db));
            }
            n = m; cur ^= 1;
        }
        fin = pol[cur];
    }
    if (n < 3) return;
    C->est.triangulos += (uint32_t)(n - 2);
#ifdef SGL_TEM_GPU
    if (es_gpu_modo == 2) { es_gpu_poligono2(fin, n); return; }
    if (es_gpu_modo) { es_gpu_poligono(fin, n); return; }
#endif
    sves_t s[ES_POL_MAX];
    for (int i = 0; i < n; i++) a_pantalha_es(&fin[i], &s[i]);
    for (int i = 1; i + 1 < n; i++) {
        sves_t tri[3];
        tri[0] = s[0]; tri[1] = s[i]; tri[2] = s[i + 1];
        triangulo_es(tri);
    }
}

static void es_tri_prim(const ves_t *a, const ves_t *b, const ves_t *c)
{
#ifdef SGL_TEM_GPU
    /* D451: por la GPU, las caras descartadas se quitan aquí (antes se recortaban y se convertían al vértice del
     * kernel para quitarlas después): con w > 0 en los tres, el signo del determinante de (x, y, w) es el del área
     * en la pantalla. Un triángulo degenerado (0) sigue el camino de siempre. Por software no cambia nada. */
    if (es_gpu_modo && C->cull && a->b.w > 0.0f && b->b.w > 0.0f && c->b.w > 0.0f) {
        if (C->cull_face == GL_FRONT_AND_BACK) return;
        float d = a->b.x * (b->b.y * c->b.w - c->b.y * b->b.w) - a->b.y * (b->b.x * c->b.w - c->b.x * b->b.w)
                + a->b.w * (b->b.x * c->b.y - c->b.x * b->b.y);
        if (d != 0.0f) {
            int frente = C->frente == GL_CCW ? d > 0 : d < 0;
            if (C->cull_face == GL_BACK && !frente) return;
            if (C->cull_face == GL_FRONT && frente) return;
        }
    }
#endif
    /* D451: si no hay que cambiar nada de los vértices y ya van seguidos, sin copiarlos (es_poligono solo los lee) */
    if (b == a + 1 && c == a + 2 && !(C->dois_lados && C->luz_v7) && !C->plano && !(C->luz_on && !C->luz_v7)) {
        es_poligono((ves_t *)a, 3);
        return;
    }
    ves_t t[3];
    t[0] = *a; t[1] = *b; t[2] = *c;
    if (C->dois_lados && C->luz_v7) {
        /* la cara de atrás usa el color calculado con la normal al revés */
        float d = t[0].b.x * (t[1].b.y * t[2].b.w - t[2].b.y * t[1].b.w) - t[0].b.y * (t[1].b.x * t[2].b.w - t[2].b.x * t[1].b.w)
                + t[0].b.w * (t[1].b.x * t[2].b.y - t[2].b.x * t[1].b.y);
        int frente = C->frente == GL_CCW ? d > 0 : d < 0;
        if (!frente)
            for (int i = 0; i < 3; i++) {
                t[i].b.r = t[i].bc[0]; t[i].b.g = t[i].bc[1]; t[i].b.b = t[i].bc[2]; t[i].b.a = t[i].bc[3];
                t[i].b.sr = t[i].bs[0]; t[i].b.sg = t[i].bs[1]; t[i].b.sb = t[i].bs[2];
            }
    }
    if (C->plano)
        for (int i = 0; i < 3; i++) {
            t[i].b.r = t[2].b.r; t[i].b.g = t[2].b.g; t[i].b.b = t[2].b.b; t[i].b.a = t[2].b.a;
            t[i].b.sr = t[2].b.sr; t[i].b.sg = t[2].b.sg; t[i].b.sb = t[2].b.sb;
        }
    if (C->luz_on && !C->luz_v7) { vert_t v[3] = { t[0].b, t[1].b, t[2].b }; luz_cara(v); for (int i = 0; i < 3; i++) t[i].b.luz = v[i].luz; }
    es_poligono(t, 3);
}

/* Un fragmento fuera del destino o de la tijera no se dibuja. */
static inline int es_dentro(int x, int y, int x0, int y0, int x1, int y1) { return x >= x0 && x < x1 && y >= y0 && y < y1; }

static void es_frag_de(frag_t *fr, const vert_t *b, float zn)
{
    const float i255 = 1.0f / 255.0f;
    fr->cor[0] = b->r * i255; fr->cor[1] = b->g * i255; fr->cor[2] = b->b * i255; fr->cor[3] = b->a * i255;
    if (C->luz_on && !C->luz_v7) {
        float m = b->luz * (1.0f / 255.0f);
        fr->cor[0] *= m * C->luz_dif[0] + C->luz_amb[0];
        fr->cor[1] *= m * C->luz_dif[1] + C->luz_amb[1];
        fr->cor[2] *= m * C->luz_dif[2] + C->luz_amb[2];
    }
    fr->esp[0] = b->sr * i255; fr->esp[1] = b->sg * i255; fr->esp[2] = b->sb * i255;
    fr->ff = b->ff; fr->zn = zn;
    fr->com_grad = 0;
}

static void es_ponto(const ves_t *o)
{
    for (int p = 0; p < es_n_planos(); p++) if (es_plano_ativo(p) && dist_plano_es(o, p) < 0) return;
    const vert_t *v = &o->b;
    float iw = 1.0f / v->w;
    float sx = (float)C->vx + (v->x * iw + 1.0f) * 0.5f * (float)C->vw;
    float sy = (float)(C->alt - C->vy) - (v->y * iw + 1.0f) * 0.5f * (float)C->vh;
    float zw = es_zjanela(v->z * iw);
    float tam = o->tam, alfa = 1.0f;
    if (C->ponto_att[0] != 1.0f || C->ponto_att[1] != 0.0f || C->ponto_att[2] != 0.0f) {
        float d = raiz(v->ex * v->ex + v->ey * v->ey + v->ez * v->ez);
        float den = C->ponto_att[0] + C->ponto_att[1] * d + C->ponto_att[2] * d * d;
        if (den > 0.0f) tam *= raiz(1.0f / den);
    }
    if (tam < C->ponto_min) tam = C->ponto_min;
    if (tam > C->ponto_max) tam = C->ponto_max;
    if (tam < C->ponto_fade) { float r = tam / C->ponto_fade; alfa = r * r; tam = C->ponto_fade; }
    if (tam < 1.0f) tam = 1.0f;
    if (tam > 128.0f) tam = 128.0f;
    C->est.triangulos += 2;
#ifdef SGL_TEM_GPU
    if (es_gpu_modo == 2) { es_gpu_ponto2(o, sx, sy, zw, tam, alfa); return; }
    if (es_gpu_modo) { es_gpu_ponto(o, sx, sy, zw, tam, alfa); return; }
#endif
    int lado = (int)(tam + 0.5f);
    if (lado < 1) lado = 1;
    /* la regla de GL, con y hacia arriba (yw): lado impar, centrado en el píxel de (xw, yw);
     * par, en la esquina más próxima */
    float yw = (float)C->alt - sy;
    int x0 = (lado & 1) ? piso_f(sx) - (lado - 1) / 2 : piso_f(sx + 0.5f) - lado / 2;
    int yb = (lado & 1) ? piso_f(yw) - (lado - 1) / 2 : piso_f(yw + 0.5f) - lado / 2;
    int y0 = C->alt - (yb + lado);   /* la fila de arriba en la pantalla */
    int cx0, cy0, cx1, cy1;
    caixa_es(&cx0, &cy0, &cx1, &cy1, 0);
    frag_t fr;
    es_frag_de(&fr, v, zw);
    fr.cor[3] *= alfa;
    double z = (double)(int)(zw * 2147483520.0f);
    float it = 1.0f / tam;
    for (int u = 0; u < EN; u++) {
        for (int k = 0; k < 4; k++) { fr.tc[u][k] = o->tc[u][k]; fr.gx[u][k] = fr.gy[u][k] = 0.0f; }
        if (C->sprite_on && C->un[u].coord_replace) { fr.gx[u][0] = it; fr.gy[u][1] = it; if (EU[u].lod) fr.com_grad = 1; }
    }
    for (int y = y0; y < y0 + lado; y++)
        for (int x = x0; x < x0 + lado; x++) {
            if (!es_dentro(x, y, cx0, cy0, cx1, cy1)) continue;
            for (int u = 0; u < EN; u++)
                if (C->sprite_on && C->un[u].coord_replace) {
                    fr.tc[u][0] = 0.5f + ((float)x + 0.5f - sx) * it;
                    fr.tc[u][1] = 0.5f + ((float)y + 0.5f - sy) * it;
                    fr.tc[u][2] = 0.0f; fr.tc[u][3] = 1.0f;
                }
            float c[4];
            es_sombrear(&fr, c);
            es_fragmento(x, y, z, c);
        }
}

static void es_linha(const ves_t *va, const ves_t *vb)
{
    ves_t a = *va, b = *vb;
    if (C->plano) { a.b.r = b.b.r; a.b.g = b.b.g; a.b.b = b.b.b; a.b.a = b.b.a; a.b.sr = b.b.sr; a.b.sg = b.b.sg; a.b.sb = b.b.sb; }
    for (int p = 0; p < es_n_planos(); p++) {
        if (!es_plano_ativo(p)) continue;
        float da = dist_plano_es(&a, p), db = dist_plano_es(&b, p);
        if (da < 0 && db < 0) return;
        if (da < 0) { ves_t r; interpolar_es(&r, &a, &b, da / (da - db)); a = r; }
        else if (db < 0) { ves_t r; interpolar_es(&r, &a, &b, da / (da - db)); b = r; }
    }
#ifdef SGL_TEM_GPU
    if (es_gpu_modo == 2) { es_gpu_linha2(&a, &b); return; }
    if (es_gpu_modo) { es_gpu_linha(&a, &b); return; }
#endif
    float ia = 1.0f / a.b.w, ib = 1.0f / b.b.w;
    float ax = (float)C->vx + (a.b.x * ia + 1.0f) * 0.5f * (float)C->vw, ay = (float)(C->alt - C->vy) - (a.b.y * ia + 1.0f) * 0.5f * (float)C->vh;
    float bx = (float)C->vx + (b.b.x * ib + 1.0f) * 0.5f * (float)C->vw, by = (float)(C->alt - C->vy) - (b.b.y * ib + 1.0f) * 0.5f * (float)C->vh;
    float za = es_zjanela(a.b.z * ia), zb = es_zjanela(b.b.z * ib);
    int x0 = arred16(ax) >> 4, y0 = arred16(ay) >> 4, x1 = arred16(bx) >> 4, y1 = arred16(by) >> 4;
    int dx = x1 - x0, dy = y1 - y0, adx = dx < 0 ? -dx : dx, ady = dy < 0 ? -dy : dy;
    int n = adx > ady ? adx : ady, xmaior = adx >= ady;
    int larg = (int)(C->linha_larg + 0.5f);
    if (larg < 1) larg = 1;
    int cx0, cy0, cx1, cy1;
    caixa_es(&cx0, &cy0, &cx1, &cy1, 0);
    for (int i = 0; i <= n; i++) {
        float t = n ? (float)i / (float)n : 0.0f;
        int x, y;
        if (n == 0) { x = x0; y = y0; }
        else {
            /* redondeo de verdad (el piso() de sgl.c redondea al par: deja huecos en las rutas de siempre) */
            x = x0 + (dx * i * 2 + (dx < 0 ? -n : n)) / (2 * n);
            y = y0 + (dy * i * 2 + (dy < 0 ? -n : n)) / (2 * n);
        }
        ves_t m;
        interpolar_es(&m, &a, &b, t);
        float zw = za + (zb - za) * t;
        frag_t fr;
        es_frag_de(&fr, &m.b, zw);
        for (int u = 0; u < EN; u++) for (int k = 0; k < 4; k++) fr.tc[u][k] = m.tc[u][k];
        float c[4];
        es_sombrear(&fr, c);
        double z = (double)(int)(zw * 2147483520.0f);
        for (int k = 0; k < larg; k++) {
            int o = k - (larg - 1) / 2;
            int px = xmaior ? x : x + o, py = xmaior ? y + o : y;
            if (es_dentro(px, py, cx0, cy0, cx1, cy1)) es_fragmento(px, py, z, c);
        }
    }
}

static int es_fbo_completo(void);

/* idx = NULL: índices consecutivos desde primeiro. */
static void desenhar_es(GLenum modo, int primeiro, int n, const void *idx, GLenum tipo, const fonte_t *f)
{
    if (C->fbo && !es_fbo_completo()) { erro(GL_INVALID_FRAMEBUFFER_OPERATION_OES); return; }
    es_preparar_unidades();
    NATU = 8 + 4 * EN;
    int gen = 0;
    for (int u = 0; u < EN; u++) if (EU[u].t && C->un[u].gen_on) gen = 1;
    if (C->luz_v7 || gen) preparar_normais();
    es_quer_normal = C->luz_v7 || gen;   /* D451 */
    for (int u = 0; u < EN; u++) es_tmat_id[u] = mat_e_identidade(C->un[u].mat[C->un[u].mat_n - 1]);
    fonte_es_t F;
    F.f = f;
    F.ps = C->ps_ativo ? vbo_resolver(C->ps_ptr, C->ps_vbo) : 0; F.ps_tipo = C->ps_tipo; F.ps_passo = C->ps_passo;
    F.mi = C->paleta_on && C->mi_ativo ? vbo_resolver(C->mi_ptr, C->mi_vbo) : 0;
    F.pe = C->paleta_on && C->pe_ativo ? vbo_resolver(C->pe_ptr, C->pe_vbo) : 0;
    for (int u = 0; u < SGL_UNIDADES; u++) {
        const unid_t *un = &C->un[u];
        F.tc[u] = 0;
        if (u == 0 && f->tcv) continue;
        if (u == 0) { if (f->tem_tc) { F.tc[0] = f->tc; F.tc_tam[0] = f->tc_tam; F.tc_tipo[0] = f->tc_tipo; F.tc_passo[0] = f->tc_passo; } continue; }
        if (f->tcv) continue;   /* glBegin: las demás unidades, su coordenada actual */
        if (un->t_ativo && un->t_ptr) {
            F.tc[u] = vbo_resolver(un->t_ptr, un->t_vbo);
            F.tc_tam[u] = un->t_tam; F.tc_tipo[u] = un->t_tipo; F.tc_passo[u] = un->t_passo;
        }
    }
#ifdef SGL_TEM_GPU
    es_gpu_modo = es_gpu_escolher(modo);
#endif
    #define VES(k, out) do { int _i = idx ? (tipo == GL_UNSIGNED_BYTE ? ((const uint8_t *)idx)[k] : tipo == GL_UNSIGNED_SHORT ? ((const uint16_t *)idx)[k] : (int)((const uint32_t *)idx)[k]) : primeiro + (k); es_buscar(&F, _i, &out); } while (0)
    static ves_t a, b, c, abc[3];
    switch (modo) {
    case GL_POINTS:
        for (int i = 0; i < n; i++) { VES(i, a); es_ponto(&a); }
        break;
    case GL_TRIANGLES:
        /* D451: los tres seguidos, para que es_tri_prim no tenga que copiarlos */
        for (int i = 0; i + 2 < n; i += 3) { VES(i, abc[0]); VES(i + 1, abc[1]); VES(i + 2, abc[2]); es_tri_prim(&abc[0], &abc[1], &abc[2]); }
        break;
    case GL_TRIANGLE_STRIP:
        for (int i = 0; i + 2 < n; i++) {
            VES(i, a); VES(i + 1, b); VES(i + 2, c);
            if (i & 1) es_tri_prim(&b, &a, &c); else es_tri_prim(&a, &b, &c);
        }
        break;
    case GL_TRIANGLE_FAN:
        if (n >= 3) VES(0, a);
        for (int i = 1; i + 1 < n; i++) { VES(i, b); VES(i + 1, c); es_tri_prim(&a, &b, &c); }
        break;
    case GL_LINES:
        for (int i = 0; i + 1 < n; i += 2) { VES(i, a); VES(i + 1, b); es_linha(&a, &b); }
        break;
    case GL_LINE_STRIP:
        for (int i = 0; i + 1 < n; i++) { VES(i, a); VES(i + 1, b); es_linha(&a, &b); }
        break;
    case GL_LINE_LOOP:
        for (int i = 0; i < n && n >= 2; i++) { VES(i, a); VES((i + 1) % n, b); es_linha(&a, &b); }
        break;
    default: erro(GL_INVALID_ENUM);
    }
    #undef VES
}

/* ------------------------------------------------------------ OES_draw_texture */

static void es_desenhar_textura(float x, float y, float z, float w, float h)
{
    if (!C) return;
    if (!(w > 0.0f) || !(h > 0.0f)) { erro(GL_INVALID_VALUE); return; }
    if (C->fbo && !es_fbo_completo()) { erro(GL_INVALID_FRAMEBUFFER_OPERATION_OES); return; }
    GPU_DESPEJAR();
#ifdef SGL_TEM_GPU
    /* por la CPU, sobre el cuadro de la GPU traído a la RAM */
    if (C->gpu && !C->gpu_sw) es_gpu_a_software();
#endif
    es_preparar_unidades();
    float zw = C->z_perto + (C->z_longe - C->z_perto) * sat01(z);
    double zq = (double)(int)(zw * 2147483520.0f);
    int cx0, cy0, cx1, cy1;
    caixa_es(&cx0, &cy0, &cx1, &cy1, 0);
    int px0 = piso_f(x + 0.5f), px1 = piso_f(x + w + 0.5f);
    int wy0 = piso_f(y + 0.5f), wy1 = piso_f(y + h + 0.5f);
    frag_t fr;
    vert_t b;
    b.r = C->cor[0]; b.g = C->cor[1]; b.b = C->cor[2]; b.a = C->cor[3];
    b.sr = b.sg = b.sb = 0; b.ff = 1.0f; b.luz = 255.0f;
    int luz = C->luz_on, fog = C->fog;
    C->luz_on = 0; C->fog = 0;          /* sin luz ni niebla (la extensión no las usa) */
    es_frag_de(&fr, &b, zw);
    float esc[SGL_UNIDADES][2];
    for (int u = 0; u < EN; u++) {
        for (int k = 0; k < 4; k++) fr.gx[u][k] = fr.gy[u][k] = 0.0f;
        fr.tc[u][2] = 0.0f; fr.tc[u][3] = 1.0f;
        esc[u][0] = esc[u][1] = 0.0f;
        if (!EU[u].t || EU[u].cubo) continue;
        const textura_t *t = EU[u].t;
        esc[u][0] = (float)t->recorte[2] / w / (float)t->larg;
        esc[u][1] = (float)t->recorte[3] / h / (float)t->alt;
        fr.gx[u][0] = esc[u][0]; fr.gy[u][1] = -esc[u][1];
        if (EU[u].lod) fr.com_grad = 1;
    }
    for (int wy = wy0; wy < wy1; wy++) {
        int sy = C->alt - 1 - wy;
        for (int px = px0; px < px1; px++) {
            if (!es_dentro(px, sy, cx0, cy0, cx1, cy1)) continue;
            for (int u = 0; u < EN; u++) {
                if (!EU[u].t || EU[u].cubo) continue;
                const textura_t *t = EU[u].t;
                fr.tc[u][0] = ((float)t->recorte[0] + ((float)px + 0.5f - x) * (float)t->recorte[2] / w) / (float)t->larg;
                fr.tc[u][1] = ((float)t->recorte[1] + ((float)wy + 0.5f - y) * (float)t->recorte[3] / h) / (float)t->alt;
            }
            float c[4];
            es_sombrear(&fr, c);
            es_fragmento(px, sy, zq, c);
        }
    }
    C->luz_on = luz; C->fog = fog;
}

void glDrawTexfOES(GLfloat x, GLfloat y, GLfloat z, GLfloat w, GLfloat h) { es_desenhar_textura(x, y, z, w, h); }
void glDrawTexfvOES(const GLfloat *c) { es_desenhar_textura(c[0], c[1], c[2], c[3], c[4]); }
void glDrawTexiOES(GLint x, GLint y, GLint z, GLint w, GLint h) { es_desenhar_textura((float)x, (float)y, (float)z, (float)w, (float)h); }
void glDrawTexivOES(const GLint *c) { glDrawTexiOES(c[0], c[1], c[2], c[3], c[4]); }
void glDrawTexsOES(GLshort x, GLshort y, GLshort z, GLshort w, GLshort h) { glDrawTexiOES(x, y, z, w, h); }
void glDrawTexsvOES(const GLshort *c) { glDrawTexiOES(c[0], c[1], c[2], c[3], c[4]); }
void glDrawTexxOES(GLfixed x, GLfixed y, GLfixed z, GLfixed w, GLfixed h)
{
    const float k = 1.0f / 65536.0f;
    es_desenhar_textura((float)x * k, (float)y * k, (float)z * k, (float)w * k, (float)h * k);
}
void glDrawTexxvOES(const GLfixed *c) { glDrawTexxOES(c[0], c[1], c[2], c[3], c[4]); }

/* ------------------------------------------------------------ OES_framebuffer_object */

static void es_fbo_aplicar_ja(void);

typedef struct { GLuint id; int usado; int fmt; int larg, alt; void *dados; } rb_t;
#define FB_NADA 0
#define FB_RB   1
#define FB_TEX  2
typedef struct {
    GLuint    id;
    int       usado;
    int       cor_tipo, z_tipo, e_tipo;   /* FB_* */
    GLuint    cor_id, z_id, e_id;
    int       cor_nivel, cor_face;        /* textura: nivel y cara (-1 = 2D) */
    uint32_t *px;                          /* textura: el destino en 0xAARRGGBB mientras está ligado */
    int       pl, pa;
} fbo_t;

static rb_t *rb_por_id(GLuint id) { rb_t *r = (rb_t *)C->rbs; for (int i = 0; i < C->n_rbs; i++) if (r[i].usado && r[i].id == id) return &r[i]; return 0; }
static fbo_t *fbo_por_id(GLuint id) { fbo_t *f = (fbo_t *)C->fbos; for (int i = 0; i < C->n_fbos; i++) if (f[i].usado && f[i].id == id) return &f[i]; return 0; }

static void *es_lista_nova(void **lista, int *n, int tam)
{
    char *l = (char *)*lista;
    for (int i = 0; i < *n; i++) if (!*(int *)(l + (size_t)i * tam + sizeof(GLuint))) return l + (size_t)i * tam;
    int nn = *n ? *n * 2 : 8;
    char *x = (char *)SGL_ALOCAR((size_t)nn * tam);
    if (!x) return 0;
    for (size_t k = 0; k < (size_t)*n * tam; k++) x[k] = l[k];
    if (l) SGL_LIBERAR(l);
    *lista = x;
    char *r = x + (size_t)*n * tam;
    *n = nn;
    return r;
}

static rb_t *rb_novo(GLuint id)
{
    rb_t *r = (rb_t *)es_lista_nova(&C->rbs, &C->n_rbs, sizeof(rb_t));
    if (!r) return 0;
    r->id = id; r->usado = 1; r->fmt = 0; r->larg = r->alt = 0; r->dados = 0;
    return r;
}

static fbo_t *fbo_novo(GLuint id)
{
    fbo_t *f = (fbo_t *)es_lista_nova(&C->fbos, &C->n_fbos, sizeof(fbo_t));
    if (!f) return 0;
    f->id = id; f->usado = 1; f->cor_tipo = f->z_tipo = f->e_tipo = FB_NADA; f->cor_id = f->z_id = f->e_id = 0;
    f->cor_nivel = 0; f->cor_face = -1; f->px = 0; f->pl = f->pa = 0;
    return f;
}

void glGenRenderbuffersOES(GLsizei n, GLuint *ids)
{
    if (!C) return;
    if (n < 0) { erro(GL_INVALID_VALUE); return; }
    for (int i = 0; i < n; i++) { GLuint id = 1; while (rb_por_id(id)) id++; ids[i] = rb_novo(id) ? id : 0; }
}

void glGenFramebuffersOES(GLsizei n, GLuint *ids)
{
    if (!C) return;
    if (n < 0) { erro(GL_INVALID_VALUE); return; }
    for (int i = 0; i < n; i++) { GLuint id = 1; while (fbo_por_id(id)) id++; ids[i] = fbo_novo(id) ? id : 0; }
}

GLboolean glIsRenderbufferOES(GLuint id) { return C && id && rb_por_id(id) ? GL_TRUE : GL_FALSE; }
GLboolean glIsFramebufferOES(GLuint id) { return C && id && fbo_por_id(id) ? GL_TRUE : GL_FALSE; }

void glBindRenderbufferOES(GLenum alvo, GLuint id)
{
    if (!C) return;
    if (alvo != GL_RENDERBUFFER_OES) { erro(GL_INVALID_ENUM); return; }
    if (id && !rb_por_id(id) && !rb_novo(id)) { erro(GL_OUT_OF_MEMORY); return; }
    C->rb = id;
}

static int rb_bytes(int fmt)
{
    switch (fmt) {
    case GL_DEPTH_COMPONENT16_OES: return 2;
    case GL_STENCIL_INDEX8_OES: return 1;
    default: return 4;
    }
}

void glRenderbufferStorageOES(GLenum alvo, GLenum fmt, GLsizei l, GLsizei a)
{
    if (!C) return;
    if (alvo != GL_RENDERBUFFER_OES) { erro(GL_INVALID_ENUM); return; }
    if (fmt != GL_RGBA4_OES && fmt != GL_RGB5_A1_OES && fmt != GL_RGB565_OES && fmt != GL_RGB8_OES && fmt != GL_RGBA8_OES &&
        fmt != GL_DEPTH_COMPONENT16_OES && fmt != GL_DEPTH_COMPONENT24_OES && fmt != GL_STENCIL_INDEX8_OES) { erro(GL_INVALID_ENUM); return; }
    if (l < 0 || a < 0 || l > SGL_MAX_LADO || a > SGL_MAX_LADO) { erro(GL_INVALID_VALUE); return; }
    rb_t *r = C->rb ? rb_por_id(C->rb) : 0;
    if (!r) { erro(GL_INVALID_OPERATION); return; }
    void *d = 0;
    if (l && a) {
        d = SGL_ALOCAR((size_t)l * a * rb_bytes((int)fmt));
        if (!d) { erro(GL_OUT_OF_MEMORY); return; }
    }
    if (r->dados) SGL_LIBERAR(r->dados);
    r->dados = d; r->fmt = (int)fmt; r->larg = l; r->alt = a;
    if (C->fbo) es_fbo_aplicar_ja();
}

void glGetRenderbufferParameterivOES(GLenum alvo, GLenum nome, GLint *v)
{
    if (!C) return;
    if (alvo != GL_RENDERBUFFER_OES) { erro(GL_INVALID_ENUM); return; }
    rb_t *r = C->rb ? rb_por_id(C->rb) : 0;
    if (!r) { erro(GL_INVALID_OPERATION); return; }
    int f = r->fmt;
    switch (nome) {
    case GL_RENDERBUFFER_WIDTH_OES: *v = r->larg; break;
    case GL_RENDERBUFFER_HEIGHT_OES: *v = r->alt; break;
    case GL_RENDERBUFFER_INTERNAL_FORMAT_OES: *v = f ? f : GL_RGBA4_OES; break;
    case GL_RENDERBUFFER_RED_SIZE_OES: case GL_RENDERBUFFER_GREEN_SIZE_OES: case GL_RENDERBUFFER_BLUE_SIZE_OES:
        *v = f == GL_RGBA8_OES || f == GL_RGB8_OES ? 8 : f == GL_RGBA4_OES ? 4 : f == GL_RGB5_A1_OES ? 5 :
             f == GL_RGB565_OES ? (nome == GL_RENDERBUFFER_GREEN_SIZE_OES ? 6 : 5) : 0;
        break;
    case GL_RENDERBUFFER_ALPHA_SIZE_OES: *v = f == GL_RGBA8_OES ? 8 : f == GL_RGBA4_OES ? 4 : f == GL_RGB5_A1_OES ? 1 : 0; break;
    case GL_RENDERBUFFER_DEPTH_SIZE_OES: *v = f == GL_DEPTH_COMPONENT16_OES ? 16 : f == GL_DEPTH_COMPONENT24_OES ? 24 : 0; break;
    case GL_RENDERBUFFER_STENCIL_SIZE_OES: *v = f == GL_STENCIL_INDEX8_OES ? 8 : 0; break;
    default: erro(GL_INVALID_ENUM);
    }
}

/* La textura (o cara) de color de f, con su nivel; NULL si no hay. */
static textura_t *fbo_tex(const fbo_t *f)
{
    if (f->cor_tipo != FB_TEX) return 0;
    textura_t *t = textura_por_id(f->cor_id);
    if (!t) return 0;
    if (f->cor_face >= 0) return t->faces ? &t->faces[f->cor_face] : 0;
    return t;
}

/* El destino de un FBO con textura vuelve a la textura (RGBA). */
static void es_fbo_resolver(void)
{
    fbo_t *f = C->fbo ? fbo_por_id(C->fbo) : 0;
    if (!f || f->cor_tipo != FB_TEX || !f->px) return;
    textura_t *t = fbo_tex(f);
    if (!t) return;
    int k = f->cor_nivel;
    unsigned char *d = k ? t->nivel_px[k] : t->px;
    int l = k ? t->nivel_l[k] : t->larg, a = k ? t->nivel_a[k] : t->alt;
    if (!d || l != f->pl || a != f->pa) return;
    /* fila 0 de la textura = abajo (t = 0); fila 0 del destino = arriba */
    for (int y = 0; y < a; y++)
        for (int x = 0; x < l; x++) {
            uint32_t v = f->px[(size_t)(a - 1 - y) * l + x];
            unsigned char *o = d + ((size_t)y * l + x) * 4;
            o[0] = (unsigned char)(v >> 16); o[1] = (unsigned char)(v >> 8); o[2] = (unsigned char)v; o[3] = (unsigned char)(v >> 24);
            es_converter_base(o, t->formato_base);
        }
    tex_mudou(t, k);
}

static int es_fbo_estado(const fbo_t *f, int *l, int *a)
{
    int lc = -1, ac = -1, tem = 0;
    if (f->cor_tipo == FB_TEX) {
        textura_t *t = fbo_tex(f);
        if (!t) return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_OES;
        int k = f->cor_nivel;
        unsigned char *d = k ? t->nivel_px[k] : t->px;
        if (!d) return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_OES;
        int b = t->formato_base;
        if (b != GL_RGB && b != GL_RGBA) return GL_FRAMEBUFFER_INCOMPLETE_FORMATS_OES;
        lc = k ? t->nivel_l[k] : t->larg; ac = k ? t->nivel_a[k] : t->alt; tem = 1;
    } else if (f->cor_tipo == FB_RB) {
        rb_t *r = rb_por_id(f->cor_id);
        if (!r || !r->dados || rb_bytes(r->fmt) != 4 || r->fmt == GL_DEPTH_COMPONENT24_OES) return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_OES;
        lc = r->larg; ac = r->alt; tem = 1;
    }
    GLuint ids[2] = { f->z_tipo == FB_RB ? f->z_id : 0, f->e_tipo == FB_RB ? f->e_id : 0 };
    int fmts[2][2] = { { GL_DEPTH_COMPONENT16_OES, GL_DEPTH_COMPONENT24_OES }, { GL_STENCIL_INDEX8_OES, GL_STENCIL_INDEX8_OES } };
    for (int i = 0; i < 2; i++) {
        if (!ids[i]) { if ((i ? f->e_tipo : f->z_tipo) != FB_NADA) return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_OES; continue; }
        rb_t *r = rb_por_id(ids[i]);
        if (!r || !r->dados || (r->fmt != fmts[i][0] && r->fmt != fmts[i][1])) return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_OES;
        if (lc >= 0 && (r->larg != lc || r->alt != ac)) return GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_OES;
        lc = r->larg; ac = r->alt;
    }
    if (!tem) return GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT_OES;   /* SaguiGL pide siempre un color */
    *l = lc; *a = ac;
    return GL_FRAMEBUFFER_COMPLETE_OES;
}

static int es_fbo_completo(void)
{
    fbo_t *f = C->fbo ? fbo_por_id(C->fbo) : 0;
    int l, a;
    return f && es_fbo_estado(f, &l, &a) == GL_FRAMEBUFFER_COMPLETE_OES && C->px;
}

/* Pone en el contexto el destino del FBO ligado (o el de la ventana). */
static void es_fbo_aplicar_ja(void)
{
    fbo_t *f = C->fbo ? fbo_por_id(C->fbo) : 0;
    if (!f) {
        C->px = C->princ.px; C->larg = C->princ.larg; C->alt = C->princ.alt;
        C->zbits = C->princ.zbits; C->zbuf = C->princ.zbuf; C->ebuf = C->princ.ebuf; C->alvo_alfa = C->princ.alfa;
        return;
    }
    int l = 0, a = 0;
    C->px = 0; C->zbuf = 0; C->ebuf = 0; C->alvo_alfa = 0;
    if (es_fbo_estado(f, &l, &a) != GL_FRAMEBUFFER_COMPLETE_OES) { C->larg = C->princ.larg; C->alt = C->princ.alt; return; }
    C->larg = l; C->alt = a;
    if (f->cor_tipo == FB_TEX) {
        textura_t *t = fbo_tex(f);
        int k = f->cor_nivel;
        const unsigned char *d = k ? t->nivel_px[k] : t->px;
        if (!f->px || f->pl != l || f->pa != a) {
            if (f->px) SGL_LIBERAR(f->px);
            f->px = (uint32_t *)SGL_ALOCAR((size_t)l * a * 4);
            f->pl = l; f->pa = a;
        }
        if (f->px)
            for (int y = 0; y < a; y++)
                for (int x = 0; x < l; x++) {
                    const unsigned char *q = d + ((size_t)(a - 1 - y) * l + x) * 4;
                    f->px[(size_t)y * l + x] = ((uint32_t)q[3] << 24) | ((uint32_t)q[0] << 16) | ((uint32_t)q[1] << 8) | q[2];
                }
        C->px = f->px;
        C->alvo_alfa = t->formato_base == GL_RGBA;
    } else {
        rb_t *r = rb_por_id(f->cor_id);
        C->px = (uint32_t *)r->dados;
        C->alvo_alfa = r->fmt == GL_RGBA4_OES || r->fmt == GL_RGB5_A1_OES || r->fmt == GL_RGBA8_OES;
    }
    if (f->z_tipo == FB_RB) { rb_t *r = rb_por_id(f->z_id); C->zbuf = r->dados; C->zbits = r->fmt == GL_DEPTH_COMPONENT16_OES ? 16 : 32; }
    if (f->e_tipo == FB_RB) { rb_t *r = rb_por_id(f->e_id); C->ebuf = (uint8_t *)r->dados; }
}

void glBindFramebufferOES(GLenum alvo, GLuint id)
{
    if (!C) return;
    if (alvo != GL_FRAMEBUFFER_OES) { erro(GL_INVALID_ENUM); return; }
    if (id && !fbo_por_id(id) && !fbo_novo(id)) { erro(GL_OUT_OF_MEMORY); return; }
    GPU_DESPEJAR();
    if (C->fbo) es_fbo_resolver();
    else if (id) {
        /* se guarda el destino de la ventana (el esténcil puede crearse después: se guarda al volver) */
        C->princ.px = C->px; C->princ.larg = C->larg; C->princ.alt = C->alt; C->princ.zbits = C->zbits;
        C->princ.zbuf = C->zbuf; C->princ.ebuf = C->ebuf; C->princ.alfa = C->alvo_alfa;
    }
    C->fbo = id;
    es_fbo_aplicar_ja();
}

GLenum glCheckFramebufferStatusOES(GLenum alvo)
{
    if (!C) return 0;
    if (alvo != GL_FRAMEBUFFER_OES) { erro(GL_INVALID_ENUM); return 0; }
    if (!C->fbo) return GL_FRAMEBUFFER_COMPLETE_OES;
    int l, a;
    return (GLenum)es_fbo_estado(fbo_por_id(C->fbo), &l, &a);
}

static fbo_t *es_fbo_ligado(GLenum alvo)
{
    if (alvo != GL_FRAMEBUFFER_OES) { erro(GL_INVALID_ENUM); return 0; }
    fbo_t *f = C->fbo ? fbo_por_id(C->fbo) : 0;
    if (!f) erro(GL_INVALID_OPERATION);
    return f;
}

void glFramebufferRenderbufferOES(GLenum alvo, GLenum anexo, GLenum rbalvo, GLuint id)
{
    if (!C) return;
    fbo_t *f = es_fbo_ligado(alvo);
    if (!f) return;
    if (rbalvo != GL_RENDERBUFFER_OES && id) { erro(GL_INVALID_ENUM); return; }
    if (id && !rb_por_id(id)) { erro(GL_INVALID_OPERATION); return; }
    es_fbo_resolver();
    int tp = id ? FB_RB : FB_NADA;
    if (anexo == GL_COLOR_ATTACHMENT0_OES) { f->cor_tipo = tp; f->cor_id = id; }
    else if (anexo == GL_DEPTH_ATTACHMENT_OES) { f->z_tipo = tp; f->z_id = id; }
    else if (anexo == GL_STENCIL_ATTACHMENT_OES) { f->e_tipo = tp; f->e_id = id; }
    else { erro(GL_INVALID_ENUM); return; }
    es_fbo_aplicar_ja();
}

void glFramebufferTexture2DOES(GLenum alvo, GLenum anexo, GLenum talvo, GLuint id, GLint nivel)
{
    if (!C) return;
    fbo_t *f = es_fbo_ligado(alvo);
    if (!f) return;
    if (anexo != GL_COLOR_ATTACHMENT0_OES) { erro(id ? GL_INVALID_OPERATION : GL_INVALID_ENUM); return; }
    if (talvo != GL_TEXTURE_2D && es_face(talvo) < 0) { erro(GL_INVALID_ENUM); return; }
    if (id) {
        textura_t *t = textura_por_id(id);
        if (!t || (talvo == GL_TEXTURE_2D) != !t->cubo) { erro(GL_INVALID_OPERATION); return; }
        if (nivel < 0 || nivel >= MAX_NIVEIS) { erro(GL_INVALID_VALUE); return; }
    }
    es_fbo_resolver();
    f->cor_tipo = id ? FB_TEX : FB_NADA; f->cor_id = id; f->cor_nivel = nivel; f->cor_face = es_face(talvo);
    es_fbo_aplicar_ja();
}

void glGetFramebufferAttachmentParameterivOES(GLenum alvo, GLenum anexo, GLenum nome, GLint *v)
{
    if (!C) return;
    fbo_t *f = es_fbo_ligado(alvo);
    if (!f) return;
    int tp; GLuint id;
    if (anexo == GL_COLOR_ATTACHMENT0_OES) { tp = f->cor_tipo; id = f->cor_id; }
    else if (anexo == GL_DEPTH_ATTACHMENT_OES) { tp = f->z_tipo; id = f->z_id; }
    else if (anexo == GL_STENCIL_ATTACHMENT_OES) { tp = f->e_tipo; id = f->e_id; }
    else { erro(GL_INVALID_ENUM); return; }
    switch (nome) {
    case GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE_OES: *v = tp == FB_RB ? GL_RENDERBUFFER_OES : tp == FB_TEX ? GL_TEXTURE : GL_NONE_OES; break;
    case GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME_OES: if (tp == FB_NADA) { erro(GL_INVALID_ENUM); return; } *v = (GLint)id; break;
    case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL_OES: if (tp != FB_TEX) { erro(GL_INVALID_ENUM); return; } *v = f->cor_nivel; break;
    case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE_OES:
        if (tp != FB_TEX) { erro(GL_INVALID_ENUM); return; }
        *v = f->cor_face < 0 ? 0 : GL_TEXTURE_CUBE_MAP_POSITIVE_X_OES + f->cor_face; break;
    default: erro(GL_INVALID_ENUM);
    }
}

void glDeleteFramebuffersOES(GLsizei n, const GLuint *ids)
{
    if (!C) return;
    if (n < 0) { erro(GL_INVALID_VALUE); return; }
    for (int i = 0; i < n; i++) {
        fbo_t *f = ids[i] ? fbo_por_id(ids[i]) : 0;
        if (!f) continue;
        if (C->fbo == ids[i]) glBindFramebufferOES(GL_FRAMEBUFFER_OES, 0);
        if (f->px) SGL_LIBERAR(f->px);
        f->px = 0; f->usado = 0;
    }
}

void glDeleteRenderbuffersOES(GLsizei n, const GLuint *ids)
{
    if (!C) return;
    if (n < 0) { erro(GL_INVALID_VALUE); return; }
    for (int i = 0; i < n; i++) {
        rb_t *r = ids[i] ? rb_por_id(ids[i]) : 0;
        if (!r) continue;
        /* se suelta de los FBO que lo usan */
        fbo_t *f = (fbo_t *)C->fbos;
        for (int k = 0; k < C->n_fbos; k++) {
            if (!f[k].usado) continue;
            if (f[k].cor_tipo == FB_RB && f[k].cor_id == ids[i]) f[k].cor_tipo = FB_NADA;
            if (f[k].z_tipo == FB_RB && f[k].z_id == ids[i]) f[k].z_tipo = FB_NADA;
            if (f[k].e_tipo == FB_RB && f[k].e_id == ids[i]) f[k].e_tipo = FB_NADA;
        }
        if (C->rb == ids[i]) C->rb = 0;
        if (r->dados) SGL_LIBERAR(r->dados);
        r->dados = 0; r->usado = 0;
        if (C->fbo) es_fbo_aplicar_ja();
    }
}

static void es_fbo_textura_apagada(int ti)
{
    fbo_t *f = (fbo_t *)C->fbos;
    GLuint id = C->tex[ti].id;
    for (int k = 0; k < C->n_fbos; k++)
        if (f[k].usado && f[k].cor_tipo == FB_TEX && f[k].cor_id == id) {
            f[k].cor_tipo = FB_NADA;
            if (C->fbo == f[k].id) es_fbo_aplicar_ja();
        }
}

static void es_fbo_soltar_tudo(sgl_ctx_t *c)
{
    fbo_t *f = (fbo_t *)c->fbos;
    for (int k = 0; k < c->n_fbos; k++) if (f[k].px) SGL_LIBERAR(f[k].px);
    rb_t *r = (rb_t *)c->rbs;
    for (int k = 0; k < c->n_rbs; k++) if (r[k].dados) SGL_LIBERAR(r[k].dados);
    vbo_t *b = (vbo_t *)c->bufs;
    for (int k = 0; k < c->n_bufs; k++) if (b[k].dados) SGL_LIBERAR(b[k].dados);
    if (c->fbos) SGL_LIBERAR(c->fbos);
    if (c->rbs) SGL_LIBERAR(c->rbs);
    if (c->bufs) SGL_LIBERAR(c->bufs);
    if (c->fbo) { c->ebuf = c->princ.ebuf; }   /* el esténcil de la ventana */
    if (c->ebuf) SGL_LIBERAR(c->ebuf);
    for (int i = 0; i < MAX_TEX; i++)
        if (c->tex[i].faces) {
            for (int fc = 0; fc < 6; fc++) { textura_t *x = &c->tex[i].faces[fc];
                for (int k = 1; k < MAX_NIVEIS; k++) if (x->nivel_px[k]) SGL_LIBERAR(x->nivel_px[k]);
                if (x->px) SGL_LIBERAR(x->px); }
            SGL_LIBERAR(c->tex[i].faces);
        }
}

/* ------------------------------------------------------------ glClear, glReadPixels */

/* glClear con lo que las rutas de siempre no saben: tijera, máscaras, valor de Z, esténcil, FBO. */
static int es_limpar_precisa(GLbitfield m)
{
    if (m & GL_STENCIL_BUFFER_BIT) return 1;
    if (C->fbo || C->tij_on) return 1;
    if ((m & GL_COLOR_BUFFER_BIT) && (C->masc_cor & 7) != 7) return 1;
    if ((m & GL_DEPTH_BUFFER_BIT) && (!C->masc_z || C->z_limpo != 1.0f)) return 1;
    return 0;
}

static void es_limpar(GLbitfield m)
{
    if (C->fbo && !es_fbo_completo()) { erro(GL_INVALID_FRAMEBUFFER_OPERATION_OES); return; }
    int x0, y0, x1, y1;
    caixa_es(&x0, &y0, &x1, &y1, 0);
    if (x0 >= x1 || y0 >= y1) return;
    if (m & GL_COLOR_BUFFER_BIT) {
        uint32_t v = C->cor_limpa | (C->alvo_alfa ? (uint32_t)C->alfa_limpo << 24 : 0u);
        uint32_t mk = 0;
        if (C->masc_cor & 1) mk |= 0x00FF0000u;
        if (C->masc_cor & 2) mk |= 0x0000FF00u;
        if (C->masc_cor & 4) mk |= 0x000000FFu;
        if ((C->masc_cor & 8) && C->alvo_alfa) mk |= 0xFF000000u;
        for (int y = y0; y < y1; y++) {
            uint32_t *p = C->px + (size_t)y * C->larg;
            for (int x = x0; x < x1; x++) p[x] = (p[x] & ~mk) | (v & mk);
        }
    }
    if ((m & GL_DEPTH_BUFFER_BIT) && C->zbuf && C->masc_z) {
        /* el mismo valor que daría un fragmento con ese z */
        double z = (double)(int)(C->z_limpo * 2147483520.0f);
        if (C->z_limpo >= 1.0f) z = 0x7FFFFFFF;
        for (int y = y0; y < y1; y++) {
            if (C->zbits == 32) { uint32_t *p = (uint32_t *)C->zbuf + (size_t)y * C->larg; for (int x = x0; x < x1; x++) p[x] = (uint32_t)z; }
            else {
                uint32_t zi = C->z_limpo >= 1.0f ? 0xFFFF : (uint32_t)deszc(z, 15);
                uint16_t *p = (uint16_t *)C->zbuf + (size_t)y * C->larg;
                for (int x = x0; x < x1; x++) p[x] = (uint16_t)zi;
            }
        }
    }
    if (m & GL_STENCIL_BUFFER_BIT) {
        uint8_t *e = es_ebuf();
        if (e) {
            unsigned mk = C->est_escr & 0xFF, v = (unsigned)C->est_limpo & mk;
            for (int y = y0; y < y1; y++) { uint8_t *p = e + (size_t)y * C->larg; for (int x = x0; x < x1; x++) p[x] = (uint8_t)((p[x] & ~mk) | v); }
        }
    }
}

void glReadPixels(GLint x, GLint y, GLsizei l, GLsizei a, GLenum fmt, GLenum tipo, GLvoid *dados)
{
    if (!C) return;
    if (l < 0 || a < 0) { erro(GL_INVALID_VALUE); return; }
    if ((fmt != GL_RGBA && fmt != GL_RGB) || tipo != GL_UNSIGNED_BYTE) { erro(GL_INVALID_OPERATION); return; }
    if (C->fbo && !es_fbo_completo()) { erro(GL_INVALID_FRAMEBUFFER_OPERATION_OES); return; }
#ifdef SGL_TEM_GPU
    gpu_sincronizar();
#endif
    int comp = fmt == GL_RGBA ? 4 : 3;
    size_t fila = es_fila(l, comp, C->pack_alin);
    unsigned char *d = (unsigned char *)dados;
    for (int j = 0; j < a; j++)
        for (int i = 0; i < l; i++) {
            int sx = x + i, sy = y + j;
            if (sx < 0 || sy < 0 || sx >= C->larg || sy >= C->alt) continue;   /* fuera: sin definir (no se toca) */
            unsigned char px[4];
            es_ler_px(sx, sy, px);
            unsigned char *o = d + (size_t)j * fila + (size_t)i * comp;
            for (int k = 0; k < comp; k++) o[k] = px[k];
        }
}

/* ------------------------------------------------------------ la tubería completa por la GPU (D425) */

#ifdef SGL_TEM_GPU
/* Qué sabe hacer el driver (GPU3D_OP_CAPACIDADES), una vez por contexto. */
static void es_gpu_capas(void)
{
    if (C->gpu_capas_ok) return;
    gpu3d_capas_t k;
    uint8_t *p = (uint8_t *)&k;
    for (unsigned i = 0; i < sizeof k; i++) p[i] = 0;
    if (gpu_chamar(GPU3D_OP_CAPACIDADES, 0, 0, 0, &k) == 0 && k.versao >= 2) {
        C->gpu_bits = k.bits; C->gpu_unid = k.unidades; C->gpu_formatos = k.formatos;
        C->gpu_bits3 = gpu_v3 && (k.bits & G3D_CAP_V2) ? k.bits3 : 0;   /* D457 */
    } else { C->gpu_bits = 0; C->gpu_unid = 1; C->gpu_formatos = 1; C->gpu_bits3 = 0; }
    C->gpu_capas_ok = 1;
}

/* El cuadro sigue por software hasta el final (un dibujo no cabe en lo que sabe el driver). */
static void es_gpu_a_software(void)
{
    if (!C->gpu || C->gpu_sw) return;
    gpu_sincronizar();               /* el color que ya dibujó la GPU, a px */
    C->gpu_sw = 1; C->gpu_coube = 0;
    /* el Z de la GPU no vuelve: el de software empieza al fondo (D425, docs/gpu3d.md) */
    int n = C->larg * C->alt;
    if (C->zbits == 32) { uint32_t *z = C->zbuf; for (int i = 0; i < n; i++) z[i] = 0x7FFFFFFFu; }
    else { uint16_t *z = C->zbuf; for (int i = 0; i < n; i++) z[i] = 0xFFFF; }
}

static int es_mezcla_v1(void)
{
    if (!C->blend) return 1;
    if (C->blend_s != C->blend_sa || C->blend_d != C->blend_da || C->blend_eq != GL_FUNC_ADD_OES || C->blend_eqa != GL_FUNC_ADD_OES) return 0;
    int s = C->blend_s, d = C->blend_d;
    return (s == GL_SRC_ALPHA || s == GL_ONE_MINUS_SRC_ALPHA || s == GL_ONE || s == GL_ZERO) &&
           (d == GL_SRC_ALPHA || d == GL_ONE_MINUS_SRC_ALPHA || d == GL_ONE || d == GL_ZERO);
}

static int es_tijera_inteira(void)
{
    return !C->tij_on || (C->tij_x <= 0 && C->tij_y <= 0 && C->tij_x + C->tij_l >= C->larg && C->tij_y + C->tij_a >= C->alt);
}

/* ¿Cabe en gpu3d v1? (una textura con MODULATE o REPLACE, mezcla y Z de siempre) */
static int es_cabe_v1(void)
{
    if (C->alfa_on || C->est_on || C->log_on || C->off_on || !es_tijera_inteira() || (C->masc_cor & 7) != 7) return 0;
    if (C->teste_z && C->func_z != GL_LESS && C->func_z != GL_LEQUAL && C->func_z != GL_ALWAYS) return 0;
    if (!es_mezcla_v1() || EN > 1) return 0;
    if (EN == 1) {
        const unid_t *u = &C->un[0];
        const textura_t *t = EU[0].t;
        if (EU[0].cubo || (u->env_modo != GL_MODULATE && u->env_modo != GL_REPLACE)) return 0;
        if (u->gen_on || !mat_e_identidade(u->mat[u->mat_n - 1])) return 0;
        if (t->wrap_s == GL_MIRRORED_REPEAT_OES || t->wrap_t == GL_MIRRORED_REPEAT_OES) return 0;
    }
    return 1;
}

static unsigned char es_g3d_fator(int f, int origem)
{
    (void)origem;
    switch (f) {
    case GL_ZERO: return G3D_F_ZERO;
    case GL_ONE: return G3D_F_UM;
    case GL_SRC_ALPHA: return G3D_F_ALFA;
    case GL_ONE_MINUS_SRC_ALPHA: return G3D_F_UM_MENOS_ALFA;
    case GL_SRC_COLOR: return G3D_F_COR_ORIG;
    case GL_ONE_MINUS_SRC_COLOR: return G3D_F_UM_MENOS_COR_ORIG;
    case GL_DST_COLOR: return G3D_F_COR_DEST;
    case GL_ONE_MINUS_DST_COLOR: return G3D_F_UM_MENOS_COR_DEST;
    /* el destino principal no tiene alfa: el del destino vale 1 */
    case GL_DST_ALPHA: return C->alvo_alfa ? G3D_F_ALFA_DEST : G3D_F_UM;
    case GL_ONE_MINUS_DST_ALPHA: return C->alvo_alfa ? G3D_F_UM_MENOS_ALFA_DEST : G3D_F_ZERO;
    default: return G3D_F_ALFA_SATURADO;
    }
}

static unsigned char es_g3d_cb(int f)
{
    switch (f) {
    case GL_REPLACE: return G3D_CB_SUBSTITUIR;
    case GL_MODULATE: return G3D_CB_MODULAR;
    case GL_ADD: return G3D_CB_SOMAR;
    case GL_ADD_SIGNED: return G3D_CB_SOMAR_SINAL;
    case GL_INTERPOLATE: return G3D_CB_INTERPOLAR;
    case GL_SUBTRACT: return G3D_CB_SUBTRAIR;
    case GL_DOT3_RGB: return G3D_CB_DOT3_RGB;
    default: return G3D_CB_DOT3_RGBA;
    }
}

static unsigned char es_g3d_ft(int s) { return s == GL_TEXTURE ? G3D_FT_TEXTURA : s == GL_CONSTANT ? G3D_FT_CONSTANTE : s == GL_PRIMARY_COLOR ? G3D_FT_PRIMARIA : G3D_FT_ANTERIOR; }
static unsigned char es_g3d_opd(int o) { return o == GL_SRC_COLOR ? G3D_OPD_COR : o == GL_ONE_MINUS_SRC_COLOR ? G3D_OPD_UM_MENOS_COR : o == GL_SRC_ALPHA ? G3D_OPD_ALFA : G3D_OPD_UM_MENOS_ALFA; }
static unsigned char es_g3d_env(int w) { return w == GL_REPEAT ? G3D_ENV_REPETIR : w == GL_MIRRORED_REPEAT_OES ? G3D_ENV_ESPELHO : G3D_ENV_BORDE; }
static unsigned char es_g3d_est(int o) { return o == GL_ZERO ? G3D_EST_ZERO : o == GL_REPLACE ? G3D_EST_SUBSTITUIR : o == GL_INCR ? G3D_EST_INCR : o == GL_DECR ? G3D_EST_DECR : o == GL_INVERT ? G3D_EST_INVERTER : G3D_EST_MANTER; }
static uint32_t es_argb(const float *c) { return ((uint32_t)es_byte(c[3]) << 24) | ((uint32_t)es_byte(c[0]) << 16) | ((uint32_t)es_byte(c[1]) << 8) | (uint32_t)es_byte(c[2]); }

static int es_pot2(int n) { return n > 0 && (n & (n - 1)) == 0; }

static int es_e2_v3;     /* D457: el estado se calcula para un dibujo v3 */
static int es_neb_cpu;   /* D451: la niebla de este dibujo v2 va mezclada en el vértice (el driver no la tiene) */

/* El estado de este dibujo en gpu3d v2 (sin los huecos de textura). 0: no cabe en lo anunciado. */
static int es_estado2(gpu3d_estado2_t *e)
{
    uint8_t *p = (uint8_t *)e;
    for (unsigned i = 0; i < sizeof *e; i++) p[i] = 0;
    uint32_t b = C->gpu_bits, falta = 0;
    e->versao = 2;
    if (C->teste_z) { e->flags |= G3D2_Z_TESTE; if (C->masc_z) e->flags |= G3D2_Z_ESCRITA; }
    e->func_z = (unsigned char)(C->func_z - GL_NEVER);
    if (C->teste_z && C->func_z != GL_LESS && C->func_z != GL_LEQUAL && C->func_z != GL_ALWAYS) falta |= G3D_CAP_Z_FUNCS;
    if (C->alfa_on && !(gpu_diag & 2)) { e->flags |= G3D2_ALFA_TESTE; falta |= G3D_CAP_ALFA_TESTE; }
    e->func_alfa = (unsigned char)(C->alfa_func - GL_NEVER); e->ref_alfa = (unsigned char)es_byte(C->alfa_ref);
    e->mascara_cor = (unsigned char)C->masc_cor;
    if ((C->masc_cor & 7) != 7) falta |= G3D_CAP_MASCARAS;
    if (C->blend) {
        e->flags |= G3D2_MEZCLA;
        e->mez_orig_rgb = es_g3d_fator(C->blend_s, 1); e->mez_dest_rgb = es_g3d_fator(C->blend_d, 0);
        e->mez_orig_alfa = es_g3d_fator(C->blend_sa, 1); e->mez_dest_alfa = es_g3d_fator(C->blend_da, 0);
        e->mez_eq_rgb = (unsigned char)(C->blend_eq == GL_FUNC_SUBTRACT_OES ? G3D_EQ_SUBTRAI : C->blend_eq == GL_FUNC_REVERSE_SUBTRACT_OES ? G3D_EQ_SUBTRAI_INV : G3D_EQ_SOMA);
        e->mez_eq_alfa = (unsigned char)(C->blend_eqa == GL_FUNC_SUBTRACT_OES ? G3D_EQ_SUBTRAI : C->blend_eqa == GL_FUNC_REVERSE_SUBTRACT_OES ? G3D_EQ_SUBTRAI_INV : G3D_EQ_SOMA);
        if (e->mez_orig_rgb > 3 || e->mez_dest_rgb > 3 || e->mez_orig_alfa > 3 || e->mez_dest_alfa > 3) falta |= G3D_CAP_MEZCLA_EXT;
        if (e->mez_orig_rgb != e->mez_orig_alfa || e->mez_dest_rgb != e->mez_dest_alfa || e->mez_eq_rgb != e->mez_eq_alfa) falta |= G3D_CAP_MEZCLA_SEP;
        if (e->mez_eq_rgb || e->mez_eq_alfa) falta |= G3D_CAP_MEZCLA_EQ;
    }
    if (C->log_on) { e->flags |= G3D2_LOGICA; e->op_logica = (unsigned char)(C->log_op - GL_CLEAR); falta |= G3D_CAP_LOGICA; }
    if (C->est_on && (C->ebuf || !C->sem_estencil)) {
        e->flags |= G3D2_ESTENCIL; falta |= G3D_CAP_ESTENCIL;
        e->est_func = (unsigned char)(C->est_func - GL_NEVER); e->est_ref = (unsigned char)C->est_ref;
        e->est_mascara = (unsigned char)C->est_masc; e->est_escrita = (unsigned char)C->est_escr;
        e->est_falha = es_g3d_est(C->est_falha); e->est_zfalha = es_g3d_est(C->est_zfalha); e->est_zpassa = es_g3d_est(C->est_zpassa);
    }
    if (C->tij_on) {
        int x0, y0, x1, y1;
        caixa_es(&x0, &y0, &x1, &y1, 0);
        if (x1 < x0) x1 = x0;
        if (y1 < y0) y1 = y0;
        e->flags |= G3D2_TIJERA; falta |= G3D_CAP_TIJERA;
        e->tijera_x = (unsigned short)x0; e->tijera_y = (unsigned short)y0; e->tijera_l = (unsigned short)(x1 - x0); e->tijera_a = (unsigned short)(y1 - y0);
    }
    /* D451: sin niebla en el driver (la X1400 no la anuncia), con una sola unidad en MODULATE o sin textura la
     * niebla se mezcla con la CPU en el color y el especular del vértice, como en v1:
     * (tex × cor) × f + neblina × (1 - f) = tex × (cor × f) + (neblina × (1 - f)). */
    es_neb_cpu = gpu_neb_cpu && C->fog && !(b & G3D_CAP_NEBLINA) &&
                 (EN == 0 || (EN == 1 && !EU[0].cubo && C->un[0].env_modo == GL_MODULATE));
    if (C->fog && !es_neb_cpu) { e->flags |= G3D2_NEBLINA; falta |= G3D_CAP_NEBLINA; }
    if (C->fog && !es_neb_cpu)
        e->cor_neblina = ((uint32_t)es_byte(C->fog_cor[0] / 255.0f) << 16) | ((uint32_t)es_byte(C->fog_cor[1] / 255.0f) << 8) | (uint32_t)es_byte(C->fog_cor[2] / 255.0f);
    e->unidades = (unsigned char)EN;
    if ((uint32_t)EN > C->gpu_unid) return 0;
    for (int i = 0; i < EN; i++) {
        const es_unid_t *x = &EU[i];
        gpu3d_unidade_t *u = &e->u[i];
        if (!x->t) continue;
        const textura_t *f = x->cubo ? &x->t->faces[0] : x->t;
        u->ativa = 1; u->tipo = x->cubo ? G3D_TEX_CUBO : G3D_TEX_2D; u->coord = (unsigned char)i;
        u->env_s = x->cubo ? G3D_ENV_BORDE : es_g3d_env(f->wrap_s); u->env_t = x->cubo ? G3D_ENV_BORDE : es_g3d_env(f->wrap_t);
        if (u->env_s == G3D_ENV_ESPELHO || u->env_t == G3D_ENV_ESPELHO) falta |= G3D_CAP_TEX_ESPELHO;
        if (!es_pot2(f->larg) || !es_pot2(f->alt)) falta |= G3D_CAP_TEX_NPOT;
        if (x->cubo) falta |= G3D_CAP_TEX_CUBO;
        u->filtro = 0;
        if (f->filtro_mag == GL_LINEAR) u->filtro |= G3D_FIL_LINEAR_MAG;
        if (f->filtro_min == GL_LINEAR || f->filtro_min == GL_LINEAR_MIPMAP_NEAREST || f->filtro_min == GL_LINEAR_MIPMAP_LINEAR) u->filtro |= G3D_FIL_LINEAR_MIN;
        if ((b & G3D_CAP_TEX_MIPMAP) && (f->filtro_min == GL_NEAREST_MIPMAP_NEAREST || f->filtro_min == GL_LINEAR_MIPMAP_NEAREST)) u->filtro |= G3D_FIL_MIP_VECINO;
        if ((b & G3D_CAP_TEX_MIPMAP) && (f->filtro_min == GL_NEAREST_MIPMAP_LINEAR || f->filtro_min == GL_LINEAR_MIPMAP_LINEAR)) u->filtro |= G3D_FIL_MIP_LINEAR;
        u->aniso = (unsigned char)(f->aniso > 1.0f && (b & G3D_CAP_TEX_ANISO) ? (f->aniso > 16.0f ? 16 : (int)f->aniso) : 1);
        u->rgb_func = es_g3d_cb(x->rgb_f); u->alfa_func = es_g3d_cb(x->alfa_f);
        u->rgb_escala = (unsigned char)x->esc_rgb; u->alfa_escala = (unsigned char)x->esc_alfa;
        for (int k = 0; k < 3; k++) {
            u->rgb_fonte[k] = es_g3d_ft(x->rgb_src[k]); u->rgb_opd[k] = es_g3d_opd(x->rgb_opd[k]);
            u->alfa_fonte[k] = es_g3d_ft(x->alfa_src[k]); u->alfa_opd[k] = es_g3d_opd(x->alfa_opd[k]);
        }
        u->constante = es_argb(x->cc);
        int classico = x->rgb_f == GL_REPLACE || x->rgb_f == GL_MODULATE;
        classico = classico && (x->alfa_f == GL_REPLACE || x->alfa_f == GL_MODULATE) && x->esc_rgb == 1.0f && x->esc_alfa == 1.0f;
        if (!classico) falta |= G3D_CAP_COMBINAR;
        if (x->rgb_f == GL_DOT3_RGB || x->rgb_f == GL_DOT3_RGBA) falta |= G3D_CAP_DOT3;
        if (x->tex_cor_zero) falta |= G3D_CAP_COMBINAR;   /* textura GL_ALPHA en COMBINE: el driver no lo sabe */
        {
            /* D457: en v3 la matriz de textura la aplica la GPU; sin fila de proyección la q sigue siendo 1 */
            const float *tm = C->un[i].mat[C->un[i].mat_n - 1];
            int afin = tm[3] == 0.0f && tm[7] == 0.0f && tm[11] == 0.0f && tm[15] == 1.0f;
            if (!mat_e_identidade(tm) && !(es_e2_v3 && afin)) falta |= G3D_CAP_TEX_PROJ;
        }
    }
    return (b & G3D_CAP_V2) && !(falta & ~b);
}

/* ¿Cómo va este dibujo? 0 software, 1 GPU v1, 2 GPU v2. Lleva la cuenta del cuadro. */
static int es_gpu_preparar2(gpu3d_estado2_t *e);

static int es_gpu_escolher(GLenum modo)
{
    (void)modo;
    if (!C->gpu || C->fbo) return 0;     /* un FBO va por software, aparte del cuadro de la GPU */
    es_gpu_capas();
    gpu3d_estado2_t e;
    int m = es_cabe_v1() && !(gpu_diag & 1) ? 1 : es_estado2(&e) ? 2 : 0;
    if (C->gpu_sw) { if (!m) C->gpu_coube = 0; return 0; }
    if (!m) { es_gpu_a_software(); return 0; }
    if (m == 1) { es_gpu_despejar2(); return 1; }
    if (!es_gpu_preparar2(&e)) { es_gpu_a_software(); return 0; }
    return 2;
}

/* Un vértice v1 a partir de uno de la tubería completa. */
static void es_gvert1(const vert_t *v, const float *tc, gvert_t *o)
{
    float iw = 1.0f / v->w;
    float nx = v->x * iw, ny = v->y * iw, nz = v->z * iw;
    float sx = (float)C->vx + (nx + 1.0f) * 0.5f * (float)C->vw;
    float sy = (float)(C->alt - C->vy) - (ny + 1.0f) * 0.5f * (float)C->vh;
    float zw = es_zjanela(nz);
    if (!(sx >= 0)) sx = 0;
    if (sx > (float)C->larg) sx = (float)C->larg;
    if (!(sy >= 0)) sy = 0;
    if (sy > (float)C->alt) sy = (float)C->alt;
    if (!(iw >= 1.0e-6f)) iw = 1.0e-6f;
    if (iw > 1.0e6f) iw = 1.0e6f;
    o->x = sx; o->y = sy; o->z = zw; o->rhw = iw;
    float r = v->r, g = v->g, b = v->b, a = v->a, er = 0, eg = 0, eb = 0;
    if (C->luz_on && !C->luz_v7) {
        float m = v->luz * (1.0f / 255.0f);
        r *= m * C->luz_dif[0] + C->luz_amb[0];
        g *= m * C->luz_dif[1] + C->luz_amb[1];
        b *= m * C->luz_dif[2] + C->luz_amb[2];
    } else if (C->luz_v7) { er = v->sr; eg = v->sg; eb = v->sb; }
    if (EN == 1 && C->un[0].env_modo == GL_REPLACE) {
        /* REPLACE en v1: el color del vértice a blanco (el alfa, según el formato) */
        int base = EU[0].t->formato_base;
        if (base != GL_ALPHA) r = g = b = 255.0f;
        if (base == GL_RGBA || base == GL_LUMINANCE_ALPHA || base == GL_ALPHA) a = 255.0f;
        er = eg = eb = 0;
    }
    if (C->fog) {
        float f = C->fog_modo ? v->ff : (C->fog_fim - zw) / (C->fog_fim - C->fog_ini);
        f = sat01(f);
        r *= f; g *= f; b *= f;
        er = er * f + C->fog_cor[0] * (1.0f - f); eg = eg * f + C->fog_cor[1] * (1.0f - f); eb = eb * f + C->fog_cor[2] * (1.0f - f);
    }
    o->cor = (gpu_byte(a) << 24) | (gpu_byte(r) << 16) | (gpu_byte(g) << 8) | gpu_byte(b);
    o->espec = (gpu_byte(er) << 16) | (gpu_byte(eg) << 8) | gpu_byte(eb);
    float tu = 0, tv = 0;
    if (tc && EN == 1) { float q = tc[3] != 0.0f ? tc[3] : 1.0f; tu = tc[0] / q; tv = tc[1] / q; }
    if (!(tu > -100.0f)) tu = -100.0f;
    if (tu > 100.0f) tu = 100.0f;
    if (!(tv > -100.0f)) tv = -100.0f;
    if (tv > 100.0f) tv = 100.0f;
    o->u = tu; o->v = tv;
}

static void es_gpu_poligono(const ves_t *p, int n)
{
    gvert_t s[ES_POL_MAX];
    if (n > ES_POL_MAX) n = ES_POL_MAX;
    for (int i = 0; i < n; i++) es_gvert1(&p[i].b, p[i].tc[0], &s[i]);
    float area = 0;
    for (int i = 0; i < n; i++) { const gvert_t *a = &s[i], *b = &s[(i + 1) % n]; area += a->x * b->y - b->x * a->y; }
    if (area == 0) return;
    int frente = C->frente == GL_CCW ? area < 0 : area > 0;
    if (C->cull) {
        if (C->cull_face == GL_FRONT_AND_BACK) return;
        if (C->cull_face == GL_BACK && !frente) return;
        if (C->cull_face == GL_FRONT && frente) return;
    }
    gvert_t *g = gpu_sitio((n - 2) * 3);
    if (!g) return;
    for (int i = 1; i + 1 < n; i++) { *g++ = s[0]; *g++ = s[i]; *g++ = s[i + 1]; }
}

static void es_gpu_quad(gvert_t c[4])
{
    for (int i = 0; i < 4; i++) {
        if (c[i].x < 0) c[i].x = 0;
        if (c[i].x > (float)C->larg) c[i].x = (float)C->larg;
        if (c[i].y < 0) c[i].y = 0;
        if (c[i].y > (float)C->alt) c[i].y = (float)C->alt;
    }
    gvert_t *g = gpu_sitio(6);
    if (!g) return;
    g[0] = c[0]; g[1] = c[1]; g[2] = c[2]; g[3] = c[0]; g[4] = c[2]; g[5] = c[3];
}

static void es_gpu_ponto(const ves_t *o, float sx, float sy, float zw, float tam, float alfa)
{
    gvert_t b;
    es_gvert1(&o->b, o->tc[0], &b);
    b.z = zw;
    uint32_t a = (uint32_t)((float)(b.cor >> 24) * alfa + 0.5f);
    b.cor = (b.cor & 0x00FFFFFFu) | (a > 255 ? 255u : a) << 24;
    int lado = (int)(tam + 0.5f);
    if (lado < 1) lado = 1;
    float x0 = (float)((lado & 1) ? piso_f(sx) - (lado - 1) / 2 : piso_f(sx + 0.5f) - lado / 2);
    float yw = (float)C->alt - sy;
    int yb = (lado & 1) ? piso_f(yw) - (lado - 1) / 2 : piso_f(yw + 0.5f) - lado / 2;
    float y0 = (float)(C->alt - (yb + lado));
    gvert_t c[4] = { b, b, b, b };
    c[0].x = x0; c[0].y = y0; c[1].x = x0 + (float)lado; c[1].y = y0;
    c[2].x = x0 + (float)lado; c[2].y = y0 + (float)lado; c[3].x = x0; c[3].y = y0 + (float)lado;
    if (C->sprite_on && EN == 1 && C->un[0].coord_replace) {
        float it = 1.0f / tam, u0 = 0.5f + (x0 - sx) * it, v0 = 0.5f + (y0 - sy) * it, u1 = u0 + (float)lado * it, v1 = v0 + (float)lado * it;
        c[0].u = u0; c[0].v = v0; c[1].u = u1; c[1].v = v0; c[2].u = u1; c[2].v = v1; c[3].u = u0; c[3].v = v1;
    }
    es_gpu_quad(c);
}

static void es_gpu_linha(const ves_t *a, const ves_t *b)
{
    gvert_t p, q;
    es_gvert1(&a->b, a->tc[0], &p); es_gvert1(&b->b, b->tc[0], &q);
    float m = (float)(int)(C->linha_larg + 0.5f);
    if (m < 1.0f) m = 1.0f;
    m *= 0.5f;
    float dx = q.x - p.x, dy = q.y - p.y, d = raiz(dx * dx + dy * dy);
    float nx = m, ny = 0.0f;
    if (d > 0.0f) { nx = -dy / d * m; ny = dx / d * m; }
    else { q.x += 1.0f; nx = 0.0f; ny = m; }
    gvert_t c[4] = { p, p, q, q };
    c[0].x += nx; c[0].y += ny; c[1].x -= nx; c[1].y -= ny;
    c[2].x -= nx; c[2].y -= ny; c[3].x += nx; c[3].y += ny;
    es_gpu_quad(c);
}

/* ---- gpu3d v2: el lote de vértices v2, su estado y las texturas de cada unidad ---- */

#define GV2_MAX GPU3D_MAX_VERT   /* D451: lotes grandes (eran 504): menos llamadas al kernel */
static gpu3d_vert2_t *es_v2;     /* el lote (uno por proceso, como el motor) */
static int es_n2;
static gpu3d_estado2_t es_e2;

/* D457: el último ESTADO2 que aceptó el kernel (los dibujos v3 solo lo mandan si cambia) y lo de v3 */
static gpu3d_estado2_t es_k2;
static int es_k2_ok;
static void es_v3_reiniciar(void);
static void es_gpu_soltar2(void) { es_n2 = 0; if (es_v2) { SGL_LIBERAR(es_v2); es_v2 = 0; } es_k2_ok = 0; es_v3_reiniciar(); }

/* Manda el lote v2. Si el kernel dice que no, el resto del cuadro va por software y este
 * contexto deja v2 (lo que ya estaba en el lote se pierde en este cuadro, como en v1). */
static void es_gpu_despejar2(void)
{
    if (!C || !C->gpu || !es_n2 || !es_v2) return;
    int n = es_n2;
    es_n2 = 0;
    int r = gpu_chamar(GPU3D_OP_ESTADO2, 0, 0, 0, &es_e2);
    es_k2_ok = r == 0;
    if (r == 0) es_k2 = es_e2;
    unsigned tk = C->perfil > 0 ? SGL_MS() : 0;
    if (r == 0) r = gpu_chamar(GPU3D_OP_TRIANGULOS2, (unsigned)n, 0, 0, es_v2);
    if (C->perfil > 0) C->pf_ms_kernel += SGL_MS() - tk;
    C->pf_lotes++; C->pf_vert += (unsigned)n;
    if (r < 0) { gpu_cair(C); return; }
    if (r > 0) { C->gpu_bits = 0; es_gpu_a_software(); return; }
    C->gpu_sujo = 1;
}

/* ¿Hacen falta los niveles de mipmap en la GPU? */
static int es_quer_mip(const textura_t *f)
{
    return (C->gpu_bits & G3D_CAP_TEX_MIPMAP) && f->filtro_min != GL_NEAREST && f->filtro_min != GL_LINEAR && f->n_niveis > 1;
}

static uint32_t es_chave_tex(const es_unid_t *x)
{
    uint32_t k = x->t->geracao;
    if (x->cubo) for (int f = 0; f < 6; f++) k += x->t->faces[f].geracao * (uint32_t)(f + 3);
    if (es_quer_mip(x->cubo ? &x->t->faces[0] : x->t)) k ^= 0x80000000u;
    return k;
}

/* Un nivel de una cara, por GPU3D_OP_TEXTURA2 (ARGB). */
static int es_subir2(int vaga, int cubo, int face, int nivel, int niveis, const textura_t *t)
{
    int l = t->nivel_l[nivel], a = t->nivel_a[nivel], n = l * a;
    const unsigned char *px = t->nivel_px[nivel];
    if (!px) return 1;
    uint32_t *argb = SGL_ALOCAR((size_t)n * 4);
    if (!argb) return -1;
    for (int i = 0; i < n; i++)
        argb[i] = ((uint32_t)px[i * 4 + 3] << 24) | ((uint32_t)px[i * 4] << 16) | ((uint32_t)px[i * 4 + 1] << 8) | px[i * 4 + 2];
    gpu3d_textura2_t d;
    uint8_t *q = (uint8_t *)&d;
    for (unsigned i = 0; i < sizeof d; i++) q[i] = 0;
    d.vaga = (unsigned)vaga; d.formato = G3D_FMT_ARGB8888; d.tipo = cubo ? G3D_TEX_CUBO : G3D_TEX_2D; d.face = (unsigned)face;
    d.nivel = (unsigned)nivel; d.niveis = (unsigned)niveis; d.larg = (unsigned)l; d.alt = (unsigned)a;
    d.dados = (unsigned)(unsigned long)argb; d.bytes = (unsigned)n * 4;
#ifdef SGL_GPU_FALSA
    sgl_falsa_dados = argb;
#endif
    int r = gpu_chamar(GPU3D_OP_TEXTURA2, 0, 0, 0, &d);
    SGL_LIBERAR(argb);
    if (r == 0) { C->est.tex_subidas++; C->est.tex_kb_subidos += (uint32_t)((n * 4 + 1023) / 1024); }
    return r;
}

/* El hueco de la GPU con la textura de la unidad x (cubos y mipmaps por TEXTURA2; lo demás como v1). */
static int es_gpu_vaga(const es_unid_t *x)
{
    const textura_t *f0 = x->cubo ? &x->t->faces[0] : x->t;
    if (!x->cubo && !es_quer_mip(f0)) return gpu_textura_vaga((textura_t *)x->t);
    int ti = (int)(x->t - C->tex);
    uint32_t k = es_chave_tex(x);
    for (int s = 0; s < C->gpu_vagas; s++)
        if (C->vaga_tex[s] == ti && C->vaga_ger[s] == k) { C->vaga_uso[s] = ++C->relogio; C->est.tex_reusos++; return s; }
    int s = 0;
    for (int i = 0; i < C->gpu_vagas; i++) {
        if (C->vaga_tex[i] < 0) { s = i; break; }
        if (C->vaga_uso[i] < C->vaga_uso[s]) s = i;
    }
    C->vaga_tex[s] = -1;
    int niveis = es_quer_mip(f0) ? f0->n_niveis : 1;
    for (int fc = 0; fc < (x->cubo ? 6 : 1); fc++) {
        const textura_t *f = x->cubo ? &x->t->faces[fc] : x->t;
        if (!f->nivel_px[0]) ((textura_t *)f)->nivel_px[0] = f->px, ((textura_t *)f)->nivel_l[0] = f->larg, ((textura_t *)f)->nivel_a[0] = f->alt;
        for (int nv = 0; nv < niveis; nv++)
            if (es_subir2(s, x->cubo, fc, nv, niveis, f) != 0) return -1;
    }
    C->vaga_tex[s] = ti; C->vaga_ger[s] = k; C->vaga_uso[s] = ++C->relogio;
    return s;
}

/* Deja listo el estado v2 de este dibujo (con los huecos): 0 si no se pudo. */
static int es_gpu_preparar2(gpu3d_estado2_t *e)
{
    /* si alguna textura no está ya en la GPU, lo pendiente sale antes (subir puede soltar un hueco) */
    int falta = 0;
    for (int i = 0; i < EN; i++) {
        if (!EU[i].t) continue;
        int ti = (int)(EU[i].t - C->tex), ok = 0;
        uint32_t k = es_chave_tex(&EU[i]);
        int simples = !EU[i].cubo && !es_quer_mip(EU[i].t);
        for (int s = 0; s < C->gpu_vagas; s++)
            if (C->vaga_tex[s] == ti && (simples ? C->vaga_ger[s] == EU[i].t->geracao : C->vaga_ger[s] == k)) ok = 1;
        if (!ok) falta = 1;
    }
    if (falta || C->gpu_n) gpu_despejar();
    if (!C->gpu || C->gpu_sw) return 0;
    for (int i = 0; i < EN; i++) {
        if (!EU[i].t) continue;
        int s = es_gpu_vaga(&EU[i]);
        if (s < 0) return 0;
        e->u[i].vaga = (unsigned char)s;
    }
    const uint8_t *a = (const uint8_t *)e, *b = (const uint8_t *)&es_e2;
    int igual = 1;
    for (unsigned i = 0; i < sizeof *e; i++) if (a[i] != b[i]) { igual = 0; break; }
    if (!igual) { es_gpu_despejar2(); es_e2 = *e; }
    if (!es_v2) { es_v2 = SGL_ALOCAR(GV2_MAX * sizeof(gpu3d_vert2_t)); if (!es_v2) return 0; }
    return 1;
}

static gpu3d_vert2_t *es_sitio2(int n)
{
    if (es_n2 + n > gpu_lote) es_gpu_despejar2();
    if (!C->gpu || C->gpu_sw || !es_v2) return 0;
    gpu3d_vert2_t *g = es_v2 + es_n2;
    es_n2 += n;
    return g;
}

static uint32_t es_fbits(float f) { union { float f; uint32_t u; } x; x.f = f; return x.u; }

/* Un vértice v2: lo de v1 sin la niebla mezclada (va en el alfa del especular) y (s, t, r, q) por unidad. */
static void es_gvert2(const ves_t *o, gpu3d_vert2_t *g, float *zw_out, float *sx_out, float *sy_out)
{
    const vert_t *v = &o->b;
    float iw = 1.0f / v->w;
    float sx = (float)C->vx + (v->x * iw + 1.0f) * 0.5f * (float)C->vw;
    float sy = (float)(C->alt - C->vy) - (v->y * iw + 1.0f) * 0.5f * (float)C->vh;
    float zw = es_zjanela(v->z * iw);
    if (!(sx >= 0)) sx = 0;
    if (sx > (float)C->larg) sx = (float)C->larg;
    if (!(sy >= 0)) sy = 0;
    if (sy > (float)C->alt) sy = (float)C->alt;
    if (!(iw >= 1.0e-6f)) iw = 1.0e-6f;
    if (iw > 1.0e6f) iw = 1.0e6f;
    float r = v->r, gg = v->g, b = v->b, er = 0, eg = 0, eb = 0, f = 1.0f;
    if (C->luz_on && !C->luz_v7) {
        float m = v->luz * (1.0f / 255.0f);
        r *= m * C->luz_dif[0] + C->luz_amb[0];
        gg *= m * C->luz_dif[1] + C->luz_amb[1];
        b *= m * C->luz_dif[2] + C->luz_amb[2];
    } else if (C->luz_v7) { er = v->sr; eg = v->sg; eb = v->sb; }
    if (C->fog) f = sat01(C->fog_modo ? v->ff : (C->fog_fim - zw) / (C->fog_fim - C->fog_ini));
    if (es_neb_cpu) {   /* D451: como en v1 (es_gvert1) */
        r *= f; gg *= f; b *= f;
        er = er * f + C->fog_cor[0] * (1.0f - f); eg = eg * f + C->fog_cor[1] * (1.0f - f); eb = eb * f + C->fog_cor[2] * (1.0f - f);
        f = 1.0f;
    }
    g->x = es_fbits(sx); g->y = es_fbits(sy); g->z = es_fbits(zw); g->rhw = es_fbits(iw);
    g->cor = (gpu_byte(v->a) << 24) | (gpu_byte(r) << 16) | (gpu_byte(gg) << 8) | gpu_byte(b);
    g->espec = (gpu_byte(f * 255.0f) << 24) | (gpu_byte(er) << 16) | (gpu_byte(eg) << 8) | gpu_byte(eb);
    for (int u = 0; u < EN; u++)
        for (int k = 0; k < 4; k++) {
            float t = o->tc[u][k];
            if (!(t > -30000.0f)) t = -30000.0f;
            if (t > 30000.0f) t = 30000.0f;
            g->tc[u][k] = es_fbits(t);
        }
    for (int u = EN; u < 4; u++) { g->tc[u][0] = g->tc[u][1] = g->tc[u][2] = 0; g->tc[u][3] = 0x3F800000u; }   /* (0, 0, 0, 1) */
    if (zw_out) *zw_out = zw;
    if (sx_out) *sx_out = sx;
    if (sy_out) *sy_out = sy;
}

static float es_fdes(uint32_t b) { union { float f; uint32_t u; } x; x.u = b; return x.f; }

static void es_gpu_poligono2(const ves_t *p, int n)
{
    gpu3d_vert2_t s[ES_POL_MAX];
    float z[ES_POL_MAX], x[ES_POL_MAX], y[ES_POL_MAX];
    if (n > ES_POL_MAX) n = ES_POL_MAX;
    for (int i = 0; i < n; i++) es_gvert2(&p[i], &s[i], &z[i], &x[i], &y[i]);
    float area = 0;
    for (int i = 0; i < n; i++) { int j = (i + 1) % n; area += x[i] * y[j] - x[j] * y[i]; }
    if (area == 0) return;
    int frente = C->frente == GL_CCW ? area < 0 : area > 0;
    if (C->cull) {
        if (C->cull_face == GL_FRONT_AND_BACK) return;
        if (C->cull_face == GL_BACK && !frente) return;
        if (C->cull_face == GL_FRONT && frente) return;
    }
    if (C->off_on) {
        /* glPolygonOffset con la CPU: fator × pendiente máxima del z + unidades × el paso del Z */
        float x1 = x[1] - x[0], y1 = y[1] - y[0], x2 = x[2] - x[0], y2 = y[2] - y[0], d = x1 * y2 - x2 * y1;
        float m = 0;
        if (d != 0) {
            float dzx = ((z[1] - z[0]) * y2 - (z[2] - z[0]) * y1) / d, dzy = ((z[2] - z[0]) * x1 - (z[1] - z[0]) * x2) / d;
            if (dzx < 0) dzx = -dzx;
            if (dzy < 0) dzy = -dzy;
            m = dzx > dzy ? dzx : dzy;
        }
        float off = C->off_fator * m + C->off_unid * (C->zbits == 16 ? 1.0f / 65536.0f : 1.0f / 16777216.0f);
        for (int i = 0; i < n; i++) s[i].z = es_fbits(sat01(es_fdes(s[i].z) + off));
    }
    gpu3d_vert2_t *g = es_sitio2((n - 2) * 3);
    if (!g) return;
    for (int i = 1; i + 1 < n; i++) { *g++ = s[0]; *g++ = s[i]; *g++ = s[i + 1]; }
}

static void es_gpu_quad2(gpu3d_vert2_t c[4])
{
    for (int i = 0; i < 4; i++) {
        float x = es_fdes(c[i].x), y = es_fdes(c[i].y);
        if (x < 0) x = 0;
        if (x > (float)C->larg) x = (float)C->larg;
        if (y < 0) y = 0;
        if (y > (float)C->alt) y = (float)C->alt;
        c[i].x = es_fbits(x); c[i].y = es_fbits(y);
    }
    gpu3d_vert2_t *g = es_sitio2(6);
    if (!g) return;
    g[0] = c[0]; g[1] = c[1]; g[2] = c[2]; g[3] = c[0]; g[4] = c[2]; g[5] = c[3];
}

static void es_gpu_ponto2(const ves_t *o, float sx, float sy, float zw, float tam, float alfa)
{
    gpu3d_vert2_t b;
    es_gvert2(o, &b, 0, 0, 0);
    b.z = es_fbits(zw);
    uint32_t a = (uint32_t)((float)(b.cor >> 24) * alfa + 0.5f);
    b.cor = (b.cor & 0x00FFFFFFu) | (a > 255 ? 255u : a) << 24;
    int lado = (int)(tam + 0.5f);
    if (lado < 1) lado = 1;
    float x0 = (float)((lado & 1) ? piso_f(sx) - (lado - 1) / 2 : piso_f(sx + 0.5f) - lado / 2);
    float yw = (float)C->alt - sy;
    int yb = (lado & 1) ? piso_f(yw) - (lado - 1) / 2 : piso_f(yw + 0.5f) - lado / 2;
    float y0 = (float)(C->alt - (yb + lado));
    gpu3d_vert2_t c[4] = { b, b, b, b };
    float xs[4] = { x0, x0 + (float)lado, x0 + (float)lado, x0 }, ys[4] = { y0, y0, y0 + (float)lado, y0 + (float)lado };
    float it = 1.0f / tam;
    for (int i = 0; i < 4; i++) {
        c[i].x = es_fbits(xs[i]); c[i].y = es_fbits(ys[i]);
        for (int u = 0; u < EN; u++)
            if (C->sprite_on && C->un[u].coord_replace) {
                c[i].tc[u][0] = es_fbits(0.5f + (xs[i] - sx) * it); c[i].tc[u][1] = es_fbits(0.5f + (ys[i] - sy) * it);
                c[i].tc[u][2] = es_fbits(0.0f); c[i].tc[u][3] = es_fbits(1.0f);
            }
    }
    es_gpu_quad2(c);
}

static void es_gpu_linha2(const ves_t *a, const ves_t *b)
{
    gpu3d_vert2_t p, q;
    float px, py, qx, qy;
    es_gvert2(a, &p, 0, &px, &py); es_gvert2(b, &q, 0, &qx, &qy);
    float m = (float)(int)(C->linha_larg + 0.5f);
    if (m < 1.0f) m = 1.0f;
    m *= 0.5f;
    float dx = qx - px, dy = qy - py, d = raiz(dx * dx + dy * dy);
    float nx = m, ny = 0.0f;
    if (d > 0.0f) { nx = -dy / d * m; ny = dx / d * m; }
    else { qx += 1.0f; nx = 0.0f; ny = m; }
    gpu3d_vert2_t c[4] = { p, p, q, q };
    float xs[4] = { px + nx, px - nx, qx - nx, qx + nx }, ys[4] = { py + ny, py - ny, qy - ny, qy + ny };
    for (int i = 0; i < 4; i++) { c[i].x = es_fbits(xs[i]); c[i].y = es_fbits(ys[i]); }
    es_gpu_quad2(c);
}

/* glClear con lo que v1 no sabe (máscaras, tijera, valor de Z, esténcil): por LIMPAR2 si el driver lo tiene. */
static int es_gpu_limpar2(GLbitfield m)
{
    es_gpu_capas();
    uint32_t b = C->gpu_bits;
    if (!(b & G3D_CAP_V2) || !(b & G3D_CAP_LIMPAR2)) return 0;
    if ((m & GL_STENCIL_BUFFER_BIT) && (!(b & G3D_CAP_ESTENCIL) || C->sem_estencil)) return 0;
    if (C->tij_on && !(b & G3D_CAP_TIJERA)) return 0;
    if ((m & GL_COLOR_BUFFER_BIT) && (C->masc_cor & 7) != 7 && !(b & G3D_CAP_MASCARAS)) return 0;
    gpu3d_limpar2_t l;
    uint8_t *q = (uint8_t *)&l;
    for (unsigned i = 0; i < sizeof l; i++) q[i] = 0;
    l.mascara = (m & GL_COLOR_BUFFER_BIT ? G3D_LIMPA_COR : 0) | (m & GL_DEPTH_BUFFER_BIT && C->masc_z ? G3D_LIMPA_Z : 0) |
                (m & GL_STENCIL_BUFFER_BIT ? G3D_LIMPA_EST : 0);
    l.cor = 0xFF000000u | C->cor_limpa;
    l.z = C->z_limpo >= 1.0f ? 0xFFFFFFu : (unsigned)(C->z_limpo * 16777215.0f + 0.5f);
    l.estencil = (unsigned)C->est_limpo;
    l.mascara_cor = (unsigned char)(C->masc_cor | 8);
    l.mascara_est = (unsigned char)C->est_escr;
    if (C->tij_on) {
        int x0, y0, x1, y1;
        caixa_es(&x0, &y0, &x1, &y1, 0);
        if (x1 < x0) x1 = x0;
        if (y1 < y0) y1 = y0;
        l.tijera = 1; l.tx = (unsigned short)x0; l.ty = (unsigned short)y0; l.tl = (unsigned short)(x1 - x0); l.ta = (unsigned short)(y1 - y0);
    }
    gpu_despejar();
    if (!C->gpu) return 0;
    if (!l.mascara) return 1;
    int r = gpu_chamar(GPU3D_OP_LIMPAR2, 0, 0, 0, &l);
    if (r < 0) { gpu_cair(C); return 0; }
    if (r > 0) return 0;
    C->gpu_sujo = 1;
    return 1;
}

/* Fin de cuadro (glClear de color y Z): vuelta a la GPU si el cuadro por software habría cabido. */
/* ------------------------------------------------------------ gpu3d v3 (D457): la GPU guarda y transforma los vértices
 *
 * Si el driver anuncia G3D_C3_V3, un glDrawArrays/glDrawElements cuyos arrays están todos en búferes (VBO) y
 * cuyo estado cabe en lo anunciado no pasa por la tubería de la CPU: los búferes se suben una vez a la memoria de
 * la GPU (y solo otra vez cuando cambian: glBufferData entero, glBufferSubData el trozo) y cada dibujo son tres
 * llamadas cortas como mucho (estado de píxeles si cambió, transformación si cambió, dibujar). Lo que no cabe
 * (arrays del cliente, luces, planos de recorte, desplazamiento del polígono, sombreado plano, generación de
 * coordenadas, puntos y líneas, tipos que no sean float o el color de 4 bytes...) va como siempre, por la CPU
 * (v2/v1/software), dibujo a dibujo: las dos formas pintan en el mismo destino de la GPU. */

static struct { GLuint id; uint32_t ger, rel; int uso; } es_vb[GPU3D_VBUFS];
static uint32_t es_vb_rel;
_Static_assert(__builtin_offsetof(gpu3d_transf_t, cor) == 113 * 4 && __builtin_offsetof(gpu3d_transf_t, neb_cor) == 135 * 4,
               "es_v3_desenhar mira que las palabras 2..134 (salvo cor) sean finitas");
static gpu3d_transf_t es_t3;       /* la última transformación que aceptó el kernel */
static int es_t3_ok, es_v3_recusas;
/* Revisión de D457 (Amilo): si lo que se dibuja en un cuadro no cabe en la memoria de la GPU para búferes, cada dibujo
 * expulsa a otro y se vuelve a subir (Brumacanto con 4 MB: 165 000 expulsiones, mucho más lento que sin v3). Las subidas
 * por expulsión de cada cuadro se cuentan; tras ES_V3_MAUS cuadros seguidos con más de ES_V3_EXP_MAX, v3 se deja (la
 * CPU transforma, como sin v3) y se sueltan los búferes. */
#define ES_V3_EXP_MAX 24
#define ES_V3_MAUS    3
static int es_v3_exp_q, es_v3_maus;

static void es_v3_reiniciar(void)
{
    for (int i = 0; i < GPU3D_VBUFS; i++) es_vb[i].id = 0;
    es_t3_ok = 0; es_v3_recusas = 0; es_v3_exp_q = es_v3_maus = 0;
}

/* ¿Sigue siendo de b ese hueco? */
static int es_vb_tem(const vbo_t *b, int uso)
{
    int s = b->vaga3[uso] - 1;
    return s >= 0 && es_vb[s].id == b->id && es_vb[s].uso == uso ? s : -1;
}

static void es_vb_soltar(vbo_t *b)
{
    for (int uso = 0; uso < 2; uso++) {
        int s = es_vb_tem(b, uso);
        b->vaga3[uso] = 0;
        if (s < 0) continue;
        es_vb[s].id = 0;
        if (C && C->gpu) {
            gpu3d_vbuf_t q = { (unsigned)s, (unsigned)uso, 0, 0, 0, 0, 0, 0 };
            gpu_chamar(GPU3D_OP_VBUF, 0, 0, 0, &q);
        }
    }
}

/* El contenido de b cambió. Un trozo de un búfer que está al día en la GPU se sube ya (glBufferSubData); si no
 * se puede, o cambió entero, la copia caduca y se vuelve a subir con el próximo dibujo que lo use. */
static void es_vb_mudou(vbo_t *b, int desl, int tam)
{
    uint32_t antes = b->ger++;
    if (!b->ger) b->ger = 1;
    if (tam <= 0 || !C || !C->gpu || !b->dados) return;
    for (int uso = 0; uso < 2; uso++) {
        int s = es_vb_tem(b, uso);
        if (s < 0 || es_vb[s].ger != antes) continue;
        int d0 = desl, n = tam;
        if (uso == 1) { n += d0 & 1; d0 &= ~1; n = (n + 1) & ~1; if (d0 + n > (b->tam & ~1)) n = (b->tam & ~1) - d0; }
        if (n <= 0) { es_vb[s].ger = b->ger; continue; }
        gpu3d_vbuf_t q = { (unsigned)s, (unsigned)uso, (unsigned)b->tam, (unsigned)d0, (unsigned)n, (unsigned)(unsigned long)(b->dados + d0), 0, 0 };
#ifdef SGL_GPU_FALSA
        sgl_falsa_dados = b->dados + d0;
#endif
        if (gpu_chamar(GPU3D_OP_VBUF, 0, 0, 0, &q) == 0) es_vb[s].ger = b->ger;
    }
}

/* El hueco de la GPU con el contenido actual de b (lo sube si hace falta). -1: no se puede (ese dibujo, por la CPU). */
static int es_vb_vaga(vbo_t *b, int uso)
{
    if (!b || !b->dados || b->tam < 4 || b->tam > (16 << 20) || b->mapeado) return -1;
    int s = es_vb_tem(b, uso);
    if (s >= 0 && es_vb[s].ger == b->ger) { es_vb[s].rel = ++es_vb_rel; return s; }
    if (s < 0) {
        /* uno libre o, si no, el que lleva más tiempo sin usarse */
        s = 0;
        for (int i = 0; i < GPU3D_VBUFS; i++) {
            if (!es_vb[i].id) { s = i; break; }
            if (es_vb[i].rel < es_vb[s].rel) s = i;
        }
    }
    es_vb[s].id = 0;
    unsigned n = uso == 1 ? (unsigned)b->tam & ~1u : (unsigned)b->tam;
    gpu3d_vbuf_t q = { (unsigned)s, (unsigned)uso, (unsigned)b->tam, 0, n, (unsigned)(unsigned long)b->dados, G3D_VB_NOVO, 0 };
#ifdef SGL_GPU_FALSA
    sgl_falsa_dados = b->dados;
#endif
    unsigned tk = C->perfil > 0 ? SGL_MS() : 0;
    int r = gpu_chamar(GPU3D_OP_VBUF, 0, 0, 0, &q);
    if (C->perfil > 0) C->pf_ms_kernel += SGL_MS() - tk;
    if (r < 0) { gpu_cair(C); return -1; }
    if (r > 0) return -1;
    es_vb[s].id = b->id; es_vb[s].ger = b->ger; es_vb[s].rel = ++es_vb_rel; es_vb[s].uso = uso;
    b->vaga3[uso] = (short)(s + 1);
    return s;
}

/* Un array de floats en un búfer como atributo v3. 0: no vale. */
static int es_v3_atrib(gpu3d_atrib_t *a, vbo_t **dono, GLuint vbo, const void *ptr, int tipo, int tam, int passo, int formato)
{
    vbo_t *b = vbo_por_id(vbo);
    size_t desl = (size_t)ptr;
    if (!b || tipo != GL_FLOAT || passo < 4 || passo > 252 || (passo & 3) || (desl & 3) || desl >= (size_t)(16 << 20)) return 0;
    (void)tam;
    a->formato = (unsigned char)formato; a->passo = (unsigned char)passo; a->desloc = (unsigned)desl; a->vaga = 0;
    *dono = b;
    return 1;
}

static int es_v3_desenhar(GLenum modo, int primeiro, int n, const void *idx, GLenum tipo)
{
    if (!C->gpu || C->fbo || C->gpu_sw) return 0;
    es_gpu_capas();
    uint32_t b3 = C->gpu_bits3;
    if (!(b3 & G3D_C3_V3) || n < 3 || primeiro < 0) return 0;
    unsigned prim;
    if (modo == GL_TRIANGLES) { prim = G3D_PRIM_TRIANGULOS; n -= n % 3; }
    else if ((modo == GL_TRIANGLE_STRIP || modo == GL_TRIANGLE_FAN) && (b3 & G3D_C3_TIRAS) && n <= GPU3D_MAX_VERT3)
        prim = modo == GL_TRIANGLE_STRIP ? G3D_PRIM_TIRA : G3D_PRIM_ABANICO;
    else return 0;
    /* lo que v3 no lleva */
    if (C->luz_on || C->plano || C->off_on || C->planos || C->paleta_on) return 0;
    if (C->cull && C->cull_face == GL_FRONT_AND_BACK) return 0;
    if (C->fog) {
        if (!(b3 & G3D_C3_NEBLINA) || !C->fog_modo) return 0;
        if (C->fog_modo != GL_LINEAR && !(b3 & G3D_C3_NEB_EXP)) return 0;
        if (C->fog_modo == GL_LINEAR && C->fog_fim == C->fog_ini) return 0;
    }
    if (C->vx < 0 || C->vy < 0 || C->vw < 1 || C->vh < 1 || C->vx + C->vw > C->larg || C->vy + C->vh > C->alt) return 0;
    if (!(C->z_perto >= 0.0f && C->z_perto <= 1.0f && C->z_longe >= 0.0f && C->z_longe <= 1.0f)) return 0;
    /* los arrays: todos en búferes */
    static gpu3d_transf_t t;
    uint8_t *tz = (uint8_t *)&t;
    for (unsigned i = 0; i < sizeof t; i++) tz[i] = 0;
    gpu3d_desenho3_t d;
    tz = (uint8_t *)&d;
    for (unsigned i = 0; i < sizeof d; i++) tz[i] = 0;
    vbo_t *dono[7] = { 0, 0, 0, 0, 0, 0, 0 };          /* pos, color, tc[0..3], índices */
    gpu3d_atrib_t *at[6] = { &d.pos, &d.cor_a, &d.tc[0], &d.tc[1], &d.tc[2], &d.tc[3] };
    if (!C->v_ativo || !C->v_vbo || !es_v3_atrib(&d.pos, &dono[0], C->v_vbo, C->v_ptr, C->v_tipo, C->v_tam, C->v_passo, C->v_tam)) return 0;
    if (C->c_ativo && (C->c_vbo || C->c_ptr)) {
        if (!C->c_vbo || C->c_tam != 4) return 0;
        if (C->c_tipo == GL_UNSIGNED_BYTE) { int tp = GL_FLOAT; if (!es_v3_atrib(&d.cor_a, &dono[1], C->c_vbo, C->c_ptr, tp, 4, C->c_passo, G3D_AT_UBYTE4N)) return 0; }
        else if (C->c_tipo == GL_FLOAT && (b3 & G3D_C3_COR_FLOAT)) { if (!es_v3_atrib(&d.cor_a, &dono[1], C->c_vbo, C->c_ptr, GL_FLOAT, 4, C->c_passo, G3D_AT_FLOAT4)) return 0; }
        else return 0;
    } else t.cor = (gpu_byte(C->cor[3]) << 24) | (gpu_byte(C->cor[0]) << 16) | (gpu_byte(C->cor[1]) << 8) | gpu_byte(C->cor[2]);
    es_preparar_unidades();
    for (int u = 0; u < EN; u++) {
        const unid_t *un = &C->un[u];
        for (int k = 0; k < 4; k++) t.tc_const[u][k] = es_fbits(un->tc[k]);
        if (!EU[u].t) continue;
        if (un->gen_on) return 0;
        /* una q que puede no ser 1 pide coordenadas proyectivas en el driver (como en v2) */
        if (!(C->gpu_bits & G3D_CAP_TEX_PROJ) && ((un->t_ativo && (un->t_vbo || un->t_ptr)) ? un->t_tam == 4 : un->tc[3] != 1.0f)) return 0;
        if (un->t_ativo && (un->t_vbo || un->t_ptr)) {
            if (!un->t_vbo || !es_v3_atrib(&d.tc[u], &dono[2 + u], un->t_vbo, un->t_ptr, un->t_tipo, un->t_tam, un->t_passo, un->t_tam)) return 0;
        }
        const float *tm = un->mat[un->mat_n - 1];
        if (!mat_e_identidade(tm)) {
            if (!(b3 & G3D_C3_TEX_MAT)) return 0;
            t.flags |= G3D3_TEX_MAT(u);
            for (int k = 0; k < 16; k++) t.tex[u][k] = es_fbits(tm[k]);
        }
    }
    for (int u = EN; u < 4; u++) t.tc_const[u][3] = 0x3F800000u;
    /* los índices */
    unsigned primeiro3 = (unsigned)primeiro;
    if (idx) {
        vbo_t *bi = C->vbo_elem ? vbo_por_id(C->vbo_elem) : 0;
        if (!bi || !bi->dados || tipo != GL_UNSIGNED_SHORT || !(b3 & G3D_C3_INDICES)) return 0;
        size_t desl = (size_t)((const uint8_t *)idx - bi->dados);
        if ((desl & 1) || desl + (size_t)n * 2 > (size_t)bi->tam) return 0;
        dono[6] = bi; primeiro3 = (unsigned)(desl / 2);
    }
    /* la parte de los píxeles: el estado v2 de siempre (texturas subidas, lo pendiente de v1/v2 entregado) */
    gpu3d_estado2_t e;
    es_e2_v3 = 1;
    int cabe = es_estado2(&e);
    es_e2_v3 = 0;
    if (!cabe) return 0;
    gpu_despejar();
    if (!C->gpu || C->gpu_sw || !es_gpu_preparar2(&e) || !C->gpu) return 0;
    /* los búferes, en la GPU */
    int vg[7];
    for (int tentativa = 0; ; tentativa++) {
        for (int k = 0; k < 7; k++) {
            vg[k] = -1;
            if (!dono[k]) continue;
            vg[k] = es_vb_vaga(dono[k], k == 6);
            if (vg[k] < 0 || !C->gpu) return 0;
        }
        /* subir uno puede haber quitado el hueco a otro de este mismo dibujo (todos ocupados): se mira otra vez */
        int todos = 1;
        for (int k = 0; k < 7; k++) if (dono[k] && es_vb_tem(dono[k], k == 6) != vg[k]) todos = 0;
        if (todos) break;
        if (tentativa >= 2) return 0;
    }
    for (int k = 0; k < 6; k++) if (dono[k]) at[k]->vaga = (unsigned short)vg[k];
    /* la transformación */
    t.versao = 3;
    const float *M = C->mv[C->mv_n - 1], *P = C->proj[C->proj_n - 1];
    for (int col = 0; col < 4; col++)
        for (int l = 0; l < 4; l++) {
            float v = P[l] * M[col * 4] + P[4 + l] * M[col * 4 + 1] + P[8 + l] * M[col * 4 + 2] + P[12 + l] * M[col * 4 + 3];
            t.mvp[col * 4 + l] = es_fbits(v);
            t.mv[col * 4 + l] = es_fbits(M[col * 4 + l]);
        }
    for (int k = 0; k < 9; k++) t.normal[k] = es_fbits(k % 4 == 0 ? 1.0f : 0.0f);
    t.normal_const[2] = 0x3F800000u;
    t.vp_escala[0] = es_fbits((float)C->vw * 0.5f); t.vp_desloc[0] = es_fbits((float)C->vx + (float)C->vw * 0.5f);
    t.vp_escala[1] = es_fbits(-(float)C->vh * 0.5f); t.vp_desloc[1] = es_fbits((float)(C->alt - C->vy) - (float)C->vh * 0.5f);
    t.vp_escala[2] = es_fbits((C->z_longe - C->z_perto) * 0.5f); t.vp_desloc[2] = es_fbits((C->z_longe + C->z_perto) * 0.5f);
    if (C->cull) {
        int frente_neg = C->frente == GL_CCW;            /* la cara de frente tiene el área negativa en la pantalla */
        int tira_frente = C->cull_face == GL_FRONT;
        t.descarte = (unsigned char)(frente_neg != tira_frente ? G3D_DESC_POS : G3D_DESC_NEG);
    }
    if (C->fog) {
        t.flags |= G3D3_NEBLINA | (es_neb_cpu ? G3D3_NEB_MEZCLADA : 0);
        if (C->fog_modo == GL_LINEAR) {
            float d = C->fog_fim - C->fog_ini;
            t.neb_modo = G3D_NEB_LINEAR; t.neb_a = es_fbits(C->fog_fim / d); t.neb_b = es_fbits(1.0f / d);
        } else { t.neb_modo = (unsigned char)(C->fog_modo == GL_EXP ? G3D_NEB_EXP : G3D_NEB_EXP2); t.neb_b = es_fbits(C->fog_dens); }
        t.neb_cor = (gpu_byte(C->fog_cor[0]) << 16) | (gpu_byte(C->fog_cor[1]) << 8) | gpu_byte(C->fog_cor[2]);
    }
    /* matrices no finitas: el kernel las rechazaría */
    for (unsigned i = 2; i < 135; i++) if (i != 113 && ((((const uint32_t *)&t)[i] >> 23) & 0xFF) == 0xFF) return 0;
    unsigned tk = C->perfil > 0 ? SGL_MS() : 0;
    int r = 0;
    {
        const uint8_t *a = (const uint8_t *)&e, *b = (const uint8_t *)&es_k2;
        int igual = es_k2_ok;
        for (unsigned i = 0; i < sizeof e && igual; i++) if (a[i] != b[i]) igual = 0;
        if (!igual) { r = gpu_chamar(GPU3D_OP_ESTADO2, 0, 0, 0, &e); es_k2_ok = r == 0; es_k2 = e; }
    }
    if (r == 0) {
        const uint8_t *a = (const uint8_t *)&t, *b = (const uint8_t *)&es_t3;
        int igual = es_t3_ok;
        for (unsigned i = 0; i < sizeof t && igual; i++) if (a[i] != b[i]) igual = 0;
        if (!igual) { r = gpu_chamar(GPU3D_OP_TRANSF, 0, 0, 0, &t); es_t3_ok = r == 0; es_t3 = t; }
    }
    int feitos = 0;
    while (r == 0 && feitos < n) {
        int m = n - feitos > GPU3D_MAX_VERT3 ? GPU3D_MAX_VERT3 : n - feitos;
        d.modo = prim; d.primeiro = primeiro3 + (unsigned)feitos; d.n = (unsigned)m; d.indices = dono[6] ? (unsigned)vg[6] : GPU3D_SEM_INDICES;
        r = gpu_chamar(GPU3D_OP_DESENHAR3, 0, 0, 0, &d);
        if (r == GPU3D_EXPULSO && !feitos) {
            /* el kernel expulsó alguno de estos búferes para hacer sitio: se suben otra vez y se repite, una vez */
            es_v3_exp_q++;
            for (int k = 0; k < 7; k++) if (dono[k] && vg[k] >= 0) es_vb[vg[k]].id = 0;
            int ok = 1;
            /* pueden caer en otros huecos (el primero libre): el dibujo va con los nuevos (revisión de D457) */
            for (int k = 0; k < 7 && ok; k++) if (dono[k] && ((vg[k] = es_vb_vaga(dono[k], k == 6)) < 0 || !C->gpu)) ok = 0;
            for (int k = 0; k < 7 && ok; k++) if (dono[k] && es_vb_tem(dono[k], k == 6) != vg[k]) ok = 0;
            if (ok) {
                for (int k = 0; k < 6; k++) if (dono[k]) at[k]->vaga = (unsigned short)vg[k];
                d.indices = dono[6] ? (unsigned)vg[6] : GPU3D_SEM_INDICES;
            }
            r = ok ? gpu_chamar(GPU3D_OP_DESENHAR3, 0, 0, 0, &d) : GPU3D_SEM_SUPORTE;
        }
        if (r == 0) feitos += m;
    }
    if (C->perfil > 0) C->pf_ms_kernel += SGL_MS() - tk;
    if (r < 0) { gpu_cair(C); return 0; }
    if (r > 0 && !feitos) {
        /* «eso no»: este dibujo, por la CPU; si pasa una y otra vez, v3 se deja */
        if (!C->gpu) return 0;
        if (++es_v3_recusas >= 32) C->gpu_bits3 = 0;
        return 0;
    }
    es_v3_recusas = 0;
    C->gpu_sujo = 1;
    C->pf_lotes++; C->pf_vert += (unsigned)n;
    C->est.triangulos += (uint32_t)(prim == G3D_PRIM_TRIANGULOS ? n / 3 : n - 2);
    return 1;
}

static void es_gpu_quadro_novo(GLbitfield m)
{
    if (!C->gpu || C->fbo || (m & (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)) != (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)) return;
    if (C->gpu_sw && C->gpu_coube) C->gpu_sw = 0;
    C->gpu_coube = 1;
    /* D457: el freno de las expulsiones (ver ES_V3_EXP_MAX) */
    es_v3_maus = es_v3_exp_q > ES_V3_EXP_MAX ? es_v3_maus + 1 : 0;
    es_v3_exp_q = 0;
    if (es_v3_maus >= ES_V3_MAUS && C->gpu_bits3) {
        C->gpu_bits3 = 0;
        for (int i = 0; i < GPU3D_VBUFS; i++) {
            if (!es_vb[i].id) continue;
            es_vb[i].id = 0;
            gpu3d_vbuf_t q = { (unsigned)i, (unsigned)es_vb[i].uso, 0, 0, 0, 0, 0, 0 };
            gpu_chamar(GPU3D_OP_VBUF, 0, 0, 0, &q);
        }
    }
}
#endif

/* ------------------------------------------------------------ arrays de vértices (ES 1.1) */

void glVertexPointer(GLint tam, GLenum tipo, GLsizei passo, const void *p)
{
    if (!C) return;
    if (!es_tipo_valido(tipo, 1, 0, 1, 1, 1)) { erro(GL_INVALID_ENUM); return; }
    if (tam < 2 || tam > 4 || passo < 0) { erro(GL_INVALID_VALUE); return; }
    C->v_tam = tam; C->v_tipo = (int)tipo; C->v_passo = passo ? passo : tam * es_tam_tipo(tipo); C->v_ptr = p; C->v_vbo = C->vbo_array;
}

void glColorPointer(GLint tam, GLenum tipo, GLsizei passo, const void *p)
{
    if (!C) return;
    if (!es_tipo_valido(tipo, 0, 1, 0, 1, 1)) { erro(GL_INVALID_ENUM); return; }
    if (tam < 3 || tam > 4 || passo < 0) { erro(GL_INVALID_VALUE); return; }   /* ES pide 4; SaguiGL aceptaba 3 */
    C->c_tam = tam; C->c_tipo = (int)tipo; C->c_passo = passo ? passo : tam * es_tam_tipo(tipo); C->c_ptr = p; C->c_vbo = C->vbo_array;
}

void glNormalPointer(GLenum tipo, GLsizei passo, const void *p)
{
    if (!C) return;
    if (!es_tipo_valido(tipo, 1, 0, 1, 1, 1)) { erro(GL_INVALID_ENUM); return; }
    if (passo < 0) { erro(GL_INVALID_VALUE); return; }
    C->n_tipo = (int)tipo; C->n_passo = passo ? passo : 3 * es_tam_tipo(tipo); C->n_ptr = p; C->n_vbo = C->vbo_array;
}

void glTexCoordPointer(GLint tam, GLenum tipo, GLsizei passo, const void *p)
{
    if (!C) return;
    if (!es_tipo_valido(tipo, 1, 1, 1, 1, 1)) { erro(GL_INVALID_ENUM); return; }   /* GL_UNSIGNED_BYTE: de SaguiGL v6 */
    if (tam < 2 || tam > 4 || passo < 0) { erro(GL_INVALID_VALUE); return; }
    unid_t *u = &UNID_CLI;
    u->t_tam = tam; u->t_tipo = (int)tipo; u->t_passo = passo ? passo : tam * es_tam_tipo(tipo); u->t_ptr = p; u->t_vbo = C->vbo_array;
}

static int es_estado_cliente(GLenum m, int v)
{
    switch (m) {
    case GL_VERTEX_ARRAY: C->v_ativo = v; return 1;
    case GL_COLOR_ARRAY: C->c_ativo = v; return 1;
    case GL_TEXTURE_COORD_ARRAY: UNID_CLI.t_ativo = v; return 1;
    case GL_NORMAL_ARRAY: C->n_ativo = v; if (v) { C->luz_nova = 1; refaz_extras(); } return 1;
    case GL_POINT_SIZE_ARRAY_OES: C->ps_ativo = v; return 1;
    case GL_MATRIX_INDEX_ARRAY_OES: C->mi_ativo = v; return 1;
    case GL_WEIGHT_ARRAY_OES: C->pe_ativo = v; return 1;
    default: return 0;
    }
}

void glEnableClientState(GLenum m) { if (C && !es_estado_cliente(m, 1)) erro(GL_INVALID_ENUM); }
void glDisableClientState(GLenum m) { if (C && !es_estado_cliente(m, 0)) erro(GL_INVALID_ENUM); }

void glDrawElements(GLenum modo, GLsizei n, GLenum tipo, const void *indices)
{
    fonte_t f;
    if (!C) return;
    if (n < 0) { erro(GL_INVALID_VALUE); return; }
    if (tipo != GL_UNSIGNED_BYTE && tipo != GL_UNSIGNED_SHORT && tipo != GL_UNSIGNED_INT) { erro(GL_INVALID_ENUM); return; }
    if (C->vbo_elem) {
        vbo_t *b = vbo_por_id(C->vbo_elem);
        if (!b || !b->dados) return;
        indices = b->dados + (size_t)indices;
    }
    if (!indices || !fonte_do_contexto(&f)) return;
    desenhar(modo, 0, n, indices, tipo, &f);
}

void glCullFace(GLenum f)
{
    if (!C) return;
    if (f != GL_FRONT && f != GL_BACK && f != GL_FRONT_AND_BACK) { erro(GL_INVALID_ENUM); return; }
    GPU_DESPEJAR();
    C->cull_face = (int)f;
}
