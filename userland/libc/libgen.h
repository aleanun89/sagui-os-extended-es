/* libgen.h - basename y dirname de POSIX (D230).
 *
 * Parte de la libc común (userland/libc, D210); las funciones están en
 * userland/libc/padroes.c. Como las de POSIX, pueden cambiar la cadena que
 * reciben y devuelven un puntero dentro de ella o a una cadena fija. */
#ifndef SAGUI_LIBGEN_H
#define SAGUI_LIBGEN_H
char *basename(char *caminho);
char *dirname(char *caminho);
#endif
