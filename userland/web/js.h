/* js.h - a API do motor de JavaScript do Cipó (melhoria 081).
 *
 * Vem do mesmo sitio que o motor (ver userland/web/ORIGEM.md). O motor de
 * JavaScript (js.c) fala com o browser por estes ganchos: o browser dá o que
 * precisa (baixar, o titulo, o layout, as fontes da pagina) e o motor devolve
 * o HTML novo quando o script mexeu no DOM.
 */
#ifndef SAGUI_WEB_JS_H
#define SAGUI_WEB_JS_H

#include "web.h"
#include "canvas2d.h"

/* O que o motor precisa do browser. */
typedef struct {
    /* Baixa um recurso (CSS, imagem, script); devolve bloco de aloca ou NULL. */
    uint8_t *(*baixar)(const char *url, unsigned *n);
    /* document.title = */
    void (*titulo)(const char *t);
    /* alert() */
    void (*alerta)(const char *t);
    /* Resposta completa (fetch): corpo de qualquer codigo, codigo, cabecalhos e URL final. */
    uint8_t *(*resposta)(const char *url, unsigned *n, int *codigo, char *cab, int cmax, char *final, int fmax);
    /* Layout sincrono (o script pediu uma medida): diagrama o HTML e preenche
     * 16 ints por elemento data-sagui-n (motor_caixas). 0 = nao deu. */
    int (*layout)(const char *html, int n, int *caixas, int qtd);
    /* getComputedStyle do elemento k do ultimo layout. */
    bool (*estilo)(int k, const char *prop, char *o, int max);
    /* Fonte de uma familia do @font-face da pagina (canvas, document.fonts); NULL se nao ha. */
    struct fonte *(*fonte)(const char *familia, int peso, int italico, int largura);
    /* iframes: diagrama o HTML do quadro e pinta na tela tela2d (l x a); e acha o
     * elemento clicado nele (data-sagui-js do alvo, ou -1) */
    int (*quadro_pintar)(int tela, const char *html, int n, const char *url, int l, int a);
    int (*quadro_alvo)(int tela, int x, int y);
} js_hooks_t;

/* ---- ciclo de vida ---- */
/* Roda os scripts de uma pagina; devolve o HTML novo se o DOM mudou. */
char *js_pagina(const char *html, int n, const char *url, const js_hooks_t *h, int *saida_n);
void  js_fim(void);
int   js_vivo(void);      /* a pagina tem JavaScript rodando */
int   js_ativo(void);     /* idem */
int   js_animando(void);  /* requestAnimationFrame pendente: ritmo de quadro */

/* ---- eventos ---- */
/* Clique: devolve o HTML novo depois do clique, ou NULL. */
char *js_clicar(int id, int *saida_n);
int   js_clique_cancelado(void);          /* o ultimo clique teve preventDefault */
void  js_clicar_quadro(int tela, int x, int y);   /* clique dentro de um iframe */
/* Timers depois do load; devolve o HTML novo se o DOM mudou. */
char *js_tique(int ms, int *saida_n);
/* A janela mudou de tamanho: eventos resize e observadores. */
char *js_redimensionar(int l, int a, int *saida_n);
/* A pagina rolou: eventos scroll e observadores. */
char *js_rolar(int rx, int ry, int *saida_n);
/* Tamanho da area da pagina e rolagem (innerWidth, scrollY...). */
void  js_janela(int l, int a, int rx, int ry);
/* scrollTo do script: 1 = ha pedido. */
int   js_rolagem_pedida(int *x, int *y);
/* location.href = / assign / replace / reload. */
int   js_navegacao(char *url, int max, int *substituir);
/* Campo n (data-sagui-campo) recebeu valor e a tecla (13 Enter, 8 apagar, ou o caractere):
 * roda keydown/input/keyup; devolve o HTML novo se os scripts mudaram o DOM. */
char *js_campo(int n, const char *valor, int tecla, int *saida_n);
/* Envio do formulario do campo n: evento submit; 1 = o script cancelou. */
int   js_enviar(int n, char **html, int *saida_n);
/* URL que o JS deu a um <video>, ou NULL. */
const char *js_video(void);

/* ---- o que o motor precisa do browser (web_compat.c) ---- */
/* Decodifica PNG ou JPEG em pixels ARGB (o motor usa no canvas e no SVG). */
bool web_imagem_decodificar(const uint8_t *d, unsigned n, uint32_t **px, int *l, int *a);
/* Guarda o metodo, os cabecalhos e o corpo do proximo pedido (fetch e POST do
 * JavaScript). O browser le com http_preparado(). */
void http_preparar(const char *metodo, const char *cabecalhos, const void *corpo, int n);
bool http_preparado(char *metodo, int mmax, char *cab, int cmax, const uint8_t **corpo, int *ncorpo);
void http_preparar_limpar(void);
/* O pote de cookies (RFC 6265): o motor usa isto para o document.cookie. */
void cookie_guardar(const char *url, const char *set_cookie, bool de_js);
int  cookie_texto(const char *url, char *o, int max, bool de_js);
void cookie_limpar(void);

#endif
