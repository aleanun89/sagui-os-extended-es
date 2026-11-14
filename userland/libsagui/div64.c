// SPDX-License-Identifier: GPL-2.0-or-later
/* div64.c - la división de 64 bits que pide el gcc (mejora 094, punto 9).
 *
 * El userland no enlaza la libgcc: en cuanto un programa divide un uint64_t,
 * el gcc llama a __udivdi3 y compañía. Hasta ahora cada port llevaba su copia
 * (sqlite_libc.c, QuickJS, OpenTyrian, NetSurf...). Aquí van para todos, pero
 * **débiles**: si un programa enlaza además una copia fuerte, manda esa y no
 * hay símbolos repetidos. Quitar las copias será parte de la libc común (094,
 * bloque A). Solo usa la división de 32 bits y desplazamientos. */
#define DEBIL __attribute__((weak))

DEBIL unsigned long long __udivmoddi4(unsigned long long a, unsigned long long b, unsigned long long *resto)
{
    if (b == 0) {                      /* como el hardware: no se divide por cero */
        if (resto) *resto = a;
        return ~0ULL;
    }
    unsigned int bh = (unsigned int)(b >> 32);
    if (bh == 0 && (unsigned int)(a >> 32) < (unsigned int)b) {
        /* el caso corriente: el cociente cabe en 32 bits, un solo divl */
        unsigned int q, r;
        __asm__("divl %4" : "=a"(q), "=d"(r) : "a"((unsigned int)a), "d"((unsigned int)(a >> 32)), "r"((unsigned int)b));
        if (resto) *resto = r;
        return q;
    }
    unsigned long long q = 0, r = 0;   /* bit a bit: lento pero simple y exacto */
    for (int i = 63; i >= 0; i--) {
        r = r << 1 | ((a >> i) & 1);
        if (r >= b) { r -= b; q |= 1ULL << i; }
    }
    if (resto) *resto = r;
    return q;
}

DEBIL unsigned long long __udivdi3(unsigned long long a, unsigned long long b) { return __udivmoddi4(a, b, 0); }
DEBIL unsigned long long __umoddi3(unsigned long long a, unsigned long long b)
{
    unsigned long long r;
    __udivmoddi4(a, b, &r);
    return r;
}

DEBIL long long __divmoddi4(long long a, long long b, long long *resto)
{
    int neg = (a < 0) != (b < 0);
    unsigned long long ua = a < 0 ? -(unsigned long long)a : (unsigned long long)a;
    unsigned long long ub = b < 0 ? -(unsigned long long)b : (unsigned long long)b, r;
    unsigned long long q = __udivmoddi4(ua, ub, &r);
    if (resto) *resto = a < 0 ? -(long long)r : (long long)r;
    return neg ? -(long long)q : (long long)q;
}

DEBIL long long __divdi3(long long a, long long b) { return __divmoddi4(a, b, 0); }
DEBIL long long __moddi3(long long a, long long b)
{
    long long r;
    __divmoddi4(a, b, &r);
    return r;
}

/* __popcountsi2: los bits a 1 de una palabra (el gcc lo llama para
 * __builtin_popcount sin la instrucción popcnt; GNU Prolog lo usa). */
DEBIL int __popcountsi2(unsigned int v)
{
    v = v - ((v >> 1) & 0x55555555u);
    v = (v & 0x33333333u) + ((v >> 2) & 0x33333333u);
    v = (v + (v >> 4)) & 0x0F0F0F0Fu;
    return (int)((v * 0x01010101u) >> 24);
}
