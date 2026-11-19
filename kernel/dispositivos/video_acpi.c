// SPDX-License-Identifier: GPL-2.0-or-later
/* video_acpi.c - el ACPI Video estándar: brillo de la pantalla del portátil y
 * avisos de las teclas de brillo y de pantalla (D403; especificación ACPI,
 * apéndice B «Video Extensions»).
 *
 * Casi todas las marcas lo tienen, desde los portátiles de la época de Windows
 * XP: un dispositivo de vídeo (el de la tarjeta gráfica en el árbol ACPI:
 * \_SB.PCI0.GFX0, \_SB.PCI0.AGP.VGA, \_SB.PCI0.PEG0.VGA...) con
 *   _DOS(n)  cómo cambia la BIOS de pantalla con la tecla (bits 0-1) y si
 *            ajusta ella el brillo (bit 2);
 *   _DOD     la lista de sus salidas;
 * y, en la salida de la pantalla del portátil (LCD, DD02...),
 *   _BCL     los niveles de brillo (el de cargador, el de batería y la lista);
 *   _BCM(v)  pone el nivel v;
 *   _BQC     el nivel actual (opcional; algunas BIOS dan el índice).
 * Las teclas llegan como Notify: en la salida 0x86 (subir), 0x87 (bajar),
 * 0x85 (ciclo), 0x88 (a cero) y 0x89 (apagar); en el dispositivo 0x80-0x84
 * (cambio de pantalla). En SaguiOS los avisos llegan de los _Qxx del
 * controlador embebido (ec.c los mira cada 200 ms) o de los métodos de cada
 * marca (Toshiba, Fujitsu).
 *
 * Qué se hace:
 *   - _DOS(0), como Linux con las BIOS anteriores a Windows 8: la BIOS sigue
 *     cambiando de pantalla ella sola con su tecla (SaguiOS no sabe hacerlo con
 *     varias salidas) y avisa;
 *   - de las salidas con _BCL y _BCM, se elige una para el brillo: la de un
 *     dispositivo de vídeo cuya tarjeta está en el PCI (si se sabe mirar);
 *   - 0x86/0x87 son acciones de brillo (acoes.c); si el firmware ya lo cambió
 *     él, portatil.c solo avisa (no sube dos pasos).
 * Nada de esto cambia el brillo al arrancar. */
#include "uacpi_sagui.h"
#include <uacpi/kernel_api.h>
#include "acoes.h"
#include "portatil.h"
#include "brilho_conv.h"
#include "console.h"
#include "cpu.h"
#include "relogio.h"
#include "texto.h"

#define MAX_BUS     4
#define MAX_SAIDAS  8
#define NIVEIS_MAX  BRILHO_MAX_NIVEIS

typedef struct {
    uacpi_namespace_node *no;
    int  presente;            /* 1 en el PCI, 0 no está, -1 no se sabe */
    bool dos;
} bus_t;

typedef struct {
    uacpi_namespace_node *no;
    int  bus;
    bool tem_bqc;
} saida_t;

static bus_t buses[MAX_BUS];
static int nbus;
static saida_t saidas[MAX_SAIDAS];
static int nsaidas;
static int lcd = -1;                       /* la salida elegida para el brillo */
static uint32_t niv[NIVEIS_MAX];
static int nniv;
static bool bqc_indice;
static char nome_lcd[40];
static uint32_t ultimo_tique, ultimo_valor;   /* para no contar dos veces el mismo aviso */

/* ------------------------------------------------------------ ¿está la tarjeta en el PCI? */

static bool ponte_raiz(uacpi_namespace_node *no)
{
    static const char *const ids[] = { "PNP0A03", "PNP0A08", NULL };
    return uacpi_device_matches_pnp_id(no, ids);
}

static uint32_t pci_ler(int bus, int dev, int fn, int reg, int largura)
{
    uacpi_pci_address e = { .segment = 0, .bus = (uacpi_u8)bus, .device = (uacpi_u8)dev, .function = (uacpi_u8)fn };
    uacpi_handle h;
    if (uacpi_kernel_pci_device_open(e, &h) != UACPI_STATUS_OK) return 0xFFFFFFFFu;
    uint32_t v = 0xFFFFFFFFu;
    if (largura == 1) { uacpi_u8 x; if (uacpi_kernel_pci_read8(h, (uacpi_size)reg, &x) == UACPI_STATUS_OK) v = x; }
    else { uacpi_u16 x; if (uacpi_kernel_pci_read16(h, (uacpi_size)reg, &x) == UACPI_STATUS_OK) v = x; }
    uacpi_kernel_pci_device_close(h);
    return v;
}

/* El bus PCI que hay debajo de "no" (un puente raíz o un puente PCI-PCI con
 * _ADR), -1 si no se sabe; prof evita ciclos raros. */
static int bus_abaixo(uacpi_namespace_node *no, int prof)
{
    if (!no || prof > 6) return -1;
    if (ponte_raiz(no)) {
        uint64_t bbn = 0;
        if (portatil_eval_int(no, "_BBN", NULL, 0, &bbn) != UACPI_STATUS_OK) bbn = 0;
        return (int)(bbn & 0xFF);
    }
    uacpi_u64 adr;
    if (uacpi_eval_adr(no, &adr) != UACPI_STATUS_OK) return -1;
    int pai = bus_abaixo(uacpi_namespace_node_parent(no), prof + 1);
    if (pai < 0) return -1;
    int dev = (int)(adr >> 16) & 0x1F, fn = (int)adr & 7;
    if (pci_ler(pai, dev, fn, 0, 2) == 0xFFFF) return -1;
    uint32_t sec = pci_ler(pai, dev, fn, 0x19, 1);
    return sec == 0xFFFFFFFFu || sec == 0 ? -1 : (int)sec;
}

/* 1 = la función PCI del dispositivo responde; 0 = no está; -1 = no se sabe. */
static int presente_pci(uacpi_namespace_node *no)
{
    uacpi_u64 adr;
    if (uacpi_eval_adr(no, &adr) != UACPI_STATUS_OK) return -1;
    int bus = bus_abaixo(uacpi_namespace_node_parent(no), 0);
    if (bus < 0) return -1;
    return pci_ler(bus, (int)(adr >> 16) & 0x1F, (int)adr & 7, 0, 2) == 0xFFFF ? 0 : 1;
}

/* ------------------------------------------------------------ el brillo */

static int ler(void)
{
    if (lcd < 0 || !saidas[lcd].tem_bqc) return -1;
    uint64_t v;
    if (portatil_eval_int(saidas[lcd].no, "_BQC", NULL, 0, &v) != UACPI_STATUS_OK) return -1;
    bool e_idx = false;
    int i = brilho_bqc(niv, nniv, v, &e_idx);
    if (e_idx && !bqc_indice) {
        bqc_indice = true;
        klog("[VIDEO] el _BQC de esta BIOS da el indice del nivel, no su valor\n");
    }
    return i;
}

static bool fixar(int i)
{
    if (lcd < 0 || i < 0 || i >= nniv) return false;
    uint64_t v = niv[i];
    uacpi_object *o = NULL;
    uacpi_status r = portatil_eval_ints(saidas[lcd].no, "_BCM", &v, 1, &o);
    if (o) uacpi_object_unref(o);
    return r == UACPI_STATUS_OK;
}

static int pct(int i) { return brilho_porcentagem(niv, nniv, i); }
static uint32_t valor(int i) { return i >= 0 && i < nniv ? niv[i] : 0; }

/* Los niveles del _BCL de una salida; false si no sirve. */
static bool ler_bcl(uacpi_namespace_node *no, uint32_t *dst, int *n)
{
    uacpi_object *o = NULL;
    uacpi_object_array a;
    *n = 0;
    if (uacpi_eval_simple_package(no, "_BCL", &o) != UACPI_STATUS_OK || uacpi_object_get_package(o, &a) != UACPI_STATUS_OK) {
        if (o) uacpi_object_unref(o);
        return false;
    }
    uint64_t brutos[NIVEIS_MAX + 2];
    int k = 0;
    for (uacpi_size i = 0; i < a.count && k < NIVEIS_MAX + 2; i++) {
        uacpi_u64 v;
        if (uacpi_object_get_integer(a.objects[i], &v) == UACPI_STATUS_OK) brutos[k++] = v;
    }
    uacpi_object_unref(o);
    *n = brilho_niveis(brutos, k, dst, NIVEIS_MAX);
    return *n >= 2;
}

/* ------------------------------------------------------------ avisos */

static uacpi_status notificacao(uacpi_handle ctx, uacpi_namespace_node *no, uacpi_u64 v)
{
    (void)no;
    int quem = (int)(uintptr_t)ctx;   /* < 0: dispositivo de vídeo -(i+1); >= 0: salida i */
    video_acpi_notificar(quem < 0 ? PORTATIL_ALVO_BUS : PORTATIL_ALVO_VIDEO, (uint32_t)v);
    return UACPI_STATUS_OK;
}

void video_acpi_notificar(int alvo, uint32_t v)
{
    if (!nbus) return;   /* un aviso simulado sin dispositivo de vídeo */
    portatil_evento(alvo, v);
    /* El mismo aviso a dos salidas en el mismo instante (BIOS con dos tarjetas
     * que avisan a las dos): una sola acción. */
    uint32_t agora = relogio_tiques();
    if (v == ultimo_valor && agora - ultimo_tique <= 2) return;
    ultimo_valor = v;
    ultimo_tique = agora;
    klog("[VIDEO] aviso %x de %s\n", v, alvo == PORTATIL_ALVO_BUS ? "el dispositivo de video" : "la salida");
    if (alvo == PORTATIL_ALVO_BUS) {
        if (v == 0x80 || v == 0x82 || v == 0x83 || v == 0x84) acao_sistema(ACAO_TELA, ORIGEM_ACAO_VIDEO);
        return;
    }
    switch (v) {
    case 0x85:   /* ciclo: sube (lo que hace Windows hasta el máximo) */
    case 0x86: acao_sistema(ACAO_BRILHO_MAIS, ORIGEM_ACAO_VIDEO); break;
    case 0x87: acao_sistema(ACAO_BRILHO_MENOS, ORIGEM_ACAO_VIDEO); break;
    case 0x88: portatil_brilho_pedir(PORTATIL_BRILHO_FIXAR, 0, ORIGEM_ACAO_VIDEO); break;
    default: break;   /* 0x89 (apagar la pantalla) y los demás: solo el registro */
    }
}

/* ------------------------------------------------------------ búsqueda */

static uacpi_iteration_decision achar_saida(void *u, uacpi_namespace_node *no, uacpi_u32 prof)
{
    (void)prof;
    int b = (int)(uintptr_t)u;
    if (nsaidas >= MAX_SAIDAS) return UACPI_ITERATION_DECISION_BREAK;
    if (!portatil_tem_metodo(no, "_BCL") || !portatil_tem_metodo(no, "_BCM")) return UACPI_ITERATION_DECISION_CONTINUE;
    saida_t *s = &saidas[nsaidas];
    s->no = no;
    s->bus = b;
    s->tem_bqc = portatil_tem_metodo(no, "_BQC");
    uacpi_install_notify_handler(no, notificacao, (uacpi_handle)(uintptr_t)nsaidas);
    nsaidas++;
    return UACPI_ITERATION_DECISION_CONTINUE;
}

static uacpi_iteration_decision achar_bus(void *u, uacpi_namespace_node *no, uacpi_u32 prof)
{
    (void)u; (void)prof;
    if (nbus >= MAX_BUS) return UACPI_ITERATION_DECISION_BREAK;
    if (!portatil_tem_metodo(no, "_DOD") && !portatil_tem_metodo(no, "_DOS")) return UACPI_ITERATION_DECISION_CONTINUE;
    uint32_t sta = 0xF;
    if (uacpi_eval_sta(no, &sta) == UACPI_STATUS_OK && !(sta & 1)) return UACPI_ITERATION_DECISION_CONTINUE;
    bus_t *b = &buses[nbus];
    b->no = no;
    b->presente = presente_pci(no);
    uacpi_install_notify_handler(no, notificacao, (uacpi_handle)(uintptr_t)(uint32_t)(-(nbus + 1)));
    int antes = nsaidas;
    uacpi_namespace_for_each_child(no, achar_saida, NULL, UACPI_OBJECT_DEVICE_BIT, 1, (void *)(uintptr_t)nbus);
    char nome[48];
    portatil_nome(no, nome, sizeof nome);
    klog("[VIDEO] dispositivo de video %s (%s): %d salida(s) con brillo\n", nome,
         b->presente == 1 ? "en el PCI" : b->presente == 0 ? "sin tarjeta en el PCI" : "PCI sin comprobar",
         nsaidas - antes);
    nbus++;
    return UACPI_ITERATION_DECISION_CONTINUE;
}

void video_acpi_iniciar(void)
{
    uacpi_namespace_for_each_child(uacpi_namespace_root(), achar_bus, NULL, UACPI_OBJECT_DEVICE_BIT,
                                   UACPI_MAX_DEPTH_ANY, NULL);
    if (!nbus) {
        klog("[VIDEO] no hay dispositivos de video ACPI (_DOD/_DOS)\n");
        return;
    }
    /* La salida para el brillo: primero una de una tarjeta que está en el PCI,
     * después una que no se sabe; nunca la de una tarjeta que no está. */
    for (int pase = 1; pase >= -1 && lcd < 0; pase -= 2)
        for (int i = 0; i < nsaidas && lcd < 0; i++) {
            if (buses[saidas[i].bus].presente != pase) continue;
            uint32_t tmp[NIVEIS_MAX];
            int n;
            if (!ler_bcl(saidas[i].no, tmp, &n)) continue;
            lcd = i;
            nniv = n;
            for (int k = 0; k < n; k++) niv[k] = tmp[k];
        }
    /* _DOS(0) en los dispositivos que están (los avisos que deje llegan después,
     * en la cola de trabajo de la tarea). Lo que conteste no importa. */
    for (int i = 0; i < nbus; i++) {
        if (buses[i].presente == 0 || !portatil_tem_metodo(buses[i].no, "_DOS")) continue;
        uint64_t zero = 0;
        uacpi_object *o = NULL;
        uacpi_status r = portatil_eval_ints(buses[i].no, "_DOS", &zero, 1, &o);
        if (o) uacpi_object_unref(o);
        buses[i].dos = r == UACPI_STATUS_OK;
        if (r != UACPI_STATUS_OK) klog("[VIDEO] _DOS(0): %s\n", uacpi_status_to_string(r));
    }
    if (lcd < 0) {
        klog("[VIDEO] ninguna salida con _BCL/_BCM utilizable\n");
        return;
    }
    portatil_nome(saidas[lcd].no, nome_lcd, sizeof nome_lcd);
    klog("[VIDEO] brillo por %s: %d niveles (%u-%u)%s\n", nome_lcd, nniv, niv[0], niv[nniv - 1],
         saidas[lcd].tem_bqc ? "" : ", sin _BQC");
    brilho_drv_t d = { PORTATIL_FONTE_VIDEO, nniv, ler, fixar, pct, valor, nome_lcd };
    portatil_registrar_brilho(&d);
}

void video_acpi_info(info_portatil_t *p)
{
    p->video_buses = (uint32_t)nbus;
    p->video_saidas = (uint32_t)nsaidas;
    p->bqc_indice = bqc_indice ? 1 : 0;
    p->dos_chamado = 0;
    for (int i = 0; i < nbus; i++) if (buses[i].dos) p->dos_chamado = 1;
    txt_copia_n(p->video_lcd, lcd >= 0 ? nome_lcd : "", sizeof p->video_lcd);
}
