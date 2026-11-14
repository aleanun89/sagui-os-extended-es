// SPDX-License-Identifier: GPL-2.0-or-later
/* p256.h - la curva elíptica NIST P-256 (FIPS 186-4, D.1.2.3; SEC 1) para el
 * SAE de WPA3 (D220, v7.3.2).
 *
 * Mismo diseño que userland/libtls/ecc.c (D060): aritmética de Montgomery con
 * miembros de 32 bits y coordenadas jacobianas en la forma de Montgomery, solo
 * con enteros (el kernel no usa la FPU). Es un archivo aparte, de tamaño fijo
 * (8 miembros), porque el de la libtls es de la libc de los programas, lleva
 * números de hasta 4096 bits en la pila (la del kernel es de 8 KB) y su
 * interfaz no tiene las operaciones que pide el SAE: sumar puntos
 * cualesquiera, multiplicar un punto que no es el generador, la raíz cuadrada
 * y el mapa SSWU del hash-to-element.
 *
 * Se compila en el kernel, en el programa «wifi» y en el anfitrión. */
#ifndef SAGUI_P256_H
#define SAGUI_P256_H
#ifdef SAGUI_KERNEL
#include "tipos.h"
#else
#include <stdint.h>
#include <stdbool.h>
#endif

#define P256_TAM 32

/* Punto en coordenadas jacobianas (X/Z^2, Y/Z^3), forma de Montgomery; Z = 0 es el infinito. */
typedef struct { uint32_t x[8], y[8], z[8]; } p256_ponto_t;

/* x || y (64 bytes, big-endian) -> punto; false si no está en la curva. */
bool p256_ler(p256_ponto_t *p, const uint8_t xy[2 * P256_TAM]);
/* punto -> x || y; false si es el infinito. */
bool p256_gravar(const p256_ponto_t *p, uint8_t xy[2 * P256_TAM]);
void p256_gerador(p256_ponto_t *p);
bool p256_infinito(const p256_ponto_t *p);
void p256_somar(p256_ponto_t *r, const p256_ponto_t *a, const p256_ponto_t *b);
void p256_negar(p256_ponto_t *r, const p256_ponto_t *a);
/* r = k·a (k de 32 bytes big-endian, cualquier valor). Escalera de Montgomery. */
void p256_mult(p256_ponto_t *r, const p256_ponto_t *a, const uint8_t k[P256_TAM]);

/* Escalares módulo el orden n (32 bytes big-endian). */
bool p256_escalar_valido(const uint8_t k[P256_TAM]);          /* 1 < k < n */
void p256_somar_n(uint8_t r[P256_TAM], const uint8_t a[P256_TAM], const uint8_t b[P256_TAM]);   /* a, b < n */
void p256_mod_n1_mais1(uint8_t r[P256_TAM], const uint8_t v[P256_TAM]);   /* (v mod (n - 1)) + 1 */
bool p256_menor_que_p(const uint8_t x[P256_TAM]);

/* SAE: el punto de abscisa x cuya ordenada tiene la paridad dada; false si
 * x >= p o x^3 - 3x + b no es un cuadrado (cazar y picotear, 12.4.4.2.2). */
bool p256_de_x(p256_ponto_t *p, const uint8_t x[P256_TAM], int paridade_y);
/* SAE hash-to-element: u = valor (48 bytes) mod p y el mapa SSWU con z = -10
 * (IEEE 802.11-2020, 12.4.4.2.3; RFC 9380, 6.6.2). */
void p256_sswu(p256_ponto_t *p, const uint8_t *valor, uint32_t nvalor);

#endif
