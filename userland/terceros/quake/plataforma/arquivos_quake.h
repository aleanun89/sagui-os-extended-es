/* arquivos_quake.h - la plataforma de Quake en el SaguiOS (D229): lo que
 * comparten los ficheros de plataforma/ (sin las cabeceras de Quake). */
#ifndef SAGUI_ARQUIVOS_QUAKE_H
#define SAGUI_ARQUIVOS_QUAKE_H

#include <stdio.h>
#include <stddef.h>

/* La superposición de ficheros (arquivos_quake.c). */
void  quake_arquivos_iniciar(const char *usuario, const char *pasta_dados, const char *pasta_id1);
/* 1 = está en los datos (real = esa ruta); 0 = real = la ruta tal cual; -1 = no cabe. */
int   quake_resolver(const char *caminho, char *real, size_t max);
FILE *quake_fopen(const char *caminho, const char *modo);

/* El lanzador (principal_quake.c). */
void  quake_janela_erro(const char *mensagem);   /* ventana del error fatal; vuelve al cerrarla */
extern const char *quake_raiz;                   /* "/home/quake": el basedir del motor */

/* sys_sagui.c: arranca el motor y no vuelve (sale con exit). */
void  quake_correr(int argc, char **argv);

/* De la libsagui (sin incluir sagui.h, que choca con los nombres de Quake). */
void     registrar(const char *fmt, ...);
unsigned tempo_ms(void);
int      dormir(unsigned ms);

#endif
