// SPDX-License-Identifier: GPL-3.0-only
/* virtio_pci.c - transporte VIRTIO 1.x moderno sobre PCI, colas "split" sondeadas (D472).
 *
 * Referencia: especificación VIRTIO 1.x, secciones 4.1 (PCI) y 2.7 (colas "split"). Solo el
 * transporte moderno (capacidades PCI de tipo 1 a 4): los dispositivos «transitional» con BAR de E/S
 * antigua no se tratan. Sin interrupciones: las respuestas se sondean en la cola usada, con plazo.
 * El kernel ve la RAM baja 1:1, así que la dirección física de una página es su puntero. */
#include "virtio_pci.h"
#include "acel2d.h"      /* acel_prazo: plazos con el TSC */
#include "memoria.h"
#include "paginacao.h"
#include "pci_realoc.h"
#include "texto.h"

/* Tipos de capacidad (cfg_type). */
#define CAP_COMUM  1
#define CAP_NOTIF  2
#define CAP_ISR    3
#define CAP_DISP   4

/* Desplazamientos de virtio_pci_common_cfg. */
#define C_DF_SEL   0
#define C_DF       4
#define C_GF_SEL   8
#define C_GF       12
#define C_NFILAS   18
#define C_ESTADO   20
#define C_FILA_SEL 22
#define C_FILA_TAM 24
#define C_FILA_MSIX 26
#define C_FILA_ON  28
#define C_FILA_OFF 30
#define C_FILA_DESC 32
#define C_FILA_DRV  40
#define C_FILA_DEV  48

static inline void barreira(void) { __asm__ volatile("" ::: "memory"); }
static inline uint8_t  r8(volatile uint8_t *b, uint32_t o)  { return *(volatile uint8_t *)(b + o); }
static inline uint16_t r16(volatile uint8_t *b, uint32_t o) { return *(volatile uint16_t *)(b + o); }
static inline uint32_t r32(volatile uint8_t *b, uint32_t o) { return *(volatile uint32_t *)(b + o); }
static inline void w8(volatile uint8_t *b, uint32_t o, uint8_t v)   { *(volatile uint8_t *)(b + o) = v; }
static inline void w16(volatile uint8_t *b, uint32_t o, uint16_t v) { *(volatile uint16_t *)(b + o) = v; }
static inline void w32(volatile uint8_t *b, uint32_t o, uint32_t v) { *(volatile uint32_t *)(b + o) = v; }

static void falla(char *motivo, int n, const char *txt) { if (motivo && n > 0) txt_copia_n(motivo, txt, (uint32_t)n); }

/* Dirección base (por debajo de 4 GB) de la BAR "b"; 0 si no es de memoria o no se puede bajar. */
static uint32_t base_bar(const pci_disp_t *p, int b)
{
    uint32_t v = pci_le32(p, (uint8_t)(0x10 + 4 * b));
    if (!v || (v & 1)) return 0;
    uint32_t alto = (v & 6) == 4 ? pci_le32(p, (uint8_t)(0x14 + 4 * b)) : 0;
    uint32_t base = v & ~0xFu;
    if (alto) {   /* D283: el kernel es de 32 bits: la BAR que el firmware dejó arriba se baja */
        uint32_t nueva = pci_realoc_bar_baixo(p, b, "virtio");
        if (!nueva) return 0;
        base = nueva;
    }
    return base;
}

bool virtio_pci_abrir(virtio_dev_t *d, const pci_disp_t *p, char *motivo, int n)
{
    memset(d, 0, sizeof *d);
    d->pci = *p;
    if (!(pci_le32(p, 0x04) & 0x00100000u)) { falla(motivo, n, "sin lista de capacidades PCI"); return false; }
    uint32_t base_cmd = pci_le32(p, 0x04) & 0xFFFFu;
    /* Memoria y maestro del bus; sin INTx (se sondea). Antes de mapear: la recolocación de una BAR alta lo necesita. */
    pci_escreve32(p, 0x04, base_cmd | 0x6 | (1u << 10));
    uint32_t cap = pci_le32(p, 0x34) & 0xFC;
    for (int vuelta = 0; cap && vuelta < 48; vuelta++) {
        uint32_t h = pci_le32(p, (uint8_t)cap);
        uint8_t id = (uint8_t)h, sig = (uint8_t)(h >> 8), tipo = (uint8_t)(h >> 24);
        if (id == 0x09 && tipo >= CAP_COMUM && tipo <= CAP_DISP) {
            uint32_t bar = pci_le32(p, (uint8_t)(cap + 4)) & 0xFF;
            uint32_t desl = pci_le32(p, (uint8_t)(cap + 8));
            uint32_t len = pci_le32(p, (uint8_t)(cap + 12));
            volatile uint8_t **dest = tipo == CAP_COMUM ? &d->comum : tipo == CAP_NOTIF ? &d->notif :
                                      tipo == CAP_ISR ? &d->isr : &d->disp;
            if (!*dest && bar < 6 && len) {
                uint32_t base = base_bar(p, (int)bar);
                if (base) {
                    uint32_t v = janela_mapear_tipo(base + desl, len < 0x1000 ? 0x1000 : len, PG_PCD);
                    if (v) *dest = (volatile uint8_t *)v;
                }
                if (tipo == CAP_NOTIF) d->notif_mult = pci_le32(p, (uint8_t)(cap + 16));
            }
        }
        cap = sig & 0xFC;
    }
    if (!d->comum || !d->notif) { falla(motivo, n, "faltan las capacidades comun o de notificacion (no es virtio moderno)"); return false; }
    return true;
}

uint8_t virtio_pci_estado(virtio_dev_t *d) { return r8(d->comum, C_ESTADO); }

void virtio_pci_reiniciar(virtio_dev_t *d)
{
    w8(d->comum, C_ESTADO, 0);
    acel_prazo_t pz;
    acel_prazo(&pz, 100);
    while (r8(d->comum, C_ESTADO) != 0 && !acel_prazo_esgotado(&pz)) { }
}

bool virtio_pci_negociar(virtio_dev_t *d, uint64_t quiere, char *motivo, int n)
{
    virtio_pci_reiniciar(d);
    w8(d->comum, C_ESTADO, VIRTIO_ST_ACK);
    w8(d->comum, C_ESTADO, VIRTIO_ST_ACK | VIRTIO_ST_DRIVER);
    w32(d->comum, C_DF_SEL, 0);
    uint32_t bajas = r32(d->comum, C_DF);
    w32(d->comum, C_DF_SEL, 1);
    uint32_t altas = r32(d->comum, C_DF);
    d->ofrecidas = ((uint64_t)altas << 32) | bajas;
    if (!(d->ofrecidas & VIRTIO_F_VERSION_1)) { falla(motivo, n, "el dispositivo no ofrece VIRTIO_F_VERSION_1"); return false; }
    d->aceptadas = VIRTIO_F_VERSION_1 | (quiere & d->ofrecidas);
    w32(d->comum, C_GF_SEL, 0);
    w32(d->comum, C_GF, (uint32_t)d->aceptadas);
    w32(d->comum, C_GF_SEL, 1);
    w32(d->comum, C_GF, (uint32_t)(d->aceptadas >> 32));
    w8(d->comum, C_ESTADO, VIRTIO_ST_ACK | VIRTIO_ST_DRIVER | VIRTIO_ST_FEATURES_OK);
    if (!(r8(d->comum, C_ESTADO) & VIRTIO_ST_FEATURES_OK)) { falla(motivo, n, "el dispositivo rechazo las caracteristicas"); return false; }
    d->num_filas = r16(d->comum, C_NFILAS);
    return true;
}

bool virtio_pci_fila(virtio_dev_t *d, uint16_t indice, virtio_fila_t *q, char *motivo, int n)
{
    memset(q, 0, sizeof *q);
    if (indice >= d->num_filas) { falla(motivo, n, "la cola no existe"); return false; }
    w16(d->comum, C_FILA_SEL, indice);
    uint16_t tam = r16(d->comum, C_FILA_TAM);
    if (!tam) { falla(motivo, n, "cola de tamaño 0"); return false; }
    if (tam > VIRTIO_FILA_MAX) tam = VIRTIO_FILA_MAX;
    w16(d->comum, C_FILA_TAM, tam);
    /* desc (16 B por entrada), avail (6 + 2 por entrada) y used (6 + 8 por entrada) en dos páginas. */
    uint32_t pg = memfisica_alocar_seguidas(2);
    if (!pg) { falla(motivo, n, "sin memoria para la cola"); return false; }
    memset((void *)pg, 0, 2 * PAGINA);
    q->mem = (uint8_t *)pg;
    q->desc = q->mem;
    q->avail = q->mem + 16u * VIRTIO_FILA_MAX;                       /* 1024 */
    q->used = q->mem + PAGINA;                                      /* alineada a 4 */
    q->indice = indice; q->tam = tam;
    w32(d->comum, C_FILA_DESC, pg);            w32(d->comum, C_FILA_DESC + 4, 0);
    w32(d->comum, C_FILA_DRV, (uint32_t)(q->avail - 0));  w32(d->comum, C_FILA_DRV + 4, 0);
    w32(d->comum, C_FILA_DEV, (uint32_t)(q->used - 0));   w32(d->comum, C_FILA_DEV + 4, 0);
    w16(d->comum, C_FILA_MSIX, 0xFFFF);        /* sin MSI-X */
    q->notificar = d->notif + (uint32_t)r16(d->comum, C_FILA_OFF) * d->notif_mult;
    w16(d->comum, C_FILA_ON, 1);
    return true;
}

void virtio_pci_listo(virtio_dev_t *d)
{
    w8(d->comum, C_ESTADO, r8(d->comum, C_ESTADO) | VIRTIO_ST_DRIVER_OK);
}

bool virtio_fila_poner(virtio_fila_t *q, const virtio_buf_t *b, int n)
{
    if (n <= 0 || q->proximo + n > q->tam || (uint16_t)(q->avail_idx - q->ultimo_usado) >= q->tam) return false;
    uint16_t cabeza = q->proximo;
    for (int i = 0; i < n; i++) {
        volatile uint8_t *e = q->desc + 16u * (uint32_t)(cabeza + i);
        *(volatile uint32_t *)(e + 0) = b[i].fis;
        *(volatile uint32_t *)(e + 4) = 0;
        *(volatile uint32_t *)(e + 8) = b[i].len;
        *(volatile uint16_t *)(e + 12) = (uint16_t)((b[i].escrita ? 2 : 0) | (i + 1 < n ? 1 : 0));   /* WRITE | NEXT */
        *(volatile uint16_t *)(e + 14) = (uint16_t)(cabeza + i + 1);
    }
    q->proximo = (uint16_t)(q->proximo + n);
    uint16_t pos = q->avail_idx % q->tam;
    *(volatile uint16_t *)(q->avail + 4 + 2u * pos) = cabeza;
    barreira();
    q->avail_idx++;
    *(volatile uint16_t *)(q->avail + 2) = q->avail_idx;
    q->pendientes++;
    return true;
}

void virtio_fila_avisar(virtio_dev_t *d, virtio_fila_t *q)
{
    (void)d;
    barreira();
    *(volatile uint16_t *)q->notificar = q->indice;
}

bool virtio_fila_esperar(virtio_fila_t *q, uint32_t ms)
{
    acel_prazo_t pz;
    acel_prazo(&pz, ms);
    while (q->pendientes) {
        barreira();
        uint16_t usado = *(volatile uint16_t *)(q->used + 2);
        while (q->ultimo_usado != usado) { q->ultimo_usado++; q->pendientes--; }
        if (!q->pendientes) break;
        if (acel_prazo_esgotado(&pz)) return false;
    }
    q->proximo = 0;   /* todo devuelto: los descriptores vuelven a estar libres */
    return true;
}
