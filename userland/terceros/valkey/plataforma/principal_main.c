/* SPDX-License-Identifier: GPL-3.0-only */
/* principal_main.c - los programas de SaguiOS empiezan en principal(argc, argv) (inicio.asm); Valkey tiene
 * main(). Esta llama a main y sale con exit(), que vacía stdio y llama a lo registrado con atexit. */
#include <stdlib.h>

extern char **environ;
int main(int argc, char **argv, char **envp);

int principal(int argc, char **argv)
{
    exit(main(argc, argv, environ));
}

/* Valkey se enlaza estático y sin módulos: dlopen y compañía, vacíos (antes estaban en la libc y rompían la carga
 * dinámica de los demás programas, h386). */
void *dlopen(const char *nome, int modo) { (void)nome; (void)modo; return 0; }
void *dlsym(void *b, const char *s) { (void)b; (void)s; return 0; }
int dlclose(void *b) { (void)b; return 0; }
char *dlerror(void) { return (char *)"no hay carga dinamica de modulos en Valkey"; }
