// SPDX-License-Identifier: GPL-2.0-or-later
/* Estructuras: agrupar datos relacionados. */
#include <stdio.h>
#include <string.h>

struct alumno {
    char nombre[20];
    int edad;
    float nota;
};

static void mostrar(const struct alumno *a)
{
    printf("%-10s %2d anos  nota %.1f\n", a->nombre, a->edad, a->nota);
}

int main(void)
{
    struct alumno clase[3];
    strcpy(clase[0].nombre, "Ana");   clase[0].edad = 12; clase[0].nota = 8.5f;
    strcpy(clase[1].nombre, "Luis");  clase[1].edad = 13; clase[1].nota = 6.0f;
    strcpy(clase[2].nombre, "Marta"); clase[2].edad = 12; clase[2].nota = 9.5f;

    float total = 0;
    for (int i = 0; i < 3; i++) {
        mostrar(&clase[i]);
        total += clase[i].nota;
    }
    printf("nota media: %.2f\n", total / 3);
    return 0;
}
