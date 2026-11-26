// SPDX-License-Identifier: GPL-3.0-only
/* virtio_pci.h - transporte VIRTIO 1.x moderno sobre PCI (D472).
 *
 * Pequeño y sin dependencias del tipo de dispositivo: encuentra las capacidades (comun, notificación,
 * ISR, dispositivo), negocia las características (siempre VIRTIO_F_VERSION_1), prepara colas "split"
 * y envía cadenas de descriptores sondeando la cola usada (sin interrupciones). Lo usa virtio_gpu.c
 * y lo podrán usar otros dispositivos virtio modernos (red, bloque...). */
#ifndef SAGUI_VIRTIO_PCI_H
#define SAGUI_VIRTIO_PCI_H
#include "tipos.h"
#include "pci.h"

#define VIRTIO_FABRICANTE 0x1AF4

/* Estado del dispositivo (device_status). */
#define VIRTIO_ST_ACK         1
#define VIRTIO_ST_DRIVER      2
#define VIRTIO_ST_DRIVER_OK   4
#define VIRTIO_ST_FEATURES_OK 8
#define VIRTIO_ST_FALLO       0x80

#define VIRTIO_F_VERSION_1    (1ull << 32)

typedef struct {
    pci_disp_t        pci;
    volatile uint8_t *comum;      /* virtio_pci_common_cfg */
    volatile uint8_t *notif;      /* base de las notificaciones */
    volatile uint8_t *isr;
    volatile uint8_t *disp;       /* configuración propia del dispositivo (NULL si no hay) */
    uint32_t          notif_mult;
    uint64_t          ofrecidas;  /* características que ofrece el dispositivo */
    uint64_t          aceptadas;  /* las que se aceptaron */
    uint16_t          num_filas;
} virtio_dev_t;

#define VIRTIO_FILA_MAX 64        /* tamaño máximo de cola que se usa */

typedef struct {
    uint16_t          indice;
    uint16_t          tam;
    uint16_t          proximo;    /* siguiente descriptor libre de la tanda */
    uint16_t          ultimo_usado;
    uint16_t          avail_idx;
    uint16_t          pendientes; /* cadenas enviadas y aún sin recoger */
    uint8_t          *mem;        /* página(s) de la cola (física = puntero, memoria baja) */
    volatile uint8_t *desc, *avail, *used;
    volatile uint8_t *notificar;  /* dirección de la notificación de esta cola */
} virtio_fila_t;

/* Un tramo de una cadena: dirección física, bytes y si lo escribe el dispositivo. */
typedef struct { uint32_t fis; uint32_t len; bool escrita; } virtio_buf_t;

/* Busca las capacidades, recoloca y mapea las BAR y activa memoria y maestro del bus. Escribe el motivo
 * del fallo en "motivo". */
bool virtio_pci_abrir(virtio_dev_t *d, const pci_disp_t *p, char *motivo, int n);
/* Reinicia, ACK + DRIVER, negocia: acepta VERSION_1 y las de "quiere" que el dispositivo ofrezca, y
 * deja FEATURES_OK. false = el dispositivo no es moderno o rechazó las características. */
bool virtio_pci_negociar(virtio_dev_t *d, uint64_t quiere, char *motivo, int n);
/* Prepara la cola "indice" (como mucho VIRTIO_FILA_MAX entradas) y la habilita. */
bool virtio_pci_fila(virtio_dev_t *d, uint16_t indice, virtio_fila_t *q, char *motivo, int n);
void virtio_pci_listo(virtio_dev_t *d);          /* DRIVER_OK */
void virtio_pci_reiniciar(virtio_dev_t *d);      /* estado a 0: el dispositivo se olvida de todo */
uint8_t virtio_pci_estado(virtio_dev_t *d);

/* Pone una cadena de descriptores (en orden) en la cola, sin avisar. false si no caben. */
bool virtio_fila_poner(virtio_fila_t *q, const virtio_buf_t *b, int n);
/* Avisa al dispositivo de lo puesto. */
void virtio_fila_avisar(virtio_dev_t *d, virtio_fila_t *q);
/* Espera (con plazo en ms) a que vuelvan todas las cadenas pendientes; false = plazo vencido.
 * Con la cola vacía de pendientes, la tanda de descriptores se reinicia. */
bool virtio_fila_esperar(virtio_fila_t *q, uint32_t ms);

#endif
