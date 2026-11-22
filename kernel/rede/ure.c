// SPDX-License-Identifier: GPL-3.0-only
/* ure.c - adaptadores Ethernet USB Realtek RTL8152, RTL8153, RTL8153B, RTL8156 y
 * RTL8156B (D433). EXPERIMENTAL: escrito sin el adaptador delante, sin probar en
 * hardware real. El driver es propio; la lógica (secuencias de registros, recepción
 * agregada, trama de envío) está en ure_logica.h, y los valores vienen del if_ure.c
 * de OpenBSD (BSD de dos clausulas, ver docs/terceros.md).
 *
 * Un solo adaptador a la vez. La interfaz la pone la pila (rede.c, rede_usb_eth_registrar):
 * si hay una tarjeta PCI activa, el USB se queda aparcado y no toca la pila.
 *
 * Como el Wi-Fi USB (wifi.c), la pila IP no habla nunca con el USB: corre con las
 * interrupciones apagadas y una transferencia USB cede la CPU mientras espera, así que
 * otro proceso podría entrar en la pila a medias. Por eso:
 *   - enviar: la trama se copia a una cola en memoria y la tarea «ure» la manda;
 *   - cabo: devuelve el último estado del enlace, que lee la tarea cada 250 ms;
 *   - receber: lee la transferencia de recepción ya terminada (usb_rx_ler no espera).
 * La tarea «ure» es una sola y vive siempre (se crea con el primer adaptador): arranca el
 * chip, vacía la cola de envío y mira el enlace; al retirarlo, vuelve a esperar otro.
 *
 * Diagnóstico: todo va con el prefijo [URE] en el registro (klog, dmesg) y en el texto
 * de «ifconfig eth0» (diagnostico), con el paso exacto donde se queda el arranque. */
#include "tipos.h"
#include "usb.h"
#include "rede.h"
#include "console.h"
#include "cpu.h"
#include "memoria.h"
#include "processo.h"
#include "relogio.h"
#include "texto.h"
#include "ure_logica.h"

#define URE_TX_FILA     16                        /* tramas en espera de la tarea (potencia de 2) */
#define URE_ENLACE_CADA (TIQUES_POR_SEGUNDO / 4)  /* el enlace se lee cada 250 ms (OpenBSD: cada segundo) */

typedef struct {
    bool       presente;          /* el chip arrancó y su placa está registrada en la pila */
    bool       pendente;          /* la tarea «ure» está arrancando el chip */
    volatile bool retirado;       /* se desenchufó mientras arrancaba: la tarea lo deja */
    bool       tarefa_criada;
    usb_disp_t *novo;             /* adaptador reconocido que la tarea aún no ha tomado */
    usb_disp_t *d;
    int        chip;
    uint16_t   ver;
    bool       super, alta;
    uint8_t    mac[6];
    int        h_rx;
    uint32_t   rx_fis;            /* búfer físico de 16 KB alineado a 16 KB */
    ure_rx_t   pend;              /* transferencia de recepción en curso */
    bool       pend_ok;
    uint8_t    rx_copia[URE_BUF_RX];   /* destino de usb_rx_ler: nunca el anillo del DMA (rx_fis) */
    uint8_t    txq[URE_TX_FILA][URE_BUF_TX + 1];   /* +1: el byte que puede añadir ure_tx_evitar_mps */
    uint16_t   txq_n[URE_TX_FILA];
    volatile uint32_t tx_ini, tx_fim;   /* ini: lo siguiente que manda la tarea; fim: el hueco que llena la pila */
    ure_io_t   io;
    volatile bool enlace;
    int        mbps;
    bool       fdx;
    uint32_t   proximo_enlace;
    uint32_t   rx_pacotes, tx_pacotes, rx_erros, tx_erros, tx_fila_cheia, rx_cheias, ultimo_erro;
    char       descricao[48];
    placa_t    placa;
} ure_t;

static ure_t U;
static const char canal_ure = 0;   /* donde duerme la tarea «ure» */

/* ---------------------------------------------------------- transferencias */

static int ure_ctl(void *ctx, int lectura, uint16_t valor, uint16_t indice, uint8_t *buf, uint16_t n)
{
    usb_disp_t *d = (usb_disp_t *)ctx;
    if (U.retirado || !d->usado) return -1;   /* desenchufado: que el arranque acabe enseguida */
    /* vendor, bRequest 0x05: lo que hace el RTL815x (lectura 0xC0, escritura 0x40) */
    int e = usb_controle(d, lectura ? 0xC0 : 0x40, 0x05, valor, indice, buf, n);
    return e < 0 ? -1 : 0;
}

/* Duerme la tarea «ure» (el reloj va a 100 Hz: las esperas de menos de un tic giran, como en OpenBSD). */
static void ure_dormir_tiques(uint32_t t)
{
    bool i0 = int_salva_desliga();
    processo_atual()->acordar_em = relogio_tiques() + (t ? t : 1);
    bloquear(&canal_ure);
    ceder();
    processo_atual()->acordar_em = 0;
    int_restaura(i0);
}

static void ure_espera(void *ctx, uint32_t ms)
{
    if (U.retirado) return;
    if (ms * TIQUES_POR_SEGUNDO >= 1000) ure_dormir_tiques((ms * TIQUES_POR_SEGUNDO + 999) / 1000);
    else usb_esperar_ms((usb_disp_t *)ctx, ms);
}

static void ure_prepara_io(usb_disp_t *d)
{
    U.io.ctx = d;
    U.io.ctl = ure_ctl;
    U.io.espera_ms = ure_espera;   /* el contador de errores (io.errores) no se toca aquí */
}

/* ---------------------------------------------------------- enlace */

static void ure_leer_enlace(void)
{
    usb_disp_t *d = U.d;   /* la retirada puede dejar U.d a NULL entre dos líneas (la tarea corre con interrupciones) */
    if (!d) return;
    ure_prepara_io(d);
    uint32_t ps = ure_r2(&U.io, URE_PLA_PHYSTATUS, URE_MCU_TYPE_PLA);
    bool fdx = false;
    int mb = ure_velocidade(ps, &fdx);
    bool antes = U.enlace;
    U.enlace = mb > 0;
    U.mbps = mb;
    U.fdx = fdx;
    if (U.enlace != antes) {
        if (U.enlace) ure_enlace_arriba(&U.io);   /* como ure_link_state de OpenBSD */
        if (U.enlace) klog("[URE] enlace arriba: %d Mb/s %s\n", mb, fdx ? "full-duplex" : "half-duplex");
        else klog("[URE] enlace abajo\n");
    }
}

/* ---------------------------------------------------------- placa para la pila */

/* Sin USB: la pila la llama con las interrupciones apagadas (y desde las llamadas al sistema). */
static bool ure_cabo(void)
{
    return U.presente && U.enlace;
}

/* La pila deja la trama en la cola y despierta a la tarea; con la cola llena, se pierde (como un anillo lleno). */
static bool ure_enviar(const void *q, uint32_t n)
{
    if (!U.presente) return false;
    bool ok = false;
    bool i0 = int_salva_desliga();
    if (U.tx_fim - U.tx_ini < URE_TX_FILA) {
        uint32_t k = U.tx_fim % URE_TX_FILA;
        uint32_t t = ure_tx_montar(U.txq[k], URE_BUF_TX, (const uint8_t *)q, n);
        if (t) {
            U.txq_n[k] = (uint16_t)t;
            U.tx_fim++;
            ok = true;
        }
    } else {
        U.tx_fila_cheia++;
    }
    int_restaura(i0);
    if (ok) acordar(&canal_ure);
    else U.tx_erros++;
    return ok;
}

/* En la tarea: manda lo que haya en la cola, una transferencia por trama. */
static void ure_vaciar_envio(void)
{
    while (U.presente && U.tx_ini != U.tx_fim) {
        usb_disp_t *d = U.d;
        if (!d) break;
        uint32_t k = U.tx_ini % URE_TX_FILA;
        uint32_t mps = d->mps_saida ? d->mps_saida : 512;
        uint32_t t = ure_tx_evitar_mps(U.txq[k], sizeof U.txq[k], U.txq_n[k], mps);
        int e = usb_bulk(d, false, U.txq[k], t);
        bool i0 = int_salva_desliga();
        if (U.presente) U.tx_ini++;   /* al retirarlo, ure_usb_retirado ya vació la cola */
        int_restaura(i0);
        if (e < 0) {
            U.tx_erros++;
            if (U.tx_erros <= 4) klog("[URE] envío: error USB %d (transferencia de %u bytes)\n", e, t);
        } else {
            U.tx_pacotes++;
        }
    }
}

static int ure_receber(void *destino)
{
    if (!U.presente || !U.d) return 0;
    for (int vueltas = 0; vueltas < 8; vueltas++) {
        if (!U.pend_ok) {
            int n = usb_rx_ler(U.d, U.h_rx, U.rx_copia, URE_BUF_RX);
            if (n == 0) return 0;
            if (n < 0) {
                U.rx_erros++;
                if (U.rx_erros <= 4) klog("[URE] recepción: error USB %d\n", n);
                return 0;
            }
            /* Solo estadística: el chip cierra la agregación antes de pasar de 16 KB (RX_EARLY_SIZE se
             * calcula con URE_BUF_RX en ure_configurar_mac), así que una transferencia casi llena no corta
             * tramas: solo dice que llega mucho tráfico. */
            if ((uint32_t)n >= URE_BUF_RX - 2 * URE_TRAMA_MAX) U.rx_cheias++;
            U.pend.buf = U.rx_copia;
            U.pend.n = (uint32_t)n;
            U.pend.pos = 0;
            U.pend_ok = true;
        }
        int r = ure_rx_siguiente(&U.pend, (uint8_t *)destino, URE_TRAMA_MAX);
        if (r > 0) { U.rx_pacotes++; return r; }
        if (r == 0) { U.pend_ok = false; continue; }
        U.rx_erros++;
        U.ultimo_erro = (uint32_t)(-r);
        if (U.rx_erros <= 4) klog("[URE] recepción: trama descartada (código %d)\n", r);
    }
    return 0;
}

static bool ure_atender(void) { return false; }   /* sin IRQ: nada que atender */

static void ure_diagnostico(char *s, uint32_t max)
{
    uint32_t k = 0;
#define UPOE(...) do { if (k < max) { int _r = snformatar(s + k, max - k, __VA_ARGS__); k += _r > 0 ? (uint32_t)_r : 0; } } while (0)
    if (!U.presente) { UPOE("Ethernet USB Realtek: sin adaptador atendido\n"); return; }
    UPOE("Chip: %s (versión 0x%04x), USB %s, EXPERIMENTAL\n", ure_nome_chip(U.chip), U.ver,
         U.super ? "super" : U.alta ? "alta" : "completa");
    UPOE("Enlace: %s", U.enlace ? "" : "no");
    if (U.enlace) UPOE("%d Mb/s %s", U.mbps, U.fdx ? "full-duplex" : "half-duplex");
    UPOE("; rx %u tx %u errores rx %u tx %u, cola de envío llena %u, recepciones casi llenas %u (errores de control %u)\n",
         U.rx_pacotes, U.tx_pacotes, U.rx_erros, U.tx_erros, U.tx_fila_cheia, U.rx_cheias, U.io.errores);
#undef UPOE
}

/* ---------------------------------------------------------- reconocer, arrancar y retirar
 *
 * La enumeración USB (con las interrupciones desactivadas) solo anota el adaptador y despierta
 * la tarea «ure»: el arranque del chip (esperas de la autocarga, reinicios) corre allí, con las
 * interrupciones activas. Así no se queda nada parado en la enumeración. */

static bool ure_arrancar_adaptador(usb_disp_t *d)
{
    int paso = URE_P_NINGUNO;
    U.io.errores = 0;
    U.h_rx = -1;
    /* La configuración 1 del RTL815x es la del fabricante (clase 0xFF) con los dos endpoints bulk; si el
     * adaptador arrancó en la de CDC-ECM (otra clase), no hay endpoints bulk en la primera interfaz. */
    if (!d->ep_entrada || !d->ep_saida) {
        klog("[URE] la configuración %u no tiene los endpoints bulk del modo Realtek (clase %02x): sin usar\n",
             d->config, d->classe);
        return false;
    }
    ure_prepara_io(d);
    U.ver = (uint16_t)(ure_r2(&U.io, URE_PLA_TCR1, URE_MCU_TYPE_PLA) & URE_VERSION_MASK);
    U.chip = ure_chip_de_versao(U.ver);
    if (!U.chip) {
        klog("[URE] versión de chip 0x%04x sin soporte (RTL8157/8159, RTL8153D o desconocida): sin usar\n", U.ver);
        return false;
    }
    if (U.retirado) return false;
    U.super = d->velocidade == USB_VEL_SUPER || d->velocidade == USB_VEL_SUPERP;
    U.alta = d->velocidade == USB_VEL_ALTA;
    klog("[URE] chip %s (versión 0x%04x), velocidad USB %s, errores de control hasta ahora %u\n",
         ure_nome_chip(U.chip), U.ver, usb_nome_velocidade(d->velocidade), U.io.errores);

    int r = ure_iniciar_chip(&U.io, U.chip, U.ver, U.super);
    if (r) {
        klog("[URE] no responde en el paso «%s» (errores USB %u): sin usar\n", ure_nome_passo(r), U.io.errores);
        return false;
    }
    if (U.retirado) return false;
    ure_ler_mac(&U.io, U.chip, U.ver, U.mac);
    if ((U.mac[0] & 1) || (U.mac[0] | U.mac[1] | U.mac[2] | U.mac[3] | U.mac[4] | U.mac[5]) == 0) {
        klog("[URE] la MAC del chip no es válida: sin usar\n");
        return false;
    }

    /* búfer físico de 16 KB alineado a 16 KB (no cruza 64 KB), como el Wi-Fi USB */
    if (!U.rx_fis) {
        uint32_t b = memfisica_alocar_seguidas(8);
        if (!b) { klog("[URE] sin memoria para la recepción: sin usar\n"); return false; }
        U.rx_fis = (b + 0x3FFFu) & ~0x3FFFu;
    }

    if (ure_arrancar(&U.io, U.chip, U.ver, U.super, U.alta, U.mac, URE_BUF_RX, &paso) < 0) {
        klog("[URE] no responde en el paso «%s» (errores USB %u): sin usar\n", ure_nome_passo(paso), U.io.errores);
        return false;
    }
    if (U.retirado) return false;
    U.h_rx = usb_rx_abrir(d, U.rx_fis, URE_BUF_RX);
    if (U.h_rx < 0) {
        klog("[URE] no se pudo abrir la recepción USB: sin usar\n");
        return false;
    }

    /* descripción para «ifconfig» */
    snformatar(U.descricao, sizeof U.descricao, "Realtek %s (USB, experimental)", ure_nome_chip(U.chip));
    memset(&U.placa, 0, sizeof U.placa);
    U.placa.nome = "ure";
    U.placa.descricao = U.descricao;
    memcpy(U.placa.mac, U.mac, 6);
    U.placa.irq = -1;               /* sin IRQ: sondeo a cada tic */
    U.placa.experimental = true;
    U.placa.enviar = ure_enviar;
    U.placa.receber = ure_receber;
    U.placa.cabo = ure_cabo;
    U.placa.atender = ure_atender;
    U.placa.provocar_irq = NULL;
    U.placa.diagnostico = ure_diagnostico;
    U.placa.ajustar = NULL;

    U.presente = true;
    U.pend_ok = false;
    klog("[URE] %s lista: MAC %02x:%02x:%02x:%02x:%02x:%02x (rx por bulk de %u bytes)\n", ure_nome_chip(U.chip),
         U.mac[0], U.mac[1], U.mac[2], U.mac[3], U.mac[4], U.mac[5], (unsigned)URE_BUF_RX);
    return true;
}

/* Arranca el adaptador que tomó la tarea y lo registra en la pila (o lo deja si se retiró a mitad). */
static void ure_tomar(usb_disp_t *d)
{
    bool ok = ure_arrancar_adaptador(d);
    bool i0 = int_salva_desliga();   /* sin que la retirada (tarea «usb») pase entre la comprobación y el registro */
    if (ok && !U.retirado) {
        U.tx_ini = U.tx_fim = 0;
        U.enlace = false;
        U.proximo_enlace = relogio_tiques();
        rede_usb_eth_registrar(&U.placa);
    } else {
        if (U.presente && U.h_rx >= 0) usb_rx_fechar(d, U.h_rx);   /* con el EHCI suelta la cola de recepción */
        U.presente = false;
        if (U.retirado) klog("[URE] adaptador retirado durante el arranque\n");
        else d->ure = false;      /* d sigue enchufado: que su retirada no llame a este driver */
        U.d = NULL;
    }
    U.pendente = false;
    U.retirado = false;
    int_restaura(i0);
}

/* La tarea «ure»: toma el adaptador nuevo, vacía la cola de envío y lee el enlace. */
static void ure_tarefa(void *arg)
{
    (void)arg;
    for (;;) {
        usb_disp_t *d = NULL;
        bool i0 = int_salva_desliga();
        if (U.novo && !U.presente && !U.pendente) {
            d = U.novo;
            U.novo = NULL;
            U.d = d;
            U.pendente = true;
            U.retirado = false;
        }
        int_restaura(i0);
        if (d) ure_tomar(d);
        if (U.presente) {
            ure_vaciar_envio();
            if ((int32_t)(relogio_tiques() - U.proximo_enlace) >= 0) {
                U.proximo_enlace = relogio_tiques() + URE_ENLACE_CADA;
                bool antes = U.enlace;
                ure_leer_enlace();
                if (U.enlace != antes) rede_acordar();
            }
        }
        /* sin adaptador, hasta que llegue uno (ure_usb_reconhecer despierta); con él, un tic o una trama */
        i0 = int_salva_desliga();
        bool nada = !U.novo && (!U.presente || U.tx_ini == U.tx_fim);
        if (nada) {
            processo_atual()->acordar_em = U.presente ? relogio_tiques() + 1 : 0;
            bloquear(&canal_ure);
            ceder();
            processo_atual()->acordar_em = 0;
        }
        int_restaura(i0);
    }
}

/* Desde remover() de usb.c (tarea «usb», interrupciones apagadas). */
void ure_usb_retirado(usb_disp_t *d)
{
    if (U.novo == d) { U.novo = NULL; klog("[URE] adaptador retirado antes de arrancar\n"); return; }
    if (U.d != d) return;
    if (U.pendente) {       /* la tarea está arrancando: que lo vea y no registre nada */
        U.retirado = true;
        acordar(&canal_ure);
        return;
    }
    if (U.presente) {
        U.presente = false;
        U.enlace = false;
        U.tx_ini = U.tx_fim;   /* lo que quedaba en la cola se pierde */
        rede_usb_eth_registrar(NULL);
        if (U.h_rx >= 0) usb_rx_fechar(d, U.h_rx);
    }
    U.d = NULL;
    U.pend_ok = false;
    klog("[URE] adaptador retirado\n");
}

/* D433: hay un adaptador reconocido (arrancando o ya en marcha); lo usa rede_iniciar() con URE_PREFERIR=1. */
bool ure_usb_visto(void)
{
    return U.novo || U.pendente || U.presente;
}

bool ure_usb_reconhecer(usb_disp_t *d)
{
    if (!ure_id_conhecido(d->id_fabricante, d->id_produto)) return false;
    klog("[URE] adaptador Ethernet USB %04x:%04x (%s) - EXPERIMENTAL, sin probar en hardware\n",
         d->id_fabricante, d->id_produto, d->produto);
    if (U.presente || U.pendente || U.novo) {
        klog("[URE] ya hay un adaptador atendido: este queda sin usar\n");
        return true;
    }
    U.novo = d;
    d->ure = true;
    if (!U.tarefa_criada) {
        U.tarefa_criada = true;
        U.h_rx = -1;
        if (!tarefa_kernel("ure", ure_tarefa, NULL)) {
            U.tarefa_criada = false;
            U.novo = NULL;
            d->ure = false;
            klog("[URE] no se pudo crear la tarea «ure»: sin usar\n");
            return true;
        }
    }
    acordar(&canal_ure);   /* el arranque del chip va en la tarea, no aquí */
    return true;
}
