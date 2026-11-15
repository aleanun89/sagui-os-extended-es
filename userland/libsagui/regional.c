// SPDX-License-Identifier: GPL-2.0-or-later
/* regional.c - formatos regionales según el idioma: decimal, miles y fecha (D313, mejora 118).
 * Español y portugués de Brasil: «1.234,56» y 31/12/2026; inglés: «1,234.56» y 12/31/2026. */
#include "sagui.h"

static bool es_ingles(void)
{
    const char *l = idioma_actual();
    return l[0] == 'e' && l[1] == 'n' && !l[2];
}

char regional_decimal(void) { return es_ingles() ? '.' : ','; }
char regional_miles(void) { return es_ingles() ? ',' : '.'; }

/* Escribe el valor absoluto "u" con "dec" decimales implícitos; devuelve la longitud (sin cerrar el cero). */
static int escribir(char *dst, size_t max, unsigned long u, int dec, bool neg, bool miles)
{
    char dig[24];
    int nd = 0;
    do { dig[nd++] = (char)('0' + u % 10); u /= 10; } while (u && nd < 22);
    while (nd <= dec && nd < 22) dig[nd++] = '0';   /* al menos un dígito entero */
    size_t k = 0;
    if (neg && k + 1 < max) dst[k++] = '-';
    for (int i = nd - 1; i >= 0; i--) {
        if (k + 1 >= max) break;
        dst[k++] = dig[i];
        if (i == dec && dec > 0 && k + 1 < max) dst[k++] = regional_decimal();
        else if (miles && i > dec && (i - dec) % 3 == 0 && k + 1 < max) dst[k++] = regional_miles();
    }
    if (max) dst[k] = 0;
    return (int)k;
}

int formatar_decimal_regional(char *dst, size_t max, long mantisa, int decimales, bool miles)
{
    if (!max) return 0;
    if (decimales < 0) decimales = 0;
    if (decimales > 9) decimales = 9;
    bool neg = mantisa < 0;
    unsigned long u = neg ? 0ul - (unsigned long)mantisa : (unsigned long)mantisa;
    return escribir(dst, max, u, decimales, neg, miles);
}

int formatar_inteiro_regional(char *dst, size_t max, long n) { return formatar_decimal_regional(dst, max, n, 0, true); }

int formatar_data_regional(char *dst, size_t max, int dia, int mes, int ano)
{
    if (es_ingles()) return formatar_texto(dst, max, "%02d/%02d/%04d", mes, dia, ano);
    return formatar_texto(dst, max, "%02d/%02d/%04d", dia, mes, ano);
}
