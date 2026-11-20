/* SPDX-License-Identifier: GPL-3.0-or-later */
/* ia_regras.c - el guion de un jugador de la IA en marcha: sus condiciones, el estado de sus reglas, las reglas que
 * lee cada valor, la fase de construir que persigue sus metas y las metas cumplidas, con lo que reserva cada parte del
 * jugador (D417, fase 6).
 *
 * Traducción a C de ReTzar (https://github.com/ColCarlo/ReTzar, GPL-3.0-or-later), src/gameplay/ai/rules.rs:
 * condiciones (0x407e60), estado de las reglas (0x408420), listas de vigilancia (0x41ab10), fase de construir
 * (0x408960) y metas (0x409550), con las reservas IA+0x914 a IA+0x988.
 * Diferencias: los recuentos de otros reinos (ENEMY, ALLY) son de lo que el reino ve; CONVERT (la taberna), las
 * criaturas del mago, la yihad y la cruzada y RESIGN no hacen nada (docs/tzar.md, «La IA (fase 6)»).
 */
#include "ia_int.h"
#include <ctype.h>
#include <stdio.h>

/* ------------------------------------------------------------------ qué reglas leen cada valor (0x41ab10) */
static int chave(const TzIa *ia, int que, int n)
{
	int R = ia->prog.nregras, G = ia->prog.ngat;
	switch (que) {
	case V_REGRA: return n >= 0 && n < R ? n : -1;
	case V_GATILHO: return n >= 0 && n < G ? R + n : -1;
	case V_RECURSO: return n >= 0 && n < 4 ? R + G + n : -1;
	case V_TIPO: return n >= 0 && n < ia->b->ntipos ? R + G + 4 + n : -1;
	case V_SONDA: return R + G + 4 + ia->b->ntipos;
	default: return R + G + 4 + ia->b->ntipos + 1;
	}
}

static const uint32_t *trecho(const TzIaPrograma *p, TzIaTrecho t, uint32_t *n)
{
	uint32_t ini = t.ini < (uint32_t)p->nlista ? t.ini : (uint32_t)p->nlista;
	uint32_t fim = ini + t.n;
	if (fim > (uint32_t)p->nlista || fim < ini) fim = (uint32_t)p->nlista;
	*n = fim - ini;
	return p->lista + ini;
}

/* las claves que lee la regla r; chama(ia, chave, r, ctx) por cada una */
static void chaves_da_regra(const TzIa *ia, int r, void (*f)(const TzIa *, int, int, void *), void *ctx)
{
	const TzIaPrograma *p = &ia->prog;
	const TzIaRegra *rg = &p->regra[r];
	int pct = 0;
	uint32_t n;
	const uint32_t *l = trecho(p, rg->quando, &n);
	for (uint32_t j = 0; j < n; ++j) {
		uint32_t c = l[j];
		/* un NOT lleva a la condición que niega */
		for (int guarda = 0; c < (uint32_t)p->nconds && p->cond[c].tipo == TZ_IA_C_NAO && guarda < 64; ++guarda) c = p->cond[c].n;
		if (c >= (uint32_t)p->nconds) continue;
		const TzIaCond *cd = &p->cond[c];
		switch (cd->tipo) {
		case TZ_IA_C_REGRA: f(ia, chave(ia, V_REGRA, (int)cd->n), r, ctx); break;
		case TZ_IA_C_GATILHO: f(ia, chave(ia, V_GATILHO, (int)(cd->n & 0x7fffffff)), r, ctx); break;
		case TZ_IA_C_RECURSO: f(ia, chave(ia, V_RECURSO, cd->recurso), r, ctx); break;
		case TZ_IA_C_CONTA:
			pct |= (cd->n & TZ_IA_PCT) != 0;
			f(ia, chave(ia, V_TIPO, cd->classe), r, ctx);
			break;
		case TZ_IA_C_TEMPO: case TZ_IA_C_ACASO: case TZ_IA_C_EXTERNO: case TZ_IA_C_INIMIGO: case TZ_IA_C_ALIADO:
			f(ia, chave(ia, V_SONDA, 0), r, ctx);
			break;
		}
	}
	l = trecho(p, rg->gatilhos, &n);
	for (uint32_t j = 0; j < n; ++j) f(ia, chave(ia, V_GATILHO, (int)(l[j] & 0x7fffffff)), r, ctx);
	if (rg->tem_meta) {
		switch (rg->meta_tipo) {
		case 0: case TZ_IA_M_RENDER: break;
		case TZ_IA_M_BLOQUEIO: case TZ_IA_M_CONVERTER: f(ia, chave(ia, V_SONDA, 0), r, ctx); break;
		case TZ_IA_M_OURO: f(ia, chave(ia, V_RECURSO, 0), r, ctx); break;
		case TZ_IA_M_PEDRA: f(ia, chave(ia, V_RECURSO, 1), r, ctx); break;
		case TZ_IA_M_COMIDA: f(ia, chave(ia, V_RECURSO, 2), r, ctx); break;
		case TZ_IA_M_MADEIRA: f(ia, chave(ia, V_RECURSO, 3), r, ctx); break;
		default:
			pct |= (rg->meta_n & TZ_IA_PCT) != 0;
			f(ia, chave(ia, V_TIPO, rg->meta_tipo), r, ctx);
		}
	}
	if (pct) f(ia, chave(ia, V_PCT, 0), r, ctx);
}

static void conta_chave(const TzIa *ia, int k, int r, void *ctx) { if (k >= 0) ia->vig_ini[k + 1]++; }
static void poe_chave(const TzIa *ia, int k, int r, void *ctx)
{
	int *pos = ctx;
	if (k >= 0) ia->vig_reg[pos[k]++] = r;
}

static void vigilancia(TzIa *ia)
{
	free(ia->vig_ini); free(ia->vig_reg);
	ia->nchaves = chave(ia, V_PCT, 0) + 1;
	ia->vig_ini = calloc((size_t)ia->nchaves + 1, sizeof *ia->vig_ini);
	if (!ia->vig_ini) { ia->vig_reg = NULL; ia->nchaves = 0; return; }
	for (int r = 0; r < ia->prog.nregras; ++r) chaves_da_regra(ia, r, conta_chave, NULL);
	for (int k = 0; k < ia->nchaves; ++k) ia->vig_ini[k + 1] += ia->vig_ini[k];
	ia->vig_reg = malloc(((size_t)ia->vig_ini[ia->nchaves] + 1) * sizeof *ia->vig_reg);
	int *pos = malloc(((size_t)ia->nchaves + 1) * sizeof *pos);
	if (!ia->vig_reg || !pos) { free(pos); free(ia->vig_ini); free(ia->vig_reg); ia->vig_ini = NULL; ia->vig_reg = NULL; ia->nchaves = 0; return; }
	memcpy(pos, ia->vig_ini, ((size_t)ia->nchaves + 1) * sizeof *pos);
	for (int r = 0; r < ia->prog.nregras; ++r) chaves_da_regra(ia, r, poe_chave, pos);
	free(pos);
	ia->vig_feito = 1;
}

/* 0x407d00: las reglas que leen el valor se evalúan otra vez */
void ia_tocar(TzIa *ia, int que, int n)
{
	if (!ia->vig_feito) vigilancia(ia);
	int k = ia->nchaves ? chave(ia, que, n) : -1;
	if (k < 0) return;
	for (int j = ia->vig_ini[k]; j < ia->vig_ini[k + 1]; ++j) ia->prog.regra[ia->vig_reg[j]].flags |= TZ_IA_R_SUJA;
}

/* ------------------------------------------------------------------ recuentos */
int ia_tec(const TzIa *ia, int tipo)
{
	if (tipo < TZ_NCLASSES || tipo >= ia->b->ntipos || !ia->m->cat) return -1;
	return tz_catalogo_tec(ia->m->cat, ia->b->tipo[tipo]);
}

int ia_trab_frente(const TzObj *o, int tipo, int arg)
{
	return o->ator && o->ator->ntrab > 0 && o->ator->trab[0].tipo == tipo && o->ator->trab[0].arg == arg;
}

/* 0x4976a0: el registro de un tipo del reino k: cuántos hay (obras incluidas, o investigaciones hechas) y cuántos van
 * en camino (obras, unidades e investigaciones empezadas). De otro reino, solo lo que se ve. */
void ia_registro(const TzIa *ia, int k, int tipo, uint32_t *feitos, uint32_t *em_curso)
{
	const TzMundo *m = ia->m;
	uint32_t f = 0, e = 0;
	if (tipo > 0 && tipo < TZ_NCLASSES) {
		for (TzId i = 0; i < m->nobj; ++i) {
			const TzObj *o = &m->obj[i];
			if (o->dono != k || !ia_contado(o)) continue;
			if (k != ia->k && !ia_ve(ia, i)) continue;
			if (o->classe == tipo) {
				f++;
				if (tz_classe_e_edificio(o->classe) && ia_obra(o)) e++;
			}
			if (k == ia->k && ia_trab_frente(o, TZ_J_TREINAR, tipo)) e++;
		}
	} else if (tz_ia_bib_categoria(ia->b, tipo) == 3) {
		int t = ia_tec(ia, tipo);
		if (t >= 0 && k == ia->k) {
			if (m->reino[k].pesquisas) f = m->reino[k].pesquisas[t];
			for (TzId i = 0; i < m->nobj; ++i)
				if (m->obj[i].dono == k && ia_contado(&m->obj[i]) && ia_trab_frente(&m->obj[i], TZ_J_PESQUISAR, t)) e++;
		}
	}
	*feitos = f;
	*em_curso = e;
}

/* 0x407da0: lo que el jugador tiene de una meta y cuánto va en camino; los recursos son lo que puede gastar */
void ia_conta(const TzIa *ia, int tipo, uint32_t *feitos, uint32_t *em_curso)
{
	*em_curso = 0;
	switch (tipo) {
	case TZ_IA_M_OURO: *feitos = (uint32_t)ia->disponivel[0]; return;
	case TZ_IA_M_PEDRA: *feitos = (uint32_t)ia->disponivel[1]; return;
	case TZ_IA_M_COMIDA: *feitos = (uint32_t)ia->disponivel[2]; return;
	case TZ_IA_M_MADEIRA: *feitos = (uint32_t)ia->disponivel[3]; return;
	case TZ_IA_M_CONVERTER: case TZ_IA_M_BLOQUEIO: case TZ_IA_M_RENDER: *feitos = 0; return;
	case TZC_FIELD: *feitos = (uint32_t)ia->campos.n; return;
	default: ia_registro(ia, ia->k, tipo, feitos, em_curso);
	}
}

/* K+0x62: el tope de población de la partida; un % de un guion es parte de él */
uint32_t ia_limite(const TzIa *ia) { return ia->m->reino[ia->k].populacao_limite; }
static uint32_t querido(uint32_t n, uint32_t limite)
{
	return n & TZ_IA_PCT ? (uint32_t)(((uint64_t)((n & 0x7fff) * limite)) / 100) : n;
}

static uint32_t unidades_de(const TzIa *ia, int b)
{
	if (b == ia->k) return ia->m->reino[b].populacao;
	uint32_t n = 0;   /* de otro reino, las que se ven */
	for (TzId i = 0; i < ia->m->nobj; ++i) {
		const TzObj *o = &ia->m->obj[i];
		if (o->dono == b && ia_contado(o) && tz_classe_e_unidade(o->classe) && o->classe != TZC_COW && ia_ve(ia, i)) n++;
	}
	return n;
}

static int avalia_categoria(const TzIa *ia, int tipo, uint32_t f, uint32_t e, uint32_t n)
{
	switch (tz_ia_bib_categoria(ia->b, tipo)) {
	case 1: case 3: return f >= n;
	case 2: return f - e >= n;    /* como el original: sin saturar */
	default: return 0;
	}
}

/* 0x407e60: una condición del guion */
static int vale(TzIa *ia, uint32_t n, int prof)
{
	const TzIaPrograma *p = &ia->prog;
	if (n >= (uint32_t)p->nconds || prof > 64) return 0;
	TzIaCond c = p->cond[n];
	switch (c.tipo) {
	case TZ_IA_C_REGRA: return c.n < (uint32_t)p->nregras && (p->regra[c.n].flags & TZ_IA_R_SATISFEITA) != 0;
	case TZ_IA_C_GATILHO: return c.n < (uint32_t)p->ngat && p->gat[c.n].ligado;
	case TZ_IA_C_RECURSO: return ia->disponivel[c.recurso & 3] >= (int32_t)c.n;
	case TZ_IA_C_CONTA: {
		uint32_t q = querido(c.n, ia_limite(ia));
		if (!c.classe) return unidades_de(ia, ia->k) >= q;
		uint32_t f, e;
		ia_conta(ia, c.classe, &f, &e);
		return avalia_categoria(ia, c.classe, f, e, q);
	}
	case TZ_IA_C_TEMPO: return ia_ms(ia) >= c.n;
	case TZ_IA_C_ACASO:
		/* se sortea una vez, R(0, 100), y se guarda (+6) */
		if (!ia->sorteio[n]) ia->sorteio[n] = c.n >= (uint32_t)ia_rng(ia, 0, 100) ? 2 : 1;
		return ia->sorteio[n] == 2;
	case TZ_IA_C_EXTERNO:
		return c.n < (uint32_t)p->ntextos && tz_ia_externo(ia->m, ia->k, p->texto[c.n]) != 0;
	case TZ_IA_C_NAO: return !vale(ia, c.n, prof + 1);
	case TZ_IA_C_CIV: return ia->civ == c.recurso;
	case TZ_IA_C_DIFICULDADE: return ia->m->regras.dificuldade >= c.recurso;
	case TZ_IA_C_INIMIGO: case TZ_IA_C_ALIADO: {
		uint32_t q = querido(c.n, ia_limite(ia)), f = 0, e = 0;
		for (int b = 1; b <= TZ_REINOS; ++b) {
			if (!ia->m->reino[b].ativo && b != ia->k) continue;
			int rel = tz_mundo_relacao(ia->m, ia->k, b);
			if (c.tipo == TZ_IA_C_INIMIGO ? rel >= 0 : rel <= 0) continue;
			if (!c.classe) { f += unidades_de(ia, b); continue; }
			uint32_t a, u;
			if (b == ia->k) ia_conta(ia, c.classe, &a, &u);
			else ia_registro(ia, b, c.classe, &a, &u);
			f += a;
			e += u;
		}
		if (!c.classe) return f >= q;
		return avalia_categoria(ia, c.classe, f, e, q);
	}
	}
	return 0;
}

/* 0x408270: la meta está alcanzada (sin meta, o bastante de ella sin contar las obras); un % nunca lo está */
static int satisfeita(const TzIa *ia, int r)
{
	const TzIaRegra *rg = &ia->prog.regra[r];
	if (!rg->tem_meta || !rg->meta_tipo) return 1;
	uint32_t f, e;
	ia_conta(ia, rg->meta_tipo, &f, &e);
	if (tz_ia_bib_categoria(ia->b, rg->meta_tipo) == 2) f = f > e ? f - e : 0;
	return f >= rg->meta_n;
}

/* 0x408220: la meta va en camino (bastante de ella con las obras) */
static int em_caminho(const TzIa *ia, int r)
{
	const TzIaRegra *rg = &ia->prog.regra[r];
	if (!rg->tem_meta || !rg->meta_tipo) return 0;
	uint32_t f, e;
	ia_conta(ia, rg->meta_tipo, &f, &e);
	return f >= rg->meta_n;
}

void ia_falar(TzIa *ia, const char *texto)
{
	uint32_t s = ia_ms(ia) / 1000;
	snprintf(ia->falas[ia->nfalas % IA_FALAS], sizeof ia->falas[0], "%s: [%02u:%02u:%02u]: %s",
	         ia->m->reino[ia->k].nome, s / 3600, s / 60 % 60, s % 60, texto);
	ia->nfalas++;
}

/* 0x408420: la regla evaluada otra vez */
static void avaliar(TzIa *ia, int r)
{
	TzIaPrograma *p = &ia->prog;
	TzIaRegra *rg = &p->regra[r];
	uint32_t velho = rg->flags;
	uint32_t f = (velho & ~TZ_IA_R_SUJA) | TZ_IA_R_VERDADE;
	rg->flags = f;
	uint32_t n;
	const uint32_t *l = trecho(p, rg->quando, &n);
	for (uint32_t j = 0; j < n; ++j)
		if (!vale(ia, l[j], 0)) { f &= ~TZ_IA_R_VERDADE; break; }
	if (f & TZ_IA_R_VERDADE) {
		if (satisfeita(ia, r)) f = (f | TZ_IA_R_SATISFEITA) & ~TZ_IA_R_EM_CURSO;
		else {
			f &= ~TZ_IA_R_SATISFEITA;
			if (em_caminho(ia, r)) f |= TZ_IA_R_EM_CURSO;
			else f &= ~TZ_IA_R_EM_CURSO;
		}
	} else {
		f &= ~(TZ_IA_R_SATISFEITA | TZ_IA_R_EM_CURSO);
	}
	rg->flags = f;
	if (f & TZ_IA_R_SATISFEITA) {
		/* los gatillos se ponen en cada evaluación que la encuentra cumplida */
		l = trecho(p, rg->gatilhos, &n);
		for (uint32_t j = 0; j < n; ++j) {
			uint8_t liga = !(l[j] & 0x80000000u);
			uint32_t t = l[j] & 0x7fffffff;
			if (t < (uint32_t)p->ngat && p->gat[t].ligado != liga) {
				ia_tocar(ia, V_GATILHO, (int)t);
				p->gat[t].ligado = liga;
			}
		}
	}
	if (!((f ^ velho) & TZ_IA_R_SATISFEITA)) return;
	ia_tocar(ia, V_REGRA, r);
	if (!(f & TZ_IA_R_SATISFEITA)) return;
	l = trecho(p, rg->esquece, &n);
	for (uint32_t j = 0; j < n; ++j)
		if (l[j] < (uint32_t)p->nregras) p->regra[l[j]].flags |= TZ_IA_R_ESQUECIDA;
	if (!rg->diz) return;
	if (rg->tem_meta && rg->meta_tipo && !(velho & TZ_IA_R_EM_CURSO)) return;
	if (!strncmp(rg->diz, "HMAI ", 5)) {
		/* una orden «AI» del guion (0x404460) */
		char o[64];
		const char *s = rg->diz + 5;
		while (*s == ' ') s++;
		int i = 0;
		for (; s[i] && i < (int)sizeof o - 1; ++i) o[i] = (char)tolower((unsigned char)s[i]);
		while (i > 0 && o[i - 1] == ' ') i--;
		o[i] = 0;
		tz_ia_ordem(ia, o);
		return;
	}
	if (ia->modo & TZ_IA_FALA) ia_falar(ia, rg->diz);
}

/* ------------------------------------------------------------------ costes y reservas */
/* 0x40af70: lo que cuesta uno de la meta */
void ia_custo(const TzIa *ia, int tipo, int32_t c[4])
{
	memset(c, 0, 4 * sizeof *c);
	switch (tipo) {
	case TZ_IA_M_CONVERTER: c[2] = 10; return;
	case TZ_IA_M_OURO: c[0] = 1; return;
	case TZ_IA_M_PEDRA: c[1] = 1; return;
	case TZ_IA_M_COMIDA: c[2] = 1; return;
	case TZ_IA_M_MADEIRA: c[3] = 1; return;
	case TZ_IA_M_BLOQUEIO: case TZ_IA_M_RENDER: return;
	}
	uint32_t u[TZ_NREC] = { 0 }, t = 0;
	if (tipo > 0 && tipo < TZ_NCLASSES) tz_mundo_custo_classe(ia->m, ia->k, tipo, u, &t);
	else {
		int tec = ia_tec(ia, tipo);
		if (tec < 0) return;
		tz_mundo_custo_tec(ia->m, ia->k, tec, u, &t);
	}
	for (int r = 0; r < 4; ++r) c[r] = u[r] > 0x7fffffffu ? 0x7fffffff : (int32_t)u[r];
}

/* 0x40b040 */
void ia_reservar(TzIa *ia, int parte, const int32_t c[4], uint32_t n)
{
	if (parte < 0 || parte >= B_N) return;
	for (int r = 0; r < 4; ++r) ia->baldes[parte][r] = (int32_t)((uint32_t)ia->baldes[parte][r] + (uint32_t)c[r] * n);
}

/* 0x40b0f0 y 0x40b130 */
void ia_comprometer(TzIa *ia, const int32_t c[4])
{
	for (int r = 0; r < 4; ++r) {
		ia->disponivel[r] = (int32_t)((uint32_t)ia->disponivel[r] - (uint32_t)c[r]);
		ia->comprometido[r] = (int32_t)((uint32_t)ia->comprometido[r] + (uint32_t)c[r]);
	}
}

void ia_devolver(TzIa *ia, const int32_t c[4])
{
	for (int r = 0; r < 4; ++r) {
		ia->comprometido[r] = (int32_t)((uint32_t)ia->comprometido[r] - (uint32_t)c[r]);
		ia->disponivel[r] = (int32_t)((uint32_t)ia->disponivel[r] + (uint32_t)c[r]);
	}
}

/* 0x40b090: los recursos del reino menos lo comprometido */
void ia_gastavel(const TzIa *ia, int32_t out[4])
{
	for (int r = 0; r < 4; ++r) out[r] = (int32_t)((uint32_t)ia->m->reino[ia->k].recursos[r] - (uint32_t)ia->comprometido[r]);
}

/* 0x40b170: se pueden pagar n tras lo que reservaron las partes anteriores; naval y defensa esperan con
 * NEEDS_LOCKED; el primer castillo o aserradero gasta también lo comprometido al empezar */
int ia_pagavel(const TzIa *ia, const int32_t c[4], int parte, uint32_t n, int primeiro)
{
	if ((parte == B_NAVAL || parte == B_DEFESA) && ia->trancado) return 0;
	if (primeiro) {
		uint32_t f, e;
		ia_registro(ia, ia->k, TZC_LUMBER, &f, &e);
		if (f) primeiro = 0;
		else {
			ia_registro(ia, ia->k, TZC_CASTLE, &f, &e);
			if (f) primeiro = 0;
		}
	}
	int32_t antes[4] = { 0, 0, 0, 0 };
	int partes = parte == B_NENHUMA ? 0 : (parte < B_N ? parte : B_N);
	for (int b = 0; b < partes; ++b)
		for (int r = 0; r < 4; ++r) antes[r] = (int32_t)((uint32_t)antes[r] + (uint32_t)ia->baldes[b][r]);
	for (int r = 0; r < 4; ++r) {
		if (!c[r]) continue;
		int32_t precisa = (int32_t)((uint32_t)c[r] * n + (uint32_t)antes[r]);
		int32_t tem = primeiro ? (int32_t)((uint32_t)ia->comprometido[r] + (uint32_t)ia->disponivel[r]) : ia->disponivel[r];
		if (tem < precisa) return 0;
	}
	return 1;
}

/* 0x40fb00 y 0x40fb60: sin aserradero ni castillo, el precio de un aserradero queda comprometido para él */
void ia_reserva_inicial(TzIa *ia, int liga)
{
	uint32_t f, e;
	ia_registro(ia, ia->k, TZC_LUMBER, &f, &e);
	if (f) return;
	ia_registro(ia, ia->k, TZC_CASTLE, &f, &e);
	if (f) return;
	int32_t c[4];
	ia_custo(ia, TZC_LUMBER, c);
	if (liga) ia_comprometer(ia, c);
	else ia_devolver(ia, c);
}

/* 0x409941: una unidad o investigación pagada en el acto (la simulación la cobra al aplicar el comando) */
void ia_pagar(TzIa *ia, const int32_t c[4])
{
	for (int r = 0; r < 4; ++r) ia->disponivel[r] = (int32_t)((uint32_t)ia->disponivel[r] - (uint32_t)c[r]);
}

/* ------------------------------------------------------------------ metas */
/* 0x40d560: de los edificios terminados y ociosos del tipo que produce, el de lugares con menos defensa; el más
 * nuevo en los empates */
static TzId produtor_ocioso(TzIa *ia, int classe)
{
	TzId melhor = TZ_NENHUM;
	uint32_t ponto = 0;
	for (TzId i = ia->m->nobj - 1; i >= 0; --i) {
		const TzObj *o = &ia->m->obj[i];
		if (o->dono != ia->k || o->classe != classe || !ia_contado(o) || ia_obra(o) || (o->flags & TZ_F_HISTORIA)) continue;
		if (!ia_edificio_ocioso(ia, i)) continue;
		uint32_t s = ia_descoberto(ia, i);
		if (melhor == TZ_NENHUM || ponto < s) { melhor = i; ponto = s; }
	}
	return melhor;
}

/* 0x409550: una meta cumplida; si se hizo */
static int executar(TzIa *ia, int tipo, uint32_t quer)
{
	if (!ia_disponivel_tipo(ia, tipo)) return 0;
	switch (tipo) {
	case TZ_IA_M_RENDER:
		return 0;   /* 0x4096d1: solo si la partida deja rendirse a la IA (TzRegras no lo tiene) */
	case TZ_IA_M_BLOQUEIO:
		memset(ia->baldes[B_NAVAL], 0, sizeof ia->baldes[0]);
		memset(ia->baldes[B_ARMAZENS], 0, sizeof ia->baldes[0]);
		memset(ia->baldes[B_TRABALHADORES], 0, sizeof ia->baldes[0]);
		memset(ia->baldes[B_DEFESA], 0, sizeof ia->baldes[0]);
		ia->trancado = 1;
		return 0;
	case TZ_IA_M_OURO: case TZ_IA_M_PEDRA: case TZ_IA_M_COMIDA: case TZ_IA_M_MADEIRA:
		return 0;
	case TZ_IA_M_CONVERTER:
		return 0;   /* 0x409771: la taberna cambia comida; mundo.h no tiene ese comando */
	case IC_JIHAD: case IC_CRUSADEANCHOR:
		return 0;   /* goals.rs jihad y crusade: pendientes */
	}
	int cat = tz_ia_bib_categoria(ia->b, tipo);
	if (cat == 1 || cat == 3) {
		int prod = ia->produtor ? ia->produtor[tipo] : -1;
		if (prod < 0 || prod == IC_MAGE) return 0;   /* goals.rs summon: pendiente */
		TzId at = produtor_ocioso(ia, prod);
		if (at == TZ_NENHUM) return 0;
		int32_t c[4];
		ia_custo(ia, tipo, c);
		if (cat == 1) { if (ia_produzir(ia, at, tipo)) return 0; }
		else { int tec = ia_tec(ia, tipo); if (tec < 0 || ia_pesquisar(ia, at, tec)) return 0; }
		ia_pagar(ia, c);
		return 1;
	}
	if (cat == 2) {
		int cx = ia->casa[0], cy = ia->casa[1];
		uint32_t mn = 5, mx = 50;
		if (tipo == TZC_LUMBER) { mn = 10; mx = 50; }
		else if (tipo == TZC_STORAGE) {
			/* junto a la primera mina de la lista, si no como un aserradero */
			if (ia->minas.n) {
				const TzObj *o = &ia->m->obj[ia->minas.v[0]];
				cx = o->cx; cy = o->cy; mn = 1; mx = 8;
			} else { mn = 10; mx = 50; }
		} else if (tipo == TZC_FIELD) { mn = 0; mx = 50; }
		return ia_colocar(ia, tipo, cx, cy, mn, mx, TZ_NENHUM);
	}
	return 0;
}

/* 0x408960: la fase de construir. Se leen otra vez los recursos, se vacía la parte del guion y las reglas corren en
 * orden; la primera meta que puede pagar se cumple y un edificio termina la fase. Una regla crítica que se para para
 * la fase, o su grupo. */
void ia_fase_construir(TzIa *ia)
{
	uint32_t cap = ia_limite(ia);
	if (cap != ia->tope) { ia->tope = (uint16_t)cap; ia_tocar(ia, V_PCT, 0); }
	int32_t agora[4];
	ia_gastavel(ia, agora);
	for (int r = 0; r < 4; ++r)
		if (agora[r] != ia->disponivel[r]) ia_tocar(ia, V_RECURSO, r);
	memcpy(ia->disponivel, agora, sizeof agora);
	memset(ia->baldes[B_GUIAO], 0, sizeof ia->baldes[0]);
	ia->trancado = 0;
	ia_tocar(ia, V_SONDA, 0);
	uint32_t grupos[64];
	int ng = 0;
	int n = ia->prog.nregras;
	ia->construindo = 1;
	for (int r = 0; r < n; ++r) {
		TzIaRegra *rg = &ia->prog.regra[r];
		uint32_t fl = rg->flags;
		int para = 0;
		if (fl & TZ_IA_R_GRUPO_INI) { if (ng < 64) grupos[ng++] = rg->fim; }
		else if (fl & TZ_IA_R_GRUPO_FIM) { if (ng) ng--; }
		else if (!(fl & TZ_IA_R_ESQUECIDA)) {
			if (fl & (TZ_IA_R_SUJA | TZ_IA_R_EM_CURSO)) avaliar(ia, r);
			fl = rg->flags;
			if (!(fl & TZ_IA_R_SATISFEITA)) {
				para = 1;
				if ((fl & TZ_IA_R_VERDADE) && !(fl & TZ_IA_R_EM_CURSO)) {
					int tipo = rg->tem_meta ? rg->meta_tipo : 0;
					if (ia_disponivel_tipo(ia, tipo)) {
						uint32_t feitos, e;
						ia_conta(ia, tipo, &feitos, &e);
						uint32_t quer = querido(rg->tem_meta ? rg->meta_n : 0, ia_limite(ia));
						int32_t um[4];
						ia_custo(ia, tipo, um);
						int parte = B_GUIAO, salta;
						if (tipo == TZC_PEON || tipo == TZC_HOUSE) { parte = B_TRABALHADORES; salta = quer == 0 || quer <= feitos; }
						else salta = quer <= feitos;
						if (salta) para = 0;
						else {
							ia_reservar(ia, parte, um, quer - feitos);
							int primeiro = tipo == TZC_CASTLE || tipo == TZC_LUMBER;
							int pg = ia_pagavel(ia, um, parte, 1, primeiro), ex = pg && executar(ia, tipo, quer);
							if (tz_ia_depurar) printf("[DBG]   regla %d meta %s x%u (tiene %u) coste %d/%d/%d/%d pagable %d hecha %d\n", r,
							                           tipo < ia->b->ntipos ? ia->b->tipo[tipo] : "?", quer, feitos, um[0], um[1], um[2], um[3], pg, ex);
							if (ex) {
								para = 0;
								rg->flags |= TZ_IA_R_EM_CURSO;
								if (tz_ia_bib_categoria(ia->b, tipo) == 2) break;
							}
						}
					}
				}
			}
		}
		if (para && (rg->flags & TZ_IA_R_CRITICA)) {
			if (ng && grupos[ng - 1]) { r = (int)grupos[--ng]; continue; }
			break;
		}
	}
	ia->construindo = 0;
}
