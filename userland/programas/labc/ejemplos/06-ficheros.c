// SPDX-License-Identifier: GPL-2.0-or-later
/* Ficheros: escribir y leer un archivo de texto. */
#include <stdio.h>

int main(void)
{
    FILE *f = fopen("/tmp/lab_prueba.txt", "w");
    if (!f) {
        printf("no se pudo crear el fichero\n");
        return 1;
    }
    for (int i = 1; i <= 5; i++)
        fprintf(f, "linea %d\n", i);
    fclose(f);
    printf("escrito /tmp/lab_prueba.txt\n");

    f = fopen("/tmp/lab_prueba.txt", "r");
    char linea[80];
    int n = 0;
    while (fgets(linea, sizeof linea, f)) {
        n++;
        printf("leido: %s", linea);
    }
    fclose(f);
    printf("%d lineas\n", n);
    return 0;
}
