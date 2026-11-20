/* SPDX-License-Identifier: GPL-3.0-or-later */
/* tzar_mundo_falso.c - un «mundo» mínimo con la interfaz de mundo.h para probar guiones, campañas y sonido sin la simulación
 * (D417, fase 7; solo pruebas en el PC, nunca va al juego). Guarda los objetos del mapa, los reinos, los eventos y los ganchos;
 * las acciones tz_mundo_guiao_* hacen lo mínimo que un guion puede observar (matar, quitar, crear, dueño, recursos...). */
#include "mundo.h"
#include <stdlib.h>
#include <string.h>

uint32_t tz_rng(TzRng *r)
{
	r->s = r->s * 1664525u + 1013904223u;
	return r->s >> 8;
}
int32_t tz_rng_entre(TzRng *r, int32_t lo, int32_t hi)
{
	if (hi <= lo) return lo;
	return lo + (int32_t)(tz_rng(r) % (uint32_t)(hi - lo + 1));
}

const TzAtributos *tz_catalogo_at(const TzCatalogo *c, int classe, int civ) { (void)c; (void)classe; (void)civ; return NULL; }

TzId tz_mundo_por_id_mapa(const TzMundo *m, int id)
{
	for (int i = 0; i < m->nobj; ++i) if (m->obj[i].id_mapa == id) return i;
	return TZ_NENHUM;
}

int tz_obj_vivo(const TzObj *o) { return o && !(o->flags & (TZ_F_MORTO | TZ_F_MORRENDO | TZ_F_DENTRO)); }
int tz_obj_unidade(const TzObj *o) { return tz_classe_e_unidade(o->classe); }
int tz_obj_edificio(const TzObj *o) { return tz_classe_e_edificio(o->classe); }

int tz_mundo_nevoa(const TzMundo *m, int k, int cx, int cy) { (void)m; (void)k; (void)cx; (void)cy; return 2; }
TzOrdem tz_mundo_ordem_padrao(const TzMundo *m, int k, const TzId *ids, int n, int cx, int cy, TzId alvo)
{
	(void)m; (void)k; (void)ids; (void)n;
	TzOrdem o;
	memset(&o, 0, sizeof o);
	o.tipo = alvo >= 0 ? TZ_O_SEGUIR : TZ_O_MOVER;
	o.alvo = alvo; o.x = (int16_t)cx; o.y = (int16_t)cy;
	return o;
}

/* ---------------------------------------------------------------- eventos */
void tz_mundo_evento(TzMundo *m, const TzEvento *e)
{
	TzEvento *d = &m->ev[m->ev_seq % TZ_EVENTOS];
	*d = *e;
	d->seq = m->ev_seq + 1;
	d->tic = m->tic;
	m->ev_seq++;
}
int tz_mundo_eventos(const TzMundo *m, uint32_t *cursor, TzEvento *out, int max)
{
	if (m->ev_seq - *cursor > TZ_EVENTOS) *cursor = m->ev_seq - TZ_EVENTOS;
	int n = 0;
	while (*cursor < m->ev_seq && n < max) out[n++] = m->ev[(*cursor)++ % TZ_EVENTOS];
	return n;
}
const char *tz_mundo_texto(const TzMundo *m, int32_t idx) { return m->textos[idx & 63]; }

static void ev(TzMundo *m, int tipo, int reino, TzId obj, TzId alvo, int32_t valor)
{
	TzEvento e;
	memset(&e, 0, sizeof e);
	e.tipo = (uint8_t)tipo; e.reino = (uint8_t)reino; e.obj = obj; e.alvo = alvo; e.valor = valor;
	tz_mundo_evento(m, &e);
}

int tz_mundo_gancho(TzMundo *m, const TzGancho *g)
{
	if (m->nganchos >= TZ_GANCHOS) return -1;
	m->gancho[m->nganchos++] = *g;
	return 0;
}

/* ---------------------------------------------------------------- vida del mundo falso */
TzMundo *falso_criar(const TzMapa *mapa, int jogador)
{
	TzMundo *m = calloc(1, sizeof *m);
	m->mapa = mapa;
	m->lado = mapa->lado;
	m->capobj = mapa->nobj + 64;
	m->obj = calloc((size_t)m->capobj, sizeof *m->obj);
	for (int i = 0; i < mapa->nobj; ++i) {
		const TzObjeto *s = &mapa->obj[i];
		TzObj *o = &m->obj[i];
		o->id_mapa = s->id; o->classe = s->classe; o->civ = s->civ; o->dono = s->dono; o->flags = s->flags;
		o->cx = (int16_t)s->x; o->cy = (int16_t)s->y; o->vida = s->vida; o->vida_max = s->vida_max;
		o->dentro_de = TZ_NENHUM; o->original = s;
	}
	m->nobj = mapa->nobj;
	for (int k = 1; k <= 8; ++k) {
		TzReinoEstado *r = &m->reino[k];
		if (k - 1 < mapa->nreinos && mapa->reinos[k - 1].ativo) {
			r->ativo = 1;
			snprintf(r->nome, sizeof r->nome, "%s", mapa->reinos[k - 1].nome);
			for (int j = 0; j < 4; ++j) r->recursos[j] = (int32_t)mapa->reinos[k - 1].recursos[j];
		}
	}
	m->jogador = jogador;
	m->regras.populacao = 200;
	m->rng.s = 12345;
	return m;
}
void falso_liberar(TzMundo *m) { free(m->obj); free(m); }

void falso_tic(TzMundo *m)
{
	m->tic++;
	for (int i = 0; i < m->nganchos; ++i) {
		TzGancho *g = &m->gancho[i];
		if (g->tic && (!g->cada || (m->tic % g->cada) == g->desfase)) g->tic(m, g->dados);
	}
}

/* ---------------------------------------------------------------- acciones de guion */
TzId tz_mundo_guiao_criar(TzMundo *m, int classe, int civ, int dono, int cx, int cy)
{
	if (m->nobj == m->capobj) {
		m->capobj *= 2;
		m->obj = realloc(m->obj, (size_t)m->capobj * sizeof *m->obj);
	}
	TzObj *o = &m->obj[m->nobj];
	memset(o, 0, sizeof *o);
	o->id_mapa = 60000 + (uint16_t)m->nobj; o->classe = (uint16_t)classe; o->civ = (uint8_t)civ; o->dono = (uint8_t)dono;
	o->cx = (int16_t)cx; o->cy = (int16_t)cy; o->vida = o->vida_max = 10; o->dentro_de = TZ_NENHUM;
	ev(m, TZ_EV_CRIADO, dono, m->nobj, TZ_NENHUM, 0);
	return m->nobj++;
}
void tz_mundo_guiao_remover(TzMundo *m, TzId i)
{
	if (i < 0 || i >= m->nobj) return;
	m->obj[i].flags |= TZ_F_MORTO;
	ev(m, TZ_EV_REMOVIDO, m->obj[i].dono, i, TZ_NENHUM, 0);
}
void tz_mundo_guiao_matar(TzMundo *m, TzId i)
{
	if (i < 0 || i >= m->nobj || (m->obj[i].flags & TZ_F_MORTO)) return;
	m->obj[i].flags |= TZ_F_MORTO;
	m->obj[i].vida = 0;
	ev(m, TZ_EV_MORREU, m->obj[i].dono, i, TZ_NENHUM, 0);
}
void tz_mundo_guiao_dono(TzMundo *m, TzId i, int dono)
{
	if (i < 0 || i >= m->nobj) return;
	int velho = m->obj[i].dono;
	m->obj[i].dono = (uint8_t)dono;
	ev(m, TZ_EV_CAPTURADO, dono, i, TZ_NENHUM, velho);
}
void tz_mundo_guiao_recursos(TzMundo *m, int k, int recurso, int32_t delta) { if (k >= 1 && k <= 8) m->reino[k].recursos[recurso] += delta; }
void tz_mundo_guiao_diplomacia(TzMundo *m, int a, int b, uint32_t bits) { if (a >= 1 && a <= 8 && b >= 1 && b <= 8) m->reino[a].diplomacia[b] = bits; }
void tz_mundo_guiao_revelar(TzMundo *m, int k, int cx, int cy, int raio) { (void)m; (void)k; (void)cx; (void)cy; (void)raio; }
void tz_mundo_guiao_pesquisa(TzMundo *m, int k, int tec) { (void)m; (void)k; (void)tec; }
void tz_mundo_guiao_proibir(TzMundo *m, int k, const char *nome, int proibido) { (void)m; (void)k; (void)nome; (void)proibido; }
void tz_mundo_guiao_ordem(TzMundo *m, TzId i, const TzOrdem *o)
{
	if (i < 0 || i >= m->nobj) return;
	static TzAtor ator_falso[1];
	(void)ator_falso;
	ev(m, TZ_EV_ORDEM, m->obj[i].dono, i, o->alvo, o->tipo);
}
void tz_mundo_guiao_mensagem(TzMundo *m, int k, const char *texto)
{
	snprintf(m->textos[m->ntextos & 63], sizeof m->textos[0], "%s", texto);
	ev(m, TZ_EV_MENSAGEM, k, TZ_NENHUM, TZ_NENHUM, (int32_t)(m->ntextos & 63));
	m->ntextos++;
}
void tz_mundo_guiao_som(TzMundo *m, const char *nome, int cx, int cy)
{
	snprintf(m->textos[m->ntextos & 63], sizeof m->textos[0], "%s", nome);
	TzEvento e;
	memset(&e, 0, sizeof e);
	e.tipo = TZ_EV_SOM; e.x = (int16_t)cx; e.y = (int16_t)cy; e.valor = (int32_t)(m->ntextos & 63);
	tz_mundo_evento(m, &e);
	m->ntextos++;
}
void tz_mundo_guiao_fim(TzMundo *m, int k, int ganhou)
{
	if (ganhou) m->reino[k].venceu = 1; else m->reino[k].perdeu = 1;
	m->acabou = 1; m->ganhou = (uint8_t)ganhou;
	ev(m, TZ_EV_FIM, k, TZ_NENHUM, TZ_NENHUM, ganhou);
}

/* las acciones que el mundo añadió en la integración (aparecer, atributos, terminar obra): lo mínimo, como el mundo */
int tz_mundo_guiao_aparecer(TzMundo *m, TzId i)
{
	TzObj *o = tz_mundo_obj(m, i);
	if (!o) return -1;
	o->flags &= ~(uint32_t)TZ_F_INATIVO;
	return 0;
}

void tz_mundo_guiao_atributos(TzMundo *m, TzId i, int32_t vida, int nivel, int mana, uint32_t poner, uint32_t quitar)
{
	TzObj *o = tz_mundo_obj(m, i);
	if (!o) return;
	if (vida >= 0) { if (vida > o->vida_max) o->vida_max = vida; o->vida = vida; }
	if (nivel >= 0) o->nivel = (uint8_t)(nivel > 255 ? 255 : nivel);
	if (mana >= 0) { o->mana = (uint16_t)(mana > 65535 ? 65535 : mana); if (o->mana > o->mana_max) o->mana_max = o->mana; }
	o->flags = (o->flags | poner) & ~quitar;
	if (vida == 0 && !(o->flags & TZ_F_DEUS)) tz_mundo_guiao_matar(m, i);
}

void tz_mundo_guiao_terminar_obra(TzMundo *m, TzId i)
{
	TzObj *o = tz_mundo_obj(m, i);
	if (o && (o->flags & TZ_F_OBRA)) { o->flags &= ~(uint32_t)TZ_F_OBRA; o->vida = o->vida_max; }
}
