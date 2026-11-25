/* SPDX-License-Identifier: GPL-3.0-or-later */
/* ui_selecao.c - selección (clic, recuadro, doble clic, grupos Ctrl+número) y órdenes del jugador (D417, fase 4).
 * Sigue frontend/gameplay.rs de ReTzar (GPL-3.0-or-later) en las reglas: el recuadro prefiere unidades propias,
 * el doble clic coge las de la misma clase que se ven en pantalla, el clic derecho pide la orden a
 * tz_mundo_ordem_padrao y con Mayús se encola. Lee el mundo solo por mundo.h (lectura de TzObj) y manda
 * comandos: nunca escribe en TzMundo.
 *
 * Nota: se recorre m->obj en vez de tz_mundo_area porque la interfaz necesita «cualquier dueño» y el contrato no
 * dice qué valor de «dono» lo significa. */
#include "ui.h"
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#define DOBLE_CLIQUE_MS 400

void tz_ui_sel_limpar(TzUiSel *s) { s->n = 0; }

void tz_ui_sel_vivos(const TzMundo *m, TzUiSel *s)
{
	int k = 0;
	for (int i = 0; i < s->n; ++i)
		if (s->id[i] >= 0 && s->id[i] < m->nobj && tz_obj_vivo(&m->obj[s->id[i]])) s->id[k++] = s->id[i];
	s->n = k;
}

static int pegada(const TzMundo *m, const TzObj *o, int *w, int *h)
{
	*w = *h = 1;
	if (!m->cat || o->classe >= TZ_NCLASSES) return 0;
	const TzAtributos *a = tz_catalogo_at(m->cat, o->classe, o->civ);
	if (a && a->pegada[0] && a->pegada[1]) { *w = a->pegada[0]; *h = a->pegada[1]; }
	return 1;
}

/* ¿el punto del mundo (píxeles) cae en el dibujo del objeto? Las unidades son altas: se dibujan sobre sus pies. */
static int toca(const TzMundo *m, const TzObj *o, int px, int py)
{
	if (!tz_obj_vivo(o) || (o->flags & TZ_F_HISTORIA && 0)) return 0;
	if (tz_obj_unidade(o)) {
		int x, y;
		tz_obj_pixel(o, &x, &y);
		return px >= x - 14 && px < x + 14 && py >= y - 34 && py < y + 8;
	}
	int w, h;
	pegada(m, o, &w, &h);
	int x0 = o->cx * TZ_CASILLA, y0 = o->cy * TZ_CASILLA;
	return px >= x0 && px < x0 + w * TZ_CASILLA && py >= y0 - (tz_obj_edificio(o) ? 16 : 0) && py < y0 + h * TZ_CASILLA;
}

TzId tz_ui_pick(const TzMundo *m, int px, int py)
{
	TzId mejor = TZ_NENHUM;
	int prio = -1;
	for (int i = 0; i < m->nobj; ++i) {
		const TzObj *o = &m->obj[i];
		if (!toca(m, o, px, py)) continue;
		int p = tz_obj_unidade(o) ? 3 : tz_obj_edificio(o) ? 2 : 1;
		if (p >= prio) { prio = p; mejor = i; }   /* a igual prioridad gana el de índice mayor (dibujado después) */
	}
	return mejor;
}

static int propio(const TzMundo *m, const TzObj *o, int jogador) { return tz_obj_vivo(o) && o->dono == jogador; }

static void emitir_selecao(TzMundo *m, const TzUiSel *s)
{
	if (!s->n) return;
	const TzObj *o = &m->obj[s->id[0]];
	TzEvento e = { 0 };
	e.tipo = TZ_EV_SELECIONADO; e.reino = (uint8_t)m->jogador; e.classe = o->classe; e.obj = s->id[0]; e.alvo = TZ_NENHUM;
	e.x = o->cx; e.y = o->cy;
	tz_mundo_evento(m, &e);
}

static void anadir(TzUiSel *s, TzId i)
{
	for (int k = 0; k < s->n; ++k) if (s->id[k] == i) return;
	if (s->n < TZ_SEL_MAX) s->id[s->n++] = i;
}

static void quitar(TzUiSel *s, TzId i)
{
	for (int k = 0; k < s->n; ++k)
		if (s->id[k] == i) { memmove(&s->id[k], &s->id[k + 1], (size_t)(s->n - k - 1) * sizeof s->id[0]); s->n--; return; }
}

int tz_ui_sel_clique(TzMundo *m, TzUiSel *s, int jogador, int px, int py, int somar, uint32_t ms, TzRect vista)
{
	TzId i = tz_ui_pick(m, px, py);
	if (i == TZ_NENHUM) {
		if (!somar) s->n = 0;
		s->clique_obj = TZ_NENHUM;
		return s->n;
	}
	const TzObj *o = &m->obj[i];
	int duplo = !somar && s->clique_obj == i && ms - s->clique_ms <= DOBLE_CLIQUE_MS;
	s->clique_obj = i; s->clique_ms = ms; s->clique_classe = o->classe;
	if (duplo && propio(m, o, jogador) && tz_obj_unidade(o)) {
		/* doble clic: todas las propias de esa clase que se ven en la ventana */
		if (!somar) s->n = 0;
		for (int k = 0; k < m->nobj; ++k) {
			const TzObj *q = &m->obj[k];
			if (!propio(m, q, jogador) || q->classe != o->classe || !tz_obj_unidade(q)) continue;
			int x, y;
			tz_obj_pixel(q, &x, &y);
			if (tz_rect_dentro(vista, x, y)) anadir(s, k);
		}
	} else if (somar && propio(m, o, jogador)) {
		int ya = 0;
		for (int k = 0; k < s->n; ++k) ya |= s->id[k] == i;
		if (ya) quitar(s, i);
		else {
			/* no se mezclan lo propio con lo ajeno ni edificios con unidades */
			if (s->n && (!propio(m, &m->obj[s->id[0]], jogador) || tz_obj_unidade(&m->obj[s->id[0]]) != tz_obj_unidade(o))) s->n = 0;
			anadir(s, i);
		}
	} else {
		s->n = 0;
		anadir(s, i);
	}
	emitir_selecao(m, s);
	return s->n;
}

int tz_ui_sel_caixa(TzMundo *m, TzUiSel *s, int jogador, int x0, int y0, int x1, int y1, int somar)
{
	if (x0 > x1) { int t = x0; x0 = x1; x1 = t; }
	if (y0 > y1) { int t = y0; y0 = y1; y1 = t; }
	TzId unid[TZ_SEL_MAX], edif = TZ_NENHUM, ajeno = TZ_NENHUM;
	int nu = 0;
	for (int i = 0; i < m->nobj; ++i) {
		const TzObj *o = &m->obj[i];
		if (!tz_obj_vivo(o)) continue;
		int x, y;
		if (tz_obj_unidade(o)) {
			tz_obj_pixel(o, &x, &y);
			if (x < x0 || x > x1 || y < y0 - 8 || y > y1 + 24) continue;   /* los pies cuentan, la cabeza algo menos */
		} else {
			int w, h;
			pegada(m, o, &w, &h);
			int ox = o->cx * TZ_CASILLA, oy = o->cy * TZ_CASILLA;
			if (ox + w * TZ_CASILLA <= x0 || ox >= x1 || oy + h * TZ_CASILLA <= y0 || oy >= y1) continue;
		}
		if (propio(m, o, jogador)) {
			if (tz_obj_unidade(o)) { if (nu < TZ_SEL_MAX) unid[nu++] = i; }
			else if (tz_obj_edificio(o) && edif == TZ_NENHUM) edif = i;
		} else if (ajeno == TZ_NENHUM && (tz_obj_unidade(o) || tz_obj_edificio(o))) ajeno = i;
	}
	if (!somar) s->n = 0;
	if (nu) {
		if (somar && s->n && !tz_obj_unidade(&m->obj[s->id[0]])) s->n = 0;
		for (int k = 0; k < nu; ++k) anadir(s, unid[k]);
	} else if (edif != TZ_NENHUM) { if (somar) s->n = 0; anadir(s, edif); }
	else if (ajeno != TZ_NENHUM) { s->n = 0; anadir(s, ajeno); }
	emitir_selecao(m, s);
	return s->n;
}

void tz_ui_sel_grupo_guardar(TzUiSel *s, int g)
{
	if (g < 0 || g > 9) return;
	memcpy(s->grupo[g], s->id, (size_t)s->n * sizeof s->id[0]);
	s->ngrupo[g] = s->n;
}

int tz_ui_sel_grupo_chamar(TzMundo *m, TzUiSel *s, int g, uint32_t ms, int *duplo)
{
	*duplo = 0;
	if (g < 0 || g > 9) return 0;
	*duplo = s->grupo_ult == g && ms - s->grupo_ms <= DOBLE_CLIQUE_MS * 2;
	s->grupo_ult = g; s->grupo_ms = ms;
	int k = 0;
	for (int i = 0; i < s->ngrupo[g]; ++i) {
		TzId id = s->grupo[g][i];
		if (id >= 0 && id < m->nobj && tz_obj_vivo(&m->obj[id])) s->grupo[g][k++] = id;
	}
	s->ngrupo[g] = k;
	if (!k) return 0;
	memcpy(s->id, s->grupo[g], (size_t)k * sizeof s->id[0]);
	s->n = k;
	emitir_selecao(m, s);
	return k;
}

/* ---------------------------------------------------------------- órdenes */

void tz_ui_aviso(TzUi *u, const char *fmt, ...)
{
	va_list a;
	va_start(a, fmt);
	vsnprintf(u->aviso, sizeof u->aviso, fmt, a);
	va_end(a);
	u->aviso_ate = u->agora_ms + 4000;
}

/* unidades de la selección que obedecen al jugador */
static int mandables(const TzUi *u, TzId *out, int max, int *so_edificios)
{
	const TzMundo *m = u->m;
	int n = 0, un = 0;
	for (int i = 0; i < u->sel.n && n < max; ++i) {
		const TzObj *o = &m->obj[u->sel.id[i]];
		if (!tz_obj_vivo(o) || o->dono != m->jogador || (o->flags & TZ_F_BLOQUEADO)) continue;
		out[n++] = u->sel.id[i];
		un += tz_obj_unidade(o);
	}
	if (so_edificios) *so_edificios = n && !un;
	return n;
}

static int mandar(TzUi *u, TzComando *c, const TzId *ids, int n)
{
	int enviados = 0;
	for (int i = 0; i < n; i += TZ_CMD_IDS) {
		c->nids = n - i < TZ_CMD_IDS ? n - i : TZ_CMD_IDS;
		memcpy(c->ids, ids + i, (size_t)c->nids * sizeof c->ids[0]);
		if (tz_mundo_comando(u->m, c) == 0) { enviados++; u->n_comandos++; }
	}
	return enviados;
}

/* el acuse visual de una orden con punto en el mapa: la cruz «cursor» y, si es de moverse, la bandera «wp_flag» (orders.rs order_feedback) */
static void ordem_ack(TzUi *u, const TzOrdem *o)
{
	if (o->tipo == TZ_O_NADA || o->tipo == TZ_O_PARAR) return;
	u->fb.on = 1; u->fb.ms = u->agora_ms;
	u->fb.bandeira = o->tipo == TZ_O_MOVER || o->tipo == TZ_O_ATACAR_CHAO || o->tipo == TZ_O_CORTAR;
	if (o->alvo >= 0 && o->alvo < u->m->nobj) { u->fb.px = u->m->obj[o->alvo].cx * TZ_CASILLA + TZ_CASILLA / 2; u->fb.py = u->m->obj[o->alvo].cy * TZ_CASILLA + TZ_CASILLA / 2; }
	else { u->fb.px = o->x * TZ_CASILLA + TZ_CASILLA / 2; u->fb.py = o->y * TZ_CASILLA + TZ_CASILLA / 2; }
}

int tz_ui_ordem_direita(TzUi *u, int px, int py, int enfileirar)
{
	TzId ids[TZ_SEL_MAX];
	int so_edif;
	int n = mandables(u, ids, TZ_SEL_MAX, &so_edif);
	if (!n) return 0;
	int cx = px / TZ_CASILLA, cy = py / TZ_CASILLA;
	if (px < 0 || py < 0 || cx >= u->m->lado || cy >= u->m->lado) return 0;
	TzComando c;
	memset(&c, 0, sizeof c);
	c.reino = (uint8_t)u->m->jogador;
	c.x = (int16_t)cx; c.y = (int16_t)cy;
	if (so_edif) {   /* los edificios solo fijan el punto de reunión */
		c.tipo = TZ_C_REUNIAO;
		return mandar(u, &c, ids, n);
	}
	TzId alvo = tz_ui_pick(u->m, px, py);
	TzOrdem o = tz_mundo_ordem_padrao(u->m, u->m->jogador, ids, n, cx, cy, alvo);
	if (o.tipo == TZ_O_NADA) return 0;
	c.tipo = TZ_C_ORDEM; c.ordem = o;
	c.flags = enfileirar ? TZ_CMD_FILA : 0;
	ordem_ack(u, &o);
	return mandar(u, &c, ids, n);
}

int tz_ui_ordem_alvo(TzUi *u, int px, int py, int enfileirar)
{
	TzId ids[TZ_SEL_MAX];
	int n = mandables(u, ids, TZ_SEL_MAX, NULL);
	if (!n || u->tipo_alvo == TZ_O_NADA) return 0;
	int cx = px / TZ_CASILLA, cy = py / TZ_CASILLA;
	if (px < 0 || py < 0 || cx >= u->m->lado || cy >= u->m->lado) return 0;
	TzOrdem o;
	memset(&o, 0, sizeof o);
	o.tipo = (uint8_t)u->tipo_alvo; o.alvo = TZ_NENHUM; o.x = (int16_t)cx; o.y = (int16_t)cy;
	o.arg = (int16_t)u->ordem_idx;
	TzId alvo = tz_ui_pick(u->m, px, py);
	/* las órdenes sobre un objeto (atacar, seguir, capturar...) llevan el objeto; sobre el suelo, la casilla */
	switch (o.tipo) {
	case TZ_O_ATACAR: case TZ_O_SEGUIR: case TZ_O_RECOLHER: case TZ_O_CONSTRUIR: case TZ_O_REPARAR: case TZ_O_CAPTURAR:
	case TZ_O_GUARDAR: case TZ_O_ENTRAR: case TZ_O_COMERCIAR: case TZ_O_PEGAR_ITEM:
		if (alvo == TZ_NENHUM) { if (o.tipo == TZ_O_ATACAR) o.tipo = TZ_O_ATACAR_CHAO; else return 0; }
		else o.alvo = alvo;
		break;
	case TZ_O_HECHIZO: case TZ_O_USAR_ITEM: o.alvo = alvo; break;
	case TZ_O_CORTAR: break;
	default: break;
	}
	if (o.tipo == TZ_O_PATRULHAR) {   /* patrulla de donde está la primera unidad al punto elegido */
		const TzObj *p = &u->m->obj[ids[0]];
		o.x2 = o.x; o.y2 = o.y; o.x = p->cx; o.y = p->cy;
	}
	TzComando c;
	memset(&c, 0, sizeof c);
	c.tipo = TZ_C_ORDEM; c.reino = (uint8_t)u->m->jogador; c.ordem = o;
	c.flags = enfileirar ? TZ_CMD_FILA : 0;
	ordem_ack(u, &o);
	return mandar(u, &c, ids, n);
}
