/* SPDX-License-Identifier: GPL-3.0-or-later */
/* ia_agua.c - el agua de un reino de la IA: si un sitio se alcanza por tierra, por mar o no, la flota (barcos,
 * pesqueros y puertos), el sitio de un puerto y el transporte de unidades por el agua (D417, fase 6; D470).
 *
 * Traducción a C de ReTzar (https://github.com/ColCarlo/ReTzar, GPL-3.0-or-later, commit cf2af88):
 * src/gameplay/ai/reach.rs (0x417470, 0x417900, 0x417640, el barco que lleva 0x416ef0 y 0x416d00), navy.rs (la flota
 * en la fase especial 0x4168d0, el pesquero 0x4164c0, los pesqueros ociosos 0x415730 y el sitio del puerto 0x40a710),
 * transport.rs (la orden a través del agua 0x415f80, 0x415ec0, y los programas Transport y Use transport 0x430200,
 * 0x4306a0) y de think.rs lo que mira el agua (water_of y la isla pequeña de la casa, SMALL_ISLAND).
 * Diferencias (docs/tzar.md, «La IA»): las regiones de agua las hace la IA con el bit de agua de las casillas (la
 * simulación solo tiene islas de tierra, que ya juntan los puentes); el transporte no es un programa de la simulación
 * sino del jugador: cada mirada manda a los pasajeros a subir (TZ_O_ENTRAR), acerca el barco al primero que falta y,
 * con todos a bordo, lo manda a descargar junto al punto (TZ_O_DESCARREGAR); luego los pasajeros siguen andando.
 * El sitio del puerto se busca en las posiciones de su huella alrededor de la casa (no recorriendo la costa).
 */
#include "ia_int.h"

#define AGUA_PARA_NAVIOS 60   /* reach.rs WATER_FOR_SHIPS */
#define MAX_NAVIOS 4          /* reach.rs MOST_SHIPS */
#define PESQUEIROS 5          /* navy.rs FISHERS */
#define PERTO_PORTO 20        /* navy.rs NEAR_HARBOR */
#define PERTO_CASA 64         /* navy.rs NEAR_HOME */
#define ILHA_PEQUENA 3000     /* think.rs SMALL_ISLAND */
#define PACIENCIA 30          /* transport.rs PATIENCE (aquí en miradas) */
#define ENCONTRO 1            /* el barco va hasta dejar esta casilla vacía con el pasajero (sube desde dos: transport.rs MEET) */
#define BUSCA_PORTO 64        /* radio de la busca del sitio del puerto */
#define MAX_AGUAS 1023

static const int NAVIOS[3] = { TZC_HULK, TZC_XEBEC, TZC_GALLEON };   /* navy.rs SHIPS: 24, 25, 23 */

static int navio(int c) { return c >= 23 && c <= 25; }

/* ------------------------------------------------------------------ regiones de agua (hechas una vez: el agua no cambia) */
static void fazer_aguas(TzIa *ia)
{
	TzMundo *m = ia->m;
	int n = m->lado;
	ia->agua = calloc((size_t)n * n, sizeof *ia->agua);
	int32_t *pilha = malloc((size_t)n * n * sizeof *pilha);
	if (!ia->agua || !pilha) { free(ia->agua); ia->agua = NULL; free(pilha); ia->nagua = -1; return; }
	int prox = 1;
	uint32_t *cel = calloc(MAX_AGUAS + 1, sizeof *cel);
	for (int i = 0; i < n * n; ++i) {
		if (ia->agua[i] || !(m->cel[i].bits & TZ_CEL_AGUA)) continue;
		int id = prox <= MAX_AGUAS ? prox++ : MAX_AGUAS;
		int np = 0;
		pilha[np++] = i;
		ia->agua[i] = (uint16_t)id;
		while (np) {
			int c = pilha[--np], x = c % n, y = c / n;
			if (cel) cel[id]++;
			for (int dy = -1; dy <= 1; ++dy)
				for (int dx = -1; dx <= 1; ++dx) {
					int nx = x + dx, ny = y + dy;
					if ((!dx && !dy) || nx < 0 || ny < 0 || nx >= n || ny >= n) continue;
					int j = ny * n + nx;
					if (ia->agua[j] || !(m->cel[j].bits & TZ_CEL_AGUA)) continue;
					ia->agua[j] = (uint16_t)id;
					pilha[np++] = j;
				}
		}
	}
	free(pilha);
	ia->nagua = prox - 1;
	ia->agua_cel = cel;
	ia->quer_navios = calloc((size_t)ia->nagua + 1, 1);
	ia->quer_portos = calloc((size_t)ia->nagua + 1, 1);
	ia->alvo_porto = malloc(((size_t)ia->nagua + 1) * sizeof *ia->alvo_porto);
	if (!ia->quer_navios || !ia->quer_portos || !ia->alvo_porto || !cel) { ia->nagua = 0; return; }
	for (int w = 0; w <= ia->nagua; ++w) ia->alvo_porto[w][0] = ia->alvo_porto[w][1] = -1;
}

int ia_agua_em(const TzIa *ia, int x, int y)
{
	if (!ia->agua || x < 0 || y < 0 || x >= ia->m->lado || y >= ia->m->lado) return 0;
	return ia->agua[y * ia->m->lado + x];
}

/* think.rs water_of: el agua de la casilla del objeto, si no la primera alrededor de él (hasta tres anillos) */
int ia_agua_de(const TzIa *ia, TzId i)
{
	const TzObj *o = ia_obj(ia, i);
	if (!o || !ia->agua) return 0;
	int w = ia_agua_em(ia, o->cx, o->cy);
	if (w) return w;
	int r[4];
	ia_rect(ia, i, r);
	if (navio(o->classe) || o->classe == TZC_FISHING_BOAT) { r[2] = r[0] + 1; r[3] = r[1] + 1; }
	for (int d = 1; d <= 3; ++d)
		for (int y = r[1] - d; y <= r[3] + d; ++y)
			for (int x = r[0] - d; x <= r[2] + d; ++x)
				if ((w = ia_agua_em(ia, x, y)) != 0) return w;
	return 0;
}

/* la región de tierra de un objeto o casilla: la suya o la primera de sus vecinas */
int ia_ilha_perto(const TzIa *ia, int x, int y, int aneis)
{
	int r = ia_ilha(ia, x, y);
	for (int d = 1; d <= aneis && !r; ++d)
		for (int yy = y - d; yy <= y + d && !r; ++yy)
			for (int xx = x - d; xx <= x + d && !r; ++xx) r = ia_ilha(ia, xx, yy);
	return r;
}

/* los pares (isla de tierra, agua) que se tocan (entre las dos suele haber una casilla de costa), y lo grande que es la
 * isla de la casa; una vez por mirada */
static int cmp_u32(const void *a, const void *b)
{
	uint32_t x = *(const uint32_t *)a, y = *(const uint32_t *)b;
	return x < y ? -1 : x > y;
}

static void fazer_ligacoes(TzIa *ia)
{
	TzMundo *m = ia->m;
	int n = m->lado;
	ia->nliga = 0;
	ia->ilha_cel = 0;
	for (int y = 0; y < n; ++y)
		for (int x = 0; x < n; ++x) {
			int a = ia_ilha(ia, x, y);
			if (!a) continue;
			if (a == ia->ilha) ia->ilha_cel++;
			for (int dy = -2; dy <= 2; ++dy)
				for (int dx = -2; dx <= 2; ++dx) {
					int w = ia_agua_em(ia, x + dx, y + dy);
					if (!w) continue;
					uint32_t par = ((uint32_t)a << 16) | (uint32_t)w;
					if (ia->nliga && ia->liga[ia->nliga - 1] == par) continue;
					if (ia->nliga >= ia->capliga) {
						int c = ia->capliga ? ia->capliga * 2 : 256;
						uint32_t *v = realloc(ia->liga, (size_t)c * sizeof *v);
						if (!v) return;
						ia->liga = v; ia->capliga = c;
					}
					ia->liga[ia->nliga++] = par;
				}
		}
	qsort(ia->liga, (size_t)ia->nliga, sizeof *ia->liga, cmp_u32);
	int k = 0;
	for (int j = 0; j < ia->nliga; ++j) if (!k || ia->liga[k - 1] != ia->liga[j]) ia->liga[k++] = ia->liga[j];
	ia->nliga = k;
}

static int liga(const TzIa *ia, int a, int w)
{
	uint32_t par = ((uint32_t)a << 16) | (uint32_t)w;
	int lo = 0, hi = ia->nliga - 1;
	while (lo <= hi) {
		int mid = (lo + hi) / 2;
		if (ia->liga[mid] == par) return 1;
		if (ia->liga[mid] < par) lo = mid + 1; else hi = mid - 1;
	}
	return 0;
}

/* ------------------------------------------------------------------ listas de la flota (think.rs classify) */
static int capacidade(const TzIa *ia, TzId s)
{
	const TzObj *o = ia_obj(ia, s);
	const TzAtributos *a = o && ia->m->cat ? tz_obj_at(ia->m, o) : NULL;
	return a ? a->capacidade : 0;
}

static TzIaTransporte *transporte_de(TzIa *ia, TzId s)
{
	for (int j = 0; j < ia->ntr; ++j) if (ia->tr[j].navio == s) return &ia->tr[j];
	return NULL;
}

int ia_em_transporte(const TzIa *ia, TzId i)
{
	for (int j = 0; j < ia->ntr; ++j) {
		if (ia->tr[j].navio == i) return 1;
		for (int q = 0; q < ia->tr[j].npass; ++q) if (ia->tr[j].pass[q] == i) return 1;
	}
	return 0;
}

static void listas_frota(TzIa *ia)
{
	ia->navios.n = ia->pesqueiros.n = ia->portos.n = 0;
	for (TzId i = ia->m->nobj - 1; i >= 0; --i) {
		const TzObj *o = &ia->m->obj[i];
		if (o->dono != ia->k || !ia_contado(o) || (o->flags & TZ_F_DENTRO)) continue;
		if (navio(o->classe)) ia_lista_por(&ia->navios, i);
		else if (o->classe == TZC_FISHING_BOAT) ia_lista_por(&ia->pesqueiros, i);
		else if (o->classe == TZC_HARBOR) ia_lista_por(&ia->portos, i);
	}
}

static int navios_em(const TzIa *ia, int w)
{
	int n = 0;
	for (int j = 0; j < ia->navios.n; ++j) if (ia_agua_de(ia, ia->navios.v[j]) == w) n++;
	return n;
}

static int portos_em(const TzIa *ia, int w)
{
	int n = 0;
	for (int j = 0; j < ia->portos.n; ++j) if (ia_agua_de(ia, ia->portos.v[j]) == w) n++;
	return n;
}

/* navy.rs ready: un edificio terminado sin nada que hacer */
static int pronto(const TzIa *ia, TzId i)
{
	const TzObj *o = ia_obj(ia, i);
	return o && ia_contado(o) && !ia_obra(o) && ia_edificio_ocioso(ia, i);
}

/* la mirada: las regiones de agua la primera vez, los pares de cada mirada y la isla pequeña de la casa (think.rs) */
void ia_agua_olhar(TzIa *ia)
{
	if (!ia->agua && ia->nagua == 0) fazer_aguas(ia);
	if (ia->nagua <= 0) return;
	fazer_ligacoes(ia);
	listas_frota(ia);
	if (ia->ilha && ia->ilha_cel < ILHA_PEQUENA) {
		int melhor = 0;
		uint32_t cel = 0;
		for (int w = 1; w <= ia->nagua; ++w)
			if (liga(ia, ia->ilha, w) && (!melhor || ia->agua_cel[w] > cel)) { melhor = w; cel = ia->agua_cel[w]; }
		if (melhor && !ia->quer_navios[melhor]) ia->quer_navios[melhor] = 1;
	}
}

/* ------------------------------------------------------------------ alcanzar (reach.rs) */
static int carregando_para(const TzIa *ia, TzId s, int terra, int cabe);

/* 0x416d00: un barco del agua w que puede llevar un pasajero ya (el más cercano a él); un agua de 60 casillas o más
 * quiere un barco; si ninguno sirve, quiere otro más, hasta 4 y nunca más de uno sobre los que hay */
static TzId navio_agua(TzIa *ia, int w, int terra, TzId pass)
{
	if (ia->agua_cel[w] >= AGUA_PARA_NAVIOS && !ia->quer_navios[w]) ia->quer_navios[w] = 1;
	TzId melhor = TZ_NENHUM;
	uint32_t dm = 0;
	int tem = 0;
	for (int j = 0; j < ia->navios.n; ++j) {
		TzId s = ia->navios.v[j];
		if (ia_agua_de(ia, s) != w) continue;
		tem++;
		const TzObj *o = &ia->m->obj[s];
		if (o->flags & (TZ_F_HISTORIA | TZ_F_MORRENDO)) continue;
		int cabe = capacidade(ia, s);
		if (o->ndentro >= cabe) continue;
		int ocioso = o->ator && o->ator->ordem.tipo == TZ_O_NADA && !transporte_de(ia, s);
		if (!ocioso && !carregando_para(ia, s, terra, cabe)) continue;
		if (pass == TZ_NENHUM) return s;
		const TzObj *p = &ia->m->obj[pass];
		int r[4];
		ia_rect(ia, s, r);
		uint32_t d = ia_dist(p->cx, p->cy, r);
		if (melhor == TZ_NENHUM || d < dm) { melhor = s; dm = d; }
	}
	if (melhor == TZ_NENHUM && tem >= ia->quer_navios[w] && ia->quer_navios[w] < MAX_NAVIOS) ia->quer_navios[w]++;
	return melhor;
}

/* 0x416ef0: el barco que llevaría al pasajero de la isla a a la b: de las aguas que tocan las dos, el más cercano */
TzId ia_navio_para(TzIa *ia, int a, int b, TzId pass)
{
	if (ia->nagua <= 0 || !a || !b || a == b) return TZ_NENHUM;
	TzId melhor = TZ_NENHUM;
	uint32_t dm = 0;
	for (int w = 1; w <= ia->nagua; ++w) {
		if (!liga(ia, a, w) || !liga(ia, b, w)) continue;
		TzId s = navio_agua(ia, w, b, pass);
		if (s == TZ_NENHUM) continue;
		if (pass == TZ_NENHUM) return s;
		const TzObj *p = &ia->m->obj[pass];
		int r[4];
		ia_rect(ia, s, r);
		uint32_t d = ia_dist(p->cx, p->cy, r);
		if (melhor == TZ_NENHUM || d < dm) { melhor = s; dm = d; }
	}
	return melhor;
}

/* 0x417470 con 0x417900: de la isla a a la b, por tierra, por mar (con un barco para cruzar) o no. Las islas de la
 * simulación ya juntan la tierra y los puentes: el original recorre las regiones unidas por tierra; aquí son la misma. */
int ia_via(TzIa *ia, int a, int b)
{
	if (!a || !b) return IA_VIA_NAO;
	if (a == b) return IA_VIA_TERRA;
	return ia_navio_para(ia, a, b, TZ_NENHUM) != TZ_NENHUM ? IA_VIA_MAR : IA_VIA_NAO;
}

/* 0x417640: del punto al rectángulo: la primera región del borde (crecido en una casilla) que contesta */
int ia_via_ponto(TzIa *ia, int fx, int fy, const int r[4])
{
	int de = ia_ilha_perto(ia, fx, fy, 1);
	if (!de) return IA_VIA_TERRA;   /* sobre una obra o puerta: como ia_alcanca */
	int vistas[16], nv = 0, mar = 0;
	for (int y = r[1] - 1; y <= r[3] + 1; ++y)
		for (int x = r[0] - 1; x <= r[2] + 1; ++x) {
			int b = ia_ilha(ia, x, y);
			if (!b) continue;
			if (b == de) return IA_VIA_TERRA;
			int ja = 0;
			for (int q = 0; q < nv && !ja; ++q) ja = vistas[q] == b;
			if (ja || nv >= 16) continue;
			vistas[nv++] = b;
			if (!mar && ia_via(ia, de, b) == IA_VIA_MAR) mar = 1;
		}
	return mar ? IA_VIA_MAR : IA_VIA_NAO;
}

/* ------------------------------------------------------------------ transporte (transport.rs) */
static int carregando_para(const TzIa *ia, TzId s, int terra, int cabe)
{
	TzIaTransporte *t = transporte_de((TzIa *)ia, s);
	if (!t || t->passo != TR_CARREGAR) return 0;
	return t->desembarque == terra && t->npass + ia->m->obj[s].ndentro < cabe;
}

static int a_bordo(const TzIa *ia, TzId p, TzId s)
{
	const TzObj *o = ia_obj(ia, p);
	return o && (o->flags & TZ_F_DENTRO) && o->dentro_de == s;
}

/* 0x415f80 con 0x415ec0: la orden de la unidad i al punto (x, y) llega por tierra (0), cruza el agua en un barco del
 * reino (1: la unidad va a subir) o no llega (2). anda: al desembarcar sigue andando hasta el punto. */
int ia_transportar(TzIa *ia, TzId i, int x, int y, int anda)
{
	const TzObj *o = ia_obj(ia, i);
	if (!o || ia->nagua <= 0) return 2;
	int aqui = ia_ilha_perto(ia, o->cx, o->cy, 1);
	int la = ia_ilha_perto(ia, x, y, 10);
	if (aqui && aqui == la) return 0;
	if (!aqui || !la) return 2;
	TzId s = ia_navio_para(ia, aqui, la, i);
	if (s == TZ_NENHUM) return 2;
	TzIaTransporte *t = transporte_de(ia, s);
	if (!t) {
		if (ia->ntr >= IA_TRANSPORTES) return 2;
		t = &ia->tr[ia->ntr++];
		memset(t, 0, sizeof *t);
		t->navio = s;
		t->ponto[0] = (int16_t)x; t->ponto[1] = (int16_t)y;
		t->desembarque = (uint16_t)la;
		t->passo = TR_CARREGAR;
	}
	if (t->npass >= IA_PASSAGEIROS) return 2;
	if (ia_ordem(ia, i, TZ_O_ENTRAR, s, ia->m->obj[s].cx, ia->m->obj[s].cy)) return 2;
	t->pass[t->npass] = i;
	t->paciencia[t->npass] = 0;
	t->anda[t->npass] = (uint8_t)anda;
	t->npass++;
	ia->contas_d470.transportes++;
	return 1;
}

static void tirar(TzIaTransporte *t, int q)
{
	for (int j = q; j + 1 < t->npass; ++j) {
		t->pass[j] = t->pass[j + 1]; t->paciencia[j] = t->paciencia[j + 1]; t->anda[j] = t->anda[j + 1];
	}
	t->npass--;
}

/* el agua más cercana al pasajero (hasta BUSCA_AGUA anillos) en la que cabe el barco, para ir a recogerlo: el barco y el
 * pasajero van el uno hacia el otro (transport.rs MEET) */
#define BUSCA_AGUA 48
static int agua_junto(const TzIa *ia, int w, int x, int y, int *ox, int *oy)
{
	for (int d = 1; d <= BUSCA_AGUA; ++d)
		for (int yy = y - d; yy <= y + d; ++yy)
			for (int xx = x - d; xx <= x + d; ++xx) {
				if (xx != x - d && xx != x + d && yy != y - d && yy != y + d) continue;
				int ok = 1;
				for (int q = 0; q < 4 && ok; ++q) ok = ia_agua_em(ia, xx + (q & 1), yy + (q >> 1)) == w;
				if (ok) { *ox = xx; *oy = yy; return 1; }
			}
	return 0;
}

/* cada mirada, un paso de cada transporte */
void ia_transportes(TzIa *ia)
{
	for (int j = 0; j < ia->ntr;) {
		TzIaTransporte *t = &ia->tr[j];
		const TzObj *s = ia_obj(ia, t->navio);
		int fim = !s || s->dono != ia->k || !ia_contado(s) || !s->ator;
		if (!fim && t->passo == TR_CARREGAR) {
			int abordo = 0;
			TzId falta = TZ_NENHUM;
			for (int q = 0; q < t->npass;) {
				TzId p = t->pass[q];
				const TzObj *o = ia_obj(ia, p);
				if (!o || o->dono != ia->k || !ia_contado(o) || !o->ator) {
					if (tz_ia_depurar) printf("[DBG]   transporte ia%d: el pasajero #%d ya no está\n", ia->k, p);
					tirar(t, q); continue;
				}
				if (a_bordo(ia, p, t->navio)) { abordo++; q++; continue; }
				if (++t->paciencia[q] > PACIENCIA) {
					if (tz_ia_depurar) printf("[DBG]   transporte ia%d: el pasajero #%d no llega al barco (%d,%d)\n", ia->k, p, o->cx, o->cy);
					tirar(t, q); continue;
				}
				if (o->ator->ordem.tipo != TZ_O_ENTRAR || o->ator->ordem.alvo != t->navio)
					ia_ordem(ia, p, TZ_O_ENTRAR, t->navio, s->cx, s->cy);
				if (falta == TZ_NENHUM) falta = p;
				q++;
			}
			if (!t->npass) fim = 1;
			else if (falta == TZ_NENHUM || s->ndentro >= capacidade(ia, t->navio)) {
				/* todos a bordo (o lleno): a descargar junto al punto */
				ia_ordem(ia, t->navio, TZ_O_DESCARREGAR, TZ_NENHUM, t->ponto[0], t->ponto[1]);
				t->passo = TR_NAVEGAR;
				t->paciencia[0] = 0;
				(void)abordo;
			} else {
				/* el barco va a buscar al primero que falta */
				const TzObj *p = &ia->m->obj[falta];
				int r[4];
				ia_rect(ia, t->navio, r);
				r[2] = r[0] + 1; r[3] = r[1] + 1;
				int w = ia_agua_de(ia, t->navio), x, y;
				int perto = ia_dist(p->cx, p->cy, r) <= ENCONTRO, achou = 0;
				if (!perto && s->ator->ordem.tipo == TZ_O_NADA && (achou = agua_junto(ia, w, p->cx, p->cy, &x, &y)))
					ia_ordem(ia, t->navio, TZ_O_MOVER, TZ_NENHUM, x, y);
				if (tz_ia_depurar)
					printf("[DBG]   transporte ia%d: barco #%d en %d,%d (agua %d, orden %d) busca a #%d en %d,%d: %s\n", ia->k, t->navio, s->cx, s->cy, w,
					       s->ator->ordem.tipo, falta, p->cx, p->cy, perto ? "cerca" : achou ? "va" : s->ator->ordem.tipo != TZ_O_NADA ? "navega" : "sin agua");
			}
		} else if (!fim && t->passo == TR_NAVEGAR) {
			if (s->ator->ordem.tipo == TZ_O_NADA) {
				if (s->ndentro && ++t->paciencia[0] <= 3) ia_ordem(ia, t->navio, TZ_O_DESCARREGAR, TZ_NENHUM, t->ponto[0], t->ponto[1]);
				else {
					/* desembarcados: los que lo piden siguen andando al punto */
					for (int q = 0; q < t->npass; ++q) {
						const TzObj *o = ia_obj(ia, t->pass[q]);
						if (o && o->dono == ia->k && ia_contado(o) && !(o->flags & TZ_F_DENTRO) && t->anda[q])
							ia_ordem(ia, t->pass[q], TZ_O_MOVER, TZ_NENHUM, t->ponto[0], t->ponto[1]);
					}
					ia->contas_d470.desembarques++;
					fim = 1;
				}
			}
		}
		if (fim && tz_ia_depurar)
			printf("[DBG]   transporte ia%d: barco #%d termina en el paso %d con %d pasajeros (%d a bordo)\n", ia->k, t->navio, t->passo, t->npass,
			       s ? s->ndentro : -1);
		if (fim) { ia->tr[j] = ia->tr[--ia->ntr]; continue; }
		j++;
	}
}

/* ------------------------------------------------------------------ la flota (navy.rs) */
/* 0x4164c0: el pesquero para un barco, con un puerto (ver navy.rs fish_patch) */
static TzId pesqueiro(TzIa *ia, int calado, int filtro, TzId *porto)
{
	TzMundo *m = ia->m;
	TzId perto = TZ_NENHUM, pedido = TZ_NENHUM, outro = TZ_NENHUM;
	uint32_t dperto = 0, dpedido = 0, doutro = 0;
	int aoutro = 0;
	*porto = TZ_NENHUM;
	for (TzId i = m->nobj - 1; i >= 0; --i) {
		const TzObj *o = &m->obj[i];
		if (o->classe != TZC_FISHPATCH || !ia_contado(o) || !ia_ve(ia, i)) continue;
		int pescado = 0;
		for (int j = 0; j < ia->pesqueiros.n && !pescado; ++j) {
			const TzObj *b = &m->obj[ia->pesqueiros.v[j]];
			pescado = b->ator && b->ator->ordem.tipo == TZ_O_RECOLHER && b->ator->ordem.alvo == i;
		}
		if (pescado) continue;
		int w = ia_agua_de(ia, i);
		if (!w || (filtro && w != filtro)) continue;
		int ri[4];
		ia_rect(ia, i, ri);
		if (portos_em(ia, w)) {
			TzId h = TZ_NENHUM;
			uint32_t melhor = 0;
			for (int j = 0; j < ia->portos.n; ++j) {
				TzId p = ia->portos.v[j];
				if (ia_agua_de(ia, p) != w || ia_obra(&m->obj[p])) continue;
				int rp[4];
				ia_rect(ia, p, rp);
				uint32_t d = ia_dist_rect(rp, ri);
				if (h == TZ_NENHUM || d < melhor) { h = p; melhor = d; }
			}
			*porto = h;
			if (h != TZ_NENHUM) {
				if (perto == TZ_NENHUM || melhor < dperto) { perto = i; dperto = melhor; }
				continue;
			}
		}
		if (calado) continue;
		uint32_t d = ia_dist(ia->casa[0], ia->casa[1], ri);
		if (ia->quer_portos[w]) { if (pedido == TZ_NENHUM || d < dpedido) { pedido = i; dpedido = d; } }
		else if (outro == TZ_NENHUM || d < doutro) { outro = i; doutro = d; aoutro = w; }
	}
	if (perto != TZ_NENHUM && dperto <= PERTO_PORTO) return perto;
	if (pedido != TZ_NENHUM && dpedido < PERTO_CASA) return pedido;
	if (outro != TZ_NENHUM && doutro < PERTO_CASA) {
		if (!ia->quer_portos[aoutro]) {
			ia->alvo_porto[aoutro][0] = m->obj[outro].cx;
			ia->alvo_porto[aoutro][1] = m->obj[outro].cy;
			ia->quer_portos[aoutro] = 1;
		}
		return outro;
	}
	return TZ_NENHUM;
}

/* 0x415730: un pesquero ocioso pesca el pesquero de su agua que da la elección con un puerto */
int ia_ocioso_pesqueiro(TzIa *ia, TzId b)
{
	if (ia->nagua <= 0) return 0;
	int w = ia_agua_de(ia, b);
	if (!w) return 0;
	TzId porto, p = pesqueiro(ia, 1, w, &porto);
	if (p == TZ_NENHUM || porto == TZ_NENHUM) return 0;
	return !ia_ordem(ia, b, TZ_O_RECOLHER, p, ia->m->obj[p].cx, ia->m->obj[p].cy);
}

/* 0x4168d0: la flota en la fase especial (ver navy.rs navy). Los barcos y pesqueros se hacen con el programa Create
 * solo: la IA no los descuenta de lo que puede gastar (la simulación sí los cobra). */
int ia_frota(TzIa *ia)
{
	if (ia->nagua <= 0) return 0;
	if (tz_ia_depurar)
		printf("[DBG]   flota ia%d: barcos %d pesqueros %d puertos %d quiere barcos %d puertos %d\n", ia->k, ia->navios.n, ia->pesqueiros.n,
		       ia->portos.n, ia->quer_navios[1], ia->quer_portos[1]);
	int barco = -1;
	for (int j = 0; j < 3 && barco < 0; ++j) if (ia_disponivel_tipo(ia, NAVIOS[j])) barco = NAVIOS[j];
	if (barco >= 0) {
		for (int w = 1; w <= ia->nagua; ++w) {
			if (!ia->quer_navios[w] || navios_em(ia, w) >= ia->quer_navios[w]) continue;
			int32_t c[4];
			ia_custo(ia, barco, c);
			if (!ia_pagavel(ia, c, B_NAVAL, 1, 0)) {
				int32_t x[4];
				ia_custo(ia, TZC_XEBEC, x);
				if (barco == TZC_HULK && ia_disponivel_tipo(ia, TZC_XEBEC) && ia_pagavel(ia, x, B_NAVAL, 1, 0)) barco = TZC_XEBEC;
				else { ia_reservar(ia, B_NAVAL, c, 1); break; }
			}
			if (!portos_em(ia, w)) { if (!ia->quer_portos[w]) ia->quer_portos[w] = 1; }
			else
				for (int j = 0; j < ia->portos.n; ++j) {
					TzId h = ia->portos.v[j];
					if (ia_agua_de(ia, h) == w && pronto(ia, h)) { if (!ia_produzir(ia, h, barco)) ia->contas_d470.navios++; break; }
				}
		}
	}
	int colina = ia->m->regras.vitoria == 4 && ia->m->regras.nivel_recursos == 3;   /* rey de la colina sin límite */
	if (!colina && ia->pesqueiros.n < PESQUEIROS && ia_disponivel_tipo(ia, TZC_FISHING_BOAT)) {
		TzId porto, p = pesqueiro(ia, 0, 0, &porto);
		if (p != TZ_NENHUM && porto != TZ_NENHUM && pronto(ia, porto)) {
			int32_t c[4];
			ia_custo(ia, TZC_FISHING_BOAT, c);
			if (ia_pagavel(ia, c, B_TRABALHADORES, 1, 0)) ia_produzir(ia, porto, TZC_FISHING_BOAT);
			else ia_reservar(ia, B_TRABALHADORES, c, 1);
		}
	}
	uint32_t feitos, em_curso;
	if (!ia_disponivel_tipo(ia, TZC_HARBOR)) return 0;
	ia_registro(ia, ia->k, TZC_HARBOR, &feitos, &em_curso);
	if (em_curso) return 0;
	int falhou = 0;
	for (int w = 1; w <= ia->nagua; ++w) {
		if (!ia->quer_portos[w] || portos_em(ia, w) >= ia->quer_portos[w]) continue;
		int32_t c[4];
		ia_custo(ia, TZC_HARBOR, c);
		int parte = ia->quer_navios[w] ? B_NAVAL : B_TRABALHADORES;
		if (!ia_pagavel(ia, c, parte, 1, 0)) { ia_reservar(ia, parte, c, 1); return falhou; }
		ia->agua_porto = (uint16_t)w;   /* 0x5af414: el agua del puerto, la lee su sitio */
		int ok = ia_colocar(ia, TZC_HARBOR, ia->casa[0], ia->casa[1], 5, 50, TZ_NENHUM);
		if (tz_ia_depurar) printf("[DBG]   flota: puerto para el agua %d: %s\n", w, ok ? "sí" : "no");
		if (ok) return 1;
		falhou = 1;
	}
	return falhou;
}

/* 0x40a710: el sitio de un puerto junto a la isla de la casa: la huella toca el agua que se quiere (si hay) y tiene al
 * lado tierra de la isla; gana la menor distancia a la referencia más la distancia al pesquero que lo pidió, y el sitio
 * tiene que ser seguro. Se miran las posiciones a BUSCA_PORTO de la referencia, en orden de fila. */
int ia_sitio_porto(TzIa *ia, int cx, int cy, int *ox, int *oy)
{
	int w0 = ia->agua_porto;
	ia->agua_porto = 0;
	if (ia->nagua <= 0 || !ia->ilha) return 0;
	int alvo = w0 && ia->alvo_porto[w0][0] >= 0;
	int w, h;
	ia_pegada(ia, TZC_HARBOR, &w, &h);
	int u = ia->m->lado - 1, melhor = 0, est[3] = { 0, 0, 0 };
	uint32_t pm = 0;
	for (int y = cy - BUSCA_PORTO; y <= cy + BUSCA_PORTO; ++y) {
		if (y < 0 || y + h - 1 > u) continue;
		for (int x = cx - BUSCA_PORTO; x <= cx + BUSCA_PORTO; ++x) {
			if (x < 0 || x + w - 1 > u) continue;
			int toca = 0, terra = 0;
			for (int yy = y - 1; yy <= y + h && (!toca || !terra); ++yy)
				for (int xx = x - 1; xx <= x + w && (!toca || !terra); ++xx) {
					int a = ia_agua_em(ia, xx, yy);
					if (a && (!w0 || a == w0)) toca = 1;
					if (ia_ilha(ia, xx, yy) == ia->ilha) terra = 1;
				}
			if (!toca || !terra) continue;
			est[0]++;
			int r[4] = { x, y, x + w - 1, y + h - 1 };
			uint32_t p = ia_dist(cx, cy, r);
			if (alvo) p += ia_dist(ia->alvo_porto[w0][0], ia->alvo_porto[w0][1], r);
			if (melhor && p >= pm) continue;
			if (tz_mundo_cabe(ia->m, ia->k, TZC_HARBOR, x, y)) { if (tz_ia_depurar && est[1] < 2) printf("[DBG]     puerto %d,%d: %s\n", x, y, tz_erro()); continue; }
			est[1]++;
			if (!ia_seguro(ia, x, y, w, h)) continue;
			est[2]++;
			*ox = x; *oy = y; pm = p; melhor = 1;
		}
	}
	if (tz_ia_depurar) printf("[DBG]   sitio del puerto (agua %d): %d tocan, %d caben, %d seguros\n", w0, est[0], est[1], est[2]);
	return melhor;
}

/* ------------------------------------------------------------------ guardar y cargar lo de la D470 */
#define ESC(v) do { if (fwrite(&(v), sizeof(v), 1, f) != 1) return -1; } while (0)
#define LER(v) do { if (lidos + sizeof(v) > resta || fread(&(v), sizeof(v), 1, f) != 1) return -1; lidos += sizeof(v); } while (0)

int ia_d470_gravar(const TzIa *ia, FILE *f)
{
	int32_t na = ia->nagua > 0 ? ia->nagua : 0;
	ESC(na);
	for (int w = 1; w <= na; ++w) { ESC(ia->quer_navios[w]); ESC(ia->quer_portos[w]); ESC(ia->alvo_porto[w]); }
	ESC(ia->agua_porto);
	ESC(ia->ntr);
	for (int j = 0; j < ia->ntr; ++j) ESC(ia->tr[j]);
	ESC(ia->nbrechas);
	for (int j = 0; j < ia->nbrechas; ++j) ESC(ia->brecha[j]);
	ESC(ia->contas_d470);
	return 0;
}

int32_t ia_d470_ler(TzIa *ia, FILE *f, uint32_t resta)
{
	uint32_t lidos = 0;
	int32_t na;
	LER(na);
	if (na < 0 || na > MAX_AGUAS) return -1;
	if (na) {
		/* las regiones de agua salen igual del mapa: se hacen ya para tener dónde leer */
		if (!ia->agua && ia->nagua == 0) fazer_aguas(ia);
		if (ia->nagua != na) return -1;
		for (int w = 1; w <= na; ++w) { LER(ia->quer_navios[w]); LER(ia->quer_portos[w]); LER(ia->alvo_porto[w]); }
	}
	LER(ia->agua_porto);
	LER(ia->ntr);
	if (ia->ntr < 0 || ia->ntr > IA_TRANSPORTES) return -1;
	for (int j = 0; j < ia->ntr; ++j) { LER(ia->tr[j]); if (ia->tr[j].npass < 0 || ia->tr[j].npass > IA_PASSAGEIROS) return -1; }
	LER(ia->nbrechas);
	if (ia->nbrechas < 0 || ia->nbrechas > IA_BRECHAS) return -1;
	for (int j = 0; j < ia->nbrechas; ++j) LER(ia->brecha[j]);
	LER(ia->contas_d470);
	return (int32_t)lidos;
}

void ia_agua_liberar(TzIa *ia)
{
	free(ia->agua); free(ia->agua_cel); free(ia->liga); free(ia->quer_navios); free(ia->quer_portos); free(ia->alvo_porto);
	free(ia->navios.v); free(ia->pesqueiros.v); free(ia->portos.v);
}
