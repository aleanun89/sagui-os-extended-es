// SPDX-License-Identifier: GPL-3.0-or-later
/* llm_programa.h - lo que comparten las partes del programa `llm` (D380): la sesión
 * (modelo, tokenizador, caché y muestreo), la salida de texto y las cuentas de memoria. */
#ifndef LLM_PROGRAMA_H
#define LLM_PROGRAMA_H
#include "llm.h"

typedef struct {
    llm_modelo_t  m;
    llm_tokens_t  t;
    llm_estado_t  e;
    llm_amostra_t a;
    int *hist; int hist_n, hist_max;     /* tokens en la caché (para la penalización) */
    int ultimo_fim;                       /* el token que paró la generación (-1 si ninguno) */
    unsigned ms_carga, ms_leitura, ms_geracao;
    int n_leitura, n_geracao;
    struct llm_visao *visao;              /* el proyector de imágenes (mmproj), si se cargó (fase 2) */
} llm_sessao_t;

/* A dónde va el texto generado: al terminal (f NULL) o a una función (el servidor). */
typedef struct {
    void (*f)(void *ctx, const char *t, int n);
    void *ctx;
    bool utf8;                /* en SaguiOS: sacar UTF-8 en vez de Latin-1 */
    bool ocultar_think;       /* no escribir las marcas <think> y </think> */
    bool pensamento_oculto;   /* no escribir nada de lo que va entre ellas */
    uint32_t cp; int pend, falta;
} llm_saida_t;

typedef struct {
    unsigned long long pesos, cache, trabalho, tokens, total, espaco, livre;
} llm_contas_t;

void llm_mb(char *b, size_t n, unsigned long long bytes);
void llm_saida_bytes(llm_saida_t *s, const char *b, int n);
int  llm_latin1_utf8(const char *s, char *o, int max);
int  llm_modelo_padrao(char *dst, size_t max);   /* 1 ok, 0 ninguno, -1 varios sin activo */
bool llm_ram_suficiente(char *msg, size_t max);
bool llm_memoria_cabe(const llm_modelo_t *m, int ctx, llm_contas_t *c, char *msg, size_t max);
int  llm_preparar(llm_sessao_t *s, const char *caminho, int ctx, bool silencio, char *erro, size_t max);
void llm_fechar_sessao(llm_sessao_t *s);
int  llm_alimentar(llm_sessao_t *s, const int *toks, int n);
int  llm_alimentar_texto(llm_sessao_t *s, const char *utf8, bool especiais);
int  llm_gerar(llm_sessao_t *s, int n, llm_saida_t *saida, bool *cheio);
int  llm_turno_usuario(llm_sessao_t *s, const char *texto_utf8, bool pensar);
int  llm_fim_turno(llm_sessao_t *s);
void llm_amostra_iniciar_semente(llm_amostra_t *a, uint32_t semente);

int  llm_servidor(llm_sessao_t *s, int porta, const char *sistema);   /* llm_servidor.c */
int  llm_servidor_pedir(int porta, const char *corpo, size_t n);       /* --probar-servidor (pruebas) */
/* Visión (llm_visao.c, fase 2): la imagen (PNG, JPEG, BMP o PPM en memoria) pasa por el
 * proyector y sus embeddings entran en la caché como si fueran tokens. -> posiciones usadas (<0 error) */
bool llm_visao_ativa(const llm_sessao_t *s);
int  llm_imagem_alimentar(llm_sessao_t *s, const uint8_t *dados, size_t n, char *erro, size_t max);
int  llm_alimentar_emb(llm_sessao_t *s, const float *emb, int n);
/* Abre el mmproj; tokens_max: vectores por imagen como mucho (0 = de serie, 144). */
int  llm_visao_abrir(struct llm_visao **v, const char *caminho, int n_embd, int tokens_max, bool silencio, char *erro, size_t max);
void llm_visao_fechar(struct llm_visao *v);
/* Los vectores de una imagen sin meterlos en la caché (*emb es memoria nueva). -> cuántos */
int  llm_imagem_vetores(llm_sessao_t *s, const uint8_t *dados, size_t n, float **emb, int *largura, int *altura, char *erro, size_t max);
/* llm_plat.c: imagen en memoria -> RGB8 (*rgb es memoria nueva). En SaguiOS, todo lo que lee la
 * libsagui (PNG, JPEG, BMP, PNM, GIF); en el PC, PPM (P6) y BMP de 24 bits, para las pruebas. */
int  llm_imagem_decodificar(const uint8_t *dados, size_t n, uint8_t **rgb, int *w, int *h, char *erro, size_t max);
int  llm_anunciar(const char *modelo);                                 /* llm_anuncio.c */
#endif
