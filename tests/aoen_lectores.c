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
		case 9: e.alive = 1; e.type = E_VILLAGER; e.owner = 0; e.plen = 0; e.pidx = 5; break;
		}
		fseek(f, (long)base, SEEK_SET);
		fwrite(&e, sizeof e, 1, f);
		if (k == 10) {   /* jugador con civilización fuera de rango */
			Player p; fseek(f, (long)(sizeof(SaveHeader) + sizeof(Setup)), SEEK_SET); fread(&p, sizeof p, 1, f);
			p.civ = 99; fseek(f, (long)(sizeof(SaveHeader) + sizeof(Setup)), SEEK_SET); fwrite(&p, sizeof p, 1, f);
		}
		if (k == 11) { fseek(f, 8 + 4 + 8 + 12, SEEK_SET); int w = 100000; fwrite(&w, 4, 1, f); }   /* w enorme en la cabecera */
		fclose(f);
		int nents = W.nents;
		int r = world_load(argv[2]);
		printf("%s %s\n", r ? "cargada" : "rechazada", W.nents == nents && W.ents ? "actual-intacta" : "actual-PERDIDA");
		return 0;
	}
	return 2;
}
