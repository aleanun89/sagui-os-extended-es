// SPDX-License-Identifier: GPL-3.0-only
/* motor.h - motor de páginas do Cipó: HTML em árvore, cascata CSS, caixas
 * (bloco, inline, float, flex, grid, tabela, posicionadas) e lista de pintura. */
#ifndef SAGUI_MOTOR_H
#define SAGUI_MOTOR_H

#include "web.h"   /* tipos (uint8_t, bool...) e a API de fontes */

typedef struct motor motor_t;

typedef struct {
    /* Baixa um recurso (CSS ou imagem); devolve bloco de aloca ou NULL. */
    uint8_t *(*baixar)(const char *url, unsigned *n);
    /* Decodifica uma imagem já baixada (PNG/JPEG). */
    bool (*imagem)(const uint8_t *d, unsigned n, uint32_t **px, int *l, int *a);
    /* Pixels atuais de um <canvas> (data-sagui-canvas); NULL se não houver. */
    const uint32_t *(*tela)(int id, int *l, int *a);
    /* Cuadro actual de un <video> (mejora 100, D276): url = el src ya resuelto, flags = VIDEO_*,
     * (x, y, l, a) = la caja en coordenadas de la página (el que llama la apunta para pintar
     * los cuadros nuevos sin repintar la página). Devuelve los píxeles 0x00RRGGBB y su tamaño
     * (*fl, *fa), o NULL si aún no hay cuadro. Puede ser NULL el gancho entero. */
    const uint32_t *(*video)(const char *url, int flags, int x, int y, int l, int a, int *fl, int *fa);
} motor_ganchos_t;
#define VIDEO_AUTOPLAY 1
#define VIDEO_LOOP     2
#define VIDEO_MUTED    4
#define VIDEO_AUDIO    8     /* es un <audio> (no hay cuadro: solo la caja y los controles, D489) */
#define VIDEO_CONTROLS 16    /* el elemento lleva el atributo controls */

/* Tipos de alvo de clique. */
#define MA_LINK  1     /* href */
#define MA_CAMPO 2     /* <input> de texto: nome do campo, ação do formulário */
#define MA_BOTAO 3     /* botão de envio */
#define MA_QUADRO 4    /* iframe pintado: js = tela do quadro; rx, ry = canto dele */
#define MA_IMAGEM 5    /* imagem: href = URL, rx/ry = canto da região */

typedef struct {
    int tipo;
    const char *href;      /* link: endereço já resolvido; campo/botão: ação do formulário */
    const char *imagem;    /* imagem dentro do link, se houver */
    const char *nome;      /* campo: atributo name */
    const char *fixos;     /* campos escondidos do formulário, "a=1&b=2" */
    const char *metodo;
    void *no;              /* campo/botão: o elemento (para foco e digitação) */
    int js;                /* clique tratado pelo JavaScript (data-sagui-js), ou -1 */
    int dom;               /* o elemento do DOM que se clicou (D160), ou -1 */
    bool envia;            /* botão que envia o formulário (submit, image, <button> sem type) */
    int campo;             /* data-sagui-campo (campos e botões depois do JavaScript), ou -1 */
    int rx, ry;            /* canto da região clicada (coordenadas da página) */
} motor_alvo_t;

/* Um nó da árvore da página, como o JavaScript a vê (D160). As cadeias
 * apontam para a memória do motor: valem até ao motor_fim(). */
#define MOTOR_DOM_ATR 8
typedef struct {
    int id;                 /* identificador do nó (único na página) */
    int pai;                /* identificador do pai, ou -1 */
    const char *tag;        /* a etiqueta; "#texto" nos nós de texto */
    const char *id_attr;    /* o atributo id, ou NULL */
    const char *classe;     /* o atributo class, ou NULL */
    const char *texto;      /* o texto (só nos nós de texto) */
    int n_atr;
    const char *atr_nome[MOTOR_DOM_ATR];
    const char *atr_valor[MOTOR_DOM_ATR];
} motor_dom_no_t;

motor_t *motor_novo(const motor_ganchos_t *g);
void     motor_fim(motor_t *m);
/* Lê o HTML (Latin-1) e as folhas de estilo; não faz layout ainda. */
void     motor_ler(motor_t *m, const char *html, int n, const char *url);
/* Baixa as imagens que faltam; devolve quantas chegaram (chamar de novo até 0). */
int      motor_imagens(motor_t *m, int max);
int      motor_urls_pendentes(motor_t *m, const char **urls, int max);   /* lo que motor_imagens bajará luego (D373) */
/* Faz o layout para a largura dada; devolve a altura da página. */
int      motor_layout(motor_t *m, int largura, int altura_tela);
/* Pinta a faixa [y, y + a) da página em px (larg x a, passo = larg). */
void     motor_pintar(motor_t *m, uint32_t *px, int larg, int passo, int y, int a);
/* Avança ou define o relógio do motor (ms desde o início, monótonos).
 * Devolve true se algo mudou e a página precisa de ser repintada (por
 * exemplo, uma animação mudou de estado). Sem chamadas a motor_tick o motor
 * fica como sempre: sem animações. */
bool     motor_tick(motor_t *m, unsigned ms);
bool     motor_tem_camadas(motor_t *m);      /* hay fixed/sticky: no se puede desplazar copiando (D441) */
void     motor_rolagem(motor_t *m, int y);   /* mantém hit-test alinhado após pintura em faixas */
/* Alvo no ponto (x, y) da página, ou NULL. */
const motor_alvo_t *motor_alvo(motor_t *m, int x, int y);
const char *motor_titulo(motor_t *m);
uint32_t motor_fundo(motor_t *m);
/* Estatísticas para o log: nós, caixas, comandos de pintura. */
void     motor_numeros(motor_t *m, int *nos, int *cmds, int *imgs);
int      motor_links(motor_t *m);
/* Campo de texto com foco: recebe a digitação dentro da própria caixa. */
void     motor_foco(motor_t *m, const motor_alvo_t *a);   /* NULL tira o foco */
bool     motor_tem_foco(motor_t *m);
const char *motor_foco_valor(motor_t *m);                  /* texto do campo (UTF-8) */
void     motor_foco_definir(motor_t *m, const char *v);    /* troca o texto; refazer o layout depois */
int      motor_foco_campo(motor_t *m);                     /* data-sagui-campo do campo, ou -1 */
bool     motor_focar_campo(motor_t *m, int campo);         /* depois de reler a página */
/* Envio do formulário de quem (campo ou botão; NULL = campo com foco) como
 * endereço GET: acao?nome=valor&... com todos os campos que têm nome. */
bool     motor_envio(motor_t *m, const motor_alvo_t *quem, char *url, int max);
bool     motor_envio_modo(motor_t *m, const motor_alvo_t *quem, char *metodo, int mmax, char *enctype, int emax, char *acao, int amax);   /* D487: POST y multipart */
bool     motor_envio_campo(motor_t *m, int campo, char *url, int max);   /* quem = o elemento data-sagui-campo */
/* Caixas dos elementos marcados com data-sagui-n (16 ints por elemento) e estilo computado. */
int      motor_caixas(motor_t *m, int *d, int max);
bool     motor_estilo_de(motor_t *m, int k, const char *prop, char *o, int max);
/* ---- o DOM para o JavaScript da página (D160) ----
 * A lista dos nós da árvore, em ordem de documento (para o texto que o
 * navegador manda ao ponte do QuickJS). Devolve quantos nós escreveu. */
int      motor_dom_lista(motor_t *m, motor_dom_no_t *lista, int max);
/* O identificador do <body> e do <html> nessa lista (-1 se não houver). */
int      motor_dom_corpo(motor_t *m);
int      motor_dom_documento(motor_t *m);
/* Marca o elemento (identificador do DOM) como tendo um tratador de clique
 * do JavaScript: a partir do próximo layout, o elemento é alvo do rato e o
 * clique chama o tratador. */
bool     motor_marcar_clique(motor_t *m, int dom);
/* O JavaScript muda a árvore (D271): um atributo de um elemento (valor NULL = tira) ou
 * o seu texto. Textos em Latin-1. Vale no motor_layout seguinte. */
bool     motor_dom_atributo(motor_t *m, int dom, const char *nome, const char *valor);
bool     motor_dom_texto(motor_t *m, int dom, const char *txt);
/* ---- D488: o DOM mutável e o remaquetado incremental (motor_dom.h) ----
 * Os nós criados pelo JavaScript nascem soltos (motor_dom_criar) e entram na página com
 * motor_dom_inserir (antes = -1: no fim); motor_dom_tirar deixa-os soltos outra vez. Cada
 * mudança marca só o que afeta; o motor_layout seguinte refaz a cascada só disso, guarda as
 * medidas intrínsecas do resto e calcula a franja da página que mudou (motor_dom_estadistica).
 * Textos em Latin-1, salvo motor_dom_exportar, que escreve UTF-8 no formato de qjs_bridge.h. */
int      motor_dom_criar(motor_t *m, const char *tag, const char *texto);   /* tag "#texto": nó de texto */
bool     motor_dom_inserir(motor_t *m, int pai, int dom, int antes);
bool     motor_dom_tirar(motor_t *m, int dom);
bool     motor_dom_dado(motor_t *m, int dom, const char *txt);             /* o texto de um nó de texto */
bool     motor_dom_html(motor_t *m, int dom, const char *html, int n);     /* innerHTML */
char    *motor_dom_exportar(motor_t *m, int dom, int *n);                  /* dom < 0: a página toda */
bool     motor_dom_caixa(motor_t *m, int dom, int *o);                     /* 16 ints, como motor_caixas */
int      motor_dom_em(motor_t *m, int x, int y);                           /* elemento no ponto da página */
bool     motor_dom_estilo(motor_t *m, int dom, const char *prop, char *o, int max);
void     motor_dom_estadistica(motor_t *m, int *estilizados, int *medidas, int *y0, int *y1);
void     motor_dom_incremental(motor_t *m, bool sim);                      /* false: sempre a cascada completa */
unsigned motor_dom_assinatura(motor_t *m);                                 /* firma da lista de pintura */
bool     motor_dom_rolagem(motor_t *m, int dom, int *o);                   /* D489+D488: scrollLeft/Top y máximos de un bloque */
bool     motor_dom_rolar(motor_t *m, int dom, int x, int y);              /* scrollTo de un bloque */
int      motor_dom_foco(motor_t *m);                                        /* o elemento do campo com foco, ou -1 */
fonte_t *motor_fonte_nomeada(motor_t *m, const char *familia, int peso, int italico, int largura);
void     motor_zoom(motor_t *m, int pct);    /* zoom de la página en % (D489); vale desde el motor_layout siguiente */
int      motor_zoom_atual(motor_t *m);
/* Bloques con overflow auto/scroll (D489): (x, y) son coordenadas de la página. */
bool     motor_rolar_caixa(motor_t *m, int x, int y, int dx, int dy);   /* la rueda; false = que suba la página */
bool     motor_barra_pulsar(motor_t *m, int x, int y);                 /* clic en una barra (carril o pulgar) */
bool     motor_barra_arrastando(motor_t *m);
void     motor_barra_mover(motor_t *m, int x, int y);                   /* con el pulgar agarrado */
void     motor_barra_soltar(motor_t *m);
bool     motor_caixa_rolagem(motor_t *m, int x, int y, int *rx, int *ry, int *mx, int *my);
void     motor_fontes_limpar(void);   /* nova página: esquece as fontes da web baixadas */
void     motor_sem_esconder(motor_t *m, bool sim);   /* ignora display:none do autor */   /* quantos links a página tem */
#endif
