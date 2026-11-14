// SPDX-License-Identifier: GPL-2.0-or-later
/* div64.c - las rutinas de 64 bits que el GCC llama en 32 bits (D192).
 *
 * En i386 el GCC no sabe dividir enteros de 64 bits con instrucciones: llama a
 * __udivdi3, __umoddi3, __divdi3... de la libgcc. El kernel no enlaza la
 * libgcc (-nostdlib) y hasta ahora no dividía en 64 bits (ver smp.c). uACPI sí
 * lo hace: los enteros del AML son de 64 bits (Divide, Mod, el formato de
 * los números del registro). Aquí van escritas a mano, por restas y
 * desplazamientos (un bit por vuelta, 64 vueltas como mucho): son lentas
 * comparadas con la libgcc, pero el kernel solo las usa en el AML.
 *
 * También las de contar bits (__ffsdi2, __clzdi2, __ctzdi2, __popcountdi2),
 * por si el GCC no expande en línea los __builtin_*ll de uACPI. */
#include "tipos.h"

/* Las declaraciones con el nombre que espera el GCC (el prototipo evita el
 * aviso de -Wmissing-prototypes si algún día se activa). */
uint64_t __udivmoddi4(uint64_t n, uint64_t d, uint64_t *resto);
int64_t  __divmoddi4(int64_t n, int64_t d, int64_t *resto);
uint64_t __udivdi3(uint64_t n, uint64_t d);
uint64_t __umoddi3(uint64_t n, uint64_t d);
int64_t  __divdi3(int64_t n, int64_t d);
int64_t  __moddi3(int64_t n, int64_t d);
int      __ffsdi2(uint64_t v);
int      __clzdi2(uint64_t v);
int      __ctzdi2(uint64_t v);
int      __popcountdi2(uint64_t v);

/* La división sin signo de verdad. Con divisor cero devuelve todo unos y el
 * dividendo como resto, en vez de fallar: quien llama (uACPI) ya comprueba el
 * cero antes de dividir, pero el kernel no debe caerse por eso. */
uint64_t __udivmoddi4(uint64_t n, uint64_t d, uint64_t *resto)
{
    if (d == 0) {
        if (resto) *resto = n;
        return ~(uint64_t)0;
    }
    /* Caso rápido: los dos caben en 32 bits (la instrucción div de 32 bits). */
    if ((n >> 32) == 0 && (d >> 32) == 0) {
        uint32_t q = (uint32_t)n / (uint32_t)d;
        if (resto) *resto = (uint32_t)n - q * (uint32_t)d;
        return q;
    }
    uint64_t q = 0, r = 0;
    for (int i = 63; i >= 0; i--) {
        r = (r << 1) | ((n >> i) & 1);
        if (r >= d) {
            r -= d;
            q |= (uint64_t)1 << i;
        }
    }
    if (resto) *resto = r;
    return q;
}

int64_t __divmoddi4(int64_t n, int64_t d, int64_t *resto)
{
    bool neg_n = n < 0, neg_d = d < 0;
    uint64_t un = neg_n ? (uint64_t)0 - (uint64_t)n : (uint64_t)n;
    uint64_t ud = neg_d ? (uint64_t)0 - (uint64_t)d : (uint64_t)d;
    uint64_t r;
    uint64_t q = __udivmoddi4(un, ud, &r);
    /* Como en C: el cociente trunca hacia cero y el resto tiene el signo del dividendo. */
    if (resto) *resto = neg_n ? -(int64_t)r : (int64_t)r;
    return (neg_n != neg_d) ? -(int64_t)q : (int64_t)q;
}

uint64_t __udivdi3(uint64_t n, uint64_t d) { return __udivmoddi4(n, d, NULL); }

uint64_t __umoddi3(uint64_t n, uint64_t d)
{
    uint64_t r;
    __udivmoddi4(n, d, &r);
    return r;
}

int64_t __divdi3(int64_t n, int64_t d) { return __divmoddi4(n, d, NULL); }

int64_t __moddi3(int64_t n, int64_t d)
{
    int64_t r;
    __divmoddi4(n, d, &r);
    return r;
}

/* Bits: posición del primero a 1 contando desde 1 (0 si no hay), ceros por
 * arriba y por abajo (indefinido con 0: aquí devuelven 64) y cuántos hay. */
int __ffsdi2(uint64_t v)
{
    for (int i = 0; i < 64; i++)
        if ((v >> i) & 1) return i + 1;
    return 0;
}

int __clzdi2(uint64_t v)
{
    for (int i = 63; i >= 0; i--)
        if ((v >> i) & 1) return 63 - i;
    return 64;
}

int __ctzdi2(uint64_t v)
{
    for (int i = 0; i < 64; i++)
        if ((v >> i) & 1) return i;
    return 64;
}

int __popcountdi2(uint64_t v)
{
    int n = 0;
    while (v) { n += (int)(v & 1); v >>= 1; }
    return n;
}
