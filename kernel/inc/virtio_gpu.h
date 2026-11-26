// SPDX-License-Identifier: GPL-3.0-only
/* virtio_gpu.h - driver virtio-gpu en modo 2D (D472): pantalla de QEMU/Proxmox (-device virtio-gpu-pci, -vga virtio).
 *
 * video.c lo usa como una tarjeta más: el framebuffer es una zona de RAM del invitado que se asocia al
 * recurso 2D de la GPU virtual; después de copiar lo que cambió en ella, se pide TRANSFER_TO_HOST_2D +
 * RESOURCE_FLUSH solo de esos rectángulos. El 3D (VirGL, D473) va en virgl.c por la misma cola de
 * control; aquí solo se negocia VIRTIO_GPU_F_VIRGL y se presta la cola. */
#ifndef SAGUI_VIRTIO_GPU_H
#define SAGUI_VIRTIO_GPU_H
#include "tipos.h"

#define VG_MAX_L 2560u   /* modo más grande que se acepta (memoria del respaldo: 16 MB como mucho) */
#define VG_MAX_A 1600u

/* ¿Hay una virtio-gpu que deba hacerse cargo de la pantalla? Con "vbe_activo" (la etapa 2 ya puso un modo
 * por la BIOS) solo si esa tarjeta es la propia virtio-vga; sin VBE, cualquier virtio-gpu (-vga none o -device). */
bool vg_candidato(bool vbe_activo);
/* Inicia el dispositivo y el primer modo: "pref_l" x "pref_a" (0 = el que dé la BIOS o 1024x768). Devuelve el
 * framebuffer (puntero = dirección física), su pitch y el modo, o false (el dispositivo queda reiniciado y todo
 * sigue como estaba: VBE o texto). */
typedef struct { uint8_t *fb; uint32_t fis, pitch, largura, altura; } vg_modo_t;
bool vg_iniciar(uint32_t pref_l, uint32_t pref_a, vg_modo_t *m);
bool vg_ativo(void);
/* Cambia el modo: respaldo y recurso nuevos; el contenido se pierde. false = todo sigue como estaba. */
bool vg_mudar(uint32_t l, uint32_t a, vg_modo_t *m);
/* La CPU escribió en (x, y, l, a): se apunta para la próxima vg_descargar(). */
void vg_atualizar(int x, int y, int l, int a);
/* Lleva a la pantalla lo apuntado (TRANSFER_TO_HOST_2D + RESOURCE_FLUSH por cada rectángulo, en una sola tanda). */
void vg_descargar(void);

/* D473 (3D, virgl.c): VIRTIO_GPU_F_VIRGL ofrecido por el dispositivo y negociado; cuántos «capsets» anuncia; y
 * una orden con la memoria del que llama (RAM baja) y su respuesta. Devuelve el tipo de respuesta (0: ninguna). */
bool     vg_virgl_ofrecido(void);
bool     vg_virgl_aceptado(void);
uint32_t vg_num_capsets(void);
uint32_t vg_orden_dma(const void *cmd, uint32_t n, void *resp, uint32_t rn, uint32_t ms);
/* D474: hay una tanda de la presentación 2D en marcha (vg_orden_dma devolvería 0 sin esperar). */
bool     vg_ocupado(void);

/* Para infopc y la prueba. */
uint32_t vg_enviados(void);        /* rectángulos presentados desde el arranque */
uint32_t vg_fallos(void);

#endif
