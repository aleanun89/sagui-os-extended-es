/* SPDX-License-Identifier: GPL-3.0-or-later */
/* acoes.c - lo que hace cada objeto en cada tic: andar, atacar, disparar, recoger, talar, construir, reparar,
 * capturar, entrar y salir, guardar, patrullar; la producción de los edificios (entrenar, investigar, mejorar), las
 * torres, los proyectiles y los comandos de los reinos (D417, fase 5).
 *
 * Traducido a C (simplificado donde se indica) de ReTzar (https://github.com/ColCarlo/ReTzar, GPL-3.0-or-later),
 * gameplay.rs: Intent y los programas de cada orden (Chop 0x42dbe0, Harvest/Dig, Deliver 0x4adc20, Build/Repair
 * 0x4aba10, Capture), physical_damage (A²/(A+D), 0x504a3f), apply_damage/wound, missile() y land_projectiles,
 * resource_yield (10 + 2 por investigación), store_accepts, Job/JobKind y la cola de producción (QUEUE_LIMIT 20);
 * gameplay/movement.rs: pasos de casilla a casilla al ritmo de move_len y espera cuando la casilla está ocupada;
 * gameplay/combat.rs: búsqueda del enemigo más cercano dentro de la vista (simplificada).
 */
#include "mundo_int.h"
#include <stdlib.h>
#include <string.h>



static const int DX[8] = { 0, 1, 1, 1, 0, -1, -1, -1 };
static const int DY[8] = { 1, 1, 0, -1, -1, -1, 0, 1 };

/* movement.rs direction: 0 sur, 1 sureste, 2 este, 3 noreste, 4 norte, 5 noroeste, 6 oeste, 7 suroeste */
static int direcao(int dx, int dy)
{
	int sx = dx > 0 ? 1 : dx < 0 ? -1 : 0, sy = dy > 0 ? 1 : dy < 0 ? -1 : 0;
	/* con pendiente fuerte, recto */
	int ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;
	if (ax > 2 * ay) sy = 0;
	if (ay > 2 * ax) sx = 0;
	for (int d = 0; d < 8; ++d) if (DX[d] == sx && DY[d] == sy) return d;
	return 0;
}

static void olhar(TzObj *o, int x, int y)
{
	if (x != o->cx || y != o->cy) o->direcao = (uint8_t)direcao(x - o->cx, y - o->cy);
}

static void olhar_obj(const TzMundo *m, TzObj *o, const TzObj *t)
{
	int r[4];
	tz_obj_ret(m, t, r);
	olhar(o, (r[0] + r[2]) / 2, (r[1] + r[3]) / 2);
}

static void estado(TzMundo *m, TzObj *o, int e, int trab)
{
	if (o->estado != e || (o->ator && o->ator->trabalho != trab)) o->anim_tic = m->tic;
	o->estado = (uint8_t)e;
	if (o->ator) o->ator->trabalho = (uint8_t)trab;
}

static int esta_andando(const TzObj *o) { return o->ator && o->ator->passo < o->ator->passo_total; }

/* ------------------------------------------------------------------ órdenes */
void tz_parar(TzMundo *m, TzId i)
{
	TzObj *o = &m->obj[i];
	TzAtor *a = o->ator;
	if (!a) return;
	a->ordem.tipo = TZ_O_NADA; a->ordem.alvo = TZ_NENHUM;
	a->nfila = 0; a->ncaminho = a->icaminho = 0;
	a->retomar.tipo = TZ_O_NADA;
	a->automatica = 0; a->espera = 0; a->falhas = 0;
	a->destino[0] = a->destino[1] = -1;
	if (!esta_andando(o)) estado(m, o, TZ_E_PARADO, 0);
}

static void iniciar(TzMundo *m, TzObj *o, const TzOrdem *ord)
{
	TzAtor *a = o->ator;
	a->ordem = *ord;
	a->ncaminho = a->icaminho = 0;
	a->destino[0] = a->destino[1] = -1;
	a->espera = 0; a->falhas = 0;
	a->trabalho_tic = 0; a->ciclos = 0;
	a->recarga = 0;
	a->automatica = 0;
	(void)m;
}

void tz_dar_ordem(TzMundo *m, TzId i, const TzOrdem *ord, int fila)
{
	TzObj *o = &m->obj[i];
	TzAtor *a = o->ator;
	if (!a) return;
	if (ord->tipo == TZ_O_PARAR) { tz_parar(m, i); return; }
	if (fila && a->ordem.tipo != TZ_O_NADA) {
		if (a->nfila < TZ_FILA_ORDENS) a->fila[a->nfila++] = *ord;
		return;
	}
	a->nfila = 0;
	a->retomar.tipo = TZ_O_NADA;
	iniciar(m, o, ord);
}

static void proxima(TzMundo *m, TzId i)
{
	TzObj *o = &m->obj[i];
	TzAtor *a = o->ator;
	if (a->nfila) {
		TzOrdem n = a->fila[0];
		memmove(a->fila, a->fila + 1, (size_t)(a->nfila - 1) * sizeof *a->fila);
		a->nfila--;
		iniciar(m, o, &n);
		return;
	}
	if (a->retomar.tipo != TZ_O_NADA) {
		TzOrdem n = a->retomar;
		a->retomar.tipo = TZ_O_NADA;
		iniciar(m, o, &n);
		return;
	}
	a->ordem.tipo = TZ_O_NADA;
	a->ncaminho = a->icaminho = 0;
	if (!esta_andando(o)) estado(m, o, TZ_E_PARADO, 0);
}

/* ------------------------------------------------------------------ andar */
static void interpolar(TzObj *o)
{
	TzAtor *a = o->ator;
	if (!a || a->passo >= a->passo_total || a->passo_total <= 0) {
		o->px = o->cx * TZ_SUB + TZ_SUB / 2; o->py = o->cy * TZ_SUB + TZ_SUB / 2;
		return;
	}
	int32_t f = (int32_t)((int64_t)a->passo * TZ_SUB / a->passo_total);
	o->px = (a->de[0] * TZ_SUB + TZ_SUB / 2) + (o->cx - a->de[0]) * f;
	o->py = (a->de[1] * TZ_SUB + TZ_SUB / 2) + (o->cy - a->de[1]) * f;
}

/* avanza el paso en curso; 1 si sigue a mitad de casilla */
static int avancar_passo(TzMundo *m, TzObj *o)
{
	TzAtor *a = o->ator;
	if (a->passo >= a->passo_total) return 0;
	a->passo += TZ_TIC_MS;
	if (a->passo >= a->passo_total) a->passo = a->passo_total;
	interpolar(o);
	(void)m;
	return a->passo < a->passo_total;
}

static void dar_passo(TzMundo *m, TzId i, int nx, int ny)
{
	TzObj *o = &m->obj[i];
	TzAtor *a = o->ator;
	const TzAtributos *at = tz_obj_at(m, o);
	tz_marcar(m, i, 0);
	a->de[0] = o->cx; a->de[1] = o->cy;
	olhar(o, nx, ny);
	o->cx = (int16_t)nx; o->cy = (int16_t)ny;
	tz_marcar(m, i, 1);
	int diag = a->de[0] != nx && a->de[1] != ny;
	int32_t t = at->passo_ms ? at->passo_ms : 640;
	/* cargado, el peón va algo más despacio (sus dibujos «carry» tienen otro move_len) */
	a->passo_total = diag ? t * 1414 / 1000 : t;
	a->passo = 0;
	if (o->estado != TZ_E_ANDANDO) estado(m, o, TZ_E_ANDANDO, 0);
}

/* Va hacia el rectángulo r hasta quedar a distancia <= alc. 1 = ya está, 0 = andando, -1 = no puede llegar. */
static int mover_para(TzMundo *m, TzId i, const int r[4], int alc)
{
	TzObj *o = &m->obj[i];
	TzAtor *a = o->ator;
	if (avancar_passo(m, o)) return 0;
	if (tz_dist_ret(o->cx, o->cy, r) <= alc) {
		a->ncaminho = a->icaminho = 0; a->espera = 0;
		return 1;
	}
	int capa = tz_obj_capa(m, o);
	/* ¿sirve el camino que lleva? */
	/* un camino parcial (a lo más cercano: falhas > 0) se sigue hasta el final antes de buscar otro */
	int valido = a->icaminho < a->ncaminho && a->destino[0] >= 0 && (a->falhas || tz_dist_ret(a->destino[0], a->destino[1], r) <= alc);
	if (!valido) {
		if (a->falhas && (m->tic + (uint32_t)i) % 10) { if (o->estado == TZ_E_ANDANDO) estado(m, o, TZ_E_PARADO, 0); return a->falhas > 4 ? -1 : 0; }
		int res = tz_caminho(m, i, o->cx, o->cy, r, alc, a->espera ? 3 : 0);
		if (res < 0 || a->ncaminho == 0) {
			a->falhas++;
			if (o->estado == TZ_E_ANDANDO) estado(m, o, TZ_E_PARADO, 0);
			return a->falhas > 4 ? -1 : 0;
		}
		a->falhas = res ? 0 : (uint16_t)(a->falhas + 1);
		if (!res && a->falhas > 4) a->falhas = 4;   /* va a lo más cercano y lo da por bueno al llegar */
	}
	int nx = a->caminho[a->icaminho][0], ny = a->caminho[a->icaminho][1];
	if (!tz_cel_transitavel(m, capa, o->dono, nx, ny)) { a->ncaminho = 0; a->falhas++; return 0; }
	if (!tz_cel_livre(m, capa, o->dono, nx, ny) && !(m->cel[ny * m->lado + nx].bits & TZ_CEL_PORTA && m->cel[ny * m->lado + nx].ocupante < 0)) {
		/* ocupada: esperar, luego buscar otro camino evitando a los parados */
		a->espera++;
		if (o->estado == TZ_E_ANDANDO) estado(m, o, TZ_E_PARADO, 0);
		if (a->espera == 8 || a->espera == 30) a->ncaminho = 0;
		if (a->espera > 60) { a->espera = 0; a->ncaminho = 0; return -1; }
		/* el último paso hasta una casilla ocupada: con estar al lado basta */
		if (a->icaminho == a->ncaminho - 1 && tz_dist_ret(o->cx, o->cy, r) <= alc + 1 && a->espera > 4) return 1;
		return 0;
	}
	a->espera = 0;
	a->icaminho++;
	dar_passo(m, i, nx, ny);
	avancar_passo(m, o);
	return 0;
}

int tz_mover_para(TzMundo *m, TzId i, const int r[4], int alc) { return mover_para(m, i, r, alc); }

static int mover_cel(TzMundo *m, TzId i, int x, int y, int alc)
{
	int r[4] = { x, y, x, y };
	return mover_para(m, i, r, alc);
}

static int mover_obj(TzMundo *m, TzId i, TzId t, int alc)
{
	int r[4];
	tz_obj_ret(m, &m->obj[t], r);
	return mover_para(m, i, r, alc);
}

/* ------------------------------------------------------------------ daño (gameplay.rs physical_damage, wound) */
static uint32_t dano_fisico(uint32_t at, uint32_t df, int cat)
{
	if (!at) return 0;
	if (cat == 3) return at;
	uint64_t d = (uint64_t)at * at / ((uint64_t)at + df);
	return d ? (uint32_t)d : 1;
}

void tz_dano(TzMundo *m, TzId j, uint32_t golpe, int cat, TzId de)
{
	TzObj *t = &m->obj[j];
	if (!tz_obj_vivo(t) || (t->flags & TZ_F_DEUS) || t->classe == 72 || t->classe == 83) return;
	if (!tz_obj_unidade(t) && !tz_obj_edificio(t) && t->classe != TZC_COW) return;
	if (de >= 0) {
		const TzObj *a = &m->obj[de];
		golpe = tz_catalogo_bonus(m->cat, a->classe, a->civ, t->classe, t->civ, golpe);
	}
	uint32_t d = dano_fisico(golpe, t->defesa, cat);
	if (!d) return;
	t->vida -= (int32_t)d;
	tz_ev(m, TZ_EV_FERIDO, t->dono, de, j, t->cx, t->cy, t->classe, (int32_t)d);
	if (t->ator) t->ator->ultimo_golpe = m->tic;
	if (t->dono >= 1 && t->dono <= 8 && m->tic - ((TzPriv *)m->priv)->ultimo_aviso[t->dono] > 10u * TZ_TICS_SEG) {
		((TzPriv *)m->priv)->ultimo_aviso[t->dono] = m->tic;
		tz_ev(m, TZ_EV_ATAQUE_AVISO, t->dono, j, de, t->cx, t->cy, t->classe, 0);
	}
	if (t->vida <= 0) { tz_matar(m, j, de); return; }
	/* responder: la unidad sin órdenes del jugador se vuelve contra quien le pega (retaliate) */
	if (de >= 0 && t->ator && tz_obj_unidade(t) && t->ataque && tz_obj_vivo(&m->obj[de]) &&
	    (t->ator->ordem.tipo == TZ_O_NADA || t->ator->automatica) && !(t->ator->modo & TZ_MODO_QUIETO) &&
	    tz_mundo_relacao(m, t->dono, m->obj[de].dono) < 1 && t->classe != TZC_PEON) {
		TzOrdem o; memset(&o, 0, sizeof o);
		o.tipo = TZ_O_ATACAR; o.alvo = de;
		if (t->ator->ordem.tipo != TZ_O_ATACAR || t->ator->ordem.alvo != de) {
			iniciar(m, t, &o);
			t->ator->automatica = 1;
		}
	}
}

/* gameplay.rs missile(): sale un proyectil */
static void disparar(TzMundo *m, TzId i, TzId j)
{
	TzObj *o = &m->obj[i], *t = &m->obj[j];
	const TzAtributos *a = tz_obj_at(m, o);
	if (m->nproj == m->capproj) {
		int cap = m->capproj ? m->capproj * 2 : 64;
		void *n = realloc(m->proj, (size_t)cap * sizeof *m->proj);
		if (!n) return;
		m->proj = n; m->capproj = cap;
	}
	struct TzProjetil *p = &m->proj[m->nproj++];
	memset(p, 0, sizeof *p);
	p->atacante = i; p->alvo = j;
	p->classe = a->missil == 0 ? TZC_ARROW : a->missil == 2 ? 120 : 96;
	p->dano = o->ataque;
	p->categoria = a->categoria_arma;
	p->area = a->missil == 1;
	int ro[4], rt[4];
	tz_obj_ret(m, o, ro); tz_obj_ret(m, t, rt);
	p->x0 = (ro[0] + ro[2] + 1) * TZ_SUB / 2; p->y0 = (ro[1] + ro[3] + 1) * TZ_SUB / 2;
	p->x1 = (rt[0] + rt[2] + 1) * TZ_SUB / 2; p->y1 = (rt[1] + rt[3] + 1) * TZ_SUB / 2;
	int32_t dx = (p->x1 - p->x0) / 8, dy = (p->y1 - p->y0) / 8;   /* en píxeles (TZ_SUB/32 = 8) */
	uint32_t dist = (uint32_t)(dx < 0 ? -dx : dx) > (uint32_t)(dy < 0 ? -dy : dy) ? (uint32_t)(dx < 0 ? -dx : dx) : (uint32_t)(dy < 0 ? -dy : dy);
	uint32_t vel = a->missil_vel ? a->missil_vel * 10u : 500u;
	p->sai = m->tic;
	p->chega = m->tic + 1 + dist * 1000u / vel / TZ_TIC_MS;
	tz_ev(m, TZ_EV_DISPARO, o->dono, i, j, o->cx, o->cy, p->classe, 0);
}

void tz_projeteis_tic(TzMundo *m)
{
	int w = 0;
	for (int k = 0; k < m->nproj; ++k) {
		struct TzProjetil p = m->proj[k];
		if (m->tic < p.chega) { m->proj[w++] = p; continue; }
		int cx = p.x1 / TZ_SUB, cy = p.y1 / TZ_SUB;
		TzId de = p.atacante >= 0 && p.atacante < m->nobj ? p.atacante : TZ_NENHUM;
		tz_ev(m, TZ_EV_IMPACTO, 0, de, p.alvo, cx, cy, p.classe, 0);
		if (p.classe == TZC_ARROW) {
			/* la flecha sigue al blanco (0x432af0) */
			if (p.alvo >= 0 && tz_obj_vivo(&m->obj[p.alvo])) tz_dano(m, p.alvo, p.dano, p.categoria, de);
		} else {
			/* bala o virote: a lo que esté en la casilla; la bala, la mitad alrededor */
			int r[4];
			if (p.alvo >= 0 && tz_obj_vivo(&m->obj[p.alvo])) {
				tz_obj_ret(m, &m->obj[p.alvo], r);
				if (tz_dist_ret(cx, cy, r) <= 0) tz_dano(m, p.alvo, p.dano, p.categoria, de);
			}
			if (p.area) {
				TzId viz[32];
				int nv = tz_mundo_area(m, cx - 1, cy - 1, cx + 1, cy + 1, TZ_FILTRO_UNIDADES | TZ_FILTRO_EDIFICIOS, -1, viz, 32);
				for (int q = 0; q < nv; ++q) if (viz[q] != p.alvo && viz[q] != de) tz_dano(m, viz[q], p.dano / 2, p.categoria, de);
			}
		}
	}
	m->nproj = w;
}

/* ------------------------------------------------------------------ combate */
static int alcance_de(const TzMundo *m, const TzObj *o)
{
	const TzAtributos *a = tz_obj_at(m, o);
	return a->missil != 0xff && o->alcance ? o->alcance : 1;
}

/* el ciclo del golpe (gameplay.rs swing): espera, image_attack hasta actionrow, golpe */
static void golpear(TzMundo *m, TzId i, TzId j)
{
	TzObj *o = &m->obj[i];
	TzAtor *a = o->ator;
	const TzAtributos *at = tz_obj_at(m, o);
	if (o->estado != TZ_E_ATACANDO) { estado(m, o, TZ_E_ATACANDO, 0); a->recarga = at->ataque_lead_ms ? at->ataque_lead_ms : 1; }
	olhar_obj(m, o, &m->obj[j]);
	if (a->recarga > TZ_TIC_MS) { a->recarga -= TZ_TIC_MS; return; }
	a->recarga = at->ataque_ms ? at->ataque_ms : 1000;
	o->anim_tic = m->tic - (uint32_t)(at->ataque_lead_ms / TZ_TIC_MS);
	tz_ev(m, TZ_EV_ATACOU, o->dono, i, j, o->cx, o->cy, o->classe, 0);
	if (at->missil != 0xff && o->alcance) disparar(m, i, j);
	else tz_dano(m, j, o->ataque, at->categoria_arma, i);
}

/* combat.rs search (simplificada): el enemigo más cercano dentro del radio, primero unidades armadas */
static TzId buscar_inimigo(TzMundo *m, TzId i, int raio)
{
	TzObj *o = &m->obj[i];
	int n = m->lado, melhor = -1, dmelhor = 1 << 30, pmelhor = 9;
	for (int dy = -raio; dy <= raio; ++dy) {
		int y = o->cy + dy;
		if (y < 0 || y >= n) continue;
		for (int dx = -raio; dx <= raio; ++dx) {
			int x = o->cx + dx;
			if (x < 0 || x >= n) continue;
			const TzCelula *c = &m->cel[y * n + x];
			TzId cand[2] = { c->ocupante, c->fixo };
			for (int q = 0; q < 2; ++q) {
				TzId j = cand[q];
				if (j < 0 || j == i) continue;
				const TzObj *t = &m->obj[j];
				if (!tz_obj_vivo(t) || !tz_hostil(m, o->dono, j)) continue;
				if (o->dono >= 1 && o->dono <= 8 && tz_mundo_nevoa(m, o->dono, x, y) != 2) continue;
				int pr = tz_obj_unidade(t) ? (t->ataque ? 0 : 1) : (t->ataque ? 2 : 3);
				int d = dx < 0 ? -dx : dx; if ((dy < 0 ? -dy : dy) > d) d = dy < 0 ? -dy : dy;
				if (pr < pmelhor || (pr == pmelhor && (d < dmelhor || (d == dmelhor && j < melhor)))) { melhor = j; dmelhor = d; pmelhor = pr; }
			}
		}
	}
	return melhor;
}

static void atacar(TzMundo *m, TzId i)
{
	TzObj *o = &m->obj[i];
	TzAtor *a = o->ator;
	TzId j = a->ordem.alvo;
	if (j < 0 || j >= m->nobj || !tz_obj_vivo(&m->obj[j]) || j == i || !o->ataque) { proxima(m, i); return; }
	/* lo automático no persigue más allá del doble de la vista */
	int alc = alcance_de(m, o);
	int d = tz_obj_dist(m, o, &m->obj[j]);
	if (a->automatica && d > 2 * (o->visao ? o->visao : 5) + alc) { proxima(m, i); return; }
	if (d <= alc && !esta_andando(o)) { a->ncaminho = 0; golpear(m, i, j); return; }
	if ((a->modo & TZ_MODO_QUIETO) && a->automatica) { proxima(m, i); return; }
	int r = mover_obj(m, i, j, alc);
	if (r < 0) proxima(m, i);
}

static int procurar_e_atacar(TzMundo *m, TzId i, int raio)
{
	TzObj *o = &m->obj[i];
	TzId j = buscar_inimigo(m, i, raio);
	if (j < 0) return 0;
	TzOrdem ord; memset(&ord, 0, sizeof ord);
	ord.tipo = TZ_O_ATACAR; ord.alvo = j;
	TzOrdem antes = o->ator->ordem;
	iniciar(m, o, &ord);
	o->ator->automatica = 1;
	if (antes.tipo != TZ_O_NADA && antes.tipo != TZ_O_ATACAR) o->ator->retomar = antes;
	return 1;
}

/* ------------------------------------------------------------------ economía */
static int rendimento(const TzMundo *m, int k, int rec)
{
	static const char *const te[4] = { "te_goldplus", "te_stoneplus", "te_fields", "te_woodplus" };
	return 10 + 2 * tz_pesquisas(m, k, te[rec]) + (rec == TZ_COMIDA ? 4 * tz_pesquisas(m, k, "te_irrigation") : 0);
}

/* gameplay.rs store_accepts */
static int aceita(const TzObj *s, int rec, int naval)
{
	switch (s->classe) {
	case TZC_CASTLE: return !naval;
	case TZC_STORAGE: return !naval && (rec == TZ_OURO || rec == TZ_PEDRA);
	case TZC_LUMBER: return !naval && rec == TZ_MADEIRA;
	case TZC_FARM: return !naval && rec == TZ_COMIDA;
	case TZC_HARBOR: return naval;
	}
	return 0;
}

static TzId armazem(TzMundo *m, TzId i)
{
	TzObj *o = &m->obj[i];
	int naval = tz_obj_at(m, o)->naval, melhor = -1, dm = 1 << 30;
	for (int j = 0; j < m->nobj; ++j) {
		const TzObj *s = &m->obj[j];
		if (!tz_obj_edificio(s) || !tz_obj_vivo(s) || (s->flags & TZ_F_OBRA)) continue;
		if (s->dono != o->dono && !(s->dono >= 1 && s->dono <= 8 && (m->reino[s->dono].diplomacia[o->dono] & TZ_DIP_RECURSOS))) continue;
		if (!aceita(s, o->ator->carga_tipo, naval)) continue;
		int d = tz_obj_dist(m, o, s);
		if (d < dm) { dm = d; melhor = j; }
	}
	return melhor;
}

/* deja el trabajo para llevar la carga; 1 si se fue a entregar */
static int ir_entregar(TzMundo *m, TzId i)
{
	TzObj *o = &m->obj[i];
	TzAtor *a = o->ator;
	TzId s = armazem(m, i);
	if (s < 0) { estado(m, o, TZ_E_PARADO, 0); return 0; }
	TzOrdem ent; memset(&ent, 0, sizeof ent);
	ent.tipo = TZ_O_ENTREGAR; ent.alvo = s;
	a->retomar = a->ordem;
	TzOrdem ret = a->retomar;
	iniciar(m, o, &ent);
	a->retomar = ret;
	return 1;
}

static void entregar(TzMundo *m, TzId i)
{
	TzObj *o = &m->obj[i];
	TzAtor *a = o->ator;
	TzId s = a->ordem.alvo;
	if (!a->carga) { proxima(m, i); return; }
	if (s < 0 || !tz_obj_vivo(&m->obj[s]) || !aceita(&m->obj[s], a->carga_tipo, tz_obj_at(m, o)->naval)) {
		s = armazem(m, i);
		if (s < 0) { a->retomar.tipo = TZ_O_NADA; proxima(m, i); return; }
		a->ordem.alvo = s;
	}
	int r = mover_obj(m, i, s, 1);
	if (r < 0) { proxima(m, i); return; }
	if (r == 0) return;
	int k = m->obj[s].dono;
	if (k >= 1 && k <= 8) {
		m->reino[k].recursos[a->carga_tipo] += a->carga;
		m->reino[k].recolhido[a->carga_tipo] += a->carga;
	}
	tz_ev(m, TZ_EV_RECURSO, o->dono, i, s, o->cx, o->cy, a->carga_tipo, a->carga);
	a->carga = 0;
	proxima(m, i);
}

static void carregar(TzObj *o, int rec, int qt, TzId de)
{
	TzAtor *a = o->ator;
	if (a->carga && a->carga_tipo != rec) a->carga = 0;   /* otra carga se tira (0x4ae2a0) */
	a->carga_tipo = (uint8_t)rec;
	a->carga = (uint16_t)(qt > 65535 ? 65535 : qt);
	a->origem = de;
}

/* el árbol más cercano (gameplay.rs next_tree): en espiral, hasta 12 casillas */
static int arvore_perto(const TzMundo *m, int x0, int y0, int *x, int *y)
{
	for (int d = 1; d <= 12; ++d)
		for (int dy = -d; dy <= d; ++dy) for (int dx = -d; dx <= d; ++dx) {
			if (dx != -d && dx != d && dy != -d && dy != d) continue;
			int xx = x0 + dx, yy = y0 + dy;
			if (xx < 0 || yy < 0 || xx >= m->lado || yy >= m->lado) continue;
			if (m->cel[yy * m->lado + xx].bits & TZ_CEL_ARVORE) { *x = xx; *y = yy; return 1; }
		}
	return 0;
}

static void cortar(TzMundo *m, TzId i)
{
	TzObj *o = &m->obj[i];
	TzAtor *a = o->ator;
	int x = a->ordem.x, y = a->ordem.y, n = m->lado;
	if (a->carga >= 10 && a->carga_tipo == TZ_MADEIRA) { if (!ir_entregar(m, i)) proxima(m, i); return; }
	if (x < 0 || y < 0 || x >= n || y >= n || !(m->cel[y * n + x].bits & TZ_CEL_ARVORE)) {
		if (esta_andando(o)) { avancar_passo(m, o); return; }
		if (!arvore_perto(m, x >= 0 ? x : o->cx, y >= 0 ? y : o->cy, &x, &y)) { proxima(m, i); return; }
		a->ordem.x = (int16_t)x; a->ordem.y = (int16_t)y;
		a->ncaminho = 0;
	}
	int r = mover_cel(m, i, x, y, 1);
	if (r < 0) {
		/* inalcanzable: otro árbol */
		m->cel[y * n + x].ilha = m->cel[y * n + x].ilha;   /* (sin marca; se buscará otro más tarde) */
		int nx, ny;
		if (arvore_perto(m, o->cx, o->cy, &nx, &ny) && (nx != x || ny != y)) { a->ordem.x = (int16_t)nx; a->ordem.y = (int16_t)ny; a->falhas = 0; return; }
		proxima(m, i); return;
	}
	if (r == 0) return;
	const TzAtributos *at = tz_obj_at(m, o);
	if (o->estado != TZ_E_TRABALHANDO || a->trabalho != TZ_TRAB_CORTAR) {
		estado(m, o, TZ_E_TRABALHANDO, TZ_TRAB_CORTAR);
		olhar(o, x, y);
		a->trabalho_tic = 1 + (6u * at->cortar_ms + TZ_TIC_MS - 1) / TZ_TIC_MS;   /* 0x4ad1cc: 50 ms y seis ciclos */
		return;
	}
	if (--a->trabalho_tic) return;
	TzCelula *c = &m->cel[y * n + x];
	c->madeira = c->madeira > 10 ? (uint16_t)(c->madeira - 10) : 0;
	carregar(o, TZ_MADEIRA, rendimento(m, o->dono, TZ_MADEIRA), TZ_NENHUM);
	if (!c->madeira) {
		c->bits &= (uint16_t)~TZ_CEL_ARVORE;
		/* tocón: el dibujo de árbol 1 (si hay) */
		m->casillas[y * n + x] = (uint16_t)((m->casillas[y * n + x] & 0x07ff) | (1u << 11));
		((TzPriv *)m->priv)->ilhas_sujas = 1;
		tz_ev(m, TZ_EV_ESGOTADO, o->dono, i, TZ_NENHUM, x, y, 0, 0);
	}
	ir_entregar(m, i);
}

static TzId campo_livre(TzMundo *m, TzId i)
{
	TzObj *o = &m->obj[i];
	int melhor = -1, dm = 21;
	/* los campos a 20 casillas o menos; luego, de esos, el que nadie trabaja */
	TzId cand[64];
	int nc = 0;
	for (int j = m->nobj - 1; j >= 0 && nc < 64; --j) {
		const TzObj *f = &m->obj[j];
		if (f->classe != TZC_FIELD || !tz_obj_vivo(f) || (f->flags & TZ_F_OBRA) || tz_mundo_relacao(m, o->dono, f->dono) < 1) continue;
		if (tz_obj_dist(m, o, f) < dm) cand[nc++] = j;
	}
	if (!nc) return TZ_NENHUM;
	uint8_t ocupado[64] = { 0 };
	for (int w = 0; w < m->nobj; ++w) {
		const TzObj *q = &m->obj[w];
		if (w == i || !q->ator || q->ator->ordem.tipo != TZ_O_RECOLHER || !tz_obj_vivo(q)) continue;
		for (int k = 0; k < nc; ++k) if (q->ator->ordem.alvo == cand[k]) ocupado[k] = 1;
	}
	for (int k = 0; k < nc; ++k) {
		if (ocupado[k]) continue;
		int d = tz_obj_dist(m, o, &m->obj[cand[k]]);
		if (d < dm) { dm = d; melhor = cand[k]; }
	}
	return melhor;
}

static void recolher(TzMundo *m, TzId i)
{
	TzObj *o = &m->obj[i];
	TzAtor *a = o->ator;
	TzId j = a->ordem.alvo;
	if (j < 0 || j >= m->nobj || !tz_obj_vivo(&m->obj[j])) {
		if (a->carga) { a->ordem.tipo = TZ_O_NADA; if (!ir_entregar(m, i)) proxima(m, i); return; }
		proxima(m, i); return;
	}
	TzObj *t = &m->obj[j];
	const TzAtributos *at = tz_obj_at(m, o);
	int mina = t->classe == TZC_RES_GOLD || t->classe == TZC_RES_STONE;
	int campo = t->classe == TZC_FIELD, vaca = t->classe == TZC_COW, peixe = t->classe == TZC_FISHPATCH, monte = t->classe == 80;
	if (!mina && !campo && !vaca && !peixe && !monte) { proxima(m, i); return; }
	int rec = mina ? (t->classe == TZC_RES_GOLD ? TZ_OURO : TZ_PEDRA) : TZ_COMIDA;
	if (monte) rec = (t->original && t->original->ndados > 60) ? t->original->dados[60] & 3 : TZ_OURO;
	/* lleno: a entregar */
	if (a->carga && ((peixe && a->carga >= 50) || (!peixe && !vaca && a->carga_tipo == rec && a->carga >= 10) || (vaca && a->carga >= 30))) {
		if (!ir_entregar(m, i)) proxima(m, i);
		return;
	}
	int r;
	if (campo) {
		/* al centro del campo (Dig, 0x42e600) */
		int cx = t->cx + 1, cy = t->cy + 1;
		/* 0x4acff7: el campo que trabaja otro peón no vale; se busca otro */
		TzId oc = m->cel[cy * m->lado + cx].ocupante;
		if (oc >= 0 && oc != i && m->obj[oc].ator && m->obj[oc].ator->ordem.tipo == TZ_O_RECOLHER && m->obj[oc].ator->ordem.alvo == j) {
			TzId f = campo_livre(m, i);
			if (f >= 0 && f != j) { a->ordem.alvo = f; a->ncaminho = 0; a->falhas = 0; return; }
			proxima(m, i); return;
		}
		r = (o->cx == cx && o->cy == cy) ? (esta_andando(o) ? (avancar_passo(m, o), 0) : 1) : mover_cel(m, i, cx, cy, 0);
		if (r < 0 && tz_obj_dist(m, o, t) == 0) r = 1;
		if (r < 0) { TzId f = campo_livre(m, i); if (f >= 0 && f != j) { a->ordem.alvo = f; a->falhas = 0; return; } }
	} else {
		r = mover_obj(m, i, j, 1);
	}
	if (r < 0) { proxima(m, i); return; }
	if (r == 0) return;
	int trab = mina ? TZ_TRAB_MINAR : vaca ? TZ_TRAB_ORDENHAR : peixe ? TZ_TRAB_PESCAR : TZ_TRAB_CAMPO;
	if (o->estado != TZ_E_TRABALHANDO || a->trabalho != trab) {
		estado(m, o, TZ_E_TRABALHANDO, trab);
		olhar_obj(m, o, t);
		uint32_t ms = peixe ? 1400 : mina ? 50 + 6u * at->minar_ms : vaca ? at->ordenhar_ms : campo ? at->cavar_ms : at->cavar_ms;
		a->trabalho_tic = (ms + TZ_TIC_MS - 1) / TZ_TIC_MS;
		if (!a->trabalho_tic) a->trabalho_tic = 1;
		if (campo && !a->ciclos) a->ciclos = (uint16_t)(12 + tz_rng_entre(&m->rng, 0, 6) + 1);   /* 0x4ad088 */
		return;
	}
	if (--a->trabalho_tic) return;
	uint32_t ms = peixe ? 1400 : mina ? 6u * at->minar_ms : vaca ? at->ordenhar_ms : at->cavar_ms;
	a->trabalho_tic = (ms + TZ_TIC_MS - 1) / TZ_TIC_MS;
	if (!a->trabalho_tic) a->trabalho_tic = 1;
	if (peixe) {
		uint32_t want = 4, falta = 50 - (a->carga_tipo == TZ_COMIDA ? a->carga : 0);
		if (want > falta) want = falta;
		if (want > t->recurso) want = t->recurso;
		t->recurso -= want;
		carregar(o, TZ_COMIDA, (a->carga_tipo == TZ_COMIDA ? a->carga : 0) + (int)want, j);
		if (!t->recurso) { tz_ev(m, TZ_EV_ESGOTADO, o->dono, j, TZ_NENHUM, t->cx, t->cy, t->classe, 0); tz_mundo_guiao_remover(m, j); }
		return;
	}
	if (vaca) {
		/* 0x4ac048: hasta tres por ordeño */
		uint32_t q = t->mana < 3 ? t->mana : 3;
		t->mana = (uint16_t)(t->mana - q);
		carregar(o, TZ_COMIDA, (a->carga_tipo == TZ_COMIDA ? a->carga : 0) + (int)q, j);
		if (!t->mana) { if (a->carga) ir_entregar(m, i); else proxima(m, i); }
		return;
	}
	if (campo) {
		/* 0x44cc70: el campo crece de 10 en 10; acabada la cuenta, da comida */
		if (t->vida < t->vida_max) { t->vida += 10; if (t->vida > t->vida_max) t->vida = t->vida_max; }
		if (t->vida >= t->vida_max && a->ciclos) a->ciclos--;
		if (a->ciclos <= 1) {
			a->ciclos = 0;
			carregar(o, TZ_COMIDA, rendimento(m, o->dono, TZ_COMIDA), j);
			ir_entregar(m, i);
		}
		return;
	}
	if (monte) {
		carregar(o, rec, (int)(t->recurso ? t->recurso : 100), j);
		tz_mundo_guiao_remover(m, j);
		ir_entregar(m, i);
		return;
	}
	/* mina */
	uint32_t quer = (uint32_t)rendimento(m, o->dono, rec);
	int pct = tz_pesquisas(m, o->dono, "te_orefloatation") ? 50 : 100;
	uint32_t gasto = quer * (uint32_t)pct / 100;
	if (t->recurso != 0xffffffffu) { if (gasto > t->recurso) gasto = t->recurso; t->recurso -= gasto; }
	carregar(o, rec, (int)(gasto * 100 / (uint32_t)pct), j);
	if (o->dono >= 1 && o->dono <= 8) m->reino[o->dono].pontos += a->carga;
	if (!t->recurso) {
		tz_marcar(m, j, 0);
		t->flags |= TZ_F_MORTO;
		tz_ev(m, TZ_EV_ESGOTADO, o->dono, j, TZ_NENHUM, t->cx, t->cy, t->classe, 0);
	}
	ir_entregar(m, i);
}

/* ------------------------------------------------------------------ construir y reparar (0x4aba10) */
void tz_obra_pronta(TzMundo *m, TzId j)
{
	TzObj *t = &m->obj[j];
	t->flags &= ~(uint32_t)TZ_F_OBRA;
	t->vida = t->vida_max;
	t->anim_tic = m->tic;
	if (t->ator) t->ator->obra_ms = 0;
	if (t->dono >= 1 && t->dono <= 8) m->reino[t->dono].pontos += tz_obj_at(m, t)->vida / 10;
	tz_ev(m, TZ_EV_OBRA_FIM, t->dono, j, TZ_NENHUM, t->cx, t->cy, t->classe, 1);
	tz_ev(m, TZ_EV_CRIADO, t->dono, j, TZ_NENHUM, t->cx, t->cy, t->classe, 1);
}

static TzId obra_perto(TzMundo *m, TzId i, int reparar)
{
	TzObj *o = &m->obj[i];
	int alc = 2 * (o->visao ? o->visao : 5), melhor = -1, dm = 1 << 30;
	for (int j = 0; j < m->nobj; ++j) {
		const TzObj *t = &m->obj[j];
		if (!tz_obj_edificio(t) || !tz_obj_vivo(t) || tz_mundo_relacao(m, o->dono, t->dono) < 1) continue;
		if (reparar ? ((t->flags & TZ_F_OBRA) || t->vida >= t->vida_max) : !(t->flags & TZ_F_OBRA)) continue;
		int d = tz_obj_dist(m, o, t);
		if (d <= alc && d < dm) { dm = d; melhor = j; }
	}
	return melhor;
}

static void construir(TzMundo *m, TzId i, int reparar)
{
	TzObj *o = &m->obj[i];
	TzAtor *a = o->ator;
	TzId j = a->ordem.alvo;
	TzObj *t = j >= 0 && j < m->nobj ? &m->obj[j] : NULL;
	int acabou = !t || !tz_obj_vivo(t) || (!reparar && !(t->flags & TZ_F_OBRA)) ||
	             (reparar && !(t->flags & TZ_F_OBRA) && t->vida >= t->vida_max);
	if (acabou) {
		if (esta_andando(o)) { avancar_passo(m, o); return; }
		TzId n = obra_perto(m, i, reparar);
		if (n >= 0 && a->nfila == 0) { a->ordem.alvo = n; a->ncaminho = 0; a->falhas = 0; return; }
		proxima(m, i); return;
	}
	int r = mover_obj(m, i, j, 1);
	if (r < 0) { proxima(m, i); return; }
	if (r == 0) return;
	const TzAtributos *at = tz_obj_at(m, o);
	uint32_t ciclo = at->construir_ms ? at->construir_ms : 100;
	if (o->estado != TZ_E_CONSTRUINDO) {
		estado(m, o, TZ_E_CONSTRUINDO, reparar ? TZ_TRAB_REPARAR : TZ_TRAB_CONSTRUIR);
		olhar_obj(m, o, t);
		a->trabalho_tic = (ciclo + TZ_TIC_MS - 1) / TZ_TIC_MS;
		return;
	}
	if (a->trabalho_tic > 1) { a->trabalho_tic--; return; }
	a->trabalho_tic = (ciclo + TZ_TIC_MS - 1) / TZ_TIC_MS;
	if (!a->trabalho_tic) a->trabalho_tic = 1;
	TzAtor *ta = t->ator;
	if ((t->flags & TZ_F_OBRA) && ta) {
		uint32_t golpe = a->trabalho_tic * TZ_TIC_MS;
		ta->obra_ms = ta->obra_ms > golpe ? ta->obra_ms - golpe : 0;
		uint64_t feito = ta->obra_total_ms - ta->obra_ms;
		t->vida = (int32_t)((uint64_t)t->vida_max * feito / (ta->obra_total_ms ? ta->obra_total_ms : 1));
		if (t->vida < 1) t->vida = 1;
		if (!ta->obra_ms) tz_obra_pronta(m, j);
		return;
	}
	if (tz_obj_unidade(t)) { t->vida += 5; }
	else {
		/* 0x4abe66: max(2, ciclo * vida_max / (TIME * 4)) */
		uint32_t tempo = tz_obj_at(m, t)->tempo_ms ? tz_obj_at(m, t)->tempo_ms : 1;
		uint64_t q = (uint64_t)a->trabalho_tic * TZ_TIC_MS * (uint64_t)t->vida_max / ((uint64_t)tempo * 4);
		t->vida += (int32_t)(q < 2 ? 2 : q);
	}
	if (t->vida >= t->vida_max) { t->vida = t->vida_max; tz_ev(m, TZ_EV_OBRA_FIM, t->dono, j, i, t->cx, t->cy, t->classe, 0); }
}

/* captura: cada ciclo quita 5 de resistencia al edificio; a cero cambia de dueño (gameplay.rs Capture) */
static void capturar(TzMundo *m, TzId i)
{
	TzObj *o = &m->obj[i];
	TzAtor *a = o->ator;
	TzId j = a->ordem.alvo;
	if (j < 0 || j >= m->nobj || !tz_obj_vivo(&m->obj[j]) || m->obj[j].dono == o->dono || !tz_obj_edificio(&m->obj[j])) { proxima(m, i); return; }
	TzObj *t = &m->obj[j];
	int r = mover_obj(m, i, j, 1);
	if (r < 0) { proxima(m, i); return; }
	if (r == 0) return;
	const TzAtributos *at = tz_obj_at(m, o);
	if (o->estado != TZ_E_TRABALHANDO || a->trabalho != TZ_TRAB_CAPTURAR) {
		estado(m, o, TZ_E_TRABALHANDO, TZ_TRAB_CAPTURAR);
		olhar_obj(m, o, t);
		a->trabalho_tic = (at->cavar_ms + TZ_TIC_MS - 1) / TZ_TIC_MS + 1;
		return;
	}
	if (--a->trabalho_tic) return;
	a->trabalho_tic = (at->cavar_ms + TZ_TIC_MS - 1) / TZ_TIC_MS + 1;
	if (t->resistencia > 5) { t->resistencia = (uint16_t)(t->resistencia - 5); if (t->ator) t->ator->ultimo_golpe = m->tic; return; }
	t->resistencia = 0;
	tz_trocar_dono(m, j, o->dono);
	proxima(m, i);
}

/* ------------------------------------------------------------------ entrar, salir */
static void entrar(TzMundo *m, TzId i)
{
	TzObj *o = &m->obj[i];
	TzAtor *a = o->ator;
	TzId j = a->ordem.alvo;
	if (j < 0 || j >= m->nobj || !tz_obj_vivo(&m->obj[j]) || tz_mundo_relacao(m, o->dono, m->obj[j].dono) < 1) { proxima(m, i); return; }
	TzObj *t = &m->obj[j];
	const TzAtributos *ct = tz_obj_at(m, t);
	if (!ct->capacidade || t->ndentro >= ct->capacidade || (t->flags & TZ_F_OBRA)) { proxima(m, i); return; }
	int r = mover_obj(m, i, j, 1);
	if (r < 0) { proxima(m, i); return; }
	if (r == 0) return;
	tz_marcar(m, i, 0);
	o->flags |= TZ_F_DENTRO;
	o->dentro_de = j;
	t->ndentro++;
	tz_ev(m, TZ_EV_REMOVIDO, o->dono, i, j, o->cx, o->cy, o->classe, 1);
	tz_parar(m, i);
}

static void descarregar(TzMundo *m, TzId j)
{
	TzObj *t = &m->obj[j];
	int r[4];
	tz_obj_ret(m, t, r);
	for (int i = 0; i < m->nobj; ++i) {
		TzObj *q = &m->obj[i];
		if (q->dentro_de != j) continue;
		int x, y;
		if (!tz_lugar_livre(m, tz_obj_capa(m, q) == TZ_CAPA_AR ? TZ_CAPA_AR : TZ_CAPA_TERRA, q->dono, r, &x, &y)) break;
		q->flags &= ~(uint32_t)TZ_F_DENTRO;
		q->dentro_de = TZ_NENHUM;
		q->cx = (int16_t)x; q->cy = (int16_t)y;
		q->px = x * TZ_SUB + TZ_SUB / 2; q->py = y * TZ_SUB + TZ_SUB / 2;
		tz_marcar(m, i, 1);
		if (t->ndentro) t->ndentro--;
		tz_ev(m, TZ_EV_CRIADO, q->dono, i, j, x, y, q->classe, 0);
	}
}

/* ------------------------------------------------------------------ cada unidad, cada tic */
void tz_ator_tic(TzMundo *m, TzId i)
{
	TzObj *o = &m->obj[i];
	TzAtor *a = o->ator;
	/* buscar enemigo: las unidades armadas sin órdenes (o de guardia/patrulla), cada 8 tics, escalonado */
	if (o->ataque && o->classe != TZC_PEON && o->classe != TZC_TRADER && !(a->modo & TZ_MODO_QUIETO) &&
	    (a->ordem.tipo == TZ_O_NADA || a->ordem.tipo == TZ_O_PATRULHAR || a->ordem.tipo == TZ_O_GUARDAR) &&
	    ((m->tic + (uint32_t)i) & 7) == 0 && o->dono != 0 && !esta_andando(o)) {
		int raio = o->visao ? o->visao : 5;
		if (alcance_de(m, o) > raio) raio = alcance_de(m, o);
		procurar_e_atacar(m, i, raio);
	}
	/* efectos: hielo y piedra no dejan hacer nada; al acabarse, los atributos vuelven */
	if (a->efeito && m->tic >= a->efeito_ate) { a->efeito = TZ_EF_NADA; tz_recalcular(m, i); }
	if (a->efeito == TZ_EF_GELO || a->efeito == TZ_EF_PEDRA) return;
	/* un evento por golpe de trabajo (para el sonido: @chop, @mine, @dig, @build) */
	if ((o->estado == TZ_E_TRABALHANDO || o->estado == TZ_E_CONSTRUINDO) && a->trabalho) {
		const TzAtributos *at = tz_obj_at(m, o);
		uint32_t ms = a->trabalho == TZ_TRAB_CORTAR ? at->cortar_ms : a->trabalho == TZ_TRAB_MINAR ? at->minar_ms :
		              a->trabalho == TZ_TRAB_ORDENHAR ? at->ordenhar_ms : a->trabalho == TZ_TRAB_PESCAR ? 1400 :
		              (a->trabalho == TZ_TRAB_CONSTRUIR || a->trabalho == TZ_TRAB_REPARAR) ? at->construir_ms : at->cavar_ms;
		uint32_t ct = (ms + TZ_TIC_MS - 1) / TZ_TIC_MS;
		if (!ct) ct = 1;
		if (m->tic > o->anim_tic && (m->tic - o->anim_tic) % ct == 0) tz_ev(m, TZ_EV_TRABALHO, o->dono, i, TZ_NENHUM, o->cx, o->cy, o->classe, a->trabalho);
	}
	/* maná: un punto por segundo (gameplay.rs mana_regeneration, simplificado) */
	if (o->mana < o->mana_max && o->classe != TZC_COW && m->tic % TZ_TICS_SEG == (uint32_t)i % TZ_TICS_SEG) o->mana++;
	switch (a->ordem.tipo) {
	case TZ_O_NADA:
		if (avancar_passo(m, o)) return;
		if (o->estado != TZ_E_PARADO) estado(m, o, TZ_E_PARADO, 0);
		/* las vacas recuperan la leche */
		if (o->classe == TZC_COW && o->mana < o->mana_max && m->tic % TZ_TICS_SEG == 0) o->mana++;
		return;
	case TZ_O_MOVER: case TZ_O_EXPLORAR: case TZ_O_FUGIR: {
		int r = mover_cel(m, i, a->ordem.x, a->ordem.y, 0);
		if (r != 0) proxima(m, i);
		return;
	}
	case TZ_O_SEGUIR: {
		TzId j = a->ordem.alvo;
		if (j < 0 || j >= m->nobj || !tz_obj_vivo(&m->obj[j])) { proxima(m, i); return; }
		if (mover_obj(m, i, j, 1) && !esta_andando(o) && o->estado != TZ_E_PARADO) estado(m, o, TZ_E_PARADO, 0);
		return;
	}
	case TZ_O_GUARDAR: {
		TzId j = a->ordem.alvo;
		if (j < 0 || j >= m->nobj || !tz_obj_vivo(&m->obj[j])) { proxima(m, i); return; }
		if (mover_obj(m, i, j, 2) && !esta_andando(o) && o->estado != TZ_E_PARADO) estado(m, o, TZ_E_PARADO, 0);
		return;
	}
	case TZ_O_PATRULHAR: {
		int r = mover_cel(m, i, a->ordem.x, a->ordem.y, 0);
		if (r != 0) {
			int16_t tx = a->ordem.x, ty = a->ordem.y;
			a->ordem.x = a->ordem.x2; a->ordem.y = a->ordem.y2;
			a->ordem.x2 = tx; a->ordem.y2 = ty;
			a->ncaminho = 0; a->falhas = 0;
		}
		return;
	}
	case TZ_O_ATACAR: atacar(m, i); return;
	case TZ_O_ATACAR_CHAO: {
		int alc = alcance_de(m, o);
		int r[4] = { a->ordem.x, a->ordem.y, a->ordem.x, a->ordem.y };
		if (tz_dist_ret(o->cx, o->cy, r) > alc || esta_andando(o)) { if (mover_para(m, i, r, alc) < 0) proxima(m, i); return; }
		TzId alvo = tz_mundo_em(m, a->ordem.x, a->ordem.y);
		if (alvo >= 0 && alvo != i) golpear(m, i, alvo);
		else { estado(m, o, TZ_E_ATACANDO, 0); olhar(o, a->ordem.x, a->ordem.y); }
		return;
	}
	case TZ_O_RECOLHER: recolher(m, i); return;
	case TZ_O_HECHIZO: if (o->mana_max) tz_hechizo_tic(m, i); else proxima(m, i); if (a->ordem.tipo == TZ_O_NADA) proxima(m, i); return;
	case TZ_O_CORTAR: cortar(m, i); return;
	case TZ_O_ENTREGAR: entregar(m, i); return;
	case TZ_O_CONSTRUIR: construir(m, i, 0); return;
	case TZ_O_REPARAR: construir(m, i, 1); return;
	case TZ_O_CAPTURAR: capturar(m, i); return;
	case TZ_O_ENTRAR: entrar(m, i); return;
	case TZ_O_DESCARREGAR: {
		if (tz_obj_at(m, o)->naval) {
			/* barco: a la costa más cercana al punto y suelta a todos */
			int r = mover_cel(m, i, a->ordem.x, a->ordem.y, 1);
			if (r != 0) { descarregar(m, i); proxima(m, i); }
		} else { descarregar(m, i); proxima(m, i); }
		return;
	}
	default:
		/* órdenes aún sin programa (comerciar, objetos, hechizos): se descartan */
		proxima(m, i);
		return;
	}
}

/* ------------------------------------------------------------------ edificios: producción, torres */
static int dono_ok(const TzMundo *m, int k) { return k >= 1 && k <= 8 && m->reino[k].ativo; }

static int pagar(TzMundo *m, int k, const uint32_t c[TZ_NREC])
{
	TzReinoEstado *r = &m->reino[k];
	for (int j = 0; j < TZ_NREC; ++j)
		if ((uint32_t)(r->recursos[j] < 0 ? 0 : r->recursos[j]) < c[j]) {
			tz_ev(m, TZ_EV_SEM_RECURSOS, k, TZ_NENHUM, TZ_NENHUM, 0, 0, j, (int32_t)c[j]);
			return tz_falha("faltan recursos");
		}
	for (int j = 0; j < TZ_NREC; ++j) r->recursos[j] -= (int32_t)c[j];
	return 0;
}

int tz_enfileirar_trabalho(TzMundo *m, TzId e, int tipo, int arg)
{
	TzObj *o = &m->obj[e];
	TzAtor *a = o->ator;
	if (!a || !dono_ok(m, o->dono)) return -1;
	if (a->ntrab >= TZ_FILA_TRABALHO) return tz_falha("la cola está llena");
	uint32_t c[TZ_NREC], t;
	if (tipo == TZ_J_TREINAR) tz_mundo_custo_classe(m, o->dono, arg, c, &t);
	else tz_mundo_custo_tec(m, o->dono, arg, c, &t);
	if (tipo != TZ_J_TREINAR) {
		/* una investigación no se pone dos veces a la vez */
		for (int q = 0; q < m->nobj; ++q) {
			const TzObj *b = &m->obj[q];
			if (b->dono != o->dono || !b->ator) continue;
			for (int w = 0; w < b->ator->ntrab; ++w) if (b->ator->trab[w].tipo != TZ_J_TREINAR && b->ator->trab[w].arg == arg) return tz_falha("ya se está investigando");
		}
	}
	if (pagar(m, o->dono, c)) return -1;
	TzTrabalho *j = &a->trab[a->ntrab++];
	memset(j, 0, sizeof *j);
	j->tipo = (uint8_t)tipo; j->arg = (int16_t)arg;
	j->total_ms = j->restante_ms = t ? t : 1000;
	memcpy(j->pago, c, sizeof c);
	return 0;
}

void tz_cancelar_trabalho(TzMundo *m, TzId e, int idx)
{
	TzObj *o = &m->obj[e];
	TzAtor *a = o->ator;
	if (!a || !a->ntrab) return;
	if (idx < 0 || idx >= a->ntrab) idx = a->ntrab - 1;
	if (dono_ok(m, o->dono)) for (int j = 0; j < TZ_NREC; ++j) m->reino[o->dono].recursos[j] += (int32_t)a->trab[idx].pago[j];
	memmove(&a->trab[idx], &a->trab[idx + 1], (size_t)(a->ntrab - idx - 1) * sizeof *a->trab);
	a->ntrab--;
}

static void treinado(TzMundo *m, TzId e, int classe)
{
	TzObj *o = &m->obj[e];
	TzObj tmp; memset(&tmp, 0, sizeof tmp); tmp.classe = (uint16_t)classe; tmp.civ = o->civ;
	int r[4], x, y;
	tz_obj_ret(m, o, r);
	int capa = tz_obj_capa(m, &tmp);
	if (!tz_lugar_livre(m, capa, o->dono, r, &x, &y)) return;
	TzId u = tz_novo_obj(m, classe, o->civ, o->dono, x, y, 0);
	if (u < 0) return;
	o = &m->obj[e];   /* tz_novo_obj puede mover m->obj */
	m->obj[u].direcao = 0;
	if (dono_ok(m, o->dono)) { m->reino[o->dono].criadas++; m->reino[o->dono].pontos += 5; }
	tz_ev(m, TZ_EV_TREINADO, o->dono, u, e, x, y, classe, 0);
	tz_ev(m, TZ_EV_CRIADO, o->dono, u, e, x, y, classe, 0);
	TzAtor *a = o->ator;
	if (a->reuniao[0] >= 0 && m->obj[u].ator) {
		TzOrdem ord; memset(&ord, 0, sizeof ord);
		ord.alvo = TZ_NENHUM; ord.x = a->reuniao[0]; ord.y = a->reuniao[1];
		ord.tipo = TZ_O_MOVER;
		if (classe == TZC_PEON) {
			TzId t = tz_mundo_em(m, ord.x, ord.y);
			if (t >= 0) {
				const TzObj *to = &m->obj[t];
				if (to->classe == TZC_RES_GOLD || to->classe == TZC_RES_STONE || to->classe == TZC_FIELD) { ord.tipo = TZ_O_RECOLHER; ord.alvo = t; }
			} else if (m->cel[ord.y * m->lado + ord.x].bits & TZ_CEL_ARVORE) ord.tipo = TZ_O_CORTAR;
		}
		tz_dar_ordem(m, u, &ord, 0);
	}
}

/* UPGRADE <clase>: el edificio cambia de clase (torres, muros, puertas) */
static void melhorar(TzMundo *m, TzId e, int tec)
{
	TzObj *o = &m->obj[e];
	const char *nome = m->cat->tec[tec].nome;
	int nova = -1;
	for (int q = 0; q < m->cat->nord; ++q) {
		const TzOrdemDef *d = &m->cat->ord[q];
		if (!strcasecmp(d->classe, nome) && !strncmp(d->acao, "upgrade ", 8)) { nova = tz_catalogo_classe(d->acao + 8); break; }
	}
	if (nova < 0 || nova == o->classe) return;
	tz_marcar(m, e, 0);
	int32_t vm = o->vida_max;
	o->classe = (uint16_t)nova;
	const TzAtributos *a = tz_obj_at(m, o);
	o->vida_max = a->vida ? a->vida : vm;
	o->vida = (int32_t)((int64_t)o->vida * o->vida_max / (vm ? vm : 1));
	if (o->vida < 1) o->vida = 1;
	o->alcance = a->alcance; o->visao = a->visao; o->resistencia = a->resistencia;
	tz_recalcular(m, e);
	tz_marcar(m, e, 1);
	tz_ev(m, TZ_EV_CRIADO, o->dono, e, TZ_NENHUM, o->cx, o->cy, nova, 2);
}

void tz_producao_tic(TzMundo *m, TzId e)
{
	TzObj *o = &m->obj[e];
	TzAtor *a = o->ator;
	if (o->flags & TZ_F_OBRA) return;
	const TzAtributos *at = tz_obj_at(m, o);
	/* puertas (gate_tests.rs): el puente baja cuando hay una unidad amiga a dos casillas o menos y sube después;
	 * ciclos = fila del puente levadizo (0 abierta, 6 cerrada), una fila cada 100 ms */
	if (o->classe == TZC_GATE_WOOD || o->classe == TZC_GATE_STONE) {
		int r[4], amiga = 0;
		tz_obj_ret(m, o, r);
		for (int y = r[1] - 2; y <= r[3] + 2 && !amiga; ++y)
			for (int x = r[0] - 2; x <= r[2] + 2; ++x) {
				if (x < 0 || y < 0 || x >= m->lado || y >= m->lado) continue;
				TzId q = m->cel[y * m->lado + x].ocupante;
				if (q >= 0 && tz_mundo_relacao(m, o->dono, m->obj[q].dono) > 0) { amiga = 1; break; }
			}
		int alvo = amiga ? 0 : 6;
		if (a->trabalho != (uint8_t)(amiga + 1)) { a->trabalho = (uint8_t)(amiga + 1); tz_ev(m, TZ_EV_PORTA, o->dono, e, TZ_NENHUM, o->cx, o->cy, o->classe, amiga); }
		if (a->ciclos != alvo && m->tic % 2 == 0) a->ciclos = (uint16_t)(a->ciclos < alvo ? a->ciclos + 1 : a->ciclos - 1);
	}
	/* guarnición (0x422a80, simplificado): los de dentro se curan 2 puntos por segundo */
	if (o->ndentro && m->tic % TZ_TICS_SEG == (uint32_t)e % TZ_TICS_SEG)
		for (int q = 0; q < m->nobj; ++q) {
			TzObj *u = &m->obj[q];
			if (u->dentro_de == e && u->vida > 0 && u->vida < u->vida_max) { u->vida += 2; if (u->vida > u->vida_max) u->vida = u->vida_max; }
		}
	/* resistencia: vuelve poco a poco si nadie la captura */
	if (o->resistencia < at->resistencia && m->tic - a->ultimo_golpe > 5u * TZ_TICS_SEG && m->tic % TZ_TICS_SEG == 0) {
		o->resistencia = (uint16_t)(o->resistencia + 5 > at->resistencia ? at->resistencia : o->resistencia + 5);
	}
	/* torres (y lo que tenga ataque y alcance): disparan a lo que entra */
	if (o->ataque && o->alcance && !(a->modo & TZ_MODO_SEM_FOGO)) {
		TzId j = a->ordem.alvo;
		if (a->ordem.tipo != TZ_O_ATACAR || j < 0 || !tz_obj_vivo(&m->obj[j]) || tz_obj_dist(m, o, &m->obj[j]) > o->alcance) {
			a->ordem.tipo = TZ_O_NADA;
			if (((m->tic + (uint32_t)e) & 7) == 0) {
				TzId n = buscar_inimigo(m, e, o->alcance);
				if (n >= 0) { a->ordem.tipo = TZ_O_ATACAR; a->ordem.alvo = n; a->recarga = 0; }
			}
		}
		if (a->ordem.tipo == TZ_O_ATACAR) {
			/* 0x4dbc43: cañón y cohetes no tiran a menos de dos casillas */
			if ((o->classe == 42 || o->classe == 43) && tz_obj_dist(m, o, &m->obj[a->ordem.alvo]) <= 2) a->ordem.tipo = TZ_O_NADA;
			else if (a->recarga > TZ_TIC_MS) a->recarga -= TZ_TIC_MS;
			else {
				a->recarga = at->ataque_ms ? at->ataque_ms : 1000;
				tz_ev(m, TZ_EV_ATACOU, o->dono, e, a->ordem.alvo, o->cx, o->cy, o->classe, 0);
				disparar(m, e, a->ordem.alvo);
			}
		}
	}
	if (!a->ntrab) return;
	TzTrabalho *j = &a->trab[0];
	if (j->tipo == TZ_J_TREINAR && j->arg != TZC_COW && dono_ok(m, o->dono)) {
		const TzReinoEstado *r = &m->reino[o->dono];
		if (r->populacao >= r->populacao_max) {
			if (!j->esperando) tz_ev(m, TZ_EV_SEM_POPULACAO, o->dono, e, TZ_NENHUM, o->cx, o->cy, j->arg, 0);
			j->esperando = 1;
			return;
		}
	}
	j->esperando = 0;
	if (j->restante_ms > TZ_TIC_MS) { j->restante_ms -= TZ_TIC_MS; return; }
	TzTrabalho feito = *j;
	memmove(&a->trab[0], &a->trab[1], (size_t)(a->ntrab - 1) * sizeof *a->trab);
	a->ntrab--;
	if (feito.tipo == TZ_J_TREINAR) {
		treinado(m, e, feito.arg);
		o = &m->obj[e];   /* treinado crea un objeto: m->obj puede moverse */
		/* la población cuenta ya la nueva (por si hay más en la cola este mismo tic) */
		if (dono_ok(m, o->dono)) m->reino[o->dono].populacao++;
	} else if (feito.tipo == TZ_J_MELHORAR) {
		if (dono_ok(m, o->dono)) tz_mundo_guiao_pesquisa(m, o->dono, feito.arg);
		melhorar(m, e, feito.arg);
	} else {
		if (dono_ok(m, o->dono)) tz_mundo_guiao_pesquisa(m, o->dono, feito.arg);
	}
}

/* ------------------------------------------------------------------ obras */
TzId tz_colocar_obra(TzMundo *m, int k, int classe, int cx, int cy)
{
	if (!tz_classe_e_edificio(classe)) { tz_falha("no es un edificio"); return TZ_NENHUM; }
	if (tz_mundo_cabe(m, k, classe, cx, cy)) return TZ_NENHUM;
	uint32_t c[TZ_NREC], t;
	tz_mundo_custo_classe(m, k, classe, c, &t);
	if (pagar(m, k, c)) return TZ_NENHUM;
	TzId j = tz_novo_obj(m, classe, m->reino[k].civ, k, cx, cy, 1);
	if (j >= 0) {
		if (m->obj[j].ator) m->obj[j].ator->obra_ms = m->obj[j].ator->obra_total_ms = t ? t : 1000;
		tz_ev(m, TZ_EV_OBRA_INICIO, k, j, TZ_NENHUM, cx, cy, classe, 0);
	}
	return j;
}

/* ------------------------------------------------------------------ comandos de los reinos */
/* la orden del menú del objeto cuya acción empieza por «prefixo» y nombra la clase o tecnología */
static const TzOrdemDef *no_menu(const TzMundo *m, const TzObj *o, const char *prefixo, const char *nome)
{
	const TzOrdemDef *menu[160];
	int n = tz_catalogo_menu(m->cat, o->classe, o->civ, menu, 160);
	size_t lp = strlen(prefixo);
	for (int q = 0; q < n; ++q)
		if (!strncmp(menu[q]->acao, prefixo, lp) && !strcasecmp(menu[q]->classe, nome)) return menu[q];
	return NULL;
}

static int meu(const TzMundo *m, const TzComando *c, TzId i)
{
	if (i < 0 || i >= m->nobj) return 0;
	const TzObj *o = &m->obj[i];
	return o->ator && tz_obj_vivo(o) && o->dono == c->reino && !(o->flags & TZ_F_BLOQUEADO);
}

/* formación: la k-ésima casilla libre en espiral alrededor del destino */
static void casilla_grupo(const TzMundo *m, int capa, int dono, int x, int y, int k, int *ox, int *oy)
{
	*ox = x; *oy = y;
	if (k == 0) return;
	int conta = 0;
	for (int d = 1; d < 12; ++d)
		for (int dy = -d; dy <= d; ++dy) for (int dx = -d; dx <= d; ++dx) {
			if (dx != -d && dx != d && dy != -d && dy != d) continue;
			if (!tz_cel_transitavel(m, capa, dono, x + dx, y + dy)) continue;
			if (++conta == k) { *ox = x + dx; *oy = y + dy; return; }
		}
}

void tz_aplicar_comando(TzMundo *m, const TzComando *c)
{
	int k = c->reino;
	if (k < 1 || k > 8 || m->reino[k].derrotado) return;
	switch (c->tipo) {
	case TZ_C_ORDEM: {
		TzOrdem ord = c->ordem;
		int primeiro = 1, nunid = 0;
		for (int q = 0; q < c->nids; ++q) {
			TzId i = c->ids[q];
			if (!meu(m, c, i)) continue;
			TzObj *o = &m->obj[i];
			if (tz_obj_edificio(o)) {
				if (ord.tipo == TZ_O_MOVER || ord.tipo == TZ_O_RECOLHER || ord.tipo == TZ_O_CORTAR) { o->ator->reuniao[0] = ord.x; o->ator->reuniao[1] = ord.y; }
				else if (ord.tipo == TZ_O_DESCARREGAR) descarregar(m, i);
				else if (ord.tipo == TZ_O_ATACAR && o->ataque && o->alcance) { o->ator->ordem.tipo = TZ_O_ATACAR; o->ator->ordem.alvo = ord.alvo; }
				continue;
			}
			TzOrdem u = ord;
			if ((u.tipo == TZ_O_MOVER || u.tipo == TZ_O_PATRULHAR) && u.alvo < 0) {
				int x, y;
				casilla_grupo(m, tz_obj_capa(m, o), o->dono, ord.x, ord.y, nunid, &x, &y);
				u.x = (int16_t)x; u.y = (int16_t)y;
				if (u.tipo == TZ_O_PATRULHAR) { u.x2 = o->cx; u.y2 = o->cy; }
			}
			/* los peones solo trabajan; los demás solo luchan */
			if (o->classe != TZC_PEON && (u.tipo == TZ_O_CONSTRUIR || u.tipo == TZ_O_REPARAR || u.tipo == TZ_O_RECOLHER || u.tipo == TZ_O_CORTAR || u.tipo == TZ_O_CAPTURAR)) {
				if (!(u.tipo == TZ_O_RECOLHER && o->classe == TZC_FISHING_BOAT)) { u.tipo = TZ_O_MOVER; u.alvo = TZ_NENHUM; }
			}
			nunid++;
			tz_dar_ordem(m, i, &u, c->flags & TZ_CMD_FILA);
			if (primeiro) { tz_ev(m, TZ_EV_ORDEM, k, i, u.alvo, u.x, u.y, o->classe, u.tipo); primeiro = 0; }
		}
		return;
	}
	case TZ_C_CONSTRUIR: {
		TzId peao = TZ_NENHUM;
		for (int q = 0; q < c->nids && peao < 0; ++q) if (meu(m, c, c->ids[q]) && no_menu(m, &m->obj[c->ids[q]], "build ", tz_classes[c->arg >= 0 && c->arg < TZ_NCLASSES ? c->arg : 0])) peao = c->ids[q];
		if (peao < 0) return;
		if (tz_disponivel(m, k, m->obj[peao].civ, tz_classes[c->arg], 0)) return;
		TzId j = tz_colocar_obra(m, k, c->arg, c->x, c->y);
		if (j < 0) return;
		TzOrdem ord; memset(&ord, 0, sizeof ord);
		ord.tipo = TZ_O_CONSTRUIR; ord.alvo = j;
		for (int q = 0; q < c->nids; ++q) if (meu(m, c, c->ids[q]) && m->obj[c->ids[q]].classe == TZC_PEON) tz_dar_ordem(m, c->ids[q], &ord, c->flags & TZ_CMD_FILA);
		tz_ev(m, TZ_EV_ORDEM, k, peao, j, c->x, c->y, m->obj[peao].classe, TZ_O_CONSTRUIR);
		return;
	}
	case TZ_C_PRODUZIR: case TZ_C_PESQUISAR: {
		if (c->nids < 1 || !meu(m, c, c->ids[0])) return;
		TzObj *e = &m->obj[c->ids[0]];
		if (e->flags & TZ_F_OBRA) return;
		if (c->tipo == TZ_C_PRODUZIR) {
			if (c->arg <= 0 || c->arg >= TZ_NCLASSES || !no_menu(m, e, "create ", tz_classes[c->arg])) return;
			if (tz_disponivel(m, k, e->civ, tz_classes[c->arg], 0)) return;
			tz_enfileirar_trabalho(m, c->ids[0], TZ_J_TREINAR, c->arg);
		} else {
			if (c->arg < 0 || c->arg >= m->cat->ntec) return;
			const char *nome = m->cat->tec[c->arg].nome;
			const TzOrdemDef *d = no_menu(m, e, "technology", nome);
			if (!d) d = no_menu(m, e, "upgrade", nome);
			if (!d) return;
			if (tz_disponivel(m, k, e->civ, nome, 1)) return;
			if (!m->cat->tec[c->arg].mejora && m->reino[k].pesquisas[c->arg]) return;
			tz_enfileirar_trabalho(m, c->ids[0], !strncmp(d->acao, "upgrade", 7) ? TZ_J_MELHORAR : TZ_J_PESQUISAR, c->arg);
		}
		return;
	}
	case TZ_C_CANCELAR:
		if (c->nids >= 1 && meu(m, c, c->ids[0])) tz_cancelar_trabalho(m, c->ids[0], c->arg);
		return;
	case TZ_C_REUNIAO:
		for (int q = 0; q < c->nids; ++q) if (meu(m, c, c->ids[q])) { m->obj[c->ids[q]].ator->reuniao[0] = c->x; m->obj[c->ids[q]].ator->reuniao[1] = c->y; }
		return;
	case TZ_C_MODO:
		for (int q = 0; q < c->nids; ++q) if (meu(m, c, c->ids[q])) m->obj[c->ids[q]].ator->modo = (uint8_t)c->arg;
		return;
	case TZ_C_DESTRUIR:
		for (int q = 0; q < c->nids; ++q) if (meu(m, c, c->ids[q])) tz_matar(m, c->ids[q], TZ_NENHUM);
		return;
	case TZ_C_DIPLOMACIA:
		if (c->arg >= 1 && c->arg <= 8 && c->arg != k) {
			m->reino[k].diplomacia[c->arg] = (uint32_t)c->valor;
			tz_ev(m, TZ_EV_DIPLOMACIA, k, TZ_NENHUM, TZ_NENHUM, 0, 0, 0, c->arg);
		}
		return;
	case TZ_C_RENDIR:
		m->reino[k].rendido = 1;
		return;
	case TZ_C_TRUCO:
		/* cheats.rs (lo básico): 1 recursos, 2 mapa a la vista, 3 terminar las obras del reino */
		if (c->arg == 1) for (int j = 0; j < TZ_NREC; ++j) m->reino[k].recursos[j] += c->valor;
		else if (c->arg == 2) memset(m->reino[k].nevoa, 2, (size_t)m->lado * m->lado);
		else if (c->arg == 3) for (int q = 0; q < m->nobj; ++q) if (m->obj[q].dono == k && (m->obj[q].flags & TZ_F_OBRA) && tz_obj_vivo(&m->obj[q])) tz_obra_pronta(m, q);
		return;
	}
}
