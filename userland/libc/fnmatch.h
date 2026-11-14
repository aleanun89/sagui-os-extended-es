/* fnmatch.h - comparar un nombre con un patrón de shell («*.MIX») (D230).
 *
 * Parte de la libc común (userland/libc, D210); la función está en
 * userland/libc/padroes.c. Admite *, ?, [abc], [a-z], [!abc] y [^abc],
 * la barra invertida para quitar el significado especial (salvo con
 * FNM_NOESCAPE), FNM_PATHNAME (los comodines no saltan la «/»), FNM_PERIOD
 * (un punto al principio solo con un punto) y FNM_CASEFOLD (sin distinguir
 * mayúsculas, en ASCII y Latin-1). Sin clases [:alpha:] ni locales. */
#ifndef SAGUI_FNMATCH_H
#define SAGUI_FNMATCH_H

#define FNM_NOMATCH     1
#define FNM_PATHNAME    0x01   /* los valores, los de glibc y los BSD */
#define FNM_NOESCAPE    0x02
#define FNM_PERIOD      0x04
#define FNM_LEADING_DIR 0x08
#define FNM_CASEFOLD    0x10
#define FNM_IGNORECASE  FNM_CASEFOLD
#define FNM_FILE_NAME   FNM_PATHNAME

int fnmatch(const char *padrao, const char *nome, int opcoes);
#endif
