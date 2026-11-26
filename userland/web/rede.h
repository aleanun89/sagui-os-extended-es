// SPDX-License-Identifier: GPL-2.0-or-later
/* rede.h - la red del Cipó: cookies, caché HTTP, pedidos, descompresión y almacenamiento (D487).
 *
 * Todo esto es lógica pura (sin ventanas ni TLS): el navegador (cipo.c) abre la conexión y
 * este módulo decide qué se envía, qué se guarda y cómo se decodifica lo que vuelve.
 * Los pedidos de JavaScript (fetch, XMLHttpRequest, WebSocket) llegan por rede_executor. */
#ifndef SAGUI_WEB_REDE_H
#define SAGUI_WEB_REDE_H

#include "sagui.h"

/* User-Agent único: el mismo que ve navigator.userAgent. */
#define REDE_UA "Mozilla/5.0 (X11; SaguiOS i686) Cipo/8.5 (like Gecko)"
#define REDE_URL_MAX 600

/* ---- utilidades ---- */
bool     rede_igual_ci(const char *a, const char *b);
int      rede_comeca_ci(const char *s, const char *prefijo);          /* 1 si s empieza por prefijo (sin mayúsculas) */
uint32_t rede_data_http(const char *s);                               /* fecha HTTP/cookie -> UTC unix (0 = no válida) */
bool     rede_origem(const char *url, char *o, int max);              /* "https://host[:puerto]" */
bool     rede_mesmo_site(const char *a, const char *b);               /* mismo sitio (dominio registrable) */
bool     rede_mesma_origem(const char *a, const char *b);
/* Llama a f(valor, ctx) por cada línea "Nombre: valor" del bloque de cabeceras. */
int      rede_cab_cada(const char *cab, const char *nome, void (*f)(const char *valor, void *ctx), void *ctx);
bool     rede_cab_valor(const char *cab, const char *nome, char *valor, int max);   /* la primera línea de ese nombre */
const char *rede_pasta(void);                                         /* /home o /tmp */
void     rede_pasta_cipo(char *o, int max);                           /* <pasta>/.config/cipo (la crea) */

/* ---- cookies (RFC 6265bis) ---- */
void cookie_guardar(const char *url, const char *set_cookie, bool de_js);
int  cookie_texto(const char *url, char *o, int max, bool de_js);     /* lo que lee document.cookie */
void cookie_limpar(void);
/* Cabecera Cookie de un pedido: "a=1; b=2". iniciador = página que lo provoca (NULL = navegación
 * escrita a mano). navegacion = pedido de nivel superior. credenciales = false no envía nada a otro sitio. */
int  rede_cookie_cabecalho(const char *url, const char *iniciador, bool navegacao, const char *metodo,
                           bool credenciales, char *o, int max);
int  rede_cookie_contar(int *de_sesion);
int  rede_cookie_lista(char *o, int max);                             /* texto para mostrar al usuario */
void rede_cookie_carregar(void);                                      /* de /home/.config/cipo/cookies */
void rede_cookie_gravar(bool forcar);
void rede_cookie_recarregar(void);                                    /* descarta la memoria y vuelve a leer el disco */
int  rede_cookie_em_disco(void);                                      /* líneas del archivo de cookies */

/* ---- caché HTTP ---- */
#define CACHE_NADA 0
#define CACHE_VELHA 1     /* hay copia, pero hay que revalidarla */
#define CACHE_FRESCA 2    /* se puede usar sin preguntar */
int      rede_cache_consultar(const char *url, char *etag, int emax, char *lm, int lmax);
uint8_t *rede_cache_ler(const char *url, int *n, char **cab);         /* copia del cuerpo y de la cabecera (libera ambos) */
void     rede_cache_guardar(const char *url, int codigo, const char *cab, const uint8_t *corpo, int n);
void     rede_cache_renovar(const char *url, const char *cab);        /* tras un 304 */
void     rede_cache_limpar(void);
int      rede_cache_contar(int *bytes);

/* ---- pedidos ---- */
#define RP_NAVEGACAO   1     /* navegación de nivel superior (cuenta para SameSite y Accept) */
#define RP_CREDENCIAIS 2     /* envía cookies aunque sea de otro origen (credentials: include) */
#define RP_MANUAL      4     /* no sigue las redirecciones (redirect: manual) */
#define RP_SEM_CACHE   8     /* no usa ni guarda la caché (cache: no-store / recarga) */
#define RP_PAGINA      16    /* es la página que se muestra */
#define RP_BRUTO       32    /* devuelve la respuesta sea cual sea el código (fetch, XHR) */
#define RP_SEM_CODIF   64    /* no pide gzip/deflate/br */
#define RP_SEM_REFERER 128
#define RP_CORS        256   /* petición entre orígenes de un script: se comprueba Access-Control-Allow-Origin */
#define RP_SEM_COOKIES 512   /* credentials: omit: ni se envían ni se guardan cookies */
#define RP_NO_CORS     1024  /* mode: no-cors: sin comprobación, la respuesta es opaca para el script */
typedef struct {
    const char *metodo;           /* "GET" */
    const char *url;
    const char *cabs;             /* cabeceras de más "K: V\r\n" (se filtran las prohibidas) */
    const uint8_t *corpo; int ncorpo;
    const char *tipo_corpo;       /* Content-Type del cuerpo (NULL = no lleva o va en cabs) */
    const char *iniciador;        /* URL de la página que lo pide */
    int maximo;
    unsigned flags;
} rede_pedido_t;
typedef struct {
    int codigo;
    char url[REDE_URL_MAX];       /* dirección final tras las redirecciones */
    char *cab;                    /* bloque de cabeceras (línea de estado incluida); libera quien llama */
    uint8_t *corpo; int tam;      /* ya descomprimido; libera quien llama */
    bool truncado, redirigido, de_cache, cors_bloqueado;
    const char *erro;
    char detalhe[200];
} rede_resposta_t;
/* El navegador pone aquí quien ejecuta los pedidos (lo usa el JavaScript). */
typedef int (*rede_executor_t)(const rede_pedido_t *q, rede_resposta_t *r);
void rede_definir_executor(rede_executor_t f);
int  rede_executar(const rede_pedido_t *q, rede_resposta_t *r);       /* -1 si no hay executor */
void rede_resposta_liberar(rede_resposta_t *r);

/* Arma el texto de la cabecera del pedido. etag/lm: validadores de la caché (o NULL). Devuelve el tamaño o -1. */
int  rede_montar_pedido(const rede_pedido_t *q, const char *metodo, const url_t *u, const char *url,
                        const char *tipo_corpo, int ncorpo, const char *etag, const char *lm, char *buf, int max);
/* Método y cuerpo del siguiente salto tras una redirección. Devuelve true si el cuerpo se conserva. */
bool rede_redirecao(int codigo, char *metodo, int mmax);
/* Todas las líneas Set-Cookie de una respuesta a la jarra. */
void rede_cookies_da_resposta(const char *url, const char *cab);
/* Descomprime el cuerpo según Content-Encoding. Toma el búfer (lo libera) y devuelve el nuevo, o NULL con *erro. */
uint8_t *rede_decodificar(const char *cab, uint8_t *corpo, int *n, int maximo, const char **erro);
/* CORS sencillo: ¿el servidor permite a esa página leer la respuesta? */
bool rede_cors_permite(const char *cab, const char *origem_pagina, bool credenciales);
bool rede_cors_precisa_preparacao(const char *metodo, const char *cabs);
/* Cabeceras que el script puede leer (sin Set-Cookie; entre orígenes, solo las seguras). */
void rede_cab_para_js(const char *cab, bool entre_origenes, char *o, int max);

/* ---- localStorage y sessionStorage por origen ---- */
#define ALM_LOCAL 0
#define ALM_SESION 1
#define ALM_QUOTA (1024 * 1024)
/* Devuelve texto (liberar con libera) o NULL. */
char *rede_alm_obter(int area, const char *url, const char *chave);
char *rede_alm_chave(int area, const char *url, int i);
int   rede_alm_tamanho(int area, const char *url);
int   rede_alm_definir(int area, const char *url, const char *chave, const char *valor);   /* 0 ok, -1 no cabe */
void  rede_alm_remover(int area, const char *url, const char *chave);
void  rede_alm_limpar(int area, const char *url);
void  rede_alm_recarregar(void);                                      /* descarta localStorage de memoria: vuelve a leerse del disco */
int   rede_alm_em_disco(void);                                        /* archivos de localStorage guardados */

/* ---- WebSocket (ws.c) ---- */
void ws_definir_trava(mutex_t *m);                   /* el cerrojo de la TLS del navegador */
int  ws_ativos(void);                                /* cuántos WebSocket hay abiertos */
void ws_liberar_todos(void);
int  ws_abrir(const char *url, const char *origem, const char *protocolos, char *erro, int emax);   /* id >= 0 */
int  ws_estado(int id);                              /* 0 conectando, 1 abierto, 2 cerrando, 3 cerrado */
int  ws_enviar(int id, const void *dados, int n, bool binario);
int  ws_fechar(int id, int codigo, const char *razao);
/* Un mensaje recibido (si lo hay): tipo 1 texto, 2 binario, 8 cierre. Devuelve bytes (>=0) o -1 si no hay. */
int  ws_receber(int id, uint8_t **dados, int *tipo, int *codigo_fecho);
void ws_liberar(int id);
const char *ws_protocolo(int id);

#endif
