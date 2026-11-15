// SPDX-License-Identifier: GPL-2.0-or-later
/* usbbt.c - transporte USB de la pila Bluetooth y llamada SIS_BLUETOOTH (D279, mejora 059).
 *
 * Transporte HCI sobre USB (Bluetooth Core, volumen 4 parte B), clase 0xE0/0x01/0x01 (controlador
 * Bluetooth inalámbrico, programación Bluetooth), sin firmware (CSR8510 y cualquier controlador que
 * arranque listo):
 *   - órdenes HCI: petición de clase al endpoint 0 (bmRequestType 0x20, bRequest 0, wIndex = interfaz);
 *   - eventos HCI: endpoint de interrupción de entrada de la interfaz 0 (un evento puede llegar en
 *     varios paquetes de mps bytes: se reensambla por su longitud);
 *   - datos ACL: endpoint bulk de entrada (recepción continua, como el Wi-Fi) y de salida.
 * Una tarea del kernel, «bluetooth», hace de bomba (cada tic, 10 ms) y es la única que toca la pila
 * de kernel/bluetooth/: las llamadas al sistema solo dejan peticiones en una cola. Los informes de los
 * teclados y ratones Bluetooth entran por hid.c, el mismo camino que los USB.
 * Sin recepción ACL (controladores UHCI/OHCI: no tienen recepción bulk continua) solo funcionan las
 * órdenes y los eventos: se puede buscar pero no emparejar ni conectar.
 * Las claves de enlace se guardan en /etc/bluetooth.conf y, si está montada, en SAGUI-CONF (D255). */
#include "usb.h"
#include "abi.h"
#include "bluetooth.h"
#include "conf_sistema.h"
#include "console.h"
#include "cpu.h"
#include "memoria.h"
#include "montagem.h"
#include "processo.h"
#include "relogio.h"
#include "sistema.h"
#include "texto.h"
#include "trava.h"
#include "vfs.h"
#include "xhci.h"
#include "../bluetooth/bt.h"

#define CONF_RAM   "/etc/bluetooth.conf"
#define CONF_DISCO CONF_PONTO "/etc/bluetooth.conf"
#define RX_TAM     16384u
#define FILA_MAX   8

_Static_assert(BT_PRONTO == BTA_PRONTO && BT_APAGADO == BTA_APAGADO && BT_FALHOU == BTA_FALHOU, "ABI de estados Bluetooth");
_Static_assert(BT_D_CONECTADO == BTD_CONECTADO && BT_D_ERRO == BTD_ERRO && BT_T_COMBO == BTT_COMBO, "ABI de dispositivos Bluetooth");

static usb_disp_t *dev;            /* el controlador atendido */
static usb_disp_t *dev_novo;       /* recién enchufado: la tarea lo abre */
static bool retirado;              /* se retiró: la tarea limpia */
static bool tarea_creada, gestor_listo;
static int  int_h = -1, rx_h = -1;
static uint32_t rx_buf;            /* buffer físico de recepción ACL (16 KB alineado a 16 KB) */
static bool sin_acl;
static const char canal = 0;

static uint8_t ev_buf[272];
static int ev_n, ev_esperado;
static uint32_t ev_t;
static uint32_t errores_int, errores_acl;
static uint8_t tx[1100] ALINHADO(4);
static uint8_t rx[1100] ALINHADO(4);

typedef struct { int op, arg; } pedido_t;
static pedido_t fila[FILA_MAX];
static int f_cab, f_cnt;

/* ------------------------------------------------------------ plataforma de la pila */

static int p_cmd(const uint8_t *p, int n)
{
    if (!dev || n > (int)sizeof tx) return -1;
    memcpy(tx, p, (size_t)n);
    return usb_controle(dev, 0x20, 0, 0, 0, tx, (uint16_t)n) < 0 ? -1 : 0;
}

static int p_acl(const uint8_t *p, int n)
{
    if (!dev || n > (int)sizeof tx) return -1;
    memcpy(tx, p, (size_t)n);
    return usb_bulk(dev, false, tx, (uint32_t)n) < 0 ? -1 : 0;
}

static uint32_t p_ms(void) { return relogio_tiques() * (1000u / TIQUES_POR_SEGUNDO); }

static void p_registro(const char *t) { klog("%s\n", t); }

/* Un archivo en un volumen montado, creándolo (modelo de area_trabalho.c). */
static int escrever_montado(const char *caminho, const char *rel_pasta, const void *dados, uint32_t n)
{
    const char *rd;
    montagem_t *m = montagem_de(caminho, &rd);
    if (!m) return ERRO_NAO_EXISTE;
    if (m->opcoes & MONTAR_SO_LEITURA) return ERRO_SO_LEITURA;
    arq_montado_t *a = kcalloc(sizeof(arq_montado_t) + m->fs->tam_arq);
    if (!a) return ERRO_SEM_MEMORIA;
    trava_pegar(&trava_fs);
    if (rel_pasta && m->fs->criar_pasta) m->fs->criar_pasta(m->priv, rel_pasta);
    uint32_t tam = 0;
    int r = m->fs->abrir(m->priv, rd, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR, a->dados, &tam);
    if (r >= 0) {
        r = m->fs->escrever(m->priv, a->dados, 0, dados, n, &tam);
        if (r >= 0 && m->fs->sincronizar) m->fs->sincronizar(m->priv, a->dados);
        m->fs->fechar(m->priv, a->dados);
    }
    trava_soltar(&trava_fs);
    kfree(a);
    return r;
}

static void p_guardar(const char *t, int n)
{
    no_t *x = vfs_achar(CONF_RAM);
    if (x) vfs_truncar(x);
    else if (vfs_criar(CONF_RAM, TIPO_ARQUIVO, &x) < 0) x = NULL;
    if (x && n) vfs_escrever(x, 0, t, (uint32_t)n);
    if (conf_sistema_montada()) {
        int r = escrever_montado(CONF_DISCO, "etc", t, (uint32_t)n);
        if (r < 0) klog("[BT] no se pudieron guardar las claves en SAGUI-CONF (%d)\n", r);
    } else {
        klog("[BT] sin SAGUI-CONF: las claves solo duran hasta apagar\n");
    }
}

static int p_hid_conectar(int tipo, const uint8_t *d, int n) { return hid_bt_conectar(tipo, d, n); }
static void p_teclado(const uint8_t *r, int n) { hid_bt_teclado(r, n); }
static void p_raton(const uint8_t *r, int n, bool boot) { hid_bt_raton(r, n, boot); }
static void p_soltar(int tipo) { hid_bt_soltar(tipo); }

static const bt_plat_t plat = {
    .enviar_cmd = p_cmd, .enviar_acl = p_acl, .ms = p_ms, .guardar = p_guardar,
    .hid_conectar = p_hid_conectar, .teclado = p_teclado, .raton = p_raton, .soltar_entrada = p_soltar,
    .registro = p_registro,
};

/* ------------------------------------------------------------ transporte */

static int ler_int(uint8_t *b, int max)
{
    if (dev->tipo_ctl == USB_XHCI) return xhci_int_ler(dev, dev->ep_int, b, max);
    return usb_int_ler(dev, int_h, b, max);
}

/* Un paquete del endpoint de eventos: reensambla por la longitud que dice el segundo byte. */
static void evento_paquete(const uint8_t *b, int n)
{
    uint32_t ahora = relogio_tiques();
    if (ev_n && ahora - ev_t > 20) ev_n = 0;   /* un resto de un evento que nunca acabó */
    ev_t = ahora;
    for (int i = 0; i < n; i++) {
        if (ev_n >= (int)sizeof ev_buf) { ev_n = 0; return; }
        ev_buf[ev_n++] = b[i];
        if (ev_n == 2) ev_esperado = 2 + ev_buf[1];
        if (ev_n >= 2 && ev_n >= ev_esperado) {
            bt_hci_evento(ev_buf, ev_esperado);
            ev_n = 0;
            if (i + 1 < n) { evento_paquete(b + i + 1, n - i - 1); return; }   /* otro evento pegado */
        }
    }
}

static void abrir_dispositivo(usb_disp_t *d)
{
    ev_n = 0;
    if (!d->ep_int) { klog("[BT] el controlador no tiene endpoint de eventos\n"); return; }
    if (d->tipo_ctl == USB_XHCI) {
        if (xhci_int_armar(d, d->ep_int, d->mps_int > 64 ? 64 : d->mps_int) < 0) { klog("[BT] no se pudo armar el endpoint de eventos\n"); return; }
    } else {
        int_h = usb_int_abrir(d, d->ep_int, d->mps_int);
        if (int_h < 0) { klog("[BT] sin sitio en la lista periodica para los eventos\n"); return; }
    }
    sin_acl = false;
    if (!d->ep_entrada || !d->ep_saida) { klog("[BT] sin endpoints bulk\n"); sin_acl = true; }
    else {
        if (!rx_buf) {
            uint32_t b = memfisica_alocar_seguidas(8);   /* dentro, 16 KB alineados a 16 KB: no cruzan 64 KB */
            if (b) rx_buf = (b + 0x3FFFu) & ~0x3FFFu;
        }
        rx_h = rx_buf ? usb_rx_abrir(d, rx_buf, RX_TAM) : USB_ERRO;
        if (rx_h < 0) {
            sin_acl = true;
            klog("[BT] sin recepcion ACL en este controlador USB (%s): solo se podra buscar\n", usb_nome_ctl(d->tipo_ctl));
        }
    }
    dev = d;
    d->bt = true;
    if (!gestor_listo) {
        gestor_listo = true;
        bt_g_iniciar();
        uint8_t *txt;
        uint32_t tam;
        if (vfs_existe_arquivo(CONF_RAM) && vfs_ler_inteiro(CONF_RAM, &txt, &tam) >= 0) {
            bt_g_carregar((const char *)txt, (int)(tam > BT_CONF_MAX ? BT_CONF_MAX : tam));
            kfree(txt);
        }
    }
    klog("[BT] controlador %s %s (%04x:%04x) por %s: iniciando\n", d->fabricante, d->produto, d->id_fabricante,
         d->id_produto, usb_nome_ctl(d->tipo_ctl));
    bt_hci_iniciar(&plat);
}

static void cerrar_dispositivo(void)
{
    for (int i = 0; i < bt_n_disp(); i++) {
        const bt_disp_t *x = bt_disp(i);
        if (x && x->estado == BT_D_CONECTADO) hid_bt_soltar(x->tipo);
    }
    bt_g_adaptador_retirado();
    if (dev) {
        if (dev->tipo_ctl != USB_XHCI && int_h >= 0) usb_int_fechar(dev, int_h);
        if (rx_h >= 0 && dev->tipo_ctl == USB_EHCI) usb_rx_fechar(dev, rx_h);
    }
    int_h = rx_h = -1;
    dev = NULL;
    f_cnt = f_cab = 0;
}

static void atender_pedidos(void)
{
    while (f_cnt) {
        pedido_t p = fila[f_cab];
        f_cab = (f_cab + 1) % FILA_MAX;
        f_cnt--;
        switch (p.op) {
        case BT_BUSCAR: bt_buscar(); break;
        case BT_EMPAREJAR: bt_conectar(p.arg, true); break;
        case BT_CONECTAR: bt_conectar(p.arg, false); break;
        case BT_DESCONECTAR: bt_desconectar(p.arg); break;
        case BT_OLVIDAR: bt_esquecer(p.arg); break;
        case BT_CONFIRMAR: bt_confirmar(p.arg != 0); break;
        case BT_ENERGIA: bt_energia(p.arg != 0); break;
        }
    }
}

static void tarea_bt(void *arg)
{
    (void)arg;
    for (;;) {
        if (retirado) { retirado = false; cerrar_dispositivo(); }
        if (dev_novo) {
            usb_disp_t *d = dev_novo;
            dev_novo = NULL;
            if (d->usado) abrir_dispositivo(d);
        }
        if (dev) {
            uint8_t b[64];
            for (int k = 0; k < 8; k++) {
                int n = ler_int(b, sizeof b);
                if (n < 0) { if (++errores_int == 50) klog("[BT] el controlador no responde (eventos)\n"); break; }
                if (n == 0) break;
                errores_int = 0;
                evento_paquete(b, n);
            }
            if (!sin_acl) {
                for (int k = 0; k < 8; k++) {
                    int n = usb_rx_ler(dev, rx_h, rx, sizeof rx);
                    if (n < 0) { if (++errores_acl == 50) klog("[BT] el controlador no responde (ACL)\n"); break; }
                    if (n == 0) break;
                    errores_acl = 0;
                    bt_hci_acl_entrada(rx, n);
                }
            }
            atender_pedidos();
            bt_hci_tic();
            bt_l2cap_tic();
            bt_g_tic();
            hid_bt_tic();
            uint8_t l;
            if (hid_bt_leds(&l))
                for (int i = 0; i < bt_n_disp(); i++) {
                    const bt_disp_t *x = bt_disp(i);
                    if (x && x->estado == BT_D_CONECTADO && x->canal_int >= 0 && (x->tipo == BT_T_TECLADO || x->tipo == BT_T_COMBO))
                        bt_hid_leds(x->canal_int, l);
                }
        }
        int_desliga();
        if (dev || dev_novo) processo_atual()->acordar_em = relogio_tiques() + 1;
        bloquear(&canal);
        ceder();
        int_liga();
    }
}

bool bt_usb_reconhecer(usb_disp_t *d)
{
    if (d->classe != 0xE0 || d->subclasse != 0x01 || d->protocolo != 0x01) return false;
    klog("[BT] controlador Bluetooth USB: %s %s (%04x:%04x)\n", d->fabricante, d->produto, d->id_fabricante, d->id_produto);
    if (dev || dev_novo) { klog("[BT] ya hay un controlador: este no se usa\n"); return true; }
    dev_novo = d;
    d->bt = true;
    if (!tarea_creada) { tarea_creada = true; tarefa_kernel("bluetooth", tarea_bt, NULL); }
    acordar(&canal);
    return true;
}

void bt_usb_retirado(usb_disp_t *d)
{
    if (d == dev_novo) { dev_novo = NULL; return; }
    if (d != dev) return;
    klog("[BT] controlador retirado\n");
    retirado = true;
    dev = NULL;   /* nada más toca el USB; la tarea limpia el resto */
    acordar(&canal);
}

int bt_relatorio(char *s, int max)
{
    bt_info_t i;
    if (!dev && !gestor_listo) return 0;
    bt_g_info(&i);
    int k = snformatar(s, (size_t)max, "Bluetooth: %s, %u ordenes/eventos, %u ACL recibidos, %u ACL enviados%s\n",
                       i.estado == BT_PRONTO ? "listo" : i.estado == BT_APAGADO ? "apagado" : i.estado == BT_INICIANDO ? "iniciando" :
                       i.estado == BT_FALHOU ? "con fallo" : "sin controlador", i.eventos, i.acl_rx, i.acl_tx,
                       sin_acl && dev ? " (sin recepcion ACL)" : "");
    for (int j = 0; j < bt_n_disp() && k < max - 80; j++) {
        const bt_disp_t *x = bt_disp(j);
        if (x->estado == BT_D_CONECTADO)
            k += snformatar(s + k, (size_t)(max - k), "Bluetooth: %s conectado\n", x->nome);
    }
    return k;
}

/* ------------------------------------------------------------ SIS_BLUETOOTH */

static info_bt_t v_info;
static disp_bt_t v_disp[BT_MAX_DISP];

static void llenar_info(void)
{
    bt_info_t i;
    memset(&v_info, 0, sizeof v_info);
    bt_g_info(&i);
    v_info.estado = dev || dev_novo || gestor_listo ? i.estado : BTA_SEM_ADAPTADOR;
    if (!dev && !dev_novo) v_info.estado = BTA_SEM_ADAPTADOR;
    if (dev)
        snformatar(v_info.chip, sizeof v_info.chip, "%s (%04x:%04x)", dev->produto, dev->id_fabricante, dev->id_produto);
    for (int k = 0; k < 6; k++) v_info.addr[k] = i.addr[5 - k];
    v_info.buscando = i.buscando;
    v_info.energia = i.energia;
    v_info.conf_modo = i.conf_modo;
    v_info.conf_disp = i.conf_disp;
    v_info.n_disp = (unsigned char)bt_n_disp();
    v_info.conf_num = i.conf_num;
    v_info.hci_ver = i.hci_ver;
    v_info.lmp_ver = i.lmp_ver;
    v_info.fabricante = i.fabricante;
    v_info.acl_mtu = i.acl_mtu;
    v_info.acl_num = i.acl_num;
    v_info.eventos = i.eventos;
    v_info.acl_rx = i.acl_rx;
    v_info.acl_tx = i.acl_tx;
    v_info.erros_hci = i.erros_hci;
    if (dev && sin_acl && !i.erro[0]) bt_copia(v_info.erro, "este controlador USB no recibe datos ACL: solo se puede buscar", sizeof v_info.erro);
    else bt_copia(v_info.erro, i.erro, sizeof v_info.erro);
}

static int encolar(int op, int arg)
{
    if (f_cnt >= FILA_MAX) return ERRO_OCUPADO;
    fila[(f_cab + f_cnt++) % FILA_MAX] = (pedido_t){ op, arg };
    acordar(&canal);
    return 0;
}

int32_t sis_bluetooth(uint32_t op, uint32_t a, uint32_t b, uint32_t c, uint32_t d_)
{
    (void)c; (void)d_;
    switch (op) {
    case BT_ESTADO: {
        uint32_t tam = (b && b < sizeof(info_bt_t)) ? b : sizeof(info_bt_t);
        if (!usuario_ok((void *)a, tam, true)) return ERRO_ENDERECO;
        bool i0 = int_salva_desliga();
        llenar_info();
        memcpy((void *)a, &v_info, tam);
        int_restaura(i0);
        return dev || dev_novo ? 0 : ERRO_NAO_EXISTE;
    }
    case BT_DISPOSITIVOS: {
        uint32_t n = b > BT_MAX_DISP ? BT_MAX_DISP : b;
        if (!usuario_ok((void *)a, n * sizeof(disp_bt_t), true)) return ERRO_ENDERECO;
        bool i0 = int_salva_desliga();
        uint32_t k = 0;
        for (; k < n && (int)k < bt_n_disp(); k++) {
            const bt_disp_t *x = bt_disp((int)k);
            disp_bt_t *o = &v_disp[k];
            memset(o, 0, sizeof *o);
            for (int j = 0; j < 6; j++) o->addr[j] = x->addr[5 - j];
            o->estado = x->estado;
            o->tipo = x->tipo;
            o->pareado = x->tem_chave;
            o->visto = x->visto;
            o->rssi = x->rssi;
            o->clase = x->clase;
            bt_copia(o->nome, x->nome, sizeof o->nome);
            bt_copia(o->erro, x->erro, sizeof o->erro);
        }
        memcpy((void *)a, v_disp, k * sizeof(disp_bt_t));
        int_restaura(i0);
        return (int32_t)k;
    }
    case BT_BUSCAR: case BT_EMPAREJAR: case BT_CONECTAR: case BT_DESCONECTAR: case BT_OLVIDAR: case BT_CONFIRMAR: case BT_ENERGIA:
        break;
    default:
        return ERRO_INVALIDO;
    }
    if (!dev) return ERRO_NAO_EXISTE;
    bool i0 = int_salva_desliga();
    bt_info_t i;
    bt_g_info(&i);
    int r = 0;
    if (i.estado != BT_PRONTO && i.estado != BT_APAGADO) r = ERRO_OCUPADO;
    else if (op == BT_ENERGIA) r = encolar(BT_ENERGIA, (int)a != 0);
    else if (i.estado == BT_APAGADO) r = ERRO_OCUPADO;
    else if (op == BT_BUSCAR || op == BT_CONFIRMAR) r = encolar((int)op, (int)a);
    else if ((int)a < 0 || (int)a >= bt_n_disp()) r = ERRO_INVALIDO;
    else r = encolar((int)op, (int)a);
    int_restaura(i0);
    return r;
}
