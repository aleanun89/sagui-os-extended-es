/* SPDX-License-Identifier: GPL-3.0-or-later */
/* ui_hud.c - barra y HUD de la partida de Tzar (D417, fase 4): marco de piedra, contadores, vista de selección,
 * rejilla de órdenes con costes y requisitos, minimapa con niebla y línea de ayuda.
 * Traducción a C de hud.rs de ReTzar (GPL-3.0-or-later; geometría LAYOUT800L de Tzar.exe 1.11): vista de
 * selección en (6,169), rejilla 2x4 de iconos de 48x48 en (16,332) con 62 px entre columnas y 57 entre filas,
 * minimapa de 128x128 en (4,31), contadores en la barra de arriba. Solo el panel a la izquierda. */
#include "ui.h"
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define COR_TEXTO  0xe8d8a0u
#define COR_BRANCO 0xffffffu

/* ---------------------------------------------------------------- geometría */

/* Un rectángulo del arte de 800x600 en la ventana. Con el panel a la derecha (LAYOUT800R) lo que está desde x = 480
 * sigue al borde derecho de la ventana (hud.rs Layout::at). */
static TzRect L(const TzUi *u, int x, int y, int w, int h)
{
	if (u->direita && x >= 480) x += u->w - 800;
	return (TzRect){ x, y, w, h };
}

static TzRect r_comando(const TzUi *u, int i) { return L(u, (u->direita ? 668 : 16) + 3 + (i % 2) * 62, 332 + 3 + (i / 2) * 57, 48, 48); }

TzRect tz_ui_rect(const TzUi *u, int qual)
{
	static const int xs_esq[3] = { 25, 63, 101 }, xs_dir[3] = { 675, 712, 750 };
	static const int rec_esq[5][2] = { { 360, 48 }, { 463, 39 }, { 549, 46 }, { 644, 46 }, { 740, 46 } };
	static const int rec_dir[5][2] = { { 30, 46 }, { 131, 39 }, { 219, 45 }, { 313, 46 }, { 408, 46 } };
	int d = u->direita;
	unsigned is = (unsigned)(qual - TZ_UR_SETA0), ir = (unsigned)(qual - TZ_UR_RECURSO0);
	if (is < 3u) return L(u, d ? xs_dir[is % 3u] : xs_esq[is % 3u], 302, 25, 25);
	if (ir < 5u) return L(u, d ? rec_dir[ir % 5u][0] : rec_esq[ir % 5u][0], 0, d ? rec_dir[ir % 5u][1] : rec_esq[ir % 5u][1], 18);
	switch (qual) {
	case TZ_UR_MENU: return L(u, d ? 668 : 4, 0, 128, 25);
	case TZ_UR_DIPLOMACIA: return L(u, d ? 573 : 166, 0, 61, 25);
	case TZ_UR_CHAT: return L(u, d ? 506 : 233, 0, 61, 25);
	case TZ_UR_PAINEL: return d ? (TzRect){ u->w - 150, 30, 150, u->h - 30 } : (TzRect){ 0, 30, 150, u->h - 30 };
	case TZ_UR_RELOGIO: return L(u, d ? 622 : 142, d ? 32 : 31, 36, 36);
	default: return (TzRect){ 0, 0, 0, 0 };
	}
}
static TzRect r_seta(const TzUi *u, int i) { return tz_ui_rect(u, TZ_UR_SETA0 + i); }

/* el marco de piedra a tamaño de ventana (hud.rs background): lo de arriba y el panel izquierdo salen del arte de
 * 800x600 y de 1024x768; las franjas que sobran se repiten en espejo para que no se vea la costura */
static int espelho(int k, int de, int len)
{
	int t = ((k % (2 * len)) + 2 * len) % (2 * len);
	return de + (t < len ? t : 2 * len - 1 - t);
}

static void fazer_fundo(TzUi *u)
{
	tz_img_liberar(&u->fundo);
	int esq = !u->direita;
	const TzImg *a8 = tz_ui_img(u->skin, esq ? "SCREENS/S_VIEW/SCREEN 800 LEFT" : "SCREENS/S_VIEW/SCREEN 800 RIGHT", -1);
	const TzImg *a10 = tz_ui_img(u->skin, esq ? "SCREENS/S_VIEW/SCREEN 1024 LEFT" : "SCREENS/S_VIEW/SCREEN 1024 RIGHT", -1);
	u->fundo.w = u->w; u->fundo.h = u->h;
	u->fundo.px = calloc((size_t)u->w * (size_t)u->h, 4);
	if (!u->fundo.px) return;
	if (!a8 || !a10 || a8->w != 800 || a8->h != 600 || a10->w != 1024 || a10->h != 768) return;
	int alto = u->h >= 680, baixo = alto ? 120 : 40, W = u->w;
	for (int y = 0; y < u->h; ++y)
		for (int x = 0; x < W; ++x) {
			const TzImg *art; int sx, sy;
			if (y < 30) {
				art = a10; sy = y;
				if (esq) sx = x < 1024 ? x : espelho(x - 1024, 800, 224);
				else if (x >= W - 294) sx = x - (W - 1024);   /* el panel derecho: la barra sigue al borde */
				else if (x < 460) sx = x;
				else sx = espelho(x - 460, 460, 270);
			} else {
				if (esq ? x >= 218 : x < W - 218) continue;
				int ax8 = esq ? x : x - (W - 800), ax10 = esq ? x : x - (W - 1024);
				if (y < 560) { art = a8; sx = ax8; sy = y; }
				else if (y >= u->h - baixo) { if (alto) { art = a10; sx = ax10; sy = y - (u->h - 768); } else { art = a8; sx = ax8; sy = y - (u->h - 600); } }
				else { art = a10; sx = ax10; sy = espelho(y - 560, 560, 88); }
			}
			if (sx < 0 || sy < 0 || sx >= art->w || sy >= art->h) continue;
			u->fundo.px[(size_t)y * u->w + x] = art->px[(size_t)sy * art->w + sx];
		}
}

static void calcular_rects(TzUi *u)
{
	/* LAYOUT800L (150,30,650,570) y LAYOUT800R (0,30,651,570) a la medida de la ventana */
	u->mapa_rect = u->direita ? (TzRect){ 0, 30, u->w - 149, u->h - 30 } : (TzRect){ 150, 30, u->w - 150, u->h - 30 };
	u->minimapa_rect = L(u, u->direita ? 668 : 4, 31, 128, 128);
	u->selecao_rect = L(u, u->direita ? 657 : 6, 169, 118, 157);
}

int tz_ui_iniciar(TzUi *u, TzUiSkin *s, TzMundo *m, const TzBiblioteca *bib, int w, int h)
{
	memset(u, 0, sizeof *u);
	u->skin = s; u->m = m; u->bib = bib;
	u->botao_sobre = -1; u->sel_menu_obj = TZ_NENHUM; u->sel.clique_obj = TZ_NENHUM;
	u->w = w < 800 ? 800 : w; u->h = h < 600 ? 600 : h;
	calcular_rects(u);
	fazer_fundo(u);
	return u->fundo.px ? 0 : -1;
}

void tz_ui_liberar(TzUi *u)
{
	tz_img_liberar(&u->fundo);
	free(u->lista);
	memset(u, 0, sizeof *u);
}

void tz_ui_lado(TzUi *u, int direita)
{
	if (u->direita == (direita != 0)) return;
	u->direita = direita != 0;
	tz_ui_redimensionar(u, u->w, u->h);
}

void tz_ui_redimensionar(TzUi *u, int w, int h)
{
	u->w = w < 800 ? 800 : w; u->h = h < 600 ? 600 : h;
	calcular_rects(u);
	fazer_fundo(u);
}

/* ---------------------------------------------------------------- menú de órdenes */

static const char *nome_recurso(const TzUi *u, int r)
{
	static const char *const k[TZ_NREC] = { "Gold", "Stone", "Food", "Wood" };
	static const char *const d[TZ_NREC] = { "oro", "piedra", "comida", "madera" };
	return tz_ui_txt(u->skin, "viewmsg", k[r], d[r]);
}

static void texto_custo(const TzUi *u, const uint32_t c[TZ_NREC], uint32_t mana, char *out, size_t n)
{
	out[0] = 0;
	for (int r = 0; r < TZ_NREC; ++r)
		if (c[r]) snprintf(out + strlen(out), n - strlen(out), "%s%s: %u", out[0] ? ", " : "", nome_recurso(u, r), (unsigned)c[r]);
	if (mana) snprintf(out + strlen(out), n - strlen(out), "%s%s: %u", out[0] ? ", " : "", tz_ui_txt(u->skin, "viewmsg", "Manna", "mana"), (unsigned)mana);
}

/* palabra tras la acción («build barrack» -> «barrack») */
static const char *segunda_palavra(const char *acao)
{
	const char *p = strchr(acao, ' ');
	return p ? p + 1 : "";
}

static void rellenar_botao(TzUi *u, TzUiBotao *b, const TzOrdemDef *d, char hotkey, TzId obj)
{
	const TzMundo *m = u->m;
	memset(b, 0, sizeof *b);
	b->def = d; b->hotkey = hotkey; b->classe = -1; b->tec = -1;
	snprintf(b->rotulo, sizeof b->rotulo, "%s", tz_ui_txt(u->skin, "orders_names", d->chave, d->chave));
	snprintf(b->submenu, sizeof b->submenu, "%s", d->submenu);
	const char *cn = d->classe[0] ? d->classe : segunda_palavra(d->acao);
	uint32_t custo[TZ_NREC] = { 0 }, t = 0, mana = 0;
	if (!strncmp(d->acao, "build", 5) || !strncmp(d->acao, "create", 6)) {
		b->classe = cn[0] ? tz_catalogo_classe(cn) : -1;
		if (b->classe >= 0) tz_mundo_custo_classe(m, m->jogador, b->classe, custo, &t);
	} else if (!strncmp(d->acao, "technology", 10) || !strncmp(d->acao, "upgrade", 7)) {
		b->tec = tz_catalogo_tec(m->cat, d->classe[0] ? d->classe : d->chave);
		if (b->tec >= 0) { tz_mundo_custo_tec(m, m->jogador, b->tec, custo, &t); mana = m->cat->tec[b->tec].mana; }
	}
	texto_custo(u, custo, mana, b->custo, sizeof b->custo);
	if (d->submenu[0]) b->ok = 1;
	else if (tz_mundo_pode(m, m->jogador, obj, d->chave) == 0) b->ok = 1;
	else snprintf(b->motivo, sizeof b->motivo, "%s", tz_erro());
}

static int nome_menu(const TzUi *u, const TzObj *o, char *out, size_t n)
{
	const TzMundo *m = u->m;
	if (u->npilha) { snprintf(out, n, "%s", u->pilha[u->npilha - 1]); return 1; }
	const TzAtributos *a = m->cat ? tz_catalogo_at(m->cat, o->classe, o->civ) : NULL;
	if (!a || !a->menu[0]) return 0;
	snprintf(out, n, "%s", a->menu);
	/* con la cola de producción en marcha, la variante «_b» si existe (como en el original) */
	if (o->ator && o->ator->ntrab > 0) {
		char b[40];
		snprintf(b, sizeof b, "%s_b", a->menu);
		if (tz_ini_secao(&m->cat->menus, b)) snprintf(out, n, "%s", b);
	}
	return 1;
}

void tz_ui_menu_atualizar(TzUi *u)
{
	const TzMundo *m = u->m;
	u->nlista = 0; u->nbotoes = 0;
	if (!u->sel.n || !m->cat) return;
	TzId id = u->sel.id[0];
	const TzObj *o = &m->obj[id];
	if (!tz_obj_vivo(o) || o->dono != m->jogador) return;
	char nome[48];
	if (!nome_menu(u, o, nome, sizeof nome)) return;
	const TzIniSecao *sec = tz_ini_secao(&m->cat->menus, nome);
	int n = 0;
	if (sec) {
		for (int i = 0; i < sec->n; ++i) {
			const TzOrdemDef *d = tz_catalogo_ordem(m->cat, sec->p[i].chave);
			if (!d) continue;
			if (n == u->caplista) {
				int c = u->caplista ? u->caplista * 2 : 16;
				TzUiBotao *nb = realloc(u->lista, (size_t)c * sizeof *nb);
				if (!nb) break;
				u->lista = nb; u->caplista = c;
			}
			const char *hk = sec->p[i].valor;
			while (*hk == ' ') ++hk;
			rellenar_botao(u, &u->lista[n++], d, *hk, id);
		}
	} else if (!u->npilha) {   /* sin sección en MENU.INI: lo que dé el catálogo */
		const TzOrdemDef *os[32];
		int k = tz_catalogo_menu(m->cat, o->classe, o->civ, os, 32);
		for (int i = 0; i < k; ++i) {
			if (n == u->caplista) {
				int c = u->caplista ? u->caplista * 2 : 16;
				TzUiBotao *nb = realloc(u->lista, (size_t)c * sizeof *nb);
				if (!nb) break;
				u->lista = nb; u->caplista = c;
			}
			rellenar_botao(u, &u->lista[n++], os[i], 0, id);
		}
	}
	u->nlista = n;
	if (u->pagina * TZ_UI_BOTOES >= n) u->pagina = 0;
	u->ntotal_botoes = n;
	u->nbotoes = n - u->pagina * TZ_UI_BOTOES;
	if (u->nbotoes > TZ_UI_BOTOES) u->nbotoes = TZ_UI_BOTOES;
	for (int i = 0; i < u->nbotoes; ++i) u->botoes[i] = u->lista[u->pagina * TZ_UI_BOTOES + i];
	u->sel_menu_obj = id;
}

int tz_ui_botao_em(const TzUi *u, int x, int y)
{
	for (int i = 0; i < u->nbotoes; ++i)
		if (tz_rect_dentro(r_comando(u, i), x, y)) return i;
	return -1;
}

/* acción de ORDERS.INI -> tipo de orden con objetivo; 0 = no lleva objetivo o no se conoce */
static int tipo_da_acao(const char *a)
{
	static const struct { const char *a; int t; } tab[] = {
		{ "move", TZ_O_MOVER }, { "move_sneak", TZ_O_MOVER }, { "move_advance", TZ_O_MOVER }, { "attack", TZ_O_ATACAR },
		{ "attack_ground", TZ_O_ATACAR_CHAO }, { "patrol", TZ_O_PATRULHAR }, { "guard", TZ_O_GUARDAR },
		{ "capture", TZ_O_CAPTURAR }, { "repair", TZ_O_REPARAR }, { "harvest", TZ_O_RECOLHER }, { "mine", TZ_O_RECOLHER },
		{ "dig", TZ_O_RECOLHER }, { "chop", TZ_O_CORTAR }, { "fishing", TZ_O_RECOLHER }, { "spell", TZ_O_HECHIZO },
		{ "item_use", TZ_O_USAR_ITEM }, { "pick_item", TZ_O_PEGAR_ITEM }, { "unload", TZ_O_DESCARREGAR },
		{ "trade", TZ_O_COMERCIAR },
	};
	for (unsigned i = 0; i < sizeof tab / sizeof *tab; ++i) if (!strcasecmp(a, tab[i].a)) return tab[i].t;
	return 0;
}

static void edificios_da_selecao(const TzUi *u, TzId *out, int *n)
{
	*n = 0;
	for (int i = 0; i < u->sel.n; ++i) {
		const TzObj *o = &u->m->obj[u->sel.id[i]];
		if (tz_obj_edificio(o) && o->dono == u->m->jogador && *n < TZ_SEL_MAX) out[(*n)++] = u->sel.id[i];
	}
}

static void comando_simples(TzUi *u, int tipo, int arg, const TzId *ids, int n, const TzOrdem *o)
{
	TzComando c;
	memset(&c, 0, sizeof c);
	c.tipo = (uint8_t)tipo; c.reino = (uint8_t)u->m->jogador; c.arg = (int16_t)arg;
	if (o) c.ordem = *o;
	for (int i = 0; i < n; i += TZ_CMD_IDS) {
		c.nids = n - i < TZ_CMD_IDS ? n - i : TZ_CMD_IDS;
		memcpy(c.ids, ids + i, (size_t)c.nids * sizeof c.ids[0]);
		if (tz_mundo_comando(u->m, &c) == 0) u->n_comandos++;
	}
}

void tz_ui_botao_premir(TzUi *u, int i, int mods)
{
	if (i < 0 || i >= u->nbotoes) return;
	const TzUiBotao *b = &u->botoes[i];
	const TzOrdemDef *d = b->def;
	if (!b->ok) { tz_ui_aviso(u, "%s", b->motivo[0] ? b->motivo : b->rotulo); return; }
	if (b->submenu[0]) {
		if (u->npilha < TZ_UI_PILHA) {
			char sub[32];
			snprintf(sub, sizeof sub, "%s", b->submenu);   /* copia: tz_ui_menu_atualizar reescribe u->botoes */
			snprintf(u->pilha[u->npilha++], 32, "%s", sub);
			u->pagina = 0;
			tz_ui_menu_atualizar(u);
		}
		return;
	}
	TzId ids[TZ_SEL_MAX];
	int n = 0;
	if (!strncmp(d->acao, "build", 5) && b->classe >= 0) {   /* edificio: se coloca con el ratón */
		u->modo = TZ_UI_COLOCAR; u->classe_colocar = b->classe; u->colocar_ok = 0;
		return;
	}
	if (!strncmp(d->acao, "create", 6) && b->classe >= 0) {   /* entrenar: Mayús x5 */
		edificios_da_selecao(u, ids, &n);
		for (int k = 0; k < (mods & TZ_MOD_SHIFT ? 5 : 1) && n; ++k) comando_simples(u, TZ_C_PRODUZIR, b->classe, ids, 1, NULL);
		return;
	}
	if (b->tec >= 0) { edificios_da_selecao(u, ids, &n); if (n) comando_simples(u, TZ_C_PESQUISAR, b->tec, ids, 1, NULL); return; }
	/* unidades */
	for (int k = 0; k < u->sel.n && n < TZ_SEL_MAX; ++k) {
		const TzObj *o = &u->m->obj[u->sel.id[k]];
		if (o->dono == u->m->jogador && tz_obj_vivo(o)) ids[n++] = u->sel.id[k];
	}
	if (!n) return;
	if (!strcasecmp(d->acao, "stop")) { TzOrdem o; memset(&o, 0, sizeof o); o.tipo = TZ_O_PARAR; o.alvo = TZ_NENHUM; comando_simples(u, TZ_C_ORDEM, 0, ids, n, &o); return; }
	if (!strcasecmp(d->acao, "stand_ground")) { comando_simples(u, TZ_C_MODO, TZ_MODO_QUIETO, ids, n, NULL); return; }
	if (!strcasecmp(d->acao, "hold_steady")) { comando_simples(u, TZ_C_MODO, TZ_MODO_DEFENSIVO, ids, n, NULL); return; }
	if (!strcasecmp(d->acao, "holdfire")) { comando_simples(u, TZ_C_MODO, TZ_MODO_SEM_FOGO, ids, n, NULL); return; }
	if (!strcasecmp(d->acao, "die")) { comando_simples(u, TZ_C_DESTRUIR, 0, ids, n, NULL); return; }
	if (!strcasecmp(d->acao, "explore")) { TzOrdem o; memset(&o, 0, sizeof o); o.tipo = TZ_O_EXPLORAR; o.alvo = TZ_NENHUM; comando_simples(u, TZ_C_ORDEM, 0, ids, n, &o); return; }
	int t = tipo_da_acao(d->acao);
	if (t) {   /* orden con objetivo: el cursor cambia y el siguiente clic izquierdo la da */
		u->modo = TZ_UI_ALVO; u->tipo_alvo = t;
		u->ordem_idx = (int)(d - u->m->cat->ord);   /* índice en el catálogo: es el «arg» de los hechizos */
		snprintf(u->acao_alvo, sizeof u->acao_alvo, "%s", d->acao);
		return;
	}
	tz_ui_aviso(u, "%s", b->rotulo);
}

/* ---------------------------------------------------------------- línea de ayuda */

const char *tz_ui_dica(TzUi *u, char *buf, int n)
{
	buf[0] = 0;
	if (u->modo == TZ_UI_COLOCAR) {
		const char *nm = tz_ui_txt(u->skin, "objects", tz_classes[u->classe_colocar], tz_classes[u->classe_colocar]);
		snprintf(buf, (size_t)n, "%s", nm);
		return buf;
	}
	if (u->botao_sobre >= 0 && u->botao_sobre < u->nbotoes) {
		const TzUiBotao *b = &u->botoes[u->botao_sobre];
		snprintf(buf, (size_t)n, "%s", b->rotulo);
		if (b->hotkey) snprintf(buf + strlen(buf), (size_t)n - strlen(buf), " (%c)", b->hotkey);
		if (b->custo[0]) snprintf(buf + strlen(buf), (size_t)n - strlen(buf), "  %s", b->custo);
		if (b->motivo[0]) snprintf(buf + strlen(buf), (size_t)n - strlen(buf), "  - %s", b->motivo);
	}
	return buf;
}

/* ---------------------------------------------------------------- dibujo */

static void caixa_texto(TzUi *u, TzTela *t, const TzUiFonte *f, TzRect r, const char *s, uint32_t cor, int centrado)
{
	tz_ui_fonte_caixa(t, f, r, s, cor, centrado);
	(void)u;
}

static void barra_vida(TzTela *t, TzRect r, int vida, int max)
{
	tz_tela_preencher(t, r, 0xff101010);
	if (max <= 0) return;
	int w = vida * r.w / max;
	if (w < 0) w = 0;
	if (w > r.w) w = r.w;
	tz_tela_preencher(t, (TzRect){ r.x, r.y, w, r.h }, 0xff20c820);
}

static void desenhar_icone(TzUi *u, TzTela *t, const char *chave, TzRect d)
{
	const TzImg *im; TzRect s;
	if (!tz_ui_icone(u->skin, chave, -1, &im, &s)) return;
	/* los iconos son cuadrados de 20x20 o más: se recortan al hueco */
	TzRect s2 = { 0, s.y, im->w < d.w ? im->w : d.w, im->w < d.h ? im->w : d.h };
	tz_tela_imagem(t, im, s2, d.x, d.y, 0);
}

static void estatistica(TzUi *u, TzTela *t, TzRect v, int k, const char *icone, const char *valor)
{
	int y = 3 + 22 * k;
	desenhar_icone(u, t, icone, (TzRect){ v.x + 66, v.y + y, 20, 20 });
	tz_ui_fonte_texto(t, &u->skin->f10, v.x + 88, v.y + y + 3, valor, COR_TEXTO);
}

static void vista_selecao(TzUi *u, TzTela *t)
{
	TzRect v = u->selecao_rect;
	const TzMundo *m = u->m;
	const TzUiFonte *f = &u->skin->f10;
	if (!u->sel.n) return;
	const TzObj *o = &m->obj[u->sel.id[0]];
	const TzImg *moldura = tz_ui_img(u->skin, "ORDERS/FRAME1", -1);
	TzRect ret = { v.x + 6, v.y + 5, 48, 48 };
	char b[96];
	if (u->sel.n > 1) {   /* varias: icono genérico, salud media y cuántas */
		const TzImg *ms = tz_ui_img(u->skin, "ICONS/MULTISELECT", -1);
		if (ms) tz_tela_imagem(t, ms, (TzRect){ 0, 0, 48, 48 }, ret.x, ret.y, 0);
		else tz_tela_preencher(t, ret, 0xff303030);
		if (moldura) tz_tela_imagem(t, moldura, (TzRect){ 0, 0, 0, 0 }, ret.x, ret.y, 0);
		int soma = 0, nv = 0;
		for (int i = 0; i < u->sel.n; ++i) {
			const TzObj *q = &m->obj[u->sel.id[i]];
			if (q->vida_max > 0) { soma += q->vida * 100 / q->vida_max; nv++; }
		}
		int pct = nv ? soma / nv : 0;
		snprintf(b, sizeof b, "%d/100(%%)", pct);
		tz_ui_fonte_texto(t, f, v.x + 3, v.y + 66, b, COR_TEXTO);
		barra_vida(t, (TzRect){ v.x + 3, v.y + 81, 54, 3 }, pct, 100);
		snprintf(b, sizeof b, "%d %s", u->sel.n, tz_ui_txt(u->skin, "selview", "units", "unidades"));
		tz_ui_fonte_texto(t, f, v.x + 3, v.y + 93, b, COR_BRANCO);
		return;
	}
	const TzImg *ic; TzRect src;
	if (tz_ui_icone_classe(u->skin, u->bib, o->classe, o->civ, o->cor, &ic, &src)) tz_tela_imagem(t, ic, src, ret.x, ret.y, 0);
	else tz_tela_preencher(t, ret, 0xff303030);
	if (moldura) tz_tela_imagem(t, moldura, (TzRect){ 0, 0, 0, 0 }, ret.x, ret.y, 0);
	if (o->vida_max > 0) {
		snprintf(b, sizeof b, "%d/%d", (int)o->vida, (int)o->vida_max);
		tz_ui_fonte_texto(t, f, v.x + 3, v.y + 66, b, COR_TEXTO);
		barra_vida(t, (TzRect){ v.x + 3, v.y + 81, 54, 3 }, o->vida, o->vida_max);
	}
	/* dos líneas: dueño y nombre de la clase (o del héroe) */
	const TzReinoEstado *rk = o->dono <= TZ_REINOS ? &m->reino[o->dono] : NULL;
	if (rk && rk->nome[0]) tz_ui_fonte_texto(t, f, v.x + 3, v.y + 93, rk->nome, COR_BRANCO);
	const char *nome = tz_ui_txt(u->skin, "objects", tz_classes[o->classe], tz_classes[o->classe]);
	caixa_texto(u, t, f, (TzRect){ v.x + 3, v.y + 108, 113, 15 }, nome, COR_BRANCO, 0);
	/* estadísticas (hud.rs object_statistics) */
	int k = 0;
	if (o->recurso && (o->classe == TZC_RES_GOLD || o->classe == TZC_RES_STONE)) {
		snprintf(b, sizeof b, "%u", (unsigned)o->recurso);
		estatistica(u, t, v, k++, o->classe == TZC_RES_GOLD ? "resgold" : "resstone", b);
		return;
	}
	if (tz_obj_edificio(o)) {
		snprintf(b, sizeof b, "%d", o->resistencia);
		estatistica(u, t, v, k++, "resistanceicon", b);
		if (o->classe >= 39 && o->classe <= 43) { snprintf(b, sizeof b, "%d", o->ataque); estatistica(u, t, v, k++, "attackicon_bow", b); }
	} else if (tz_obj_unidade(o)) {
		const TzAtributos *a = m->cat ? tz_catalogo_at(m->cat, o->classe, o->civ) : NULL;
		if (o->ataque > 0) { snprintf(b, sizeof b, "%d", o->ataque); estatistica(u, t, v, k++, a && a->categoria_arma ? "attackicon_bow" : "attackicon_hand", b); }
		snprintf(b, sizeof b, "%d", o->defesa);
		estatistica(u, t, v, k++, "defenceicon", b);
		if (o->classe != TZC_TRADER) { snprintf(b, sizeof b, "%d", o->nivel ? o->nivel : 1); estatistica(u, t, v, k++, "levelicon", b); }
	}
	if (o->ator && k < 5) {
		const TzAtor *at = o->ator;
		if ((o->flags & TZ_F_OBRA) && at->obra_total_ms) { snprintf(b, sizeof b, "%u%%", (unsigned)(at->obra_ms * 100 / at->obra_total_ms)); estatistica(u, t, v, k++, "buildicon", b); }
		else if (at->ntrab > 0 && at->trab[0].total_ms) {
			snprintf(b, sizeof b, "%02u%%", (unsigned)((at->trab[0].total_ms - at->trab[0].restante_ms) * 100 / at->trab[0].total_ms));
			estatistica(u, t, v, k++, "buildicon", b);
		}
		if (at->carga > 0 && k < 5) {
			static const char *const rc[TZ_NREC] = { "resgold", "resstone", "resfood", "reswood" };
			snprintf(b, sizeof b, "%u", at->carga);
			estatistica(u, t, v, k++, rc[at->carga_tipo & 3], b);
		}
	}
	if (o->mana_max > 0 && k < 5) { snprintf(b, sizeof b, "%d/%d", o->mana, o->mana_max); estatistica(u, t, v, k++, "mannaicon", b); }
}

static void rejilla(TzUi *u, TzTela *t)
{
	const TzImg *ordens = tz_ui_img(u->skin, "ORDERS/BUTTONS", -1);
	const TzImg *moldura = tz_ui_img(u->skin, "ORDERS/FRAME1", -1);
	for (int i = 0; i < u->nbotoes; ++i) {
		const TzUiBotao *b = &u->botoes[i];
		TzRect r = r_comando(u, i);
		int desloca = u->botao_sobre == i && (u->mods & 0x100);   /* pulsado */
		int icone = b->def->icone;
		if (ordens && icone >= 0 && (icone + 1) * 48 <= ordens->h)
			tz_tela_imagem(t, ordens, (TzRect){ 0, icone * 48, 48, 48 }, r.x + desloca, r.y + desloca, b->ok ? 0 : 50);
		else {
			tz_tela_preencher(t, r, 0xff404038);
			tz_ui_fonte_caixa(t, &u->skin->f10, r, b->rotulo, b->ok ? COR_TEXTO : 0x808080, 1);
		}
		if (moldura) tz_tela_imagem(t, moldura, (TzRect){ 0, 0, 0, 0 }, r.x, r.y, 0);
		if (u->modo == TZ_UI_ALVO && !strcasecmp(b->def->acao, u->acao_alvo)) tz_tela_moldura(t, r, 0xffe0e040);
		if (b->classe >= 0 && !strncmp(b->def->acao, "create", 6)) {   /* cuántas hay en cola de esa clase */
			int cnt = 0;
			if (u->sel.n) {
				const TzObj *o = &u->m->obj[u->sel.id[0]];
				if (o->ator) for (int k = 0; k < o->ator->ntrab; ++k) cnt += o->ator->trab[k].tipo == TZ_J_TREINAR && o->ator->trab[k].arg == b->classe;
			}
			if (cnt) { char s[8]; snprintf(s, sizeof s, "%3d", cnt); tz_ui_fonte_texto(t, &u->skin->f10, r.x + 24, r.y + 31, s, COR_BRANCO); }
		}
	}
	/* flechas de página / submenú anterior / contenido */
	const char *n = u->skin->rec ? "ORDERS" : NULL;
	static const char *const art[3] = { "ORDERS/LEFT", "ORDERS/HOLDER", "ORDERS/RIGHT" };
	int paginas = u->ntotal_botoes ? (u->ntotal_botoes + TZ_UI_BOTOES - 1) / TZ_UI_BOTOES : 1;
	int ativa[3] = { u->pagina > 0 || u->npilha > 0, 0, u->pagina + 1 < paginas };
	for (int i = 0; i < 3 && n; ++i) {
		const TzImg *im = tz_ui_img(u->skin, art[i], -1);
		if (im && im->w >= 100) tz_tela_imagem(t, im, (TzRect){ ativa[i] ? 0 : 75, 0, 25, 25 }, r_seta(u, i).x, r_seta(u, i).y, 0);
	}
}

static void minimapa(TzUi *u, TzTela *t)
{
	const TzMundo *m = u->m;
	TzRect r = u->minimapa_rect;
	tz_tela_preencher(t, r, 0xff000000);
	if (!m || m->lado <= 0) return;
	int lado = m->lado, jog = m->jogador;
	for (int j = 0; j < r.h; ++j) {
		int cy = j * lado / r.h;
		for (int i = 0; i < r.w; ++i) {
			int cx = i * lado / r.w;
			int ng = tz_mundo_nevoa(m, jog, cx, cy);
			uint32_t c = u->minimapa ? u->minimapa[(size_t)cy * lado + cx] & 0xffffff : 0x304020;
			if (ng == 0 && !m->regras.explorado) c = 0;
			else if (ng == 1) c = (c >> 1) & 0x7f7f7f;
			t->px[(size_t)(r.y + j) * t->w + r.x + i] = 0xff000000u | c;
		}
	}
	/* objetos como puntos del color del reino (los ajenos solo si se ven) */
	for (int i = 0; i < m->nobj; ++i) {
		const TzObj *o = &m->obj[i];
		if (!tz_obj_vivo(o) || (!tz_obj_unidade(o) && !tz_obj_edificio(o))) continue;
		if (o->dono != jog && tz_mundo_nevoa(m, jog, o->cx, o->cy) != 2 && !(tz_obj_edificio(o) && tz_mundo_nevoa(m, jog, o->cx, o->cy) == 1)) continue;
		uint32_t cor = o->dono == 0 ? 0xa0a0a0 : u->skin->cores_mini[o->cor & 7];
		int x = r.x + o->cx * r.w / lado, y = r.y + o->cy * r.h / lado, s = tz_obj_edificio(o) ? 2 : 1;
		tz_tela_preencher(t, (TzRect){ x, y, s, s }, 0xff000000u | cor);
	}
	/* rectángulo de la vista */
	int mw = lado * TZ_CASILLA;
	TzRect v = { r.x + u->camx * r.w / mw, r.y + u->camy * r.h / mw, u->mapa_rect.w * r.w / mw, u->mapa_rect.h * r.h / mw };
	if (v.w > r.w) v.w = r.w;
	if (v.h > r.h) v.h = r.h;
	if (v.x + v.w > r.x + r.w) v.w = r.x + r.w - v.x;
	if (v.y + v.h > r.y + r.h) v.h = r.y + r.h - v.y;
	tz_tela_moldura(t, v, 0xffffffff);
}

void tz_ui_desenhar_hud(TzUi *u, TzTela *t)
{
	const TzMundo *m = u->m;
	tz_ui_sel_vivos(m, &u->sel);
	if (u->sel_menu_obj != (u->sel.n ? u->sel.id[0] : TZ_NENHUM) || (u->sel.n && m->tic % 20 == 0)) tz_ui_menu_atualizar(u);
	if (u->fundo.px) tz_tela_imagem(t, &u->fundo, (TzRect){ 0, 0, 0, 0 }, 0, 0, 0);
	else {   /* sin arte: marco liso */
		tz_tela_preencher(t, (TzRect){ 0, 0, u->w, 30 }, 0xff383830);
		tz_tela_preencher(t, tz_ui_rect(u, TZ_UR_PAINEL), 0xff383830);
	}
	/* contadores: oro, madera, comida, piedra y población/límite */
	const TzReinoEstado *k = &m->reino[m->jogador > 0 && m->jogador <= TZ_REINOS ? m->jogador : 1];
	char b[48];
	const int ord[4] = { TZ_OURO, TZ_MADEIRA, TZ_COMIDA, TZ_PEDRA };
	for (int i = 0; i < 4; ++i) {
		snprintf(b, sizeof b, "%d", (int)k->recursos[ord[i]]);
		tz_ui_fonte_caixa(t, &u->skin->f10, tz_ui_rect(u, TZ_UR_RECURSO0 + i), b, COR_TEXTO, 1);
	}
	int lim = k->populacao_max < k->populacao_limite || !k->populacao_limite ? k->populacao_max : k->populacao_limite;
	snprintf(b, sizeof b, "%d/%d", k->populacao, lim);
	tz_ui_fonte_caixa(t, &u->skin->f10, tz_ui_rect(u, TZ_UR_RECURSO0 + 4), b, COR_TEXTO, 1);
	/* botones de arriba */
	const char *lado = u->direita ? "RIGHT" : "LEFT";
	char nb[64];
	TzRect r_menu = tz_ui_rect(u, TZ_UR_MENU), r_dipl = tz_ui_rect(u, TZ_UR_DIPLOMACIA), r_chat = tz_ui_rect(u, TZ_UR_CHAT);
	snprintf(nb, sizeof nb, "SCREENS/S_VIEW/BTN MENU %s", lado);
	const TzImg *bm = tz_ui_img(u->skin, nb, -1);
	int premido = u->mods & 0x100;
	int sobre_menu = tz_rect_dentro(r_menu, u->mx, u->my);
	if (bm) tz_tela_imagem(t, bm, (TzRect){ sobre_menu && premido ? 128 : 0, 0, 128, 25 }, r_menu.x, r_menu.y, 0);
	else tz_tela_preencher(t, r_menu, 0xff484038);
	tz_ui_fonte_caixa(t, &u->skin->f10, r_menu, tz_ui_txt(u->skin, "game_scr", "menu", "Menu"), COR_TEXTO, 1);
	snprintf(nb, sizeof nb, "SCREENS/S_VIEW/BTN DIPLOMACY %s", lado);
	const TzImg *bd = tz_ui_img(u->skin, nb, -1);
	snprintf(nb, sizeof nb, "SCREENS/S_VIEW/BTN CHAT %s", lado);
	const TzImg *bc = tz_ui_img(u->skin, nb, -1);
	int sd = tz_rect_dentro(r_dipl, u->mx, u->my) && premido, sc = tz_rect_dentro(r_chat, u->mx, u->my) && premido;
	if (bd) tz_tela_imagem(t, bd, (TzRect){ sd && bd->w >= 122 ? 61 : 0, 0, 61, 25 }, r_dipl.x, r_dipl.y, 0);
	else { tz_tela_preencher(t, r_dipl, 0xff484038); tz_ui_fonte_caixa(t, &u->skin->f10, r_dipl, tz_ui_txt(u->skin, "game_scr", "diplomacy", "Diplom."), COR_TEXTO, 1); }
	if (bc) tz_tela_imagem(t, bc, (TzRect){ sc && bc->w >= 122 ? 61 : 0, 0, 61, 25 }, r_chat.x, r_chat.y, 0);
	else { tz_tela_preencher(t, r_chat, 0xff484038); tz_ui_fonte_caixa(t, &u->skin->f10, r_chat, tz_ui_txt(u->skin, "game_scr", "chat", "Chat"), COR_TEXTO, 1); }
	minimapa(u, t);
	vista_selecao(u, t);
	rejilla(u, t);
	/* línea de ayuda abajo del mapa y avisos arriba */
	char dica[220];
	tz_ui_dica(u, dica, sizeof dica);
	if (dica[0]) {
		TzRect r = { u->mapa_rect.x, u->h - 20, u->mapa_rect.w, 20 };
		tz_tela_preencher(t, r, 0xa0000000);
		tz_ui_fonte_caixa(t, &u->skin->f10, (TzRect){ r.x + 6, r.y, r.w - 12, r.h }, dica, COR_BRANCO, 0);
	}
	if (u->aviso[0] && u->agora_ms < u->aviso_ate) {
		int w = tz_ui_fonte_medir(&u->skin->f12b, u->aviso) + 16;
		TzRect r = { u->mapa_rect.x + (u->mapa_rect.w - w) / 2, u->mapa_rect.y + 6, w, 22 };
		tz_tela_preencher(t, r, 0xa0000000);
		tz_ui_fonte_caixa(t, &u->skin->f12b, r, u->aviso, 0xffe080, 1);
	}
	if (u->pausa) tz_ui_fonte_caixa(t, &u->skin->f22, u->mapa_rect, tz_ui_txt(u->skin, "viewmsg", "pause", "PAUSA"), 0xffffff, 1);
	tz_ui_janela_desenhar(u, t);
}

/* marcas sobre el mapa: círculos y vida de lo seleccionado, recuadro y fantasma del edificio por colocar */
void tz_ui_desenhar_marcas(TzUi *u, TzTela *t)
{
	const TzMundo *m = u->m;
	TzRect mr = u->mapa_rect;
	for (int i = 0; i < u->sel.n; ++i) {
		const TzObj *o = &m->obj[u->sel.id[i]];
		if (!tz_obj_vivo(o)) continue;
		uint32_t cor = o->dono == m->jogador ? 0xff30e030 : tz_mundo_relacao(m, m->jogador, o->dono) < 0 ? 0xffe03030 : 0xffe0e030;
		if (tz_obj_unidade(o)) {
			int x, y;
			tz_obj_pixel(o, &x, &y);
			int sx = mr.x + x - u->camx, sy = mr.y + y - u->camy;
			/* elipse a los pies, cada 4 grados aproximada con 24 puntos enteros */
			static const int cs[24][2] = { {12,0},{11,2},{10,3},{8,4},{6,5},{3,5},{0,6},{-3,5},{-6,5},{-8,4},{-10,3},{-11,2},{-12,0},{-11,-2},{-10,-3},{-8,-4},{-6,-5},{-3,-5},{0,-6},{3,-5},{6,-5},{8,-4},{10,-3},{11,-2} };
			for (int p = 0; p < 24; ++p) {
				int px = sx + cs[p][0], py = sy + 2 + cs[p][1];
				if (tz_rect_dentro(mr, px, py)) t->px[(size_t)py * t->w + px] = cor;
			}
			if (o->vida_max > 0 && tz_rect_dentro(mr, sx, sy - 34)) barra_vida(t, (TzRect){ sx - 10, sy - 36, 20, 3 }, o->vida, o->vida_max);
		} else {
			const TzAtributos *a = m->cat ? tz_catalogo_at(m->cat, o->classe, o->civ) : NULL;
			int w = a && a->pegada[0] ? a->pegada[0] : 1, h = a && a->pegada[1] ? a->pegada[1] : 1;
			TzRect r = { mr.x + o->cx * TZ_CASILLA - u->camx, mr.y + o->cy * TZ_CASILLA - u->camy, w * TZ_CASILLA, h * TZ_CASILLA };
			tz_tela_moldura(t, r, cor);
			if (o->vida_max > 0) barra_vida(t, (TzRect){ r.x + 2, r.y - 6, r.w - 4, 4 }, o->vida, o->vida_max);
		}
	}
	if (u->arrasta) {
		int x0 = u->ax < u->mx ? u->ax : u->mx, y0 = u->ay < u->my ? u->ay : u->my;
		TzRect r = { x0, y0, abs(u->mx - u->ax), abs(u->my - u->ay) };
		if (r.w > 3 || r.h > 3) { tz_tela_preencher(t, r, 0x2020e020); tz_tela_moldura(t, r, 0xff30e030); }
	}
	if (u->modo == TZ_UI_COLOCAR && tz_rect_dentro(mr, u->mx, u->my)) {
		const TzAtributos *a = m->cat ? tz_catalogo_at(m->cat, u->classe_colocar, m->reino[m->jogador].civ) : NULL;
		int w = a && a->pegada[0] ? a->pegada[0] : 1, h = a && a->pegada[1] ? a->pegada[1] : 1;
		int cx = u->colocar_cx, cy = u->colocar_cy;
		TzRect r = { mr.x + cx * TZ_CASILLA - u->camx, mr.y + cy * TZ_CASILLA - u->camy, w * TZ_CASILLA, h * TZ_CASILLA };
		tz_tela_preencher(t, r, u->colocar_ok ? 0x6030e030 : 0x60e03030);
		tz_tela_moldura(t, r, u->colocar_ok ? 0xff30ff30 : 0xffff3030);
	}
}
