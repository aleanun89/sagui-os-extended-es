/* getopt.h - opciones de la línea de órdenes (D184).
 *
 * Parte de la libc común (userland/libc, D210; nació con NetSurf, D184)
 * sobre la libsagui; las funciones están en los .c de userland/libc/. */
#ifndef NS_GETOPT_H
#define NS_GETOPT_H

extern char *optarg;
extern int optind, opterr, optopt;
struct option {
    const char *name;
    int has_arg;
    int *flag;
    int val;
};
#define no_argument       0
#define required_argument 1
#define optional_argument 2
int getopt(int argc, char *const argv[], const char *opcoes);
int getopt_long(int argc, char *const argv[], const char *opcoes,
                const struct option *longas, int *indice);
#endif
