// SPDX-License-Identifier: GPL-2.0-or-later
/* uvc.c - cámaras UVC 1.1 (clase vídeo USB) y la fuente de prueba (D277).
 *
 * Solo especificaciones públicas (USB Video Class 1.1, USB 2.0). La parte pura (descriptores,
 * PROBE/COMMIT, montaje de cuadros) está en uvc_nucleo.c, que prueban las pruebas del anfitrión.
 * Aquí está lo que toca el hardware y el kernel:
 *   1. al conectar (uvc_conectar) se lee el descriptor de configuración y se anota la cámara,
 *      sin encender nada (una cámara integrada no debe transmitir hasta que alguien la use);
 *   2. INFO_CAMARA (camara_info) la describe (modos YUY2 y MJPEG), la abre con CAM_ABRIR y
 *      entrega el último cuadro completo con CAM_CUADRO. Abrir = PROBE/COMMIT con el modo
 *      pedido, elegir la alternativa isócrona que cabe, abrir el flujo isócrono de entrada
 *      (usb_iso_*), SET_INTERFACE y encender;
 *   3. el reloj llama a uvc_tique (cada 10 ms): recoge los paquetes que ya llegaron
 *      (usb_iso_ler) y los pasa al montador, que junta los cuadros con la cabecera de carga
 *      útil (FID, EOF, ERR). Con la cámara cerrada no cuesta nada.
 * La fuente de prueba (fuente 2) es una cámara virtual: genera un patrón YUY2 y lo parte en
 * paquetes UVC que pasan por el mismo montador (y de vez en cuando pierde el último paquete,
 * para ver los cuadros descartados), de modo que la aplicación se prueba sin hardware.
 * Una sola cámara a la vez, de un solo proceso (el que la abrió; si ese proceso murió, la
 * puede tomar otro). Los servicios (de_servico) no la pueden usar. */
#include "usb.h"
#include "uvc_nucleo.h"
#include "abi.h"
#include "console.h"
#include "cpu.h"
#include "memoria.h"
#include "processo.h"
#include "relogio.h"
#include "sistema.h"
#include "texto.h"

#define UVC_CONFIG_MAX 4096
#define UVC_CASILLAS_ALTA 384u       /* múltiplo de 8 (el iTD reparte 8 microcuadros) */
#define UVC_CASILLAS_PLENA 128u
#define UVC_CUADRO_MAX (4u << 20)
#define SIM_PAQUETE 1020u            /* tamaño del paquete de la fuente de prueba (un FS típico) */

typedef struct {
    bool     presente;
    usb_disp_t *d;
    uvc_info_t info;
    char     nome[48];
    /* captura */
    bool     aberta, simulada;
    int      dono;                   /* pid del proceso que la abrió */
    int      h;                      /* flujo isócrono */
    uint8_t  modo;
    uint32_t ring_fis, ring_paginas;
    uint8_t *b0, *b1;
    uint32_t cap;
    uvc_montador_t m;
    uint8_t  formato;
    uint16_t larg, alt;
    uint8_t  alt_if;                 /* alternativa activada */
    /* brillo y contraste */
    bool     tem_ctl[2];
    /* fuente de prueba */
    uint8_t *sim_q;
    uint32_t sim_tam, sim_pos, sim_n, sim_cargo;   /* cuadro actual, posición, cuadros generados, bytes por tic */
    uint8_t  sim_fid;
} cam_t;

static cam_t cam;

/* ------------------------------------------------------------ conexión */

bool uvc_conectar(usb_disp_t *d)
{
    bool pinta = d->classe == 0x0E || (d->classe == 0xEF && d->subclasse == 2 && d->protocolo == 1);
    if (!pinta) return false;
    if (cam.presente) { klog("[UVC] ya hay una camara: se ignora la segunda\n"); return true; }
    uint8_t cab[9];
    if (usb_controle(d, 0x80, 6, 0x0200, 0, cab, 9) < 9) return true;
    uint16_t total = (uint16_t)(cab[2] | cab[3] << 8);
    if (total < 9 || total > UVC_CONFIG_MAX) { klog("[UVC] descriptor de configuracion de %u bytes: no se usa\n", total); return true; }
    uint8_t *b = kmalloc(total);
    if (!b) return true;
    int n = usb_controle(d, 0x80, 6, 0x0200, 0, b, total);
    uvc_info_t *info = kmalloc(sizeof *info);
    int formatos = info && n >= 9 ? uvc_analizar(b, n, info) : 0;
    kfree(b);
    if (!formatos) {
        if (info) kfree(info);
        klog("[UVC] %s %s: sin formatos YUY2 ni MJPEG con transporte isocrono (se ignora)\n", d->fabricante, d->produto);
        return true;
    }
    memset(&cam, 0, sizeof cam);
    cam.info = *info;
    kfree(info);
    cam.presente = true;
    cam.d = d;
    cam.h = -1;
    snformatar(cam.nome, sizeof cam.nome, "%s %s", d->fabricante, d->produto);
    d->camara = true;
    int modos = 0;
    for (int k = 0; k < cam.info.nf; k++) modos += cam.info.f[k].nq;
    klog("[UVC] %s: camara UVC %x.%02x, %d formato(s), %d tamano(s), %u alternativa(s) isocrona(s), %s\n", cam.nome,
         cam.info.bcd >> 8, cam.info.bcd & 0xFF, cam.info.nf, modos, cam.info.nalt,
         cam.info.pu_id ? "con unidad de procesamiento" : "sin unidad de procesamiento");
    return true;
}

static void cerrar_captura(void);

void uvc_desconectar(usb_disp_t *d)
{
    if (!cam.presente || cam.d != d) return;
    if (cam.aberta && !cam.simulada) cerrar_captura();
    cam.presente = false;
    cam.d = NULL;
    d->camara = false;
    klog("[UVC] camara retirada\n");
}

/* ------------------------------------------------------------ modos */

static uint8_t fps_de(uint32_t intervalo)
{
    if (!intervalo) return 0;
    uint32_t f = (10000000u + intervalo / 2) / intervalo;
    return (uint8_t)(f > 255 ? 255 : f);
}

/* Lista los modos de la fuente: el índice es el orden (formatos y, dentro, tamaños). */
static uint32_t listar_modos(bool prueba, info_camara_modo_t *o)
{
    uint32_t n = 0;
    memset(o, 0, CAM_MODOS_MAX * sizeof *o);
    if (prueba) {
        static const uint16_t t[2][2] = { { 320, 240 }, { 640, 480 } };
        for (int i = 0; i < 2; i++) { o[n].larg = t[i][0]; o[n].alt = t[i][1]; o[n].formato = CAM_FMT_YUY2; o[n].fps = 15; n++; }
        return n;
    }
    for (int k = 0; k < cam.info.nf && n < CAM_MODOS_MAX; k++)
        for (int j = 0; j < cam.info.f[k].nq && n < CAM_MODOS_MAX; j++) {
            const uvc_quadro_t *q = &cam.info.f[k].q[j];
            o[n].larg = q->larg;
            o[n].alt = q->alt;
            o[n].formato = cam.info.f[k].tipo == UVC_FMT_MJPEG ? CAM_FMT_MJPEG : CAM_FMT_YUY2;
            o[n].fps = fps_de(q->intervalo_def);
            n++;
        }
    return n;
}

/* ------------------------------------------------------------ captura real */

static int pedido_usb(void *ctx, uint8_t tipo, uint8_t req, uint16_t valor, uint16_t indice, void *dados, uint16_t n)
{
    return usb_controle((usb_disp_t *)ctx, tipo, req, valor, indice, dados, n);
}

static void liberar_buffers(void)
{
    if (cam.b0) kfree(cam.b0);
    if (cam.b1) kfree(cam.b1);
    cam.b0 = cam.b1 = NULL;
    if (cam.ring_fis) memfisica_liberar_seguidas(cam.ring_fis, cam.ring_paginas);
    cam.ring_fis = 0;
}

static void cerrar_captura(void)
{
    bool i0 = int_salva_desliga();
    cam.aberta = false;                  /* el tic ya no la toca */
    int_restaura(i0);
    if (cam.simulada) {
        if (cam.sim_q) kfree(cam.sim_q);
        cam.sim_q = NULL;
        if (cam.b0) kfree(cam.b0);
        if (cam.b1) kfree(cam.b1);
        cam.b0 = cam.b1 = NULL;
        cam.simulada = false;
        return;
    }
    if (cam.h >= 0 && cam.d) {
        usb_iso_parar(cam.d, cam.h);
        usb_controle(cam.d, 0x01, 11, 0, cam.info.iface_vs, NULL, 0);    /* alternativa 0: la cámara deja de transmitir */
        usb_iso_fechar(cam.d, cam.h);
    }
    cam.h = -1;
    liberar_buffers();
}

static int abrir_real(uint32_t modo)
{
    if (!cam.presente || !cam.d) return ERRO_NAO_EXISTE;
    uint32_t n = 0;
    const uvc_formato_t *f = NULL;
    const uvc_quadro_t *q = NULL;
    for (int k = 0; k < cam.info.nf && !q; k++)
        for (int j = 0; j < cam.info.f[k].nq; j++, n++)
            if (n == modo) { f = &cam.info.f[k]; q = &f->q[j]; break; }
    if (!q) return ERRO_INVALIDO;
    usb_disp_t *d = cam.d;
    uvc_ctl_t res;
    int tam_ctl = cam.info.bcd >= 0x0110 ? UVC_CTL_TAM_11 : UVC_CTL_TAM_10;
    int r = uvc_negociar(pedido_usb, d, cam.info.iface_vs, tam_ctl, f->idx, q->idx, q->intervalo_def, &res);
    if (r < 0 && tam_ctl == UVC_CTL_TAM_11)
        r = uvc_negociar(pedido_usb, d, cam.info.iface_vs, UVC_CTL_TAM_10, f->idx, q->idx, q->intervalo_def, &res);
    if (r < 0) { klog("[UVC] PROBE/COMMIT fallo (%d)\n", r); return ERRO_NAO_SUPORTADO; }
    const uvc_alt_t *a = uvc_elegir_alt(&cam.info, res.tam_payload);
    if (!a) return ERRO_NAO_SUPORTADO;
    uint32_t cap = res.tam_quadro > q->tam_max ? res.tam_quadro : q->tam_max;
    if (f->tipo == UVC_FMT_YUY2 && cap < (uint32_t)q->larg * q->alt * 2u) cap = (uint32_t)q->larg * q->alt * 2u;
    if (cap < 65536u) cap = 65536u;
    if (cap > UVC_CUADRO_MAX) return ERRO_NAO_SUPORTADO;
    cam.b0 = kmalloc(cap);
    cam.b1 = kmalloc(cap);
    if (!cam.b0 || !cam.b1) { liberar_buffers(); return ERRO_SEM_MEMORIA; }
    uint32_t slot = (uint32_t)a->mps * a->mult;
    uint32_t casillas = d->velocidade == USB_VEL_ALTA || d->velocidade >= USB_VEL_SUPER ? UVC_CASILLAS_ALTA : UVC_CASILLAS_PLENA;
    uint32_t pag = 0;
    for (;;) {                           /* si no hay memoria contigua para tanto anillo, uno más pequeño */
        pag = (casillas * slot + 4095) / 4096;
        cam.ring_fis = memfisica_alocar_seguidas(pag);
        if (cam.ring_fis || casillas <= 64) break;
        casillas /= 2;
        casillas -= casillas % 8;
    }
    if (!cam.ring_fis) { liberar_buffers(); return ERRO_SEM_MEMORIA; }
    cam.ring_paginas = pag;
    usb_iso_cfg_t cfg = { .ep = a->ep, .entrada = true, .mult = a->mult, .mps = a->mps, .intervalo = a->intervalo,
                          .anel_fis = cam.ring_fis, .anel_tam = casillas * slot };
    int h = usb_iso_abrir(d, &cfg);
    if (h < 0) { klog("[UVC] no se pudo abrir el flujo isocrono (%d)\n", h); liberar_buffers(); return ERRO_NAO_SUPORTADO; }
    if (usb_controle(d, 0x01, 11, a->alt, cam.info.iface_vs, NULL, 0) < 0) {
        klog("[UVC] SET_INTERFACE %u/%u fallo\n", cam.info.iface_vs, a->alt);
        usb_iso_fechar(d, h);
        liberar_buffers();
        return ERRO_NAO_SUPORTADO;
    }
    cam.h = h;
    cam.alt_if = a->alt;
    cam.formato = f->tipo == UVC_FMT_MJPEG ? CAM_FMT_MJPEG : CAM_FMT_YUY2;
    cam.larg = q->larg;
    cam.alt = q->alt;
    cam.cap = cap;
    cam.modo = (uint8_t)modo;
    uvc_montador_iniciar(&cam.m, cam.b0, cam.b1, cap, f->tipo, q->larg, q->alt);
    usb_iso_ligar(d, h);
    bool i0 = int_salva_desliga();
    cam.simulada = false;
    cam.aberta = true;
    int_restaura(i0);
    klog("[UVC] abierta: %ux%u %s, alternativa %u (%u x %u bytes), PROBE: %u bytes por cuadro, %u por intervalo\n", q->larg, q->alt,
         cam.formato == CAM_FMT_MJPEG ? "MJPEG" : "YUY2", a->alt, a->mps, a->mult, res.tam_quadro, res.tam_payload);
    return 0;
}

/* ------------------------------------------------------------ fuente de prueba */

static int abrir_prueba(uint32_t modo)
{
    info_camara_modo_t m[CAM_MODOS_MAX];
    uint32_t n = listar_modos(true, m);
    if (modo >= n) return ERRO_INVALIDO;
    uint32_t tam = (uint32_t)m[modo].larg * m[modo].alt * 2u;
    cam.b0 = kmalloc(tam);
    cam.b1 = kmalloc(tam);
    cam.sim_q = kmalloc(tam);
    if (!cam.b0 || !cam.b1 || !cam.sim_q) {
        if (cam.b0) kfree(cam.b0);
        if (cam.b1) kfree(cam.b1);
        if (cam.sim_q) kfree(cam.sim_q);
        cam.b0 = cam.b1 = cam.sim_q = NULL;
        return ERRO_SEM_MEMORIA;
    }
    cam.cap = tam;
    cam.formato = CAM_FMT_YUY2;
    cam.larg = m[modo].larg;
    cam.alt = m[modo].alt;
    cam.modo = (uint8_t)modo;
    cam.sim_tam = tam;
    cam.sim_pos = tam;                    /* sin cuadro en curso: el primer tic genera uno */
    cam.sim_n = 0;
    cam.sim_fid = 0;
    cam.sim_cargo = (tam * m[modo].fps) / TIQUES_POR_SEGUNDO + 1;   /* bytes por tic para los fps del modo */
    uvc_montador_iniciar(&cam.m, cam.b0, cam.b1, tam, UVC_FMT_YUY2, cam.larg, cam.alt);
    bool i0 = int_salva_desliga();
    cam.simulada = true;
    cam.aberta = true;
    int_restaura(i0);
    return 0;
}

/* Un tic de la fuente de prueba: unos pocos paquetes UVC; cuando se acabó el cuadro, el
 * siguiente tic genera otro (con el FID cambiado). */
static void tique_prueba(void)
{
    uint8_t pac[SIM_PAQUETE];
    if (cam.sim_pos >= cam.sim_tam) {
        uvc_gerar_padrao(cam.sim_q, cam.larg, cam.alt, cam.sim_n);
        cam.sim_pos = 0;
        cam.sim_fid ^= 1;
        cam.sim_n++;
    }
    uint32_t pendente = cam.sim_cargo;
    while (pendente && cam.sim_pos < cam.sim_tam) {
        uint32_t k = uvc_gerar_paquete(cam.sim_q, cam.sim_tam, &cam.sim_pos, cam.sim_fid, SIM_PAQUETE, pac);
        if (!k) break;
        /* cada quinto cuadro pierde su último paquete (lo recupera el cambio de FID y se descarta) */
        bool ultimo = cam.sim_pos >= cam.sim_tam;
        if (!(ultimo && cam.sim_n % 5 == 0)) uvc_montador_paquete(&cam.m, pac, k);
        pendente = pendente > k ? pendente - k : 0;
    }
}

/* ------------------------------------------------------------ el reloj */

void uvc_tique(void)
{
    if (!cam.aberta) return;
    if (cam.simulada) { tique_prueba(); return; }
    usb_iso_posicao(cam.d, cam.h);                   /* completa lo recibido y reabastece la cola */
    const uint8_t *ring = (const uint8_t *)cam.ring_fis;
    uint32_t off, len;
    bool erro;
    uint32_t tope = 1024;                            /* no más de esto por tic */
    while (tope-- && usb_iso_ler(cam.d, cam.h, &off, &len, &erro)) {
        if (erro) { cam.m.errores++; cam.m.malo = cam.m.len != 0 ? true : cam.m.malo; continue; }
        uvc_montador_paquete(&cam.m, ring + off, len);
    }
}

/* ------------------------------------------------------------ controles (brillo y contraste) */

static int ctl_pedido(uint8_t req, uint8_t sel, int16_t *v)
{
    if (!cam.presente || !cam.d || !cam.info.pu_id) return ERRO_NAO_SUPORTADO;
    uint8_t b[2] = { (uint8_t)*v, (uint8_t)(*v >> 8) };
    uint8_t tipo = (req & 0x80) ? 0xA1 : 0x21;
    int r = usb_controle(cam.d, tipo, req, (uint16_t)(sel << 8), (uint16_t)(cam.info.pu_id << 8 | cam.info.iface_vc), b, 2);
    if (r < 2) return ERRO_NAO_SUPORTADO;
    *v = (int16_t)(b[0] | b[1] << 8);
    return 0;
}

static int ajustar(info_camara_t *c)
{
    if (cam.simulada || !cam.presente) return ERRO_NAO_SUPORTADO;
    if (c->control > CAM_CTL_CONTRASTE || !(cam.info.pu_controles & (1u << c->control))) return ERRO_NAO_SUPORTADO;
    uint8_t sel = c->control == CAM_CTL_BRILLO ? 0x02 : 0x03;      /* PU_BRIGHTNESS_CONTROL / PU_CONTRAST_CONTROL */
    int16_t v = 0, mn = 0, mx = 0, def = 0;
    if (ctl_pedido(0x82, sel, &mn) < 0 || ctl_pedido(0x83, sel, &mx) < 0) return ERRO_NAO_SUPORTADO;
    ctl_pedido(0x87, sel, &def);
    if (c->escribir) {
        v = (int16_t)(c->valor < mn ? mn : c->valor > mx ? mx : c->valor);
        uint8_t b[2] = { (uint8_t)v, (uint8_t)(v >> 8) };
        if (usb_controle(cam.d, 0x21, 0x01, (uint16_t)(sel << 8), (uint16_t)(cam.info.pu_id << 8 | cam.info.iface_vc), b, 2) < 0)
            return ERRO_NAO_SUPORTADO;
    } else if (ctl_pedido(0x81, sel, &v) < 0) return ERRO_NAO_SUPORTADO;
    c->valor = v; c->minimo = mn; c->maximo = mx; c->defecto = def;
    return 0;
}

/* ------------------------------------------------------------ INFO_CAMARA */

static bool dono_vivo(void)
{
    processo_t *p = processo_por_pid(cam.dono);
    return p && p->estado != P_LIVRE && p->estado != P_ZUMBI;
}

int32_t camara_info(uint32_t ptr, uint32_t n)
{
    info_camara_t *c = (info_camara_t *)ptr;
    if (n < sizeof *c || !usuario_ok(c, sizeof *c, true)) return ERRO_ENDERECO;
    processo_t *eu = processo_recursos();
    if (eu->de_servico) return c->resultado = ERRO_ACESSO;
    int pid = eu->pid;
    int32_t r = ERRO_INVALIDO;
    switch (c->orden) {
    case CAM_ESTADO: {
        bool prueba = c->fuente == CAM_FUENTE_PRUEBA;
        c->presente = prueba ? 1 : cam.presente;
        c->abierta = cam.aberta && cam.simulada == prueba;
        c->n_modos = c->presente ? listar_modos(prueba, c->modos) : 0;
        txt_copia_n(c->nome, prueba ? "Fuente de prueba (patron)" : cam.presente ? cam.nome : "", sizeof c->nome);
        c->controles = !prueba && cam.presente && cam.info.pu_id ? (uint8_t)(cam.info.pu_controles & 3) : 0;
        if (cam.aberta && cam.simulada == prueba) {
            c->modo = cam.modo; c->seq = cam.m.seq; c->descartados = cam.m.descartados;
            c->paquetes = cam.m.paquetes; c->errores = cam.m.errores;
        }
        r = c->presente ? 0 : ERRO_NAO_EXISTE;
        break;
    }
    case CAM_ABRIR: {
        bool prueba = c->fuente == CAM_FUENTE_PRUEBA;
        if (cam.aberta) {
            if (cam.dono != pid && dono_vivo()) { r = ERRO_OCUPADO; break; }
            cerrar_captura();               /* la misma aplicación cambia de modo, o el dueño murió */
        }
        r = prueba ? abrir_prueba(c->modo) : abrir_real(c->modo);
        if (r == 0) { cam.dono = pid; c->larg = cam.larg; c->alt = cam.alt; c->formato = cam.formato; }
        break;
    }
    case CAM_CUADRO: {
        if (!cam.aberta || cam.dono != pid) { r = ERRO_NAO_EXISTE; break; }
        if (!c->buf || !usuario_ok(c->buf, c->buf_tam, true)) { r = ERRO_ENDERECO; break; }
        r = 0;
        for (int tentativa = 0; tentativa < 3; tentativa++) {
            uint32_t s0 = cam.m.seq;
            if (s0 == c->seq_visto || s0 == 0) { r = 0; break; }
            uint32_t len = cam.m.ult_len;
            if (len > c->buf_tam) { r = ERRO_SEM_MEMORIA; break; }
            memcpy(c->buf, uvc_montador_ultimo(&cam.m), len);
            if (cam.m.seq == s0) {          /* nadie cambió el búfer mientras se copiaba */
                c->seq = s0; c->larg = cam.larg; c->alt = cam.alt; c->formato = cam.formato;
                r = (int32_t)len;
                break;
            }
        }
        break;
    }
    case CAM_CERRAR:
        if (cam.aberta && cam.dono == pid) cerrar_captura();
        r = 0;
        break;
    case CAM_AJUSTE:
        r = cam.aberta && cam.dono != pid && dono_vivo() ? ERRO_OCUPADO : ajustar(c);
        break;
    }
    c->resultado = r;
    return r;
}

int uvc_relatorio(char *s, int max)
{
    if (!cam.presente) return 0;
    return snformatar(s, (size_t)max, "Camara UVC: %s, %s; %u cuadros, %u descartados, %u paquetes, %u errores\n", cam.nome,
                      cam.aberta ? (cam.simulada ? "capturando (prueba)" : "capturando") : "en reposo", cam.m.seq, cam.m.descartados,
                      cam.m.paquetes, cam.m.errores);
}
