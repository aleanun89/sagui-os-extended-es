/* SPDX-License-Identifier: GPL-3.0-or-later */
/* guiao_ler.c - lector de los guiones de misión de Tzar: .events, .actions, .rules, .defeat.messages, .conv y .objext (D417, fase 7).
 *
 * Traducción a C de ReTzar (https://github.com/ColCarlo/ReTzar, GPL-3.0-or-later): src/script.rs y src/dialogue.rs, y de
 * src/world.rs la lectura de .objext y .diplomacy.
 *
 * Los tres ficheros son series de registros: nombre de 32 bytes (con NUL; el resto es memoria vieja del editor), un
 * u16 de tipo (en las reglas, cuántos nombres de acción siguen), un u16 de longitud y los datos. Las palabras 0x0c..0x12 de
 * la cabecera del mapa dan cuántas reglas, acciones y sucesos hay. Sucesos: tipos 1000-1007; acciones: 0-27. */
#include "guiao_i.h"
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* Windows-1252 -> ISO-8859-1: lo que no existe en Latin-1 pasa a su parecido en ASCII */
static void latin1(char *s)
{
	for (unsigned char *p = (unsigned char *)s; *p; ++p) {
		if (*p < 0x80 || *p >= 0xa0) continue;
		switch (*p) {
		case 0x80: *p = 'E'; break;
		case 0x82: case 0x91: case 0x92: *p = '\''; break;
		case 0x84: case 0x93: case 0x94: *p = '"'; break;
		case 0x85: *p = '.'; break;
		case 0x8a: *p = 'S'; break;
		case 0x8c: *p = 'O'; break;
		case 0x96: case 0x97: *p = '-'; break;
		case 0x99: *p = 'T'; break;
		case 0x9a: *p = 's'; break;
		case 0x9c: *p = 'o'; break;
		default: *p = '?'; break;
		}
	}
}

typedef struct { const uint8_t *d; uint32_t n, at; int mal; } Lector;

static const uint8_t *tomar(Lector *r, uint32_t k)
{
	if ((uint64_t)r->at + k > r->n) { r->mal = 1; return NULL; }
	const uint8_t *p = r->d + r->at;
	r->at += k;
	return p;
}
static uint8_t l8(Lector *r) { const uint8_t *p = tomar(r, 1); return p ? p[0] : 0; }
static uint16_t l16(Lector *r) { const uint8_t *p = tomar(r, 2); return p ? tz_u16(p) : 0; }
static uint32_t l32(Lector *r) { const uint8_t *p = tomar(r, 4); return p ? tz_u32(p) : 0; }

/* una cadena con NUL, sin bytes <= 0x20 a los lados */
static char *ltexto(Lector *r, int recortar)
{
	if (r->mal || r->at > r->n) { r->mal = 1; return tz_strdup(""); }
	const uint8_t *s = r->d + r->at;
	uint32_t k = 0, resto = r->n - r->at;
	while (k < resto && s[k]) k++;
	if (k >= resto) { r->mal = 1; return tz_strdup(""); }   /* texto sin terminador */
	r->at += k + 1;
	uint32_t a = 0, b = k;
	if (recortar) {
		while (a < k && s[a] <= 0x20) a++;
		while (b > a && s[b - 1] <= 0x20) b--;
	}
	char *out = tz_malloc((size_t)(b - a) + 1);
	if (!out) { r->mal = 1; return NULL; }
	memcpy(out, s + a, b - a);
	out[b - a] = 0;
	latin1(out);
	return out;
}

static void sel_liberar(TzSel *s) { free(s->ids); free(s->conta); memset(s, 0, sizeof *s); }
static void area_liberar(TzArea *a) { free(a->raio); memset(a, 0, sizeof *a); }

static void sel_ler(Lector *r, TzSel *s)
{
	memset(s, 0, sizeof *s);
	s->por_classe = l8(r) != 0;
	s->no_maximo = l8(r) != 0;
	s->classe = l16(r);
	s->reinos = l16(r);
	s->todos = l8(r) != 0;
	l8(r);   /* una marca del editor que el juego nunca lee */
	uint16_t n = l16(r);
	if ((uint32_t)n * 2 > r->n - r->at) { r->mal = 1; n = 0; }
	if (n) {
		s->ids = tz_malloc((size_t)n * 2);
		if (!s->ids) { r->mal = 1; n = 0; }
	}
	for (int i = 0; i < n; ++i) s->ids[i] = l16(r);
	s->nids = n;
	s->conta = ltexto(r, 1);
}

static void area_ler(Lector *r, TzArea *a)
{
	memset(a, 0, sizeof *a);
	a->em_volta = l8(r) != 0;
	for (int i = 0; i < 4; ++i) a->rect[i] = (int16_t)l32(r);
	a->objeto = l16(r);
	a->raio = ltexto(r, 1);
}

static void suceso_liberar(TzSuceso *e) { sel_liberar(&e->sel); area_liberar(&e->area); free(e->expr); }
static void accion_liberar(TzAccion *a)
{
	for (int i = 0; i < 5; ++i) free(a->t[i]);
	sel_liberar(&a->sel); area_liberar(&a->a); area_liberar(&a->a2);
}

/* un registro: nombre, tipo y datos; devuelve 0 si está entero */
static int registro(Lector *r, char nome[40], uint16_t *tipo, Lector *corpo)
{
	const uint8_t *h = tomar(r, 36);
	if (!h) return -1;
	memset(nome, 0, 40);
	int k = 0;
	while (k < 32 && h[k]) { nome[k] = (char)h[k]; k++; }
	while (k > 0 && (unsigned char)nome[k - 1] <= 0x20) nome[--k] = 0;
	latin1(nome);
	*tipo = tz_u16(h + 32);
	uint16_t len = tz_u16(h + 34);
	const uint8_t *b = tomar(r, len);
	if (!b) return -1;
	corpo->d = b; corpo->n = len; corpo->at = 0; corpo->mal = 0;
	return 0;
}

static int suceso_ler(TzSuceso *e, uint16_t tipo, Lector *r)
{
	e->tipo = tipo;
	switch (tipo) {
	case TZ_EVT_EXISTE: case TZ_EVT_DESTRUIDO: sel_ler(r, &e->sel); break;
	case TZ_EVT_DENTRO:
		sel_ler(r, &e->sel);
		e->sup.por_classe = l8(r) != 0; e->sup.classe = l16(r); e->sup.objeto = l16(r); e->sup.reinos = l16(r);
		break;
	case TZ_EVT_SAIU: break;
	case TZ_EVT_NA_AREA: sel_ler(r, &e->sel); area_ler(r, &e->area); break;
	case TZ_EVT_DERROTADO: case TZ_EVT_VENCEU: e->mascara = l16(r); break;
	case TZ_EVT_EXPRESSAO: sel_ler(r, &e->sel); e->expr = ltexto(r, 1); break;
	default: return -1;
	}
	return r->mal ? -1 : 0;
}

/* qué campos usa cada acción: ver el comentario de TzAccion en guiao.h y la tabla de guiao.c */
static int accion_ler(TzAccion *a, uint16_t tipo, Lector *r)
{
	a->tipo = tipo;
	switch (tipo) {
	case TZ_ACC_MISSAO: a->t[0] = ltexto(r, 1); a->t[1] = ltexto(r, 1); break;
	case TZ_ACC_TIRAR_MISSAO: a->t[0] = ltexto(r, 1); break;
	case TZ_ACC_DIPLOMACIA: a->b[0] = l8(r); a->b[1] = l8(r); a->flags = l32(r); break;
	case TZ_ACC_DONO: sel_ler(r, &a->sel); a->b[0] = l8(r); break;
	case TZ_ACC_PROPRIEDADE: sel_ler(r, &a->sel); a->t[0] = ltexto(r, 1); break;
	case TZ_ACC_EXPRESSAO: a->t[0] = ltexto(r, 1); break;
	case TZ_ACC_TEMPORIZADOR: a->t[0] = ltexto(r, 1); a->b[0] = l8(r) != 0; break;
	case TZ_ACC_PARAR_TEMP: a->t[0] = ltexto(r, 1); break;
	case TZ_ACC_PERDER: case TZ_ACC_GANHAR: a->u = l16(r); a->t[0] = ltexto(r, 1); break;
	case TZ_ACC_MANTER: break;
	case TZ_ACC_MAPA: a->t[0] = ltexto(r, 1); break;
	case TZ_ACC_TRAVAR_VISTA: a->t[0] = ltexto(r, 1); area_ler(r, &a->a); break;
	case TZ_ACC_LINHA: a->t[0] = ltexto(r, 1); break;
	case TZ_ACC_HISTORIA:
		for (int i = 0; i < 5; ++i) a->t[i] = ltexto(r, 1);
		{ uint32_t at = r->at; int m = r->mal; a->b[0] = l8(r) != 0; if (r->mal && at == r->n) { r->mal = m; a->b[0] = 0; } }
		break;
	case TZ_ACC_AVISO: a->t[0] = ltexto(r, 1); a->t[1] = ltexto(r, 1); a->t[2] = ltexto(r, 1); break;
	case TZ_ACC_CRIAR: a->u = l16(r); a->t[0] = ltexto(r, 1); a->b[0] = l8(r); area_ler(r, &a->a); a->b[1] = l8(r); break;
	case TZ_ACC_DESTRAVAR_VISTA: break;
	case TZ_ACC_ORDEM: sel_ler(r, &a->sel); area_ler(r, &a->a); a->b[0] = l8(r); break;
	case TZ_ACC_ROLAR_VISTA: area_ler(r, &a->a); area_ler(r, &a->a2); break;
	case TZ_ACC_CONVERSA: a->t[0] = ltexto(r, 1); break;
	case TZ_ACC_DESTRUIR: sel_ler(r, &a->sel); break;
	case TZ_ACC_EXPLORAR: area_ler(r, &a->a); a->t[0] = ltexto(r, 1); break;
	case TZ_ACC_APARECER: sel_ler(r, &a->sel); break;
	case TZ_ACC_ESTADO: a->b[0] = l8(r); a->b[1] = l8(r) != 0; a->b[2] = l8(r) != 0; break;
	case TZ_ACC_BLOQUEAR: sel_ler(r, &a->sel); a->b[0] = l8(r) != 0; break;
	case TZ_ACC_APAGAR: sel_ler(r, &a->sel); break;
	case TZ_ACC_ESPERAR: a->t[0] = ltexto(r, 1); break;
	default: return -1;
	}
	return r->mal ? -1 : 0;
}

void tz_guiao_liberar(TzGuiao *g)
{
	for (int i = 0; i < g->neventos; ++i) suceso_liberar(&g->eventos[i]);
	for (int i = 0; i < g->nacoes; ++i) accion_liberar(&g->acoes[i]);
	for (int i = 0; i < g->nregras; ++i) { free(g->regras[i].condicao); free(g->regras[i].acoes); }
	for (int i = 0; i < 8; ++i) free(g->derrota[i]);
	for (int i = 0; i < g->nconversas; ++i) {
		for (int j = 0; j < g->conversas[i].nfrases; ++j) {
			TzFrase *f = &g->conversas[i].frases[j];
			free(f->condicao); free(f->acao); free(f->voz); free(f->texto); free(f->seg);
		}
		free(g->conversas[i].frases);
	}
	free(g->eventos); free(g->acoes); free(g->regras); free(g->conversas); free(g->pers);
	free(g->run); free(g->temps); free(g->esperando); free(g->oculto); free(g->missoes); free(g->tmp);
	memset(g, 0, sizeof *g);
}

int tz_guiao_de_memoria(TzGuiao *g, const uint16_t cont[4], const uint8_t *ev, uint32_t nev, const uint8_t *ac, uint32_t nac,
                        const uint8_t *rg, uint32_t nrg)
{
	int rc = 0;
	uint16_t nregras = cont[0], nacoes = cont[1], neventos = cont[2];
	g->eventos = tz_malloc(((size_t)neventos + 1) * sizeof *g->eventos);
	g->acoes = tz_malloc(((size_t)nacoes + 1) * sizeof *g->acoes);
	g->regras = tz_malloc(((size_t)nregras + 1) * sizeof *g->regras);
	if (!g->eventos || !g->acoes || !g->regras) return -1;
	memset(g->eventos, 0, ((size_t)neventos + 1) * sizeof *g->eventos);
	memset(g->acoes, 0, ((size_t)nacoes + 1) * sizeof *g->acoes);
	memset(g->regras, 0, ((size_t)nregras + 1) * sizeof *g->regras);
	Lector r = { ev, nev, 0, 0 };
	for (int i = 0; i < neventos && !rc; ++i) {
		Lector b; uint16_t tipo;
		if (registro(&r, g->eventos[i].nome, &tipo, &b)) { rc = tz_falha("registro de suceso truncado"); break; }
		g->neventos = i + 1;
		if (suceso_ler(&g->eventos[i], tipo, &b)) rc = tz_falha("suceso %d de tipo %u no válido", i, tipo);
	}
	r = (Lector){ ac, nac, 0, 0 };
	for (int i = 0; i < nacoes && !rc; ++i) {
		Lector b; uint16_t tipo;
		if (registro(&r, g->acoes[i].nome, &tipo, &b)) { rc = tz_falha("registro de acción truncado"); break; }
		g->nacoes = i + 1;
		if (accion_ler(&g->acoes[i], tipo, &b)) rc = tz_falha("acción %d de tipo %u no válida", i, tipo);
	}
	r = (Lector){ rg, nrg, 0, 0 };
	for (int i = 0; i < nregras && !rc; ++i) {
		Lector b; uint16_t cnt;
		TzRegra *q = &g->regras[i];
		if (registro(&r, q->nome, &cnt, &b)) { rc = tz_falha("registro de regla truncado"); break; }
		g->nregras = i + 1;
		char *cond = ltexto(&b, 1);
		if (b.mal) { free(cond); rc = tz_falha("regla %d sin condición", i); break; }
		q->condicao = cond;
		for (char *p = cond; *p; ++p) if (*p == '\r' || *p == '\n') *p = ' ';
		q->acoes = tz_malloc(((size_t)cnt + 1) * sizeof(int));
		if (!q->acoes) { rc = -1; break; }
		for (int k = 0; k < cnt; ++k) {
			char *nome = ltexto(&b, 1);
			if (b.mal) { free(nome); break; }   /* map_while: se acaba la lista */
			if (nome && nome[0]) {
				int idx = -1;
				for (int j = 0; j < g->nacoes; ++j) if (!strcasecmp(g->acoes[j].nome, nome)) { idx = j; break; }
				q->acoes[q->nacoes++] = idx;
			}
			free(nome);
		}
	}
	if (rc) { g->erros++; }
	return rc;
}

int tz_guiao_derrotas(TzGuiao *g, const uint8_t *d, uint32_t n)
{
	Lector r = { d, n, 0, 0 };
	for (int i = 0; i < 8; ++i) {
		char *t = ltexto(&r, 1);
		l8(&r);
		if (r.mal) { free(t); break; }
		if (t && t[0]) g->derrota[i] = t; else free(t);
	}
	return 0;
}

int tz_guiao_conversas(TzGuiao *g, const uint8_t *d, uint32_t n)
{
	if (!n) return 0;
	Lector r = { d, n, 0, 0 };
	uint16_t cnt = l16(&r);
	g->conversas = tz_malloc(((size_t)cnt + 1) * sizeof *g->conversas);
	if (!g->conversas) return -1;
	memset(g->conversas, 0, ((size_t)cnt + 1) * sizeof *g->conversas);
	for (int i = 0; i < cnt; ++i) {
		TzConversa *c = &g->conversas[i];
		char *nome = ltexto(&r, 0);
		if (r.mal) { free(nome); return tz_falha("conversaciones truncadas"); }
		snprintf(c->nome, sizeof c->nome, "%s", nome);
		free(nome);
		uint16_t nf = l16(&r);
		g->nconversas = i + 1;
		c->frases = tz_malloc(((size_t)nf + 1) * sizeof *c->frases);
		if (!c->frases) return -1;
		memset(c->frases, 0, ((size_t)nf + 1) * sizeof *c->frases);
		for (int j = 0; j < nf; ++j) {
			TzFrase *f = &c->frases[j];
			f->indice = l16(&r);
			f->fala_de = l16(&r);
			f->inicio = l8(&r) != 0;
			free(ltexto(&r, 0));   /* nombre de la frase */
			f->condicao = ltexto(&r, 0);
			f->acao = ltexto(&r, 0);
			free(ltexto(&r, 0));   /* siempre vacío */
			f->voz = ltexto(&r, 0);
			f->texto = ltexto(&r, 1);
			uint16_t nl = l16(&r);
			c->nfrases = j + 1;
			if (r.mal || nl > nf) return tz_falha("conversación «%s», frase %d: enlaces no válidos", c->nome, f->indice);
			f->seg = tz_malloc(((size_t)nl + 1) * 2);
			if (!f->seg) return -1;
			for (int k = 0; k < nl; ++k) f->seg[k] = l16(&r);
			f->nseg = nl;
			/* \r\n del texto a \n */
			char *w = f->texto;
			for (char *p = f->texto; *p; ++p) { if (p[0] == '\r' && p[1] == '\n') continue; *w++ = *p; }
			*w = 0;
		}
		if (r.mal) return tz_falha("conversaciones truncadas");
	}
	if (r.at != n) return tz_falha("%u bytes sobran tras las conversaciones", (unsigned)(n - r.at));
	return 0;
}

static int objext(TzGuiao *g, const uint8_t *d, uint32_t n)
{
	if (n % 258) return tz_falha("registros de héroes no válidos");
	int cnt = (int)(n / 258);
	g->pers = tz_malloc(((size_t)cnt + 1) * sizeof *g->pers);
	if (!g->pers) return -1;
	for (int i = 0; i < cnt; ++i) {
		const uint8_t *p = d + (size_t)i * 258;
		TzPersonagem *q = &g->pers[i];
		memset(q, 0, sizeof *q);
		q->id = tz_u16(p);
		const struct { size_t off, len; char *dst; size_t cap; } c[4] = {
			{ 2, 32, q->nome, sizeof q->nome }, { 34, 32, q->subtitulo, sizeof q->subtitulo },
			{ 66, 64, q->retrato, sizeof q->retrato }, { 130, 64, q->retrato_grande, sizeof q->retrato_grande } };
		for (int k = 0; k < 4; ++k) {
			size_t l = 0;
			while (l < c[k].len && p[c[k].off + l] && l + 1 < c[k].cap) { c[k].dst[l] = (char)p[c[k].off + l]; l++; }
			c[k].dst[l] = 0;
			while (l && (unsigned char)c[k].dst[l - 1] <= 0x20) c[k].dst[--l] = 0;
			latin1(c[k].dst);
		}
	}
	g->npers = cnt;
	return 0;
}

int tz_guiao_ler(TzGuiao *g, TzHpfs *c)
{
	memset(g, 0, sizeof *g);
	const TzEntrada *h = NULL;
	for (int i = 0; i < c->n && !h; ++i) {
		size_t l = strlen(c->e[i].nome);
		if (l > 7 && !strcasecmp(c->e[i].nome + l - 7, ".header")) h = &c->e[i];
	}
	if (!h) return tz_falha("el contenedor no tiene ningún escenario (.header)");
	char base[600], nome[640];
	snprintf(base, sizeof base, "%.*s", (int)(strlen(h->nome) - 7), h->nome);
	{
		const char *barra = strrchr(base, '/');
		snprintf(g->nome_mapa, sizeof g->nome_mapa, "%s", barra ? barra + 1 : base);
	}
	uint32_t nh;
	uint8_t *cab = tz_hpfs_ler(c, h->nome, &nh);
	if (!cab) return -1;
	uint16_t cont[4] = { 0, 0, 0, 0 };
	for (int i = 0; i < 4; ++i) if (nh >= (uint32_t)(14 + 2 * i)) cont[i] = tz_u16(cab + 12 + 2 * i);
	free(cab);
	uint8_t *ev = NULL, *ac = NULL, *rg = NULL;
	uint32_t nev = 0, nac = 0, nrg = 0;
#define TROZO(suf, p, n) do { snprintf(nome, sizeof nome, "%s." suf, base); p = tz_hpfs_entrada(c, nome) ? tz_hpfs_ler(c, nome, &n) : NULL; } while (0)
	TROZO("events", ev, nev);
	TROZO("actions", ac, nac);
	TROZO("rules", rg, nrg);
	int rc = tz_guiao_de_memoria(g, cont, ev ? ev : (const uint8_t *)"", nev, ac ? ac : (const uint8_t *)"", nac,
	                             rg ? rg : (const uint8_t *)"", nrg);
	char motivo[256];
	motivo[0] = 0;
	if (rc) snprintf(motivo, sizeof motivo, "%s", tz_erro());
	free(ev); free(ac); free(rg);
	if (rc) {   /* un guion dañado no impide abrir el mapa: queda vacío */
		int er = g->erros;
		tz_guiao_liberar(g);
		g->erros = er;
		snprintf(g->nome_mapa, sizeof g->nome_mapa, "%s", base);
	}
	uint8_t *d; uint32_t n;
	TROZO("defeat.messages", d, n);
	if (d) { tz_guiao_derrotas(g, d, n); free(d); }
	TROZO("conv", d, n);
	if (d) { if (tz_guiao_conversas(g, d, n)) { g->erros++; if (!motivo[0]) snprintf(motivo, sizeof motivo, "%s", tz_erro()); } free(d); }
	TROZO("objext", d, n);
	if (d) { if (objext(g, d, n)) { g->erros++; if (!motivo[0]) snprintf(motivo, sizeof motivo, "%s", tz_erro()); } free(d); }
	g->jogador = 1;
	TROZO("kingdominfo", d, n);
	if (d) {
		if (n == 8 * 152) {
			int primeiro = 0, humano = 0;
			for (int i = 7; i >= 0; --i) {
				if (d[i * 152 + 64]) { primeiro = i + 1; if (d[i * 152 + 122] == 0) humano = i + 1; }
			}
			g->jogador = humano ? humano : primeiro ? primeiro : 1;
		}
		free(d);
	}
	TROZO("diplomacy", d, n);
	if (d) {
		if (n == 256) { for (int i = 0; i < 64; ++i) g->diplomacia[i] = tz_u32(d + i * 4); g->tem_diplomacia = 1; }
		free(d);
	}
#undef TROZO
	if (motivo[0]) return tz_falha("%s", motivo);
	return rc;
}
