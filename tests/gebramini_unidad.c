// SPDX-License-Identifier: GPL-3.0-or-later
// GebraMini, © 2026 Alejandro Anuncibay (prototipo original); versión nativa de SaguiOS.
/* Pruebas de unidad del motor de GebraMini (D309) en el anfitrión: matemáticas x87 frente a la libm,
 * analizador, derivada simbólica, formato de números, geometría dinámica (puntos medios que siguen
 * a sus extremos, cortes, rectas notables), raíces numéricas, órdenes en español y fichero .gbm.
 * Se compila con: gcc -DSAGUI_HOST -Iuserland/inc -Iuserland/gebramini (lo hace h309). */
#include <stdio.h>
#include <stdarg.h>
#include <math.h>
#include "gbm.h"
#include "gbm_mat.h"

/* Lo que el motor pide a la libsagui. */
size_t tam_texto(const char *s) { return strlen(s); }
int compara_texto(const char *a, const char *b) { return strcmp(a, b); }
char *copia_texto(char *dst, const char *src, size_t max)
{
    if (!max) return dst;
    size_t i = 0;
    for (; i + 1 < max && src[i]; i++) dst[i] = src[i];
    dst[i] = 0;
    return dst;
}
char *acha_char(const char *s, int c) { return strchr(s, c); }
int formatar_texto(char *dst, size_t max, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int r = vsnprintf(dst, max, fmt, ap);
    va_end(ap);
    return r;
}

static int fallos, total;
#define VER(c, ...) do { total++; if (!(c)) { fallos++; printf("FALLO %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)
#define CERCA(a, b, tol, ...) do { double _a = (a), _b = (b); total++; if (!(fabs(_a - _b) <= (tol))) { fallos++; printf("FALLO %d: %g != %g: ", __LINE__, _a, _b); printf(__VA_ARGS__); printf("\n"); } } while (0)

static double ev1(const char *t, double x)
{
    int r = gbm_analizar(t, true);
    if (r < 0) return NAN;
    gbm_val_t v = gbm_avaliar(r, x);
    return v.a;
}

static void prueba_mat(void)
{
    for (double x = -9.5; x <= 9.5; x += 0.37) {
        CERCA(gm_sin(x), sin(x), 1e-12, "sin %g", x);
        CERCA(gm_cos(x), cos(x), 1e-12, "cos %g", x);
        CERCA(gm_atan(x), atan(x), 1e-12, "atan %g", x);
        CERCA(gm_exp(x), exp(x), 1e-11 * exp(x), "exp %g", x);
        CERCA(gm_pow(2.0, x), pow(2.0, x), 1e-11 * pow(2.0, x), "2^%g", x);
        if (fabs(cos(x)) > 0.05) CERCA(gm_tan(x), tan(x), 1e-9 * (1 + fabs(tan(x))), "tan %g", x);
        if (x > 0) { CERCA(gm_ln(x), log(x), 1e-12, "ln %g", x); CERCA(gm_sqrt(x), sqrt(x), 1e-13, "sqrt %g", x); CERCA(gm_log10(x), log10(x), 1e-12, "log10 %g", x); }
        if (fabs(x) <= 1) { CERCA(gm_asin(x), asin(x), 1e-12, "asin %g", x); CERCA(gm_acos(x), acos(x), 1e-12, "acos %g", x); }
        CERCA(gm_floor(x), floor(x), 0, "floor %g", x);
        CERCA(gm_fmod(x, 1.7), fmod(x, 1.7), 1e-12, "fmod %g", x);
    }
    CERCA(gm_sin(1e6), sin(1e6), 1e-9, "sin grande");
    CERCA(gm_pow(-2.0, 3.0), -8.0, 1e-12, "(-2)^3");
    CERCA(gm_pow(2.0, 0.5), sqrt(2.0), 1e-13, "2^0.5");
    VER(gm_nan(gm_pow(-2.0, 0.5)), "raíz de negativo");
    CERCA(gm_cbrt(-27.0), -3.0, 1e-12, "cbrt");
    CERCA(gm_tanh(0.5), tanh(0.5), 1e-12, "tanh");
}

static void prueba_expr(void)
{
    gbm_limpar();
    CERCA(ev1("2+3*4", 0), 14, 0, "precedencia");
    CERCA(ev1("-2^2", 0), -4, 0, "-2^2 = -(2^2)");
    CERCA(ev1("2^3^2", 0), 512, 0, "potencia por la derecha");
    CERCA(ev1("2x", 3), 6, 0, "producto implícito");
    CERCA(ev1("3(x+1)", 1), 6, 0, "3(x+1)");
    CERCA(ev1("(x+1)(x-1)", 3), 8, 0, "(x+1)(x-1)");
    CERCA(ev1("x\xB2 - 2", 3), 7, 0, "² postfijo");
    CERCA(ev1("sin(pi/2)", 0), 1, 1e-15, "sin(pi/2)");
    CERCA(ev1("SEN(pi/6)", 0), 0.5, 1e-15, "SEN en mayúsculas");
    CERCA(ev1("2^-1", 0), 0.5, 0, "exponente negativo");
    CERCA(ev1("1.5e2 + .5", 0), 150.5, 0, "notación científica");
    CERCA(ev1("log(100)", 0), 2, 1e-12, "log base 10");
    CERCA(ev1("log(2, 8)", 0), 3, 1e-12, "log con base");
    CERCA(ev1("atan2(1, 1)", 0), M_PI / 4, 1e-13, "atan2");
    CERCA(ev1("max(3, 7) + min(3, 7) + mod(-1, 3)", 0), 12, 0, "max min mod");
    VER(isinf(ev1("x/0", 1)), "dividir por cero da infinito");
    VER(gbm_analizar("2+", false) < 0 && strstr(gbm_erro, "incompleta"), "error de expresión incompleta: %s", gbm_erro);
    VER(gbm_analizar("2 $ 3", false) < 0, "carácter inesperado");
    VER(gbm_analizar("foo(2)", false) < 0 && strstr(gbm_erro, "foo"), "función desconocida: %s", gbm_erro);
    VER(gbm_analizar("x + 1", false) < 0 && strstr(gbm_erro, "solo se puede"), "x fuera de función: %s", gbm_erro);
    VER(gbm_analizar("(1, 2", false) < 0, "paréntesis sin cerrar");
    /* vectores */
    int r = gbm_analizar("(1, 2) + (3, 4)", false);
    gbm_val_t v = gbm_avaliar(r, 0);
    VER(v.vec && v.a == 4 && v.b == 6, "suma de puntos");
    r = gbm_analizar("2 * (1, 2)", false);
    v = gbm_avaliar(r, 0);
    VER(v.vec && v.a == 2 && v.b == 4, "número por punto");
    r = gbm_analizar("abs((3, 4))", false);
    v = gbm_avaliar(r, 0);
    VER(!v.vec && v.a == 5, "norma de un punto");
    r = gbm_analizar("abscisa((3, 4)) + ordenada((3, 4))", false);
    CERCA(gbm_avaliar(r, 0).a, 7, 0, "abscisa y ordenada");
}

static void prueba_derivada(void)
{
    gbm_limpar();
    const char *fs[] = { "x^2 - 2", "sin(x) + x^2/4", "x^3*cos(x)", "exp(2x)/(1+x^2)", "sqrt(x^2+1)", "x^x", "atan(3x)", "ln(x)/x", "2^x", "(x+1)^5", "tan(x)", "abs(x)" };
    const double pts[] = { 0.7, 1.3, 2.1 };
    for (unsigned i = 0; i < sizeof fs / sizeof fs[0]; i++) {
        int f = gbm_analizar(fs[i], true);
        int d = gbm_derivada(f);
        VER(f >= 0 && d >= 0, "derivar %s", fs[i]);
        for (int k = 0; k < 3; k++) {
            double x = pts[k], h = 1e-6;
            double num = (gbm_avaliar(f, x + h).a - gbm_avaliar(f, x - h).a) / (2 * h);
            double sim = gbm_avaliar(d, x).a;
            CERCA(sim, num, 1e-5 * (1 + fabs(num)), "d/dx %s en %g", fs[i], x);
        }
    }
    char t[200];
    int d = gbm_derivada(gbm_analizar("x^2 - 2", true));
    gbm_escrever(d, t, sizeof t, false);
    VER(strcmp(t, "2 * x") == 0, "derivada de x^2-2 sale limpia: '%s'", t);
    d = gbm_derivada(gbm_analizar("3x^2 + 5x + 1", true));
    gbm_escrever(d, t, sizeof t, false);
    VER(strcmp(t, "6 * x + 5") == 0, "derivada de un polinomio: '%s'", t);
    gbm_escrever(gbm_analizar("x^2 - 2", true), t, sizeof t, true);
    VER(strcmp(t, "x\xB2 - 2") == 0, "escritura bonita: '%s'", t);
    gbm_escrever(gbm_analizar("sin(x) + x^2/4", true), t, sizeof t, false);
    VER(strcmp(t, "sin(x) + x^2 / 4") == 0, "escritura: '%s'", t);
    gbm_escrever(gbm_analizar("2 - (3 - x)", true), t, sizeof t, false);
    VER(strcmp(t, "2 - (3 - x)") == 0, "paréntesis necesarios: '%s'", t);
    gbm_escrever(gbm_analizar("(a)", false), t, sizeof t, false);
    /* escribir y volver a leer da el mismo valor */
    const char *rs[] = { "-(x+1)^2", "2^(x+1)", "1/(x*(x+2))", "(x-1)/(x+1)^2", "-x^-2" };
    for (unsigned i = 0; i < sizeof rs / sizeof rs[0]; i++) {
        int a = gbm_analizar(rs[i], true);
        gbm_escrever(a, t, sizeof t, false);
        int b = gbm_analizar(t, true);
        VER(b >= 0, "'%s' -> '%s' se puede leer", rs[i], t);
        if (b >= 0) CERCA(gbm_avaliar(a, 0.37).a, gbm_avaliar(b, 0.37).a, 1e-12, "ida y vuelta de '%s' ('%s')", rs[i], t);
    }
}

static void prueba_formato(void)
{
    char t[48];
    struct { double v; int d; const char *s; } c[] = {
        { 3.14159, 2, "3.14" }, { 2.0, 2, "2" }, { -0.5, 2, "-0.5" }, { 0.004, 2, "0" }, { 1234567.891, 2, "1234567.89" },
        { -1.005, 3, "-1.005" }, { 1e-12, 4, "1e-12" }, { 2.5e20, 2, "2.5e20" }, { 100, 0, "100" }, { 0.1 + 0.2, 10, "0.3" },
    };
    for (unsigned i = 0; i < sizeof c / sizeof c[0]; i++) {
        gbm_formatar(c[i].v, c[i].d, t, sizeof t);
        VER(strcmp(t, c[i].s) == 0, "formato de %g con %d decimales: '%s' (esperaba '%s')", c[i].v, c[i].d, t, c[i].s);
    }
    const char *fim;
    CERCA(gbm_texto_num("12.375abc", &fim), 12.375, 1e-15, "leer número");
    VER(strcmp(fim, "abc") == 0, "resto tras el número");
}

static double px(const char *n) { double x, y; int i = gbm_buscar(n); return i >= 0 && gbm_ponto_xy(i, &x, &y) ? x : NAN; }
static double py(const char *n) { double x, y; int i = gbm_buscar(n); return i >= 0 && gbm_ponto_xy(i, &x, &y) ? y : NAN; }
static double num_de(const char *n) { int i = gbm_buscar(n); return i >= 0 && gbm_obj[i].ok ? gbm_obj[i].v[0] : NAN; }
static int ord(const char *t) { int r = gbm_orden(t, NULL); if (r) printf("   (orden '%s': %s)\n", t, gbm_erro); return r; }

static void prueba_geometria(void)
{
    gbm_limpar();
    VER(ord("A = (1, 2)") == 0, "crear A");
    VER(ord("B = (5, 4)") == 0, "crear B");
    VER(ord("M = PuntoMedio(A, B)") == 0, "punto medio");
    CERCA(px("M"), 3, 1e-12, "M.x"); CERCA(py("M"), 3, 1e-12, "M.y");
    /* arrastrar A: M lo sigue */
    gbm_arrastar(gbm_buscar("A"), -3, 0);
    CERCA(px("M"), 1, 1e-12, "M.x tras mover A"); CERCA(py("M"), 2, 1e-12, "M.y tras mover A");
    VER(ord("s = Segmento(A, B)") == 0, "segmento con nombre");
    VER(gbm_buscar("s") >= 0 && gbm_obj[gbm_buscar("s")].tipo == G_SEGMENTO, "el segmento se llama s");
    VER(ord("d = Distancia(A, B)") == 0, "distancia");
    CERCA(num_de("d"), sqrt(64 + 16), 1e-12, "|AB|");
    VER(ord("m = Mediatriz(s)") == 0, "mediatriz de un segmento");
    VER(ord("P = Interseca(m, s, 1)") == 0, "corte de la mediatriz y el segmento");
    CERCA(px("P"), px("M"), 1e-12, "la mediatriz corta al segmento en su punto medio");
    CERCA(py("P"), py("M"), 1e-12, "idem y");
    /* circunferencias */
    VER(ord("c = Circunferencia(A, B)") == 0, "circunferencia centro y punto");
    VER(ord("Interseca(c, m)") == 0, "cortes recta y circunferencia (varios)");
    int antes = gbm_n;
    VER(antes >= 9, "hay objetos");
    /* triángulo, circuncentro por tres puntos */
    VER(ord("T1 = (0, 0)") == 0 && ord("T2 = (4, 0)") == 0 && ord("T3 = (0, 3)") == 0, "triángulo");
    VER(ord("circ = Circunferencia(T1, T2, T3)") == 0, "circunferencia por tres puntos");
    int ci = gbm_buscar("circ");
    CERCA(gbm_obj[ci].v[0], 2, 1e-12, "circuncentro x"); CERCA(gbm_obj[ci].v[1], 1.5, 1e-12, "circuncentro y"); CERCA(gbm_obj[ci].v[2], 2.5, 1e-12, "circunradio");
    VER(ord("tri = Poligono(T1, T2, T3)") == 0, "polígono");
    VER(ord("ar = Area(tri)") == 0, "área");
    CERCA(num_de("ar"), 6, 1e-12, "área del triángulo 3-4-5");
    VER(ord("ang = Angulo(T2, T1, T3)") == 0, "ángulo");
    CERCA(num_de("ang"), M_PI / 2, 1e-12, "ángulo recto");
    VER(ord("pe = Perpendicular(T1, Recta(T2, T3))") == 0, "perpendicular");
    VER(ord("bis = Bisectriz(T2, T1, T3)") == 0, "bisectriz");
    int bi = gbm_buscar("bis");
    CERCA(gbm_obj[bi].v[2] - gbm_obj[bi].v[0], gbm_obj[bi].v[3] - gbm_obj[bi].v[1], 1e-12, "la bisectriz del ángulo recto va a 45 grados");
    VER(ord("reg = Poligono(T1, T2, 6)") == 0, "hexágono regular");
    int rg = gbm_buscar("reg");
    CERCA(gbm_obj[rg].v[1], 24, 1e-9, "perímetro del hexágono de lado 4");
    CERCA(gbm_obj[rg].v[0], 6 * 16 * sqrt(3) / 4, 1e-9, "área del hexágono");
    /* paralela y corte de dos rectas */
    VER(ord("pa = Paralela(T3, Recta(T1, T2))") == 0, "paralela");
    int pa = gbm_buscar("pa");
    CERCA(gbm_obj[pa].v[3] - gbm_obj[pa].v[1], 0, 1e-12, "paralela horizontal");
    /* dependencias */
    VER(gbm_depende(gbm_buscar("P"), gbm_buscar("A")), "P depende de A");
    VER(!gbm_depende(gbm_buscar("A"), gbm_buscar("P")), "A no depende de P");
    /* un segmento limita: la recta no corta fuera */
    VER(ord("S1 = Segmento((0, 0), (1, 0))") == 0, "segmento corto");
    VER(ord("r2 = Recta((3, -1), (3, 1))") == 0, "recta vertical");
    double xs[4], ys[4];
    VER(gbm_interseccoes(gbm_buscar("S1"), gbm_buscar("r2"), xs, ys, 4) == 0, "un segmento no se prolonga");
    VER(gbm_interseccoes(gbm_buscar("r2"), gbm_buscar("c"), xs, ys, 4) >= 0, "recta y circunferencia");
}

static void prueba_funciones(void)
{
    gbm_limpar();
    VER(ord("f(x) = x^2 - 2") == 0, "definir f");
    int f = gbm_buscar("f");
    VER(f >= 0 && gbm_obj[f].tipo == G_FUNCAO, "f es una función");
    CERCA(gbm_valor_funcao(f, 3), 7, 0, "f(3)");
    VER(ord("Raiz(f)") == 0, "raíces");
    VER(gbm_buscar("A") >= 0 && gbm_buscar("B") >= 0 && gbm_buscar("C") < 0, "dos raíces: A y B");
    CERCA(px("A"), -sqrt(2.0), 1e-9, "raíz negativa"); CERCA(px("B"), sqrt(2.0), 1e-9, "raíz positiva");
    CERCA(py("A"), 0, 0, "y de la raíz");
    VER(ord("g(x) = 2x") == 0, "otra función");
    VER(ord("Interseca(f, g)") == 0, "corte de dos funciones");
    int na = gbm_n;
    (void)na;
    VER(ord("E = Extremo(f, 1)") == 0, "extremo");
    CERCA(px("E"), 0, 1e-5, "el vértice de la parábola"); CERCA(py("E"), -2, 1e-9, "y del vértice");
    VER(ord("h = Derivada(f)") == 0, "derivada");
    CERCA(gbm_valor_funcao(gbm_buscar("h"), 3), 6, 1e-9, "f'(3)");
    /* deslizadores */
    VER(ord("a = 2") == 0, "a = 2 crea un deslizador");
    VER(gbm_obj[gbm_buscar("a")].tipo == G_SLIDER, "es un deslizador");
    VER(ord("p(x) = a x^2 - 1") == 0, "parábola con deslizador");
    CERCA(gbm_valor_funcao(gbm_buscar("p"), 2), 7, 0, "p(2) con a=2");
    VER(ord("a = 3") == 0, "cambiar a");
    CERCA(gbm_valor_funcao(gbm_buscar("p"), 2), 11, 0, "p(2) con a=3: se recalcula");
    VER(ord("Raiz(p)") == 0, "raíces de p");
    /* raíz doble y asíntotas */
    VER(ord("q(x) = (x - 1)^2") == 0, "raíz doble");
    int antes = gbm_n;
    VER(ord("Raiz(q)") == 0 && gbm_n == antes + 1, "la raíz doble se encuentra una sola vez");
    VER(ord("t(x) = 1/x") == 0, "hipérbola");
    VER(ord("Raiz(t)") != 0, "1/x no tiene raíces: la asíntota no cuenta");
    VER(ord("u(x) = tan(x)") == 0, "tangente");
    antes = gbm_n;
    VER(ord("Raiz(u, 1)") == 0, "raíz de tan");
    /* B = A + (3, 0) */
    VER(ord("Q = (1, 1)") == 0, "punto Q");
    VER(ord("R = Q + (3, 0)") == 0, "punto dependiente");
    CERCA(px("R"), 4, 0, "R.x");
    gbm_arrastar(gbm_buscar("Q"), 10, 2);
    CERCA(px("R"), 13, 0, "R sigue a Q");
    VER(ord("Q = (7, 7)") == 0, "mover Q con una orden");
    CERCA(px("R"), 10, 0, "R tras la orden");
    VER(ord("cos(") != 0, "error de sintaxis");
    VER(ord("Recta(Z, W)") != 0, "nombres que no existen");
    VER(ord("Recta(Q)") != 0, "faltan argumentos");
    VER(ord("sin = 3") != 0, "nombre reservado");
    VER(ord("Q = Q + (1, 0)") != 0, "definición circular (Q es libre)");
}

static void prueba_ficheros(void)
{
    gbm_limpar();
    ord("A = (1, 2)"); ord("B = (4, -1)"); ord("M = PuntoMedio(A, B)"); ord("s = Segmento(A, B)");
    ord("c = Circunferencia(M, 3)"); ord("f(x) = sin(x) + x^2/4"); ord("a = 1.5"); ord("g(x) = a * f(x)");
    ord("Interseca(c, s)"); ord("t = Texto(\"hola\", 1, 2)"); ord("poli = Poligono(A, B, (0, 5))");
    ord("P = PuntoEn(c, 0.7)"); ord("D = Derivada(g)"); ord("Raiz(f)");
    gbm_obj[gbm_buscar("A")].cor = 0xFF0000; gbm_obj[gbm_buscar("A")].grosor = 5; gbm_obj[gbm_buscar("s")].estilo = 1; gbm_obj[gbm_buscar("c")].visivel = 0;
    static char t1[65536], t2[65536];
    double vista[3] = { 1.5, -2, 33 }, v2[3] = { 0, 0, 0 };
    gbm_recalcular();
    gbm_serializar(t1, sizeof t1, vista);
    int n1 = gbm_n;
    VER(strstr(t1, "A = (1, 2)") && strstr(t1, "M = PuntoMedio(A, B)") && strstr(t1, "f(x) = sin(x) + x^2 / 4"), "el fichero es legible");
    VER(gbm_carregar(t1, v2) == 0, "cargar lo guardado: %s", gbm_erro);
    VER(gbm_n == n1, "mismo número de objetos (%d y %d)", gbm_n, n1);
    CERCA(v2[2], 33, 0, "la vista se conserva");
    gbm_serializar(t2, sizeof t2, vista);
    VER(strcmp(t1, t2) == 0, "serializar, cargar y serializar da el mismo texto");
    if (strcmp(t1, t2)) printf("---\n%s\n---\n%s\n---\n", t1, t2);
    VER(gbm_obj[gbm_buscar("A")].cor == 0xFF0000 && gbm_obj[gbm_buscar("A")].grosor == 5 && gbm_obj[gbm_buscar("s")].estilo == 1 && !gbm_obj[gbm_buscar("c")].visivel, "las propiedades se guardan");
    CERCA(px("M"), 2.5, 1e-9, "M tras cargar");
    /* borrar un punto borra lo que depende de él */
    int antes = gbm_n;
    gbm_apagar(gbm_buscar("A"));
    VER(gbm_buscar("A") < 0 && gbm_buscar("M") < 0 && gbm_buscar("s") < 0 && gbm_buscar("poli") < 0, "borrar A arrastra a M, s y el polígono");
    VER(gbm_buscar("f") >= 0 && gbm_buscar("B") >= 0 && gbm_n < antes, "el resto se conserva");
    VER(gbm_carregar("A = (1, 2\n", NULL) != 0 && strstr(gbm_erro, "Línea 1"), "error con número de línea: %s", gbm_erro);
    VER(gbm_carregar("A = (1, 2)\nB = Recta(A, Z)\n", NULL) != 0 && strstr(gbm_erro, "Línea 2"), "error en la línea 2: %s", gbm_erro);
    /* las órdenes con acentos */
    gbm_limpar();
    ord("f(x) = x^2 - 4");
    VER(ord("Ra\xED" "z(f)") == 0, "«Raíz» con tilde");
    VER(ord("a = Pol\xED" "gono((0,0), (1,0), (0,1))") == 0, "«Polígono» con tilde");
    VER(ord("Q = Punto(f)") == 0, "punto sobre una función");
}

static void prueba_compactar(void)
{
    /* Muchas órdenes llenan el cajón de nodos: el motor recarga la construcción para compactarlo y nada se pierde. */
    gbm_limpar();
    ord("A = (1, 2)"); ord("f(x) = sin(x) + x^2/4"); ord("a = 1"); ord("g(x) = a * f(x)");
    int max_nodos = 0;
    for (int i = 0; i < 9000; i++) {
        char t[64];
        snprintf(t, sizeof t, "a = %d", i % 7);
        if (gbm_orden(t, NULL) != 0) { VER(0, "orden %d: %s", i, gbm_erro); break; }
        if (gbm_nodos_usados() > max_nodos) max_nodos = gbm_nodos_usados();
    }
    VER(max_nodos < GBM_NODOS, "el cajón de nodos no se desborda (máximo %d de %d)", max_nodos, GBM_NODOS);
    CERCA(num_de("a"), 8999 % 7, 0, "a conserva su valor tras compactar");
    CERCA(gbm_valor_funcao(gbm_buscar("g"), 2), (8999 % 7) * (sin(2.0) + 1.0), 1e-9, "g(2) tras compactar");
    VER(gbm_buscar("A") >= 0 && gbm_n == 4, "los objetos siguen siendo cuatro (%d)", gbm_n);
    /* una expresión larga (más de 256 caracteres) se guarda y se recarga entera */
    static char larga[700], t[800];
    strcpy(larga, "x");
    for (int i = 0; i < 60; i++) strcat(larga, " + 0.5 * x");
    snprintf(t, sizeof t, "h(x) = %s", larga);
    VER(gbm_orden(t, NULL) == 0, "función de más de 400 caracteres: %s", gbm_erro);
    static char fichero[131072];
    gbm_serializar(fichero, sizeof fichero, NULL);
    VER(gbm_carregar(fichero, NULL) == 0, "se recarga: %s", gbm_erro);
    CERCA(gbm_valor_funcao(gbm_buscar("h"), 2), 2 + 60 * 0.5 * 2, 1e-9, "h(2) tras recargar");
}

static void prueba_ejemplos(const char *carpeta)
{
    const char *nombres[] = { "triangulo", "circunferencia-3-puntos", "parabola", "tales" };
    static char buf[65536];
    for (int k = 0; k < 4; k++) {
        char ruta[300];
        snprintf(ruta, sizeof ruta, "%s/%s.gbm", carpeta, nombres[k]);
        FILE *f = fopen(ruta, "rb");
        VER(f != NULL, "existe %s", ruta);
        if (!f) continue;
        size_t n = fread(buf, 1, sizeof buf - 1, f);
        buf[n] = 0;
        fclose(f);
        double v[3] = { 0, 0, 0 };
        VER(gbm_carregar(buf, v) == 0, "%s se carga: %s", nombres[k], gbm_erro);
        int mal = 0;
        for (int i = 0; i < gbm_n; i++) if (!gbm_obj[i].ok && gbm_obj[i].tipo != G_FUNCAO) { mal++; printf("   %s: %s no está definido\n", nombres[k], gbm_obj[i].nome); }
        VER(mal == 0, "%s: todo definido", nombres[k]);
        VER(v[2] > 0, "%s trae su vista", nombres[k]);
        if (k == 0) {   /* la recta de Euler: O, G y H están alineados */
            double ox = px("O"), oy = py("O"), gx = px("G"), gy = py("G"), hx = px("H"), hy = py("H");
            CERCA((gx - ox) * (hy - oy) - (gy - oy) * (hx - ox), 0, 1e-9, "O, G y H alineados");
            CERCA(hx - ox, 3 * (gx - ox), 1e-9, "OH = 3 OG");
            double ix = px("I"), iy = py("I");
            VER(gm_fin(ix) && gm_fin(iy), "incentro definido");
            /* arrastrar C y comprobar que sigue la relación */
            gbm_arrastar(gbm_buscar("C"), 0.3, 4.2);
            ox = px("O"); oy = py("O"); gx = px("G"); gy = py("G"); hx = px("H"); hy = py("H");
            CERCA(hx - ox, 3 * (gx - ox), 1e-9, "OH = 3 OG tras arrastrar C");
            CERCA(hy - oy, 3 * (gy - oy), 1e-9, "idem y");
        }
        if (k == 1) CERCA(num_de("r"), gbm_obj[gbm_buscar("c")].v[2], 1e-12, "el radio medido es el de la circunferencia");
        if (k == 2) {
            CERCA(px("R1"), -1, 1e-9, "raíz 1 de x²-2x-3"); CERCA(px("R2"), 3, 1e-9, "raíz 2");
            CERCA(px("V"), 1, 1e-4, "vértice"); CERCA(py("V"), -4, 1e-6, "vértice y");
            VER(gbm_orden("a = 2", NULL) == 0, "mover el deslizador a");
            CERCA(px("V"), 0.5, 1e-3, "el vértice se recalcula con a = 2");
        }
        if (k == 3) { CERCA(num_de("k1"), num_de("k2"), 1e-9, "Tales: OA/OB = OC/OD"); gbm_arrastar(gbm_buscar("B"), 5, 2.5); CERCA(num_de("k1"), num_de("k2"), 1e-9, "Tales tras mover B"); }
    }
}

int main(int argc, char **argv)
{
    prueba_mat();
    prueba_expr();
    prueba_derivada();
    prueba_formato();
    prueba_geometria();
    prueba_funciones();
    prueba_ficheros();
    prueba_compactar();
    prueba_ejemplos(argc > 1 ? argv[1] : "rootfs/ejemplos/gebramini");
    printf("gebramini_unidad: %d comprobaciones, %d fallos\n", total, fallos);
    if (!fallos) printf("todo bien\n");
    return fallos != 0;
}
