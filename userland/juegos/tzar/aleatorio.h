/* SPDX-License-Identifier: GPL-3.0-or-later */
/* aleatorio.h - mapas aleatorios de Tzar (D417, fase 7). Traducción a C de ReTzar (https://github.com/ColCarlo/ReTzar,
 * GPL-3.0-or-later): src/random_map.rs. Da un TzMapa como el de un fichero, listo para tz_mundo_criar. */
#ifndef TZAR_ALEATORIO_H
#define TZAR_ALEATORIO_H

#include "mundo.h"

typedef struct {
	int lado;                    /* 64, 96, 128, 192, 256, 384 o 512 */
	int jogadores;               /* 2..8 reinos */
	int humano;                  /* índice (0..jogadores-1) del que juega la persona */
	int tipo;                    /* 0 al azar (parámetros del terreno), 1 archipiélagos, 2 islas, 3 continente, 4 lagos, 5 tierra */
	int recursos_mapa;           /* minas del mapa: 0 pocas, 1 normales, 2 muchas */
	int nivel_recursos;          /* recursos iniciales: 0 bajo, 1 medio, 2 alto (RES.INI) */
	int guardado;                /* torres de madera junto a cada castillo */
	int animais;
	int portas;                  /* puertas mágicas (la mitad que reinos) */
	int itens;                   /* objetos al azar: libros, equipo y pociones */
	int penhascos;               /* v8.5.6: montañas (cliffs de ReTzar: cadenas de piezas de 2x2 que no se cruzan) */
	char bioma[16];              /* grass, summer, snow, desert */
	uint64_t semente;
	uint8_t equipe[8];           /* equipo de cada reino (0 = el propio) */
	uint8_t civ[8];              /* civilización 1..3 de cada reino (0 = al azar) */
	/* D470: el comportamiento de la IA de cada reino (guion de DATA/AI, «» = el del mapa: random.ais) y la rendición de la IA */
	char ia[8][33];
	int rendicao;
} TzAleatorio;

void tz_aleatorio_padrao(TzAleatorio *o);   /* 2 reinos, 128, continente al azar, hierba */
/* Genera el mapa. cat da el tamaño de castillos, minas y torres (puede ser NULL: tamaños del original). Devuelve -1 con tz_erro(). */
int tz_mapa_aleatorio(TzMapa *out, TzRecursos *r, const TzCatalogo *cat, const TzAleatorio *op);

/* v8.5.5.2: las partidas en mapas aleatorios se pueden cargar. El mapa sale siempre igual de los parámetros (y el catálogo del
 * bioma), así que basta guardar estos en una línea de texto: «ALEAT lado jugadores humano tipo mapa recursos guardado animales bioma
 * semilla equipos[8] civilizaciones[8] puertas objetos [IA rendición guion[8]]» (puertas y objetos pueden faltar: valen 1; la parte
 * «IA» (D470) también: sin ella, guiones del mapa y sin rendición; un guion vacío se escribe «-»). Devuelven 0, o -1 con tz_erro() si el texto no sirve (valores fuera de rango incluidos). */
int tz_aleatorio_texto(const TzAleatorio *op, char *out, size_t n);
int tz_aleatorio_de_texto(const char *texto, TzAleatorio *op);

/* piezas que prueba el arnés */
uint64_t tz_aleatorio_proximo(uint64_t *estado);                                  /* SplitMix64 */
void tz_aleatorio_plasma(int tamanho, uint64_t semente, int aspereza, int divisor, uint8_t *saida);   /* tamanho² bytes */

#endif
