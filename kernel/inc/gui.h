// SPDX-License-Identifier: GPL-3.0-only
/* gui.h - área de trabalho e gestor de ventanas (fase 5). */
#ifndef SAGUI_GUI_H
#define SAGUI_GUI_H
#include "tipos.h"
#include "desenho.h"
#include "abi.h"

#define BARRA_SUPERIOR 24

/* Menús montados por el kernel (barra, escritorio): el texto y, para
 * cada línea, la acción que ejecuta la respuesta y el icono. */
#define MENU_NADA (-1)
typedef struct {
    char texto[MENU_MAX_ITENS * (MENU_MAX_TEXTO + 2) + 1];
    int  tam, qtd;
    int  acao[MENU_MAX_ITENS];
    const desenho_t *icone[MENU_MAX_ITENS];
} menu_lista_t;

/* Escritorio (area_trabalho.c) */
bool area_iniciar(void);
void area_fundo(superficie_t *s);          /* copia el fondo listo para el recorte de s */
int  area_icone_em(int x, int y);          /* índice del icono bajo el punto, o -1 */
void area_icone_abrir(int i);
int  area_qtd_icones(void);
void area_icone_retangulo(int i, int *x, int *y, int *l, int *a);
void area_icone_clique(int ic, int x, int y, bool ctrl);   /* botón izquierdo en el escritorio: selección y arrastre (D268) */
bool area_icone_arrasto(int x, int y, uint8_t botoes);     /* movimiento/soltado con un arrastre en marcha; true = consumido */
bool area_icone_tecla(int t, uint32_t mods);               /* flechas, Intro y Esc con el foco en el escritorio */
void area_foco_perder(void);                               /* otra ventana tomó el foco del teclado */
void area_icone_direito(int i, int x, int y);   /* botón derecho en un icono: menú (D051) */
void area_menu_fundo(int x, int y);             /* botón derecho en el escritorio vacío (D051) */
bool area_pendrive(bool presente, const char *rotulo);
int  escrever_atomico(const char *dest, const char *tmp, const void *dados, uint32_t n);   /* /home/.config: escribe y renombra (D268, D290) */
bool area_tela_mudou(void);                /* fondo nuevo después de cambiar el modo de vídeo (D059) */
bool area_fundo_personalizado(void);       /* D112: imagen de fondo elegida en la Configuración */
void area_fundo_padrao(void);
int  area_fundo_linhas(const uint32_t *px, int y, int n);   /* n líneas de píxeles de ancho de pantalla a partir de la línea y */
void area_fundo_fim(void);                 /* termina la imagen: redibuja el fondo y la pantalla */
void area_abrir_pantalla(void);            /* "Configuración de pantalla" (D066) */
void area_menu_resolucao(menu_lista_t *ml);/* submenú "Resolución" con los modos en caliente (D066) */
bool area_resposta_resolucao(int acao);    /* true si la acción era una resolución (y ya cambió) */

/* Gestor de ventanas (janelas.c) */
bool gui_iniciar(void);                    /* activa el escritorio y el gestor */
bool gui_ativa(void);
uint32_t gui_tc_quadros(void);   /* D245: cuadros de pantalla completa dibujados */
uint32_t gui_tc_fps(void);       /* D245: fps medidos (0 = aún sin medida) */
void gui_redesenhar(int x, int y, int l, int a);
void gui_mostrar(void);
void gui_aviso(const char *texto);   /* notificación en la esquina de la pantalla durante unos segundos */
struct processo;
void janelas_do_processo_fechar(struct processo *p);
int  janelas_contar(int pid);                       /* D043 */
int  janelas_listar(void *v, int n);                /* rellena info_janela_t[] (abi.h) */
void gui_abrir_tarefas(void);                       /* abre el Administrador de tareas o lo trae al frente */

/* Menú contextual (menu.c, D051). Uno solo abierto a la vez, encima de todo.
 * elementos: líneas separadas por '\n'; "-" = separador; "~Texto" = desactivada;
 * "*Texto" = marcada; ">Texto" abre un submenú con las líneas siguientes hasta
 * una línea "<" (D064). La respuesta recibe el índice de la línea (contando
 * todas) o -1 si se canceló, y los dos números pasados en la apertura. */
typedef void (*menu_resposta_t)(int indice, int janela, int dado);
bool menu_abrir(int x, int y, const char *itens, menu_resposta_t resposta, int janela, int dado);
bool menu_abrir_icones(int x, int y, const char *itens, const desenho_t *const *icones, menu_resposta_t resposta,
                       int janela, int dado);   /* icones[linha]: 32x32, dibujados en 16x16 (o NULL) */
void menu_destacar_primeira(void);                  /* abierto con el teclado: la primera línea ya destacada */
bool menu_aberto(void);
int  menu_janela(void);                             /* "janela" del menú abierto (0 = ninguno) */
void menu_cancelar(void);
bool menu_tratar_mouse(int x, int y, unsigned botoes, unsigned mudaram);   /* true = el menú se quedó con el evento */
bool menu_tratar_tecla(int t);
void menu_desenhar(superficie_t *t);
void gui_tela_mudou(void);                          /* D059: recoloca las ventanas en el tamaño nuevo de la pantalla */

void menu_lista_iniciar(menu_lista_t *ml);
void menu_lista_linha(menu_lista_t *ml, const char *linha, int acao, const desenho_t *icone);
int  menu_lista_acao(const menu_lista_t *ml, int indice);
bool menu_lista_abrir(int x, int y, const menu_lista_t *ml, menu_resposta_t resposta, int janela, int dado);

/* Ventanas vistas por la barra de tareas (janelas.c, D065). */
typedef struct {
    int  id, pid;
    bool minimizada, foco;
    char titulo[48];
    char processo[32];
} janela_barra_t;
int  janelas_da_barra(janela_barra_t *v, int max);  /* en orden de creación, sin los diálogos */
void gui_janela_ativar(int id);                     /* trae al frente (y restaura si está minimizada) */
void gui_janela_minimizar(int id);
void gui_janela_menu(int id, int x, int y);         /* el menú de la barra de título, abierto en (x, y) */
bool gui_abrir_unico(const char *processo, const char *caminho, int argc, char *argv[]);
void gui_sair_tela_cheia(void);                     /* la ventana con el foco sale de la pantalla completa */
int  janelas_por_uso(janela_barra_t *v, int max);   /* orden de uso reciente: visibles desde arriba, luego minimizadas (D119) */
int  gui_janela_focada(void);                       /* id de la ventana con el foco, 0 = ninguna */
void gui_janela_pedir_fechar(int id);               /* el mismo aviso que la X (EV_FECHAR) */

/* Atajos de teclado del gestor de ventanas (atalhos.c, D119). */
bool atalhos_tecla(int t, uint32_t mods);           /* true = el atajo se quedó con la tecla */
bool janela_quer_imprpant(void);                    /* la ventana activa pidió recibir Impr Pant (JAN_IMPRPANT) */
void atalhos_tique(void);                           /* en cada tic: soltar Alt o la tecla Windows */
void atalhos_desenhar(superficie_t *s);             /* panel de Alt+Tab, encima de las ventanas */
bool atalhos_visivel(void);                         /* D195: el panel de Alt+Tab está en la pantalla */
const desenho_t *barra_icone_processo(const char *nome);   /* icono del programa por el nombre del proceso */

/* Barra superior: botón Inicio, botones de las tareas y reloj (barra.c, D065). */
void barra_desenhar(superficie_t *s);
bool barra_tratar_clique(int x, int y, unsigned botoes);  /* clic en la barra (y < BARRA_SUPERIOR) */
void barra_tique(void);                             /* en cada tic: reloj y cambios en las ventanas */
void barra_abrir_inicio(bool teclado);              /* abre (o cierra, si está abierto) el menú Inicio */
void barra_janela_criada(const char *processo);     /* cuenta los programas más usados */
/* Bandeja (volumen, red, temperatura) y su panel emergente (barra.c, D114). */
bool barra_popup_mouse(int x, int y, unsigned botoes, unsigned mudaram);   /* true = el panel se quedó con el evento */
bool barra_roda(int x, int y, int dz);              /* rueda sobre el icono o el panel del volumen */
bool barra_popup_tecla(int t);
void barra_popup_desenhar(superficie_t *s);         /* panel (o la sugerencia) encima de todo, debajo del menú */
bool barra_popup_visivel(void);                     /* D195: hay panel o sugerencia de la bandeja */
int32_t barra_info_bandeja(uint32_t ptr, uint32_t n);

#endif
