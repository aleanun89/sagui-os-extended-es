/* SPDX-License-Identifier: GPL-3.0-or-later */
/* demo.c - un jugador automático muy simple para probar la simulación de Tzar sin la IA original (D417, fase 5):
 * a todos los reinos, cada 160 tics (escalonado), les hace un castillo si no tienen, casas cuando falta población,
 * manda los peones libres a la mina, al campo o al árbol más cercano, entrena peones en el castillo y lanza a los
 * soldados libres contra lo enemigo visible más cercano. Solo usa consultas y comandos de mundo.h, como un jugador.
 * Lo usan tests/tzar_simulacao.c y el visor con TZAR_DEMO=1. La IA de verdad (guiones *.ais) es la fase 6.
 * Código propio (no sale de ReTzar).
 */
#include "partida.h"
#include <stdlib.h>
#include <string.h>

int tz_demo_so_reino;

void tz_demo_tic(TzMundo *m, void *dados)
{
	(void)dados;
	for (int k = 1; k <= 8; ++k) {
		if (!m->reino[k].ativo || m->reino[k].derrotado) continue;
		if (tz_demo_so_reino && k != tz_demo_so_reino) continue;
		if ((m->tic / 20) % 8 != (uint32_t)(k - 1)) continue;   /* cada reino piensa una vez cada 160 tics */
		if (m->tic % 20) continue;
		TzId peoes[TZ_CMD_IDS], soldados[TZ_CMD_IDS], castelo = TZ_NENHUM;
		int np = 0, ns = 0;
		for (int i = 0; i < m->nobj; ++i) {
			const TzObj *o = &m->obj[i];
			if (o->dono != k || !tz_obj_vivo(o) || !o->ator) continue;
			if (o->classe == TZC_CASTLE && !(o->flags & TZ_F_OBRA)) castelo = i;
			if (o->classe == TZC_PEON && o->ator->ordem.tipo == TZ_O_NADA && np < TZ_CMD_IDS) peoes[np++] = i;
			else if (tz_obj_unidade(o) && o->ataque && o->classe != TZC_PEON && o->ator->ordem.tipo == TZ_O_NADA && ns < TZ_CMD_IDS) soldados[ns++] = i;
		}
		TzComando c;
		/* sin castillo (ni en obras): todos los peones libres a hacer uno */
		int tem_castelo = 0;
		for (int i = 0; i < m->nobj && !tem_castelo; ++i) if (m->obj[i].dono == k && m->obj[i].classe == TZC_CASTLE && tz_obj_vivo(&m->obj[i])) tem_castelo = 1;
		if (!tem_castelo && np) {
			const TzObj *p0 = &m->obj[peoes[0]];
			memset(&c, 0, sizeof c);
			c.reino = (uint8_t)k; c.tipo = TZ_C_CONSTRUIR; c.arg = TZC_CASTLE; c.nids = np;
			memcpy(c.ids, peoes, (size_t)np * sizeof *peoes);
			int ok = 0;
			for (int d = 2; d < 20 && !ok; ++d)
				for (int t = 0; t < 9 && !ok; ++t) {
					int x = p0->cx + (t % 3 - 1) * d - 3, y = p0->cy + (t / 3 - 1) * d - 3;
					if (!tz_mundo_cabe(m, k, TZC_CASTLE, x, y)) { c.x = (int16_t)x; c.y = (int16_t)y; ok = 1; }
				}
			if (ok) { tz_mundo_comando(m, &c); continue; }
		}
		/* peones: el primero, una casa si hace falta; los demás, a la mina, campo o árbol más cercano */
		for (int q = 0; q < np; ++q) {
			const TzObj *p = &m->obj[peoes[q]];
			memset(&c, 0, sizeof c);
			c.reino = (uint8_t)k; c.nids = 1; c.ids[0] = peoes[q];
			if (q == 0 && m->reino[k].populacao + 2 >= m->reino[k].populacao_max && m->reino[k].recursos[TZ_MADEIRA] >= 100) {
				c.tipo = TZ_C_CONSTRUIR; c.arg = TZC_HOUSE;
				int ok = 0;
				for (int d = 3; d < 12 && !ok; ++d)
					for (int t = 0; t < 8 && !ok; ++t) {
						int x = p->cx + (t % 3 - 1) * d, y = p->cy + (t / 3 - 1) * d;
						if (!tz_mundo_cabe(m, k, TZC_HOUSE, x, y)) { c.x = (int16_t)x; c.y = (int16_t)y; ok = 1; }
					}
				if (ok) { tz_mundo_comando(m, &c); continue; }
			}
			TzId alvo = TZ_NENHUM; int dm = 1 << 30;
			TzId viz[256];
			int nv = tz_mundo_area(m, p->cx - 20, p->cy - 20, p->cx + 20, p->cy + 20, TZ_FILTRO_RECURSOS | TZ_FILTRO_EDIFICIOS, -1, viz, 256);
			for (int v = 0; v < nv; ++v) {
				const TzObj *t = &m->obj[viz[v]];
				int bom = t->classe == TZC_RES_GOLD || t->classe == TZC_RES_STONE || (t->classe == TZC_FIELD && t->dono == k);
				if (!bom || !tz_mundo_ve(m, k, viz[v])) continue;
				int d = abs(t->cx - p->cx) + abs(t->cy - p->cy) + (int)(viz[v] % 7) * 0;
				if (d < dm) { dm = d; alvo = viz[v]; }
			}
			c.tipo = TZ_C_ORDEM;
			c.ordem = tz_mundo_ordem_padrao(m, k, &peoes[q], 1, alvo >= 0 ? m->obj[alvo].cx : p->cx, alvo >= 0 ? m->obj[alvo].cy : p->cy, alvo);
			if (alvo < 0) {
				/* sin mina: a talar (árbol más cercano en espiral) */
				for (int d = 1; d < 40 && c.ordem.tipo != TZ_O_CORTAR; ++d)
					for (int dy = -d; dy <= d && c.ordem.tipo != TZ_O_CORTAR; ++dy)
						for (int dx = -d; dx <= d; ++dx) {
							int x = p->cx + dx, y = p->cy + dy;
							if (x < 0 || y < 0 || x >= m->lado || y >= m->lado) continue;
							if (m->cel[y * m->lado + x].bits & TZ_CEL_ARVORE) { c.ordem.tipo = TZ_O_CORTAR; c.ordem.x = (int16_t)x; c.ordem.y = (int16_t)y; c.ordem.alvo = TZ_NENHUM; break; }
						}
			}
			if (c.ordem.tipo != TZ_O_MOVER) tz_mundo_comando(m, &c);
		}
		/* economía y ejército: granja, cuartel, campos; soldados en el cuartel */
		TzId cuartel = TZ_NENHUM;
		int tem_granja = 0, tem_cuartel = 0, campos = 0;
		for (int i = 0; i < m->nobj; ++i) {
			const TzObj *o = &m->obj[i];
			if (o->dono != k || !tz_obj_vivo(o)) continue;
			if (o->classe == TZC_FARM) tem_granja = 1;
			if (o->classe == TZC_FIELD) campos++;
			if (o->classe == TZC_BARRACK) { tem_cuartel = 1; if (!(o->flags & TZ_F_OBRA)) cuartel = i; }
		}
		if (castelo >= 0 && np >= 2) {
			int quer = !tem_granja ? TZC_FARM : !tem_cuartel ? TZC_BARRACK : (campos < 4 && m->reino[k].recursos[TZ_MADEIRA] >= 300) ? TZC_FIELD : -1;
			uint32_t custo[TZ_NREC], t;
			if (quer > 0) {
				tz_mundo_custo_classe(m, k, quer, custo, &t);
				int pode = 1;
				for (int j = 0; j < TZ_NREC; ++j) if ((uint32_t)m->reino[k].recursos[j] < custo[j]) pode = 0;
				const TzObj *cs = &m->obj[castelo];
				for (int d = 8; d < 24 && pode; ++d)
					for (int t2 = 0; t2 < 8; ++t2) {
						int x = cs->cx + 3 + (t2 % 3 - 1) * d, y = cs->cy + 3 + (t2 / 3 - 1) * d;
						if (t2 == 4 || tz_mundo_cabe(m, k, quer, x, y)) continue;
						memset(&c, 0, sizeof c);
						c.reino = (uint8_t)k; c.tipo = TZ_C_CONSTRUIR; c.arg = (int16_t)quer; c.x = (int16_t)x; c.y = (int16_t)y;
						c.nids = 2; c.ids[0] = peoes[np - 1]; c.ids[1] = peoes[np - 2];
						tz_mundo_comando(m, &c);
						np -= 2; pode = 0;
						break;
					}
			}
		}
		if (cuartel >= 0 && m->obj[cuartel].ator->ntrab < 2 && m->reino[k].recursos[TZ_COMIDA] >= 150) {
			memset(&c, 0, sizeof c);
			c.reino = (uint8_t)k; c.tipo = TZ_C_PRODUZIR; c.arg = (int16_t)(m->tic / 160 % 2 ? 4 : 3); c.nids = 1; c.ids[0] = cuartel;
			tz_mundo_comando(m, &c);
		}
		/* el castillo: un peón si hay comida */
		if (castelo >= 0 && m->obj[castelo].ator->ntrab < 2) {
			memset(&c, 0, sizeof c);
			c.reino = (uint8_t)k; c.tipo = TZ_C_PRODUZIR; c.arg = TZC_PEON; c.nids = 1; c.ids[0] = castelo;
			tz_mundo_comando(m, &c);
		}
		/* soldados: al enemigo visible más cercano (o a explorar hacia el centro) */
		if (ns) {
			TzId alvo = TZ_NENHUM; int dm = 1 << 30;
			const TzObj *s0 = &m->obj[soldados[0]];
			for (int i = 0; i < m->nobj; ++i) {
				const TzObj *t = &m->obj[i];
				if (!tz_obj_vivo(t) || t->dono == 0 || t->dono == k || tz_mundo_relacao(m, k, t->dono) >= 0) continue;
				if (!tz_obj_unidade(t) && !tz_obj_edificio(t)) continue;
				if (!tz_mundo_ve(m, k, i)) continue;
				int d = abs(t->cx - s0->cx) + abs(t->cy - s0->cy);
				if (d < dm) { dm = d; alvo = i; }
			}
			memset(&c, 0, sizeof c);
			c.reino = (uint8_t)k; c.tipo = TZ_C_ORDEM; c.nids = ns;
			memcpy(c.ids, soldados, (size_t)ns * sizeof *soldados);
			c.ordem.alvo = alvo;
			if (alvo >= 0) { c.ordem.tipo = TZ_O_ATACAR; c.ordem.x = m->obj[alvo].cx; c.ordem.y = m->obj[alvo].cy; }
			else { c.ordem.tipo = TZ_O_MOVER; c.ordem.x = (int16_t)(m->lado / 2); c.ordem.y = (int16_t)(m->lado / 2); }
			tz_mundo_comando(m, &c);
		}
	}
}

