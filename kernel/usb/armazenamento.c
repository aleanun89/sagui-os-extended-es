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
 * especificación Bulk-Only y se vuelve a intentar.
 *
 * D468: órdenes más grandes y menos esperas.
 *   - Trozo (sectores por READ/WRITE(10)): 128 KB en USB 1.1, 240 KB en alta velocidad y 1 MB
 *     en USB 3.0 (lo que usa Linux); la página Block Limits (VPD B0) lo baja y, en alta
 *     velocidad, puede subirlo hasta 1 MB. Nunca más de lo que admite el controlador. Si una
 *     orden de más de 128 KB falla, el trozo de ESE pendrive baja a la mitad (no menos de
 *     128 KB, lo de siempre) y se repite la orden.
 *   - Las fases de una orden son secuenciales (CBW, datos, CSW), pero las que van por el mismo
 *     endpoint se entregan juntas: lectura = CBW y luego datos + CSW; escritura = CBW + datos y
 *     luego CSW. Dos esperas por orden en vez de tres. */
#include "usb.h"
#include "xhci.h"
#include "console.h"
#include "abi.h"
#include "disco.h"
#include "memoria.h"
#include "texto.h"
#include "trava.h"

#define CBW_ASSINATURA 0x43425355u
#define CSW_ASSINATURA 0x53425355u
#define TROZO_USB1   256u     /* 128 KB: USB 1.1 (y el mínimo al que se baja tras un fallo) */
#define TROZO_ALTA   480u     /* 240 KB: alta velocidad, sin más datos del pendrive */
#define TROZO_SUPER  2048u    /* 1 MB: USB 3.0 */
#define TROZO_MAX    2048u
#define CSW_FALHOU   -110     /* el CSW dice "falló" (status 1): quien llama mira el sentido */

static uint32_t etiqueta = 1;

static void destravar(usb_disp_t *d, bool entrada)
{
    uint8_t ep = entrada ? (uint8_t)(d->ep_entrada | 0x80) : d->ep_saida;
    usb_controle(d, 0x02, 1, 0, ep, NULL, 0);         /* CLEAR_FEATURE(ENDPOINT_HALT) */
    if (entrada) d->alterna_entrada = 0; else d->alterna_saida = 0;
    /* D468: en el xHCI la alternancia (o la secuencia, en USB 3.0) la lleva el controlador: hay que
     * ponerla a cero con la del dispositivo, también si el endpoint no estaba parado (un plazo). */
    xhci_reiniciar_ep(d, entrada);
}

/* false = el dispositivo no contesta (retirado o colgado); entonces no se gastan los plazos de lo demás. */
static bool recuperar(usb_disp_t *d)
{
    int r = usb_controle(d, 0x21, 0xFF, 0, d->interface, NULL, 0);   /* Bulk-Only Mass Storage Reset */
    if (r < 0 && r != USB_ERRO_PAROU) return false;
    destravar(d, true);
    destravar(d, false);
    return true;
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

/* D468: READ(10)/WRITE(10) con las fases del mismo endpoint encadenadas. Misma recuperación
 * que scsi_sem_trava: si una parte de la cadena no llegó a hacerse, se hace por separado. */
static int bot_rw_sem_trava(usb_disp_t *d, const uint8_t *cdb, void *dados, uint32_t tam, bool entrada)
{
    for (int tentativa = 0; tentativa < 3; tentativa++) {
        uint8_t cbw[31];
        memset(cbw, 0, sizeof cbw);
        uint32_t tag = etiqueta++;
        *(uint32_t *)&cbw[0] = CBW_ASSINATURA;
        *(uint32_t *)&cbw[4] = tag;
        *(uint32_t *)&cbw[8] = tam;
        cbw[12] = entrada ? 0x80 : 0x00;
        cbw[14] = 10;
        memcpy(&cbw[15], cdb, 10);
        uint8_t csw[13];
        int feito = 0, rc = USB_NAO_FEITA;   /* rc: resultado de leer el CSW (USB_NAO_FEITA = falta) */
        if (entrada) {
            int r = usb_bulk(d, false, cbw, 31);
            if (r == USB_ERRO_PAROU) { recuperar(d); continue; }
            if (r < 0) return r;
            usb_parte_t p[2] = { { dados, tam, 0 }, { csw, 13, 0 } };
            r = usb_bulk_cadeia(d, true, p, 2);
            if (r < 0) return r;
            feito = p[0].r;
            if (feito == USB_ERRO_PAROU) { destravar(d, true); feito = 0; }
            else if (feito < 0) return feito;
            else rc = p[1].r;   /* usb_bulk_cadeia hace la entrada de una en una: no cuenta como encadenada */
        } else {
            usb_parte_t p[2] = { { cbw, 31, 0 }, { dados, tam, 0 } };
            int r = usb_bulk_cadeia(d, false, p, 2);
            if (r < 0) return r;
            if (p[0].r == USB_ERRO_PAROU) { recuperar(d); continue; }
            if (p[0].r < 0) return p[0].r;
            feito = p[1].r;
            if (feito == USB_NAO_FEITA) feito = usb_bulk(d, false, dados, tam);   /* no debería pasar */
            else if (feito >= 0) d->armaz_encadeadas++;
            if (feito == USB_ERRO_PAROU) { destravar(d, false); feito = 0; }
            else if (feito < 0) return feito;
        }
        int r = rc;
        if (r == USB_NAO_FEITA) r = usb_bulk(d, true, csw, 13);
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
        if (csw[12] == 1) return CSW_FALHOU;
        recuperar(d);                        /* 2 = error de fase */
    }
    return USB_ERRO;
}

static int bot_rw(usb_disp_t *d, const uint8_t *cdb, void *dados, uint32_t tam, bool entrada)
{
    trava_pegar(&trava_scsi);
    int r = bot_rw_sem_trava(d, cdb, dados, tam, entrada);
    trava_soltar(&trava_scsi);
    return r;
}

/* D468: tras un fallo de una orden de "parte" sectores, ¿se repite con un trozo menor? Solo si
 * la orden pasaba de 128 KB (lo probado de siempre) y el pendrive sigue ahí. */
static bool reduzir_trozo(disco_t *k, usb_disp_t *d, uint32_t parte, int r)
{
    if (parte <= TROZO_USB1 || !k->presente || (d->disco && d->disco != k)) return false;
    /* Sin un CSW que cierre la orden, el pendrive puede seguir en la fase de datos y tomaría el CBW
     * siguiente como datos: antes de repetir, el reset de recuperación del Bulk-Only. Si tampoco
     * contesta a eso (retirado en mitad de la copia), no se repite. */
    if (r < 0 && r != CSW_FALHOU) {
        trava_pegar(&trava_scsi);
        bool vivo = recuperar(d);
        trava_soltar(&trava_scsi);
        if (!vivo || !k->presente) return false;
    }
    uint32_t novo = (parte / 2) & ~7u;
    if (novo < TROZO_USB1) novo = TROZO_USB1;
    d->armaz_trozo = (uint16_t)novo;
    d->armaz_reducoes++;
    k->trozo = novo;
    klog("[USB] %s: una orden de %u KB ha fallado (%d); a partir de ahora, trozos de %u KB\n", k->nome, parte / 2, r,
         novo / 2);
    return true;
}

static uint32_t trozo_de(usb_disp_t *d)
{
    uint32_t t = d->armaz_trozo ? d->armaz_trozo : TROZO_USB1;
    return t;
}

static void contar(usb_disp_t *d, bool leitura, uint32_t parte)
{
    if (leitura) { d->armaz_ord_ler++; d->armaz_kb_ler += parte / 2; }
    else { d->armaz_ord_escr++; d->armaz_kb_escr += parte / 2; }
    if (parte / 2 > d->armaz_maior_kb) d->armaz_maior_kb = parte / 2;
}

static int ler(disco_t *k, uint32_t lba, uint32_t n, void *buf)
{
    usb_disp_t *d = k->dono;
    if (!k->presente) return USB_ERRO;
    uint8_t *p = buf;
    while (n) {
        uint32_t t = trozo_de(d), parte = n > t ? t : n;
        uint8_t cdb[10] = { 0x28, 0, (uint8_t)(lba >> 24), (uint8_t)(lba >> 16), (uint8_t)(lba >> 8), (uint8_t)lba,
                            0, (uint8_t)(parte >> 8), (uint8_t)parte, 0 };
        /* El pendrive puede arrancarse mientras esta lectura duerme: con
         * hilos (D167) quien lee conserva el fichero, y la estructura del
         * dispositivo puede estar ya reutilizada por el siguiente. Mientras
         * se registra, d->disco aún es NULL (disco_registrar ya lee). */
        if (!k->presente || (d->disco && d->disco != k)) return USB_ERRO;
        int r = bot_rw(d, cdb, p, parte * k->tam_setor, true);
        if (r < (int)(parte * k->tam_setor)) {
            if (reduzir_trozo(k, d, parte, r)) continue;   /* D468: otra vez, más pequeña */
            return r < 0 ? (r == CSW_FALHOU ? USB_ERRO : r) : USB_ERRO;
        }
        contar(d, true, parte);
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
        uint32_t t = trozo_de(d), parte = n > t ? t : n;
        uint8_t cdb[10] = { 0x2A, 0, (uint8_t)(lba >> 24), (uint8_t)(lba >> 16), (uint8_t)(lba >> 8), (uint8_t)lba,
                            0, (uint8_t)(parte >> 8), (uint8_t)parte, 0 };
        if (!k->presente || (d->disco && d->disco != k)) return USB_ERRO;   /* arrancado (ver ler) */
        int r = bot_rw(d, cdb, (void *)p, parte * k->tam_setor, false);
        if (r < 0) {
            if (reduzir_trozo(k, d, parte, r)) continue;   /* D468: otra vez, más pequeña */
            return r == CSW_FALHOU ? USB_ERRO : r;
        }
        contar(d, false, parte);
        lba += parte; n -= parte; p += parte * k->tam_setor;
    }
    return 0;
}

/* D468: el trozo de un pendrive nuevo. Devuelve sectores y deja en "motivo" de dónde sale. */
static uint32_t escolher_trozo(usb_disp_t *d, uint8_t versao_scsi, const char **motivo)
{
    bool usb1 = d->tipo_ctl == USB_UHCI || d->tipo_ctl == USB_OHCI || d->velocidade == USB_VEL_PLENA ||
                d->velocidade == USB_VEL_BAIXA;
    uint32_t t;
    if (usb1) { t = TROZO_USB1; *motivo = "USB 1.1"; }
    else if (d->velocidade >= USB_VEL_SUPER) { t = TROZO_SUPER; *motivo = "USB 3.0"; }
    else { t = TROZO_ALTA; *motivo = "alta velocidad"; }
    d->armaz_vpd = 0;
    /* Block Limits (VPD B0): solo a los que dicen cumplir SPC-3 o más (versión 5 o más en el
     * INQUIRY): Linux no pide VPD a los pendrives porque alguno se cuelga; los que se declaran
     * SPC-3 la suelen tener. Primero la lista de páginas (00): B0 solo si está. */
    if (!usb1 && versao_scsi >= 5) {
        uint8_t v[64];
        memset(v, 0, sizeof v);
        uint8_t pag0[6] = { 0x12, 0x01, 0x00, 0, sizeof v, 0 };
        int r = scsi(d, pag0, 6, v, sizeof v, true);
        bool tem_b0 = false;
        if (r >= 4 && v[1] == 0x00)
            for (int i = 4; i < r && i < 4 + v[3]; i++) if (v[i] == 0xB0) tem_b0 = true;
        if (r < 0) d->armaz_vpd = 2;
        if (tem_b0) {
            memset(v, 0, sizeof v);
            uint8_t pagb0[6] = { 0x12, 0x01, 0xB0, 0, sizeof v, 0 };
            r = scsi(d, pagb0, 6, v, sizeof v, true);
            if (r >= 16 && v[1] == 0xB0) {
                uint32_t max = (uint32_t)v[8] << 24 | (uint32_t)v[9] << 16 | (uint32_t)v[10] << 8 | v[11];
                d->armaz_vpd = 1;
                klog("[USB] VPD B0: transferencia maxima %u sectores, optima %u\n", max,
                     (uint32_t)v[12] << 24 | (uint32_t)v[13] << 16 | (uint32_t)v[14] << 8 | v[15]);
                if (max && max < t) { t = max; *motivo = "VPD B0"; }
                else if (max > t && d->velocidade == USB_VEL_ALTA) { t = max > TROZO_MAX ? TROZO_MAX : max; *motivo = "VPD B0"; }
            } else d->armaz_vpd = 2;
        }
    }
    uint32_t ctl = usb_bulk_max(d) / 512u;
    if (t > ctl) { t = ctl; *motivo = "limite del controlador"; }
    if (t > 8) t &= ~7u;                                /* bloques de 4 KB enteros */
    if (t == 0) t = 1;
    return t;
}

int armaz_conectar(usb_disp_t *d)
{
    if (!d->ep_entrada || !d->ep_saida) return 0;
    d->alterna_entrada = d->alterna_saida = 0;
    d->armaz_trozo = 0;
    d->armaz_reducoes = 0;
    d->armaz_ord_ler = d->armaz_kb_ler = d->armaz_ord_escr = d->armaz_kb_escr = 0;
    d->armaz_maior_kb = d->armaz_encadeadas = 0;
    uint8_t buf[64];
    uint8_t inquiry[6] = { 0x12, 0, 0, 0, 36, 0 };
    if (scsi(d, inquiry, 6, buf, 36, true) < 0) { klog("[USB] INQUIRY falhou\n"); return -1; }
    uint8_t versao_scsi = buf[2];
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
    const char *motivo = "";
    d->armaz_trozo = (uint16_t)escolher_trozo(d, versao_scsi, &motivo);
    klog("[USB] armazenamento: ordenes de hasta %u KB (%s, SCSI %u)\n", d->armaz_trozo / 2, motivo, versao_scsi);
    disco_t *k = disco_registrar("usb", d->fabricante, d->produto, ultimo + 1, tam_setor, d, true, ler, escrever);
    d->disco = k;
    if (k) k->trozo = d->armaz_trozo;                   /* D468: la caché lee por delante hasta ahí */
    if (k && d->tipo_ctl == USB_XHCI)
        k->usb = (uint8_t)((DISCO_XHCI | (d->velocidade >= USB_VEL_SUPER ? DISCO_USB3 : 0)) >> 8);
    klog("[USB] armazenamento: %u setores de %u bytes (%u MB)%s\n", ultimo + 1, tam_setor,
         (ultimo + 1) / 2048, pronto ? "" : ", ainda nao estava pronto");
    return k != NULL ? 1 : 0;
}

/* D468: una línea por pendrive con las órdenes hechas («sync -v»). */
void armaz_registar(void)
{
    for (int i = 0; i < USB_MAX_DISP; i++) {
        usb_disp_t *d = usb_disp(i);
        disco_t *k = d ? d->disco : NULL;
        if (!k) continue;
        klog("[USB] %s: lectura %u ordenes, %u KB, media %u KB; escritura %u ordenes, %u KB, media %u KB; "
             "mayor %u KB, trozo %u KB, reducciones %u, encadenadas %u, VPD %s\n", k->nome,
             d->armaz_ord_ler, d->armaz_kb_ler, d->armaz_ord_ler ? d->armaz_kb_ler / d->armaz_ord_ler : 0,
             d->armaz_ord_escr, d->armaz_kb_escr, d->armaz_ord_escr ? d->armaz_kb_escr / d->armaz_ord_escr : 0,
             d->armaz_maior_kb, d->armaz_trozo / 2, d->armaz_reducoes, d->armaz_encadeadas,
             d->armaz_vpd == 1 ? "B0" : d->armaz_vpd == 2 ? "fallo" : "no");
    }
}

void armaz_desconectar(usb_disp_t *d)
{
    if (d->disco) disco_removido(d->disco);
    d->disco = NULL;
}
