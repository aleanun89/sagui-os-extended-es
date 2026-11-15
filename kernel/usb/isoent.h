// SPDX-License-Identifier: GPL-2.0-or-later
/* isoent.h - cola de paquetes recibidos de un flujo isócrono de entrada (D277).
 *
 * Un flujo de entrada reparte su anillo de datos en "casillas" de slot_tam bytes (una por
 * transacción isócrona: un microcuadro de alta velocidad o un cuadro de plena velocidad).
 * Cada paquete que el controlador recibe cae en la siguiente casilla; la longitud que
 * llegó de verdad se anota aquí, en el orden de los paquetes. El consumidor (el driver
 * UVC) lee en orden y, al leer, libera la casilla para que el controlador la reutilice.
 * Es C puro, sin dependencias del kernel: lo prueban las pruebas del anfitrión. */
#ifndef ISOENT_H
#define ISOENT_H

#ifdef UVC_ANFITRION
#include <stdint.h>
#include <stdbool.h>
#else
#include "tipos.h"
#endif

#define ISOENT_CASILLAS_MAX 512

typedef struct {
    uint32_t ncasillas, slot_tam;
    uint32_t postados;      /* paquetes puestos en el controlador (siguiente casilla = postados % ncasillas) */
    uint32_t recibidos;     /* paquetes que el controlador ya completó (en orden) */
    uint32_t lidos;         /* paquetes que el consumidor ya leyó */
    uint32_t perdidos;      /* paquetes con error o que el controlador no llegó a ejecutar */
    uint16_t len[ISOENT_CASILLAS_MAX];
    uint8_t  err[ISOENT_CASILLAS_MAX];
} isoent_t;

static inline bool isoent_iniciar(isoent_t *r, uint32_t anel_tam, uint32_t slot_tam)
{
    if (!slot_tam || anel_tam % slot_tam) return false;
    uint32_t n = anel_tam / slot_tam;
    if (n < 2 || n > ISOENT_CASILLAS_MAX) return false;
    r->ncasillas = n;
    r->slot_tam = slot_tam;
    r->postados = r->recibidos = r->lidos = r->perdidos = 0;
    return true;
}

/* Casillas libres para poner un paquete nuevo (las ya puestas y aún no leídas están ocupadas). */
static inline uint32_t isoent_livres(const isoent_t *r) { return r->ncasillas - (r->postados - r->lidos); }

/* Pone un paquete: devuelve el desplazamiento de su casilla en el anillo. */
static inline uint32_t isoent_postar(isoent_t *r) { return (r->postados++ % r->ncasillas) * r->slot_tam; }

/* El controlador completó el paquete más antiguo sin completar. */
static inline void isoent_completar(isoent_t *r, uint32_t len, bool erro)
{
    uint32_t c = r->recibidos % r->ncasillas;
    if (len > r->slot_tam) len = r->slot_tam;
    r->len[c] = (uint16_t)len;
    r->err[c] = erro ? 1 : 0;
    if (erro) r->perdidos++;
    r->recibidos++;
}

/* Siguiente paquete completado sin leer: desplazamiento, longitud y si trae error. */
static inline bool isoent_ler(isoent_t *r, uint32_t *off, uint32_t *len, bool *erro)
{
    if (r->lidos == r->recibidos) return false;
    uint32_t c = r->lidos % r->ncasillas;
    *off = c * r->slot_tam;
    *len = r->len[c];
    *erro = r->err[c] != 0;
    r->lidos++;
    return true;
}

/* Vuelta al principio (usb_iso_ligar): lo que quedaba pendiente se descarta. */
static inline void isoent_reiniciar(isoent_t *r) { r->postados = r->recibidos = r->lidos = r->perdidos = 0; }

#endif
