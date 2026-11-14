// SPDX-License-Identifier: GPL-2.0-or-later
/* truncar - prueba de SIS_TRUNCAR y SIS_SINCRONIZAR (D116, h87): truncar RUTA.
 * Escribe 10000 bytes, reduce a 100, agranda a 6000 (lo nuevo tiene que ser cero), vuelve a
 * reducir a 0 y comprueba tamaños y contenido en cada paso. Imprime "truncar: OK" o el fallo. */
#include "sagui.h"

static int tam(const char *r)
{
    info_arquivo_t i;
    return info_arquivo(r, &i) < 0 ? -1 : (int)i.tamanho;
}

#define FALLA(txt) do { imprimir("truncar: FALLO: %s\n", txt); return 1; } while (0)

int principal(int argc, char **argv)
{
    if (argc < 2) FALLA("uso: truncar RUTA");
    static unsigned char b[10000];
    for (int i = 0; i < 10000; i++) b[i] = (unsigned char)(1 + i % 250);
    int fd = abrir(argv[1], ABRIR_LER | ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    if (fd < 0) FALLA("abrir");
    if (escrever(fd, b, 10000) != 10000) FALLA("escrever");
    if (sincronizar(fd) < 0) FALLA("sincronizar");
    if (tam(argv[1]) != 10000) FALLA("tamanho 10000");
    if (truncar(fd, 100) < 0) FALLA("truncar 100");
    if (tam(argv[1]) != 100) FALLA("tamanho 100");
    if (truncar(fd, 6000) < 0) FALLA("truncar 6000");
    if (tam(argv[1]) != 6000) FALLA("tamanho 6000");
    static unsigned char l[6000];
    posicionar(fd, 0, POS_INICIO);
    int n = 0;
    while (n < 6000) { int r = ler(fd, l + n, (unsigned)(6000 - n)); if (r <= 0) break; n += r; }
    if (n != 6000) FALLA("ler 6000");
    for (int i = 0; i < 100; i++) if (l[i] != b[i]) FALLA("los primeros 100 bytes cambiaron");
    for (int i = 100; i < 6000; i++) if (l[i] != 0) FALLA("lo agrandado no es cero");
    /* escribir pasado el final deja un hueco a cero */
    posicionar(fd, 7000, POS_INICIO);
    if (escrever(fd, "fin", 3) != 3) FALLA("escribir tras el final");
    posicionar(fd, 6000, POS_INICIO);
    if (ler(fd, l, 1000) != 1000) FALLA("leer el hueco");
    for (int i = 0; i < 1000; i++) if (l[i] != 0) FALLA("el hueco no es cero");
    if (truncar(fd, 0) < 0 || tam(argv[1]) != 0) FALLA("truncar 0");
    if (sincronizar(fd) < 0) FALLA("sincronizar final");
    fechar(fd);
    imprimir("truncar: OK\n");
    return 0;
}
