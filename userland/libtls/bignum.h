// SPDX-License-Identifier: GPL-2.0-or-later
/* bignum.h - inteiros grandes da libtls (interno: RSA e curvas). D060.
 *
 * Números em "membros" de 32 bits, do menos significativo para o mais.
 * A multiplicação é a de Montgomery (CIOS): a*b*R^-1 mod m, R = 2^(32n),
 * sem divisão. Serve para o RSA (até 4096 bits) e para P-256/P-384. */
#ifndef SAGUI_BIGNUM_H
#define SAGUI_BIGNUM_H
#include "cripto.h"

#define BN_MAX 130   /* 4096 bits + folga */

typedef struct {
    uint32_t m[BN_MAX];    /* módulo (ímpar) */
    uint32_t r2[BN_MAX];   /* R^2 mod m: converte para a forma de Montgomery */
    uint32_t um[BN_MAX];   /* R mod m: o "1" na forma de Montgomery */
    uint32_t m0inv;        /* -m^-1 mod 2^32 */
    int      n;            /* membros */
} mont_t;

void bn_de_bytes(uint32_t *r, int n, const uint8_t *b, unsigned nb);   /* big-endian -> membros (nb <= 4n) */
void bn_para_bytes(uint8_t *b, unsigned nb, const uint32_t *a, int n);
int  bn_comparar(const uint32_t *a, const uint32_t *b, int n);         /* -1, 0, 1 (tempo variável) */
bool bn_zero(const uint32_t *a, int n);
uint32_t bn_somar(uint32_t *r, const uint32_t *a, const uint32_t *b, int n);   /* devolve o vai-um */
uint32_t bn_subtrair(uint32_t *r, const uint32_t *a, const uint32_t *b, int n); /* devolve o empresta-um */

bool mont_preparar(mont_t *c, const uint8_t *modulo, unsigned nb);
void mont_mul(const mont_t *c, uint32_t *r, const uint32_t *a, const uint32_t *b);   /* r pode ser a ou b */
void mont_para(const mont_t *c, uint32_t *r, const uint32_t *a);    /* a < m -> forma de Montgomery */
void mont_de(const mont_t *c, uint32_t *r, const uint32_t *a);      /* volta à forma normal */
/* r = a^e (tudo na forma de Montgomery; e em bytes big-endian, público) */
void mont_potencia(const mont_t *c, uint32_t *r, const uint32_t *a, const uint8_t *e, unsigned ne);
void mod_somar(const mont_t *c, uint32_t *r, const uint32_t *a, const uint32_t *b);   /* (a + b) mod m, tempo constante */
void mod_subtrair(const mont_t *c, uint32_t *r, const uint32_t *a, const uint32_t *b);

#endif
