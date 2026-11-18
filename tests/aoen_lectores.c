/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Arnés de h389 para el anfitrión (con ASan): lectores de DRS/SLP y de partidas de AoEN frente a ficheros manipulados.
 *   aoen_lectores drs FICHERO   abre el DRS y decodifica todos los SLP; imprime «rechazado» o «ok»
 *   aoen_lectores sav-crear RUTA   guarda una partida nueva
 *   aoen_lectores sav-mutar RUTA K  guarda, cambia un campo según K, intenta cargar: debe rechazarla y conservar la actual */
#include "../userland/juegos/aoen/save.c"
#include "../userland/juegos/aoen/ui.h"

int main(int argc, char **argv) {
	if (argc >= 3 && !strcmp(argv[1], "drs")) {
		Drs d;
		if (!drs_open(&d, argv[2])) { puts("rechazado"); return 0; }
		for (int i = 0; i < d.count; ++i) {
			Slp *s = slp_load(&d, d.items[i].id);
			if (s) for (int f = 0; f < s->nframes; ++f) slp_frame(s, f);
			u32 pal[256]; pal_load(&d, d.items[i].id, pal);
		}
		drs_close(&d);
		puts("ok");
		return 0;
	}
	Setup s;
	memset(&s, 0, sizeof s);
	s.width = s.height = 48; s.seed = 5; s.players = 2; s.map_kind = 1; s.popcap = 50; s.villagers = 4;
	for (int i = 0; i < MAX_PLAYERS; ++i) { s.ai_diff[i] = 1; s.civ[i] = i; s.team[i] = i + 1; }
	world_new(&s);
	/* dos transportes, para las mutaciones de la carga (los barcos se colocan donde sea: solo se guardan) */
	int tr1 = dev_place(E_TRANSPORT, 0, 10.5f, 10.5f), tr2 = dev_place(E_TRANSPORT, 0, 12.5f, 10.5f), ald = -1;
	int mer = dev_place(E_TRADESHIP, 0, 14.5f, 10.5f);   /* mercante, para los campos del comercio */
	int mu0 = dev_place(E_DOCK, 0, 20.5f, 20.5f), mu1 = dev_place(E_DOCK, 1, 30.5f, 20.5f);   /* muelles de dos jugadores */
	for (int i = 0; i < W.nents && ald < 0; ++i) if (W.ents[i].alive && W.ents[i].type == E_VILLAGER) ald = i;
	if (argc >= 3 && !strcmp(argv[1], "sav-crear")) { printf("%d\n", world_save(argv[2], 1)); return 0; }
	if (argc >= 4 && !strcmp(argv[1], "sav-mutar")) {
		int k = atoi(argv[3]);
		if (!world_save(argv[2], 1)) return 1;
		FILE *f = fopen(argv[2], "r+b");
		size_t base = sizeof(SaveHeader) + sizeof(Setup) + sizeof(Player) * MAX_PLAYERS + 4 * (size_t)W.w * W.h;   /* primera entidad */
		Entity e;
		fseek(f, (long)base, SEEK_SET);
		fread(&e, sizeof e, 1, f);
		switch (k) {
		case 0: e.type = 9999; break;
		case 1: e.owner = 77; break;
		case 2: e.target.idx = 123456789; break;
		case 3: e.bx = -5; e.size = 3; break;
		case 4: e.x = 1e9f; break;
		case 5: e.ntrain = 99; break;
		case 6: e.carry_kind = 50; break;
		case 7: e.pidx = -3; break;
		case 8: e.state = 99; break;
		case 12: e.alive = 1; e.type = E_CATAPULT; e.owner = 0; e.nwp = 9; break;     /* puntos de ruta de más */
		case 13: e.alive = 1; e.type = E_CATAPULT; e.owner = 0; e.patrol = 7; break;
		case 14: e.alive = 1; e.type = E_VILLAGER; e.owner = 0; e.wpx[2] = 1e9f; break;
		case 9: e.alive = 1; e.type = E_VILLAGER; e.owner = 0; e.plen = 0; e.pidx = 5; break;
		}
		fseek(f, (long)base, SEEK_SET);
		fwrite(&e, sizeof e, 1, f);
		if (k == 10) {   /* jugador con civilización fuera de rango */
			Player p; fseek(f, (long)(sizeof(SaveHeader) + sizeof(Setup)), SEEK_SET); fread(&p, sizeof p, 1, f);
			p.civ = 99; fseek(f, (long)(sizeof(SaveHeader) + sizeof(Setup)), SEEK_SET); fwrite(&p, sizeof p, 1, f);
		}
		if (k >= 15 && k <= 17) {   /* campos nuevos de Setup y Player */
			Setup st; fseek(f, (long)sizeof(SaveHeader), SEEK_SET); fread(&st, sizeof st, 1, f);
			if (k == 15) st.victory = 99; else if (k == 16) st.color[1] = -3; else st.time_limit = 100000;
			fseek(f, (long)sizeof(SaveHeader), SEEK_SET); fwrite(&st, sizeof st, 1, f);
		}
		if (k == 18) { Player p; long o = (long)(sizeof(SaveHeader) + sizeof(Setup)); fseek(f, o, SEEK_SET); fread(&p, sizeof p, 1, f); p.dipl[1] = 9; fseek(f, o, SEEK_SET); fwrite(&p, sizeof p, 1, f); }
		if (k == 19) { Player p; long o = (long)(sizeof(SaveHeader) + sizeof(Setup)); fseek(f, o, SEEK_SET); fread(&p, sizeof p, 1, f); p.hist_n = 5000; fseek(f, o, SEEK_SET); fwrite(&p, sizeof p, 1, f); }
		if (k == 11) { fseek(f, 8 + 4 + 8 + 12, SEEK_SET); int w = 100000; fwrite(&w, 4, 1, f); }   /* w enorme en la cabecera */
		if (k >= 25 && k <= 27) {   /* dificultades e historia fuera de rango */
			if (k == 25) { Setup st; fseek(f, (long)sizeof(SaveHeader), SEEK_SET); fread(&st, sizeof st, 1, f); st.ai_diff[1] = 4; fseek(f, (long)sizeof(SaveHeader), SEEK_SET); fwrite(&st, sizeof st, 1, f); }
			else {
				Player p; long o = (long)(sizeof(SaveHeader) + sizeof(Setup) + sizeof(Player)); fseek(f, o, SEEK_SET); fread(&p, sizeof p, 1, f);
				if (k == 26) p.diff = 4; else p.hist_n = -1;
				fseek(f, o, SEEK_SET); fwrite(&p, sizeof p, 1, f);
			}
		}
		if (k >= 38 && k <= 40) {   /* opciones nuevas de Setup fuera de rango */
			Setup st; fseek(f, (long)sizeof(SaveHeader), SEEK_SET); fread(&st, sizeof st, 1, f);
			if (k == 38) st.victoria_compartida = 3; else if (k == 39) st.recursos_mapa = 7; else st.comercio_mejorado = 2;
			fseek(f, (long)sizeof(SaveHeader), SEEK_SET); fwrite(&st, sizeof st, 1, f);
		}
		if (k >= 41 && k <= 53) {   /* el mercante */
			long o = (long)base;
			for (int i = 0; i < mer; ++i) { Entity t; int plen; fseek(f, o, SEEK_SET); fread(&t, sizeof t, 1, f); fread(&plen, sizeof plen, 1, f); o += (long)sizeof t + 4 + 4L * plen; }
			Entity m; fseek(f, o, SEEK_SET); fread(&m, sizeof m, 1, f);
			switch (k) {
			case 41: m.com_modo = 5; break;
			case 42: m.com_fase = 7; break;
			case 43: m.com_dar = -1; break;
			case 44: m.com_recibir = 9; break;
			case 45: m.com_origen.idx = 99999; break;
			case 46: m.com_destino.idx = -7; break;
			case 47: m.carry = 99; break;
			case 48: m.com_dado = 4; break;
			case 50: m.com_origen = (Ref){ tr1, W.ents[tr1].gen }; break;                  /* origen que no es un muelle */
			case 51: m.com_origen = (Ref){ mu1, W.ents[mu1].gen }; m.com_destino = (Ref){ mu1, W.ents[mu1].gen }; break;   /* origen ajeno */
			case 52: m.com_origen = (Ref){ mu0, W.ents[mu0].gen }; m.com_destino = (Ref){ mu0, W.ents[mu0].gen }; break;   /* destino propio */
			case 53: m.com_origen = (Ref){ mu0, W.ents[mu0].gen }; m.com_destino = (Ref){ mu1, W.ents[mu1].gen }; m.com_fase = 1; break;   /* válida */
			}
			fseek(f, o, SEEK_SET); fwrite(&m, sizeof m, 1, f);
		}
		if (((k >= 20 && k <= 37) || k == 49) && k != 25 && k != 26 && k != 27) {
			/* entidades distintas de la 0: se busca su posición saltando las anteriores (cada una lleva detrás su ruta) */
			long pos[3]; int idx[3] = { ald, tr1, tr2 };
			for (int q = 0; q < 3; ++q) {
				long o = (long)base;
				for (int i = 0; i < idx[q]; ++i) { Entity t; int plen; fseek(f, o, SEEK_SET); fread(&t, sizeof t, 1, f); fread(&plen, sizeof plen, 1, f); o += (long)sizeof t + 4 + 4L * plen; }
				pos[q] = o;
			}
			Entity v, a, b;
			fseek(f, pos[0], SEEK_SET); fread(&v, sizeof v, 1, f);
			fseek(f, pos[1], SEEK_SET); fread(&a, sizeof a, 1, f);
			fseek(f, pos[2], SEEK_SET); fread(&b, sizeof b, 1, f);
			switch (k) {
			case 20: v.ncargo = 1; v.cargo[0].idx = 999999; break;                           /* carga en un aldeano y fuera de rango */
			case 21: a.ncargo = 7; break;                                                     /* más de 5 pasajeros */
			case 22: a.unload = 2; break;
			case 23: v.ptx = 1e9f; break;
			case 24: v.state = ST_ABOARD + 1; break;
			case 28: a.ncargo = 1; a.cargo[0] = (Ref){ tr2, b.gen }; b.ncargo = 1; b.cargo[0] = (Ref){ tr1, a.gen }; break;   /* ciclo */
			case 29: a.ncargo = 1; a.cargo[0] = (Ref){ tr1, a.gen }; break;                  /* a sí mismo */
			case 30: v.relics = 100000; break;
			case 31: v.hx = NAN; break;
			case 32: v.racc = NAN; break;
			case 33: v.state = ST_ABOARD; v.target = (Ref){ 0, 0 }; break;                   /* a bordo de algo que no es un transporte */
			case 34: a.ncargo = 2; a.cargo[0] = (Ref){ ald, v.gen }; a.cargo[1] = (Ref){ ald, v.gen }; break;   /* el mismo dos veces */
			case 35: v.rx = 1e30f; break;
			case 49: v.com_destino = (Ref){ tr1, a.gen }; break;   /* ruta comercial en algo que no es un mercante */
			case 36: a.ncargo = 1; a.cargo[0] = (Ref){ ald, v.gen }; v.state = ST_ABOARD; v.target = (Ref){ tr1, a.gen }; break;   /* válido: debe cargar */
			case 37: break;   /* sin cambios: debe cargar */
			}
			fseek(f, pos[0], SEEK_SET); fwrite(&v, sizeof v, 1, f);
			fseek(f, pos[1], SEEK_SET); fwrite(&a, sizeof a, 1, f);
			fseek(f, pos[2], SEEK_SET); fwrite(&b, sizeof b, 1, f);
		}
		fclose(f);
		int nents = W.nents;
		int r = world_load(argv[2]);
		printf("%s %s\n", r ? "cargada" : "rechazada", W.nents == nents && W.ents ? "actual-intacta" : "actual-PERDIDA");
		return 0;
	}
	return 2;
}
