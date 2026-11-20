/* SPDX-License-Identifier: GPL-3.0-or-later */
/* tzar_simulacao.c - prueba sin cabeza de la simulación de Tzar (D417, fase 5), en el PC.
 *
 * Uso: tzar_simulacao <instalación> <mapa> <tics> [semente] [--gravar f] [--bioma b]
 *   mapa: Maps/X.wmp o Campaign/C.wcm|MAPS/Y.wmp
 * Abre la instalación y el mapa, monta el catálogo y el mundo y juega N tics con un «jugador de prueba» que manda
 * comandos deterministas a todos los reinos (demo.c: castillo, casas, peones a recoger, soldados al ataque). Imprime la suma cada 100 tics y al final; además comprueba:
 *  - determinismo: la misma partida dos veces da las mismas sumas;
 *  - guardar/cargar: guardar a mitad, cargar y seguir da la misma suma final que sin cargar.
 * Sale con 0 si todo cuadra. Líneas «[SIM] ...» para tests/h417_tzar.py.
 */
#include "mundo.h"
#include "partida.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <dirent.h>
#include <strings.h>

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

typedef struct { TzCatalogo *cat; TzMapa *mapa; TzRegras regras; uint32_t semente; } Partida;

static int inicio;   /* --inicio: partida libre (castillo y tres peones) */

static int jogar(Partida *p, TzMundo *m, int tics, uint32_t *sumas, int nsumas, const char *gravar_em, int gravar_tic, double *ms)
{
	if (tz_mundo_criar(m, p->cat, p->mapa, &p->regras, p->semente)) return -1;
	if (inicio && tz_mundo_inicio_livre(m)) return -1;
	TzGancho g; memset(&g, 0, sizeof g);
	g.tic = tz_demo_tic; g.cada = 1; snprintf(g.etiqueta, sizeof g.etiqueta, "PRUEBA");
	tz_mundo_gancho(m, &g);
	double t0 = agora();
	int k = 0;
	for (int t = 0; t < tics; ++t) {
		if (gravar_em && t == gravar_tic && tz_mundo_gravar(m, gravar_em)) { fprintf(stderr, "[SIM] error al guardar: %s\n", tz_erro()); return -1; }
		tz_mundo_tic(m);
		if ((t + 1) % 100 == 0 && k < nsumas) sumas[k++] = tz_mundo_suma(m);
		if (getenv("TZAR_SIM_DEBUG") && (t + 1) % atoi(getenv("TZAR_SIM_DEBUG")) == 0)
			for (int i = 0; i < m->nobj; ++i) {
				const TzObj *o = &m->obj[i];
				if (!o->ator || !tz_obj_vivo(o) || !o->dono) continue;
				printf("[DBG] t%d #%d %s d%d (%d,%d) vida %d est %d ord %d alvo %d (%d,%d) carga %d/%d cam %d/%d falhas %d trab %d\n", t + 1, i, tz_classes[o->classe], o->dono, o->cx, o->cy,
				       o->vida, o->estado, o->ator->ordem.tipo, o->ator->ordem.alvo, o->ator->ordem.x, o->ator->ordem.y, o->ator->carga_tipo, o->ator->carga,
				       o->ator->icaminho, o->ator->ncaminho, o->ator->falhas, o->ator->ntrab);
			}
	}
	if (ms) *ms = (agora() - t0) * 1000.0;
	return k;
}

/* --listar: los mapas sueltos y los de las campañas, uno por línea, como los pide el segundo argumento */
static int listar(const char *raiz)
{
	char d[1100];
	const char *pastas[2] = { "Maps", "Campaign" };
	for (int k = 0; k < 2; ++k) {
		snprintf(d, sizeof d, "%s/%s", raiz, pastas[k]);
		DIR *dir = opendir(d);
		if (!dir) continue;
		struct dirent *e;
		char nomes[256][256]; int n = 0;
		while ((e = readdir(dir)) && n < 256) {
			size_t l = strlen(e->d_name);
			if (l > 4 && !strcasecmp(e->d_name + l - 4, k ? ".wcm" : ".wmp")) snprintf(nomes[n++], 256, "%s", e->d_name);
		}
		closedir(dir);
		qsort(nomes, (size_t)n, sizeof nomes[0], (int (*)(const void *, const void *))strcmp);
		for (int i = 0; i < n; ++i) {
			if (!k) { printf("Maps/%s\n", nomes[i]); continue; }
			char ruta[1400];
			snprintf(ruta, sizeof ruta, "%s/%s", d, nomes[i]);
			TzHpfs c;
			if (tz_hpfs_abrir(&c, ruta)) continue;
			for (int j = 0; j < c.n; ++j) {
				size_t l = strlen(c.e[j].nome);
				if (l > 4 && !strcasecmp(c.e[j].nome + l - 4, ".wmp")) printf("Campaign/%s|%s\n", nomes[i], c.e[j].nome);
			}
			tz_hpfs_fechar(&c);
		}
	}
	return 0;
}

int main(int argc, char **argv)
{
	if (argc == 3 && !strcmp(argv[2], "--listar")) return listar(argv[1]);
	if (argc < 4) { fprintf(stderr, "uso: %s <instalación> <mapa> <tics> [semente] [--gravar f]\n", argv[0]); return 2; }
	const char *raiz = argv[1], *nome = argv[2], *gravar = NULL, *bioma = NULL;
	int tics = atoi(argv[3]);
	uint32_t semente = 666;
	for (int i = 4; i < argc; ++i) {
		if (!strcmp(argv[i], "--gravar") && i + 1 < argc) gravar = argv[++i];
		else if (!strcmp(argv[i], "--bioma") && i + 1 < argc) bioma = argv[++i];
		else if (!strcmp(argv[i], "--inicio")) inicio = 1;
		else semente = (uint32_t)strtoul(argv[i], NULL, 0);
	}
	static TzRecursos r;
	static TzBiblioteca b;
	static TzCatalogo cat;
	static TzHpfs c, sub;
	static TzMapa mp;
	double t0 = agora();
	if (tz_recursos_abrir(&r, raiz)) { fprintf(stderr, "[SIM] sin datos: %s\n", tz_erro()); return 1; }
	if (abrir_mapa(raiz, nome, &c, &sub, &mp)) { fprintf(stderr, "[SIM] mapa: %s\n", tz_erro()); return 1; }
	if (tz_biblioteca_carregar(&b, &r, bioma ? bioma : mp.bioma)) { fprintf(stderr, "[SIM] biblioteca: %s\n", tz_erro()); return 1; }
	if (tz_catalogo_carregar(&cat, &r, &b)) { fprintf(stderr, "[SIM] catálogo: %s\n", tz_erro()); return 1; }
	printf("[SIM] mapa %s: lado %d objetos %d tecnologias %d ordenes %d carga %.0f ms\n", nome, mp.lado, mp.nobj, cat.ntec, cat.nord, (agora() - t0) * 1000.0);
	Partida p = { &cat, &mp, { 0 }, semente };
	p.regras.populacao = 200;
	static TzMundo m1, m2;
	enum { NS = 4096 };
	static uint32_t s1[NS], s2[NS], s3[NS];
	double ms = 0;
	int n1 = jogar(&p, &m1, tics, s1, NS, NULL, 0, &ms);
	if (n1 < 0) { fprintf(stderr, "[SIM] mundo: %s\n", tz_erro()); return 1; }
	/* resumen de la partida */
	int vivos = 0, unid = 0, edif = 0;
	for (int i = 0; i < m1.nobj; ++i) if (tz_obj_vivo(&m1.obj[i])) { vivos++; unid += tz_obj_unidade(&m1.obj[i]); edif += tz_obj_edificio(&m1.obj[i]); }
	printf("[SIM] %d tics en %.0f ms (%.3f ms por tic): objetos %d vivos %d unidades %d edificios %d proyectiles %d eventos %u\n",
	       tics, ms, tics ? ms / tics : 0.0, m1.nobj, vivos, unid, edif, m1.nproj, m1.ev_seq);
	for (int k = 1; k <= 8; ++k) {
		const TzReinoEstado *rk = &m1.reino[k];
		if (!rk->ativo) continue;
		printf("[SIM] reino %d%s: recursos %d/%d/%d/%d poblacion %u/%u creadas %u perdidas %u matadas %u recogido %u/%u/%u/%u %s\n", k, rk->ia ? " (IA)" : "",
		       rk->recursos[0], rk->recursos[1], rk->recursos[2], rk->recursos[3], rk->populacao, rk->populacao_max, rk->criadas, rk->perdidas, rk->matadas,
		       rk->recolhido[0], rk->recolhido[1], rk->recolhido[2], rk->recolhido[3], rk->derrotado ? "DERROTADO" : "");
	}
	if (m1.acabou) printf("[SIM] fin de la partida: el jugador %s\n", m1.ganhou ? "gana" : "pierde");
	uint32_t final1 = tz_mundo_suma(&m1);
	printf("[SIM] suma final %08x\n", final1);
	/* determinismo */
	int n2 = jogar(&p, &m2, tics, s2, NS, NULL, 0, NULL);
	int iguais = n2 == n1 && !memcmp(s1, s2, (size_t)n1 * sizeof *s1) && tz_mundo_suma(&m2) == final1;
	printf("[SIM] determinismo: %s\n", iguais ? "las dos partidas dan las mismas sumas" : "LAS SUMAS DIFIEREN");
	tz_mundo_liberar(&m2);
	/* guardar y cargar a mitad */
	int ok_gc = 1;
	if (gravar) {
		int mitad = tics / 2;
		int n3 = jogar(&p, &m2, mitad, s3, NS, NULL, 0, NULL);
		(void)n3;
		uint32_t antes = tz_mundo_suma(&m2);
		if (tz_mundo_gravar(&m2, gravar)) { printf("[SIM] guardar: %s\n", tz_erro()); ok_gc = 0; }
		tz_mundo_liberar(&m2);
		static TzMundo m3;
		memset(&m3, 0, sizeof m3);
		if (ok_gc && tz_mundo_carregar(&m3, &cat, &mp, gravar)) { printf("[SIM] cargar: %s\n", tz_erro()); ok_gc = 0; }
		if (ok_gc) {
			TzGancho g; memset(&g, 0, sizeof g);
			g.tic = tz_demo_tic; g.cada = 1; snprintf(g.etiqueta, sizeof g.etiqueta, "PRUEBA");
			tz_mundo_gancho(&m3, &g);
			uint32_t depois = tz_mundo_suma(&m3);
			for (int t = mitad; t < tics; ++t) tz_mundo_tic(&m3);
			uint32_t f3 = tz_mundo_suma(&m3);
			ok_gc = antes == depois && f3 == final1;
			printf("[SIM] guardar/cargar en el tic %d: suma al guardar %08x, al cargar %08x, final %08x: %s\n", mitad, antes, depois, f3,
			       ok_gc ? "igual" : "DIFIERE");
			tz_mundo_liberar(&m3);
		}
	}
	tz_mundo_liberar(&m1);
	tz_catalogo_liberar(&cat);
	tz_biblioteca_liberar(&b);
	tz_mapa_liberar(&mp);
	if (sub.e || sub.pai) tz_hpfs_fechar(&sub);
	tz_hpfs_fechar(&c);
	tz_recursos_fechar(&r);
	return iguais && ok_gc ? 0 : 3;
}
