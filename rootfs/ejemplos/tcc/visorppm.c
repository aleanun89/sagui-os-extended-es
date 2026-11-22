// SPDX-License-Identifier: GPL-3.0-only
/* visorppm: abre una imagen PPM P6 en una ventana de la libsagui (ejemplo de imágenes y ventanas).
 * Origen: proyecto SaguiOS (ejemplos para aprender C compilando con tcc dentro del sistema).
 * Es la versión sencilla; la del sistema (más completa) está en userland/programas/. */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sagui.h"

#define ANCHO_MAXIMO 640
#define ALTO_MAXIMO 480

/* Los comentarios valen en la cabecera. Tras el separador final empiezan
 * los bytes de la imagen, que pueden contener espacios y almohadillas. */
static int leer_palabra(FILE *entrada, char *palabra, size_t capacidad)
{
    int c;
    do {
        c = fgetc(entrada);
        if (c == '#') {
            do {
                c = fgetc(entrada);
            } while (c != '\n' && c != EOF);
        }
    } while (c != EOF && isspace(c));

    size_t longitud = 0;
    while (c != EOF && !isspace(c) && c != '#') {
        if (longitud + 1 >= capacidad)
            return -1;
        palabra[longitud++] = (char)c;
        c = fgetc(entrada);
    }
    palabra[longitud] = '\0';

    if (c == '#')
        ungetc(c, entrada);
    return longitud && c != EOF ? c : -1;
}

static int entero_positivo(const char *texto, int maximo)
{
    int valor = 0;
    for (const char *p = texto; *p; p++) {
        if (*p < '0' || *p > '9')
            return -1;
        valor = valor * 10 + (*p - '0');
        if (valor > maximo)
            return -1;
    }
    return valor > 0 ? valor : -1;
}

static int mostrar(FILE *entrada, unsigned segundos)
{
    char palabra[16];
    if (leer_palabra(entrada, palabra, sizeof palabra) < 0 ||
        strcmp(palabra, "P6") != 0)
        return -1;
    if (leer_palabra(entrada, palabra, sizeof palabra) < 0)
        return -1;
    int ancho = entero_positivo(palabra, ANCHO_MAXIMO);
    if (leer_palabra(entrada, palabra, sizeof palabra) < 0)
        return -1;
    int alto = entero_positivo(palabra, ALTO_MAXIMO);
    int separador = leer_palabra(entrada, palabra, sizeof palabra);
    if (ancho < 1 || alto < 1 || separador < 0 || !isspace(separador) ||
        entero_positivo(palabra, 255) != 255)
        return -1;

    size_t bytes = (size_t)ancho * alto * 3;
    unsigned char *rgb = malloc(bytes);
    if (!rgb) {
        fputs("visorppm: sin memoria\n", stderr);
        return 1;
    }
    if (fread(rgb, 1, bytes, entrada) != bytes) {
        free(rgb);
        return -1;
    }

    uint32_t *pixeles;
    int ventana = criar_janela("Visor PPM", ancho, alto, &pixeles);
    if (ventana < 0) {
        fputs("visorppm: no se pudo abrir la ventana\n", stderr);
        free(rgb);
        return 1;
    }
    for (size_t i = 0; i < bytes / 3; i++)
        pixeles[i] = ((uint32_t)rgb[3 * i] << 16) |
                     ((uint32_t)rgb[3 * i + 1] << 8) | rgb[3 * i + 2];

    free(rgb);
    atualizar_janela(ventana, NULL);
    registrar("visorppm: imagen %dx%d abierta\n", ancho, alto);

    uint32_t inicio = tempo_ms();
    evento_t evento;
    while (!segundos || tempo_ms() - inicio < segundos * 1000u) {
        if (esperar_evento(ventana, &evento, 100) != 1)
            continue;
        if (evento.tipo == EV_FECHAR || (evento.tipo == EV_TECLA && evento.tecla == TECLA_ESC))
            break;
    }

    fechar_janela(ventana);
    return 0;
}

int main(int argc, char **argv)
{
    unsigned segundos = 0;
    if (argc == 2 && strcmp(argv[1], "--ayuda") == 0) {
        puts("Uso: visorppm imagen.ppm [segundos]");
        puts("Abre PPM P6 de 8 bits por canal, hasta 640x480. Esc cierra.");
        return 0;
    }
    if (argc == 3) {
        int cantidad = entero_positivo(argv[2], 3600);
        if (cantidad > 0)
            segundos = (unsigned)cantidad;
    }
    if (argc < 2 || argc > 3 || (argc == 3 && !segundos)) {
        fputs("Uso: visorppm imagen.ppm [segundos]\n", stderr);
        return 2;
    }

    FILE *entrada = fopen(argv[1], "rb");
    if (!entrada) {
        perror(argv[1]);
        return 1;
    }

    int resultado = mostrar(entrada, segundos);
    fclose(entrada);
    if (resultado < 0) {
        fputs("visorppm: imagen P6 incompleta o formato incompatible\n", stderr);
        return 1;
    }
    return resultado;
}
