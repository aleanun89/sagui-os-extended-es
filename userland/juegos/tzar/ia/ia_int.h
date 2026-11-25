/* SPDX-License-Identifier: GPL-3.0-or-later */
/* ia_int.h - lo interno del jugador de la IA de Tzar, compartido por ia_*.c (D417, fase 6).
 *
 * Traducción a C de ReTzar (https://github.com/ColCarlo/ReTzar, GPL-3.0-or-later): el Player de src/gameplay/ai.rs y
 * lo que guardan think.rs (listas y grupos de campesinos), rules.rs (lo que cada parte reserva), harvest.rs (la busca
 * de un bosque), places.rs y records.rs (lugares y lo que el jugador sabe de ellos).
 */
#ifndef TZAR_IA_INT_H
#define TZAR_IA_INT_H

#include "ia.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* grupos de campesinos (think.rs group: +0x9a6 ociosos, +0x9aa constructores, +0x9ae ordeñadores, +0x9b2 los de cada
 * recurso) */
enum { G_OCIOSO = 0, G_CONSTRUTOR, G_ORDENHA, G_OURO, G_PEDRA, G_COMIDA, G_MADEIRA, G_N };
/* las partes que reservan recursos (rules.rs bucket, IA+0x93c + 0x14 i); cada una se sirve tras las anteriores */
enum { B_NAVAL = 0, B_ARMAZENS, B_TRABALHADORES, B_GUIAO, B_DEFESA, B_N, B_NENHUMA = 0xff };
/* categorías de fuerzas y sus máscaras (places.rs category, mask) */
enum { CAT_MILITAR = 0, CAT_CIVIL, CAT_TORRES, CAT_EDIFICIOS, CAT_QUARTEIS, CAT_N };
enum { M_MILITAR = 1, M_TORRES = 2, M_CIVIL = 4, M_EDIFICIOS = 8, M_QUARTEIS = 0x10 };
/* clases de lugar (places.rs kind) */
enum { L_CASA = 1, L_RECURSO, L_PERDAS, L_PORTAL, L_BASE, L_UNIDADES };

/* clases que la IA nombra y mundo.h no (world.rs class) */
enum {
	IC_MAGE = 17, IC_PRIEST = 18, IC_JIHAD = 33, IC_TOWER_WOOD = 40, IC_TOWER_WOOD2 = 41, IC_TOWER_STONE2 = 42,
	IC_TAVERN = 48, IC_OUTPOST = 52, IC_CATHEDRAL = 55, IC_ACADEMY = 56, IC_WORKSHOP = 59, IC_MAGETOWER = 60,
	IC_TCGUILD = 65, IC_MAGICGATEWAY = 72, IC_SHAOLINN = 70, IC_MOSQUE = 71, IC_RESITEM = 80, IC_ITEM_PLUSPLUS = 81,
	IC_ITEM_MAGIC = 104, IC_ITEM_HEALTH = 106, IC_CRUSADEANCHOR = 116,
};

#define IA_PERIODOS {2000, 1500, 1000}    /* think.rs PERIODS: ms entre miradas por dificultad */
#define IA_PAZ_FACIL_MS 1800000u          /* ai.rs EASY_PEACE_MS: 30 min sin atacar casas en fácil */
#define IA_PAUSA_CASA_MS 900000u          /* war.rs HOME_PAUSE_MS */
#define IA_FALAS 16

typedef struct { TzId *v; int n, cap; } TzIaLista;

/* lo que el jugador sabe del trabajo de un campesino (think.rs Peon) */
typedef struct {
	uint8_t grupo;                /* G_*, 0xff = ninguno */
	uint8_t campo;                /* cuenta como granjero de su campo */
	TzId alvo;                    /* lo que trabaja (mina, campo, vaca, obra) */
	int16_t x, y;                 /* centro del trabajo */
} TzIaPeao;

typedef struct { uint16_t n; uint32_t forca; } TzIaForca;
typedef struct { TzIaForca c[CAT_N]; } TzIaForcas;

/* un lugar del mapa y lo que el jugador sabe de él (places.rs Place y records.rs Record juntos) */
typedef struct {
	uint8_t tipo;                 /* L_* */
	uint32_t param;               /* reino (casa, base, unidades), recurso (0 oro, 1 piedra) o máscara (pérdidas) */
	int16_t rect[4];              /* casillas, alineado a sectores de 8x8 */
	TzId ancora;
	int16_t arect[4];             /* el rectángulo del ancla */
	TzIaForcas amigo, hostil, amigo_vindo, meu, meu_vindo;
	uint32_t minimo, desejado, flags, distancia, prioridade;
} TzIaLugar;

typedef struct { int16_t x, y; uint32_t mascara, ate_ms; } TzIaPerda;

/* D470: un transporte por el agua (transport.rs Transport y Ride, llevado por el jugador) */
#define IA_TRANSPORTES 8
#define IA_PASSAGEIROS 16
enum { TR_CARREGAR = 1, TR_NAVEGAR };
typedef struct {
	TzId navio;
	int16_t ponto[2];             /* adonde van */
	uint16_t desembarque;         /* la isla en la que desembarcan */
	uint8_t passo;                /* TR_* */
	int32_t npass;
	TzId pass[IA_PASSAGEIROS];
	uint16_t paciencia[IA_PASSAGEIROS];   /* miradas esperando a cada uno (en NAVEGAR, [0] = descargas pedidas) */
	uint8_t anda[IA_PASSAGEIROS];         /* al desembarcar sigue andando al punto */
} TzIaTransporte;
enum { IA_VIA_NAO = 0, IA_VIA_TERRA = 1, IA_VIA_MAR = 2 };   /* reach.rs NO, BY_LAND, BY_SEA */

/* D470: un muro marcado para abrir brecha (breach.rs marked) */
#define IA_BRECHAS 32
typedef struct { TzId muro; int16_t x, y; } TzIaBrecha;


struct TzIa {
	TzMundo *m;
	const TzIaBiblioteca *b;
	int k, civ;
	uint16_t modo;                /* IA+0x910: bit 0 economía, bit 1 guerra; 0x80 pausa fijada; 0x4000 SAY */
	char guiao[64];
	TzIaPrograma prog;
	TzRng rng;
	uint32_t paz_ate;             /* IA+0x1a20, ms */
	uint32_t proximo_olhar;       /* ms */
	uint8_t fase;                 /* IA+0x9a0 */
	uint8_t tem_casa, sem_casa;   /* IA+0x900, IA+0x908 */
	int16_t casa[2];
	uint16_t ilha;
	uint16_t tope;                /* IA+0x912 */
	int32_t disponivel[4], comprometido[4];   /* IA+0x914, IA+0x928 */
	int32_t baldes[B_N][4];       /* IA+0x93c */
	uint8_t trancado;             /* IA+0x988: NEEDS_LOCKED */
	uint8_t ordem[4];             /* IA+0x9fa: recursos por necesidad */
	uint16_t quer[4];             /* IA+0x9fe */
	uint8_t bloqueado[4];         /* IA+0xa06 */
	uint8_t construindo;          /* 0x5abb10 */
	uint8_t *sorteio;             /* RANDOM por condición: 0 sin sortear, 1 no, 2 sí */
	/* la busca de un bosque (harvest.rs Forest) */
	struct { uint16_t raio, guardado; int16_t at[2]; uint8_t tem_madeira, nenhuma; int16_t madeira[2]; } floresta;
	/* qué reglas leen cada valor (rules.rs Watch), hecho del programa: claves -> reglas */
	int *vig_ini, *vig_reg, nchaves, vig_feito;
	/* recuentos de la mirada anterior por tipo, para volver a evaluar lo que cambió */
	uint32_t *conta_ant;
	uint8_t *pesq_ant;            /* investigaciones hechas vistas */
	/* listas, rehechas en cada mirada (think.rs Lists) */
	TzIaLista campones, grupo[G_N], castelos, nus, obras, magos, armazens, serrarias, granjas, minas, campos, torres,
	          postos, oficinas, bonecos, exercito, quarteis;
	TzIaPeao *peao; int cappeao;  /* por índice de objeto */
	uint32_t *ocioso;             /* ms + 1 desde que está ocioso, por índice; 0 = no */
	uint32_t *mandado;            /* tic + 1 de la última orden de la IA a ese objeto */
	/* lugares y su registro (places.rs, records.rs) */
	TzIaLugar *lugar; int nlugares, caplugares;
	TzIaPerda perda[32]; int nperdas;
	TzId ameaca[8]; int nameacas;     /* amenazas vistas en los eventos (milicias) */
	uint32_t cursor_ev;
	TzId explorador;              /* unidad que explora */
	/* árbol de la civilización (tree.rs), por tipo */
	uint8_t *presente; int16_t *produtor;
	/* lo que dijo */
	char falas[IA_FALAS][160]; uint32_t nfalas;
	TzIaContas contas;
	/* D470 */
	uint8_t rendicao;             /* las reglas de la partida dejan rendirse (0x5cf6d4) */
	uint8_t rendeu;               /* ya mandó la rendición */
	/* agua, flota y transporte (ia_agua.c) */
	uint16_t *agua;               /* región de agua de cada casilla (0 = no es agua) */
	int nagua;                    /* regiones de agua 1..nagua (-1 sin memoria) */
	uint32_t *agua_cel;           /* casillas de cada región */
	uint32_t *liga; int nliga, capliga;   /* pares (isla << 16 | agua) que se tocan, de cada mirada */
	uint32_t ilha_cel;            /* casillas de la isla de la casa */
	uint8_t *quer_navios, *quer_portos;   /* IA+0x1a34 y los puertos queridos, por agua */
	int16_t (*alvo_porto)[2];     /* el pesquero que pidió el puerto de cada agua (-1) */
	uint16_t agua_porto;          /* 0x5af414 */
	TzIaLista navios, pesqueiros, portos;
	TzIaTransporte tr[IA_TRANSPORTES]; int32_t ntr;
	/* brechas (ia_brecha.c) */
	TzIaBrecha brecha[IA_BRECHAS]; int32_t nbrechas;
	uint16_t *sem_muros;          /* islas sin los muros enemigos (de la mirada, para las brechas) */
	uint32_t sem_muros_olhar; uint8_t tem_muros;
	TzIaContasD470 contas_d470;
};

/* ------------------------------------------------------------------ utilidades (ia_jogador.c) */
static inline uint32_t ia_ms(const TzIa *ia) { return ia->m->tic * (uint32_t)TZ_TIC_MS; }
static inline TzObj *ia_obj(const TzIa *ia, TzId i) { return i >= 0 && i < ia->m->nobj ? &ia->m->obj[i] : NULL; }
/* contado en el registro de su tipo (rules.rs counted): vivo (dentro de otro también) y sin morir */
static inline int ia_contado(const TzObj *o) { return o && o->vida > 0 && !(o->flags & (TZ_F_MORTO | TZ_F_MORRENDO)); }
static inline int ia_obra(const TzObj *o) { return (o->flags & TZ_F_OBRA) != 0; }
int ia_meu(const TzIa *ia, TzId i);                    /* objeto contado del reino */
int ia_ve(const TzIa *ia, TzId i);                     /* lo propio siempre; lo demás si tz_mundo_ve */
void ia_lista_por(TzIaLista *l, TzId i);
void ia_lista_tira(TzIaLista *l, TzId i);
int ia_lista_tem(const TzIaLista *l, TzId i);
void ia_rect(const TzIa *ia, TzId i, int r[4]);        /* casillas que ocupa */
void ia_pegada(const TzIa *ia, int classe, int *w, int *h);
uint32_t ia_dist(int px, int py, const int r[4]);      /* records.rs gap: casillas vacías de un punto a un rect */
uint32_t ia_dist_rect(const int a[4], const int b[4]); /* build.rs rect_gap */
int ia_militar(const TzIa *ia, const TzObj *o);        /* gameplay.rs military */
int ia_combate(const TzObj *o);                        /* selection.rs combat */
uint32_t ia_forca(const TzObj *o);                     /* 0x401fa0 */
int ia_ilha(const TzIa *ia, int x, int y);
int ia_alcanca(const TzIa *ia, int fx, int fy, const int r[4]);   /* 0x417640: llega por tierra */
int ia_rng(TzIa *ia, int lo, int hi);
uint32_t ia_periodo(const TzIa *ia);                  /* think.rs period */
/* comandos (todos pasan por aquí: cuentan y marcan el objeto) */
int ia_ordem(TzIa *ia, TzId id, int tipo, TzId alvo, int x, int y);
int ia_ordem_grupo(TzIa *ia, const TzId *ids, int n, int tipo, TzId alvo, int x, int y);
int ia_construir(TzIa *ia, TzId peao, int classe, int x, int y);
int ia_produzir(TzIa *ia, TzId edif, int classe);
int ia_pesquisar(TzIa *ia, TzId edif, int tec);
int ia_enviar(TzIa *ia, TzComando *c);              /* cualquier comando del reino (cuenta y marca sus objetos) */
void ia_falar(TzIa *ia, const char *texto);
void ia_reclassificar(TzIa *ia, TzId i, int tipo, TzId alvo, int x, int y);   /* think.rs hooked */
int ia_trab_frente(const TzObj *o, int tipo, int arg);   /* el primer trabajo de su fila */
int ia_edificio_ocioso(const TzIa *ia, TzId i);          /* rules.rs idle_building */

/* ------------------------------------------------------------------ guion (ia_regras.c, rules.rs) */
enum { V_REGRA, V_GATILHO, V_RECURSO, V_TIPO, V_SONDA, V_PCT };
void ia_tocar(TzIa *ia, int que, int n);
void ia_registro(const TzIa *ia, int k, int tipo, uint32_t *feitos, uint32_t *em_curso);
void ia_conta(const TzIa *ia, int tipo, uint32_t *feitos, uint32_t *em_curso);
uint32_t ia_limite(const TzIa *ia);
void ia_custo(const TzIa *ia, int tipo, int32_t c[4]);
void ia_reservar(TzIa *ia, int parte, const int32_t c[4], uint32_t n);
void ia_comprometer(TzIa *ia, const int32_t c[4]);
void ia_devolver(TzIa *ia, const int32_t c[4]);
void ia_gastavel(const TzIa *ia, int32_t out[4]);
int ia_pagavel(const TzIa *ia, const int32_t c[4], int parte, uint32_t n, int primeiro);
void ia_reserva_inicial(TzIa *ia, int liga);
int ia_disponivel_tipo(const TzIa *ia, int tipo);       /* 0x407d50 */
void ia_pagar(TzIa *ia, const int32_t c[4]);
void ia_fase_construir(TzIa *ia);                       /* 0x408960 */
int ia_tec(const TzIa *ia, int tipo);                   /* índice de tecnología del catálogo, o -1 */

/* ------------------------------------------------------------------ árbol y obras (ia_obras.c, tree.rs, build.rs) */
void ia_arvore(TzIa *ia);                               /* produtor[] y presente[] de su civilización */
int ia_livre(const TzIa *ia, TzId i);                   /* build.rs free */
int ia_seguro(const TzIa *ia, int x, int y, int w, int h);   /* 0x411a30 */
int ia_colocar(TzIa *ia, int classe, int cx, int cy, uint32_t min, uint32_t max, TzId prefer);   /* 0x40a990 */
int ia_armazem_de(int recurso);                         /* 0x40d460 */

/* ------------------------------------------------------------------ economía (ia_economia.c, harvest.rs) */
void ia_fase_colher(TzIa *ia);                          /* 0x40dd90 */
void ia_fase_armazens(TzIa *ia);                        /* 0x40dff0 */
void ia_floresta_inicio(TzIa *ia);
void ia_ocioso_campones(TzIa *ia, TzId i, uint32_t ms_ocioso);   /* idle.rs idle_peasant */

/* ------------------------------------------------------------------ guerra (ia_guerra.c: places, records, war, defence) */
void ia_lugares(TzIa *ia);                              /* rehace los lugares que el reino conoce */
void ia_fase_guerra(TzIa *ia);                          /* 0x410be0 */
void ia_fase_especial(TzIa *ia);                        /* 0x409220 */
void ia_ocioso_unidade(TzIa *ia, TzId i, uint32_t ms_ocioso);    /* idle.rs idle_unit */
void ia_milicia(TzIa *ia, TzId ameaca);                 /* 0x4120d0 */
int ia_capturavel(const TzIa *ia, TzId i);              /* gameplay.rs capturable (D470) */
void ia_explorar(TzIa *ia);
uint32_t ia_descoberto(const TzIa *ia, TzId edif);      /* 0x40d4a0 */

/* ------------------------------------------------------------------ agua, flota y transporte (ia_agua.c: reach, navy, transport) */
void ia_agua_olhar(TzIa *ia);
int ia_agua_em(const TzIa *ia, int x, int y);
int ia_agua_de(const TzIa *ia, TzId i);
int ia_ilha_perto(const TzIa *ia, int x, int y, int aneis);
TzId ia_navio_para(TzIa *ia, int a, int b, TzId pass);   /* 0x416ef0 */
int ia_via(TzIa *ia, int a, int b);                      /* 0x417470: IA_VIA_* */
int ia_via_ponto(TzIa *ia, int fx, int fy, const int r[4]);   /* 0x417640 */
int ia_transportar(TzIa *ia, TzId i, int x, int y, int anda); /* 0x415f80: 0 por tierra, 1 en barco, 2 no llega */
int ia_em_transporte(const TzIa *ia, TzId i);
void ia_transportes(TzIa *ia);
int ia_frota(TzIa *ia);                                  /* 0x4168d0 */
int ia_ocioso_pesqueiro(TzIa *ia, TzId b);               /* 0x415730 */
int ia_sitio_porto(TzIa *ia, int cx, int cy, int *ox, int *oy);   /* 0x40a710 */
void ia_agua_liberar(TzIa *ia);
void ia_preparar(TzIa *ia);                              /* pruebas: listas, agua y lugares, sin decidir */

/* ------------------------------------------------------------------ brechas (ia_brecha.c, breach.rs) */
int ia_brecha_alcanca(TzIa *ia, int fx, int fy, const int r[4]);
int ia_brecha_mandar(TzIa *ia, TzId u, int px, int py);
int ia_brecha_de(const TzIa *ia, TzId i);
void ia_brechas_olhar(TzIa *ia);

/* ------------------------------------------------------------------ hechizos y metas (ia_magia.c: spells.rs, goals.rs) */
void ia_magias(TzIa *ia);                               /* 0x414730 antes de cada ataque, 0x472760 */
int ia_ocioso_sacerdote(TzIa *ia, TzId i);              /* 0x414f10 */
int ia_avancar(TzIa *ia, TzId *ids, int n, int px, int py);   /* 0x4ebe10: el ninja se esconde al salir; los que quedan */
int ia_invocar(TzIa *ia, int tipo);                     /* 0x409ab0 */
int ia_cruzada(TzIa *ia);                               /* 0x409ed0 */
int ia_jihad(TzIa *ia);                                 /* 0x409500 */
void ia_converter(TzIa *ia, uint32_t quer);             /* rules.rs convert: la taberna cambia comida */

/* ------------------------------------------------------------------ D470: guardar lo nuevo (ia_agua.c) */
int ia_d470_gravar(const TzIa *ia, FILE *f);            /* 0 bien */
int32_t ia_d470_ler(TzIa *ia, FILE *f, uint32_t resta);  /* bytes leídos, o -1 */

#endif
