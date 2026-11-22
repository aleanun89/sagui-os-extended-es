// SPDX-License-Identifier: GPL-3.0-only
/* huella: calcula el CRC-32 de archivos o de la entrada estándar (ejemplo de ficheros y tablas).
 * Origen: proyecto SaguiOS (ejemplos para aprender C compilando con tcc dentro del sistema).
 * Es la versión sencilla; la del sistema (más completa) está en userland/programas/. */
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* CRC-32 de ZIP y PNG, con el polinomio reflejado. */
static void preparar_tabla(uint32_t tabla[256])
{
    for (unsigned i = 0; i < 256; i++) {
        uint32_t resto = i;
        for (int bit = 0; bit < 8; bit++)
            resto = (resto >> 1) ^ ((resto & 1u) ? UINT32_C(0xedb88320) : 0u);
        tabla[i] = resto;
    }
}

static int calcular(FILE *entrada, const uint32_t tabla[256], uint32_t *huella)
{
    unsigned char bloque[4096];
    uint32_t crc = UINT32_MAX;
    size_t cantidad;

    while ((cantidad = fread(bloque, 1, sizeof bloque, entrada)) != 0)
        for (size_t i = 0; i < cantidad; i++)
            crc = (crc >> 8) ^ tabla[(crc ^ bloque[i]) & 0xffu];

    if (ferror(entrada))
        return -1;
    *huella = crc ^ UINT32_MAX;
    return 0;
}

int main(int argc, char **argv)
{
    uint32_t tabla[256];
    int estado = 0;

    if (argc == 2 && strcmp(argv[1], "--ayuda") == 0) {
        puts("Uso: huella archivo [...]");
        puts("Calcula CRC-32. Usa - para leer de la entrada estandar.");
        return 0;
    }
    preparar_tabla(tabla);

    int primero = argc > 1 ? 1 : 0;
    for (int i = primero; i < argc; i++) {
        const char *ruta = i ? argv[i] : "-";
        FILE *entrada = strcmp(ruta, "-") == 0 ? stdin : fopen(ruta, "rb");
        if (!entrada) {
            perror(ruta);
            estado = 1;
            continue;
        }

        uint32_t crc;
        if (calcular(entrada, tabla, &crc) < 0) {
            perror(ruta);
            estado = 1;
        } else {
            printf("%08" PRIX32 "  %s\n", crc, ruta);
        }
        if (entrada != stdin)
            fclose(entrada);
    }
    return estado;
}
