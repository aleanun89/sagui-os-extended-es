/* SPDX-License-Identifier: GPL-3.0-or-later */
/* tzar_ia_prova.c - pruebas de unidad de la IA de Tzar en el PC (D417, fase 6; tests/h417_tzar.py «ia»).
 *
 * Sin datos del juego: guiones sintéticos escritos aquí (los casos de src/gameplay/ai/script_tests.rs de ReTzar,
 * GPL-3.0-or-later) y, para los gestores, un mundo de prueba (tzar_ia_mundo.c). Con «real <carpeta>» compila todos los
 * guiones de una instalación real (comprobación PRIVADA, nunca en la batería).
 */
#include "ia/ia.h"
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <strings.h>

static int falhas, provas;
#define VER(c) do { provas++; if (!(c)) { falhas++; printf("FALLA %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

static TzIaBiblioteca bib;
static const char *const EXTRA[] = { "to_wtower2", "te_feudalism" };

static void bib_nova(void)
{
	tz_ia_bib_liberar(&bib);
	tz_ia_bib_iniciar(&bib, EXTRA, 2);
}

static void fich(const char *nome, const char *texto)
{
	char ruta[128];
	snprintf(ruta, sizeof ruta, "data\\ai\\%s", nome);
	tz_ia_bib_ficheiro(&bib, ruta, (const uint8_t *)texto, (uint32_t)strlen(texto));
}

static TzIaPrograma prog;
static TzIaErro erro;

static int compila(const char *texto)
{
	tz_ia_programa_liberar(&prog);
	bib_nova();
	fich("test.ais", texto);
	return tz_ia_compilar(&bib, "test.ais", &prog, &erro);
}

static const char *erro_de(const char *texto)
{
	if (!compila(texto)) return "(compila)";
	return erro.mensagem;
}

static int cond_de(int regra, int j, TzIaCond *out)
{
	TzIaTrecho t = prog.regra[regra].quando;
	if ((uint32_t)j >= t.n || t.ini + j >= (uint32_t)prog.nlista) return 0;
	*out = prog.cond[prog.lista[t.ini + j]];
	return 1;
}

static int cond_e(int regra, int j, int tipo, int recurso, int classe, uint32_t n)
{
	TzIaCond c;
	if (!cond_de(regra, j, &c)) return 0;
	return c.tipo == tipo && c.recurso == recurso && c.classe == classe && c.n == n;
}

enum { PEON = 1, ARCHER = 4, BARRACK = 45 };

static void prova_compilador(void)
{
	/* las cláusulas en cualquier orden */
	VER(!compila("; un comentario\n"
	             "rule First when gold 100, archers 5 have 2 barracks trigger Army, !Peace say \"Ready\" end\n"
	             "critical rule have peon end\n"
	             "rule forget this, First end"));
	VER(prog.nregras == 3);
	VER(prog.regra[0].nome && !strcmp(prog.regra[0].nome, "FIRST"));
	VER(prog.regra[0].flags == TZ_IA_R_SUJA);
	VER(prog.regra[0].quando.n == 2);
	VER(cond_e(0, 0, TZ_IA_C_RECURSO, 0, 0, 100));
	VER(cond_e(0, 1, TZ_IA_C_CONTA, 0, ARCHER, 5));
	VER(prog.regra[0].tem_meta && prog.regra[0].meta_tipo == BARRACK && prog.regra[0].meta_n == 2);
	VER(prog.regra[0].diz && !strcmp(prog.regra[0].diz, "READY"));
	VER(prog.regra[0].gatilhos.n == 2 && prog.lista[prog.regra[0].gatilhos.ini] == 0 &&
	    prog.lista[prog.regra[0].gatilhos.ini + 1] == (1u | 0x80000000u));
	VER(prog.ngat == 2 && !strcmp(prog.gat[0].nome, "ARMY") && !strcmp(prog.gat[1].nome, "PEACE"));
	VER(prog.regra[1].flags == (TZ_IA_R_SUJA | TZ_IA_R_CRITICA));
	VER(prog.regra[1].meta_tipo == PEON && prog.regra[1].meta_n == 1 && !prog.regra[1].nome);
	VER(prog.regra[2].esquece.n == 2 && prog.lista[prog.regra[2].esquece.ini] == 2 &&
	    prog.lista[prog.regra[2].esquece.ini + 1] == 0);

	/* los argumentos de las condiciones */
	VER(!compila("rule when units, units 30, peon 40%, time 60000, time 5%, random 40%, "
	             "civ asian, difficulty normal, enemy archers, ally units 3, extern \"$aik == 2\" end"));
	VER(cond_e(0, 0, TZ_IA_C_CONTA, 0, 0, 1));
	VER(cond_e(0, 1, TZ_IA_C_CONTA, 0, 0, 30));
	VER(cond_e(0, 2, TZ_IA_C_CONTA, 0, PEON, 40 | TZ_IA_PCT));
	VER(cond_e(0, 3, TZ_IA_C_TEMPO, 0, 0, 60000));
	VER(cond_e(0, 4, TZ_IA_C_TEMPO, 0, 0, 5 | TZ_IA_PCT));
	VER(cond_e(0, 5, TZ_IA_C_ACASO, 0, 0, 40));
	VER(cond_e(0, 6, TZ_IA_C_CIV, 3, 0, 0));
	VER(cond_e(0, 7, TZ_IA_C_DIFICULDADE, 1, 0, 0));
	VER(cond_e(0, 8, TZ_IA_C_INIMIGO, 0, ARCHER, 1));
	VER(cond_e(0, 9, TZ_IA_C_ALIADO, 0, 0, 3));
	VER(cond_e(0, 10, TZ_IA_C_EXTERNO, 0, 0, 0));
	VER(prog.ntextos == 1 && !strcmp(prog.texto[0], "$AIK == 2"));
	VER(!compila("rule have 40000% peon end") && prog.regra[0].meta_n == 0xffff);
	VER(!compila("rule when gold 70000 have 70000 gold end"));
	VER(cond_e(0, 0, TZ_IA_C_RECURSO, 0, 0, 70000));
	VER(prog.regra[0].meta_tipo == TZ_IA_M_OURO && prog.regra[0].meta_n == (uint16_t)70000u);

	/* la negación ocupa el lugar de su condición */
	VER(!compila("rule when gold 5, not trigger T, not not stone 7 trigger T end"));
	VER(prog.nconds == 6);
	VER(prog.cond[2].tipo == TZ_IA_C_NAO && prog.cond[2].n == 1);
	VER(prog.cond[4].tipo == TZ_IA_C_NAO && prog.cond[4].n == 3 && prog.cond[5].tipo == TZ_IA_C_NAO && prog.cond[5].n == 4);
	VER(prog.regra[0].quando.n == 3 && prog.lista[0] == 0 && prog.lista[1] == 2 && prog.lista[2] == 5);

	/* lo que sigue a una condición, si no es palabra clave, la separa */
	VER(!compila("rule when gold 1 } stone 2 3 food 4 end") && prog.regra[0].quando.n == 3);
	VER(!strcmp(erro_de("rule when gold 1 stone 2 end"), "Identifier expected"));

	/* los nombres enteros o sin la S final */
	VER(!compila("rule when barracks, peons, te_feudalism have 2 to_wtower2 end"));
	VER(cond_e(0, 0, TZ_IA_C_CONTA, 0, BARRACK, 1));
	VER(cond_e(0, 1, TZ_IA_C_CONTA, 0, PEON, 1));
	VER(cond_e(0, 2, TZ_IA_C_CONTA, 0, TZ_NCLASSES + 1, 1));
	VER(prog.regra[0].meta_tipo == TZ_NCLASSES);
	VER(!compila("rule have convert end rule have 5 stone end rule have needs_locked end rule have resign end"));
	VER(prog.regra[0].meta_tipo == TZ_IA_M_CONVERTER && prog.regra[1].meta_tipo == TZ_IA_M_PEDRA &&
	    prog.regra[2].meta_tipo == TZ_IA_M_BLOQUEIO && prog.regra[3].meta_tipo == TZ_IA_M_RENDER);
	VER(!strcmp(erro_de("rule have none end"), "Invalid type"));
	VER(!strcmp(erro_de("rule have wizard end"), "Invalid type"));
	VER(!strcmp(erro_de("rule have units end"), "Invalid type"));
	VER(!strcmp(erro_de("rule when wizard end"), "Bad condition"));
	VER(!strcmp(erro_de("rule have rule end"), "Unexpected keyword"));

	/* reglas y gatillos nombrados antes de existir */
	VER(!compila("rule when rule Later, trigger Flag forget Later end\nrule Later trigger Flag end"));
	VER(cond_e(0, 0, TZ_IA_C_REGRA, 0, 0, 1) && cond_e(0, 1, TZ_IA_C_GATILHO, 0, 0, 0));
	VER(prog.lista[prog.regra[0].esquece.ini] == 1);
	VER(!strcmp(erro_de("rule when rule Nowhere end"), "Unresolved rule"));
	VER(!strcmp(erro_de("rule forget Nowhere end"), "Unresolved rule"));
	VER(!strcmp(erro_de("rule when trigger Nowhere end"), "Unresolved trigger"));
	VER(!strcmp(erro_de("rule Me when rule Me end"), "Recursed rule"));
	VER(!strcmp(erro_de("rule Twice end rule Twice end"), "Rule name already used"));

	/* grupos: marcan su final y los vacíos desaparecen */
	VER(!compila("{ critical rule when gold 1 end\n { }\n { rule A end }\n}\nrule B when rule A end"));
	{
		static const uint32_t esperado[7] = { TZ_IA_R_GRUPO_INI, TZ_IA_R_SUJA | TZ_IA_R_CRITICA, TZ_IA_R_GRUPO_INI,
		                                      TZ_IA_R_SUJA, TZ_IA_R_GRUPO_FIM, TZ_IA_R_GRUPO_FIM, TZ_IA_R_SUJA };
		VER(prog.nregras == 7);
		for (int i = 0; i < 7 && i < prog.nregras; ++i) VER(prog.regra[i].flags == esperado[i]);
		VER(prog.regra[0].fim == 5 && prog.regra[2].fim == 4);
		VER(cond_e(6, 0, TZ_IA_C_REGRA, 0, 0, 3));
	}
	VER(!strcmp(erro_de("}"), "Unexpected \"}\""));
	{
		char muitos[100] = "";
		for (int i = 0; i < 33; ++i) strcat(muitos, "{ ");
		VER(!strcmp(erro_de(muitos), "Too deeep group nesting"));
	}
	VER(!compila("{ rule end rule end") && prog.regra[0].fim == 3);

	/* una cláusula repetida empieza otro tramo pero sigue contando */
	VER(!compila("rule trigger A trigger B end rule when gold 3 end"));
	VER(prog.regra[0].gatilhos.ini == 1 && prog.regra[0].gatilhos.n == 2);
	VER(prog.lista[1] == 1 && prog.lista[2] == 0);
	VER(!compila("rule have peon say \"A\" have 3 archer say \"B\" end"));
	VER(prog.regra[0].meta_tipo == ARCHER && prog.regra[0].meta_n == 3 && !strcmp(prog.regra[0].diz, "B"));

	/* errores */
	VER(!strcmp(erro_de("when"), "Statement expected"));
	VER(!strcmp(erro_de("rule 5 end"), "Identifier expected"));
	VER(!strcmp(erro_de("rule Name 5 end"), "Keyword expected"));
	VER(!strcmp(erro_de("rule rule end"), "Statement expected"));
	VER(!strcmp(erro_de("critical when"), "RULE expected"));
	VER(!strcmp(erro_de("rule when gold 5% end"), "Number expected"));
	VER(!strcmp(erro_de("rule when time end"), "Number expected"));
	VER(!strcmp(erro_de("rule when civ french end"), "Invalid civilization"));
	VER(!strcmp(erro_de("rule when difficulty easy2 end"), "Invalid difficulty"));
	VER(!strcmp(erro_de("rule say Hello end"), "Quote (\") expected"));
	VER(!strcmp(erro_de("rule say \"Hello\nend"), "Quote (\") expected"));
	VER(!strcmp(erro_de("rule when gold 1 & end"), "Unrecognized token"));
	VER(!strcmp(erro_de("rule trigger end end"), "Unexpected keyword"));
	VER(!compila("") && prog.nregras == 0);

	/* INCLUDE se lee junto al fichero que incluye */
	tz_ia_programa_liberar(&prog);
	bib_nova();
	fich("main.ais", "rule A end\ninclude \"part.ai\"\nrule when rule B end");
	fich("part.ai", "rule B trigger T end\ninclude \"deeper.ai\"");
	fich("deeper.ai", "rule when trigger T end");
	fich("broken.ais", "rule A end\n\n  include \"bad.ai\"");
	fich("bad.ai", "rule C end\nrule\n  when bogus end");
	VER(!tz_ia_compilar(&bib, "main.ais", &prog, &erro));
	VER(prog.nregras == 4 && !strcmp(prog.regra[0].nome, "A") && !strcmp(prog.regra[1].nome, "B") && !prog.regra[2].nome);
	VER(cond_e(3, 0, TZ_IA_C_REGRA, 0, 0, 1));
	tz_ia_programa_liberar(&prog);
	VER(tz_ia_compilar(&bib, "broken.ais", &prog, &erro) == -1);
	VER(!strcmp(erro.ficheiro, "data\\ai\\BAD.AI") && erro.linha == 3 && erro.coluna == 8 &&
	    !strcmp(erro.mensagem, "Bad condition"));
	VER(strstr(erro.incluido, "data\\ai\\broken.ais:3") != NULL);
	VER(tz_ia_compilar(&bib, "missing.ais", &prog, &erro) == -1 && !strcmp(erro.mensagem, "File not found"));
	VER(!tz_ia_compilar(&bib, "DATA\\AI\\MAIN.AIS", &prog, &erro));
	tz_ia_programa_liberar(&prog);
}

/* ------------------------------------------------------------------ privado: los guiones de una instalación real */
static int guioes_reais(const char *raiz)
{
	TzRecursos r;
	if (tz_recursos_abrir(&r, raiz)) { printf("no se puede abrir %s: %s\n", raiz, tz_erro()); return 1; }
	TzIaBiblioteca b;
	if (tz_ia_bib_carregar(&b, &r)) { printf("biblioteca: %s\n", tz_erro()); return 1; }
	int n = 0, mal = 0;
	for (int i = 0; i < b.nfich; ++i) {
		size_t l = strlen(b.nome[i]);
		if (l < 4 || strcmp(b.nome[i] + l - 4, ".AIS")) continue;
		TzIaPrograma p;
		TzIaErro e;
		n++;
		if (tz_ia_compilar(&b, b.nome[i], &p, &e)) {
			char t[600];
			tz_ia_erro_texto(&e, t, sizeof t);
			printf("FALLA %s\n", t);
			mal++;
			continue;
		}
		if (p.ngat >= 256) { printf("FALLA %s: %d gatillos (el original los cuenta en un byte)\n", b.nome[i], p.ngat); mal++; }
		printf("%-26s reglas %4d condiciones %4d gatillos %3d\n", b.nome[i], p.nregras, p.nconds, p.ngat);
		tz_ia_programa_liberar(&p);
	}
	/* los guiones que nombran los reinos de los mapas (sueltos y de las campañas) */
	int nmapas = 0, nomeados = 0;
	static const char *const pastas[2] = { "Maps", "Campaign" };
	for (int pz = 0; pz < 2; ++pz) {
		char dir[1024];
		snprintf(dir, sizeof dir, "%s/%s", raiz, pastas[pz]);
		DIR *dd = opendir(dir);
		struct dirent *de;
		while (dd && (de = readdir(dd))) {
			size_t l = strlen(de->d_name);
			if (l < 4 || (strcasecmp(de->d_name + l - 4, ".wmp") && strcasecmp(de->d_name + l - 4, ".wcm"))) continue;
			char ruta[1400];
			snprintf(ruta, sizeof ruta, "%s/%s", dir, de->d_name);
			TzHpfs c;
			memset(&c, 0, sizeof c);
			if (tz_hpfs_abrir(&c, ruta)) continue;
			int camp = !strcasecmp(de->d_name + l - 4, ".wcm");
			for (int e = 0; e < (camp ? c.n : 1); ++e) {
				TzHpfs sub;
				TzHpfs *alvo = &c;
				memset(&sub, 0, sizeof sub);
				if (camp) {
					size_t le = strlen(c.e[e].nome);
					if (le < 4 || strcasecmp(c.e[e].nome + le - 4, ".wmp") || tz_hpfs_anidado(&c, c.e[e].nome, &sub)) continue;
					alvo = &sub;
				}
				TzMapa mp;
				memset(&mp, 0, sizeof mp);
				if (!tz_mapa_ler(alvo, &mp)) {
					nmapas++;
					for (int k = 0; k < mp.nreinos; ++k) {
						if (!mp.reinos[k].ativo || !mp.reinos[k].ia || !(mp.reinos[k].ia_modo & 3)) continue;
						const char *g = mp.reinos[k].ia_guiao[0] ? mp.reinos[k].ia_guiao : "random.ais";
						TzIaPrograma p;
						TzIaErro er;
						nomeados++;
						if (tz_ia_compilar(&b, g, &p, &er)) {
							char t[600];
							tz_ia_erro_texto(&er, t, sizeof t);
							printf("FALLA %s (%s): %s\n", de->d_name, camp ? c.e[e].nome : "", t);
							mal++;
						} else tz_ia_programa_liberar(&p);
					}
					tz_mapa_liberar(&mp);
				}
				if (camp) tz_hpfs_fechar(&sub);
			}
			tz_hpfs_fechar(&c);
		}
		if (dd) closedir(dd);
	}
	printf("mapas reales: %d, reinos con IA y guion: %d\n", nmapas, nomeados);
	printf("guiones reales: %d compilados, %d fallos (tipos %d, investigaciones desde %d)\n", n - mal, mal, b.ntipos, b.tecnologias);
	tz_ia_bib_liberar(&b);
	tz_recursos_fechar(&r);
	return mal || n == 0;
}

int main(int argc, char **argv)
{
	if (argc > 2 && !strcmp(argv[1], "real")) return guioes_reais(argv[2]);
	prova_compilador();
	tz_ia_programa_liberar(&prog);
	tz_ia_bib_liberar(&bib);
	printf("tzar_ia_prova: %d comprobaciones, %d fallos\n", provas, falhas);
	return falhas != 0;
}
