// SPDX-License-Identifier: GPL-3.0-only
/* gui.h - kit gráfico de libsagui (H22).
 *
 * Todo aquí corre en el propio programa, en el anillo 3: el kernel sólo entrega el área
 * de píxeles de la ventana y los eventos. Cada componente es una estructura con la
 * posición y el estado, una función que dibuja y otra que trata un evento. */
#ifndef SAGUI_GUI_H
#define SAGUI_GUI_H
#include "sagui.h"
#include "imagem.h"   /* imagem_t, para el lienzo (mejora 042) */

/* Colores del tema. */
#define COR_FUNDO      0xF4F4F0
#define COR_TEXTO      0x1E2328
#define COR_TEXTO_FRACO 0x6A737C
#define COR_BORDA      0x8A939C
#define COR_BOTAO      0xE2E6EA
#define COR_BOTAO_APERTADO 0xC4CBD2
#define COR_DESTAQUE   0x0B7A3E   /* verde de SaguiOS */
#define COR_SELECAO    0x2A6FD8
#define COR_BRANCO     0xFFFFFF

typedef struct {
    int       id;
    uint32_t *px;
    int       larg, alt;
    unsigned  estilo;          /* lo que se pidió en jan_estilo (D044) */
    int       min_l, min_a;
} jan_t;

/* ---- ventana y dibujo ---- */
jan_t *jan_abrir(const char *titulo, int largura, int altura);
void   jan_fechar(jan_t *j);
void   jan_mostrar(jan_t *j);                                   /* toda la ventana */
void   jan_mostrar_area(jan_t *j, int x, int y, int l, int a);
/* Tamaño variable (D044): jan_estilo activa JAN_REDIMENSIONAVEL y/o
 * JAN_TELA_CHEIA_OK con el menor tamaño aceptado. Después, en el bucle de eventos:
 *     if (jan_redimensionar(j, &e)) { rehacer el layout con j->larg, j->alt y redibujar }
 * (el kernel ya copió el dibujo anterior a la esquina de arriba a la izquierda). */
int    jan_estilo(jan_t *j, unsigned estilo, int min_l, int min_a);
bool   jan_redimensionar(jan_t *j, const evento_t *e);    /* true = EV_REDIMENSIONAR aceptado */
bool   jan_tela_cheia(jan_t *j, bool ligar);              /* pide (o suelta) la pantalla completa */
bool   jan_cursor_visivel(jan_t *j, bool visivel);        /* D230: oculta el puntero del sistema sobre la ventana */
void   g_retangulo(jan_t *j, int x, int y, int l, int a, uint32_t cor);
void   g_moldura(jan_t *j, int x, int y, int l, int a, uint32_t cor);
void   g_linha(jan_t *j, int x0, int y0, int x1, int y1, uint32_t cor);
/* Copia un bloque l x a de (sx, sy) a (dx, dy) dentro de la propia ventana (D102). Acepta áreas que
 * se superponen y recorta lo que sale de la ventana. */
void   g_copiar_area(jan_t *j, int sx, int sy, int l, int a, int dx, int dy);
void   g_texto(jan_t *j, int x, int y, const char *t, uint32_t cor);
void   g_texto_n(jan_t *j, int x, int y, const char *t, int n, uint32_t cor);
void   g_desenho(jan_t *j, int x, int y, const char *const *linhas, int l, int a,
                 uint32_t (*paleta)(char), int escala);   /* paleta devuelve 0xFF000000 = transparente */
void   g_relevo(jan_t *j, int x, int y, int l, int a, bool afundado);
int    g_largura_texto(const char *t);
#define LETRA_L 8
#define LETRA_A 16

/* ---- etiqueta ---- */
void rotulo(jan_t *j, int x, int y, const char *texto, uint32_t cor);

/* ---- botón ---- */
typedef struct {
    int x, y, l, a;
    const char *texto;
    bool apertado;
} botao_t;
void botao_desenhar(jan_t *j, botao_t *b);
bool botao_tratar(jan_t *j, botao_t *b, const evento_t *e);   /* true = fue pulsado */

/* ---- cuadro de texto de una línea ---- */
typedef struct {
    int  x, y, l;
    char texto[128];
    int  cursor;
    bool foco;
    bool oculta;   /* D145: si es cierto, el texto se dibuja con asteriscos
                    * (para las claves de API). */
} caixa_t;
void caixa_desenhar(jan_t *j, caixa_t *c);
bool caixa_tratar(jan_t *j, caixa_t *c, const evento_t *e);   /* true = Enter */
void caixa_definir(caixa_t *c, const char *texto);

/* ---- área de texto con varias líneas y desplazamiento ---- */
typedef struct {
    int   x, y, l, a;
    char *buf;            /* texto (Latin-1), terminado en cero */
    int   tam, cap;
    int   cursor;         /* posición en el búfer */
    int   topo;           /* primera línea visible */
    bool  foco;
    bool  modificado;
    bool  somente_leitura;
    int   ancora;         /* otro extremo de la selección (D074); -1 = sin selección */
    bool  arrastando;     /* botón apretado: mover el ratón selecciona */
} area_t;
bool area_iniciar(area_t *a, int x, int y, int l, int al);
void area_definir(area_t *a, const char *texto, int n);
void area_desenhar(jan_t *j, area_t *a);
bool area_tratar(jan_t *j, area_t *a, const evento_t *e);    /* true = cambió el texto; EV_RODA desplaza (D048) */
int  area_linhas_visiveis(area_t *a);
void area_mudar_tamanho(area_t *a, int l, int al);          /* después de redimensionar la ventana */
/* Selección con Shift o arrastrando; Ctrl+A/C/X/V en la propia area_tratar (D074).
 * Para los menús: */
bool area_copiar(area_t *a);              /* false = no hay selección */
bool area_cortar(jan_t *j, area_t *a);
bool area_colar(jan_t *j, area_t *a);     /* true = pegó algo */

/* ---- lista con selección ---- */
#define LISTA_NADA       0
#define LISTA_SELECIONOU 1
#define LISTA_ATIVOU     2   /* doble clic ou Enter */
typedef struct lista_gui {
    int    x, y, l, a;
    char (*itens)[64];
    int    qtd;
    int    sel;
    int    topo;
    /* Dibuja un icono pequeño (16x16) antes del texto del ítem, si no es NULL. */
    void (*icone)(jan_t *j, int x, int y, int item);
    /* Dibujo propio (D217): si no es NULL, lista_tratar lo usa en lugar de lista_desenhar al
     * redibujar tras la rueda, el teclado o un clic. Así un programa con una vista propia
     * (Archivos: iconos, detalles) no ve su lista cambiada por la genérica mientras rueda.
     * Al final de la estructura: los inicializadores posicionales antiguos lo dejan en NULL. */
    void (*desenhar)(jan_t *j, struct lista_gui *l);
} lista_t;
void lista_desenhar(jan_t *j, lista_t *l);
int  lista_tratar(jan_t *j, lista_t *l, const evento_t *e);   /* EV_RODA desplaza RODA_LINHAS ítems por muesca; RePág/AvPág/Inicio/Fin (D217) */
#define LISTA_ALTURA_ITEM 20

/* ---- diálogos (ventanas propias, esperan la respuesta) ---- */
bool confirmar(const char *titulo, const char *pergunta);      /* "¿Seguro?" Sí/No */
void mensagem(const char *titulo, const char *texto);           /* OK */
bool pedir_texto(const char *titulo, const char *pergunta, char *resposta, int max);

/* ---- lienzo (mejora 042): superficie con elementos retenidos ----
 *
 * Los elementos se guardan en coordenadas del lienzo y se ven a través de una
 * ventana con desplazamiento y zoom. El zoom va en punto fijo 16.16
 * (0x10000 = 1.0), porque libsagui se compila sin coma flotante (D076). */
typedef struct lenzo lenzo_t;

lenzo_t *lenzo_criar(int x, int y, int l, int a);
void     lenzo_liberar(lenzo_t *c);
void     lenzo_limpar(lenzo_t *c);
/* Añadir elementos; devuelven el índice del elemento o -1 si no hay memoria.
 * Las coordenadas van en píxeles del lienzo. */
int      lenzo_linha(lenzo_t *c, int x0, int y0, int x1, int y1, uint32_t cor, int espessura);
int      lenzo_retangulo(lenzo_t *c, int x, int y, int l, int a, uint32_t cor,
                         uint32_t cor_linha, bool preenchido, int espessura);
int      lenzo_elipse(lenzo_t *c, int x, int y, int l, int a, uint32_t cor,
                      uint32_t cor_linha, bool preenchido, int espessura);
/* "pontos" son pares (x, y); n es el número de puntos (máx. 64). */
int      lenzo_poligono(lenzo_t *c, const int *pontos, int n, uint32_t cor,
                        uint32_t cor_linha, bool preenchido, int espessura);
int      lenzo_texto(lenzo_t *c, int x, int y, const char *texto, uint32_t cor);
/* El lienzo guarda una copia de la imagen; l/a = 0 para el tamaño natural. */
int      lenzo_imagem(lenzo_t *c, int x, int y, const imagem_t *img, int l, int a);

void     lenzo_desenhar(jan_t *j, lenzo_t *c);
bool     lenzo_tratar(jan_t *j, lenzo_t *c, const evento_t *e);   /* true = hay que redibujar */
void     lenzo_deslocar(lenzo_t *c, int dx, int dy);
void     lenzo_zoom(lenzo_t *c, int por_100);      /* 100 = normal; de 25 a 400 */
int      lenzo_elementos(lenzo_t *c);
int      lenzo_em_baixo_de(lenzo_t *c, int lx, int ly);   /* en coordenadas del lienzo */
int      lenzo_sobre_mouse(lenzo_t *c);      /* elemento bajo el ratón (-1 = ninguno) */
int      lenzo_selecionado(lenzo_t *c);      /* elemento elegido con un clic (-1 = ninguno) */

/* ---- barra de progreso (mejora 042) ---- */
typedef struct progresso progresso_t;

progresso_t *progresso_criar(int x, int y, int l, int a, bool vertical);
void         progresso_definir(progresso_t *p, int valor);      /* 0..100 */
void         progresso_indeterminado(progresso_t *p, bool ligar); /* banda que se mueve */
void         progresso_texto(progresso_t *p, bool mostrar);     /* escribe "42%" */
void         progresso_desenhar(jan_t *j, progresso_t *p);
bool         progresso_avancar(progresso_t *p);   /* mueve la banda; true = redibujar */
void         progresso_rect(progresso_t *p, retangulo_t *r);   /* sitio que ocupa */
void         progresso_liberar(progresso_t *p);   /* suelta la barra (al redimensionar) */

/* ---- casilla de verificación (mejora 042) ---- */
typedef struct {
    int  x, y;
    bool marcada;
} casilla_t;
void casilla_desenhar(jan_t *j, casilla_t *c);
bool casilla_tratar(casilla_t *c, const evento_t *e);   /* true = cambió */

/* ---- visor de texto con estilos: Markdown por secciones (D239) ----
 *
 * Muestra texto con formato (negrita, cursiva, títulos, listas, citas, código
 * en línea y bloques de código con fondo y botón «Copiar» al portapapeles) en
 * un rectángulo de la ventana, con desplazamiento. El documento es una lista
 * de secciones (un mensaje del chat, por ejemplo), cada una con un rótulo
 * opcional; rico_juntar añade texto al final de la última y solo vuelve a
 * maquetar esa (vale para el texto que llega en streaming). Texto Latin-1. */
#define RICO_NEGRITA  1
#define RICO_CURSIVA  2
#define RICO_CODIGO   4      /* código en línea */
#define RICO_ENLACE   8
#define RICO_TITULO   16
#define RICO_MONO     32     /* bloque de código o tabla */
/* Lo que devuelve rico_tratar. */
#define RICO_NADA     0
#define RICO_ROLOU    1      /* se desplazó (ya está redibujado) */
#define RICO_COPIOU   2      /* se pulsó «Copiar»: rico_copiado() dice cuántos bytes */
typedef struct {             /* lo que encontró el análisis de una sección */
    int negrita, cursiva, codigo_linha, titulos, itens_lista, blocos_codigo;
} rico_contagem_t;
typedef struct rico rico_t;
rico_t *rico_criar(int x, int y, int l, int a);
void    rico_liberar(rico_t *r);
void    rico_limpar(rico_t *r);
void    rico_mover(rico_t *r, int x, int y, int l, int a);   /* al redimensionar la ventana */
int     rico_secao(rico_t *r, const char *rotulo, uint32_t cor, bool markdown);   /* -> índice */
void    rico_juntar(rico_t *r, const char *t, int n);   /* al final de la última sección */
void    rico_remover_ultima(rico_t *r);
int     rico_secoes(const rico_t *r);
const char *rico_texto(const rico_t *r, int i, int *n);   /* la fuente de la sección i */
void    rico_desenhar(jan_t *j, rico_t *r);
int     rico_tratar(jan_t *j, rico_t *r, const evento_t *e);   /* rueda, RePág/AvPág, barra, «Copiar» */
bool    rico_no_fim(const rico_t *r);
void    rico_ir_fim(rico_t *r);
int     rico_secao_em(const rico_t *r, int x, int y);   /* sección bajo el punto, o -1 */
bool    rico_botao_copiar(const rico_t *r, int i, int k, retangulo_t *ret);   /* el k-ésimo de la sección i, si se ve */
int     rico_copiado(const rico_t *r);   /* bytes del último «Copiar» */
void    rico_contar(const rico_t *r, int i, rico_contagem_t *c);

#endif
