/* SPDX-License-Identifier: GPL-3.0-or-later */
/* AoEN, copyright de su autor (el mantenedor de SaguiOS-Extended); GPL v3 o posterior. Port a SaguiOS: D382. */
/* Órdenes de los jugadores en el multijugador (D461): cola local, formato de red y aplicación. Ver ordenes.h. */
#include "ordenes.h"

int ord_red, ord_ctx;

#define COLA_MAX 768
static OrdRed cola[COLA_MAX];
static int cola_n, cola_perdidas;

void ord_reiniciar(void) { cola_n = 0; cola_perdidas = 0; }
int ord_pendientes(void) { return cola_n; }

void ord_poner(const OrdRed *o) {
	if (cola_n >= COLA_MAX) { if (!cola_perdidas++) AOEN_LOG("[AOEN] red: cola de órdenes llena, se descarta la orden"); return; }
	cola[cola_n++] = *o;
}

/* cuántas referencias a entidades lleva cada orden (las de grupo llevan las que se apuntaron) */
static int refs_de(int op) {
	switch (op) {
	case OP_MOVE: case OP_MOVEQ: case OP_PATRULLA: case OP_STANCE: case OP_COMERCIO: case OP_ELIMINAR: case OP_TRAIN: case OP_CANCELA: case OP_RESEARCH: return 1;
	case OP_TARGET: case OP_GUARD: case OP_RALLY: return 2;
	default: return 0;
	}
}

int ord_cap(int op, int a, int b, int c, float x, float y, Ref r0, Ref r1) {
	if (!ord_red || ord_ctx > 0) return 0;
	OrdRed o; memset(&o, 0, sizeof o);
	o.op = (u8)op; o.jug = (u8)(W.human & 255);
	o.a = a; o.b = b; o.c = c; o.x = x; o.y = y;
	o.nref = (u16)refs_de(op);
	if (o.nref > 0) o.ref[0] = r0;
	if (o.nref > 1) o.ref[1] = r1;
	ord_poner(&o);
	return 1;
}

int ord_cap_grupo(int op, const Ref *refs, int n, int a, int b, int c) {
	if (!ord_red || ord_ctx > 0) return 0;
	OrdRed o; memset(&o, 0, sizeof o);
	o.op = (u8)op; o.jug = (u8)(W.human & 255); o.a = a; o.b = b; o.c = c;
	o.nref = (u16)MIN(n, ORD_REFS);
	for (int k = 0; k < o.nref; ++k) o.ref[k] = refs[k];
	ord_poner(&o);
	return 1;
}

/* ---------- formato de red: little endian, sin relleno ----------
 * op(1) jug(1) nref(1) máscara(1: bits de a, b, c, x, y que no son cero) refs(8 cada una: idx, gen) campos(4 cada uno) */
static void p32(u8 *b, unsigned v) { b[0] = (u8)v; b[1] = (u8)(v >> 8); b[2] = (u8)(v >> 16); b[3] = (u8)(v >> 24); }
static unsigned g32(const u8 *b) { return (unsigned)b[0] | (unsigned)b[1] << 8 | (unsigned)b[2] << 16 | (unsigned)b[3] << 24; }
static unsigned fbits(float f) { unsigned v; memcpy(&v, &f, 4); return v; }
static float bitsf(unsigned v) { float f; memcpy(&f, &v, 4); return f; }

int ord_codificar(const OrdRed *o, u8 *buf, int max) {
	unsigned campos[5] = { (unsigned)o->a, (unsigned)o->b, (unsigned)o->c, fbits(o->x), fbits(o->y) };
	unsigned mask = 0;
	int n = 4 + 8 * o->nref;
	for (int k = 0; k < 5; ++k) if (campos[k]) { mask |= 1u << k; n += 4; }
	if (n > max) return -1;
	buf[0] = o->op; buf[1] = o->jug; buf[2] = (u8)o->nref; buf[3] = (u8)mask;
	int p = 4;
	for (int k = 0; k < o->nref; ++k) { p32(buf + p, (unsigned)o->ref[k].idx); p32(buf + p + 4, (unsigned)o->ref[k].gen); p += 8; }
	for (int k = 0; k < 5; ++k) if (mask & (1u << k)) { p32(buf + p, campos[k]); p += 4; }
	return p;
}

int ord_decodificar(const u8 *buf, int n, OrdRed *o) {
	if (n < 4) return -1;
	memset(o, 0, sizeof *o);
	o->op = buf[0]; o->jug = buf[1]; o->nref = buf[2];
	unsigned mask = buf[3];
	if (o->nref > ORD_REFS || mask > 31) return -1;
	int p = 4;
	if (n < p + 8 * o->nref) return -1;
	for (int k = 0; k < o->nref; ++k) { o->ref[k].idx = (int)g32(buf + p); o->ref[k].gen = (int)g32(buf + p + 4); p += 8; }
	unsigned campos[5] = { 0, 0, 0, 0, 0 };
	for (int k = 0; k < 5; ++k) if (mask & (1u << k)) { if (n < p + 4) return -1; campos[k] = g32(buf + p); p += 4; }
	o->a = (int)campos[0]; o->b = (int)campos[1]; o->c = (int)campos[2]; o->x = bitsf(campos[3]); o->y = bitsf(campos[4]);
	return p;
}

int ord_sacar(u8 *buf, int max) {
	int p = 0, k = 0;
	for (; k < cola_n; ++k) {
		int r = ord_codificar(&cola[k], buf + p, max - p);
		if (r < 0) break;   /* no cabe: queda para el turno siguiente */
		p += r;
	}
	if (k < cola_n) memmove(cola, cola + k, sizeof(OrdRed) * (size_t)(cola_n - k));
	cola_n -= k;
	return p;
}

/* ---------- aplicación ---------- */
static int dueno(Ref r, int jug) { Entity *e = ent_get(r); return e && e->owner == jug; }

static void aplicar(const OrdRed *o) {
	int j = o->jug;
	if (j == 255) {   /* bloque de control del anfitrión: soltar a un jugador y cambiar la duración del turno */
		if (o->op == OP_SOLTAR || o->op == OP_TURNO) red_control(o->op, j, o->a, o->b);
		return;
	}
	if (j < 0 || j >= MAX_PLAYERS || !W.pl[j].active) return;
	/* una civilización que ya lleva la IA no recibe órdenes de nadie (un jugador soltado) */
	if (o->op < OP_PAUSA && W.pl[j].ai) return;
	const Ref r0 = o->nref > 0 ? o->ref[0] : (Ref){ -1, 0 }, r1 = o->nref > 1 ? o->ref[1] : (Ref){ -1, 0 };
	switch (o->op) {
	case OP_MOVE: if (dueno(r0, j)) cmd_move(r0, o->x, o->y); break;
	case OP_MOVEQ: if (dueno(r0, j)) cmd_move_queue(r0, o->x, o->y); break;
	case OP_PATRULLA: if (dueno(r0, j)) cmd_patrol(r0, o->x, o->y); break;
	case OP_TARGET: if (dueno(r0, j)) cmd_target(r0, r1); break;
	case OP_STANCE: if (dueno(r0, j)) cmd_set_stance(r0, o->a); break;
	case OP_GUARD: if (dueno(r0, j)) cmd_guard(r0, r1); break;
	case OP_RALLY: if (dueno(r0, j)) cmd_set_rally(r0, o->x, o->y, r1); break;
	case OP_COMERCIO: if (dueno(r0, j)) cmd_comercio(r0, o->a, o->b, o->c); break;
	case OP_ELIMINAR: if (dueno(r0, j)) cmd_eliminar(r0); break;
	case OP_TRAIN: if (dueno(r0, j)) cmd_train(j, r0, (EntType)o->a); break;
	case OP_CANCELA: if (dueno(r0, j)) cmd_cancel_train(j, r0, o->a); break;
	case OP_RESEARCH: if (dueno(r0, j)) cmd_research(j, r0, o->a); break;
	case OP_AGE: cmd_advance_age(j); break;
	case OP_TRADE: cmd_trade(j, o->a, o->b); break;
	case OP_DIPL: cmd_diplomacy(j, o->a, o->b); break;
	case OP_TRIBUTO: cmd_tribute(j, o->a, o->b, o->c); break;
	case OP_PEDIR: cmd_pedir(j, o->a, o->b, o->c); break;
	case OP_BUILD: {
		Ref trab[ORD_REFS]; int n = 0;
		for (int k = 0; k < o->nref; ++k) if (dueno(o->ref[k], j)) trab[n++] = o->ref[k];
		if (n) cmd_build_grupo(j, trab, n, (EntType)o->a, o->b, o->c);
		break;
	}
	case OP_PAUSA: case OP_VELOCIDAD: case OP_SOLTAR: case OP_TURNO: red_control(o->op, j, o->a, o->b); break;
	default: break;
	}
}

int ord_aplicar_bloque(int jug, const u8 *buf, int n) {
	int p = 0, hechas = 0;
	++ord_ctx;
	while (p < n) {
		OrdRed o;
		int r = ord_decodificar(buf + p, n - p, &o);
		if (r < 0) { AOEN_LOG("[AOEN] red: bloque de órdenes mal formado (jugador %d)", jug); break; }
		p += r;
		o.jug = (u8)jug;   /* manda el jugador al que pertenece el bloque, no lo que diga la orden */
		aplicar(&o);
		++hechas;
	}
	--ord_ctx;
	return hechas;
}
