/* SPDX-License-Identifier: GPL-3.0-or-later */
/* guiao_gravar.c - guardar y cargar el estado del guion en la partida (bloque «GUIA» de TZSV) (D417, fase 7).
 * Propio (ReTzar guarda ScriptState en JSON; aquí va en binario little-endian). Lo leído de los ficheros del mapa NO se guarda: se
 * vuelve a leer del mapa; solo el estado que cambia (reglas en cola, temporizadores, variables, ventanas, ocultos, perdidos...). */
#include "guiao_i.h"
#include <stdlib.h>
#include <string.h>

#define VERSAO 1u

typedef struct { FILE *f; int mal; } Esc;
static void w8(Esc *e, uint8_t v) { if (fwrite(&v, 1, 1, e->f) != 1) e->mal = 1; }
static void w32(Esc *e, uint32_t v) { uint8_t b[4] = { (uint8_t)v, (uint8_t)(v >> 8), (uint8_t)(v >> 16), (uint8_t)(v >> 24) }; if (fwrite(b, 1, 4, e->f) != 4) e->mal = 1; }
static void w64(Esc *e, uint64_t v) { w32(e, (uint32_t)v); w32(e, (uint32_t)(v >> 32)); }
static void wbytes(Esc *e, const void *p, size_t n) { if (n && fwrite(p, 1, n, e->f) != n) e->mal = 1; }
static void wstr(Esc *e, const char *s, size_t cap) { size_t l = strnlen(s, cap - 1); w32(e, (uint32_t)l); wbytes(e, s, l); }

typedef struct { const uint8_t *d; uint32_t n, at; int mal; } Les;
static uint8_t r8(Les *l) { if (l->at + 1 > l->n) { l->mal = 1; return 0; } return l->d[l->at++]; }
static uint32_t r32(Les *l)
{
	if (l->at + 4 > l->n) { l->mal = 1; return 0; }
	uint32_t v = tz_u32(l->d + l->at);
	l->at += 4;
	return v;
}
static uint64_t r64(Les *l) { uint64_t a = r32(l); return a | ((uint64_t)r32(l) << 32); }
static void rbytes(Les *l, void *p, size_t n)
{
	if (l->at + n > l->n) { l->mal = 1; memset(p, 0, n); return; }
	memcpy(p, l->d + l->at, n);
	l->at += (uint32_t)n;
}
static void rstr(Les *l, char *dst, size_t cap)
{
	uint32_t n = r32(l);
	if (l->mal || n >= cap || l->at + n > l->n) { l->mal = 1; dst[0] = 0; return; }
	memcpy(dst, l->d + l->at, n);
	dst[n] = 0;
	l->at += n;
}

int tz_guiao_gravar(TzMundo *m, void *dados, FILE *f)
{
	(void)m;
	TzGuiao *g = dados;
	Esc e = { f, 0 };
	w32(&e, VERSAO);
	w8(&e, (uint8_t)g->iniciado); w64(&e, g->seq);
	w32(&e, (uint32_t)g->nrun);
	for (int i = 0; i < g->nrun; ++i) { w64(&e, g->run[i].due); w64(&e, g->run[i].seq); w32(&e, (uint32_t)g->run[i].regra); w32(&e, (uint32_t)g->run[i].cursor); w8(&e, g->run[i].manter); }
	w32(&e, (uint32_t)g->ntemps);
	for (int i = 0; i < g->ntemps; ++i) { wstr(&e, g->temps[i].nome, 64); w64(&e, g->temps[i].due); w64(&e, g->temps[i].seq); w8(&e, g->temps[i].contra); }
	w32(&e, (uint32_t)g->vars.nmapa);
	for (int i = 0; i < g->vars.nmapa; ++i) { wstr(&e, g->vars.mapa[i].nome, 64); w32(&e, (uint32_t)g->vars.mapa[i].valor); }
	w32(&e, (uint32_t)g->vars.nmantidas);
	for (int i = 0; i < g->vars.nmantidas; ++i) { wstr(&e, g->vars.mantidas[i].nome, 64); w32(&e, (uint32_t)g->vars.mantidas[i].valor); }
	w32(&e, (uint32_t)g->nesperando); wbytes(&e, g->esperando, (size_t)g->nesperando);
	w32(&e, (uint32_t)g->nobj_oculto); wbytes(&e, g->oculto, (size_t)g->nobj_oculto);
	for (int p = 0; p < 12; ++p) for (int k = 0; k < 256; ++k) w32(&e, g->perdidos[p][k]);
	w32(&e, g->cursor_ev);
	w8(&e, g->modal); wstr(&e, g->proximo_mapa, sizeof g->proximo_mapa); wstr(&e, g->texto_fim, sizeof g->texto_fim);
	w8(&e, (uint8_t)g->acabou); w8(&e, (uint8_t)g->ganhou_fim); w32(&e, g->tic_fim); w8(&e, g->fim_devido);
	w8(&e, (uint8_t)g->conv_activa); w32(&e, (uint32_t)g->conv_i); w32(&e, (uint32_t)g->conv_frase);
	w32(&e, (uint32_t)g->nescolhas_conv);
	for (int i = 0; i < 8; ++i) w32(&e, (uint32_t)g->escolhas_conv[i]);
	w8(&e, g->sem_apagar); w8(&e, g->sem_diplomacia); w32(&e, (uint32_t)g->hora);
	w32(&e, (uint32_t)g->nmissoes);
	for (int i = 0; i < g->nmissoes; ++i) { wstr(&e, g->missoes[i].texto, sizeof g->missoes[i].texto); wstr(&e, g->missoes[i].retrato, sizeof g->missoes[i].retrato); w32(&e, (uint32_t)g->missoes[i].id); }
	wstr(&e, g->msg.titulo, sizeof g->msg.titulo); wstr(&e, g->msg.texto, sizeof g->msg.texto); wstr(&e, g->msg.retrato, sizeof g->msg.retrato);
	w32(&e, (uint32_t)g->msg.nescolhas);
	for (int i = 0; i < 8; ++i) wstr(&e, g->msg.escolhas[i], sizeof g->msg.escolhas[i]);
	return e.mal ? -1 : 0;
}

int tz_guiao_carregar(TzMundo *m, void *dados, FILE *f, uint32_t tam)
{
	(void)m;
	TzGuiao *g = dados;
	if (tam > 64u << 20) return tz_falha("bloque GUIA demasiado grande");
	uint8_t *buf = tz_malloc(tam ? tam : 1);
	if (!buf) return -1;
	if (tam && fread(buf, 1, tam, f) != tam) { free(buf); return tz_falha("bloque GUIA truncado"); }
	Les l = { buf, tam, 0, 0 };
	int rc = -1;
	if (r32(&l) != VERSAO) { tz_falha("versión del bloque GUIA desconocida"); goto fim; }
	g->iniciado = r8(&l); g->seq = r64(&l);
	uint32_t n = r32(&l);
	if (n > (uint32_t)g->nregras) { tz_falha("GUIA: demasiadas reglas"); goto fim; }
	if (g->caprun < g->nregras) { free(g->run); g->run = calloc((size_t)g->nregras + 1, sizeof *g->run); g->caprun = g->nregras + 1; }
	g->nrun = (int)n;
	for (uint32_t i = 0; i < n; ++i) {
		g->run[i].due = r64(&l); g->run[i].seq = r64(&l); g->run[i].regra = (int)r32(&l); g->run[i].cursor = (int)r32(&l); g->run[i].manter = r8(&l);
		if (g->run[i].regra < 0 || g->run[i].regra >= g->nregras) { tz_falha("GUIA: regla fuera de rango"); goto fim; }
		if (g->run[i].cursor >= g->regras[g->run[i].regra].nacoes) { tz_falha("GUIA: acción fuera de rango"); goto fim; }
	}
	n = r32(&l);
	if (n > 4096) { tz_falha("GUIA: demasiados temporizadores"); goto fim; }
	if (g->captemps < (int)n) { free(g->temps); g->temps = calloc((size_t)n + 1, sizeof *g->temps); g->captemps = (int)n + 1; }
	g->ntemps = (int)n;
	for (uint32_t i = 0; i < n; ++i) { rstr(&l, g->temps[i].nome, 64); g->temps[i].due = r64(&l); g->temps[i].seq = r64(&l); g->temps[i].contra = r8(&l); }
	n = r32(&l);
	if (n > 256) { tz_falha("GUIA: demasiadas variables"); goto fim; }
	g->vars.nmapa = (int)n;
	for (uint32_t i = 0; i < n; ++i) { rstr(&l, g->vars.mapa[i].nome, 64); g->vars.mapa[i].valor = (int32_t)r32(&l); }
	n = r32(&l);
	if (n > 256) { tz_falha("GUIA: demasiadas variables"); goto fim; }
	g->vars.nmantidas = (int)n;
	for (uint32_t i = 0; i < n; ++i) { rstr(&l, g->vars.mantidas[i].nome, 64); g->vars.mantidas[i].valor = (int32_t)r32(&l); }
	n = r32(&l);
	if (n != (uint32_t)(g->nacoes ? g->nacoes : 1)) { tz_falha("GUIA: las acciones no coinciden"); goto fim; }
	free(g->esperando); g->esperando = calloc(n, 1); g->nesperando = (int)n;
	rbytes(&l, g->esperando, n);
	n = r32(&l);
	if (n > (1u << 24)) { tz_falha("GUIA: demasiados objetos"); goto fim; }
	free(g->oculto); g->oculto = calloc(n ? n : 1, 1); g->nobj_oculto = (int)n;
	rbytes(&l, g->oculto, n);
	for (int p = 0; p < 12; ++p) for (int k = 0; k < 256; ++k) g->perdidos[p][k] = r32(&l);
	g->cursor_ev = r32(&l);
	g->modal = r8(&l); rstr(&l, g->proximo_mapa, sizeof g->proximo_mapa); rstr(&l, g->texto_fim, sizeof g->texto_fim);
	g->acabou = r8(&l); g->ganhou_fim = r8(&l); g->tic_fim = r32(&l); g->fim_devido = r8(&l);
	g->conv_activa = r8(&l); g->conv_i = (int)r32(&l); g->conv_frase = (int)r32(&l);
	g->nescolhas_conv = (int)r32(&l);
	for (int i = 0; i < 8; ++i) g->escolhas_conv[i] = (int)r32(&l);
	g->sem_apagar = r8(&l); g->sem_diplomacia = r8(&l); g->hora = (int)r32(&l);
	if (g->conv_activa) {
		if (g->conv_i < 0 || g->conv_i >= g->nconversas) { tz_falha("GUIA: conversación fuera de rango"); goto fim; }
		const TzConversa *cv = &g->conversas[g->conv_i];
		if (g->conv_frase >= cv->nfrases || g->nescolhas_conv < 0 || g->nescolhas_conv > 8) { tz_falha("GUIA: frase fuera de rango"); goto fim; }
		for (int i = 0; i < g->nescolhas_conv; ++i) if (g->escolhas_conv[i] < 0 || g->escolhas_conv[i] >= cv->nfrases) { tz_falha("GUIA: frase fuera de rango"); goto fim; }
	}
	n = r32(&l);
	if (n > 4096) { tz_falha("GUIA: demasiados objetivos"); goto fim; }
	free(g->missoes); g->missoes = calloc(n ? n : 1, sizeof *g->missoes); g->capmissoes = (int)(n ? n : 1); g->nmissoes = (int)n;
	for (uint32_t i = 0; i < n; ++i) { rstr(&l, g->missoes[i].texto, sizeof g->missoes[i].texto); rstr(&l, g->missoes[i].retrato, sizeof g->missoes[i].retrato); g->missoes[i].id = (int32_t)r32(&l); }
	rstr(&l, g->msg.titulo, sizeof g->msg.titulo); rstr(&l, g->msg.texto, sizeof g->msg.texto); rstr(&l, g->msg.retrato, sizeof g->msg.retrato);
	g->msg.nescolhas = (int)r32(&l);
	if (g->msg.nescolhas < 0 || g->msg.nescolhas > 8) { tz_falha("GUIA: elecciones fuera de rango"); goto fim; }
	for (int i = 0; i < 8; ++i) rstr(&l, g->msg.escolhas[i], sizeof g->msg.escolhas[i]);
	if (l.mal) { tz_falha("bloque GUIA truncado"); goto fim; }
	/* lo que se estaba mostrando se vuelve a pedir a la interfaz */
	g->fila_ini = g->fila_n = 0;
	g->cursor_ev = m->ev_seq;
	if (g->conv_activa) { TzPedido *p = &g->fila[0]; memset(p, 0, sizeof *p); p->tipo = TZ_PD_FALA; p->obj = TZ_NENHUM; g->fila_n = 1; }
	else if (g->modal) { TzPedido *p = &g->fila[0]; memset(p, 0, sizeof *p); p->tipo = TZ_PD_AVISO_JANELA; p->obj = TZ_NENHUM; g->fila_n = 1; }
	rc = 0;
fim:
	free(buf);
	return rc;
}
