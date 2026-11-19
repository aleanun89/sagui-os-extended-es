// SPDX-License-Identifier: GPL-2.0-or-later
/* toshiba.c - la interfaz HCI de los portátiles Toshiba (D403).
 *
 * Escrito para SaguiOS a partir del AML de los Satellite (L630/L635, L655,
 * L850: dispositivo \_SB.VALZ, TOS1900); los números de registro y los códigos
 * de las teclas son los que usa el driver toshiba_acpi de Linux
 * (GPL-2.0-or-later), que sirvió de referencia del protocolo; no hay código
 * copiado de él.
 *
 * El dispositivo (TOS1900; en los más viejos TOS6200, TOS6207, TOS6208) tiene
 * un método de seis enteros, GHCI o SPFC, que es la «HCI»:
 *   entrada  { 0xF300 leer | 0xFF00 escribir, registro, valor, extra, 0, 0 }
 *   salida   paquete de seis: [0] resultado (0 bien, 0x8000 no está, 0x8C00
 *            cola vacía...), [2] el valor leído.
 * Registros que se usan: 0x001E activar las teclas (0x09), 0x0016 la cola de
 * eventos (si no hay INFO), 0x002A el brillo (bits 13-15: 8 niveles), 0x0004
 * el ventilador (solo se lee), 0x0056 la radio (extra 1: bit 0 = el
 * interruptor encendido; solo se lee).
 * Las teclas: ENAB y HCI 0x001E = 0x09 (como Linux); después, cada tecla deja
 * un código en la cola del firmware y avisa con Notify(0x80); se saca con INFO
 * (0 = vacía) o con la HCI 0x0016. Al soltar llega el mismo código | 0x80 y se
 * ignora. 0x140/0x141 brillo, 0x101 silencio, 0x142 la radio, 0x13F pantalla.
 *
 * Ojo (visto en el AML del L850): con las teclas activadas el firmware deja de
 * cambiar él la radio con Fn+F8 y de avisar al ACPI Video con el brillo: todo
 * llega aquí y lo hace SaguiOS (el brillo por el ACPI Video si lo hay). */
#include "uacpi_sagui.h"
#include "acoes.h"
#include "brilho_conv.h"
#include "console.h"
#include "relogio.h"
#include "texto.h"

#define HCI_LER        0xF300
#define HCI_ESCREVER   0xFF00
#define TOS_BEM        0x0000
#define TOS_BEM2       0x0001
#define TOS_FILA_VAZIA 0x8C00
#define TOS_NAO_EXISTE 0x8000
#define HCI_TECLAS     0x001E
#define HCI_TECLAS_ON  0x0009
#define HCI_EVENTO     0x0016
#define HCI_BRILHO     0x002A
#define HCI_VENTILADOR 0x0004
#define HCI_RADIO      0x0056
#define BRILHO_DESL    13
#define BRILHO_NIVEIS  8
#define PERIODO        (10 * TIQUES_POR_SEGUNDO)

static uacpi_namespace_node *dev;
static const char *metodo;            /* "GHCI" o "SPFC" */
static int teclas;                    /* 0 no; 1 por INFO; 2 por HCI_EVENTO */
static int ventilador = -1, radio = -1;
static bool tem_brilho;
static uint32_t ultima_leitura;
static char nome[40];

/* Una llamada a la HCI: devuelve el resultado (out[0]) y deja out[2] en *v. */
static uint32_t hci(uint32_t op, uint32_t reg, uint32_t valor, uint32_t extra, uint32_t *v)
{
    uint64_t in[6] = { op, reg, valor, extra, 0, 0 };
    uacpi_object *o = NULL;
    uacpi_object_array a;
    uint32_t r = 0xFFFF;
    if (portatil_eval_ints(dev, metodo, in, 6, &o) == UACPI_STATUS_OK && o &&
        uacpi_object_get_package(o, &a) == UACPI_STATUS_OK && a.count >= 3) {
        uacpi_u64 x;
        if (uacpi_object_get_integer(a.objects[0], &x) == UACPI_STATUS_OK) r = (uint32_t)x;
        if (v && uacpi_object_get_integer(a.objects[2], &x) == UACPI_STATUS_OK) *v = (uint32_t)x;
    }
    if (o) uacpi_object_unref(o);
    return r;
}

static bool bem(uint32_t r) { return r == TOS_BEM || r == TOS_BEM2; }

/* ------------------------------------------------------------ brillo (si no hay ACPI Video) */

static int b_ler(void)
{
    uint32_t v = 0;
    if (!bem(hci(HCI_LER, HCI_BRILHO, 0, 0, &v))) return -1;
    return (int)((v >> BRILHO_DESL) & (BRILHO_NIVEIS - 1));
}

static bool b_fixar(int n)
{
    if (n < 0 || n >= BRILHO_NIVEIS) return false;
    return bem(hci(HCI_ESCREVER, HCI_BRILHO, (uint32_t)n << BRILHO_DESL, 0, NULL));
}

static int b_pct(int n) { return brilho_pct_linear(n, BRILHO_NIVEIS); }
static uint32_t b_valor(int n) { return (uint32_t)n; }

/* ------------------------------------------------------------ teclas */

static void tecla(uint32_t c)
{
    if (c == 0x100 || (c & 0x80)) return;   /* soltar (o la propia Fn) */
    switch (c) {
    case 0x101: acao_sistema(ACAO_SILENCIO, ORIGEM_ACAO_TOSHIBA); break;
    case 0x140: acao_sistema(ACAO_BRILHO_MENOS, ORIGEM_ACAO_TOSHIBA); break;
    case 0x141: acao_sistema(ACAO_BRILHO_MAIS, ORIGEM_ACAO_TOSHIBA); break;
    case 0x142:
    case 0x09E: acao_sistema(ACAO_RADIO, ORIGEM_ACAO_TOSHIBA); break;
    case 0x13F: acao_sistema(ACAO_TELA, ORIGEM_ACAO_TOSHIBA); break;
    default: klog("[TOSHIBA] tecla %x sin accion\n", c); break;
    }
}

void toshiba_notificar(uint32_t v)
{
    portatil_evento(PORTATIL_ALVO_TOSHIBA, v);
    if (!dev) return;
    if (v != 0x80) { klog("[TOSHIBA] aviso %x desconocido\n", v); return; }
    if (teclas == 1) {
        /* Una por aviso: el firmware vuelve a avisar si quedan (GETE del L850). */
        uint64_t c;
        if (portatil_eval_int(dev, "INFO", NULL, 0, &c) == UACPI_STATUS_OK && c) {
            klog("[TOSHIBA] tecla %x\n", (uint32_t)c);
            tecla((uint32_t)c);
        }
    } else if (teclas == 2) {
        for (int i = 0; i < 8; i++) {
            uint32_t c = 0, r = hci(HCI_LER, HCI_EVENTO, 0, 0, &c);
            if (bem(r)) { klog("[TOSHIBA] tecla %x\n", c); tecla(c); continue; }
            if (r == TOS_NAO_EXISTE) hci(HCI_ESCREVER, HCI_EVENTO, 1, 0, NULL);   /* algunos la apagan solos */
            break;
        }
    }
}

static uacpi_status notificacao(uacpi_handle ctx, uacpi_namespace_node *no, uacpi_u64 v)
{
    (void)ctx; (void)no;
    toshiba_notificar((uint32_t)v);
    return UACPI_STATUS_OK;
}

/* ------------------------------------------------------------ búsqueda */

static uacpi_iteration_decision achar(void *u, uacpi_namespace_node *no, uacpi_u32 prof)
{
    (void)u; (void)prof;
    static const char *const ids[] = { "TOS1900", "TOS6200", "TOS6207", "TOS6208", NULL };
    if (!uacpi_device_matches_pnp_id(no, ids)) return UACPI_ITERATION_DECISION_CONTINUE;
    uint32_t sta = 0xF;
    if (uacpi_eval_sta(no, &sta) == UACPI_STATUS_OK && !(sta & 1)) return UACPI_ITERATION_DECISION_CONTINUE;
    const char *m = portatil_tem_metodo(no, "GHCI") ? "GHCI" : portatil_tem_metodo(no, "SPFC") ? "SPFC" : NULL;
    if (!m) return UACPI_ITERATION_DECISION_CONTINUE;
    dev = no;
    metodo = m;
    return UACPI_ITERATION_DECISION_BREAK;
}

static void ler_estado(void)
{
    uint32_t v = 0;
    ventilador = bem(hci(HCI_LER, HCI_VENTILADOR, 0, 0, &v)) ? (int)v : -1;
    v = 0;
    radio = bem(hci(HCI_LER, HCI_RADIO, 0, 1, &v)) ? (int)(v & 1) : -1;
    ultima_leitura = relogio_tiques();
}

void toshiba_iniciar(void)
{
    uacpi_namespace_for_each_child(uacpi_namespace_root(), achar, NULL, UACPI_OBJECT_DEVICE_BIT,
                                   UACPI_MAX_DEPTH_ANY, NULL);
    if (!dev) {
        klog("[TOSHIBA] no hay interfaz HCI de Toshiba (TOS1900/TOS620x)\n");
        return;
    }
    portatil_nome(dev, nome, sizeof nome);
    /* Las teclas: INFO si existe; si no, la cola por la HCI (si se deja activar). */
    if (portatil_tem_metodo(dev, "INFO")) teclas = 1;
    else if (bem(hci(HCI_ESCREVER, HCI_EVENTO, 1, 0, NULL))) teclas = 2;
    /* El manejador antes de ENAB: algunos firmwares avisan ya al activarlas. */
    if (teclas) uacpi_install_notify_handler(dev, notificacao, NULL);
    if (teclas) {
        uacpi_object *o = NULL;
        uacpi_status r = uacpi_eval(dev, "ENAB", NULL, &o);
        if (o) uacpi_object_unref(o);
        uint32_t h = r == UACPI_STATUS_OK ? hci(HCI_ESCREVER, HCI_TECLAS, HCI_TECLAS_ON, 0, NULL) : 0xFFFF;
        if (r != UACPI_STATUS_OK || !bem(h)) {
            klog("[TOSHIBA] no se pudieron activar las teclas (ENAB %s, HCI %x)\n", uacpi_status_to_string(r), h);
            teclas = 0;
        }
    }
    tem_brilho = b_ler() >= 0;
    ler_estado();
    klog("[TOSHIBA] %s por %s: teclas %s, brillo HCI %s, ventilador %d, radio %d\n", nome, metodo,
         teclas == 1 ? "por INFO" : teclas == 2 ? "por la cola HCI" : "no", tem_brilho ? "si" : "no", ventilador, radio);
    if (tem_brilho) {
        brilho_drv_t d = { PORTATIL_FONTE_TOSHIBA, BRILHO_NIVEIS, b_ler, b_fixar, b_pct, b_valor, nome };
        portatil_registrar_brilho(&d);
    }
}

void toshiba_ciclo(void)
{
    if (dev && relogio_tiques() - ultima_leitura >= PERIODO) ler_estado();
}

void toshiba_info(info_portatil_t *p)
{
    p->toshiba = dev ? 1 : 0;
    p->toshiba_teclas = (uint32_t)teclas;
    p->toshiba_ventilador = ventilador;
    p->toshiba_radio = radio;
    txt_copia_n(p->toshiba_no, dev ? nome : "", sizeof p->toshiba_no);
}
