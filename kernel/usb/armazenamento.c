// SPDX-License-Identifier: GPL-3.0-only
/* armazenamento.c - pendrives: Bulk-Only Transport + SCSI (H31).
 *
 * Un pendrive USB "habla" SCSI dentro de un sobre simple (Bulk-Only):
 *   1. el sistema manda un CBW de 31 bytes por el endpoint bulk de salida, con
 *      el comando SCSI dentro;
 *   2. los datos van o vienen por los endpoints bulk;
 *   3. el pendrive responde un CSW de 13 bytes con el resultado.
 * Comandos usados: INQUIRY (quién es), TEST UNIT READY, REQUEST SENSE (por
 * qué no está listo), READ CAPACITY(10) (tamaño), READ(10) y WRITE(10).
 * Si un endpoint se para (STALL), se desbloquea con CLEAR_FEATURE; si el
 * pendrive se pierde en el protocolo, se hace el "reset de recuperación" de la
 * especificación Bulk-Only y se vuelve a intentar. */
#include "usb.h"
#include "console.h"
#include "abi.h"
#include "disco.h"
#include "memoria.h"
#include "texto.h"
#include "trava.h"

#define CBW_ASSINATURA 0x43425355u
#define CSW_ASSINATURA 0x53425355u
#define MAX_SETORES_POR_COMANDO 256   /* 128 KB: el tamaño del buffer del EHCI (D095) y del xHCI (D214) */

static uint32_t etiqueta = 1;

static void destravar(usb_disp_t *d, bool entrada)
{
    uint8_t ep = entrada ? (uint8_t)(d->ep_entrada | 0x80) : d->ep_saida;
    usb_controle(d, 0x02, 1, 0, ep, NULL, 0);         /* CLEAR_FEATURE(ENDPOINT_HALT) */
    if (entrada) d->alterna_entrada = 0; else d->alterna_saida = 0;
}

static void recuperar(usb_disp_t *d)
{
    usb_controle(d, 0x21, 0xFF, 0, d->interface, NULL, 0);   /* Bulk-Only Mass Storage Reset */
    destravar(d, true);
    destravar(d, false);
}

/* Un comando SCSI completo. Devuelve bytes de datos, o <0. */
static int scsi_sem_trava(usb_disp_t *d, const uint8_t *cdb, uint8_t tam_cdb, void *dados, uint32_t tam, bool entrada)
{
    for (int tentativa = 0; tentativa < 3; tentativa++) {
        uint8_t cbw[31];
        memset(cbw, 0, sizeof cbw);
        uint32_t tag = etiqueta++;
        *(uint32_t *)&cbw[0] = CBW_ASSINATURA;
        *(uint32_t *)&cbw[4] = tag;
        *(uint32_t *)&cbw[8] = tam;
        cbw[12] = entrada ? 0x80 : 0x00;
        cbw[13] = 0;   /* LUN 0 */
        cbw[14] = tam_cdb;
        memcpy(&cbw[15], cdb, tam_cdb);
        int r = usb_bulk(d, false, cbw, 31);
        if (r == USB_ERRO_PAROU) { recuperar(d); continue; }
        if (r < 0) return r;
        int feito = 0;
        if (tam) {
            feito = usb_bulk(d, entrada, dados, tam);
            if (feito == USB_ERRO_PAROU) { destravar(d, entrada); feito = 0; }
            else if (feito < 0) return feito;
        }
        uint8_t csw[13];
        r = usb_bulk(d, true, csw, 13);
        if (r == USB_ERRO_PAROU) {
            destravar(d, true);
            r = usb_bulk(d, true, csw, 13);
        }
        if (r < 13 || *(uint32_t *)&csw[0] != CSW_ASSINATURA || *(uint32_t *)&csw[4] != tag) {
            if (r < 0 && r != USB_ERRO_PAROU) return r;
            recuperar(d);
            continue;
        }
        if (csw[12] == 0) return feito;
        if (csw[12] == 1) return USB_ERRO;   /* falló: quien llamó decide (REQUEST SENSE) */
        recuperar(d);                        /* 2 = error de fase */
    }
    return USB_ERRO;
}

/* El sobre CBW + datos + CSW no puede mezclarse con el de otro comando
 * (u otro pendrive, o el sondeo de la tarea USB): mientras el proceso duerme
 * esperando el DMA, otro podría entrar. El bloqueo cubre el comando entero. */
static trava_t trava_scsi;

static int scsi(usb_disp_t *d, const uint8_t *cdb, uint8_t tam_cdb, void *dados, uint32_t tam, bool entrada)
{
    trava_pegar(&trava_scsi);
    int r = scsi_sem_trava(d, cdb, tam_cdb, dados, tam, entrada);
    trava_soltar(&trava_scsi);
    return r;
}

static int ler(disco_t *k, uint32_t lba, uint32_t n, void *buf)
{
    usb_disp_t *d = k->dono;
    if (!k->presente) return USB_ERRO;
    uint8_t *p = buf;
    while (n) {
        uint32_t parte = n > MAX_SETORES_POR_COMANDO ? MAX_SETORES_POR_COMANDO : n;
        uint8_t cdb[10] = { 0x28, 0, (uint8_t)(lba >> 24), (uint8_t)(lba >> 16), (uint8_t)(lba >> 8), (uint8_t)lba,
                            0, (uint8_t)(parte >> 8), (uint8_t)parte, 0 };
        /* El pendrive puede arrancarse mientras esta lectura duerme: con
         * hilos (D167) quien lee conserva el fichero, y la estructura del
         * dispositivo puede estar ya reutilizada por el siguiente. Mientras
         * se registra, d->disco aún es NULL (disco_registrar ya lee). */
        if (!k->presente || (d->disco && d->disco != k)) return USB_ERRO;
        int r = scsi(d, cdb, 10, p, parte * k->tam_setor, true);
        if (r < (int)(parte * k->tam_setor)) return r < 0 ? r : USB_ERRO;
        lba += parte; n -= parte; p += parte * k->tam_setor;
    }
    return 0;
}

static int escrever(disco_t *k, uint32_t lba, uint32_t n, const void *buf)
{
    usb_disp_t *d = k->dono;
    if (!k->presente) return USB_ERRO;
    const uint8_t *p = buf;
    while (n) {
        uint32_t parte = n > MAX_SETORES_POR_COMANDO ? MAX_SETORES_POR_COMANDO : n;
        uint8_t cdb[10] = { 0x2A, 0, (uint8_t)(lba >> 24), (uint8_t)(lba >> 16), (uint8_t)(lba >> 8), (uint8_t)lba,
                            0, (uint8_t)(parte >> 8), (uint8_t)parte, 0 };
        if (!k->presente || (d->disco && d->disco != k)) return USB_ERRO;   /* arrancado (ver ler) */
        int r = scsi(d, cdb, 10, (void *)p, parte * k->tam_setor, false);
        if (r < 0) return r;
        lba += parte; n -= parte; p += parte * k->tam_setor;
    }
    return 0;
}

int armaz_conectar(usb_disp_t *d)
{
    if (!d->ep_entrada || !d->ep_saida) return 0;
    d->alterna_entrada = d->alterna_saida = 0;
    uint8_t buf[64];
    uint8_t inquiry[6] = { 0x12, 0, 0, 0, 36, 0 };
    if (scsi(d, inquiry, 6, buf, 36, true) < 0) { klog("[USB] INQUIRY falhou\n"); return -1; }
    /* Espera a que el pendrive esté listo (algunos tardan un tiempo después de encendidos). */
    bool pronto = false;
    for (int i = 0; i < 20 && !pronto; i++) {
        uint8_t tur[6] = { 0 };
        if (scsi(d, tur, 6, NULL, 0, false) == 0) { pronto = true; break; }
        uint8_t sense[6] = { 0x03, 0, 0, 0, 18, 0 };
        scsi(d, sense, 6, buf, 18, true);
        usb_esperar_ms(d, 100);
    }
    uint8_t cap[10] = { 0x25, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    if (scsi(d, cap, 10, buf, 8, true) < 8) { klog("[USB] READ CAPACITY falhou\n"); return -1; }
    uint32_t ultimo = (uint32_t)buf[0] << 24 | (uint32_t)buf[1] << 16 | (uint32_t)buf[2] << 8 | buf[3];
    uint32_t tam_setor = (uint32_t)buf[4] << 24 | (uint32_t)buf[5] << 16 | (uint32_t)buf[6] << 8 | buf[7];
    if (tam_setor != 512) { klog("[USB] setor de %u bytes nao suportado\n", tam_setor); return 0; }
    disco_t *k = disco_registrar("usb", d->fabricante, d->produto, ultimo + 1, tam_setor, d, true, ler, escrever);
    d->disco = k;
    if (k && d->tipo_ctl == USB_XHCI)
        k->usb = (uint8_t)((DISCO_XHCI | (d->velocidade >= USB_VEL_SUPER ? DISCO_USB3 : 0)) >> 8);
    klog("[USB] armazenamento: %u setores de %u bytes (%u MB)%s\n", ultimo + 1, tam_setor,
         (ultimo + 1) / 2048, pronto ? "" : ", ainda nao estava pronto");
    return k != NULL ? 1 : 0;
}

void armaz_desconectar(usb_disp_t *d)
{
    if (d->disco) disco_removido(d->disco);
    d->disco = NULL;
}
