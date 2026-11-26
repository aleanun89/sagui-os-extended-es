// SPDX-License-Identifier: GPL-3.0-only
/* virtio_gpu.c - driver virtio-gpu en modo 2D (D472).
 *
 * Especificación VIRTIO 1.x, «GPU Device». Un solo scanout (el 0) y un recurso 2D en formato
 * B8G8R8X8_UNORM, que en la memoria little-endian es justo el 0x00RRGGBB del búfer de atrás de video.c
 * (formato directo, sin conversión). El respaldo del recurso es una zona seguida de RAM baja que el
 * kernel ve 1:1: video.c la usa como "framebuffer de la tarjeta".
 *
 * Presentación: video.c llama a vg_atualizar() con cada rectángulo que la CPU escribió y, al terminar
 * el cuadro, a vg_descargar(), que manda TRANSFER_TO_HOST_2D y RESOURCE_FLUSH de cada rectángulo en una
 * sola tanda (un aviso a la cola y una espera). Así no se vuelca la pantalla entera en cada cuadro.
 *
 * Sondeo, sin interrupciones (se puede llamar desde la IRQ del ratón, que también dibuja). Un cerrojo de
 * reentrada deja lo apuntado para la vuelta siguiente si ya hay una tanda en marcha.
 *
 * Fase 3D (VirGL, pendiente): la negociación lee VIRTIO_GPU_F_VIRGL (bit 0) y la anota en el log, pero no
 * la acepta; los contextos y SUBMIT_3D irían por la misma cola de control, con el mismo envío. */
#include "virtio_gpu.h"
#include "virtio_pci.h"
#include "acel2d.h"
#include "bootinfo.h"
#include "console.h"
#include "cpu.h"
#include "memoria.h"
#include "pci.h"
#include "texto.h"

#define ID_GPU 0x1050
#define F_VIRGL (1ull << 0)
#define F_EDID  (1ull << 1)

/* Órdenes y respuestas. */
#define CMD_INFO_PANTALLA   0x0100
#define CMD_CREAR_2D        0x0101
#define CMD_LIBERAR         0x0102
#define CMD_SCANOUT         0x0103
#define CMD_FLUSH           0x0104
#define CMD_TRANSFERIR_2D   0x0105
#define CMD_RESPALDO        0x0106
#define RESP_OK             0x1100
#define RESP_INFO_PANTALLA  0x1101
#define FORMATO_B8G8R8X8    2

typedef struct { uint32_t tipo, flags; uint64_t fence; uint32_t ctx, pad; } EMPACOTADO cab_t;
typedef struct { uint32_t x, y, l, a; } EMPACOTADO rect_t;
typedef struct { cab_t c; uint32_t recurso, formato, l, a; } EMPACOTADO cmd_crear_t;
typedef struct { cab_t c; uint32_t recurso, pad; } EMPACOTADO cmd_recurso_t;
typedef struct { cab_t c; rect_t r; uint32_t scanout, recurso; } EMPACOTADO cmd_scanout_t;
typedef struct { cab_t c; rect_t r; uint64_t desl; uint32_t recurso, pad; } EMPACOTADO cmd_transf_t;
typedef struct { cab_t c; rect_t r; uint32_t recurso, pad; } EMPACOTADO cmd_flush_t;
typedef struct { cab_t c; uint32_t recurso, n; uint64_t dir; uint32_t len, pad; } EMPACOTADO cmd_respaldo_t;
typedef struct { rect_t r; uint32_t activa, flags; } EMPACOTADO modo_pant_t;
typedef struct { cab_t c; modo_pant_t m[16]; } EMPACOTADO resp_info_t;

#define RANURA_CMD  128     /* bytes por orden en la página de órdenes */
#define RANURA_RESP 64
#define RANURAS     16      /* 16 órdenes por tanda = 8 rectángulos */
#define PEND_MAX    4       /* rectángulos apuntados antes de fundirlos */

static virtio_dev_t dev;
static virtio_fila_t ctl;
static uint8_t *pg_cmd, *pg_resp, *pg_info;
static bool ativo, ocupado, muerto, virgl_aceptado, virgl_ofrecido;
static uint32_t recurso_actual, respaldo_pag;
static uint8_t *respaldo;
static uint32_t larg, alt;
static uint32_t n_enviados, n_fallos, fallos_seguidos;
static rect_t pend[PEND_MAX];
static int n_pend;

bool vg_ativo(void) { return ativo && !muerto; }
uint32_t vg_enviados(void) { return n_enviados; }
uint32_t vg_fallos(void) { return n_fallos; }

static bool buscar(pci_disp_t *out)
{
    static pci_disp_t v[64];
    int n = pci_listar(v, 64);
    for (int i = 0; i < n; i++)
        if (v[i].fabricante == VIRTIO_FABRICANTE && v[i].modelo == ID_GPU) { *out = v[i]; return true; }
    return false;
}

bool vg_candidato(bool vbe_activo)
{
    pci_disp_t p;
    if (!buscar(&p)) return false;
    /* virtio-vga (clase 0x0300) trae VGA y VBE: la etapa 2 ya puso un modo por ella. Un VBE de otra
     * tarjeta (-vga std junto a un -device virtio-gpu-pci) se respeta: esa es la pantalla del arranque. */
    return !vbe_activo || (p.classe == 0x03 && p.subclasse == 0x00);
}

/* ---- órdenes */

static void cab(cab_t *c, uint32_t tipo) { memset(c, 0, sizeof *c); c->tipo = tipo; }

/* Pone una orden (cmd, n bytes) y su respuesta en la tanda; devuelve false si no caben. */
static bool poner(int ranura, const void *cmd, uint32_t n, uint32_t resp_len, uint8_t *resp)
{
    uint8_t *c = pg_cmd + ranura * RANURA_CMD;
    memcpy(c, cmd, n);
    memset(resp, 0, resp_len);
    virtio_buf_t b[2] = { { (uint32_t)c, n, false }, { (uint32_t)resp, resp_len, true } };
    return virtio_fila_poner(&ctl, b, 2);
}

/* Una orden sola, con espera. Devuelve el tipo de respuesta (0 = no respondió). */
static uint32_t orden(const void *cmd, uint32_t n, uint32_t *tipo_resp)
{
    uint8_t *resp = pg_resp;
    if (!poner(0, cmd, n, RANURA_RESP, resp)) return 0;
    virtio_fila_avisar(&dev, &ctl);
    if (!virtio_fila_esperar(&ctl, 500)) return 0;
    uint32_t t = ((cab_t *)resp)->tipo;
    if (tipo_resp) *tipo_resp = t;
    return t;
}

static bool orden_ok(const void *cmd, uint32_t n)
{
    return orden(cmd, n, NULL) == RESP_OK;
}

static bool crear_recurso(uint32_t id, uint32_t l, uint32_t a, const uint8_t *fb)
{
    cmd_crear_t c; cab(&c.c, CMD_CREAR_2D);
    c.recurso = id; c.formato = FORMATO_B8G8R8X8; c.l = l; c.a = a;
    if (!orden_ok(&c, sizeof c)) return false;
    cmd_respaldo_t r; cab(&r.c, CMD_RESPALDO);
    r.recurso = id; r.n = 1; r.dir = (uint32_t)fb; r.len = l * a * 4; r.pad = 0;
    if (!orden_ok(&r, sizeof r)) {
        cmd_recurso_t u; cab(&u.c, CMD_LIBERAR); u.recurso = id; u.pad = 0;
        orden_ok(&u, sizeof u);
        return false;
    }
    return true;
}

static bool poner_scanout(uint32_t id, uint32_t l, uint32_t a)
{
    cmd_scanout_t s; cab(&s.c, CMD_SCANOUT);
    s.r = (rect_t){ 0, 0, l, a }; s.scanout = 0; s.recurso = id;
    return orden_ok(&s, sizeof s);
}

static void liberar_recurso(uint32_t id)
{
    cmd_recurso_t u; cab(&u.c, CMD_LIBERAR); u.recurso = id; u.pad = 0;
    orden_ok(&u, sizeof u);
}

/* Memoria del respaldo: zona seguida de RAM baja, a cero (pantalla negra). */
static uint8_t *pedir_respaldo(uint32_t l, uint32_t a, uint32_t *paginas)
{
    uint32_t pg = (l * a * 4 + PAGINA - 1) / PAGINA;
    uint32_t fis = memfisica_alocar_seguidas(pg);
    if (!fis) return NULL;
    memset((void *)fis, 0, pg * PAGINA);
    *paginas = pg;
    return (uint8_t *)fis;
}

static void reiniciar_todo(void)
{
    virtio_pci_reiniciar(&dev);
    ativo = false;
}

/* ---- inicio */

bool vg_iniciar(uint32_t pref_l, uint32_t pref_a, vg_modo_t *m)
{
    pci_disp_t p;
    char motivo[96] = "";
    if (ativo) return false;
    if (!buscar(&p)) return false;
    if (!virtio_pci_abrir(&dev, &p, motivo, sizeof motivo)) { klog("[VIRTIOGPU] %s\n", motivo); return false; }
    if (!virtio_pci_negociar(&dev, F_VIRGL, motivo, sizeof motivo)) { klog("[VIRTIOGPU] %s\n", motivo); return false; }
    virgl_ofrecido = (dev.ofrecidas & F_VIRGL) != 0;
    virgl_aceptado = (dev.aceptadas & F_VIRGL) != 0;
    klog("[VIRTIOGPU] dispositivo %x:%x en %u:%u.%u, caracteristicas altas %x bajas %x; VIRGL %s%s, EDID %s\n",
         p.fabricante, p.modelo, p.barramento, p.dispositivo, p.funcao, (uint32_t)(dev.ofrecidas >> 32), (uint32_t)dev.ofrecidas,
         virgl_ofrecido ? "ofrecido" : "no ofrecido", virgl_aceptado ? " y negociado (D473)" : "", (dev.ofrecidas & F_EDID) ? "si" : "no");
    uint32_t pgs = memfisica_alocar_seguidas(3);
    if (!pgs || !virtio_pci_fila(&dev, 0, &ctl, motivo, sizeof motivo)) {
        klog("[VIRTIOGPU] cola de control: %s\n", pgs ? motivo : "sin memoria");
        reiniciar_todo();
        return false;
    }
    memset((void *)pgs, 0, 3 * PAGINA);
    pg_cmd = (uint8_t *)pgs; pg_resp = pg_cmd + PAGINA; pg_info = pg_resp + PAGINA;
    virtio_pci_listo(&dev);
    ativo = true;   /* para que reiniciar_todo() y las órdenes funcionen */

    /* La pantalla 0: lo que dice el dispositivo (informativo; el modo lo elige el sistema). */
    cab_t ci; cab(&ci, CMD_INFO_PANTALLA);
    uint8_t *ri = pg_info;
    memset(ri, 0, sizeof(resp_info_t));
    memcpy(pg_cmd, &ci, sizeof ci);
    virtio_buf_t b[2] = { { (uint32_t)pg_cmd, sizeof ci, false }, { (uint32_t)ri, sizeof(resp_info_t), true } };
    bool info_ok = virtio_fila_poner(&ctl, b, 2);
    if (info_ok) { virtio_fila_avisar(&dev, &ctl); info_ok = virtio_fila_esperar(&ctl, 500); }
    resp_info_t *inf = (resp_info_t *)ri;
    if (!info_ok || inf->c.tipo != RESP_INFO_PANTALLA) {
        klog("[VIRTIOGPU] GET_DISPLAY_INFO sin respuesta valida (tipo %x)\n", info_ok ? inf->c.tipo : 0);
        reiniciar_todo();
        return false;
    }
    klog("[VIRTIOGPU] pantalla 0: %s, %ux%u\n", inf->m[0].activa ? "activa" : "sin monitor", inf->m[0].r.l, inf->m[0].r.a);

    uint32_t l = pref_l, a = pref_a;
    if (!l || !a) { l = 1024; a = 768; }
    if (l < 640 || a < 480 || l > VG_MAX_L || a > VG_MAX_A) { l = 1024; a = 768; }
    uint32_t pgr;
    uint8_t *fb = pedir_respaldo(l, a, &pgr);
    if (!fb) { klog("[VIRTIOGPU] sin memoria seguida para %ux%u\n", l, a); reiniciar_todo(); return false; }
    if (!crear_recurso(1, l, a, fb) || !poner_scanout(1, l, a)) {
        klog("[VIRTIOGPU] no se pudo crear el recurso 2D o el scanout\n");
        reiniciar_todo();
        memfisica_liberar_seguidas((uint32_t)fb, pgr);
        return false;
    }
    respaldo = fb; respaldo_pag = pgr; recurso_actual = 1; larg = l; alt = a;
    m->fb = fb; m->fis = (uint32_t)fb; m->pitch = l * 4; m->largura = l; m->altura = a;
    klog("[VIRTIOGPU] activo: %ux%u, 32 bits, scanout 0, recurso 1, respaldo %x (%u KB), 2D por TRANSFER_TO_HOST_2D + RESOURCE_FLUSH\n",
         l, a, (uint32_t)fb, pgr * 4);
    return true;
}

bool vg_mudar(uint32_t l, uint32_t a, vg_modo_t *m)
{
    if (!vg_ativo() || ocupado || l < 640 || a < 480 || l > VG_MAX_L || a > VG_MAX_A) return false;
    ocupado = true;
    n_pend = 0;
    uint32_t pgr;
    uint8_t *fb = pedir_respaldo(l, a, &pgr);
    uint32_t id = recurso_actual == 1 ? 2 : 1;
    bool ok = fb && crear_recurso(id, l, a, fb);
    if (ok && !poner_scanout(id, l, a)) { liberar_recurso(id); ok = false; }
    if (!ok) {
        if (fb) memfisica_liberar_seguidas((uint32_t)fb, pgr);
        ocupado = false;
        return false;
    }
    liberar_recurso(recurso_actual);
    memfisica_liberar_seguidas((uint32_t)respaldo, respaldo_pag);
    respaldo = fb; respaldo_pag = pgr; recurso_actual = id; larg = l; alt = a;
    m->fb = fb; m->fis = (uint32_t)fb; m->pitch = l * 4; m->largura = l; m->altura = a;
    ocupado = false;
    return true;
}

/* ---- presentación */

void vg_atualizar(int x, int y, int l, int a)
{
    if (!vg_ativo() || l <= 0 || a <= 0) return;
    int x1 = x + l, y1 = y + a;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x1 > (int)larg) x1 = (int)larg;
    if (y1 > (int)alt) y1 = (int)alt;
    if (x >= x1 || y >= y1) return;
    bool sim = int_salva_desliga();
    rect_t r = { (uint32_t)x, (uint32_t)y, (uint32_t)(x1 - x), (uint32_t)(y1 - y) };
    /* Si se solapa o toca con uno ya apuntado, se funden. */
    for (int i = 0; i < n_pend; i++) {
        rect_t *q = &pend[i];
        if (r.x <= q->x + q->l && q->x <= r.x + r.l && r.y <= q->y + q->a && q->y <= r.y + r.a) {
            uint32_t ax = r.x < q->x ? r.x : q->x, ay = r.y < q->y ? r.y : q->y;
            uint32_t bx = r.x + r.l > q->x + q->l ? r.x + r.l : q->x + q->l;
            uint32_t by = r.y + r.a > q->y + q->a ? r.y + r.a : q->y + q->a;
            *q = (rect_t){ ax, ay, bx - ax, by - ay };
            int_restaura(sim);
            return;
        }
    }
    if (n_pend == PEND_MAX) {   /* lleno: el último pasa a abarcar al nuevo */
        rect_t *q = &pend[PEND_MAX - 1];
        uint32_t ax = r.x < q->x ? r.x : q->x, ay = r.y < q->y ? r.y : q->y;
        uint32_t bx = r.x + r.l > q->x + q->l ? r.x + r.l : q->x + q->l;
        uint32_t by = r.y + r.a > q->y + q->a ? r.y + r.a : q->y + q->a;
        *q = (rect_t){ ax, ay, bx - ax, by - ay };
    } else {
        pend[n_pend++] = r;
    }
    int_restaura(sim);
}

void vg_descargar(void)
{
    if (!vg_ativo() || ocupado) return;   /* si hay una tanda en marcha, lo apuntado espera */
    for (int vuelta = 0; vuelta < 4 && n_pend > 0 && vg_ativo(); vuelta++) {
        bool sim = int_salva_desliga();
        if (ocupado) { int_restaura(sim); return; }   /* D474: la comprobación y la toma, sin una IRQ en medio */
        rect_t lista[PEND_MAX];
        int n = n_pend;
        memcpy(lista, pend, (uint32_t)n * sizeof(rect_t));
        n_pend = 0;
        ocupado = true;
        int_restaura(sim);

        bool ok = true;
        int ranura = 0;
        for (int i = 0; i < n && ok; i++) {
            cmd_transf_t t; cab(&t.c, CMD_TRANSFERIR_2D);
            t.r = lista[i]; t.desl = ((uint64_t)lista[i].y * larg + lista[i].x) * 4; t.recurso = recurso_actual; t.pad = 0;
            cmd_flush_t f; cab(&f.c, CMD_FLUSH);
            f.r = lista[i]; f.recurso = recurso_actual; f.pad = 0;
            ok = poner(ranura, &t, sizeof t, RANURA_RESP, pg_resp + ranura * RANURA_RESP);
            ranura++;
            ok = ok && poner(ranura, &f, sizeof f, RANURA_RESP, pg_resp + ranura * RANURA_RESP);
            ranura++;
        }
        if (ok) {
            virtio_fila_avisar(&dev, &ctl);
            ok = virtio_fila_esperar(&ctl, 200);
        }
        if (ok)
            for (int k = 0; k < ranura; k++)
                if (((cab_t *)(pg_resp + k * RANURA_RESP))->tipo != RESP_OK) ok = false;
        if (ok) { n_enviados += (uint32_t)n; fallos_seguidos = 0; }
        else {
            n_fallos++;
            if (++fallos_seguidos >= 3) {
                muerto = true;
                klog("[VIRTIOGPU] la GPU no responde: se deja de presentar\n");
            }
        }
        ocupado = false;
    }
}

/* ---- D473: puerta para el 3D (virgl.c) */

bool vg_virgl_aceptado(void) { return vg_ativo() && virgl_aceptado; }
bool vg_ocupado(void) { return ocupado; }
bool vg_virgl_ofrecido(void) { return virgl_ofrecido; }
uint32_t vg_num_capsets(void) { return dev.disp ? *(volatile uint32_t *)(dev.disp + 12) : 0; }

/* Una orden de cualquier tamaño por la cola de control, con la memoria del que llama (RAM baja, 1:1: el
 * puntero es la dirección física). La respuesta empieza con la cabecera; devuelve su tipo (0: sin respuesta
 * a tiempo). Con la presentación en marcha por otro camino no hace nada. */
uint32_t vg_orden_dma(const void *cmd, uint32_t n, void *resp, uint32_t rn, uint32_t ms)
{
    if (!vg_ativo()) return 0;
    bool sim = int_salva_desliga();   /* D474: comprobar y tomar la cola sin que la IRQ del ratón entre en medio */
    if (ocupado) { int_restaura(sim); return 0; }
    ocupado = true;
    int_restaura(sim);
    uint32_t t = 0;
    memset(resp, 0, rn);
    virtio_buf_t b[2] = { { (uint32_t)cmd, n, false }, { (uint32_t)resp, rn, true } };
    if (virtio_fila_poner(&ctl, b, 2)) {
        virtio_fila_avisar(&dev, &ctl);
        if (virtio_fila_esperar(&ctl, ms)) t = ((cab_t *)resp)->tipo;
        else n_fallos++;
    }
    ocupado = false;
    return t;
}
