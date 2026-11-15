// SPDX-License-Identifier: GPL-2.0-or-later
/* ar9485_logica.h - la parte pura del driver del Atheros AR9485 (D280):
 * lectura y restauración de la imagen de la EEPROM/OTP (bloques con suma de
 * control y plantillas), tablas de canales, interpolación de las potencias de
 * calibración, pendiente delta del PHY, suma de control del descriptor de
 * transmisión y tabla de tasas. Sin acceso al hardware, para probarla en el
 * anfitrión (tests/recursos/ar9485_anfitriao.c, tests/h280_wifi_ar9485.py)
 * con el mismo código que compila el kernel. La lógica sigue la de la familia
 * AR9003 del athn(4) de OpenBSD (ISC, kernel/terceros/bsd/athn/). */
#ifndef SAGUI_AR9485_LOGICA_H
#define SAGUI_AR9485_LOGICA_H
#ifdef SAGUI_KERNEL
#include "tipos.h"
#else
#include <stdbool.h>
#include <stdint.h>
#endif

/* ---------------------------------------------------------- imagen de la ROM */

#define AR9485_ROM_MAX       4096      /* la EEPROM del AR9485 es de 4 KB (la OTP, de 1 KB) */
#define AR9485_COMPRIME_NADA 0         /* el bloque es la imagen entera */
#define AR9485_COMPRIME_BLOQUE 3       /* trozos que tapan la plantilla */

/* Cabecera de bloque: 4 bytes (el mismo reparto de bits que ar9003_read_rom). */
typedef struct { uint8_t alg, ref; uint16_t len; } ar9485_rom_cab_t;

static inline void ar9485_rom_cabecera(const uint8_t h[4], ar9485_rom_cab_t *c)
{
    c->alg = (uint8_t)((h[0] & 0xe0) >> 5);
    c->ref = (uint8_t)(((h[1] & 0x80) >> 2) | (h[0] & 0x1f));
    c->len = (uint16_t)(((h[1] & 0x7f) << 4) | ((h[2] & 0xf0) >> 4));
}

/* Suma de 16 bits de los bytes de un bloque (la que guarda la ROM tras él). */
static inline uint16_t ar9485_rom_soma(const uint8_t *b, int n)
{
    uint16_t s = 0;
    for (int i = 0; i < n; i++) s = (uint16_t)(s + b[i]);
    return s;
}

/* Pone un bloque en la imagen «eep» (de «tam» bytes). Con AR9485_COMPRIME_BLOQUE
 * parte de la plantilla (tam bytes) y le superpone los trozos (hueco, longitud,
 * datos); con AR9485_COMPRIME_NADA el bloque es la imagen. 0 = bien, -1 = bloque
 * mal formado (nunca escribe fuera de la imagen). */
static inline int ar9485_rom_restaurar(uint8_t *eep, int tam, const uint8_t *plantilla, uint8_t alg,
                                       const uint8_t *buf, int len)
{
    if (alg == AR9485_COMPRIME_BLOQUE) {
        if (!plantilla) return -1;
        for (int i = 0; i < tam; i++) eep[i] = plantilla[i];
        int off = 0;
        const uint8_t *p = buf, *fim = buf + len;
        while (p + 2 <= fim) {
            off += *p++;                 /* hueco con el trozo anterior */
            int clen = *p++;
            if (p + clen > fim) return -1;
            if (off + clen > tam) return -1;
            for (int i = 0; i < clen; i++) eep[off + i] = p[i];
            p += clen;
            off += clen;
        }
        return 0;
    }
    if (alg == AR9485_COMPRIME_NADA) {
        if (len != tam) return -1;
        for (int i = 0; i < tam; i++) eep[i] = buf[i];
        return 0;
    }
    return -1;
}

/* Lector de la ROM: lee «n» bytes desde «dir» hacia abajo (como en ar9003_read_*_data:
 * los datos de la EEPROM se guardan desde el final y cada bloque está al revés en
 * direcciones). 0 = bien. */
typedef int (*ar9485_lector_t)(void *ctx, uint32_t dir, void *buf, int n);
typedef const uint8_t *(*ar9485_plantilla_t)(void *ctx, uint8_t ref);

/* Recorre los bloques desde «base» y arma la imagen en «eep». Devuelve cuántos bloques
 * buenos restauró (0 si ninguno); los de suma mala se saltan, como en athn. «tmp» es un
 * búfer de 2048 bytes. */
static inline int ar9485_rom_montar(ar9485_lector_t ler, ar9485_plantilla_t plantilla, void *ctx, uint32_t base,
                                    uint8_t *eep, int tam, uint8_t *tmp)
{
    int buenos = 0;
    int addr = (int)base;
    for (int i = 0; i < 100; i++) {
        uint8_t h[4];
        if (addr < 3 || ler(ctx, (uint32_t)addr, h, 4) != 0) break;
        if ((h[0] | h[1] | h[2] | h[3]) == 0 || (h[0] & h[1] & h[2] & h[3]) == 0xff) break;
        addr -= 4;
        ar9485_rom_cab_t c;
        ar9485_rom_cabecera(h, &c);
        if (c.len > 2047 || addr < (int)c.len + 2) break;
        if (ler(ctx, (uint32_t)addr, tmp, c.len) != 0) break;
        addr -= c.len;
        uint8_t s[2];
        if (ler(ctx, (uint32_t)addr, s, 2) != 0) break;
        addr -= 2;
        uint16_t soma = (uint16_t)(s[0] | s[1] << 8);
        if (soma != ar9485_rom_soma(tmp, c.len)) continue;   /* bloque malo: se salta */
        const uint8_t *pl = c.alg == AR9485_COMPRIME_BLOQUE ? plantilla(ctx, c.ref) : (const uint8_t *)0;
        if (ar9485_rom_restaurar(eep, tam, pl, c.alg, tmp, c.len) == 0) buenos++;
    }
    return buenos;
}

/* ---------------------------------------------------------- canales y potencias */

#define AR9485_BCHAN_UNUSED 0xff

/* Frecuencia en MHz de un canal de 2,4 GHz (1 a 14); 0 si no es válido. */
static inline uint16_t ar9485_canal_freq(uint8_t canal)
{
    if (canal == 14) return 2484;
    if (canal >= 1 && canal <= 13) return (uint16_t)(2407 + 5 * canal);
    return 0;
}

/* «fbin» de la ROM: 2,4 GHz, MHz sobre 2300. */
static inline uint8_t ar9485_chan2fbin(uint16_t freq) { return (uint8_t)(freq - 2300); }

static inline int ar9485_interpolar(int x, int x1, int y1, int x2, int y2)
{
    if (x1 == x2) return y1;
    return y1 + ((x - x1) * (y2 - y1)) / (x2 - x1);
}

/* Los dos «piers» vecinos de fbin en una tabla de frecuencias. */
static inline void ar9485_pier_ival(uint8_t fbin, const uint8_t *pierfreq, int npiers, int *lo, int *hi)
{
    int i;
    for (i = 0; i < npiers; i++)
        if (pierfreq[i] == AR9485_BCHAN_UNUSED || pierfreq[i] > fbin) break;
    *hi = i;
    *lo = *hi - 1;
    if (*lo == -1) *lo = *hi;
    else if (*hi == npiers || pierfreq[*hi] == AR9485_BCHAN_UNUSED) *hi = *lo;
}

/* Potencias objetivo por canal: «fbins» tiene «nchans» frecuencias y «pot» las
 * potencias (n por entrada, a «paso» bytes una de otra) de esa entrada; interpola
 * entre las dos entradas que rodean a fbin (la lógica de ar9003_get_lg_tpow). */
static inline void ar9485_tpow(uint8_t fbin, const uint8_t *fbins, int nchans, const uint8_t *pot, int paso, int n,
                               uint8_t *saida)
{
    int lo = -1, hi = -1;
    for (int i = 0; i < nchans; i++) {
        int delta = fbin - fbins[i];
        if (delta >= 0 && (lo == -1 || delta < fbin - fbins[lo])) lo = i;
        if (delta <= 0 && (hi == -1 || delta > fbin - fbins[hi])) hi = i;
    }
    if (lo == -1) lo = hi;
    else if (hi == -1) hi = lo;
    if (lo == -1) { for (int i = 0; i < n; i++) saida[i] = 0; return; }   /* tabla vacía */
    for (int i = 0; i < n; i++)
        saida[i] = (uint8_t)ar9485_interpolar(fbin, fbins[lo], pot[lo * paso + i], fbins[hi], pot[hi * paso + i]);
}

/* Pendiente delta del PHY (athn_get_delta_slope): coeff = (100 << 24) / MHz. */
static inline void ar9485_delta_slope(uint32_t coeff, uint32_t *exponente, uint32_t *mantisa)
{
    uint32_t e;
    for (e = 31; e > 0; e--)
        if (coeff & (1u << e)) break;
    e = 14 - (e - 24);
    uint32_t man = coeff + (1u << (24 - e - 1));
    *mantisa = man >> (24 - e);
    *exponente = e - 16;
}

/* ---------------------------------------------------------- tasas */

/* Índice de tasa (el mismo que el del RTL8188EU: 0..3 CCK 1, 2, 5,5 y 11 Mb/s;
 * 4..11 OFDM 6 a 54 Mb/s; 12..19 MCS 0 a 7) -> código de tasa del descriptor. */
static inline uint8_t ar9485_taxa_hw(uint8_t ridx)
{
    static const uint8_t t[20] = { 0x1b, 0x1a, 0x19, 0x18, 0x0b, 0x0f, 0x0a, 0x0e, 0x09, 0x0d, 0x08, 0x0c,
                                   0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87 };
    return ridx < 20 ? t[ridx] : 0x1b;
}

/* Tasa en unidades de 500 kb/s (el campo «rate» de athn_rates). */
static inline uint16_t ar9485_taxa_500k(uint8_t ridx)
{
    static const uint16_t t[20] = { 2, 4, 11, 22, 12, 18, 24, 36, 48, 72, 96, 108,
                                    13, 26, 39, 52, 78, 104, 117, 130 };
    return ridx < 20 ? t[ridx] : 2;
}

/* Tasa de la respuesta de control (ACK) a la que se contesta a una tasa dada. */
static inline uint8_t ar9485_taxa_resposta(uint8_t ridx)
{
    static const uint8_t t[20] = { 0, 1, 1, 1, 4, 4, 6, 6, 8, 8, 8, 8, 4, 6, 6, 8, 8, 8, 8, 8 };
    return ridx < 20 ? t[ridx] : 0;
}

static inline bool ar9485_taxa_ofdm(uint8_t ridx) { return ridx >= 4; }

/* Duración en microsegundos de «len» bytes a la tasa «ridx» (athn_txtime, 2,4 GHz,
 * sin preámbulo corto salvo que se pida). */
static inline int ar9485_txtime(int len, uint8_t ridx, bool preambulo_curto)
{
    int rate = ar9485_taxa_500k(ridx);
    int txtime;
    if (ar9485_taxa_hw(ridx) & 0x80) {
        txtime = 8 + 8 + 4 + 4 + 4 * 4 + 8 + 4 * ((8 * len + 16 + 6) / (rate * 2));
        txtime += 6;
    } else if (ar9485_taxa_ofdm(ridx)) {
        txtime = (8 + 4 * len + 3 + rate / 2) / rate;
        txtime = 16 + 4 + 4 * txtime + 16;
    } else {
        txtime = (16 * len + rate / 2) / rate;
        if (ridx != 0 && preambulo_curto) txtime += 72 + 24;
        else txtime += 144 + 48;
        txtime += 10;
    }
    return txtime;
}

/* ---------------------------------------------------------- descriptores */

/* Suma de control de un descriptor de transmisión AR9003 (128 bytes): ds_info, ds_link y
 * los cuatro pares (dato, control) de los fragmentos; la guarda ds_ctl10. */
static inline uint16_t ar9485_tx_soma(uint32_t info, uint32_t link, const uint32_t segs[8])
{
    uint32_t sum = info + link;
    for (int i = 0; i < 8; i++) sum += segs[i];
    sum = (sum >> 16) + (sum & 0xffff);
    return (uint16_t)sum;
}

/* Una trama recibida es buena si el descriptor de estado está terminado y es de datos de
 * recepción con el identificador del fabricante (0x168c). */
static inline bool ar9485_rx_valido(uint32_t info, uint32_t status11)
{
    if (!(status11 & 0x00000001u)) return false;                       /* DONE */
    return (info & (0xffff0000u | 0x00008000u | 0x00004000u)) == (0x168cu << 16);
}

/* Entrada de la caché de claves: la MAC va desplazada un bit a la derecha. */
static inline void ar9485_chave_mac(const uint8_t m[6], uint32_t *lo, uint32_t *hi)
{
    uint32_t l = (uint32_t)m[0] | (uint32_t)m[1] << 8 | (uint32_t)m[2] << 16 | (uint32_t)m[3] << 24;
    uint32_t h = (uint32_t)m[4] | (uint32_t)m[5] << 8;
    *lo = l >> 1 | h << 31;
    *hi = h >> 1;
}

/* Control de secuencia de una trama (bytes 22 y 23): el número de secuencia en los 12 bits altos, dejando
 * el número de fragmento. wifi.c no numera las tramas (el RTL8188EU lo hace el hardware); en el AR9485, con la
 * numeración del chip apagada como en athn, la pone el driver. Las tramas de control no llevan secuencia. */
static inline void ar9485_seq_poner(uint8_t *f, uint16_t seq)
{
    if ((f[0] & 0x0C) == 0x04) return;
    f[22] = (uint8_t)(((seq & 0x0f) << 4) | (f[22] & 0x0f));
    f[23] = (uint8_t)((seq >> 4) & 0xff);
}

/* MAC que vale: ni toda ceros, ni toda unos, ni multidifusión. */
static inline bool ar9485_mac_valida(const uint8_t m[6])
{
    if (m[0] & 1) return false;
    int z = 0, f = 0;
    for (int i = 0; i < 6; i++) { z += m[i] == 0; f += m[i] == 0xff; }
    return z < 6 && f < 6;
}

#endif
