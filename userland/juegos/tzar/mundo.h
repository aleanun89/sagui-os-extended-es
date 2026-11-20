/* SPDX-License-Identifier: GPL-3.0-or-later */
/* mundo.h - el modelo del mundo de Tzar y las interfaces entre sus módulos (D417, fase 5).
 *
 * Traducción a C de la simulación de ReTzar (https://github.com/ColCarlo/ReTzar, GPL-3.0-or-later):
 * simulation.rs (atributos, reloj, NativeRng), gameplay.rs (Catalog, Intent, Actor, Job, Runtime),
 * gameplay/movement.rs y pathing.rs (casillas y pasos), gameplay/combat.rs, gameplay/scripting.rs (victoria).
 * La arquitectura y los módulos están en docs/tzar.md, «Arquitectura del juego».
 *
 * Reglas que valen para TODO lo que use este fichero (interfaz, IA, guiones, sonido, red):
 *
 *  1. La simulación es DETERMINISTA: solo enteros (nada de float/double), un único generador (TzRng) y un orden
 *     fijo (los objetos por su índice, los reinos de 1 a 8). Con el mismo mapa, la misma semilla y los mismos
 *     comandos en los mismos tics, tz_mundo_suma() da lo mismo en el PC y en SaguiOS.
 *  2. Nadie fuera de la simulación escribe en TzMundo. La interfaz, la IA, los guiones y la red mandan COMANDOS
 *     (tz_mundo_comando), que se aplican al principio del tic siguiente en el orden en que llegaron. La IA y los
 *     guiones corren DENTRO del tic (por sus ganchos) y también mandan comandos, que se aplican en ese mismo tic,
 *     justo después del gancho. Los guiones de misión tienen además acciones directas (crear, quitar, dar
 *     recursos, diplomacia...), las tz_mundo_guiao_* de abajo, porque el original las hace sin pasar por órdenes.
 *  3. Lo que pasa se cuenta con EVENTOS (TzEvento) en una cola circular: el sonido, la interfaz, los guiones y
 *     la IA los leen con su propio cursor (tz_mundo_eventos). Leer no consume: cada lector lleva su cursor.
 *  4. Los objetos se nombran por su ÍNDICE (TzId) en m->obj. Un índice no se reutiliza en toda la partida: un
 *     objeto muerto queda con TZ_F_MORTO hasta el final (sin ator ni casilla) y el índice sigue siendo válido.
 *     Los punteros (TzObj *) solo valen hasta el siguiente tic (el vector puede crecer).
 *  5. Unidades de medida: casillas de 32x32 píxeles; las posiciones finas en 1/256 de casilla (TZ_SUB); el
 *     tiempo en tics de TZ_TIC_MS ms (gameplay.rs: TICK_MS = 50); 8 direcciones como el original (movement.rs
 *     direction, 0x4fd450): 0 sur, 1 sureste, 2 este, 3 noreste, 4 norte, 5 noroeste, 6 oeste, 7 suroeste.
 *  6. Reino 0 = naturaleza (recursos, objetos, animales sin dueño); 1..8 los reinos del mapa; 10 = discordia
 *     (gameplay.rs DISCORD_OWNER). Recursos en el orden del motor: oro, piedra, comida, madera.
 */
#ifndef TZAR_MUNDO_H
#define TZAR_MUNDO_H

#include "tzar.h"

/* ================================================================== constantes */
#define TZ_TIC_MS        50                 /* gameplay.rs TICK_MS */
#define TZ_TICS_SEG      (1000 / TZ_TIC_MS)
#define TZ_SUB           256                /* subdivisiones de una casilla */
#define TZ_REINOS        8
#define TZ_DONO_DISCORDIA 10
#define TZ_FILA_ORDENS   8                  /* órdenes encoladas (Mayús) por unidad */
#define TZ_FILA_TRABALHO 20                 /* gameplay.rs QUEUE_LIMIT */
#define TZ_CAMINHO_MAX   1024               /* pasos como mucho de un camino */
#define TZ_EVENTOS       4096               /* cola circular de eventos (potencia de 2) */
#define TZ_COMANDOS      1024               /* comandos pendientes para el tic siguiente */
#define TZ_NENHUM        (-1)               /* TzId vacío */
#define TZ_VISOES        32                 /* zonas reveladas a la vez por los guiones */

typedef int32_t TzId;

enum { TZ_OURO = 0, TZ_PEDRA = 1, TZ_COMIDA = 2, TZ_MADEIRA = 3, TZ_NREC = 4 };
enum { TZ_CIV_NENHUMA = 0, TZ_CIV_EURO = 1, TZ_CIV_ARAB = 2, TZ_CIV_ASIA = 3 };   /* byte 13 del registro */

/* Clases de mapa.c (tz_classes[]): las que nombra la simulación. */
enum {
	TZC_PEON = 1, TZC_TRADER = 2, TZC_SPY = 14, TZC_MESSIAH = 38,
	TZC_TOWER_STONE = 39, TZC_TOWER_ROCKET = 43, TZC_CASTLE = 44, TZC_BARRACK = 45, TZC_LUMBER = 46, TZC_FARM = 47,
	TZC_STORAGE = 51, TZC_MARKET = 53, TZC_HARBOR = 54, TZC_SHIPYARD = 57, TZC_WALL_STONE = 61, TZC_WALL_WOOD = 62,
	TZC_FIELD = 63, TZC_BRIDGE = 64, TZC_HOUSE = 66, TZC_GATE_WOOD = 67, TZC_GATE_STONE = 68, TZC_EVILCASTLE = 73,
	TZC_ARROW = 74, TZC_RES_GOLD = 76, TZC_RES_STONE = 78, TZC_ITEM = 79, TZC_DUMMY = 133, TZC_COW = 110,
	TZC_FISH = 111, TZC_FISHPATCH = 115, TZC_FISHING_BOAT = 136,
	TZ_NCLASSES = 144,
};

/* ================================================================== números aleatorios (simulation.rs NativeRng) */
typedef struct { uint32_t s; } TzRng;
uint32_t tz_rng(TzRng *r);                       /* registro de desplazamiento de 32 pasos */
int32_t tz_rng_entre(TzRng *r, int32_t lo, int32_t hi);   /* lo..=hi; lo sin sorteo si hi <= lo */

/* ================================================================== catálogo: lo que dicen los datos (catalogo.c)
 * Se lee una vez por instalación de DATA/{COMM,EURO,ARAB,ASIA} {UNITS,BUILDINGS}.TXT, DATA/OBJECTS/<clase>.INI,
 * ORDERS.INI, MENU.INI, TECHTREE <civ>.INI, TECHNOLOGIES.INI, UPGRADES.INI, BONUS.INI, RES.INI, HEROES.INI y del
 * IMAGES.INI (huella de los edificios). Es de solo lectura durante la partida: la IA y la interfaz lo consultan. */

enum {   /* TzAtributos.tipo */
	TZ_T_NADA = 0, TZ_T_UNIDADE, TZ_T_EDIFICIO, TZ_T_RECURSO, TZ_T_ITEM, TZ_T_DECORADO, TZ_T_PROJETIL, TZ_T_MARCA,
};

typedef struct {
	uint8_t tipo;                 /* TZ_T_* */
	uint8_t categoria_arma;       /* simulation.rs weapon_category: 0 cuerpo a cuerpo, 1 arco, 2 asedio, 3 magia, 5 cohete */
	uint8_t pegada[2];            /* casillas que ocupa (ancho, alto): edificios y minas por su dibujo, el resto 1x1 */
	uint8_t naval, voa;           /* va por el agua / vuela */
	uint16_t vida, ataque, defesa, visao, alcance, resistencia;
	uint16_t velocidade;          /* columna SPEED (casillas por ... ver movimento.c) */
	uint16_t cadencia;            /* columna RATE */
	uint32_t custo[TZ_NREC];
	uint16_t mana, mana_max;
	uint32_t tempo_ms;            /* TIME * 1000 */
	int16_t emite_proprio, emite_outro;   /* EMIT de los edificios (resistencia de los vecinos) */
	uint16_t capacidade;          /* max_obj_in_holder */
	uint16_t pop_conta;           /* pop_count del INI: lo que suma al límite de población (casas, castillo...) */
	/* tiempos de los dibujos (IMAGES.INI anim_time/delay/actionrow/move_len, animation.rs): en ms */
	uint16_t passo_ms;            /* una casilla andando: 32 * ((32000 / move_len) >> 5) */
	uint16_t ataque_ms, ataque_lead_ms;   /* periodo de image_attack y momento del golpe (actionrow) */
	uint16_t construir_ms, minar_ms, cortar_ms, cavar_ms, ordenhar_ms;   /* un ciclo de image_build/mine/chop/dig/milk */
	uint8_t missil, missil_vel;   /* gameplay.rs missile(): tipo (0 flecha, 1 bala, 2 virote) y px/s / 10; 0xff = cuerpo a cuerpo */
	char menu[32];                /* sección de MENU.INI */
	char depende[32];             /* DEPEND ON / DEPENDS ON (edificio que hace falta) */
	char especial[32];            /* SPECIAL: dónde se produce (Castle, Barracks...) */
	uint8_t existe;               /* hay fila en las tablas para esta clase y civilización */
} TzAtributos;

typedef struct {                 /* una investigación o mejora (TECHNOLOGIES.INI, UPGRADES.INI) */
	char nome[40];                /* «te_crossbow»... en minúsculas */
	uint32_t custo[TZ_NREC];
	uint16_t mana;
	uint32_t tempo_ms;
	uint8_t mejora;               /* 1 = de UPGRADES.INI */
} TzTecnologia;

typedef struct {                 /* una orden de ORDERS.INI */
	char chave[40];               /* nombre de la sección */
	char acao[48];                /* ACTION en minúsculas («build barrack», «move», «technology»...) */
	char classe[32];              /* CLASSNAME, o la palabra que sigue a la acción */
	char submenu[32];
	int16_t icone;                /* IMG */
	uint8_t com_alvo;             /* necesita casilla u objeto (Catalog::targeted) */
} TzOrdemDef;

typedef struct {
	const TzBiblioteca *bib;      /* los INI de los objetos e IMAGES.INI (de la escena) */
	TzAtributos at[4][TZ_NCLASSES];     /* [civilización 0..3][clase]; [0] = «comm» */
	TzTecnologia *tec; int ntec;
	TzOrdemDef *ord; int nord;
	TzIni menus, arvores[4], bonus, ordens_ini, recursos_ini;   /* INI tal cual, para lo que haga falta */
	int16_t *bonus_tab;           /* [atacante 0..73][civ][defensor 0..73][civ]: % si bonus_pct, si no puntos */
	uint8_t *bonus_pct;
	uint32_t recursos_ini_nivel[4][TZ_NREC];   /* RES.INI Low/Medium/High (+ el del mapa) */
	uint32_t mina_ouro, mina_pedra, pesqueiro;  /* RES.INI [Others] */
} TzCatalogo;

int tz_catalogo_carregar(TzCatalogo *c, TzRecursos *r, const TzBiblioteca *b);
void tz_catalogo_liberar(TzCatalogo *c);
const TzAtributos *tz_catalogo_at(const TzCatalogo *c, int classe, int civ);
int tz_catalogo_tec(const TzCatalogo *c, const char *nome);          /* índice o -1 */
const TzOrdemDef *tz_catalogo_ordem(const TzCatalogo *c, const char *chave);
int tz_catalogo_classe(const char *nome);                            /* índice en tz_classes o -1 */
/* las órdenes que tiene el menú de una clase (con los submenús), para los botones de la interfaz */
int tz_catalogo_menu(const TzCatalogo *c, int classe, int civ, const TzOrdemDef **out, int max);
/* el golpe con el bono de BONUS.INI del atacante (clase, civ) contra el defensor (gameplay.rs received) */
uint32_t tz_catalogo_bonus(const TzCatalogo *c, int ka, int ca, int kb, int cb, uint32_t golpe);

/* ================================================================== casillas del mundo (pathing.rs: bits de Cells) */
enum {
	TZ_CEL_AGUA      = 1 << 0,    /* WATER: solo barcos */
	TZ_CEL_PENHASCO  = 1 << 1,    /* CLIFF */
	TZ_CEL_ARVORE    = 1 << 2,    /* árbol en pie (se tala) */
	TZ_CEL_EDIFICIO  = 1 << 3,    /* STATIC: edificio, mina u objeto fijo */
	TZ_CEL_MURO      = 1 << 4,    /* WALL */
	TZ_CEL_PORTA     = 1 << 5,    /* GATEWAY: puerta (abre para su reino y aliados) */
	TZ_CEL_PONTE     = 1 << 6,    /* BRIDGE: se anda aunque sea agua */
	TZ_CEL_CAMPO     = 1 << 7,    /* FIELD: se anda por encima */
	TZ_CEL_OCUPADA   = 1 << 8,    /* OCCUPIED: una unidad de tierra está o entra en ella */
	TZ_CEL_OCUPADA_AR = 1 << 9,   /* una unidad que vuela */
	TZ_CEL_COSTA     = 1 << 10,   /* tierra junto al agua (puertos, desembarco) */
};

typedef struct {
	uint16_t bits;                /* TZ_CEL_* */
	TzId ocupante;                /* la unidad de tierra (o el barco) de la casilla, o TZ_NENHUM */
	TzId fixo;                    /* el edificio, mina o muro de la casilla, o TZ_NENHUM */
	uint16_t madeira;             /* madera que le queda al árbol */
	uint16_t ilha;                /* región conexa a pie (islands.rs) para descartar caminos imposibles */
} TzCelula;

/* ================================================================== órdenes (gameplay.rs Intent) */
enum TzOrdemTipo {
	TZ_O_NADA = 0,
	TZ_O_MOVER,          /* casilla */
	TZ_O_SEGUIR,         /* objeto (Reach) */
	TZ_O_EXPLORAR,
	TZ_O_ATACAR,         /* objeto */
	TZ_O_ATACAR_CHAO,    /* casilla (catapultas) */
	TZ_O_RECOLHER,       /* objeto: mina, granja/campo, vaca, pesquero, montón de recursos */
	TZ_O_CORTAR,         /* casilla con árbol */
	TZ_O_CONSTRUIR,      /* objeto: obra (TZ_F_OBRA) ya colocada */
	TZ_O_REPARAR,        /* objeto */
	TZ_O_CAPTURAR,       /* objeto */
	TZ_O_GUARDAR,        /* objeto (proteger) */
	TZ_O_PATRULHAR,      /* casilla a casilla2 */
	TZ_O_ENTRAR,         /* objeto: guarnición, barco, edificio (Board) */
	TZ_O_DESCARREGAR,    /* casilla: barco o edificio suelta lo que lleva */
	TZ_O_ENTREGAR,       /* objeto: llevar la carga a un almacén (lo pone la simulación) */
	TZ_O_COMERCIAR,      /* objeto: mercado o puerto de otro reino */
	TZ_O_PEGAR_ITEM,     /* objeto */
	TZ_O_USAR_ITEM,      /* item, objeto o casilla */
	TZ_O_SOLTAR_ITEM,
	TZ_O_HECHIZO,        /* arg = índice de la orden «spell ...» en cat->ord; objeto o casilla */
	TZ_O_FUGIR,
	TZ_O_PARAR,          /* vaciar órdenes */
	TZ_O_NTIPOS
};

typedef struct {
	uint8_t tipo;                 /* TzOrdemTipo */
	uint8_t modo;                 /* bits TZ_MODO_* */
	int16_t arg;                  /* hechizo, recurso... */
	TzId alvo;                    /* objeto o TZ_NENHUM */
	int16_t x, y;                 /* casilla */
	int16_t x2, y2;               /* segunda casilla (patrulla) */
} TzOrdem;

enum { TZ_MODO_AGRESSIVO = 0, TZ_MODO_DEFENSIVO = 1, TZ_MODO_QUIETO = 2, TZ_MODO_SEM_FOGO = 4, TZ_MODO_FURTIVO = 8 };

/* Estado visible (simulation.rs OrderState): qué dibujo toca (lo traduce la escena a image, image_attack...). */
enum TzEstadoSim {
	TZ_E_PARADO = 0, TZ_E_ANDANDO, TZ_E_TRABALHANDO, TZ_E_ATACANDO, TZ_E_CONSTRUINDO, TZ_E_GUARDANDO,
	TZ_E_LANCANDO, TZ_E_MORRENDO, TZ_E_APARECENDO,
};
/* Para TZ_E_TRABALHANDO, qué trabajo (elige image_chop, image_mine, image_build, image_milk...). */
enum { TZ_TRAB_NADA = 0, TZ_TRAB_CORTAR, TZ_TRAB_MINAR, TZ_TRAB_CAMPO, TZ_TRAB_ORDENHAR, TZ_TRAB_CARNE, TZ_TRAB_PESCAR,
       TZ_TRAB_CONSTRUIR, TZ_TRAB_REPARAR, TZ_TRAB_CAPTURAR };

/* ================================================================== producción (gameplay.rs Job/JobKind) */
enum { TZ_J_TREINAR = 1, TZ_J_PESQUISAR, TZ_J_MELHORAR, TZ_J_ITEM };
typedef struct {
	uint8_t tipo;                 /* TZ_J_* */
	int16_t arg;                  /* clase a entrenar o índice de tecnología */
	uint32_t restante_ms, total_ms;
	uint32_t pago[TZ_NREC];       /* lo cobrado (se devuelve al cancelar) */
	uint8_t esperando;            /* falta población o recursos: no avanza */
} TzTrabalho;

/* ================================================================== el estado de cada objeto que actúa (gameplay.rs Actor)
 * Solo lo tienen las unidades y los edificios (TzObj.ator != NULL); la decoración no gasta memoria. */
typedef struct {
	TzOrdem ordem;                /* la de ahora (tipo TZ_O_NADA = ninguna) */
	TzOrdem fila[TZ_FILA_ORDENS]; int nfila;
	TzOrdem retomar;              /* el trabajo que se deja para entregar la carga o defenderse (tipo NADA = ninguno) */
	uint8_t automatica;           /* la orden de ahora la eligió la unidad (atacar al que ve), no el jugador */
	/* camino: casillas por recorrer desde icaminho; se recalcula si se bloquea (memoria propia, ncap) */
	int16_t (*caminho)[2]; int ncaminho, icaminho, capcaminho;
	int16_t destino[2];           /* casilla a la que lleva el camino (-1 sin camino) */
	uint16_t falhas;              /* búsquedas sin camino seguidas */
	uint16_t espera;              /* tics bloqueado esperando a que se libere la casilla */
	/* paso en curso (movement.rs Walk): de la casilla «de» a la del objeto, progreso en TZ_SUB */
	int16_t de[2];
	int32_t passo, passo_total;   /* en milésimas de tic: progreso = passo * TZ_SUB / passo_total */
	/* trabajo y combate */
	uint32_t recarga;             /* tics hasta el siguiente golpe o disparo */
	uint32_t trabalho_tic;        /* tics hasta el siguiente golpe de trabajo */
	uint16_t ciclos;              /* ciclos de trabajo que faltan (campos: 12-18) */
	TzId alvo_auto;               /* enemigo elegido solo (no ordenado) */
	uint8_t trabalho;             /* TZ_TRAB_* */
	uint8_t carga_tipo;           /* recurso que lleva (TZ_OURO...) */
	uint16_t carga;               /* cantidad que lleva */
	TzId origem;                  /* de dónde vino la carga, para volver */
	/* producción (edificios) */
	TzTrabalho trab[TZ_FILA_TRABALHO]; int ntrab;
	int16_t reuniao[2];           /* punto de reunión (rally), -1 si no hay */
	/* obra (edificios en construcción): progreso en ms de trabajo de peón */
	uint32_t obra_ms, obra_total_ms;
	uint8_t modo;                 /* TZ_MODO_* */
	uint32_t ultimo_golpe;        /* tic del último daño recibido (para avisos y huidas) */
	uint8_t efeito;               /* TZ_EF_*: hechizo que lo afecta (gameplay.rs effect) */
	uint32_t efeito_ate;          /* tic en que se acaba */
} TzAtor;

/* efectos de los hechizos sobre una unidad (gameplay.rs effect: BLESS 5, FREEZE 6, STONE 14...) */
enum { TZ_EF_NADA = 0, TZ_EF_BENDICAO = 5, TZ_EF_GELO = 6, TZ_EF_FURIA = 7, TZ_EF_PEDRA = 14 };

/* ================================================================== objetos */
enum {   /* TzObj.flags: los bits de world.rs (FLAG_*) que se conservan del mapa, más los propios de la simulación */
	TZ_F_MORTO       = 0x00000002,    /* FLAG_DEAD */
	TZ_F_CARREGADO   = 0x00001000,    /* FLAG_CARRIED */
	TZ_F_HISTORIA    = 0x00002000,    /* FLAG_STORY */
	TZ_F_INATIVO     = 0x00004000,    /* FLAG_INACTIVE */
	TZ_F_HEROI       = 0x02000000,    /* FLAG_HERO */
	TZ_F_DEUS        = 0x04000000,    /* FLAG_GOD: no recibe daño */
	TZ_F_BLOQUEADO   = 0x08000000,    /* FLAG_LOCKED: no obedece al jugador */
	TZ_F_MORRENDO    = 0x10000000,    /* FLAG_DYING */
	TZ_F_DENTRO      = 0x20000000,    /* FLAG_HELD: dentro de otro (guarnición, barco) */
	TZ_F_OBRA        = 0x40000000,    /* FLAG_CONSTRUCTION */
};

typedef struct {
	uint16_t id_mapa;             /* id del registro del mapa (los guiones lo usan), o el siguiente libre */
	uint16_t classe;              /* tz_classes */
	uint8_t civ, dono, cor;       /* cor: 0-7, el color del reino (para la escena) */
	uint8_t estado;               /* enum TzEstadoSim (TZ_E_*) */
	uint8_t direcao;              /* 0-7 */
	uint8_t nivel;
	uint32_t flags;               /* TZ_F_* */
	int16_t cx, cy;               /* casilla (la de arriba a la izquierda en los edificios) */
	int32_t px, py;               /* posición fina en 1/TZ_SUB de casilla (centro de la casilla para unidades) */
	int32_t vida, vida_max;
	uint16_t ataque, defesa, visao, alcance, resistencia;
	uint16_t xp, mana, mana_max;
	uint32_t recurso;             /* lo que queda en minas, montones, campos, vacas y pesqueros */
	uint32_t anim_tic;            /* tic en que empezó el estado (para elegir el cuadro) */
	TzId dentro_de;               /* el que lo lleva (TZ_F_DENTRO), o TZ_NENHUM */
	uint16_t ndentro;             /* cuántos lleva */
	const TzObjeto *original;     /* el registro del mapa (nombre de héroe, dibujo propio...), o NULL */
	TzAtor *ator;                 /* NULL en decoración, recursos y objetos muertos */
} TzObj;

/* ================================================================== reinos */
enum {   /* TzReinoEstado.diplomacia[k]: bits de world.rs/scripting.rs */
	TZ_DIP_ALIADO   = 0x40,
	TZ_DIP_VISAO    = 0x01,       /* comparte la vista (con ALIADO = 0x41) */
	TZ_DIP_NEUTRO   = 0x20,
	TZ_DIP_GUERRA   = 0x10,
	TZ_DIP_RECURSOS = 0x02,       /* comparte almacenes */
	TZ_DIP_CONTROLE = 0x04,
	TZ_DIP_VITORIA_PARTILHADA = 0x08,   /* en [k][k]: comparte la victoria con sus aliados */
};

typedef struct {
	uint8_t ativo, ia, civ, cor, equipe;
	uint8_t derrotado, venceu, perdeu, nao_pode_perder, nao_pode_ganhar, rendido;
	char nome[33];
	int32_t recursos[TZ_NREC];
	uint16_t populacao, populacao_max, populacao_limite;   /* actual, la que dan las casas, el tope de la partida */
	uint32_t diplomacia[9];       /* [0..8] hacia cada reino (índice 1..8; [0] sin uso) */
	uint8_t *pesquisas;           /* veces que se completó cada tecnología (ntec del catálogo) */
	uint8_t *proibido;            /* [ntec + TZ_NCLASSES]: prohibido por la misión */
	uint32_t pontos;              /* puntuación cruda (scripting.rs score) */
	uint32_t criadas, perdidas, matadas, recolhido[TZ_NREC];
	/* niebla (simulation.rs Fog): 0 nunca visto, 1 visto, 2 visible ahora; lado*lado */
	uint8_t *nevoa;
} TzReinoEstado;

/* ================================================================== eventos (gameplay.rs GameEvent, SoundEvent, Show) */
enum TzEventoTipo {
	TZ_EV_NADA = 0,
	TZ_EV_CRIADO,        /* obj nació (entrenado, construido, por guion); valor = 1 si era una obra terminada */
	TZ_EV_MORREU,        /* obj murió; alvo = quién lo mató o TZ_NENHUM */
	TZ_EV_REMOVIDO,      /* obj desaparece sin morir (guion, entró en otro) */
	TZ_EV_ATACOU,        /* obj golpeó o disparó a alvo (sonido de ataque) */
	TZ_EV_FERIDO,        /* alvo recibió valor de daño de obj */
	TZ_EV_DISPARO,       /* sale un proyectil de obj hacia alvo; classe = la del proyectil */
	TZ_EV_IMPACTO,       /* llega un proyectil a (x, y) */
	TZ_EV_ORDEM,         /* el reino dio una orden a obj (sonido de «sí, señor»); valor = TzOrdemTipo */
	TZ_EV_SELECIONADO,   /* lo emite la interfaz para el sonido de selección (no la simulación) */
	TZ_EV_OBRA_INICIO,   /* se colocó una obra */
	TZ_EV_OBRA_FIM,      /* terminó una obra o una reparación */
	TZ_EV_TREINADO,      /* obj salió de alvo (edificio) */
	TZ_EV_PESQUISA,      /* reino terminó la tecnología valor */
	TZ_EV_RECURSO,       /* obj entregó valor del recurso classe en alvo */
	TZ_EV_ESGOTADO,      /* obj (mina, árbol en x,y, campo) se acabó */
	TZ_EV_SEM_RECURSOS,  /* reino no pudo pagar (classe = recurso que falta) */
	TZ_EV_SEM_POPULACAO,
	TZ_EV_ATAQUE_AVISO,  /* reino está siendo atacado en (x, y): para el aviso y el sonido */
	TZ_EV_CAPTURADO,     /* obj pasó del reino valor al reino reino */
	TZ_EV_NIVEL,         /* obj subió de nivel */
	TZ_EV_PORTA,         /* obj (puerta) se abre (valor 1) o se cierra (0) */
	TZ_EV_HECHIZO,       /* obj lanzó el hechizo valor */
	TZ_EV_DERROTADO,     /* reino quedó derrotado */
	TZ_EV_FIM,           /* la partida terminó; reino = el jugador local, valor = 1 si ganó */
	TZ_EV_MENSAGEM,      /* texto de un guion: valor = índice en m->textos */
	TZ_EV_SOM,           /* sonido con nombre de un guion: valor = índice en m->textos */
	TZ_EV_DIPLOMACIA,    /* reino cambió su relación con valor */
	TZ_EV_TRABALHO,      /* obj dio un golpe de trabajo (talar, minar, cavar, construir...): valor = TZ_TRAB_*, para el sonido */
	TZ_EV_NTIPOS
};

typedef struct {
	uint32_t seq;                 /* número de evento (crece siempre) */
	uint32_t tic;
	uint8_t tipo, reino;
	uint16_t classe;
	TzId obj, alvo;
	int16_t x, y;                 /* casilla */
	int32_t valor;
} TzEvento;

/* ================================================================== comandos: la única forma de cambiar el mundo desde fuera */
enum TzComandoTipo {
	TZ_C_ORDEM = 1,      /* ordem a los objetos ids[] (con TZ_CMD_FILA, se encola) */
	TZ_C_CONSTRUIR,      /* colocar una obra de la clase arg en (x, y) y mandar a ids[] a construirla */
	TZ_C_PRODUZIR,       /* edificio ids[0]: entrenar la clase arg */
	TZ_C_PESQUISAR,      /* edificio ids[0]: investigar la tecnología arg */
	TZ_C_CANCELAR,       /* edificio ids[0]: quitar el trabajo arg de su fila (-1 = el último) */
	TZ_C_REUNIAO,        /* edificio ids[]: punto de reunión (x, y) */
	TZ_C_MODO,           /* unidades ids[]: TZ_MODO_* = arg */
	TZ_C_DESTRUIR,       /* objetos ids[] propios: derribar */
	TZ_C_DIPLOMACIA,     /* relación del reino con arg = valor (bits TZ_DIP_*) */
	TZ_C_RENDIR,         /* el reino se rinde */
	TZ_C_TRUCO,          /* cheats.rs: arg = código, valor */
};
#define TZ_CMD_FILA   1          /* encolar en vez de sustituir */
#define TZ_CMD_IDS    64         /* objetos por comando (más: varios comandos) */

typedef struct {
	uint8_t tipo, reino, flags;
	int16_t arg;
	int32_t valor;
	int16_t x, y;
	TzOrdem ordem;
	int nids;
	TzId ids[TZ_CMD_IDS];
} TzComando;

/* ================================================================== ganchos: IA, guiones y quien quiera correr dentro del tic */
struct TzMundo;
typedef struct {
	void *dados;
	/* se llama una vez por tic tras los comandos de fuera y antes de mover; los comandos que mande se aplican ya */
	void (*tic)(struct TzMundo *m, void *dados);
	/* guardar/cargar su estado propio en la partida (bloque con su etiqueta); pueden ser NULL */
	int (*gravar)(struct TzMundo *m, void *dados, FILE *f);
	int (*carregar)(struct TzMundo *m, void *dados, FILE *f, uint32_t tam);
	char etiqueta[8];             /* «GUIA», «IA1»... identifica el bloque en la partida guardada */
	/* cada cuántos tics corre (1 = cada tic; la IA del original piensa cada pocos) y con qué desfase */
	uint16_t cada, desfase;
} TzGancho;
#define TZ_GANCHOS 12

/* ================================================================== reglas de la partida (world.rs GameRules) */
typedef struct {
	uint8_t sem_nevoa, explorado;
	uint16_t populacao;           /* tope por reino (200) */
	uint8_t dificuldade;
	uint8_t vitoria;              /* 0 normal (sin unidades ni edificios mayores), 1 sin castillo, 2 tiempo, 3 puntos */
	uint32_t vitoria_limite;      /* minutos (2) o puntos (3) */
	uint8_t nivel_recursos;       /* 0 mapa, 1 bajo, 2 medio, 3 alto */
	uint8_t campanha;             /* las reglas las ponen los guiones */
	uint8_t troncos;              /* árboles con dibujo hasta este número son tocones (TERRAIN.INI treename; 0 = 3) */
	uint8_t sem_vitoria_auto;     /* un guion decide el final: no comprobar la victoria normal */
} TzRegras;

/* ================================================================== el mundo */
typedef struct TzMundo {
	/* lo fijo de la partida */
	const TzCatalogo *cat;
	const TzMapa *mapa;           /* el mapa leído (registros originales, reinos, bioma) */
	int lado;
	uint16_t *casillas;           /* copia de m->mapa->casillas que cambia (árboles talados) */
	TzRegras regras;
	int jogador;                  /* reino local (la vista, la niebla y el final se miden desde él) */
	/* reloj */
	uint32_t tic;
	TzRng rng;                    /* el de la partida (sim); la interfaz y el sonido usan el suyo propio */
	uint32_t semente;
	/* objetos */
	TzObj *obj; int nobj, capobj;
	TzCelula *cel;                /* lado*lado */
	/* reinos: [0] naturaleza, [1..8] */
	TzReinoEstado reino[TZ_REINOS + 1];
	/* proyectiles en vuelo (gameplay.rs Projectile) */
	struct TzProjetil { TzId atacante, alvo; uint16_t classe, dano, area; uint8_t categoria;
	                    int32_t x0, y0, x1, y1; uint32_t sai, chega; } *proj; int nproj, capproj;
	/* comandos pendientes para el tic siguiente */
	TzComando *cmd; int ncmd;
	/* eventos */
	TzEvento ev[TZ_EVENTOS]; uint32_t ev_seq;
	char textos[64][160]; uint32_t ntextos;   /* textos de los eventos (circular de 64) */
	/* ganchos (IA de cada reino, guion de la misión...) */
	TzGancho gancho[TZ_GANCHOS]; int nganchos;
	/* final */
	uint8_t acabou, ganhou;
	uint8_t dentro_tic;           /* los comandos se aplican ya (ganchos) */
	struct TzVisao { uint8_t reino; int16_t x, y, raio; uint32_t ate; } visao[TZ_VISOES];   /* revelados temporales */
	/* rendimiento y depuración */
	uint32_t ms_ultimo_tic;
	void *priv;                   /* lo interno de la simulación (búsqueda de caminos...) */
} TzMundo;

/* ---------------------------------------------------------------- vida de la partida (mundo.c) */
/* Crea el mundo del mapa: objetos con sus atributos, casillas, reinos (recursos según regras), niebla. */
int tz_mundo_criar(TzMundo *m, const TzCatalogo *c, const TzMapa *mapa, const TzRegras *regras, uint32_t semente);
void tz_mundo_liberar(TzMundo *m);
/* Avanza un tic: comandos -> ganchos -> órdenes -> movimiento -> combate y proyectiles -> producción y obras ->
 * muertes -> niebla -> victoria. */
void tz_mundo_tic(TzMundo *m);
/* Encola un comando para el tic siguiente; -1 si la cola está llena o el comando no es de ese reino. */
int tz_mundo_comando(TzMundo *m, const TzComando *c);
/* Suma del estado (FNV-1a de todo lo que influye en el futuro): la misma en todas las máquinas. */
uint32_t tz_mundo_suma(const TzMundo *m);
/* Partidas guardadas: formato propio «TZSV» (little-endian, por bloques con etiqueta), con los ganchos. */
int tz_mundo_gravar(TzMundo *m, const char *ruta);
int tz_mundo_carregar(TzMundo *m, const TzCatalogo *c, const TzMapa *mapa, const char *ruta);
int tz_mundo_gancho(TzMundo *m, const TzGancho *g);   /* registra un gancho; -1 si no caben */

/* ---------------------------------------------------------------- consultas (para la interfaz, la IA y los guiones) */
static inline TzObj *tz_mundo_obj(TzMundo *m, TzId i) { return i >= 0 && i < m->nobj ? &m->obj[i] : NULL; }
int tz_obj_vivo(const TzObj *o);                       /* existe, no muerto ni muriendo, no dentro de otro */
int tz_obj_unidade(const TzObj *o);
int tz_obj_edificio(const TzObj *o);
const TzAtributos *tz_obj_at(const TzMundo *m, const TzObj *o);   /* atributos de su clase y civilización */
TzId tz_mundo_em(const TzMundo *m, int cx, int cy);    /* lo que hay en la casilla (unidad, si no el fijo) */
TzId tz_mundo_por_id_mapa(const TzMundo *m, int id_mapa);
/* objetos vivos de la zona (casillas, incluidas) que cumplan el filtro; dono < 0 = de cualquier reino (0 = naturaleza);
 * devuelve cuántos escribió */
enum { TZ_FILTRO_TODOS = 0, TZ_FILTRO_UNIDADES = 1, TZ_FILTRO_EDIFICIOS = 2, TZ_FILTRO_RECURSOS = 4 };
int tz_mundo_area(const TzMundo *m, int x0, int y0, int x1, int y1, int filtro, int dono, TzId *out, int max);
/* relación de a con b: 1 aliado (o el mismo), 0 neutral, -1 enemigo */
int tz_mundo_relacao(const TzMundo *m, int a, int b);
/* niebla: 0 nunca visto, 1 visto, 2 visible ahora (para el reino k) */
int tz_mundo_nevoa(const TzMundo *m, int k, int cx, int cy);
/* lo que el reino k sabe de un objeto: vivo y visible (o edificio ya visto); la IA solo debe usar esto */
int tz_mundo_ve(const TzMundo *m, int k, TzId i);
/* ¿puede el reino k dar esta orden (clave de ORDERS.INI) con ese objeto? 0 sí; si no, -1 y el motivo en tz_erro() */
int tz_mundo_pode(const TzMundo *m, int k, TzId obj, const char *chave_ordem);
/* coste de entrenar/construir una clase o de una tecnología para el reino (ya con sus investigaciones) */
void tz_mundo_custo_classe(const TzMundo *m, int k, int classe, uint32_t custo[TZ_NREC], uint32_t *tempo_ms);
void tz_mundo_custo_tec(const TzMundo *m, int k, int tec, uint32_t custo[TZ_NREC], uint32_t *tempo_ms);
/* ¿se puede poner una obra de la clase en (cx, cy)? 0 sí */
int tz_mundo_cabe(const TzMundo *m, int k, int classe, int cx, int cy);
/* la orden que daría el clic derecho de la interfaz con esos objetos sobre la casilla u objeto (Default) */
TzOrdem tz_mundo_ordem_padrao(const TzMundo *m, int k, const TzId *ids, int n, int cx, int cy, TzId alvo);
/* posición de dibujo (píxeles del mundo) de un objeto, con el paso interpolado */
void tz_obj_pixel(const TzObj *o, int *x, int *y);

/* ---------------------------------------------------------------- eventos */
/* Copia a out los eventos posteriores a *cursor (máx. max) y avanza el cursor. Si el lector se quedó atrás más de
 * TZ_EVENTOS, salta lo perdido (devuelve lo que queda). Cursor inicial: 0. */
int tz_mundo_eventos(const TzMundo *m, uint32_t *cursor, TzEvento *out, int max);
void tz_mundo_evento(TzMundo *m, const TzEvento *e);   /* para la interfaz (TZ_EV_SELECIONADO) y los guiones */
const char *tz_mundo_texto(const TzMundo *m, int32_t idx);

/* ---------------------------------------------------------------- acciones directas de los guiones de misión (scripting.rs)
 * Solo para el gancho del guion (corren dentro del tic, deterministas). */
TzId tz_mundo_guiao_criar(TzMundo *m, int classe, int civ, int dono, int cx, int cy);
void tz_mundo_guiao_remover(TzMundo *m, TzId i);
void tz_mundo_guiao_matar(TzMundo *m, TzId i);
void tz_mundo_guiao_dono(TzMundo *m, TzId i, int dono);
void tz_mundo_guiao_recursos(TzMundo *m, int k, int recurso, int32_t delta);
void tz_mundo_guiao_diplomacia(TzMundo *m, int a, int b, uint32_t bits);
void tz_mundo_guiao_revelar(TzMundo *m, int k, int cx, int cy, int raio);
void tz_mundo_guiao_pesquisa(TzMundo *m, int k, int tec);
void tz_mundo_guiao_proibir(TzMundo *m, int k, const char *nome, int proibido);
void tz_mundo_guiao_ordem(TzMundo *m, TzId i, const TzOrdem *o);   /* sin comprobar el dueño */
void tz_mundo_guiao_mensagem(TzMundo *m, int k, const char *texto);
void tz_mundo_guiao_som(TzMundo *m, const char *nome, int cx, int cy);
void tz_mundo_guiao_fim(TzMundo *m, int k, int ganhou);           /* venceu/perdeu y TZ_EV_FIM si es el jugador */
/* poner en juego un objeto oculto (TZ_F_INATIVO) en su casilla o en la libre más cercana; 0 o -1 si no cabe */
int tz_mundo_guiao_aparecer(TzMundo *m, TzId i);
/* fijar vida, nivel y maná (-1 = no tocar) y poner/quitar banderas (TZ_F_BLOQUEADO, TZ_F_DEUS, TZ_F_HEROI...) */
void tz_mundo_guiao_atributos(TzMundo *m, TzId i, int32_t vida, int nivel, int mana, uint32_t poner, uint32_t quitar);
void tz_mundo_guiao_terminar_obra(TzMundo *m, TzId i);             /* @build: la obra queda hecha */
/* revelar una zona solo durante unos tics (vuelve a la niebla después); hasta TZ_VISOES a la vez */
void tz_mundo_guiao_revelar_temp(TzMundo *m, int k, int cx, int cy, int raio, uint32_t tics);
/* los recursos de un reino sumando los de los reinos que comparten con él (relación 0x02 en los dos sentidos) */
void tz_mundo_recursos_compartidos(const TzMundo *m, int k, int32_t out[TZ_NREC]);
/* partida libre (world/starting.rs reset_initial_forces): quita las unidades y edificios de los reinos y pone a cada
 * reino activo un castillo y tres peones en su casilla de inicio (o donde estaban sus fuerzas). Antes del primer tic. */
int tz_mundo_inicio_livre(TzMundo *m);

/* ---------------------------------------------------------------- puente con el dibujo de la fase 2 (escena.c)
 * Rellena un TzObjeto (como los del mapa) con el estado actual del objeto: casilla, dueño, variante con la dirección
 * (0x8000 | dir) y la vida; para que tz_escena_montar dibuje el mundo vivo sin saber de la simulación. */
void tz_mundo_objeto_mapa(const TzMundo *m, TzId i, TzObjeto *out);
/* La pose para tz_dibujar_objeto (animacao.c, fase 3) y el píxel de la esquina de su casilla con el avance del paso:
 * TzObjeto (tz_mundo_objeto_mapa) + TzEstado + (px, py) es todo lo que necesita el dibujo de un objeto vivo. */
void tz_mundo_pose(const TzMundo *m, TzId i, TzEstado *pose, int *px, int *py);

#endif
