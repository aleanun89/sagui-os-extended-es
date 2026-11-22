/* SPDX-License-Identifier: MIT */
/* escritor.h - edición de escenas (.esc) línea a línea, conservando los comentarios, el orden y los espacios (D460).
 * El editor de Capibara no guarda un árbol de nodos reescrito, sino el propio texto de la escena: cada orden (poner una
 * propiedad, añadir, borrar, renombrar o mover un nodo, pintar una casilla...) cambia solo las líneas que le tocan. Lo que
 * no se toca sale igual que entró (también las líneas en blanco y el final de línea). No usa la SDL. */
#ifndef CAPI_ESCRITOR_H
#define CAPI_ESCRITOR_H
#include "capi.h"

enum { ES_NODO, ES_INSTANCIA, ES_CAPA, ES_CONEXION, ES_OTRO };

typedef struct {
	int clase;                       /* ES_* */
	int ini, fin;                    /* líneas [ini, fin): la cabecera y su contenido (sin las líneas en blanco o de comentario del final) */
	int ant;                         /* primera línea de los comentarios pegados encima de la cabecera (= ini si no hay) */
	char tipo[24];                   /* «Sprite», «Instancia», «capa»... */
	char nombre[CAPI_NOME_MAX];
	char padre[CAPI_RUTA_MAX];       /* atributo padre= tal como está en el fichero ("" = la raíz) */
	char ruta[CAPI_RUTA_MAX];        /* ruta del nodo desde la raíz de la escena; la raíz es "." */
	int mapa;                        /* ES_CAPA: sección del MapaCasillas al que pertenece (-1 si ninguna) */
} EscSec;

typedef struct {
	char **lin; int n, cap;          /* las líneas, sin el salto final */
	int nl_final;                    /* el texto acababa en salto de línea */
	EscSec *sec; int nsec, csec;
} EscDoc;

EscDoc *esc_abrir_texto(const char *texto);
void esc_cerrar(EscDoc *d);
char *esc_texto(const EscDoc *d, size_t *n);                 /* el texto completo (malloc) */
void esc_analizar(EscDoc *d);                                /* se llama sola tras cada orden */
int esc_buscar_ruta(const EscDoc *d, const char *ruta);      /* sección de un nodo o instancia por su ruta ("." = raíz); -1 si no hay */
int esc_buscar_nombre_libre(const EscDoc *d, const char *base_padre, const char *base, char *out, size_t no);   /* Moneda -> Moneda2 */
const char *esc_base_hijos(const EscDoc *d, int sec);        /* valor de padre= que llevan los hijos de esta sección ("" para la raíz) */

/* propiedades de una sección de nodo */
int esc_prop_valor(const EscDoc *d, int sec, const char *clave, char *out, size_t no);   /* 1 si está; el texto del valor sin comentario */
int esc_prop_poner(EscDoc *d, int sec, const char *clave, const char *valor);             /* la crea si falta */
int esc_prop_quitar(EscDoc *d, int sec, const char *clave);
int esc_attr_valor(const EscDoc *d, int sec, const char *clave, char *out, size_t no);   /* atributo de la cabecera (padre, escena, desde...) */
int esc_attr_poner(EscDoc *d, int sec, const char *clave, const char *valor);             /* "" lo quita */

/* nodos */
int esc_nodo_nuevo(EscDoc *d, const char *tipo, const char *nombre, const char *padre_ruta, int antes);   /* sección nueva; antes = sección hermana o -1 (al final) */
int esc_instancia_nueva(EscDoc *d, const char *escena, const char *nombre, const char *padre_ruta, int antes);
int esc_nodo_borrar(EscDoc *d, int sec);                     /* con sus hijos y las conexiones que los nombran */
int esc_nodo_duplicar(EscDoc *d, int sec);                   /* devuelve la sección de la copia, justo detrás */
int esc_nodo_renombrar(EscDoc *d, int sec, const char *nombre);   /* actualiza también los hijos, las conexiones y «seguir» */
int esc_nodo_mover(EscDoc *d, int sec, const char *padre_ruta, int antes);   /* cambia de padre y/o de sitio; devuelve la sección nueva o -1 */
int esc_es_descendiente(const EscDoc *d, int sec, int de);

/* conexiones */
int esc_conexion_nueva(EscDoc *d, const char *desde, const char *senal, const char *hacia, const char *metodo);
int esc_conexion_borrar(EscDoc *d, int sec);                 /* quita una sección [Conexion] */

/* mapas de casillas */
int esc_capas(const EscDoc *d, int mapa, int *secs, int max);                   /* las secciones [capa] del mapa */
int esc_celda_leer(const EscDoc *d, int capa_sec, int cx, int cy);             /* -1 vacía */
int esc_celda_poner(EscDoc *d, int capa_sec, int cx, int cy, int v, int ancho, int alto);   /* -1 la vacía; crea las filas que falten */
int esc_capa_nueva(EscDoc *d, int mapa, const char *nombre, int solida, int ancho, int alto);

int esc_guardar(const EscDoc *d, const char *ruta);                            /* 0 bien */
void texto_valor(const char *texto, Valor *v);                                 /* separa un valor en trozos (como el cargador) */

#endif
