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

/* ---- diálogos de archivos estilo Windows 95 (mejora 103, D272; gui_arquivos.c) ----
 * Modales; devuelven true y la ruta absoluta en "saida" si se aceptó. "filtros":
 * "Texto (*.txt)|*.txt|Todos|*" (NULL = todos); un «>» antes del nombre de un tipo lo deja elegido de entrada. "inicial": carpeta, o carpeta + nombre propuesto.
 * Guardar pide confirmación al sobrescribir y añade la extensión del tipo si falta. */
bool dialogo_abrir(const char *titulo, const char *filtros, const char *inicial, char *saida, int max);
bool dialogo_guardar(const char *titulo, const char *filtros, const char *inicial, char *saida, int max);
bool dialogo_carpeta(const char *titulo, const char *inicial, char *saida, int max);

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

/* ======================================================================
 * Componentes de aplicaciones de escritorio (mejora 116, D291): barra de menús, barra de
 * herramientas con iconos, lista desplegable, regla, barra de estado y el control de edición
 * de texto con formato (el de «Pluma»). Aspecto Windows 98: gris, biseles. Todos siguen el
 * patrón de siempre: una estructura del programa, una función que dibuja y otra que trata
 * el evento. Las medidas del documento van en TWIPS (1/20 de punto; 1 cm = 567 twips).
 * ====================================================================== */
#define W98_FACE    0xC0C0C0   /* gris de los botones y las barras */
#define W98_SOMBRA  0x808080
#define W98_OSCURO  0x404040
#define W98_LUZ     0xFFFFFF
#define W98_AZUL    0x000080   /* barra de título y selección */
#define W98_NEGRO   0x000000
void g_bisel(jan_t *j, int x, int y, int l, int a, bool afundado);   /* borde de 2 px estilo Win98 */
void g_flecha(jan_t *j, int x, int y, int dir, uint32_t cor);        /* triángulo 7x4: 0 arriba, 1 abajo, 2 izq., 3 der. */

/* ---- barra de menús (con menús desplegables del sistema) ----
 * itens[i] es el texto de su menú (líneas separadas por '\n', "-" = separador, "~x" = desactivada,
 * "Texto\tAtajo"). barra_menu_tratar devuelve BARRA_MENU_ITEM(menu, línea) al elegir, o -1. */
#define BARRA_MENU_MAX 8
#define BARRA_MENU_ITEM(m, k) ((m) * 100 + (k))
typedef struct {
    int x, y, l, a;
    int n;
    const char *nomes[BARRA_MENU_MAX];
    const char *itens[BARRA_MENU_MAX];
    int aberto;          /* menú desplegado (-1 = ninguno) */
    int x_item[BARRA_MENU_MAX], l_item[BARRA_MENU_MAX];
} barra_menu_t;
void barra_menu_iniciar(barra_menu_t *b, int x, int y, int l);   /* rellenar nomes/itens y n antes */
void barra_menu_desenhar(jan_t *j, barra_menu_t *b);
int  barra_menu_tratar(jan_t *j, barra_menu_t *b, const evento_t *e);

/* ---- barra de herramientas con iconos y botones conmutables ----
 * id > 0: botón (barra_ferr_tratar devuelve su id); id = 0: separador; id < 0: hueco de "larg"
 * píxeles para colocar otro componente (una lista desplegable). El icono es una función que dibuja
 * 16x16 en (x, y) (o un texto corto en "texto"; "negrita" lo dibuja en negrita). */
typedef struct {
    int  id;
    void (*icone)(jan_t *j, int x, int y, bool ativo);
    const char *texto;
    bool negrita;
    bool conmutavel;     /* botón de dos estados (N, C, S, alineación) */
    bool marcado;
    bool desativado;
    int  larg;           /* 0 = 23 px (separador: 8 px) */
} barra_item_t;
typedef struct {
    int x, y, l, a;
    barra_item_t *itens;
    int n;
    int apertado;        /* índice pulsado (-1 = ninguno) */
    int sobre;           /* índice bajo el ratón */
} barra_ferr_t;
void barra_ferr_iniciar(barra_ferr_t *b, int x, int y, int l, barra_item_t *itens, int n);
void barra_ferr_desenhar(jan_t *j, barra_ferr_t *b);
int  barra_ferr_tratar(jan_t *j, barra_ferr_t *b, const evento_t *e);   /* id pulsado o 0 */
int  barra_ferr_x(const barra_ferr_t *b, int indice);                    /* x del ítem (para los huecos) */
void barra_ferr_marcar(barra_ferr_t *b, int id, bool marcado);           /* cambia el estado de un conmutable */

/* ---- lista desplegable (combo): texto editable opcional + flecha ---- */
#define COMBO_NADA        0
#define COMBO_SELECIONOU  1     /* eligió un ítem de la lista */
#define COMBO_ENTROU      2     /* Enter en el texto editable */
typedef struct {
    int x, y, l, a;                  /* a = 21 por defecto (combo_iniciar) */
    const char *const *itens;
    int qtd, sel;
    bool editavel;
    char texto[32];
    int  cursor;
    bool foco;
    int  codigo;                     /* identificador del menú desplegable (único por ventana) */
} combo_t;
void combo_iniciar(combo_t *c, int x, int y, int l, const char *const *itens, int qtd, bool editavel, int codigo);
void combo_definir(combo_t *c, const char *texto);
void combo_desenhar(jan_t *j, combo_t *c);
int  combo_tratar(jan_t *j, combo_t *c, const evento_t *e);

/* ---- regla horizontal con márgenes y sangrías (en twips; ox = x del borde izquierdo de la hoja) ---- */
#define REGUA_NADA     0
#define REGUA_ARRASTA  1       /* se está moviendo un marcador (ya redibujada) */
#define REGUA_MUDOU    2       /* soltó: hay un valor nuevo en regua_t */
#define REGUA_M_ESQ    1       /* qué se movió (regua_t.marcador) */
#define REGUA_M_DIR    2
#define REGUA_M_PRIM   3       /* sangría de primera línea */
#define REGUA_M_SANG   4       /* sangría izquierda (colgante) */
#define REGUA_M_SANG_D 5       /* sangría derecha */
typedef struct {
    int x, y, l, a;
    int ox, zoom;                 /* píxel de la hoja y zoom en % */
    int pagina;                   /* ancho de la hoja */
    int m_esq, m_dir;             /* márgenes */
    int sang_esq, sang_prim, sang_dir;   /* respecto al margen (prim: relativa al margen izquierdo, puede ser negativa) */
    int marcador;                 /* el último arrastrado */
    int arrastando;
} regua_t;
void regua_iniciar(regua_t *r, int x, int y, int l);
void regua_desenhar(jan_t *j, regua_t *r);
int  regua_tratar(jan_t *j, regua_t *r, const evento_t *e);

/* ---- barra de estado con secciones ----
 * Anchos fijos en larg[]; la última sección ocupa el resto. */
#define BARRA_ESTADO_MAX 8
typedef struct {
    int x, y, l, a;
    int n;
    int larg[BARRA_ESTADO_MAX];
    char texto[BARRA_ESTADO_MAX][64];
} barra_estado_t;
void barra_estado_iniciar(barra_estado_t *b, int x, int y, int l, int n, const int *larg);
void barra_estado_texto(barra_estado_t *b, int i, const char *texto);
void barra_estado_desenhar(jan_t *j, barra_estado_t *b);

/* ---- control de edición de texto con formato (mejora 116) ----
 * Vista de página (hoja blanca sobre fondo gris, con zoom), formato por caracteres (fuente, tamaño,
 * negrita/cursiva/subrayado/tachado, color) y por párrafos (alineación, sangrías, interlineado,
 * viñetas, espaciado), selección con ratón y teclado, portapapeles, deshacer/rehacer y buscar. Texto en
 * Latin-1; el párrafo termina en '\n' y '\f' es un salto de página. Las medidas de las fuentes son las
 * de Helvetica/Times/Courier (las mismas del PDF), de modo que la paginación en pantalla y en el PDF
 * coinciden. En pantalla se dibujan con la fuente de mapa de bits escalada. */
#define RTE_SANS   0
#define RTE_SERIF  1
#define RTE_MONO   2
#define RTE_NEGRITA    1
#define RTE_CURSIVA    2
#define RTE_SUBLINHADO 4
#define RTE_RISCADO    8
typedef struct { uint8_t fam, est, tam; uint32_t cor; } rte_fmt_t;     /* tam en puntos */
#define RTE_ESQ    0
#define RTE_CENTRO 1
#define RTE_DIR    2
#define RTE_JUST   3
typedef struct {
    uint8_t alin, marcador;          /* marcador: 1 = viñeta */
    int16_t esq, dir, prim;          /* sangrías en twips (prim relativa a esq: negativa = colgante) */
    uint16_t interlin;               /* % (100, 150, 200) */
    int16_t antes, depois;           /* espaciado en twips */
} rte_par_t;
/* campos de rte_fmt_aplicar / rte_par_aplicar */
#define RTE_C_FAM   1
#define RTE_C_TAM   2
#define RTE_C_COR   4
#define RTE_P_ALIN    1
#define RTE_P_SANG    2
#define RTE_P_INTER   4
#define RTE_P_MARC    8
#define RTE_P_ESPACO  16
/* lo que devuelve rte_tratar (máscara) */
#define RTE_R_MUDOU   1      /* cambió el documento */
#define RTE_R_CURSOR  2      /* se movió el cursor o la selección (actualizar barra y estado) */
typedef struct rte rte_t;
rte_t *rte_criar(int x, int y, int l, int a);
void   rte_liberar(rte_t *r);
void   rte_mover(rte_t *r, int x, int y, int l, int a);
void   rte_limpar(rte_t *r);
void   rte_papel(rte_t *r, int largura, int altura, int m_esq, int m_dir, int m_sup, int m_inf);   /* twips */
void   rte_papel_obter(const rte_t *r, int *largura, int *altura, int *m_esq, int *m_dir, int *m_sup, int *m_inf);
void   rte_zoom(rte_t *r, int por_100);                /* 25..400 */
int    rte_zoom_obter(const rte_t *r);
void   rte_foco(rte_t *r, bool foco);
bool   rte_modificado(const rte_t *r);
void   rte_marcar_limpo(rte_t *r);
void   rte_desenhar(jan_t *j, rte_t *r);
int    rte_tratar(jan_t *j, rte_t *r, const evento_t *e);
int    rte_pagina_x(rte_t *r);                         /* x (en la ventana) del borde izquierdo de la hoja */
/* contenido */
int    rte_tamanho(const rte_t *r);
const char *rte_texto(const rte_t *r);
void   rte_doc_anexar(rte_t *r, const char *t, int n, const rte_fmt_t *f);   /* carga: añade al final */
void   rte_doc_par(rte_t *r, const rte_par_t *p);                            /* atributos del último párrafo */
void   rte_doc_fim(rte_t *r);                                                /* acabó la carga */
int    rte_n_par(const rte_t *r);
bool   rte_par_dados(const rte_t *r, int i, int *ini, int *fim, rte_par_t *p);
void   rte_fmt_em(const rte_t *r, int pos, rte_fmt_t *f);
/* cursor y selección */
int    rte_cursor(const rte_t *r);
bool   rte_selecao(const rte_t *r, int *a, int *b);
void   rte_selecionar(rte_t *r, int a, int b);
void   rte_linha_coluna(rte_t *r, int *lin, int *col);
int    rte_pagina_atual(rte_t *r);
int    rte_paginas(rte_t *r);
void   rte_ir_pagina(rte_t *r, int pagina);
/* formato (sobre la selección o, sin ella, sobre lo que se escriba después) */
void   rte_fmt_atual(const rte_t *r, rte_fmt_t *f);
void   rte_fmt_aplicar(rte_t *r, unsigned campos, const rte_fmt_t *f);
void   rte_estilo(rte_t *r, unsigned bits, bool ligar);
void   rte_par_atual(const rte_t *r, rte_par_t *p);
void   rte_par_aplicar(rte_t *r, unsigned campos, const rte_par_t *p);
/* edición */
void   rte_inserir(rte_t *r, const char *t, int n);        /* sustituye la selección */
void   rte_salto_pagina(rte_t *r);
bool   rte_copiar(rte_t *r);
bool   rte_cortar(jan_t *j, rte_t *r);
bool   rte_colar(jan_t *j, rte_t *r);
void   rte_selecionar_tudo(rte_t *r);
bool   rte_desfazer(rte_t *r);
bool   rte_refazer(rte_t *r);
bool   rte_pode_desfazer(const rte_t *r);
bool   rte_pode_refazer(const rte_t *r);
/* buscar y reemplazar */
bool   rte_buscar(rte_t *r, const char *t, bool maiusculas, bool adiante);   /* selecciona la siguiente coincidencia */
bool   rte_substituir(rte_t *r, const char *por);                            /* si la selección es la coincidencia */
int    rte_substituir_tudo(rte_t *r, const char *t, const char *por, bool maiusculas);
void   rte_contar(const rte_t *r, int *palavras, int *caracteres, int *paragrafos);
/* Vista previa de un formato en un rectángulo (diálogo Fuente). */
void   rte_amostra(jan_t *j, int x, int y, int l, int a, const rte_fmt_t *f, const char *texto);
/* Exporta a PDF con la paginación de la pantalla; *dados se libera con libera(). 0 = bien. */
int    rte_exportar_pdf(rte_t *r, const char *titulo, uint8_t **dados, int *n);
/* Diálogos (ventanas modales): true = Aceptar. */
bool   dialogo_fonte(rte_fmt_t *f);
bool   dialogo_paragrafo(rte_par_t *p);
/* Buscar/Reemplazar: devuelve DLG_BUSCAR_* (0 = cerrado). com_substituir muestra el campo y los botones de reemplazo. */
#define DLG_BUSCAR_NADA      0
#define DLG_BUSCAR_SEGUINTE  1
#define DLG_BUSCAR_SUBSTITUIR 2
#define DLG_BUSCAR_TUDO      3
int    dialogo_buscar(char *buscar, int max_b, char *por, int max_p, bool *maiusculas, bool com_substituir);
/* Cm <-> twips con texto ("1,5"): sin coma flotante. */
int    twips_de_cm_texto(const char *t);
char  *cm_texto_de_twips(char *d, int max, int twips);

/* ---- iconos de 16x16 propios para barras de herramientas (mejora 117, D293) ----
 * Tienen la forma de barra_item_t.icone. */
void ic98_novo(jan_t *j, int x, int y, bool ativo);
void ic98_abrir(jan_t *j, int x, int y, bool ativo);
void ic98_guardar(jan_t *j, int x, int y, bool ativo);
void ic98_pdf(jan_t *j, int x, int y, bool ativo);
void ic98_cortar(jan_t *j, int x, int y, bool ativo);
void ic98_copiar(jan_t *j, int x, int y, bool ativo);
void ic98_colar(jan_t *j, int x, int y, bool ativo);
void ic98_desfazer(jan_t *j, int x, int y, bool ativo);
void ic98_esq(jan_t *j, int x, int y, bool ativo);
void ic98_centro(jan_t *j, int x, int y, bool ativo);
void ic98_dir(jan_t *j, int x, int y, bool ativo);

/* ---- pestañas (mejora 117): fila de pestañas de un diálogo de varias páginas ----
 * pestanas_tratar devuelve true si cambió la pestaña activa (en "sel"). */
#define PESTANAS_MAX 8
typedef struct {
    int x, y, l;                 /* y: arriba de las pestañas; alto fijo de 22 px */
    int n, sel;
    const char *nomes[PESTANAS_MAX];
} pestanas_t;
void pestanas_desenhar(jan_t *j, pestanas_t *p);
bool pestanas_tratar(jan_t *j, pestanas_t *p, const evento_t *e);

/* ---- controles Win98 de diálogo, compartidos (D291/D293): botón, casilla y lista con marco ---- */
void boton98(jan_t *j, botao_t *b);
bool boton98_tratar(jan_t *j, botao_t *b, const evento_t *e);
void casilla98(jan_t *j, casilla_t *c, const char *texto);
bool casilla98_tratar(jan_t *j, casilla_t *c, const char *texto, const evento_t *e);
void lista_en_marco(jan_t *j, lista_t *l);

#endif
