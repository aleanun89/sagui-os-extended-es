// SPDX-License-Identifier: GPL-3.0-only
/* virgl.c - 3D por VirGL sobre virtio-gpu (D473).
 *
 * VirGL es el protocolo del virglrenderer del anfitrión: el invitado manda por la cola de control de la
 * virtio-gpu un flujo de órdenes parecido a Gallium (crear objetos de estado, sombreadores en TGSI de
 * texto, framebuffer, viewport, limpiar, dibujar) dentro de SUBMIT_3D, y el anfitrión lo ejecuta con OpenGL.
 * Aquí está lo mínimo: capsets, contexto, recursos 3D con respaldo en RAM, el codificador de órdenes y una
 * autoprueba (limpiar y un triángulo con color por vértice) con vuelta del resultado por TRANSFER_FROM_HOST_3D.
 *
 * El kernel no usa coma flotante: los números reales de las órdenes se escriben ya codificados (IEEE 754).
 *
 * Referencias: virgl_protocol.h y vrend_decode.c de virglrenderer; especificación VIRTIO 1.x, «GPU Device». */
#include "virgl.h"
#include "virtio_gpu.h"
#include "console.h"
#include "memoria.h"
#include "texto.h"
#include "acel2d.h"

/* ---- órdenes virtio-gpu 3D */
#define CMD_OBT_CAPSET_INFO 0x0108
#define CMD_OBT_CAPSET      0x0109
#define CMD_CTX_CREAR       0x0200
#define CMD_CTX_DESTRUIR    0x0201
#define CMD_CTX_ASOCIAR     0x0202
#define CMD_CREAR_3D        0x0204
#define CMD_TRANSF_A_HOST   0x0205
#define CMD_TRANSF_DEL_HOST 0x0206
#define CMD_ENVIAR_3D       0x0207
#define CMD_RECURSO_LIBERAR 0x0102
#define CMD_RESPALDO        0x0106
#define RESP_OK             0x1100
#define RESP_CAPSET_INFO    0x1102
#define RESP_CAPSET         0x1103
#define FLAG_VALLA          1u

typedef struct { uint32_t tipo, flags; uint64_t valla; uint32_t ctx, pad; } EMPACOTADO cab_t;

/* Las constantes del flujo de órdenes de virglrenderer están en virgl.h (D474: las usa también gpu3d_virgl.c). */

/* Reales en IEEE 754. */
#define F_0     0x00000000u
#define F_1     0x3F800000u
#define F_M1    0xBF800000u
#define F_05    0x3F000000u
#define F_M08   0xBF4CCCCDu
#define F_08    0x3F4CCCCDu
#define F_64    0x42800000u
#define F_025   0x3E800000u

typedef virgl_flujo_t flujo_t;
void virgl_w(flujo_t *f, uint32_t v) { if (f->n < f->max) f->p[f->n++] = v; else f->lleno = true; }
void virgl_cmd(flujo_t *f, uint32_t c, uint32_t obj, uint32_t len) { virgl_w(f, c | (obj << 8) | (len << 16)); }
#define w   virgl_w
#define cmd virgl_cmd

/* Los objetos de estado. */
static void crear_blend(flujo_t *f, uint32_t h)
{
    cmd(f, VC_CREAR_OBJETO, VO_BLEND, 11);
    w(f, h); w(f, 0); w(f, 0);
    w(f, 0xFu << 27);                 /* RT0: sin mezcla, máscara de color RGBA */
    for (int i = 1; i < 8; i++) w(f, 0);
}
static void crear_rasterizer(flujo_t *f, uint32_t h)
{
    cmd(f, VC_CREAR_OBJETO, VO_RASTERIZER, 9);
    w(f, h);
    w(f, (1u << 1) | (1u << 29) | (1u << 30));   /* depth_clip, half_pixel_center, bottom_edge_rule */
    w(f, F_1);                        /* tamaño de punto */
    w(f, 0); w(f, 0);
    w(f, F_1);                        /* ancho de línea */
    w(f, 0); w(f, 0); w(f, 0);
}
static void crear_dsa(flujo_t *f, uint32_t h)
{
    cmd(f, VC_CREAR_OBJETO, VO_DSA, 5);
    w(f, h); w(f, 0); w(f, 0); w(f, 0); w(f, 0);   /* sin Z, sin esténcil, sin alfa */
}
static void crear_elementos(flujo_t *f, uint32_t h)
{
    cmd(f, VC_CREAR_OBJETO, VO_VERTEX_ELEMENTS, 1 + 2 * 4);
    w(f, h);
    w(f, 0);  w(f, 0); w(f, 0); w(f, VFMT_R32G32B32A32_F);   /* posición */
    w(f, 16); w(f, 0); w(f, 0); w(f, VFMT_R32G32B32A32_F);   /* color */
}
/* Sombreador TGSI en texto (tipo 0 vértices, 1 fragmentos). */
void virgl_sombreador(flujo_t *f, uint32_t h, uint32_t tipo, const char *txt)
{
    uint32_t len = (uint32_t)txt_tamanho(txt) + 1, dw = (len + 3) / 4;
    cmd(f, VC_CREAR_OBJETO, VO_SHADER, 5 + dw);
    w(f, h); w(f, tipo);
    w(f, len);          /* primera parte: la longitud total del texto */
    w(f, 300 + len / 2);   /* tokens: holgado (el anfitrión reserva con esto al traducir el texto) */
    w(f, 0);            /* sin salidas de streamout */
    for (uint32_t i = 0; i < dw; i++) {
        uint32_t v = 0;
        for (uint32_t k = 0; k < 4; k++) {
            uint32_t j = i * 4 + k;
            if (j < len) v |= (uint32_t)(uint8_t)txt[j] << (8 * k);
        }
        w(f, v);
    }
}
static void asociar(flujo_t *f, uint32_t obj, uint32_t h) { cmd(f, VC_ASOCIAR_OBJETO, obj, 1); w(f, h); }
static void asociar_sombreador(flujo_t *f, uint32_t h, uint32_t tipo) { cmd(f, VC_ASOCIAR_SOMBR, 0, 2); w(f, h); w(f, tipo); }

static const char vs_txt[] =
    "VERT\n"
    "DCL IN[0]\n"
    "DCL IN[1]\n"
    "DCL OUT[0], POSITION\n"
    "DCL OUT[1], GENERIC[0]\n"
    "  0: MOV OUT[0], IN[0]\n"
    "  1: MOV OUT[1], IN[1]\n"
    "  2: END\n";
static const char fs_txt[] =
    "FRAG\n"
    "PROPERTY FS_COLOR0_WRITES_ALL_CBUFS 1\n"
    "DCL IN[0], GENERIC[0], PERSPECTIVE\n"
    "DCL OUT[0], COLOR\n"
    "  0: MOV OUT[0], IN[0]\n"
    "  1: END\n";

/* ---- recursos y memoria */
#define LADO 128u           /* el destino de la autoprueba: 128x128, 64 KB */
#define ID_RT 10u
#define ID_VB 11u
#define CTX   1u

static int estado;
static uint32_t valla_n;

bool virgl_disponible(void) { return vg_virgl_aceptado(); }
int  virgl_estado(void) { return estado; }

typedef struct { cab_t c; uint32_t id, tipo, fmt, bind, l, a, prof, capas, nivel, muestras, flags, pad; } EMPACOTADO cmd_crear3d_t;
typedef struct { cab_t c; uint32_t id, n; uint64_t dir; uint32_t len, pad; } EMPACOTADO cmd_respaldo_t;
typedef struct { cab_t c; uint32_t id, pad; } EMPACOTADO cmd_recurso_t;
typedef struct { cab_t c; uint32_t nlen, init; char nombre[64]; } EMPACOTADO cmd_ctx_t;
typedef struct { cab_t c; uint32_t x, y, z, l, a, p; uint64_t desl; uint32_t id, nivel, stride, stride_capa; } EMPACOTADO cmd_transf_t;
typedef struct { cab_t c; uint32_t tam, pad; } EMPACOTADO cmd_enviar_t;

static uint8_t *area;      /* 4 páginas: el flujo de la autoprueba (con la cabecera de SUBMIT_3D delante) */
static uint8_t *peq;       /* 1 página: las órdenes pequeñas (D474: fija desde el primer uso) */
static uint8_t *resp;      /* 1 página: su respuesta */

static void cab(cab_t *c, uint32_t tipo, bool valla, uint32_t ctx)
{
    memset(c, 0, sizeof *c);
    c->tipo = tipo; c->ctx = ctx;
    if (valla) { c->flags = FLAG_VALLA; c->valla = ++valla_n; }
}

static bool paginas_fijas(void)
{
    if (!peq) { uint32_t f = memfisica_alocar_seguidas(1); if (!f) return false; peq = (uint8_t *)f; }
    if (!resp) { uint32_t f = memfisica_alocar_seguidas(1); if (!f) return false; resp = (uint8_t *)f; }
    return true;
}

/* D474: una orden por la cola de control. Si la presentación 2D tiene la cola (una tanda de rectángulos en marcha,
 * que dura poco), se reintenta dentro del plazo en vez de fallar. Devuelve el tipo de respuesta (0: ninguna). */
static uint32_t ultima = 1;
uint32_t virgl_ultima(void) { return ultima; }

static uint32_t orden_dma(const void *c, uint32_t n, void *r, uint32_t rn, uint32_t ms)
{
    acel_prazo_t pz;
    acel_prazo(&pz, ms);
    for (;;) {
        uint32_t t = vg_orden_dma(c, n, r, rn, ms);
        if (t || !vg_ocupado() || acel_prazo_esgotado(&pz)) return ultima = t;
    }
}

static bool orden(const void *c, uint32_t n, uint32_t ms)
{
    if (!paginas_fijas() || n > PAGINA) return false;
    memcpy(peq, c, n);
    uint32_t t = orden_dma(peq, n, resp, PAGINA, ms);
    if (t != RESP_OK) klog("[VIRGL] orden %x: respuesta %x\n", ((const cab_t *)c)->tipo, t);
    return t == RESP_OK;
}

static uint8_t *pedir(uint32_t bytes, uint32_t *paginas)
{
    uint32_t pg = (bytes + PAGINA - 1) / PAGINA;
    uint32_t fis = memfisica_alocar_seguidas(pg);
    if (!fis) return NULL;
    memset((void *)fis, 0, pg * PAGINA);
    *paginas = pg;
    return (uint8_t *)fis;
}

bool virgl_ctx_crear(uint32_t ctx, const char *nombre)
{
    cmd_ctx_t cc; cab(&cc.c, CMD_CTX_CREAR, false, ctx);
    uint32_t n = (uint32_t)txt_tamanho(nombre);
    if (n > 63) n = 63;
    cc.nlen = n; cc.init = 0; memset(cc.nombre, 0, sizeof cc.nombre); memcpy(cc.nombre, nombre, n);
    return orden(&cc, sizeof cc, 500);
}

void virgl_ctx_destruir(uint32_t ctx)
{
    cab_t d; cab(&d, CMD_CTX_DESTRUIR, false, ctx);
    orden(&d, sizeof d, 500);
}

bool virgl_recurso_crear(uint32_t ctx, uint32_t id, uint32_t tipo, uint32_t fmt, uint32_t bind, uint32_t l, uint32_t a,
                         void *respaldo, uint32_t bytes)
{
    cmd_crear3d_t c; cab(&c.c, CMD_CREAR_3D, false, ctx);
    c.id = id; c.tipo = tipo; c.fmt = fmt; c.bind = bind; c.l = l; c.a = a; c.prof = 1; c.capas = 1;
    c.nivel = 0; c.muestras = 0; c.flags = 0; c.pad = 0;
    if (!orden(&c, sizeof c, 500)) return false;
    if (respaldo) {
        cmd_respaldo_t r; cab(&r.c, CMD_RESPALDO, false, ctx);
        r.id = id; r.n = 1; r.dir = (uint32_t)respaldo; r.len = bytes; r.pad = 0;
        if (!orden(&r, sizeof r, 500)) { virgl_recurso_soltar(id); return false; }
    }
    cmd_recurso_t as; cab(&as.c, CMD_CTX_ASOCIAR, false, ctx);
    as.id = id; as.pad = 0;
    if (!orden(&as, sizeof as, 500)) { virgl_recurso_soltar(id); return false; }
    return true;
}

void virgl_recurso_soltar(uint32_t id)
{
    /* RESOURCE_UNREF suelta también el respaldo y lo quita de los contextos. */
    cmd_recurso_t u; cab(&u.c, CMD_RECURSO_LIBERAR, false, 0);
    u.id = id; u.pad = 0;
    orden(&u, sizeof u, 500);
}

bool virgl_transferir(uint32_t ctx, bool a_host, uint32_t id, uint32_t x, uint32_t y, uint32_t l, uint32_t a, uint32_t stride,
                      uint32_t desl, bool valla, uint32_t ms)
{
    cmd_transf_t t; cab(&t.c, a_host ? CMD_TRANSF_A_HOST : CMD_TRANSF_DEL_HOST, valla, ctx);
    t.x = x; t.y = y; t.z = 0; t.l = l; t.a = a; t.p = 1; t.desl = desl; t.id = id; t.nivel = 0; t.stride = stride; t.stride_capa = 0;
    return orden(&t, sizeof t, ms);
}

uint32_t virgl_enviar(uint32_t ctx, uint8_t *buf, uint32_t palabras, bool valla, uint32_t ms)
{
    if (!paginas_fijas()) return 0;
    cmd_enviar_t *e = (cmd_enviar_t *)buf;
    cab(&e->c, CMD_ENVIAR_3D, valla, ctx);
    e->tam = palabras * 4; e->pad = 0;
    return orden_dma(buf, (uint32_t)sizeof *e + palabras * 4, resp, PAGINA, ms);
}

/* ---- capsets */
static void capsets(void)
{
    uint32_t n = vg_num_capsets();
    klog("[VIRGL] el dispositivo anuncia %u capsets\n", n);
    for (uint32_t i = 0; i < n && i < 8; i++) {
        struct { cab_t c; uint32_t indice, pad; } EMPACOTADO ci;
        cab(&ci.c, CMD_OBT_CAPSET_INFO, false, 0); ci.indice = i; ci.pad = 0;
        memcpy(peq, &ci, sizeof ci);
        if (orden_dma(peq, sizeof ci, resp, PAGINA, 500) != RESP_CAPSET_INFO) { klog("[VIRGL] capset %u: sin informacion\n", i); continue; }
        uint32_t *r = (uint32_t *)(resp + sizeof(cab_t));
        uint32_t id = r[0], ver = r[1], tam = r[2];
        klog("[VIRGL] capset %u: id %u (%s), version maxima %u, %u bytes\n", i, id, id == 1 ? "virgl" : id == 2 ? "virgl2" : "otro", ver, tam);
        if ((id == 1 || id == 2) && tam && tam <= PAGINA - sizeof(cab_t)) {
            struct { cab_t c; uint32_t id, ver; } EMPACOTADO cs;
            cab(&cs.c, CMD_OBT_CAPSET, false, 0); cs.id = id; cs.ver = ver;
            memcpy(peq, &cs, sizeof cs);
            if (orden_dma(peq, sizeof cs, resp, PAGINA, 500) == RESP_CAPSET) {
                uint32_t *d = (uint32_t *)(resp + sizeof(cab_t));
                klog("[VIRGL] capset %u leido: version %u, primera palabra %x, segunda %x\n", id, d[0], d[1], d[2]);
            } else klog("[VIRGL] capset %u: GET_CAPSET sin respuesta\n", id);
        }
    }
}

/* D474: el capset virgl v1 (el que hay siempre) en d: versión y las máscaras de formatos (muestreo, destino,
 * profundidad y vértices, 16 palabras cada una: el bit f % 32 de la palabra f / 32 es el formato f). */
bool virgl_caps(uint32_t *d, uint32_t ndw)
{
    if (!paginas_fijas() || ndw > 77) return false;
    struct { cab_t c; uint32_t id, ver; } EMPACOTADO cs;
    cab(&cs.c, CMD_OBT_CAPSET, false, 0); cs.id = 1; cs.ver = 1;
    memcpy(peq, &cs, sizeof cs);
    if (orden_dma(peq, sizeof cs, resp, PAGINA, 500) != RESP_CAPSET) return false;
    memcpy(d, resp + sizeof(cab_t), ndw * 4);
    return true;
}

/* ---- la autoprueba */
bool virgl_autoprueba(void)
{
    estado = 0;
    if (!vg_virgl_ofrecido()) { klog("[VIRGL] la GPU virtual no ofrece VIRGL: sin 3D (con QEMU: -device virtio-vga-gl)\n"); return false; }
    if (!vg_virgl_aceptado()) { klog("[VIRGL] VIRGL ofrecido pero no negociado\n"); return false; }
    uint32_t pg_area = 0, pg_rt = 0, pg_vb = 0;
    area = pedir(4 * PAGINA, &pg_area);
    uint8_t *rt = pedir(LADO * LADO * 4, &pg_rt);
    uint8_t *vb = pedir(PAGINA, &pg_vb);
    if (!paginas_fijas() || !area || !rt || !vb) {
        klog("[VIRGL] sin memoria para la autoprueba\n");
        if (area) memfisica_liberar_seguidas((uint32_t)area, pg_area);
        if (rt) memfisica_liberar_seguidas((uint32_t)rt, pg_rt);
        if (vb) memfisica_liberar_seguidas((uint32_t)vb, pg_vb);
        area = NULL;
        return false;
    }
    bool ok = false, ctx = false, r1 = false, r2 = false;
    capsets();

    ctx = virgl_ctx_crear(CTX, "sagui");
    if (!ctx) goto fin;
    klog("[VIRGL] contexto %u creado\n", CTX);
    r1 = virgl_recurso_crear(CTX, ID_RT, TGT_2D, VFMT_B8G8R8A8, BIND_RENDER_TARGET | BIND_SAMPLER_VIEW, LADO, LADO, rt, LADO * LADO * 4);
    r2 = virgl_recurso_crear(CTX, ID_VB, TGT_BUFFER, FMT_BUFFER, BIND_VERTEX_BUFFER, 96, 1, vb, 96);
    if (!r1 || !r2) goto fin;
    klog("[VIRGL] recursos 3D creados: destino %ux%u y bufer de vertices\n", LADO, LADO);

    /* Tres vértices: posición (x, y, z, w) y color (r, g, b, a). */
    uint32_t *v = (uint32_t *)vb;
    static const uint32_t datos[3][8] = {
        { F_M08, F_M08, F_0, F_1,  F_1, F_0, F_0, F_1 },   /* rojo, abajo a la izquierda */
        { F_08,  F_M08, F_0, F_1,  F_0, F_1, F_0, F_1 },   /* verde, abajo a la derecha */
        { F_0,   F_08,  F_0, F_1,  F_0, F_0, F_1, F_1 },   /* azul, arriba */
    };
    memcpy(v, datos, sizeof datos);
    if (!virgl_transferir(CTX, true, ID_VB, 0, 0, 96, 1, 0, 0, true, 1500)) goto fin;

    uint32_t *flujo_mem = (uint32_t *)(area + VIRGL_CAB_ENVIAR);
    flujo_t f = { flujo_mem, 0, (4 * PAGINA - VIRGL_CAB_ENVIAR) / 4, false };
    /* destino y estado fijo */
    cmd(&f, VC_CREAR_OBJETO, VO_SURFACE, 5); w(&f, 7); w(&f, ID_RT); w(&f, VFMT_B8G8R8A8); w(&f, 0); w(&f, 0);
    cmd(&f, VC_SET_FRAMEBUFFER, 0, 3); w(&f, 1); w(&f, 0); w(&f, 7);
    crear_blend(&f, 1); asociar(&f, VO_BLEND, 1);
    crear_rasterizer(&f, 2); asociar(&f, VO_RASTERIZER, 2);
    crear_dsa(&f, 3); asociar(&f, VO_DSA, 3);
    virgl_sombreador(&f, 5, 0, vs_txt); asociar_sombreador(&f, 5, 0);
    virgl_sombreador(&f, 6, 1, fs_txt); asociar_sombreador(&f, 6, 1);
    crear_elementos(&f, 4); asociar(&f, VO_VERTEX_ELEMENTS, 4);
    cmd(&f, VC_SET_VIEWPORT, 0, 7); w(&f, 0); w(&f, F_64); w(&f, F_64); w(&f, F_05); w(&f, F_64); w(&f, F_64); w(&f, F_05);
    /* limpiar a gris oscuro y dibujar */
    cmd(&f, VC_LIMPIAR, 0, 8); w(&f, 4); w(&f, F_025); w(&f, F_025); w(&f, F_025); w(&f, F_1); w(&f, 0); w(&f, 0); w(&f, 0);
    cmd(&f, VC_SET_VERTEX_BUF, 0, 3); w(&f, 32); w(&f, 0); w(&f, ID_VB);
    cmd(&f, VC_DIBUJAR, 0, 12);
    w(&f, 0); w(&f, 3); w(&f, 4); w(&f, 0); w(&f, 1); w(&f, 0); w(&f, 0); w(&f, 0); w(&f, 0); w(&f, 0); w(&f, 2); w(&f, 0);
    if (f.lleno) { klog("[VIRGL] el flujo no cabe\n"); goto fin; }
    uint32_t t = virgl_enviar(CTX, area, f.n, true, 1500);
    klog("[VIRGL] SUBMIT_3D de %u palabras: respuesta %x\n", f.n, t);
    if (t != RESP_OK) goto fin;
    if (!virgl_transferir(CTX, false, ID_RT, 0, 0, LADO, LADO, LADO * 4, 0, true, 1500)) goto fin;

    /* Comprobación: cuenta los píxeles del color de fondo y los dominados por cada color. */
    uint32_t *px = (uint32_t *)rt, fondo = 0, rojo = 0, verde = 0, azul = 0, otros = 0;
    for (uint32_t i = 0; i < LADO * LADO; i++) {
        uint32_t r = (px[i] >> 16) & 255, g = (px[i] >> 8) & 255, b = px[i] & 255;
        if (r >= 0x38 && r <= 0x48 && g >= 0x38 && g <= 0x48 && b >= 0x38 && b <= 0x48) fondo++;
        else if (r > 160 && g < 90 && b < 90) rojo++;
        else if (g > 160 && r < 90 && b < 90) verde++;
        else if (b > 160 && r < 90 && g < 90) azul++;
        else otros++;
    }
    klog("[VIRGL] autoprueba del triangulo: fondo %u, rojo %u, verde %u, azul %u, mezclados %u (de %u pixeles)\n",
         fondo, rojo, verde, azul, otros, LADO * LADO);
    ok = fondo > 3000 && rojo > 20 && verde > 20 && azul > 20 && otros > 1000;
fin:
    if (ctx) {
        if (r1) virgl_recurso_soltar(ID_RT);
        if (r2) virgl_recurso_soltar(ID_VB);
        virgl_ctx_destruir(CTX);
    }
    memfisica_liberar_seguidas((uint32_t)area, pg_area);
    memfisica_liberar_seguidas((uint32_t)rt, pg_rt);
    memfisica_liberar_seguidas((uint32_t)vb, pg_vb);
    area = NULL;
    estado = ok ? 1 : -1;

    klog("[VIRGL] autoprueba: %s\n", ok ? "BIEN" : "MAL");
    return ok;
}
