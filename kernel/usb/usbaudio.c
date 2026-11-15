// SPDX-License-Identifier: GPL-2.0-or-later
/* usbaudio.c - audio USB, clase Audio 1.0 (UAC 1), salida de sonido (D275).
 *
 * Solo se usan las especificaciones públicas (USB 2.0, USB Device Class Definition for
 * Audio Devices 1.0 y para los formatos de audio 1.0). Al conectar un dispositivo con
 * interfaces de audio:
 *   1. se lee el descriptor de configuración completo y se recorren las interfaces:
 *      AudioControl (terminales y unidades de función) y AudioStreaming, con sus
 *      alternativas (formato PCM, canales, bits, frecuencias y el endpoint isócrono);
 *   2. se elige la alternativa de salida PCM de 16 bits y 2 canales con la frecuencia
 *      más cercana a 48 kHz (la del mezclador); la mono o de otros tamaños de muestra
 *      queda para más adelante;
 *   3. se abre el flujo isócrono (usb_iso_abrir, un anillo de 32768 cuadros que el
 *      controlador recorre sin parar), se activa la alternativa (SET_INTERFACE), se pone
 *      la unidad de función a volumen máximo y sin silencio y, si el endpoint admite
 *      control de frecuencia, se fija;
 *   4. se registra como una tarjeta de sonido más del mezclador (audio_registrar): el
 *      mezclador escribe en el anillo, delante de la posición que el controlador USB
 *      va completando, como con el HDA. El silencio del mezclador también se manda por
 *      la unidad de función (silencio_hw); el volumen lo aplica el mezclador, igual que
 *      con el resto de tarjetas (el del amplificador se queda en 0 dB).
 * Al retirar el dispositivo se quita la tarjeta del mezclador (que pasa a otra salida o,
 * sin ninguna, tira los datos de los flujos abiertos) y se cierra el flujo.
 * Una sola tarjeta USB a la vez: las funciones de la tarjeta no llevan contexto. */
#include "usb.h"
#include "audio.h"
#include "console.h"
#include "memoria.h"
#include "texto.h"

#define UA_QUADROS   32768u     /* anillo de cuadros (potencia de 2, como el de las otras tarjetas) */
#define UA_PAGINAS   (UA_QUADROS * 4 / 4096)
#define UA_TAM_QUADRO 4         /* 16 bits estéreo */
#define UA_CONFIG_MAX 2048

typedef struct {
    bool     usado;
    usb_disp_t *d;
    int      placa;             /* índice en el mezclador */
    int      h;                 /* flujo isócrono */
    uint8_t  iface_ac, iface_as, alt, ep;
    uint16_t mps;
    uint8_t  intervalo;
    uint32_t taxa;
    /* unidad de función (volumen y silencio) */
    uint8_t  fu;
    uint8_t  fu_canais;         /* 0 = maestro; si no, el 1 y el 2 */
    bool     tem_silencio, tem_volume;
    bool     calado_hw;
    int16_t  vol_max;
    uint32_t anel_fis;
    int16_t *anel_cpu;
    uint32_t sets_silencio;     /* órdenes de silencio mandadas (diagnóstico) */
} ua_t;

static ua_t ua;
static uint32_t anel_fis_fijo;  /* el anillo se reutiliza entre conexiones (audio_dma_alocar no libera) */
static int16_t *anel_cpu_fijo;

/* Candidata: una alternativa de AudioStreaming de salida. */
typedef struct {
    bool     ok;
    uint8_t  iface, alt, ep, intervalo, link;
    uint16_t mps;
    uint32_t taxa;
    bool     ctl_taxa;
} cand_t;

static bool taxa_cabe(uint32_t taxa, uint16_t mps)
{
    return (taxa + 999) / 1000 * UA_TAM_QUADRO <= mps;
}

/* Frecuencia más cercana a 48000 de las que ofrece la alternativa (3 bytes cada una;
 * n = 0: rango continuo entre las dos primeras). */
static uint32_t elegir_taxa(const uint8_t *f, uint8_t n, uint16_t mps)
{
    uint32_t mejor = 0;
    uint32_t dmejor = 0xFFFFFFFFu;
    if (n == 0) {
        uint32_t lo = f[0] | f[1] << 8 | f[2] << 16, hi = f[3] | f[4] << 8 | f[5] << 16;
        uint32_t t = 48000 < lo ? lo : 48000 > hi ? hi : 48000;
        return taxa_cabe(t, mps) ? t : 0;
    }
    for (uint8_t i = 0; i < n; i++) {
        uint32_t t = f[3 * i] | f[3 * i + 1] << 8 | f[3 * i + 2] << 16;
        if (!t || t < AUDIO_TAXA_MIN || t > AUDIO_TAXA_MAX || !taxa_cabe(t, mps)) continue;
        uint32_t dd = t > 48000 ? t - 48000 : 48000 - t;
        if (dd < dmejor) { dmejor = dd; mejor = t; }
    }
    return mejor;
}

/* Órdenes de clase a una unidad de función. canal 0 = maestro. */
static int fu_pedido(usb_disp_t *d, uint8_t pedido, uint8_t sel, uint8_t canal, uint8_t fu, uint8_t iface, void *dados, uint16_t tam)
{
    uint8_t tipo = (pedido & 0x80) ? 0xA1 : 0x21;
    return usb_controle(d, tipo, pedido, (uint16_t)(sel << 8 | canal), (uint16_t)(fu << 8 | iface), dados, tam);
}

static void fu_silencio(bool calado)
{
    if (!ua.usado || !ua.tem_silencio) return;
    uint8_t v = calado ? 1 : 0;
    int r = 0;
    for (uint8_t c = ua.fu_canais ? 1 : 0; c <= (ua.fu_canais ? 2 : 0); c++)
        r = fu_pedido(ua.d, 0x01, 1, c, ua.fu, ua.iface_ac, &v, 1);
    ua.sets_silencio++;
    ua.calado_hw = calado;
    klog("[UAC] silencio del hardware %s%s\n", calado ? "activado" : "quitado", r < 0 ? " (la orden falló)" : "");
}

/* Funciones de la tarjeta (el mezclador las llama con el flujo ya abierto). */
static void ua_ligar(void) { if (ua.usado) usb_iso_ligar(ua.d, ua.h); }
static void ua_parar(void) { if (ua.usado) usb_iso_parar(ua.d, ua.h); }
static uint32_t ua_posicao(void)
{
    if (!ua.usado) return 0;
    return (usb_iso_posicao(ua.d, ua.h) / UA_TAM_QUADRO) % UA_QUADROS;
}
static bool ua_atender(void) { return false; }   /* sin IRQ propia: el mezclador mira en cada tic */

/* ------------------------------------------------------------ conexión */

bool usbaudio_conectar(usb_disp_t *d)
{
    if (d->classe != 0x01) return false;            /* la primera interfaz (o el dispositivo) es de audio */
    if (ua.usado) { klog("[UAC] ya hay una tarjeta de audio USB: se ignora la segunda\n"); return true; }

    uint8_t cab[9];
    if (usb_controle(d, 0x80, 6, 0x0200, 0, cab, 9) < 9) return true;
    uint16_t total = (uint16_t)(cab[2] | cab[3] << 8);
    if (total < 9 || total > UA_CONFIG_MAX) { klog("[UAC] descriptor de configuracion de %u bytes: no se usa\n", total); return true; }
    uint8_t *b = kmalloc(total);
    if (!b) return true;
    int n = usb_controle(d, 0x80, 6, 0x0200, 0, b, total);
    if (n < 9) { kfree(b); return true; }

    /* Recorrido del descriptor: AudioControl y las alternativas de AudioStreaming. */
    uint8_t iface_ac = 0xFF, fu_id = 0, fu_fuente = 0, fu_tam = 0, fu_ctl[3] = { 0, 0, 0 }, fu_cnt = 0;
    struct { uint8_t id, fuente, tam, ctl[3], cnt; } fus[4];
    int nfu = 0;
    cand_t mejor;
    memset(&mejor, 0, sizeof mejor);
    uint8_t ifc = 0, alt = 0, cls = 0, sub = 0;
    cand_t cur;
    memset(&cur, 0, sizeof cur);
    bool pcm16 = false, estereo = false;
    uint32_t taxa_cur = 0;
    uint8_t freqs[3 * 32];
    uint8_t nfreq = 0;
    bool tiene_freq = false;
    for (int i = 0; i + 1 < n && b[i] >= 2 && i + b[i] <= n; i += b[i]) {
        uint8_t t = b[i + 1], l = b[i];
        if (t == 4 && l >= 9) {
            /* cerrar la candidata anterior */
            if (cur.ok && cur.taxa && (!mejor.ok || (cur.taxa > 47000 && cur.taxa <= 49000))) mejor = cur;
            memset(&cur, 0, sizeof cur);
            ifc = b[i + 2]; alt = b[i + 3]; cls = b[i + 5]; sub = b[i + 6];
            pcm16 = estereo = tiene_freq = false;
            nfreq = 0;
            taxa_cur = 0;
            if (cls == 1 && sub == 1 && iface_ac == 0xFF) iface_ac = ifc;
            cur.iface = ifc;
            cur.alt = alt;
        } else if (t == 0x24 && cls == 1 && sub == 1 && l >= 5) {
            uint8_t st = b[i + 2];
            if (st == 6 && l >= 8 && nfu < 4) {              /* FEATURE_UNIT */
                uint8_t cs = b[i + 5];
                fus[nfu].id = b[i + 3];
                fus[nfu].fuente = b[i + 4];
                fus[nfu].tam = cs;
                for (int c = 0; c < 3; c++)
                    fus[nfu].ctl[c] = (cs && 6 + (c + 1) * cs <= l - 1) ? b[i + 6 + c * cs] : 0;
                fus[nfu].cnt = (uint8_t)((l - 7) / (cs ? cs : 1));
                nfu++;
            }
        } else if (t == 0x24 && cls == 1 && sub == 2 && l >= 7) {
            uint8_t st = b[i + 2];
            if (st == 1) {                                      /* AS_GENERAL */
                cur.link = b[i + 3];
                pcm16 = (b[i + 5] | b[i + 6] << 8) == 1;
            } else if (st == 2 && b[i + 3] == 1 && l >= 8) {    /* FORMAT_TYPE_I */
                estereo = b[i + 4] == 2 && b[i + 5] == 2 && b[i + 6] == 16;
                nfreq = b[i + 7];
                uint8_t cuantos = nfreq ? nfreq : 2;
                for (int k = 0; k < cuantos * 3 && k < (int)sizeof freqs && i + 8 + k < i + l; k++) freqs[k] = b[i + 8 + k];
                if (nfreq > 32) nfreq = 32;
            }
        } else if (t == 5 && cls == 1 && sub == 2 && l >= 7 && !(b[i + 2] & 0x80) && (b[i + 3] & 3) == 1) {
            cur.ep = b[i + 2] & 0x0F;
            cur.mps = (uint16_t)((b[i + 4] | b[i + 5] << 8) & 0x7FF);
            cur.intervalo = b[i + 6];
            if (pcm16 && estereo) taxa_cur = elegir_taxa(freqs, nfreq, cur.mps);
            cur.taxa = taxa_cur;
            cur.ok = taxa_cur != 0 && cur.alt != 0;
        } else if (t == 0x25 && cls == 1 && sub == 2 && l >= 4 && b[i + 2] == 1) {
            cur.ctl_taxa = (b[i + 3] & 1) != 0;
        }
        (void)tiene_freq;
    }
    if (cur.ok && cur.taxa && (!mejor.ok || (cur.taxa > 47000 && cur.taxa <= 49000))) mejor = cur;
    /* La unidad de función de la ruta: la que sale del terminal de entrada del flujo */
    for (int k = 0; k < nfu; k++) {
        if (!fu_id || fus[k].fuente == mejor.link) {
            fu_id = fus[k].id; fu_fuente = fus[k].fuente; fu_tam = fus[k].tam; fu_cnt = fus[k].cnt;
            for (int c = 0; c < 3; c++) fu_ctl[c] = fus[k].ctl[c];
        }
    }
    (void)fu_fuente; (void)fu_tam;
    kfree(b);
    if (iface_ac == 0xFF) iface_ac = 0;
    if (!mejor.ok) {
        klog("[UAC] %s %s: sin una salida PCM de 16 bits estereo (se ignora)\n", d->fabricante, d->produto);
        return true;
    }
    if (mejor.taxa > 0 && mejor.mps > 1023) mejor.mps = 1023;

    /* El anillo de datos (una vez: audio_dma_alocar no devuelve la memoria) */
    if (!anel_fis_fijo) {
        void *cpu = NULL;
        anel_fis_fijo = audio_dma_alocar(UA_PAGINAS, &cpu);
        anel_cpu_fijo = cpu;
    }
    if (!anel_fis_fijo) { klog("[UAC] sin memoria para el anillo\n"); return true; }

    usb_iso_cfg_t cfg = { .ep = mejor.ep, .entrada = false, .mps = mejor.mps, .intervalo = mejor.intervalo,
                          .anel_fis = anel_fis_fijo, .anel_tam = UA_QUADROS * UA_TAM_QUADRO,
                          .bytes_s = mejor.taxa * UA_TAM_QUADRO, .tam_quadro = UA_TAM_QUADRO };
    int h = usb_iso_abrir(d, &cfg);
    if (h < 0) { klog("[UAC] no se pudo abrir el flujo isocrono (%d)\n", h); return true; }
    if (usb_controle(d, 0x01, 11, mejor.alt, mejor.iface, NULL, 0) < 0) {
        klog("[UAC] SET_INTERFACE %u/%u fallo\n", mejor.iface, mejor.alt);
        usb_iso_fechar(d, h);
        return true;
    }
    if (mejor.ctl_taxa) {
        uint8_t f[3] = { (uint8_t)mejor.taxa, (uint8_t)(mejor.taxa >> 8), (uint8_t)(mejor.taxa >> 16) };
        usb_controle(d, 0x22, 1, 0x0100, mejor.ep, f, 3);   /* control de frecuencia del endpoint (salida) */
    }

    memset(&ua, 0, sizeof ua);
    ua.usado = true;
    ua.d = d;
    ua.h = h;
    ua.iface_ac = iface_ac;
    ua.iface_as = mejor.iface;
    ua.alt = mejor.alt;
    ua.ep = mejor.ep;
    ua.mps = mejor.mps;
    ua.intervalo = mejor.intervalo;
    ua.taxa = mejor.taxa;
    ua.anel_fis = anel_fis_fijo;
    ua.anel_cpu = anel_cpu_fijo;
    ua.fu = fu_id;
    /* Controles de la unidad de función: del maestro, o de los canales 1 y 2 */
    if (fu_id) {
        if (fu_ctl[0] & 1) { ua.tem_silencio = true; }
        if (fu_ctl[0] & 2) { ua.tem_volume = true; }
        if (!(fu_ctl[0] & 3) && fu_cnt >= 3 && (fu_ctl[1] & fu_ctl[2] & 1)) { ua.tem_silencio = true; ua.fu_canais = 1; }
        if (!ua.tem_volume && fu_cnt >= 3 && (fu_ctl[1] & fu_ctl[2] & 2)) { ua.tem_volume = true; ua.fu_canais = 1; }
    }
    if (ua.tem_volume) {
        /* el amplificador del dispositivo, a su máximo (el volumen lo pone el mezclador) */
        int16_t max = 0;
        uint8_t c = ua.fu_canais ? 1 : 0;
        if (fu_pedido(d, 0x83, 2, c, ua.fu, iface_ac, &max, 2) >= 2) {
            ua.vol_max = max;
            for (uint8_t k = c; k <= (ua.fu_canais ? 2 : 0); k++) fu_pedido(d, 0x01, 2, k, ua.fu, iface_ac, &max, 2);
        }
    }
    if (ua.tem_silencio) fu_silencio(false);

    placa_som_t p;
    memset(&p, 0, sizeof p);
    p.driver = "usb";
    txt_copia_n(p.placa, d->produto, sizeof p.placa);
    snformatar(p.codec, sizeof p.codec, "audio USB (UAC 1), %u Hz, interfaz %u/%u, endpoint %u", mejor.taxa,
               mejor.iface, mejor.alt, mejor.ep);
    p.taxa = mejor.taxa;
    p.quadros = UA_QUADROS;
    p.anel = anel_cpu_fijo;
    p.ligar = ua_ligar;
    p.parar = ua_parar;
    p.posicao = ua_posicao;
    p.atender = ua_atender;
    p.silencio_hw = ua.tem_silencio ? fu_silencio : NULL;
    ua.placa = audio_registrar(&p);
    if (ua.placa < 0) {
        klog("[UAC] el mezclador no admite otra tarjeta\n");
        usb_iso_fechar(d, h);
        ua.usado = false;
        return true;
    }
    d->audio = true;
    klog("[UAC] %s %s: salida PCM 16 bits estereo a %u Hz, endpoint %u (paquete de hasta %u bytes, cada %u ms), "
         "unidad %u: silencio %s, volumen %s\n", d->fabricante, d->produto, mejor.taxa, mejor.ep, mejor.mps,
         1u << (mejor.intervalo ? mejor.intervalo - 1 : 0), fu_id, ua.tem_silencio ? "si" : "no", ua.tem_volume ? "si" : "no");
    return true;
}

void usbaudio_desconectar(usb_disp_t *d)
{
    if (!ua.usado || ua.d != d) return;
    audio_retirar(ua.placa);      /* primero fuera del mezclador: ya no llama a la tarjeta */
    ua.usado = false;
    usb_iso_fechar(d, ua.h);
    d->audio = false;
    klog("[UAC] tarjeta de audio USB retirada\n");
}

int usbaudio_relatorio(char *s, int max)
{
    if (!ua.usado) return 0;
    usb_iso_info_t i;
    memset(&i, 0, sizeof i);
    usb_iso_info(ua.d, ua.h, &i);
    return snformatar(s, (size_t)max, "Audio USB: %s, %u Hz, endpoint %u por %s; %u paquetes, %u completados, %u huecos, %u errores; "
                      "silencio por hardware %s\n", ua.d->produto, ua.taxa, ua.ep, usb_nome_ctl(ua.d->tipo_ctl),
                      i.paquetes, i.completados, i.subdesbordes, i.errores, ua.calado_hw ? "si" : "no");
}
