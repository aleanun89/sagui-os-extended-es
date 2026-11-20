/* SPDX-License-Identifier: GPL-3.0-or-later */
/* campanha.h - campañas de Tzar: catálogo de mapas y misiones, orden de la historia, progreso guardado y paso de una misión a la
 * siguiente (D417, fase 7). Traducción a C de ReTzar (https://github.com/ColCarlo/ReTzar, GPL-3.0-or-later): scenario.rs,
 * y de frontend/menu_ui.rs next_map y CarryOver. */
#ifndef TZAR_CAMPANHA_H
#define TZAR_CAMPANHA_H

#include "guiao.h"

typedef struct {
	char id[200];                /* «fichero.wcm::mapa» en minúsculas: estable aunque cambie el orden */
	char titulo[100];
	char *descricao;             /* <mapa>.description */
	char fich[300];              /* relativo a la instalación: «Campaign/Tzar.wcm» o «Maps/MP1.wmp» */
	char dentro[200];            /* mapa anidado en la campaña («MAPS/x.wmp») o "" */
	int lado, jogadores[2];
	uint8_t tipo;                /* icono de la lista de mapas (byte 0x138 de la cabecera) */
	int inicial;                 /* disponible de entrada (MAPS\Maps.ini dice «yes») */
} TzMissao;

typedef struct {
	char ficheiro[300];          /* relativo: «Campaign/La Reconquista.wcm» */
	char titulo[100];
	char *descricao;
	TzMissao *missoes; int nmissoes;
} TzCampanha;

typedef struct {
	char raiz[512];
	TzMissao *soltas; int nsoltas;   /* mapas sueltos de Maps */
	TzCampanha *camp; int ncamp;     /* campañas de Campaign */
} TzMapas;

/* Lee los mapas sueltos y las campañas de una instalación. Una campaña o un mapa que no se pueda leer se salta y se cuenta en
 * *falhas (puede ser NULL). Devuelve -1 si la carpeta no existe. */
int tz_mapas_ler(TzMapas *c, const char *raiz, int *falhas);
void tz_mapas_liberar(TzMapas *c);
/* La misión de la campaña a la que lleva una acción «cargar mapa» (el nombre, o el que solo añade el sufijo de idioma): índice o -1. */
int tz_campanha_achar(const TzCampanha *c, const char *ruta);

/* Abre los contenedores de una misión y lee mapa y guion. */
typedef struct {
	TzHpfs c, sub; int tem_sub;
	TzMapa mapa;
	TzGuiao guiao;
	int guiao_ok;                /* 0 si el guion no se pudo leer (queda vacío) */
	char motivo[256];
} TzMissaoAberta;
int tz_missao_abrir(const TzMapas *c, const TzMissao *mi, TzMissaoAberta *out);
void tz_missao_fechar(TzMissaoAberta *a);

/* Lo que pasa a la misión siguiente: los héroes con nombre y las variables «#». */
#define TZ_LEVAR_HEROIS 64
typedef struct { TzHeroi herois[TZ_LEVAR_HEROIS]; int nherois; TzVar mantidas[256]; int nmantidas; } TzLevar;
void tz_levar_de(const TzMundo *m, const TzGuiao *g, TzLevar *out);
void tz_levar_para(TzMundo *m, TzGuiao *g, const TzLevar *l);

/* Progreso: las misiones alcanzadas (una id por línea en un fichero de texto «TZPROG1»). */
typedef struct { char **ids; int n, cap; } TzProgresso;
void tz_progresso_liberar(TzProgresso *p);
int tz_progresso_ler(TzProgresso *p, const char *ruta);                     /* no existe = vacío, devuelve 0 */
int tz_progresso_tem(const TzProgresso *p, const char *id);
int tz_progresso_marcar(TzProgresso *p, const char *id, const char *ruta);  /* añade y guarda; ruta puede ser NULL */
/* ¿Se puede jugar la misión? Las iniciales y las ya alcanzadas. */
int tz_missao_disponivel(const TzProgresso *p, const TzMissao *mi);

#endif
