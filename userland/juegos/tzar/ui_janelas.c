/* SPDX-License-Identifier: GPL-3.0-or-later */
/* ui_janelas.c - ventanas de la partida de Tzar: diplomacia y chat (D417, fase 4).
 * Sigue a ReTzar (https://github.com/ColCarlo/ReTzar, GPL-3.0-or-later): hud.rs Layout::window (diplomacia de 500x420 y chat de
 * 500x143 centrados en la pantalla) y las relaciones de mundo.h (TZ_DIP_ALIADO 0x40 con la vista 0x01, NEUTRO 0x20, GUERRA
 * 0x10). La diplomacia manda TZ_C_DIPLOMACIA; el chat deja lo escrito en chat_saida para el programa (la orden «ai ...» de la
 * IA) y enseña las frases que dicen los reinos del ordenador. Sin arte propio: marco de piedra dibujado y las fuentes del juego. */
#include "ui.h"
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#define COR_TITULO 0xe8d890u
#define COR_TEXTO  0xd8c890u

TzRect tz_ui_janela_rect(const TzUi *u)
{
	if (u->janela >= TZ_UJ_TABERNA) return tz_ui_neg_rect(u);
	int w = 500, h = u->janela == TZ_UJ_DIPLOMACIA ? 420 : 143 + 6 * 16;
	if (w > u->w) w = u->w;
	int y = (u->h - h) / 2;
	return (TzRect){ (u->w - w) / 2, y < 0 ? 0 : y, w, h };
}

static void caixa(TzTela *t, TzRect r)
{
	tz_tela_preencher(t, r, 0xf0201810);
	tz_tela_moldura(t, r, 0xffc0a060);
	tz_tela_moldura(t, (TzRect){ r.x + 3, r.y + 3, r.w - 6, r.h - 6 }, 0xff605030);
}

static void botao(TzTela *t, const TzUiSkin *s, TzRect r, const char *txt, int sobre)
{
	tz_tela_preencher(t, r, sobre ? 0xff584830 : 0xff403420);
	tz_tela_moldura(t, r, 0xff907850);
	tz_ui_fonte_caixa(t, &s->f12b, r, txt, sobre ? 0xffffe0 : COR_TEXTO, 1);
}

/* ------------------------------------------------------------------ diplomacia */
static const char *postura(int rel) { return rel > 0 ? "Aliado" : rel < 0 ? "Enemigo" : "Neutral"; }

/* las filas: los reinos activos que no son el del jugador, en orden */
static int reinos_dipl(const TzUi *u, int *ks)
{
	int n = 0;
	for (int k = 1; k <= 8; ++k)
		if (u->m->reino[k].ativo && k != u->m->jogador) ks[n++] = k;
	return n;
}

static TzRect r_fila(TzRect j, int i) { return (TzRect){ j.x + 20, j.y + 70 + i * 36, j.w - 40, 30 }; }
static TzRect r_postura(TzRect j, int i) { TzRect f = r_fila(j, i); return (TzRect){ f.x + 230, f.y + 2, 110, 26 }; }
static TzRect r_fechar(TzRect j) { return (TzRect){ j.x + (j.w - 140) / 2, j.y + j.h - 40, 140, 27 }; }
static TzRect r_tributo(TzRect j) { return (TzRect){ j.x + 20, j.y + j.h - 40, 150, 27 }; }
static TzRect r_rendir(TzRect j) { return (TzRect){ j.x + j.w - 170, j.y + j.h - 40, 150, 27 }; }

static void diplomacia_desenhar(TzUi *u, TzTela *t, TzRect j)
{
	const TzUiSkin *s = u->skin;
	const TzMundo *m = u->m;
	tz_ui_fonte_caixa(t, &s->f14b, (TzRect){ j.x, j.y + 14, j.w, 20 }, tz_ui_txt(s, "game_scr", "diplomacy", "Diplomacia"), COR_TITULO, 1);
	tz_ui_fonte_texto(t, &s->f10, j.x + 50, j.y + 50, "Reino", 0xb0a070);
	tz_ui_fonte_texto(t, &s->f10, j.x + 250, j.y + 50, "Tu actitud", 0xb0a070);
	tz_ui_fonte_texto(t, &s->f10, j.x + 380, j.y + 50, "Su actitud", 0xb0a070);
	int ks[8], n = reinos_dipl(u, ks);
	for (int i = 0; i < n; ++i) {
		int k = ks[i];
		const TzReinoEstado *r = &m->reino[k];
		TzRect f = r_fila(j, i);
		tz_tela_preencher(t, (TzRect){ f.x, f.y + 8, 14, 14 }, 0xff000000u | s->cores[r->cor & 7]);
		char nome[64];
		snprintf(nome, sizeof nome, "%s%s", r->nome[0] ? r->nome : "Reino", r->derrotado ? " (derrotado)" : "");
		tz_ui_fonte_caixa(t, &s->f12b, (TzRect){ f.x + 22, f.y + 6, 200, 18 }, nome, COR_TEXTO, 0);
		TzRect p = r_postura(j, i);
		botao(t, s, p, postura(tz_mundo_relacao(m, m->jogador, k)), tz_rect_dentro(p, u->mx, u->my));
		int dele = tz_mundo_relacao(m, k, m->jogador);
		tz_ui_fonte_caixa(t, &s->f12b, (TzRect){ f.x + 360, f.y + 6, 100, 18 }, postura(dele), dele > 0 ? 0x80e080 : dele < 0 ? 0xe08070 : COR_TEXTO, 0);
	}
	if (!n) tz_ui_fonte_caixa(t, &s->f12b, (TzRect){ j.x, j.y + 100, j.w, 20 }, "No hay otros reinos.", COR_TEXTO, 1);
	botao(t, s, r_fechar(j), tz_ui_txt(s, "general", "close", "Cerrar"), tz_rect_dentro(r_fechar(j), u->mx, u->my));
	botao(t, s, r_tributo(j), tz_ui_txt(s, "diplomacy", "tribute", "TRIBUTOS"), tz_rect_dentro(r_tributo(j), u->mx, u->my));
	botao(t, s, r_rendir(j), tz_ui_txt(s, "menu", "surr", "RENDIRSE"), tz_rect_dentro(r_rendir(j), u->mx, u->my));
}

static void diplomacia_mudar(TzUi *u, int k)
{
	TzMundo *m = u->m;
	int atual = tz_mundo_relacao(m, m->jogador, k);
	/* aliado -> neutral -> enemigo -> aliado */
	uint32_t novo = atual > 0 ? TZ_DIP_NEUTRO : atual == 0 ? TZ_DIP_GUERRA : TZ_DIP_ALIADO | TZ_DIP_VISAO;
	TzComando c;
	memset(&c, 0, sizeof c);
	c.tipo = TZ_C_DIPLOMACIA; c.reino = (uint8_t)m->jogador; c.arg = (int16_t)k; c.valor = (int32_t)novo;
	if (tz_mundo_comando(m, &c) == 0) {
		u->n_comandos++;
		tz_ui_aviso(u, "%s: %s", m->reino[k].nome[0] ? m->reino[k].nome : "Reino", postura(novo & TZ_DIP_ALIADO ? 1 : novo & TZ_DIP_GUERRA ? -1 : 0));
	}
}

/* ------------------------------------------------------------------ chat */
static const char *const DESTINOS[3] = { "A todos", "A los aliados", "A los enemigos" };
static TzRect r_texto(TzRect j) { return (TzRect){ j.x + 20, j.y + j.h - 70, j.w - 40, 22 }; }
static TzRect r_destino(TzRect j) { return (TzRect){ j.x + 20, j.y + j.h - 38, 150, 27 }; }
static TzRect r_enviar(TzRect j) { return (TzRect){ j.x + j.w - 160, j.y + j.h - 38, 140, 27 }; }

static void historico(TzUi *u, const char *txt, uint32_t cor)
{
	if (u->nchat == 6) { memmove(u->chat_hist[0], u->chat_hist[1], sizeof u->chat_hist[0] * 5); memmove(u->chat_cor, u->chat_cor + 1, sizeof u->chat_cor[0] * 5); u->nchat = 5; }
	snprintf(u->chat_hist[u->nchat], sizeof u->chat_hist[0], "%s", txt);
	u->chat_cor[u->nchat++] = cor;
	u->chat_ate = u->agora_ms + 12000;
}

void tz_ui_chat_recebido(TzUi *u, const char *texto)
{
	if (texto && *texto) historico(u, texto, 0xffe0a0);
}

void tz_ui_chat_linha(TzUi *u, int reino, const char *extra)
{
	(void)reino;
	if (extra) historico(u, extra, 0xa0a0a0);
}

static void chat_enviar(TzUi *u)
{
	if (!u->chat[0]) { u->janela = 0; return; }
	char b[128];
	snprintf(b, sizeof b, "Tú (%s): %s", DESTINOS[u->chat_dest % 3], u->chat);
	historico(u, b, 0xffffff);
	snprintf(u->chat_saida, sizeof u->chat_saida, "%s", u->chat);
	u->chat[0] = 0;
	u->janela = 0;
}

static void chat_desenhar(TzUi *u, TzTela *t, TzRect j)
{
	const TzUiSkin *s = u->skin;
	tz_ui_fonte_caixa(t, &s->f14b, (TzRect){ j.x, j.y + 12, j.w, 20 }, tz_ui_txt(s, "game_scr", "chat", "Chat"), COR_TITULO, 1);
	for (int i = 0; i < u->nchat; ++i) tz_ui_fonte_caixa(t, &s->f12b, (TzRect){ j.x + 20, j.y + 40 + i * 16, j.w - 40, 16 }, u->chat_hist[i], u->chat_cor[i], 0);
	TzRect r = r_texto(j);
	tz_tela_preencher(t, r, 0xff000000);
	tz_tela_moldura(t, r, 0xff907850);
	char b[90];
	snprintf(b, sizeof b, "%s%s", u->chat, (u->agora_ms / 500) & 1 ? "_" : "");
	tz_ui_fonte_caixa(t, &s->f12b, (TzRect){ r.x + 4, r.y + 2, r.w - 8, r.h - 4 }, b, 0xffffe0, 0);
	botao(t, s, r_destino(j), DESTINOS[u->chat_dest % 3], tz_rect_dentro(r_destino(j), u->mx, u->my));
	botao(t, s, r_enviar(j), "Enviar", tz_rect_dentro(r_enviar(j), u->mx, u->my));
}

/* ------------------------------------------------------------------ comunes */
void tz_ui_janela_desenhar(TzUi *u, TzTela *t)
{
	/* las últimas líneas del chat sobre el mapa, un rato, con la ventana cerrada */
	if (!u->janela && u->nchat && u->agora_ms < u->chat_ate)
		for (int i = 0; i < u->nchat; ++i) {
			TzRect r = { u->mapa_rect.x + 10, u->mapa_rect.y + 34 + i * 16, u->mapa_rect.w - 20, 16 };
			int w = tz_ui_fonte_medir(&u->skin->f12b, u->chat_hist[i]) + 8;
			tz_tela_preencher(t, (TzRect){ r.x, r.y, w < r.w ? w : r.w, r.h }, 0x90000000);
			tz_ui_fonte_caixa(t, &u->skin->f12b, (TzRect){ r.x + 4, r.y, r.w - 4, r.h }, u->chat_hist[i], u->chat_cor[i], 0);
		}
	if (!u->janela) return;
	TzRect j = tz_ui_janela_rect(u);
	caixa(t, j);
	if (u->janela == TZ_UJ_DIPLOMACIA) diplomacia_desenhar(u, t, j);
	else if (u->janela >= TZ_UJ_TABERNA) tz_ui_neg_desenhar(u, t, j);
	else chat_desenhar(u, t, j);
}

int tz_ui_janela_entrada(TzUi *u, const TzUiEntrada *e)
{
	if (u->janela >= TZ_UJ_TABERNA) return tz_ui_neg_entrada(u, e);
	TzRect j = tz_ui_janela_rect(u);
	if (e->tipo == TZ_IN_MOVER) { u->mx = e->x; u->my = e->y; return 1; }
	if (e->tipo == TZ_IN_TECLA) {
		if (e->tecla == TZ_TC_ESC) { u->janela = 0; return 1; }
		if (u->janela == TZ_UJ_CHAT) {
			size_t l = strlen(u->chat);
			if (e->tecla == TZ_TC_ENTER) chat_enviar(u);
			else if (e->tecla == TZ_TC_BORRAR) { if (l) u->chat[l - 1] = 0; }
			else if (e->tecla >= 32 && e->tecla < 256 && l + 1 < sizeof u->chat) {
				int c = e->tecla;
				if ((e->mods & TZ_MOD_SHIFT) && c >= 'a' && c <= 'z') c -= 32;
				u->chat[l] = (char)c; u->chat[l + 1] = 0;
			}
		}
		return 1;
	}
	if (e->tipo == TZ_IN_BAIXO && e->botao == 1) {
		u->mx = e->x; u->my = e->y;
		if (!tz_rect_dentro(j, e->x, e->y)) { u->janela = 0; return 1; }   /* un clic fuera la cierra */
		if (u->janela == TZ_UJ_DIPLOMACIA) {
			if (tz_rect_dentro(r_fechar(j), e->x, e->y)) { u->janela = 0; return 1; }
			if (tz_rect_dentro(r_tributo(j), e->x, e->y)) { tz_ui_janela_abrir(u, TZ_UJ_TRIBUTO, TZ_NENHUM); return 1; }
			if (tz_rect_dentro(r_rendir(j), e->x, e->y)) { tz_ui_janela_abrir(u, TZ_UJ_RENDICION, TZ_NENHUM); return 1; }
			int ks[8], n = reinos_dipl(u, ks);
			for (int i = 0; i < n; ++i) if (tz_rect_dentro(r_postura(j, i), e->x, e->y)) { diplomacia_mudar(u, ks[i]); break; }
		} else {
			if (tz_rect_dentro(r_destino(j), e->x, e->y)) u->chat_dest = (u->chat_dest + 1) % 3;
			else if (tz_rect_dentro(r_enviar(j), e->x, e->y)) chat_enviar(u);
		}
		return 1;
	}
	if (e->tipo == TZ_IN_CIMA || e->tipo == TZ_IN_BAIXO || e->tipo == TZ_IN_RODA) return 1;
	return 0;
}
