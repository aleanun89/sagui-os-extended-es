// SPDX-License-Identifier: GPL-3.0-or-later
/* llm.h - motor propio de modelos de lenguaje locales (D380, mejora 139).
 *
 * Un motor pequeño en C, al estilo de llama2.c, que lee modelos GGUF y los ejecuta
 * en la CPU. Empieza por la arquitectura LFM3 (bloques de convolución corta con
 * compuertas y atención con GQA, la mitad con ventana deslizante) y las
 * cuantizaciones de un Q4_K_M: Q4_K, Q6_K, Q8_0, F16 y F32. La caché K/V se guarda en
 * Q8_0 y el contexto se elige al arrancar (de serie 12k; 12k-48k).
 *
 * El mismo código compila para SaguiOS (libsagui, x87 y SSE2 elegido por CPUID) y para
 * el PC (LLM_ANFITRIAO, con la libc), para comparar los logits con llama.cpp. Lo que
 * depende de la plataforma (ficheros, memoria, tiempo, salida, hilos) está en
 * llm_plat.c; las funciones matemáticas, en llm_mat.c.
 *
 * Archivos:
 *   llm_plat.c        plataforma (SaguiOS o PC)
 *   llm_mat.c         expf, sqrtf, senos... (x87 en SaguiOS)
 *   llm_gguf.c        lector GGUF (metadatos y tensores)
 *   llm_quant.c       bloques cuantizados: decuantizar y productos punto (C puro)
 *   llm_quant_sse2.c  los mismos productos punto con SSE2 (solo si CPUID lo dice)
 *   llm_modelo.c      la red LFM3, la caché K/V Q8_0 y la multiplicación por lotes
 *   llm_token.c       tokenizador BPE byte a byte (gpt2) con el pretokenizador llama3
 *   llm_amostra.c     muestreo: temperatura, top-k, top-p, penalización, semilla
 *   llm_principal.c   el programa `llm` (línea de órdenes, chat, --info, servidor)
 */
#ifndef LLM_H
#define LLM_H

#ifdef LLM_ANFITRIAO
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdarg.h>
#else
#include "sagui.h"     /* tipos básicos y va_list de la libsagui */
#endif

/* ------------------------------------------------------------ plataforma */
void    *llm_aloca(size_t n);            /* NULL si no hay */
void    *llm_aloca_zero(size_t n);
void     llm_libera(void *p);
void    *llm_mapa(size_t n);             /* páginas grandes (pesos, caché); NULL si no hay */
void     llm_mapa_libera(void *p, size_t n);
int      llm_abrir(const char *caminho);              /* <0 = error */
long long llm_tamanho_fd(int fd);
int      llm_ler_em(int fd, unsigned long long desloc, void *buf, size_t n);   /* 0 = ok */
void     llm_fechar(int fd);
unsigned llm_ms(void);
void     llm_escrever(int fd, const char *s, size_t n);
void     llm_printf(const char *fmt, ...);            /* a la salida estándar */
void     llm_eprintf(const char *fmt, ...);           /* a la salida de errores */
int      llm_formatar(char *dst, size_t max, const char *fmt, ...);
size_t   llm_strlen(const char *s);
int      llm_strcmp(const char *a, const char *b);
int      llm_strncmp(const char *a, const char *b, size_t n);
void    *llm_memcpy(void *d, const void *s, size_t n);
void    *llm_memset(void *d, int v, size_t n);
int      llm_memcmp(const void *a, const void *b, size_t n);
bool     llm_cpu_sse2(void);
bool     llm_cpu_ssse3(void);
int      llm_cpus(void);                       /* CPU que trabajan (1 sin SMP) */
unsigned long long llm_ram_fisica(void);      /* bytes de RAM del PC (0 = no se sabe) */
unsigned long long llm_memoria_livre(void);   /* bytes que un programa aún puede pedir (aprox.) */
unsigned long long llm_espaco_processo(void); /* hueco de direcciones libre del proceso */
uint32_t llm_aleatorio(void);                 /* semilla por defecto */

/* Hilos: un grupo de trabajadores que repite una tarea por trozos. */
typedef void (*llm_tarefa_t)(void *ctx, int ini, int fim);
int      llm_hilos_iniciar(int n);       /* -> cuántos hay (1 = sin hilos) */
void     llm_hilos_parar(void);
void     llm_hilos_repartir(llm_tarefa_t f, void *ctx, int n);   /* f(ctx, ini, fim) en trozos de [0, n) */

/* ------------------------------------------------------------ matemática */
float llm_expf(float x);
float llm_sqrtf(float x);
float llm_logf(float x);
float llm_powf(float b, float e);
void  llm_sincosf(float x, float *s, float *c);
float llm_fabsf(float x);

/* ------------------------------------------------------------ GGUF */
enum {
    GGUF_UINT8 = 0, GGUF_INT8 = 1, GGUF_UINT16 = 2, GGUF_INT16 = 3, GGUF_UINT32 = 4, GGUF_INT32 = 5,
    GGUF_FLOAT32 = 6, GGUF_BOOL = 7, GGUF_STRING = 8, GGUF_ARRAY = 9, GGUF_UINT64 = 10, GGUF_INT64 = 11,
    GGUF_FLOAT64 = 12
};
/* Tipos de tensor de ggml que conocemos. */
enum { T_F32 = 0, T_F16 = 1, T_Q4_0 = 2, T_Q8_0 = 8, T_Q4_K = 12, T_Q6_K = 14, T_Q8_K = 15 };

typedef struct { const char *s; uint32_t n; } llm_texto_t;   /* texto del GGUF (sin cero final) */

typedef struct {
    llm_texto_t nome;
    uint32_t    tipo;            /* GGUF_* */
    uint32_t    arr_tipo;        /* si es GGUF_ARRAY */
    uint64_t    arr_n;
    const uint8_t *dados;        /* el valor (o los elementos del vector) dentro de la cabecera */
} gguf_kv_t;

typedef struct {
    llm_texto_t nome;
    uint32_t    ndim;
    uint64_t    ne[4];
    uint32_t    tipo;            /* T_* */
    uint64_t    desloc;          /* desde el inicio de los datos */
    uint64_t    bytes;
    const void *dados;           /* ya en memoria */
} gguf_tensor_t;

typedef struct {
    uint32_t    versao;
    uint8_t    *cab;             /* la cabecera entera (metadatos y lista de tensores) */
    size_t      cab_n;
    uint64_t    n_kv, n_tensores;
    gguf_kv_t  *kv;
    gguf_tensor_t *t;
    uint32_t    alinhamento;
    uint64_t    dados_ini;       /* posición de los datos en el fichero */
    uint64_t    dados_n;
    uint8_t    *dados;           /* los pesos (llm_mapa) */
    int         fd;
    char        erro[160];
} gguf_t;

int  gguf_abrir(gguf_t *g, const char *caminho);          /* solo la cabecera; 0 = ok */
int  gguf_carregar(gguf_t *g, void (*progresso)(unsigned pct));   /* los pesos a memoria; 0 = ok */
void gguf_fechar(gguf_t *g);
const gguf_kv_t *gguf_kv(const gguf_t *g, const char *nome);
bool gguf_u32(const gguf_t *g, const char *nome, uint32_t *v);
bool gguf_f32(const gguf_t *g, const char *nome, float *v);
bool gguf_texto(const gguf_t *g, const char *nome, llm_texto_t *v);
/* Elemento i de un vector de números (enteros o bool) -> uint32. */
bool gguf_arr_u32(const gguf_t *g, const char *nome, uint64_t i, uint32_t *v);
/* Recorre un vector de textos sin pasar de fim: devuelve el puntero al siguiente (NULL si no cabe). */
const uint8_t *gguf_arr_texto(const uint8_t *p, const uint8_t *fim, llm_texto_t *t);
gguf_tensor_t *gguf_tensor(const gguf_t *g, const char *nome);
const char *gguf_nome_tipo(uint32_t tipo);
size_t gguf_tamanho_linha(uint32_t tipo, uint64_t n);      /* bytes de una fila de n elementos */

/* ------------------------------------------------------------ cuantización */
#define QK_K 256
#define QK8_0 32
typedef struct { uint16_t d; int8_t qs[QK8_0]; } bloco_q8_0;            /* 34 bytes */
typedef struct { uint16_t d; uint8_t qs[QK8_0 / 2]; } bloco_q4_0;      /* 18 bytes */
typedef struct { float d; int8_t qs[QK_K]; int16_t bsums[QK_K / 16]; } bloco_q8_K;   /* 292 bytes */
typedef struct { uint16_t d, dmin; uint8_t scales[12]; uint8_t qs[QK_K / 2]; } bloco_q4_K;   /* 144 */
typedef struct { uint8_t ql[QK_K / 2]; uint8_t qh[QK_K / 4]; int8_t scales[QK_K / 16]; uint16_t d; } bloco_q6_K;   /* 210 */

float    llm_f16_f32_calc(uint16_t h);
/* f16 -> f32 por tabla (65536 floats, 256 KB, como llama.cpp): F16C es VEX y el kernel aún no
 * guarda el estado AVX (D381). La rellena llm_quant_escolher. */
extern float llm_f16_tab[65536];
static inline float llm_f16_f32(uint16_t h) { return llm_f16_tab[h]; }
uint16_t llm_f32_f16(float f);
void     llm_quant_q8_K(const float *x, bloco_q8_K *y, int n);
void     llm_quant_q8_0(const float *x, bloco_q8_0 *y, int n);
void     llm_decuant_linha(uint32_t tipo, const void *src, float *y, int n);
/* Productos punto: fila cuantizada (n elementos) con el vector cuantizado de entrada. */
typedef float (*llm_dot_t)(const void *w, const void *x, int n);
extern llm_dot_t llm_dot_q4_K, llm_dot_q6_K, llm_dot_q8_0, llm_dot_q4_0;
float llm_dot_q4_0_c(const void *w, const void *x, int n);
float llm_dot_q4_0_sse2(const void *w, const void *x, int n);
float llm_dot_q4_K_c(const void *w, const void *x, int n);
float llm_dot_q6_K_c(const void *w, const void *x, int n);
float llm_dot_q8_0_c(const void *w, const void *x, int n);
float llm_dot_q4_K_sse2(const void *w, const void *x, int n);
float llm_dot_q6_K_sse2(const void *w, const void *x, int n);
float llm_dot_q8_0_sse2(const void *w, const void *x, int n);
bool  llm_quant_escolher(const char *nome);   /* NULL = la mejor; "c", "sse2", "ssse3" */
float llm_dot_q4_K_ssse3(const void *w, const void *x, int n);
float llm_dot_q6_K_ssse3(const void *w, const void *x, int n);
float llm_dot_q8_0_ssse3(const void *w, const void *x, int n);
float llm_dot_q4_0_ssse3(const void *w, const void *x, int n);
const char *llm_quant_rota(void);

/* ------------------------------------------------------------ modelo LFM3 */
#define LLM_MAX_CAMADAS 64
#define LLM_LOTE 16          /* tokens por lote al leer el texto de entrada */

typedef struct {
    const gguf_tensor_t *norma_op, *norma_op_pos, *norma_ffn, *norma_ffn_pos;
    const gguf_tensor_t *ffn_gate, *ffn_up, *ffn_down;
    /* convolución corta */
    const gguf_tensor_t *conv, *conv_in, *conv_out;
    /* atención */
    const gguf_tensor_t *wq, *wk, *wv, *wo, *wgate, *q_norma, *k_norma;
    int  atencao;            /* 1 = capa de atención */
    int  janela;             /* 1 = ventana deslizante */
    int  n_kv;               /* cabezas K/V */
    int  idx_cache;          /* índice en la caché K/V */
    int  idx_conv;           /* índice en el estado de la convolución */
} llm_camada_t;

typedef struct {
    gguf_t g;
    /* hiperparámetros */
    int n_camadas, n_embd, n_ff, n_cab, dim_cab, n_vocab, l_conv, n_janela;
    float eps, rope_base, rope_base_janela;
    int ctx_treino;
    llm_camada_t c[LLM_MAX_CAMADAS];
    const gguf_tensor_t *emb, *saida, *norma_saida, *rope_freqs;
    int n_atencao, n_completas, n_janelas, n_conv;
    char arquitetura[32];
    char nome[64];
    uint32_t tipo_ficheiro;
} llm_modelo_t;

/* La caché K/V en Q8_0. Las capas completas guardan ctx posiciones; las de ventana,
 * un anillo de n_janela + LLM_LOTE (un lote entero cabe sin pisar lo que aún se mira). */
typedef struct {
    int ctx;
    int pos;                 /* tokens ya en la caché */
    int n_kv_max;
    uint8_t *mem;            /* llm_mapa */
    size_t   mem_n;
    bloco_q8_0 **k, **v;     /* por capa de atención: [posiciones][n_kv*dim_cab/32] */
    int *posicoes;           /* por capa de atención: cuántas ranuras */
    float *conv_estado;      /* por capa de convolución: [(l_conv-1) * n_embd] */
} llm_cache_t;

typedef struct {
    llm_modelo_t *m;
    llm_cache_t   kv;
    /* trabajo */
    float *x, *xb, *xb2, *hb, *hb2, *q, *k, *v, *bcx, *gate, *att, *logits, *pont;
    void  *xq;               /* entrada cuantizada (Q8_K) para las multiplicaciones */
    bloco_q8_0 *qq;          /* una cabeza de Q en Q8_0 */
    float *rope_cos, *rope_sin;          /* tablas por posición del lote */
    float *inv_freq_comp, *inv_freq_jan; /* factores de cada par (con rope_freqs en las completas) */
    int    lote_max;
    unsigned long long bytes;            /* memoria de trabajo */
} llm_estado_t;

int  llm_modelo_abrir(llm_modelo_t *m, const char *caminho, char *erro, size_t max);   /* cabecera y validación */
int  llm_modelo_carregar(llm_modelo_t *m, void (*progresso)(unsigned pct));
void llm_modelo_fechar(llm_modelo_t *m);
/* Memoria que hará falta (bytes): pesos, caché y trabajo, con ese contexto. */
unsigned long long llm_memoria_pesos(const llm_modelo_t *m);
unsigned long long llm_memoria_cache(const llm_modelo_t *m, int ctx);
unsigned long long llm_memoria_trabalho(const llm_modelo_t *m, int ctx);
int  llm_estado_criar(llm_estado_t *e, llm_modelo_t *m, int ctx);
void llm_estado_libera(llm_estado_t *e);
void llm_estado_reiniciar(llm_estado_t *e);     /* vacía la caché */
/* Pasa n tokens por la red desde la posición actual; deja en e->logits los del último
 * (todos si logits_todos: [n][n_vocab], para comparar). 0 = ok, <0 = contexto lleno. */
int  llm_avaliar(llm_estado_t *e, const int *toks, int n, float *logits_todos);

/* ------------------------------------------------------------ tokenizador */
typedef struct {
    int n;                       /* tokens */
    llm_texto_t *tok;            /* texto de cada token (con los bytes mapeados de gpt2) */
    uint8_t *tipo;               /* 1 normal, 3 control, 4 definido por el usuario... */
    uint32_t *hash_tok; uint32_t hash_tok_n;      /* texto -> id */
    uint64_t *hash_par; uint32_t *hash_par_val; uint32_t hash_par_n;   /* (a,b) -> rango<<? */
    int n_merges;
    int bos, eos, pad, im_start, im_end, think, think_fim;
    bool add_bos, ignorar_merges;
    int  especiais[256]; int n_especiais;   /* ids de control/definidos, del más largo al más corto */
} llm_tokens_t;

int  llm_tokens_criar(llm_tokens_t *t, const gguf_t *g, char *erro, size_t max);
void llm_tokens_libera(llm_tokens_t *t);
/* Texto UTF-8 -> tokens. especiais: reconoce <|im_start|> y demás en el texto. */
int  llm_tokenizar(const llm_tokens_t *t, const char *s, size_t n, bool especiais, int *saida, int max);
/* Bytes UTF-8 de un token (los especiales salen tal cual). -> bytes. */
int  llm_token_bytes(const llm_tokens_t *t, int id, char *buf, int max);
int  llm_token_id(const llm_tokens_t *t, const char *s);   /* -1 si no está */

/* ------------------------------------------------------------ muestreo */
typedef struct {
    float temperatura;   /* 0 = el más probable */
    int   top_k;
    float top_p;
    float penalizacao;   /* repetición (1 = nada) */
    int   janela_pen;    /* últimos tokens que cuentan */
    uint64_t estado;     /* semilla */
} llm_amostra_t;

void llm_amostra_iniciar(llm_amostra_t *a, uint32_t semente);
int  llm_amostrar(llm_amostra_t *a, float *logits, int n, const int *ultimos, int n_ultimos);
int  llm_argmax(const float *v, int n);

#endif
