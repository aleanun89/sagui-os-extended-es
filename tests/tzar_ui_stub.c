/* SPDX-License-Identifier: GPL-3.0-or-later */
/* tzar_ui_stub.c - ver tzar_ui_stub.h. */
#include "tzar_ui_stub.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

TzComando stub_log[512];
int stub_nlog;

/* ORDERS.INI y MENU.INI mínimos con el mismo formato del original, para las pruebas sin datos */
static const char ORDENES_SINT[] =
	"[move]\nACTION = move\nPLACE = command_move\nIMG = 1\n\n[attack]\nACTION = attack\nPLACE = command_attack\nIMG = 2\n\n"
	"[stop]\nACTION = stop\nIMG = 3\n\n[e_build]\nSUBMENU = e_build\nIMG = 4\n\n[e_barrack]\nACTION = BUILD barrack\nPLACE = barrack\nIMG = 5\nCLASSNAME = barrack\n\n"
	"[e_house]\nACTION = BUILD house\nPLACE = house\nIMG = 6\nCLASSNAME = house\n\n[e_archer]\nACTION = CREATE archer\nIMG = 7\nCLASSNAME = archer\n\n"
	"[e_footman]\nACTION = CREATE footman\nIMG = 8\nCLASSNAME = footman\n\n[te_feudalism]\nACTION = technology\nIMG = 9\nCLASSNAME = te_feudalism\n\n"
	"[stand_ground]\nACTION = stand_ground\nIMG = 10\n\n";
static const char MENUS_SINT[] =
	"[e_peon]\nmove = M\nattack = A\nstop = S\ne_build = B\nstand_ground = G\n\n[e_build]\ne_barrack = C\ne_house = H\n\n"
	"[e_barrack]\ne_archer = A\ne_footman = F\n\n[e_barrack_b]\ne_archer = A\n\n[e_castle]\nte_feudalism = D\n\n[footman]\nmove = M\nattack = A\nstop = S\n\n";

static void ordens_de(TzCatalogo *c)
{
	c->nord = c->ordens_ini.ns;
	c->ord = calloc((size_t)c->nord + 1, sizeof *c->ord);
	int k = 0;
	for (int i = 0; i < c->ordens_ini.ns; ++i) {
		const TzIniSecao *s = &c->ordens_ini.s[i];
		TzOrdemDef *o = &c->ord[k++];
		snprintf(o->chave, sizeof o->chave, "%s", s->nome);
		for (char *p = o->chave; *p; ++p) *p = (char)tolower((unsigned char)*p);
		const char *a = tz_ini(&c->ordens_ini, s->nome, "action");
		snprintf(o->acao, sizeof o->acao, "%s", a ? a : "");
		for (char *p = o->acao; *p; ++p) *p = (char)tolower((unsigned char)*p);
		const char *cn = tz_ini(&c->ordens_ini, s->nome, "classname");
		snprintf(o->classe, sizeof o->classe, "%s", cn ? cn : "");
		for (char *p = o->classe; *p; ++p) *p = (char)tolower((unsigned char)*p);
		const char *sm = tz_ini(&c->ordens_ini, s->nome, "submenu");
		snprintf(o->submenu, sizeof o->submenu, "%s", sm ? sm : "");
		const char *im = tz_ini(&c->ordens_ini, s->nome, "img");
		o->icone = (int16_t)(im && *im ? atoi(im) : -1);
		o->com_alvo = tz_ini(&c->ordens_ini, s->nome, "place") != NULL;
	}
	c->nord = k;
}

static void atributos(TzCatalogo *c, TzRecursos *rec)
{
	for (int civ = 0; civ < 4; ++civ)
		for (int cl = 1; cl < TZ_NCLASSES; ++cl) {
			TzAtributos *a = &c->at[civ][cl];
			int u = tz_classe_e_unidade(cl), e = tz_classe_e_edificio(cl);
			if (!u && !e) continue;
			a->existe = 1; a->tipo = u ? TZ_T_UNIDADE : TZ_T_EDIFICIO;
			a->pegada[0] = a->pegada[1] = 1;
			a->vida = 100; a->ataque = 10; a->defesa = 5; a->resistencia = 3; a->visao = 8;
			if (e) { a->pegada[0] = a->pegada[1] = cl == TZC_CASTLE ? 5 : cl == TZC_HOUSE ? 2 : 3; }
			if (cl == TZC_BARRACK) { a->custo[TZ_MADEIRA] = 100; a->custo[TZ_PEDRA] = 20; }
			if (cl == TZC_HOUSE) a->custo[TZ_MADEIRA] = 50;
			if (cl == 4) { a->custo[TZ_OURO] = 40; a->custo[TZ_MADEIRA] = 20; }   /* archer */
			if (cl == 3) a->custo[TZ_OURO] = 30;                                     /* footman */
			const char *sec = civ == 2 ? "arab" : civ == 3 ? "asia" : "euro";
			if (rec) {   /* menú de la clase: DATA/OBJECTS/<CLASE>.INI */
				char n[96];
				snprintf(n, sizeof n, "DATA/OBJECTS/%s.INI", tz_classes[cl]);
				uint32_t sz;
				uint8_t *d = tz_recursos_ler(rec, n, &sz);
				if (d) {
					TzIni ini = { 0 };
					tz_ini_ler(&ini, d, sz);
					const char *mn = tz_ini(&ini, sec, "menu");
					if (mn) snprintf(a->menu, sizeof a->menu, "%s", mn);
					tz_ini_liberar(&ini);
					free(d);
				}
			} else {
				const char *mn = cl == TZC_PEON ? "e_peon" : cl == TZC_BARRACK ? "e_barrack" : cl == TZC_CASTLE ? "e_castle" : cl == 3 ? "footman" : "";
				snprintf(a->menu, sizeof a->menu, "%s", mn);
			}
		}
}

void stub_catalogo(TzCatalogo *c, TzRecursos *rec, const TzBiblioteca *bib)
{
	(void)bib;
	memset(c, 0, sizeof *c);
	uint32_t n;
	if (rec) {
		tz_ini_recurso(rec, &c->ordens_ini, "DATA/ORDERS.INI");
		tz_ini_recurso(rec, &c->menus, "DATA/MENU.INI");
	} else {
		tz_ini_ler(&c->ordens_ini, (const uint8_t *)ORDENES_SINT, sizeof ORDENES_SINT - 1);
		tz_ini_ler(&c->menus, (const uint8_t *)MENUS_SINT, sizeof MENUS_SINT - 1);
	}
	(void)n;
	ordens_de(c);
	atributos(c, rec);
	c->ntec = 1;
	c->tec = calloc(1, sizeof *c->tec);
	snprintf(c->tec[0].nome, sizeof c->tec[0].nome, "te_feudalism");
	c->tec[0].custo[TZ_OURO] = 200;
}

void stub_catalogo_liberar(TzCatalogo *c)
{
	free(c->ord); free(c->tec);
	tz_ini_liberar(&c->ordens_ini); tz_ini_liberar(&c->menus);
}

const TzAtributos *tz_catalogo_at(const TzCatalogo *c, int classe, int civ)
{
	return classe > 0 && classe < TZ_NCLASSES && civ >= 0 && civ < 4 ? &c->at[civ][classe] : NULL;
}
int tz_catalogo_tec(const TzCatalogo *c, const char *nome)
{
	for (int i = 0; i < c->ntec; ++i) if (!strcasecmp(c->tec[i].nome, nome)) return i;
	return -1;
}
const TzOrdemDef *tz_catalogo_ordem(const TzCatalogo *c, const char *chave)
{
	for (int i = 0; i < c->nord; ++i) if (!strcasecmp(c->ord[i].chave, chave)) return &c->ord[i];
	return NULL;
}
int tz_catalogo_classe(const char *nome)
{
	for (int i = 0; i < TZ_NCLASSES; ++i) if (!strcasecmp(tz_classes[i], nome)) return i;
	return -1;
}
int tz_catalogo_menu(const TzCatalogo *c, int classe, int civ, const TzOrdemDef **out, int max)
{
	const TzAtributos *a = tz_catalogo_at(c, classe, civ);
	const TzIniSecao *s = a && a->menu[0] ? tz_ini_secao(&c->menus, a->menu) : NULL;
	int n = 0;
	for (int i = 0; s && i < s->n && n < max; ++i) { const TzOrdemDef *o = tz_catalogo_ordem(c, s->p[i].chave); if (o) out[n++] = o; }
	return n;
}

/* ---------------------------------------------------------------- mundo */

TzId stub_obj(TzMundo *m, int classe, int civ, int dono, int cx, int cy)
{
	if (m->nobj == m->capobj) {
		m->capobj = m->capobj ? m->capobj * 2 : 64;
		m->obj = realloc(m->obj, (size_t)m->capobj * sizeof *m->obj);
	}
	TzObj *o = &m->obj[m->nobj];
	memset(o, 0, sizeof *o);
	o->id_mapa = (uint16_t)m->nobj; o->classe = (uint16_t)classe; o->civ = (uint8_t)civ; o->dono = (uint8_t)dono;
	o->cor = dono > 0 ? (uint8_t)(dono - 1) : 0;
	o->cx = (int16_t)cx; o->cy = (int16_t)cy;
	o->px = cx * TZ_SUB + (tz_classe_e_unidade(classe) ? TZ_SUB / 2 : 0); o->py = cy * TZ_SUB + (tz_classe_e_unidade(classe) ? TZ_SUB / 2 : 0);
	o->vida = o->vida_max = 100; o->ataque = 10; o->defesa = 5; o->resistencia = 3; o->visao = 8; o->nivel = 1;
	o->dentro_de = TZ_NENHUM;
	if (tz_classe_e_unidade(classe) || tz_classe_e_edificio(classe)) o->ator = calloc(1, sizeof *o->ator);
	return m->nobj++;
}

void stub_mundo(TzMundo *m, const TzCatalogo *c, int lado)
{
	memset(m, 0, sizeof *m);
	m->cat = c; m->lado = lado; m->jogador = 1;
	for (int k = 1; k <= TZ_REINOS; ++k) {
		TzReinoEstado *r = &m->reino[k];
		r->ativo = k <= 2; r->civ = 1; r->cor = (uint8_t)(k - 1);
		snprintf(r->nome, sizeof r->nome, "Jugador %d", k);
		r->recursos[TZ_OURO] = 500; r->recursos[TZ_MADEIRA] = 300; r->recursos[TZ_COMIDA] = 200; r->recursos[TZ_PEDRA] = 100;
		r->populacao = 5; r->populacao_max = 40; r->populacao_limite = 200;
		r->nevoa = calloc((size_t)lado * (size_t)lado, 1);
	}
	m->regras.populacao = 200;
	m->ev_seq = 0;
}

void stub_nevoa(TzMundo *m)
{
	uint8_t *f = m->reino[m->jogador].nevoa;
	for (int i = 0; i < m->lado * m->lado; ++i) if (f[i] == 2) f[i] = 1;
	for (int i = 0; i < m->nobj; ++i) {
		const TzObj *o = &m->obj[i];
		if (!tz_obj_vivo(o) || o->dono != m->jogador) continue;
		int r = o->visao ? o->visao : 6;
		for (int y = o->cy - r; y <= o->cy + r; ++y)
			for (int x = o->cx - r; x <= o->cx + r; ++x)
				if (x >= 0 && y >= 0 && x < m->lado && y < m->lado && (x - o->cx) * (x - o->cx) + (y - o->cy) * (y - o->cy) <= r * r) f[y * m->lado + x] = 2;
	}
}

void stub_mundo_de_mapa(TzMundo *m, const TzCatalogo *c, const TzMapa *mapa, int jogador)
{
	stub_mundo(m, c, mapa->lado);
	m->mapa = mapa; m->jogador = jogador;
	for (int i = 0; i < mapa->nobj; ++i) {
		const TzObjeto *s = &mapa->obj[i];
		if (!tz_classe_e_unidade(s->classe) && !tz_classe_e_edificio(s->classe) && s->classe != TZC_RES_GOLD && s->classe != TZC_RES_STONE) continue;
		int dono = s->dono;
		TzId id = stub_obj(m, s->classe, s->civ, dono, s->x, s->y);
		if (s->vida_max) { m->obj[id].vida = s->vida; m->obj[id].vida_max = s->vida_max; }
		if (s->classe == TZC_RES_GOLD || s->classe == TZC_RES_STONE) m->obj[id].recurso = 20000;
	}
	for (int k = 1; k <= TZ_REINOS && k <= mapa->nreinos; ++k) {
		m->reino[k].ativo = mapa->reinos[k - 1].ativo;
		if (mapa->reinos[k - 1].nome[0]) snprintf(m->reino[k].nome, sizeof m->reino[k].nome, "%s", mapa->reinos[k - 1].nome);
		m->reino[k].civ = (uint8_t)(mapa->reinos[k - 1].civ ? mapa->reinos[k - 1].civ : 1);
		m->reino[k].cor = (uint8_t)(mapa->reinos[k - 1].cor > 0 ? mapa->reinos[k - 1].cor - 1 : k - 1);
	}
	stub_nevoa(m);
}

void stub_liberar(TzMundo *m)
{
	for (int i = 0; i < m->nobj; ++i) free(m->obj[i].ator);
	for (int k = 0; k <= TZ_REINOS; ++k) free(m->reino[k].nevoa);
	free(m->obj); free(m->cmd);
	memset(m, 0, sizeof *m);
}

int tz_obj_vivo(const TzObj *o) { return o && !(o->flags & (TZ_F_MORTO | TZ_F_MORRENDO | TZ_F_DENTRO)); }
int tz_obj_unidade(const TzObj *o) { return o && tz_classe_e_unidade(o->classe); }
int tz_obj_edificio(const TzObj *o) { return o && tz_classe_e_edificio(o->classe); }
void tz_obj_pixel(const TzObj *o, int *x, int *y) { *x = o->px * TZ_CASILLA / TZ_SUB; *y = o->py * TZ_CASILLA / TZ_SUB; }

int tz_mundo_relacao(const TzMundo *m, int a, int b)
{
	if (a == b) return 1;
	if (a == 0 || b == 0) return 0;
	return m->reino[a].equipe && m->reino[a].equipe == m->reino[b].equipe ? 1 : -1;
}
int tz_mundo_nevoa(const TzMundo *m, int k, int cx, int cy)
{
	if (cx < 0 || cy < 0 || cx >= m->lado || cy >= m->lado || !m->reino[k].nevoa) return 0;
	return m->reino[k].nevoa[cy * m->lado + cx];
}
int tz_mundo_pode(const TzMundo *m, int k, TzId obj, const char *chave)
{
	(void)m; (void)k; (void)obj;
	if (!strcasecmp(chave, "te_feudalism")) return tz_falha("Hace falta un castillo mejorado");
	return 0;
}
void tz_mundo_custo_classe(const TzMundo *m, int k, int classe, uint32_t custo[TZ_NREC], uint32_t *t)
{
	const TzAtributos *a = tz_catalogo_at(m->cat, classe, m->reino[k].civ);
	for (int r = 0; r < TZ_NREC; ++r) custo[r] = a ? a->custo[r] : 0;
	*t = 10000;
}
void tz_mundo_custo_tec(const TzMundo *m, int k, int tec, uint32_t custo[TZ_NREC], uint32_t *t)
{
	(void)k;
	for (int r = 0; r < TZ_NREC; ++r) custo[r] = m->cat->tec[tec].custo[r];
	*t = 20000;
}
int tz_mundo_cabe(const TzMundo *m, int k, int classe, int cx, int cy)
{
	const TzAtributos *a = tz_catalogo_at(m->cat, classe, m->reino[k].civ);
	int w = a ? a->pegada[0] : 1, h = a ? a->pegada[1] : 1;
	if (cx < 0 || cy < 0 || cx + w > m->lado || cy + h > m->lado) return tz_falha("Fuera del mapa");
	for (int i = 0; i < m->nobj; ++i) {
		const TzObj *o = &m->obj[i];
		if (!tz_obj_vivo(o)) continue;
		int ow = 1, oh = 1;
		if (tz_obj_edificio(o)) { const TzAtributos *b = tz_catalogo_at(m->cat, o->classe, o->civ); if (b) { ow = b->pegada[0]; oh = b->pegada[1]; } }
		else if (!tz_obj_unidade(o) && o->classe != TZC_RES_GOLD && o->classe != TZC_RES_STONE) continue;
		if (cx < o->cx + ow && cx + w > o->cx && cy < o->cy + oh && cy + h > o->cy) return tz_falha("No se puede construir aquí");
	}
	if (tz_mundo_nevoa(m, k, cx, cy) == 0) return tz_falha("Zona sin explorar");
	return 0;
}
TzOrdem tz_mundo_ordem_padrao(const TzMundo *m, int k, const TzId *ids, int n, int cx, int cy, TzId alvo)
{
	(void)ids; (void)n;
	TzOrdem o;
	memset(&o, 0, sizeof o);
	o.alvo = TZ_NENHUM; o.x = (int16_t)cx; o.y = (int16_t)cy; o.tipo = TZ_O_MOVER;
	if (alvo >= 0 && alvo < m->nobj) {
		const TzObj *t = &m->obj[alvo];
		if (t->dono != k && tz_mundo_relacao(m, k, t->dono) < 0 && (tz_obj_unidade(t) || tz_obj_edificio(t))) { o.tipo = TZ_O_ATACAR; o.alvo = alvo; }
		else if (t->classe == TZC_RES_GOLD || t->classe == TZC_RES_STONE) { o.tipo = TZ_O_RECOLHER; o.alvo = alvo; }
	}
	return o;
}

int tz_mundo_comando(TzMundo *m, const TzComando *c)
{
	if (c->reino != m->jogador) return -1;
	if (stub_nlog < 512) stub_log[stub_nlog++] = *c;
	if (c->tipo == TZ_C_CONSTRUIR) {   /* la obra aparece al momento, para ver el resultado en pantalla */
		TzId id = stub_obj(m, c->arg, m->reino[m->jogador].civ, m->jogador, c->x, c->y);
		m->obj[id].flags |= TZ_F_OBRA;
	}
	return 0;
}
void tz_mundo_evento(TzMundo *m, const TzEvento *e)
{
	TzEvento *d = &m->ev[m->ev_seq % TZ_EVENTOS];
	*d = *e; d->seq = ++m->ev_seq; d->tic = m->tic;
}
void stub_evento(TzMundo *m, int tipo, int reino, int classe)
{
	TzEvento e;
	memset(&e, 0, sizeof e);
	e.tipo = (uint8_t)tipo; e.reino = (uint8_t)reino; e.classe = (uint16_t)classe; e.obj = e.alvo = TZ_NENHUM;
	tz_mundo_evento(m, &e);
}
int tz_mundo_eventos(const TzMundo *m, uint32_t *cursor, TzEvento *out, int max)
{
	int n = 0;
	while (*cursor < m->ev_seq && n < max) { out[n++] = m->ev[*cursor % TZ_EVENTOS]; (*cursor)++; }
	return n;
}
const char *tz_mundo_texto(const TzMundo *m, int32_t idx) { return m->textos[(uint32_t)idx % 64]; }

/* ui_mapa.c dibuja el mundo vivo con tz_desenhar_mundo (partida.c, la simulación de verdad): en el mundo de prueba no
 * hay objetos que dibujar más allá del terreno */
int tz_desenhar_mundo(TzMundo *m, TzEscena *e, TzLienzo *l, int camx, int camy, uint64_t ms)
{
	(void)m; (void)e; (void)l; (void)camx; (void)camy; (void)ms;
	return 0;
}
