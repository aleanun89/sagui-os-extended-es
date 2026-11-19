// SPDX-License-Identifier: GPL-3.0-only
/* usb.c - dispositivos USB: enumeración, hubs y conexión en caliente (H30, D214).
 *
 * Enumerar un dispositivo nuevo, como manda la especificación USB 2.0:
 *   1. reiniciar el puerto (del controlador o del hub) donde apareció;
 *   2. en la dirección 0, leer los 8 primeros bytes del descriptor de dispositivo
 *      para saber el tamaño de paquete del endpoint 0;
 *   3. darle una dirección propia (SET_ADDRESS);
 *   4. leer el descriptor de dispositivo completo, los textos (fabricante y
 *      producto) y el descriptor de configuración con las interfaces y endpoints;
 *   5. elegir la configuración (SET_CONFIGURATION).
 * Si es un hub (clase 9), enciende sus puertos y enumera lo que haya en
 * cada uno. Si es almacenamiento (clase 8, SCSI, Bulk-Only), pasa a
 * armazenamento.c; si tiene interfaces HID (teclado, ratón), a hid.c.
 *
 * D214: lo mismo vale para el xHCI (USB 3.0), con dos diferencias que pone
 * xhci.c: la dirección la da el controlador (Enable Slot + Address Device, en
 * vez de SET_ADDRESS) y los endpoints se le anuncian (Configure Endpoint)
 * antes de SET_CONFIGURATION. Por el xHCI hay dispositivos de todas las
 * velocidades; por el EHCI, los de baja y plena velocidad solo detrás de un hub
 * de alta (su "transaction translator").
 *
 * Conexión y remoción en caliente: una tarea del kernel consulta, dos veces
 * por segundo, el bit "la conexión cambió" de los puertos de los controladores y de cada
 * hub (petición GET_STATUS de puerto). Ver D025. D227: cada puerto tiene su estado
 * y una conexión que no se reconoce a la primera se reintenta (hasta 10 veces, con
 * un reset nuevo cada vez); la tarea trabaja con las interrupciones encendidas y
 * sus esperas duermen, y el xHCI la despierta cuando cambia un puerto. */
#include "usb.h"
#include "bootinfo.h"
#include "console.h"
#include "cpu.h"
#include "processo.h"
#include "relogio.h"
#include "texto.h"
#include "xhci.h"
#include "wifi.h"

static usb_disp_t disps[USB_MAX_DISP];

usb_disp_t *usb_disp(int i) { return (i >= 0 && i < USB_MAX_DISP && disps[i].usado) ? &disps[i] : NULL; }

int usb_controle(usb_disp_t *d, uint8_t tipo, uint8_t pedido, uint16_t valor, uint16_t indice, void *dados, uint16_t tam)
{
    uint8_t setup[8] = { tipo, pedido, (uint8_t)valor, (uint8_t)(valor >> 8),
                         (uint8_t)indice, (uint8_t)(indice >> 8), (uint8_t)tam, (uint8_t)(tam >> 8) };
    if (d->tipo_ctl == USB_XHCI) return xhci_controle(d, setup, dados, tam, (tipo & 0x80) != 0);
    if (d->tipo_ctl == USB_UHCI) return uhci_controle(d, setup, dados, tam, (tipo & 0x80) != 0);
    if (d->tipo_ctl == USB_OHCI) return ohci_controle(d, setup, dados, tam, (tipo & 0x80) != 0);
    return ehci_controle(d, setup, dados, tam, (tipo & 0x80) != 0);
}

int usb_bulk(usb_disp_t *d, bool entrada, void *dados, uint32_t tam)
{
    if (d->tipo_ctl == USB_XHCI) return xhci_bulk(d, entrada, dados, tam);
    if (d->tipo_ctl == USB_UHCI)
        return uhci_bulk(d, entrada ? d->ep_entrada : d->ep_saida, entrada ? d->mps_entrada : d->mps_saida, entrada,
                         entrada ? &d->alterna_entrada : &d->alterna_saida, dados, tam);
    if (d->tipo_ctl == USB_OHCI)
        return ohci_bulk(d, entrada ? d->ep_entrada : d->ep_saida, entrada ? d->mps_entrada : d->mps_saida, entrada,
                         entrada ? &d->alterna_entrada : &d->alterna_saida, dados, tam);
    return ehci_bulk(d, entrada ? d->ep_entrada : d->ep_saida, entrada ? d->mps_entrada : d->mps_saida, entrada,
                     entrada ? &d->alterna_entrada : &d->alterna_saida, dados, tam);
}

/* ------------------------------------------------------------ isócrono (D275) */

/* Bytes del próximo paquete de un flujo de bytes_s bytes por segundo con paquetes cada
 * periodo_us microsegundos: se acumulan cuadros de audio en millonésimas y sale un número
 * entero de cuadros (nunca más que mps). Con 48 kHz y 1 ms: 48 cuadros siempre; con 44,1 kHz:
 * 44 o 45. El resto pasado de mps se queda en el acumulador. */
uint32_t usb_iso_calcular(uint32_t *acc, uint32_t bytes_s, uint8_t tam_quadro, uint32_t periodo_us, uint32_t mps)
{
    uint32_t taxa = bytes_s / tam_quadro;               /* cuadros por segundo */
    uint32_t a = *acc + taxa * periodo_us;               /* millonésimas de cuadro: cabe en 32 bits hasta 4294 s*Hz */
    uint32_t q = a / 1000000u;
    a -= q * 1000000u;
    uint32_t n = q * tam_quadro;
    if (n > mps) {
        uint32_t max = mps - mps % tam_quadro;
        a += (n - max) / tam_quadro * 1000000u;
        n = max;
    }
    *acc = a;
    return n;
}

/* Isócrono: xHCI, EHCI, UHCI y OHCI (D275). */
int usb_iso_abrir(usb_disp_t *d, const usb_iso_cfg_t *cfg)
{
    if (d->tipo_ctl == USB_XHCI) return xhci_iso_abrir(d, cfg);
    if (d->tipo_ctl == USB_EHCI) return ehci_iso_abrir(d, cfg);
    if (d->tipo_ctl == USB_UHCI) return uhci_iso_abrir(d, cfg);
    if (d->tipo_ctl == USB_OHCI) return ohci_iso_abrir(d, cfg);
    return USB_ERRO;
}

void usb_iso_ligar(usb_disp_t *d, int h)
{
    if (d->tipo_ctl == USB_XHCI) xhci_iso_ligar(d, h);
    else if (d->tipo_ctl == USB_EHCI) ehci_iso_ligar(d, h);
    else if (d->tipo_ctl == USB_UHCI) uhci_iso_ligar(d, h);
    else if (d->tipo_ctl == USB_OHCI) ohci_iso_ligar(d, h);
}

void usb_iso_parar(usb_disp_t *d, int h)
{
    if (d->tipo_ctl == USB_XHCI) xhci_iso_parar(d, h);
    else if (d->tipo_ctl == USB_EHCI) ehci_iso_parar(d, h);
    else if (d->tipo_ctl == USB_UHCI) uhci_iso_parar(d, h);
    else if (d->tipo_ctl == USB_OHCI) ohci_iso_parar(d, h);
}

uint32_t usb_iso_posicao(usb_disp_t *d, int h)
{
    if (d->tipo_ctl == USB_XHCI) return xhci_iso_posicao(d, h);
    if (d->tipo_ctl == USB_EHCI) return ehci_iso_posicao(d, h);
    if (d->tipo_ctl == USB_UHCI) return uhci_iso_posicao(d, h);
    if (d->tipo_ctl == USB_OHCI) return ohci_iso_posicao(d, h);
    return 0;
}

void usb_iso_fechar(usb_disp_t *d, int h)
{
    if (d->tipo_ctl == USB_XHCI) xhci_iso_fechar(d, h);
    else if (d->tipo_ctl == USB_EHCI) ehci_iso_fechar(d, h);
    else if (d->tipo_ctl == USB_UHCI) uhci_iso_fechar(d, h);
    else if (d->tipo_ctl == USB_OHCI) ohci_iso_fechar(d, h);
}

bool usb_iso_info(usb_disp_t *d, int h, usb_iso_info_t *i)
{
    if (d->tipo_ctl == USB_XHCI) return xhci_iso_info(d, h, i);
    if (d->tipo_ctl == USB_EHCI) return ehci_iso_info(d, h, i);
    if (d->tipo_ctl == USB_UHCI) return uhci_iso_info(d, h, i);
    if (d->tipo_ctl == USB_OHCI) return ohci_iso_info(d, h, i);
    return false;
}

bool usb_iso_ler(usb_disp_t *d, int h, uint32_t *off, uint32_t *len, bool *erro)
{
    if (d->tipo_ctl == USB_XHCI) return xhci_iso_ler(d, h, off, len, erro);
    if (d->tipo_ctl == USB_EHCI) return ehci_iso_ler(d, h, off, len, erro);
    if (d->tipo_ctl == USB_UHCI) return uhci_iso_ler(d, h, off, len, erro);
    if (d->tipo_ctl == USB_OHCI) return ohci_iso_ler(d, h, off, len, erro);
    return false;
}

/* ------------------------------------------------------------ esperas (D227) */

static processo_t *tarefa;          /* la tarea "usb"; NULL durante el arranque */
static const char canal_usb = 0;    /* donde duerme la tarea (el xHCI la despierta) */
static uint8_t prova;               /* CAB_USB_PRUEBA: fallos simulados para las pruebas */

/* Duerme la tarea "usb" al menos ms milisegundos (un tic más, para no quedarse corta). */
static void dormir_ms(uint32_t ms)
{
    uint32_t alvo = relogio_tiques() + (ms * TIQUES_POR_SEGUNDO + 999) / 1000 + 1;
    while ((int32_t)(alvo - relogio_tiques()) > 0) {
        bool i0 = int_salva_desliga();
        processo_atual()->acordar_em = alvo;
        bloquear(&canal_usb);
        ceder();
        processo_atual()->acordar_em = 0;
        int_restaura(i0);
    }
}

/* Antes de la D227 todas las esperas giraban, y la tarea "usb" las hacía con
 * las interrupciones apagadas: enchufar algo congelaba el sistema (y el reloj)
 * entre 200 ms y varios segundos. Ahora solo giran en el arranque. */
void usb_pausa_ms(ehci_t *c, uint32_t ms)
{
    if (tarefa && processo_atual() == tarefa && !contexto_irq()) dormir_ms(ms);
    else if (c) ehci_esperar_ms(c, ms);
    else xhci_esperar_ms(ms);
}

void usb_esperar_ms(usb_disp_t *d, uint32_t ms)
{
    usb_pausa_ms(d && d->tipo_ctl == USB_EHCI ? d->ctl : NULL, ms);
}

/* Endpoints de interrupción de los controladores que no son xHCI (HID, D214 y D267). */
int usb_int_abrir(usb_disp_t *d, uint8_t ep, uint16_t mps)
{
    if (d->tipo_ctl == USB_UHCI) return uhci_int_abrir(d, ep, mps);
    if (d->tipo_ctl == USB_OHCI) return ohci_int_abrir(d, ep, mps);
    return ehci_int_abrir(d, ep, mps);
}

int usb_int_ler(usb_disp_t *d, int h, uint8_t *dados, int max)
{
    if (d->tipo_ctl == USB_UHCI) return uhci_int_ler(h, dados, max);
    if (d->tipo_ctl == USB_OHCI) return ohci_int_ler(h, dados, max);
    return ehci_int_ler(h, dados, max);
}

void usb_int_fechar(usb_disp_t *d, int h)
{
    if (d->tipo_ctl == USB_UHCI) uhci_int_fechar(h);
    else if (d->tipo_ctl == USB_OHCI) ohci_int_fechar(h);
    else ehci_int_fechar(h);
}

/* D215: recepción continua del endpoint bulk de entrada (el pincho Wi-Fi). En
 * el xHCI la lectura armada se queda en el anillo hasta que se retire el
 * dispositivo: el buffer no se puede liberar (el driver lo guarda para siempre). */
int usb_rx_abrir(usb_disp_t *d, uint32_t buffer_fis, uint32_t tam)
{
    if (!d->ep_entrada || tam > 16384) return USB_ERRO;
    if (d->tipo_ctl == USB_XHCI) return xhci_rx_abrir(d, buffer_fis, (uint16_t)tam);
    if (d->tipo_ctl != USB_EHCI) return USB_ERRO;   /* UHCI/OHCI: sin recepción continua (D267) */
    return ehci_rx_abrir(d, d->ep_entrada, d->mps_entrada, buffer_fis, tam);
}

/* D238: con el xHCI, el identificador de una cola es USB_FILA_XHCI (la de
 * siempre devuelve 0). */
#define USB_FILA_XHCI 1

int usb_rx_ler(usb_disp_t *d, int h, uint8_t *dados, int max)
{
    if (d->tipo_ctl == USB_XHCI) {
        if (h == USB_FILA_XHCI) return xhci_fila_ler(d, dados, max);
        return xhci_int_ler(d, d->ep_entrada, dados, max);
    }
    if (d->tipo_ctl != USB_EHCI) return USB_ERRO;
    return ehci_rx_ler(h, dados, max);
}

void usb_rx_fechar(usb_disp_t *d, int h)
{
    if (d->tipo_ctl == USB_EHCI) ehci_rx_fechar(h);
    else if (h == USB_FILA_XHCI) xhci_fila_fechar(d, true);
}

int usb_rx_abrir_fila(usb_disp_t *d, uint32_t base_fis, uint32_t tam, int n)
{
    if (!d->ep_entrada || n < 1 || n > USB_FILA_MAX || tam > 16384) return USB_ERRO;
    if (d->tipo_ctl == USB_XHCI) return xhci_fila_abrir(d, true, base_fis, tam, n) < 0 ? USB_ERRO : USB_FILA_XHCI;
    if (d->tipo_ctl != USB_EHCI) return USB_ERRO;
    return ehci_rx_abrir_fila(d, d->ep_entrada, d->mps_entrada, base_fis, tam, n);
}

int usb_tx_abrir_fila(usb_disp_t *d, uint32_t base_fis, uint32_t tam, int n)
{
    if (!d->ep_saida || n < 1 || n > USB_FILA_MAX || d->tipo_ctl != USB_XHCI) return USB_ERRO;
    return xhci_fila_abrir(d, false, base_fis, tam, n);
}

int usb_tx_fila(usb_disp_t *d, const void *dados, uint32_t n)
{
    return d->tipo_ctl == USB_XHCI ? xhci_fila_enviar(d, dados, n) : USB_ERRO;
}

void usb_tx_fechar_fila(usb_disp_t *d)
{
    if (d->tipo_ctl == USB_XHCI) xhci_fila_fechar(d, false);
}

bool usb_fila_info(usb_disp_t *d, int h, bool entrada, uint32_t *emvoo, uint32_t *completadas, uint32_t *perdidas)
{
    if (d->tipo_ctl == USB_XHCI) return xhci_fila_info(d, entrada, emvoo, completadas, perdidas);
    return d->tipo_ctl == USB_EHCI && entrada && ehci_rx_info(h, emvoo, completadas, perdidas);
}

void usb_avisar_fila(const void *canal)
{
    xhci_avisar_fila(canal);
}

static int descritor(usb_disp_t *d, uint8_t tipo, uint8_t indice, uint16_t idioma, void *buf, uint16_t tam)
{
    int r = USB_ERRO;
    for (int t = 0; t < 3 && r < 0; t++)
        r = usb_controle(d, 0x80, 6, (uint16_t)((tipo << 8) | indice), idioma, buf, tam);
    return r;
}

/* Texto USB (UTF-16LE) para Latin-1: lo que no quepa se vuelve '?'. */
static void ler_texto(usb_disp_t *d, uint8_t indice, uint16_t idioma, char *saida, int max)
{
    saida[0] = 0;
    if (!indice) return;
    uint8_t b[128];
    int n = descritor(d, 3, indice, idioma, b, sizeof b);
    if (n < 2 || b[1] != 3) return;
    int k = 0;
    for (int i = 2; i + 1 < n && i + 1 < b[0] && k < max - 1; i += 2) {
        uint16_t c = (uint16_t)(b[i] | (b[i + 1] << 8));
        saida[k++] = c < 0x100 ? (char)c : '?';
    }
    while (k > 0 && saida[k - 1] == ' ') k--;   /* muchos dispositivos completan con espacios */
    saida[k] = 0;
}

static usb_disp_t *novo(void)
{
    for (int i = 0; i < USB_MAX_DISP; i++)
        if (!disps[i].usado) {
            memset(&disps[i], 0, sizeof disps[i]);
            disps[i].usado = true;
            return &disps[i];
        }
    return NULL;
}

/* Un puerto donde puede conectarse algo (D227): del EHCI, del xHCI o de un hub. */
typedef struct {
    uint8_t      tipo;     /* USB_EHCI, USB_XHCI, USB_UHCI o USB_OHCI */
    uint8_t      k;        /* número del controlador (para el log) */
    ehci_t      *e;
    xhci_t      *x;
    uhci_t      *u;
    ohci_t      *o;
    usb_disp_t  *hub;      /* NULL = puerto del controlador */
    uint8_t      porta;    /* del controlador: desde 0; de un hub: desde 1 */
    usb_porta_t *st;
} ponto_t;

/* Cómo acabó el último intento, además del paso (ultimo_passo). */
#define FALHA_NORMAL     0   /* se puede reintentar */
#define FALHA_DEFINITIVA 1   /* otro intento no lo arreglaría (USB 1.x sin compañero, sin TT...) */
#define FALHA_CEDIDA     2   /* EHCI: el puerto pasó al compañero */
static int falha;

static void remover(usb_disp_t *d);
static bool configurar_hub(usb_disp_t *h);
static ponto_t ponto_hub(usb_disp_t *h, uint8_t p);
static int ponto_ler(const ponto_t *pt, bool *mudou, bool *conectado);
static void iniciar_conexao(usb_porta_t *st);
static void ponto_tentar(const ponto_t *pt);

static const char *nome_classe(uint8_t c)
{
    switch (c) {
    case 0x03: return "HID";
    case 0x08: return "almacenamiento";
    case 0x09: return "hub";
    case 0x0E: return "vídeo";
    case 0xE0: return "inalámbrico";
    default: return "otra";
    }
}

static int ultimo_passo;   /* en qué paso se detuvo la última enumeración (diagnóstico) */

static void anotar_resultado(const usb_disp_t *m, int r)
{
    if (m->hub) return;
    if (m->tipo_ctl == USB_XHCI) xhci_porta_resultado(m->xctl, m->porta, r);
    else if (m->tipo_ctl == USB_UHCI) m->uctl->resultado[m->porta] = r;
    else if (m->tipo_ctl == USB_OHCI) m->octl->resultado[m->porta] = r;
    else m->ctl->resultado[m->porta] = r;
}

/* Recorre el descriptor de configuración: la primera interfaz (con sus
 * endpoints bulk, para el almacenamiento) y las interfaces HID con su
 * endpoint de interrupción de entrada. En USB 3.0, el "companion" de cada
 * endpoint bulk da su ráfaga. */
static void ler_configuracao(usb_disp_t *d, const uint8_t *b, int n)
{
    bool primeira = false, em_primeira = false;
    uint8_t classe_primeira = 0;
    usb_hid_if_t *hid = NULL;
    uint8_t *ultimo_burst = NULL;
    for (int i = 0; i + 1 < n && b[i] >= 2 && i + b[i] <= n; i += b[i]) {
        uint8_t tipo = b[i + 1];
        if (tipo == 4 && b[i] >= 9) {                       /* interfaz */
            ultimo_burst = NULL;
            hid = NULL;
            em_primeira = false;
            if (b[i + 3] != 0) continue;                    /* solo la alternativa 0 */
            if (!primeira) {
                primeira = em_primeira = true;
                classe_primeira = b[i + 5];
                d->interface = b[i + 2];
                if (d->classe == 0) { d->classe = b[i + 5]; d->subclasse = b[i + 6]; d->protocolo = b[i + 7]; }
            }
            /* D307: el mando de Xbox 360 con cable no es HID (0xFF/0x5D/0x01), pero se lee igual:
             * un endpoint de interrupción de entrada con informes de 20 bytes. */
            bool xbox = b[i + 5] == 0xFF && b[i + 6] == 0x5D && b[i + 7] == 0x01;
            if ((b[i + 5] == 3 || xbox) && d->n_hid < USB_MAX_HID) {
                hid = &d->hid[d->n_hid];
                memset(hid, 0, sizeof *hid);
                hid->interface = b[i + 2];
                hid->subclasse = b[i + 6];
                hid->protocolo = b[i + 7];
                hid->xbox360 = xbox;
            }
        } else if (tipo == 0x21 && hid && b[i] >= 9) {      /* descriptor HID: longitud del informe */
            hid->tam_relatorio = (uint16_t)(b[i + 7] | (b[i + 8] << 8));
        } else if (tipo == 5 && b[i] >= 7) {                /* endpoint */
            uint8_t ep = b[i + 2], atr = b[i + 3] & 3;
            uint16_t mps = (uint16_t)((b[i + 4] | (b[i + 5] << 8)) & 0x7FF);
            ultimo_burst = NULL;
            if (em_primeira && classe_primeira != 3 && atr == 3 && (ep & 0x80) && !d->ep_int) {   /* D279: eventos HCI de un Bluetooth */
                d->ep_int = ep & 0x0F;
                d->mps_int = mps;
                d->int_intervalo = b[i + 6];
            }
            if (em_primeira && atr == 2) {
                if (ep & 0x80) {
                    if (!d->ep_entrada) { d->ep_entrada = ep & 0x0F; d->mps_entrada = mps; ultimo_burst = &d->burst_entrada; }
                } else if (!d->ep_saida) { d->ep_saida = ep & 0x0F; d->mps_saida = mps; ultimo_burst = &d->burst_saida; }
            } else if (hid && atr == 3 && (ep & 0x80) && !hid->ep) {
                hid->ep = ep & 0x0F;
                hid->mps = mps;
                hid->intervalo = b[i + 6];
                d->n_hid++;
            }
        } else if (tipo == 0x30 && b[i] >= 6 && ultimo_burst) {   /* SuperSpeed Endpoint Companion */
            *ultimo_burst = b[i + 2] > 15 ? 15 : b[i + 2];
        }
    }
}

/* Los dispositivos de un mismo controlador (D267: sirve para EHCI, UHCI y OHCI). */
static bool mismo_ctl(const usb_disp_t *a, const usb_disp_t *b)
{
    if (a->tipo_ctl != b->tipo_ctl) return false;
    switch (a->tipo_ctl) {
    case USB_UHCI: return a->uctl == b->uctl;
    case USB_OHCI: return a->octl == b->octl;
    case USB_XHCI: return a->xctl == b->xctl;
    default: return a->ctl == b->ctl;
    }
}

/* Una dirección que no tenga ya otro dispositivo del mismo controlador (con los
 * reintentos de la D227 se gastan más deprisa y la cuenta da la vuelta). */
static uint8_t endereco_livre(const usb_disp_t *m)
{
    uint8_t *prox = m->tipo_ctl == USB_UHCI ? &m->uctl->proximo_endereco
                  : m->tipo_ctl == USB_OHCI ? &m->octl->proximo_endereco : &m->ctl->proximo_endereco;
    for (int t = 0; t < 127; t++) {
        uint8_t e = (*prox)++;
        if (*prox > 127 || *prox == 0) *prox = 1;
        if (e == 0 || e > 127) continue;
        bool usado = false;
        for (int i = 0; i < USB_MAX_DISP && !usado; i++)
            usado = disps[i].usado && mismo_ctl(&disps[i], m) && disps[i].endereco == e;
        if (!usado) return e;
    }
    return 0;
}

const char *usb_nome_ctl(uint8_t t)
{
    return t == USB_XHCI ? "xHCI" : t == USB_UHCI ? "UHCI" : t == USB_OHCI ? "OHCI" : "EHCI";
}
#define nome_ctl usb_nome_ctl

/* Enumera el dispositivo que acaba de aparecer donde dice "m" (controlador,
 * hub y puerto, velocidad, ruta). "tentativa" es el número de intento en esta
 * conexión (D227): en los pares, por el EHCI, el esquema antiguo de Linux
 * (primero SET_ADDRESS y después el descriptor), que algunos dispositivos
 * prefieren. Si falla, deja el paso en ultimo_passo y no deja nada a medias. */
static usb_disp_t *enumerar(const usb_disp_t *m, int tentativa)
{
    ultimo_passo = 1;
    usb_disp_t *d = novo();
    if (!d) { ultimo_passo = 10; anotar_resultado(m, 10); return NULL; }
    d->tipo_ctl = m->tipo_ctl;
    d->ctl = m->ctl;
    d->xctl = m->xctl;
    d->uctl = m->uctl;
    d->octl = m->octl;
    d->hub = m->hub;
    d->porta = m->porta;
    d->porta_raiz = m->porta_raiz;
    d->velocidade = m->velocidade;
    d->rota = m->rota;
    d->profundidade = m->profundidade;
    d->tt_endereco = m->tt_endereco;
    d->tt_slot = m->tt_slot;
    d->tt_porta = m->tt_porta;
    d->endereco = 0;
    d->mps0 = d->velocidade == USB_VEL_BAIXA ? 8 : 64;
    uint8_t b[256];
    if (d->tipo_ctl == USB_XHCI) {
        /* El controlador da la dirección; después, el tamaño del paquete del endpoint 0. */
        ultimo_passo = 2;
        if (xhci_endereco(d) < 0) goto falhou;
        usb_pausa_ms(NULL, 10);   /* tras SET_ADDRESS (lo manda el controlador), como con el EHCI */
        ultimo_passo = 1;
        if (descritor(d, 1, 0, 0, b, 8) < 8) goto falhou;
        uint16_t bcd = (uint16_t)(b[2] | (b[3] << 8));
        uint16_t mps = bcd >= 0x0300 ? (uint16_t)(b[7] <= 15 ? 1u << b[7] : 512) : (uint16_t)(b[7] ? b[7] : 8);
        if (mps != d->mps0 && xhci_mps0(d, mps) < 0) goto falhou;
        d->mps0 = mps;
    } else {
        bool antigo = (tentativa % 2) == 0;
        if (!antigo) {
            if (descritor(d, 1, 0, 0, b, 8) < 8) goto falhou;
            d->mps0 = b[7] ? b[7] : 64;
            /* D389: en la raíz de un UHCI, otro reset entre el primer descriptor y SET_ADDRESS, como el
             * esquema nuevo de Linux (drivers/usb/core/hub.c, hub_port_init): el clon CSR 0a12:0001 del
             * Amilo, sin él, daba la dirección pero no el descriptor completo («paso 3») al primer intento. */
            if (d->tipo_ctl == USB_UHCI && !d->hub) {
                int vel = uhci_porta_resetar(d->uctl, d->porta);
                if (vel < 0) { ultimo_passo = 9; goto falhou; }
                d->velocidade = (uint8_t)vel;
            }
        }
        ultimo_passo = 2;
        uint8_t endereco = endereco_livre(d);
        if (!endereco || usb_controle(d, 0x00, 5, endereco, 0, NULL, 0) < 0) goto falhou;
        d->endereco = endereco;
        usb_esperar_ms(d, 10);
        if (antigo) {
            /* Con la dirección ya puesta: 8 bytes caben en un paquete de cualquier tamaño. */
            ultimo_passo = 1;
            if (descritor(d, 1, 0, 0, b, 8) < 8) goto falhou;
            d->mps0 = b[7] ? b[7] : 64;
        }
    }
    if ((prova & USB_PRUEBA_SEMPRE) || ((prova & USB_PRUEBA_PRIMEIRO) && tentativa == 1)) {
        ultimo_passo = 3;
        klog("[USB] fallo simulado en el intento %d (--usb-prueba)\n", tentativa);
        goto falhou;
    }
    ultimo_passo = 3;
    if (descritor(d, 1, 0, 0, b, 18) < 18) goto falhou;
    ultimo_passo = 4;
    d->bcd_usb = (uint16_t)(b[2] | (b[3] << 8));
    d->id_fabricante = (uint16_t)(b[8] | (b[9] << 8));
    d->id_produto = (uint16_t)(b[10] | (b[11] << 8));
    d->classe = b[4]; d->subclasse = b[5]; d->protocolo = b[6];
    uint8_t i_fab = b[14], i_prod = b[15];
    uint16_t idioma = 0x0409;
    uint8_t l[8];
    if (descritor(d, 3, 0, 0, l, 4) >= 4) idioma = (uint16_t)(l[2] | (l[3] << 8));
    ler_texto(d, i_fab, idioma, d->fabricante, sizeof d->fabricante);
    ler_texto(d, i_prod, idioma, d->produto, sizeof d->produto);
    if (!d->fabricante[0]) snformatar(d->fabricante, sizeof d->fabricante, "%04x", d->id_fabricante);
    if (!d->produto[0]) snformatar(d->produto, sizeof d->produto, "%04x", d->id_produto);

    /* Configuración 0: encabezado de 9 bytes, después completa. */
    if (descritor(d, 2, 0, 0, b, 9) < 9) goto falhou;
    uint16_t total = (uint16_t)(b[2] | (b[3] << 8));
    if (total > sizeof b) total = sizeof b;
    int n = descritor(d, 2, 0, 0, b, total);
    if (n < 9) goto falhou;
    d->config = b[5];
    ler_configuracao(d, b, n);
    ultimo_passo = 5;
    if (d->tipo_ctl == USB_XHCI && xhci_configurar(d) < 0) goto falhou;
    if (usb_controle(d, 0x00, 9, d->config, 0, NULL, 0) < 0) goto falhou;
    ultimo_passo = 0;
    anotar_resultado(m, 0);
    klog("[USB] conectado: %s %s (%04x:%04x) classe %x (%s), endereco %u, %s %u porta %u, %s, %s\n",
         d->fabricante, d->produto, d->id_fabricante, d->id_produto, d->classe, nome_classe(d->classe),
         d->endereco, d->hub ? "hub" : "raiz", d->hub ? d->hub->endereco : 0,
         d->tipo_ctl == USB_XHCI && !d->hub ? d->porta + 1u : d->porta,
         nome_ctl(d->tipo_ctl), usb_nome_velocidade(d->velocidade));
    if (d->classe == 0x09) {
        if (!configurar_hub(d)) { ultimo_passo = 6; goto falhou; }
    } else {
        /* Los drivers apuntan el dispositivo en tablas del kernel (discos, Wi-Fi):
         * como antes de la D227, sin interrupciones (sus transferencias duermen igual). */
        bool i0 = int_salva_desliga();
        int r = 1;
        if (wifi_usb_reconhecer(d)) { }   /* D215: el pincho Wi-Fi o su CD virtual */
        else if (bt_usb_reconhecer(d)) { }   /* D279: controlador Bluetooth (0xE0/0x01/0x01) */
        else if (usbaudio_conectar(d)) { }   /* D275: audio USB (clase Audio 1.0) */
        else if (uvc_conectar(d)) { }        /* D277: cámara UVC (clase vídeo) */
        else if (d->classe == 0x08 && d->subclasse == 0x06 && d->protocolo == 0x50) r = armaz_conectar(d);
        else if (d->classe == 0x08) klog("[USB] armazenamento com protocolo nao suportado (%x/%x)\n", d->subclasse, d->protocolo);
        int_restaura(i0);
        if (r < 0) { ultimo_passo = 11; goto falhou; }   /* el pendrive no respondió: otro intento */
    }
    if (d->n_hid) {
        bool i0 = int_salva_desliga();
        hid_conectar(d);
        int_restaura(i0);
    }
    return d;
falhou:
    anotar_resultado(m, ultimo_passo);
    if (d->tipo_ctl == USB_XHCI && d->slot) xhci_liberar(d);
    d->usado = false;
    return NULL;
}

static usb_disp_t *enumerar_ehci(ehci_t *c, uint8_t porta, int tentativa)
{
    usb_disp_t m;
    memset(&m, 0, sizeof m);
    m.tipo_ctl = USB_EHCI;
    m.ctl = c;
    m.porta = m.porta_raiz = porta;
    m.velocidade = USB_VEL_ALTA;
    return enumerar(&m, tentativa);
}

static usb_disp_t *enumerar_xhci(xhci_t *c, int p, int tentativa)
{
    int vel = xhci_porta_resetar(c, p);
    if (vel < 0) { ultimo_passo = 9; xhci_porta_resultado(c, p, 9); return NULL; }
    usb_disp_t m;
    memset(&m, 0, sizeof m);
    m.tipo_ctl = USB_XHCI;
    m.xctl = c;
    m.porta = m.porta_raiz = (uint8_t)p;
    m.velocidade = (uint8_t)vel;
    return enumerar(&m, tentativa);
}

/* D267: los puertos de un UHCI y de un OHCI (reset, velocidad por el estado del puerto). */
static usb_disp_t *enumerar_uhci(uhci_t *c, int p, int tentativa)
{
    c->ultimo_td = 0;
    int vel = uhci_porta_resetar(c, p);
    usb_disp_t *d = NULL;
    if (vel < 0) { ultimo_passo = 9; c->resultado[p] = 9; }
    else {
        usb_disp_t m;
        memset(&m, 0, sizeof m);
        m.tipo_ctl = USB_UHCI;
        m.uctl = c;
        m.porta = m.porta_raiz = (uint8_t)p;
        m.velocidade = (uint8_t)vel;
        d = enumerar(&m, tentativa);
    }
    if (!d) {
        /* D389: lo necesario para saber qué es y por qué no contesta: el estado del puerto (bit 8 = baja
         * velocidad, bits 4-5 = líneas D+/D-, bit 2 = habilitado) y los bits de error del último TD. */
        char e[64];
        uhci_erro_texto(c->ultimo_td, e, sizeof e);
        klog("[UHCI] puerto %d: intento %d fallido en el paso %d; PORTSC %04x antes del reset, %04x despues; ultimo TD %s; %s\n",
             p, tentativa, ultimo_passo, c->porta_antes, c->porta_depois, c->ultimo_td ? e : "sin error", vel == USB_VEL_BAIXA ?
             "baja velocidad" : "velocidad completa");
    }
    return d;
}

static usb_disp_t *enumerar_ohci(ohci_t *c, int p, int tentativa)
{
    int vel = ohci_porta_resetar(c, p);
    if (vel < 0) { ultimo_passo = 9; c->resultado[p] = 9; return NULL; }
    usb_disp_t m;
    memset(&m, 0, sizeof m);
    m.tipo_ctl = USB_OHCI;
    m.octl = c;
    m.porta = m.porta_raiz = (uint8_t)p;
    m.velocidade = (uint8_t)vel;
    return enumerar(&m, tentativa);
}

/* ------------------------------------------------------------ hubs (USB 2.0, capítulo 11) */

#define PORTA_CONEXAO      0
#define PORTA_RESET        4
#define PORTA_ENERGIA      8
#define C_PORTA_CONEXAO    16
#define C_PORTA_RESET      20

static int hub_estado(usb_disp_t *h, uint8_t p, uint16_t *estado, uint16_t *mudou)
{
    uint8_t b[4];
    int r = usb_controle(h, 0xA3, 0, 0, p, b, 4);
    if (r < 4) return USB_ERRO;
    *estado = (uint16_t)(b[0] | (b[1] << 8));
    *mudou = (uint16_t)(b[2] | (b[3] << 8));
    return 0;
}

/* La velocidad del dispositivo en el puerto p del hub h (el estado del puerto
 * de un hub USB 3.0 no la dice: detrás de él solo hay SuperSpeed). */
static uint8_t hub_velocidade(usb_disp_t *h, uint16_t estado)
{
    if (h->velocidade >= USB_VEL_SUPER) return h->velocidade;
    if (estado & (1u << 9)) return USB_VEL_BAIXA;
    if (estado & (1u << 10)) return USB_VEL_ALTA;
    return USB_VEL_PLENA;
}

/* Reinicia el puerto p del hub h y enumera lo que haya. NULL si no: el paso, en
 * ultimo_passo; falha = FALHA_DEFINITIVA si no tiene arreglo con otro intento. */
static usb_disp_t *hub_porta_conectou(usb_disp_t *h, uint8_t p, int tentativa)
{
    uint16_t estado = 0, mudou = 0;
    ultimo_passo = 9;
    usb_controle(h, 0x23, 3, PORTA_RESET, p, NULL, 0);
    for (int i = 0; i < 50; i++) {
        usb_esperar_ms(h, 10);
        if (hub_estado(h, p, &estado, &mudou) == 0 && (mudou & (1u << 4))) break;
    }
    usb_controle(h, 0x23, 1, C_PORTA_RESET, p, NULL, 0);
    usb_esperar_ms(h, 10);
    if (hub_estado(h, p, &estado, &mudou) < 0 || !(estado & 1) || !(estado & 2)) {
        klog("[USB] hub %u puerto %u: no se habilito despues del reset (estado %x)\n", h->endereco, p, estado);
        return NULL;
    }
    usb_disp_t m;
    memset(&m, 0, sizeof m);
    m.tipo_ctl = h->tipo_ctl;
    m.ctl = h->ctl;
    m.xctl = h->xctl;
    m.uctl = h->uctl;
    m.octl = h->octl;
    m.hub = h;
    m.porta = p;
    m.porta_raiz = h->porta_raiz;
    m.profundidade = (uint8_t)(h->profundidade + 1);
    m.velocidade = hub_velocidade(h, estado);
    if (h->profundidade >= 5) {
        klog("[USB] hub %u: demasiados hubs en cadena\n", h->endereco);
        ultimo_passo = 7;
        falha = FALHA_DEFINITIVA;
        return NULL;
    }
    m.rota = h->rota | ((uint32_t)(p > 15 ? 15 : p) << (4 * h->profundidade));
    /* Baja o plena velocidad: por el "transaction translator" del hub de alta
     * velocidad más cercano (el propio hub o el que ya usaba él). */
    if (m.velocidade == USB_VEL_BAIXA || m.velocidade == USB_VEL_PLENA) {
        if (h->velocidade == USB_VEL_ALTA) { m.tt_endereco = h->endereco; m.tt_slot = h->slot; m.tt_porta = p; }
        else { m.tt_endereco = h->tt_endereco; m.tt_slot = h->tt_slot; m.tt_porta = h->tt_porta; }
        if (m.tipo_ctl == USB_EHCI && !m.tt_endereco) {
            klog("[USB] hub %u puerto %u: dispositivo de velocidad %s sin transaction translator\n",
                 h->endereco, p, m.velocidade == USB_VEL_BAIXA ? "baja" : "plena");
            ultimo_passo = 7;
            falha = FALHA_DEFINITIVA;
            return NULL;
        }
    }
    return enumerar(&m, tentativa);
}

/* Configura el hub recién enumerado: enciende sus puertos y enumera lo que ya
 * tenga conectado (cada puerto con su estado de la D227: si algo falla, se
 * reintenta como en los puertos del controlador). false si no da su descriptor. */
static bool configurar_hub(usb_disp_t *h)
{
    uint8_t b[16];
    int n = USB_ERRO;
    bool usb3 = h->velocidade >= USB_VEL_SUPER;
    /* Un hub USB 3.0 tiene otro descriptor (0x2A) y hay que decirle a qué
     * profundidad está (SET_HUB_DEPTH) antes de usar sus puertos. */
    for (int t = 0; t < 3 && n < 0; t++) n = usb_controle(h, 0xA0, 6, usb3 ? 0x2A00 : 0x2900, 0, b, usb3 ? 12 : 9);
    if (n < 7) { klog("[USB] hub sem descritor\n"); return false; }
    uint8_t portas = b[2];
    if (portas > USB_HUB_MAX_PORTAS) {
        klog("[USB] hub %u: %u puertos, solo se usan los %u primeros\n", h->endereco, portas, USB_HUB_MAX_PORTAS);
        portas = USB_HUB_MAX_PORTAS;
    }
    if (usb3) usb_controle(h, 0x20, 12, h->profundidade, 0, NULL, 0);
    if (h->tipo_ctl == USB_XHCI && xhci_hub(h, b[2], (uint8_t)((b[3] >> 5) & 3), false) < 0)
        klog("[USB] hub %u: el xHCI no ha aceptado los datos del hub\n", h->endereco);
    uint32_t espera = (uint32_t)b[5] * 2 + 20;
    for (uint8_t p = 1; p <= portas; p++)
        usb_controle(h, 0x23, 3, PORTA_ENERGIA, p, NULL, 0);
    usb_esperar_ms(h, espera);
    memset(h->hub_porta, 0, sizeof h->hub_porta);
    h->hub_portas = portas;
    klog("[USB] hub %u com %u portas (%s)\n", h->endereco, portas, usb_nome_velocidade(h->velocidade));
    for (uint8_t p = 1; p <= portas; p++) {
        ponto_t pt = ponto_hub(h, p);
        bool mudou, conectado;
        if (ponto_ler(&pt, &mudou, &conectado) < 0 || !conectado) continue;
        iniciar_conexao(pt.st);
        ponto_tentar(&pt);
    }
    return true;
}

/* ------------------------------------------------------------ remoción */

static void remover(usb_disp_t *d)
{
    /* Los drivers lo quitan de sus tablas sin interrupciones, como antes de la D227. */
    bool i0 = int_salva_desliga();
    for (int i = 0; i < USB_MAX_DISP; i++)
        if (disps[i].usado && disps[i].hub == d) remover(&disps[i]);   /* primero el que estaba detrás de él */
    if (d->n_hid) hid_desconectar(d);
    if (d->disco) armaz_desconectar(d);
    if (d->wifi) wifi_usb_retirado(d);
    if (d->bt) bt_usb_retirado(d);
    if (d->audio) usbaudio_desconectar(d);
    if (d->camara) uvc_desconectar(d);
    klog("[USB] removido: %s %s (endereco %u)\n", d->fabricante, d->produto, d->endereco);
    if (d->tipo_ctl == USB_XHCI) xhci_liberar(d);
    d->usado = false;
    int_restaura(i0);
}

/* ------------------------------------------------------------ puertos y conexión en caliente (D227)
 *
 * Cada puerto (del EHCI, del xHCI o de un hub) tiene su estado (usb_porta_t).
 * Una conexión nueva: antirrebote de 100 ms (USB 2.0, 7.1.7.3) y hasta
 * USB_INTENTOS_MAX intentos, cada uno con un reset nuevo del puerto: el primero
 * enseguida, los siguientes a 1, 2 y 5 s y después cada 10 s. Un dispositivo
 * recién encendido (o detrás de usbipd) a veces tarda en contestar, y el bit de
 * "la conexión cambió" ya se borró: sin esto, se quedaba sin reconocer para siempre.
 * Un puerto conectado sin estado (un cambio que se perdió) también se mira. */

static usb_porta_t portas_ehci[USB_MAX_CONTROLADORES][USB_MAX_PORTAS];
static usb_porta_t portas_xhci[XHCI_MAX_CTL][XHCI_MAX_PORTAS];
static usb_porta_t portas_uhci[USB_MAX_CONTROLADORES][USB_MAX_PORTAS];
static usb_porta_t portas_ohci[USB_MAX_CONTROLADORES][USB_MAX_PORTAS];

static ponto_t ponto_ehci(int k, int p)
{
    ponto_t pt = { .tipo = USB_EHCI, .k = (uint8_t)k, .e = ehci_controlador(k), .porta = (uint8_t)p, .st = &portas_ehci[k][p] };
    return pt;
}

static ponto_t ponto_xhci(int k, int p)
{
    ponto_t pt = { .tipo = USB_XHCI, .k = (uint8_t)k, .x = xhci_controlador(k), .porta = (uint8_t)p, .st = &portas_xhci[k][p] };
    return pt;
}

static ponto_t ponto_uhci(int k, int p)
{
    ponto_t pt = { .tipo = USB_UHCI, .k = (uint8_t)k, .u = uhci_controlador(k), .porta = (uint8_t)p, .st = &portas_uhci[k][p] };
    return pt;
}

static ponto_t ponto_ohci(int k, int p)
{
    ponto_t pt = { .tipo = USB_OHCI, .k = (uint8_t)k, .o = ohci_controlador(k), .porta = (uint8_t)p, .st = &portas_ohci[k][p] };
    return pt;
}

static ponto_t ponto_hub(usb_disp_t *h, uint8_t p)
{
    ponto_t pt = { .tipo = h->tipo_ctl, .e = h->ctl, .x = h->xctl, .u = h->uctl, .o = h->octl, .hub = h, .porta = p,
                   .st = &h->hub_porta[p - 1] };
    return pt;
}

/* Como se llama en el log: "1 (EHCI 0)", "3 (xHCI 0)" (los del xHCI desde 1,
 * como en sus otros mensajes), "2 (hub 5)" (la dirección del hub). */
static const char *ponto_nome(const ponto_t *pt, char *s, int n)
{
    if (pt->hub) snformatar(s, (size_t)n, "%u (hub %u)", pt->porta, pt->hub->endereco);
    else if (pt->tipo == USB_XHCI) snformatar(s, (size_t)n, "%u (xHCI %u)", pt->porta + 1u, pt->k);
    else if (pt->tipo == USB_UHCI) snformatar(s, (size_t)n, "%u (UHCI %u)", pt->porta, pt->k);
    else if (pt->tipo == USB_OHCI) snformatar(s, (size_t)n, "%u (OHCI %u)", pt->porta, pt->k);
    else snformatar(s, (size_t)n, "%u (EHCI %u)", pt->porta, pt->k);
    return s;
}

/* El EHCI que da el reloj de las esperas (NULL: el TSC). */
static ehci_t *ponto_relogio(const ponto_t *pt)
{
    return pt->tipo == USB_EHCI ? pt->e : NULL;
}

static usb_disp_t *ponto_disp(const ponto_t *pt)
{
    for (int i = 0; i < USB_MAX_DISP; i++) {
        usb_disp_t *d = &disps[i];
        if (!d->usado || d->hub != pt->hub || d->porta != pt->porta || d->tipo_ctl != pt->tipo) continue;
        if (!pt->hub) {
            void *a = pt->tipo == USB_EHCI ? (void *)d->ctl : pt->tipo == USB_XHCI ? (void *)d->xctl
                    : pt->tipo == USB_UHCI ? (void *)d->uctl : (void *)d->octl;
            void *b = pt->tipo == USB_EHCI ? (void *)pt->e : pt->tipo == USB_XHCI ? (void *)pt->x
                    : pt->tipo == USB_UHCI ? (void *)pt->u : (void *)pt->o;
            if (a != b) continue;
        }
        return d;
    }
    return NULL;
}

/* Lee si hay algo conectado y si la conexión cambió (y borra ese aviso). */
static int ponto_ler(const ponto_t *pt, bool *mudou, bool *conectado)
{
    if (pt->hub) {
        uint16_t estado, m;
        if (hub_estado(pt->hub, pt->porta, &estado, &m) < 0) return USB_ERRO;
        *mudou = (m & 1) != 0;
        if (*mudou) usb_controle(pt->hub, 0x23, 1, C_PORTA_CONEXAO, pt->porta, NULL, 0);
        *conectado = (estado & 1) != 0;
    } else if (pt->tipo == USB_EHCI) {
        *mudou = ehci_porta_mudou(pt->e, pt->porta);
        *conectado = (ehci_porta_estado(pt->e, pt->porta) & 1) != 0;
    } else if (pt->tipo == USB_UHCI) {
        *mudou = uhci_porta_mudou(pt->u, pt->porta);
        *conectado = uhci_porta_conectada(pt->u, pt->porta);
    } else if (pt->tipo == USB_OHCI) {
        *mudou = ohci_porta_mudou(pt->o, pt->porta);
        *conectado = ohci_porta_conectada(pt->o, pt->porta);
    } else {
        *mudou = xhci_porta_mudou(pt->x, pt->porta);
        *conectado = xhci_porta_conectada(pt->x, pt->porta);
    }
    return 0;
}

static void iniciar_conexao(usb_porta_t *st)
{
    st->estado = USB_PORTA_PENDENTE;
    st->tentativas = 0;
    st->passo = 0;
    st->proxima = relogio_tiques();
    st->conexoes++;
}

/* Antirrebote: la conexión tiene que seguir 100 ms sin cambiar (hasta 5 vueltas;
 * si sigue rebotando, se intenta igual y los reintentos harán el resto). */
static bool estabilizar(const ponto_t *pt)
{
    for (int i = 0; i < 5; i++) {
        usb_pausa_ms(ponto_relogio(pt), 100);
        bool mudou, conectado;
        if (ponto_ler(pt, &mudou, &conectado) < 0 || !conectado) return false;
        if (!mudou) return true;
    }
    return true;
}

/* Un intento: reset del puerto y enumeración. */
static usb_disp_t *ponto_enumerar(const ponto_t *pt, int tentativa)
{
    if (pt->hub) return hub_porta_conectou(pt->hub, pt->porta, tentativa);
    if (pt->tipo == USB_XHCI) return enumerar_xhci(pt->x, pt->porta, tentativa);
    if (pt->tipo == USB_UHCI) return enumerar_uhci(pt->u, pt->porta, tentativa);
    if (pt->tipo == USB_OHCI) return enumerar_ohci(pt->o, pt->porta, tentativa);
    int r = ehci_porta_resetar(pt->e, pt->porta);
    if (r == 0) return enumerar_ehci(pt->e, pt->porta, tentativa);
    if (r == USB_PORTA_CEDIDA) { ultimo_passo = 8; falha = FALHA_CEDIDA; }
    else if (r == USB_PORTA_USB1) { ultimo_passo = 8; falha = FALHA_DEFINITIVA; }
    else ultimo_passo = 9;
    pt->e->resultado[pt->porta] = ultimo_passo;
    return NULL;
}

static void ponto_tentar(const ponto_t *pt)
{
    static const uint8_t espera_s[USB_INTENTOS_RAPIDOS - 1] = { 1, 2, 5 };
    usb_porta_t *st = pt->st;
    char nome[32];
    ponto_nome(pt, nome, sizeof nome);
    st->tentativas++;
    unsigned de = st->tentativas <= USB_INTENTOS_RAPIDOS ? USB_INTENTOS_RAPIDOS : USB_INTENTOS_MAX;
    klog("[USB] porta %s: conectado, intento %u de %u\n", nome, st->tentativas, de);
    ultimo_passo = 1;
    falha = FALHA_NORMAL;
    usb_disp_t *d = ponto_enumerar(pt, st->tentativas);
    if (d) {
        st->estado = USB_PORTA_OK;
        st->passo = 0;
        klog("[USB] porta %s: reconocido (%s %s) al intento %u\n", nome, d->fabricante, d->produto, st->tentativas);
        return;
    }
    st->passo = (uint8_t)ultimo_passo;
    if (falha == FALHA_CEDIDA) {
        st->estado = USB_PORTA_COMPANHEIRO;
        klog("[USB] porta %s: USB 1.x, cedido al controlador companero\n", nome);
        return;
    }
    if (falha == FALHA_DEFINITIVA || st->tentativas >= USB_INTENTOS_MAX) {
        st->estado = USB_PORTA_DESISTIU;
        klog("[USB] porta %s: sin reconocer tras %u intentos (paso %u: %s); se deja: desenchufarlo y volver a enchufarlo\n",
             nome, st->tentativas, st->passo, usb_nome_passo(st->passo));
        return;
    }
    uint32_t s = st->tentativas < USB_INTENTOS_RAPIDOS ? espera_s[st->tentativas - 1] : 10;
    if (st->tentativas == USB_INTENTOS_RAPIDOS)
        klog("[USB] porta %s: sin reconocer tras %u intentos (paso %u: %s); se vuelve a probar cada %u s, hasta %u veces mas\n",
             nome, st->tentativas, st->passo, usb_nome_passo(st->passo), s, USB_INTENTOS_MAX - USB_INTENTOS_RAPIDOS);
    else
        klog("[USB] porta %s: intento %u fallido (paso %u: %s); el siguiente en %u s\n",
             nome, st->tentativas, st->passo, usb_nome_passo(st->passo), s);
    st->estado = USB_PORTA_PENDENTE;
    st->proxima = relogio_tiques() + s * TIQUES_POR_SEGUNDO;
}

/* Una vuelta por un puerto: cambios, retirada, antirrebote e intentos.
 * <0 si no se pudo leer (el hub ya no contesta). */
static int ponto_atender(const ponto_t *pt)
{
    usb_porta_t *st = pt->st;
    bool mudou, conectado;
    char nome[32];
    if (ponto_ler(pt, &mudou, &conectado) < 0) return USB_ERRO;
    if (!pt->hub && pt->tipo == USB_EHCI) {
        /* Un USB 1.x cedido al compañero (bit PO): el EHCI no lo toca. Al
         * desenchufarlo, el puerto vuelve solo al EHCI (EHCI 4.2.2) y el
         * siguiente de alta velocidad se enumera aquí. */
        if (ehci_porta_estado(pt->e, pt->porta) & EHCI_PORTA_PO) {
            st->estado = USB_PORTA_COMPANHEIRO;
            return 0;
        }
        if (st->estado == USB_PORTA_COMPANHEIRO) {
            klog("[USB] porta %s: el USB 1.x se ha retirado, el puerto vuelve al EHCI\n", ponto_nome(pt, nome, sizeof nome));
            st->estado = USB_PORTA_VAZIA;
            st->tentativas = 0;
        }
    }
    usb_disp_t *d = ponto_disp(pt);
    if (mudou || (!conectado && (d || st->estado != USB_PORTA_VAZIA))) {
        /* Desenchufado, o desenchufado y vuelto a enchufar entre dos vueltas:
         * fuera el que había; si hay otro, empieza de cero. Un dispositivo sin
         * reconocer que se ha vuelto a conectar solo (algunos lo hacen tras un
         * intento fallido) sigue con su cuenta: así no se reintenta sin fin. */
        if (d) remover(d);
        if (!d && conectado && st->estado == USB_PORTA_PENDENTE && st->tentativas) return 0;
        if (!d && (st->estado == USB_PORTA_PENDENTE || st->estado == USB_PORTA_DESISTIU))
            klog("[USB] porta %s: desconectado sin haberse reconocido\n", ponto_nome(pt, nome, sizeof nome));
        st->estado = USB_PORTA_VAZIA;
        st->tentativas = 0;
        d = NULL;
    }
    if (!conectado) return 0;
    if (st->estado == USB_PORTA_VAZIA) {
        if (d) { st->estado = USB_PORTA_OK; return 0; }
        if (!estabilizar(pt)) return 0;
        iniciar_conexao(st);
    }
    if (st->estado == USB_PORTA_PENDENTE && (int32_t)(relogio_tiques() - st->proxima) >= 0) ponto_tentar(pt);
    return 0;
}

void usb_verificar_conexoes(void)
{
    for (int k = 0; k < ehci_quantidade(); k++) {
        ehci_t *c = ehci_controlador(k);
        for (int p = 0; p < c->portas; p++) {
            ponto_t pt = ponto_ehci(k, p);
            ponto_atender(&pt);
        }
    }
    xhci_processar_todos();
    for (int k = 0; k < xhci_quantidade(); k++) {
        xhci_t *c = xhci_controlador(k);
        for (int p = 0; p < xhci_portas(c); p++) {
            ponto_t pt = ponto_xhci(k, p);
            ponto_atender(&pt);
        }
    }
    for (int k = 0; k < uhci_quantidade(); k++)
        for (int p = 0; p < uhci_controlador(k)->portas; p++) {
            ponto_t pt = ponto_uhci(k, p);
            ponto_atender(&pt);
        }
    for (int k = 0; k < ohci_quantidade(); k++)
        for (int p = 0; p < ohci_controlador(k)->portas; p++) {
            ponto_t pt = ponto_ohci(k, p);
            ponto_atender(&pt);
        }
    for (int i = 0; i < USB_MAX_DISP; i++) {
        usb_disp_t *h = &disps[i];
        if (!h->usado || h->classe != 0x09) continue;
        for (uint8_t p = 1; p <= h->hub_portas && h->usado; p++) {
            ponto_t pt = ponto_hub(h, p);
            if (ponto_atender(&pt) < 0) break;
        }
    }
}

static void tarefa_usb(void *arg)
{
    (void)arg;
    tarefa = processo_atual();   /* desde aquí, las esperas de los puertos duermen */
    for (;;) {
        /* D227: con las interrupciones encendidas; cada transferencia toma el
         * cerrojo de su controlador y las esperas duermen (usb_pausa_ms). */
        usb_verificar_conexoes();
        int_desliga();
        processo_atual()->acordar_em = relogio_tiques() + TIQUES_POR_SEGUNDO / 2;
        bloquear(&canal_usb);   /* también la despierta el xHCI cuando cambia un puerto */
        ceder();
        processo_atual()->acordar_em = 0;
        int_liga();
    }
}

void usb_iniciar(void)
{
    prova = cabecalho_boot[CAB_USB_PRUEBA];
    if (prova) klog("[USB] fallos simulados (--usb-prueba): %x\n", prova);
    /* D214: primero el xHCI (en un PC moderno lleva todos los puertos; la BIOS
     * puede haber dejado el teclado en él) y después el EHCI. */
    int nx = xhci_iniciar_todos();
    if (nx) klog("[USB] %d controlador(es) xHCI\n", nx);
    int n = ehci_iniciar_todos();
    klog("[USB] %d controlador(es) EHCI\n", n);
    /* D267: los de USB 1.1 después del EHCI (que se queda con todos los puertos y
     * cede al compañero lo que es de baja o plena velocidad). */
    int nu = uhci_iniciar_todos();
    int no = ohci_iniciar_todos();
    if (nu || no) klog("[USB] %d controlador(es) UHCI, %d OHCI\n", nu, no);
    if (!n && !nx && !nu && !no) return;
    /* Lo que ya está enchufado entra ahora (sin antirrebote: el controlador
     * acaba de encender los puertos y ya esperó), no como "cambió". Si algo
     * falla, la tarea lo reintenta. */
    bool i0 = int_salva_desliga();
    for (int k = 0; k < nx; k++) {
        xhci_t *c = xhci_controlador(k);
        for (int p = 0; p < xhci_portas(c); p++) {
            ponto_t pt = ponto_xhci(k, p);
            bool mudou, conectado;
            ponto_ler(&pt, &mudou, &conectado);
            if (conectado) { iniciar_conexao(pt.st); ponto_tentar(&pt); }
        }
    }
    for (int k = 0; k < n; k++) {
        ehci_t *c = ehci_controlador(k);
        for (int p = 0; p < c->portas; p++) {
            ponto_t pt = ponto_ehci(k, p);
            bool mudou, conectado;
            ponto_ler(&pt, &mudou, &conectado);
            if (conectado) { iniciar_conexao(pt.st); ponto_tentar(&pt); }
        }
    }
    for (int k = 0; k < nu; k++)
        for (int p = 0; p < uhci_controlador(k)->portas; p++) {
            ponto_t pt = ponto_uhci(k, p);
            bool mudou, conectado;
            ponto_ler(&pt, &mudou, &conectado);
            if (conectado) { iniciar_conexao(pt.st); ponto_tentar(&pt); }
        }
    for (int k = 0; k < no; k++)
        for (int p = 0; p < ohci_controlador(k)->portas; p++) {
            ponto_t pt = ponto_ohci(k, p);
            bool mudou, conectado;
            ponto_ler(&pt, &mudou, &conectado);
            if (conectado) { iniciar_conexao(pt.st); ponto_tentar(&pt); }
        }
    int_restaura(i0);
    xhci_avisar_usb(&canal_usb);
    tarefa_kernel("usb", tarefa_usb, NULL);
}

/* ------------------------------------------------------------ informe */

const char *usb_nome_passo(int r)
{
    static const char *const nomes[] = {
        "reconocido", "no respondió al primer descriptor", "no aceptó la dirección",
        "no dio el descriptor completo", "no dio la configuración", "no aceptó la configuración",
        "el hub no dio su descriptor", "sin transaction translator o demasiados hubs en cadena",
        "USB 1.x sin controlador compañero", "no se habilitó después del reset",
        "no hay sitio para más dispositivos", "el almacenamiento no respondió",
    };
    if (r < 0) return "aún no enumerado";
    return r < (int)(sizeof nomes / sizeof nomes[0]) ? nomes[r] : "?";
}

/* snformatar devuelve lo que habría escrito: sin este tope, un informe largo
 * pasaría del buffer del programa (k > max). */
#define JUNTAR(...)                                                            \
    do {                                                                       \
        if (k < max - 1) {                                                     \
            k += snformatar(s + k, (size_t)(max - k), __VA_ARGS__);            \
            if (k > max - 1) k = max - 1;                                      \
        }                                                                      \
    } while (0)

static int relatorio_porta(char *s, int k, int max, const ponto_t *pt, bool conectado)
{
    const usb_porta_t *st = pt->st;
    if (st->estado == USB_PORTA_VAZIA && !conectado) return k;
    char nome[32];
    ponto_nome(pt, nome, sizeof nome);
    switch (st->estado) {
    case USB_PORTA_OK: {
        const usb_disp_t *d = ponto_disp(pt);
        JUNTAR("  puerto %s: reconocido al intento %u (%s %s); %u conexiones\n", nome, st->tentativas,
               d ? d->fabricante : "?", d ? d->produto : "?", st->conexoes);
        break;
    }
    case USB_PORTA_PENDENTE:
        if (!st->tentativas) JUNTAR("  puerto %s: conectado, esperando el primer intento\n", nome);
        else {
            int32_t falta = (int32_t)(st->proxima - relogio_tiques());
            if (falta < 0) falta = 0;
            JUNTAR("  puerto %s: sin reconocer, intento %u de %u fallido (paso %u: %s); el siguiente en %u s\n", nome,
                   st->tentativas, st->tentativas <= USB_INTENTOS_RAPIDOS ? USB_INTENTOS_RAPIDOS : USB_INTENTOS_MAX,
                   st->passo, usb_nome_passo(st->passo), (uint32_t)(falta + TIQUES_POR_SEGUNDO - 1) / TIQUES_POR_SEGUNDO);
        }
        break;
    case USB_PORTA_DESISTIU:
        JUNTAR("  puerto %s: sin reconocer tras %u intentos (paso %u: %s); desenchufarlo y volver a enchufarlo\n", nome,
               st->tentativas, st->passo, usb_nome_passo(st->passo));
        break;
    case USB_PORTA_COMPANHEIRO:
        JUNTAR("  puerto %s: USB 1.x, cedido al controlador compañero (UHCI/OHCI)\n", nome);
        break;
    default:
        JUNTAR("  puerto %s: conectado, aún sin mirar\n", nome);
    }
    return k;
}

/* Informe de diagnóstico (comando "usb"): controladores, puertos y dispositivos. */
int usb_relatorio(char *s, int max)
{
    if (max < 2) return 0;
    int k = xhci_relatorio(s, max);
    if (k > max - 1) k = max - 1;
    JUNTAR("Controladores USB 2.0 (EHCI): %d\n", ehci_quantidade());
    for (int c = 0; c < ehci_quantidade() && k < max - 1; c++) {
        ehci_t *e = ehci_controlador(c);
        static const char *bios[] = { "no lo liberó", "no lo usaba", "lo liberó" };
        JUNTAR("EHCI %d: PCI %02x:%02x.%x, %04x:%04x, %d puertos, %s, BIOS %s, %d compañeros\n", c,
               e->pci.barramento, e->pci.dispositivo, e->pci.funcao, e->pci.fabricante, e->pci.modelo, e->portas,
               e->enderecos64 ? "64 bits" : "32 bits", bios[e->bios + 1], e->companheiros);
        JUNTAR("  espera de transferencias: %s (IRQ %u, %u interrupciones)\n",
               e->irq_usavel ? "durmiendo hasta la IRQ" : e->irq ? "cediendo la CPU (la IRQ no llega)" : "cediendo la CPU (sin IRQ)",
               e->irq, e->irqs);
        for (int p = 0; p < e->portas && k < max - 1; p++) {
            uint32_t v = ehci_porta_estado(e, p);
            if (!(v & 1)) continue;
            JUNTAR("  puerto %d: dispositivo conectado, %s (PORTSC %x)\n", p, usb_nome_passo(e->resultado[p]), v);
        }
    }
    JUNTAR("Controladores USB 1.1: %d UHCI, %d OHCI\n", uhci_quantidade(), ohci_quantidade());
    for (int c = 0; c < uhci_quantidade() && k < max - 1; c++) {
        uhci_t *u = uhci_controlador(c);
        JUNTAR("UHCI %d: PCI %02x:%02x.%x, %04x:%04x, E/S %x, %d puertos, BIOS %s\n", c,
               u->pci.barramento, u->pci.dispositivo, u->pci.funcao, u->pci.fabricante, u->pci.modelo, u->io,
               u->portas, u->bios ? "lo usaba" : "no lo usaba");
    }
    for (int c = 0; c < ohci_quantidade() && k < max - 1; c++) {
        ohci_t *o = ohci_controlador(c);
        JUNTAR("OHCI %d: PCI %02x:%02x.%x, %04x:%04x, %d puertos, BIOS %s\n", c,
               o->pci.barramento, o->pci.dispositivo, o->pci.funcao, o->pci.fabricante, o->pci.modelo, o->portas,
               o->bios < 0 ? "no lo soltó" : o->bios == 1 ? "lo soltó (SMM)" : o->bios == 2 ? "lo usaba" : "no lo usaba");
    }
    /* D227: estado de la conexión en caliente de cada puerto con algo. */
    JUNTAR("Puertos (conexión en caliente: hasta %u intentos):\n", USB_INTENTOS_MAX);
    for (int c = 0; c < xhci_quantidade(); c++) {
        xhci_t *x = xhci_controlador(c);
        for (int p = 0; p < xhci_portas(x); p++) {
            ponto_t pt = ponto_xhci(c, p);
            k = relatorio_porta(s, k, max, &pt, xhci_porta_conectada(x, p));
        }
    }
    for (int c = 0; c < ehci_quantidade(); c++) {
        ehci_t *e = ehci_controlador(c);
        for (int p = 0; p < e->portas; p++) {
            ponto_t pt = ponto_ehci(c, p);
            k = relatorio_porta(s, k, max, &pt, (ehci_porta_estado(e, p) & 1) != 0);
        }
    }
    for (int c = 0; c < uhci_quantidade(); c++)
        for (int p = 0; p < uhci_controlador(c)->portas; p++) {
            ponto_t pt = ponto_uhci(c, p);
            k = relatorio_porta(s, k, max, &pt, uhci_porta_conectada(pt.u, p));
        }
    for (int c = 0; c < ohci_quantidade(); c++)
        for (int p = 0; p < ohci_controlador(c)->portas; p++) {
            ponto_t pt = ponto_ohci(c, p);
            k = relatorio_porta(s, k, max, &pt, ohci_porta_conectada(pt.o, p));
        }
    for (int i = 0; i < USB_MAX_DISP; i++) {
        usb_disp_t *h = &disps[i];
        if (!h->usado || h->classe != 0x09) continue;
        for (uint8_t p = 1; p <= h->hub_portas; p++) {
            ponto_t pt = ponto_hub(h, p);
            k = relatorio_porta(s, k, max, &pt, false);
        }
    }
    for (int i = 0; i < USB_MAX_DISP && k < max - 1; i++) {
        usb_disp_t *d = &disps[i];
        if (!d->usado) continue;
        JUNTAR("Dispositivo %u: %s %s (%04x:%04x), clase %s, %s por %s%s\n", d->endereco,
               d->fabricante, d->produto, d->id_fabricante, d->id_produto, nome_classe(d->classe),
               usb_nome_velocidade(d->velocidade), nome_ctl(d->tipo_ctl),
               d->hub ? ", detrás de un hub" : "");
    }
    if (k < max - 1) {
        k += hid_relatorio(s + k, max - k);
        if (k > max - 1) k = max - 1;
    }
    if (k < max - 1) {
        k += usbaudio_relatorio(s + k, max - k);   /* D275 */
        k += uvc_relatorio(s + k, max - k);        /* D277 */
        if (k > max - 1) k = max - 1;
    }
    if (k < max - 1) {
        k += bt_relatorio(s + k, max - k);         /* D279 */
        if (k > max - 1) k = max - 1;
    }
    return k;
}
