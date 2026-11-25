/* SPDX-License-Identifier: GPL-3.0-or-later */
/* guiao.c - el intérprete de los guiones de misión de Tzar: reglas, sucesos, acciones, temporizadores, conversaciones y el final
 * de la partida (D417, fase 7).
 *
 * Traducción a C de ReTzar (https://github.com/ColCarlo/ReTzar, GPL-3.0-or-later): src/gameplay/scripting.rs (0x4b6800-0x4bd700 y
 * 0x442d10, 0x442e30, 0x4431c0 del Tzar.exe), contra mundo.h. Las reglas y los temporizadores son mensajes de una cola con
 * tiempo: cada regla se encola 1 ms tras cargar el mapa, en el orden del fichero, y vuelve a probar su condición cada 1000 ms;
 * un temporizador suma o resta 1000 cada 1000 ms; a igual hora, en el orden en que se encolaron. Una regla que se cumple corre sus
 * acciones en el mismo reparto hasta que una pide esperar; se borra tras la última salvo que una sea «mantener regla».
 *
 * Diferencias con ReTzar, todas por mundo.h: (1) y (2), que eran los recursos compartidos y «explorar» temporal, ya no lo son desde la
 * v8.5.5.2 (tz_mundo_recursos_compartidos, tz_mundo_guiao_revelar_temp); (3) lo que el mundo no ofrece lo escribe el guion en
 * TzObj (ver las funciones gm_*); (4) la IA se maneja por TzGuiaoServicos. */
#include "guiao_i.h"
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define PERIODO   1000u
#define ESPERA    100u

typedef struct { TzMundo *m; TzGuiao *g; } Ctx;

/* ------------------------------------------------------------------ adaptación a lo que el mundo ofrece */
/* El guion corre dentro del tic y cambia el mundo con las acciones tz_mundo_guiao_*. Solo el ataque, la defensa y los
 * máximos de vida y maná de @setproperty se escriben aún en TzObj (mundo.h no tiene acción para ellos). */
static void gm_flags(TzMundo *m, int i, uint32_t pon, uint32_t quita) { tz_mundo_guiao_atributos(m, i, -1, -1, -1, pon, quita); }
static void gm_vida(TzMundo *m, int i, int32_t v)
{
	if (!tz_mundo_obj(m, i)) return;
	if (v <= 0) { tz_mundo_guiao_matar(m, i); return; }
	tz_mundo_guiao_atributos(m, i, v > 65535 ? 65535 : v, -1, -1, 0, 0);
}

#define CLAMP16(v, lo) ((v) < (lo) ? (lo) : (v) > 65535 ? 65535 : (v))

/* ------------------------------------------------------------------ consultas */
static int eh_unidade(const TzObj *o) { return tz_classe_e_unidade(o->classe); }
static int eh_edificio(const TzObj *o) { return tz_classe_e_edificio(o->classe); }

/* alive de gameplay.rs: ni inactivo ni muerto, con vida (los edificios y los objetos sin vida cuentan aunque la tengan a 0) */
static int presente_basico(const TzObj *o)
{
	return !(o->flags & (TZ_F_INATIVO | TZ_F_MORTO)) && (o->vida > 0 || !(eh_unidade(o) || eh_edificio(o)));
}
/* el filtro «existe» (0x4b9970): presente, sin morir (ni la marca 4) y los edificios terminados */
static int existe(const TzMundo *m, int i)
{
	const TzObj *o = &m->obj[i];
	return presente_basico(o) && !(o->flags & (TZ_F_MORRENDO | 4)) && !(eh_edificio(o) && (o->flags & TZ_F_OBRA));
}
static int presente(const TzMundo *m, int i)
{
	const TzObj *o = &m->obj[i];
	return presente_basico(o) && !(o->flags & (TZ_F_MORRENDO | 4));
}

static int grande(int classe)   /* major(): lo que mantiene vivo a un reino (0x442bd0) */
{
	return !(classe == 2 || (classe >= 61 && classe <= 64) || classe == 67 || classe == 68 || classe == 133 || classe == 136);
}

static uint32_t relacao(const TzMundo *m, int a, int b)
{
	if (a == b || a == 0 || b == 0) return 0x47;
	if (b == 9) return 0x40;
	if (b >= 10 || a >= 9) return 0x10;
	uint32_t d = m->reino[a].diplomacia[b];
	if ((d & 0x70) == 0 && m->reino[a].ativo && m->reino[b].ativo) return d | 0x10;
	return d;
}

/* 0x467ef0: junta la relación pedida de a hacia b con lo que b quiere: la guerra es de un lado, la neutralidad y la alianza piden
 * los dos */
static void juntar_relacao(TzMundo *m, int a, int b, uint32_t novo)
{
	uint32_t adiante = novo, volta = relacao(m, b, a);
	if (adiante == volta && adiante == relacao(m, a, b)) return;
	uint32_t quer = adiante | volta;
	if ((quer & 0x10000000u) && !(adiante & 0x10)) {
		adiante = (adiante & ~0x67u) | 0x10; volta = (volta & ~0x67u) | 0x10;
	} else if (((adiante & 0x20000000u) && (volta & 0x20000000u) && !(adiante & 0x20)) || ((adiante & 0x40) && (quer & 0x20000000u))) {
		adiante = (adiante & ~0x57u) | 0x20; volta = (volta & ~0x57u) | 0x20;
	} else if ((adiante & 0x40000000u) && (volta & 0x40000000u) && !(adiante & 0x40)) {
		adiante = (adiante & ~0x36u) | 0x41; volta = (volta & ~0x36u) | 0x41;
	}
	tz_mundo_guiao_diplomacia(m, a, b, adiante);
	tz_mundo_guiao_diplomacia(m, b, a, volta);
}

static int classe_casa(int codigo, int classe)
{
	switch (codigo) {
	case 0xffff: return 1;
	case 0xfffe: return tz_classe_e_unidade(classe);
	case 0xfffd: return classe >= 3 && classe <= 38;
	case 0xfffc: return tz_classe_e_edificio(classe);
	case 0xfffb: return (tz_classe_e_unidade(classe) || tz_classe_e_edificio(classe)) && grande(classe);
	default: return classe == codigo;
	}
}

static int id_a_obj(Ctx *c, int id)
{
	TzId i = tz_mundo_por_id_mapa(c->m, id);
	return i >= 0 && i < c->m->nobj ? (int)i : -1;
}

static int oculto(const Ctx *c, int i) { return i >= 0 && i < c->g->nobj_oculto && c->g->oculto[i]; }

static void pegada_obj(const Ctx *c, const TzObj *o, int *w, int *h)
{
	const TzAtributos *at = c->m->cat ? tz_catalogo_at(c->m->cat, o->classe, o->civ) : NULL;
	*w = at && at->pegada[0] ? at->pegada[0] : 1;
	*h = at && at->pegada[1] ? at->pegada[1] : 1;
}

/* ------------------------------------------------------------------ pedidos a la interfaz */
static TzPedido *pedir(TzGuiao *g, int tipo)
{
	if (g->fila_n >= 256) { g->fila_ini = (g->fila_ini + 1) & 255; g->fila_n--; }   /* la cola llena pierde el más viejo */
	TzPedido *p = &g->fila[(g->fila_ini + g->fila_n) & 255];
	g->fila_n++;
	memset(p, 0, sizeof *p);
	p->tipo = tipo;
	p->obj = TZ_NENHUM;
	return p;
}

static void copiar(char *dst, size_t n, const char *src) { snprintf(dst, n, "%s", src ? src : ""); }

int tz_guiao_pedido(TzGuiao *g, TzPedido *out)
{
	if (!g->fila_n) return 0;
	*out = g->fila[g->fila_ini];
	g->fila_ini = (g->fila_ini + 1) & 255;
	g->fila_n--;
	return 1;
}
int tz_guiao_modal(const TzGuiao *g) { return g->modal; }
const TzMensagem *tz_guiao_mensagem(const TzGuiao *g) { return &g->msg; }
const char *tz_guiao_proximo_mapa(const TzGuiao *g) { return g->proximo_mapa; }
int tz_guiao_acabou(const TzGuiao *g, int *ganhou) { if (ganhou) *ganhou = g->ganhou_fim; return g->acabou; }
void tz_guiao_servicos(TzGuiao *g, const TzGuiaoServicos *s) { g->serv = *s; }

/* ------------------------------------------------------------------ evaluador: lo que el guion le deja a las expresiones */
static void ev_evento_drenar(Ctx *c);

static uint64_t agora(const TzMundo *m) { return (uint64_t)m->tic * TZ_TIC_MS; }

static int32_t recurso(Ctx *c, int p, int r)
{
	if (p < 1 || p > 8 || !c->m->reino[p].ativo) return 0;
	/* lo que puede gastar: lo propio y lo de los que comparten con él (ReTzar stock) */
	int32_t t[TZ_NREC];
	tz_mundo_recursos_compartidos(c->m, p, t);
	return t[r] < 0 ? 0 : t[r];
}
static int32_t definir_recurso(Ctx *c, int p, int r, int32_t v)
{
	if (p < 1 || p > 8 || !c->m->reino[p].ativo) return 0;
	/* escribe solo lo propio, tal cual (set_own, 0x4678d0) */
	int32_t atual = c->m->reino[p].recursos[r];
	tz_mundo_guiao_recursos(c->m, p, r, (v < 0 ? 0 : v) - atual);
	return recurso(c, p, r);
}

static uint32_t palavra(Ctx *c, int a, int b)
{
	if (a < 1 || a > 8 || b < 1 || b > 8) return 0;
	return relacao(c->m, a, b);
}

/* @count y @inprogress: cuántos objetos de la clase tiene el reino (255 = naturaleza) y cuántos están en marcha (obras y
 * entrenamientos). El original lo lee del registro de la IA. */
static int32_t contar(Ctx *c, int reino, int classe, int em_marcha)
{
	int k = reino == 255 ? 0 : reino;
	if (k < 0 || k >= 12) return 0;
	int32_t n = 0;
	for (int i = 0; i < c->m->nobj; ++i) {
		const TzObj *o = &c->m->obj[i];
		if (o->dono != k || (o->flags & (TZ_F_MORTO | TZ_F_INATIVO))) continue;
		if (em_marcha) {
			if (o->classe == classe && (o->flags & TZ_F_OBRA)) n++;
			if (o->ator && (o->flags & TZ_F_OBRA) == 0)
				for (int j = 0; j < o->ator->ntrab; ++j)
					if (o->ator->trab[j].tipo == TZ_J_TREINAR && o->ator->trab[j].arg == classe) n++;
		} else if (o->classe == classe) n++;
	}
	return n;
}

static int32_t ex_chamar(void *vc, int fn, const int32_t a[8], int obj)
{
	Ctx *c = vc;
	TzMundo *m = c->m;
	TzGuiao *g = c->g;
	switch (fn) {
	case TZ_FN_GOLD: return recurso(c, a[0], TZ_OURO);
	case TZ_FN_STONE: return recurso(c, a[0], TZ_PEDRA);
	case TZ_FN_FOOD: return recurso(c, a[0], TZ_COMIDA);
	case TZ_FN_WOOD: return recurso(c, a[0], TZ_MADEIRA);
	case TZ_FN_SET_GOLD: return definir_recurso(c, a[0], TZ_OURO, a[1]);
	case TZ_FN_SET_STONE: return definir_recurso(c, a[0], TZ_PEDRA, a[1]);
	case TZ_FN_SET_FOOD: return definir_recurso(c, a[0], TZ_COMIDA, a[1]);
	case TZ_FN_SET_WOOD: return definir_recurso(c, a[0], TZ_MADEIRA, a[1]);
	case TZ_FN_RND: {
		int64_t span = (int64_t)a[1] - a[0] + 1;
		if (span < 1) span = 1;
		if (span == 1) return a[0];
		return (int32_t)(a[0] + (int64_t)tz_rng_entre(&m->rng, 0, (int32_t)(span > 0x7fffffff ? 0x7fffffff : span) - 1));
	}
	case TZ_FN_STR: snprintf(g->texto_str, sizeof g->texto_str, "%d", (int)a[0]); return 1;
	case TZ_FN_MIN: return a[0] < a[1] ? a[0] : a[1];
	case TZ_FN_MAX: return a[0] > a[1] ? a[0] : a[1];
	case TZ_FN_GET_DIP: return (int32_t)palavra(c, a[0], a[1]);
	case TZ_FN_IS_ALLY: return (palavra(c, a[0], a[1]) & 0x40) != 0;
	case TZ_FN_IS_NEUTRAL: return (palavra(c, a[0], a[1]) & 0x20) != 0;
	case TZ_FN_IS_ENEMY: return (palavra(c, a[0], a[1]) & 0x10) != 0;
	case TZ_FN_SHARED_VISION: return (palavra(c, a[0], a[1]) & 1) != 0;
	case TZ_FN_SHARED_RES: return (palavra(c, a[0], a[1]) & 2) != 0;
	case TZ_FN_SHARED_UNITS: return (palavra(c, a[0], a[1]) & 4) != 0;
	case TZ_FN_COUNT: return contar(c, a[0], a[1], 0);
	case TZ_FN_IN_PROGRESS: return contar(c, a[0], a[1], 1);
	case TZ_FN_SET_POP:
		if (a[0] >= 1 && a[0] <= 8) m->reino[a[0]].populacao_limite = (uint16_t)(a[1] < 0 ? 0 : a[1] > 65535 ? 65535 : a[1]);
		return a[1] < 0 ? 0 : a[1];
	case TZ_FN_SET_AI: {
		int k = a[0];
		int existe_k = k == 0 || (k >= 9 && k <= 11) || (k >= 1 && k <= 8 && m->reino[k].ativo);
		if (!existe_k || a[1] < 0 || a[1] > 3) return 0;
		if (g->serv.ia_modo) g->serv.ia_modo(m, g->serv.dados, k, a[1]);
		TzPedido *p = pedir(g, TZ_PD_IA);
		p->cx = k; p->cy = a[1];
		return 1;
	}
	case TZ_FN_NO_ATTACK:
		if (g->serv.ia_sem_ataque) g->serv.ia_sem_ataque(m, g->serv.dados, (uint32_t)a[0], (uint32_t)a[1]);
		return 1;
	case TZ_FN_PODE_RESGATAR: {
		int k = a[0];
		int existe_k = k == 0 || (k >= 9 && k <= 11) || (k >= 1 && k <= 8 && m->reino[k].ativo);
		if (!existe_k) return 0;
		if (g->serv.pode_resgatar) g->serv.pode_resgatar(m, g->serv.dados, k, a[1] != 0);
		tz_mundo_pode_resgatar(m, k, a[1] != 0);
		return 1;
	}
	case TZ_FN_SET_TIME: g->hora = ((unsigned)a[0] & 0xff) % 24; return 1;
	case TZ_FN_GET_TIME: return g->hora;
	case TZ_FN_SELECT: case TZ_FN_UNSELECT:
		if (obj >= 0) { TzPedido *p = pedir(g, fn == TZ_FN_SELECT ? TZ_PD_SELECIONAR : TZ_PD_DESSELECIONAR); p->obj = obj; }
		return 0;
	case TZ_FN_CLEAR_SEL: pedir(g, TZ_PD_LIMPAR_SELECAO); return 0;
	case TZ_FN_GOD:
		if (obj < 0) return 0;
		if (a[0]) gm_flags(m, obj, TZ_F_DEUS, 0); else gm_flags(m, obj, 0, TZ_F_DEUS);
		return 1;
	case TZ_FN_BUILD:
		/* 0x4b83a0: la obra recibe 999999 de construcción y termina como cualquiera */
		if (obj >= 0) tz_mundo_guiao_terminar_obra(m, obj);
		return 0;
	}
	return 0;
}

static void ex_filme(void *vc, const char *nome)
{
	Ctx *c = vc;
	TzPedido *p = pedir(c->g, TZ_PD_FILME);
	copiar(p->texto, sizeof p->texto, nome);
}

static int32_t ex_propriedade(void *vc, int i, int prop)
{
	Ctx *c = vc;
	TzMundo *m = c->m;
	const TzObj *o = &m->obj[i];
	switch (prop) {
	case TZ_PR_HEALTH: return o->vida;
	case TZ_PR_MAXHEALTH: return o->vida_max;
	case TZ_PR_ATTACK: return o->ataque;
	case TZ_PR_DEFENCE: return o->defesa;
	case TZ_PR_LEVEL: return o->nivel;
	case TZ_PR_MANNA: return o->mana;
	case TZ_PR_MAXMANNA: return o->mana_max;
	case TZ_PR_KINGDOM: return o->dono;
	case TZ_PR_RACE: return o->civ;
	case TZ_PR_X: return o->cx;
	case TZ_PR_Y: return o->cy;
	case TZ_PR_CARGO: return o->ndentro;
	case TZ_PR_ISSELECTED: return c->g->serv.selecionado ? c->g->serv.selecionado(m, c->g->serv.dados, i) != 0 : 0;
	case TZ_PR_INHOLDER: return o->dentro_de >= 0 && o->dentro_de < m->nobj ? m->obj[o->dentro_de].id_mapa : 0;
	case TZ_PR_PROGRAM: return o->ator ? o->ator->ordem.tipo : 0;
	case TZ_PR_FLAGS: return (int32_t)o->flags;
	case TZ_PR_UNDERFOG: return tz_mundo_nevoa(m, m->jogador, o->cx, o->cy) != 2;
	}
	return 0;
}

static void ex_def_propriedade(void *vc, int i, int prop, int32_t v)
{
	Ctx *c = vc;
	TzMundo *m = c->m;
	TzObj *o = &m->obj[i];
	switch (prop) {
	case TZ_PR_HEALTH: gm_vida(m, i, v); break;
	case TZ_PR_MAXHEALTH: o->vida_max = CLAMP16(v, 1); break;
	case TZ_PR_ATTACK: o->ataque = v < 1 ? 1 : CLAMP16(v, 1); break;
	case TZ_PR_DEFENCE: o->defesa = v < 0 ? 0 : CLAMP16(v, 0); break;
	case TZ_PR_LEVEL: tz_mundo_guiao_atributos(m, i, -1, v < 0 ? 0 : v > 255 ? 255 : v, -1, 0, 0); break;
	case TZ_PR_MANNA: tz_mundo_guiao_atributos(m, i, -1, -1, CLAMP16(v, 0), 0, 0); break;
	case TZ_PR_MAXMANNA: o->mana_max = CLAMP16(v, 0); break;
	}
}

static int32_t ex_global(void *vc, int gl)
{
	Ctx *c = vc;
	int k = c->m->jogador;
	switch (gl) {
	case TZ_GL_GOLD: return recurso(c, k, TZ_OURO);
	case TZ_GL_STONE: return recurso(c, k, TZ_PEDRA);
	case TZ_GL_FOOD: return recurso(c, k, TZ_COMIDA);
	case TZ_GL_WOOD: return recurso(c, k, TZ_MADEIRA);
	case TZ_GL_DIFFICULTY: return c->m->regras.dificuldade;
	}
	return 0;
}
static void ex_def_global(void *vc, int gl, int32_t v)
{
	Ctx *c = vc;
	int k = c->m->jogador;
	switch (gl) {
	case TZ_GL_GOLD: definir_recurso(c, k, TZ_OURO, v); break;
	case TZ_GL_STONE: definir_recurso(c, k, TZ_PEDRA, v); break;
	case TZ_GL_FOOD: definir_recurso(c, k, TZ_COMIDA, v); break;
	case TZ_GL_WOOD: definir_recurso(c, k, TZ_MADEIRA, v); break;
	case TZ_GL_CAN_DELETE: c->g->sem_apagar = v == 0; break;
	case TZ_GL_CAN_DIPLOMACY: c->g->sem_diplomacia = v == 0; break;
	}
}

static int avaliar_suceso(Ctx *c, const TzSuceso *e);

static int ex_evento(void *vc, const char *nome, int *valor)
{
	Ctx *c = vc;
	for (int i = 0; i < c->g->neventos; ++i) {
		if (strcmp(c->g->eventos[i].nome, nome)) continue;
		if (c->g->profundidade > 8) { *valor = 0; return 1; }
		c->g->profundidade++;
		*valor = avaliar_suceso(c, &c->g->eventos[i]);
		c->g->profundidade--;
		return 1;
	}
	return 0;
}

static TzVariaveis *ex_vars(void *vc) { return &((Ctx *)vc)->g->vars; }

static const TzExprOps OPS = { ex_chamar, ex_filme, ex_propriedade, ex_def_propriedade, ex_global, ex_def_global, ex_evento, ex_vars };

static int32_t eval(Ctx *c, const char *texto, int obj) { return tz_expr_eval(&OPS, c, texto, obj); }

int32_t tz_guiao_expr(TzMundo *m, TzGuiao *g, const char *texto, int obj)
{
	Ctx c = { m, g };
	return eval(&c, texto, obj);
}

/* ------------------------------------------------------------------ selectores, áreas y sucesos */
typedef struct { int *v; int n, cap; } Lista;
static void lista_add(Lista *l, int i)
{
	if (l->n == l->cap) {
		int nc = l->cap ? l->cap * 2 : 16;
		int *nv = realloc(l->v, (size_t)nc * sizeof *nv);
		if (!nv) return;
		l->v = nv; l->cap = nc;
	}
	l->v[l->n++] = i;
}

enum { F_EXISTE, F_DESTRUIDO, F_DENTRO, F_AREA, F_EXPR, F_PRESENTE };
typedef struct { int modo; const TzSuporte *sup; int x1, y1, x2, y2; const char *expr; } Filtro;

static int dentro_casa(Ctx *c, const TzSuporte *h, int i)
{
	int j = c->m->obj[i].dentro_de;
	if (j < 0 || j >= c->m->nobj) return 0;
	if (h->por_classe) {
		const TzObj *o = &c->m->obj[j];
		return classe_casa(h->classe, o->classe) && o->dono >= 1 && o->dono <= 8 && (h->reinos & (1u << (o->dono - 1)));
	}
	return id_a_obj(c, h->objeto) == j;
}

static int passa(Ctx *c, const Filtro *f, int i)
{
	switch (f->modo) {
	case F_EXISTE: return i >= 0 && existe(c->m, i);
	case F_PRESENTE: return i >= 0 && presente(c->m, i);
	case F_DESTRUIDO: return i < 0 || !existe(c->m, i);
	case F_DENTRO: return i >= 0 && existe(c->m, i) && dentro_casa(c, f->sup, i);
	case F_AREA: {
		if (i < 0 || !existe(c->m, i)) return 0;
		const TzObj *o = &c->m->obj[i];
		return o->cx >= f->x1 && o->cx <= f->x2 && o->cy >= f->y1 && o->cy <= f->y2;
	}
	case F_EXPR: return i >= 0 && existe(c->m, i) && eval(c, f->expr, i) != 0;
	}
	return 0;
}

/* 0x4b9670: si el selector se cumple y qué objetos casaron (out puede ser NULL) */
static int selecionar(Ctx *c, const TzSel *s, const Filtro *f, Lista *out)
{
	TzMundo *m = c->m;
	int nm = 0;
	if (s->por_classe) {
		for (int i = 0; i < m->nobj; ++i) {
			const TzObj *o = &m->obj[i];
			if (!(o->dono >= 1 && o->dono <= 8 && (s->reinos & (1u << (o->dono - 1))) && classe_casa(s->classe, o->classe))) continue;
			if (passa(c, f, i)) { nm++; if (out) lista_add(out, i); }
		}
		int32_t n = eval(c, s->conta ? s->conta : "", -1);
		return s->no_maximo ? nm <= n : nm >= n;
	}
	int passados = 0;
	for (int k = 0; k < s->nids; ++k) {
		int i = id_a_obj(c, s->ids[k]);
		if (i >= 0 && oculto(c, i)) i = -1;
		if (passa(c, f, i)) { passados++; if (i >= 0 && out) lista_add(out, i); }
	}
	return s->todos ? passados == s->nids : passados > 0;
}

static int sel_existente(Ctx *c, const TzSel *s, Lista *out)
{
	Filtro f = { F_EXISTE };
	return selecionar(c, s, &f, out);
}

/* las casillas de un área, incluidas; 0 si el objeto ya no está */
static int area_rect(Ctx *c, const TzArea *a, int r[4])
{
	if (!a->em_volta) {
		int x1 = a->rect[0], y1 = a->rect[1], x2 = a->rect[2], y2 = a->rect[3];
		r[0] = x1 < x2 ? x1 : x2; r[1] = y1 < y2 ? y1 : y2; r[2] = x1 > x2 ? x1 : x2; r[3] = y1 > y2 ? y1 : y2;
		return 1;
	}
	int i = id_a_obj(c, a->objeto);
	if (i < 0) return 0;
	const TzObj *o = &c->m->obj[i];
	int w, h;
	pegada_obj(c, o, &w, &h);
	int d = eval(c, a->raio ? a->raio : "", -1);
	r[0] = o->cx - d; r[1] = o->cy - d; r[2] = o->cx + w - 1 + d; r[3] = o->cy + h - 1 + d;
	return 1;
}

static int clamp_lado(const Ctx *c, int v) { return v < 0 ? 0 : v > c->m->lado - 1 ? c->m->lado - 1 : v; }

static int centro_area(Ctx *c, const TzArea *a, int out[2])
{
	int r[4];
	if (!area_rect(c, a, r)) return 0;
	out[0] = clamp_lado(c, (r[0] + r[2]) / 2);
	out[1] = clamp_lado(c, (r[1] + r[3]) / 2);
	return 1;
}

/* una esquina de ScrollView (0x4bca60): la casilla del objeto del área o la mitad de su rectángulo */
static int celda_vista(Ctx *c, const TzArea *a, int out[2])
{
	if (a->objeto) {
		int i = id_a_obj(c, a->objeto);
		if (i < 0) return 0;
		out[0] = c->m->obj[i].cx; out[1] = c->m->obj[i].cy;
		return 1;
	}
	int x1 = a->rect[0], y1 = a->rect[1], x2 = a->rect[2], y2 = a->rect[3];
	out[0] = clamp_lado(c, x1 + (x2 - x1 + 1) / 2);
	out[1] = clamp_lado(c, y1 + (y2 - y1 + 1) / 2);
	return 1;
}

static int avaliar_suceso(Ctx *c, const TzSuceso *e)
{
	TzGuiao *g = c->g;
	switch (e->tipo) {
	case TZ_EVT_EXISTE: return sel_existente(c, &e->sel, NULL);
	case TZ_EVT_DESTRUIDO:
		if (e->sel.por_classe) {
			int64_t perdidos = 0;
			for (int p = 1; p <= 11; ++p) {
				if (!(e->sel.reinos & (1u << (p - 1)))) continue;
				for (int k = 0; k < 256; ++k)
					if (classe_casa(e->sel.classe, k)) perdidos += g->perdidos[p][k];
			}
			int32_t n = eval(c, e->sel.conta ? e->sel.conta : "", -1);
			return e->sel.no_maximo ? perdidos <= n : perdidos >= n;
		} else {
			Filtro f = { F_DESTRUIDO };
			return selecionar(c, &e->sel, &f, NULL);
		}
	case TZ_EVT_DENTRO: {
		Filtro f = { F_DENTRO, &e->sup };
		return selecionar(c, &e->sel, &f, NULL);
	}
	case TZ_EVT_SAIU: return 0;
	case TZ_EVT_NA_AREA: {
		int r[4];
		if (!area_rect(c, &e->area, r)) return 0;
		Filtro f = { F_AREA, NULL, r[0], r[1], r[2], r[3] };
		return selecionar(c, &e->sel, &f, NULL);
	}
	case TZ_EVT_DERROTADO:
		for (int p = 1; p <= 8; ++p) if ((e->mascara & (1u << (p - 1))) && c->m->reino[p].derrotado) return 1;
		return 0;
	case TZ_EVT_VENCEU:
		for (int p = 1; p <= 8; ++p) if ((e->mascara & (1u << (p - 1))) && c->m->reino[p].venceu) return 1;
		return 0;
	case TZ_EVT_EXPRESSAO: {
		Filtro f = { F_EXPR, NULL, 0, 0, 0, 0, e->expr ? e->expr : "" };
		return selecionar(c, &e->sel, &f, NULL);
	}
	}
	return 0;
}

/* ------------------------------------------------------------------ eventos del mundo: lo que murió cuenta para «destruido» */
static void ev_evento_drenar(Ctx *c)
{
	TzEvento ev[64];
	int n;
	while ((n = tz_mundo_eventos(c->m, &c->g->cursor_ev, ev, 64)) > 0) {
		for (int i = 0; i < n; ++i) {
			switch (ev[i].tipo) {
			case TZ_EV_MORREU: {
				const TzObj *o = tz_mundo_obj(c->m, ev[i].obj);
				c->g->fim_devido = 1;
				if (o && !(o->flags & TZ_F_INATIVO) && o->dono >= 1 && o->dono <= 11 && o->classe < 256) c->g->perdidos[o->dono][o->classe]++;
				break;
			}
			case TZ_EV_CAPTURADO: case TZ_EV_DIPLOMACIA: case TZ_EV_DERROTADO: c->g->fim_devido = 1; break;
			default: break;
			}
		}
		if (n < 64) break;
	}
}

/* ------------------------------------------------------------------ el final de la partida (0x4431c0, 0x442d10, 0x442e30) */
static void acabar(Ctx *c)
{
	TzGuiao *g = c->g;
	TzMundo *m = c->m;
	if (g->acabou) return;
	int local = m->jogador;
	const TzReinoEstado *k = &m->reino[local];
	int ganhou = !(k->derrotado || k->perdeu);
	m->reino[local].venceu = (uint8_t)ganhou;
	g->acabou = 1; g->ganhou_fim = ganhou; g->tic_fim = m->tic;
	tz_mundo_guiao_fim(m, local, ganhou);
}

static int espias(const TzMundo *m, int p)
{
	int n = 0;
	for (int i = 0; i < m->nobj; ++i) {
		const TzObj *o = &m->obj[i];
		if (o->dono == p && o->classe == TZC_SPY && presente_basico(o) && !(o->flags & TZ_F_MORRENDO)) n++;
	}
	return n;
}

static int64_t pontos_mostrados(const TzMundo *m, int p, int bonus_reino, int64_t bonus)
{
	int64_t cru = m->reino[p].pontos + (p == bonus_reino ? bonus : 0);
	return (cru + 50) / 100;
}

static void teste_derrota(Ctx *c, int p, int tipo, int64_t limite, int bonus_reino, int64_t bonus)
{
	TzMundo *m = c->m;
	TzReinoEstado *k = &m->reino[p];
	if (k->nao_pode_perder) return;
	if (k->perdeu || k->rendido) { k->derrotado = 1; return; }
	if (espias(m, p) > 0) return;
	int resta = 0;
	for (int i = 0; i < m->nobj && !resta; ++i) {
		const TzObj *o = &m->obj[i];
		if (o->dono == p && presente_basico(o) && !(o->flags & TZ_F_MORRENDO) && (eh_unidade(o) || eh_edificio(o)) && grande(o->classe)) resta = 1;
	}
	if (!resta) k->derrotado = 1;
	if (tipo == 1) {
		int castelos = 0;
		for (int i = 0; i < m->nobj && !castelos; ++i) {
			const TzObj *o = &m->obj[i];
			if (o->dono == p && o->classe == TZC_CASTLE && presente_basico(o)) castelos = 1;
		}
		if (!castelos) k->derrotado = 1;
	} else if (tipo == 3) {
		for (int q = 1; q <= 8; ++q)
			if (q != p && m->reino[q].ativo && pontos_mostrados(m, q, bonus_reino, bonus) > limite) { k->derrotado = 1; return; }
	}
}

static int vitoria_partilhada(const TzMundo *m, int p) { return (m->reino[p].diplomacia[p] & TZ_DIP_VITORIA_PARTILHADA) != 0; }

static void verificar_fim(Ctx *c, int com_regras, int tipo, int64_t limite, int bonus_reino, int64_t bonus)
{
	TzMundo *m = c->m;
	TzGuiao *g = c->g;
	if (!com_regras) {
		tipo = m->regras.vitoria;
		limite = m->regras.vitoria_limite;
		if (tipo == 2 && agora(m) > (uint64_t)limite * 60000u) {
			/* la mayor puntuación cruda gana 1 000 000 y las reglas pasan a un límite de puntos de esa puntuación - 1 */
			int mejor = 0; int64_t pm = -1;
			for (int p = 1; p <= 8; ++p) if ((int64_t)m->reino[p].pontos > pm) { pm = m->reino[p].pontos; mejor = p; }
			verificar_fim(c, 1, 3, pm - 1, mejor, 1000000);
			return;
		}
	}
	int local = m->jogador;
	for (int p = 1; p <= 8; ++p) {
		if (!m->reino[p].ativo || m->reino[p].derrotado) continue;
		teste_derrota(c, p, tipo, limite, bonus_reino, bonus);
		if (!m->reino[p].derrotado) continue;
		if (g->derrota[p - 1]) tz_mundo_guiao_mensagem(m, local, g->derrota[p - 1]);
		else if (!g->campanha) { TzPedido *pd = pedir(g, TZ_PD_NOTA); copiar(pd->chave, sizeof pd->chave, "player_defeated"); copiar(pd->texto, sizeof pd->texto, m->reino[p].nome); }
	}
	unsigned perdedores = 0;
	for (int p = 1; p <= 8; ++p) {
		if (!m->reino[p].ativo) continue;
		const TzReinoEstado *k = &m->reino[p];
		if (k->derrotado || k->nao_pode_ganhar) { perdedores |= 1u << p; continue; }
		for (int q = 1; q <= 8; ++q)
			if (q != p && m->reino[q].ativo && !m->reino[q].derrotado &&
			    !((relacao(m, p, q) & 0x40) && vitoria_partilhada(m, p) && vitoria_partilhada(m, q)))
				perdedores |= 1u << p;
	}
	int ganhador = 0;
	for (int p = 1; p <= 8; ++p) if (m->reino[p].ativo && !(perdedores & (1u << p))) ganhador = 1;
	if (m->reino[local].derrotado || ganhador) acabar(c);
}

/* ------------------------------------------------------------------ acciones */
static int32_t id_missao(const char *nome)
{
	int32_t h = 0;
	for (const unsigned char *p = (const unsigned char *)nome; *p; ++p) h = (int32_t)((uint32_t)h * 5u + (uint32_t)(int32_t)(signed char)*p);
	return h;
}

static int janela(Ctx *c, int idx, const TzAccion *a)
{
	TzGuiao *g = c->g;
	if (g->modal) return ESPERA;
	if (g->esperando[idx]) { g->esperando[idx] = 0; return 0; }
	TzMensagem *ms = &g->msg;
	memset(ms, 0, sizeof *ms);
	if (a->tipo == TZ_ACC_HISTORIA) {
		int32_t t = eval(c, a->t[4] ? a->t[4] : "", -1);
		copiar(ms->texto, sizeof ms->texto, a->t[0]);
		copiar(ms->retrato, sizeof ms->retrato, a->t[1]);
		TzPedido *p = pedir(g, TZ_PD_HISTORIA);
		copiar(p->som, sizeof p->som, a->t[2]);
		copiar(p->musica, sizeof p->musica, a->t[3]);
		p->ms = t < 0 ? 0 : (uint32_t)t;
		p->fecha = a->b[0];
	} else {
		copiar(ms->titulo, sizeof ms->titulo, a->t[2]);
		copiar(ms->texto, sizeof ms->texto, a->t[0]);
		copiar(ms->retrato, sizeof ms->retrato, a->t[1]);
		pedir(g, TZ_PD_AVISO_JANELA);
	}
	g->modal = 1;
	g->esperando[idx] = 1;
	return ESPERA;
}

static void ordem(Ctx *c, const TzSel *s, const TzArea *a, int tipo)
{
	TzMundo *m = c->m;
	Lista l = { 0 };
	sel_existente(c, s, &l);
	int alvo = a->em_volta ? id_a_obj(c, a->objeto) : -1;
	int centro[2] = { 0, 0 }, canto[2] = { 0, 0 };
	if (alvo >= 0) {
		const TzObj *o = &m->obj[alvo];
		int w, h;
		pegada_obj(c, o, &w, &h);
		centro[0] = o->cx + w / 2 < m->lado - 1 ? o->cx + w / 2 : m->lado - 1;
		centro[1] = o->cy + h / 2 < m->lado - 1 ? o->cy + h / 2 : m->lado - 1;
		canto[0] = o->cx; canto[1] = o->cy;
	} else if (!a->em_volta) {
		if (!centro_area(c, a, centro)) { free(l.v); return; }
		canto[0] = centro[0]; canto[1] = centro[1];
	}
	/* orden del guion (script.rs order): 0 por defecto, 1 mover, 2 atacar, 3 capturar, 5 descargar, 6 recoger, 7 patrullar, 8 avanzar, 9 guardar */
	int tipo_o = TZ_O_NADA, usar_alvo = 0, cx = canto[0], cy = canto[1], padrao = 0;
	switch (tipo) {
	case 0: padrao = 1; cx = centro[0]; cy = centro[1]; usar_alvo = alvo >= 0; break;
	case 1: tipo_o = TZ_O_MOVER; usar_alvo = 0; break;
	case 2: if (alvo >= 0) { tipo_o = TZ_O_ATACAR; usar_alvo = 1; } else { tipo_o = TZ_O_ATACAR_CHAO; cx = centro[0]; cy = centro[1]; } break;
	case 3: if (alvo < 0) { free(l.v); return; } tipo_o = TZ_O_CAPTURAR; usar_alvo = 1; break;
	case 5: tipo_o = TZ_O_DESCARREGAR; cx = centro[0]; cy = centro[1]; break;
	case 6: if (alvo < 0) { free(l.v); return; } tipo_o = TZ_O_PEGAR_ITEM; usar_alvo = 1; break;
	case 7: tipo_o = TZ_O_PATRULHAR; break;
	case 8: tipo_o = TZ_O_EXPLORAR; break;
	case 9: if (alvo < 0) { free(l.v); return; } tipo_o = TZ_O_GUARDAR; usar_alvo = 1; break;
	default: free(l.v); return;   /* 4 y 10-12 no hacen nada */
	}
	for (int k = 0; k < l.n; ++k) {
		int i = l.v[k];
		TzOrdem o;
		memset(&o, 0, sizeof o);
		o.alvo = TZ_NENHUM;
		o.x = (int16_t)cx; o.y = (int16_t)cy;
		if (padrao) {
			TzId ids = i;
			o = tz_mundo_ordem_padrao(m, m->obj[i].dono, &ids, 1, cx, cy, usar_alvo ? alvo : TZ_NENHUM);
		} else {
			o.tipo = (uint8_t)tipo_o;
			if (usar_alvo) o.alvo = alvo;
			if (tipo_o == TZ_O_PATRULHAR) { o.x2 = (int16_t)m->obj[i].cx; o.y2 = (int16_t)m->obj[i].cy; }
		}
		tz_mundo_guiao_ordem(m, i, &o);
	}
	free(l.v);
}

static void converter(Ctx *c, int at);
static void oferecer(Ctx *c, const int *cand, int n);

static void aparecer(Ctx *c, int i)
{
	TzGuiao *g = c->g;
	if (i < 0 || !oculto(c, i)) return;
	g->oculto[i] = 0;
	/* el mundo lo coloca (casilla libre más cercana si la suya está ocupada) */
	tz_mundo_guiao_aparecer(c->m, i);
}

/* devuelve los ms que la regla espera, o 0 */
static uint64_t executar(Ctx *c, int idx, uint64_t tempo)
{
	TzGuiao *g = c->g;
	TzMundo *m = c->m;
	const TzAccion *a = &g->acoes[idx];
	g->n_acoes++;
	switch (a->tipo) {
	case TZ_ACC_MISSAO: {
		if (g->nmissoes == g->capmissoes) {
			int nc = g->capmissoes ? g->capmissoes * 2 : 8;
			TzObjetivo *nv = realloc(g->missoes, (size_t)nc * sizeof *nv);
			if (!nv) break;
			g->missoes = nv; g->capmissoes = nc;
		}
		TzObjetivo *q = &g->missoes[g->nmissoes++];
		copiar(q->texto, sizeof q->texto, a->t[0]);
		copiar(q->retrato, sizeof q->retrato, a->t[1]);
		q->id = id_missao(a->nome);
		TzPedido *p = pedir(g, TZ_PD_NOTA);
		copiar(p->chave, sizeof p->chave, "newquest");
		break;
	}
	case TZ_ACC_TIRAR_MISSAO: {
		int32_t id = id_missao(a->t[0]);
		for (int i = 0; i < g->nmissoes; ++i)
			if (g->missoes[i].id == id) { memmove(&g->missoes[i], &g->missoes[i + 1], (size_t)(g->nmissoes - i - 1) * sizeof *g->missoes); g->nmissoes--; break; }
		break;
	}
	case TZ_ACC_DIPLOMACIA: {
		int de = a->b[0], para = a->b[1];
		if (de >= 1 && de <= 8 && para >= 1 && para <= 8 && de != para) {
			uint32_t novo = a->flags | (relacao(m, de, para) & 0x70);
			juntar_relacao(m, de, para, novo);
			g->fim_devido = 1;
			pedir(g, TZ_PD_DIPLOMACIA);
			if (para == m->jogador) {
				uint32_t r = relacao(m, de, para);
				const char *chave = (r & 0x10) ? "player_enemy" : (r & 0x20) ? "player_neutral" : (r & 0x40) ? "player_ally" : NULL;
				if (chave) { TzPedido *p = pedir(g, TZ_PD_NOTA); copiar(p->chave, sizeof p->chave, chave); copiar(p->texto, sizeof p->texto, m->reino[de].nome); }
			}
		}
		break;
	}
	case TZ_ACC_DONO: {
		Lista l = { 0 };
		if (sel_existente(c, &a->sel, &l)) for (int k = 0; k < l.n; ++k) tz_mundo_guiao_dono(m, l.v[k], a->b[0]);
		free(l.v);
		break;
	}
	case TZ_ACC_PROPRIEDADE: {
		Lista l = { 0 };
		Filtro f = { F_PRESENTE };
		selecionar(c, &a->sel, &f, &l);
		for (int k = 0; k < l.n; ++k) eval(c, a->t[0] ? a->t[0] : "", l.v[k]);
		free(l.v);
		ev_evento_drenar(c);
		break;
	}
	case TZ_ACC_EXPRESSAO: {
		char buf[2100];
		tz_expr_sentencias(a->t[0] ? a->t[0] : "", buf, sizeof buf);
		eval(c, buf, -1);
		break;
	}
	case TZ_ACC_TEMPORIZADOR: {
		int32_t v = eval(c, a->t[0] ? a->t[0] : "", -1);
		tz_vars_set(&g->vars, a->nome, v);
		if (g->ntemps == g->captemps) {
			int nc = g->captemps ? g->captemps * 2 : 8;
			TzTempRun *nv = realloc(g->temps, (size_t)nc * sizeof *nv);
			if (!nv) break;
			g->temps = nv; g->captemps = nc;
		}
		TzTempRun *t = &g->temps[g->ntemps++];
		copiar(t->nome, sizeof t->nome, a->nome);
		t->due = tempo + PERIODO; t->seq = ++g->seq; t->contra = a->b[0];
		break;
	}
	case TZ_ACC_PARAR_TEMP:
		for (int i = 0; i < g->ntemps; ++i)
			if (!strcmp(g->temps[i].nome, a->t[0])) { memmove(&g->temps[i], &g->temps[i + 1], (size_t)(g->ntemps - i - 1) * sizeof *g->temps); g->ntemps--; break; }
		break;
	case TZ_ACC_PERDER: case TZ_ACC_GANHAR: {
		int ganhar = a->tipo == TZ_ACC_GANHAR;
		const char *barra = a->t[0] ? strchr(a->t[0], '|') : NULL;
		copiar(g->texto_fim, sizeof g->texto_fim, barra ? barra + 1 : "");
		for (int p = 1; p <= 8; ++p) {
			int na_lista = (a->u & (1u << (p - 1))) != 0;
			if (na_lista == ganhar) m->reino[p].venceu = 1; else m->reino[p].perdeu = 1;
		}
		acabar(c);
		break;
	}
	case TZ_ACC_MANTER: break;
	case TZ_ACC_MAPA:
		m->reino[m->jogador].venceu = 1;
		acabar(c);
		copiar(g->proximo_mapa, sizeof g->proximo_mapa, a->t[0]);
		break;
	case TZ_ACC_TRAVAR_VISTA: {
		TzPedido *p = pedir(g, TZ_PD_TRAVAR);
		p->cx = clamp_lado(c, a->a.rect[0]); p->cy = clamp_lado(c, a->a.rect[1]);
		if (a->a.objeto) p->obj = id_a_obj(c, a->a.objeto);
		int32_t ms = eval(c, a->t[0] ? a->t[0] : "", -1);
		p->ms = ms < 0 ? 0 : (uint32_t)ms;
		break;
	}
	case TZ_ACC_LINHA: {
		const char *linha = a->t[0] ? a->t[0] : "";
		if (linha[0] == '@') {
			g->texto_str[0] = 0;
			eval(c, linha, -1);
			linha = g->texto_str;
		}
		tz_mundo_guiao_mensagem(m, m->jogador, linha);
		break;
	}
	case TZ_ACC_HISTORIA: case TZ_ACC_AVISO: return janela(c, idx, a);
	case TZ_ACC_CRIAR: {
		int32_t n = eval(c, a->t[0] ? a->t[0] : "", -1);
		n = n < 0 ? 0 : n > 1000 ? 1000 : n;
		int r[4];
		if (!area_rect(c, &a->a, r) || a->u >= TZ_NCLASSES) break;
		int civ = a->b[1] >= 1 && a->b[1] <= 3 ? a->b[1] : 1;
		for (int k = 0; k < n; ++k) {
			int w = r[2] - r[0] + 1, h = r[3] - r[1] + 1;
			int x = r[0] + (w > 1 ? tz_rng_entre(&m->rng, 0, w - 1) : 0);
			int y = r[1] + (h > 1 ? tz_rng_entre(&m->rng, 0, h - 1) : 0);
			tz_mundo_guiao_criar(m, a->u, civ, a->b[0], clamp_lado(c, x), clamp_lado(c, y));
		}
		break;
	}
	case TZ_ACC_DESTRAVAR_VISTA: pedir(g, TZ_PD_DESTRAVAR); break;
	case TZ_ACC_ORDEM: ordem(c, &a->sel, &a->a, a->b[0]); break;
	case TZ_ACC_ROLAR_VISTA: {
		int de[2], para[2];
		if (celda_vista(c, &a->a, de) && celda_vista(c, &a->a2, para)) {
			TzPedido *p = pedir(g, TZ_PD_ROLAR);
			p->cx = de[0]; p->cy = de[1]; p->cx2 = para[0]; p->cy2 = para[1];
		}
		break;
	}
	case TZ_ACC_CONVERSA: {
		/* 0x4bc180: espera si hay una ventana abierta o la partida acabó, pero no a que termine la conversación */
		if (g->modal || g->acabou) return ESPERA;
		for (int i = 0; i < g->nconversas; ++i)
			if (!strcasecmp(g->conversas[i].nome, a->t[0])) { converter(c, i); break; }
		break;
	}
	case TZ_ACC_DESTRUIR: {
		Lista l = { 0 };
		sel_existente(c, &a->sel, &l);
		for (int k = 0; k < l.n; ++k) tz_mundo_guiao_matar(m, l.v[k]);
		free(l.v);
		ev_evento_drenar(c);
		break;
	}
	case TZ_ACC_EXPLORAR: {
		int cx, cy, raio;
		int32_t ms = eval(c, a->t[0] ? a->t[0] : "", -1);
		if (ms < 0) ms = 0;
		if (a->a.em_volta) {
			int i = id_a_obj(c, a->a.objeto);
			if (i < 0) break;
			cx = m->obj[i].cx; cy = m->obj[i].cy; raio = m->obj[i].visao ? m->obj[i].visao : 8;
		} else {
			int r[4];
			if (!area_rect(c, &a->a, r)) break;
			cx = (r[0] + r[2]) / 2; cy = (r[1] + r[3]) / 2;
			int rr = (r[2] - r[0] + 1 + r[3] - r[1] + 1) / 4;
			raio = rr >= 1 && rr <= 31 ? rr : 8;
		}
		/* un rato y no para siempre (0x4bc3dc): el tiempo dado; un área sin tiempo, 4 s; alrededor de un objeto sin tiempo, nada */
		if (!ms && !a->a.em_volta) ms = 4000;
		if (ms) tz_mundo_guiao_revelar_temp(m, m->jogador, clamp_lado(c, cx), clamp_lado(c, cy), raio, ((uint32_t)ms + TZ_TIC_MS - 1) / TZ_TIC_MS);
		break;
	}
	case TZ_ACC_APARECER:
		for (int k = 0; k < a->sel.nids; ++k) aparecer(c, id_a_obj(c, a->sel.ids[k]));
		break;
	case TZ_ACC_ESTADO: {
		int k = a->b[0];
		if (k >= 1 && k <= 8) {
			TzReinoEstado *r = &m->reino[k];
			int livre = (r->nao_pode_ganhar && !a->b[1]) || (r->nao_pode_perder && !a->b[2]);
			r->nao_pode_ganhar = a->b[1]; r->nao_pode_perder = a->b[2];
			if (livre) g->fim_devido = 1;
		}
		break;
	}
	case TZ_ACC_BLOQUEAR: {
		Lista l = { 0 };
		sel_existente(c, &a->sel, &l);
		for (int k = 0; k < l.n; ++k) {
			if (a->b[0]) gm_flags(m, l.v[k], 0, TZ_F_BLOQUEADO);
			else { gm_flags(m, l.v[k], TZ_F_BLOQUEADO, 0); TzPedido *p = pedir(g, TZ_PD_DESSELECIONAR); p->obj = l.v[k]; }
		}
		free(l.v);
		break;
	}
	case TZ_ACC_APAGAR: {
		Lista l = { 0 };
		sel_existente(c, &a->sel, &l);
		for (int k = 0; k < l.n; ++k) tz_mundo_guiao_remover(m, l.v[k]);
		free(l.v);
		break;
	}
	case TZ_ACC_ESPERAR: {
		if (g->esperando[idx]) { g->esperando[idx] = 0; return 0; }
		int32_t ms = eval(c, a->t[0] ? a->t[0] : "", -1);
		if (ms > 0) { g->esperando[idx] = 1; return (uint64_t)ms; }
		break;
	}
	}
	(void)tempo;
	return 0;
}

/* ------------------------------------------------------------------ conversaciones (0x4382c0, 0x4380f0, 0x438380) */
static void falante(Ctx *c, int id, const char **nome, const char **retrato)
{
	*nome = ""; *retrato = "";
	const TzPersonagem *p = tz_guiao_personagem(c->g, id);
	if (p) { *nome = p->nome; *retrato = p->retrato_grande; if (!p->nome[0]) { int i = id_a_obj(c, id); if (i >= 0) *nome = tz_classes[c->m->obj[i].classe]; } return; }
	int i = id_a_obj(c, id);
	if (i >= 0) *nome = tz_classes[c->m->obj[i].classe];
}

static void dizer(Ctx *c, int frase)
{
	TzGuiao *g = c->g;
	if (!g->conv_activa) return;
	g->conv_frase = frase;
	g->nescolhas_conv = 0;
	const TzFrase *f = &g->conversas[g->conv_i].frases[frase];
	const char *nome, *ret;
	falante(c, f->fala_de, &nome, &ret);
	memset(&g->msg, 0, sizeof g->msg);
	copiar(g->msg.titulo, sizeof g->msg.titulo, nome);
	copiar(g->msg.texto, sizeof g->msg.texto, f->texto);
	copiar(g->msg.retrato, sizeof g->msg.retrato, ret);
	pedir(g, TZ_PD_FALA);
}

static void oferecer(Ctx *c, const int *cand, int ncand)
{
	TzGuiao *g = c->g;
	if (!g->conv_activa) return;
	const TzConversa *cv = &g->conversas[g->conv_i];
	int abertas[256], n = 0;
	for (int k = 0; k < ncand && n < 256; ++k) {
		const TzFrase *f = &cv->frases[cand[k]];
		int ok = 1;
		if (f->condicao) {
			const char *s = f->condicao;
			while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n') s++;
			if (*s) ok = eval(c, f->condicao, -1) != 0;
		}
		if (ok) abertas[n++] = cand[k];
	}
	if (n == 0) {
		g->conv_activa = 0; g->modal = 0; g->conv_frase = -1; g->nescolhas_conv = 0;
		memset(&g->msg, 0, sizeof g->msg);
		pedir(g, TZ_PD_FECHAR);
	} else if (n == 1) {
		dizer(c, abertas[0]);
	} else {
		if (n > 8) n = 8;
		/* agrupadas por quien habla (qsort 0x52e2b0): estable */
		for (int i = 1; i < n; ++i) {
			int v = abertas[i], j = i - 1;
			while (j >= 0 && cv->frases[abertas[j]].fala_de > cv->frases[v].fala_de) { abertas[j + 1] = abertas[j]; j--; }
			abertas[j + 1] = v;
		}
		g->conv_frase = -1;
		g->nescolhas_conv = n;
		memset(&g->msg, 0, sizeof g->msg);
		const char *nome, *ret;
		falante(c, cv->frases[abertas[0]].fala_de, &nome, &ret);
		copiar(g->msg.titulo, sizeof g->msg.titulo, nome);
		copiar(g->msg.retrato, sizeof g->msg.retrato, ret);
		for (int i = 0; i < n; ++i) { g->escolhas_conv[i] = abertas[i]; copiar(g->msg.escolhas[i], sizeof g->msg.escolhas[i], cv->frases[abertas[i]].texto); }
		g->msg.nescolhas = n;
		pedir(g, TZ_PD_FALA);
	}
}

static void converter(Ctx *c, int at)
{
	TzGuiao *g = c->g;
	const TzConversa *cv = &g->conversas[at];
	int inicios[8], n = 0;
	for (int i = 0; i < cv->nfrases && n < 8; ++i) if (cv->frases[i].inicio) inicios[n++] = i;
	g->conv_activa = 1; g->conv_i = at; g->conv_frase = -1; g->nescolhas_conv = 0;
	g->modal = 1;
	oferecer(c, inicios, n);
}

int tz_guiao_historia_texto(TzGuiao *g, const char *texto, const char *retrato, const char *som, const char *musica, uint32_t ms, int fecha)
{
	if (g->modal) return -1;
	TzMensagem *ms_ = &g->msg;
	memset(ms_, 0, sizeof *ms_);
	copiar(ms_->texto, sizeof ms_->texto, texto);
	copiar(ms_->retrato, sizeof ms_->retrato, retrato);
	TzPedido *p = pedir(g, TZ_PD_HISTORIA);
	copiar(p->som, sizeof p->som, som);
	copiar(p->musica, sizeof p->musica, musica);
	p->ms = ms;
	p->fecha = fecha;
	g->modal = 1;
	return 0;
}

void tz_guiao_fechar(TzMundo *m, TzGuiao *g)
{
	(void)m;
	if (!g->conv_activa) { g->modal = 0; memset(&g->msg, 0, sizeof g->msg); }
}

void tz_guiao_responder(TzMundo *m, TzGuiao *g, int escolha)
{
	Ctx c = { m, g };
	if (!g->conv_activa) return;
	const TzConversa *cv = &g->conversas[g->conv_i];
	if (g->conv_frase < 0) {
		if (g->nescolhas_conv <= 0) { oferecer(&c, NULL, 0); return; }
		int k = escolha >= 0 && escolha < g->nescolhas_conv ? escolha : 0;
		dizer(&c, g->escolhas_conv[k]);
		return;
	}
	const TzFrase *f = &cv->frases[g->conv_frase];
	if (f->acao && f->acao[0]) {
		const char *s = f->acao;
		while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n') s++;
		if (*s) {
			char buf[2100];
			tz_expr_sentencias(f->acao, buf, sizeof buf);
			eval(&c, buf, -1);
		}
	}
	int seg[256], n = 0;
	for (int k = 0; k < f->nseg && n < 256; ++k)
		for (int j = 0; j < cv->nfrases; ++j)
			if (cv->frases[j].indice == f->seg[k]) { seg[n++] = j; break; }
	oferecer(&c, seg, n);
}

const TzFrase *tz_guiao_fala(const TzGuiao *g)
{
	if (!g->conv_activa || g->conv_frase < 0) return NULL;
	return &g->conversas[g->conv_i].frases[g->conv_frase];
}

const TzPersonagem *tz_guiao_personagem(const TzGuiao *g, int id)
{
	for (int i = 0; i < g->npers; ++i) if (g->pers[i].id == id) return &g->pers[i];
	return NULL;
}

/* ------------------------------------------------------------------ reparto: reglas y temporizadores */

static void repostar(Ctx *c, int i, uint64_t tempo, uint64_t atraso, int cursor, int manter)
{
	TzRegraRun *r = &c->g->run[i];
	r->due = tempo + atraso;
	r->seq = ++c->g->seq;
	r->cursor = cursor;
	r->manter = (uint8_t)manter;
}

static void executar_regra(Ctx *c, int i, uint64_t tempo)
{
	TzGuiao *g = c->g;
	TzRegraRun run = g->run[i];
	const TzRegra *regra = &g->regras[run.regra];
	int manter = run.manter;
	int inicio;
	if (run.cursor >= 0) inicio = run.cursor;
	else {
		if (tz_expr_eval(&OPS, c, regra->condicao ? regra->condicao : "", -1) == 0) { repostar(c, i, tempo, PERIODO, -1, manter); return; }
		g->n_regras_disparadas++;
		inicio = 0;
	}
	for (int at = inicio; at < regra->nacoes; ++at) {
		int idx = regra->acoes[at];
		if (idx < 0) { g->erros++; break; }   /* un nombre desconocido termina la lista */
		uint64_t espera = executar(c, idx, tempo);
		if (espera) { repostar(c, i, tempo, espera, at, manter); return; }
		if (g->acoes[idx].tipo == TZ_ACC_MANTER) manter = 1;
	}
	if (manter) repostar(c, i, tempo, PERIODO, -1, 1);
	else { memmove(&g->run[i], &g->run[i + 1], (size_t)(g->nrun - i - 1) * sizeof *g->run); g->nrun--; }
}

static void tic_temp(Ctx *c, int i, uint64_t tempo)
{
	TzGuiao *g = c->g;
	TzTempRun t = g->temps[i];
	int32_t v = tz_vars_get(&g->vars, t.nome);
	if (t.contra) {
		v = (int32_t)((uint32_t)v - PERIODO);
		tz_vars_set(&g->vars, t.nome, v);
		if (v <= 0) { memmove(&g->temps[i], &g->temps[i + 1], (size_t)(g->ntemps - i - 1) * sizeof *g->temps); g->ntemps--; return; }
	} else tz_vars_set(&g->vars, t.nome, (int32_t)((uint32_t)v + PERIODO));
	g->temps[i].due = tempo + PERIODO;
	g->temps[i].seq = ++g->seq;
}

static void repartir(Ctx *c, uint64_t agora_ms)
{
	TzGuiao *g = c->g;
	for (;;) {
		int ri = -1, ti = -1;
		for (int i = 0; i < g->nrun; ++i)
			if (g->run[i].due <= agora_ms && (ri < 0 || g->run[i].due < g->run[ri].due || (g->run[i].due == g->run[ri].due && g->run[i].seq < g->run[ri].seq))) ri = i;
		for (int i = 0; i < g->ntemps; ++i)
			if (g->temps[i].due <= agora_ms && (ti < 0 || g->temps[i].due < g->temps[ti].due || (g->temps[i].due == g->temps[ti].due && g->temps[i].seq < g->temps[ti].seq))) ti = i;
		if (ri < 0 && ti < 0) break;
		if (ri >= 0 && ti >= 0 && (g->temps[ti].due < g->run[ri].due || (g->temps[ti].due == g->run[ri].due && g->temps[ti].seq < g->run[ri].seq))) tic_temp(c, ti, g->temps[ti].due);
		else if (ri >= 0) executar_regra(c, ri, g->run[ri].due);
		else tic_temp(c, ti, g->temps[ti].due);
	}
}

static void iniciar(Ctx *c)
{
	TzGuiao *g = c->g;
	TzMundo *m = c->m;
	g->iniciado = 1;
	g->vars.nmapa = 0;
	free(g->esperando);
	g->nesperando = g->nacoes ? g->nacoes : 1;
	g->esperando = calloc((size_t)g->nesperando, 1);
	free(g->oculto);
	g->nobj_oculto = m->nobj;
	g->oculto = calloc((size_t)(m->nobj ? m->nobj : 1), 1);
	for (int i = 0; i < m->nobj; ++i)
		if ((m->obj[i].flags & TZ_F_INATIVO) && !(m->obj[i].flags & 4)) g->oculto[i] = 1;
	if (g->nregras > g->caprun) {
		free(g->run);
		g->run = calloc((size_t)g->nregras, sizeof *g->run);
		g->caprun = g->nregras;
	}
	g->nrun = 0;
	for (int r = 0; r < g->nregras; ++r) {
		TzRegraRun *q = &g->run[g->nrun++];
		q->regra = r; q->due = 1; q->seq = ++g->seq; q->cursor = -1; q->manter = 0;
	}
	g->hora = 12;
}

void tz_guiao_tic(TzMundo *m, TzGuiao *g)
{
	Ctx c = { m, g };
	if (!g->iniciado) {
		iniciar(&c);
		g->cursor_ev = m->ev_seq;   /* lo anterior al guion no cuenta */
	}
	/* objetos nacidos después del arranque: no están ocultos */
	if (m->nobj > g->nobj_oculto) {
		uint8_t *n = realloc(g->oculto, (size_t)m->nobj);
		if (n) { memset(n + g->nobj_oculto, 0, (size_t)(m->nobj - g->nobj_oculto)); g->oculto = n; g->nobj_oculto = m->nobj; }
	}
	ev_evento_drenar(&c);
	uint64_t ahora = agora(m);
	repartir(&c, ahora);
	/* ReTzar prueba el final tras un suceso (muerte, cambio de dueño o de diplomacia, rendición) y, sin suceso, solo los
	 * límites de tiempo y de puntos; nunca al principio */
	int limites = (m->regras.vitoria == 2 && ahora > (uint64_t)m->regras.vitoria_limite * 60000u) || m->regras.vitoria == 3;
	for (int p = 1; p <= 8; ++p) if (m->reino[p].rendido && !m->reino[p].derrotado) g->fim_devido = 1;
	int devido = g->fim_devido;
	g->fim_devido = 0;
	if ((devido || limites) && !g->acabou && g->campanha) verificar_fim(&c, 0, 0, 0, 0, 0);
}

static void gancho_tic(TzMundo *m, void *dados) { tz_guiao_tic(m, (TzGuiao *)dados); }

/* ------------------------------------------------------------------ registro en el mundo */
int tz_guiao_registrar(TzMundo *m, TzGuiao *g, int campanha)
{
	g->campanha = campanha != 0;
	m->regras.campanha = (uint8_t)(campanha != 0);
	m->regras.sem_vitoria_auto = (uint8_t)(campanha != 0);
	if (campanha && g->jogador >= 1 && g->jogador <= 8) m->jogador = g->jogador;
	if (g->tem_diplomacia) {
		for (int a = 1; a <= 8; ++a)
			for (int b = 1; b <= 8; ++b) {
				uint32_t d = g->diplomacia[(a - 1) * 8 + (b - 1)];
				if (d) tz_mundo_guiao_diplomacia(m, a, b, d);
			}
	}
	extern int tz_guiao_gravar(TzMundo *, void *, FILE *);
	extern int tz_guiao_carregar(TzMundo *, void *, FILE *, uint32_t);
	TzGancho gc;
	memset(&gc, 0, sizeof gc);
	gc.dados = g;
	gc.tic = gancho_tic;
	gc.gravar = tz_guiao_gravar;
	gc.carregar = tz_guiao_carregar;
	memcpy(gc.etiqueta, "GUIA", 4);
	gc.cada = 1; gc.desfase = 0;
	return tz_mundo_gancho(m, &gc);
}

/* ------------------------------------------------------------------ de una misión a la siguiente (0x43ea20, 0x440c58) */
int tz_guiao_herois(const TzMundo *m, const TzGuiao *g, TzHeroi *out, int max)
{
	int n = 0;
	for (int pass = 0; pass < 2; ++pass)
		for (int k = pass ? 0 : 1; k <= (pass ? 0 : 11); ++k)
			for (int i = 0; i < m->nobj && n < max; ++i) {
				const TzObj *o = &m->obj[i];
				if (!(o->flags & TZ_F_CARREGADO)) continue;
				const TzPersonagem *p = tz_guiao_personagem(g, o->id_mapa);
				if (!p || !p->nome[0]) continue;
				int oculto_ = i < g->nobj_oculto && g->oculto && g->oculto[i];
				if (pass == 0) { if (o->dono != k || oculto_ || (o->flags & (TZ_F_MORTO | TZ_F_INATIVO | TZ_F_MORRENDO))) continue; }
				else if (!oculto_) continue;
				snprintf(out[n].nome, sizeof out[n].nome, "%s", p->nome);
				snprintf(out[n].subtitulo, sizeof out[n].subtitulo, "%s", p->subtitulo);
				out[n].vida = (uint16_t)(o->vida < 0 ? 0 : o->vida > 65535 ? 65535 : o->vida);
				out[n].vida_max = (uint16_t)(o->vida_max > 65535 ? 65535 : o->vida_max);
				out[n].nivel = o->nivel;
				out[n].promovido = pass ? 1 : (o->flags & TZ_F_HEROI) != 0;
				n++;
			}
	return n;
}

void tz_guiao_restaurar_herois(TzMundo *m, const TzGuiao *g, const TzHeroi *h, int n)
{
	for (int i = 0; i < m->nobj; ++i) {
		TzObj *o = &m->obj[i];
		if ((o->flags & (TZ_F_CARREGADO | TZ_F_INATIVO | TZ_F_MORTO)) != TZ_F_CARREGADO) continue;
		const TzPersonagem *p = tz_guiao_personagem(g, o->id_mapa);
		if (!p || !p->nome[0]) continue;
		const TzHeroi *x = NULL;
		for (int k = 0; k < n && !x; ++k) if (!strcasecmp(h[k].nome, p->nome)) x = &h[k];
		if (!x) continue;
		/* el subtítulo pasa también (el héroe ascendido lo cambia); el personaje es de solo lectura para quien llama */
		TzPersonagem *pw = (TzPersonagem *)p;
		snprintf(pw->subtitulo, sizeof pw->subtitulo, "%s", x->subtitulo);
		o->vida_max = x->vida_max ? x->vida_max : 1;
		tz_mundo_guiao_atributos(m, i, x->vida < 1 ? 1 : x->vida > o->vida_max ? o->vida_max : x->vida, x->nivel, -1,
		                         x->promovido ? TZ_F_HEROI : 0, 0);
		o->xp = 0;
	}
}

int tz_guiao_mantidas(const TzGuiao *g, TzVar *out, int max)
{
	int n = g->vars.nmantidas < max ? g->vars.nmantidas : max;
	memcpy(out, g->vars.mantidas, (size_t)n * sizeof *out);
	return n;
}

void tz_guiao_mantidas_definir(TzGuiao *g, const TzVar *v, int n)
{
	if (n > 256) n = 256;
	memcpy(g->vars.mantidas, v, (size_t)n * sizeof *v);
	g->vars.nmantidas = n;
}
