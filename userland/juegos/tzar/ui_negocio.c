/* SPDX-License-Identifier: GPL-3.0-or-later */
/* ui_negocio.c - ventanas propias de la partida de Tzar para lo que el original resuelve con los diálogos de órdenes (D417, D471,
 * v8.5.6): taberna (mercenarios y héroes), dados, préstamos, intercambio de recursos, rutas de comercio, inventario de objetos,
 * cruzada, tributos, rendición, fábrica mágica y mercado (joyas y venta).
 *
 * Sigue a ReTzar (https://github.com/ColCarlo/ReTzar, GPL-3.0-or-later): dialogs.rs (D_GAMBLE, D_TRADE1, D_TRADE y D_build: los
 * recursos y cantidades, los pasos de 50/200, 500/1000 y 50/100, las cuatro probabilidades de los dados, los dados «zarove» de 15
 * cuadros cada 70 ms y la cara «winlose»), hud.rs (ventana de tributos «D_ADVANCED») y gameplay.rs (precios de la taberna,
 * préstamos, objetos). El estilo es el de ui_janelas.c: marco de piedra dibujado, las fuentes y los textos de STRINGS.INI del
 * juego, los iconos de recursos de ICONS.INI y el arte de los dados cuando existe. Todo se manda con comandos del mundo
 * (TZ_C_CONTRATAR, APOSTAR, EMPRESTAR, DEVOLVER, TROCAR, FABRICAR, COMPRAR, VENDER, CRUZADA, TRIBUTO, RENDIR, ALMA y órdenes de
 * comercio y objetos); la interfaz no escribe nunca en TzMundo.
 *
 * Cada ventana se dibuja y se maneja con el mismo código (neg_*): al dibujar se anotan los botones en u->jw y la entrada busca el
 * clic entre los de la última pasada, de modo que lo que se ve es lo que se pulsa. */
#include "ui.h"
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#define COR_TITULO 0xe8d890u
#define COR_TEXTO  0xd8c890u
#define COR_BRANCO 0xffffe0u
#define COR_MAL    0xe08070u
#define COR_BIEN   0x80e080u
#define COR_APAGADO 0x908868u


TzRect tz_ui_neg_rect(const TzUi *u)
{
	static const struct { int j, w, h; } t[] = {
		{ TZ_UJ_TABERNA, 620, 430 }, { TZ_UJ_DADOS, 440, 340 }, { TZ_UJ_PRESTAMOS, 480, 300 }, { TZ_UJ_INTERCAMBIO, 480, 340 },
		{ TZ_UJ_COMERCIO, 540, 380 }, { TZ_UJ_OBJETOS, 640, 470 }, { TZ_UJ_CRUZADA, 460, 320 }, { TZ_UJ_TRIBUTO, 480, 320 },
		{ TZ_UJ_RENDICION, 440, 170 }, { TZ_UJ_FABRICA, 500, 390 }, { TZ_UJ_MERCADO, 620, 430 },
	};
	int w = 500, h = 340;
	for (unsigned i = 0; i < sizeof t / sizeof *t; ++i) if (t[i].j == u->janela) { w = t[i].w; h = t[i].h; }
	if (w > u->w - 10) w = u->w - 10;
	if (h > u->h - 10) h = u->h - 10;
	return (TzRect){ (u->w - w) / 2, (u->h - h) / 2, w, h };
}

/* ------------------------------------------------------------------ utilidades */
static const char *nome_rec(const TzUi *u, int r)
{
	static const char *const k[TZ_NREC] = { "Gold", "Stone", "Food", "Wood" };
	static const char *const d[TZ_NREC] = { "oro", "piedra", "comida", "madera" };
	return tz_ui_txt(u->skin, "viewmsg", k[r & 3], d[r & 3]);
}

static void aviso_neg(TzUi *u, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(u->jan.msg, sizeof u->jan.msg, fmt, ap);
	va_end(ap);
	u->jan.msg_ate = u->agora_ms + 5000;
}

static void texto(TzUi *u, TzTela *t, int x, int y, uint32_t cor, const char *fmt, ...)
{
	if (!t) return;
	char b[200];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(b, sizeof b, fmt, ap);
	va_end(ap);
	tz_ui_fonte_texto(t, &u->skin->f12b, x, y, b, cor);
}

static void texto_c(TzUi *u, TzTela *t, TzRect r, uint32_t cor, const char *fmt, ...)
{
	if (!t) return;
	char b[200];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(b, sizeof b, fmt, ap);
	va_end(ap);
	tz_ui_fonte_caixa(t, &u->skin->f12b, r, b, cor, 1);
}

/* un botón: se dibuja (si hay lienzo) y, si está activo, se anota para la entrada */
static void btn(TzUi *u, TzTela *t, TzRect r, int id, const char *txt, int ativo)
{
	if (ativo && u->njw < TZ_UI_JW) { u->jw[u->njw].r = r; u->jw[u->njw].id = id; u->njw++; }
	if (!t) return;
	int sobre = ativo && tz_rect_dentro(r, u->mx, u->my);
	tz_tela_preencher(t, r, !ativo ? 0xff2a2218 : sobre ? 0xff584830 : 0xff403420);
	tz_tela_moldura(t, r, ativo ? 0xff907850 : 0xff504830);
	tz_ui_fonte_caixa(t, &u->skin->f12b, r, txt, !ativo ? COR_APAGADO : sobre ? 0xffffe0 : COR_TEXTO, 1);
}

static void titulo(TzUi *u, TzTela *t, TzRect j, const char *txt)
{
	if (t) tz_ui_fonte_caixa(t, &u->skin->f14b, (TzRect){ j.x, j.y + 12, j.w, 20 }, txt, COR_TITULO, 1);
}

/* icono del recurso (ICONS.INI) a 20 píxeles de lado; sin él, solo el nombre */
static void icono_rec(TzUi *u, TzTela *t, int r, int x, int y)
{
	static const char *const k[TZ_NREC] = { "gold", "stone", "food", "wood" };
	const TzImg *im; TzRect src;
	if (t && tz_ui_icone(u->skin, k[r & 3], -1, &im, &src)) tz_tela_imagem_escala(t, im, src, (TzRect){ x, y, 20, 20 });
}

static void custo_txt(const TzUi *u, const uint32_t c[TZ_NREC], char *out, size_t n)
{
	out[0] = 0;
	for (int r = 0; r < TZ_NREC; ++r)
		if (c[r]) snprintf(out + strlen(out), n - strlen(out), "%s%s %u", out[0] ? ", " : "", nome_rec(u, r), (unsigned)c[r]);
	if (!out[0]) snprintf(out, n, "%s 0", nome_rec(u, 0));
}

static int pode_pagar(const TzUi *u, const uint32_t c[TZ_NREC])
{
	int32_t tem[TZ_NREC];
	tz_mundo_recursos_compartidos(u->m, u->m->jogador, tem);
	for (int r = 0; r < TZ_NREC; ++r) if (tem[r] < (int32_t)c[r]) return 0;
	return 1;
}

static const char *nome_classe(const TzUi *u, int classe)
{
	return tz_ui_txt(u->skin, "objects", tz_classes[classe], tz_classes[classe]);
}

static int pesquisado(const TzUi *u, const char *nome)
{
	int tec = tz_catalogo_tec(u->m->cat, nome);
	const TzReinoEstado *r = &u->m->reino[u->m->jogador];
	return tec >= 0 && r->pesquisas && r->pesquisas[tec];
}

static int dist_cheb(const TzObj *a, const TzObj *b) { int dx = abs(a->cx - b->cx), dy = abs(a->cy - b->cy); return dx > dy ? dx : dy; }

static int enviar(TzUi *u, TzComando *c)
{
	c->reino = (uint8_t)u->m->jogador;
	if (tz_mundo_comando(u->m, c) == 0) { u->n_comandos++; return 1; }
	return 0;
}

static int eh_meu_edificio(const TzUi *u, TzId id, int classe)
{
	if (id < 0 || id >= u->m->nobj) return 0;
	const TzObj *o = &u->m->obj[id];
	return o->classe == classe && o->dono == u->m->jogador && tz_obj_vivo(o) && !(o->flags & TZ_F_OBRA);
}

/* ------------------------------------------------------------------ abrir */
static int classe_oficina(void) { return tz_catalogo_classe("magicworkshop"); }

static TzId primeiro_sel(const TzUi *u, int classe)
{
	for (int i = 0; i < u->sel.n; ++i) if (eh_meu_edificio(u, u->sel.id[i], classe)) return u->sel.id[i];
	return TZ_NENHUM;
}

static TzId taberna_do_reino(const TzUi *u)
{
	TzId t = primeiro_sel(u, TZC_TAVERN);
	if (t >= 0) return t;
	for (int q = 0; q < u->m->nobj; ++q) if (eh_meu_edificio(u, q, TZC_TAVERN)) return q;
	return TZ_NENHUM;
}

int tz_ui_janela_abrir(TzUi *u, int janela, TzId obj)
{
	TzMundo *m = u->m;
	TzId o = obj;
	switch (janela) {
	case TZ_UJ_TABERNA: case TZ_UJ_DADOS: case TZ_UJ_PRESTAMOS: case TZ_UJ_INTERCAMBIO:
		if (!eh_meu_edificio(u, o, TZC_TAVERN)) o = taberna_do_reino(u);
		if (o < 0) { tz_ui_aviso(u, "No hay taberna"); return -1; }
		break;
	case TZ_UJ_CRUZADA: if (!eh_meu_edificio(u, o, TZC_CATHEDRAL)) o = primeiro_sel(u, TZC_CATHEDRAL); if (o < 0) { tz_ui_aviso(u, "No hay catedral"); return -1; } break;
	case TZ_UJ_FABRICA: if (!eh_meu_edificio(u, o, classe_oficina())) o = primeiro_sel(u, classe_oficina()); if (o < 0) { tz_ui_aviso(u, "No hay fábrica mágica"); return -1; } break;
	case TZ_UJ_MERCADO: if (!eh_meu_edificio(u, o, TZC_MARKET)) o = primeiro_sel(u, TZC_MARKET); if (o < 0) { tz_ui_aviso(u, "No hay mercado"); return -1; } break;
	case TZ_UJ_OBJETOS:
		if (o < 0 || o >= m->nobj || !tz_mundo_capacidade_itens(m, o)) {
			o = TZ_NENHUM;
			for (int i = 0; i < u->sel.n && o < 0; ++i) if (tz_mundo_capacidade_itens(m, u->sel.id[i]) && m->obj[u->sel.id[i]].dono == m->jogador) o = u->sel.id[i];
		}
		if (o < 0) { tz_ui_aviso(u, "La unidad no lleva objetos"); return -1; }
		break;
	default: break;
	}
	struct TzJan antes = u->jan;
	memset(&u->jan, 0, sizeof u->jan);
	u->jan.obj = o;
	u->jan.mkt[0] = u->jan.mkt[1] = TZ_NENHUM;
	u->jan.quant = 500; u->jan.rec[0] = TZ_OURO; u->jan.rec[1] = TZ_MADEIRA;
	u->jan.dado_quant = 50;
	u->jan.item = 0; u->jan.tributo_quant = 100; u->jan.fab_ouro = 100; u->jan.fab_item = 0;
	u->jan.dado_valor = 0; u->jan.dado_cara = 0;
	if (janela == TZ_UJ_DADOS) u->jan.dado_quant = antes.dado_quant >= 50 ? antes.dado_quant : 50;
	if (janela == TZ_UJ_COMERCIO) {
		/* por defecto: el mercado propio más cercano a la unidad de origen y el más lejano de destino */
		const TzObj *c0 = NULL;
		for (int i = 0; i < u->sel.n && !c0; ++i) if (tz_mundo_capacidade_comercio(m, u->sel.id[i])) c0 = &m->obj[u->sel.id[i]];
		if (c0) {
			int mel = 1 << 30, pior = -1;
			for (int q = 0; q < m->nobj; ++q) {
				const TzObj *k = &m->obj[q];
				if ((k->classe != TZC_MARKET && k->classe != TZC_HARBOR) || k->dono != m->jogador || !tz_obj_vivo(k) || (k->flags & TZ_F_OBRA)) continue;
				int d = dist_cheb(c0, k);
				if (d < mel) { mel = d; u->jan.mkt[0] = q; }
				if (d > pior) { pior = d; u->jan.mkt[1] = q; }
			}
			if (u->jan.mkt[0] == u->jan.mkt[1]) u->jan.mkt[1] = TZ_NENHUM;
			u->jan.quant = 50; u->jan.rec[0] = TZ_OURO; u->jan.rec[1] = TZ_OURO;
		}
	}
	if (janela == TZ_UJ_FABRICA) { u->jan.cat = 0; u->jan.fab_item = 0; u->jan.fab_ouro = 200; }
	u->janela = janela;
	u->njw = 0;
	return 0;
}

/* ------------------------------------------------------------------ pestañas de la taberna */
static const char *const ABAS[5] = { "Mercenarios", "Héroes", "Dados", "Préstamo", "Intercambio" };
static const int ABA_JANELA[5] = { TZ_UJ_TABERNA, TZ_UJ_TABERNA, TZ_UJ_DADOS, TZ_UJ_PRESTAMOS, TZ_UJ_INTERCAMBIO };

static int aba_atual(const TzUi *u)
{
	if (u->janela == TZ_UJ_TABERNA) return u->jan.aba ? 1 : 0;
	return u->janela == TZ_UJ_DADOS ? 2 : u->janela == TZ_UJ_PRESTAMOS ? 3 : 4;
}

static void abas(TzUi *u, TzTela *t, TzRect j, int base_titulo)
{
	(void)base_titulo;
	int n = 5, w = (j.w - 40) / n, a = aba_atual(u);
	for (int i = 0; i < n; ++i) {
		TzRect r = { j.x + 20 + i * w, j.y + 40, w - 4, 24 };
		btn(u, t, r, TZ_W_ABA0 + i, ABAS[i], i != a);
		if (t && i == a) { tz_tela_preencher(t, r, 0xff706040); tz_tela_moldura(t, r, 0xffe0c070); tz_ui_fonte_caixa(t, &u->skin->f12b, r, ABAS[i], COR_BRANCO, 1); }
	}
}

static void mudar_aba(TzUi *u, int i)
{
	TzId o = u->jan.obj;
	int dq = u->jan.dado_quant;
	int cara = u->jan.dado_cara; uint32_t rolando = u->jan.dado_ms;
	if (tz_ui_janela_abrir(u, ABA_JANELA[i], o) == 0) {
		u->jan.aba = i == 1;
		u->jan.dado_quant = dq >= 50 ? dq : 50;
		u->jan.dado_cara = cara; u->jan.dado_ms = rolando;
	}
}

/* ------------------------------------------------------------------ taberna: mercenarios y héroes */
static void taberna(TzUi *u, TzTela *t, TzRect j)
{
	TzMundo *m = u->m;
	titulo(u, t, j, tz_ui_txt(u->skin, "objects", "tavern", "Taberna"));
	abas(u, t, j, 0);
	struct TzOferta of[8];
	int lista = u->jan.aba ? 1 : 0, n = tz_mundo_ofertas(m, u->jan.obj, lista, of, 8);
	texto(u, t, j.x + 24, j.y + 74, COR_APAGADO, "%s", lista ? "Héroes que se ofrecen (nivel de partida y precio)" : "Mercenarios que se ofrecen (número y precio)");
	if (!n) texto_c(u, t, (TzRect){ j.x + 20, j.y + 160, j.w - 40, 20 }, COR_APAGADO, "%s",
	                lista ? "No hay héroes: investiga «Heroes Quest»" : "No hay mercenarios: investiga «Mercenaries»");
	for (int i = 0; i < n; ++i) {
		TzRect f = { j.x + 20, j.y + 96 + i * 36, j.w - 40, 32 };
		if (t) { tz_tela_preencher(t, f, 0x60000000); tz_tela_moldura(t, f, 0xff504830); }
		const TzImg *ic; TzRect src;
		if (t && tz_ui_icone_classe(u->skin, u->bib, of[i].classe, of[i].civ, m->reino[m->jogador].cor, &ic, &src)) tz_tela_imagem_escala(t, ic, src, (TzRect){ f.x + 4, f.y + 2, 28, 28 });
		uint32_t preco[TZ_NREC];
		tz_mundo_preco_oferta_reino(m, m->jogador, &of[i], preco);
		char ptxt[120];
		custo_txt(u, preco, ptxt, sizeof ptxt);
		int ok = pode_pagar(u, preco);
		if (lista) texto(u, t, f.x + 38, f.y + 2, COR_BRANCO, "%s (nivel %d)", nome_classe(u, of[i].classe), of[i].nivel);
		else texto(u, t, f.x + 38, f.y + 2, COR_BRANCO, "%d x %s", of[i].quantidade, nome_classe(u, of[i].classe));
		texto(u, t, f.x + 38, f.y + 16, ok ? COR_TEXTO : COR_MAL, "%s", ptxt);
		btn(u, t, (TzRect){ f.x + f.w - 100, f.y + 3, 96, 26 }, TZ_W_LINHA + i, "Contratar", 1);
	}
	texto(u, t, j.x + 24, j.y + j.h - 66, COR_APAGADO, "Población: %d/%d", m->reino[m->jogador].populacao, m->reino[m->jogador].populacao_max);
	btn(u, t, (TzRect){ j.x + (j.w - 140) / 2, j.y + j.h - 40, 140, 27 }, TZ_W_FECHAR, tz_ui_txt(u->skin, "general", "close", "Cerrar"), 1);
}

static void taberna_clique(TzUi *u, int id)
{
	if (id < TZ_W_LINHA || id >= TZ_W_LINHA + 8) return;
	TzMundo *m = u->m;
	struct TzOferta of[8];
	int n = tz_mundo_ofertas(m, u->jan.obj, u->jan.aba ? 1 : 0, of, 8), i = id - TZ_W_LINHA;
	if (i >= n) return;
	uint32_t preco[TZ_NREC];
	tz_mundo_preco_oferta_reino(m, m->jogador, &of[i], preco);
	if (!pode_pagar(u, preco)) { aviso_neg(u, "No hay suficientes recursos"); return; }
	if (m->reino[m->jogador].populacao >= m->reino[m->jogador].populacao_max) { aviso_neg(u, "%s", tz_ui_txt(u->skin, "viewmsg", "poplimit2", "No hay suficientes casas")); return; }
	TzComando c;
	memset(&c, 0, sizeof c);
	c.tipo = TZ_C_CONTRATAR; c.arg = (int16_t)of[i].id; c.nids = 1; c.ids[0] = u->jan.obj;
	if (enviar(u, &c)) aviso_neg(u, "Contratado: %s", nome_classe(u, of[i].classe));
}

/* ------------------------------------------------------------------ dados (D_GAMBLE) */
static const int DADO_CHANCE[4] = { 50, 33, 20, 10 }, DADO_PAGA[4] = { 1, 2, 5, 10 };
#define DADO_QUADROS 15
#define DADO_QUADRO_MS 70

static void dados(TzUi *u, TzTela *t, TzRect j)
{
	TzMundo *m = u->m;
	const int32_t *rec = m->reino[m->jogador].recursos;
	titulo(u, t, j, tz_ui_txt(u->skin, "orders_names", "gamble", "Apostar"));
	abas(u, t, j, 0);
	int rolando = u->agora_ms < u->jan.dado_ms;
	int r = u->jan.rec[0] & 3;
	/* la puesta cabe en lo que hay, de 50 en 50 (clamp_stake 0x4a3b50) */
	if (u->jan.dado_quant > rec[r]) u->jan.dado_quant = (int)(rec[r] / 50 * 50);
	if (u->jan.dado_quant < 50) u->jan.dado_quant = 50;
	int tem = rec[r] >= 50;
	int y = j.y + 80;
	texto(u, t, j.x + 24, y, COR_APAGADO, "%s", tz_ui_txt(u->skin, "orders_add", "gm_res", "Recurso que se apuesta"));
	icono_rec(u, t, r, j.x + 24, y + 18);
	btn(u, t, (TzRect){ j.x + 50, y + 16, 150, 26 }, TZ_W_REC_A, nome_rec(u, r), !rolando);
	texto(u, t, j.x + 224, y, COR_APAGADO, "%s", tz_ui_txt(u->skin, "orders_add", "gm_amount", "Cantidad que se apuesta"));
	btn(u, t, (TzRect){ j.x + 224, y + 16, 34, 26 }, TZ_W_Q_MENOS, "-", !rolando && tem);
	{ char b[24]; snprintf(b, sizeof b, "%d", u->jan.dado_quant); texto_c(u, t, (TzRect){ j.x + 262, y + 16, 80, 26 }, COR_BRANCO, "%s", b); }
	btn(u, t, (TzRect){ j.x + 346, y + 16, 34, 26 }, TZ_W_Q_MAIS, "+", !rolando && tem);
	y += 58;
	texto(u, t, j.x + 24, y, COR_APAGADO, "%s", tz_ui_txt(u->skin, "orders_add", "gm_prop", "Probabilidad"));
	char pb[80];
	snprintf(pb, sizeof pb, "Gana el %d %% y paga x%d", DADO_CHANCE[u->jan.prop & 3], DADO_PAGA[u->jan.prop & 3]);
	btn(u, t, (TzRect){ j.x + 24, y + 16, 260, 26 }, TZ_W_PROP, pb, !rolando && tem);
	/* los dados mientras ruedan; después, la cara del resultado */
	TzRect area = { j.x + 24, y + 54, 99, 48 };
	if (t) {
		const TzImg *dd = rolando ? tz_ui_img(u->skin, "SCREENS/D_GAMBLE/ZAROVE", -1) : NULL;
		const TzImg *cara = !rolando ? tz_ui_img(u->skin, "SCREENS/D_GAMBLE/WINLOSE", -1) : NULL;
		if (dd) {
			uint32_t el = u->jan.dado_ms - u->agora_ms, tot = DADO_QUADROS * DADO_QUADRO_MS;
			int fr = (int)(((tot > el ? tot - el : 0) / DADO_QUADRO_MS) % DADO_QUADROS);
			int fw = dd->w / DADO_QUADROS;
			tz_tela_imagem_escala(t, dd, (TzRect){ fw * fr, 0, fw, dd->h }, (TzRect){ area.x, area.y, 93, 42 });
		} else if (cara) {
			int fh = cara->h / 3;
			tz_tela_imagem_escala(t, cara, (TzRect){ 0, fh * (u->jan.dado_cara % 3), cara->w, fh }, area);
		} else {
			tz_tela_moldura(t, area, 0xff504830);
			texto_c(u, t, area, COR_APAGADO, "%s", rolando ? "..." : u->jan.dado_cara == 2 ? "Ganas" : u->jan.dado_cara == 1 ? "Pierdes" : "Dados");
		}
	}
	if (!rolando && u->jan.dado_cara)
		texto(u, t, j.x + 140, y + 70, u->jan.dado_cara == 2 ? COR_BIEN : COR_MAL, u->jan.dado_cara == 2 ? "¡Has ganado %d de %s!" : "Has perdido %d de %s", u->jan.dado_valor < 0 ? -u->jan.dado_valor : u->jan.dado_valor, nome_rec(u, u->jan.dado_rec));
	btn(u, t, (TzRect){ j.x + 24, j.y + j.h - 40, 140, 27 }, TZ_W_ACEITAR, tz_ui_txt(u->skin, "orders_names", "gamble", "Apostar"), !rolando && tem);
	btn(u, t, (TzRect){ j.x + j.w - 164, j.y + j.h - 40, 140, 27 }, TZ_W_FECHAR, tz_ui_txt(u->skin, "general", "close", "Cerrar"), !rolando);
}

static void dados_clique(TzUi *u, int id, int dir, int shift)
{
	TzMundo *m = u->m;
	int r = u->jan.rec[0] & 3;
	switch (id) {
	case TZ_W_REC_A: u->jan.rec[0] = (r + 1) & 3; u->jan.dado_quant = 50; break;
	case TZ_W_Q_MENOS: u->jan.dado_quant -= shift ? 200 : 50; break;
	case TZ_W_Q_MAIS: u->jan.dado_quant += shift ? 200 : 50; break;
	case TZ_W_PROP: u->jan.prop = (u->jan.prop + 1) & 3; break;
	case TZ_W_ACEITAR: {
		if (m->reino[m->jogador].recursos[r] < u->jan.dado_quant || u->jan.dado_quant < 50) return;
		TzComando c;
		memset(&c, 0, sizeof c);
		c.tipo = TZ_C_APOSTAR; c.arg = (int16_t)(r | ((u->jan.prop & 3) << 2)); c.valor = u->jan.dado_quant; c.nids = 1; c.ids[0] = u->jan.obj;
		if (enviar(u, &c)) { u->jan.dado_ms = u->agora_ms + DADO_QUADROS * DADO_QUADRO_MS; u->jan.dado_cara = 0; u->jan.dado_rec = r; }
		break;
	}
	}
	(void)dir;
	if (u->jan.dado_quant < 50) u->jan.dado_quant = 50;
}

/* el resultado de la tirada llega por el evento del mundo */
void tz_ui_neg_evento(TzUi *u, const TzEvento *e)
{
	if (e->tipo == TZ_EV_APOSTA) {
		u->jan.dado_valor = e->valor; u->jan.dado_cara = e->valor > 0 ? 2 : 1; u->jan.dado_rec = e->classe & 3;
		if (u->agora_ms >= u->jan.dado_ms) u->jan.dado_ms = u->agora_ms;   /* sin la ventana abierta no hay nada que esperar */
	} else if (e->tipo == TZ_EV_TRIBUTO) tz_ui_aviso(u, "Tributo enviado: %d de %s", e->valor, nome_rec(u, e->classe));
}

/* ------------------------------------------------------------------ préstamos */
static void prestamos(TzUi *u, TzTela *t, TzRect j)
{
	const TzReinoEstado *r = &u->m->reino[u->m->jogador];
	titulo(u, t, j, tz_ui_txt(u->skin, "orders_names", "loan", "Préstamos"));
	abas(u, t, j, 0);
	int y = j.y + 80;
	if (r->emp_rec) {
		int rec = r->emp_rec - 1;
		uint32_t dev[TZ_NREC] = { 0 };
		dev[rec] = r->emp_quantia + 200;
		icono_rec(u, t, rec, j.x + 24, y);
		texto(u, t, j.x + 52, y + 2, COR_BRANCO, "Tienes un préstamo de %u de %s.", (unsigned)r->emp_quantia, nome_rec(u, rec));
		texto(u, t, j.x + 24, y + 30, COR_TEXTO, "Para devolverlo hacen falta %u de %s (200 más).", (unsigned)dev[rec], nome_rec(u, rec));
		texto(u, t, j.x + 24, y + 54, COR_APAGADO, "Al devolverlo, el siguiente préstamo será 200 mayor un minuto después.");
		btn(u, t, (TzRect){ j.x + (j.w - 200) / 2, y + 100, 200, 28 }, TZ_W_ACEITAR, tz_ui_txt(u->skin, "orders_names", "loan_return", "Devolver préstamo"), pode_pagar(u, dev));
	} else {
		texto(u, t, j.x + 24, y, COR_TEXTO, "Se concede un préstamo de %u del recurso que elijas.", (unsigned)r->emp_quantia);
		texto(u, t, j.x + 24, y + 22, COR_APAGADO, "Solo puedes tener uno a la vez; devolverlo cuesta 200 más.");
		static const char *const ch[TZ_NREC] = { "loan_gold", "loan_stone", "loan_food", "loan_wood" };
		for (int q = 0; q < TZ_NREC; ++q) {
			TzRect b = { j.x + 24 + (q & 1) * 220, y + 60 + (q >> 1) * 40, 210, 30 };
			icono_rec(u, t, q, b.x - 0, b.y + 5);
			btn(u, t, (TzRect){ b.x + 24, b.y, b.w - 24, b.h }, TZ_W_LINHA + q, tz_ui_txt(u->skin, "orders_names", ch[q], nome_rec(u, q)), 1);
		}
	}
	btn(u, t, (TzRect){ j.x + (j.w - 140) / 2, j.y + j.h - 40, 140, 27 }, TZ_W_FECHAR, tz_ui_txt(u->skin, "general", "close", "Cerrar"), 1);
}

static void prestamos_clique(TzUi *u, int id)
{
	TzComando c;
	memset(&c, 0, sizeof c);
	c.nids = 1; c.ids[0] = u->jan.obj;
	if (id == TZ_W_ACEITAR) { c.tipo = TZ_C_DEVOLVER; if (enviar(u, &c)) aviso_neg(u, "Préstamo devuelto"); }
	else if (id >= TZ_W_LINHA && id < TZ_W_LINHA + TZ_NREC) { c.tipo = TZ_C_EMPRESTAR; c.arg = (int16_t)(id - TZ_W_LINHA); if (enviar(u, &c)) aviso_neg(u, "Préstamo concedido"); }
}

/* ------------------------------------------------------------------ intercambio (D_TRADE1) */
static const struct TzTroca *troca_activa(const TzUi *u)
{
	for (int q = 0; q < u->m->nobj; ++q) {
		const TzObj *o = &u->m->obj[q];
		if (o->classe == TZC_TAVERN && o->dono == u->m->jogador && o->ator && o->ator->troca.ativa) return &o->ator->troca;
	}
	return NULL;
}

static void intercambio(TzUi *u, TzTela *t, TzRect j)
{
	const TzReinoEstado *r = &u->m->reino[u->m->jogador];
	titulo(u, t, j, tz_ui_txt(u->skin, "orders_names", "trade1", "Cambiar recursos"));
	abas(u, t, j, 0);
	int a = u->jan.rec[0] & 3, b = u->jan.rec[1] & 3, q = u->jan.quant;
	if (q < 500) q = 500;
	if (q > 20000) q = 20000;
	u->jan.quant = q;
	int y = j.y + 82;
	texto(u, t, j.x + 24, y, COR_APAGADO, "%s", tz_ui_txt(u->skin, "orders_add", "tr1_srcres", "Recurso que entregas"));
	icono_rec(u, t, a, j.x + 24, y + 18);
	btn(u, t, (TzRect){ j.x + 50, y + 16, 150, 26 }, TZ_W_REC_A, nome_rec(u, a), 1);
	texto(u, t, j.x + 240, y, COR_APAGADO, "%s", tz_ui_txt(u->skin, "orders_add", "tr1_amount1", "Cuánto ofreces"));
	btn(u, t, (TzRect){ j.x + 240, y + 16, 34, 26 }, TZ_W_Q_MENOS, "-", 1);
	texto_c(u, t, (TzRect){ j.x + 278, y + 16, 80, 26 }, r->recursos[a] >= q ? COR_BRANCO : COR_MAL, "%d", q);
	btn(u, t, (TzRect){ j.x + 362, y + 16, 34, 26 }, TZ_W_Q_MAIS, "+", 1);
	y += 56;
	texto(u, t, j.x + 24, y, COR_APAGADO, "%s", tz_ui_txt(u->skin, "orders_add", "tr1_dstres", "Recurso que obtienes"));
	icono_rec(u, t, b, j.x + 24, y + 18);
	btn(u, t, (TzRect){ j.x + 50, y + 16, 150, 26 }, TZ_W_REC_B, nome_rec(u, b), 1);
	texto(u, t, j.x + 240, y, COR_APAGADO, "%s", tz_ui_txt(u->skin, "orders_add", "tr1_amount2", "Cuánto pides"));
	texto_c(u, t, (TzRect){ j.x + 240, y + 16, 156, 26 }, COR_TEXTO, "%d", q / 10);
	y += 62;
	const struct TzTroca *tr = troca_activa(u);
	if (tr) texto(u, t, j.x + 24, y, COR_BIEN, "Intercambio en marcha: %u de %u entregados.", (unsigned)tr->entregue, (unsigned)tr->total);
	else texto(u, t, j.x + 24, y, COR_APAGADO, "Recibes una décima parte, una unidad cada 0,3 segundos. Un intercambio a la vez.");
	btn(u, t, (TzRect){ j.x + 24, j.y + j.h - 40, 200, 27 }, TZ_W_ACEITAR, tz_ui_txt(u->skin, "orders_add", "tr1_trade", "Comenzar intercambio"), !tr && r->recursos[a] >= q);
	btn(u, t, (TzRect){ j.x + j.w - 164, j.y + j.h - 40, 140, 27 }, TZ_W_FECHAR, tz_ui_txt(u->skin, "general", "close", "Cerrar"), 1);
}

static void intercambio_clique(TzUi *u, int id, int shift)
{
	switch (id) {
	case TZ_W_REC_A: u->jan.rec[0] = (u->jan.rec[0] + 1) & 3; break;
	case TZ_W_REC_B: u->jan.rec[1] = (u->jan.rec[1] + 1) & 3; break;
	case TZ_W_Q_MENOS: u->jan.quant -= shift ? 1000 : 500; break;
	case TZ_W_Q_MAIS: u->jan.quant += shift ? 1000 : 500; break;
	case TZ_W_ACEITAR: {
		TzComando c;
		memset(&c, 0, sizeof c);
		c.tipo = TZ_C_TROCAR; c.arg = (int16_t)((u->jan.rec[0] & 3) | ((u->jan.rec[1] & 3) << 2)); c.valor = u->jan.quant; c.nids = 1; c.ids[0] = u->jan.obj;
		if (enviar(u, &c)) aviso_neg(u, "Intercambio iniciado");
		break;
	}
	}
	if (u->jan.quant < 500) u->jan.quant = 500;
	if (u->jan.quant > 20000) u->jan.quant = 20000;
}

/* ------------------------------------------------------------------ comercio: ruta de mercaderes (D_TRADE) */
static int comerciantes(const TzUi *u, TzId *ids, int max)
{
	int n = 0;
	for (int i = 0; i < u->sel.n && n < max; ++i) {
		const TzObj *o = &u->m->obj[u->sel.id[i]];
		if (o->dono == u->m->jogador && tz_obj_vivo(o) && tz_mundo_capacidade_comercio(u->m, u->sel.id[i])) ids[n++] = u->sel.id[i];
	}
	return n;
}

static void comercio(TzUi *u, TzTela *t, TzRect j)
{
	TzMundo *m = u->m;
	TzId ids[TZ_SEL_MAX];
	int n = comerciantes(u, ids, TZ_SEL_MAX);
	titulo(u, t, j, tz_ui_txt(u->skin, "orders_names", "trade", "Comercio"));
	if (!n) { texto_c(u, t, (TzRect){ j.x + 20, j.y + 100, j.w - 40, 20 }, COR_MAL, "Selecciona un mercader (o un peón asiático)"); btn(u, t, (TzRect){ j.x + (j.w - 140) / 2, j.y + j.h - 40, 140, 27 }, TZ_W_FECHAR, tz_ui_txt(u->skin, "general", "close", "Cerrar"), 1); return; }
	uint32_t cap = tz_mundo_capacidade_comercio(m, ids[0]);
	if (u->jan.quant < 50) u->jan.quant = 50;
	if (u->jan.quant > (int)cap) u->jan.quant = (int)cap;
	int y = j.y + 50;
	for (int k = 0; k < 2; ++k) {
		TzId mk = u->jan.mkt[k];
		int ok = mk >= 0 && mk < m->nobj && tz_obj_vivo(&m->obj[mk]);
		texto(u, t, j.x + 24, y, COR_APAGADO, "%s", tz_ui_txt(u->skin, "orders_add", k ? "tr_dstobj" : "tr_srcobj", k ? "Destino" : "Origen"));
		char b[80];
		if (ok) snprintf(b, sizeof b, "%s en (%d,%d)", nome_classe(u, m->obj[mk].classe), m->obj[mk].cx, m->obj[mk].cy);
		else snprintf(b, sizeof b, "(elige un mercado)");
		texto(u, t, j.x + 24, y + 20, ok ? COR_BRANCO : COR_MAL, "%s", b);
		btn(u, t, (TzRect){ j.x + 270, y + 12, 120, 26 }, k ? TZ_W_ESCOLHER_B : TZ_W_ESCOLHER_A, "Elegir en el mapa", 1);
		texto(u, t, j.x + 400, y, COR_APAGADO, "%s", tz_ui_txt(u->skin, "orders_add", k ? "tr_dstres" : "tr_srcres", k ? "Traer recursos" : "Llevar recursos"));
		icono_rec(u, t, u->jan.rec[k] & 3, j.x + 400, y + 16);
		btn(u, t, (TzRect){ j.x + 424, y + 12, 90, 26 }, k ? TZ_W_REC_B : TZ_W_REC_A, nome_rec(u, u->jan.rec[k] & 3), 1);
		y += 60;
	}
	texto(u, t, j.x + 24, y, COR_APAGADO, "%s", tz_ui_txt(u->skin, "orders_add", "tr_amount", "Establecer cantidad"));
	btn(u, t, (TzRect){ j.x + 24, y + 16, 34, 26 }, TZ_W_Q_MENOS, "-", 1);
	texto_c(u, t, (TzRect){ j.x + 62, y + 16, 80, 26 }, COR_BRANCO, "%d", u->jan.quant);
	btn(u, t, (TzRect){ j.x + 146, y + 16, 34, 26 }, TZ_W_Q_MAIS, "+", 1);
	texto(u, t, j.x + 196, y + 20, COR_APAGADO, "Carga máxima por viaje: %u", (unsigned)cap);
	int dos = u->jan.mkt[0] >= 0 && u->jan.mkt[1] >= 0 && u->jan.mkt[0] != u->jan.mkt[1];
	y += 56;
	if (dos) {
		uint32_t g = tz_mundo_lucro_rota(m, m->jogador, u->jan.mkt[0], u->jan.mkt[1], (uint32_t)u->jan.quant);
		texto(u, t, j.x + 24, y, g ? COR_BIEN : COR_APAGADO, g ? "Ganancia estimada por viaje: +%u" : "Los mercados están demasiado cerca: sin ganancia", (unsigned)g);
	} else texto(u, t, j.x + 24, y, COR_APAGADO, "Hacen falta dos mercados o puertos distintos.");
	texto(u, t, j.x + 24, y + 22, COR_APAGADO, "%d mercader%s con esta ruta", n, n == 1 ? "" : "es");
	btn(u, t, (TzRect){ j.x + 24, j.y + j.h - 40, 200, 27 }, TZ_W_ACEITAR, tz_ui_txt(u->skin, "orders_add", "tr_trade", "Iniciar compra-venta"), dos);
	btn(u, t, (TzRect){ j.x + j.w - 164, j.y + j.h - 40, 140, 27 }, TZ_W_FECHAR, tz_ui_txt(u->skin, "orders_add", "tr_cancel", "Cancelar"), 1);
}

static void esperar_mapa(TzUi *u, int pick)
{
	u->pick = pick; u->janela_pick = u->janela; u->janela = 0;
	u->modo = TZ_UI_ALVO; u->tipo_alvo = TZ_O_NADA; u->acao_alvo[0] = 0;
}

static void comercio_clique(TzUi *u, int id, int shift)
{
	TzMundo *m = u->m;
	TzId ids[TZ_SEL_MAX];
	int n = comerciantes(u, ids, TZ_SEL_MAX);
	switch (id) {
	case TZ_W_REC_A: u->jan.rec[0] = (u->jan.rec[0] + 1) & 3; break;
	case TZ_W_REC_B: u->jan.rec[1] = (u->jan.rec[1] + 1) & 3; break;
	case TZ_W_Q_MENOS: u->jan.quant -= shift ? 100 : 50; break;
	case TZ_W_Q_MAIS: u->jan.quant += shift ? 100 : 50; break;
	case TZ_W_ESCOLHER_A: esperar_mapa(u, TZ_PICK_MERCADO0); tz_ui_aviso(u, "Pulsa sobre el mercado o puerto de origen"); break;
	case TZ_W_ESCOLHER_B: esperar_mapa(u, TZ_PICK_MERCADO1); tz_ui_aviso(u, "Pulsa sobre el mercado o puerto de destino"); break;
	case TZ_W_ACEITAR: {
		if (!n || u->jan.mkt[0] < 0 || u->jan.mkt[1] < 0 || u->jan.mkt[0] == u->jan.mkt[1]) return;
		TzOrdem o;
		memset(&o, 0, sizeof o);
		o.tipo = TZ_O_COMERCIAR; o.alvo = u->jan.mkt[1]; o.x2 = m->obj[u->jan.mkt[0]].cx; o.y2 = m->obj[u->jan.mkt[0]].cy;
		o.arg = TZ_ARG_COMERCIO(u->jan.rec[0] & 3, u->jan.rec[1] & 3, u->jan.quant);
		TzComando c;
		memset(&c, 0, sizeof c);
		c.tipo = TZ_C_ORDEM; c.ordem = o;
		for (int i = 0; i < n && c.nids < TZ_CMD_IDS; ++i) c.ids[c.nids++] = ids[i];
		if (enviar(u, &c)) { aviso_neg(u, "Ruta comercial iniciada"); u->janela = 0; }
		break;
	}
	}
	TzId i0 = n ? ids[0] : TZ_NENHUM;
	uint32_t cap = i0 >= 0 ? tz_mundo_capacidade_comercio(m, i0) : 500;
	if (u->jan.quant < 50) u->jan.quant = 50;
	if (u->jan.quant > (int)cap) u->jan.quant = (int)cap;
}

/* un clic sobre el mapa mientras se espera un objetivo de ventana (mercado de la ruta, destino de la cruzada) */
int tz_ui_neg_pick(TzUi *u, int px, int py)
{
	TzMundo *m = u->m;
	int cx = px / TZ_CASILLA, cy = py / TZ_CASILLA;
	if (px < 0 || py < 0 || cx >= m->lado || cy >= m->lado) return 0;
	if (u->pick == TZ_PICK_CRUZADA) {
		TzComando c;
		memset(&c, 0, sizeof c);
		c.tipo = TZ_C_CRUZADA; c.x = (int16_t)cx; c.y = (int16_t)cy; c.nids = 1; c.ids[0] = u->jan.obj;
		if (u->jan.obj >= 0 && enviar(u, &c)) tz_ui_aviso(u, "Cruzada convocada");
		u->pick = 0; u->modo = TZ_UI_NORMAL; u->janela = u->janela_pick;
		return 1;
	}
	TzId alvo = tz_ui_pick(m, px, py);
	if (alvo < 0 || (m->obj[alvo].classe != TZC_MARKET && m->obj[alvo].classe != TZC_HARBOR) || m->obj[alvo].dono != m->jogador) {
		tz_ui_aviso(u, "Elige un mercado o puerto tuyo");
		return 1;
	}
	u->jan.mkt[u->pick == TZ_PICK_MERCADO1] = alvo;
	u->pick = 0; u->modo = TZ_UI_NORMAL; u->janela = u->janela_pick;
	return 1;
}

/* ------------------------------------------------------------------ objetos del héroe */
static void item_texto(const TzUi *u, const TzObj *o, char *name, size_t nn, char *det, size_t nd)
{
	const char *img = o->item.imagem[0] ? o->item.imagem : o->original ? o->original->imagem : "";
	const char *cl = nome_classe(u, o->classe);
	snprintf(name, nn, "%s", img[0] ? tz_ui_txt(u->skin, "images", img, cl) : cl);
	det[0] = 0;
	const TzItem *it = &o->item;
	switch (o->classe) {
	case TZC_ITEM_MAIS:
		snprintf(det, nd, "Equipo: ataque +%d, defensa +%d (mientras se lleva)", it->ataque, it->defesa);
		break;
	case TZC_POCAO: {
		char *p = det;
		p += snprintf(p, nd, "Poción:");
		if (it->vida) p += snprintf(p, nd - (size_t)(p - det), " vida +%d", it->vida);
		if (it->mana) p += snprintf(p, nd - (size_t)(p - det), " maná +%d", it->mana);
		if (it->ataque) p += snprintf(p, nd - (size_t)(p - det), " ataque +%d", it->ataque);
		if (it->defesa) p += snprintf(p, nd - (size_t)(p - det), " defensa +%d", it->defesa);
		if (it->xp) snprintf(p, nd - (size_t)(p - det), " experiencia +%d", it->xp);
		break;
	}
	case TZC_LIVRO: {
		const char *k = tz_feitico_chave(it->feitico);
		const char *nk = k ? tz_ui_txt(u->skin, "spell_names", k, k) : "?";
		if (it->finito) snprintf(det, nd, "Libro: %s, cargas %d", nk, it->cargas);
		else snprintf(det, nd, "Libro: %s", nk);
		break;
	}
	case TZC_PERGAMINHO: snprintf(det, nd, "Pergamino con texto"); break;
	case TZC_ITEM_ALMA: {
		const TzId b = it->ligado - 1;
		snprintf(det, nd, "Piedra del alma: %s", b >= 0 && b < u->m->nobj && tz_obj_vivo(&u->m->obj[b]) ? nome_classe(u, u->m->obj[b].classe) : "(ya no existe)");
		break;
	}
	default: break;
	}
}

/* quién puede recibir un objeto: unidades propias con sitio a diez casillas del que lo lleva */
static int receptores(const TzUi *u, TzId de, TzId *out, int max)
{
	const TzMundo *m = u->m;
	int n = 0;
	const TzObj *h = &m->obj[de];
	for (int q = 0; q < m->nobj && n < max; ++q) {
		const TzObj *o = &m->obj[q];
		if (q == de || o->dono != m->jogador || !tz_obj_vivo(o) || !tz_obj_unidade(o) || (o->flags & TZ_F_DENTRO)) continue;
		if (!tz_mundo_capacidade_itens(m, q) || (o->ator && o->ator->nitens >= tz_mundo_capacidade_itens(m, q))) continue;
		if (abs(o->cx - h->cx) > 10 || abs(o->cy - h->cy) > 10) continue;
		out[n++] = q;
	}
	return n;
}

static void objetos(TzUi *u, TzTela *t, TzRect j)
{
	TzMundo *m = u->m;
	TzId h = u->jan.obj;
	titulo(u, t, j, "Objetos");
	if (h < 0 || h >= m->nobj || !tz_obj_vivo(&m->obj[h])) { btn(u, t, (TzRect){ j.x + (j.w - 140) / 2, j.y + j.h - 40, 140, 27 }, TZ_W_FECHAR, "Cerrar", 1); return; }
	const TzObj *ho = &m->obj[h];
	int cap = tz_mundo_capacidade_itens(m, h);
	TzId its[16];
	int n = tz_mundo_itens(m, h, its, 16);
	const char *nh = tz_heroi_nome(m, ho);
	texto(u, t, j.x + 24, j.y + 36, COR_BRANCO, "%s - lleva %d de %d objetos", nh ? nh : nome_classe(u, ho->classe), n, cap);
	texto(u, t, j.x + 24, j.y + 54, COR_APAGADO, "ataque %d  defensa %d  vida %d/%d", ho->ataque, ho->defesa, ho->vida, ho->vida_max);
	TzId rec[8];
	int nr = receptores(u, h, rec, 8);
	if (u->jan.dar_idx >= nr) u->jan.dar_idx = 0;
	TzRect rpara = { j.x + j.w - 300, j.y + 32, 276, 24 };
	{
		char b[100];
		if (nr) snprintf(b, sizeof b, "Dar a: %s (%d,%d)", nome_classe(u, m->obj[rec[u->jan.dar_idx]].classe), m->obj[rec[u->jan.dar_idx]].cx, m->obj[rec[u->jan.dar_idx]].cy);
		else snprintf(b, sizeof b, "Nadie cerca para recibir");
		btn(u, t, rpara, TZ_W_PARA, b, nr > 1);
		if (t && nr == 1) { tz_ui_fonte_caixa(t, &u->skin->f12b, rpara, b, COR_TEXTO, 1); tz_tela_moldura(t, rpara, 0xff504830); }
	}
	int y = j.y + 82;
	if (!n) texto(u, t, j.x + 24, y, COR_APAGADO, "No lleva ningún objeto.");
	for (int i = 0; i < n && i < 6; ++i) {
		const TzObj *o = &m->obj[its[i]];
		TzRect f = { j.x + 20, y + i * 38, j.w - 40, 34 };
		if (t) { tz_tela_preencher(t, f, 0x60000000); tz_tela_moldura(t, f, 0xff504830); }
		char nm[80], dt[160];
		item_texto(u, o, nm, sizeof nm, dt, sizeof dt);
		texto(u, t, f.x + 6, f.y + 2, COR_BRANCO, "%s", nm);
		texto(u, t, f.x + 6, f.y + 17, COR_TEXTO, "%s", dt);
		int bx = f.x + f.w - 4;
		bx -= 66; btn(u, t, (TzRect){ bx, f.y + 4, 62, 26 }, TZ_W_LINHA3 + i, "Dar", nr > 0);
		bx -= 66; btn(u, t, (TzRect){ bx, f.y + 4, 62, 26 }, TZ_W_LINHA2 + i, "Soltar", 1);
		if (o->classe == TZC_ITEM_ALMA) {
			bx -= 66; btn(u, t, (TzRect){ bx, f.y + 4, 62, 26 }, TZ_W_LINHA + i, "Matar", 1);
			bx -= 66; btn(u, t, (TzRect){ bx, f.y + 4, 62, 26 }, TZ_W_LINHA + 8 + i, "Furia", 1);
		} else if (o->classe == TZC_ITEM_MAIS) {
			bx -= 66; if (t) tz_ui_fonte_caixa(t, &u->skin->f12b, (TzRect){ bx, f.y + 4, 62, 26 }, "puesto", COR_BIEN, 1);
		} else {
			bx -= 66; btn(u, t, (TzRect){ bx, f.y + 4, 62, 26 }, TZ_W_LINHA + i, o->classe == TZC_POCAO ? "Beber" : o->classe == TZC_LIVRO ? "Aprender" : "Leer", 1);
		}
	}
	/* los del suelo, a seis casillas */
	int y2 = y + 6 * 38 + 6;
	texto(u, t, j.x + 24, y2 - 4, COR_APAGADO, "En el suelo cerca:");
	int ns = 0;
	for (int q = 0; q < m->nobj && ns < 3; ++q) {
		const TzObj *o = &m->obj[q];
		if (!tz_obj_item(o) || (o->flags & (TZ_F_CARREGADO | TZ_F_MORTO | TZ_F_INATIVO))) continue;
		if (abs(o->cx - ho->cx) > 6 || abs(o->cy - ho->cy) > 6) continue;
		char nm[80], dt[160];
		item_texto(u, o, nm, sizeof nm, dt, sizeof dt);
		TzRect f = { j.x + 20, y2 + 14 + ns * 26, j.w - 40, 24 };
		texto(u, t, f.x + 6, f.y + 4, COR_TEXTO, "%s (%d,%d)", nm, o->cx, o->cy);
		btn(u, t, (TzRect){ f.x + f.w - 90, f.y, 86, 24 }, TZ_W_MUNDO + q, "Recoger", n < cap);
		ns++;
	}
	if (!ns) texto(u, t, j.x + 24, y2 + 14, COR_APAGADO, "(nada)");
	btn(u, t, (TzRect){ j.x + (j.w - 140) / 2, j.y + j.h - 38, 140, 27 }, TZ_W_FECHAR, tz_ui_txt(u->skin, "general", "close", "Cerrar"), 1);
}

static void ordem_item(TzUi *u, int tipo, TzId alvo, int arg, int x, int y)
{
	TzOrdem o;
	memset(&o, 0, sizeof o);
	o.tipo = (uint8_t)tipo; o.alvo = alvo; o.arg = (int16_t)arg; o.x = (int16_t)x; o.y = (int16_t)y; o.x2 = o.y2 = -1;
	TzComando c;
	memset(&c, 0, sizeof c);
	c.tipo = TZ_C_ORDEM; c.ordem = o; c.nids = 1; c.ids[0] = u->jan.obj;
	enviar(u, &c);
}

static void objetos_clique(TzUi *u, int id)
{
	TzMundo *m = u->m;
	TzId h = u->jan.obj;
	if (id == TZ_W_PARA) { u->jan.dar_idx++; return; }
	TzId its[16];
	int n = tz_mundo_itens(m, h, its, 16);
	if (id >= TZ_W_MUNDO && id < TZ_W_MUNDO + m->nobj) {
		ordem_item(u, TZ_O_PEGAR_ITEM, id - TZ_W_MUNDO, 0, 0, 0);
		return;
	}
	int fila = -1, qual = 0;
	if (id >= TZ_W_LINHA3 && id < TZ_W_LINHA3 + 8) { fila = id - TZ_W_LINHA3; qual = 3; }
	else if (id >= TZ_W_LINHA2 && id < TZ_W_LINHA2 + 8) { fila = id - TZ_W_LINHA2; qual = 2; }
	else if (id >= TZ_W_LINHA + 8 && id < TZ_W_LINHA + 16) { fila = id - TZ_W_LINHA - 8; qual = 4; }
	else if (id >= TZ_W_LINHA && id < TZ_W_LINHA + 8) { fila = id - TZ_W_LINHA; qual = 1; }
	if (fila < 0 || fila >= n) return;
	TzId item = its[fila];
	const TzObj *o = &m->obj[item];
	if (qual == 1 && o->classe == TZC_ITEM_ALMA) { TzComando c; memset(&c, 0, sizeof c); c.tipo = TZ_C_ALMA; c.arg = 0; c.nids = 1; c.ids[0] = item; enviar(u, &c); return; }
	if (qual == 4) { TzComando c; memset(&c, 0, sizeof c); c.tipo = TZ_C_ALMA; c.arg = 1; c.nids = 1; c.ids[0] = item; enviar(u, &c); return; }
	if (qual == 1) { ordem_item(u, TZ_O_USAR_ITEM, item, 0, 0, 0); return; }
	if (qual == 2) { ordem_item(u, TZ_O_SOLTAR_ITEM, item, 0, 0, 0); return; }
	TzId rc[8];
	int nr = receptores(u, h, rc, 8);
	if (!nr) return;
	TzId para = rc[u->jan.dar_idx % nr];
	ordem_item(u, TZ_O_SOLTAR_ITEM, item, 1, m->obj[para].cx, m->obj[para].cy);
}

/* ------------------------------------------------------------------ cruzada */
static void cruzada(TzUi *u, TzTela *t, TzRect j)
{
	TzMundo *m = u->m;
	const TzReinoEstado *r = &m->reino[m->jogador];
	titulo(u, t, j, tz_ui_txt(u->skin, "orders_names", "preach_crusade", "Convocar Cruzada"));
	int techo = 16 + 4 * r->cruzadas_completas;
	if (techo > 36) techo = 36;
	int marcha = r->cruz_ancla >= 0 && r->cruzados == 0xffff;
	int y = j.y + 56;
	if (marcha) {
		texto(u, t, j.x + 24, y, COR_BIEN, "Hay una cruzada en marcha: faltan %d cruzados por llegar.", r->cruz_faltan);
		texto(u, t, j.x + 24, y + 24, COR_APAGADO, "La catedral volverá a reunir cruzados cuando acabe.");
	} else {
		texto(u, t, j.x + 24, y, COR_BRANCO, "Cruzados reunidos: %d de %d", r->cruzados, techo);
		texto(u, t, j.x + 24, y + 24, COR_APAGADO, "Cada cruzada completa sube el techo en 4 (hasta 36).");
	}
	uint32_t custo[TZ_NREC] = { 0 };
	tz_catalogo_custo_ini(m->cat, TZC_ANCORA, r->civ, custo);
	char b[120];
	custo_txt(u, custo, b, sizeof b);
	texto(u, t, j.x + 24, y + 56, pode_pagar(u, custo) ? COR_TEXTO : COR_MAL, "Coste del estandarte: %s", b);
	btn(u, t, (TzRect){ j.x + 24, y + 96, 250, 28 }, TZ_W_ACEITAR, "Elegir destino y convocar", !marcha && r->cruzados > 0 && pode_pagar(u, custo));
	btn(u, t, (TzRect){ j.x + 24, y + 134, 250, 28 }, TZ_W_SEL_CRUZ, tz_ui_txt(u->skin, "orders_names", "select_crusaders", "Seleccionar Cruzada"), 1);
	btn(u, t, (TzRect){ j.x + (j.w - 140) / 2, j.y + j.h - 40, 140, 27 }, TZ_W_FECHAR, tz_ui_txt(u->skin, "general", "close", "Cerrar"), 1);
}

void tz_ui_neg_selecionar_cruzados(TzUi *u)
{
	tz_ui_sel_limpar(&u->sel);
	for (int q = 0; q < u->m->nobj && u->sel.n < TZ_SEL_MAX; ++q) {
		const TzObj *o = &u->m->obj[q];
		if (o->classe == TZC_CRUSADER && o->dono == u->m->jogador && tz_obj_vivo(o) && !(o->flags & TZ_F_DENTRO)) u->sel.id[u->sel.n++] = q;
	}
	if (!u->sel.n) tz_ui_aviso(u, "No hay cruzados");
	else tz_ui_menu_atualizar(u);
}

static void cruzada_clique(TzUi *u, int id)
{
	if (id == TZ_W_ACEITAR) { esperar_mapa(u, TZ_PICK_CRUZADA); tz_ui_aviso(u, "Pulsa en el mapa el destino de la cruzada"); }
	else if (id == TZ_W_SEL_CRUZ) { u->janela = 0; tz_ui_neg_selecionar_cruzados(u); }
}

/* ------------------------------------------------------------------ tributos y rendición (desde la diplomacia) */
static int reinos_tributo(const TzUi *u, int *ks)
{
	int n = 0;
	for (int k = 1; k <= 8; ++k) if (u->m->reino[k].ativo && k != u->m->jogador && !u->m->reino[k].derrotado) ks[n++] = k;
	return n;
}

static void tributo(TzUi *u, TzTela *t, TzRect j)
{
	TzMundo *m = u->m;
	int ks[8], n = reinos_tributo(u, ks);
	titulo(u, t, j, tz_ui_txt(u->skin, "tribute", "t_title", "ENVIAR RECURSOS"));
	if (!n) { texto_c(u, t, (TzRect){ j.x, j.y + 100, j.w, 20 }, COR_APAGADO, "No hay otros reinos."); btn(u, t, (TzRect){ j.x + (j.w - 140) / 2, j.y + j.h - 40, 140, 27 }, TZ_W_FECHAR, tz_ui_txt(u->skin, "tribute", "cancel", "Cancelar"), 1); return; }
	if (u->jan.item >= n) u->jan.item = 0;
	int k = ks[u->jan.item], r = u->jan.rec[0] & 3;
	int y = j.y + 56;
	texto(u, t, j.x + 24, y, COR_APAGADO, "Reino que recibe");
	if (t) tz_tela_preencher(t, (TzRect){ j.x + 24, y + 22, 14, 14 }, 0xff000000u | u->skin->cores[m->reino[k].cor & 7]);
	btn(u, t, (TzRect){ j.x + 44, y + 16, 260, 26 }, TZ_W_REI, m->reino[k].nome[0] ? m->reino[k].nome : "Reino", n > 1);
	y += 56;
	texto(u, t, j.x + 24, y, COR_APAGADO, "Recurso");
	icono_rec(u, t, r, j.x + 24, y + 18);
	btn(u, t, (TzRect){ j.x + 50, y + 16, 150, 26 }, TZ_W_REC_A, nome_rec(u, r), 1);
	texto(u, t, j.x + 240, y, COR_APAGADO, "Cantidad (tienes %d)", m->reino[m->jogador].recursos[r]);
	btn(u, t, (TzRect){ j.x + 240, y + 16, 34, 26 }, TZ_W_Q_MENOS, "-", 1);
	texto_c(u, t, (TzRect){ j.x + 278, y + 16, 90, 26 }, COR_BRANCO, "%d", u->jan.tributo_quant);
	btn(u, t, (TzRect){ j.x + 372, y + 16, 34, 26 }, TZ_W_Q_MAIS, "+", 1);
	texto(u, t, j.x + 24, y + 60, COR_APAGADO, "Pulsa con Mayús para pasos de 1000. La cantidad se recorta a lo que tengas.");
	btn(u, t, (TzRect){ j.x + 24, j.y + j.h - 40, 140, 27 }, TZ_W_ACEITAR, tz_ui_txt(u->skin, "tribute", "send", "ENVIAR"), m->reino[m->jogador].recursos[r] > 0);
	btn(u, t, (TzRect){ j.x + j.w - 164, j.y + j.h - 40, 140, 27 }, TZ_W_FECHAR, tz_ui_txt(u->skin, "tribute", "cancel", "CANCELAR"), 1);
}

static void tributo_clique(TzUi *u, int id, int shift)
{
	int ks[8], n = reinos_tributo(u, ks);
	if (!n) return;
	switch (id) {
	case TZ_W_REI: u->jan.item = (u->jan.item + 1) % n; break;
	case TZ_W_REC_A: u->jan.rec[0] = (u->jan.rec[0] + 1) & 3; break;
	case TZ_W_Q_MENOS: u->jan.tributo_quant -= shift ? 1000 : 100; break;
	case TZ_W_Q_MAIS: u->jan.tributo_quant += shift ? 1000 : 100; break;
	case TZ_W_ACEITAR: {
		TzComando c;
		memset(&c, 0, sizeof c);
		c.tipo = TZ_C_TRIBUTO; c.arg = (int16_t)ks[u->jan.item % n]; c.x = (int16_t)(u->jan.rec[0] & 3); c.valor = u->jan.tributo_quant;
		if (enviar(u, &c)) aviso_neg(u, "Tributo enviado");
		break;
	}
	}
	if (u->jan.tributo_quant < 100) u->jan.tributo_quant = 100;
	if (u->jan.tributo_quant > 100000) u->jan.tributo_quant = 100000;
}

static void rendicion(TzUi *u, TzTela *t, TzRect j)
{
	titulo(u, t, j, tz_ui_txt(u->skin, "menu", "surr", "RENDIRSE"));
	texto_c(u, t, (TzRect){ j.x + 20, j.y + 56, j.w - 40, 20 }, COR_BRANCO, "¿Seguro que quieres rendirte? Perderás la partida.");
	btn(u, t, (TzRect){ j.x + 40, j.y + j.h - 50, 160, 30 }, TZ_W_SIM, tz_ui_txt(u->skin, "menu", "surr", "RENDIRSE"), 1);
	btn(u, t, (TzRect){ j.x + j.w - 200, j.y + j.h - 50, 160, 30 }, TZ_W_FECHAR, tz_ui_txt(u->skin, "diplomacy", "cancel", "CANCELAR"), 1);
}

/* ------------------------------------------------------------------ fábrica mágica (D_build) */
static const int FAB_N[3] = { 21, 5, 3 };
static const char *const FAB_POCOES[5] = { "Poción de vida", "Poción de experiencia", "Poción de maná", "Poción de ataque", "Poción de defensa" };
static const char *const FAB_PODER[3] = { "Espada", "Arco", "Escudo" };

static int fab_salto(int cat, int i) { return (cat == 0 && (i == 8 || i == 13)) || (cat == 1 && i == 0); }

static void fab_nome(const TzUi *u, int cat, int i, char *out, size_t n)
{
	if (cat == 0) { const char *k = tz_feitico_chave(i); snprintf(out, n, "Libro: %s", k ? tz_ui_txt(u->skin, "spell_names", k, k) : "?"); }
	else if (cat == 1) snprintf(out, n, "%s", FAB_POCOES[i % 5]);
	else snprintf(out, n, "%s", FAB_PODER[i % 3]);
}

static void fabrica(TzUi *u, TzTela *t, TzRect j)
{
	TzMundo *m = u->m;
	titulo(u, t, j, tz_ui_txt(u->skin, "orders_names", "build_item", "Construir objeto mágico"));
	static const char *const cat[3] = { "Libros", "Pociones", "Objetos de poder" };
	for (int i = 0; i < 3; ++i) {
		TzRect r = { j.x + 24 + i * 150, j.y + 44, 144, 26 };
		btn(u, t, r, TZ_W_CAT0 + i, cat[i], i != u->jan.cat);
		if (t && i == u->jan.cat) { tz_tela_preencher(t, r, 0xff706040); tz_tela_moldura(t, r, 0xffe0c070); tz_ui_fonte_caixa(t, &u->skin->f12b, r, cat[i], COR_BRANCO, 1); }
	}
	int c = u->jan.cat;
	if (u->jan.fab_item >= FAB_N[c] || fab_salto(c, u->jan.fab_item)) { u->jan.fab_item = c == 1 ? 1 : 0; }
	int it = u->jan.fab_item;
	char nm[100];
	fab_nome(u, c, it, nm, sizeof nm);
	int y = j.y + 90;
	texto(u, t, j.x + 24, y, COR_APAGADO, "%s", tz_ui_txt(u->skin, "orders_add", "bu_item", "Objeto a construir"));
	btn(u, t, (TzRect){ j.x + 24, y + 16, 300, 26 }, TZ_W_ITEM, nm, 1);
	y += 56;
	uint32_t min = c == 0 ? tz_mundo_livro_minimo(m, it) : 10;
	if (u->jan.fab_ouro < (int)min) u->jan.fab_ouro = (int)min;
	texto(u, t, j.x + 24, y, COR_APAGADO, "%s", tz_ui_txt(u->skin, "orders_add", "bu_amount", "Con más oro se mejoran sus propiedades"));
	btn(u, t, (TzRect){ j.x + 24, y + 18, 34, 26 }, TZ_W_Q_MENOS, "-", 1);
	texto_c(u, t, (TzRect){ j.x + 62, y + 18, 90, 26 }, m->reino[m->jogador].recursos[TZ_OURO] >= u->jan.fab_ouro ? COR_BRANCO : COR_MAL, "%d", u->jan.fab_ouro);
	btn(u, t, (TzRect){ j.x + 156, y + 18, 34, 26 }, TZ_W_Q_MAIS, "+", 1);
	icono_rec(u, t, TZ_OURO, j.x + 200, y + 21);
	if (c == 0) texto(u, t, j.x + 24, y + 54, COR_APAGADO, "Mínimo para este libro: %u de oro", (unsigned)min);
	int g4 = u->jan.fab_ouro / 4;
	char res[120] = "";
	if (c == 1) snprintf(res, sizeof res, it == 1 ? "da %d de experiencia" : it == 2 ? "da %d de maná" : it == 3 ? "da +%d de ataque" : it == 4 ? "da +%d de defensa" : "da %d de vida", it == 1 ? u->jan.fab_ouro / 5 : (it == 3 || it == 4) ? u->jan.fab_ouro / 2 : u->jan.fab_ouro);
	else if (c == 2) snprintf(res, sizeof res, it == 2 ? "defensa +%d" : "ataque +%d", it == 2 ? g4 : (g4 < 250 ? g4 : 250));
	if (res[0]) texto(u, t, j.x + 24, y + 78, COR_TEXTO, "Resultado: %s", res);
	int ocupada = tz_mundo_fabrica_ocupada(m, m->jogador);
	if (ocupada) texto(u, t, j.x + 24, y + 102, COR_BIEN, "Se está fabricando un objeto (uno a la vez)...");
	btn(u, t, (TzRect){ j.x + 24, j.y + j.h - 40, 160, 27 }, TZ_W_ACEITAR, tz_ui_txt(u->skin, "orders_add", "bu_build", "Crear objeto"), !ocupada && m->reino[m->jogador].recursos[TZ_OURO] >= u->jan.fab_ouro);
	btn(u, t, (TzRect){ j.x + j.w - 164, j.y + j.h - 40, 140, 27 }, TZ_W_FECHAR, tz_ui_txt(u->skin, "general", "close", "Cerrar"), 1);
}

static void fabrica_clique(TzUi *u, int id, int shift)
{
	TzMundo *m = u->m;
	int c = u->jan.cat;
	if (id >= TZ_W_CAT0 && id < TZ_W_CAT0 + 3) { u->jan.cat = id - TZ_W_CAT0; u->jan.fab_item = u->jan.cat == 1 ? 1 : 0; u->jan.fab_ouro = u->jan.cat == 0 ? 200 : 50; return; }
	switch (id) {
	case TZ_W_ITEM: {
		int i = u->jan.fab_item;
		do i = (i + 1) % FAB_N[c]; while (fab_salto(c, i));
		u->jan.fab_item = i;
		break;
	}
	case TZ_W_Q_MENOS: u->jan.fab_ouro -= shift ? 50 : 10; break;
	case TZ_W_Q_MAIS: u->jan.fab_ouro += shift ? 50 : 10; break;
	case TZ_W_ACEITAR: {
		uint32_t min = c == 0 ? tz_mundo_livro_minimo(m, u->jan.fab_item) : 1;
		if (u->jan.fab_ouro < (int)min) { aviso_neg(u, "Este libro mágico requiere al menos %u unidades de oro", (unsigned)min); return; }
		TzComando cm;
		memset(&cm, 0, sizeof cm);
		cm.tipo = TZ_C_FABRICAR; cm.arg = (int16_t)(c | (u->jan.fab_item << 2)); cm.valor = u->jan.fab_ouro; cm.nids = 1; cm.ids[0] = u->jan.obj;
		if (enviar(u, &cm)) aviso_neg(u, "Fabricando...");
		break;
	}
	}
	uint32_t min = c == 0 ? tz_mundo_livro_minimo(m, u->jan.fab_item) : 10;
	if (u->jan.fab_ouro < (int)min) u->jan.fab_ouro = (int)min;
	if (u->jan.fab_ouro > 100000) u->jan.fab_ouro = 100000;
}

/* ------------------------------------------------------------------ mercado: joyas y venta */
static void joia_texto(const TzUi *u, const struct TzJoia *jo, char *nm, size_t nn, char *dt, size_t nd)
{
	TzObj tmp;
	memset(&tmp, 0, sizeof tmp);
	tmp.classe = jo->classe; tmp.item = jo->item;
	item_texto(u, &tmp, nm, nn, dt, nd);
}

static void mercado(TzUi *u, TzTela *t, TzRect j)
{
	TzMundo *m = u->m;
	titulo(u, t, j, tz_ui_txt(u->skin, "objects", "market", "Mercado"));
	static const char *const ab[2] = { "Joyas", "Vender objetos" };
	for (int i = 0; i < 2; ++i) {
		TzRect r = { j.x + 24 + i * 180, j.y + 40, 174, 24 };
		btn(u, t, r, TZ_W_ABA0 + i, ab[i], i != (u->jan.aba & 1));
		if (t && i == (u->jan.aba & 1)) { tz_tela_preencher(t, r, 0xff706040); tz_tela_moldura(t, r, 0xffe0c070); tz_ui_fonte_caixa(t, &u->skin->f12b, r, ab[i], COR_BRANCO, 1); }
	}
	int y = j.y + 76;
	if (!(u->jan.aba & 1)) {
		struct TzJoia jo[8];
		int n = tz_mundo_joias(m, m->jogador, jo, 8);
		if (!n) texto_c(u, t, (TzRect){ j.x + 20, j.y + 150, j.w - 40, 20 }, COR_APAGADO, "%s", pesquisado(u, "te_jewelry") ? "Aún no hay joyas: llegan una por minuto" : "Investiga «Jewelry» para que el mercado ofrezca joyas");
		for (int i = 0; i < n; ++i) {
			TzRect f = { j.x + 20, y + i * 36, j.w - 40, 32 };
			if (t) { tz_tela_preencher(t, f, 0x60000000); tz_tela_moldura(t, f, 0xff504830); }
			char nm[80], dt[160];
			joia_texto(u, &jo[i], nm, sizeof nm, dt, sizeof dt);
			uint32_t pr = tz_mundo_preco_joia(m, &jo[i]);
			uint32_t c[TZ_NREC] = { pr };
			texto(u, t, f.x + 6, f.y + 2, COR_BRANCO, "%s", nm);
			texto(u, t, f.x + 6, f.y + 16, COR_TEXTO, "%s", dt);
			char pt[40];
			snprintf(pt, sizeof pt, "%u", (unsigned)pr);
			icono_rec(u, t, TZ_OURO, f.x + f.w - 190, f.y + 6);
			texto(u, t, f.x + f.w - 166, f.y + 9, pode_pagar(u, c) ? COR_TEXTO : COR_MAL, "%s", pt);
			btn(u, t, (TzRect){ f.x + f.w - 100, f.y + 3, 96, 26 }, TZ_W_LINHA + i, "Comprar", pode_pagar(u, c));
		}
	} else {
		int ns = 0;
		for (int q = 0; q < m->nobj && ns < 8; ++q) {
			const TzObj *o = &m->obj[q];
			if (!(o->flags & TZ_F_CARREGADO) || o->dentro_de < 0 || m->obj[o->dentro_de].dono != m->jogador || !tz_obj_item(o)) continue;
			uint32_t v = tz_mundo_valor_item(m, q);
			if (!v) continue;
			TzRect f = { j.x + 20, y + ns * 36, j.w - 40, 32 };
			if (t) { tz_tela_preencher(t, f, 0x60000000); tz_tela_moldura(t, f, 0xff504830); }
			char nm[80], dt[160];
			item_texto(u, o, nm, sizeof nm, dt, sizeof dt);
			texto(u, t, f.x + 6, f.y + 2, COR_BRANCO, "%s", nm);
			texto(u, t, f.x + 6, f.y + 16, COR_TEXTO, "%s", dt);
			icono_rec(u, t, TZ_OURO, f.x + f.w - 190, f.y + 6);
			texto(u, t, f.x + f.w - 166, f.y + 9, COR_BIEN, "+%u", (unsigned)v);
			btn(u, t, (TzRect){ f.x + f.w - 100, f.y + 3, 96, 26 }, TZ_W_MUNDO + q, "Vender", 1);
			ns++;
		}
		if (!ns) texto_c(u, t, (TzRect){ j.x + 20, j.y + 150, j.w - 40, 20 }, COR_APAGADO, "Ninguna unidad lleva objetos que se puedan vender");
	}
	btn(u, t, (TzRect){ j.x + (j.w - 140) / 2, j.y + j.h - 40, 140, 27 }, TZ_W_FECHAR, tz_ui_txt(u->skin, "general", "close", "Cerrar"), 1);
}

static void mercado_clique(TzUi *u, int id)
{
	TzMundo *m = u->m;
	if (id == TZ_W_ABA0 || id == TZ_W_ABA0 + 1) { u->jan.aba = id - TZ_W_ABA0; return; }
	TzComando c;
	memset(&c, 0, sizeof c);
	c.nids = 1; c.ids[0] = u->jan.obj;
	if (!(u->jan.aba & 1)) {
		struct TzJoia jo[8];
		int n = tz_mundo_joias(m, m->jogador, jo, 8), i = id - TZ_W_LINHA;
		if (i < 0 || i >= n) return;
		c.tipo = TZ_C_COMPRAR; c.arg = (int16_t)jo[i].id;
		if (enviar(u, &c)) aviso_neg(u, "Comprado");
	} else if (id >= TZ_W_MUNDO && id < TZ_W_MUNDO + m->nobj) {
		c.tipo = TZ_C_VENDER; c.valor = id - TZ_W_MUNDO;
		if (enviar(u, &c)) aviso_neg(u, "Vendido");
	}
}

/* ------------------------------------------------------------------ dibujo y entrada comunes */
void tz_ui_neg_desenhar(TzUi *u, TzTela *t, TzRect j)
{
	u->njw = 0;
	switch (u->janela) {
	case TZ_UJ_TABERNA: taberna(u, t, j); break;
	case TZ_UJ_DADOS: dados(u, t, j); break;
	case TZ_UJ_PRESTAMOS: prestamos(u, t, j); break;
	case TZ_UJ_INTERCAMBIO: intercambio(u, t, j); break;
	case TZ_UJ_COMERCIO: comercio(u, t, j); break;
	case TZ_UJ_OBJETOS: objetos(u, t, j); break;
	case TZ_UJ_CRUZADA: cruzada(u, t, j); break;
	case TZ_UJ_TRIBUTO: tributo(u, t, j); break;
	case TZ_UJ_RENDICION: rendicion(u, t, j); break;
	case TZ_UJ_FABRICA: fabrica(u, t, j); break;
	case TZ_UJ_MERCADO: mercado(u, t, j); break;
	default: break;
	}
	if (t && u->jan.msg[0] && u->agora_ms < u->jan.msg_ate)
		tz_ui_fonte_caixa(t, &u->skin->f12b, (TzRect){ j.x + 10, j.y + j.h - 66, j.w - 20, 18 }, u->jan.msg, 0xe0d070, 1);
}

static int neg_id_em(TzUi *u, int x, int y)
{
	for (int i = u->njw - 1; i >= 0; --i) if (tz_rect_dentro(u->jw[i].r, x, y)) return u->jw[i].id;
	return 0;
}

static void clique(TzUi *u, int id, int direita, int shift)
{
	if (id == TZ_W_FECHAR) { u->janela = 0; return; }
	if (id >= TZ_W_ABA0 && id < TZ_W_ABA0 + 5 && u->janela != TZ_UJ_MERCADO) { mudar_aba(u, id - TZ_W_ABA0); return; }
	switch (u->janela) {
	case TZ_UJ_TABERNA: taberna_clique(u, id); break;
	case TZ_UJ_DADOS: dados_clique(u, id, direita, shift); break;
	case TZ_UJ_PRESTAMOS: prestamos_clique(u, id); break;
	case TZ_UJ_INTERCAMBIO: intercambio_clique(u, id, shift); break;
	case TZ_UJ_COMERCIO: comercio_clique(u, id, shift); break;
	case TZ_UJ_OBJETOS: objetos_clique(u, id); break;
	case TZ_UJ_CRUZADA: cruzada_clique(u, id); break;
	case TZ_UJ_TRIBUTO: tributo_clique(u, id, shift); break;
	case TZ_UJ_RENDICION:
		if (id == TZ_W_SIM) { TzComando c; memset(&c, 0, sizeof c); c.tipo = TZ_C_RENDIR; enviar(u, &c); u->janela = 0; tz_ui_aviso(u, "Te has rendido"); }
		break;
	case TZ_UJ_FABRICA: fabrica_clique(u, id, shift); break;
	case TZ_UJ_MERCADO: mercado_clique(u, id); break;
	default: break;
	}
}

int tz_ui_neg_entrada(TzUi *u, const TzUiEntrada *e)
{
	TzRect j = tz_ui_neg_rect(u);
	if (e->tipo == TZ_IN_MOVER) { u->mx = e->x; u->my = e->y; return 1; }
	if (e->tipo == TZ_IN_TECLA) { if (e->tecla == TZ_TC_ESC) u->janela = 0; return 1; }
	if (e->tipo == TZ_IN_BAIXO && (e->botao == 1 || e->botao == 3)) {
		u->mx = e->x; u->my = e->y;
		if (!tz_rect_dentro(j, e->x, e->y)) { if (e->botao == 1) u->janela = 0; return 1; }
		tz_ui_neg_desenhar(u, NULL, j);   /* los botones de este momento */
		int id = neg_id_em(u, e->x, e->y);
		if (id) clique(u, id, e->botao == 3, (e->mods & TZ_MOD_SHIFT) != 0);
		return 1;
	}
	if (e->tipo == TZ_IN_CIMA || e->tipo == TZ_IN_BAIXO || e->tipo == TZ_IN_RODA) return 1;
	return 0;
}

/* Si el botón de órdenes abre una ventana en vez de mandar una orden: devuelve 1 y la abre. acao y chave son los de ORDERS.INI. */
int tz_ui_neg_acao(TzUi *u, const char *acao, const char *chave, const char *submenu)
{
	if (!strcmp(submenu, "tavern_merc") || !strcmp(submenu, "tavern_hero")) {
		if (tz_ui_janela_abrir(u, TZ_UJ_TABERNA, u->sel.n ? u->sel.id[0] : TZ_NENHUM) == 0) u->jan.aba = !strcmp(submenu, "tavern_hero");
		return 1;
	}
	if (!strcmp(submenu, "loan") || !strcmp(acao, "loan_return")) { tz_ui_janela_abrir(u, TZ_UJ_PRESTAMOS, u->sel.n ? u->sel.id[0] : TZ_NENHUM); return 1; }
	if (!strcmp(acao, "trade1")) { tz_ui_janela_abrir(u, TZ_UJ_INTERCAMBIO, u->sel.n ? u->sel.id[0] : TZ_NENHUM); return 1; }
	if (!strcmp(acao, "gamble")) { tz_ui_janela_abrir(u, TZ_UJ_DADOS, u->sel.n ? u->sel.id[0] : TZ_NENHUM); return 1; }
	if (!strcmp(acao, "trade")) { tz_ui_janela_abrir(u, TZ_UJ_COMERCIO, TZ_NENHUM); return 1; }
	if (!strcmp(acao, "build_item")) { tz_ui_janela_abrir(u, TZ_UJ_FABRICA, u->sel.n ? u->sel.id[0] : TZ_NENHUM); return 1; }
	if (!strcmp(submenu, "market_items") || !strcmp(submenu, "market_sell")) {
		if (tz_ui_janela_abrir(u, TZ_UJ_MERCADO, u->sel.n ? u->sel.id[0] : TZ_NENHUM) == 0) u->jan.aba = !strcmp(submenu, "market_sell");
		return 1;
	}
	if (!strcmp(chave, "preach_crusade")) { tz_ui_janela_abrir(u, TZ_UJ_CRUZADA, u->sel.n ? u->sel.id[0] : TZ_NENHUM); return 1; }
	if (!strcmp(acao, "select_crusaders")) { tz_ui_neg_selecionar_cruzados(u); return 1; }
	return 0;
}

/* al soltar el botón o cada cuadro: si se cancela la espera de un objetivo, la ventana vuelve */
void tz_ui_neg_tic(TzUi *u)
{
	if (u->pick && u->modo != TZ_UI_ALVO) { u->pick = 0; u->janela = u->janela_pick; }
}
