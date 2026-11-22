// SPDX-License-Identifier: GPL-2.0-or-later
/* ocio_unidad.c - prueba de unidad de los efectos del salvapantallas (D422, h422): kernel/lib/ocio_efectos.c
 * en el PC. Comprueba que cada efecto dibuja (el negro no), que es determinista con la misma semilla, que
 * nunca escribe fuera del lienzo (centinelas alrededor y, si se compila con ASan, también ahí), que aguanta
 * horas de cuadros (las tuberías se reinician al llenarse) y tamaños pequeños y raros, y que el reloj cambia
 * de sitio con el minuto. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../kernel/lib/ocio_efectos.c"

#define CENT 64
static int fallos;
#define COMPROBAR(c, ...) do { if (!(c)) { printf("FALLO: " __VA_ARGS__); printf("\n"); fallos++; } } while (0)

static unsigned *lienzo(int w, int h, int st)
{
    (void)w;
    unsigned *b = malloc((size_t)(h * st + 2 * CENT) * 4);
    for (int i = 0; i < h * st + 2 * CENT; i++) b[i] = 0xDEADBEEF;
    return b;
}

static int centinelas_ok(const unsigned *b, int h, int st)
{
    for (int i = 0; i < CENT; i++) if (b[i] != 0xDEADBEEF || b[CENT + h * st + i] != 0xDEADBEEF) return 0;
    return 1;
}

static unsigned suma(const unsigned *p, int w, int h, int st)
{
    unsigned s = 0;
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) s = s * 31 + p[y * st + x];
    return s;
}

static int no_negros(const unsigned *p, int w, int h, int st)
{
    int n = 0;
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) if (p[y * st + x]) n++;
    return n;
}

int main(void)
{
    static ocio_ef_t e1, e2;
    int w = 320, h = 200;
    for (int ef = 0; ef < OCIO_EF_N; ef++) {
        unsigned *b1 = lienzo(w, h, w), *b2 = lienzo(w, h, w);
        ocio_ef_iniciar(&e1, ef, 1234, w, h, 1000);
        ocio_ef_iniciar(&e2, ef, 1234, w, h, 1000);
        unsigned ms = 1000;
        for (int c = 0; c < 400; c++) {   /* 400 cuadros de 80 ms: ~32 s */
            ms += 80;
            ocio_ef_cuadro(&e1, b1 + CENT, w, h, w, ms, 1, 12, 34, c % 60);
            ocio_ef_cuadro(&e2, b2 + CENT, w, h, w, ms, 1, 12, 34, c % 60);
        }
        COMPROBAR(centinelas_ok(b1, h, w), "efecto %d escribe fuera del lienzo", ef);
        COMPROBAR(suma(b1 + CENT, w, h, w) == suma(b2 + CENT, w, h, w), "efecto %d no es determinista", ef);
        int vivos = no_negros(b1 + CENT, w, h, w);
        printf("efecto %d: %d píxeles encendidos\n", ef, vivos);
        if (ef == OCIO_EF_NEGRO) COMPROBAR(vivos == 0, "el efecto negro dibuja algo");
        else COMPROBAR(vivos > 50, "el efecto %d no dibuja casi nada (%d)", ef, vivos);
        /* con stride mayor que el ancho (como un búfer de pantalla recortado) */
        int st = w + 37;
        unsigned *b3 = lienzo(w, h, st);
        ocio_ef_iniciar(&e1, ef, 99, w, h, 0);
        for (int c = 0; c < 50; c++) ocio_ef_cuadro(&e1, b3 + CENT, w, h, st, (unsigned)c * 50, 1, 0, 0, 0);
        COMPROBAR(centinelas_ok(b3, h, st), "efecto %d con stride escribe fuera", ef);
        for (int y = 0; y < h; y++) for (int x = w; x < st; x++)
            if (b3[CENT + y * st + x] != 0xDEADBEEF) { COMPROBAR(0, "efecto %d pisa el relleno del stride", ef); y = h; break; }
        /* horas de cuadros: las tuberías se llenan y se reinician sin salirse */
        ocio_ef_iniciar(&e1, ef, 7, w, h, 0);
        ms = 0;
        for (int c = 0; c < 20000; c++) { ms += 60; ocio_ef_cuadro(&e1, b1 + CENT, w, h, w, ms, 0, 23, 59, 59); }
        COMPROBAR(centinelas_ok(b1, h, w), "efecto %d, 20 000 cuadros, escribe fuera", ef);
        if (ef == OCIO_EF_TUBERIAS) COMPROBAR(e1.n_tubos <= OCIO_TUBOS_MAX, "tuberías sin tope");
        free(b1); free(b2); free(b3);
    }
    /* tamaños pequeños y raros */
    int tam[][2] = { { 1, 1 }, { 3, 2 }, { 17, 5 }, { 64, 48 }, { 1920, 20 }, { 20, 1080 }, { 4096, 2160 } };
    for (unsigned t = 0; t < sizeof tam / sizeof tam[0]; t++)
        for (int ef = 0; ef < OCIO_EF_N; ef++) {
            int tw = tam[t][0], th = tam[t][1];
            unsigned *b = lienzo(tw, th, tw);
            ocio_ef_iniciar(&e1, ef, 5, tw, th, 0);
            for (int c = 0; c < 30; c++) ocio_ef_cuadro(&e1, b + CENT, tw, th, tw, (unsigned)c * 90, 1, 1, 2, 3);
            COMPROBAR(centinelas_ok(b, th, tw), "efecto %d en %dx%d escribe fuera", ef, tw, th);
            free(b);
        }
    /* el reloj: dibuja la hora y cambia de sitio con el minuto */
    {
        unsigned *a = lienzo(w, h, w), *b = lienzo(w, h, w);
        ocio_ef_iniciar(&e1, OCIO_EF_RELOJ, 3, w, h, 0);
        ocio_ef_cuadro(&e1, a + CENT, w, h, w, 0, 1, 10, 15, 0);
        ocio_ef_cuadro(&e1, b + CENT, w, h, w, 0, 1, 10, 16, 0);
        COMPROBAR(suma(a + CENT, w, h, w) != suma(b + CENT, w, h, w), "el reloj no cambia con el minuto");
        ocio_ef_cuadro(&e1, b + CENT, w, h, w, 0, 1, 10, 15, 0);
        COMPROBAR(suma(a + CENT, w, h, w) == suma(b + CENT, w, h, w), "el reloj no es estable dentro del minuto");
        free(a); free(b);
    }
    /* el logotipo se mueve */
    {
        unsigned *a = lienzo(w, h, w), *b = lienzo(w, h, w);
        ocio_ef_iniciar(&e1, OCIO_EF_LOGOTIPO, 3, w, h, 0);
        ocio_ef_cuadro(&e1, a + CENT, w, h, w, 0, 1, 0, 0, 0);
        ocio_ef_cuadro(&e1, b + CENT, w, h, w, 5000, 1, 0, 0, 0);
        COMPROBAR(suma(a + CENT, w, h, w) != suma(b + CENT, w, h, w), "el logotipo no rebota");
        free(a); free(b);
    }
    if (fallos) { printf("ocio_unidad: %d fallos\n", fallos); return 1; }
    printf("ocio_unidad: OK\n");
    return 0;
}
