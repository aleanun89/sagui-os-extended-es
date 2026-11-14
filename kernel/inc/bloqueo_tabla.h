// SPDX-License-Identifier: GPL-2.0-or-later
/* bloqueo_tabla.h - el formato de la tabla del bloqueo de publicidad por DNS
 * y la búsqueda en ella (D218). Lo comparten el kernel (kernel/rede/bloqueo.c),
 * el programa «bloqueo», que la compila desde las listas en formato hosts, y
 * la prueba en el PC (tests/h218_bloqueo_dns.py).
 *
 * La tabla no guarda los nombres sino un hash de 64 bits (FNV-1a) de cada uno,
 * en dos vectores ordenados: los bloqueados (las listas y los puestos a mano) y
 * los permitidos. Con 100 000 dominios son 800 KB y una consulta es una
 * búsqueda binaria (17 comparaciones) por cada sufijo del nombre.
 *
 * Archivo /home/.config/bloqueo/tabla.bin (lo que el kernel carga):
 *   cabecera de 32 bytes (bloqueo_cabecera_t) y, detrás, n_bloq + n_perm
 *   números de 64 bits (little-endian), cada vector en orden estrictamente
 *   creciente. El kernel lo comprueba todo antes de usarlo.
 *
 * Quien lo incluya ya debe tener uint32_t, uint64_t y bool definidos. */
#ifndef SAGUI_BLOQUEO_TABLA_H
#define SAGUI_BLOQUEO_TABLA_H

#define BLOQ_DIR        "/home/.config/bloqueo"
#define BLOQ_CONF       "/home/.config/bloqueo/bloqueo.conf"
#define BLOQ_TABLA      "/home/.config/bloqueo/tabla.bin"
#define BLOQ_MAGIA      "SGBLOQ01"
#define BLOQ_MAX_ENTRADAS 500000u   /* 4 MB: de sobra para las listas habituales (~100 000) */
#define BLOQ_NOME_MAX   253         /* un nombre DNS completo */

typedef struct {
    char     magia[8];       /* BLOQ_MAGIA, sin el cero final */
    uint32_t versao;         /* 1 */
    uint32_t n_bloq;         /* hashes bloqueados */
    uint32_t n_perm;         /* hashes permitidos */
    uint32_t reservado[3];
} bloqueo_cabecera_t;

/* Resultado de la búsqueda (también lo devuelve BLOQ_CONSULTAR). */
#define BLOQ_LIBRE      0    /* no está en ninguna lista */
#define BLOQ_BLOQUEADO  1
#define BLOQ_PERMITIDO  2    /* está (él o un dominio padre) en los permitidos */

/* FNV-1a de 64 bits de los n primeros caracteres, pasados a minúsculas. */
static inline uint64_t bloqueo_hash(const char *s, uint32_t n)
{
    uint64_t h = 0xcbf29ce484222325ull;
    for (uint32_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c >= 'A' && c <= 'Z') c = (unsigned char)(c + 32);
        h ^= c;
        h *= 0x100000001b3ull;
    }
    return h;
}

static inline bool bloqueo_tem(const uint64_t *v, uint32_t n, uint64_t h)
{
    uint32_t a = 0, b = n;
    while (a < b) {
        uint32_t m = a + (b - a) / 2;
        if (v[m] == h) return true;
        if (v[m] < h) a = m + 1; else b = m;
    }
    return false;
}

/* Decide sobre un nombre ya validado (sin el punto final). Se miran sus
 * sufijos del más largo al más corto (ads.ejemplo.com, ejemplo.com, com):
 * gana el más concreto que esté en alguna lista y, en el mismo sufijo, el
 * permitido gana al bloqueado. Sin «subdominios», un bloqueado solo vale para
 * el nombre exacto (los permitidos siempre valen para sus subdominios). */
static inline int bloqueo_decidir(const char *nome, uint32_t n,
                                  const uint64_t *bloq, uint32_t n_bloq,
                                  const uint64_t *perm, uint32_t n_perm, bool subdominios)
{
    uint32_t i = 0;
    while (i < n) {
        uint64_t h = bloqueo_hash(nome + i, n - i);
        if (n_perm && bloqueo_tem(perm, n_perm, h)) return BLOQ_PERMITIDO;
        if (n_bloq && (i == 0 || subdominios) && bloqueo_tem(bloq, n_bloq, h)) return BLOQ_BLOQUEADO;
        while (i < n && nome[i] != '.') i++;      /* el siguiente sufijo, tras el punto */
        i++;
    }
    return BLOQ_LIBRE;
}

/* Comprueba una tabla entera antes de usarla (el kernel no se fía del
 * archivo): cabecera, tamaño exacto y los dos vectores en orden estrictamente
 * creciente. 0 = se puede usar; BLOQ_MAL_FORMATO o BLOQ_MAL_LIMITE. */
#define BLOQ_MAL_FORMATO (-1)
#define BLOQ_MAL_LIMITE  (-2)
static inline int bloqueo_validar(const unsigned char *d, uint32_t tam)
{
    if (tam < sizeof(bloqueo_cabecera_t)) return BLOQ_MAL_FORMATO;
    const bloqueo_cabecera_t *c = (const bloqueo_cabecera_t *)d;
    for (int i = 0; i < 8; i++)
        if (c->magia[i] != BLOQ_MAGIA[i]) return BLOQ_MAL_FORMATO;
    if (c->versao != 1) return BLOQ_MAL_FORMATO;
    if (c->n_bloq > BLOQ_MAX_ENTRADAS || c->n_perm > BLOQ_MAX_ENTRADAS ||
        c->n_bloq + c->n_perm > BLOQ_MAX_ENTRADAS) return BLOQ_MAL_LIMITE;
    if (tam != sizeof *c + 8u * (c->n_bloq + c->n_perm)) return BLOQ_MAL_FORMATO;
    const uint64_t *v = (const uint64_t *)(d + sizeof *c);
    for (uint32_t i = 1; i < c->n_bloq; i++)
        if (v[i - 1] >= v[i]) return BLOQ_MAL_FORMATO;
    v += c->n_bloq;
    for (uint32_t i = 1; i < c->n_perm; i++)
        if (v[i - 1] >= v[i]) return BLOQ_MAL_FORMATO;
    return 0;
}

#endif
