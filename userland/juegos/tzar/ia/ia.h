/* SPDX-License-Identifier: GPL-3.0-or-later */
/* ia.h - la IA original de Tzar: guiones de IA (*.ais, *.ai), su intérprete y los gestores de cada reino (D417, fase 6).
 *
 * Traducción a C de la IA de ReTzar (https://github.com/ColCarlo/ReTzar, GPL-3.0-or-later), src/gameplay/ai.rs y
 * src/gameplay/ai/: script.rs (el lenguaje y su compilador), rules.rs (condiciones, reglas, fase de construir y metas),
 * think.rs (el jugador: periodo, casa, listas y fases), harvest.rs (economía), build.rs y tree.rs (obras y árbol de
 * tecnologías), places.rs, records.rs, war.rs y defence.rs (lugares, ejército, ataque y defensa) e idle.rs (unidades
 * ociosas); y desde la D470 reach.rs, navy.rs y transport.rs (ia_agua.c), spells.rs y goals.rs (ia_magia.c) y breach.rs
 * (ia_brecha.c). Cada fichero dice de qué sale cada función. Diferencias con el original: docs/tzar.md, «La IA (fase 6)».
 *
 * La IA es un JUGADOR más (mundo.h, regla 2): un gancho por reino que corre dentro del tic; solo ve lo que le dejan
 * tz_mundo_ve y tz_mundo_nevoa (sus propios objetos los ve siempre) y solo actúa con comandos (tz_mundo_comando).
 * Es determinista: solo enteros, su propio TzRng (sembrado con la semilla de la partida y el reino) y orden fijo.
 */
#ifndef TZAR_IA_H
#define TZAR_IA_H

#include "../mundo.h"

/* ================================================================== el lenguaje de los guiones (ia_guiao.c, script.rs) */
#define TZ_IA_PCT 0x8000u             /* un número con % es esa parte del tope de población */

enum {   /* TzIaRegra.flags (rule +0) */
	TZ_IA_R_CRITICA   = 0x01,
	TZ_IA_R_SUJA      = 0x02,         /* hay que evaluarla otra vez */
	TZ_IA_R_VERDADE   = 0x04,         /* sus condiciones se cumplen */
	TZ_IA_R_EM_CURSO  = 0x08,         /* su meta va en camino (obras, colas) */
	TZ_IA_R_SATISFEITA = 0x10,
	TZ_IA_R_ESQUECIDA = 0x20,
	TZ_IA_R_GRUPO_INI = 0x40,
	TZ_IA_R_GRUPO_FIM = 0x80,
};

enum {   /* metas de HAVE que no son tipos de objeto (0x581700: 0xfffe - índice) */
	TZ_IA_M_CONVERTER = 0xfffe, TZ_IA_M_OURO = 0xfffd, TZ_IA_M_PEDRA = 0xfffc, TZ_IA_M_COMIDA = 0xfffb,
	TZ_IA_M_MADEIRA = 0xfffa, TZ_IA_M_BLOQUEIO = 0xfff9, TZ_IA_M_RENDER = 0xfff8,
};

enum {   /* TzIaCond.tipo (condición de 10 bytes del original) */
	TZ_IA_C_REGRA = 1, TZ_IA_C_GATILHO = 2, TZ_IA_C_RECURSO = 3, TZ_IA_C_CONTA = 4, TZ_IA_C_TEMPO = 5,
	TZ_IA_C_ACASO = 6, TZ_IA_C_EXTERNO = 7, TZ_IA_C_NAO = 8, TZ_IA_C_CIV = 10, TZ_IA_C_DIFICULDADE = 11,
	TZ_IA_C_INIMIGO = 12, TZ_IA_C_ALIADO = 13,
};

typedef struct {
	uint8_t tipo;                 /* TZ_IA_C_* */
	uint8_t recurso;              /* RECURSO: 0 oro, 1 piedra, 2 comida, 3 madera; CIV 1-3; DIFICULDADE 0-2 */
	uint16_t classe;              /* CONTA, INIMIGO, ALIADO: tipo de objeto (0 = UNITS) */
	uint32_t n;                   /* cantidad, tiempo en ms, probabilidad, regla, gatillo, texto o condición negada */
} TzIaCond;

typedef struct { uint32_t ini, n; } TzIaTrecho;   /* un tramo de la lista compartida */

typedef struct {
	char *nome;                   /* en mayúsculas; NULL si no tiene (y en las marcas de grupo) */
	uint32_t flags;               /* TZ_IA_R_* */
	TzIaTrecho quando;            /* WHEN: números de condición */
	TzIaTrecho gatilhos;          /* TRIGGER: números de gatillo (bit 31 = apagarlo) */
	TzIaTrecho esquece;           /* FORGET: números de regla */
	uint8_t tem_meta;             /* HAVE */
	uint16_t meta_tipo, meta_n;   /* tipo (o TZ_IA_M_*) y cuántos (con TZ_IA_PCT si llevaba %) */
	char *diz;                    /* SAY, o NULL */
	uint32_t fim;                 /* en el inicio de un grupo: el índice de su final */
} TzIaRegra;

typedef struct { char *nome; uint8_t ligado; } TzIaGatilho;

typedef struct {
	TzIaRegra *regra; int nregras, capregras;
	TzIaCond *cond; int nconds, capconds;
	uint32_t *lista; int nlista, caplista;
	TzIaGatilho *gat; int ngat, capgat;
	char **texto; int ntextos, captextos;    /* las expresiones de EXTERN */
} TzIaPrograma;

typedef struct {
	char ficheiro[128];
	int linha, coluna;            /* desde 1; 0 si es un nombre que faltaba al final */
	const char *mensagem;         /* el texto del original («Unrecognized token»...) */
	char texto[160];              /* la línea del guion o el nombre que falta */
	char incluido[256];           /* «(incluido desde X:L) ...» */
} TzIaErro;

/* Los ficheros de DATA/AI y los nombres de los tipos de objeto (0x5a972c): las clases, las secciones de UPGRADES.INI
 * y las de TECHNOLOGIES.INI en ese orden. */
typedef struct {
	char **nome; uint8_t **dados; uint32_t *tam; int nfich, capfich;   /* nombre normalizado (tz_chave) */
	char **tipo; int ntipos, captipos;
	int tecnologias;              /* índice del primer tipo que es una investigación */
} TzIaBiblioteca;

int tz_ia_bib_carregar(TzIaBiblioteca *b, TzRecursos *r);
/* para las pruebas: biblioteca con las clases, los tipos dados (las investigaciones son las que empiezan por «te_») */
void tz_ia_bib_iniciar(TzIaBiblioteca *b, const char *const *extra, int nextra);
void tz_ia_bib_ficheiro(TzIaBiblioteca *b, const char *nome, const uint8_t *dados, uint32_t n);   /* copia */
void tz_ia_bib_liberar(TzIaBiblioteca *b);
int tz_ia_bib_tipo(const TzIaBiblioteca *b, const char *nome);       /* sin distinguir mayúsculas; -1 */
/* 0x401a10: 1 unidad, 2 edificio, 3 investigación; 0 las mejoras y lo demás */
int tz_ia_bib_categoria(const TzIaBiblioteca *b, int tipo);

/* 0x418b80: compila un guion (sin carpeta: de data\ai\). 0 bien; -1 y el error en *e. Un fichero vacío es un guion
 * sin reglas. */
int tz_ia_compilar(const TzIaBiblioteca *b, const char *nome, TzIaPrograma *p, TzIaErro *e);
void tz_ia_programa_liberar(TzIaPrograma *p);
void tz_ia_erro_texto(const TzIaErro *e, char *out, int n);

/* ================================================================== el jugador (ia_jogador.c y los gestores) */
typedef struct TzIa TzIa;

enum { TZ_IA_ECONOMIA = 1, TZ_IA_GUERRA = 2, TZ_IA_COMPLETA = 3, TZ_IA_FALA = 0x4000 };

/* Crea el jugador del reino k con su guion (NULL o "" = random.ais) y lo registra como gancho del mundo. modo:
 * TZ_IA_ECONOMIA, TZ_IA_GUERRA o TZ_IA_COMPLETA (los bits del registro del reino, K+0xa). ordem/total escalonan la
 * primera mirada de cada jugador (0x402760). Si el guion no compila devuelve NULL y el motivo en *e. */
TzIa *tz_ia_criar(TzMundo *m, const TzIaBiblioteca *b, int k, int modo, const char *guiao, int ordem, int total,
                  TzIaErro *e);
/* Uno por cada reino con TzReinoEstado.ia (orden de reino); devuelve cuántos y deja en erro el primero que falla
 * (0x402760: los siguientes al que falla tampoco tienen IA). */
int tz_ia_criar_todas(TzMundo *m, const TzIaBiblioteca *b, TzIa **out, int max, TzIaErro *e);
void tz_ia_liberar(TzIa *ia);
/* La orden «AI» del chat y de los SAY «HMAI ...» (0x404460): talk on/off, none, eco, war, full. 0 si la entendió. */
int tz_ia_ordem(TzIa *ia, const char *ordem);
/* @setnoattacktime (0x4b81d0): no ataca la casa de otro reino hasta ms. */
void tz_ia_sem_ataque(TzIa *ia, uint32_t ms);
/* Las frases SAY (con «talk on»): devuelve la siguiente posterior a *cursor, o NULL. */
const char *tz_ia_fala(const TzIa *ia, uint32_t *cursor);
/* Para las pruebas y la depuración: el programa del jugador, su suma (FNV-1a de su estado) y lo que hizo. */
const TzIaPrograma *tz_ia_programa(const TzIa *ia);
uint32_t tz_ia_suma(const TzIa *ia);
typedef struct { uint32_t comandos, construidos, treinados, pesquisas, grupos, olhares; } TzIaContas;
TzIaContas tz_ia_contas(const TzIa *ia);
/* D470: lo que hizo con el agua, la magia, los muros y las metas propias */
typedef struct { uint32_t transportes, desembarques, navios, hechizos, invocacoes, curas, jihad, cruzadas, trocas, brechas, capturas; } TzIaContasD470;
TzIaContasD470 tz_ia_contas_d470(const TzIa *ia);

/* EXTERN: el evaluador de expresiones de los guiones de misión (fase 7) puede sustituir al pequeño de la IA. */
typedef int32_t (*TzIaExterno)(TzMundo *m, int k, const char *expr);
extern TzIaExterno tz_ia_externo;
extern int tz_ia_depurar;            /* 1: cada mirada escribe lo que piensa (pruebas en el PC) */
int32_t tz_ia_expr(TzMundo *m, int k, const char *expr);   /* el de la IA: números, @gold..@wood($aik), $aik, + - * / ! < > == && || */

#endif
