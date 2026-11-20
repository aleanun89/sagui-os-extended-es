/* SPDX-License-Identifier: GPL-3.0-only */
/* compat.c - lo que GNU make pide y la libc de SaguiOS no trae. */
#include <string.h>
#include <stdlib.h>

void *mempcpy(void *d, const void *s, size_t n)
{
    return (char *)memcpy(d, s, n) + n;
}

/* Sin medida de carga: -l no limita nada (devuelve -1, como un sistema sin /proc/loadavg). */
int getloadavg(double *carga, int n)
{
    (void)carga; (void)n;
    return -1;
}

/* Para «~usuario»: SaguiOS no guarda el nombre de acceso en el proceso. */
char *getlogin(void)
{
    return getenv("USER");
}

/* Sin Guile (--without-guile): main.c lo llama siempre. */
void guile_gmake_setup(void *flocp)
{
    (void)flocp;
}
