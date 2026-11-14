// SPDX-License-Identifier: GPL-2.0-or-later
/* prova_numeros.c - compara la conversión de reales de la libc común
 * (userland/libc/numeros.c, D210) con la glibc del PC.
 *
 * Se compila en el PC (no en el SaguiOS) junto con numeros.c, que se renombra
 * con -Dstrtod=libc_strtod -Dstrtof=libc_strtof -Datof=libc_atof. La glibc
 * redondea bien (es el oráculo):
 *
 *   prova aleatorio N semilla   dobles al azar (todos los patrones de bits,
 *                               subnormales incluidos) por %e/%f/%g con
 *                               precisión y '#' al azar, ida y vuelta con
 *                               %.17g, y cadenas decimales al azar por strtod
 *                               y strtof;
 *   prova entrada               una cadena por línea de la entrada estándar
 *                               (los casos difíciles los genera
 *                               tests/h210_numeros_libc.py) por strtod/strtof.
 *
 * Escribe los fallos (los 30 primeros) y un resumen; sale con 1 si hay fallos.
 */
#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../userland/libc/numeros_libc.h"

double libc_strtod(const char *s, char **fim);
float libc_strtof(const char *s, char **fim);

static long fallos, pruebas;

static void fallo(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void fallo(const char *fmt, ...)
{
    fallos++;
    if (fallos > 30) return;
    va_list ap;
    va_start(ap, fmt);
    printf("FALLO: ");
    vprintf(fmt, ap);
    printf("\n");
    va_end(ap);
}

/* xorshift64*: el mismo resultado en cualquier PC con la misma semilla */
static uint64_t estado = 88172645463325252ull;
static uint64_t azar(void)
{
    estado ^= estado >> 12;
    estado ^= estado << 25;
    estado ^= estado >> 27;
    return estado * 2685821657736338717ull;
}

static double de_bits(uint64_t u) { double d; memcpy(&d, &u, 8); return d; }
static uint64_t bits_de(double d) { uint64_t u; memcpy(&u, &d, 8); return u; }
static uint32_t bits_f(float f) { uint32_t u; memcpy(&u, &f, 4); return u; }

/* Lo que daría el printf de la libc común: signo + __libc_real_texto. */
static void nossa(char *saida, double v, char conv, int prec, int alt)
{
    char *p = saida;
    if (bits_de(v) >> 63) *p++ = '-';
    __libc_real_texto(p, v, conv, prec, alt);
}

/* Fallo conocido de la glibc: con %#g, si el redondeo añade una cifra
 * (999999.5 -> 1e+06), se come los ceros que '#' obliga a dejar («1.e+06» en
 * vez de «1.00000e+06»; Python y el estándar C dan lo segundo). Se acepta si
 * lo nuestro, sin esos ceros, es lo de la glibc. */
static long fallos_glibc;
static int glibc_g_alt(const char *nosso, const char *deles)
{
    char t[LIBC_REAL_TAM + 8];
    const char *ponto = strchr(nosso, '.'), *e = strpbrk(nosso, "eE");
    if (!ponto || !e || e < ponto) return 0;
    const char *q = ponto + 1;
    while (q < e && *q == '0') q++;
    if (q != e) return 0;
    snprintf(t, sizeof t, "%.*s%s", (int)(ponto + 1 - nosso), nosso, e);
    if (strcmp(t, deles)) return 0;
    fallos_glibc++;
    return 1;
}

static void compara_formato(double v, char conv, int prec, int alt)
{
    static char nosso[LIBC_REAL_TAM + 8], deles[LIBC_REAL_TAM + 8];
    char fmt[16];
    snprintf(fmt, sizeof fmt, "%%%s.*%c", alt ? "#" : "", conv);
    snprintf(deles, sizeof deles, fmt, prec, v);
    nossa(nosso, v, conv, prec, alt);
    pruebas++;
    if (strcmp(nosso, deles) && alt && (conv | 0x20) == 'g' && glibc_g_alt(nosso, deles)) return;
    if (strcmp(nosso, deles))
        fallo("printf %s prec %d de %a (bits %016llx): libc «%s» glibc «%s»", fmt, prec, v,
              (unsigned long long)bits_de(v), nosso, deles);
}

static void compara_strtod(const char *s)
{
    char *f1, *f2;
    errno = 0;
    double a = libc_strtod(s, &f1);
    int e1 = errno;
    errno = 0;
    double b = strtod(s, &f2);
    int e2 = errno;
    pruebas++;
    int igual = bits_de(a) == bits_de(b) || (a != a && b != b && (bits_de(a) >> 63) == (bits_de(b) >> 63));
    if (!igual || f1 != f2 || (e1 == ERANGE) != (e2 == ERANGE))
        fallo("strtod «%.80s»: libc %a fin %d errno %d; glibc %a fin %d errno %d", s,
              a, (int)(f1 - s), e1, b, (int)(f2 - s), e2);
    errno = 0;
    float c = libc_strtof(s, &f1);
    e1 = errno;
    errno = 0;
    float d = strtof(s, &f2);
    e2 = errno;
    pruebas++;
    igual = bits_f(c) == bits_f(d) || (c != c && d != d);
    if (!igual || f1 != f2 || (e1 == ERANGE) != (e2 == ERANGE))
        fallo("strtof «%.80s»: libc %a fin %d errno %d; glibc %a fin %d errno %d", s,
              (double)c, (int)(f1 - s), e1, (double)d, (int)(f2 - s), e2);
}

static void prueba_doble(double v)
{
    static const char convs[] = "efgEG";
    /* las precisiones habituales y unas al azar */
    compara_formato(v, 'g', 17, 0);
    compara_formato(v, 'e', 16, 0);
    compara_formato(v, 'g', 6, 0);
    compara_formato(v, 'f', 6, 0);
    compara_formato(v, 'e', 6, 0);
    for (int i = 0; i < 4; i++) {
        char c = convs[azar() % 5];
        int prec = (int)(azar() % (c == 'f' ? 40 : 30));
        compara_formato(v, c, prec, (int)(azar() & 1));
    }
    /* ida y vuelta */
    if (v == v) {
        char t[64];
        snprintf(t, sizeof t, "%.17g", v);
        double w = libc_strtod(t, NULL);
        pruebas++;
        if (bits_de(w) != bits_de(v)) fallo("ida y vuelta %%.17g de %a: «%s» -> %a", v, t, w);
        char n[LIBC_REAL_TAM + 8];
        nossa(n, v, 'e', 16, 0);   /* nuestra salida leída por la glibc */
        pruebas++;
        if (bits_de(strtod(n, NULL)) != bits_de(v)) fallo("ida y vuelta nuestro %%.16e de %a: «%s»", v, n);
        compara_strtod(t);
        float fl = (float)v;
        if (fl == fl) {
            snprintf(t, sizeof t, "%.9g", (double)fl);
            pruebas++;
            if (bits_f(libc_strtof(t, NULL)) != bits_f(fl)) fallo("ida y vuelta %%.9g de float %a", (double)fl);
        }
    }
}

static void cadena_azar(char *s)
{
    char *p = s;
    int cifras = 1 + (int)(azar() % 25);
    if (azar() % 4 == 0) *p++ = '-';
    int punto = (int)(azar() % (cifras + 1));
    for (int i = 0; i < cifras; i++) {
        if (i == punto && azar() % 2) *p++ = '.';
        *p++ = (char)('0' + azar() % 10);
    }
    if (azar() % 3) p += sprintf(p, "e%d", (int)(azar() % 680) - 350);
    *p = 0;
}

static const double especiales[] = {
    0.0, -0.0, 0.1, 0.2, 0.3, 1e23, 1e22, 5e-324, 1e-323, 2.2250738585072014e-308,
    2.2250738585072009e-308, 1.7976931348623157e308, 0.5, 1.5, 2.5, 9.5, 0.125, 0.375,
    123456789012345678.0, 9007199254740992.0, 9007199254740994.0, 1e-300, 1e300, 1e-5, 1e-4,
    123456.0, 1234567.0, 0.000123456, 999999.5, 9.9999995, 99.5, 0.05, 0.15, 0.25, 1.0 / 3.0,
    2.0 / 3.0, 3.141592653589793, 2.718281828459045, 1e15, 1e16, 1e17, 4.35, 8.125,
};

int main(int argc, char **argv)
{
    if (argc >= 2 && !strcmp(argv[1], "entrada")) {
        static char linha[1 << 16];
        while (fgets(linha, sizeof linha, stdin)) {
            size_t n = strlen(linha);
            if (n && linha[n - 1] == '\n') linha[--n] = 0;
            compara_strtod(linha);
        }
    } else if (argc >= 4 && !strcmp(argv[1], "aleatorio")) {
        long n = atol(argv[2]);
        estado ^= (uint64_t)atoll(argv[3]) * 0x9E3779B97F4A7C15ull;
        if (!estado) estado = 1;
        for (size_t i = 0; i < sizeof especiales / sizeof especiales[0]; i++) {
            prueba_doble(especiales[i]);
            prueba_doble(-especiales[i]);
            for (int prec = 0; prec < 25; prec++)
                for (const char *c = "efgEG"; *c; c++) {
                    compara_formato(especiales[i], *c, prec, 0);
                    compara_formato(especiales[i], *c, prec, 1);
                }
        }
        double raros[] = { __builtin_inf(), -__builtin_inf(), __builtin_nan(""), -__builtin_nan("") };
        for (int i = 0; i < 4; i++)
            for (const char *c = "efgEG"; *c; c++) compara_formato(raros[i], *c, 3, 0);
        for (long i = 0; i < n; i++) {
            uint64_t u = azar();
            /* la mitad, en todo el rango; la otra mitad, cerca de 1 y subnormales */
            switch (i % 4) {
            case 1: u = (u & 0x800FFFFFFFFFFFFFull) | ((uint64_t)(1023 - 40 + (int)(azar() % 80)) << 52); break;
            case 2: u &= 0x800FFFFFFFFFFFFFull; break;   /* subnormal */
            case 3: u = (u & 0x800FFFFFFFFFFFFFull) | ((uint64_t)(1023 + 52) << 52); break;
            }
            double v = de_bits(u);
            if (v != v || v - v != 0) continue;
            prueba_doble(v);
            char s[128];
            cadena_azar(s);
            compara_strtod(s);
        }
        /* %f largo: todas las cifras de los extremos */
        compara_formato(5e-324, 'f', 1100, 0);
        compara_formato(1.7976931348623157e308, 'f', 20, 0);
        compara_formato(2.2250738585072009e-308, 'e', 800, 0);
        compara_formato(0.1, 'f', 60, 0);
    } else {
        fprintf(stderr, "uso: prova aleatorio N semilla | prova entrada\n");
        return 2;
    }
    printf("pruebas %ld, fallos %ld (y %ld del %%#g de la glibc, aceptados)\n", pruebas, fallos, fallos_glibc);
    return fallos ? 1 : 0;
}
