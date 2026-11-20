/* SPDX-License-Identifier: GPL-3.0-or-later */
/* guiao.h - guiones de misión de Tzar (D417, fase 7): sucesos, acciones, reglas, conversaciones y expresiones.
 *
 * Traducción a C de ReTzar (https://github.com/ColCarlo/ReTzar, GPL-3.0-or-later): script.rs, script/expr.rs,
 * dialogue.rs y gameplay/scripting.rs. Se programa SOLO contra mundo.h: el guion es un TzGancho que corre dentro del
 * tic y actúa con las acciones tz_mundo_guiao_*. Lo que el mundo no ofrece (poner un objeto en juego, fijar la vida,
 * el nivel, el bloqueo...) lo escribe el guion en TzObj a mano, porque corre dentro del tic y es determinista; está
 * todo en las funciones «gm_*» de guiao.c, para sustituirlas por llamadas del mundo si la simulación las ofrece.
 *
 * La interfaz habla con el guion por una cola de PEDIDOS (tz_guiao_pedido: historia, aviso, conversación, cámara...)
 * y le contesta con tz_guiao_fechar y tz_guiao_responder. Una historia, un aviso o una conversación abiertos
 * (tz_guiao_modal) paran las reglas que esperan una ventana, como en el original.
 */
#ifndef TZAR_GUIAO_H
#define TZAR_GUIAO_H

#include "mundo.h"

/* ------------------------------------------------------------------ el guion leído */
typedef struct {                 /* script.rs Selector */
	uint8_t por_classe, no_maximo, todos;
	uint16_t classe, reinos;     /* reinos: el bit p-1 elige el reino p */
	int nids; uint16_t *ids;
	char *conta;                 /* N, una expresión */
} TzSel;

typedef struct {                 /* script.rs Area */
	uint8_t em_volta;
	int16_t rect[4];             /* x1, y1, x2, y2 (casillas, incluidas) */
	uint16_t objeto;
	char *raio;                  /* expresión */
} TzArea;

typedef struct { uint8_t por_classe; uint16_t classe, objeto, reinos; } TzSuporte;   /* script.rs Holder */

enum {   /* tipos de suceso (script.rs Event) */
	TZ_EVT_EXISTE = 1000, TZ_EVT_DESTRUIDO, TZ_EVT_DENTRO, TZ_EVT_SAIU, TZ_EVT_NA_AREA, TZ_EVT_DERROTADO, TZ_EVT_VENCEU,
	TZ_EVT_EXPRESSAO,
};
enum {   /* tipos de acción (script.rs Action) */
	TZ_ACC_MISSAO = 0, TZ_ACC_TIRAR_MISSAO, TZ_ACC_DIPLOMACIA, TZ_ACC_DONO, TZ_ACC_PROPRIEDADE, TZ_ACC_EXPRESSAO,
	TZ_ACC_TEMPORIZADOR, TZ_ACC_PARAR_TEMP, TZ_ACC_PERDER, TZ_ACC_GANHAR, TZ_ACC_MANTER, TZ_ACC_MAPA, TZ_ACC_TRAVAR_VISTA,
	TZ_ACC_LINHA, TZ_ACC_HISTORIA, TZ_ACC_AVISO, TZ_ACC_CRIAR, TZ_ACC_DESTRAVAR_VISTA, TZ_ACC_ORDEM, TZ_ACC_ROLAR_VISTA,
	TZ_ACC_CONVERSA, TZ_ACC_DESTRUIR, TZ_ACC_EXPLORAR, TZ_ACC_APARECER, TZ_ACC_ESTADO, TZ_ACC_BLOQUEAR, TZ_ACC_APAGAR,
	TZ_ACC_ESPERAR, TZ_ACC_N
};

typedef struct {
	char nome[40];
	int tipo;                    /* TZ_EVT_* */
	TzSel sel;
	TzSuporte sup;               /* TZ_EVT_DENTRO */
	TzArea area;                 /* TZ_EVT_NA_AREA */
	uint16_t mascara;            /* TZ_EVT_DERROTADO, TZ_EVT_VENCEU */
	char *expr;                  /* TZ_EVT_EXPRESSAO */
} TzSuceso;

typedef struct {
	char nome[40];
	int tipo;                    /* TZ_ACC_*; ver guiao.c para qué campos usa cada una */
	char *t[5];
	uint8_t b[3];
	uint16_t u;
	uint32_t flags;
	TzSel sel;
	TzArea a, a2;
} TzAccion;

typedef struct { char nome[40]; char *condicao; int nacoes; int *acoes; } TzRegra;   /* acciones ya resueltas (índice o -1) */

typedef struct {                 /* dialogue.rs Phrase */
	uint16_t indice, fala_de;    /* fala_de: id del objeto del mapa que habla, 0 = narración */
	uint8_t inicio;
	char *condicao, *acao, *voz, *texto;
	int nseg; uint16_t *seg;
} TzFrase;
typedef struct { char nome[40]; int nfrases; TzFrase *frases; } TzConversa;

typedef struct {                 /* un registro de .objext: los héroes y personajes con nombre */
	uint16_t id;
	char nome[33], subtitulo[33], retrato[65], retrato_grande[65];
} TzPersonagem;

/* ------------------------------------------------------------------ expresiones (script/expr.rs) */
typedef struct { char nome[64]; int32_t valor; } TzVar;
typedef struct { TzVar mapa[256]; int nmapa; TzVar mantidas[256]; int nmantidas; } TzVariaveis;   /* «$x» y «#x» */

/* ------------------------------------------------------------------ lo que se le pide a la interfaz */
enum TzPedidoTipo {
	TZ_PD_AVISO_JANELA = 1,      /* ventana de mensaje (tz_guiao_mensagem) */
	TZ_PD_HISTORIA,              /* pantalla de historia: som, musica, ms, fecha */
	TZ_PD_FALA,                  /* línea o elección de una conversación (tz_guiao_fala) */
	TZ_PD_FECHAR,                /* se acabó la conversación: cerrar los globos */
	TZ_PD_LINHA,                 /* una línea de texto sobre el mapa */
	TZ_PD_NOTA,                  /* una frase de [viewmsg] o [msg] por su clave (texto = nombre, chave) */
	TZ_PD_TRAVAR,                /* vista fija en (cx, cy) o siguiendo a obj, ms (0 = hasta que se suelte) */
	TZ_PD_ROLAR,                 /* la vista pasa de (cx, cy) a (cx2, cy2) */
	TZ_PD_DESTRAVAR,
	TZ_PD_SELECIONAR, TZ_PD_DESSELECIONAR, TZ_PD_LIMPAR_SELECAO,
	TZ_PD_FILME,                 /* @playmovie(texto) */
	TZ_PD_DIPLOMACIA,            /* cambiaron las relaciones */
	TZ_PD_IA,                    /* @setai: reino = cx, modo = cy (la IA lo lee) */
};
typedef struct {
	int tipo;
	int cx, cy, cx2, cy2;
	TzId obj;
	uint32_t ms;
	int fecha;
	char texto[160], chave[24];
	char som[64], musica[64];
} TzPedido;

typedef struct {                 /* la ventana de aviso, historia o frase que se muestra */
	char titulo[64], texto[700], retrato[80];
	int nescolhas; char escolhas[8][160];
} TzMensagem;

typedef struct { char texto[300]; char retrato[80]; int32_t id; } TzObjetivo;   /* una misión del jugador */

/* Servicios de quien lleva la IA y lo que el mundo no ofrece; los pone el integrador (pueden ser NULL). */
typedef struct {
	void *dados;
	void (*ia_modo)(TzMundo *m, void *dados, int reino, int modo);               /* @setai: 0 nada, 1 eco, 2 guerra, 3 todo */
	void (*ia_sem_ataque)(TzMundo *m, void *dados, uint32_t mascara, uint32_t tempo);   /* @setnoattacktime */
	void (*pode_resgatar)(TzMundo *m, void *dados, int reino, int sim);          /* @kingdomcanrescue */
	int (*selecionado)(TzMundo *m, void *dados, TzId obj);                       /* $isselected */
} TzGuiaoServicos;

typedef struct { uint64_t due, seq; int regra; int cursor; uint8_t manter; } TzRegraRun;
typedef struct { char nome[64]; uint64_t due, seq; uint8_t contra; } TzTempRun;

typedef struct TzGuiao {
	/* lo leído */
	TzSuceso *eventos; int neventos;
	TzAccion *acoes; int nacoes;
	TzRegra *regras; int nregras;
	char *derrota[8];            /* texto al caer cada reino (1..8) o NULL */
	TzConversa *conversas; int nconversas;
	TzPersonagem *pers; int npers;
	uint32_t diplomacia[64];     /* .diplomacy: [(a-1)*8 + (b-1)] */
	int tem_diplomacia;
	int jogador;                 /* el reino humano según .kingdominfo: el primero activo que no juega la máquina (1..8) */
	int erros;                   /* registros que no se pudieron leer o acciones que el guion nombra y no existen */
	char nome_mapa[80];
	/* estado */
	int iniciado;
	uint64_t seq;
	TzRegraRun *run; int nrun, caprun;
	TzTempRun *temps; int ntemps, captemps;
	TzVariaveis vars;
	uint8_t *esperando; int nesperando;   /* por índice de acción: segunda fase de una ventana o una espera */
	uint8_t *oculto; int nobj_oculto;     /* por índice de objeto: oculto por el mapa (INATIVO) hasta «aparecer» */
	uint32_t perdidos[12][256];  /* objetos de cada reino y clase que murieron */
	uint32_t cursor_ev;
	uint8_t modal;
	char proximo_mapa[160];
	char texto_fim[160];
	int acabou, ganhou_fim; uint32_t tic_fim;
	uint8_t fim_devido;
	/* conversación abierta */
	int conv_activa, conv_i, conv_frase;   /* conv_frase = -1: se ofrecen escolhas */
	int nescolhas_conv, escolhas_conv[8];
	uint8_t sem_apagar, sem_diplomacia;
	char texto_str[64];
	int hora;                    /* el reloj del día de @settime/@gettime */
	TzObjetivo *missoes; int nmissoes, capmissoes;
	TzMensagem msg;
	TzPedido fila[256]; unsigned fila_ini, fila_n;
	TzGuiaoServicos serv;
	int *tmp; int captmp;        /* objetos que casan, de trabajo */
	int profundidade;
	int campanha;
	uint32_t ultimo_hecho;
	/* contadores para pruebas */
	uint32_t n_regras_disparadas, n_acoes;
	int hora_dia_tic;
} TzGuiao;

/* ------------------------------------------------------------------ leer */
/* Lee los guiones de un mapa: los .events/.actions/.rules/.defeat.messages/.conv/.objext/.diplomacy del contenedor
 * (base = nombre del mapa sin extensión, el del primer *.header). Un guion dañado no impide abrir el mapa: queda
 * vacío, cuenta en g->erros y devuelve -1 con el motivo en tz_erro(); el TzGuiao vale igualmente. */
int tz_guiao_ler(TzGuiao *g, TzHpfs *c);
/* Lo mismo desde los trozos ya leídos (pruebas); contadores = palabras 0x0c..0x12 de la cabecera. */
int tz_guiao_de_memoria(TzGuiao *g, const uint16_t contadores[4], const uint8_t *eventos, uint32_t neventos,
                        const uint8_t *acoes, uint32_t nacoes, const uint8_t *regras, uint32_t nregras);
int tz_guiao_derrotas(TzGuiao *g, const uint8_t *d, uint32_t n);   /* .defeat.messages */
int tz_guiao_conversas(TzGuiao *g, const uint8_t *d, uint32_t n);  /* .conv */
void tz_guiao_liberar(TzGuiao *g);

/* ------------------------------------------------------------------ en la partida */
/* Registra el guion en el mundo (un gancho «GUIA»), pone regras.campanha y, si campanha != 0, sem_vitoria_auto (el
 * final lo decide el guion) y m->jogador = g->jogador (la campaña de Gengis Khan empieza como reino 2). Aplica el .diplomacy del mapa. El TzGuiao tiene que vivir tanto como el mundo. */
int tz_guiao_registrar(TzMundo *m, TzGuiao *g, int campanha);
void tz_guiao_servicos(TzGuiao *g, const TzGuiaoServicos *s);
/* Un tic del guion (lo llama el gancho; las pruebas, a mano). */
void tz_guiao_tic(TzMundo *m, TzGuiao *g);

/* ------------------------------------------------------------------ interfaz */
int tz_guiao_pedido(TzGuiao *g, TzPedido *out);                /* saca el siguiente; 0 si no hay */
int tz_guiao_modal(const TzGuiao *g);                          /* una ventana del guion espera al jugador */
const TzMensagem *tz_guiao_mensagem(const TzGuiao *g);         /* lo que muestra la ventana abierta */
void tz_guiao_fechar(TzMundo *m, TzGuiao *g);                  /* el jugador cerró la historia o el aviso */
/* La conversación sigue: con escolhas ofrecidas elige la número «escolha»; con una línea, pasa a las siguientes. */
void tz_guiao_responder(TzMundo *m, TzGuiao *g, int escolha);
const TzFrase *tz_guiao_fala(const TzGuiao *g);                /* la frase que se dice ahora, o NULL (ofrece escolhas) */
const TzPersonagem *tz_guiao_personagem(const TzGuiao *g, int id);
int tz_guiao_acabou(const TzGuiao *g, int *ganhou);            /* el guion terminó la partida */
const char *tz_guiao_proximo_mapa(const TzGuiao *g);           /* ruta de LoadMap o "" */

/* ------------------------------------------------------------------ de una misión a la siguiente (0x43ea20) */
typedef struct { char nome[33], subtitulo[33]; uint16_t vida, vida_max, nivel; uint8_t promovido; } TzHeroi;
int tz_guiao_herois(const TzMundo *m, const TzGuiao *g, TzHeroi *out, int max);
/* Aplica los héroes de la misión anterior a los objetos del mapa con TZ_F_CARREGADO y el mismo nombre. */
void tz_guiao_restaurar_herois(TzMundo *m, const TzGuiao *g, const TzHeroi *h, int n);
/* Las variables «#» que sobreviven al paso de misión. */
int tz_guiao_mantidas(const TzGuiao *g, TzVar *out, int max);
void tz_guiao_mantidas_definir(TzGuiao *g, const TzVar *v, int n);

/* ------------------------------------------------------------------ expresiones, por si las quiere la IA o una prueba */
int32_t tz_guiao_expr(TzMundo *m, TzGuiao *g, const char *texto, int obj);   /* obj = TZ_NENHUM sin objeto */

#endif
