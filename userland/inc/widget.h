// SPDX-License-Identifier: GPL-3.0-only
/* widget.h - widgets de escritorio (D365, v8.5.3).
 *
 * Un widget es un programa normal que abre una ventana «de escritorio» (SIS_CRIAR_WIDGET): sin marco,
 * sin botón en la barra, debajo de las ventanas normales y sin tomar el foco. Esta biblioteca pone lo
 * común: abrir la ventana en su sitio guardado, la configuración (/home/.config/widgets.conf, de la
 * cuenta que tiene la sesión), el bucle con temporizador y los datos del sistema que leen el monitor
 * y Conky.
 *
 *   widgets.conf    clave=valor, una por línea:
 *                     <widget>=1|0            activo (arranca al iniciar la sesión)
 *                     <widget>.x / .y         posición en la pantalla (la guarda el propio widget al arrastrarlo)
 *                     <widget>.opacidad       10-100 (% con que se mezcla con el fondo)
 *                     <widget>.<opcion>       las propias de cada widget (reloj.modo=digital...) */
#ifndef SAGUI_WIDGET_H
#define SAGUI_WIDGET_H
#include "sagui.h"
#include "gui.h"

#define WIDGET_CONF "/home/.config/widgets.conf"

/* Los widgets incluidos. «programa» es el ejecutable; «chave» la clave de widgets.conf. */
typedef struct {
    const char *chave;
    const char *nome;        /* N_(): se traduce al usarlo */
    const char *descricao;   /* N_() */
    const char *programa;
} widget_def_t;
extern const widget_def_t widget_lista[];
extern const int widget_qtd;

/* Configuración. Devuelven 0 / el valor; «gravar» con valor NULL borra la clave. */
int  widget_ler(const char *chave, char *dst, int n);
int  widget_gravar(const char *chave, const char *valor);
int  widget_ler_int(const char *chave, int padrao);
int  widget_gravar_int(const char *chave, int valor);
bool widget_ativo(const char *widget);
void widget_ativar(const char *widget, bool ligar);

/* Abre la ventana del widget (l x a) y la coloca donde se dejó (x_def, y_def si no hay posición guardada;
 * un valor negativo cuenta desde el borde derecho/inferior de la pantalla). «clave»: el fondo
 * 0xFF000000 es transparente (esquinas redondeadas). La opacidad sale de <widget>.opacidad. */
jan_t *widget_abrir(const char *widget, int l, int a, int x_def, int y_def, bool clave, int opacidad_def);

/* Bucle del widget: llama a «tique» cada «ms» milisegundos (devuelve true si hay que repintar; no se
 * repinta si no cambió nada) y a «evento» con los clics y demás (también devuelve true para repintar).
 * Termina con EV_FECHAR, al quitar el widget (queda desactivado) o al cerrarse la sesión.
 * Guarda la posición cuando se arrastra. */
void widget_ejecutar(jan_t *j, const char *widget, int ms, bool (*tique)(jan_t *), bool (*evento)(jan_t *, const evento_t *));

/* Dibujo común: panel de fondo de SaguiOS con esquinas redondeadas (con «clave») y un filete de acento. */
#define WCOR_PAINEL   0x16231C
#define WCOR_PAINEL2  0x20362A
#define WCOR_BORDA    0x3C6A4E
#define WCOR_TEXTO    0xE8F2EA
#define WCOR_FRACO    0x93A89A
#define WCOR_ACENTO   0x5CD08A
#define WCOR_AMARELO  0xE8C65A
#define WCOR_AZUL     0x5AA6E8
#define WCOR_VERMELHO 0xE86A5A
#define WCOR_CLAVE    0xFF000000u
void widget_painel(jan_t *j, int x, int y, int l, int a, int raio);
void widget_texto(jan_t *j, int x, int y, const char *t, uint32_t cor, int escala);   /* la letra de 8x16 ampliada */
void widget_barra(jan_t *j, int x, int y, int l, int a, int pct, uint32_t cor);          /* barra de progreso 0-100 */
void widget_grafica(jan_t *j, int x, int y, int l, int a, const int *v, int n, int max, uint32_t cor);   /* v[0..n-1], el más nuevo al final */
void widget_numero(char *dst, int n, unsigned valor_x10);                                  /* 123 -> "12,3" */
void widget_bytes(char *dst, int n, unsigned long long b);                                 /* 1536 -> "1.5KiB" (como Conky) */
void widget_kb(char *dst, int n, unsigned kb);                                             /* KiB -> "1.5GiB" */
int  widget_seno(int grados);                                                              /* seno x 1000 (grados enteros) */
static inline int widget_coseno(int g) { return widget_seno(g + 90); }

/* Datos del sistema, con una muestra por llamada a widget_datos_tique() (cada widget la llama una vez por
 * ciclo; lo que no se pide, no se lee). */
void widget_datos_tique(void);
unsigned widget_cpu_x10(void);        /* uso total de las CPU desde el ciclo anterior, en décimas de % */
int  widget_ncpu(void);
void widget_mem(unsigned *usada_kb, unsigned *total_kb);
void widget_swap(unsigned *usada_kb, unsigned *total_kb);
void widget_rede(unsigned *rx_bps, unsigned *tx_bps, const int **hrx, const int **htx, int *n);   /* eth0: último segundo y los últimos n */
int  widget_disco(const char *ponto, unsigned *usado_kb, unsigned *total_kb);                    /* 0 = existe */
int  widget_temperatura(void);        /* décimas de grado de la primera sonda; -1 = no hay */
int  widget_bateria(void);            /* % ; -1 = no hay */
unsigned widget_segundos_ligado(void);
const char *widget_nome_host(void);
const char *widget_versao(void);
/* Procesos que más CPU (o memoria) gastan entre dos ciclos; i = 0 el primero. */
int  widget_top(int i, bool por_mem, char *nome, int n, unsigned *cpu_x10, unsigned *mem_kb, int *pid);

#endif
