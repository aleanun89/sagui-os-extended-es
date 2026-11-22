/* SPDX-License-Identifier: MIT */
/* Capibara: motor de juegos 2D sencillo (D452). Copyright (c) 2026 los autores de Capibara. Licencia MIT.
 * Cabecera interna del motor. El núcleo (nodos, escenas, física, guiones, entrada, mezclador) no usa la SDL: así se
 * prueba en el PC sin ventana. Solo dibujo_sdl.c y plataforma_sdl.c la usan. Los comentarios y la API de Lua están en
 * español; los identificadores, en portugués como el resto del proyecto. */
#ifndef CAPI_H
#define CAPI_H
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#define CAPI_VERSION "0.1"
#define CAPI_NOME_MAX 48
#define CAPI_RUTA_MAX 128

/* ---------------------------------------------------------------------------------------------------- tipos de nodo */
typedef enum {
	T_NODO, T_NODO2D, T_SPRITE, T_MAPA, T_CAMARA, T_ETIQUETA, T_BOTON, T_AREA, T_CUERPO, T_PARTICULAS, T_AUDIO,
	T_TEMP, T_N
} CapiTipo;

typedef struct Nodo Nodo;
typedef struct Textura Textura;

/* ---------------------------------------------------------------------------------------------------- valores del texto */
#define VAL_MAX 64
typedef struct { char str; double num; char s[96]; } Tok;   /* str=1: cadena (s); si no, número (num) */
typedef struct { int n; Tok t[VAL_MAX]; } Valor;

/* ---------------------------------------------------------------------------------------------------- recursos */
struct Textura {
	char ruta[CAPI_RUTA_MAX];
	int w, h;
	uint32_t *px;        /* ARGB8888 (NULL si solo se conocen las medidas: pruebas sin ventana) */
	void *gpu;           /* lo que quiera la capa de dibujo (SDL_Texture *) */
	int id;
	Textura *sig;
};
/* decodificador de imágenes (la capa de la SDL lo pone): devuelve píxeles ARGB8888 con malloc, o NULL */
typedef uint32_t *(*RecDecodificador)(const unsigned char *datos, size_t n, int *w, int *h);
void rec_poner_decodificador(RecDecodificador f);
Textura *rec_textura(const char *ruta);          /* con caché; NULL si no existe */
Textura *rec_textura_memoria(const char *nombre, int w, int h, uint32_t *px);   /* creada por el motor (fuente, blanco) */
typedef struct { int16_t *pcm; int n; } Sonido;  /* estéreo intercalado a 44100 Hz */
Sonido *rec_sonido(const char *ruta);            /* con caché; NULL si no existe o no es un WAV válido */
void rec_liberar_todo(void);
void rec_liberar_proyecto(void);                 /* editor: suelta las texturas y sonidos del proyecto (no la fuente incorporada) */
int rec_numero_texturas(void);
Textura *rec_primera(void);
/* Lectura de ficheros del proyecto (de la carpeta o del paquete .capi) */
unsigned char *cap_leer(const char *ruta, size_t *tam);   /* con malloc; NULL si no existe; deja un 0 tras los datos */
int cap_existe(const char *ruta);
int cap_abrir_proyecto(const char *ruta, char *err, size_t nerr);   /* carpeta o fichero .capi */
int cap_empaquetar(const char *carpeta, const char *salida, char *err, size_t nerr);
const char *cap_raiz(void);
int cap_es_paquete(void);
char *cap_unir(const char *base, const char *rel, char *out, size_t n);   /* une rutas y quita «./» y «x/../» */

/* ---------------------------------------------------------------------------------------------------- formas y física */
typedef struct { int forma; float w, h, radio, ox, oy; unsigned capa, mascara; } Forma;   /* forma 0 caja, 1 círculo */

/* ---------------------------------------------------------------------------------------------------- datos por tipo */
typedef struct { char nome[24]; int fr[48]; int n; float fps; int bucle; } Anim;
typedef struct {
	char ruta[CAPI_RUTA_MAX]; Textura *tex;
	int hf, vf, fot;                      /* cuadrícula de fotogramas y fotograma actual */
	int rx, ry, rw, rh;                   /* región del atlas en píxeles (rw=0: toda la textura o la cuadrícula) */
	int volt_h, volt_v, centrado;
	float ofx, ofy;
	Anim anims[12]; int nanims; int act; float t; float vel; int reproduciendo; char act_nome[24]; int fin_enviado;
} DSprite;

#define MAPA_TROZO 16
typedef struct { char nome[24]; int visible, solida; int16_t **trozos; } Capa;   /* celda: -1 vacía, o el índice de casilla */
typedef struct {
	char ruta[CAPI_RUTA_MAX]; Textura *tex;
	int cw, ch;                           /* tamaño de la casilla */
	int sep;                              /* separación en el atlas */
	int cols;                             /* casillas por fila del atlas */
	int w, h;                             /* celdas (ancho y alto del mapa) */
	int tw, th;                           /* trozos */
	Capa capas[6]; int ncapas;
	unsigned char solidas[1024]; int usa_solidas;
	unsigned capa_col, mascara_col;
} DMapa;

typedef struct {
	int actual; float zoom, suav; float limites[4]; int con_limites; float dx, dy; int seguir_id;
	float sac_amp, sac_t, sac_dur; float cx, cy; char seguir_ruta[64];
} DCamara;

typedef struct {
	char texto[256]; int escala; int alineado;   /* 0 izquierda, 1 centro, 2 derecha */
	float ancho;                                 /* ancho máximo (0: sin ajuste de línea) */
	int sombra; int fijo;
} DEtiqueta;

typedef struct {
	char texto[64]; float w, h; int estado;      /* 0 normal, 1 encima, 2 pulsado */
	uint8_t col_n[4], col_e[4], col_p[4]; int escala; int fijo; int deshabilitado; int previo;
} DBoton;

typedef struct { int id; } Dentro;
typedef struct { Forma f; int vigilar; Dentro *d; int nd, cd; Dentro *d_ant; int nd_ant, cd_ant; } DArea;
typedef struct {
	Forma f; int modo;                           /* 0 estático, 1 cinemático, 2 dinámico */
	float vx, vy, escala_grav, rebote, friccion;
	int suelo, techo, pared; float nx, ny; int col_id;
} DCuerpo;

typedef struct { float x, y, vx, vy, vida, vida0, rot, vrot; } Part;
typedef struct {
	char ruta[CAPI_RUTA_MAX]; Textura *tex; int hf, vf, fot;
	int emitiendo, una_vez, cantidad, max; float vida, vida_az, v_min, v_max, angulo, dispersion, grav, tam_ini, tam_fin;
	uint8_t col_ini[4], col_fin[4]; float tasa_acc; uint32_t sem; int aditivo, local;
	Part *p; int np;
} DParticulas;

typedef struct { char ruta[CAPI_RUTA_MAX]; Sonido *snd; float vol; int bucle, musica, autoplay, voz; } DAudio;
typedef struct { float tiempo, resto; int una_vez, activo, autoinicio; } DTemp;

/* ---------------------------------------------------------------------------------------------------- nodo */
typedef struct { char senal[32]; int ref_fn; int destino_id; char metodo[32]; } Conexion;
enum { F_VISIBLE = 1, F_LISTO = 2, F_BORRAR = 4, F_ARBOL = 8, F_INTERP = 16, F_PROC = 32, F_FISICA = 64, F_ENTRADA = 128,
       F_PAUSA = 256, F_PROCESAR = 512 /* procesar (script y animación) */ };

struct Nodo {
	int id; CapiTipo tipo; char nombre[CAPI_NOME_MAX];
	Nodo *padre, **hijos; int nh, ch;
	unsigned flags;
	float x, y, rot, sx, sy, ax, ay;            /* posición local, rotación (grados), escala y posición previa (interpolación) */
	int z; uint8_t mod[4];                      /* orden de dibujo (relativo al padre) y color de modulación */
	float m[6]; unsigned m_ep;                  /* matriz global (a b c d tx ty) y su época */
	float mr[6]; unsigned long long mr_ep;      /* matriz global interpolada del cuadro */
	char grupos[4][20]; int ng;
	Conexion *con; int ncon, ccon;
	int ref_clase, ref_datos;                   /* Lua: tabla de métodos del guion y tabla de variables del nodo */
	char guion[CAPI_RUTA_MAX]; char escena[CAPI_RUTA_MAX];
	void *d;                                    /* datos del tipo */
	int cuenta_mov;                             /* cuántas veces se movió (pruebas) */
};

/* ---------------------------------------------------------------------------------------------------- nodos */
Nodo *nodo_nuevo(CapiTipo t, const char *nombre);
void nodo_liberar_ya(Nodo *n);                  /* libera el nodo y sus hijos (sin pasar por la cola) */
void nodo_agregar(Nodo *padre, Nodo *hijo);     /* lo mete en el árbol; si el padre está en el árbol llama a _listo */
void nodo_quitar(Nodo *n);                      /* lo saca de su padre sin liberarlo */
void nodo_encolar_liberar(Nodo *n);
void nodo_vaciar_cola(void);
Nodo *nodo_de_id(int id);
Nodo *nodo_buscar(Nodo *desde, const char *ruta);        /* ruta relativa: «A/B», «..», «.» o «/A/B» desde la raíz */
Nodo *nodo_buscar_nombre(Nodo *desde, const char *nombre);   /* recursivo */
void nodo_grupo_agregar(Nodo *n, const char *g);
void nodo_grupo_quitar(Nodo *n, const char *g);
int nodo_en_grupo(const Nodo *n, const char *g);
int nodo_en_arbol(const Nodo *n);
void nodo_matriz(Nodo *n, float out[6]);       /* matriz global lógica (sin interpolar) */
void nodo_matriz_dibujo(Nodo *n, float out[6]);   /* matriz global interpolada (la que se dibuja) */
void nodo_pos_global(Nodo *n, float *x, float *y);
void nodo_poner_pos_global(Nodo *n, float x, float y);
void nodo_teletransportar(Nodo *n, float x, float y);
void nodo_fijar_cuadro_dibujo(unsigned long long cuadro, float alpha);
void nodo_moverse(Nodo *n);                      /* avisa de que cambió una transformación (invalida las cachés) */
extern unsigned g_epoca;
int nodo_es2d(const Nodo *n);
extern const char *const CAPI_TIPOS[T_N];        /* nombres en español: Nodo, Nodo2D, Sprite, MapaCasillas... */
int capi_tipo_por_nombre(const char *s);
void nodo_recorrer(Nodo *r, void (*f)(Nodo *, void *), void *u);   /* preorden */
int nodo_contar(Nodo *r);

/* propiedades (las usan el cargador de escenas y Lua) */
int prop_poner(Nodo *n, const char *nombre, const Valor *v);       /* 1 bien, 0 desconocida */
int prop_obtener_num(Nodo *n, const char *nombre, double *out);    /* 1 si es numérica o booleana */
int prop_obtener_str(Nodo *n, const char *nombre, char *out, size_t no);   /* 1 si es una cadena */
int prop_existe(CapiTipo t, const char *nombre);
int prop_es_bool(CapiTipo t, const char *nombre);
int prop_es_entero(CapiTipo t, const char *nombre);
void prop_tras_cambiar(Nodo *n, const char *nombre);

/* Introspección de la tabla (D460): el editor genera su inspector a partir de ella, sin código por nodo. */
enum { PK_FLOAT, PK_INT, PK_BOOL, PK_TEXT, PK_COLOR, PK_V2, PK_V2I, PK_V4, PK_V4I, PK_ENUM, PK_LISTA };
typedef struct {
	const char *nombre;      /* clave en el fichero .esc */
	int tipo;                /* PK_* */
	int n;                   /* componentes (1, 2 o 4) */
	float min, max, paso;    /* límites sugeridos para el editor */
	const char *opciones;    /* PK_ENUM: nombres separados por «|» */
	const char *etiquetas;   /* vectores: nombre de cada componente separado por «|» */
	int ruta;                /* PK_TEXT: 1 imagen, 2 sonido, 3 guion (para elegir fichero) */
	int oculta;              /* 1: alias de otra propiedad o sin edición en el inspector */
	int grupo;               /* 0 nodo, 1 transformación, 2 apariencia, 3 propias del tipo */
} PropInfo;
int prop_listar(CapiTipo t, int i, PropInfo *out);                       /* 0 si i está fuera de la tabla */
int prop_texto(Nodo *n, const char *nombre, char *out, size_t no);        /* valor actual en el formato del fichero de escena */
int nodo_caja_local(Nodo *n, float *x, float *y, float *w, float *h);     /* caja que ocupa el nodo en sus coordenadas locales (0 si no tiene) */
extern int g_modo_editor;      /* 1: el editor muestra escenas (sin guiones ni sonidos) */

/* ---------------------------------------------------------------------------------------------------- texto (formato) */
typedef struct { int nt; char t[6][96]; int na; char an[10][32]; char av[10][96]; } Cab;   /* [Tipo Nombre clave=valor ...] */
typedef struct {
	int (*seccion)(void *u, const Cab *c, int linea);
	int (*propiedad)(void *u, const char *clave, const Valor *v, int linea);
	int (*fila)(void *u, const char *texto, int linea);
} TextoCB;
int texto_analizar(const char *buf, TextoCB *cb, void *u, char *err, size_t nerr);

/* ---------------------------------------------------------------------------------------------------- escenas */
Nodo *escena_instanciar(const char *ruta, char *err, size_t nerr);      /* NULL si falla (error en err) */
Nodo *escena_instanciar_texto(const char *texto, const char *nombre, char *err, size_t nerr);
typedef struct {
	char nombre[64]; char principal[CAPI_RUTA_MAX]; int ancho, alto; int entera; uint8_t fondo[4];
	float gravedad; int paso; int semilla; int zoom_ventana;
} Proyecto;
extern Proyecto g_proy;
void escena_vaciar_cache(void);
int proyecto_cargar(char *err, size_t nerr);    /* lee capibara.cfg del proyecto abierto */
int escena_cambiar(const char *ruta);           /* se hace al final del cuadro */

/* ---------------------------------------------------------------------------------------------------- entrada */
#define ACC_MAX 32
typedef struct { char tipo; char nome[32]; float umbral; } Ligadura;   /* tipo: t tecla, r ratón, b botón de mando, e eje, h tecla recibida por texto */
typedef struct { char nome[24]; Ligadura l[8]; int nl; int pres, antes, fuerza_x1000; int rec_pres, rec_sol; } Accion;
extern Accion g_acc[ACC_MAX]; extern int g_nacc;
int accion_declarar(const char *nome, const char *ligaduras);    /* «tecla:Left tecla:A mando:izquierda eje:leftx-» */
void entrada_tecla(const char *nome, int abajo);
void entrada_raton_boton(int b, int abajo);                       /* 1 izquierdo, 2 central, 3 derecho */
void entrada_raton_pos(int x, int y);
void entrada_mando_boton(const char *nome, int abajo);
void entrada_mando_eje(const char *nome, float v);                /* -1..1 */
void entrada_nuevo_cuadro(void);                                   /* borra «recién pulsada/soltada» */
int accion_presionada(const char *a); int accion_recien(const char *a); int accion_soltada(const char *a);
float accion_fuerza(const char *a);
float entrada_eje(const char *neg, const char *pos);
extern int g_raton_x, g_raton_y, g_raton_pulso_x, g_raton_pulso_y;   /* ... y dónde estaba al pulsar el botón */                                   /* en píxeles lógicos de la pantalla */
typedef struct { char tipo[16]; char nome[32]; int abajo, x, y, boton; } Evento;   /* tipo: accion, tecla, raton, raton_mov */
void entrada_enviar_scripts(Nodo *raiz);                           /* manda _entrada(ev) a los nodos */
void guion_enviar_evento(Nodo *raiz, const Evento *e);
extern int g_en_fisica;                                            /* 1 mientras corre un paso de física (las «recién» se leen por paso) */
void entrada_fin_paso_fisica(void);
void entrada_actualizar(void);
void entrada_pulsa(const char *nome, int abajo);                   /* para las pruebas: simula una acción por nombre */

/* ---------------------------------------------------------------------------------------------------- física */
void fisica_preparar(Nodo *raiz);                 /* recoge cuerpos, áreas y mapas del árbol (antes de los guiones de _fisica) */
void fisica_paso(Nodo *raiz, float dt);           /* cuerpos dinámicos y áreas (después de los guiones) */
void fisica_mover(Nodo *n, float dx, float dy, float *rx, float *ry);   /* cuerpo cinemático: mueve y desliza */
int fisica_choca_mapa_punto(Nodo *raiz, float x, float y);
extern float g_gravedad;
extern long g_fis_pasos, g_fis_pares;
void fisica_area_cuerpos(Nodo *a, Nodo ***lista, int *n);
int fisica_solapa(Nodo *a, Nodo *b);
int mapa_celda(Nodo *m, int capa, int cx, int cy);                    /* -1 si vacía o fuera */
void mapa_poner(Nodo *m, int capa, int cx, int cy, int v);
int mapa_solida(Nodo *m, int cx, int cy);
int mapa_capa(Nodo *m, const char *nome);

/* ---------------------------------------------------------------------------------------------------- dibujo (sin SDL) */
typedef struct {
	float x[4], y[4], u[4], v[4];     /* esquinas: arriba-izq, arriba-der, abajo-der, abajo-izq (coordenadas de pantalla y de textura en píxeles) */
	Textura *tex; uint32_t col; int mezcla;   /* mezcla: 0 normal, 1 aditiva */
	int z; unsigned seq; int recto;   /* recto: sin giro (se puede dibujar con una copia) */
	int abs;                          /* 1: coordenadas del lienzo, sin la transformación de la vista (HUD) */
} Quad;
extern Quad *g_quads; extern int g_nquads;
extern int g_vista_w, g_vista_h;
void dib_empezar_cuadro(Nodo *raiz, float alpha);    /* recolecta las órdenes de dibujo del árbol */
void dib_hud(void);
extern int g_hud, g_dep_col; extern long g_hud_lotes, g_hud_nodos;
extern char g_hud_txt[10][100]; extern int g_hud_n;      /* líneas del HUD que pone la plataforma */
int entrada_boton_raton(int b);
void dib_texto(float x, float y, const char *s, int escala, uint32_t col, int sombra, int z);
float dib_texto_ancho(const char *s, int escala);
void dib_rect(float x, float y, float w, float h, uint32_t col, int z, int mezcla);
Textura *dib_fuente(void);
void dib_cam_mundo_a_pantalla(float wx, float wy, float *sx, float *sy);
void dib_cam_pantalla_a_mundo(float sx, float sy, float *wx, float *wy);
Nodo *dib_camara_actual(void);
void dib_camara_libre(int activa, float cx, float cy, float zoom);   /* editor: cámara propia (centro del mundo y zoom) en lugar de la del juego */
void camara_actualizar(Nodo *raiz, float dt);

/* ---------------------------------------------------------------------------------------------------- partículas, animación, temporizadores */
void particulas_actualizar(Nodo *n, float dt);
void particulas_emitir(Nodo *n, int cantidad);
void sprite_actualizar(Nodo *n, float dt);
void sprite_animar(Nodo *n, const char *nome);

/* ---------------------------------------------------------------------------------------------------- audio */
#define MEZ_TASA 44100
#define MEZ_VOCES 24
int mez_voz(const Sonido *s, float vol, float pan, int bucle, int musica);   /* devuelve el número de voz (o -1) */
void mez_detener(int voz);
void mez_detener_musica(void);
void mez_musica(const Sonido *s, float vol, int bucle);
void mez_volumen(float v);
void mez_renderizar(int16_t *sal, int cuadros);   /* hilo de audio: llena «cuadros» cuadros estéreo */
typedef struct { long long cuadros; long llamadas; long voces; long perdidas; int activas; } MezEst;
MezEst mez_estadisticas(void);
int mez_grabar(int max_cuadros); int mez_grabar_guardar(const char *ruta);
int mez_voz_activa(int voz);

/* ---------------------------------------------------------------------------------------------------- guiones (Lua) */
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
extern lua_State *g_L;
int guion_iniciar(void);
void guion_cerrar(void);
int guion_cargar(Nodo *n, const char *ruta);                       /* ejecuta el guion y lo liga al nodo */
int guion_llamar(Nodo *n, const char *metodo, int tiene_num, double num);   /* llama a n:metodo([num]); 0 si el guion no lo tiene */
void guion_proceso(Nodo *raiz, float dt);
void guion_fisica(Nodo *raiz, float dt);
void guion_listo(Nodo *n);                                         /* el nodo y sus hijos (hijos primero) */
void guion_dato_liberar(Nodo *n);
void guion_poner_dato(Nodo *n, const char *clave, const Valor *v);   /* variable del nodo desde el texto de la escena */
void guion_empujar_nodo(lua_State *L, Nodo *n);
void sen_emitir(Nodo *n, const char *senal, int nargs);            /* con los nargs valores en la pila de Lua */
void sen_conectar_nodo(Nodo *n, const char *senal, Nodo *dest, const char *metodo);
int guion_ejecutar_texto(const char *texto, const char *nombre);   /* para pruebas: ejecuta un trozo de Lua */
extern int g_salir;

/* ---------------------------------------------------------------------------------------------------- bucle */
typedef struct {
	Nodo *arbol;                /* raíz oculta del árbol; la escena actual cuelga de ella */
	Nodo *raiz;                 /* la escena actual */
	unsigned long long cuadro; /* cuadros dibujados */
	double tiempo;
	float alpha;
} Mundo;
extern Mundo g_mundo;
int mundo_iniciar(char *err, size_t nerr);
void mundo_cambiar_escena(const char *ruta);
void mundo_paso_fisica(float dt);              /* un paso fijo: física, guiones de _fisica, áreas */
void mundo_proceso(float dt);                  /* _proceso, animaciones, temporizadores, partículas, cámara */
void mundo_fin_cuadro(void);                   /* cola de liberar y cambio de escena */
void mundo_cerrar(void);
extern int g_hz;

/* utilidades */
void capi_log(const char *fmt, ...);
extern void (*g_capi_log_hook)(const char *);   /* si no es NULL, recibe los mensajes en vez de la salida estándar */
uint32_t capi_rnd(uint32_t *s);
static inline uint32_t col_pack(const uint8_t c[4]) { return ((uint32_t)c[3] << 24) | ((uint32_t)c[0] << 16) | ((uint32_t)c[1] << 8) | c[2]; }
static inline float capi_min(float a, float b) { return a < b ? a : b; }
static inline float capi_max(float a, float b) { return a > b ? a : b; }
static inline float capi_clamp(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }

#endif
