/* SPDX-License-Identifier: GPL-3.0-or-later */
/* AoEN, copyright de su autor (el mantenedor de SaguiOS-Extended); GPL v3 o posterior. Port a SaguiOS: D382. */
/* AoEN multijugador en red local (D461): canal fiable sobre UDP, sala y lockstep. Ver red.h y docs/aoen.md.
 *
 * Capas:
 *  1. Datagramas UDP (un socket sin bloqueo). Para las pruebas, AOEN_RED_PERDIDA=%, AOEN_RED_RETARDO=ms y AOEN_RED_JITTER=ms
 *     pierden, retrasan y desordenan lo que se envía.
 *  2. Enlaces: cada pareja (anfitrión, cliente) tiene un canal de mensajes fiable y ordenado (números de secuencia, ack
 *     acumulativo con bitmap, reenvío con plazo adaptado al ida y vuelta, ventana de 256 mensajes).
 *  3. Sala: un anfitrión y hasta 7 clientes; estado completo de la sala en cada cambio; chat.
 *  4. Lockstep: estrella. Cada cliente manda al anfitrión sus órdenes para el turno t+RED_RETRASO cuando empieza el turno t; el
 *     anfitrión las junta (por jugador) y reparte el turno cerrado. Nadie ejecuta un turno sin tenerlo entero, así que todos
 *     simulan lo mismo. Al acabar un turno se calcula la suma de sincronía y se compara con la del anfitrión. */
#include "red.h"
#include "ordenes.h"
#include "config.h"
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#define MSG_MAX 1100            /* bytes de un mensaje del canal */
#define VENTANA 256             /* mensajes sin confirmar por enlace */
#define RX_VENT 64
#define NENL 8
#define RING 64                 /* turnos que se recuerdan */
#define TURNO_MAX (MAX_PLAYERS * (MSG_MAX + 4) + 400)
#define PARTE 1000

/* tipos de datagrama */
enum { DG_BUSCAR = 1, DG_ANUNCIO, DG_UNIR, DG_UNIDO, DG_RECHAZO, DG_DATOS };
/* tipos de mensaje del canal */
enum { M_SALA = 1, M_ELEGIR, M_CHAT, M_CHATDE, M_EMPEZAR, M_LISTO, M_COMIENZA, M_ORD, M_TURNO, M_SUMA, M_DESYNC, M_SALIR, M_KICK };

/* ---------- utilidades ---------- */
typedef struct { u8 *p; int n, max, mal; } Esc;
static void w8(Esc *e, unsigned v) { if (e->n + 1 > e->max) { e->mal = 1; return; } e->p[e->n++] = (u8)v; }
static void w16(Esc *e, unsigned v) { w8(e, v & 255); w8(e, v >> 8); }
static void w32(Esc *e, unsigned v) { w16(e, v & 0xffff); w16(e, v >> 16); }
static void wb(Esc *e, const void *b, int n) { if (e->n + n > e->max) { e->mal = 1; return; } memcpy(e->p + e->n, b, (size_t)n); e->n += n; }
typedef struct { const u8 *p; int n, i, mal; } Lee;
static unsigned r8(Lee *l) { if (l->i + 1 > l->n) { l->mal = 1; return 0; } return l->p[l->i++]; }
static unsigned r16(Lee *l) { unsigned a = r8(l); return a | r8(l) << 8; }
static unsigned r32(Lee *l) { unsigned a = r16(l); return a | r16(l) << 16; }
static void rb(Lee *l, void *b, int n) { if (l->i + n > l->n) { l->mal = 1; memset(b, 0, (size_t)n); return; } memcpy(b, l->p + l->i, (size_t)n); l->i += n; }

static unsigned ahora(void) { return (unsigned)SDL_GetTicks(); }
static unsigned lcg_s = 2463534242u;
static unsigned azar(void) { lcg_s = lcg_s * 1664525u + 1013904223u; return lcg_s >> 8; }
static int ambiente_int(const char *n, int def) { const char *v = getenv(n); return v && *v ? atoi(v) : def; }

static void copiar_nombre(char *dst, const char *src) {
	int k = 0;
	for (; src && src[k] && k < 15; ++k) { unsigned char c = (unsigned char)src[k]; dst[k] = c >= 32 && c != 127 ? (char)c : '?'; }
	dst[k] = 0;
}

/* ---------- estado ---------- */
typedef struct { u32 seq; int len, reenvios; unsigned t_env; u8 dat[MSG_MAX]; } MsgEnv;
typedef struct { int len; u8 dat[MSG_MAX]; } MsgRec;
typedef struct {
	int usado, plaza;
	RDir dir;
	unsigned idcon, token;
	char nombre[16];
	u32 tx_next, tx_una;                 /* siguiente a asignar y más antiguo sin confirmar */
	MsgEnv *env;                          /* VENTANA */
	u32 rx_next;
	MsgRec *rx;                           /* RX_VENT */
	u32 rx_seq[RX_VENT];                  /* seq guardada en cada hueco (0 = vacío) */
	unsigned ult_rx, ult_tx;
	float srtt;                           /* ms */
	int listo, muerto, pendiente_ack;
} Enlace;

typedef struct { int turno, len; u8 dat[MSG_MAX]; } OrdP;
typedef struct { int turno, len; u8 dat[TURNO_MAX]; } TurnoD;
typedef struct { int turno; unsigned total, g[16]; } SumaT;

static struct {
	int estado, anf, mi_jug;
	char error[120];
	char mi_nombre[16];
	Enlace enl[NENL];
	int n_activos;
	/* partida */
	int N;                                /* jugadores de la partida */
	int humano[MAX_PLAYERS];              /* lo lleva una persona conectada */
	int activo[MAX_PLAYERS];              /* anfitrión: esperamos sus órdenes */
	char nombres[MAX_PLAYERS][16];
	int T, sub_hasta, cerrado, ticks;     /* turno a ejecutar, última orden enviada, siguiente a cerrar, tics por turno */
	float acc;
	int pausa, vel;
	int desinc; unsigned ult_suma;
	OrdP *ordp;                           /* [RING][MAX_PLAYERS] */
	TurnoD *td;                           /* [RING] */
	SumaT *s_anf, *s_cli;                 /* [RING], [RING*MAX_PLAYERS] */
	u8 ctrl[512]; int ctrl_n;
	int asm_turno, asm_len; u8 asm_dat[TURNO_MAX];
	unsigned t_estancado, t_ultimo_turno, t_adapt, t_ini_arranque;
	int bajar_cuenta, falla_hecha;
	/* anunciado */
	RPartida partidas[16]; int n_part;
	/* chat */
	struct { int de; char txt[96]; } chat[32]; int chat_n;
	/* sala */
	int arranque_listos;
} R;

Plaza red_plaza[MAX_PLAYERS];
Setup red_cfg;
int red_mi_plaza, red_ticks_turno = 4, red_suma_cada = 4, red_rapido, red_turno_max;
void (*red_cb_inicio)(void);

static int sock = -1;
static unsigned short sock_puerto;
static int sim_perdida, sim_retardo, sim_jitter, timeout_juego = 10000, timeout_sala = 6000, fijo;

typedef struct { unsigned due; RDir d; int len; u8 dat[1200]; } Retr;
static Retr retr[256];
static int retr_n;

static void error(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
#include <stdarg.h>
static void error(const char *fmt, ...) {
	va_list ap; va_start(ap, fmt);
	vsnprintf(R.error, sizeof R.error, fmt, ap);
	va_end(ap);
	AOEN_LOG("[AOEN] red: %s", R.error);
}

int red_estado(void) { return R.estado; }
int red_abierta(void) { return sock >= 0; }
const char *red_error(void) { return R.error; }
int red_anfitrion(void) { return R.anf; }
int red_turno(void) { return R.T; }
int red_pausado(void) { return R.pausa; }
int red_velocidad(void) { return R.vel ? R.vel : 1; }
int red_desincronizado(void) { return R.desinc; }
unsigned red_ultima_suma(void) { return R.ult_suma; }
int red_jugador_humano(int p) { return p >= 0 && p < MAX_PLAYERS && R.humano[p]; }
const char *red_nombre(int p) { return p >= 0 && p < MAX_PLAYERS ? R.nombres[p] : ""; }

/* ---------- datagramas ---------- */
static void raw_enviar(RDir d, const u8 *b, int n) {
	if (sock < 0) return;
	struct sockaddr_in a;
	memset(&a, 0, sizeof a);
	a.sin_family = AF_INET; a.sin_port = htons(d.puerto); a.sin_addr.s_addr = htonl(d.ip);
	sendto(sock, b, (size_t)n, 0, (struct sockaddr *)&a, sizeof a);
}

static void enviar_dg(RDir d, const u8 *b, int n) {
	if (n > 1200) return;
	if (sim_perdida > 0 && (int)(azar() % 100) < sim_perdida) return;
	if (sim_retardo > 0) {
		if (retr_n >= (int)(sizeof retr / sizeof retr[0])) return;
		Retr *r = &retr[retr_n++];
		r->due = ahora() + (unsigned)sim_retardo + (sim_jitter > 0 ? azar() % (unsigned)(sim_jitter + 1) : 0);
		r->d = d; r->len = n; memcpy(r->dat, b, (size_t)n);
		return;
	}
	raw_enviar(d, b, n);
}

static void vaciar_retardados(void) {
	unsigned t = ahora();
	for (int k = 0; k < retr_n; ) {
		if ((int)(t - retr[k].due) >= 0) { raw_enviar(retr[k].d, retr[k].dat, retr[k].len); retr[k] = retr[--retr_n]; }
		else ++k;
	}
}

static int misma_dir(RDir a, RDir b) { return a.ip == b.ip && a.puerto == b.puerto; }

/* ---------- enlaces: canal fiable ---------- */
static void enl_liberar(Enlace *e) {
	free(e->env); free(e->rx);
	memset(e, 0, sizeof *e);
}

static Enlace *enl_nuevo(RDir d, unsigned idcon, unsigned token, const char *nombre, int plaza) {
	for (int k = 0; k < NENL; ++k) {
		Enlace *e = &R.enl[k];
		if (e->usado) continue;
		memset(e, 0, sizeof *e);
		e->env = (MsgEnv *)calloc(VENTANA, sizeof(MsgEnv));
		e->rx = (MsgRec *)calloc(RX_VENT, sizeof(MsgRec));
		if (!e->env || !e->rx) { free(e->env); free(e->rx); memset(e, 0, sizeof *e); return NULL; }
		e->usado = 1; e->dir = d; e->idcon = idcon; e->token = token; e->plaza = plaza;
		copiar_nombre(e->nombre, nombre);
		e->tx_next = e->tx_una = 1; e->rx_next = 1; e->srtt = 60;
		e->ult_rx = e->ult_tx = ahora();
		return e;
	}
	return NULL;
}

static void enl_dg(Enlace *e, u32 seq, const u8 *m, int len) {
	u8 b[40 + MSG_MAX];
	Esc w = { b, 0, sizeof b, 0 };
	w8(&w, 'A'); w8(&w, RED_PROTO); w8(&w, DG_DATOS); w16(&w, e->idcon);
	w32(&w, e->rx_next);
	unsigned sack = 0;
	for (int k = 0; k < 32; ++k) { u32 s = e->rx_next + 1 + (u32)k; if (e->rx_seq[s % RX_VENT] == s) sack |= 1u << k; }
	w32(&w, sack);
	w32(&w, seq); w16(&w, (unsigned)len);
	if (len) wb(&w, m, len);
	enviar_dg(e->dir, b, w.n);
	e->ult_tx = ahora(); e->pendiente_ack = 0;
}

/* encola un mensaje fiable; 0 si la ventana está llena */
static int enl_enviar(Enlace *e, const u8 *m, int len) {
	if (!e->usado || len > MSG_MAX) return 0;
	if (e->tx_next - e->tx_una >= VENTANA) return 0;
	MsgEnv *x = &e->env[e->tx_next % VENTANA];
	x->seq = e->tx_next; x->len = len; x->reenvios = 0; x->t_env = ahora();
	memcpy(x->dat, m, (size_t)len);
	++e->tx_next;
	enl_dg(e, x->seq, x->dat, len);
	return 1;
}

static void procesar_msg(Enlace *e, const u8 *m, int n);

static void enl_recibir(Enlace *e, const u8 *b, int n) {
	Lee l = { b, n, 3 + 2, 0 };
	u32 ack = r32(&l); unsigned sack = r32(&l); u32 seq = r32(&l); int len = (int)r16(&l);
	if (l.mal || l.i + len > n || len > MSG_MAX) return;
	e->ult_rx = ahora();
	/* confirmaciones */
	for (u32 s = e->tx_una; (int)(ack - s) > 0 && s < e->tx_next; ++s) {
		MsgEnv *x = &e->env[s % VENTANA];
		if (x->reenvios == 0) { float r = (float)(ahora() - x->t_env); e->srtt = e->srtt * 0.875f + r * 0.125f; }
		x->len = -1;
	}
	if ((int)(ack - e->tx_una) > 0) e->tx_una = ack < e->tx_next ? ack : e->tx_next;
	for (int k = 0; k < 32; ++k)
		if (sack & (1u << k)) { u32 s = ack + 1 + (u32)k; if (s >= e->tx_una && s < e->tx_next) e->env[s % VENTANA].len = -1; }
	while (e->tx_una < e->tx_next && e->env[e->tx_una % VENTANA].len == -1) ++e->tx_una;
	if (!len || !seq) return;
	/* datos */
	if ((int)(seq - e->rx_next) < 0) { e->pendiente_ack = 1; return; }   /* repetido */
	if (seq - e->rx_next >= RX_VENT) return;
	if (e->rx_seq[seq % RX_VENT] != seq) {
		MsgRec *rr = &e->rx[seq % RX_VENT];
		rr->len = len; memcpy(rr->dat, b + l.i, (size_t)len);
		e->rx_seq[seq % RX_VENT] = seq;
	}
	while (e->rx_seq[e->rx_next % RX_VENT] == e->rx_next) {
		MsgRec tmp = e->rx[e->rx_next % RX_VENT];
		e->rx_seq[e->rx_next % RX_VENT] = 0;
		++e->rx_next;
		if (!e->usado) break;
		procesar_msg(e, tmp.dat, tmp.len);
		if (!e->usado) return;
	}
	enl_dg(e, 0, NULL, 0);   /* ack inmediato */
}

static void enl_tick(Enlace *e) {
	unsigned t = ahora();
	float rto = e->srtt * 1.5f + 25.0f;
	if (rto < 40) rto = 40;
	for (u32 s = e->tx_una; s < e->tx_next; ++s) {
		MsgEnv *x = &e->env[s % VENTANA];
		if (x->len < 0) continue;
		float plazo = rto * (float)(1 << MIN(x->reenvios, 4));
		if (plazo > 1000) plazo = 1000;
		if ((float)(t - x->t_env) >= plazo) {
			/* Karn: lo reenviado no da muestra del ida y vuelta; si el plazo se queda corto (la red es más lenta de lo que se
			 * creía) la primera vez se agranda la estimación, o no se volvería a medir nunca */
			if (x->reenvios == 0) e->srtt = MIN(e->srtt * 1.5f, 1000.0f);
			x->t_env = t; ++x->reenvios; enl_dg(e, x->seq, x->dat, x->len);
		}
	}
	if (t - e->ult_tx >= 200) enl_dg(e, 0, NULL, 0);   /* mantiene vivo el enlace y repite las confirmaciones */
}

/* ---------- apertura y cierre ---------- */
int red_iniciar(void) {
	if (sock >= 0) return 1;
	sim_perdida = ambiente_int("AOEN_RED_PERDIDA", 0); sim_retardo = ambiente_int("AOEN_RED_RETARDO", 0); sim_jitter = ambiente_int("AOEN_RED_JITTER", 0);
	timeout_juego = ambiente_int("AOEN_RED_TIMEOUT", 10000); fijo = ambiente_int("AOEN_RED_FIJO", 0);
	red_rapido = ambiente_int("AOEN_RED_RAPIDO", 0);
	lcg_s = (unsigned)(ahora() * 2654435761u) ^ (unsigned)(size_t)&R ^ (unsigned)ambiente_int("AOEN_RED_AZAR", 0);
	int fd = socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK, 0);
	if (fd < 0) { error("No se pudo abrir la red"); return 0; }
	int uno = 1;
	setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &uno, sizeof uno);
	int base = ambiente_int("AOEN_RED_PUERTO", RED_PUERTO);
	/* el anfitrión usa el puerto de la red; los demás, uno libre a continuación */
	int quiero = ambiente_int("AOEN_RED_MIPUERTO", 0);
	int ok = 0;
	for (int k = 0; k < 24 && !ok; ++k) {
		int pto = quiero ? quiero : base + k;
		struct sockaddr_in a;
		memset(&a, 0, sizeof a);
		a.sin_family = AF_INET; a.sin_port = htons((unsigned short)pto); a.sin_addr.s_addr = htonl(INADDR_ANY);
		if (bind(fd, (struct sockaddr *)&a, sizeof a) == 0) { sock_puerto = (unsigned short)pto; ok = 1; }
		if (quiero) break;
	}
	if (!ok) { close(fd); error("No se pudo reservar un puerto de red"); return 0; }
	sock = fd;
	memset(&R, 0, sizeof R);
	R.estado = RED_NADA; R.desinc = -1; R.vel = 1;
	AOEN_LOG("[AOEN] red: socket abierto en el puerto %d (pérdida %d%%, retardo %d ms)", sock_puerto, sim_perdida, sim_retardo);
	return 1;
}

static void liberar_partida(void) {
	free(R.ordp); free(R.td); free(R.s_anf); free(R.s_cli);
	R.ordp = NULL; R.td = NULL; R.s_anf = R.s_cli = NULL;
}

void red_cerrar(void) {
	if (sock >= 0) {
		u8 m[1] = { M_SALIR };
		for (int k = 0; k < NENL; ++k) if (R.enl[k].usado) { enl_enviar(&R.enl[k], m, 1); }
		vaciar_retardados();
	}
	for (int k = 0; k < NENL; ++k) if (R.enl[k].usado) enl_liberar(&R.enl[k]);
	liberar_partida();
	if (sock >= 0) { close(sock); sock = -1; }
	retr_n = 0;
	ord_red = 0; ord_reiniciar();
	memset(&R, 0, sizeof R);
	R.desinc = -1; R.vel = 1;
}

/* ---------- sala ---------- */
static void sala_por_defecto(void) {
	memset(red_plaza, 0, sizeof red_plaza);
	for (int p = 0; p < MAX_PLAYERS; ++p) {
		red_plaza[p].tipo = p == 0 ? PL_HUMANO : p == 1 ? PL_LIBRE : PL_CERRADA;
		red_plaza[p].civ = p % NUM_CIVS; red_plaza[p].color = p + 1; red_plaza[p].equipo = 0; red_plaza[p].dif = 1;
	}
}

int red_crear_sala(const char *nombre) {
	if (!red_iniciar()) return 0;
	for (int k = 0; k < NENL; ++k) if (R.enl[k].usado) enl_liberar(&R.enl[k]);
	R.estado = RED_SALA_ANF; R.anf = 1; R.mi_jug = 0; red_mi_plaza = 0;
	copiar_nombre(R.mi_nombre, nombre && *nombre ? nombre : "Anfitrión");
	if (ambiente_int("AOEN_RED_MIPUERTO", 0) == 0 && sock_puerto != RED_PUERTO && !getenv("AOEN_RED_PUERTO")) AOEN_LOG("[AOEN] red: el puerto %d no es el de la red local (%d): los demás no te encontrarán por difusión", sock_puerto, RED_PUERTO);
	sala_por_defecto();
	copiar_nombre(red_plaza[0].nombre, R.mi_nombre);
	R.chat_n = 0;
	AOEN_LOG("[AOEN] red: sala creada por %s (puerto %d)", R.mi_nombre, sock_puerto);
	return 1;
}

int red_dir_texto(const char *txt, RDir *d) {
	unsigned a, b, c, e, p = RED_PUERTO;
	if (!txt) return 0;
	int n = sscanf(txt, "%u.%u.%u.%u:%u", &a, &b, &c, &e, &p);
	if (n < 4 || a > 255 || b > 255 || c > 255 || e > 255 || p == 0 || p > 65535) return 0;
	d->ip = a << 24 | b << 16 | c << 8 | e; d->puerto = (unsigned short)p;
	return 1;
}

static RDir dir_difusion(void) { RDir d = { 0xffffffffu, (unsigned short)ambiente_int("AOEN_RED_PUERTO", RED_PUERTO) }; return d; }

void red_buscar(void) {
	if (!red_iniciar()) return;
	u8 b[3] = { 'A', RED_PROTO, DG_BUSCAR };
	enviar_dg(dir_difusion(), b, 3);
	const char *extra = getenv("AOEN_RED_BUSCAR");   /* lista de IP[:puerto] separadas por comas (pruebas y redes sin difusión) */
	while (extra && *extra) {
		char t[48]; int k = 0;
		while (*extra && *extra != ',' && k < 47) t[k++] = *extra++;
		t[k] = 0; if (*extra == ',') ++extra;
		RDir d; if (red_dir_texto(t, &d)) enviar_dg(d, b, 3);
	}
}

int red_partidas(RPartida *out, int max) {
	unsigned t = ahora();
	int n = 0;
	for (int k = 0; k < R.n_part && n < max; ++k) if (t - R.partidas[k].visto < 5000) out[n++] = R.partidas[k];
	return n;
}

static void anunciar_a(RDir d) {
	u8 b[80]; Esc w = { b, 0, sizeof b, 0 };
	int hum = 0, tot = 0;
	for (int p = 0; p < MAX_PLAYERS; ++p) { if (red_plaza[p].tipo == PL_HUMANO) ++hum; if (red_plaza[p].tipo == PL_HUMANO || red_plaza[p].tipo == PL_IA) ++tot; }
	w8(&w, 'A'); w8(&w, RED_PROTO); w8(&w, DG_ANUNCIO);
	char nom[16] = { 0 }; copiar_nombre(nom, R.mi_nombre);
	wb(&w, nom, 16); w8(&w, (unsigned)tot); w8(&w, (unsigned)hum); w8(&w, (unsigned)red_cfg.map_kind); w16(&w, (unsigned)red_cfg.width);
	enviar_dg(d, b, w.n);
}

int red_unirse(RDir dir, const char *nombre) {
	if (!red_iniciar()) return 0;
	for (int k = 0; k < NENL; ++k) if (R.enl[k].usado) enl_liberar(&R.enl[k]);
	copiar_nombre(R.mi_nombre, nombre && *nombre ? nombre : "Jugador");
	R.estado = RED_UNIENDO; R.anf = 0;
	R.enl[0].dir = dir;   /* se reutiliza el hueco para recordar a quién se llama (usado=0 hasta que conteste) */
	R.enl[0].token = azar() | 1; R.enl[0].ult_tx = 0; R.enl[0].ult_rx = ahora();
	R.error[0] = 0;
	AOEN_LOG("[AOEN] red: llamando a %u.%u.%u.%u:%u", dir.ip >> 24, (dir.ip >> 16) & 255, (dir.ip >> 8) & 255, dir.ip & 255, dir.puerto);
	return 1;
}

static int enlaces_vivos(void) { int n = 0; for (int k = 0; k < NENL; ++k) n += R.enl[k].usado; return n; }

/* ---------- la sala por la red ---------- */
static void escribir_sala(Esc *w, int codigo, int plaza_dest, const Plaza *pl, int N) {
	w8(w, (unsigned)codigo); w8(w, RED_PROTO); w16(w, (unsigned)sizeof(Setup));
	wb(w, &red_cfg, (int)sizeof(Setup));
	for (int p = 0; p < MAX_PLAYERS; ++p) {
		w8(w, (unsigned)pl[p].tipo); w8(w, (unsigned)pl[p].civ); w8(w, (unsigned)pl[p].color); w8(w, (unsigned)pl[p].equipo); w8(w, (unsigned)pl[p].dif); w8(w, (unsigned)pl[p].listo);
		char nom[16] = { 0 }; copiar_nombre(nom, pl[p].nombre); wb(w, nom, 16);
	}
	w8(w, (unsigned)plaza_dest); w8(w, (unsigned)red_ticks_turno); w8(w, (unsigned)red_suma_cada); w8(w, (unsigned)N);
}

static int leer_sala(const u8 *m, int n, Setup *cfg, Plaza *pl, int *plaza_dest, int *N) {
	Lee l = { m, n, 1, 0 };
	if (r8(&l) != RED_PROTO) return 0;
	if (r16(&l) != sizeof(Setup)) return 0;
	rb(&l, cfg, (int)sizeof(Setup));
	for (int p = 0; p < MAX_PLAYERS; ++p) {
		pl[p].tipo = (int)r8(&l); pl[p].civ = (int)r8(&l); pl[p].color = (int)r8(&l); pl[p].equipo = (int)r8(&l); pl[p].dif = (int)r8(&l); pl[p].listo = (int)r8(&l);
		rb(&l, pl[p].nombre, 16); pl[p].nombre[15] = 0;
		if (pl[p].tipo > PL_CERRADA || pl[p].civ >= NUM_CIVS || pl[p].color > 8 || pl[p].equipo > 4 || pl[p].dif > 3) l.mal = 1;
	}
	*plaza_dest = (int)r8(&l);
	int tk = (int)r8(&l), sc = (int)r8(&l);
	*N = (int)r8(&l);
	if (tk >= 3 && tk <= 6) red_ticks_turno = tk;
	if (sc >= 1 && sc <= 60) red_suma_cada = sc;
	return !l.mal && *plaza_dest < MAX_PLAYERS;
}

void red_sala_cambiada(void) {
	if (!R.anf || R.estado != RED_SALA_ANF) return;
	for (int k = 0; k < NENL; ++k) {
		Enlace *e = &R.enl[k];
		if (!e->usado) continue;
		u8 b[MSG_MAX]; Esc w = { b, 0, sizeof b, 0 };
		escribir_sala(&w, M_SALA, e->plaza, red_plaza, 0);
		enl_enviar(e, b, w.n);
	}
}

static void anotar_chat(int de, const char *txt) {
	int k = R.chat_n++ % 32;
	R.chat[k].de = de; snprintf(R.chat[k].txt, sizeof R.chat[k].txt, "%s", txt);
}
int red_chat_n(void) { return R.chat_n; }
const char *red_chat_texto(int k, int *de) {
	if (k < 0 || k >= R.chat_n || k < R.chat_n - 32) { if (de) *de = -1; return ""; }
	if (de) *de = R.chat[k % 32].de;
	return R.chat[k % 32].txt;
}

static void difundir_chat(int de, const char *txt) {
	anotar_chat(de, txt);
	if (R.estado == RED_JUEGO) world_chat(de, txt);
	u8 b[MSG_MAX]; Esc w = { b, 0, sizeof b, 0 };
	w8(&w, M_CHATDE); w8(&w, (unsigned)(de + 1)); wb(&w, txt, (int)strlen(txt) + 1);
	for (int k = 0; k < NENL; ++k) if (R.enl[k].usado) enl_enviar(&R.enl[k], b, w.n);
}

void red_chat(const char *texto) {
	if (!texto || !*texto || sock < 0) return;
	char t[96]; snprintf(t, sizeof t, "%s", texto);
	if (R.anf) { difundir_chat(red_mi_plaza >= 0 && R.estado >= RED_ARRANCANDO ? R.mi_jug : 0, t); return; }
	u8 b[MSG_MAX]; Esc w = { b, 0, sizeof b, 0 };
	w8(&w, M_CHAT); wb(&w, t, (int)strlen(t) + 1);
	if (R.enl[0].usado) enl_enviar(&R.enl[0], b, w.n);
}

void red_elegir(int civ, int color, int equipo, int listo) {
	if (R.anf) {
		Plaza *p = &red_plaza[0];
		p->civ = CLAMP(civ, 0, NUM_CIVS - 1); p->color = CLAMP(color, 1, 8); p->equipo = CLAMP(equipo, 0, 4); p->listo = listo ? 1 : 0;
		red_sala_cambiada();
		return;
	}
	if (!R.enl[0].usado) return;
	u8 b[8]; Esc w = { b, 0, sizeof b, 0 };
	w8(&w, M_ELEGIR); w8(&w, (unsigned)civ); w8(&w, (unsigned)color); w8(&w, (unsigned)equipo); w8(&w, (unsigned)listo);
	enl_enviar(&R.enl[0], b, w.n);
}

int red_ping_ms(int plaza) {
	if (!R.anf) return R.enl[0].usado ? (int)R.enl[0].srtt : 0;
	for (int k = 0; k < NENL; ++k) if (R.enl[k].usado && R.enl[k].plaza == plaza) return (int)R.enl[k].srtt;
	return 0;
}

int red_humanos_listos(void) {
	int n = 0;
	for (int p = 1; p < MAX_PLAYERS; ++p) if (red_plaza[p].tipo == PL_HUMANO && red_plaza[p].listo) ++n;
	return n;
}

static void quitar_enlace(Enlace *e, const char *por) {
	int p = e->plaza;
	if (R.anf && R.estado == RED_SALA_ANF && p > 0 && p < MAX_PLAYERS) {
		red_plaza[p].tipo = PL_LIBRE; red_plaza[p].listo = 0;
		char t[96]; snprintf(t, sizeof t, "%s se ha ido (%s)", e->nombre, por);
		difundir_chat(-1, t);
	}
	enl_liberar(e);
	if (R.anf && R.estado == RED_SALA_ANF) red_sala_cambiada();
}

int red_expulsar(int plaza) {
	for (int k = 0; k < NENL; ++k) {
		Enlace *e = &R.enl[k];
		if (e->usado && e->plaza == plaza && R.anf) { u8 m[1] = { M_KICK }; enl_enviar(e, m, 1); vaciar_retardados(); quitar_enlace(e, "expulsado"); return 1; }
	}
	return 0;
}

/* ---------- comenzar la partida ---------- */
static SumaT *suma_cli(int p, int t) { return &R.s_cli[p * RING + (t % RING + RING) % RING]; }

static int montar_mundo(const Setup *s, const Plaza *pl, int N, int mi_jug) {
	liberar_partida();
	R.ordp = (OrdP *)calloc((size_t)RING * MAX_PLAYERS, sizeof(OrdP));
	R.td = (TurnoD *)calloc(RING, sizeof(TurnoD));
	R.s_anf = (SumaT *)calloc(RING, sizeof(SumaT));
	R.s_cli = (SumaT *)calloc((size_t)RING * MAX_PLAYERS, sizeof(SumaT));
	if (!R.ordp || !R.td || !R.s_anf || !R.s_cli) { liberar_partida(); error("Sin memoria para la partida en red"); return 0; }
	for (int k = 0; k < RING; ++k) R.s_anf[k].turno = -1;
	for (int k = 0; k < RING * MAX_PLAYERS; ++k) R.s_cli[k].turno = -1;
	world_new(s);
	if (!W.nents || !W.tile) { world_free(); error("No se pudo crear el mapa"); return 0; }
	R.N = N;
	for (int p = 0; p < MAX_PLAYERS; ++p) {
		R.humano[p] = R.activo[p] = 0; R.nombres[p][0] = 0;
		if (p >= N) continue;
		W.pl[p].ai = pl[p].tipo == PL_IA;
		W.pl[p].diff = CLAMP(pl[p].dif, 0, 3);
		R.humano[p] = pl[p].tipo == PL_HUMANO; R.activo[p] = R.humano[p];
		copiar_nombre(R.nombres[p], pl[p].tipo == PL_IA ? "IA" : pl[p].nombre);
	}
	W.human = mi_jug; R.mi_jug = mi_jug; red_mi_plaza = mi_jug;
	ord_reiniciar();
	ord_red = 1;
	R.T = 0; R.cerrado = RED_RETRASO; R.sub_hasta = RED_RETRASO - 1; R.acc = 0; R.pausa = 0; R.vel = 1; R.ticks = red_ticks_turno;
	R.desinc = -1; R.ult_suma = 0; R.ctrl_n = 0; R.asm_len = 0; R.asm_turno = -1; R.bajar_cuenta = 0; R.falla_hecha = 0;
	for (int t = 0; t < RED_RETRASO; ++t) { R.td[t % RING].turno = t; R.td[t % RING].len = 0; }
	AOEN_LOG("[AOEN] red: partida montada: %d jugadores, yo soy el %d, semilla %d, mapa %dx%d, suma inicial %08x", N, mi_jug + 1, s->seed, W.w, W.h, world_suma());
	if (red_cb_inicio) red_cb_inicio();
	return 1;
}

int red_empezar(void) {
	if (!R.anf || R.estado != RED_SALA_ANF) { error("Solo el anfitrión puede empezar"); return 0; }
	Plaza pl[MAX_PLAYERS]; int viejo[MAX_PLAYERS], N = 0;
	memset(pl, 0, sizeof pl);
	for (int p = 0; p < MAX_PLAYERS; ++p) {
		if (red_plaza[p].tipo != PL_HUMANO && red_plaza[p].tipo != PL_IA) continue;
		if (red_plaza[p].tipo == PL_HUMANO && p > 0 && !red_plaza[p].listo) { error("%s aún no está listo", red_plaza[p].nombre); return 0; }
		viejo[N] = p; pl[N++] = red_plaza[p];
	}
	if (N < 2) { error("Hace falta al menos un rival: añade una plaza de la máquina"); return 0; }
	Setup s = red_cfg;
	s.players = N;
	for (int i = 0; i < N; ++i) {
		s.civ[i] = pl[i].civ; s.color[i] = pl[i].color; s.ai_diff[i] = pl[i].dif;
		s.team[i] = pl[i].equipo > 0 ? pl[i].equipo : 10 + i;
	}
	/* a cada cliente se le dice su jugador nuevo; sus enlaces pasan a usar ese número */
	for (int i = 0; i < N; ++i)
		for (int k = 0; k < NENL; ++k) if (R.enl[k].usado && R.enl[k].plaza == viejo[i]) { R.enl[k].plaza = i; R.enl[k].listo = 0; }
	for (int k = 0; k < NENL; ++k) {
		Enlace *e = &R.enl[k];
		if (!e->usado) continue;
		Setup tmp = red_cfg; red_cfg = s;
		u8 b[MSG_MAX]; Esc w = { b, 0, sizeof b, 0 };
		escribir_sala(&w, M_EMPEZAR, e->plaza, pl, N);
		red_cfg = tmp;
		enl_enviar(e, b, w.n);
	}
	R.estado = RED_ARRANCANDO; R.t_ini_arranque = ahora();
	if (!montar_mundo(&s, pl, N, 0)) { R.estado = RED_ERROR; return 0; }
	AOEN_LOG("[AOEN] red: el anfitrión empieza la partida con %d jugadores", N);
	return 1;
}

static void comenzar(void) {
	R.estado = RED_JUEGO; R.t_ultimo_turno = R.t_adapt = ahora();
	AOEN_LOG("[AOEN] red: comienza la partida en red (turnos de %d tics, suma cada %d)", R.ticks, red_suma_cada);
}

/* ---------- lockstep ---------- */
static void escribir_ctrl(int op, int a) {
	OrdRed o; memset(&o, 0, sizeof o);
	o.op = (u8)op; o.jug = 255; o.a = a;
	int r = ord_codificar(&o, R.ctrl + R.ctrl_n, (int)sizeof R.ctrl - R.ctrl_n);
	if (r > 0) R.ctrl_n += r;
}

static void soltar_jugador(int p, const char *motivo) {
	if (p <= 0 || p >= R.N || !R.activo[p]) return;
	R.activo[p] = 0;
	escribir_ctrl(OP_SOLTAR, p);
	AOEN_LOG("[AOEN] red: el jugador %d (%s) se va: %s; pasa a la máquina", p + 1, R.nombres[p], motivo);
	for (int k = 0; k < NENL; ++k) if (R.enl[k].usado && R.enl[k].plaza == p) enl_liberar(&R.enl[k]);
}

void red_control(int op, int jug, int a, int b) {
	(void)b;
	switch (op) {
	case OP_PAUSA:
		R.pausa = !R.pausa;
		{ char m[64]; snprintf(m, sizeof m, R.pausa ? "%s pausa la partida" : "%s sigue la partida", jug >= 0 && jug < MAX_PLAYERS ? R.nombres[jug] : "?"); world_msg(m); }
		break;
	case OP_VELOCIDAD: R.vel = CLAMP(a, 1, 4); break;
	case OP_TURNO: R.ticks = CLAMP(a, 3, 6); break;
	case OP_SOLTAR:
		if (a >= 0 && a < MAX_PLAYERS && W.pl[a].active && !W.pl[a].ai) {
			W.pl[a].ai = 1; W.pl[a].diff = 1; W.pl[a].ai_t = 0;
			R.humano[a] = 0; R.activo[a] = 0;
			char m[96]; snprintf(m, sizeof m, "%s se va: su civilización pasa a la máquina", R.nombres[a]); world_msg(m); world_chat(a, "…");
			AOEN_LOG("[AOEN] red: el jugador %d pasa a la máquina (turno %d)", a + 1, R.T);
		}
		break;
	default: break;
	}
}

void red_pedir_pausa(void) { if (!ord_red) return; OrdRed o; memset(&o, 0, sizeof o); o.op = OP_PAUSA; o.jug = (u8)W.human; ord_poner(&o); }
void red_pedir_velocidad(int v) { if (!ord_red) return; OrdRed o; memset(&o, 0, sizeof o); o.op = OP_VELOCIDAD; o.jug = (u8)W.human; o.a = CLAMP(v, 1, 4); ord_poner(&o); }

static void enviar_mi_orden(int t) {
	u8 buf[1000];
	int n = ord_sacar(buf, (int)sizeof buf);
	if (R.anf) {
		OrdP *o = &R.ordp[(t % RING) * MAX_PLAYERS + R.mi_jug];
		o->turno = t; o->len = n; memcpy(o->dat, buf, (size_t)n);
		return;
	}
	u8 b[MSG_MAX]; Esc w = { b, 0, sizeof b, 0 };
	w8(&w, M_ORD); w32(&w, (unsigned)t); wb(&w, buf, n);
	if (R.enl[0].usado && !enl_enviar(&R.enl[0], b, w.n)) AOEN_LOG("[AOEN] red: ventana de envío llena");
}

static void difundir_turno(int t) {
	TurnoD *d = &R.td[t % RING];
	int partes = MAX(1, (d->len + PARTE - 1) / PARTE);
	for (int k = 0; k < NENL; ++k) {
		Enlace *e = &R.enl[k];
		if (!e->usado || e->plaza <= 0) continue;
		for (int q = 0; q < partes; ++q) {
			u8 b[MSG_MAX]; Esc w = { b, 0, sizeof b, 0 };
			int n = MIN(PARTE, d->len - q * PARTE);
			w8(&w, M_TURNO); w32(&w, (unsigned)t); w8(&w, (unsigned)q); w8(&w, (unsigned)partes); wb(&w, d->dat + q * PARTE, n);
			if (!enl_enviar(e, b, w.n)) { soltar_jugador(e->plaza, "no contesta"); break; }
		}
	}
}

/* el anfitrión cierra todos los turnos que ya tienen las órdenes de todos */
static void anfitrion_cerrar(void) {
	for (;;) {
		int X = R.cerrado;
		if (X >= RED_RETRASO && R.ordp[(X % RING) * MAX_PLAYERS + R.mi_jug].turno != X) return;   /* aún no di las mías */
		for (int p = 1; p < R.N; ++p)
			if (R.humano[p] && R.activo[p] && R.ordp[(X % RING) * MAX_PLAYERS + p].turno != X) return;
		TurnoD *d = &R.td[X % RING];
		d->turno = X; d->len = 0;
		for (int p = 0; p < R.N; ++p) {
			OrdP *o = &R.ordp[(X % RING) * MAX_PLAYERS + p];
			if (o->turno != X || o->len <= 0 || !(R.activo[p] || p == R.mi_jug)) continue;
			d->dat[d->len++] = (u8)p; d->dat[d->len++] = (u8)(o->len & 255); d->dat[d->len++] = (u8)(o->len >> 8);
			memcpy(d->dat + d->len, o->dat, (size_t)o->len); d->len += o->len;
		}
		if (R.ctrl_n > 0) {
			d->dat[d->len++] = 255; d->dat[d->len++] = (u8)(R.ctrl_n & 255); d->dat[d->len++] = (u8)(R.ctrl_n >> 8);
			memcpy(d->dat + d->len, R.ctrl, (size_t)R.ctrl_n); d->len += R.ctrl_n; R.ctrl_n = 0;
		}
		difundir_turno(X);
		++R.cerrado;
	}
}

unsigned red_esperando(void) {
	if (R.estado != RED_JUEGO) return 0;
	if (R.anf) {
		unsigned m = 0;
		int X = R.cerrado;
		if (X > R.T + RED_RETRASO + 2) return 0;
		for (int p = 1; p < R.N; ++p)
			if (R.humano[p] && R.activo[p] && R.ordp[(X % RING) * MAX_PLAYERS + p].turno != X && X >= RED_RETRASO) m |= 1u << p;
		return m;
	}
	return R.td[R.T % RING].turno == R.T ? 0 : 1u;
}

static void volcar_desinc(int turno, const unsigned *otro) {
	char dir[1100], ruta[1200];
	const char *e = getenv("AOEN_RED_VOLCADO");
	if (e && *e) snprintf(dir, sizeof dir, "%s/", e); else cfg_saves_dir(dir, sizeof dir);
	snprintf(ruta, sizeof ruta, "%sdesinc-j%d-t%d.txt", dir, R.mi_jug + 1, turno);
	if (world_volcar(ruta)) {
		FILE *f = fopen(ruta, "a");
		if (f) {
			fprintf(f, "# desincronización en el turno %d (el volcado es del turno %d)\n", turno, R.T);
			SumaT *m = &R.s_anf[turno % RING];
			if (m->turno == turno) { fprintf(f, "# mis grupos en ese turno:"); for (int k = 0; k < 10; ++k) fprintf(f, " %08x", m->g[k]); fprintf(f, "\n"); }
			if (otro) { fprintf(f, "# grupos del anfitrión:"); for (int k = 0; k < 10; ++k) fprintf(f, " %08x", otro[k]); fprintf(f, "\n"); }
			fclose(f);
		}
	}
	AOEN_LOG("[AOEN] red: volcado de depuración en %s", ruta);
}

static void desincronizacion(int turno, int quien, const unsigned *g_anf, const unsigned *g_cli) {
	if (R.desinc >= 0) return;   /* solo la primera: después todo seguirá distinto y no hay nada nuevo que decir */
	R.desinc = turno;
	char grupos[160] = ""; int n = 0;
	for (int k = 0; k < 10 && g_anf && g_cli; ++k) if (g_anf[k] != g_cli[k]) n += snprintf(grupos + n, sizeof grupos - (size_t)n, " %d", k);
	AOEN_LOG("[AOEN] DESINCRONIZACION en el turno %d con el jugador %d: grupos distintos:%s", turno, quien + 1, grupos);
	volcar_desinc(turno, g_anf);
}

static void comprobar_suma(int t) {
	SumaT *a = &R.s_anf[t % RING];
	if (a->turno != t) return;
	for (int p = 1; p < R.N; ++p) {
		SumaT *c = suma_cli(p, t);
		if (c->turno != t) continue;
		c->turno = -2;   /* ya comparada */
		if (c->total == a->total || R.desinc >= 0) continue;
		desincronizacion(t, p, a->g, c->g);
		u8 b[80]; Esc w = { b, 0, sizeof b, 0 };
		w8(&w, M_DESYNC); w32(&w, (unsigned)t); w32(&w, a->total);
		for (int k = 0; k < 10; ++k) w32(&w, a->g[k]);
		for (int q = 0; q < NENL; ++q) if (R.enl[q].usado) enl_enviar(&R.enl[q], b, w.n);
	}
}

static int ejecutar_turno(int t) {
	int hechos = 0;
	TurnoD *d = &R.td[t % RING];
	world_invalidar_cachas();
	for (int i = 0; i + 3 <= d->len; ) {
		int jug = d->dat[i], n = d->dat[i + 1] | d->dat[i + 2] << 8;
		i += 3;
		if (i + n > d->len) break;
		ord_aplicar_bloque(jug, d->dat + i, n);
		i += n;
	}
	if (!R.pausa) for (int k = 0; k < R.ticks; ++k) { world_update(1.0f / 30); ++hechos; }
	++R.T;
	if (red_suma_cada > 0 && t % red_suma_cada == 0) {
		int falla = ambiente_int("AOEN_RED_FALLA", -1);
		if (falla >= 0 && t >= falla && !R.falla_hecha && R.mi_jug == ambiente_int("AOEN_RED_FALLA_JUG", 1)) { R.falla_hecha = 1; W.pl[0].res[RES_WOOD] += 1; AOEN_LOG("[AOEN] red: (prueba) se provoca una desincronización en el turno %d", t); }
		unsigned g[16]; unsigned total = world_suma_grupos(g);
		R.ult_suma = total;
		AOEN_LOG("[AOEN] red: suma turno %d: %08x", t, total);
		if (R.anf) {
			SumaT *a = &R.s_anf[t % RING];
			a->turno = t; a->total = total; memcpy(a->g, g, sizeof a->g);
			comprobar_suma(t);
		} else if (R.enl[0].usado) {
			u8 b[80]; Esc w = { b, 0, sizeof b, 0 };
			w8(&w, M_SUMA); w32(&w, (unsigned)t); w32(&w, total);
			for (int k = 0; k < 10; ++k) w32(&w, g[k]);
			enl_enviar(&R.enl[0], b, w.n);
			SumaT *a = &R.s_anf[t % RING];   /* la mía, por si el anfitrión avisa de una desincronización */
			a->turno = t; a->total = total; memcpy(a->g, g, sizeof a->g);
		}
	}
	return hechos;
}

int red_avanzar(float dt) {
	if (R.estado != RED_JUEGO) return 0;
	int ticks_hechos = 0, turnos = 0;
	unsigned t = ahora();
	if (R.anf) anfitrion_cerrar();
	R.acc += dt * (float)R.vel;
	float dur = (float)R.ticks / 30.0f;
	if (red_rapido) R.acc = dur * 4;
	while (R.acc >= dur && turnos < (red_rapido ? 64 : 4) && (!red_turno_max || R.T < red_turno_max)) {
		if (R.sub_hasta < R.T + RED_RETRASO) { R.sub_hasta = R.T + RED_RETRASO; enviar_mi_orden(R.sub_hasta); if (R.anf) anfitrion_cerrar(); }
		if (R.td[R.T % RING].turno != R.T) {   /* el turno aún no está cerrado: se espera */
			if (R.acc > dur * 3) R.acc = dur * 3;
			if (!R.t_estancado) R.t_estancado = t | 1;
			break;
		}
		R.t_estancado = 0; R.t_ultimo_turno = t;
		ticks_hechos += ejecutar_turno(R.T);
		R.acc -= dur; ++turnos;
		dur = (float)R.ticks / 30.0f;
		if (red_rapido) R.acc = dur * 4;
	}
	if (turnos == 4 && !red_rapido && R.acc > dur * 4) R.acc = 0;
	return ticks_hechos;
}

void red_salir_partida(void) {
	if (R.estado == RED_JUEGO || R.estado == RED_ARRANCANDO) AOEN_LOG("[AOEN] red: sale de la partida (turno %d)", R.T);
	red_cerrar();
}

/* si se pierde al anfitrión, la partida sigue sola: los demás jugadores pasan a la máquina */
static void seguir_solo(const char *motivo) {
	AOEN_LOG("[AOEN] red: %s; la partida sigue sola contra la máquina", motivo);
	for (int p = 0; p < R.N; ++p) if (p != R.mi_jug && W.pl[p].active) { W.pl[p].ai = 1; W.pl[p].diff = CLAMP(W.pl[p].diff, 1, 3); }
	world_msg("Se perdió al anfitrión: sigues contra la máquina");
	int n = R.N;
	for (int k = 0; k < NENL; ++k) if (R.enl[k].usado) enl_liberar(&R.enl[k]);
	liberar_partida();
	ord_red = 0; ord_reiniciar();
	R.estado = RED_NADA; R.N = n;
	if (sock >= 0) { close(sock); sock = -1; }
}

/* ---------- mensajes del canal ---------- */
static Enlace *enlace_de_plaza(int p) { for (int k = 0; k < NENL; ++k) if (R.enl[k].usado && R.enl[k].plaza == p) return &R.enl[k]; return NULL; }

static void procesar_msg(Enlace *e, const u8 *m, int n) {
	if (n < 1) return;
	Lee l = { m, n, 1, 0 };
	switch (m[0]) {
	case M_SALA:
		if (!R.anf && (R.estado == RED_SALA_CLI || R.estado == RED_UNIENDO)) {
			Plaza pl[MAX_PLAYERS]; Setup cfg; int pd, N;
			if (leer_sala(m, n, &cfg, pl, &pd, &N)) { red_cfg = cfg; memcpy(red_plaza, pl, sizeof pl); red_mi_plaza = pd; }
		}
		break;
	case M_ELEGIR:
		if (R.anf && R.estado == RED_SALA_ANF) {
			int civ = (int)r8(&l), col = (int)r8(&l), eq = (int)r8(&l), listo = (int)r8(&l);
			Plaza *p = &red_plaza[e->plaza];
			if (!l.mal && civ < NUM_CIVS && col >= 1 && col <= 8 && eq <= 4) {
				p->civ = civ; p->equipo = eq; p->listo = listo ? 1 : 0;
				for (int q = 0; q < MAX_PLAYERS; ++q) if (q != e->plaza && red_plaza[q].tipo != PL_LIBRE && red_plaza[q].tipo != PL_CERRADA && red_plaza[q].color == col) { col = 0; break; }
				if (col) p->color = col;
				red_sala_cambiada();
			}
		}
		break;
	case M_CHAT:
		if (R.anf) { char t[96]; int k = MIN(n - 1, 95); memcpy(t, m + 1, (size_t)k); t[k] = 0; difundir_chat(e->plaza, t); }
		break;
	case M_CHATDE:
		if (!R.anf && n > 2) { char t[96]; int k = MIN(n - 2, 95); memcpy(t, m + 2, (size_t)k); t[k] = 0; anotar_chat((int)m[1] - 1, t); if (R.estado == RED_JUEGO) world_chat((int)m[1] - 1, t); }
		break;
	case M_EMPEZAR:
		if (!R.anf && R.estado == RED_SALA_CLI) {
			Plaza pl[MAX_PLAYERS]; Setup cfg; int mi, N;
			if (!leer_sala(m, n, &cfg, pl, &mi, &N) || N < 2 || N > MAX_PLAYERS || mi >= N) { error("Mensaje de arranque dañado"); R.estado = RED_ERROR; break; }
			red_cfg = cfg; R.estado = RED_ARRANCANDO; R.t_ini_arranque = ahora();
			memcpy(red_plaza, pl, sizeof pl);
			if (!montar_mundo(&cfg, pl, N, mi)) { R.estado = RED_ERROR; break; }
			u8 b[1] = { M_LISTO };
			enl_enviar(e, b, 1);
		}
		break;
	case M_LISTO:
		if (R.anf && R.estado == RED_ARRANCANDO) {
			e->listo = 1;
			int todos = 1;
			for (int k = 0; k < NENL; ++k) if (R.enl[k].usado && !R.enl[k].listo) todos = 0;
			if (todos) {
				u8 b[1] = { M_COMIENZA };
				for (int k = 0; k < NENL; ++k) if (R.enl[k].usado) enl_enviar(&R.enl[k], b, 1);
				comenzar();
			}
		}
		break;
	case M_COMIENZA: if (!R.anf && R.estado == RED_ARRANCANDO) comenzar(); break;
	case M_ORD:
		if (R.anf && R.estado == RED_JUEGO && e->plaza > 0 && e->plaza < R.N) {
			int t = (int)r32(&l), len = n - 5;
			if (!l.mal && len >= 0 && len <= MSG_MAX && t >= R.cerrado && t < R.cerrado + RING / 2 && R.activo[e->plaza]) {
				OrdP *o = &R.ordp[(t % RING) * MAX_PLAYERS + e->plaza];
				o->turno = t; o->len = len; memcpy(o->dat, m + 5, (size_t)len);
				anfitrion_cerrar();
			}
		}
		break;
	case M_TURNO:
		if (!R.anf && (R.estado == RED_JUEGO || R.estado == RED_ARRANCANDO)) {
			int t = (int)r32(&l), q = (int)r8(&l), partes = (int)r8(&l), len = n - l.i;
			if (l.mal || len < 0) break;
			if (q == 0) { R.asm_turno = t; R.asm_len = 0; }
			if (t != R.asm_turno || R.asm_len + len > TURNO_MAX) break;
			memcpy(R.asm_dat + R.asm_len, m + l.i, (size_t)len); R.asm_len += len;
			if (q == partes - 1) {
				TurnoD *d = &R.td[t % RING];
				d->turno = t; d->len = R.asm_len; memcpy(d->dat, R.asm_dat, (size_t)R.asm_len);
			}
		}
		break;
	case M_SUMA:
		if (R.anf && R.estado == RED_JUEGO && e->plaza > 0 && e->plaza < MAX_PLAYERS) {
			int t = (int)r32(&l);
			SumaT *c = suma_cli(e->plaza, t);
			c->turno = t; c->total = r32(&l);
			for (int k = 0; k < 10; ++k) c->g[k] = r32(&l);
			if (!l.mal) comprobar_suma(t);
		}
		break;
	case M_DESYNC:
		if (!R.anf && R.estado == RED_JUEGO) {
			int t = (int)r32(&l); unsigned total = r32(&l), g[10];
			for (int k = 0; k < 10; ++k) g[k] = r32(&l);
			if (l.mal) break;
			SumaT *a = &R.s_anf[t % RING];
			desincronizacion(t, 0, g, a->turno == t ? a->g : g);
			(void)total;
		}
		break;
	case M_SALIR:
		if (R.anf) {
			if (R.estado == RED_SALA_ANF) quitar_enlace(e, "salió");
			else if (R.estado == RED_JUEGO || R.estado == RED_ARRANCANDO) soltar_jugador(e->plaza, "salió");
		} else {
			error("El anfitrión cerró la sala");
			if (R.estado == RED_JUEGO) seguir_solo("el anfitrión sale"); else R.estado = RED_ERROR;
		}
		break;
	case M_KICK:
		if (!R.anf) { error("El anfitrión te ha expulsado de la sala"); R.estado = RED_ERROR; }
		break;
	default: break;
	}
}

/* ---------- datagramas recibidos ---------- */
static void dg_recibido(RDir de, const u8 *b, int n) {
	if (n < 3 || b[0] != 'A') return;
	int tipo = b[2];
	switch (tipo) {
	case DG_BUSCAR: if (R.anf && R.estado == RED_SALA_ANF && b[1] == RED_PROTO) anunciar_a(de); break;
	case DG_ANUNCIO: {
		if (b[1] != RED_PROTO || n < 3 + 16 + 5) break;
		Lee l = { b, n, 3, 0 };
		RPartida p; memset(&p, 0, sizeof p);
		rb(&l, p.nombre, 16); p.nombre[15] = 0; p.jugadores = (int)r8(&l); p.humanos = (int)r8(&l); p.mapa = (int)r8(&l); p.tam = (int)r16(&l);
		p.dir = de; p.visto = ahora();
		for (int k = 0; k < R.n_part; ++k) if (misma_dir(R.partidas[k].dir, de)) { R.partidas[k] = p; return; }
		if (R.n_part < 16) R.partidas[R.n_part++] = p;
		break;
	}
	case DG_UNIR:
		if (!R.anf) break;
		{
			Lee l = { b, n, 3, 0 };
			char nom[16]; rb(&l, nom, 16); nom[15] = 0; unsigned token = r32(&l);
			if (l.mal) break;
			u8 o[16]; Esc w = { o, 0, sizeof o, 0 };
			if (b[1] != RED_PROTO || R.estado != RED_SALA_ANF) { w8(&w, 'A'); w8(&w, RED_PROTO); w8(&w, DG_RECHAZO); w32(&w, token); w8(&w, b[1] != RED_PROTO ? 2u : 3u); enviar_dg(de, o, w.n); break; }
			Enlace *e = NULL; int nuevo = 0;
			for (int k = 0; k < NENL; ++k) if (R.enl[k].usado && R.enl[k].token == token && misma_dir(R.enl[k].dir, de)) e = &R.enl[k];
			if (!e) {
				int p = -1;
				for (int q = 1; q < MAX_PLAYERS; ++q) if (red_plaza[q].tipo == PL_LIBRE) { p = q; break; }
				if (p < 0) { w8(&w, 'A'); w8(&w, RED_PROTO); w8(&w, DG_RECHAZO); w32(&w, token); w8(&w, 1); enviar_dg(de, o, w.n); break; }
				e = enl_nuevo(de, 1 + (azar() % 60000), token, nom, p);
				if (!e) break;
				red_plaza[p].tipo = PL_HUMANO; red_plaza[p].listo = 0; copiar_nombre(red_plaza[p].nombre, nom);
				nuevo = 1;
			}
			w8(&w, 'A'); w8(&w, RED_PROTO); w8(&w, DG_UNIDO); w32(&w, token); w16(&w, e->idcon); w8(&w, (unsigned)e->plaza);
			enviar_dg(de, o, w.n);
			if (nuevo) {
				char t[96]; snprintf(t, sizeof t, "%s se une a la sala", e->nombre);
				AOEN_LOG("[AOEN] red: %s entra en la plaza %d", e->nombre, e->plaza + 1);
				difundir_chat(-1, t);
				red_sala_cambiada();
			}
		}
		break;
	case DG_UNIDO:
		if (R.estado == RED_UNIENDO && b[1] == RED_PROTO && n >= 3 + 4 + 2 + 1) {
			Lee l = { b, n, 3, 0 };
			unsigned token = r32(&l), idcon = r16(&l); int plaza = (int)r8(&l);
			if (token != R.enl[0].token || !misma_dir(R.enl[0].dir, de)) break;
			Enlace *e = enl_nuevo(de, idcon, token, "anfitrión", plaza);
			if (!e) break;
			R.estado = RED_SALA_CLI; red_mi_plaza = plaza;
			AOEN_LOG("[AOEN] red: dentro de la sala, plaza %d", plaza + 1);
		}
		break;
	case DG_RECHAZO:
		if (R.estado == RED_UNIENDO && n >= 3 + 4 + 1) {
			Lee l = { b, n, 3, 0 };
			unsigned token = r32(&l), motivo = r8(&l);
			if (token != R.enl[0].token) break;
			error(motivo == 1 ? "La sala está llena" : motivo == 2 ? "El anfitrión usa otra versión de AoEN" : "La partida ya ha empezado");
			R.estado = RED_ERROR;
		}
		break;
	case DG_DATOS:
		if (b[1] != RED_PROTO || n < 5) break;
		{
			unsigned idcon = (unsigned)b[3] | (unsigned)b[4] << 8;
			for (int k = 0; k < NENL; ++k) {
				Enlace *e = &R.enl[k];
				if (e->usado && e->idcon == idcon && misma_dir(e->dir, de)) { enl_recibir(e, b, n); break; }
			}
		}
		break;
	default: break;
	}
}

/* ---------- el pulso de la red ---------- */
void red_tick(void) {
	if (sock < 0) return;
	vaciar_retardados();
	for (int k = 0; k < 128; ++k) {
		u8 b[1500]; struct sockaddr_in a; socklen_t al = sizeof a;
		memset(&a, 0, sizeof a);
		int n = (int)recvfrom(sock, b, sizeof b, MSG_DONTWAIT, (struct sockaddr *)&a, &al);
		if (n <= 0) break;
		RDir de = { ntohl(a.sin_addr.s_addr), ntohs(a.sin_port) };
		dg_recibido(de, b, n);
	}
	unsigned t = ahora();
	for (int k = 0; k < NENL; ++k) if (R.enl[k].usado) enl_tick(&R.enl[k]);
	switch (R.estado) {
	case RED_UNIENDO:
		if (t - R.enl[0].ult_tx >= 300) {
			R.enl[0].ult_tx = t;
			u8 b[40]; Esc w = { b, 0, sizeof b, 0 };
			w8(&w, 'A'); w8(&w, RED_PROTO); w8(&w, DG_UNIR); { char nom[16] = { 0 }; copiar_nombre(nom, R.mi_nombre); wb(&w, nom, 16); } w32(&w, R.enl[0].token);
			enviar_dg(R.enl[0].dir, b, w.n);
		}
		if (t - R.enl[0].ult_rx > 5000) { error("El anfitrión no contesta: ¿está en la misma red y con la sala creada?"); R.estado = RED_ERROR; }
		break;
	case RED_SALA_CLI:
		if (R.enl[0].usado && t - R.enl[0].ult_rx > (unsigned)timeout_sala) { error("Se perdió la conexión con el anfitrión"); R.estado = RED_ERROR; }
		break;
	case RED_SALA_ANF:
		for (int k = 0; k < NENL; ++k) if (R.enl[k].usado && t - R.enl[k].ult_rx > (unsigned)timeout_sala) quitar_enlace(&R.enl[k], "sin respuesta");
		break;
	case RED_ARRANCANDO:
		if (t - R.t_ini_arranque > 180000) { error("Alguien tardó demasiado en preparar la partida"); R.estado = RED_ERROR; }
		break;
	case RED_JUEGO:
		if (R.anf) {
			for (int k = 0; k < NENL; ++k) if (R.enl[k].usado && t - R.enl[k].ult_rx > (unsigned)timeout_juego) soltar_jugador(R.enl[k].plaza, "sin respuesta");
			anfitrion_cerrar();
			/* turno adaptado a la latencia: la ventana de órdenes dura RED_RETRASO turnos */
			if (!fijo && t - R.t_adapt > 1000) {
				R.t_adapt = t;
				float peor = 0;
				for (int k = 0; k < NENL; ++k) if (R.enl[k].usado && R.enl[k].srtt > peor) peor = R.enl[k].srtt;
				int quiero = CLAMP((int)(peor * 0.7f / 33.3f + 0.99f), red_ticks_turno, 6);
				if (quiero > R.ticks) { escribir_ctrl(OP_TURNO, quiero); R.bajar_cuenta = 0; AOEN_LOG("[AOEN] red: turnos de %d tics (ida y vuelta %.0f ms)", quiero, peor); }
				else if (quiero < R.ticks && ++R.bajar_cuenta >= 5) { escribir_ctrl(OP_TURNO, quiero); R.bajar_cuenta = 0; }
				else if (quiero >= R.ticks) R.bajar_cuenta = 0;
			}
		} else if (R.enl[0].usado && t - R.enl[0].ult_rx > (unsigned)timeout_juego && !W.over) seguir_solo("el anfitrión no contesta");
		break;
	default: break;
	}
	(void)enlaces_vivos; (void)enlace_de_plaza;
}

/* ---------- órdenes al azar (pruebas) ---------- */
static unsigned az_s = 1;
static int rnd(int n) { az_s = az_s * 1103515245u + 12345u; return (int)((az_s >> 8) % (unsigned)n); }
void red_azar_semilla(unsigned s) { az_s = s ? s : 1; }

/* con probabilidad `pct`, una orden de un jugador de verdad: mover, entrenar, atacar, construir, investigar, diplomacia... */
void red_ordenes_azar(int pct) {
	if (rnd(100) >= pct || !W.nents) return;
	int mios[512], n = 0, tc = -1;
	for (int i = 0; i < W.nents && n < 512; ++i) {
		Entity *e = &W.ents[i];
		if (!e->alive || e->owner != W.human || e->state == ST_DYING) continue;
		if (is_unit(e->type)) mios[n++] = i;
		if (e->type == E_TOWN_CENTER) tc = i;
	}
	if (!n) return;
	int op = rnd(10);
	Entity *u = &W.ents[mios[rnd(n)]];
	float x = u->x + rnd(21) - 10, y = u->y + rnd(21) - 10;
	switch (op) {
	case 0: case 1: case 2: cmd_move(ent_ref((int)(u - W.ents)), x, y); break;
	case 3: if (tc >= 0) cmd_train(W.human, ent_ref(tc), E_VILLAGER); break;
	case 4: {   /* a por el enemigo más cercano */
		int mejor = -1; float bd = 1e9f;
		for (int i = 0; i < W.nents; ++i) {
			Entity *t = &W.ents[i];
			if (!t->alive || t->owner < 0 || t->owner == W.human || is_resource(t->type)) continue;
			float d = (t->x - u->x) * (t->x - u->x) + (t->y - u->y) * (t->y - u->y);
			if (d < bd) { bd = d; mejor = i; }
		}
		if (mejor >= 0) cmd_target(ent_ref((int)(u - W.ents)), ent_ref(mejor));
		break;
	}
	case 5: if (u->type == E_VILLAGER) { Ref t = ent_ref((int)(u - W.ents)); cmd_build_grupo(W.human, &t, 1, E_HOUSE, (int)x, (int)y); } break;
	case 6: cmd_move_queue(ent_ref((int)(u - W.ents)), x, y); break;
	case 7: if (tc >= 0) cmd_research(W.human, ent_ref(tc), T_TOOLWORKING); break;
	case 8: cmd_diplomacy(W.human, rnd(W.setup.players), rnd(3)); break;
	case 9: if (rnd(4) == 0) cmd_advance_age(W.human); else if (rnd(6) == 0) cmd_eliminar(ent_ref((int)(u - W.ents))); break;
	}
}
