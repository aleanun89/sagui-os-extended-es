// SPDX-License-Identifier: GPL-2.0-or-later
/* padroes_unidad.c - prueba de unidad en el PC de fnmatch, basename y dirname de la libc común (D230, h230).
 *
 *   gcc -O2 -Wall -Wextra -o padroes_unidad tests/padroes_unidad.c
 *
 * Compila userland/libc/padroes.c con otros nombres (s_fnmatch, s_basename,
 * s_dirname) y los compara con los de la glibc del PC en cientos de casos:
 * patrones con *, ?, [..], [!..], [^..], la barra invertida, y las opciones
 * FNM_PATHNAME, FNM_PERIOD, FNM_NOESCAPE y FNM_CASEFOLD (la que usa Vanilla
 * Conquer para encontrar los ficheros sin distinguir mayúsculas).
 * Escribe "todo bien" y sale con 0 si pasa todo. */
#define _GNU_SOURCE
#include <fnmatch.h>
#include <libgen.h>
#include <stdio.h>
#include <string.h>

/* Las de la libc común, con otro nombre para no chocar con las de la glibc. */
#undef basename
#define fnmatch s_fnmatch
#define basename s_basename
#define dirname s_dirname
#define SAGUI_FNMATCH_H
#define SAGUI_LIBGEN_H
int s_fnmatch(const char *padrao, const char *nome, int opcoes);
char *s_basename(char *caminho);
char *s_dirname(char *caminho);
#include "../userland/libc/padroes.c"
#undef fnmatch
#undef basename
#undef dirname

int main(void)
{
    static const char *const padroes[] = { "*", "*.MIX", "*.mix", "conquer.mix", "?onquer.*", "[a-c]*", "[!a-c]*",
                                           "[^x]*", "\\*", "*/x", "a*b*c", "*.", "[", "[a", "a[]]b", ".*", "*a",
                                           "sc*.ini", "SC?01EA.INI", "*/*", "[A-Z]*.MIX", "gdi", "*/GENERAL.MIX", 0 };
    static const char *const nomes[] = { "CONQUER.MIX", "conquer.mix", ".hidden", "a/x", "abc", "aXbYc", "x", "*",
                                         "[", "[a", "a]b", "b", "SCG01EA.INI", "scg01ea.ini", "dir/file", "",
                                         "GDI", "gdi/GENERAL.MIX", "nod/general.mix", 0 };
    static const int opcoes[] = { 0, FNM_CASEFOLD, FNM_PATHNAME, FNM_PATHNAME | FNM_CASEFOLD, FNM_PERIOD,
                                  FNM_NOESCAPE, FNM_PATHNAME | FNM_PERIOD };
    int ruins = 0, total = 0;
    for (int i = 0; padroes[i]; i++)
        for (int j = 0; nomes[j]; j++)
            for (unsigned k = 0; k < sizeof opcoes / sizeof opcoes[0]; k++) {
                int g = fnmatch(padroes[i], nomes[j], opcoes[k]) != 0;
                int s = s_fnmatch(padroes[i], nomes[j], opcoes[k]) != 0;
                total++;
                if (g != s) {
                    ruins++;
                    printf("FALLO fnmatch(\"%s\", \"%s\", %#x): glibc %d, libc común %d\n", padroes[i], nomes[j], opcoes[k], g, s);
                }
            }
    /* Latin-1 sin distinguir mayúsculas (la glibc no lo hace en la locale C). */
    total++;
    if (s_fnmatch("\xc9QUIPO", "\xe9quipo", FNM_CASEFOLD) != 0) {
        ruins++;
        printf("FALLO fnmatch: É y é con FNM_CASEFOLD\n");
    }
    /* basename y dirname (dirname("//") es «/» aquí y «//» en la glibc: POSIX deja elegir). */
    static const char *const caminhos[] = { "/a/b", "/a/b/", "a", "/", "", "a/", "/a", "a//b//", "/jogos/CnC", 0 };
    for (int i = 0; caminhos[i]; i++) {
        char a[32], b[32], c[32], d[32];
        strcpy(a, caminhos[i]); strcpy(b, caminhos[i]); strcpy(c, caminhos[i]); strcpy(d, caminhos[i]);
        const char *gb = __xpg_basename(a), *sb = s_basename(b), *gd = dirname(c), *sd = s_dirname(d);
        total += 2;
        if (strcmp(gb, sb)) { ruins++; printf("FALLO basename(\"%s\"): glibc «%s», libc común «%s»\n", caminhos[i], gb, sb); }
        if (strcmp(gd, sd)) { ruins++; printf("FALLO dirname(\"%s\"): glibc «%s», libc común «%s»\n", caminhos[i], gd, sd); }
    }
    printf("%d comprobaciones, %d fallos\n", total, ruins);
    if (!ruins) printf("todo bien\n");
    return ruins ? 1 : 0;
}
