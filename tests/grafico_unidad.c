// SPDX-License-Identifier: GPL-3.0-only
/* grafico_unidad.c - prueba de unidad en el PC de la parte de cálculo del componente de gráficos (D469).
 *   gcc -O2 -Wall -Wextra -I userland/inc -o grafico_unidad tests/grafico_unidad.c userland/libsagui/grafico_calc.c -lm
 * (la libm solo sirve aquí para comparar con sin/cos/sqrt del anfitrión; grafico_calc.c no la usa). */
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "grafico.h"

static int fallos, total;
#define VERIFICAR(c, ...) do { total++; if (!(c)) { fallos++; printf("FALLA %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)
static int cerca(double a, double b, double e) { return fabs(a - b) <= e; }

static void t_escala(void)
{
    gr_escala_t e;
    gr_escala(0, 97, 6, 1, &e);
    VERIFICAR(e.min == 0 && e.max == 100 && e.paso == 20 && e.n == 6, "0..97 -> %g..%g paso %g n %d", e.min, e.max, e.paso, e.n);
    gr_escala(3, 7, 6, 0, &e);
    VERIFICAR(e.min == 3 && e.max == 7 && cerca(e.paso, 1, 1e-9), "3..7 sin cero -> %g..%g paso %g", e.min, e.max, e.paso);
    gr_escala(-13, 42, 6, 1, &e);
    VERIFICAR(e.min == -20 && e.max == 60 && e.paso == 20, "-13..42 -> %g..%g paso %g", e.min, e.max, e.paso);
    gr_escala(0, 1, 5, 1, &e);
    VERIFICAR(cerca(e.paso, 0.25, 1e-9) || cerca(e.paso, 0.2, 1e-9) || cerca(e.paso, 0.5, 1e-9), "0..1 paso %g", e.paso);
    gr_escala(5, 5, 5, 0, &e);
    VERIFICAR(e.min < 5 && e.max > 5 && e.n >= 2, "rango nulo -> %g..%g", e.min, e.max);
    gr_escala(0, 0, 5, 1, &e);
    VERIFICAR(e.max > e.min && e.n >= 2, "todo cero -> %g..%g", e.min, e.max);
    gr_escala(0, 0.0123, 6, 1, &e);
    VERIFICAR(e.max >= 0.0123 && e.n <= 6 && e.paso > 0, "pequeño -> %g..%g paso %g", e.min, e.max, e.paso);
    gr_escala(1200, 98000, 6, 1, &e);
    VERIFICAR(e.max >= 98000 && e.n <= 6, "grande -> %g..%g n %d", e.min, e.max, e.n);
    /* siempre: cubre el rango, las marcas son 1-2-5 y no hay más que las pedidas */
    int malos = 0;
    for (int k = 0; k < 2000; k++) {
        double lo = (k % 17 - 8) * 1.37 * (1 + k % 5), hi = lo + (k % 29 + 1) * 0.913 * (1 + k % 7);
        for (int m = 3; m <= 10; m++) {
            gr_escala(lo, hi, m, k % 2, &e);
            double mant = e.paso;
            while (mant >= 10 - 1e-9) mant /= 10;
            while (mant < 1 - 1e-9) mant *= 10;
            int bonito = cerca(mant, 1, 1e-6) || cerca(mant, 2, 1e-6) || cerca(mant, 5, 1e-6) || cerca(mant, 10, 1e-6);
            if (e.min > lo + 1e-9 || e.max < hi - 1e-9 || e.n > m || !bonito) malos++;
        }
    }
    VERIFICAR(malos == 0, "escala: %d casos que no cubren, sobran marcas o no son 1-2-5", malos);
}

static void t_formato(void)
{
    gr_fmt_t f;
    char b[64];
    gr_fmt_parse("0", &f);   gr_fmt_num(&f, 1234.5, b, sizeof b);  VERIFICAR(!strcmp(b, "1235") || !strcmp(b, "1234"), "0: %s", b);
    gr_fmt_parse("0.0", &f); gr_fmt_num(&f, 3.14159, b, sizeof b); VERIFICAR(!strcmp(b, "3,1"), "0.0: %s", b);
    gr_fmt_parse("#,##0", &f); gr_fmt_num(&f, 1234567, b, sizeof b); VERIFICAR(!strcmp(b, "1.234.567"), "miles: %s", b);
    gr_fmt_parse("#,##0.00", &f); gr_fmt_num(&f, -1234.567, b, sizeof b); VERIFICAR(!strcmp(b, "-1.234,57"), "miles y decimales: %s", b);
    gr_fmt_parse("0%", &f); gr_fmt_num(&f, 0.256, b, sizeof b); VERIFICAR(!strcmp(b, "26%"), "pct: %s", b);
    gr_fmt_parse("0.0%", &f); gr_fmt_num(&f, 0.256, b, sizeof b); VERIFICAR(!strcmp(b, "25,6%"), "pct con decimal: %s", b);
    gr_fmt_parse("$0.00", &f); gr_fmt_num(&f, 7, b, sizeof b); VERIFICAR(!strcmp(b, "$7,00"), "prefijo: %s", b);
    gr_fmt_parse("0 kg", &f); gr_fmt_num(&f, 12, b, sizeof b); VERIFICAR(!strcmp(b, "12 kg"), "sufijo: %s", b);
    gr_fmt_parse("auto", &f);
    gr_fmt_num(&f, 5, b, sizeof b); VERIFICAR(!strcmp(b, "5"), "auto entero: %s", b);
    gr_fmt_num(&f, 2.5, b, sizeof b); VERIFICAR(!strcmp(b, "2,5"), "auto 2.5: %s", b);
    gr_fmt_num(&f, 0.125, b, sizeof b); VERIFICAR(!strcmp(b, "0,125"), "auto 0.125: %s", b);
    gr_fmt_num(&f, 0, b, sizeof b); VERIFICAR(!strcmp(b, "0"), "auto cero: %s", b);
    gr_fmt_num(&f, -0.0004, b, sizeof b); VERIFICAR(!strcmp(b, "0"), "auto -0.0004 no saca «-0»: %s", b);
    gr_fmt_parse("%", &f); gr_fmt_num(&f, 0.5, b, sizeof b); VERIFICAR(!strcmp(b, "50%"), "solo porcentaje: %s", b);
    gr_fmt_parse("0", &f); f.dec = '.'; f.mil = ',';
    gr_fmt_parse("0.00", &f); f.dec = '.'; gr_fmt_num(&f, 1.5, b, sizeof b); VERIFICAR(!strcmp(b, "1.50"), "separador inglés: %s", b);
    gr_fmt_num(&f, 1.5, b, 3); VERIFICAR(strlen(b) == 2, "el búfer corto no se desborda: %s", b);
}

static void t_ease(void)
{
    for (int c = 0; c < 4; c++) {
        VERIFICAR(gr_ease(c, 0) == 0 && gr_ease(c, 1) == 1 && gr_ease(c, -3) == 0 && gr_ease(c, 7) == 1, "extremos de la curva %d", c);
        double ant = 0;
        int mono = 1;
        for (int i = 1; i <= 100; i++) { double v = gr_ease(c, i / 100.0); if (v < ant - 1e-12) mono = 0; ant = v; }
        VERIFICAR(mono, "la curva %d crece", c);
    }
    VERIFICAR(gr_ease(GR_EASE_SALIDA, 0.5) > 0.5, "ease-out va por delante a mitad");
    VERIFICAR(cerca(gr_ease(GR_EASE_AMBAS, 0.5), 0.5, 1e-9), "ease-in-out a mitad es 0.5");
    VERIFICAR(gr_lerp(10, 20, 0.25) == 12.5, "lerp");
}

static void t_trig(void)
{
    double peor = 0;
    for (double a = -20; a < 20; a += 0.0137) {
        double e1 = fabs(gr_seno(a) - sin(a)), e2 = fabs(gr_coseno(a) - cos(a));
        if (e1 > peor) peor = e1;
        if (e2 > peor) peor = e2;
    }
    VERIFICAR(peor < 2e-7, "seno/coseno error %g", peor);
    VERIFICAR(cerca(gr_raiz(2), sqrt(2), 1e-12) && gr_raiz(0) == 0, "raiz");
    VERIFICAR(gr_floor(-1.5) == -2 && gr_floor(2.5) == 2 && gr_ceil(-1.5) == -1 && gr_ceil(2.1) == 3 && gr_floor(3) == 3, "floor/ceil");
}

static void t_apilar(void)
{
    double a[] = { 1, 2, -1 }, b[] = { 3, -2, -4 }, c[] = { 5, 5, 5 };
    const double *ys[3] = { a, b, c };
    int np[3] = { 3, 3, 3 };
    double base[9], cima[9];
    gr_apilar(ys, np, 3, 0, 3, base, cima);
    /* categoría 0: 1, 3, 5 apilados: [0,1] [1,4] [4,9] */
    VERIFICAR(base[0] == 0 && cima[0] == 1 && base[3] == 1 && cima[3] == 4 && base[6] == 4 && cima[6] == 9, "apilado positivo");
    /* categoría 1: 2 sube, -2 baja, 5 sube sobre 2 */
    VERIFICAR(base[1] == 0 && cima[1] == 2 && base[4] == 0 && cima[4] == -2 && base[7] == 2 && cima[7] == 7, "apilado mixto");
    /* categoría 2: -1, -4 bajan juntos, 5 desde 0 */
    VERIFICAR(base[2] == 0 && cima[2] == -1 && base[5] == -1 && cima[5] == -5 && base[8] == 0 && cima[8] == 5, "apilado negativo");
    gr_apilar(ys, np, 3, 2u, 3, base, cima);   /* oculta la serie 1 */
    VERIFICAR(base[3] == cima[3] && base[6] == 1 && cima[6] == 6, "serie oculta no ocupa sitio: %g %g", base[6], cima[6]);
    int np2[3] = { 2, 3, 0 };
    gr_apilar(ys, np2, 3, 0, 3, base, cima);
    VERIFICAR(cima[2] == 0 + 0 || cima[2] == 0, "serie corta cuenta como cero");
}

static void t_barras(void)
{
    double x, w;
    gr_barra_geom(10, 100, 5, 0, 1, 0, 0, 0.2, &x, &w);   /* slot 20, hueco 20% = 4: barra de 16 con 2 de margen */
    VERIFICAR(cerca(x, 12, 1e-9) && cerca(w, 16, 1e-9), "una barra: x=%g w=%g", x, w);
    gr_barra_geom(10, 100, 5, 2, 2, 1, 0, 0.2, &x, &w);   /* 2 barras juntas en la categoría 2: slot [50,70) */
    VERIFICAR(cerca(x, 50 + 2 + 8, 1e-9) && cerca(w, 8, 1e-9), "agrupada k=1: x=%g w=%g", x, w);
    gr_barra_geom(10, 100, 5, 2, 3, 2, 1, 0.2, &x, &w);   /* apilado: una sola barra */
    VERIFICAR(cerca(x, 52, 1e-9) && cerca(w, 16, 1e-9), "apilada: x=%g w=%g", x, w);
    gr_barra_geom(0, 100, 4, 3, 2, 1, 0, 5, &x, &w);
    VERIFICAR(w > 0 && x + w <= 100, "hueco absurdo se limita");
}

static void t_bins(void)
{
    double v[] = { 1, 2, 2, 3, 3, 3, 4, 4, 4, 4, 10 };
    int c[32];
    double lo, paso;
    int nb = gr_bins(v, 11, 3, &lo, &paso, c, 32);
    VERIFICAR(nb == 3 && lo == 1 && cerca(paso, 3, 1e-9), "3 bins: lo %g paso %g", lo, paso);
    VERIFICAR(c[0] == 6 && c[1] == 4 && c[2] == 1, "cuentas %d %d %d", c[0], c[1], c[2]);   /* [1,4) [4,7) [7,10]: el máximo cae en el último */
    nb = gr_bins(v, 11, 0, &lo, &paso, c, 32);
    int suma = 0;
    for (int j = 0; j < nb; j++) suma += c[j];
    VERIFICAR(nb >= 3 && nb <= 6 && suma == 11, "Sturges: %d intervalos, suma %d", nb, suma);
    double iguales[] = { 5, 5, 5 };
    nb = gr_bins(iguales, 3, 4, &lo, &paso, c, 32);
    int s2 = 0;
    for (int j = 0; j < nb; j++) s2 += c[j];
    VERIFICAR(nb == 4 && s2 == 3 && paso > 0, "datos iguales");
    VERIFICAR(gr_bins(v, 0, 0, &lo, &paso, c, 32) == 0, "sin datos");
}

static void t_catmull(void)
{
    VERIFICAR(cerca(gr_catmull(0, 1, 2, 3, 0), 1, 1e-12) && cerca(gr_catmull(0, 1, 2, 3, 1), 2, 1e-12), "pasa por p1 y p2");
    VERIFICAR(cerca(gr_catmull(0, 1, 2, 3, 0.5), 1.5, 1e-12), "una recta sigue siendo recta");
    double m = gr_catmull(0, 0, 10, 10, 0.5);
    VERIFICAR(m > 4 && m < 6, "tramo suave %g", m);
}

static void t_colision(void)
{
    VERIFICAR(gr_en_rect(5, 5, 0, 0, 10, 10) && !gr_en_rect(10, 5, 0, 0, 10, 10) && !gr_en_rect(-1, 5, 0, 0, 10, 10), "rect");
    VERIFICAR(gr_en_circulo(3, 4, 0, 0, 5) && !gr_en_circulo(4, 4, 0, 0, 5), "círculo");
    double pi = 3.14159265358979, c = 100;
    /* cuarto de tarta: de las 12 a las 3 (a = 0..pi/2): el cuadrante superior derecho */
    VERIFICAR(gr_en_sector(c + 20, c - 20, c, c, 0, 50, 0, pi / 2), "sector: punto arriba a la derecha");
    VERIFICAR(!gr_en_sector(c - 20, c - 20, c, c, 0, 50, 0, pi / 2), "sector: arriba a la izquierda queda fuera");
    VERIFICAR(!gr_en_sector(c + 20, c + 20, c, c, 0, 50, 0, pi / 2), "sector: abajo a la derecha queda fuera");
    VERIFICAR(!gr_en_sector(c + 40, c - 40, c, c, 0, 50, 0, pi / 2), "sector: fuera del radio");
    VERIFICAR(!gr_en_sector(c + 5, c - 5, c, c, 20, 50, 0, pi / 2), "anillo: dentro del hueco");
    /* mitad derecha de las 12 a las 6 */
    VERIFICAR(gr_en_sector(c + 10, c, c, c, 0, 50, 0, pi) && !gr_en_sector(c - 10, c, c, c, 0, 50, 0, pi), "semicírculo derecho");
    /* tres cuartos (más de pi): de las 12 a las 9 pasando por 3 y 6 */
    VERIFICAR(gr_en_sector(c + 10, c, c, c, 0, 50, 0, 1.5 * pi) && gr_en_sector(c, c + 10, c, c, 0, 50, 0, 1.5 * pi) &&
              gr_en_sector(c - 10, c + 3, c, c, 0, 50, 0, 1.5 * pi), "tres cuartos incluye derecha, abajo e izquierda");
    VERIFICAR(!gr_en_sector(c - 10, c - 10, c, c, 0, 50, 0, 1.5 * pi), "tres cuartos deja fuera arriba a la izquierda");
    VERIFICAR(gr_en_sector(c - 10, c - 10, c, c, 0, 50, 0, 2 * pi), "vuelta entera");
    /* sector que no empieza en 0: de las 6 a las 9 (pi .. 1.5pi) */
    VERIFICAR(gr_en_sector(c - 10, c + 10, c, c, 0, 50, pi, 1.5 * pi) && !gr_en_sector(c + 10, c + 10, c, c, 0, 50, pi, 1.5 * pi), "cuadrante inferior izquierdo");
    double dx, dy;
    gr_dir(0, &dx, &dy);
    VERIFICAR(cerca(dx, 0, 1e-9) && cerca(dy, -1, 1e-9), "ángulo 0 mira a las 12");
    gr_dir(pi / 2, &dx, &dy);
    VERIFICAR(cerca(dx, 1, 1e-9) && cerca(dy, 0, 1e-9), "ángulo pi/2 mira a las 3");
}

static void t_paleta(void)
{
    VERIFICAR(gr_color_serie(0, 0) == 0x2A6FD8 && gr_color_serie(12, 0) == gr_color_serie(0, 0) && gr_color_serie(0, 1) != gr_color_serie(0, 0), "paleta");
    int distintos = 1;
    for (int i = 0; i < 12; i++)
        for (int j = i + 1; j < 12; j++) if (gr_color_serie(i, 0) == gr_color_serie(j, 0) || gr_color_serie(i, 1) == gr_color_serie(j, 1)) distintos = 0;
    VERIFICAR(distintos, "los 12 colores son distintos");
}

int main(void)
{
    t_escala(); t_formato(); t_ease(); t_trig(); t_apilar(); t_barras(); t_bins(); t_catmull(); t_colision(); t_paleta();
    printf("grafico_unidad: %d comprobaciones, %d fallos\n", total, fallos);
    if (!fallos) printf("todo bien\n");
    return fallos ? 1 : 0;
}
