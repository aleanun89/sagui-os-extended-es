// SPDX-License-Identifier: GPL-3.0-only
/* sgl_ri.h - la representación intermedia (RI) de los sombreadores de SaguiGL 2.0 (D475).
 *
 * Es la pieza común: el compilador de GLSL ES 1.00 (sgl_glsl.c) la produce, el intérprete de la CPU
 * (sgl_ri_cpu.c) la ejecuta y cada backend de GPU (VirGL/TGSI, R5xx, R7xx, Evergreen, Gen) la traducirá.
 * Está pensada para traducirse 1:1 a TGSI y no usa nada que una GPU de la época de ES 2.0 no tenga:
 *
 *  - registros vec4 de float en ficheros (TEMP, ENT, SAI, UNI, IMED, END = a0);
 *  - cada fuente con swizzle, negación y valor absoluto; cada destino con máscara y saturación;
 *    una fuente o el destino pueden ir indexados por a0.x (arrays con índice dinámico);
 *  - int y bool se guardan como float (0.0 / 1.0);
 *  - control de flujo estructurado (IF/ELSE/ENDIF, BGNLOOP/ENDLOOP, BRK, CONT), sin saltos libres ni llamadas;
 *  - las operaciones escalares (RCP, RSQ, EX2, LG2, SIN, COS, POW) leen la primera componente del swizzle
 *    y escriben el mismo valor en todas las de la máscara.
 *
 * Disposición: escalar y vector = 1 registro; matN = N registros (columnas); cada elemento de un array y
 * cada campo de una struct empieza en un registro. */
#ifndef SAGUI_SGL_RI_H
#define SAGUI_SGL_RI_H

#ifdef SGL_ANFITRIAO
#include <stdint.h>
#else
#include "sagui.h"
#endif

/* ---- ficheros de registros */
enum {
    RI_NULO = 0,
    RI_TEMP,       /* temporales */
    RI_ENT,        /* entradas: atributos (vértices) o varyings y valores del sistema (fragmentos) */
    RI_SAI,        /* salidas (solo se escriben) */
    RI_UNI,        /* uniformes (vec4 cada uno) */
    RI_IMED,       /* inmediatos (vec4 cada uno, en ri_prog_t.imed) */
    RI_END,        /* registro de dirección a0 (solo ARL lo escribe; se lee como índice) */
    RI_AMOST       /* unidad de muestreo (solo en TEX/TXB/TXL: ind = número de sampler del sombreador) */
};

/* ---- operaciones */
enum {
    RI_NOP = 0,
    /* por componente */
    RI_MOV, RI_ADD, RI_MUL, RI_MAD, RI_MIN, RI_MAX, RI_FLR, RI_FRC, RI_TRUNC,
    RI_SLT, RI_SGE, RI_SEQ, RI_SNE,      /* 1.0 si a < b (>=, ==, !=), si no 0.0 */
    RI_CMP,                              /* a < 0 ? b : c */
    /* productos escalares (el resultado va a todas las componentes de la máscara) */
    RI_DP2, RI_DP3, RI_DP4,
    /* escalares */
    RI_RCP, RI_RSQ, RI_EX2, RI_LG2, RI_SIN, RI_COS, RI_POW,
    /* derivadas (solo fragmentos) */
    RI_DDX, RI_DDY,
    /* a0 = floor(a) (por componente de la máscara) */
    RI_ARL,
    /* texturas: f[0] = coordenadas (x, y[, z]; w = sesgo en TXB y nivel en TXL), f[1] = RI_AMOST */
    RI_TEX, RI_TXB, RI_TXL,
    /* control de flujo */
    RI_IF,                               /* entra si f[0].x != 0 */
    RI_ELSE, RI_ENDIF, RI_BGNLOOP, RI_ENDLOOP, RI_BRK, RI_CONT,
    RI_KILL,                             /* descarta el fragmento (las vías activas) */
    RI_FIM,
    RI_N_OPS
};

/* alvo de una textura */
enum { RI_TEX_2D = 0, RI_TEX_CUBO = 1 };

/* modificadores de una fuente */
#define RI_NEG 1
#define RI_ABS 2

typedef struct {
    uint8_t  arq;       /* RI_* */
    uint8_t  sw;        /* swizzle: 2 bits por componente (x = bits 0-1) */
    uint8_t  mod;       /* RI_NEG | RI_ABS (abs antes que neg) */
    uint8_t  rel;       /* 1: el índice es ind + a0.x */
    uint16_t ind;
} ri_fonte_t;

typedef struct {
    uint8_t  arq;
    uint8_t  masc;      /* bits 1 x, 2 y, 4 z, 8 w */
    uint8_t  sat;       /* satura a [0, 1] */
    uint8_t  rel;
    uint16_t ind;
} ri_dest_t;

typedef struct {
    uint8_t    op;
    uint8_t    tex;     /* RI_TEX_2D / RI_TEX_CUBO */
    uint16_t   linha;   /* línea de la fuente (para depurar) */
    ri_dest_t  d;
    ri_fonte_t f[3];
    int        alvo;    /* control de flujo: IF -> ELSE o ENDIF; ELSE -> ENDIF; BGNLOOP -> ENDLOOP;
                         * ENDLOOP -> BGNLOOP; BRK/CONT -> ENDLOOP del bucle (ri_ligar) */
} ri_ins_t;

#define RI_SW(x, y, z, w) ((uint8_t)((x) | ((y) << 2) | ((z) << 4) | ((w) << 6)))
#define RI_SW_XYZW RI_SW(0, 1, 2, 3)
#define RI_SW_C(sw, c) (((sw) >> ((c) * 2)) & 3)

/* ---- semánticas de entradas y salidas */
enum {
    RI_SEM_ATRIB = 0,   /* entrada de vértices: atributo (idx = registro del atributo) */
    RI_SEM_POSICAO,     /* salida gl_Position / entrada gl_FragCoord */
    RI_SEM_PONTO_TAM,   /* gl_PointSize */
    RI_SEM_GENERICO,    /* varying (idx = orden en el sombreador; el enlace los empareja por nombre) */
    RI_SEM_COR,         /* gl_FragColor / gl_FragData[0] */
    RI_SEM_FACE,        /* gl_FrontFacing (x = 1.0 de frente, 0.0 de espaldas) */
    RI_SEM_PONTO_COORD  /* gl_PointCoord */
};
typedef struct { uint8_t sem, idx; } ri_es_t;

/* ---- tipos de las variables visibles (los mismos números que GL para glGetActiveUniform) */
typedef struct {
    char     nome[64];
    uint32_t tipo_gl;   /* GL_FLOAT_VEC3, GL_FLOAT_MAT4, GL_SAMPLER_2D... */
    int      arr;       /* 0 = no es array; si no, elementos */
    int      reg;       /* primer registro (UNI, ENT o SAI) o primer sampler (RI_AMOST) */
    int      nreg;      /* registros por elemento (matN: N) */
    int      comps;     /* componentes usadas de cada registro (vecN: N; matN: N) */
} ri_var_t;

#define RI_MAX_ENT  32
#define RI_MAX_SAI  32
#define RI_MAX_AMOST 16

typedef struct ri_prog {
    int        tipo;            /* 0 vértices, 1 fragmentos */
    ri_ins_t  *ins; int nins, cins;
    int        ntemp;           /* temporales */
    int        nuni;            /* registros de uniformes */
    float    (*imed)[4]; int nimed, cimed;
    int        nent, nsai;
    ri_es_t    ent[RI_MAX_ENT], sai[RI_MAX_SAI];
    int        namost;          /* samplers usados */
    uint8_t    amost_tipo[RI_MAX_AMOST];   /* RI_TEX_2D / RI_TEX_CUBO */
    ri_var_t  *unif; int nunif;  /* uniformes (las structs, campo a campo: "luz.cor", "l[1].cor") y samplers */
    ri_var_t  *attr; int nattr;  /* atributos (vértices) */
    ri_var_t  *vary; int nvary;  /* varyings (SAI en vértices, ENT en fragmentos) */
    int        usa_kill, usa_ddx, usa_a0, usa_ponto_coord, usa_face, usa_fragcoord;
    int        tarr_base, tarr_n; /* temporales de arrays con índice dinámico (no se compactan) */
} ri_prog_t;

/* ---- compilador (sgl_glsl.c) */
/* tipo: 0 vértices, 1 fragmentos. Devuelve NULL con el log en «erro» («ERROR: 0:LÍNEA: mensaje»). */
ri_prog_t *sgl_glsl_compilar(const char *fonte, int n, int tipo, char *erro, int max_erro);
void       ri_liberar(ri_prog_t *p);

/* ---- utilidades (sgl_ri.c) */
int  ri_ligar(ri_prog_t *p);                     /* resuelve los saltos del control de flujo; 0 si está mal anidado */
void ri_otimizar(ri_prog_t *p);                  /* quita copias y código muerto, compacta los temporales */
int  ri_validar(const ri_prog_t *p, char *erro, int max);   /* comprueba índices y anidamiento */
int  ri_texto(const ri_prog_t *p, char *s, int max);         /* volcado legible (devuelve la longitud) */
extern const char *const ri_nomes_ops[RI_N_OPS];

/* ---- intérprete por la CPU (sgl_ri_cpu.c) */
typedef struct ri_exec ri_exec_t;
/* amostrar: las 4 vías de una vez. coord[c][v] = componente c de la vía v; op = RI_TEX/TXB/TXL;
 * vivo = máscara de vías; out[c][v]. «quad»: las vías son un cuadrado de 2x2 (fragmentos: LOD por las
 * derivadas); si no, vértices (nivel 0 salvo TXL). */
typedef void (*ri_amostrar_fn)(void *ctx, int amost, int alvo, int op, float coord[4][4], int vivo, int quad, float out[4][4]);
ri_exec_t *ri_exec_criar(const ri_prog_t *p);
void       ri_exec_liberar(ri_exec_t *e);
/* uniformes: nuni registros de 4 float (se replican en las 4 vías; llamar cuando cambien) */
void       ri_exec_uniformes(ri_exec_t *e, const float (*uni)[4]);
/* registros de entrada y salida: [reg][comp][vía] */
float    (*ri_exec_ent(ri_exec_t *e))[4][4];
float    (*ri_exec_sai(ri_exec_t *e))[4][4];
/* ejecuta las vías de «vivo»; devuelve las que siguen vivas (KILL las quita) */
int        ri_exec_rodar(ri_exec_t *e, int vivo, ri_amostrar_fn amostrar, void *ctx);

#endif
