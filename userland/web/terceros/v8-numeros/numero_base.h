// SPDX-License-Identifier: BSD-3-Clause
/* numero_base.h - Number.prototype.toString(base) del JavaScript.
 *
 * CÓDIGO DE TERCEROS (D262). Es el algoritmo DoubleToRadixCString del V8
 * (Copyright 2006-2011, the V8 project authors; licencia BSD de 3 cláusulas, texto en
 * LICENSE, en esta carpeta), reproducido de memoria en C por una IA (primero para el
 * Cipó de robson-apk, D141), por lo que se trata como derivado del V8. Ver ORIGEM.md.
 *
 * La idea: la parte fraccionaria se escribe dígito a dígito solo hasta donde el double
 * distingue el número del vecino más próximo ("delta" es la mitad de esa distancia),
 * redondeando el último dígito; la parte entera, de abajo arriba, con ceros donde el
 * double ya no guarda los dígitos bajos.
 *
 * Cabecera con función "static inline" (el motor de JavaScript la incluye desde
 * js_nativo.h). v finito, base de 2 a 36; "buf" necesita 2200 bytes. Devuelve el tamaño
 * del texto, que queda al principio de "buf" (sin cero final). "piso" y "resto" son
 * Math.floor y el % de JavaScript (no hay libm).
 */
#ifndef TERCEIROS_V8_NUMERO_BASE_H
#define TERCEIROS_V8_NUMERO_BASE_H

typedef unsigned long long v8_u64;
typedef union { double d; v8_u64 b; } v8_dbits;
static inline v8_u64 v8_d_bits(double d) { v8_dbits x; x.d = d; return x.b; }
static inline double v8_d_de(v8_u64 b) { v8_dbits x; x.b = b; return x.d; }

static inline int v8_numero_em_base(double v, int b, char *buf, double (*piso)(double), double (*resto)(double, double))
{
    static const char cs[] = "0123456789abcdefghijklmnopqrstuvwxyz";
    int pos = 1100, ini = pos;
    int neg = v < 0; if (neg) v = -v;
    double inteiro = piso(v), fr = v - inteiro;
    double delta = 0.5 * (v8_d_de(v8_d_bits(v) + 1) - v);
    double minimo = v8_d_de(1);
    if (delta < minimo) delta = minimo;
    int fp = pos;
    if (fr >= delta) {
        buf[fp++] = '.';
        do {
            fr *= b; delta *= b;
            int dg = (int)fr;
            buf[fp++] = cs[dg];
            fr -= dg;
            if (fr > 0.5 || (fr == 0.5 && (dg & 1))) {
                if (fr + delta > 1) {
                    /* arredonda para cima e propaga */
                    for (;;) {
                        fp--;
                        if (fp == pos) { inteiro += 1; break; }
                        char c = buf[fp];
                        int d = c > '9' ? c - 'a' + 10 : c - '0';
                        if (d + 1 < b) { buf[fp++] = cs[d + 1]; break; }
                    }
                    break;
                }
            }
        } while (fr >= delta && fp < 2190);
    }
    /* parte inteira: dígitos de baixo para cima, exatos enquanto o expoente permite */
    while (((v8_d_bits(inteiro / b) >> 52) & 0x7FF) > 1075) { inteiro /= b; buf[--ini] = '0'; }
    do {
        double r = resto(inteiro, b);
        buf[--ini] = cs[(int)r];
        inteiro = (inteiro - r) / b;
    } while (inteiro > 0 && ini > 1);
    if (neg) buf[--ini] = '-';
    /* o texto vai para o começo do buffer */
    int n = fp - ini;
    for (int i = 0; i < n; i++) buf[i] = buf[ini + i];
    return n;
}

#endif
