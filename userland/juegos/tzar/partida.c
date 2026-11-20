/* SPDX-License-Identifier: GPL-3.0-or-later */
/* partida.c - una partida jugable en el visor «tzar» (D417, fase 5): el mundo de la simulación (mundo.h) dibujado
 * con la escena y la animación de la fase 3, con selección y órdenes mínimas con el ratón y el teclado para probar.
 * La interfaz original (barra, botones, menús) es de la fase 4; esto es solo lo necesario para jugar y depurar.
 * Código propio (no sale de ReTzar), sin SDL: el visor le pasa el ratón y las teclas.
 *
 * Controles (con la partida en marcha): clic izquierdo o arrastre = seleccionar (unidades propias; un clic, también
 * un edificio u otro objeto); clic derecho = orden por defecto (andar, atacar, recoger, talar, construir, reparar,
 * entrar); Mayús + clic derecho = encolar; 1-9 = con un edificio, entrenar o investigar la opción n de su menú;
 * con peones, elegir el edificio n de su menú y colocarlo con el siguiente clic izquierdo; Supr = parar;
 * Espacio = pausa; + y - = velocidad; F5 = guardar en tzar.sav, F9 = cargar.
 */
#include "partida.h"
#include <stdlib.h>
#include <string.h>

int tz_partida_iniciar(TzPartida *p, TzRecursos *r, TzBiblioteca *b, TzMapa *m, uint32_t semente)
{
	memset(p, 0, sizeof *p);
	p->mapa = m;
	if (tz_catalogo_carregar(&p->cat, r, b)) return -1;
	TzRegras rg;
	memset(&rg, 0, sizeof rg);
	rg.populacao = 200;
	if (tz_mundo_criar(&p->m, &p->cat, m, &rg, semente)) { tz_catalogo_liberar(&p->cat); return -1; }
	p->ativa = 1;
	p->velocidade = 10;
	p->colocar = -1;
	return 0;
}

void tz_partida_liberar(TzPartida *p)
{
	if (!p->ativa) return;
	tz_mundo_liberar(&p->m);
	tz_catalogo_liberar(&p->cat);
	free(p->vis);
	memset(p, 0, sizeof *p);
}

/* avanza los tics que tocan en dt ms (a la velocidad elegida; 10 = normal) */
int tz_partida_avancar(TzPartida *p, int dt_ms)
{
	if (!p->ativa || p->pausa) return 0;
	p->acum += dt_ms * p->velocidade;
	int n = 0;
	while (p->acum >= TZ_TIC_MS * 10 && n < 8) { tz_mundo_tic(&p->m); p->acum -= TZ_TIC_MS * 10; ++n; }
	if (n == 8) p->acum = 0;
	/* el mapa de la escena sigue a los árboles talados */
	if (n) memcpy(p->mapa->casillas, p->m.casillas, (size_t)p->m.lado * p->m.lado * sizeof *p->mapa->casillas);
	return n;
}

static int dinamico(const TzObj *o)
{
	return tz_obj_unidade(o) || tz_obj_edificio(o) || o->classe == TZC_RES_GOLD || o->classe == TZC_RES_STONE || o->classe == 80;
}

int tz_partida_esconder(const TzPartida *p, int obj_mapa)
{
	if (!p->ativa || obj_mapa < 0 || obj_mapa >= p->m.nobj) return 0;
	return dinamico(&p->m.obj[obj_mapa]);
}

typedef struct { int grupo, prio; float prof; int obj; } Vis;

static int cmp_vis(const void *a, const void *b)
{
	const Vis *x = a, *y = b;
	if (x->grupo != y->grupo) return x->grupo - y->grupo;
	if (x->prio != y->prio) return x->prio - y->prio;
	if (x->prof != y->prof) return x->prof < y->prof ? -1 : 1;
	return x->obj - y->obj;
}

static int prio(const TzObj *o)
{
	if (o->flags & TZ_F_MORTO) return 1;
	if (o->classe == TZC_FIELD) return 50;
	if (o->classe == 15 || o->classe == 22 || o->classe == 32) return 300;
	return 100;
}

static int selecionado(const TzPartida *p, TzId i)
{
	for (int k = 0; k < p->nsel; ++k) if (p->sel[k] == i) return 1;
	return 0;
}

/* El mundo vivo en el lienzo: los visuales fijos de la escena (árboles, decoración, acantilados) mezclados en el orden del
 * pintor con los objetos de la simulación (tz_dibujar_objeto en su pose), los proyectiles y la niebla del jugador. El
 * terreno lo pone antes quien llama. Lo usan el visor (partida.c) y la interfaz del juego (ui_mapa.c). */
static Vis *vis_buf; static int vis_cap;

int tz_desenhar_mundo(TzMundo *m, TzEscena *e, TzLienzo *l, int camx, int camy, uint64_t ms)
{
	int falhas = 0;
	int n = 0;
	if (vis_cap < m->nobj) { free(vis_buf); vis_buf = malloc((size_t)m->nobj * sizeof(Vis)); vis_cap = vis_buf ? m->nobj : 0; }
	Vis *vis = vis_buf;
	if (!vis) return -1;
	for (int i = 0; i < m->nobj; ++i) {
		const TzObj *o = &m->obj[i];
		if (!dinamico(o) || (o->flags & (TZ_F_INATIVO | TZ_F_DENTRO | TZ_F_CARREGADO))) continue;
		if ((o->flags & TZ_F_MORTO) && !(o->classe == TZC_RES_GOLD || o->classe == TZC_RES_STONE)) continue;
		int ox, oy;
		tz_obj_pixel(o, &ox, &oy);
		if (!(ox > camx - 256 && oy > camy - 256 && ox < camx + l->w + 256 && oy < camy + l->h + 320)) continue;
		/* la niebla esconde lo ajeno que no se ve ahora (los edificios vistos se quedan) */
		if (o->dono != m->jogador && !tz_mundo_ve(m, m->jogador, i)) continue;
		int px, py;
		TzEstado s;
		tz_mundo_pose(m, i, &s, &px, &py);
		vis[n].grupo = 2; vis[n].prio = prio(o); vis[n].prof = tz_profundidade(px, py, m->lado); vis[n].obj = i;
		++n;
	}
	qsort(vis, (size_t)n, sizeof *vis, cmp_vis);
	int k = 0;
	for (int i = 0; i < e->nv; ++i) {
		TzVisual v = e->v[i];
		if (v.obj >= 0 && v.obj < m->nobj && dinamico(&m->obj[v.obj])) continue;
		int g = v.chao == 1 ? 0 : v.chao == 2 ? 1 : 2;
		while (k < n && (vis[k].grupo < g || (vis[k].grupo == g && (vis[k].prio < v.prioridade || (vis[k].prio == v.prioridade && vis[k].prof < v.profundidade))))) {
			TzObjeto ob; TzEstado s; int px, py;
			tz_mundo_objeto_mapa(m, vis[k].obj, &ob);
			tz_mundo_pose(m, vis[k].obj, &s, &px, &py);
			if (tz_dibujar_objeto(e, l, &ob, &s, px, py, camx, camy, ms)) falhas++;
			++k;
		}
		const TzImg *im = &e->imgs[v.img];
		int x = v.x - camx, y = v.y - camy;
		if (x >= l->w || y >= l->h || x + im->w <= 0 || y + im->h <= 0) continue;
		tz_pintar(l, im, x, y);
	}
	for (; k < n; ++k) {
		TzObjeto ob; TzEstado s; int px, py;
		tz_mundo_objeto_mapa(m, vis[k].obj, &ob);
		tz_mundo_pose(m, vis[k].obj, &s, &px, &py);
		if (tz_dibujar_objeto(e, l, &ob, &s, px, py, camx, camy, ms)) falhas++;
	}
	/* proyectiles: un punto que vuela */
	for (int q = 0; q < m->nproj; ++q) {
		const struct TzProjetil *pr = &m->proj[q];
		uint32_t tot = pr->chega > pr->sai ? pr->chega - pr->sai : 1, ja = m->tic > pr->sai ? m->tic - pr->sai : 0;
		if (ja > tot) ja = tot;
		int x = (int)((pr->x0 + (int64_t)(pr->x1 - pr->x0) * ja / tot) / 8) - camx;
		int y = (int)((pr->y0 + (int64_t)(pr->y1 - pr->y0) * ja / tot) / 8) - camy - 16;
		tz_ret(l, x - 1, y - 1, 3, 3, 0x202020);
	}
	/* niebla del jugador: negro lo nunca visto, oscuro lo que no se ve ahora */
	if (!m->regras.sem_nevoa) {
		int cx0 = camx / 32, cy0 = camy / 32, cx1 = (camx + l->w + 31) / 32, cy1 = (camy + l->h + 31) / 32;
		for (int cy = cy0; cy < cy1 && cy < m->lado; ++cy)
			for (int cx = cx0; cx < cx1 && cx < m->lado; ++cx) {
				int f = tz_mundo_nevoa(m, m->jogador, cx, cy);
				if (f == 0 && !m->regras.explorado) tz_ret(l, cx * 32 - camx, cy * 32 - camy, 32, 32, 0);
				else if (f != 2) tz_ret_escuro(l, cx * 32 - camx, cy * 32 - camy, 32, 32);
			}
	}
	return falhas;
}

void tz_partida_dibujar(TzPartida *p, TzEscena *e, TzLienzo *l, int camx, int camy, uint64_t ms)
{
	TzMundo *m = &p->m;
	p->falhas_dibujo += tz_desenhar_mundo(m, e, l, camx, camy, ms);
	/* selección */
	for (int q = 0; q < p->nsel; ++q) {
		const TzObj *o = tz_mundo_obj(m, p->sel[q]);
		if (!o || !tz_obj_vivo(o)) continue;
		int r[4];
		int x, y, w, h;
		if (tz_obj_unidade(o)) { tz_obj_pixel(o, &x, &y); x -= 16; y -= 24; w = 32; h = 32; }
		else { r[0] = o->cx; r[1] = o->cy; x = r[0] * 32; y = r[1] * 32; w = tz_obj_at(m, o)->pegada[0] * 32; h = tz_obj_at(m, o)->pegada[1] * 32; }
		tz_marco_selecao(l, x - camx, y - camy, w, h, 0xffffffu);
		tz_barra_vida(l, x - camx, y - camy - 6, tz_obj_unidade(o) ? 1 : w / 32, o->vida > 0 ? o->vida : 0, o->vida_max, o->mana, o->mana_max);
	}
	/* obra por colocar: el rectángulo bajo el ratón */
	if (p->colocar >= 0) {
		const TzAtributos *a = tz_catalogo_at(&p->cat, p->colocar, m->reino[m->jogador].civ);
		int ok = !tz_mundo_cabe(m, m->jogador, p->colocar, p->rato_cx, p->rato_cy);
		tz_marco_selecao(l, p->rato_cx * 32 - camx, p->rato_cy * 32 - camy, a->pegada[0] * 32, a->pegada[1] * 32, ok ? 0x40ff40u : 0xff4040u);
	}
}

/* ------------------------------------------------------------------ entrada */
static void comando_ordem(TzPartida *p, TzOrdem o, int fila)
{
	TzComando c;
	memset(&c, 0, sizeof c);
	c.tipo = TZ_C_ORDEM; c.reino = (uint8_t)p->m.jogador; c.ordem = o; c.flags = (uint8_t)(fila ? TZ_CMD_FILA : 0);
	for (int q = 0; q < p->nsel && c.nids < TZ_CMD_IDS; ++q) if (p->m.obj[p->sel[q]].dono == p->m.jogador) c.ids[c.nids++] = p->sel[q];
	if (c.nids) tz_mundo_comando(&p->m, &c);
}

static TzId apontar(const TzPartida *p, int wx, int wy)
{
	const TzMundo *m = &p->m;
	int melhor = -1, dm = 24 * 24;
	for (int i = 0; i < m->nobj; ++i) {
		const TzObj *o = &m->obj[i];
		if (!tz_obj_vivo(o) || !tz_obj_unidade(o)) continue;
		int x, y;
		tz_obj_pixel(o, &x, &y);
		int d = (x - wx) * (x - wx) + (y - 8 - wy) * (y - 8 - wy);
		if (d < dm && (o->dono == m->jogador || tz_mundo_ve(m, m->jogador, i))) { dm = d; melhor = i; }
	}
	if (melhor >= 0) return melhor;
	TzId j = tz_mundo_em(m, wx / 32, wy / 32);
	if (j >= 0 && (m->obj[j].dono == m->jogador || tz_mundo_ve(m, m->jogador, j))) return j;
	return TZ_NENHUM;
}

void tz_partida_rato(TzPartida *p, int wx, int wy)
{
	p->rato_cx = wx / 32; p->rato_cy = wy / 32;
	if (p->colocar >= 0) {
		const TzAtributos *a = tz_catalogo_at(&p->cat, p->colocar, p->m.reino[p->m.jogador].civ);
		p->rato_cx -= a->pegada[0] / 2; p->rato_cy -= a->pegada[1] / 2;
	}
}

void tz_partida_clique(TzPartida *p, int wx, int wy, int botao, int mayus)
{
	TzMundo *m = &p->m;
	tz_partida_rato(p, wx, wy);
	if (botao == 1 && p->colocar >= 0) {
		TzComando c;
		memset(&c, 0, sizeof c);
		c.tipo = TZ_C_CONSTRUIR; c.reino = (uint8_t)m->jogador; c.arg = (int16_t)p->colocar;
		c.x = (int16_t)p->rato_cx; c.y = (int16_t)p->rato_cy; c.flags = (uint8_t)(mayus ? TZ_CMD_FILA : 0);
		for (int q = 0; q < p->nsel && c.nids < TZ_CMD_IDS; ++q) c.ids[c.nids++] = p->sel[q];
		tz_mundo_comando(m, &c);
		if (!mayus) p->colocar = -1;
		return;
	}
	if (botao == 1) {
		TzId j = apontar(p, wx, wy);
		if (!mayus) p->nsel = 0;
		if (j >= 0 && p->nsel < TZ_CMD_IDS && !selecionado(p, j)) {
			p->sel[p->nsel++] = j;
			TzEvento e; memset(&e, 0, sizeof e);
			e.tipo = TZ_EV_SELECIONADO; e.reino = (uint8_t)m->jogador; e.obj = j; e.alvo = TZ_NENHUM; e.classe = m->obj[j].classe;
			e.x = m->obj[j].cx; e.y = m->obj[j].cy;
			tz_mundo_evento(m, &e);
		}
		return;
	}
	if (botao == 3) {
		if (p->colocar >= 0) { p->colocar = -1; return; }
		if (!p->nsel) return;
		TzId alvo = apontar(p, wx, wy);
		if (alvo >= 0 && selecionado(p, alvo)) alvo = TZ_NENHUM;
		TzOrdem o = tz_mundo_ordem_padrao(m, m->jogador, p->sel, p->nsel, wx / 32, wy / 32, alvo);
		comando_ordem(p, o, mayus);
	}
}

void tz_partida_arrastar(TzPartida *p, int x0, int y0, int x1, int y1, int mayus)
{
	TzMundo *m = &p->m;
	if (!mayus) p->nsel = 0;
	if (x0 > x1) { int t = x0; x0 = x1; x1 = t; }
	if (y0 > y1) { int t = y0; y0 = y1; y1 = t; }
	for (int i = 0; i < m->nobj && p->nsel < TZ_CMD_IDS; ++i) {
		const TzObj *o = &m->obj[i];
		if (!tz_obj_vivo(o) || !tz_obj_unidade(o) || o->dono != m->jogador || selecionado(p, i)) continue;
		int x, y;
		tz_obj_pixel(o, &x, &y);
		if (x >= x0 && x <= x1 && y >= y0 && y <= y1) p->sel[p->nsel++] = i;
	}
}

/* la n-ésima opción (1-9) del menú: entrenar/investigar en un edificio; elegir obra con peones */
static void opcao(TzPartida *p, int n)
{
	TzMundo *m = &p->m;
	if (!p->nsel) return;
	const TzObj *o = &m->obj[p->sel[0]];
	if (o->dono != m->jogador) return;
	const TzOrdemDef *menu[160];
	int nm = tz_catalogo_menu(&p->cat, o->classe, o->civ, menu, 160), k = 0;
	for (int q = 0; q < nm; ++q) {
		const TzOrdemDef *d = menu[q];
		int util = !strncmp(d->acao, "create ", 7) || !strcmp(d->acao, "technology") || !strncmp(d->acao, "upgrade", 7) || !strncmp(d->acao, "build ", 6);
		if (!util || ++k != n) continue;
		TzComando c;
		memset(&c, 0, sizeof c);
		c.reino = (uint8_t)m->jogador; c.nids = 1; c.ids[0] = p->sel[0];
		if (!strncmp(d->acao, "build ", 6)) { p->colocar = tz_catalogo_classe(d->classe); return; }
		if (!strncmp(d->acao, "create ", 7)) { c.tipo = TZ_C_PRODUZIR; c.arg = (int16_t)tz_catalogo_classe(d->classe); }
		else { c.tipo = TZ_C_PESQUISAR; c.arg = (int16_t)tz_catalogo_tec(&p->cat, d->classe); }
		if (c.arg >= 0) tz_mundo_comando(m, &c);
		return;
	}
}

int tz_partida_tecla(TzPartida *p, int tecla)
{
	if (!p->ativa) return 0;
	if (tecla >= '1' && tecla <= '9') { opcao(p, tecla - '0'); return 1; }
	switch (tecla) {
	case ' ': p->pausa = !p->pausa; return 1;
	case '+': if (p->velocidade < 40) p->velocidade += 5; return 1;
	case '-': if (p->velocidade > 5) p->velocidade -= 5; return 1;
	case 127: { TzOrdem o; memset(&o, 0, sizeof o); o.tipo = TZ_O_PARAR; o.alvo = TZ_NENHUM; comando_ordem(p, o, 0); return 1; }
	}
	return 0;
}

/* resumen de una línea para la barra de arriba (Latin-1 de 8x16) */
void tz_partida_resumo(const TzPartida *p, char *dst, size_t n)
{
	const TzMundo *m = &p->m;
	const TzReinoEstado *r = &m->reino[m->jogador];
	int s = (int)(m->tic / TZ_TICS_SEG);
	snprintf(dst, n, "Oro %d  Piedra %d  Comida %d  Madera %d  Pobl. %u/%u  %02d:%02d%s%s", r->recursos[0], r->recursos[1], r->recursos[2],
	         r->recursos[3], r->populacao, r->populacao_max, s / 60, s % 60, p->pausa ? "  (pausa)" : "",
	         m->acabou ? (m->ganhou ? "  ¡VICTORIA!" : "  DERROTA") : "");
}
