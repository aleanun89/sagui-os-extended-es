// SPDX-License-Identifier: GPL-3.0-only
/* virgl.h - 3D por VirGL sobre virtio-gpu (D473): codificador del flujo de órdenes de virglrenderer y autoprueba.
 * D474: las órdenes y el codificador se exportan para el backend de gpu3d (gpu3d_virgl.c). */
#ifndef SAGUI_VIRGL_H
#define SAGUI_VIRGL_H
#include "tipos.h"

/* ¿La GPU virtual ofrece VirGL y se negoció? */
bool virgl_disponible(void);
/* Lee los capsets, crea un contexto 3D, limpia un recurso a un color, dibuja un triángulo con color por vértice
 * (sombreadores TGSI en texto), trae el resultado a la RAM por TRANSFER_FROM_HOST_3D y lo comprueba píxel a píxel.
 * Deja en el log [VIRGL] cada paso. true = todo bien. Se llama una vez en el arranque (principal.c). */
bool virgl_autoprueba(void);
/* Resultado de la última autoprueba para infopc y las pruebas: 0 no hecha, 1 bien, -1 mal. */
int  virgl_estado(void);

/* ---- flujo de órdenes de virglrenderer (virgl_protocol.h): orden | objeto << 8 | palabras << 16 */
#define VC_CREAR_OBJETO     1
#define VC_ASOCIAR_OBJETO   2
#define VC_DESTRUIR_OBJETO  3
#define VC_SET_VIEWPORT     4
#define VC_SET_FRAMEBUFFER  5
#define VC_SET_VERTEX_BUF   6
#define VC_LIMPIAR          7
#define VC_DIBUJAR          8
#define VC_ESCRIBIR         9    /* RESOURCE_INLINE_WRITE */
#define VC_SAMPLER_VIEWS    10
#define VC_CONSTANTES       12   /* SET_CONSTANT_BUFFER */
#define VC_TIJERA           15   /* SET_SCISSOR_STATE */
#define VC_BLIT             16
#define VC_ASOCIAR_SAMPLERS 18
#define VC_ASOCIAR_SOMBR    31
#define VO_BLEND            1
#define VO_RASTERIZER       2
#define VO_DSA              3
#define VO_SHADER           4
#define VO_VERTEX_ELEMENTS  5
#define VO_SAMPLER_VIEW     6
#define VO_SAMPLER_STATE    7
#define VO_SURFACE          8
/* Formatos (los de pipe_format de Gallium) */
#define VFMT_B8G8R8A8       1    /* palabras 0xAARRGGBB en memoria */
#define VFMT_Z16            16
#define VFMT_Z32F           18
#define VFMT_Z24S8          19   /* Z24_UNORM_S8_UINT */
#define VFMT_S8Z24          20
#define VFMT_Z24X8          21
#define VFMT_R32G32_F       29
#define VFMT_R32G32B32A32_F 31
#define VFMT_R8G8B8A8       67
#define BIND_DEPTH_STENCIL  (1u << 0)
#define BIND_RENDER_TARGET  (1u << 1)
#define BIND_SAMPLER_VIEW   (1u << 3)
#define BIND_VERTEX_BUFFER  (1u << 4)
#define TGT_BUFFER          0
#define TGT_2D              2
#define FMT_BUFFER          64   /* R8_UNORM: los búferes se crean con un formato de un byte */
#define VSOMBRA_VERTICES    0
#define VSOMBRA_FRAGMENTOS  1

typedef struct { uint32_t *p; uint32_t n, max; bool lleno; } virgl_flujo_t;
void virgl_w(virgl_flujo_t *f, uint32_t v);
void virgl_cmd(virgl_flujo_t *f, uint32_t c, uint32_t obj, uint32_t len);
/* Crea el objeto sombreador «h» (tipo VSOMBRA_*) desde el texto TGSI. */
void virgl_sombreador(virgl_flujo_t *f, uint32_t h, uint32_t tipo, const char *txt);

/* ---- órdenes por la cola de control (memoria en RAM baja, 1:1). Los id de recurso son de todo el dispositivo:
 * 1 y 2 son del escritorio (D472), 10 y 11 de la autoprueba; gpu3d_virgl.c usa desde 0x100. */
#define VIRGL_CAB_ENVIAR 32   /* bytes que SUBMIT_3D necesita delante del flujo en el búfer de virgl_enviar */
bool     virgl_ctx_crear(uint32_t ctx, const char *nombre);
void     virgl_ctx_destruir(uint32_t ctx);
/* respaldo NULL: sin memoria del invitado (el recurso solo vive en el anfitrión: Z, búferes que se escriben en el flujo). */
bool     virgl_recurso_crear(uint32_t ctx, uint32_t id, uint32_t tipo, uint32_t fmt, uint32_t bind, uint32_t l, uint32_t a,
                             void *respaldo, uint32_t bytes);
void     virgl_recurso_soltar(uint32_t id);
/* a_host: TRANSFER_TO_HOST_3D (si no, FROM). desl: byte del respaldo donde empieza la caja. Con valla, la respuesta
 * llega cuando la GPU terminó todo lo anterior del contexto. */
bool     virgl_transferir(uint32_t ctx, bool a_host, uint32_t id, uint32_t x, uint32_t y, uint32_t l, uint32_t a, uint32_t stride,
                          uint32_t desl, bool valla, uint32_t ms);
/* SUBMIT_3D: buf = VIRGL_CAB_ENVIAR bytes libres + el flujo (palabras). Devuelve el tipo de respuesta (0x1100 = bien). */
uint32_t virgl_enviar(uint32_t ctx, uint8_t *buf, uint32_t palabras, bool valla, uint32_t ms);

/* El capset virgl v1: d[0] versión; d[1 + 16 * k + f / 32] bit f % 32: formato f soportado para k = 0 muestreo,
 * 1 destino de color, 2 profundidad y esténcil, 3 vértices. Hasta 77 palabras. */
#define VCAPS_MUESTREO 1
#define VCAPS_DESTINO  17
#define VCAPS_Z        33
#define VCAPS_VERTICES 49
bool     virgl_caps(uint32_t *d, uint32_t ndw);
/* Tipo de respuesta de la última orden (0: el dispositivo no respondió a tiempo). */
uint32_t virgl_ultima(void);

#endif
