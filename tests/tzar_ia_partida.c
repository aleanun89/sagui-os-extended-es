/* SPDX-License-Identifier: GPL-3.0-or-later */
/* tzar_ia_partida.c - partidas sin cabeza de la IA original de Tzar contra sí misma (D417, fase 6), en el PC.
 *
 * Uso: tzar_ia_partida <instalación> <mapa> <tics> [--semente n] [--dificuldade 0-2] [--gravar f] [--bioma b]
 *   mapa: Maps/X.wmp o Campaign/C.wcm|MAPS/Y.wmp
 * Todos los reinos activos los juega la IA (random.ais). Imprime cada 1000 tics el estado de los reinos y al final
 * comprueba: que la partida corre sin fallos, determinismo (dos partidas iguales dan las mismas sumas del mundo y de
 * cada IA) y guardar/cargar a mitad (sigue con la misma suma). Líneas «[IA] ...» para tests/h417_tzar.py.
 * Con la instalación real es una comprobación PRIVADA (nunca en la batería); con la sintética, la de la batería.
 */
#include "mundo.h"
#include "ia/ia.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

static double agora(void)
{
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return (double)t.tv_sec + t.tv_nsec / 1e9;
}

static int abrir_mapa(const char *raiz, const char *nome, TzHpfs *c, TzHpfs *sub, TzMapa *mp)
{
	char ruta[1024];
	const char *bar = strchr(nome, '|');
	if (!bar) {
		snprintf(ruta, sizeof ruta, "%s/%s", raiz, nome);
		if (tz_hpfs_abrir(c, ruta)) return -1;
		return tz_mapa_ler(c, mp);
	}
	snprintf(ruta, sizeof ruta, "%s/%.*s", raiz, (int)(bar - nome), nome);
	if (tz_hpfs_abrir(c, ruta)) return -1;
	if (tz_hpfs_anidado(c, bar + 1, sub)) return -1;
	return tz_mapa_ler(sub, mp);
}

static TzCatalogo cat;
static TzMapa mp;
static TzIaBiblioteca bib;
static TzRegras regras;
static uint32_t semente = 666;

/* --castelo: como la preparación de una partida libre del original (world/starting.rs reset_initial_forces), un
 * castillo para cada reino que no tiene, junto a la media de sus unidades */
static int castelo;
static void por_castelos(TzMundo *m)
{
	for (int k = 1; k <= TZ_REINOS; ++k) {
		if (!m->reino[k].ativo) continue;
		long sx = 0, sy = 0, n = 0, tem = 0;
		for (int i = 0; i < m->nobj; ++i) {
			const TzObj *o = &m->obj[i];
			if (o->dono != k || !tz_obj_vivo(o)) continue;
			if (o->classe == TZC_CASTLE) tem = 1;
			if (tz_obj_unidade(o)) { sx += o->cx; sy += o->cy; n++; }
		}
		if (tem || !n) continue;
		int sem = m->regras.sem_nevoa;
		m->regras.sem_nevoa = 1;   /* la preparación pone el castillo sin mirar la niebla */
		int cx = (int)(sx / n), cy = (int)(sy / n), ok = 0;
		for (int d = 0; d < 30 && !ok; ++d)
			for (int y = cy - d; y <= cy + d && !ok; ++y)
				for (int x = cx - d; x <= cx + d && !ok; ++x)
					if ((x == cx - d || x == cx + d || y == cy - d || y == cy + d) && !tz_mundo_cabe(m, k, TZC_CASTLE, x - 3, y - 3)) {
						TzId c = tz_mundo_guiao_criar(m, TZC_CASTLE, m->reino[k].civ, k, x - 3, y - 3);
						printf("[IA] castillo del reino %d en %d,%d (objeto %d)\n", k, x - 3, y - 3, c);
						ok = 1;
					}
		m->regras.sem_nevoa = (uint8_t)sem;
		if (!ok) printf("[IA] reino %d: no cabe el castillo (%s)\n", k, tz_erro());
	}
}

/* todos los reinos activos con IA; devuelve cuántas */
static int com_ia(TzMundo *m, TzIa **ias)
{
	int total = 0, n = 0;
	for (int k = 1; k <= TZ_REINOS; ++k) if (m->reino[k].ativo) total++;
	for (int k = 1; k <= TZ_REINOS; ++k) {
		if (!m->reino[k].ativo) continue;
		TzIaErro e;
		/* los reinos del ordenador con su guion; los demás también juegan, con random.ais */
		const TzReino *r = k <= mp.nreinos ? &mp.reinos[k - 1] : NULL;
		const char *g = r && r->ia && (r->ia_modo & 3) ? r->ia_guiao : NULL;
		TzIa *ia = tz_ia_criar(m, &bib, k, TZ_IA_COMPLETA, g, n, total, &e);
		if (!ia) {
			char t[600];
			tz_ia_erro_texto(&e, t, sizeof t);
			printf("[IA] reino %d sin IA: %s\n", k, t);
			continue;
		}
		ias[n++] = ia;
	}
	return n;
}

static uint32_t suma_ias(TzIa **ias, int n)
{
	uint32_t h = 0;
	for (int j = 0; j < n; ++j) h = h * 31u + tz_ia_suma(ias[j]);
	return h;
}

static void estado(const TzMundo *m, TzIa **ias, int nia, const char *pre)
{
	for (int k = 1; k <= TZ_REINOS; ++k) {
		const TzReinoEstado *r = &m->reino[k];
		if (!r->ativo) continue;
		int unid = 0, edif = 0, sold = 0;
		for (int i = 0; i < m->nobj; ++i) {
			const TzObj *o = &m->obj[i];
			if (o->dono != k || !tz_obj_vivo(o)) continue;
			if (tz_obj_unidade(o)) { unid++; if (o->classe != TZC_PEON && o->ataque) sold++; }
			else if (tz_obj_edificio(o)) edif++;
		}
		printf("[IA] %s reino %d: recursos %d/%d/%d/%d pob %u/%u unidades %d (soldados %d) edificios %d creadas %u perdidas %u matadas %u%s\n",
		       pre, k, r->recursos[0], r->recursos[1], r->recursos[2], r->recursos[3], r->populacao, r->populacao_max, unid, sold, edif,
		       r->criadas, r->perdidas, r->matadas, r->derrotado ? " DERROTADO" : "");
	}
	for (int j = 0; j < nia; ++j) {
		TzIaContas c = tz_ia_contas(ias[j]);
		printf("[IA] %s ia %d: miradas %u comandos %u obras %u entrenadas %u investigaciones %u grupos %u suma %08x\n", pre, j,
		       c.olhares, c.comandos, c.construidos, c.treinados, c.pesquisas, c.grupos, tz_ia_suma(ias[j]));
	}
}

static int jogar(TzMundo *m, TzIa **ias, int *nia, int tics, uint32_t *sumas, int nsumas, int falar, double *ms)
{
	memset(m, 0, sizeof *m);
	if (tz_mundo_criar(m, &cat, &mp, &regras, semente)) return -1;
	if (castelo) por_castelos(m);
	*nia = com_ia(m, ias);
	double t0 = agora();
	int k = 0;
	for (int t = 0; t < tics && !m->acabou; ++t) {
		tz_mundo_tic(m);
		if (falar && getenv("TZAR_IA_SEGUIR") && (t + 1) % 20 == 0) {
			TzId i = atoi(getenv("TZAR_IA_SEGUIR"));
			const TzObj *o = &m->obj[i];
			if (o->ator) printf("[SEG] t%d #%d %s (%d,%d) est %d ord %d alvo %d (%d,%d) carga %d/%d trab %d ret %d\n", t + 1, i, tz_classes[o->classe], o->cx, o->cy, o->estado,
			                    o->ator->ordem.tipo, o->ator->ordem.alvo, o->ator->ordem.x, o->ator->ordem.y, o->ator->carga_tipo, o->ator->carga, o->ator->trabalho, o->ator->retomar.tipo);
		}
		if ((t + 1) % 100 == 0 && k < nsumas) sumas[k++] = tz_mundo_suma(m) ^ suma_ias(ias, *nia);
		if (falar && (t + 1) % 2000 == 0) {
			char pre[32];
			snprintf(pre, sizeof pre, "t%d", t + 1);
			estado(m, ias, *nia, pre);
		}
	}
	if (ms) *ms = (agora() - t0) * 1000.0;
	return k;
}

static void soltar(TzMundo *m, TzIa **ias, int n)
{
	tz_mundo_liberar(m);
	for (int j = 0; j < n; ++j) tz_ia_liberar(ias[j]);
}

int main(int argc, char **argv)
{
	if (argc < 4) { fprintf(stderr, "uso: %s <instalación> <mapa> <tics> [--semente n] [--dificuldade d] [--gravar f]\n", argv[0]); return 2; }
	const char *raiz = argv[1], *nome = argv[2], *gravar = NULL, *bioma = NULL;
	int tics = atoi(argv[3]), dif = 1;
	for (int i = 4; i < argc; ++i) {
		if (!strcmp(argv[i], "--gravar") && i + 1 < argc) gravar = argv[++i];
		else if (!strcmp(argv[i], "--bioma") && i + 1 < argc) bioma = argv[++i];
		else if (!strcmp(argv[i], "--semente") && i + 1 < argc) semente = (uint32_t)strtoul(argv[++i], NULL, 0);
		else if (!strcmp(argv[i], "--dificuldade") && i + 1 < argc) dif = atoi(argv[++i]);
		else if (!strcmp(argv[i], "--castelo")) castelo = 1;
	}
	static TzRecursos r;
	static TzBiblioteca b;
	static TzHpfs c, sub;
	if (tz_recursos_abrir(&r, raiz)) { printf("[IA] sin datos: %s\n", tz_erro()); return 1; }
	if (abrir_mapa(raiz, nome, &c, &sub, &mp)) { printf("[IA] mapa: %s\n", tz_erro()); return 1; }
	if (tz_biblioteca_carregar(&b, &r, bioma ? bioma : mp.bioma)) { printf("[IA] biblioteca: %s\n", tz_erro()); return 1; }
	if (tz_catalogo_carregar(&cat, &r, &b)) { printf("[IA] catálogo: %s\n", tz_erro()); return 1; }
	if (tz_ia_bib_carregar(&bib, &r)) { printf("[IA] guiones: %s\n", tz_erro()); return 1; }
	regras.populacao = 200;
	if (getenv("TZAR_IA_DEBUG")) tz_ia_depurar = 1;
	regras.dificuldade = (uint8_t)dif;
	printf("[IA] mapa %s lado %d, %d guiones, dificultad %d, semilla %u\n", nome, mp.lado, bib.nfich, dif, semente);
	static TzMundo m1, m2, m3;
	static TzIa *ia1[8], *ia2[8], *ia3[8];
	int n1, n2, n3 = 0;
	enum { NS = 8192 };
	static uint32_t s1[NS], s2[NS];
	double ms = 0;
	int k1 = jogar(&m1, ia1, &n1, tics, s1, NS, 1, &ms);
	if (k1 < 0) { printf("[IA] mundo: %s\n", tz_erro()); return 1; }
	printf("[IA] %u tics en %.0f ms (%.3f ms por tic) con %d IA\n", m1.tic, ms, m1.tic ? ms / m1.tic : 0.0, n1);
	estado(&m1, ia1, n1, "final");
	if (m1.acabou) printf("[IA] la partida terminó en el tic %u\n", m1.tic);
	uint32_t final1 = tz_mundo_suma(&m1) ^ suma_ias(ia1, n1);
	int k2 = jogar(&m2, ia2, &n2, tics, s2, NS, 0, NULL);
	int iguais = k2 == k1 && !memcmp(s1, s2, (size_t)k1 * sizeof *s1) && (tz_mundo_suma(&m2) ^ suma_ias(ia2, n2)) == final1;
	printf("[IA] determinismo: %s (suma final %08x)\n", iguais ? "las dos partidas dan las mismas sumas" : "LAS SUMAS DIFIEREN", final1);
	soltar(&m2, ia2, n2);
	int ok_gc = 1;
	if (gravar) {
		int mitad = tics / 2;
		static uint32_t s3[NS];
		jogar(&m2, ia2, &n2, mitad, s3, NS, 0, NULL);
		uint32_t antes = tz_mundo_suma(&m2) ^ suma_ias(ia2, n2);
		if (tz_mundo_gravar(&m2, gravar)) { printf("[IA] guardar: %s\n", tz_erro()); ok_gc = 0; }
		soltar(&m2, ia2, n2);
		/* el mundo y sus IA primero (los ganchos se conservan al cargar y leen su bloque) */
		memset(&m3, 0, sizeof m3);
		n3 = 0;
		if (ok_gc && tz_mundo_criar(&m3, &cat, &mp, &regras, semente)) ok_gc = 0;
		if (ok_gc && castelo) por_castelos(&m3);
		if (ok_gc) n3 = com_ia(&m3, ia3);
		if (ok_gc && tz_mundo_carregar(&m3, &cat, &mp, gravar)) { printf("[IA] cargar: %s\n", tz_erro()); ok_gc = 0; }
		if (ok_gc) {
			uint32_t depois = tz_mundo_suma(&m3) ^ suma_ias(ia3, n3);
			for (int t = mitad; t < tics && !m3.acabou; ++t) tz_mundo_tic(&m3);
			uint32_t f3 = tz_mundo_suma(&m3) ^ suma_ias(ia3, n3);
			ok_gc = antes == depois && f3 == final1;
			printf("[IA] guardar/cargar en el tic %d: suma al guardar %08x, al cargar %08x, final %08x: %s\n", mitad, antes, depois, f3,
			       ok_gc ? "igual" : "DIFIERE");
		}
		soltar(&m3, ia3, n3);
	}
	soltar(&m1, ia1, n1);
	tz_ia_bib_liberar(&bib);
	tz_catalogo_liberar(&cat);
	tz_biblioteca_liberar(&b);
	tz_mapa_liberar(&mp);
	if (sub.e || sub.pai) tz_hpfs_fechar(&sub);
	tz_hpfs_fechar(&c);
	tz_recursos_fechar(&r);
	return iguais && ok_gc ? 0 : 3;
}
