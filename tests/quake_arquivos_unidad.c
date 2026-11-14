// SPDX-License-Identifier: GPL-2.0-or-later
/* quake_arquivos_unidad.c - prueba en el PC de la superposición de ficheros de
 * Quake (userland/terceros/quake/plataforma/arquivos_quake.c, D229).
 *
 *   quake_arquivos_unidad CARPETA_TEMPORAL
 *
 * Arma en CARPETA_TEMPORAL un /home/quake y dos carpetas de datos (una con
 * ID1/PAK0.PAK en mayúsculas, como el shareware de DOS, y otra con el
 * pak0.pak suelto, como input_share/games/Quake/) y comprueba:
 *   - id1/pak0.pak se resuelve a los datos, sin distinguir mayúsculas;
 *   - con el pak suelto, id1/... es la carpeta misma;
 *   - un mod (hipnotic/...) se busca en <datos>/hipnotic;
 *   - lo que hay en /home/quake manda sobre los datos;
 *   - lo que no está en ningún sitio queda como estaba (el motor ve el fallo);
 *   - quake_fopen en escritura crea las carpetas y escribe en /home/quake;
 *   - las rutas fuera de /home/quake no se tocan.
 * Termina con «FIN n fallos» y el código n.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "arquivos_quake.h"

static int fallos = 0;
static char raiz[512];

static void escribir(const char *rel, const char *texto)
{
    char c[1024];
    snprintf(c, sizeof c, "%s/%s", raiz, rel);
    for (char *p = c + strlen(raiz) + 1; *p; p++) {
        if (*p == '/') { *p = 0; mkdir(c, 0777); *p = '/'; }
    }
    FILE *f = fopen(c, "wb");
    if (!f) { perror(c); exit(2); }
    fputs(texto, f);
    fclose(f);
}

static void ver(int ok, const char *que)
{
    printf("%s %s\n", ok ? "ok  " : "FALLO", que);
    if (!ok) fallos++;
}

/* Lee la primera línea del fichero que da quake_fopen(ruta, "rb"). */
static int contenido(const char *ruta, const char *esperado)
{
    char buf[64] = { 0 };
    FILE *f = quake_fopen(ruta, "rb");
    if (!f) return esperado == NULL;
    if (!fgets(buf, sizeof buf, f)) buf[0] = 0;
    fclose(f);
    return esperado && strcmp(buf, esperado) == 0;
}

int main(int argc, char **argv)
{
    char usuario[600], dados[600], id1[600], v[1024], real[1024];
    if (argc != 2) { fprintf(stderr, "uso: %s CARPETA_TEMPORAL\n", argv[0]); return 2; }
    snprintf(raiz, sizeof raiz, "%s", argv[1]);

    escribir("jogos/quake/ID1/PAK0.PAK", "pak0 shareware");
    escribir("jogos/quake/ID1/GFX/ENV.LMP", "suelto");
    escribir("jogos/quake/HIPNOTIC/PAK0.PAK", "pak0 hipnotic");
    escribir("jogos/Quake/pak0.pak", "pak0 suelto");
    escribir("home/quake/id1/autoexec.cfg", "del usuario");
    escribir("home/quake/id1/GFX/ENV.LMP", "tapado");

    snprintf(usuario, sizeof usuario, "%s/home/quake", raiz);

    /* 1. Datos con ID1/ en mayúsculas. */
    snprintf(dados, sizeof dados, "%s/jogos/quake", raiz);
    snprintf(id1, sizeof id1, "%s/jogos/quake/ID1", raiz);
    quake_arquivos_iniciar(usuario, dados, id1);
    snprintf(v, sizeof v, "%s/id1/pak0.pak", usuario);
    ver(quake_resolver(v, real, sizeof real) == 1 && strstr(real, "/jogos/quake/ID1/PAK0.PAK"),
        "id1/pak0.pak -> los datos (ID1/PAK0.PAK, sin distinguir mayúsculas)");
    ver(contenido(v, "pak0 shareware"), "quake_fopen lee el pak de los datos");
    snprintf(v, sizeof v, "%s/id1/gfx/env.lmp", usuario);
    ver(quake_resolver(v, real, sizeof real) == 1 && strstr(real, "/jogos/quake/ID1/GFX/ENV.LMP"),
        "una ruta con subcarpetas, también sin mayúsculas");
    snprintf(v, sizeof v, "%s/id1/GFX/ENV.LMP", usuario);
    ver(quake_resolver(v, real, sizeof real) == 0 && contenido(v, "tapado"),
        "lo de /home/quake manda sobre los datos");
    snprintf(v, sizeof v, "%s/hipnotic/pak0.pak", usuario);
    ver(quake_resolver(v, real, sizeof real) == 1 && contenido(v, "pak0 hipnotic"),
        "un mod (hipnotic/) se busca en <datos>/HIPNOTIC");
    snprintf(v, sizeof v, "%s/id1/pak1.pak", usuario);
    ver(quake_resolver(v, real, sizeof real) == 0 && strcmp(real, v) == 0 && contenido(v, NULL),
        "lo que no existe queda como estaba y no se abre");
    snprintf(v, sizeof v, "%s/id1/autoexec.cfg", usuario);
    ver(contenido(v, "del usuario"), "lo que solo está en /home/quake se lee de allí");

    /* 2. El pak0.pak suelto (input_share/games/Quake/pak0.pak). */
    snprintf(dados, sizeof dados, "%s/jogos/Quake", raiz);
    quake_arquivos_iniciar(usuario, dados, dados);
    snprintf(v, sizeof v, "%s/id1/pak0.pak", usuario);
    ver(contenido(v, "pak0 suelto"), "con el pak suelto, id1/ es la carpeta de los datos");

    /* 3. Escribir: siempre en /home/quake, creando las carpetas. */
    snprintf(v, sizeof v, "%s/id1/partidas/s0.sav", usuario);
    FILE *f = quake_fopen(v, "w");
    ver(f != NULL, "quake_fopen en escritura crea las carpetas que faltan");
    if (f) { fputs("partida", f); fclose(f); }
    ver(contenido(v, "partida"), "y lo escrito se vuelve a leer");
    snprintf(v, sizeof v, "%s/home/quake/id1/config.cfg", raiz);
    f = quake_fopen(v, "w");
    if (f) { fputs("bind x y", f); fclose(f); }
    snprintf(real, sizeof real, "%s/jogos/Quake/config.cfg", raiz);
    struct stat st;
    ver(stat(v, &st) == 0 && stat(real, &st) != 0, "config.cfg va a /home/quake, nunca a los datos");

    /* 4. Fuera de /home/quake no se toca nada. */
    snprintf(v, sizeof v, "%s/jogos/quake/ID1/PAK0.PAK", raiz);
    ver(quake_resolver(v, real, sizeof real) == 0 && strcmp(real, v) == 0, "una ruta fuera de /home/quake queda igual");
    ver(quake_resolver("/home/quakeX/id1/pak0.pak", real, sizeof real) == 0, "/home/quakeX no es /home/quake");

    printf("FIN %d fallos\n", fallos);
    return fallos;
}
