// SPDX-License-Identifier: GPL-3.0-only
/* sgl2.h - SaguiGL 2.0 (D475): OpenGL ES 2.0 por la CPU. Cabecera interna de la libsgl (sgl2.c, la API y
 * los objetos; sgl2_desenho.c, la tubería: vértices, recorte, rasterizado, fragmentos y texturas).
 *
 * Un contexto de ES 2.0 lleva debajo un sgl_ctx_t (el de ES 1.1) que solo da el destino: los píxeles de la
 * ventana o del pbuffer (0x00RRGGBB) y sgl_trocar(). Todo lo demás (texturas, búferes, FBO, programas,
 * Z y esténcil del framebuffer por defecto) es de aquí. Las filas de los framebuffers van de abajo arriba,
 * como en GL: el por defecto apunta a la última fila de los píxeles con paso negativo. */
#ifndef SAGUI_SGL2_H
#define SAGUI_SGL2_H

#include "../inc/GLES2/gl2.h"
#include "../inc/GLES2/gl2ext.h"
#include "sgl_ri.h"
#include "sgl2_base.h"

struct sgl_ctx;

#define S2_MAX_ATRIB    16
#define S2_MAX_UNID     16    /* unidades de textura (8 por etapa) */
#define S2_MAX_LADO     2048
#define S2_NIVEIS       12
#define S2_MAX_VARY     8
#define S2_MAX_UNIF_VEC 256

/* D480: el recurso de la GPU (gpu3d v4) que lleva el contenido de un objeto. «ver» es la versión del contenido de la CPU que
 * ya está en la GPU; «sujo»: la GPU tiene un contenido más nuevo que la CPU (hay que leerlo antes de usarlo en la CPU). */
typedef struct { uint32_t id, epoca, ver; int l, a, niv; uint8_t tipo, sujo, zest, zbits; } s2_gres_t;

/* px en 0xAARRGGBB, fila 0 = t 0. D486: una textura de profundidad (GL_OES_depth_texture, GL_OES_packed_depth_stencil) no tiene px:
 * guarda la profundidad en «z» (floats de 0 a 1, para que sea también el Z de un FBO) y, si es DEPTH24_STENCIL8, el esténcil en «est»;
 * «zb» son los bits de profundidad que dijo el programa (16, 24 o 32). */
typedef struct { int larg, alt; uint32_t *px; GLenum fmt; float *z; uint8_t *est; uint8_t zb; } s2_nivel_t;

typedef struct {
    GLuint     nome;
    GLenum     alvo;                     /* 0 hasta el primer glBindTexture */
    s2_nivel_t niv[6][S2_NIVEIS];        /* [cara][nivel] (2D: cara 0) */
    GLenum     min_f, mag_f, wrap_s, wrap_t;
    int        completa;                 /* -1 sin calcular, 0 o 1 */
    int        nniv;                     /* niveles que usa el muestreo (1 sin mipmaps) */
    uint32_t   ver;                      /* D480: versión de los píxeles de la CPU (sube con cada escritura) */
    s2_gres_t  g;
} s2_tex_t;

typedef struct { GLuint nome; uint8_t *dados; int tam; GLenum uso; int mapeado; } s2_buf_t;   /* D486: mapeado = glMapBufferOES activo */

typedef struct { GLuint nome; GLenum fmt; int larg, alt; uint32_t *cor; float *z; uint8_t *est; uint32_t ver; s2_gres_t g; } s2_rb_t;

typedef struct { GLenum tipo; GLuint nome; int nivel; int face; } s2_anexo_t;   /* tipo: GL_NONE, GL_TEXTURE, GL_RENDERBUFFER */
typedef struct { GLuint nome; s2_anexo_t cor, z, est; s2_gres_t gz; } s2_fbo_t;

typedef struct {
    GLuint      nome;
    GLenum      tipo;
    char       *fonte; int nfonte;
    int         compilado, apagar, ligados;
    char        log[512];
} s2_shader_t;

typedef struct {
    char   nome[96];                  /* el del elemento: «luz.cor», «m[2]» */
    GLenum tipo;
    int    tam;                       /* elementos (en la entrada del elemento 0; 1 en las demás) */
    int    reg[2];                    /* registro (UNI o sampler) en vértices y fragmentos; -1 = no está */
    int    nreg, comps;
    int    ativo;                     /* índice para glGetActiveUniform (solo el elemento 0) o -1 */
} s2_unif_t;

typedef struct {
    GLuint      nome;
    GLuint      vs, fs;               /* adjuntos */
    int         ligado, validado, apagar, em_uso;
    char        log[512];
    /* lo que deja glBindAttribLocation (vale en el próximo enlace) */
    char        bind_nome[S2_MAX_ATRIB][64]; int nbind; int bind_loc[S2_MAX_ATRIB];
    /* el ejecutable */
    ri_prog_t  *pv, *pf;
    ri_exec_t  *xv, *xf;
    float     (*uni[2])[4];            /* valores de los uniformes de cada etapa */
    int         uni_sujo;
    int         amost[2][RI_MAX_AMOST];   /* unidad de textura de cada sampler */
    s2_unif_t  *unif; int nunif, nativos;
    int         atr_loc[RI_MAX_ENT];   /* registro de entrada de vértices -> posición de atributo */
    int         nativos_at;            /* atributos activos */
    int         mapa_vary[RI_MAX_ENT]; /* entrada de fragmentos -> salida de vértices (-1: valor del sistema) */
    int         sai_pos, sai_ptam, sai_cor;
    int         vary_ent[RI_MAX_ENT], nvary;   /* entradas GENERICO de fragmentos (las que se interpolan) */
    int         ent_coord, ent_face, ent_pcoord;   /* entradas del sistema en fragmentos (-1) */
    int         gslot, gepoca, gnao;   /* D480: el programa en la GPU (hueco + 1), su época y «no se pudo traducir» */
} s2_prog_t;

typedef struct { int ativo, tam, norm, passo; GLenum tipo; const void *ptr; GLuint buf; } s2_atrib_t;
/* un atributo ya resuelto para un dibujo (sgl2_desenho.c): ptr NULL = el valor constante «atual» */
typedef struct { const uint8_t *ptr, *lim; int passo, tam, norm; GLenum tipo; const float *atual; } at_fonte_t;

/* un destino de dibujo ya resuelto */
typedef struct {
    uint32_t *cor; int passo;          /* fila y: cor + y * passo (0xAARRGGBB) */
    float    *z;   int passo_z;
    uint8_t  *est; int passo_e;
    int       larg, alt, alfa, zbits, estbits;
    s2_gres_t *g, *gz;                 /* D480: los recursos de la GPU de este destino */
    uint32_t  *ver;
    int        gpu_ok;                 /* el destino se puede dibujar en la GPU (nivel 0 y cara 0 de una textura, o un renderbuffer) */
} s2_alvo_t;

typedef struct sgl2_ctx {
    struct sgl_ctx *base;
    GLenum      erro;
    int         zbits_cfg, est_cfg;   /* los de la configuración de EGL */
    /* objetos: tablas por nombre (el índice es el nombre) */
    s2_tex_t  **tex; int ntex;
    s2_buf_t  **buf; int nbuf;
    s2_rb_t   **rb;  int nrb;
    s2_fbo_t  **fbo; int nfbo;
    void      **obj; uint8_t *obj_tipo; int nobj;   /* sombreadores (1) y programas (2): un mismo espacio de nombres */
    s2_tex_t    tex0[2];               /* las texturas 0 (2D y cubo) */
    /* estado */
    int         unid; GLuint un_tex[2][S2_MAX_UNID];   /* [0] 2D, [1] cubo */
    GLuint      buf_array, buf_elem, fbo_ligado, rb_ligado, prog_uso;
    s2_atrib_t  at[S2_MAX_ATRIB]; float atual[S2_MAX_ATRIB][4];
    int         vp[4]; float z_perto, z_longe;
    int         tij_on, tij[4];
    float       cor_limpa[4], z_limpo; int est_limpo;
    int         masc_cor[4], masc_z;
    int         cull_on; GLenum cull_face, frente;
    int         z_on; GLenum z_func;
    int         blend_on; GLenum b_src[2], b_dst[2], b_eq[2]; float b_cor[4];   /* [0] RGB, [1] alfa */
    int         est_on;
    struct { GLenum func; int ref; GLuint masc_v, masc_e; GLenum falha, zfalha, zpassa; } est[2];   /* [0] frente, [1] espaldas */
    int         off_on; float off_fator, off_unid;
    int         dither, alfa_cob, cob_on, cob_inv; float cob_valor;
    float       linha_larg;
    int         pack_alin, unpack_alin;
    GLenum      dica_mip, dica_deriv;
    /* framebuffer por defecto: Z y esténcil propios */
    float      *z_def; uint8_t *est_def; int def_larg, def_alt;
    /* tubería (sgl2_desenho.c) */
    float      *vsai; int vsai_cap;    /* vértices ya sombreados */
    int         fs_ativo;              /* el programa de fragmentos del dibujo */
    s2_prog_t  *pd;                    /* el programa del dibujo */
    s2_alvo_t   alvo;
    char        exts[160];
    /* D480: gpu3d v4 */
    uint32_t    def_ver;               /* versión de los píxeles del framebuffer por defecto */
    s2_gres_t   gdef, gdef_z;
    int         gpu_pend;              /* hay recursos con «sujo» */
} sgl2_ctx_t;

extern sgl2_ctx_t *sgl2_C;            /* el contexto de ES 2.0 actual */
extern void *sgl2_atual;              /* lo mismo, para el despacho (sgl2_despacho.c) */

/* sgl2.c */
void        sgl2_erro(GLenum e);
s2_tex_t   *sgl2_tex(GLuint nome);
s2_buf_t   *sgl2_buf(GLuint nome);
s2_rb_t    *sgl2_rb(GLuint nome);
s2_fbo_t   *sgl2_fbo(GLuint nome);
int         sgl2_tex_completa(s2_tex_t *t);
GLenum      sgl2_fbo_estado(sgl2_ctx_t *c);
int         sgl2_alvo(sgl2_ctx_t *c, s2_alvo_t *a);   /* 0 si el framebuffer no está completo; D480: antes lee lo que la GPU tenga pendiente */
int         sgl2_alvo_x(sgl2_ctx_t *c, s2_alvo_t *a, int para_gpu);   /* para_gpu: sin leer de la GPU */
void        sgl2_tex_mudou(s2_tex_t *t);             /* D480: la CPU escribió sus píxeles */
void        sgl2_uniformes_subir(s2_prog_t *p);

/* sgl2_desenho.c */
void        sgl2_desenhar(sgl2_ctx_t *c, GLenum modo, GLint primeiro, GLsizei n, GLenum tipo_idx, const void *idx);
void        sgl2_limpar(sgl2_ctx_t *c, GLbitfield m);
void        sgl2_at_preparar(sgl2_ctx_t *c, at_fonte_t *at);   /* D480: para sgl2_gpu.c */
void        sgl2_at_ler(const at_fonte_t *f, int i, float *o);

/* sgl2_gpu.c (D480): el backend de gpu3d v4. */
int         sgl2_gpu_desenhar(sgl2_ctx_t *c, GLenum modo, GLint primeiro, GLsizei n, GLenum tipo_idx, const void *idx);   /* 1: lo hizo la GPU */
int         sgl2_gpu_limpar(sgl2_ctx_t *c, GLbitfield m);                                                                  /* 1: lo hizo la GPU */
void        sgl2_gpu_sincronizar(sgl2_ctx_t *c);       /* trae lo que la GPU dibujó a los píxeles de la CPU */
void        sgl2_gpu_ctx_novo(sgl2_ctx_t *c);
void        sgl2_gpu_ctx_livre(sgl2_ctx_t *c);
void        sgl2_gpu_soltar(s2_gres_t *g);              /* suelta el recurso de la GPU de un objeto */
void        sgl2_gpu_prog_solto(s2_prog_t *p);
void        sgl2_gpu_prog_ligado(s2_prog_t *p);         /* en el anfitrión, con SGL2_TGSI en el entorno, vuelca el TGSI */
void        sgl2_gpu_modo(int modo);                    /* 0 solo CPU, 1 la GPU si hay gpu3d v4 (por defecto), 2 como 1 sin autoprueba */
int         sgl2_gpu_estado(void);                      /* 1 la GPU dibuja, 0 no (todavía), -1 no hay o falló */
void        sgl2_gpu_contadores(unsigned *des, unsigned *lim, unsigned *cpu);
void        sgl2_amostrar(void *ctx, int amost, int alvo, int op, float coord[4][4], int vivo, int quad, float out[4][4]);

/* para EGL (sgl_egl.c) y las pruebas */
sgl2_ctx_t *sgl2_criar(struct sgl_ctx *base, int zbits, int estbits);
void        sgl2_destruir(sgl2_ctx_t *c);
void        sgl2_usar(sgl2_ctx_t *c);   /* NULL: vuelve a ES 1.1 */
int         sgl_destino(struct sgl_ctx *c, uint32_t **px, int *larg, int *alt);   /* sgl.c */

#endif
