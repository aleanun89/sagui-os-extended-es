// SPDX-License-Identifier: GPL-3.0-only
/* compressor.c - compresión de páginas para la swap comprimida (D352). Ver compressor.h.
 *
 * Formato de una secuencia:
 *   token        1 byte: 4 bits altos = literales, 4 bajos = longitud de la copia - 4
 *                (15 en cualquiera de los dos = sigue en bytes de extensión)
 *   [extensión]  si literales = 15: bytes que se suman mientras valgan 255
 *   literales    tal cual
 *   distancia    2 bytes (little endian), de 1 a 4095: cuánto atrás empieza la copia
 *   [extensión]  si longitud = 15: bytes que se suman mientras valgan 255
 * La última secuencia lleva solo literales: el trozo acaba al acabar sus bytes (la
 * longitud del trozo la guarda quien lo almacena). Las copias pueden solaparse con lo
 * que escriben (distancia < longitud: repeticiones), como en LZ4.
 *
 * El compresor busca coincidencias de 4 bytes con una tabla hash de 4096 posiciones
 * (la última vista de cada hash), sin cadenas: voraz y rápido. En datos que no se
 * repiten avanza cada vez más deprisa (un salto por cada 64 fallos), así que una
 * página aleatoria cuesta poco antes de darse por incomprimible. */
#include "compressor.h"
#include "texto.h"

#define HASH_BITS 12
#define MIN_COPIA 4u

static uint16_t tabela[1u << HASH_BITS];   /* posición + 1 (0 = vacía) */

static inline uint32_t ler32(const uint8_t *p)
{
    uint32_t v;
    memcpy(&v, p, 4);
    return v;
}

static inline uint32_t hash(uint32_t v) { return (v * 2654435761u) >> (32 - HASH_BITS); }

/* Bytes de extensión de una longitud (n ya sin los 15 del token). NULL si no cabe. */
static uint8_t *por_extensao(uint8_t *o, const uint8_t *fim, uint32_t n)
{
    while (n >= 255) {
        if (o >= fim) return NULL;
        *o++ = 255;
        n -= 255;
    }
    if (o >= fim) return NULL;
    *o++ = (uint8_t)n;
    return o;
}

/* Una secuencia: los literales [ancora, ancora+lit) y, si len > 0, la copia. */
static uint8_t *secuencia(uint8_t *o, const uint8_t *fim, const uint8_t *lits, uint32_t lit,
                          uint32_t dist, uint32_t len)
{
    if (o >= fim) return NULL;
    uint32_t ml = len ? len - MIN_COPIA : 0;
    uint8_t *token = o++;
    *token = (uint8_t)(((lit < 15 ? lit : 15) << 4) | (ml < 15 ? ml : 15));
    if (lit >= 15 && !(o = por_extensao(o, fim, lit - 15))) return NULL;
    if ((uint32_t)(fim - o) < lit) return NULL;
    memcpy(o, lits, lit);
    o += lit;
    if (!len) return o;
    if (fim - o < 2) return NULL;
    *o++ = (uint8_t)dist;
    *o++ = (uint8_t)(dist >> 8);
    if (ml >= 15 && !(o = por_extensao(o, fim, ml - 15))) return NULL;
    return o;
}

uint32_t comp_pagina(const uint8_t *e, uint8_t *s, uint32_t max)
{
    const uint32_t N = COMP_PAGINA;
    const uint8_t *fim = s + max;
    uint8_t *o = s;
    uint32_t i = 0, ancora = 0, falhas = 0;
    memset(tabela, 0, sizeof tabela);
    while (i + MIN_COPIA <= N) {
        uint32_t v = ler32(e + i), h = hash(v);
        uint32_t ref = tabela[h];
        tabela[h] = (uint16_t)(i + 1);
        if (ref && ler32(e + ref - 1) == v) {
            ref--;
            uint32_t len = MIN_COPIA;
            while (i + len < N && e[ref + len] == e[i + len]) len++;
            o = secuencia(o, fim, e + ancora, i - ancora, i - ref, len);
            if (!o) return 0;
            i += len;
            ancora = i;
            falhas = 0;
            /* La posición de justo antes también entra en la tabla: las repeticiones
             * largas encuentran así su continuación. */
            if (i >= 2 && i + 2 <= N) tabela[hash(ler32(e + i - 2))] = (uint16_t)(i - 1);
        } else {
            i += 1 + (falhas++ >> 6);
        }
    }
    if (ancora < N) {
        o = secuencia(o, fim, e + ancora, N - ancora, 0, 0);
        if (!o) return 0;
    }
    return (uint32_t)(o - s);
}

/* Lee una extensión de longitud; false si el trozo se acaba en medio. */
static bool ler_extensao(const uint8_t **e, const uint8_t *fim, uint32_t *n)
{
    uint8_t b;
    do {
        if (*e >= fim) return false;
        b = *(*e)++;
        *n += b;
        if (*n > COMP_PAGINA) return false;   /* nunca hace falta más: corrupto */
    } while (b == 255);
    return true;
}

bool descomp_pagina(const uint8_t *e, uint32_t n, uint8_t *s)
{
    const uint8_t *fim = e + n;
    uint32_t o = 0;
    while (e < fim) {
        uint8_t t = *e++;
        uint32_t lit = t >> 4;
        if (lit == 15 && !ler_extensao(&e, fim, &lit)) return false;
        if (lit > (uint32_t)(fim - e) || lit > COMP_PAGINA - o) return false;
        memcpy(s + o, e, lit);
        e += lit;
        o += lit;
        if (e == fim) break;   /* la última secuencia: solo literales */
        if (fim - e < 2) return false;
        uint32_t dist = (uint32_t)e[0] | (uint32_t)e[1] << 8;
        e += 2;
        if (dist == 0 || dist > o) return false;
        uint32_t len = t & 15;
        if (len == 15 && !ler_extensao(&e, fim, &len)) return false;
        len += MIN_COPIA;
        if (len > COMP_PAGINA - o) return false;
        const uint8_t *de = s + o - dist;
        uint8_t *para = s + o;
        if (dist >= len) memcpy(para, de, len);
        else for (uint32_t k = 0; k < len; k++) para[k] = de[k];   /* se solapa: byte a byte */
        o += len;
    }
    return o == COMP_PAGINA;
}

uint16_t comp_soma(const uint8_t *pagina)
{
    uint32_t a = 0, b = 0;
    for (uint32_t i = 0; i < COMP_PAGINA; i += 4) {
        a += ler32(pagina + i);
        b += a;
    }
    uint32_t r = a ^ (b * 31u) ^ (a >> 16) ^ (b >> 13);
    return (uint16_t)(r ^ (r >> 16));
}
