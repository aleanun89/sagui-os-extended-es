/* SPDX-License-Identifier: GPL-3.0-or-later */
/* AoEN, copyright de su autor (el mantenedor de SaguiOS-Extended); GPL v3 o posterior. Port a SaguiOS: D382. */
/* Efectos gráficos graduables (D382, mejora 140): transiciones del terreno, agua animada, niebla difuminada, sombras,
 * partículas, animaciones de construcción y destrucción, día y noche, zoom suave y escala de la interfaz.
 * Todo se activa por separado en Opciones; hay tres perfiles (equipos lentos, normal y completo). */
#ifndef AOEN_EFECTOS_H
#define AOEN_EFECTOS_H
#include "game.h"

enum { FX_LENTO, FX_NORMAL, FX_COMPLETO, FX_PERSONAL };

typedef struct {
	int perfil;
	int transiciones;   /* mezcla suave entre terrenos distintos, con espuma en la costa */
	int agua;           /* reflejos que se mueven en el agua */
	int niebla;         /* niebla de guerra con borde difuminado */
	int sombras;        /* sombras de unidades y edificios */
	int particulas;     /* humo, polvo, fuego y escombros */
	int construccion;   /* andamios al construir, derrumbe al destruir */
	int diaynoche;      /* ciclo de luz (apagado en todos los perfiles menos el completo) */
	int suave;          /* zoom con filtrado bilineal */
	int escala;         /* escala de la interfaz: 0 automática, 1, 2 */
} Fx;

extern Fx efx;

enum { FX_EV_EDIFICIO_ACABADO, FX_EV_EDIFICIO_DESTRUIDO, FX_EV_UNIDAD_MUERE, FX_EV_ARBOL_CAE };

void fx_init(void);                 /* lee la configuración (y AOEN_EFECTOS=0/1/2) y elige el perfil según la CPU */
void fx_perfil(int p);              /* aplica un perfil */
void fx_guardar(void);              /* guarda en aoen.cfg */
const char *fx_nombre_perfil(int p);
int fx_escala_ui(int w, int h);     /* escala de interfaz efectiva (1 o 2) para esa resolución */

void fx_evento(int ev, const Entity *e);
void fx_actualizar(float dt);       /* mueve las partículas y crea las que salen del mundo (una vez por cuadro) */
void fx_dibujar(void);              /* partículas, en coordenadas de pantalla (tras las entidades) */
void fx_limpiar(void);              /* al empezar o cargar una partida */
int fx_derrumbes(void);
const Entity *fx_derrumbe(int i, float *t);   /* edificio en ruinas y su progreso 0..1 */
void fx_luz(int y0, int y1);        /* día y noche: tiñe el mundo entre las filas y0 e y1 */
float fx_luz_dia(void);             /* 1 = mediodía, 0 = noche cerrada */
int fx_particulas_vivas(void);
#endif
