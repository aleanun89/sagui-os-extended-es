// SPDX-License-Identifier: GPL-3.0-only
/* texto.c - rutinas de memoria y de texto del kernel.
 *
 * memcpy y memset usan "rep movsd" / "rep stosd" (D042): 4 bytes por paso
 * en cualquier i686 y, en las CPU con "fast strings" (desde el Pentium Pro; el
 * ERMSB del Sandy Bridge del G470 mejora aún más), la propia CPU copia en
 * bloques de 16 o más bytes. No usan SSE: se llaman desde cualquier sitio,
 * incluso desde interrupciones. Las copias grandes de gráficos usan rapido.c. */
#include "texto.h"

void *memcpy(void *dst, const void *src, size_t n)
{
    void *d = dst;
    const void *s = src;
    uint32_t q = n >> 2, r = n & 3;
    __asm__ volatile("rep movsl" : "+D"(d), "+S"(s), "+c"(q) : : "memory");
    __asm__ volatile("rep movsb" : "+D"(d), "+S"(s), "+c"(r) : : "memory");
    return dst;
}

void *memmove(void *dst, const void *src, size_t n)
{
    uint8_t *d = dst;
    const uint8_t *s = src;
    if (d == s || n == 0)
        return dst;
    if (d < s || d >= s + n)
        return memcpy(dst, src, n);
    /* Regiones solapadas con destino después del origen: copia de atrás hacia
 * delante (DF activado solo durante la copia; el resto del kernel cuenta con DF=0). */
    void *dp = d + n - 1;
    const void *sp = s + n - 1;
    __asm__ volatile("std; rep movsb; cld" : "+D"(dp), "+S"(sp), "+c"(n) : : "memory");
    return dst;
}

void *memset(void *dst, int v, size_t n)
{
    void *d = dst;
    uint32_t v4 = (uint8_t)v * 0x01010101u;
    uint32_t q = n >> 2, r = n & 3;
    __asm__ volatile("rep stosl" : "+D"(d), "+c"(q) : "a"(v4) : "memory");
    __asm__ volatile("rep stosb" : "+D"(d), "+c"(r) : "a"(v4) : "memory");
    return dst;
}

int memcmp(const void *a, const void *b, size_t n)
{
    const uint8_t *x = a, *y = b;
    for (; n; n--, x++, y++)
        if (*x != *y)
            return *x - *y;
    return 0;
}

size_t txt_tamanho(const char *s)
{
    size_t n = 0;
    while (s[n]) n++;
    return n;
}

int txt_compara(const char *a, const char *b)
{
    while (*a && *a == *b) { a++; b++; }
    return (uint8_t)*a - (uint8_t)*b;
}

int txt_compara_n(const char *a, const char *b, size_t n)
{
    for (; n; n--, a++, b++) {
        if (*a != *b) return (uint8_t)*a - (uint8_t)*b;
        if (!*a) return 0;
    }
    return 0;
}

/* Copia como máximo n-1 caracteres y siempre termina con cero (n > 0). */
char *txt_copia_n(char *dst, const char *src, size_t n)
{
    size_t i = 0;
    if (!n) return dst;
    for (; i + 1 < n && src[i]; i++)
        dst[i] = src[i];
    dst[i] = 0;
    return dst;
}

bool txt_comeca_com(const char *s, const char *prefixo)
{
    while (*prefixo)
        if (*s++ != *prefixo++)
            return false;
    return true;
}
