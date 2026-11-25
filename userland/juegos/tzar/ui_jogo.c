/* SPDX-License-Identifier: GPL-3.0-or-later */
/* ui_jogo.c - la entrada de la partida de Tzar (D417, fase 4): ratón, teclado, cámara, colocación de edificios,
 * grupos y avisos que salen de los eventos del mundo. Sigue frontend/gameplay.rs de ReTzar (GPL-3.0-or-later).
 * Habla con el mundo solo por consultas, comandos y eventos con cursor propio (mundo.h). */
#include "ui.h"
#include <stdlib.h>
#include <string.h>

static void limitar_camara(TzUi *u)
{
	int mw = u->m->lado * TZ_CASILLA;
	int max_x = mw - u->mapa_rect.w, max_y = mw - u->mapa_rect.h;
	if (u->camx > max_x) u->camx = max_x;
	if (u->camy > max_y) u->camy = max_y;
	if (u->camx < 0) u->camx = 0;
	if (u->camy < 0) u->camy = 0;
}

void tz_ui_mover_camara(TzUi *u, int dx, int dy)
{
	u->camx += dx; u->camy += dy;
	limitar_camara(u);
}

void tz_ui_centrar(TzUi *u, int cx, int cy)
{
	u->camx = cx * TZ_CASILLA + TZ_CASILLA / 2 - u->mapa_rect.w / 2;
	u->camy = cy * TZ_CASILLA + TZ_CASILLA / 2 - u->mapa_rect.h / 2;
	limitar_camara(u);
}

/* píxeles de la ventana -> píxeles del mundo */
static int mundo_x(const TzUi *u, int x) { return x - u->mapa_rect.x + u->camx; }
static int mundo_y(const TzUi *u, int y) { return y - u->mapa_rect.y + u->camy; }

static void colocar_posicao(TzUi *u)
{
	const TzMundo *m = u->m;
	const TzAtributos *a = m->cat ? tz_catalogo_at(m->cat, u->classe_colocar, m->reino[m->jogador].civ) : NULL;
	int w = a && a->pegada[0] ? a->pegada[0] : 1, h = a && a->pegada[1] ? a->pegada[1] : 1;
	/* la huella queda centrada en el cursor */
	int cx = (mundo_x(u, u->mx) - w * TZ_CASILLA / 2 + TZ_CASILLA / 2) / TZ_CASILLA;
	int cy = (mundo_y(u, u->my) - h * TZ_CASILLA / 2 + TZ_CASILLA / 2) / TZ_CASILLA;
	if (cx < 0) cx = 0;
	if (cy < 0) cy = 0;
	u->colocar_cx = cx; u->colocar_cy = cy;
	u->colocar_ok = tz_mundo_cabe(m, m->jogador, u->classe_colocar, cx, cy) == 0;
}

static void centrar_na_selecao(TzUi *u)
{
	if (!u->sel.n) return;
	const TzObj *o = &u->m->obj[u->sel.id[0]];
	tz_ui_centrar(u, o->cx, o->cy);
}

static void minimapa_para_mundo(const TzUi *u, int x, int y, int *cx, int *cy)
{
	*cx = (x - u->minimapa_rect.x) * u->m->lado / u->minimapa_rect.w;
	*cy = (y - u->minimapa_rect.y) * u->m->lado / u->minimapa_rect.h;
	if (*cx < 0) *cx = 0;
	if (*cy < 0) *cy = 0;
	if (*cx >= u->m->lado) *cx = u->m->lado - 1;
	if (*cy >= u->m->lado) *cy = u->m->lado - 1;
}

static void construir(TzUi *u, int enfileirar)
{
	TzMundo *m = u->m;
	if (!u->colocar_ok) {
		tz_mundo_cabe(m, m->jogador, u->classe_colocar, u->colocar_cx, u->colocar_cy);
		tz_ui_aviso(u, "%s", tz_erro());
		return;
	}
	TzComando c;
	memset(&c, 0, sizeof c);
	c.tipo = TZ_C_CONSTRUIR; c.reino = (uint8_t)m->jogador; c.arg = (int16_t)u->classe_colocar;
	c.x = (int16_t)u->colocar_cx; c.y = (int16_t)u->colocar_cy;
	c.flags = enfileirar ? TZ_CMD_FILA : 0;
	for (int i = 0; i < u->sel.n && c.nids < TZ_CMD_IDS; ++i) {
		const TzObj *o = &m->obj[u->sel.id[i]];
		if (tz_obj_unidade(o) && o->dono == m->jogador && tz_obj_vivo(o)) c.ids[c.nids++] = u->sel.id[i];
	}
	if (!c.nids) { tz_ui_aviso(u, "%s", tz_ui_txt(u->skin, "viewmsg", "notres", "Sin constructores")); return; }
	if (tz_mundo_comando(m, &c) == 0) u->n_comandos++;
	if (!enfileirar) u->modo = TZ_UI_NORMAL;
}

static void tecla(TzUi *u, const TzUiEntrada *e)
{
	int k = e->tecla;
	if (k == TZ_TC_ESC) {
		if (u->modo != TZ_UI_NORMAL) u->modo = TZ_UI_NORMAL;
		else if (u->npilha) { u->npilha--; u->pagina = 0; tz_ui_menu_atualizar(u); }
		else u->pedir_menu = 1;
		return;
	}
	if ((k == 'i' || k == 'I') && (e->mods & TZ_MOD_CTRL)) { tz_ui_janela_abrir(u, TZ_UJ_OBJETOS, TZ_NENHUM); return; }   /* inventario de la unidad */
	if (k == TZ_TC_ENTER) { u->janela = TZ_UJ_CHAT; u->chat[0] = 0; return; }   /* Intro abre el chat, como el original */
	if (k >= '0' && k <= '9') {
		int g = k - '0';
		if (e->mods & TZ_MOD_CTRL) { tz_ui_sel_grupo_guardar(&u->sel, g); tz_ui_aviso(u, "Grupo %d", g); }
		else {
			int duplo;
			if (tz_ui_sel_grupo_chamar(u->m, &u->sel, g, u->agora_ms, &duplo) && duplo) centrar_na_selecao(u);
			u->npilha = 0; u->pagina = 0;
		}
		return;
	}
	switch (k) {
	case TZ_TC_ESQ: tz_ui_mover_camara(u, -96, 0); return;
	case TZ_TC_DIR: tz_ui_mover_camara(u, 96, 0); return;
	case TZ_TC_CIMA: tz_ui_mover_camara(u, 0, -96); return;
	case TZ_TC_BAIXO: tz_ui_mover_camara(u, 0, 96); return;
	case TZ_TC_INICIO: centrar_na_selecao(u); return;
	case TZ_TC_PGDN: if ((u->pagina + 1) * TZ_UI_BOTOES < u->ntotal_botoes) { u->pagina++; tz_ui_menu_atualizar(u); } return;
	case TZ_TC_PGUP: if (u->pagina > 0) { u->pagina--; tz_ui_menu_atualizar(u); } return;
	default: break;
	}
	/* atajos de las órdenes (la letra de MENU.INI) */
	if (k < 128 && (e->mods & TZ_MOD_CTRL) == 0)
		for (int i = 0; i < u->nbotoes; ++i)
			if (u->botoes[i].hotkey && (u->botoes[i].hotkey | 0x20) == (k | 0x20)) { tz_ui_botao_premir(u, i, e->mods); return; }
}

void tz_ui_entrada(TzUi *u, const TzUiEntrada *e, uint32_t ms)
{
	u->agora_ms = ms;
	if (u->janela && tz_ui_janela_entrada(u, e)) return;
	int shift = e->mods & TZ_MOD_SHIFT;
	switch (e->tipo) {
	case TZ_IN_MOVER:
		u->mx = e->x; u->my = e->y;
		u->botao_sobre = tz_ui_botao_em(u, e->x, e->y);
		if (u->modo == TZ_UI_COLOCAR) colocar_posicao(u);
		if (u->arrasta_mini && tz_rect_dentro(u->minimapa_rect, e->x, e->y)) {
			int cx, cy;
			minimapa_para_mundo(u, e->x, e->y, &cx, &cy);
			tz_ui_centrar(u, cx, cy);
		}
		break;
	case TZ_IN_BAIXO:
		u->mx = e->x; u->my = e->y;
		if (e->botao == 1) {
			u->mods |= TZ_MOD_BOTAO;
			if (tz_rect_dentro(u->minimapa_rect, e->x, e->y)) {
				int cx, cy;
				minimapa_para_mundo(u, e->x, e->y, &cx, &cy);
				if (u->modo == TZ_UI_ALVO && u->pick) { tz_ui_neg_pick(u, cx * TZ_CASILLA + 16, cy * TZ_CASILLA + 16); break; }
				if (u->modo == TZ_UI_ALVO) {   /* la orden pendiente también se da sobre el minimapa */
					tz_ui_ordem_alvo(u, cx * TZ_CASILLA + 16, cy * TZ_CASILLA + 16, shift);
					if (!shift) u->modo = TZ_UI_NORMAL;
					break;
				}
				u->arrasta_mini = 1;
				tz_ui_centrar(u, cx, cy);
			} else if (tz_rect_dentro(tz_ui_rect(u, TZ_UR_MENU), e->x, e->y)) u->pedir_menu = 1;
			else if (tz_rect_dentro(tz_ui_rect(u, TZ_UR_DIPLOMACIA), e->x, e->y)) u->janela = TZ_UJ_DIPLOMACIA;
			else if (tz_rect_dentro(tz_ui_rect(u, TZ_UR_CHAT), e->x, e->y)) { u->janela = TZ_UJ_CHAT; u->chat[0] = 0; }
			else if (u->sel.n == 1 && tz_mundo_capacidade_itens(u->m, u->sel.id[0]) && u->m->obj[u->sel.id[0]].dono == u->m->jogador &&
			         tz_rect_dentro((TzRect){ u->selecao_rect.x + 6, u->selecao_rect.y + 5, 48, 60 }, e->x, e->y))
				tz_ui_janela_abrir(u, TZ_UJ_OBJETOS, u->sel.id[0]);   /* el retrato abre los objetos */
			else if (tz_rect_dentro(tz_ui_rect(u, TZ_UR_PAINEL), e->x, e->y) || e->y < 30) {
				int b = tz_ui_botao_em(u, e->x, e->y);
				u->botao_press = b >= 0 ? b + 1 : 0;
				for (int s = 0; s < 3; ++s)
					if (tz_rect_dentro(tz_ui_rect(u, TZ_UR_SETA0 + s), e->x, e->y)) {
						if (s == 0 && u->npilha) { u->npilha--; u->pagina = 0; tz_ui_menu_atualizar(u); }
						else if (s == 0 && u->pagina > 0) { u->pagina--; tz_ui_menu_atualizar(u); }
						else if (s == 2 && (u->pagina + 1) * TZ_UI_BOTOES < u->ntotal_botoes) { u->pagina++; tz_ui_menu_atualizar(u); }
					}
			} else if (tz_rect_dentro(u->mapa_rect, e->x, e->y)) {
				int wx = mundo_x(u, e->x), wy = mundo_y(u, e->y);
				if (u->modo == TZ_UI_COLOCAR) { colocar_posicao(u); construir(u, shift); }
				else if (u->modo == TZ_UI_ALVO && u->pick) tz_ui_neg_pick(u, wx, wy);
				else if (u->modo == TZ_UI_ALVO) {
					tz_ui_ordem_alvo(u, wx, wy, shift);
					if (!shift) u->modo = TZ_UI_NORMAL;
				} else { u->arrasta = 1; u->ax = e->x; u->ay = e->y; }
			}
		} else if (e->botao == 3) {
			if (u->modo != TZ_UI_NORMAL) { u->modo = TZ_UI_NORMAL; break; }
			if (tz_rect_dentro(u->minimapa_rect, e->x, e->y)) {
				int cx, cy;
				minimapa_para_mundo(u, e->x, e->y, &cx, &cy);
				tz_ui_ordem_direita(u, cx * TZ_CASILLA + 16, cy * TZ_CASILLA + 16, shift);
			} else if (tz_rect_dentro(u->mapa_rect, e->x, e->y)) tz_ui_ordem_direita(u, mundo_x(u, e->x), mundo_y(u, e->y), shift);
		}
		break;
	case TZ_IN_CIMA:
		u->mx = e->x; u->my = e->y;
		if (e->botao != 1) break;
		u->mods &= ~TZ_MOD_BOTAO;
		u->arrasta_mini = 0;
		if (u->botao_press) {
			int b = tz_ui_botao_em(u, e->x, e->y);
			if (b == u->botao_press - 1) tz_ui_botao_premir(u, b, e->mods);
			u->botao_press = 0;
		}
		if (u->arrasta) {
			u->arrasta = 0;
			if (abs(e->x - u->ax) < 4 && abs(e->y - u->ay) < 4)
				tz_ui_sel_clique(u->m, &u->sel, u->m->jogador, mundo_x(u, e->x), mundo_y(u, e->y), shift, u->agora_ms,
				                 (TzRect){ u->camx, u->camy, u->mapa_rect.w, u->mapa_rect.h });
			else
				tz_ui_sel_caixa(u->m, &u->sel, u->m->jogador, mundo_x(u, u->ax), mundo_y(u, u->ay), mundo_x(u, e->x), mundo_y(u, e->y), shift);
			u->npilha = 0; u->pagina = 0;
			tz_ui_menu_atualizar(u);
		}
		break;
	case TZ_IN_TECLA: tecla(u, e); break;
	default: break;
	}
}

static void avisar_recurso(TzUi *u, int r)
{
	static const char *const k[TZ_NREC] = { "Gold", "Stone", "Food", "Wood" };
	static const char *const d[TZ_NREC] = { "oro", "piedra", "comida", "madera" };
	if (r < 0 || r >= TZ_NREC) r = 0;
	const char *nome = tz_ui_txt(u->skin, "viewmsg", k[r], d[r]);
	const char *fmt = tz_ui_txt(u->skin, "viewmsg", "notres", "No hay suficiente %s");
	const char *p = strstr(fmt, "%s");
	if (!p) { tz_ui_aviso(u, "%s", fmt); return; }
	tz_ui_aviso(u, "%.*s%s%s", (int)(p - fmt), fmt, nome, p + 2);
}

void tz_ui_tic(TzUi *u, uint32_t ms)
{
	uint32_t dt = u->ultimo_ms ? ms - u->ultimo_ms : 0;
	u->ultimo_ms = ms;
	u->agora_ms = ms;
	tz_ui_neg_tic(u);
	if (dt > 100) dt = 100;
	if (u->scroll_bordes && dt) {
		int vel = (int)dt / 2 + 1, dx = 0, dy = 0;
		if (u->mx <= 1) dx = -vel;
		if (u->mx >= u->w - 2) dx = vel;
		if (u->my <= 1) dy = -vel;
		if (u->my >= u->h - 2) dy = vel;
		if (dx || dy) tz_ui_mover_camara(u, dx, dy);
	}
	tz_ui_sel_vivos(u->m, &u->sel);
	TzEvento ev[64];
	int n;
	while ((n = tz_mundo_eventos(u->m, &u->ev_cursor, ev, 64)) > 0)
		for (int i = 0; i < n; ++i) {
			const TzEvento *e = &ev[i];
			if (e->reino != u->m->jogador && e->tipo != TZ_EV_MENSAGEM && e->tipo != TZ_EV_FIM) continue;
			tz_ui_neg_evento(u, e);
			switch (e->tipo) {
			case TZ_EV_SEM_RECURSOS: avisar_recurso(u, e->classe); break;
			case TZ_EV_SEM_POPULACAO: tz_ui_aviso(u, "%s", tz_ui_txt(u->skin, "viewmsg", "poplimit2", "No hay suficientes casas")); break;
			case TZ_EV_ATAQUE_AVISO: tz_ui_aviso(u, "%s", tz_ui_txt(u->skin, "notify", "under_attack", "- la unidad está siendo atacada")); break;
			case TZ_EV_DERROTADO: tz_ui_aviso(u, "%s", tz_ui_txt(u->skin, "general", "defeated", "Derrota")); break;
			case TZ_EV_MENSAGEM: tz_ui_aviso(u, "%s", tz_mundo_texto(u->m, e->valor)); break;
			case TZ_EV_PERGAMINHO: u->pergaminho_pedido = e->alvo; u->pergaminho_ao_chegar = e->valor; break;
			case TZ_EV_FIM: tz_ui_aviso(u, e->valor ? "Victoria" : "Derrota"); break;
			default: break;
			}
		}
}
